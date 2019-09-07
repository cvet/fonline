//////////////////////////////////////////////////////////////////////////////////
// SPARK particle engine														//
// Copyright (C) 2008-2011 - Julien Fryer - julienfryer@gmail.com				//
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

#include <SPARK_Core.h>
#include "Rendering/DX9/SPK_DX9_QuadRenderer.h"

namespace SPK
{
namespace DX9
{
	void (DX9QuadRenderer::*DX9QuadRenderer::renderParticle)(const Particle&,DX9Buffer& renderBuffer) const = NULL;

	DX9QuadRenderer::DX9QuadRenderer(float scaleX,float scaleY) :
		DX9Renderer(false),
		QuadRenderBehavior(scaleX,scaleY),
		Oriented3DRenderBehavior(),
		textureIndex(0)
	{}

	DX9QuadRenderer::DX9QuadRenderer(const DX9QuadRenderer& renderer) :
		DX9Renderer(renderer),
		QuadRenderBehavior(renderer),
		Oriented3DRenderBehavior(renderer),
		textureIndex(renderer.textureIndex)
	{}

	bool DX9QuadRenderer::setTexturingMode(TextureMode mode)
	{
		texturingMode = mode;
		return true;
	}

	RenderBuffer* DX9QuadRenderer::attachRenderBuffer(const Group& group) const
	{
		BufferInfo info;
		ZeroMemory(&info, sizeof(info));
		info.nbVertices = group.getCapacity() << 2;
		info.nbIndices = group.getCapacity() * 6;
		return SPK_NEW(DX9Buffer,info);
	}

	void DX9QuadRenderer::render(const Group& group,const DataSet* dataSet,RenderBuffer* renderBuffer) const
	{
		SPK_ASSERT(renderBuffer != NULL,"DX9QuadRenderer::render(const Group&,const DataSet*,RenderBuffer*) - renderBuffer must not be NULL");
		DX9Buffer& buffer = static_cast<DX9Buffer&>(*renderBuffer);
		buffer.positionAtStart(); // Repositions all the buffers at the start

		D3DXMATRIX view,world,modelView;
		DX9Info::getDevice()->GetTransform(D3DTS_VIEW, &view);
		DX9Info::getDevice()->GetTransform(D3DTS_WORLD, &world);
		modelView = world * view;
		D3DXMatrixInverse((D3DXMATRIX*)&invModelView, NULL, &modelView);

		initBlending();
		initRenderingOptions();

		DX9Info::getDevice()->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_FLAT);
		DX9Info::getDevice()->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

		switch(texturingMode)
		{
		case TEXTURE_MODE_2D :
			// Creates and inits the 2D TexCoord buffer if necessary
			if (buffer.getNbTexCoords() != 2)
			{
				buffer.setNbTexCoords(2);
				if (!group.isEnabled(PARAM_TEXTURE_INDEX))
				{
					float t[8] = {0.0f,0.0f,1.0f,0.0f,1.0f,1.0f,0.0f,1.0f};
					buffer.lock(TEXCOORD_LOCK);
					for (size_t i = 0; i < group.getCapacity() << 3; ++i)
						buffer.setNextTexCoord(t[i & 7]);
					buffer.unlock();
				}
			}

			// Binds the texture
			DX9Info::getDevice()->SetTexture(0, textureIndex);
			DX9Info::getDevice()->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
			DX9Info::getDevice()->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
			DX9Info::getDevice()->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TEXTURE);
			DX9Info::getDevice()->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
			DX9Info::getDevice()->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
			DX9Info::getDevice()->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
			DX9Info::getDevice()->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);


			// Selects the correct function
			if (!group.isEnabled(PARAM_TEXTURE_INDEX))
			{
				if (!group.isEnabled(PARAM_ANGLE))
					renderParticle = &DX9QuadRenderer::render2D;
				else
					renderParticle = &DX9QuadRenderer::render2DRot;
			}
			else
			{
				if (!group.isEnabled(PARAM_ANGLE))
					renderParticle = &DX9QuadRenderer::render2DAtlas;
				else
					renderParticle = &DX9QuadRenderer::render2DAtlasRot;
			}
			break;

		case TEXTURE_MODE_3D :
			// Creates and inits the 3D TexCoord buffer if necessery
			if (buffer.getNbTexCoords() != 3)
			{
				buffer.setNbTexCoords(3);
				float t[12] = {0.0f,0.0f,0.0f,1.0f,0.0f,0.0f,1.0f,1.0f,0.0f,0.0f,1.0f,0.0f};
				buffer.lock(TEXCOORD_LOCK);
				for (size_t i = 0; i < group.getCapacity() * 12; ++i)
					buffer.setNextTexCoord(t[i % 12]);
				buffer.unlock();
			}

