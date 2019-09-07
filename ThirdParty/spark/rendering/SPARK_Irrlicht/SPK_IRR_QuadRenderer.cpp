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
#include "Rendering/Irrlicht/SPK_IRR_QuadRenderer.h"

namespace SPK
{
namespace IRR
{
	IRRQuadRenderer::IRRQuadRenderer(irr::IrrlichtDevice* d,float scaleX,float scaleY) :
		IRRRenderer(d),
		QuadRenderBehavior(scaleX,scaleY),
		Oriented3DRenderBehavior()
	{}

	IRRQuadRenderer::IRRQuadRenderer(const IRRQuadRenderer& renderer) :
		IRRRenderer(renderer),
		QuadRenderBehavior(renderer),
		Oriented3DRenderBehavior(renderer)
	{}

	bool IRRQuadRenderer::setTexturingMode(TextureMode mode)
	{
		if (mode == TEXTURE_MODE_3D)
		{
			SPK_LOG_WARNING("IRRQuadRenderer::setTexturingMode(TextureMode) - Textures 3D are not supported by Irrlicht - Call is ignored");
			return false;
		}
		
		texturingMode = mode;
		return true;
	}

	RenderBuffer* IRRQuadRenderer::attachRenderBuffer(const Group& group) const
	{
		// Creates the render buffer
		IRRBuffer* buffer = SPK_NEW(IRRBuffer,device,group.getCapacity(),NB_VERTICES_PER_PARTICLE,NB_INDICES_PER_PARTICLE);
	
		buffer->positionAtStart();

		// Initializes the index array
		for (size_t i = 0; i < group.getCapacity(); ++i)
		{
			buffer->setNextIndex(NB_VERTICES_PER_PARTICLE * i + 0);
            buffer->setNextIndex(NB_VERTICES_PER_PARTICLE * i + 1);
            buffer->setNextIndex(NB_VERTICES_PER_PARTICLE * i + 2);
            buffer->setNextIndex(NB_VERTICES_PER_PARTICLE * i + 0);
            buffer->setNextIndex(NB_VERTICES_PER_PARTICLE * i + 2);
            buffer->setNextIndex(NB_VERTICES_PER_PARTICLE * i + 3);
		}

		// Initializes the texture array
		for (size_t i = 0; i < group.getCapacity(); ++i)
		{
			buffer->setNextTexCoords(0.0f,0.0f);
			buffer->setNextTexCoords(1.0f,0.0f);
			buffer->setNextTexCoords(1.0f,1.0f);
			buffer->setNextTexCoords(0.0f,1.0f);
		}

		buffer->getMeshBuffer().setDirty(irr::scene::EBT_VERTEX_AND_INDEX);

		return buffer;
	}

	void IRRQuadRenderer::render(const Group& group,const DataSet* dataSet,RenderBuffer* renderBuffer) const
	{
		SPK_ASSERT(renderBuffer != NULL,"IRRQuadRenderer::render(const Group&,const DataSet*,RenderBuffer*) - renderBuffer must not be NULL");
		IRRBuffer& buffer = static_cast<IRRBuffer&>(*renderBuffer);
		buffer.positionAtStart(); // Repositions all the buffers at the start

		irr::video::IVideoDriver* driver = device->getVideoDriver();

		// Computes the inverse model view
		irr::core::matrix4 invModelView;
		{
			irr::core::matrix4 modelView(driver->getTransform(irr::video::ETS_VIEW));
			modelView *= driver->getTransform(irr::video::ETS_WORLD);
			modelView.getInversePrimitive(invModelView); // wont work for odd modelview matrices (but should happen in very special cases)
		}

		// Saves the renderer texture
		irr::video::ITexture* savedTexture = material.TextureLayer[0].Texture;
		if (texturingMode == TEXTURE_MODE_NONE)
			material.TextureLayer[0].Texture = NULL;

		if ((texturingMode == TEXTURE_MODE_2D)&&(group.isEnabled(PARAM_TEXTURE_INDEX)))
		{
			if (group.isEnabled(PARAM_ANGLE))
				renderParticle = &IRRQuadRenderer::renderAtlasRot;
			else
				renderParticle = &IRRQuadRenderer::renderAtlas;
		}
		else
		{
			if (group.isEnabled(PARAM_ANGLE))
				renderParticle = &IRRQuadRenderer::renderRot;
			else
				renderParticle = &IRRQuadRenderer::renderBasic;
		}

		buffer.setUsed(group.getNbParticles());

		bool globalOrientation = precomputeOrientation3D(
			group,
			Vector3D(invModelView[8],invModelView[9],invModelView[10]),
			Vector3D(invModelView[4],invModelView[5],invModelView[6]),
			Vector3D(invModelView[12],invModelView[13],invModelView[14]));

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
		buffer.getMeshBuffer().setDirty(irr::scene::EBT_VERTEX);

		driver->setMaterial(material);
		driver->drawMeshBuffer(&buffer.getMeshBuffer()); // this draw call is used in order to be able to use VBOs

		material.TextureLayer[0].Texture = savedTexture; // Restores the texture
	}

	void IRRQuadRenderer::computeAABB(Vector3D& AABBMin,Vector3D& AABBMax,const Group& group,const DataSet* dataSet) const
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

	void IRRQuadRenderer::renderBasic(const Particle& particle,IRRBuffer& renderBuffer) const
	{
		scaleQuadVectors(particle,scaleX,scaleY);
		FillBufferColorAndVertex(particle,renderBuffer);
	}

	void IRRQuadRenderer::renderRot(const Particle& particle,IRRBuffer& renderBuffer) const
	{
		rotateAndScaleQuadVectors(particle,scaleX,scaleY);
		FillBufferColorAndVertex(particle,renderBuffer);
	}

	void IRRQuadRenderer::renderAtlas(const Particle& particle,IRRBuffer& renderBuffer) const
	{
		scaleQuadVectors(particle,scaleX,scaleY);
		FillBufferColorAndVertex(particle,renderBuffer);
		FillBufferTextureAtlas(particle,renderBuffer);
	}

	void IRRQuadRenderer::renderAtlasRot(const Particle& particle,IRRBuffer& renderBuffer) const
	{
		rotateAndScaleQuadVectors(particle,scaleX,scaleY);
		FillBufferColorAndVertex(particle,renderBuffer);
		FillBufferTextureAtlas(particle,renderBuffer);
	}
}}
