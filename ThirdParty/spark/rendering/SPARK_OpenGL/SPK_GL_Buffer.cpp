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

#include "GL/glew.h"
#include <SPARK_Core.h>
#include "SPK_GL_Buffer.h"

static GLuint CreateShaderProgram(const GLchar* vertex_shader, const GLchar* fragment_shader)
{
	// Create shaders
	const GLchar* vertex_shader_arr[1] = { vertex_shader };
	const auto vert = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vert, 1, vertex_shader_arr, nullptr);
	glCompileShader(vert);
	GLint vs_status = 0;
	glGetShaderiv(vert, GL_COMPILE_STATUS, &vs_status);
	assert(vs_status);

	const GLchar* fragment_shader_arr[1] = { fragment_shader };
	const auto frag = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(frag, 1, fragment_shader_arr, nullptr);
	glCompileShader(frag);
	GLint fs_status = 0;
	glGetShaderiv(frag, GL_COMPILE_STATUS, &fs_status);
	assert(fs_status);

	const auto program = glCreateProgram();
	glAttachShader(program, vert);
	glAttachShader(program, frag);
	glLinkProgram(program);
	GLint program_status = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &program_status);
	assert(program_status);

	return program;
}

static GLuint ShaderColorOnly;
static GLuint ShaderColorTex;
static GLuint ShaderColorTexNoAlpha;
static GLuint ShaderColorTexAlphaOnly;

struct Vertex
{
	SPK::Vector3D pos;
	SPK::Color color;
	SPK::Vector3D texCoord;
};

