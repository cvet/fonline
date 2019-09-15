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

#ifndef H_SPK_IRR_SYSTEM
#define H_SPK_IRR_SYSTEM

#include "Rendering/Irrlicht/SPK_IRR_DEF.h"

namespace irr
{
namespace scene
{
	/**
	* @brief A particle system adapted to Irrlicht
	*
	* This particle system is also a scene node, so you can apply transformations on it.<br>
	* Be aware that only group using an Irrlicht renderer will work correctly.<br>
	* <br>
	* The particle system is rendered automatically in the render process of Irrlicht.<br>
    * If specified it can automatically update all particles (auto-update) on animation pass.<br>
    * It is possible to specify if the system should update particles when not visible.<br>
    * By default, auto-update is enabled only when visible.<br>
	* <br>
	* Note also that an CSPKParticleSystemNode takes care of the camera position automatically when distance computation is enabled
	* on one of its Group. Therefore there is no need to call System::setCameraPosition(Vector3D).
	*/
	class SPK_IRR_PREFIX CSPKParticleSystemNode : public ISceneNode
	{
	SPK_IMPLEMENT_SYSTEM_WRAPPER

	public :

		//////////////////
		// Constructors //
		//////////////////

		/**
		* @brief Constructor of CSPKParticleSystemNode
		* @param parent : the parent node of the particle system
		* @param mgr : the Irrlicht scene manager
		* @param worldTransformed : true to emit particles in world, false to emit them localy
		* @param initialized : tru to initialize the system, false not to
		* @param id : the ID of the node
		*/
		CSPKParticleSystemNode(ISceneNode* parent,ISceneManager* mgr,bool worldTransformed = true,bool initialize = true,s32 id=-1);

		/**
		* @brief Constructor of CSPKParticleSystemNode from a System
		* This should be used with care. The system must not be null and not already wrapped
		* @param system : The system to wrap for a use in Irrlicht
		* @param parent : the parent node of the particle system
		* @param mgr : the Irrlicht scene manager
		* @param worldTransformed : true to emit particles in world, false to emit them localy
		* @param id : the ID of the node
		*/
		CSPKParticleSystemNode(const SPK::Ref<SPK::System>& system,ISceneNode* parent,ISceneManager* mgr,bool worldTransformed = true,s32 id=-1);

		/**
		* @brief Copy constructor of CSPKParticleSystemNode
		*/
		CSPKParticleSystemNode(const CSPKParticleSystemNode& system,ISceneNode* newParent = NULL,ISceneManager* newManager = NULL);

		virtual CSPKParticleSystemNode* clone(ISceneNode* newParent = NULL,ISceneManager* newManager = NULL);



		/**
		* @brief Enables or disables update when the system is not visible
        * @param onlyWhenVisible : True to perform update only if node is visible
		*/
		void setUpdateOnlyWhenVisible(bool onlyWhenVisible);

		/**
		* @brief Returns true if update is performed only when visible
		* @return True if particles are updated only if the scene node is visible (use setVisible() to change visibility).
		*/
        bool isUpdatedOnlyWhenVisible() const;

		/**
		* @brief Tells whether this system is world transformed or not
		*
		* If a system is transformed in the world, only its emitter and zones will be transformed.<br>
		* The emitted particles will remain independent from the system transformation.<br>
		* <br>
		* If it is transformed locally, the emitted particles will be transformed as well.
		*
		* @return true if this system is world transformed, false if not
		*/
		bool isWorldTransformed() const;

		/**
		* @brief Tells whether the system is alive or not
		*
		* This method will return false if :
		* <ul>
		* <li>There is no more active particles in the system</li>
		* <li>All the emitters in the system are sleeping</li>
		* </ul>
		* The CSPKParticleSystemNode provides an accessor for this as the update can be done within the Irrlicht scene manager.
		* In that case the user cannot get the returned value of updateParticles(float).
		*
		* @return true if the system is still alive, false otherwise
		*/
		bool isAlive() const;

		/** 
		* @brief Gets the bounding box
		*
		* Note that the returned bounding box is invalid if aabb computation is disabled.
		*
		* @return the bounding box containing all particles
		*/
		virtual const core::aabbox3d<f32>& getBoundingBox() const;

		/** 
		* @brief Renders the particles in the system 
		* (Reimplementation of the irr::scene::SceneNode render method.)
		*/
		virtual void render();

		/** @brief This method is called just before the rendering process of the whole scene */
		virtual void OnRegisterSceneNode();

		/** 
		* @brief This method is called just before rendering the whole scene 
		*
		* It should be called only by engine.
		*
		* @param timeMs : Current time in milliseconds
		**/
        virtual void OnAnimate(u32 timeMs);

		/** @brief Updates the absolute transformation of this Irrlicht Node */
		virtual void updateAbsolutePosition();
	
	private :

		bool onlyWhenVisible;

		const bool worldTransformed;

		bool alive;

        mutable core::aabbox3d<f32> BBox;
		mutable u32 lastUpdatedTime;

		// This sets the right camera position if distance computation is enabled for a group of the system
		void updateCameraPosition() const;
	};

	inline void CSPKParticleSystemNode::setUpdateOnlyWhenVisible(bool onlyWhenVisible)
	{
		this->onlyWhenVisible = onlyWhenVisible;
	}

    inline bool CSPKParticleSystemNode::isUpdatedOnlyWhenVisible() const
	{
		return onlyWhenVisible;
	}

	inline bool CSPKParticleSystemNode::isWorldTransformed() const
	{
		return worldTransformed;
	}

	inline bool CSPKParticleSystemNode::isAlive() const
	{
		return alive;
	}
}}

#endif
