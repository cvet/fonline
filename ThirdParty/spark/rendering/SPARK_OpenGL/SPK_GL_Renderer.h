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

#ifndef H_SPK_GL_RENDERER
#define H_SPK_GL_RENDERER

#include "SPK_GL_DEF.h"
#include <Core/SPK_Renderer.h>

#ifndef SPK_GL_NO_EXT
#define SPK_GL_CHECK_EXTENSION(glExt) GLRenderer::checkExtension(glExt)
#else
#define SPK_GL_CHECK_EXTENSION(glExt) false
#endif

namespace SPK
{
namespace GL
{
	/**
	* @class GLRenderer
	* @brief An abstract Renderer for the OpenGL renderers
	*/
	class SPK_GL_PREFIX GLRenderer : public Renderer
	{
		SPK_START_DESCRIPTION
		SPK_PARENT_ATTRIBUTES(Renderer)
		SPK_ATTRIBUTE("blend mode", ATTRIBUTE_TYPE_STRING)
		SPK_END_DESCRIPTION
		
	public :

		////////////////
		// Destructor //
		////////////////

		/** @brief Destructor of GLRenderer */
		virtual  ~GLRenderer() {}

		/////////////
		// Setters //
		/////////////

		/**
		* @brief Enables or disables the blending of this GLRenderer
		* @param blendingEnabled true to enable the blending, false to disable it
		*/
		virtual  void enableBlending(bool blendingEnabled);

		/**
		* @brief Sets the blending functions of this GLRenderer
		*
		* the blending functions are one of the OpenGL blending functions.
		*
		* @param src : the source blending function of this GLRenderer
		* @param dest : the destination blending function of this GLRenderer
		*/
		void setBlendingFunctions(GLuint src,GLuint dest);
		void setBlendMode(BlendMode blendMode) override;

		/////////////
		// Getters //
		/////////////

		/**
		* @brief Tells whether blending is enabled for this GLRenderer
		* @return true if blending is enabled, false if it is disabled
		*/
		bool isBlendingEnabled() const;

		/**
		* @brief Gets the source blending function of this GLRenderer
		* @return the source blending function of this GLRenderer
		*/
		GLuint getSrcBlendingFunction() const;

		/**
		* @brief Gets the destination blending function of this GLRenderer
		* @return the source destination function of this GLRenderer
		*/
		GLuint getDestBlendingFunction() const;

		///////////////
		// Interface //
		///////////////

		/**
		* @brief Saves the current OpenGL states
		*
		* This method saves all the states that are likely to be modified by a GLRenderer.<br>
		* Use restoreGLStates() to restore the states.<br>
		* <br>
		* Note that for one saveGLStates call, a call to restoreGLStates must occur.
		* In case of several saveGLStates with no restoreGLStates, the restoreGLStates is called priorly in an implicit way.
		*/
		static void saveGLStates();

		/**
		* @brief Restores the OpenGL states
		*
		* This method restores the OpenGL states at the values they were at the last call of saveGLStates().
		*/
		static void restoreGLStates();

	public :
        /*spark_description(GLRenderer, Renderer)
		(
        );*/

	protected :

		GLRenderer(bool NEEDS_DATASET);

		/** @brief Inits the blending of this GLRenderer */
		void initBlending() const;

		/** @brief Inits the rendering hints of this GLRenderer */
		void initRenderingOptions() const;

		/** 
		* @brief Checks whether an extension is supported 
		* Note that this method does not exist if the macro SPK_GL_NO_EXT is defined
		* @param glExt : the glew extension to check
		* @return true is the extension is supported, false if not
		*/
#ifndef SPK_GL_NO_EXT
		static bool checkExtension(GLboolean* glExt);
#endif

		void innerImport(const IO::Descriptor& descriptor) override;
		void innerExport(IO::Descriptor& descriptor) const override;

	private :

#ifndef SPK_GL_NO_EXT
		enum GlewStatus
		{
			GLEW_UNLOADED,		// Not yet initialize
			GLEW_SUPPORTED,		// Initialized correctly
			GLEW_UNSUPPORTED,	// Error during initialization
		};
		static GlewStatus glewStatus;
#endif

		bool blendingEnabled;
		GLuint srcBlending;
		GLuint destBlending;
	};

	inline GLRenderer::GLRenderer(bool NEEDS_DATASET) :
		Renderer(NEEDS_DATASET),
		blendingEnabled(false),
		srcBlending(GL_SRC_ALPHA),
		destBlending(GL_ONE_MINUS_SRC_ALPHA)
	{}

	inline void GLRenderer::enableBlending(bool blendingEnabled)
	{
		this->blendingEnabled = blendingEnabled;
	}

	inline void GLRenderer::setBlendingFunctions(GLuint src,GLuint dest)
	{
		srcBlending = src;
		destBlending = dest;
	}

	inline bool GLRenderer::isBlendingEnabled() const
	{
		return blendingEnabled;
	}

	inline GLuint GLRenderer::getSrcBlendingFunction() const
	{
		return srcBlending;
	}

	inline GLuint GLRenderer::getDestBlendingFunction() const
	{
		return destBlending;
	}

	inline void GLRenderer::initBlending() const
	{
		if (blendingEnabled)
		{
			glBlendFunc(srcBlending,destBlending);
			glEnable(GL_BLEND);
		}
		else
			glDisable(GL_BLEND);
	}

	inline void GLRenderer::initRenderingOptions() const
	{
		// alpha test
		if (isRenderingOptionEnabled(RENDERING_OPTION_ALPHA_TEST))
		{
			glAlphaFunc(GL_GEQUAL,getAlphaTestThreshold());
			glEnable(GL_ALPHA_TEST);
		}
		else
			glDisable(GL_ALPHA_TEST);

		// depth write
		glDepthMask(isRenderingOptionEnabled(RENDERING_OPTION_DEPTH_WRITE));
	}
}}

#endif
