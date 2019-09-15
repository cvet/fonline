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

#include <cstring> // for memmove and memcpy

#include <SPARK_Core.h>
#include "Rendering/DX9/SPK_DX9_LineTrailRenderer.h"

namespace SPK
{
namespace DX9
{
	DX9LineTrailRenderer::DX9LineTrailRenderer(size_t nbSamples,float duration,float width) :
		DX9Renderer(true),
		width(width),
		degeneratedColor(0x00000000)
	{
		setNbSamples(nbSamples);
		setDuration(duration);
	}

	DX9LineTrailRenderer::DX9LineTrailRenderer(const DX9LineTrailRenderer& renderer) :
		DX9Renderer(renderer),
		width(renderer.width),
		degeneratedColor(renderer.degeneratedColor),
		nbSamples(renderer.nbSamples),
		duration(renderer.duration)
	{}

	void DX9LineTrailRenderer::setNbSamples(size_t nbSamples)
	{
		SPK_ASSERT(nbSamples >= 2,"DX9LineTrailRenderer::setNbSamples(size_t) - The number of samples cannot be less than 2");
		this->nbSamples = nbSamples;
	}

	void DX9LineTrailRenderer::setDuration(float duration)
	{
		SPK_ASSERT(nbSamples > 0.0f,"DX9LineTrailRenderer::setDuration(float) - The duration cannot be less or equal to 0.0f");
		this->duration = duration;
	}

	void DX9LineTrailRenderer::enableBlending(bool blendingEnabled)
	{
		if (!blendingEnabled)
			SPK_LOG_WARNING("DX9LineTrailRenderer::enableBlending(bool) - The blending cannot be disables for this renderer");
		DX9Renderer::enableBlending(true);
	}

	void DX9LineTrailRenderer::createData(DataSet& dataSet,const Group& group) const
	{
		dataSet.init(NB_DATA);
		dataSet.setData(VERTEX_BUFFER_INDEX,SPK_NEW(Vector3DArrayData,group.getCapacity(),nbSamples + 2));
		dataSet.setData(COLOR_BUFFER_INDEX,SPK_NEW(ColorArrayData,group.getCapacity(),nbSamples + 2));
		dataSet.setData(AGE_DATA_INDEX,SPK_NEW(FloatArrayData,group.getCapacity(),nbSamples));
		dataSet.setData(START_ALPHA_DATA_INDEX,SPK_NEW(ArrayData<unsigned char>,group.getCapacity(),nbSamples));

		// Inits the buffers
		for (ConstGroupIterator particleIt(group); !particleIt.end(); ++particleIt)
			init(*particleIt,&dataSet);
	}

	void DX9LineTrailRenderer::checkData(DataSet& dataSet,const Group& group) const
	{
		// If the number of samples has changed, we must recreate the buffers
		if (SPK_GET_DATA(FloatArrayData,&dataSet,AGE_DATA_INDEX).getSizePerParticle() != nbSamples)
		{
			dataSet.destroyAllData();
			createData(dataSet,group);
		}
	}

	void DX9LineTrailRenderer::init(const Particle& particle,DataSet* dataSet) const
	{
		size_t index = particle.getIndex();
		Vector3D* vertexIt = SPK_GET_DATA(Vector3DArrayData,dataSet,VERTEX_BUFFER_INDEX).getParticleData(index);
		Color* colorIt = SPK_GET_DATA(ColorArrayData,dataSet,COLOR_BUFFER_INDEX).getParticleData(index);
		float* ageIt = SPK_GET_DATA(FloatArrayData,dataSet,AGE_DATA_INDEX).getParticleData(index);
		unsigned char* startAlphaIt = SPK_GET_DATA(ArrayData<unsigned char>,dataSet,START_ALPHA_DATA_INDEX).getParticleData(index);

		// Gets the particle's values
		const Vector3D& pos = particle.position();
		const Color& color = particle.getColor();
		float age = particle.getAge();

		// Inits position
		for (size_t i = 0; i < nbSamples + 2; ++i)
			*(vertexIt++) = pos;

		// Inits color
		*(colorIt++) = degeneratedColor; // degenerate pre vertex
		for (size_t i = 0; i < nbSamples; ++i)
			*(colorIt++) = color;
		*colorIt = degeneratedColor; // degenerate post vertex

		// Inits age
		for (size_t i = 0; i < nbSamples; ++i)
			*(ageIt++) = age;

		// Inits start alpha
		for (size_t i = 0; i < nbSamples; ++i)
			*(startAlphaIt++) = color.a;
	}