			// Binds the texture
			DX9Info::getDevice()->SetTexture(0, textureIndex);
			DX9Info::getDevice()->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
			DX9Info::getDevice()->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
			DX9Info::getDevice()->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TEXTURE);
			DX9Info::getDevice()->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
			DX9Info::getDevice()->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
			DX9Info::getDevice()->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
			DX9Info::getDevice()->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);


			// Selects the correct function
			if (!group.isEnabled(PARAM_ANGLE))
				renderParticle = &DX9QuadRenderer::render3D;
			else
				renderParticle = &DX9QuadRenderer::render3DRot;
			break;

		case TEXTURE_MODE_NONE :
			if (buffer.getNbTexCoords() != 0)
				buffer.setNbTexCoords(0);

			DX9Info::getDevice()->SetTexture(0, NULL);

			// Selects the correct function
			if (!group.isEnabled(PARAM_ANGLE))
				renderParticle = &DX9QuadRenderer::render2D;
			else
				renderParticle = &DX9QuadRenderer::render2DRot;
			break;
		}

		bool GlobalOrientation = precomputeOrientation3D(
			group,
			Vector3D(invModelView[8],invModelView[9],invModelView[10]),
			Vector3D(invModelView[4],invModelView[5],invModelView[6]),
			Vector3D(invModelView[12],invModelView[13],invModelView[14]));


		unsigned int lockType = VERTEX_AND_COLOR_LOCK;
		if (group.isEnabled(PARAM_TEXTURE_INDEX))
		{
			// enable lock on texcoord buffer
			lockType |= TEXCOORD_LOCK;
		}

		// Fills the buffers
		if( GlobalOrientation )
		{
			computeGlobalOrientation3D(group);

			buffer.lock(lockType);
			for (ConstGroupIterator particleIt(group); !particleIt.end(); ++particleIt)
				(this->*renderParticle)(*particleIt,buffer);
			buffer.unlock();
		}
		else
		{
			buffer.lock(lockType);
			for (ConstGroupIterator particleIt(group); !particleIt.end(); ++particleIt)
			{
				computeSingleOrientation3D(*particleIt);
				(this->*renderParticle)(*particleIt,buffer);
			}
			buffer.unlock();
		}

		buffer.render(D3DPT_TRIANGLELIST, group.getNbParticles());

		// reset texture at stage 0
		if( texturingMode != TEXTURE_MODE_NONE ) DX9Info::getDevice()->SetTexture( 0, NULL );
	}

	void DX9QuadRenderer::computeAABB(Vector3D& AABBMin,Vector3D& AABBMax,const Group& group,const DataSet* dataSet) const
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

	void DX9QuadRenderer::render2D(const Particle& particle,DX9Buffer& renderBuffer) const
	{
		scaleQuadVectors(particle,scaleX,scaleY);
		DX9CallColorAndVertex(particle,renderBuffer);
	}

	void DX9QuadRenderer::render2DRot(const Particle& particle,DX9Buffer& renderBuffer) const
	{
		rotateAndScaleQuadVectors(particle,scaleX,scaleY);
		DX9CallColorAndVertex(particle,renderBuffer);
	}

	void DX9QuadRenderer::render3D(const Particle& particle,DX9Buffer& renderBuffer) const
	{
		scaleQuadVectors(particle,scaleX,scaleY);
		DX9CallColorAndVertex(particle,renderBuffer);
		DX9CallTexture3D(particle,renderBuffer);
	}

	void DX9QuadRenderer::render3DRot(const Particle& particle,DX9Buffer& renderBuffer) const
	{
		rotateAndScaleQuadVectors(particle,scaleX,scaleY);
		DX9CallColorAndVertex(particle,renderBuffer);
		DX9CallTexture3D(particle,renderBuffer);
	}

	void DX9QuadRenderer::render2DAtlas(const Particle& particle,DX9Buffer& renderBuffer) const
	{
		scaleQuadVectors(particle,scaleX,scaleY);
		DX9CallColorAndVertex(particle,renderBuffer);
		DX9CallTexture2DAtlas(particle,renderBuffer);
	}

	void DX9QuadRenderer::render2DAtlasRot(const Particle& particle,DX9Buffer& renderBuffer) const
	{
		rotateAndScaleQuadVectors(particle,scaleX,scaleY);
		DX9CallColorAndVertex(particle,renderBuffer);
		DX9CallTexture2DAtlas(particle,renderBuffer);
	}
}}
