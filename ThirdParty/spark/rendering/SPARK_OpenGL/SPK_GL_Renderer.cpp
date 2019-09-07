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

#ifndef SPK_GL_NO_EXT
#include <GL/glew.h>
#endif

#include <SPARK_Core.h>
#include "SPK_GL_Renderer.h"

namespace SPK
{
namespace GL
{
#ifndef SPK_GL_NO_EXT
	GLRenderer::GlewStatus GLRenderer::glewStatus = GLRenderer::GLEW_UNLOADED;
#endif

	void GLRenderer::setBlendMode(BlendMode blendMode)
	{
		switch(blendMode)
		{
		case BLEND_MODE_NONE :
			srcBlending = GL_ONE;
			destBlending = GL_ZERO;
			blendingEnabled = false;
			break;

		case BLEND_MODE_ADD :
			srcBlending = GL_SRC_ALPHA;
			destBlending = GL_ONE;
			blendingEnabled = true;
			break;

		case BLEND_MODE_ALPHA :
			srcBlending = GL_SRC_ALPHA;
			destBlending = GL_ONE_MINUS_SRC_ALPHA;
			blendingEnabled = true;
			break;

		default :
			SPK_LOG_WARNING("GLRenderer::setBlendMode(BlendMode) - Unsupported blending mode. Nothing happens");
			break;
		}
	}

	void GLRenderer::saveGLStates()
	{
		glPushAttrib(GL_POINT_BIT |
			GL_LINE_BIT |
			GL_ENABLE_BIT |
			GL_COLOR_BUFFER_BIT |
			GL_CURRENT_BIT |
			GL_TEXTURE_BIT |
			GL_DEPTH_BUFFER_BIT |
			GL_LIGHTING_BIT |
			GL_POLYGON_BIT);
	}

	void GLRenderer::restoreGLStates()
	{
		glPopAttrib();
	}

#ifndef SPK_GL_NO_EXT
	bool GLRenderer::checkExtension(GLboolean* glExt)
	{
		if (glewStatus == GLEW_UNLOADED)
		{
			int error = glewInit();
			if (error == GLEW_OK)
				glewStatus = GLEW_SUPPORTED;
			else
			{
				SPK_LOG_ERROR("GLRenderer::checkExtensions(bool) - Cannot initialize Glew library")
				glewStatus = GLEW_UNSUPPORTED;
			}
		}
		if (glewStatus == GLEW_SUPPORTED)
			return *glExt != 0;
		return false;	
	}
#endif
}}
