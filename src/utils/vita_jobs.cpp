//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026 SuperTuxKart-Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include "utils/vita_jobs.hpp"

#include "network/network_config.hpp"
#include "utils/log.hpp"

#ifdef __VITA__
#include <psp2/kernel/threadmgr.h>
#include <pthread.h>
#include <stdint.h>

// main.cpp wraps pthread_create to give every thread a 2 MB stack: the real
// one keeps the small stacks of the pool
extern "C" int __real_pthread_create(pthread_t* thread,
                                     const pthread_attr_t* attr,
                                     void* (*start)(void*), void* arg);
#endif

namespace VitaJobs
{
#ifdef __VITA__
namespace
{
    const int THREADS = 2;
    /** The jobs need little stack, and the system has little memory left
     *  for thread stacks next to a race. */
    const size_t STACK_SIZE = 256 * 1024;
    /** Threads of the pool that could be created. */
    int g_threads = 0;
    thread_local int g_thread_index = 0;
    /** Signalled once per thread for each batch. */
    SceUID g_semaphore = -1;
    JobFunction g_function;
    void* g_data;
    int g_count = 0;
    unsigned int g_batch = 0;
    /** True if the current batch ran on the main thread (see start()). */
    bool g_on_main = false;
    /** Next job a thread tries to take. */
    int g_next = 0;
    /** Threads that were signalled and did not leave the batch yet. */
    int g_out = 0;
    /** 0 waiting, 1 running, 2 done. */
    int g_state[MAX_JOBS];

    // ------------------------------------------------------------------------
    bool take(int index)
    {
        int waiting = 0;
        return __atomic_compare_exchange_n(&g_state[index], &waiting, 1,
                                           false, __ATOMIC_ACQUIRE,
                                           __ATOMIC_RELAXED);
    }   // take

    // ------------------------------------------------------------------------
    void run(int index)
    {
        g_function(g_data, index);
        __atomic_store_n(&g_state[index], 2, __ATOMIC_RELEASE);
    }   // run

    // ------------------------------------------------------------------------
    /** Runs the jobs that no thread took yet. */
    void runFreeJobs()
    {
        int index;
        while ((index = __atomic_fetch_add(&g_next, 1, __ATOMIC_RELAXED)) <
               g_count)
        {
            if (take(index))
                run(index);
        }
    }   // runFreeJobs

    // ------------------------------------------------------------------------
    void* threadMain(void* arg)
    {
        const int index = (int)(intptr_t)arg;
        g_thread_index = index;
        sceKernelChangeThreadCpuAffinityMask(sceKernelGetThreadId(),
                                             SCE_KERNEL_CPU_MASK_USER_0 << index);
        while (true)
        {
            sceKernelWaitSema(g_semaphore, 1, NULL);
            runFreeJobs();
            __atomic_sub_fetch(&g_out, 1, __ATOMIC_RELEASE);
        }
        return NULL;
    }   // threadMain

    // ------------------------------------------------------------------------
    bool startThreads()
    {
        static bool tried = false;
        if (tried)
            return g_threads > 0;
        tried = true;
        g_semaphore = sceKernelCreateSema("stk_jobs", 0, 0, THREADS, NULL);
        if (g_semaphore < 0)
            return false;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, STACK_SIZE);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        // The threads of the pool get one of the other cores each
        for (int i = 1; i <= THREADS; i++)
        {
            pthread_t thread;
            if (__real_pthread_create(&thread, &attr, threadMain,
                                      (void*)(intptr_t)i) != 0)
                break;
            g_threads++;
        }
        pthread_attr_destroy(&attr);
        Log::info("VitaJobs", "%d of %d threads started.", g_threads, THREADS);
        // and the main thread keeps the first one
        if (g_threads > 0)
        {
            sceKernelChangeThreadCpuAffinityMask(sceKernelGetThreadId(),
                                                 SCE_KERNEL_CPU_MASK_USER_0);
        }
        return g_threads > 0;
    }   // startThreads
}   // namespace
#endif

// ----------------------------------------------------------------------------
bool isEnabled()
{
#ifdef __VITA__
    return !NetworkConfig::get()->isNetworking() && startThreads();
#else
    return false;
#endif
}   // isEnabled

// ----------------------------------------------------------------------------
unsigned int start(JobFunction function, void* data, int count)
{
#ifdef __VITA__
    g_batch++;
    // A thread that has not left the previous batch (it was slow to wake up)
    // would find this one: then the main thread runs it alone
    g_on_main = !isEnabled() ||
                __atomic_load_n(&g_out, __ATOMIC_ACQUIRE) > 0;
    if (!g_on_main)
    {
        g_function = function;
        g_data     = data;
        g_count    = count;
        g_next     = 0;
        for (int i = 0; i < count; i++)
            g_state[i] = 0;
        __atomic_store_n(&g_out, g_threads, __ATOMIC_RELEASE);
        sceKernelSignalSema(g_semaphore, g_threads);
        return g_batch;
    }
#endif
    for (int i = 0; i < count; i++)
        function(data, i);
#ifdef __VITA__
    return g_batch;
#else
    return 0;
#endif
}   // start

// ----------------------------------------------------------------------------
void wait(unsigned int batch, int index)
{
#ifdef __VITA__
    if (batch != g_batch || g_on_main)
        return;
    if (take(index))
    {
        run(index);
        return;
    }
    while (__atomic_load_n(&g_state[index], __ATOMIC_ACQUIRE) != 2) {}
#endif
}   // wait

// ----------------------------------------------------------------------------
void finish()
{
#ifdef __VITA__
    if (g_on_main)
        return;
    runFreeJobs();
    for (int i = 0; i < g_count; i++)
    {
        while (__atomic_load_n(&g_state[i], __ATOMIC_ACQUIRE) != 2) {}
    }
#endif
}   // finish

// ----------------------------------------------------------------------------
int getThreadIndex()
{
#ifdef __VITA__
    return g_thread_index;
#else
    return 0;
#endif
}   // getThreadIndex

}   // namespace VitaJobs
