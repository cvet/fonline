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
#include "Rendering/DX9/SPK_DX9_LineRenderer.h"

namespace SPK
{
namespace DX9
{
	DX9LineRenderer::DX9LineRenderer(float length) :
		DX9Renderer(false),
		LineRenderBehavior(length, 1.0f)
	{}

	DX9LineRenderer::DX9LineRenderer(const DX9LineRenderer& renderer) :
		DX9Renderer(renderer),
		LineRenderBehavior(renderer)
	{}

	RenderBuffer* DX9LineRenderer::attachRenderBuffer(const Group& group) const
	{
		BufferInfo info;
		ZeroMemory(&info, sizeof(info));
		info.nbVertices = group.getCapacity() << 1;
		return SPK_NEW(DX9Buffer,info);
	}

	void DX9LineRenderer::render(const Group& group,const DataSet* dataSet,RenderBuffer* renderBuffer) const
	{
		SPK_ASSERT(renderBuffer != NULL,"DX9LinesRenderer::render(const Group&,const DataSet*,RenderBuffer*) - renderBuffer must not be NULL");
		DX9Buffer& buffer = static_cast<DX9Buffer&>(*renderBuffer);
		buffer.positionAtStart();

		initBlending();
		initRenderingOptions();

		DX9Info::getDevice()->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_FLAT);

		for (ConstGroupIterator particleIt(group); !particleIt.end(); ++particleIt)
		{
			const Particle& particle = *particleIt;

			buffer.setNextVertex(particle.position());
			buffer.setNextVertex(particle.position() + particle.velocity() * length);

			buffer.setNextColor(particle.getColor());
			buffer.setNextColor(particle.getColor());
		}

		buffer.render(D3DPT_LINELIST, group.getNbParticles());
	}

	void DX9LineRenderer::computeAABB(Vector3D& AABBMin,Vector3D& AABBMax,const Group& group,const DataSet* dataSet) const
	{
		for (ConstGroupIterator particleIt(group); !particleIt.end(); ++particleIt)
		{
			Vector3D v = particleIt->position() + particleIt->velocity() * length;
			AABBMin.setMin(particleIt->position());
			AABBMin.setMin(v);
			AABBMax.setMax(particleIt->position());
			AABBMax.setMax(v);
		}
	}
}}
