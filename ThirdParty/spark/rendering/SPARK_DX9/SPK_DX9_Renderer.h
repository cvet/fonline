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

#ifndef H_SPK_DX9_RENDERER
#define H_SPK_DX9_RENDERER

#include "Rendering/DX9/SPK_DX9_Info.h"

namespace SPK
{
namespace DX9
{
	/**
	* @class DX9Renderer
	* @brief An abstract Renderer for the DirectX 9.0 renderers
	*/
	class SPK_DX9_PREFIX DX9Renderer : public Renderer
	{
	public :

		////////////////
		// Destructor //
		////////////////

		/** @brief Destructor of DX9Renderer */
		virtual  ~DX9Renderer() {}

		/////////////
		// Setters //
		/////////////

		/**
		* @brief Enables or disables the blending of this DX9Renderer
		* @param blendingEnabled true to enable the blending, false to disable it
		*/
		virtual  void enableBlending(bool blendingEnabled);

		/**
		* @brief Sets the blending functions of this DX9Renderer
		*
		* the blending functions are one of the DirectX 9.0 blending functions.
		*
		* @param src : the source blending function of this DX9Renderer
		* @param dest : the destination blending function of this DX9Renderer
		*/
		void setBlendingFunctions(D3DBLEND src, D3DBLEND dest);
		virtual void setBlendMode(BlendMode blendMode);

		/////////////
		// Getters //
		/////////////

		/**
		* @brief Tells whether blending is enabled for this DX9Renderer
		* @return true if blending is enabled, false if it is disabled
		*/
		bool isBlendingEnabled() const;

		/**
		* @brief Gets the source blending function of this DX9Renderer
		* @return the source blending function of this DX9Renderer
		*/
		D3DBLEND getSrcBlendingFunction() const;

		/**
		* @brief Gets the destination blending function of this DX9Renderer
		* @return the source destination function of this DX9Renderer
		*/
		D3DBLEND getDestBlendingFunction() const;

	public :
		spark_description(DX9Renderer, Renderer)
		(
		);

	protected :

		DX9Renderer(bool NEEDS_DATASET);

		/** @brief Inits the blending of this DX9Renderer */
		void initBlending() const;

		/** @brief Inits the rendering hints of this DX9Renderer */
		void initRenderingOptions() const;

	private :

		bool blendingEnabled;
		D3DBLEND srcBlending;
		D3DBLEND destBlending;
	};

	inline DX9Renderer::DX9Renderer(bool NEEDS_DATASET) :
		Renderer(NEEDS_DATASET),
		blendingEnabled(false),
		srcBlending(D3DBLEND_SRCALPHA),
		destBlending(D3DBLEND_INVSRCALPHA)
	{}

	inline void DX9Renderer::enableBlending(bool blendingEnabled)
	{
		this->blendingEnabled = blendingEnabled;
	}

	inline void DX9Renderer::setBlendingFunctions(D3DBLEND src, D3DBLEND dest)
	{
		srcBlending = src;
		destBlending = dest;
	}

	inline bool DX9Renderer::isBlendingEnabled() const
	{
		return blendingEnabled;
	}

	inline D3DBLEND DX9Renderer::getSrcBlendingFunction() const
	{
		return srcBlending;
	}

	inline D3DBLEND DX9Renderer::getDestBlendingFunction() const
	{
		return destBlending;
	}

	inline void DX9Renderer::initBlending() const
	{
		if (blendingEnabled)
		{
			DX9Info::getDevice()->SetRenderState(D3DRS_SRCBLEND, srcBlending);
			DX9Info::getDevice()->SetRenderState(D3DRS_DESTBLEND, destBlending);
			DX9Info::getDevice()->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
		}
		else
			DX9Info::getDevice()->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
	}

	inline void DX9Renderer::initRenderingOptions() const
	{
		// alpha test
		if( isRenderingOptionEnabled(RENDERING_OPTION_ALPHA_TEST) )
		{
			// TODO : glAlphaFunc(GL_GEQUAL,getAlphaTestThreshold());
			DX9Info::getDevice()->SetRenderState(D3DRS_ALPHATESTENABLE, true);
		}
		else
			DX9Info::getDevice()->SetRenderState(D3DRS_ALPHATESTENABLE, false);

		// depth write
		DX9Info::getDevice()->SetRenderState(D3DRS_ZWRITEENABLE, isRenderingOptionEnabled(RENDERING_OPTION_DEPTH_WRITE));
	}
}}

#endif
