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

#ifndef H_SPK_IRRBUFFER
#define H_SPK_IRRBUFFER

#include "Rendering/Irrlicht/SPK_IRR_DEF.h"

namespace SPK
{
namespace IRR
{
	class SPK_IRR_PREFIX IRRBuffer : public RenderBuffer
	{
	public :

		IRRBuffer(irr::IrrlichtDevice* device,size_t nbParticles,size_t nbVerticesPerParticle,size_t nbIndicesPerParticle);
		~IRRBuffer();

		irr::video::E_INDEX_TYPE getIndiceType() const;

		void positionAtStart();

		void setNextIndex(int index);
		void setNextVertex(const Vector3D& vertex);

		void setNextColor(const Color& color);
		void skipNextColors(size_t nb);

		void setNextTexCoords(float u,float v);
		void skipNextTexCoords(size_t nb);

		irr::scene::IDynamicMeshBuffer& getMeshBuffer();
		const irr::scene::IDynamicMeshBuffer& getMeshBuffer() const;

		void setUsed(size_t nb);

	private :

		irr::scene::CDynamicMeshBuffer* meshBuffer;
		irr::IrrlichtDevice* device;

		size_t nbParticles;
		size_t nbVerticesPerParticle;
		size_t nbIndicesPerParticle;

		size_t currentIndexIndex;
		size_t currentVertexIndex;
		size_t currentColorIndex;
		size_t currentTexCoordIndex;
	};

	inline irr::video::E_INDEX_TYPE IRRBuffer::getIndiceType() const
	{
		return nbParticles * nbVerticesPerParticle > 65536 ? irr::video::EIT_32BIT : irr::video::EIT_16BIT;
	}

	inline void IRRBuffer::positionAtStart()
	{
		currentIndexIndex = 0;
		currentVertexIndex = 0;
		currentColorIndex = 0;
		currentTexCoordIndex = 0;
	}

	inline void IRRBuffer::setNextIndex(int index)
	{
		meshBuffer->getIndexBuffer().setValue(currentIndexIndex++,index);
	}

	inline void IRRBuffer::setNextVertex(const Vector3D& vertex)
	{
		meshBuffer->getVertexBuffer()[currentVertexIndex++].Pos.set(
			vertex.x,
			vertex.y,
			vertex.z);
	}

	inline void IRRBuffer::setNextColor(const Color& color)
	{
		meshBuffer->getVertexBuffer()[currentColorIndex++].Color.set(color.getARGB());
	}

	inline void IRRBuffer::skipNextColors(size_t nb)
	{
		currentColorIndex += nb;
	}

	inline void IRRBuffer::setNextTexCoords(float u,float v)
	{
		meshBuffer->getVertexBuffer()[currentTexCoordIndex++].TCoords.set(u,v);
	}

	inline void IRRBuffer::skipNextTexCoords(size_t nb)
	{
		currentTexCoordIndex += nb;
	}

	inline irr::scene::IDynamicMeshBuffer& IRRBuffer::getMeshBuffer()
	{
		return *meshBuffer;
	}

	inline const irr::scene::IDynamicMeshBuffer& IRRBuffer::getMeshBuffer() const
	{
		return *meshBuffer;
	}
}}

#endif
