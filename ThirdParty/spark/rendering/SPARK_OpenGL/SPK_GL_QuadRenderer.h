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

#ifndef H_SPK_GLQUADRENDERER
#define H_SPK_GLQUADRENDERER

#include "SPK_GL_Renderer.h"
#include "Extensions/Renderers/SPK_QuadRenderBehavior.h"
#include "Extensions/Renderers/SPK_Oriented3DRenderBehavior.h"
#include "SPK_GL_Buffer.h"

namespace SPK
{
namespace GL
{
	/**
	* @class GLQuadRenderer
	* @brief A Renderer drawing particles as OpenGL quads
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
	class SPK_GL_PREFIX GLQuadRenderer :	public GLRenderer,
											public QuadRenderBehavior,
											public Oriented3DRenderBehavior
	{
        SPK_IMPLEMENT_OBJECT(GLQuadRenderer);

		SPK_START_DESCRIPTION
		SPK_PARENT_ATTRIBUTES(GLRenderer)
		SPK_ATTRIBUTE("texture", ATTRIBUTE_TYPE_STRING)
		SPK_ATTRIBUTE("texturing mode", ATTRIBUTE_TYPE_STRING)
		SPK_ATTRIBUTE("scale", ATTRIBUTE_TYPE_FLOATS)
		SPK_ATTRIBUTE("atlas dimensions", ATTRIBUTE_TYPE_UINT32S)
		SPK_ATTRIBUTE("look orientation", ATTRIBUTE_TYPE_STRING)
		SPK_ATTRIBUTE("up orientation", ATTRIBUTE_TYPE_STRING)
		SPK_ATTRIBUTE("locked axis", ATTRIBUTE_TYPE_STRING)
		SPK_ATTRIBUTE("locked look vector", ATTRIBUTE_TYPE_VECTOR)
		SPK_ATTRIBUTE("locked up vector", ATTRIBUTE_TYPE_VECTOR)
		SPK_END_DESCRIPTION

	public :

		static void setTextureLoader(GLuint(*loader)(const std::string&));

		/**
		* @brief Creates and registers a new GLQuadRenderer
		* @param scaleX the scale of the width of the quad
		* @param scaleY the scale of the height of the quad
		* @return A new registered GLQuadRenderer
		*/
		static  Ref<GLQuadRenderer> create(float scaleX = 1.0f,float scaleY = 1.0f);

		/////////////
		// Setters //
		/////////////

		bool setTexturingMode(TextureMode mode) override;

		void setTexture(GLuint textureIndex);
		
		void setTextureName(const std::string& textureName);
		std::string getTextureName() const;

		/////////////
		// Getters //
		/////////////

		/**
		* @brief Gets the texture of this GLQuadRenderer
		* @return the texture of this GLQuadRenderer
		*/
		GLuint getTexture() const;

	protected:

		void innerImport(const IO::Descriptor& descriptor) override;
		void innerExport(IO::Descriptor& descriptor) const override;

	private :

		static GLboolean* const SPK_GL_TEXTURE_3D_EXT;

		std::string textureName;

		mutable float modelView[16];
		mutable float invModelView[16];

		GLuint textureIndex;

		GLQuadRenderer(float scaleX = 1.0f,float scaleY = 1.0f);
		GLQuadRenderer(const GLQuadRenderer& renderer);

		RenderBuffer* attachRenderBuffer(const Group& group) const override;

		void render(const Group& group,const DataSet* dataSet,RenderBuffer* renderBuffer) const override;
		void computeAABB(Vector3D& AABBMin,Vector3D& AABBMax,const Group& group,const DataSet* dataSet) const override;

		void invertModelView() const;

		void GLCallColorAndVertex(const Particle& particle,GLBuffer& renderBuffer) const;	// OpenGL calls for color and position
		void GLCallTexture2DAtlas(const Particle& particle,GLBuffer& renderBuffer) const;	// OpenGL calls for 2D atlastexturing 
		void GLCallTexture3D(const Particle& particle,GLBuffer& renderBuffer) const;		// OpenGL calls for 3D texturing

		mutable void (GLQuadRenderer::*renderParticle)(const Particle&,GLBuffer& renderBuffer) const;	// pointer to the right render method

