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
#include "GL/glew.h"
#endif

#include <SPARK_Core.h>
#include "SPK_GL_QuadRenderer.h"

namespace SPK
{
namespace GL
{
	static GLuint(*TextureLoader)(const std::string&);

	void GLQuadRenderer::setTextureLoader(GLuint(*loader)(const std::string&))
	{
		TextureLoader = loader;
	}

#ifndef SPK_GL_NO_EXT
	GLboolean* const GLQuadRenderer::SPK_GL_TEXTURE_3D_EXT = &__GLEW_EXT_texture3D;
#endif

	GLQuadRenderer::GLQuadRenderer(float scaleX,float scaleY) :
		GLRenderer(false),
		QuadRenderBehavior(scaleX,scaleY),
		Oriented3DRenderBehavior(),
		textureIndex(0)
	{}

	GLQuadRenderer::GLQuadRenderer(const GLQuadRenderer& renderer) :
		GLRenderer(renderer),
		QuadRenderBehavior(renderer),
		Oriented3DRenderBehavior(renderer),
		textureIndex(renderer.textureIndex)
	{}

	bool GLQuadRenderer::setTexturingMode(TextureMode mode)
	{
		if ((mode == TEXTURE_MODE_3D && !SPK_GL_CHECK_EXTENSION(SPK_GL_TEXTURE_3D_EXT)))
			return false;

		texturingMode = mode;
		return true;
	}

	RenderBuffer* GLQuadRenderer::attachRenderBuffer(const Group& group) const
	{
		return SPK_NEW(GLBuffer,group.getCapacity() << 2);
	}

	void GLQuadRenderer::render(const Group& group,const DataSet* dataSet,RenderBuffer* renderBuffer) const
	{
		SPK_ASSERT(renderBuffer != NULL,"GLQuadRenderer::render(const Group&,const DataSet*,RenderBuffer*) - renderBuffer must not be NULL");
		GLBuffer& buffer = static_cast<GLBuffer&>(*renderBuffer);
		buffer.positionAtStart(); // Repositions all the buffers at the start

		float oldModelView[16];
		for (int i = 0; i < 16; ++i)
			oldModelView[i] = modelView[i];
		glGetFloatv(GL_MODELVIEW_MATRIX,modelView);
		for (int i = 0; i < 16; ++i)
			if (oldModelView[i] != modelView[i])
			{
				invertModelView();
				break;
			}

		initBlending();
		initRenderingOptions();

		glShadeModel(GL_FLAT);

		switch(texturingMode)
		{
		case TEXTURE_MODE_2D :
		case TEXTURE_MODE_2D_NO_ALPHA :
		case TEXTURE_MODE_2D_ONLY_ALPHA :
			// Creates and inits the 2D TexCoord buffer if necessary
			if (buffer.getNbTexCoords() != 2)
			{
				buffer.setNbTexCoords(2);
				if (!group.isEnabled(PARAM_TEXTURE_INDEX))
				{
					float t[8] = {1.0f,0.0f,0.0f,0.0f,0.0f,1.0f,1.0f,1.0f};
					for (size_t i = 0; i < group.getCapacity() << 3; ++i)
						buffer.setNextTexCoord(t[i & 7]);
				}
			}

			// Binds the texture
#ifndef SPK_GL_NO_EXT
			if (SPK_GL_CHECK_EXTENSION(SPK_GL_TEXTURE_3D_EXT))
				glDisable(GL_TEXTURE_3D_EXT);
#endif
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D,textureIndex);

			// Selects the correct function
			if (!group.isEnabled(PARAM_TEXTURE_INDEX))
			{
				if (!group.isEnabled(PARAM_ANGLE))
					renderParticle = &GLQuadRenderer::render2D;
				else
					renderParticle = &GLQuadRenderer::render2DRot;
			}
			else
			{
				if (!group.isEnabled(PARAM_ANGLE))
					renderParticle = &GLQuadRenderer::render2DAtlas;
				else
					renderParticle = &GLQuadRenderer::render2DAtlasRot;
			}
			break;

		case TEXTURE_MODE_3D :
			// Creates and inits the 3D TexCoord buffer if necessery
			if (buffer.getNbTexCoords() != 3)
			{
				buffer.setNbTexCoords(3);
				float t[12] =  {1.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,1.0f,0.0f,1.0f,1.0f,0.0f};
				for (size_t i = 0; i < group.getCapacity() * 12; ++i)
					buffer.setNextTexCoord(t[i % 12]);
			}

			// Binds the texture
			glDisable(GL_TEXTURE_2D);
#ifndef SPK_GL_NO_EXT
			glEnable(GL_TEXTURE_3D_EXT);
			glBindTexture(GL_TEXTURE_3D_EXT,textureIndex);
#endif

			// Selects the correct function
			if (!group.isEnabled(PARAM_ANGLE))
				renderParticle = &GLQuadRenderer::render3D;
			else
				renderParticle = &GLQuadRenderer::render3DRot;
			break;

		case TEXTURE_MODE_NONE :
			if (buffer.getNbTexCoords() != 0)
				buffer.setNbTexCoords(0);

			glDisable(GL_TEXTURE_2D);

			// Selects the correct function
#ifndef SPK_GL_NO_EXT
			if (SPK_GL_CHECK_EXTENSION(SPK_GL_TEXTURE_3D_EXT))
				glDisable(GL_TEXTURE_3D_EXT);
#endif
			if (!group.isEnabled(PARAM_ANGLE))
				renderParticle = &GLQuadRenderer::render2D;
			else
				renderParticle = &GLQuadRenderer::render2DRot;
			break;
		}

