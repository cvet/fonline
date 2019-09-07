//////////////////////////////////////////////////////////////////////////////////
// SPARK Irrlicht Rendering library												//
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

#ifndef H_SPK_IRR_POINTRENDERER
#define H_SPK_IRR_POINTRENDERER

#include "Rendering/Irrlicht/SPK_IRR_Renderer.h"
#include "Extensions/Renderers/SPK_PointRenderBehavior.h"

namespace SPK
{
namespace IRR
{
	/**
	* @class IRRPointRenderer
	* @brief A Renderer drawing particles as points with Irrlicht
	*
	* Rendering can be done in 2 ways :
	* <ul>
	* <li>SPK::POINT_TYPE_SQUARE : standard points</li>
	* <li>SPK::POINT_TYPE_SPRITE : point sprites (must be supported by the hardware)</li>
	* </ul>
	* Note that SPK::POINT_TYPE_CIRCLE cannot be set with this renderer.<br>
	* <br>
	* Regarding the size of the rendered point, they are dependant of the Irrlicht settings.<br>
	* Basically size of the points is neither in pixels nor in the universe unit.<br>
	* Moreover, points are scaling with the distance but are rapidly clamped.<br>
	* So Irrlicht does not handle point size very well at the moment, maybe it will do better in the future.<br>
	* In that case, this renderer will become very useful.
	*/
	class SPK_IRR_PREFIX IRRPointRenderer :	public IRRRenderer,
											public PointRenderBehavior
	{
	public :

		/**
		* @brief Creates and registers a new IRRPointRenderer
		* @param d : the Irrlicht device
		* @param size : the size of the points
		* @return A new registered IRRPointRenderer
		*/
		static  Ref<IRRPointRenderer> create(irr::IrrlichtDevice* d = NULL,float screenSize = 1.0f);

		// Reimplemented from PointRenderBehavior
		virtual bool setType(PointType type);
		virtual  void setScreenSize(float screenSize);
		virtual bool enableWorldSize(bool worldSizeEnabled);

		//////////////
		// Textures //
		//////////////

		/**
		* @brief Sets the texture to map on particles
		* 
		* Note that this only works with points being rendered as SPK::POINT_SPRITE
		*
		* @param texture : the texture to set
		*/
		void setTexture(irr::video::ITexture* texture);

		/**
		* @brief Gets the texture of this renderer
		* @return the texture of this renderer
		*/
		irr::video::ITexture* getTexture() const;

		/**
		* @brief Gets the material texture layer
		* @return the material texture layer
		*/
		irr::video::SMaterialLayer& getMaterialLayer();

		/**
		* @brief Gets the material texture layer in a constant way
		* @return the material texture layer
		*/
		const irr::video::SMaterialLayer& getMaterialLayer() const;

	public :
		spark_description(IRRPointRenderer, IRRRenderer)
		(
		);

	private :

		static const size_t NB_INDICES_PER_PARTICLE = 1;
		static const size_t NB_VERTICES_PER_PARTICLE = 1;

		IRRPointRenderer(irr::IrrlichtDevice* d = NULL,float screenSize = 1.0f);
		IRRPointRenderer(const IRRPointRenderer& renderer);

		virtual RenderBuffer* attachRenderBuffer(const Group& group) const;

		virtual void render(const Group& group,const DataSet* dataSet,RenderBuffer* renderBuffer) const;
		virtual void computeAABB(Vector3D& AABBMin,Vector3D& AABBMax,const Group& group,const DataSet* dataSet) const;
	};

	inline Ref<IRRPointRenderer> IRRPointRenderer::create(irr::IrrlichtDevice* d,float screenSize)
	{
		return SPK_NEW(IRRPointRenderer,d,screenSize);
	}

	inline void IRRPointRenderer::setScreenSize(float screenSize)
	{
		material.Thickness = this->screenSize = screenSize;
	}
	
	inline void IRRPointRenderer::setTexture(irr::video::ITexture* texture)
	{
		material.TextureLayer[0].Texture = texture;
	}

	inline irr::video::ITexture* IRRPointRenderer::getTexture() const
	{
		return material.TextureLayer[0].Texture;
	}

	inline irr::video::SMaterialLayer& IRRPointRenderer::getMaterialLayer()
	{
		return material.TextureLayer[0];
	}
		
	inline const irr::video::SMaterialLayer& IRRPointRenderer::getMaterialLayer() const
	{
		return material.TextureLayer[0];
	}
}}

#endif
