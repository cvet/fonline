//////////////////////////////////////////////////////////////////////////////////
// SPARK particle engine														//
// Copyright (C) 2008-2013 - Julien Fryer - julienfryer@gmail.com				//
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

#ifndef H_SPK_GL_POINTRENDERER
#define H_SPK_GL_POINTRENDERER

#include "SPK_GL_Renderer.h"
#include "Extensions/Renderers/SPK_PointRenderBehavior.h"

namespace SPK
{
namespace GL
{
	/**
	* @class GLPointRenderer
	* @brief A Renderer drawing drawing particles as OpenGL points
	*
	* OpenGL points can be configured to render them in 3 different ways :
	* <ul>
	* <li>SPK::POINT_SQUARE : standard OpenGL points</li>
	* <li>SPK::POINT_CIRCLE : antialiased OpenGL points</li>
	* <li>SPK::POINT_SPRITE : OpenGL point sprites (must be supported by the hardware)</li>
	* </ul>
	* Moreover, points size can either be defined in screen space (in pixels) or in the universe space (must be supported by the hardware).
	* The advantage of the universe space is that points size on the screen will be dependant to their distance from the camera, whereas in screen space
	* all points will have the same size on the screen no matter what their distance from the camera is.
	* <br>
	* This renderer do not use any parameters of particles.
	*/
	class SPK_GL_PREFIX GLPointRenderer :	public GLRenderer,
											public PointRenderBehavior
	{
        SPK_IMPLEMENT_OBJECT(GLPointRenderer);

	public :

		/**
		* @brief Creates and registers a new GLPointRenderer
		* @param size : the size of the points
		* @return A new registered GLPointRenderer
		*/
		static  Ref<GLPointRenderer> create(float screenSize = 1.0f);

		bool setType(PointType type) override;

		/**
		* @brief Sets the way size of points is computed in this GLPointRenderer
		*
		* if universe size is used (true), the extension is checked.<br>
		* if universe size is not supported by the hardware, false is returned and nothing happens.<br>
		* <br>
		* If world size is enabled, the static method setPixelPetUnit(float,int) must be called to set the conversion between pixels and world units.
		*
		* @param worldSizeEnabled : true to enable universe size, false to use screen size
		* @return true the type of size can be set, false otherwise
		*/
		bool enableWorldSize(bool worldSizeEnabled) override;

		/**
		* @brief Sets the texture of this GLPointRenderer
		*
		* Note that the texture is only used if point sprites are used (type is set to SPK::POINT_SPRITE)
		*
		* @param textureIndex : the index of the OpenGL texture of this GLPointRenderer
		*/
		void setTexture(GLuint textureIndex);

		/**
		* @brief Gets the texture of this GLPointRenderer
		* @return the texture of this GLPointRenderer
		*/
		GLuint getTexture() const;

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

		static bool isPointSpriteSupported();
		static bool isWorldSizeSupported();

	private :

		static GLboolean* const SPK_GL_POINT_SPRITE_EXT;
		static GLboolean* const SPK_GL_POINT_PARAMETERS_EXT;

		static float pixelPerUnit;

		GLuint textureIndex;

		GLPointRenderer(float screenSize = 1.0f);
		GLPointRenderer(const GLPointRenderer& renderer);

		void render(const Group& group,const DataSet* dataSet,RenderBuffer* renderBuffer) const override;
		void computeAABB(Vector3D& AABBMin,Vector3D& AABBMax,const Group& group,const DataSet* dataSet) const override;
	};

	inline GLPointRenderer::GLPointRenderer(float screenSize) :
		GLRenderer(false),
		PointRenderBehavior(POINT_TYPE_SQUARE,screenSize),
		textureIndex(0)
	{}

	inline GLPointRenderer::GLPointRenderer(const GLPointRenderer& renderer) :
		GLRenderer(renderer),
		PointRenderBehavior(renderer),
		textureIndex(renderer.textureIndex)
	{}

	inline Ref<GLPointRenderer> GLPointRenderer::create(float screenSize)
	{
		return SPK_NEW(GLPointRenderer,screenSize);
	}
		
	inline void GLPointRenderer::setTexture(GLuint textureIndex)
	{
		this->textureIndex = textureIndex;
	}

	inline GLuint GLPointRenderer::getTexture() const
	{
		return textureIndex;
	}

	inline void GLPointRenderer::setPixelPerUnit(float fovy,int screenHeight)
	{
		// the pixel per unit is computed for a distance from the camera of screenHeight
		pixelPerUnit = screenHeight / (2.0f * tan(fovy * 0.5f));
	}
}}

#endif