		bool globalOrientation = precomputeOrientation3D(
			group,
			Vector3D(-invModelView[8],-invModelView[9],-invModelView[10]),
			Vector3D(invModelView[4],invModelView[5],invModelView[6]),
			Vector3D(invModelView[12],invModelView[13],invModelView[14]));

		// Fills the buffers
		if (globalOrientation)
		{
			computeGlobalOrientation3D(group);

			for (ConstGroupIterator particleIt(group); !particleIt.end(); ++particleIt)
				(this->*renderParticle)(*particleIt,buffer);
		}
		else
		{
			for (ConstGroupIterator particleIt(group); !particleIt.end(); ++particleIt)
			{
				computeSingleOrientation3D(*particleIt);
				(this->*renderParticle)(*particleIt,buffer);
			}
		}

		buffer.render(GL_QUADS,group.getNbParticles() << 2, texturingMode != TEXTURE_MODE_2D_ONLY_ALPHA, texturingMode != TEXTURE_MODE_2D_NO_ALPHA);
	}

	void GLQuadRenderer::computeAABB(Vector3D& AABBMin,Vector3D& AABBMax,const Group& group,const DataSet* dataSet) const
	{
		float diagonal = group.getGraphicalRadius() * std::sqrt(scaleX * scaleX + scaleY * scaleY);
		Vector3D diagV(diagonal,diagonal,diagonal);

		if (group.isEnabled(PARAM_SCALE))
			for (ConstGroupIterator particleIt(group); !particleIt.end(); ++particleIt)
			{
				Vector3D scaledDiagV = diagV * particleIt->getParamNC(PARAM_SCALE);
				AABBMin.setMin(particleIt->position() - scaledDiagV);
				AABBMax.setMax(particleIt->position() + scaledDiagV);
			}
		else
		{
			for (ConstGroupIterator particleIt(group); !particleIt.end(); ++particleIt)
			{
				AABBMin.setMin(particleIt->position());
				AABBMax.setMax(particleIt->position());
			}
			AABBMin -= diagV;
			AABBMax += diagV;
		}
	}

	void GLQuadRenderer::render2D(const Particle& particle,GLBuffer& renderBuffer) const
	{
		scaleQuadVectors(particle,scaleX,scaleY);
		GLCallColorAndVertex(particle,renderBuffer);
	}

	void GLQuadRenderer::render2DRot(const Particle& particle,GLBuffer& renderBuffer) const
	{
		rotateAndScaleQuadVectors(particle,scaleX,scaleY);
		GLCallColorAndVertex(particle,renderBuffer);
	}

	void GLQuadRenderer::render3D(const Particle& particle,GLBuffer& renderBuffer) const
	{
		scaleQuadVectors(particle,scaleX,scaleY);
		GLCallColorAndVertex(particle,renderBuffer);
		GLCallTexture3D(particle,renderBuffer);
	}

	void GLQuadRenderer::render3DRot(const Particle& particle,GLBuffer& renderBuffer) const
	{
		rotateAndScaleQuadVectors(particle,scaleX,scaleY);
		GLCallColorAndVertex(particle,renderBuffer);
		GLCallTexture3D(particle,renderBuffer);
	}

