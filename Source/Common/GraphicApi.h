#pragma once

#include "Common.h"
#include "GluStuff.h"
#include "SDL.h"
#include "SDL_syswm.h"
#ifndef FO_OPENGL_ES
#include "GL/glew.h"
#include "SDL_opengl.h"
#else
#include "SDL_opengles2.h"
#endif
#include "SDL_vulkan.h"

#ifdef FO_HAVE_DX
#include <d3d9.h>
#endif

#ifndef FO_OPENGL_ES
#ifdef FO_MAC
#undef glGenVertexArrays
#undef glBindVertexArray
#undef glDeleteVertexArrays
#define glGenVertexArrays glGenVertexArraysAPPLE
#define glBindVertexArray glBindVertexArrayAPPLE
#define glDeleteVertexArrays glDeleteVertexArraysAPPLE
#endif
#else
#define glGenVertexArrays glGenVertexArraysOES
#define glBindVertexArray glBindVertexArrayOES
#define glDeleteVertexArrays glDeleteVertexArraysOES
#define glGenFramebuffersEXT glGenFramebuffers
#define glBindFramebufferEXT glBindFramebuffer
#define glFramebufferTexture2DEXT glFramebufferTexture2D
#define glRenderbufferStorageEXT glRenderbufferStorage
#define glGenRenderbuffersEXT glGenRenderbuffers
#define glBindRenderbufferEXT glBindRenderbuffer
#define glFramebufferRenderbufferEXT glFramebufferRenderbuffer
#define glCheckFramebufferStatusEXT glCheckFramebufferStatus
#define glDeleteRenderbuffersEXT glDeleteRenderbuffers
#define glDeleteFramebuffersEXT glDeleteFramebuffers
#define glProgramBinary(a, b, c, d)
#define glGetProgramBinary(a, b, c, d, e)
#define glProgramParameteri glProgramParameteriEXT
#define GL_PROGRAM_BINARY_RETRIEVABLE_HINT 0
#define GL_PROGRAM_BINARY_LENGTH 0
#define GL_FRAMEBUFFER_COMPLETE_EXT GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_EXT GL_FRAMEBUFFER
#ifndef GL_COLOR_ATTACHMENT0_EXT
#define GL_COLOR_ATTACHMENT0_EXT GL_COLOR_ATTACHMENT0
#endif
#define GL_RENDERBUFFER_EXT GL_RENDERBUFFER
#define GL_DEPTH_ATTACHMENT_EXT GL_DEPTH_ATTACHMENT
#define GL_RENDERBUFFER_BINDING_EXT GL_RENDERBUFFER_BINDING
#define GL_CLAMP GL_CLAMP_TO_EDGE
#define GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_OES
#define GL_DEPTH24_STENCIL8_EXT GL_DEPTH24_STENCIL8_OES
#define GL_STENCIL_ATTACHMENT_EXT GL_STENCIL_ATTACHMENT
#define glGetTexImage(a, b, c, d, e)
#define glDrawBuffer(a)
#ifndef GL_MAX_COLOR_TEXTURE_SAMPLES
#define GL_MAX_COLOR_TEXTURE_SAMPLES 0x910E
#endif
#ifndef GL_TEXTURE_2D_MULTISAMPLE
#define GL_TEXTURE_2D_MULTISAMPLE 0x9100
#endif
#ifndef GL_MAX
#define GL_MAX GL_MAX_EXT
#define GL_MIN GL_MIN_EXT
#endif
#if defined(FO_IOS)
#define glTexImage2DMultisample(a, b, c, d, e, f)
#define glRenderbufferStorageMultisample glRenderbufferStorageMultisampleAPPLE
#define glRenderbufferStorageMultisampleEXT glRenderbufferStorageMultisampleAPPLE
#elif defined(FO_ANDROID)
#define glTexImage2DMultisample(a, b, c, d, e, f)
#define glRenderbufferStorageMultisample glRenderbufferStorageMultisampleIMG
#define glRenderbufferStorageMultisampleEXT glRenderbufferStorageMultisampleIMG
#elif defined(FO_WEB)
#define glTexImage2DMultisample(a, b, c, d, e, f)
#define glRenderbufferStorageMultisample(a, b, c, d, e)
#define glRenderbufferStorageMultisampleEXT(a, b, c, d, e)
#endif
#endif

#define GL(expr) \
    { \
        expr; \
        if (GameOpt.OpenGLDebug) \
        { \
            GLenum err__ = glGetError(); \
            RUNTIME_ASSERT_STR(err__ == GL_NO_ERROR, _str(#expr " error {:#X}", err__)); \
        } \
    }

extern bool OGL_version_2_0;
extern bool OGL_vertex_buffer_object;
extern bool OGL_framebuffer_object;
extern bool OGL_framebuffer_object_ext;
extern bool OGL_framebuffer_multisample;
extern bool OGL_packed_depth_stencil;
extern bool OGL_texture_multisample;
extern bool OGL_vertex_array_object;
extern bool OGL_get_program_binary;
#define GL_HAS(extension) (OGL_##extension)

namespace GraphicApi
{
bool Init();
}
