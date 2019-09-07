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

#ifndef H_SPK_IRR_QUADRENDERER
#define H_SPK_IRR_QUADRENDERER

#include "Rendering/Irrlicht/SPK_IRR_Renderer.h"
#include "Extensions/Renderers/SPK_QuadRenderBehavior.h"
#include "Extensions/Renderers/SPK_Oriented3DRenderBehavior.h"

namespace SPK
{
namespace IRR
{
	/**
	* @class IRRQuadRenderer
	* @brief A Renderer drawing particles as quads with Irrlicht
	*
	* The orientation of the quads depends on the orientation parameters set.
	* <br>
	* Below are the parameters of Particle that are used in this Renderer (others have no effects) :
	* <ul>
	* <li>SPK::PARAM_ANGLE</li>
	* <li>SPK::PARAM_TEXTURE_INDEX (only if not in TEXTURE_NONE mode)</li>
	* </ul>
	*/
	class SPK_IRR_PREFIX IRRQuadRenderer :	public IRRRenderer,
											public QuadRenderBehavior,
											public Oriented3DRenderBehavior
	{
	public :

		/**
		* @brief Creates and registers a new IRRQuadRenderer
		* @param d : the Irrlicht device
		* @param scaleX the scale of the width of the quad
		* @param scaleY the scale of the height of the quad
		* @return A new registered IRRQuadRenderer
		*/
		static  Ref<IRRQuadRenderer> create(irr::IrrlichtDevice* d = NULL,float scaleX = 1.0f,float scaleY = 1.0f);

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

		virtual bool setTexturingMode(TextureMode mode);

		/**
		* @brief Sets the atlas dimension of the texture in an Irrlicht way
		*
		* see setQuadAtlasDimension(unsigned int,unsigned int) for more information
		*
		* @param dim : the atlas dimension of the texture
		*/
		void setAtlasDimensions(irr::core::dimension2du dim);
		using QuadRenderBehavior::setAtlasDimensions;

		/**
		* @brief Gets the atlas dimension of the texture in an Irrlicht way
		* @return the atlas dimension of the texture
		*/
        inline irr::core::dimension2du getAtlasDimensions() const;

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
		spark_description(IRRQuadRenderer, IRRRenderer)
		(
		);

	private :

		static const size_t NB_INDICES_PER_PARTICLE = 6;
		static const size_t NB_VERTICES_PER_PARTICLE = 4;

		IRRQuadRenderer(irr::IrrlichtDevice* d = NULL,float scaleX = 1.0f,float scaleY = 1.0f);
		IRRQuadRenderer(const IRRQuadRenderer& renderer);

		virtual RenderBuffer* attachRenderBuffer(const Group& group) const;

		virtual void render(const Group& group,const DataSet* dataSet,RenderBuffer* renderBuffer) const;
		virtual void computeAABB(Vector3D& AABBMin,Vector3D& AABBMax,const Group& group,const DataSet* dataSet) const;

		void FillBufferColorAndVertex(const Particle& particle,IRRBuffer& renderBuffer) const;	// Fills Irrlicht buffer with color and position
		void FillBufferTextureAtlas(const Particle& particle,IRRBuffer& renderBuffer) const;		// Fills Irrlicht buffer with atlas texture coordinates

		mutable void (IRRQuadRenderer::*renderParticle)(const Particle&,IRRBuffer& renderBuffer) const;	// pointer to the right render method

		void renderBasic(const Particle& particle,IRRBuffer& renderBuffer) const;		// Rendering for particles with texture or no texture
		void renderRot(const Particle& particle,IRRBuffer& renderBuffer) const;			// Rendering for particles with texture or no texture and rotation
		void renderAtlas(const Particle& particle,IRRBuffer& renderBuffer) const;		// Rendering for particles with texture atlas
		void renderAtlasRot(const Particle& particle,IRRBuffer& renderBuffer) const;	// Rendering for particles with texture atlas and rotation
	};


	inline Ref<IRRQuadRenderer> IRRQuadRenderer::create(irr::IrrlichtDevice* d,float scaleX,float scaleY)
	{
		return SPK_NEW(IRRQuadRenderer,d,scaleX,scaleY);
	}

	inline void IRRQuadRenderer::setTexture(irr::video::ITexture* texture)
	{
		material.TextureLayer[0].Texture = texture;
	}

	inline irr::video::ITexture* IRRQuadRenderer::getTexture() const
	{
		return material.TextureLayer[0].Texture;
	}

	inline void IRRQuadRenderer::setAtlasDimensions(irr::core::dimension2du dim)
	{
		setAtlasDimensions(dim.Width,dim.Height);
	}

	inline irr::core::dimension2du IRRQuadRenderer::getAtlasDimensions() const
	{
		return irr::core::dimension2du(textureAtlasNbX,textureAtlasNbY);
	}

	inline irr::video::SMaterialLayer& IRRQuadRenderer::getMaterialLayer()
	{
		return material.TextureLayer[0];
	}

	inline const irr::video::SMaterialLayer& IRRQuadRenderer::getMaterialLayer() const
	{
		return material.TextureLayer[0];
	}

	inline void IRRQuadRenderer::FillBufferColorAndVertex(const Particle& particle,IRRBuffer& renderBuffer) const
	{
		// According to Irrlicht coordinates system, quads are drawn in clockwise order
		// Note that the quad side points towards the left as it is a left handed system
		renderBuffer.setNextVertex(particle.position() + quadSide() + quadUp()); // top left vertex
		renderBuffer.setNextVertex(particle.position() - quadSide() + quadUp()); // top right vertex
		renderBuffer.setNextVertex(particle.position() - quadSide() - quadUp()); // bottom right vertex
		renderBuffer.setNextVertex(particle.position() + quadSide() - quadUp()); // bottom left vertex

		const Color& color = particle.getColor();
		renderBuffer.setNextColor(color);
		renderBuffer.setNextColor(color);
		renderBuffer.setNextColor(color);
		renderBuffer.setNextColor(color);
	}

	inline void IRRQuadRenderer::FillBufferTextureAtlas(const Particle& particle,IRRBuffer& renderBuffer) const
	{
		computeAtlasCoordinates(particle);

		renderBuffer.setNextTexCoords(textureAtlasU0(),textureAtlasV0());
		renderBuffer.setNextTexCoords(textureAtlasU1(),textureAtlasV0());
		renderBuffer.setNextTexCoords(textureAtlasU1(),textureAtlasV1());
		renderBuffer.setNextTexCoords(textureAtlasU0(),textureAtlasV1());
	}
}}

#endif
