//////////////////////////////////////////////////////////////////////////////////
// SPARK particle engine														//
// Copyright (C) 2008-2009 - Julien Fryer - julienfryer@gmail.com				//
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

#ifndef SPK_GL_NO_EXT
#include <GL/glew.h>
#endif

#include <SPARK_Core.h>
#include "SPK_GL_PointRenderer.h"

namespace SPK
{
namespace GL
{
	float GLPointRenderer::pixelPerUnit = 1024.0f;

#ifndef SPK_GL_NO_EXT
	GLboolean* const GLPointRenderer::SPK_GL_POINT_SPRITE_EXT = &__GLEW_ARB_point_sprite;
	GLboolean* const GLPointRenderer::SPK_GL_POINT_PARAMETERS_EXT = &__GLEW_EXT_point_parameters;
#endif
	bool GLPointRenderer::isPointSpriteSupported()	{ return SPK_GL_CHECK_EXTENSION(SPK_GL_POINT_SPRITE_EXT); }
	bool GLPointRenderer::isWorldSizeSupported()	{ return SPK_GL_CHECK_EXTENSION(SPK_GL_POINT_PARAMETERS_EXT); }


	bool GLPointRenderer::setType(PointType type)
	{
		if ((type == POINT_TYPE_SPRITE)&&(!SPK_GL_CHECK_EXTENSION(SPK_GL_POINT_SPRITE_EXT)))
		{
			SPK_LOG_WARNING("GLPointRenderer::setType(PointType) - POINT_TYPE_SPRITE is not available on the hardware");
			return false;
		}

		this->type = type;
		return true;
	}

	bool GLPointRenderer::enableWorldSize(bool worldSizeEnabled)
	{
		worldSize = (worldSizeEnabled && SPK_GL_CHECK_EXTENSION(SPK_GL_POINT_PARAMETERS_EXT));
		if (worldSize != worldSizeEnabled)
			SPK_LOG_WARNING("GLPointRenderer::enableWorldSize(true) - World size for points is not available on the hardware");
		return worldSize;
	}

	void GLPointRenderer::render(const Group& group,const DataSet* dataSet,RenderBuffer* renderBuffer) const
	{
		// Sets the different states for rendering
		initBlending();
		initRenderingOptions();

		switch(type)
		{
		case POINT_TYPE_SQUARE :
			glDisable(GL_TEXTURE_2D);
			glDisable(GL_POINT_SMOOTH);
#ifndef SPK_GL_NO_EXT
			if (SPK_GL_CHECK_EXTENSION(SPK_GL_POINT_SPRITE_EXT))
				glDisable(GL_POINT_SPRITE_ARB);
#endif
			break;

		case POINT_TYPE_SPRITE :
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D,textureIndex);
#ifndef SPK_GL_NO_EXT
			glTexEnvf(GL_POINT_SPRITE_ARB,GL_COORD_REPLACE_ARB,GL_TRUE);
			glEnable(GL_POINT_SPRITE_ARB);
#endif
			break;

		case POINT_TYPE_CIRCLE :
			glDisable(GL_TEXTURE_2D);
			glEnable(GL_POINT_SMOOTH);
#ifndef SPK_GL_NO_EXT
			if (SPK_GL_CHECK_EXTENSION(SPK_GL_POINT_SPRITE_EXT))
				glDisable(GL_POINT_SPRITE_ARB);
#endif
			break;
		}

		if (worldSize) 
		{
#ifndef SPK_GL_NO_EXT
			// derived size = size * sqrt(1 / (A + B * distance + C * distance²))
			const float POINT_SIZE_CURRENT = 32.0f;
			const float POINT_SIZE_MIN = 1.0f;
			const float POINT_SIZE_MAX = 1024.0f;
			const float sqrtC = POINT_SIZE_CURRENT / (group.getGraphicalRadius() * worldScale * 2.0f * pixelPerUnit);
			const float QUADRATIC_WORLD[3] = {0.0f,0.0f,sqrtC * sqrtC}; // A = 0; B = 0; C = (POINT_SIZE_CURRENT / (size * pixelPerUnit))²
			glPointParameterfvEXT(GL_DISTANCE_ATTENUATION_EXT,QUADRATIC_WORLD);
			glPointSize(POINT_SIZE_CURRENT);
			glPointParameterfEXT(GL_POINT_SIZE_MIN_EXT,POINT_SIZE_MIN);
			glPointParameterfEXT(GL_POINT_SIZE_MAX_EXT,POINT_SIZE_MAX);
#endif
		}
		else
		{
			glPointSize(screenSize);
#ifndef SPK_GL_NO_EXT
			if (SPK_GL_CHECK_EXTENSION(SPK_GL_POINT_PARAMETERS_EXT)) 
			{
				const float QUADRATIC_SCREEN[3] = {1.0f,0.0f,0.0f};
				glPointParameterfvEXT(GL_DISTANCE_ATTENUATION_EXT,QUADRATIC_SCREEN);
			}
#endif
		}

		// Sends the data to the GPU
		// RenderBuffer is not used as the data are already well organized for rendering
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);

		glVertexPointer(3,GL_FLOAT,0,group.getPositionAddress());
		glColorPointer(4,GL_UNSIGNED_BYTE,0,group.getColorAddress());

		glDrawArrays(GL_POINTS,0,group.getNbParticles());

		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
	}

	void GLPointRenderer::computeAABB(Vector3D& AABBMin,Vector3D& AABBMax,const Group& group,const DataSet* dataSet) const
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
}}