static std::vector<Vertex> VertexBuf;
static std::vector<unsigned short> IndexBuf;

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

		if (!ShaderColorOnly)
		{
			ShaderColorOnly = CreateShaderProgram(
				"#version 110\n"
				"attribute vec3 InPosition;\n"
				"attribute vec4 InColor;\n"
				"varying vec4 Color;\n"
				"void main()\n"
				"{\n"
				"    gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * vec4(InPosition, 1.0);\n"
				"    Color = InColor;\n"
				"}\n",
				"#version 110\n"
				"varying vec4 Color;\n"
				"void main()\n"
				"{\n"
				"    gl_FragColor = Color;\n"
				"}\n");
			SPK_ASSERT(ShaderColorOnly != 0, "ShaderColorOnly");

			ShaderColorTex = CreateShaderProgram(
				"#version 110\n"
				"attribute vec3 InPosition;\n"
				"attribute vec4 InColor;\n"
				"attribute vec2 InTexCoord;\n"
				"varying vec4 Color;\n"
				"varying vec2 TexCoord;\n"
				"void main()\n"
				"{\n"
				"    gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * vec4(InPosition, 1.0);\n"
				"    Color = InColor;\n"
				"    TexCoord = InTexCoord;\n"
				"}\n",
				"#version 110\n"
				"uniform sampler2D ColorMap;\n"
				"varying vec4 Color;\n"
				"varying vec2 TexCoord;\n"
				"void main()\n"
				"{\n"
				"    gl_FragColor = Color * texture2D(ColorMap, TexCoord);\n"
				"}\n");
			SPK_ASSERT(ShaderColorTex != 0, "ShaderColorTex");

			ShaderColorTexNoAlpha = CreateShaderProgram(
				"#version 110\n"
				"attribute vec3 InPosition;\n"
				"attribute vec4 InColor;\n"
				"attribute vec2 InTexCoord;\n"
				"varying vec4 Color;\n"
				"varying vec2 TexCoord;\n"
				"void main()\n"
				"{\n"
				"    gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * vec4(InPosition, 1.0);\n"
				"    Color = InColor;\n"
				"    TexCoord = InTexCoord;\n"
				"}\n",
				"#version 110\n"
				"uniform sampler2D ColorMap;\n"
				"varying vec4 Color;\n"
				"varying vec2 TexCoord;\n"
				"void main()\n"
				"{\n"
				"    gl_FragColor = vec4(texture2D(ColorMap, TexCoord).rgb, Color.a);\n"
				"}\n");
			SPK_ASSERT(ShaderColorTexNoAlpha != 0, "ShaderColorTexNoAlpha");
			
			ShaderColorTexAlphaOnly = CreateShaderProgram(
				"#version 110\n"
				"attribute vec3 InPosition;\n"
				"attribute vec4 InColor;\n"
				"attribute vec2 InTexCoord;\n"
				"varying vec4 Color;\n"
				"varying vec2 TexCoord;\n"
				"void main()\n"
				"{\n"
				"    gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * vec4(InPosition, 1.0);\n"
				"    Color = InColor;\n"
				"    TexCoord = InTexCoord;\n"
				"}\n",
				"#version 110\n"
				"uniform sampler2D ColorMap;\n"
				"varying vec4 Color;\n"
				"varying vec2 TexCoord;\n"
				"void main()\n"
				"{\n"
				"    gl_FragColor = vec4(Color.rgb, Color.a * texture2D(ColorMap, TexCoord).a);\n"
				"}\n");
			SPK_ASSERT(ShaderColorTexAlphaOnly != 0, "ShaderColorTexAlphaOnly");
		}

		vertexBuffer = SPK_NEW_ARRAY(Vector3D,nbVertices);
		colorBuffer = SPK_NEW_ARRAY(Color,nbVertices);

		if (nbTexCoords > 0)
			texCoordBuffer = SPK_NEW_ARRAY(float,nbVertices * nbTexCoords);

		glGenBuffers(1, &vbo);
		glGenBuffers(1, &ibo);
	}

	GLBuffer::~GLBuffer()
	{
		SPK_DELETE_ARRAY(vertexBuffer);
		SPK_DELETE_ARRAY(colorBuffer);
		SPK_DELETE_ARRAY(texCoordBuffer);

		glDeleteBuffers(1, &vbo);
		glDeleteBuffers(1, &ibo);
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

	void GLBuffer::render(GLuint primitive, size_t nbVertices, bool useTextureColor, bool useTextureAlpha)
	{
		if (nbVertices == 0)
			return;
		
		if (primitive == GL_QUADS)
		{
			const auto quadsCount = nbVertices / 4;
			const auto indicesCount = quadsCount * 6;

			if (VertexBuf.size() < nbVertices)
				VertexBuf.resize(nbVertices);
			if (IndexBuf.size() < indicesCount)
				IndexBuf.resize(indicesCount);
			
			for (size_t i = 0; i < nbVertices; i++)
			{
				VertexBuf[i].pos = vertexBuffer[i];
				VertexBuf[i].color = colorBuffer[i];
			}

			if (nbTexCoords > 0)
			{
				SPK_ASSERT(nbTexCoords == 2, "nbTexCoords");
				for (size_t i = 0; i < nbVertices; i++)
					VertexBuf[i].texCoord = Vector3D(texCoordBuffer[i * 2 + 0], texCoordBuffer[i * 2 + 1]);
			}

			for (size_t i = 0; i < quadsCount; i++)
			{
				IndexBuf[i * 6 + 0] = i * 4 + 0;
				IndexBuf[i * 6 + 1] = i * 4 + 1;
				IndexBuf[i * 6 + 2] = i * 4 + 2;
				IndexBuf[i * 6 + 3] = i * 4 + 2;
				IndexBuf[i * 6 + 4] = i * 4 + 3;
				IndexBuf[i * 6 + 5] = i * 4 + 0;
			}

			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glBufferData(GL_ARRAY_BUFFER, nbVertices * sizeof(Vertex), VertexBuf.data(), GL_DYNAMIC_DRAW);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, indicesCount * sizeof(unsigned short), IndexBuf.data(), GL_DYNAMIC_DRAW);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(size_t)offsetof(Vertex, pos));
			glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)(size_t)offsetof(Vertex, color));
			glEnableVertexAttribArray(0);
			glEnableVertexAttribArray(1);

			if (nbTexCoords > 0)
			{
				glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(size_t)offsetof(Vertex, texCoord));
				glEnableVertexAttribArray(2);

				if (useTextureColor && useTextureAlpha)
					glUseProgram(ShaderColorTex);
				else if (useTextureColor)
					glUseProgram(ShaderColorTexNoAlpha);
				else if (useTextureAlpha)
					glUseProgram(ShaderColorTexAlphaOnly);
				else
					glUseProgram(ShaderColorOnly);
			}
			else
			{
				glUseProgram(ShaderColorOnly);
			}
			
			glDrawElements(GL_TRIANGLES, indicesCount, GL_UNSIGNED_SHORT, (void*)0);

			glDisableVertexAttribArray(0);
			glDisableVertexAttribArray(1);
			if (nbTexCoords > 0)
				glDisableVertexAttribArray(2);
		}

		/*glEnableClientState(GL_VERTEX_ARRAY);
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
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);*/
	}
}}
