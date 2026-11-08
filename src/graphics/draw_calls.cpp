//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2015 SuperTuxKart-Team
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

#ifndef SERVER_ONLY
#include "graphics/draw_calls.hpp"
#include "graphics/central_settings.hpp"

#include "config/stk_config.hpp"
#include "config/user_config.hpp"
#include "graphics/cpu_particle_manager.hpp"
#include "graphics/irr_driver.hpp"
#include "graphics/light.hpp"
#include "graphics/lod_node.hpp"
#include "graphics/shaders.hpp"
#include "graphics/stk_particle.hpp"
#include "graphics/stk_text_billboard.hpp"
#include "graphics/text_billboard_drawer.hpp"
#include "graphics/sp/sp_base.hpp"
#include "graphics/sp/sp_mesh_node.hpp"
#include "tracks/track.hpp"
#include "utils/profiler.hpp"
#include "utils/vita_jobs.hpp"

// The scene walk on several cores (see VitaJobs): the children of the root
// are split in chunks walked on all cores, each recording what it found, in
// order: 0 mesh node, 1 particles, 2 billboard, 3 text billboard.
struct WalkFound
{
    void* m_node;
    int   m_kind;
};
static const int WALK_CHUNKS = 24;
static core::array<scene::ISceneNode*> g_walk_chunk[WALK_CHUNKS];
static std::vector<WalkFound> g_walk_found_chunk[WALK_CHUNKS];
static const scene::ICameraSceneNode* g_walk_cam = NULL;
/** Where the walk on this thread records what it finds, NULL to add it. */
static thread_local std::vector<WalkFound>* g_walk_found = NULL;

#include <numeric>

#include <ICameraSceneNode.h>
#include <ISceneManager.h>
#include <SViewFrustum.h>
#include "../../lib/irrlicht/source/Irrlicht/CEmptySceneNode.h"

// ----------------------------------------------------------------------------
/** Nodes that only change when they move. */
static bool isStaticSPNode(scene::ISceneNode* node)
{
    switch ((int)node->getType())
    {
    case scene::ESNT_EMPTY:
    case scene::ESNT_LOD_NODE:
        return true;
    case scene::ESNT_ANIMATED_MESH:
        return !static_cast<SP::SPMeshNode*>(node)->getAnimationState();
    case scene::ESNT_UNKNOWN:
        return dynamic_cast<LightNode*>(node) != NULL;
    default:
        return false;
    }
}   // isStaticSPNode

// ----------------------------------------------------------------------------
/** Adds the boxes of the meshes under node, whose transformation is m. */
static void addStaticBox(scene::ISceneNode* node, const core::matrix4& m,
                         core::aabbox3df& box, bool& empty)
{
    const core::array<scene::ISceneNode*>& children = node->getChildren();
    for (unsigned i = 0; i < children.size(); i++)
    {
        const core::matrix4 child_m = m * children[i]->getRelativeTransformation();
        if (children[i]->getType() == scene::ESNT_ANIMATED_MESH)
        {
            core::aabbox3df b = children[i]->getBoundingBox();
            child_m.transformBoxEx(b);
            if (empty)
                box = b;
            else
                box.addInternalBox(b);
            empty = false;
        }
        addStaticBox(children[i], child_m, box, empty);
    }
}   // addStaticBox

// ----------------------------------------------------------------------------
/** True if node is a static subtree (CEmptySceneNode::OnAnimate)
 *  whose meshes are all out of the camera frustum. */
static bool isStaticSubtreeOutOfView(scene::ISceneNode* node,
                                     const scene::ICameraSceneNode* cam)
{
    scene::CEmptySceneNode* e = static_cast<scene::CEmptySceneNode*>(node);
    if (!e->StaticSubtree || CVS->isDeferredEnabled())
        return false;
    if (e->StaticBoxCount != e->AnimateCount)
    {
        e->StaticBoxEmpty = true;
        addStaticBox(e, e->getAbsoluteTransformation(), e->StaticBox,
            e->StaticBoxEmpty);
        e->StaticBoxCount = e->AnimateCount;
    }
    if (e->StaticBoxEmpty)
        return true;
    const core::aabbox3df& box = e->StaticBox;
    const scene::SViewFrustum& frust = *cam->getViewFrustum();
    for (s32 i = 0; i < scene::SViewFrustum::VF_PLANE_COUNT; i++)
    {
        // The box corner nearest to the inside of the plane
        const core::plane3df& plane = frust.planes[i];
        const core::vector3df corner(
            plane.Normal.X >= 0.0f ? box.MinEdge.X : box.MaxEdge.X,
            plane.Normal.Y >= 0.0f ? box.MinEdge.Y : box.MaxEdge.Y,
            plane.Normal.Z >= 0.0f ? box.MinEdge.Z : box.MaxEdge.Z);
        if (plane.Normal.dotProduct(corner) + plane.D > 0.0f)
            return true;
    }
    return false;
}   // isStaticSubtreeOutOfView

