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

#include <cstring> // for memmove and memcpy

#include <SPARK_Core.h>
#include "SPK_GL_LineTrailRenderer.h"

namespace SPK
{
namespace GL
{
	GLLineTrailRenderer::GLLineTrailRenderer(size_t nbSamples,float duration,float width) :
		GLRenderer(true),
		width(width),
		degeneratedColor(0x00000000)
	{
		setNbSamples(nbSamples);
		setDuration(duration);
	}

	GLLineTrailRenderer::GLLineTrailRenderer(const GLLineTrailRenderer& renderer) :
		GLRenderer(renderer),
        nbSamples(renderer.nbSamples),
		width(renderer.width),
        duration(renderer.duration),
        degeneratedColor(renderer.degeneratedColor)
	{}

	void GLLineTrailRenderer::setNbSamples(size_t nbSamples)
	{
		SPK_ASSERT(nbSamples >= 2,"GLLineTrailRenderer::setNbSamples(size_t) - The number of samples cannot be less than 2");
		this->nbSamples = nbSamples;
	}

	void GLLineTrailRenderer::setDuration(float duration)
	{
		SPK_ASSERT(nbSamples > 0.0f,"GLLineTrailRenderer::setDuration(float) - The duration cannot be less or equal to 0.0f");
		this->duration = duration;
	}

	void GLLineTrailRenderer::enableBlending(bool blendingEnabled)
	{
		if (!blendingEnabled)
			SPK_LOG_WARNING("GLLineTrailRenderer::enableBlending(bool) - The blending cannot be disabled for this renderer");
		GLRenderer::enableBlending(true);
	}

	void GLLineTrailRenderer::createData(DataSet& dataSet,const Group& group) const
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

	void GLLineTrailRenderer::checkData(DataSet& dataSet,const Group& group) const
	{
		// If the number of samples has changed, we must recreate the buffers
		if (SPK_GET_DATA(FloatArrayData,&dataSet,AGE_DATA_INDEX).getSizePerParticle() != nbSamples)
		{
			dataSet.destroyAllData();
			createData(dataSet,group);
		}
	}

	void GLLineTrailRenderer::init(const Particle& particle,DataSet* dataSet) const
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

	void GLLineTrailRenderer::update(const Group& group,DataSet* dataSet) const
	{
		Vector3D* vertexIt = SPK_GET_DATA(Vector3DArrayData,dataSet,VERTEX_BUFFER_INDEX).getData();
		Color* colorIt = SPK_GET_DATA(ColorArrayData,dataSet,COLOR_BUFFER_INDEX).getData();
		float* ageIt = SPK_GET_DATA(FloatArrayData,dataSet,AGE_DATA_INDEX).getData();
		unsigned char* startAlphaIt = SPK_GET_DATA(ArrayData<unsigned char>,dataSet,START_ALPHA_DATA_INDEX).getData();

		float ageStep = duration / (nbSamples - 1);
		for (ConstGroupIterator particleIt(group); !particleIt.end(); ++particleIt)
		{
			const Particle& particle = *particleIt;
			float age = particle.getAge();

			if (age - *(ageIt + 1) >= ageStep) // shifts the data by one
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

	void GLLineTrailRenderer::render(const Group& group,const DataSet* dataSet,RenderBuffer* renderBuffer) const
	{
		// RenderBuffer is not used as dataset already contains organized data for rendering
		const Vector3D* vertexBuffer = SPK_GET_DATA(const Vector3DArrayData,dataSet,VERTEX_BUFFER_INDEX).getData();
		const Color* colorBuffer = SPK_GET_DATA(const ColorArrayData,dataSet,COLOR_BUFFER_INDEX).getData();

		initBlending();
		initRenderingOptions();

		// Inits lines' parameters
		glLineWidth(width);
		glDisable(GL_TEXTURE_2D);
		glShadeModel(GL_SMOOTH);

		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);

		glVertexPointer(3,GL_FLOAT,0,vertexBuffer);
		glColorPointer(4,GL_UNSIGNED_BYTE,0,colorBuffer);

		glDrawArrays(GL_LINE_STRIP,0,group.getNbParticles() * (nbSamples + 2));

		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
	}

	void GLLineTrailRenderer::computeAABB(Vector3D& AABBMin,Vector3D& AABBMax,const Group& group,const DataSet* dataSet) const
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
