//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#pragma once

#include "Common.h"

#include "SDL.h"
#ifndef FO_OPENGL_ES
#include "GL/glew.h"
#include "SDL_opengl.h"
#else
#include "SDL_opengles2.h"
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

#define GL(expr) expr
/*#define GL(expr) \
    { \
        expr; \
        if (GameOpt.OpenGLDebug) \
        { \
            GLenum err__ = glGetError(); \
            RUNTIME_ASSERT_STR(err__ == GL_NO_ERROR, _str(#expr " error {:#X}", err__)); \
        } \
    }*/

extern bool OGL_version_2_0;
extern bool OGL_vertex_buffer_object;
extern bool OGL_framebuffer_object;
extern bool OGL_framebuffer_object_ext;
extern bool OGL_framebuffer_multisample;
extern bool OGL_texture_multisample;
extern bool OGL_vertex_array_object;
#define GL_HAS(extension) (OGL_##extension)

namespace GraphicApi
{
    bool Init();
    void MatrixOrtho(float* matrix, float left, float right, float bottom, float top, float near, float far);
    bool MatrixProject(float objx, float objy, float objz, const float model_matrix[16], const float proj_matrix[16],
        const int viewport[4], float* winx, float* winy, float* winz);
    bool MatrixUnproject(float winx, float winy, float winz, const float model_matrix[16], const float proj_matrix[16],
        const int viewport[4], float* objx, float* objy, float* objz);
}
