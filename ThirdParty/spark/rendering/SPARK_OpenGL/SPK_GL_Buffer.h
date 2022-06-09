//////////////////////////////////////////////////////////////////////////////////
// SPARK particle engine														//
// Copyright (C) 2008-2013 - Julien Fryer - julienfryer@gmail.com				//
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

#ifndef H_SPK_GL_BUFFER
#define H_SPK_GL_BUFFER

#include "SPK_GL_DEF.h"

namespace SPK
{
namespace GL
{
	class SPK_GL_PREFIX GLBuffer : public RenderBuffer
	{
	public :

		GLBuffer(size_t nbVertices,size_t nbTexCoords = 0);
		~GLBuffer();

		void positionAtStart();

		void setNextVertex(const Vector3D& vertex);

		void setNextColor(const Color& color);
		void skipNextColors(size_t nb);

		void setNextTexCoord(float texCoord);
		void skipNextTexCoords(size_t nb);

		void setNbTexCoords(size_t nb);
		size_t getNbTexCoords();

		void render(GLuint primitive,size_t nbVertices, bool useTextureColor, bool useTextureAlpha);

	private :

		GLuint vbo;
		GLuint ibo;

		const size_t nbVertices;
		size_t nbTexCoords;

		Vector3D* vertexBuffer;
		Color* colorBuffer;
		float* texCoordBuffer;

		size_t currentVertexIndex;
		size_t currentColorIndex;
		size_t currentTexCoordIndex;
	};

	inline void GLBuffer::positionAtStart()
	{
		currentVertexIndex = 0;
		currentColorIndex = 0;
		currentTexCoordIndex = 0;
	}

	inline void GLBuffer::setNextVertex(const Vector3D& vertex)
	{
		vertexBuffer[currentVertexIndex++] = vertex;
	}

	inline void GLBuffer::setNextColor(const Color& color)
	{
		colorBuffer[currentColorIndex++] = color;
	}

	inline void GLBuffer::skipNextColors(size_t nb)
	{
		currentColorIndex += nb;
	}

	inline void GLBuffer::setNextTexCoord(float texCoord)
	{
		texCoordBuffer[currentTexCoordIndex++] = texCoord;
	}

	inline void GLBuffer::skipNextTexCoords(size_t nb)
	{
		currentTexCoordIndex += nb;
	}

	inline size_t GLBuffer::getNbTexCoords()
	{
		return nbTexCoords;
	}
}}

#endif
