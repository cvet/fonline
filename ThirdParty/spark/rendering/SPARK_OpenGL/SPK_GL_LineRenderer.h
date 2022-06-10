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

#ifndef H_SPK_GL_LINERENDERER
#define H_SPK_GL_LINERENDERER

#include "SPK_GL_Renderer.h"
#include "Extensions/Renderers/SPK_LineRenderBehavior.h"
#include "SPK_GL_Buffer.h"

namespace SPK
{
namespace GL
{
	/**
	* @class GLLineRenderer
	* @brief A Renderer drawing particles as OpenGL lines
	*
	* The length of the lines is function of the Particle velocity and is defined in the universe space
	* while the width is fixed and defines in the screen space (in pixels).<br>
	* <br>
	* Below are the parameters of Particle that are used in this Renderer (others have no effects) :
	* <ul>
	* <li>SPK::PARAM_RED</li>
	* <li>SPK::PARAM_GREEN</li>
	* <li>SPK::PARAM_BLUE</li>
	* <li>SPK::PARAM_ALPHA (only if blending is enabled)</li>
	* </ul>
	*/
	class SPK_GL_PREFIX GLLineRenderer : public GLRenderer, public LineRenderBehavior
	{
        SPK_IMPLEMENT_OBJECT(GLLineRenderer);

	public :

		/**
		* @brief Creates and registers a new GLLineRenderer
		* @param length : the length multiplier of the lines
		* @param width : the width of the lines (in screen space)
		* @return A new GLLineRenderer
		*/
		static  Ref<GLLineRenderer> create(float length = 1.0f,float width = 1.0f);

	private :

		GLLineRenderer(float length = 1.0f,float width = 1.0f);
		GLLineRenderer(const GLLineRenderer& renderer);

		RenderBuffer* attachRenderBuffer(const Group& group) const override;

		void render(const Group& group,const DataSet* dataSet,RenderBuffer* renderBuffer) const override;
		void computeAABB(Vector3D& AABBMin,Vector3D& AABBMax,const Group& group,const DataSet* dataSet) const override;
	};

	inline Ref<GLLineRenderer> GLLineRenderer::create(float length,float width)
	{
		return SPK_NEW(GLLineRenderer,length,width);
	}
}}

#endif
