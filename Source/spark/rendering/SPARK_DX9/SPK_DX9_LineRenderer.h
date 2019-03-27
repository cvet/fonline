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

#ifndef H_SPK_DX9_LINERENDERER
#define H_SPK_DX9_LINERENDERER

#include "Rendering/DX9/SPK_DX9_Renderer.h"
#include "Extensions/Renderers/SPK_LineRenderBehavior.h"
#include "Rendering/DX9/SPK_DX9_Buffer.h"

namespace SPK
{
namespace DX9
{
	/**
	* @class DX9LineRenderer
	* @brief A Renderer drawing particles as DirectX 9.0 lines
	*
	* The length of the lines is function of the Particle velocity and is defined in the universe space
	* while the width is fixed to 1 pixel due to directx limitation.<br>
	* <br>
	* Below are the parameters of Particle that are used in this Renderer (others have no effects) :
	* <ul>
	* <li>SPK::PARAM_RED</li>
	* <li>SPK::PARAM_GREEN</li>
	* <li>SPK::PARAM_BLUE</li>
	* <li>SPK::PARAM_ALPHA (only if blending is enabled)</li>
	* </ul>
	*/
	class SPK_DX9_PREFIX DX9LineRenderer : public DX9Renderer, public LineRenderBehavior
	{
	public :

		/**
		* @brief Creates and registers a new DX9LineRenderer
		* @param length : the length multiplier of the lines
		* @param width : the width of the lines (in screen space)
		* @return A new DX9LineRenderer
		*/
		static  DX9LineRenderer* create(float length = 1.0f);

	public :
		spark_description(DX9LineRenderer, DX9Renderer)
		(
		);

	private :

		DX9LineRenderer(float length = 1.0f);
		DX9LineRenderer(const DX9LineRenderer& renderer);

		virtual RenderBuffer* attachRenderBuffer(const Group& group) const;

		virtual void render(const Group& group,const DataSet* dataSet,RenderBuffer* renderBuffer) const;
		virtual void computeAABB(Vector3D& AABBMin,Vector3D& AABBMax,const Group& group,const DataSet* dataSet) const;
	};

	inline DX9LineRenderer* DX9LineRenderer::create(float length)
	{
		return SPK_NEW(DX9LineRenderer,length);
	}
}}

#endif
