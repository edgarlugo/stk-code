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

#include "physics/stk_dynamics_world.hpp"

#include "physics/btKart.hpp"
#include "utils/vita_jobs.hpp"

// ----------------------------------------------------------------------------
/** Casts the wheel rays of one action (a kart). */
static void castWheelRays(void* actions, int index)
{
    btAlignedObjectArray<btActionInterface*>& all =
        *static_cast<btAlignedObjectArray<btActionInterface*>*>(actions);
    static_cast<btKart*>(all[index])->updateAllWheelTransformsWS();
}   // castWheelRays

// ----------------------------------------------------------------------------
/** The rays of a kart only read the world, and the rest of its update only
 *  changes its own chassis: so the rays of all karts are cast first, on all
 *  cores, then the karts are updated in order as usual. */
void STKDynamicsWorld::updateActions(btScalar timeStep)
{
    if (m_actions.size() < 2 || m_actions.size() > VitaJobs::MAX_JOBS ||
        !VitaJobs::isEnabled())
    {
        btDiscreteDynamicsWorld::updateActions(timeStep);
        return;
    }
    VitaJobs::start(castWheelRays, &m_actions, m_actions.size());
    VitaJobs::finish();

    for (int i = 0; i < m_actions.size(); i++)
        static_cast<btKart*>(m_actions[i])->updateVehicle(timeStep, true);
}   // updateActions
