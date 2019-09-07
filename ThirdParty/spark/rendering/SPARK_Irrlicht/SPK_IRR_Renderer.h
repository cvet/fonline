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

#ifndef H_SPK_IRR_RENDERER
#define H_SPK_IRR_RENDERER

#include "Rendering/Irrlicht/SPK_IRR_DEF.h"
#include "Rendering/Irrlicht/SPK_IRR_Buffer.h"

namespace SPK
{
namespace IRR
{
	/**
	* @brief The base renderer for all Irrlicht renderers
	* 
	* This class presents a convenient interface to set some parameters common to all Irrlicht renderers (blending mode...).<br>
	* <br>
	* Note that rendering hints work with Irrlicht renderers except the SPK::ALPHA_TEST
	* which is always enabled with a threshold of 0. (meaning alpha values of 0 are never rendered).
	*/
	class SPK_IRR_PREFIX IRRRenderer : public Renderer
	{
	public :

		////////////////
		// Destructor //
		////////////////

		/** @brief Destructor of IRRRenderer */
		virtual ~IRRRenderer(){};

		/////////////
		// Setters //
		/////////////

		/**
		* @brief Sets the blending mode in a very accurate way
		*
		* This method allows to set any blending mode supported by Irrlicht.<br>
		* Note that a simpler helper method exist to set the most common blending modes :<br>
		* <i>setBlendMode(BlendingMode)</i>
		*
		* @param srcFunc : the blending source function
		* @param destFunc : the blending destination function
		* @param alphaSrc : the alpha source
		*/
		void setBlendMode(irr::video::E_BLEND_FACTOR srcFunc,irr::video::E_BLEND_FACTOR destFunc,unsigned int alphaSrc);
		virtual void setBlendMode(BlendMode blendMode);

		/** @brief Changes the Irrlicht device */
		virtual void setDevice(irr::IrrlichtDevice* d);

		virtual void enableRenderingOption(RenderingOption renderingHint,bool enable);
		virtual void setAlphaTestThreshold(float alphaThreshold);

		/////////////
		// Getters //
		/////////////

		/**
		* @brief Gets the Irrlicht device of this renderer
		* @return the device of this renderer
		*/
        inline irr::IrrlichtDevice* getDevice() const;

		/**
		* @brief Gets the source blending funtion of this renderer
		* @return the source blending funtion of this renderer
		*/
		irr::video::E_BLEND_FACTOR getBlendSrcFunc() const;

		/**
		* @brief Gets the destination blending funtion of this renderer
		* @return the destination blending funtion of this renderer
		*/
		irr::video::E_BLEND_FACTOR getBlendDestFunc() const;

		/**
		* @brief Gets the alpha source of this renderer
		* @return the alpha source of this renderer
		*/
		unsigned int getAlphaSource() const;

		/**
		* @brief Gets the material of this renderer
		*
		* Note that the renderer is constant and therefore cannot be modified directly
		*
		* @return the material of this renderer
		*/
		const irr::video::SMaterial& getMaterial() const;

		virtual bool isRenderingOptionEnabled(RenderingOption renderingHint) const;

	public :
		spark_description(IRRRenderer, Renderer)
		(
		);

	protected :

		irr::IrrlichtDevice* device;	// the device
		mutable irr::video::SMaterial material;	// the material

		IRRRenderer(irr::IrrlichtDevice* device = NULL,bool NEEDS_DATASET = false);

	private :

		irr::video::E_BLEND_FACTOR blendSrcFunc;
		irr::video::E_BLEND_FACTOR blendDestFunc;
		unsigned int alphaSource;

		void updateMaterialBlendingMode();
	};
	
	inline irr::IrrlichtDevice* IRRRenderer::getDevice() const
	{
		return device;
	}

	inline irr::video::E_BLEND_FACTOR IRRRenderer::getBlendSrcFunc() const
	{
		return blendSrcFunc;
	}

	inline void IRRRenderer::setDevice(irr::IrrlichtDevice* d)
	{
		device = d;
	}

	inline irr::video::E_BLEND_FACTOR IRRRenderer::getBlendDestFunc() const
	{
		return blendDestFunc;
	}

	inline unsigned int IRRRenderer::getAlphaSource() const
	{
		return alphaSource;
	}

	inline const irr::video::SMaterial& IRRRenderer::getMaterial() const
	{
		return material;
	}

	inline void IRRRenderer::updateMaterialBlendingMode()
	{
#if (IRRLICHT_VERSION_MAJOR > 1 || IRRLICHT_VERSION_MINOR >= 8) // Dont handle 0.x
		material.MaterialTypeParam = irr::video::pack_textureBlendFunc(
#else
		material.MaterialTypeParam = irr::video::pack_texureBlendFunc( // typo error until Irrlich 1.7
#endif
			blendSrcFunc,
			blendDestFunc,
			irr::video::EMFN_MODULATE_1X,
			alphaSource);
	}
}}

#endif