		void render2D(const Particle& particle,GLBuffer& renderBuffer) const;			// Rendering for particles with texture 2D or no texture
		void render2DRot(const Particle& particle,GLBuffer& renderBuffer) const;		// Rendering for particles with texture 2D or no texture and rotation
		void render3D(const Particle& particle,GLBuffer& renderBuffer) const;			// Rendering for particles with texture 3D
		void render3DRot(const Particle& particle,GLBuffer& renderBuffer) const;		// Rendering for particles with texture 3D and rotation
		void render2DAtlas(const Particle& particle,GLBuffer& renderBuffer) const;		// Rendering for particles with texture 2D atlas
		void render2DAtlasRot(const Particle& particle,GLBuffer& renderBuffer) const;	// Rendering for particles with texture 2D atlas and rotation
	};

	inline Ref<GLQuadRenderer> GLQuadRenderer::create(float scaleX,float scaleY)
	{
		return SPK_NEW(GLQuadRenderer,scaleX,scaleY);
	}
		
	inline void GLQuadRenderer::setTexture(GLuint textureIndex)
	{
		this->textureIndex = textureIndex;
	}

	inline GLuint GLQuadRenderer::getTexture() const
	{
		return textureIndex;
	}

	inline void GLQuadRenderer::GLCallColorAndVertex(const Particle& particle,GLBuffer& renderBuffer) const
	{
		// quads are drawn in a counter clockwise order :
		renderBuffer.setNextVertex(particle.position() + quadSide() + quadUp());	// top right vertex
		renderBuffer.setNextVertex(particle.position() - quadSide() + quadUp());	// top left vertex
		renderBuffer.setNextVertex(particle.position() - quadSide() - quadUp());	// bottom left vertex
		renderBuffer.setNextVertex(particle.position() + quadSide() - quadUp());	// bottom right vertex
		
		renderBuffer.setNextColor(particle.getColor());
		renderBuffer.setNextColor(particle.getColor());
		renderBuffer.setNextColor(particle.getColor());
		renderBuffer.setNextColor(particle.getColor());
	}

	inline void GLQuadRenderer::GLCallTexture2DAtlas(const Particle& particle,GLBuffer& renderBuffer) const
	{
		computeAtlasCoordinates(particle);

		renderBuffer.setNextTexCoord(textureAtlasU1());
		renderBuffer.setNextTexCoord(textureAtlasV0());

		renderBuffer.setNextTexCoord(textureAtlasU0());
		renderBuffer.setNextTexCoord(textureAtlasV0());

		renderBuffer.setNextTexCoord(textureAtlasU0());
		renderBuffer.setNextTexCoord(textureAtlasV1());

		renderBuffer.setNextTexCoord(textureAtlasU1());
		renderBuffer.setNextTexCoord(textureAtlasV1());	
	}

	inline void GLQuadRenderer::GLCallTexture3D(const Particle& particle,GLBuffer& renderBuffer) const
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