	void GLQuadRenderer::render2DAtlas(const Particle& particle,GLBuffer& renderBuffer) const
	{
		scaleQuadVectors(particle,scaleX,scaleY);
		GLCallColorAndVertex(particle,renderBuffer);
		GLCallTexture2DAtlas(particle,renderBuffer);
	}

	void GLQuadRenderer::render2DAtlasRot(const Particle& particle,GLBuffer& renderBuffer) const
	{
		rotateAndScaleQuadVectors(particle,scaleX,scaleY);
		GLCallColorAndVertex(particle,renderBuffer);
		GLCallTexture2DAtlas(particle,renderBuffer);
	}

	void GLQuadRenderer::setTextureName(const std::string& textureName)
	{
		this->textureName = textureName;
		this->textureIndex = TextureLoader(textureName);
	}
	
	std::string GLQuadRenderer::getTextureName() const
	{
		return this->textureName;
	}

	void GLQuadRenderer::innerImport(const IO::Descriptor& descriptor)
	{
		GLRenderer::innerImport(descriptor);

		textureName = "";

		texturingMode = TEXTURE_MODE_NONE;
		scaleX = 1.0f;
		scaleY = 1.0f;
		textureAtlasNbX = 1;
		textureAtlasNbY = 1;
		textureAtlasW = 1.0f;
		textureAtlasH = 1.0f;

		lookOrientation = LOOK_CAMERA_PLANE;
		upOrientation = UP_CAMERA;
		lockedAxis = LOCK_UP;
		lookVector.set(0.0f, 0.0f, 1.0f);
		upVector.set(0.0f, 1.0f, 0.0f);
		
		const IO::Attribute* attrib = nullptr;

		if ((attrib = descriptor.getAttributeWithValue("texture")))
			setTextureName(attrib->getValue<std::string>());

		if ((attrib = descriptor.getAttributeWithValue("texturing mode")))
		{
			const auto texMode = attrib->getValue<std::string>();
			if (texMode == "TEXTURE_MODE_NONE")
				setTexturingMode(TEXTURE_MODE_NONE);
			else if (texMode == "TEXTURE_MODE_2D")
				setTexturingMode(TEXTURE_MODE_2D);
			else if (texMode == "TEXTURE_MODE_2D_NO_ALPHA")
				setTexturingMode(TEXTURE_MODE_2D_NO_ALPHA);
			else if (texMode == "TEXTURE_MODE_2D_ONLY_ALPHA")
				setTexturingMode(TEXTURE_MODE_2D_ONLY_ALPHA);
			else if (texMode == "TEXTURE_MODE_3D")
				setTexturingMode(TEXTURE_MODE_3D);
		}

		if ((attrib = descriptor.getAttributeWithValue("scale")))
		{
			const auto tmpScale = attrib->getValues<float>();
			switch (tmpScale.size())
			{
			case 1: setScale(tmpScale[0], scaleY); break;
			case 2: setScale(tmpScale[0], tmpScale[1]); break;
			default: SPK_LOG_ERROR("Emitter::innerImport(const IO::Descriptor&) - Wrong number of scale : " << tmpScale.size());
			}
		}

		if ((attrib = descriptor.getAttributeWithValue("atlas dimensions")))
		{
			const auto tmpAtlasDimensions = attrib->getValues<uint32_t>();
			switch (tmpAtlasDimensions.size())
			{
			case 1: setAtlasDimensions(tmpAtlasDimensions[0], textureAtlasNbY); break;
			case 2: setAtlasDimensions(tmpAtlasDimensions[0], tmpAtlasDimensions[1]); break;
			default: SPK_LOG_ERROR("Emitter::innerImport(const IO::Descriptor&) - Wrong number of atlas dimensions : " << tmpAtlasDimensions.size());
			}
		}

		if ((attrib = descriptor.getAttributeWithValue("look orientation")))
		{
			const auto lookOrient = attrib->getValue<std::string>();
			if (lookOrient == "LOOK_CAMERA_PLANE")
				lookOrientation = LOOK_CAMERA_PLANE;
			else if (lookOrient == "LOOK_CAMERA_POINT")
				lookOrientation = LOOK_CAMERA_POINT;
			else if (lookOrient == "LOOK_AXIS")
				lookOrientation = LOOK_AXIS;
			else if (lookOrient == "LOOK_POINT")
				lookOrientation = LOOK_POINT;
		}

		if ((attrib = descriptor.getAttributeWithValue("up orientation")))
		{
			const auto upOrient = attrib->getValue<std::string>();
			if (upOrient == "UP_CAMERA")
				upOrientation = UP_CAMERA;
			else if (upOrient == "UP_DIRECTION")
				upOrientation = UP_DIRECTION;
			else if (upOrient == "UP_AXIS")
				upOrientation = UP_AXIS;
			else if (upOrient == "UP_POINT")
				upOrientation = UP_POINT;
		}

		if ((attrib = descriptor.getAttributeWithValue("locked axis")))
		{
			const auto lockAx = attrib->getValue<std::string>();
			if (lockAx == "LOCK_LOOK")
				lockedAxis = LOCK_LOOK;
			else if (lockAx == "LOCK_UP")
				lockedAxis = LOCK_UP;
		}

		if ((attrib = descriptor.getAttributeWithValue("locked look vector")))
			lookVector = attrib->getValue<Vector3D>();

		if ((attrib = descriptor.getAttributeWithValue("locked up vector")))
			upVector = attrib->getValue<Vector3D>();
	}
	
