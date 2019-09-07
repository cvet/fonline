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
#include "Rendering/Irrlicht/SPK_IRR_Renderer.h"

namespace SPK
{
namespace IRR
{
	IRRRenderer::IRRRenderer(irr::IrrlichtDevice* device,bool NEEDS_DATASET) :
		Renderer(NEEDS_DATASET),
		device(device)
	{
		material.GouraudShading = true;									// fix for ATI cards
		material.Lighting = false;										// No lights per default
		material.BackfaceCulling = false;								// Deactivates backface culling
		material.MaterialType = irr::video::EMT_ONETEXTURE_BLEND;		// To allow complex blending functions
		setBlendMode(BLEND_MODE_NONE);										// BlendMode is disabled per default
	}

	void IRRRenderer::setBlendMode(irr::video::E_BLEND_FACTOR srcFunc,irr::video::E_BLEND_FACTOR destFunc,unsigned int alphaSrc)
	{
		blendSrcFunc = srcFunc;
		blendDestFunc = destFunc;
		alphaSource = alphaSrc;
		updateMaterialBlendingMode();
	}

	void IRRRenderer::setBlendMode(BlendMode blendMode)
	{
		switch(blendMode)
		{
		case BLEND_MODE_NONE :
			blendSrcFunc = irr::video::EBF_ONE;
			blendDestFunc = irr::video::EBF_ZERO;
			alphaSource = irr::video::EAS_NONE;
			break;

		case BLEND_MODE_ADD :
			blendSrcFunc = irr::video::EBF_SRC_ALPHA;
			blendDestFunc = irr::video::EBF_ONE;
			alphaSource = irr::video::EAS_VERTEX_COLOR | irr::video::EAS_TEXTURE;
			break;

		case BLEND_MODE_ALPHA :
			blendSrcFunc = irr::video::EBF_SRC_ALPHA;
			blendDestFunc = irr::video::EBF_ONE_MINUS_SRC_ALPHA;
			alphaSource = irr::video::EAS_VERTEX_COLOR | irr::video::EAS_TEXTURE;
			break;

		default :
			SPK_LOG_WARNING("IRRRenderer::setBlendMode(BlendMode) - Unsupported blending mode. Nothing happens");
			break;
		}
		updateMaterialBlendingMode();
	}

	void IRRRenderer::enableRenderingOption(RenderingOption option,bool enable)
	{
		switch(option)
		{
		case RENDERING_OPTION_DEPTH_WRITE :
			material.ZWriteEnable = enable;
			break;

		case RENDERING_OPTION_ALPHA_TEST :
			if (!enable)
				SPK_LOG_WARNING("IRRRenderer::enableRenderingOption(RenderingOption) - Alpha test cannot be unset in Irricht and is always active");
			break;

		default :
			SPK_LOG_WARNING("IRRRenderer::enableRenderingOption(RenderingOption) - Unsupported rendering option. Nothing happens");
			break;
		}
	}

	bool IRRRenderer::isRenderingOptionEnabled(RenderingOption option) const
	{
		switch(option)
		{
		case RENDERING_OPTION_DEPTH_WRITE :
			return material.ZWriteEnable;

		case RENDERING_OPTION_ALPHA_TEST :
			return true; // always enabled in the irrlicht material
		}

		return false;
	}

	void IRRRenderer::setAlphaTestThreshold(float alphaThreshold)
	{
		SPK_LOG_WARNING("IRRRenderer::setAlphaTestThreshold(float) - The alpha threshold cannot be configured in Irrlicht and is always 0.0");
		Renderer::setAlphaTestThreshold(0.0f); // the alpha threshold of the irrlicht material is always 0
	}
}}
