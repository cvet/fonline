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

#include <SPARK_Core.h>
#include "SPK_GL_LineRenderer.h"

namespace SPK
{
namespace GL
{
	GLLineRenderer::GLLineRenderer(float length,float width) :
		GLRenderer(false),
		LineRenderBehavior(length,width)
	{}

	GLLineRenderer::GLLineRenderer(const GLLineRenderer& renderer) :
		GLRenderer(renderer),
		LineRenderBehavior(renderer)
	{}

	RenderBuffer* GLLineRenderer::attachRenderBuffer(const Group& group) const
	{
		return SPK_NEW(GLBuffer,group.getCapacity() << 1);
	}

	void GLLineRenderer::render(const Group& group,const DataSet* dataSet,RenderBuffer* renderBuffer) const
	{
		SPK_ASSERT(renderBuffer != NULL,"GLLinesRenderer::render(const Group&,const DataSet*,RenderBuffer*) - renderBuffer must not be NULL");
		GLBuffer& buffer = static_cast<GLBuffer&>(*renderBuffer);
		buffer.positionAtStart();

		initBlending();
		initRenderingOptions();

		glLineWidth(width);
		glDisable(GL_TEXTURE_2D);
		glShadeModel(GL_FLAT);

		for (ConstGroupIterator particleIt(group); !particleIt.end(); ++particleIt)
		{
			const Particle& particle = *particleIt;

			buffer.setNextVertex(particle.position());
			buffer.setNextVertex(particle.position() + particle.velocity() * length);

			buffer.skipNextColors(1); // skips the first vertex color data as GL_FLAT was forced
			buffer.setNextColor(particle.getColor());
		}

		buffer.render(GL_LINES,group.getNbParticles() << 1);
	}

	void GLLineRenderer::computeAABB(Vector3D& AABBMin,Vector3D& AABBMax,const Group& group,const DataSet* dataSet) const
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