// ----------------------------------------------------------------------------
bool DrawCalls::isCulledPrecise(const scene::ICameraSceneNode *cam,
                                const scene::ISceneNode* node,
                                bool visualization)
{
    if (!node->getAutomaticCulling() && !visualization)
        return false;

    const core::matrix4 &trans = node->getAbsoluteTransformation();
    core::vector3df edges[8];
    node->getBoundingBox().getEdges(edges);
    for (unsigned i = 0; i < 8; i++)
        trans.transformVect(edges[i]);

    /* From irrlicht
       /3--------/7
      / |       / |
     /  |      /  |
    1---------5   |
    |  /2- - -|- -6
    | /       |  /
    |/        | /
    0---------4/
    */

    if (visualization)
    {
        addEdgeForViz(edges[0], edges[1]);
        addEdgeForViz(edges[1], edges[5]);
        addEdgeForViz(edges[5], edges[4]);
        addEdgeForViz(edges[4], edges[0]);
        addEdgeForViz(edges[2], edges[3]);
        addEdgeForViz(edges[3], edges[7]);
        addEdgeForViz(edges[7], edges[6]);
        addEdgeForViz(edges[6], edges[2]);
        addEdgeForViz(edges[0], edges[2]);
        addEdgeForViz(edges[1], edges[3]);
        addEdgeForViz(edges[5], edges[7]);
        addEdgeForViz(edges[4], edges[6]);
        if (!node->getAutomaticCulling())
        {
            return false;
        }
    }

    const scene::SViewFrustum &frust = *cam->getViewFrustum();
    for (s32 i = 0; i < scene::SViewFrustum::VF_PLANE_COUNT; i++)
    {
        if (isBoxInFrontOfPlane(frust.planes[i], edges))
        {
            return true;
        }
    }
    return false;

}   // isCulledPrecise

// ----------------------------------------------------------------------------
bool DrawCalls::isBoxInFrontOfPlane(const core::plane3df &plane,
                                    const core::vector3df* edges)
{
    for (u32 i = 0; i < 8; i++)
    {
        if (plane.classifyPointRelation(edges[i]) != core::ISREL3D_FRONT)
            return false;
    }
    return true;
}   // isBoxInFrontOfPlane

// ----------------------------------------------------------------------------
void DrawCalls::addEdgeForViz(const core::vector3df &p0,
                              const core::vector3df &p1)
{
    m_bounding_boxes.push_back(p0.X);
    m_bounding_boxes.push_back(p0.Y);
    m_bounding_boxes.push_back(p0.Z);
    m_bounding_boxes.push_back(p1.X);
    m_bounding_boxes.push_back(p1.Y);
    m_bounding_boxes.push_back(p1.Z);
}   // addEdgeForViz

// ----------------------------------------------------------------------------
void DrawCalls::renderBoundingBoxes()
{
    Shaders::ColoredLine *line = Shaders::ColoredLine::getInstance();
    line->use();
    line->bindVertexArray();
    line->bindBuffer();
    line->setUniforms(irr::video::SColor(255, 255, 0, 0));
    const float *tmp = m_bounding_boxes.data();
    for (unsigned int i = 0; i < m_bounding_boxes.size(); i += 1024 * 6)
    {
        unsigned count = std::min((unsigned)m_bounding_boxes.size() - i,
            (unsigned)1024 * 6);
        glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(float), &tmp[i]);

        glDrawArrays(GL_LINES, 0, count / 3);
    }
    m_bounding_boxes.clear();
}   // renderBoundingBoxes

