//////////////////////////////////////////////////////////////////////////////////
// SPARK particle engine														//
// Copyright (C) 2008-2013														//
// Julien Fryer - julienfryer@gmail.com											//
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

#ifndef H_SPK_IRR_OBJECT
#define H_SPK_IRR_OBJECT

#include "Rendering/Irrlicht/SPK_IRR_DEF.h"

namespace irr
{
namespace scene
{
	/** @brief A wrapper class to be able to transform spark object using Irrlicht scene graph. */
	template<typename T>
	class CSPKObjectNode : public ISceneNode 
	{
	public : 

		CSPKObjectNode<T>(const SPK::Ref<T>& object,ISceneNode* parent,ISceneManager* mgr,s32 id=-1) :
			ISceneNode(parent,mgr,id),
			SPKObject(object) {}

		CSPKObjectNode<T>(const CSPKObjectNode<T>& object,ISceneNode* newParent = NULL,ISceneManager* newManager = NULL) :
			ISceneNode(NULL,NULL)
		{
			SPKObject = SPK::SPKObject::copy(object.SPKObject);

			if (newParent == NULL)
				newParent = object.Parent;
			if (newManager == NULL)
				newManager = object.SceneManager;

			setParent(newParent);		
			cloneMembers(const_cast<CSPKObjectNode<T>*>(&object),newManager);
		}

		virtual CSPKObjectNode<T>* clone(ISceneNode* newParent = NULL,ISceneManager* newManager = NULL)
		{
			CSPKObjectNode<T>* o = new CSPKObjectNode<T>(*this,newParent,newManager);
			if (Parent != NULL)
				o->drop();
			return o;
		}

		const SPK::Ref<T>& getSPKObject() { return SPKObject; }

		virtual void updateAbsolutePosition()
		{
			ISceneNode::updateAbsolutePosition();
			SPKObject->getTransform().set(AbsoluteTransformation.pointer());
		}

		virtual void render() {} // Does nothing
		virtual const core::aabbox3d<f32>& getBoundingBox() const { return dummyBox; }

	private :

		SPK::Ref<T> SPKObject;
		core::aabbox3d<f32> dummyBox;
	};

	typedef CSPKObjectNode<SPK::Emitter> CSPKEmitterNode; /**< @brief CSPKObjectNode for emitters */
}}

#endif

