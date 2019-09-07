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

#ifndef H_SPK_DX9_QUADRENDERER
#define H_SPK_DX9_QUADRENDERER

#include "Rendering/DX9/SPK_DX9_Renderer.h"
#include "Extensions/Renderers/SPK_QuadRenderBehavior.h"
#include "Extensions/Renderers/SPK_Oriented3DRenderBehavior.h"
#include "Rendering/DX9/SPK_DX9_Buffer.h"

namespace SPK
{
namespace DX9
{
	/**
	* @class DX9QuadRenderer
	* @brief A Renderer drawing particles as DirectX 9.0 quads
	*
	* the orientation of the quads depends on the orientation parameters set.
	* This orientation is computed during rendering by the CPU (further improvement of SPARK will allow to make the computation on GPU side).<br>
	* <br>
	* Below are the parameters of Particle that are used in this Renderer (others have no effects) :
	* <ul>
	* <li>SPK::PARAM_SCALE</li>
	* <li>SPK::PARAM_ANGLE</li>
	* <li>SPK::PARAM_TEXTURE_INDEX (only if not in TEXTURE_NONE mode)</li>
	* </ul>
	*/
	class SPK_DX9_PREFIX DX9QuadRenderer :	public DX9Renderer,
											public QuadRenderBehavior,
											public Oriented3DRenderBehavior
	{
	public :

		/**
		* @brief Creates and registers a new DX9QuadRenderer
		* @param scaleX the scale of the width of the quad
		* @param scaleY the scale of the height of the quad
		* @return A new registered DX9QuadRenderer
		*/
		static  DX9QuadRenderer* create(float scaleX = 1.0f,float scaleY = 1.0f);

		/////////////
		// Setters //
		/////////////

		virtual bool setTexturingMode(TextureMode mode);

		void setTexture(LPDIRECT3DTEXTURE9 textureIndex);

		/////////////
		// Getters //
		/////////////

		/**
		* @brief Gets the texture of this DX9QuadRenderer
		* @return the texture of this DX9QuadRenderer
		*/
		LPDIRECT3DTEXTURE9 getTexture() const;

	public :
		spark_description(DX9QuadRenderer, DX9Renderer)
		(
		);

	private :

		mutable D3DXMATRIX invModelView;

		LPDIRECT3DTEXTURE9 textureIndex;

		DX9QuadRenderer(float scaleX = 1.0f,float scaleY = 1.0f);
		DX9QuadRenderer(const DX9QuadRenderer& renderer);

		virtual RenderBuffer* attachRenderBuffer(const Group& group) const;

		virtual void render(const Group& group,const DataSet* dataSet,RenderBuffer* renderBuffer) const;
		virtual void computeAABB(Vector3D& AABBMin,Vector3D& AABBMax,const Group& group,const DataSet* dataSet) const;

		void DX9CallColorAndVertex(const Particle& particle,DX9Buffer& renderBuffer) const;	// DirectX 9.0 calls for color and position
		void DX9CallTexture2DAtlas(const Particle& particle,DX9Buffer& renderBuffer) const;	// DirectX 9.0 calls for 2D atlastexturing 
		void DX9CallTexture3D(const Particle& particle,DX9Buffer& renderBuffer) const;		// DirectX 9.0 calls for 3D texturing

		static void (DX9QuadRenderer::*renderParticle)(const Particle&,DX9Buffer& renderBuffer)  const;	// pointer to the right render method

		void render2D(const Particle& particle,DX9Buffer& renderBuffer) const;			// Rendering for particles with texture 2D or no texture
		void render2DRot(const Particle& particle,DX9Buffer& renderBuffer) const;		// Rendering for particles with texture 2D or no texture and rotation
		void render3D(const Particle& particle,DX9Buffer& renderBuffer) const;			// Rendering for particles with texture 3D
		void render3DRot(const Particle& particle,DX9Buffer& renderBuffer) const;		// Rendering for particles with texture 3D and rotation
		void render2DAtlas(const Particle& particle,DX9Buffer& renderBuffer) const;		// Rendering for particles with texture 2D atlas
		void render2DAtlasRot(const Particle& particle,DX9Buffer& renderBuffer) const;	// Rendering for particles with texture 2D atlas and rotation
	};

	inline DX9QuadRenderer* DX9QuadRenderer::create(float scaleX,float scaleY)
	{
		return SPK_NEW(DX9QuadRenderer,scaleX,scaleY);
	}
		
	inline void DX9QuadRenderer::setTexture(LPDIRECT3DTEXTURE9 textureIndex)
	{
		this->textureIndex = textureIndex;
	}

	inline LPDIRECT3DTEXTURE9 DX9QuadRenderer::getTexture() const
	{
		return textureIndex;
	}

	inline void DX9QuadRenderer::DX9CallColorAndVertex(const Particle& particle,DX9Buffer& renderBuffer) const
	{
		// quads are drawn in a clockwise order :
		renderBuffer.setNextVertex(particle.position() + quadSide() + quadUp());	// top left vertex
		renderBuffer.setNextVertex(particle.position() - quadSide() + quadUp());	// top right vertex
		renderBuffer.setNextVertex(particle.position() - quadSide() - quadUp());	// bottom right vertex
		renderBuffer.setNextVertex(particle.position() + quadSide() - quadUp());	// bottom left vertex
		
		//renderBuffer.skipNextColors(3);
		renderBuffer.setNextColor(particle.getColor());
		renderBuffer.setNextColor(particle.getColor());
		renderBuffer.setNextColor(particle.getColor());
		renderBuffer.setNextColor(particle.getColor());
	}

	inline void DX9QuadRenderer::DX9CallTexture2DAtlas(const Particle& particle,DX9Buffer& renderBuffer) const
	{
		computeAtlasCoordinates(particle);

		renderBuffer.setNextTexCoord(textureAtlasU0());
		renderBuffer.setNextTexCoord(textureAtlasV0());

		renderBuffer.setNextTexCoord(textureAtlasU1());
		renderBuffer.setNextTexCoord(textureAtlasV0());

		renderBuffer.setNextTexCoord(textureAtlasU1());
		renderBuffer.setNextTexCoord(textureAtlasV1());

		renderBuffer.setNextTexCoord(textureAtlasU0());
		renderBuffer.setNextTexCoord(textureAtlasV1());	
	}

	inline void DX9QuadRenderer::DX9CallTexture3D(const Particle& particle,DX9Buffer& renderBuffer) const
	{
		float textureIndex = particle.getParam(PARAM_TEXTURE_INDEX);

		renderBuffer.skipNextTexCoords(2);
		renderBuffer.setNextTexCoord(textureIndex);
		
		renderBuffer.skipNextTexCoords(2);
		renderBuffer.setNextTexCoord(textureIndex);

		renderBuffer.skipNextTexCoords(2);
		renderBuffer.setNextTexCoord(textureIndex);

		renderBuffer.skipNextTexCoords(2);
		renderBuffer.setNextTexCoord(textureIndex);
	}
}}

#endif
