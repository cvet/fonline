//////////////////////////////////////////////////////////////////////////////////
// SPARK particle engine														//
// Copyright (C) 2008-2013														//
// Julien Fryer - julienfryer@gmail.com											//
// Thibault Lescoat - info-tibo <at> orange <dot> fr							//
//																				//
// This software is provided 'as-is', without any express or implied			//
// warranty.  In no event will the authors be held liable for any damages		//
// arising from the use of this software.										//
//																				//
// Permission is granted to anyone to use this software for any purpose,		//
// including commercial applications, and to alter it and redistribute it		//
// freely, subject to the following restrictions:								//
//																				//
// 1. The origin of this software must not be misrepresented; you must not		//
//    claim that you wrote the original software. If you use this software		//
//    in a product, an acknowledgment in the product documentation would be		//
//    appreciated but is not required.											//
// 2. Altered source versions must be plainly marked as such, and must not be	//
//    misrepresented as being the original software.							//
// 3. This notice may not be removed or altered from any source distribution.	//
//////////////////////////////////////////////////////////////////////////////////

#include <SPARK_Core.h>
#include "Rendering/Irrlicht/CSPKParticleSystemNode.h"

namespace irr
{
namespace scene
{
	CSPKParticleSystemNode::CSPKParticleSystemNode(
		ISceneNode* parent,
		ISceneManager* mgr,
		bool worldTransformed,
		bool initialize,
		s32 id) :
			irr::scene::ISceneNode(parent,mgr,id),
			SPKSystem(SPK::System::create(initialize)),
			worldTransformed(worldTransformed),
			onlyWhenVisible(false),
			alive(true),
			lastUpdatedTime(0)
    {
		SPKSystem->enableAABBComputation(true); // as culling is enabled by default in Irrlicht

		if (parent != NULL && !SPKSystem->isInitialized())
			SPK_LOG_WARNING("CSPKParticleSystemNode(ISceneNode*,ISceneManager*,bool,bool,s32) - A uninitialized system should have a NULL parent");
	}

	CSPKParticleSystemNode::CSPKParticleSystemNode(
		const SPK::Ref<SPK::System>& system,
		ISceneNode* parent,
		ISceneManager* mgr,
		bool worldTransformed,
		s32 id) :
			irr::scene::ISceneNode(parent,mgr,id),
			SPKSystem(system),
			worldTransformed(worldTransformed),
			onlyWhenVisible(false),
			alive(true),
			lastUpdatedTime(0)
    {
		if (SPKSystem == NULL)
		{
			SPK_LOG_ERROR("CSPKParticleSystemNode(const Ref<System>&,ISceneNode*,ISceneManager*,bool,s32) - The underlying system must not be NULL");
		}
		else if (parent != NULL && !SPKSystem->isInitialized())
		{
			SPK_LOG_WARNING("CSPKParticleSystemNode(const Ref<System>&,ISceneNode*,ISceneManager*,bool,s32) - A uninitialized system should have a NULL parent");
		}
	}

	CSPKParticleSystemNode::CSPKParticleSystemNode(
		const CSPKParticleSystemNode& system,
		ISceneNode* newParent,
		ISceneManager* newManager) :
			ISceneNode(NULL,NULL),
			worldTransformed(system.worldTransformed),
			onlyWhenVisible(system.onlyWhenVisible),
			alive(system.alive),
			lastUpdatedTime(0)
	{
		SPKSystem = SPK::SPKObject::copy(system.SPKSystem);

		if (newParent == NULL)
			newParent = system.Parent;
		if (newManager == NULL)
			newManager = system.SceneManager;

		setParent(newParent);
		
		if (getParent() != NULL && !SPKSystem->isInitialized())
			SPK_LOG_WARNING("CSPKParticleSystemNode(const CSPKParticleSystemNode&,ISceneNode*,ISceneManager*) - A uninitialized system should have a NULL parent");
		
		cloneMembers(const_cast<CSPKParticleSystemNode*>(&system),newManager);
	}

	CSPKParticleSystemNode* CSPKParticleSystemNode::clone(ISceneNode* newParent,ISceneManager* newManager)
	{
		CSPKParticleSystemNode* nb = new CSPKParticleSystemNode(*this,newParent,newManager);
		if (Parent != NULL)
			nb->drop();
		return nb;
	}

	const core::aabbox3d<f32>& CSPKParticleSystemNode::getBoundingBox() const
    {
		BBox.MaxEdge = SPK::IRR::spk2irr(SPKSystem->getAABBMax());
		BBox.MinEdge = SPK::IRR::spk2irr(SPKSystem->getAABBMin());
        return BBox;
    }

	void CSPKParticleSystemNode::render()
	{
        SceneManager->getVideoDriver()->setTransform(video::ETS_WORLD,AbsoluteTransformation);
        SPKSystem->renderParticles();
	}

	void CSPKParticleSystemNode::OnRegisterSceneNode()
	{
		if (IsVisible && alive)
			SceneManager->registerNodeForRendering(this,irr::scene::ESNRP_TRANSPARENT_EFFECT); // Draws in transparent effect pass (may be optimized)

       ISceneNode::OnRegisterSceneNode();
	}

	void CSPKParticleSystemNode::OnAnimate(u32 timeMs)
	{
		ISceneNode::OnAnimate(timeMs);

		if (lastUpdatedTime == 0)
			lastUpdatedTime = timeMs;

        if (!onlyWhenVisible || IsVisible)
		{
			updateCameraPosition();

			if (!SPKSystem->isAABBComputationEnabled() && AutomaticCullingState != EAC_OFF)
			{
				SPK_LOG_INFO("CSPKParticleSystemNode::OnAnimate(u32) - The culling is activated for the system but not the bounding box computation - BB computation is enabled");
				SPKSystem->enableAABBComputation(true);
			}

			alive = SPKSystem->updateParticles((timeMs - lastUpdatedTime) * 0.001f);
		}

        lastUpdatedTime = timeMs;
	}

	void CSPKParticleSystemNode::updateAbsolutePosition()
	{
		ISceneNode::updateAbsolutePosition();

		if (worldTransformed)
		{
			SPKSystem->getTransform().set(AbsoluteTransformation.pointer());
			AbsoluteTransformation.makeIdentity();
		}
	}

	void CSPKParticleSystemNode::updateCameraPosition() const
	{
		for (size_t i = 0; i < SPKSystem->getNbGroups(); ++i)
		{
			if (SPKSystem->getGroup(i)->isDistanceComputationEnabled())
			{
				core::vector3df pos = SceneManager->getActiveCamera()->getAbsolutePosition();
				if (!worldTransformed)
				{
					core::matrix4 invTransform;
					AbsoluteTransformation.getInversePrimitive(invTransform);
					invTransform.transformVect(pos);
				}
				SPKSystem->setCameraPosition(SPK::IRR::irr2spk(pos));
				break;
			}
		}
	}
}}
