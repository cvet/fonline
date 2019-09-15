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
#include "SPK_GL_Buffer.h"

namespace SPK
{
namespace GL
{
	GLBuffer::GLBuffer(size_t nbVertices,size_t nbTexCoords) :
		nbVertices(nbVertices),
		nbTexCoords(nbTexCoords),
		texCoordBuffer(NULL),
		currentVertexIndex(0),
		currentColorIndex(0),
		currentTexCoordIndex(0)
	{
		SPK_ASSERT(nbVertices > 0,"GLBuffer::GLBuffer(size_t,size_t) - The number of vertices cannot be 0");

		vertexBuffer = SPK_NEW_ARRAY(Vector3D,nbVertices);
		colorBuffer = SPK_NEW_ARRAY(Color,nbVertices);

		if (nbTexCoords > 0)
			texCoordBuffer = SPK_NEW_ARRAY(float,nbVertices * nbTexCoords);
	}

	GLBuffer::~GLBuffer()
	{
		SPK_DELETE_ARRAY(vertexBuffer);
		SPK_DELETE_ARRAY(colorBuffer);
		SPK_DELETE_ARRAY(texCoordBuffer);
	}

	void GLBuffer::setNbTexCoords(size_t nb)
	{
		if (nbTexCoords != nb)
		{
			nbTexCoords = nb;
			SPK_DELETE_ARRAY(texCoordBuffer);
			if (nbTexCoords > 0)
				texCoordBuffer = SPK_NEW_ARRAY(float,nbVertices * nbTexCoords);
			currentTexCoordIndex = 0;
		}
	}

	void GLBuffer::render(GLuint primitive,size_t nbVertices)
	{
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);

		if (nbTexCoords > 0)
		{
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glTexCoordPointer(nbTexCoords,GL_FLOAT,0,texCoordBuffer);
		}

		glVertexPointer(3,GL_FLOAT,0,vertexBuffer);
		glColorPointer(4,GL_UNSIGNED_BYTE,0,colorBuffer);
	
		glDrawArrays(primitive,0,nbVertices);

		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);

		if (nbTexCoords > 0)
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
}}
