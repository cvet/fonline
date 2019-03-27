//////////////////////////////////////////////////////////////////////////////////
// SPARK particle engine														//
// Copyright (C) 2008-2013 - Julien Fryer - julienfryer@gmail.com				//
//                           foulon matthieu - stardeath@wanadoo.fr				//
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

#ifndef H_SPK_DX9_POINTRENDERER
#define H_SPK_DX9_POINTRENDERER

#include "Rendering/DX9/SPK_DX9_Renderer.h"
#include "Extensions/Renderers/SPK_PointRenderBehavior.h"

namespace SPK
{
namespace DX9
{
	/**
	* @class DX9PointRenderer
	* @brief A Renderer drawing drawing particles as DirectX 9.0 point sprites
	*
	* Moreover, points size can either be defined in screen space (in pixels) or in the universe space (must be supported by the hardware).
	* The advantage of the universe space is that points size on the screen will be dependant to their distance from the camera, whereas in screen space
	* all points will have the same size on the screen no matter what their distance from the camera is.
	* <br>
	* This renderer do not use any parameters of particles.
	*/
	class SPK_DX9_PREFIX DX9PointRenderer :	public DX9Renderer,
											public PointRenderBehavior
	{
	public :

		/**
		* @brief Creates and registers a new DX9PointRenderer
		* @param size : the size of the points
		* @return A new registered DX9PointRenderer
		*/
		static  DX9PointRenderer* create(float screenSize = 1.0f);

		virtual bool setType(PointType type);

		/**
		* @brief Sets the way size of points is computed in this DX9PointRenderer
		*
		* if universe size is used (true), the extension is checked.<br>
		* if universe size is not supported by the hardware, false is returned and nothing happens.<br>
		* <br>
		* If world size is enabled, the static method setPixelPetUnit(float,int) must be called to set the conversion between pixels and world units.
		*
		* @param worldSizeEnabled : true to enable universe size, false to use screen size
		* @return true the type of size can be set, false otherwise
		*/
		bool enableWorldSize(bool worldSizeEnabled);

		/**
		* @brief Sets the texture of this DX9PointRenderer
		*
		* Note that the texture is only used if point sprites are used (type is set to SPK::POINT_SPRITE)
		*
		* @param textureIndex : the index of the DirectX 9.0 texture of this DX9PointRenderer
		*/
		void setTexture(LPDIRECT3DTEXTURE9 textureIndex);

		/**
		* @brief Gets the texture of this DX9PointRenderer
		* @return the texture of this DX9PointRenderer
		*/
		LPDIRECT3DTEXTURE9 getTexture() const;

		///////////////////
		// Point Sprites //
		///////////////////

		/**
		* @brief Computes a conversion ratio between pixels and universe units
		*
		* This method must be called when using GLPointRenderer with world size enabled.<br>
		* It allows to well transpose world size to pixel size by setting the right OpenGL parameters.<br>
		* <br>
		* Note that fovy can be replaced by fovx if screenHeight is replaced by screenWidth.
		*
		* @param fovy : the field of view in the y axis in radians
		* @param screenHeight : the height of the viewport in pixels
		*/
		static  void setPixelPerUnit(float fovy,int screenHeight);

	public :
		spark_description(DX9PointRenderer, DX9Renderer)
		(
		);

	protected:

		//////////////////////
		// Point Parameters //
		//////////////////////

		/**
		* @brief Enables the use of point parameters
		*
		* This method will set the right point parameters to get the desired point size.<br>
		* <br>
		* It can either be used to have points size function of the distance to the camera (is distance is true)
		* or only to allow bigger range for point sizes (if distance is false).
		* <br>
		* Note that if distance is set to true setPixelPerUnit(float,int) must be call once before.
		* <br>
		* Note that before calling this method, the user must ensure that the point parameters extension is loaded.
		*
		* @param size : the size of the point
		* @param distance : true to enable the modification of the size function of the distance, false not to.
		*/
		static void enablePointParameter(float size,bool distance);

	private :

		LPDIRECT3DTEXTURE9 textureIndex;

		DX9PointRenderer(float screenSize = 1.0f);
		DX9PointRenderer(const DX9PointRenderer& renderer);

		virtual RenderBuffer* attachRenderBuffer(const Group& group) const;

		virtual void render(const Group& group,const DataSet* dataSet,RenderBuffer* renderBuffer) const;
		virtual void computeAABB(Vector3D& AABBMin,Vector3D& AABBMax,const Group& group,const DataSet* dataSet) const;

		//////////////////////
		// Point Parameters //
		//////////////////////

		static const float POINT_SIZE_CURRENT;
		static const float POINT_SIZE_MIN;
		static const float POINT_SIZE_MAX;

		static float pixelPerUnit;

		static const float QUADRATIC_SCREEN[3];
	};

	inline DX9PointRenderer::DX9PointRenderer(float screenSize) :
		DX9Renderer(false),
		PointRenderBehavior(POINT_TYPE_SQUARE, screenSize),
		textureIndex(0)
	{}

	inline DX9PointRenderer::DX9PointRenderer(const DX9PointRenderer& renderer) :
		DX9Renderer(renderer),
		PointRenderBehavior(renderer),
		textureIndex(renderer.textureIndex)
	{}

	inline DX9PointRenderer* DX9PointRenderer::create(float screenSize)
	{
		return SPK_NEW(DX9PointRenderer,screenSize);
	}
		
	inline void DX9PointRenderer::setTexture(LPDIRECT3DTEXTURE9 textureIndex)
	{
		this->textureIndex = textureIndex;
	}

	inline LPDIRECT3DTEXTURE9 DX9PointRenderer::getTexture() const
	{
		return textureIndex;
	}

	inline void DX9PointRenderer::setPixelPerUnit(float fovy,int screenHeight)
	{
		// the pixel per unit is computed for a distance from the camera of screenHeight
		pixelPerUnit = screenHeight / (2.0f * std::tan(fovy * 0.5f));
	}
}}

#endif
