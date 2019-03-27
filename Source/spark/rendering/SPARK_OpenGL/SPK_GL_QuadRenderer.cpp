//////////////////////////////////////////////////////////////////////////////////
// SPARK particle engine														//
// Copyright (C) 2008-2011 - Julien Fryer - julienfryer@gmail.com				//
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

#ifndef SPK_GL_NO_EXT
#include <GL/glew.h>
#endif

#include <SPARK_Core.h>
#include "SPK_GL_QuadRenderer.h"

namespace SPK
{
namespace GL
{

#ifndef SPK_GL_NO_EXT
	GLboolean* const GLQuadRenderer::SPK_GL_TEXTURE_3D_EXT = &__GLEW_EXT_texture3D;
#endif

	GLQuadRenderer::GLQuadRenderer(float scaleX,float scaleY) :
		GLRenderer(false),
		QuadRenderBehavior(scaleX,scaleY),
		Oriented3DRenderBehavior(),
		textureIndex(0)
	{}

	GLQuadRenderer::GLQuadRenderer(const GLQuadRenderer& renderer) :
		GLRenderer(renderer),
		QuadRenderBehavior(renderer),
		Oriented3DRenderBehavior(renderer),
		textureIndex(renderer.textureIndex)
	{}

	bool GLQuadRenderer::setTexturingMode(TextureMode mode)
	{
		if ((mode == TEXTURE_MODE_3D && !SPK_GL_CHECK_EXTENSION(SPK_GL_TEXTURE_3D_EXT)))
			return false;

		texturingMode = mode;
		return true;
	}

	RenderBuffer* GLQuadRenderer::attachRenderBuffer(const Group& group) const
	{
		return SPK_NEW(GLBuffer,group.getCapacity() << 2);
	}

	void GLQuadRenderer::render(const Group& group,const DataSet* dataSet,RenderBuffer* renderBuffer) const
	{
		SPK_ASSERT(renderBuffer != NULL,"GLQuadRenderer::render(const Group&,const DataSet*,RenderBuffer*) - renderBuffer must not be NULL");
		GLBuffer& buffer = static_cast<GLBuffer&>(*renderBuffer);
		buffer.positionAtStart(); // Repositions all the buffers at the start

		float oldModelView[16];
		for (int i = 0; i < 16; ++i)
			oldModelView[i] = modelView[i];
		glGetFloatv(GL_MODELVIEW_MATRIX,modelView);
		for (int i = 0; i < 16; ++i)
			if (oldModelView[i] != modelView[i])
			{
				invertModelView();
				break;
			}

		initBlending();
		initRenderingOptions();

		glShadeModel(GL_FLAT);

		switch(texturingMode)
		{
		case TEXTURE_MODE_2D :
			// Creates and inits the 2D TexCoord buffer if necessary
			if (buffer.getNbTexCoords() != 2)
			{
				buffer.setNbTexCoords(2);
				if (!group.isEnabled(PARAM_TEXTURE_INDEX))
				{
					float t[8] = {1.0f,0.0f,0.0f,0.0f,0.0f,1.0f,1.0f,1.0f};
					for (size_t i = 0; i < group.getCapacity() << 3; ++i)
						buffer.setNextTexCoord(t[i & 7]);
				}
			}

			// Binds the texture
#ifndef SPK_GL_NO_EXT
			if (SPK_GL_CHECK_EXTENSION(SPK_GL_TEXTURE_3D_EXT))
				glDisable(GL_TEXTURE_3D_EXT);
#endif
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D,textureIndex);

			// Selects the correct function
			if (!group.isEnabled(PARAM_TEXTURE_INDEX))
			{
				if (!group.isEnabled(PARAM_ANGLE))
					renderParticle = &GLQuadRenderer::render2D;
				else
					renderParticle = &GLQuadRenderer::render2DRot;
			}
			else
			{
				if (!group.isEnabled(PARAM_ANGLE))
					renderParticle = &GLQuadRenderer::render2DAtlas;
				else
					renderParticle = &GLQuadRenderer::render2DAtlasRot;
			}
			break;

		case TEXTURE_MODE_3D :
			// Creates and inits the 3D TexCoord buffer if necessery
			if (buffer.getNbTexCoords() != 3)
			{
				buffer.setNbTexCoords(3);
				float t[12] =  {1.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,1.0f,0.0f,1.0f,1.0f,0.0f};
				for (size_t i = 0; i < group.getCapacity() * 12; ++i)
					buffer.setNextTexCoord(t[i % 12]);
			}

			// Binds the texture
			glDisable(GL_TEXTURE_2D);
#ifndef SPK_GL_NO_EXT
			glEnable(GL_TEXTURE_3D_EXT);
			glBindTexture(GL_TEXTURE_3D_EXT,textureIndex);
#endif

			// Selects the correct function
			if (!group.isEnabled(PARAM_ANGLE))
				renderParticle = &GLQuadRenderer::render3D;
			else
				renderParticle = &GLQuadRenderer::render3DRot;
			break;

		case TEXTURE_MODE_NONE :
			if (buffer.getNbTexCoords() != 0)
				buffer.setNbTexCoords(0);

			glDisable(GL_TEXTURE_2D);

			// Selects the correct function
#ifndef SPK_GL_NO_EXT
			if (SPK_GL_CHECK_EXTENSION(SPK_GL_TEXTURE_3D_EXT))
				glDisable(GL_TEXTURE_3D_EXT);
#endif
			if (!group.isEnabled(PARAM_ANGLE))
				renderParticle = &GLQuadRenderer::render2D;
			else
				renderParticle = &GLQuadRenderer::render2DRot;
			break;
		}