	inline void GLQuadRenderer::invertModelView() const
	{
		float tmp[12];
		float src[16];

		/* transpose matrix */
		for (int i = 0; i < 4; ++i)
		{
			src[i] = modelView[i << 2];
			src[i + 4] = modelView[(i << 2) + 1];
			src[i + 8] = modelView[(i << 2) + 2];
			src[i + 12] = modelView[(i << 2) + 3];
		}

		/* calculate pairs for first 8 elements (cofactors) */
		tmp[0] = src[10] * src[15];
		tmp[1] = src[11] * src[14];
		tmp[2] = src[9] * src[15];
		tmp[3] = src[11] * src[13];
		tmp[4] = src[9] * src[14];
		tmp[5] = src[10] * src[13];
		tmp[6] = src[8] * src[15];
		tmp[7] = src[11] * src[12];
		tmp[8] = src[8] * src[14];
		tmp[9] = src[10] * src[12];
		tmp[10] = src[8] * src[13];
		tmp[11] = src[9] * src[12];

		/* calculate first 8 elements (cofactors) */
		invModelView[0] = tmp[0] * src[5] + tmp[3] * src[6] + tmp[4] * src[7] - tmp[1] * src[5] - tmp[2] * src[6] - tmp[5] * src[7];
		invModelView[1] = tmp[1] * src[4] + tmp[6] * src[6] + tmp[9] * src[7] - tmp[0] * src[4] - tmp[7] * src[6] - tmp[8] * src[7];
		invModelView[2] = tmp[2] * src[4] + tmp[7] * src[5] + tmp[10] * src[7] - tmp[3] * src[4] - tmp[6] * src[5] - tmp[11] * src[7];
		invModelView[3] = tmp[5] * src[4] + tmp[8] * src[5] + tmp[11] * src[6] - tmp[4] * src[4] - tmp[9] * src[5] - tmp[10] * src[6];
		invModelView[4] = tmp[1] * src[1] + tmp[2] * src[2] + tmp[5] * src[3] - tmp[0] * src[1] - tmp[3] * src[2] - tmp[4] * src[3];
		invModelView[5] = tmp[0] * src[0] + tmp[7] * src[2] + tmp[8] * src[3] - tmp[1] * src[0]- tmp[6] * src[2] - tmp[9] * src[3];
		invModelView[6] = tmp[3] * src[0] + tmp[6] * src[1] + tmp[11] * src[3] - tmp[2] * src[0] - tmp[7] * src[1] - tmp[10] * src[3];
		invModelView[7] = tmp[4] * src[0] + tmp[9] * src[1] + tmp[10] * src[2] - tmp[5]*src[0] - tmp[8]*src[1] - tmp[11]*src[2];

		/* calculate pairs for second 8 elements (cofactors) */
		tmp[0] = src[2] * src[7];
		tmp[1] = src[3] * src[6];
		tmp[2] = src[1] * src[7];
		tmp[3] = src[3] * src[5];
		tmp[4] = src[1] * src[6];
		tmp[5] = src[2] * src[5];
		tmp[6] = src[0] * src[7];
		tmp[7] = src[3] * src[4];
		tmp[8] = src[0] * src[6];
		tmp[9] = src[2] * src[4];
		tmp[10] = src[0] * src[5];
		tmp[11] = src[1] * src[4];

		/* calculate second 8 elements (cofactors) */
		invModelView[8] = tmp[0] * src[13] + tmp[3] * src[14] + tmp[4] * src[15] - tmp[1] * src[13] - tmp[2] * src[14] - tmp[5] * src[15];
		invModelView[9] = tmp[1] * src[12] + tmp[6] * src[14] + tmp[9] * src[15] - tmp[0] * src[12] - tmp[7] * src[14] - tmp[8] * src[15];
		invModelView[10] = tmp[2] * src[12] + tmp[7] * src[13] + tmp[10] * src[15] - tmp[3] * src[12] - tmp[6] * src[13] - tmp[11] * src[15];
		invModelView[11] = tmp[5] * src[12] + tmp[8] * src[13] + tmp[11] * src[14] - tmp[4] * src[12] - tmp[9] * src[13] - tmp[10] * src[14];
		invModelView[12] = tmp[2] * src[10] + tmp[5] * src[11] + tmp[1] * src[9] - tmp[4] * src[11] - tmp[0] * src[9] - tmp[3] * src[10];
		invModelView[13] = tmp[8] * src[11] + tmp[0] * src[8] + tmp[7] * src[10] - tmp[6] * src[10] - tmp[9] * src[11] - tmp[1] * src[8];
		invModelView[14] = tmp[6] * src[9] + tmp[11] * src[11] + tmp[3] * src[8] - tmp[10] * src[11] - tmp[2] * src[8] - tmp[7] * src[9];
		invModelView[15] = tmp[10] * src[10] + tmp[4] * src[8] + tmp[9] * src[9] - tmp[8] * src[9] - tmp[11] * src[10] - tmp[5] * src[8];

		/* calculate determinant */
		float det = src[0] * invModelView[0] + src[1] * invModelView[1] + src[2] * invModelView[2] + src[3] * invModelView[3];

		/* calculate matrix inverse */
		det = 1 / det;
		for (int i = 0; i < 16; ++i)
			invModelView[i] *= det;
	}
}}

#endif