// ----------------------------------------------------------------------------
void DrawCalls::parseSceneManager(core::array<scene::ISceneNode*> &List,
                                  const scene::ICameraSceneNode *cam)
{
    for (unsigned i = 0; i < List.size(); ++i)
    {
        if (!List[i]->isVisible())
            continue;

        // Try the dynamic_casts below only on the node types they can match
        const scene::ESCENE_NODE_TYPE type = List[i]->getType();
        // Static subtrees out of view
        if (type == ESNT_EMPTY && isStaticSubtreeOutOfView(List[i], cam))
            continue;
        if (type == ESNT_LOD_NODE)
        {
            LODNode *node = static_cast<LODNode *>(List[i]);
            // With shadows the SP also culls against the shadow cascades
            if (!CVS->isDeferredEnabled() && node->isOutOfView(cam))
                continue;
            node->updateVisibility();
            node->animateDrawnLevel();

            // One child array per LOD depth, reused every frame
            static thread_local core::array<scene::ISceneNode*> lod_child[8];
            static thread_local unsigned lod_depth = 0;
            core::array<scene::ISceneNode*> deep_child;
            core::array<scene::ISceneNode*>& child =
                lod_depth < 8 ? lod_child[lod_depth] : deep_child;
            child.set_used(0);
            if (node->getLevel() >= 0)
                child.push_back(node->getAllNodes()[node->getLevel()]);
            for (unsigned int i = 0; node->getChildren().size() >
                node->getAllNodes().size() && i < node->getChildren().size(); i++)
            {
                if (node->getNodesSet().find(node->getChildren()[i]) == node->getNodesSet().end())
                    child.push_back(node->getChildren()[i]);
            }
            lod_depth++;
            parseSceneManager(child, cam);
            lod_depth--;
            continue;
        }
        else if (type == ESNT_ANIMATED_MESH)
        {
            SP::SPMeshNode* node = static_cast<SP::SPMeshNode*>(List[i]);
            if (g_walk_found)
                g_walk_found->push_back({ node, 0 });
            else
                SP::addObject(node);
        }
        else if (STKParticle *node = type != ESNT_PARTICLE_SYSTEM ? NULL :
            dynamic_cast<STKParticle*>(List[i]))
        {
            node->updateAbsolutePosition();
            if (!isCulledPrecise(cam, List[i], irr_driver->getBoundingBoxesViz()))
            {
                if (g_walk_found)
                    g_walk_found->push_back({ node, 1 });
                else
                    CPUParticleManager::getInstance()->addParticleNode(node);
            }
            continue;
        }
        else if (scene::IBillboardSceneNode *node =
            type != ESNT_BILLBOARD && type != ESNT_TEXT ? NULL :
            dynamic_cast<scene::IBillboardSceneNode*>(List[i]))
        {
            node->updateAbsolutePosition();
            if (!isCulledPrecise(cam, List[i]))
            {
                if (g_walk_found)
                    g_walk_found->push_back({ node, 2 });
                else
                    CPUParticleManager::getInstance()->addBillboardNode(node);
            }
            continue;
        }
        else if (STKTextBillboard *tb = type != ESNT_UNKNOWN ? NULL :
            dynamic_cast<STKTextBillboard*>(List[i]))
        {
            tb->updateAbsolutePosition();
            if (!isCulledPrecise(cam, List[i], irr_driver->getBoundingBoxesViz()))
            {
                if (g_walk_found)
                    g_walk_found->push_back({ tb, 3 });
                else
                    TextBillboardDrawer::addTextBillboard(tb);
            }
            continue;
        }
        parseSceneManager(List[i]->getChildren(), cam);
    }
}

// ----------------------------------------------------------------------------
/** Walks one chunk of the root's children, recording what it finds. */
void DrawCalls::parseChunk(void* draw_calls, int chunk)
{
    g_walk_found = &g_walk_found_chunk[chunk];
    static_cast<DrawCalls*>(draw_calls)->parseSceneManager(g_walk_chunk[chunk],
                                                           g_walk_cam);
    g_walk_found = NULL;
}   // parseChunk

