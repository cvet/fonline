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
#include "Rendering/Irrlicht/SPK_IRR_Buffer.h"

namespace SPK
{
namespace IRR
{
	IRRBuffer::IRRBuffer(irr::IrrlichtDevice* device,size_t nbParticles,size_t nbVerticesPerParticle,size_t nbIndicesPerParticle) :
		RenderBuffer(),
		device(device),
		meshBuffer(NULL),
		nbParticles(nbParticles),
		nbVerticesPerParticle(nbVerticesPerParticle),
		nbIndicesPerParticle(nbIndicesPerParticle),
		currentIndexIndex(0),
		currentVertexIndex(0),
		currentColorIndex(0),
		currentTexCoordIndex(0)
	{
		SPK_ASSERT(nbParticles > 0,"IRRBuffer::IRRBuffer(irr::IrrlichtDevice*,irr::scene::E_PRIMITIVE_TYPE,size_t) - The number of particles cannot be 0");
		SPK_ASSERT(nbVerticesPerParticle > 0,"IRRBuffer::IRRBuffer(irr::IrrlichtDevice*,irr::scene::E_PRIMITIVE_TYPE,size_t) - The number of vertices per particle cannot be 0");
		SPK_ASSERT(nbIndicesPerParticle > 0,"IRRBuffer::IRRBuffer(irr::IrrlichtDevice*,irr::scene::E_PRIMITIVE_TYPE,size_t) - The number of indices per particle cannot be 0");

		meshBuffer = new irr::scene::CDynamicMeshBuffer(irr::video::EVT_STANDARD,getIndiceType());
			
		// Allocates space for buffers
		setUsed(nbParticles);

		// TODO handle vbo
		meshBuffer->setHardwareMappingHint(irr::scene::EHM_NEVER,irr::scene::EBT_VERTEX_AND_INDEX);
	}

	IRRBuffer::~IRRBuffer()
	{
		device->getVideoDriver()->removeHardwareBuffer(meshBuffer);
		meshBuffer->drop();
	}

	void IRRBuffer::setUsed(size_t nb)
	{
		if (nb > nbParticles) // Prevents the buffers from growing too much
		{
			SPK_LOG_WARNING("IRRBuffer::setUsed(nb) - The number of active particles cannot be more than the initial capacity. Value is clamped")
			nb = nbParticles;
		}

		meshBuffer->getVertexBuffer().set_used(nb * nbVerticesPerParticle);
		meshBuffer->getIndexBuffer().set_used(nb * nbIndicesPerParticle);
	}
}}