	void GLQuadRenderer::innerExport(IO::Descriptor& descriptor) const
	{
		GLRenderer::innerExport(descriptor);

		descriptor.getAttribute("texture")->setValue(textureName);

		if (texturingMode == TEXTURE_MODE_NONE)
			descriptor.getAttribute("texturing mode")->setValue(std::string("TEXTURE_MODE_NONE"));
		else if (texturingMode == TEXTURE_MODE_2D)
			descriptor.getAttribute("texturing mode")->setValue(std::string("TEXTURE_MODE_2D"));
		else if (texturingMode == TEXTURE_MODE_2D_NO_ALPHA)
			descriptor.getAttribute("texturing mode")->setValue(std::string("TEXTURE_MODE_2D_NO_ALPHA"));
		else if (texturingMode == TEXTURE_MODE_2D_ONLY_ALPHA)
			descriptor.getAttribute("texturing mode")->setValue(std::string("TEXTURE_MODE_2D_ONLY_ALPHA"));
		else if (texturingMode == TEXTURE_MODE_3D)
			descriptor.getAttribute("texturing mode")->setValue(std::string("TEXTURE_MODE_3D"));

		std::vector<float> tmpScale = { scaleX, scaleY };
		descriptor.getAttribute("scale")->setValues(tmpScale.data(), 2);

		std::vector<uint32_t> tmpAtlasDimensions = { (uint32_t)textureAtlasNbX, (uint32_t)textureAtlasNbY };
		descriptor.getAttribute("atlas dimensions")->setValues(tmpAtlasDimensions.data(), 2);

		if (lookOrientation == LOOK_CAMERA_PLANE)
			descriptor.getAttribute("look orientation")->setValue(std::string("LOOK_CAMERA_PLANE"));
		else if (lookOrientation == LOOK_CAMERA_POINT)
			descriptor.getAttribute("look orientation")->setValue(std::string("LOOK_CAMERA_POINT"));
		else if (lookOrientation == LOOK_AXIS)
			descriptor.getAttribute("look orientation")->setValue(std::string("LOOK_AXIS"));
		else if (lookOrientation == LOOK_POINT)
			descriptor.getAttribute("look orientation")->setValue(std::string("LOOK_POINT"));

		if (upOrientation == UP_CAMERA)
			descriptor.getAttribute("up orientation")->setValue(std::string("UP_CAMERA"));
		else if (upOrientation == UP_DIRECTION)
			descriptor.getAttribute("up orientation")->setValue(std::string("UP_DIRECTION"));
		else if (upOrientation == UP_AXIS)
			descriptor.getAttribute("up orientation")->setValue(std::string("UP_AXIS"));
		else if (upOrientation == UP_POINT)
			descriptor.getAttribute("up orientation")->setValue(std::string("UP_POINT"));

		if (lockedAxis == LOCK_LOOK)
			descriptor.getAttribute("locked axis")->setValue(std::string("LOCK_LOOK"));
		else if (lockedAxis == LOCK_UP)
			descriptor.getAttribute("locked axis")->setValue(std::string("LOCK_UP"));

		descriptor.getAttribute("locked look vector")->setValue(lookVector);

		descriptor.getAttribute("locked up vector")->setValue(upVector);
	}
}}