// ----------------------------------------------------------------------------
DrawCalls::DrawCalls()
{
    m_sync = 0;
    // With the SP, OnAnimate skips the static subtrees while they don't change
    scene::CEmptySceneNode::StaticNodeCheck = isStaticSPNode;
} //DrawCalls

// ----------------------------------------------------------------------------
DrawCalls::~DrawCalls()
{
    scene::CEmptySceneNode::StaticNodeCheck = NULL;
    CPUParticleManager::kill();
    STKParticle::destroyFlipsBuffer();
} //~DrawCalls

// ----------------------------------------------------------------------------
 /** Prepare draw calls before scene rendering
 */
void DrawCalls::prepareDrawCalls(scene::ICameraSceneNode *camnode)
{
    CPUParticleManager::getInstance()->reset();
    TextBillboardDrawer::reset();
    PROFILER_PUSH_CPU_MARKER("- culling", 0xFF, 0xFF, 0x0);
    SP::prepareDrawCalls();
    // The walk only reads the scene graph and updates the LOD nodes it meets,
    // each inside its own subtree. What it finds is added on the main thread
    // in the order of a single walk: SP::addObject (draw call maps, GL
    // uploads) only touches the found nodes and their mesh buffers.
    core::array<scene::ISceneNode*>& root =
        irr_driver->getSceneManager()->getRootSceneNode()->getChildren();
    if (root.size() >= (unsigned)WALK_CHUNKS && VitaJobs::isEnabled())
    {
        for (int c = 0; c < WALK_CHUNKS; c++)
        {
            g_walk_chunk[c].set_used(0);
            g_walk_found_chunk[c].clear();
        }
        for (unsigned i = 0; i < root.size(); i++)
            g_walk_chunk[i * WALK_CHUNKS / root.size()].push_back(root[i]);
        g_walk_cam = camnode;
        // Each chunk is added as soon as it is walked, while the other
        // cores walk the next ones
        const unsigned int batch =
            VitaJobs::start(parseChunk, this, WALK_CHUNKS);
        for (int c = 0; c < WALK_CHUNKS; c++)
        {
            VitaJobs::wait(batch, c);
            for (const WalkFound& f : g_walk_found_chunk[c])
            {
                if (f.m_kind == 0)
                    SP::addObject(static_cast<SP::SPMeshNode*>(f.m_node));
                else if (f.m_kind == 1)
                    CPUParticleManager::getInstance()->addParticleNode(
                        static_cast<STKParticle*>(f.m_node));
                else if (f.m_kind == 2)
                    CPUParticleManager::getInstance()->addBillboardNode(
                        static_cast<scene::IBillboardSceneNode*>(f.m_node));
                else
                    TextBillboardDrawer::addTextBillboard(
                        static_cast<STKTextBillboard*>(f.m_node));
            }
        }
        VitaJobs::finish();
    }
    else
        parseSceneManager(root, camnode);
    SP::handleDynamicDrawCall();
    SP::updateModelMatrix();
    PROFILER_POP_CPU_MARKER();

    PROFILER_PUSH_CPU_MARKER("- cpu particle generation", 0x2F, 0x1F, 0x11);
    CPUParticleManager::getInstance()->generateAll();
    PROFILER_POP_CPU_MARKER();

    // Add a 1 s timeout
    if (m_sync != 0)
    {
        PROFILER_PUSH_CPU_MARKER("- Sync Stall", 0xFF, 0x0, 0x0);
        GLenum reason = glClientWaitSync(m_sync, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
        if (reason != GL_ALREADY_SIGNALED)
        {
            do
            {
                reason = glClientWaitSync(m_sync, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000);
            }
            while (reason == GL_TIMEOUT_EXPIRED);
        }
        glDeleteSync(m_sync);
        m_sync = 0;
        PROFILER_POP_CPU_MARKER();
    }

    PROFILER_PUSH_CPU_MARKER("- particle and text billboard upload", 0x3F,
        0x03, 0x61);
    CPUParticleManager::getInstance()->uploadAll();
    TextBillboardDrawer::updateAll();
    PROFILER_POP_CPU_MARKER();

    PROFILER_PUSH_CPU_MARKER("- SP::upload instance and skinning matrices",
        0xFF, 0x0, 0xFF);
    SP::uploadAll();
    PROFILER_POP_CPU_MARKER();
}

#endif
