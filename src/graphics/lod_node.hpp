//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2011-2015 Marianne Gagnon
//  based on code Copyright 2002-2010 Nikolaus Gebhardt
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

#ifndef HEADER_LOD_NODE_HPP
#define HEADER_LOD_NODE_HPP

#include <aabbox3d.h>
#include <matrix4.h>
#include <ISceneNode.h>
#include <vector>
#include <string>

namespace irr
{
    namespace scene { class ISceneManager; class ISceneNode; }
}
using namespace irr;

#include <set>
#include <memory>

namespace irr
{
    namespace scene
    {
        const int ESNT_LOD_NODE = MAKE_IRR_ID('l','o','d','n');
    }
}

/**
 * \brief manages level-of-detail
 * \ingroup graphics
 */
namespace irr { namespace scene { class ICameraSceneNode; } }

class LODNode : public scene::ISceneNode
{
private:
    core::matrix4 RelativeTransformationMatrix;
    core::aabbox3d<f32> Box;

    std::vector<int> m_detail;
    std::vector<irr::scene::ISceneNode*> m_nodes;

    std::set<scene::ISceneNode*> m_nodes_set;

    std::string m_group_name;

    /** The normal level of detail can be overwritten. If
     *  m_forced_lod is >=0, only this level is be used. */
    int m_forced_lod;

    int m_current_level;
    bool m_current_level_dirty;

    /** Level animated by the last OnAnimate (fixed pipeline only, -1 if none) and its time */
    int m_animated_level;
    u32 m_animated_time;

    // Distance below which switching between two levels of detail is avoided
    int m_min_switch_distance;

    // Area of the bounding box (for autoLOD computation)
    float m_area;

    bool m_update_box_every_frame;
    bool m_lod_distances_updated;

    /** Fixed pipeline: box of the levels in world space, made for the
     *  absolute transformation m_view_box_matrix, and the number of children
     *  m_view_box_children it was checked with (-1: to check). */
    core::aabbox3d<f32> m_view_box;
    core::matrix4 m_view_box_matrix;
    int m_view_box_children;
    bool m_view_box_static;
    bool m_view_box_valid;

public:

    LODNode(std::string group_name, scene::ISceneNode* parent, scene::ISceneManager* mgr, s32 id=-1);
    virtual     ~LODNode();

    //! returns the axis aligned bounding box of this node
    virtual const core::aabbox3d<f32>& getBoundingBox() const { return Box; }

    int getLevel();

    void updateVisibility();

    /*
    //! Returns a reference to the current relative transformation matrix.
    //! This is the matrix, this scene node uses instead of scale, translation
    //! and rotation.
    virtual core::matrix4& getRelativeTransformationMatrix() { return RelativeTransformationMatrix; }

    //! Returns the relative transformation of the scene node.
    virtual core::matrix4 getRelativeTransformation() const { return RelativeTransformationMatrix; }
    */

    /**
      * Adds a node associated with a level of detail.
      * \note The LOD levels must be added in ascending order.
      * \param level Distance (number of units) from which this level of detail kicks in
      * \param node  The node to show at this level
      * \param reparent If true, node will be removed from its current parent first
      */
    void add(int level, scene::ISceneNode* node, bool reparent);

    /**
      * This method can be used to automatically compute LoD level
      */
    void autoComputeLevel(float scale);

    void forceLevelOfDetail(int n);
    // ------------------------------------------------------------------------
    bool isOutOfView(const scene::ICameraSceneNode* cam);
    // ------------------------------------------------------------------------
    /** With the shader-based renderer, animates the level to draw if
     *  OnAnimate animated another one, as OnRegisterSceneNode does with the
     *  fixed pipeline. */
    void animateDrawnLevel()
    {
        int level = getLevel();
        if (level >= 0 && level != m_animated_level)
        {
            m_nodes[level]->OnAnimate(m_animated_time);
            m_animated_level = level;
        }
    }

    /** Squared distances up to which each level is shown */
    const std::vector<int>& getDetail() const { return m_detail; }
    void setDetail(const std::vector<int>& detail) { m_detail = detail; }
    /** A level shown only closer than this squared distance is skipped */
    int getMinSwitchDistance() const { return m_min_switch_distance; }

    /** Get the highest level of detail node */
    scene::ISceneNode* getFirstNode()
    {
        if (m_nodes.size() > 0) return m_nodes[0];
        else                    return NULL;
    }

    std::vector<scene::ISceneNode*>& getAllNodes() { return m_nodes; }
    std::set<scene::ISceneNode*>& getNodesSet() { return m_nodes_set; }

    //! OnAnimate() is called just before rendering the whole scene.
    /** This method will be called once per frame, independent
        of whether the scene node is visible or not. */
    virtual void OnAnimate(u32 timeMs);

    virtual void OnRegisterSceneNode();
    virtual void render();

    virtual scene::ESCENE_NODE_TYPE getType() const { return (scene::ESCENE_NODE_TYPE)scene::ESNT_LOD_NODE; }

    const std::string& getGroupName() const { return m_group_name; }
};

#endif
