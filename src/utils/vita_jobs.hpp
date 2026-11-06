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

#ifndef HEADER_VITA_JOBS_HPP
#define HEADER_VITA_JOBS_HPP

/** A small pool of threads on the two other user cores of the Vita. The main
 *  thread (pinned to the first core) hands them batches of independent jobs,
 *  e.g. the raycasts of the karts, and takes jobs of the batch itself while
 *  it waits. Elsewhere, and in network games (where the order of the work
 *  must not differ from the server), the jobs run on the main thread.
 *  Only the main thread starts and waits for batches, one at a time: a batch
 *  must be finished before the next one starts.
 */
namespace VitaJobs
{
    typedef void (*JobFunction)(void* data, int index);

    /** Most jobs in a batch. */
    const int MAX_JOBS = 128;

    /** True if batches run on the other cores. */
    bool isEnabled();
    /** Starts the jobs 0 to count-1 of a new batch and returns the batch
     *  number. Count must not be larger than MAX_JOBS. */
    unsigned int start(JobFunction function, void* data, int count);
    /** Waits until a job of a batch is done, and runs it on this thread if
     *  no thread took it yet. Returns at once for an older batch. */
    void wait(unsigned int batch, int index);
    /** Runs the jobs of the batch no thread took yet, then waits for all. */
    void finish();
    /** 0 on the main thread, 1 or 2 on the threads of the pool. */
    int getThreadIndex();
}   // namespace VitaJobs

#endif
