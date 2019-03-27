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
#include "Rendering/Irrlicht/SPK_IRR_LineRenderer.h"

namespace SPK
{
namespace IRR
{
	IRRLineRenderer::IRRLineRenderer(irr::IrrlichtDevice* d,float length,float width) :
		IRRRenderer(d),
		LineRenderBehavior(length,width)
	{
		material.Thickness = width;
	}

	IRRLineRenderer::IRRLineRenderer(const IRRLineRenderer& renderer) :
		IRRRenderer(renderer),
		LineRenderBehavior(renderer)
	{}

	RenderBuffer* IRRLineRenderer::attachRenderBuffer(const Group& group) const
	{
		// Creates the render buffer
		IRRBuffer* buffer = SPK_NEW(IRRBuffer,device,group.getCapacity(),NB_VERTICES_PER_PARTICLE,NB_INDICES_PER_PARTICLE);
	
		buffer->positionAtStart();

		// Initializes the index array
		for (size_t i = 0; i < group.getCapacity() * NB_INDICES_PER_PARTICLE; ++i)
			buffer->setNextIndex(i);

		buffer->getMeshBuffer().setDirty(irr::scene::EBT_INDEX);

		return buffer;
	}

	void IRRLineRenderer::render(const Group& group,const DataSet* dataSet,RenderBuffer* renderBuffer) const
	{
		SPK_ASSERT(renderBuffer != NULL,"IRRLineRenderer::render(const Group&,const DataSet*,RenderBuffer*) - renderBuffer must not be NULL");
		IRRBuffer& buffer = static_cast<IRRBuffer&>(*renderBuffer);
		
		buffer.positionAtStart();
		for (ConstGroupIterator particleIt(group); !particleIt.end(); ++particleIt)
		{
			buffer.setNextVertex(particleIt->position());
			buffer.setNextVertex(particleIt->position() + particleIt->velocity() * length);

			const Color& color = particleIt->getColor();
			buffer.setNextColor(color);
			buffer.setNextColor(color);
		}
		buffer.getMeshBuffer().setDirty(irr::scene::EBT_VERTEX);

		irr::video::IVideoDriver* driver = device->getVideoDriver();
		driver->setMaterial(material);
		driver->drawVertexPrimitiveList(
			buffer.getMeshBuffer().getVertexBuffer().pointer(),
			group.getNbParticles() * NB_VERTICES_PER_PARTICLE,
			buffer.getMeshBuffer().getIndexBuffer().pointer(),
			group.getNbParticles(),
			irr::video::EVT_STANDARD,
			irr::scene::EPT_LINES,
			buffer.getMeshBuffer().getIndexBuffer().getType());
	}

	void IRRLineRenderer::computeAABB(Vector3D& AABBMin,Vector3D& AABBMax,const Group& group,const DataSet* dataSet) const
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