		bool globalOrientation = precomputeOrientation3D(
			group,
			Vector3D(-invModelView[8],-invModelView[9],-invModelView[10]),
			Vector3D(invModelView[4],invModelView[5],invModelView[6]),
			Vector3D(invModelView[12],invModelView[13],invModelView[14]));

		// Fills the buffers
		if (globalOrientation)
		{
			computeGlobalOrientation3D(group);

			for (ConstGroupIterator particleIt(group); !particleIt.end(); ++particleIt)
				(this->*renderParticle)(*particleIt,buffer);
		}
		else
		{
			for (ConstGroupIterator particleIt(group); !particleIt.end(); ++particleIt)
			{
				computeSingleOrientation3D(*particleIt);
				(this->*renderParticle)(*particleIt,buffer);
			}
		}

		buffer.render(GL_QUADS,group.getNbParticles() << 2);
	}

	void GLQuadRenderer::computeAABB(Vector3D& AABBMin,Vector3D& AABBMax,const Group& group,const DataSet* dataSet) const
	{
		float diagonal = group.getGraphicalRadius() * std::sqrt(scaleX * scaleX + scaleY * scaleY);
		Vector3D diagV(diagonal,diagonal,diagonal);

		if (group.isEnabled(PARAM_SCALE))
			for (ConstGroupIterator particleIt(group); !particleIt.end(); ++particleIt)
			{
				Vector3D scaledDiagV = diagV * particleIt->getParamNC(PARAM_SCALE);
				AABBMin.setMin(particleIt->position() - scaledDiagV);
				AABBMax.setMax(particleIt->position() + scaledDiagV);
			}
		else
		{
			for (ConstGroupIterator particleIt(group); !particleIt.end(); ++particleIt)
			{
				AABBMin.setMin(particleIt->position());
				AABBMax.setMax(particleIt->position());
			}
			AABBMin -= diagV;
			AABBMax += diagV;
		}
	}

	void GLQuadRenderer::render2D(const Particle& particle,GLBuffer& renderBuffer) const
	{
		scaleQuadVectors(particle,scaleX,scaleY);
		GLCallColorAndVertex(particle,renderBuffer);
	}

	void GLQuadRenderer::render2DRot(const Particle& particle,GLBuffer& renderBuffer) const
	{
		rotateAndScaleQuadVectors(particle,scaleX,scaleY);
		GLCallColorAndVertex(particle,renderBuffer);
	}

	void GLQuadRenderer::render3D(const Particle& particle,GLBuffer& renderBuffer) const
	{
		scaleQuadVectors(particle,scaleX,scaleY);
		GLCallColorAndVertex(particle,renderBuffer);
		GLCallTexture3D(particle,renderBuffer);
	}

	void GLQuadRenderer::render3DRot(const Particle& particle,GLBuffer& renderBuffer) const
	{
		rotateAndScaleQuadVectors(particle,scaleX,scaleY);
		GLCallColorAndVertex(particle,renderBuffer);
		GLCallTexture3D(particle,renderBuffer);
	}

	void GLQuadRenderer::render2DAtlas(const Particle& particle,GLBuffer& renderBuffer) const
	{
		scaleQuadVectors(particle,scaleX,scaleY);
		GLCallColorAndVertex(particle,renderBuffer);
		GLCallTexture2DAtlas(particle,renderBuffer);
	}

	void GLQuadRenderer::render2DAtlasRot(const Particle& particle,GLBuffer& renderBuffer) const
	{
		rotateAndScaleQuadVectors(particle,scaleX,scaleY);
		GLCallColorAndVertex(particle,renderBuffer);
		GLCallTexture2DAtlas(particle,renderBuffer);
	}
}}
