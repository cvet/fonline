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

#include "GraphicApi.h"
#include "Log.h"
#include "Testing.h"

bool OGL_version_2_0 = false;
bool OGL_vertex_buffer_object = false;
bool OGL_framebuffer_object = false;
bool OGL_framebuffer_object_ext = false;
bool OGL_framebuffer_multisample = false;
bool OGL_texture_multisample = false;
bool OGL_vertex_array_object = false;

bool GraphicApi::Init()
{
// Initialize GLEW
#ifndef FO_OPENGL_ES
    GLenum glew_result = glewInit();
    if (glew_result != GLEW_OK)
    {
        WriteLog("GLEW not initialized, result {}.\n", glew_result);
        return false;
    }
    OGL_version_2_0 = GLEW_VERSION_2_0 != 0;
    OGL_vertex_buffer_object = GLEW_ARB_vertex_buffer_object != 0;
    OGL_framebuffer_object = GLEW_ARB_framebuffer_object != 0;
    OGL_framebuffer_object_ext = GLEW_EXT_framebuffer_object != 0;
    OGL_framebuffer_multisample = GLEW_EXT_framebuffer_multisample != 0;
    OGL_texture_multisample = GLEW_ARB_texture_multisample != 0;
#ifdef FO_MAC
    OGL_vertex_array_object = GLEW_APPLE_vertex_array_object != 0;
#else
    OGL_vertex_array_object = GLEW_ARB_vertex_array_object != 0;
#endif
#endif

// OpenGL ES extensions
#ifdef FO_OPENGL_ES
    OGL_version_2_0 = true;
    OGL_vertex_buffer_object = true;
    OGL_framebuffer_object = true;
    OGL_framebuffer_object_ext = false;
    OGL_framebuffer_multisample = false;
    OGL_texture_multisample = false;
    OGL_vertex_array_object = false;
#ifdef FO_ANDROID
    OGL_vertex_array_object = SDL_GL_ExtensionSupported("GL_OES_vertex_array_object");
    OGL_framebuffer_multisample = SDL_GL_ExtensionSupported("GL_IMG_multisampled_render_to_texture");
#endif
#ifdef FO_IOS
    OGL_vertex_array_object = true;
    OGL_framebuffer_multisample = true;
#endif
#endif

    return true;
}

static void MultMatricesf(const float a[16], const float b[16], float r[16]);
static void MultMatrixVecf(const float matrix[16], const float in[4], float out[4]);
static bool InvertMatrixf(const float m[16], float inv_out[16]);

void GraphicApi::MatrixOrtho(float* matrix, float left, float right, float bottom, float top, float near, float far)
{
    float r_l = right - left;
    float t_b = top - bottom;
    float f_n = far - near;
    float tx = -(right + left) / (right - left);
    float ty = -(top + bottom) / (top - bottom);
    float tz = -(far + near) / (far - near);

    matrix[0] = 2.0f / r_l;
    matrix[1] = 0.0f;
    matrix[2] = 0.0f;
    matrix[3] = tx;

    matrix[4] = 0.0f;
    matrix[5] = 2.0f / t_b;
    matrix[6] = 0.0f;
    matrix[7] = ty;

    matrix[8] = 0.0f;
    matrix[9] = 0.0f;
    matrix[10] = -2.0f / f_n;
    matrix[11] = tz;

    matrix[12] = 0.0f;
    matrix[13] = 0.0f;
    matrix[14] = 0.0f;
    matrix[15] = 1.0f;
}

bool GraphicApi::MatrixProject(float objx, float objy, float objz, const float model_matrix[16],
    const float proj_matrix[16], const int viewport[4], float* winx, float* winy, float* winz)
{
    float in[4];
    in[0] = objx;
    in[1] = objy;
    in[2] = objz;
    in[3] = 1.0;

    float out[4];
    MultMatrixVecf(model_matrix, in, out);
    MultMatrixVecf(proj_matrix, out, in);
    if (in[3] == 0.0)
        return false;

    in[0] /= in[3];
    in[1] /= in[3];
    in[2] /= in[3];

    in[0] = in[0] * 0.5f + 0.5f;
    in[1] = in[1] * 0.5f + 0.5f;
    in[2] = in[2] * 0.5f + 0.5f;

    in[0] = in[0] * viewport[2] + viewport[0];
    in[1] = in[1] * viewport[3] + viewport[1];

    *winx = in[0];
    *winy = in[1];
    *winz = in[2];
    return true;
}

bool GraphicApi::MatrixUnproject(float winx, float winy, float winz, const float model_matrix[16],
    const float proj_matrix[16], const int viewport[4], float* objx, float* objy, float* objz)
{
    float final_matrix[16];
    MultMatricesf(model_matrix, proj_matrix, final_matrix);
    if (!InvertMatrixf(final_matrix, final_matrix))
        return false;

    float in[4];
    in[0] = winx;
    in[1] = winy;
    in[2] = winz;
    in[3] = 1.0;

    in[0] = (in[0] - viewport[0]) / viewport[2];
    in[1] = (in[1] - viewport[1]) / viewport[3];

    in[0] = in[0] * 2 - 1;
    in[1] = in[1] * 2 - 1;
    in[2] = in[2] * 2 - 1;

    float out[4];
    MultMatrixVecf(final_matrix, in, out);
    if (out[3] == 0.0)
        return false;

    out[0] /= out[3];
    out[1] /= out[3];
    out[2] /= out[3];
    *objx = out[0];
    *objy = out[1];
    *objz = out[2];
    return true;
}

static void MultMatricesf(const float a[16], const float b[16], float r[16])
{
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            r[i * 4 + j] = a[i * 4 + 0] * b[0 * 4 + j] + a[i * 4 + 1] * b[1 * 4 + j] + a[i * 4 + 2] * b[2 * 4 + j] +
                a[i * 4 + 3] * b[3 * 4 + j];
        }
    }
}

static void MultMatrixVecf(const float matrix[16], const float in[4], float out[4])
{
    for (int i = 0; i < 4; i++)
    {
        out[i] = in[0] * matrix[0 * 4 + i] + in[1] * matrix[1 * 4 + i] + in[2] * matrix[2 * 4 + i] +
            in[3] * matrix[3 * 4 + i];
    }
}

static bool InvertMatrixf(const float m[16], float inv_out[16])
{
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] +
        m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] -
        m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] +
        m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] -
        m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] -
        m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] +
        m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] -
        m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
        m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
        m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
        m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
        m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
        m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
        m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
        m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
        m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] -
        m[8] * m[2] * m[5];

    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (det == 0.0f)
        return false;

    det = 1.0f / det;

    for (int i = 0; i < 16; i++)
        inv_out[i] = inv[i] * det;
    return true;
}
