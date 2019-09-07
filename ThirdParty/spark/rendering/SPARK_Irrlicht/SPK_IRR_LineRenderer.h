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

#ifndef H_PK_IRR_LINERENDERER
#define H_SPK_IRR_LINERENDERER

#include "Rendering/Irrlicht/SPK_IRR_Renderer.h"
#include "Extensions/Renderers/SPK_LineRenderBehavior.h"

namespace SPK
{
namespace IRR
{
	/**
	* @class IRRLineRenderer
	* @brief A Renderer drawing particles as lines with Irrlicht
	*
	* The length of the lines is function of the Particle velocity and is defined in the universe space
	* while the width is fixed and defines in the screen space (in pixels).<br>
	* Note that the width only works when using Irrlicht with OpenGL. With Direct3D, the parameter is ignored and 1 is used instead.
	*/
	class SPK_IRR_PREFIX IRRLineRenderer :	public IRRRenderer,
											public LineRenderBehavior
	{
	public :

		/**
		* @brief Creates and registers a new IRRLineRenderer
		* @param d : the Irrlicht device
		* @param length : the length multiplier of this IRRLineRenderer
		* @param width : the width of this IRRLineRenderer in pixels
		* @return A new registered IRRLineRenderer
		*/
		static  Ref<IRRLineRenderer> create(irr::IrrlichtDevice* d = NULL,float length = 1.0f,float width = 1.0f);

		virtual  void setWidth(float width);

	public :
		spark_description(IRRLineRenderer, IRRRenderer)
		(
		);

	private :

		static const size_t NB_INDICES_PER_PARTICLE = 2;
		static const size_t NB_VERTICES_PER_PARTICLE = 2;

		IRRLineRenderer(irr::IrrlichtDevice* d = NULL,float length = 1.0f,float width = 1.0f);
		IRRLineRenderer(const IRRLineRenderer& renderer);
	
		virtual RenderBuffer* attachRenderBuffer(const Group& group) const;

		virtual void render(const Group& group,const DataSet* dataSet,RenderBuffer* renderBuffer) const;
		virtual void computeAABB(Vector3D& AABBMin,Vector3D& AABBMax,const Group& group,const DataSet* dataSet) const;
	};


	inline Ref<IRRLineRenderer> IRRLineRenderer::create(irr::IrrlichtDevice* d,float length,float width)
	{
		return SPK_NEW(IRRLineRenderer,d,length,width);
	}

	inline void IRRLineRenderer::setWidth(float width)
	{
		material.Thickness = this->width = width;
	}
}}

#endif
