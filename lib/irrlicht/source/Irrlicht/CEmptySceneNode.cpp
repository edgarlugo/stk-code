// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CEmptySceneNode.h"
#include "ISceneManager.h"

namespace irr
{
namespace scene
{

//! constructor
CEmptySceneNode::CEmptySceneNode(ISceneNode* parent, ISceneManager* mgr, s32 id)
: ISceneNode(parent, mgr, id)
{
	#ifdef _DEBUG
	setDebugName("CEmptySceneNode");
	#endif

	setAutomaticCulling(scene::EAC_OFF);
}


//! pre render event
void CEmptySceneNode::OnRegisterSceneNode()
{
	// Nothing to draw: registering this node only cost a culling test, a slot in the
	// sorted solid node list and an empty render() call. Tracks group every library
	// object under an empty node, so there can be thousands of them.
	ISceneNode::OnRegisterSceneNode();
}


bool (*CEmptySceneNode::StaticNodeCheck)(ISceneNode* node) = 0;

static bool isStaticSubtree(ISceneNode* node)
{
	const core::array<ISceneNode*>& children = node->getChildren();
	for (unsigned i = 0; i < children.size(); ++i)
	{
		if (!children[i]->getAnimators().empty() ||
			!CEmptySceneNode::StaticNodeCheck(children[i]) ||
			!isStaticSubtree(children[i]))
			return false;
	}
	return true;
}


void CEmptySceneNode::OnAnimate(u32 timeMs)
{
	if (!IsVisible)
		return;

	if (StaticNodeCheck)
	{
		if (SubtreeChanged & 2)
		{
			StaticSubtree = Animators.empty() && isStaticSubtree(this);
			SubtreeChanged &= ~2;
		}
		if (StaticSubtree && !(SubtreeChanged & 1) &&
			!(Parent && Parent->getUpdatedAbsTrans()))
		{
			UpdatedAbsTrans = false;
			return;
		}
	}

	ISceneNode::OnAnimate(timeMs);
	SubtreeChanged &= ~1;
	AnimateCount++;
}


//! render
void CEmptySceneNode::render()
{
	// do nothing
}


//! returns the axis aligned bounding box of this node
const core::aabbox3d<f32>& CEmptySceneNode::getBoundingBox() const
{
	return Box;
}


//! Creates a clone of this scene node and its children.
ISceneNode* CEmptySceneNode::clone(ISceneNode* newParent, ISceneManager* newManager)
{
	if (!newParent)
		newParent = Parent;
	if (!newManager)
		newManager = SceneManager;

	CEmptySceneNode* nb = new CEmptySceneNode(newParent,
		newManager, ID);

	nb->cloneMembers(this, newManager);
	nb->Box = Box;

	if ( newParent )
		nb->drop();
	return nb;
}


} // end namespace scene
} // end namespace irr