	void DX9LineTrailRenderer::update(const Group& group,DataSet* dataSet) const
	{
		Vector3D* vertexIt = SPK_GET_DATA(Vector3DArrayData,dataSet,VERTEX_BUFFER_INDEX).getData();
		Color* colorIt = SPK_GET_DATA(ColorArrayData,dataSet,COLOR_BUFFER_INDEX).getData();
		float* ageIt = SPK_GET_DATA(FloatArrayData,dataSet,AGE_DATA_INDEX).getData();
		unsigned char* startAlphaIt = SPK_GET_DATA(ArrayData<unsigned char>,dataSet,START_ALPHA_DATA_INDEX).getData();

		for (ConstGroupIterator particleIt(group); !particleIt.end(); ++particleIt)
		{
			const Particle& particle = *particleIt;
			float age = particle.getAge();

			if (age - *(ageIt + 1) >= duration / (nbSamples - 1)) // shifts the data by one
			{
				std::memmove(vertexIt + 2,vertexIt + 1,(nbSamples - 1) * sizeof(Vector3D));
				std::memmove(colorIt + 2,colorIt + 1,(nbSamples - 1) * sizeof(Color));
				std::memmove(ageIt + 1,ageIt,(nbSamples - 1) * sizeof(float));
				std::memmove(startAlphaIt + 1,startAlphaIt,(nbSamples - 1) * sizeof(unsigned char));

				// post degenerated vertex copy
				std::memcpy(vertexIt + (nbSamples + 1),vertexIt + nbSamples,sizeof(Vector3D));
			}

			// Updates the current sample
			*(vertexIt++) = particle.position();
			std::memcpy(vertexIt,vertexIt - 1,sizeof(Vector3D));
			vertexIt += nbSamples + 1;

			++colorIt; // skips post degenerated vertex color
			*(colorIt++) = particle.getColor();
			*(startAlphaIt++) = particle.getColor().a;

			*(ageIt++) = age;

			// Updates alpha
			for (size_t i = 0; i < nbSamples - 1; ++i)
			{
				float ratio = 1.0f - (age - *(ageIt++)) / duration;
				(colorIt++)->a = static_cast<unsigned char>(*(startAlphaIt++) * (ratio > 0.0f ? ratio : 0.0f));
			}
			++colorIt;
		}
	}

	RenderBuffer* DX9LineTrailRenderer::attachRenderBuffer(const Group& group) const
	{
		BufferInfo info;
		ZeroMemory(&info, sizeof(info));
		info.nbVertices = group.getCapacity() * (nbSamples + 2);
		return SPK_NEW(DX9Buffer,info);
	}

	void DX9LineTrailRenderer::render(const Group& group,const DataSet* dataSet,RenderBuffer* renderBuffer) const
	{
		// RenderBuffer is not used as dataset already contains organized data for rendering
		const Vector3D* vertexBuffer = SPK_GET_DATA(const Vector3DArrayData,dataSet,VERTEX_BUFFER_INDEX).getData();
		const Color* colorBuffer = SPK_GET_DATA(const ColorArrayData,dataSet,COLOR_BUFFER_INDEX).getData();

		DX9Buffer& buffer = static_cast<DX9Buffer&>(*renderBuffer);
		buffer.positionAtStart(); // Repositions all the buffers at the start

		initBlending();
		initRenderingOptions();

		// Inits lines' parameters
		DX9Info::getDevice()->SetRenderState( D3DRS_SHADEMODE, D3DSHADE_GOURAUD );

		buffer.lock(VERTEX_AND_COLOR_LOCK);

		for( unsigned int i = 0; i < group.getNbParticles() * (nbSamples + 2); ++i )
		{
			buffer.setNextVertex(vertexBuffer[i]);
			buffer.setNextColor(colorBuffer[i]);
		}
		// WARNING : D3DXVECTOR3 and Vector3D are the same in memory now
		//buffer.copy(VERTEX_LOCK, (void *)vertexBuffer, group.getNbParticles() * (nbSamples + 2) * sizeof(D3DXVECTOR3));
		//buffer.copy(COLOR_LOCK, (void *)colorBuffer, group.getNbParticles() * (nbSamples + 2) * sizeof(DWORD));

		buffer.unlock();

		buffer.render(D3DPT_LINESTRIP, group.getNbParticles() * (nbSamples + 2) - 1);
	}

	void DX9LineTrailRenderer::computeAABB(Vector3D& AABBMin,Vector3D& AABBMax,const Group& group,const DataSet* dataSet) const
	{
		const Vector3D* vertexIt = SPK_GET_DATA(const Vector3DArrayData,dataSet,VERTEX_BUFFER_INDEX).getData();

		for (ConstGroupIterator particleIt(group); !particleIt.end(); ++particleIt)
		{
			++vertexIt; // skips pre degenerated vertex
			for (size_t i = 0; i < nbSamples; ++i)
			{
				AABBMin.setMin(*vertexIt);
				AABBMax.setMax(*vertexIt);
				++vertexIt;
			}
			++vertexIt; // skips post degenerated vertex
		}
	}
}}
