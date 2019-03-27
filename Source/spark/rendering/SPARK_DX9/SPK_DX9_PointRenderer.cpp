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
#include "Rendering/DX9/SPK_DX9_PointRenderer.h"
#include "Rendering/DX9/SPK_DX9_Buffer.h"

namespace SPK
{
namespace DX9
{
	const float DX9PointRenderer::QUADRATIC_SCREEN[3] = {1.0f, 0.0f, 0.0f};

	float DX9PointRenderer::pixelPerUnit = 1024.0f;

	const float DX9PointRenderer::POINT_SIZE_CURRENT 	= 32.0f;
	const float DX9PointRenderer::POINT_SIZE_MIN 		= 1.0f;
	const float DX9PointRenderer::POINT_SIZE_MAX 		= 1024.0f;

	bool DX9PointRenderer::setType(PointType type)
	{
		SPK_LOG_WARNING("DX9PointRenderer::setType(PointType) - only POINT_TYPE_SPRITE is available with this renderer");

		return true;
	}

	bool DX9PointRenderer::enableWorldSize(bool worldSizeEnabled)
	{
		worldSize = worldSizeEnabled;
		return worldSize;
	}

	RenderBuffer* DX9PointRenderer::attachRenderBuffer(const Group& group) const
	{
		BufferInfo info;
		ZeroMemory(&info, sizeof(info));
		info.nbVertices = group.getCapacity();
		return SPK_NEW(DX9Buffer,info);
	}

	void DX9PointRenderer::render(const Group& group,const DataSet* dataSet,RenderBuffer* renderBuffer) const
	{
		SPK_ASSERT(renderBuffer != NULL,"DX9PointRenderer::render(const Group&,const DataSet*,RenderBuffer*) - renderBuffer must not be NULL");
		DX9Buffer& buffer = static_cast<DX9Buffer&>(*renderBuffer);
		buffer.positionAtStart(); // Repositions all the buffers at the start

		buffer.lock(VERTEX_AND_COLOR_LOCK);
		for( ConstGroupIterator particleIt(group); !particleIt.end(); ++particleIt )
		{
			buffer.setNextVertex(particleIt->position());
			buffer.setNextColor(particleIt->getColor());
		}
		buffer.unlock();

		DX9Info::getDevice()->SetRenderState(D3DRS_POINTSPRITEENABLE, true);

		// Sets the different states for rendering
		initBlending();
		initRenderingOptions();

		if (worldSize)
		{
			enablePointParameter(group.getGraphicalRadius() * worldScale * 2.0f, true);
			DX9Info::getDevice()->SetRenderState(D3DRS_POINTSCALEENABLE, true);
		}
		else
		{
			DX9Info::getDevice()->SetRenderState(D3DRS_POINTSIZE, FtoDW(screenSize));
			DX9Info::getDevice()->SetRenderState(D3DRS_POINTSCALEENABLE, false);
		}

		DX9Info::getDevice()->SetTexture(0, textureIndex);
		DX9Info::getDevice()->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		DX9Info::getDevice()->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
		DX9Info::getDevice()->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TEXTURE);
		DX9Info::getDevice()->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		DX9Info::getDevice()->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
		DX9Info::getDevice()->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		DX9Info::getDevice()->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

		// Sends the data to the GPU
		buffer.render(D3DPT_POINTLIST, group.getNbParticles());

		// reset texture at stage 0
		DX9Info::getDevice()->SetTexture( 0, NULL );
	}

	void DX9PointRenderer::computeAABB(Vector3D& AABBMin,Vector3D& AABBMax,const Group& group,const DataSet* dataSet) const
	{
		if (isWorldSizeEnabled())
		{
			float radius = group.getGraphicalRadius();
			for (ConstGroupIterator particleIt(group); !particleIt.end(); ++particleIt)
			{
				AABBMin.setMin(particleIt->position() - radius);
				AABBMax.setMax(particleIt->position() + radius);
			}
		}
		else
			for (ConstGroupIterator particleIt(group); !particleIt.end(); ++particleIt)
			{
				AABBMin.setMin(particleIt->position());
				AABBMax.setMax(particleIt->position());
			}
	}

	void DX9PointRenderer::enablePointParameter(float size,bool distance)
	{
		// derived size = size * sqrt(1 / (A + B * distance + C * distance²))
		if (distance)
		{
			const float sqrtC = POINT_SIZE_CURRENT / (size * pixelPerUnit);
			const float QUADRATIC_WORLD[3] = {0.0f,0.0f,sqrtC * sqrtC}; // A = 0; B = 0; C = (POINT_SIZE_CURRENT / (size * pixelPerUnit))²
			DX9Info::getDevice()->SetRenderState(D3DRS_POINTSCALE_A, FtoDW(QUADRATIC_WORLD[0]));
			DX9Info::getDevice()->SetRenderState(D3DRS_POINTSCALE_B, FtoDW(QUADRATIC_WORLD[1]));
			DX9Info::getDevice()->SetRenderState(D3DRS_POINTSCALE_C, FtoDW(QUADRATIC_WORLD[2]));
		}
		else
		{
			const float sqrtA = POINT_SIZE_CURRENT / size;
			const float QUADRATIC_WORLD[3] = {sqrtA * sqrtA,0.0f,0.0f};	// A = (POINT_SIZE_CURRENT / size)²; B = 0; C = 0
			DX9Info::getDevice()->SetRenderState(D3DRS_POINTSCALE_A, FtoDW(QUADRATIC_WORLD[0]));
			DX9Info::getDevice()->SetRenderState(D3DRS_POINTSCALE_B, FtoDW(QUADRATIC_WORLD[1]));
			DX9Info::getDevice()->SetRenderState(D3DRS_POINTSCALE_C, FtoDW(QUADRATIC_WORLD[2]));
		}

		DX9Info::getDevice()->SetRenderState(D3DRS_POINTSIZE, FtoDW(POINT_SIZE_CURRENT));
		DX9Info::getDevice()->SetRenderState(D3DRS_POINTSIZE_MIN, FtoDW(POINT_SIZE_MIN));
		DX9Info::getDevice()->SetRenderState(D3DRS_POINTSIZE_MAX, FtoDW(POINT_SIZE_MAX));
	}
}}
