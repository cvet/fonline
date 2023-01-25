//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Rendering.h"

#if FO_HAVE_OPENGL

#include "Application.h"
#include "GenericUtils.h"
#include "Log.h"
#include "StringUtils.h"
#include "WinApi-Include.h"

#include "SDL.h"
#include "SDL_video.h"

#if !FO_OPENGL_ES
#include "GL/glew.h"
#include "SDL_opengl.h"
#endif

#if FO_OPENGL_ES
#define GLES_SILENCE_DEPRECATION
#include "SDL_opengles2.h"
#endif

#if FO_MAC && !FO_OPENGL_ES
#undef glGenVertexArrays
#undef glBindVertexArray
#undef glDeleteVertexArrays
#define glGenVertexArrays glGenVertexArraysAPPLE
#define glBindVertexArray glBindVertexArrayAPPLE
#define glDeleteVertexArrays glDeleteVertexArraysAPPLE
#endif

#if FO_OPENGL_ES
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
#ifndef GL_MAX
#define GL_MAX GL_MAX_EXT
#define GL_MIN GL_MIN_EXT
#endif
#endif

#if FO_DEBUG
#define GL(expr) \
    do { \
        expr; \
        if (RenderDebug) { \
            GLenum err__ = glGetError(); \
            RUNTIME_ASSERT_STR(err__ == GL_NO_ERROR, _str(#expr " error {}", ErrCodeToString(err__))); \
        } \
    } while (0)

static auto ErrCodeToString(GLenum err_code) -> string
{
    STACK_TRACE_ENTRY();

#define ERR_CODE_CASE(err_code_variant) \
    case err_code_variant: \
        return #err_code_variant

    switch (err_code) {
        ERR_CODE_CASE(GL_INVALID_ENUM);
        ERR_CODE_CASE(GL_INVALID_VALUE);
        ERR_CODE_CASE(GL_INVALID_OPERATION);
        ERR_CODE_CASE(GL_INVALID_FRAMEBUFFER_OPERATION);
        ERR_CODE_CASE(GL_OUT_OF_MEMORY);
#if !FO_OPENGL_ES
        ERR_CODE_CASE(GL_STACK_OVERFLOW);
        ERR_CODE_CASE(GL_STACK_UNDERFLOW);
#endif
    default:
        return _str("{:#X}", err_code).str();
    }

#undef ERR_CODE_CASE
}
#else
#define GL(expr) expr
#endif

#define GL_HAS(extension) (OGL_##extension)

static bool RenderDebug {};
static bool ForceGlslEsProfile {};
static SDL_Window* SdlWindow {};
static SDL_GLContext GlContext {};
static GLint BaseFrameBufObj {};
static mat44 ProjectionMatrixColMaj {};
static RenderTexture* DummyTexture {};

// ReSharper disable CppInconsistentNaming
static bool OGL_version_2_0 {};
static bool OGL_vertex_buffer_object {};
static bool OGL_framebuffer_object {};
static bool OGL_framebuffer_object_ext {};
static bool OGL_vertex_array_object {};
// ReSharper restore CppInconsistentNaming

class OpenGL_Texture final : public RenderTexture
{
public:
    OpenGL_Texture(uint width, uint height, bool linear_filtered, bool with_depth) : RenderTexture(width, height, linear_filtered, with_depth)
    {
        // Make object outside of constructor to guarantee destructor calling
    }

    ~OpenGL_Texture() override
    {
        if (DepthBuffer != 0u) {
            glDeleteRenderbuffers(1, &DepthBuffer);
        }
        if (TexId != 0u) {
            glDeleteTextures(1, &TexId);
        }
        if (FramebufObj != 0u) {
            glDeleteFramebuffers(1, &FramebufObj);
        }
    }

    [[nodiscard]] auto GetTexturePixel(int x, int y) -> uint override
    {
        auto prev_fbo = 0;
        GL(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo));
        GL(glBindFramebuffer(GL_FRAMEBUFFER, FramebufObj));
        uint result = 0;
        GL(glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &result));
        GL(glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo));
        return result;
    }

    [[nodiscard]] auto GetTextureRegion(int x, int y, int width, int height) -> vector<uint> override
    {
        RUNTIME_ASSERT(width > 0);
        RUNTIME_ASSERT(height > 0);

        const auto size = width * height;
        vector<uint> result(size);

        GLint prev_fbo;
        GL(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo));
        GL(glBindFramebuffer(GL_FRAMEBUFFER, FramebufObj));
        GL(glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, result.data()));
        GL(glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo));

        return result;
    }

    void UpdateTextureRegion(const IRect& r, const uint* data) override
    {
        GL(glBindTexture(GL_TEXTURE_2D, TexId));
        GL(glTexSubImage2D(GL_TEXTURE_2D, 0, r.Left, r.Top, r.Width(), r.Height(), GL_RGBA, GL_UNSIGNED_BYTE, data));
        GL(glBindTexture(GL_TEXTURE_2D, 0));
    }

    GLuint FramebufObj {};
    GLuint TexId {};
    GLuint DepthBuffer {};
};

class OpenGL_DrawBuffer final : public RenderDrawBuffer
{
public:
    explicit OpenGL_DrawBuffer(bool is_static);
    ~OpenGL_DrawBuffer() override;

    void Upload(EffectUsage usage, size_t custom_vertices_size, size_t custom_indices_size) override;

    GLuint VertexArrObj {};
    GLuint VertexBufObj {};
    GLuint IndexBufObj {};
};

class OpenGL_Effect final : public RenderEffect
{
    friend class OpenGL_Renderer;

public:
    OpenGL_Effect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) : RenderEffect(usage, name, loader) { }
    ~OpenGL_Effect() override;

    void DrawBuffer(RenderDrawBuffer* dbuf, size_t start_index, size_t indices_to_draw, RenderTexture* custom_tex) override;

    GLuint Program[EFFECT_MAX_PASSES] {};

    GLint Location_ZoomFactor[EFFECT_MAX_PASSES] {};
    GLint Location_ColorMap[EFFECT_MAX_PASSES] {};
    GLint Location_ColorMapSize[EFFECT_MAX_PASSES] {};
    GLint Location_EggMap[EFFECT_MAX_PASSES] {};
    GLint Location_EggMapSize[EFFECT_MAX_PASSES] {};
    GLint Location_SpriteBorder[EFFECT_MAX_PASSES] {};
    GLint Location_ProjectionMatrix[EFFECT_MAX_PASSES] {};
    GLint Location_GroundPosition[EFFECT_MAX_PASSES] {};
    GLint Location_LightColor[EFFECT_MAX_PASSES] {};
    GLint Location_WorldMatrices[EFFECT_MAX_PASSES] {};
};

void OpenGL_Renderer::Init(GlobalSettings& settings, WindowInternalHandle* window)
{
    STACK_TRACE_ENTRY();

    RenderDebug = settings.RenderDebug;
    ForceGlslEsProfile = settings.ForceGlslEsProfile;
    SdlWindow = static_cast<SDL_Window*>(window);

    // Create context
#if !FO_WEB
    GlContext = SDL_GL_CreateContext(SdlWindow);
    RUNTIME_ASSERT_STR(GlContext, _str("OpenGL context not created, error '{}'", SDL_GetError()));

    const auto make_current = SDL_GL_MakeCurrent(SdlWindow, GlContext);
    RUNTIME_ASSERT_STR(make_current >= 0, _str("Can't set current context, error '{}'", SDL_GetError()));

    if (settings.ClientMode && settings.VSync) {
        if (SDL_GL_SetSwapInterval(-1) == -1) {
            SDL_GL_SetSwapInterval(1);
        }
    }
    else {
        SDL_GL_SetSwapInterval(0);
    }

#else
    EmscriptenWebGLContextAttributes attr;
    emscripten_webgl_init_context_attributes(&attr);
    attr.alpha = EM_FALSE;
    attr.depth = EM_FALSE;
    attr.stencil = EM_FALSE;
    attr.antialias = EM_TRUE;
    attr.premultipliedAlpha = EM_TRUE;
    attr.preserveDrawingBuffer = EM_FALSE;
    attr.powerPreference = EM_WEBGL_POWER_PREFERENCE_HIGH_PERFORMANCE;
    attr.failIfMajorPerformanceCaveat = EM_FALSE;
    attr.enableExtensionsByDefault = EM_TRUE;
    attr.explicitSwapControl = EM_FALSE;
    attr.renderViaOffscreenBackBuffer = EM_FALSE;

    attr.majorVersion = 2;
    attr.minorVersion = 0;
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE gl_context = emscripten_webgl_create_context("#canvas", &attr);
    if (gl_context <= 0) {
        attr.majorVersion = 1;
        attr.minorVersion = 0;
        gl_context = emscripten_webgl_create_context(nullptr, &attr);
        RUNTIME_ASSERT_STR(gl_context > 0, _str("Failed to create WebGL context, error '{}'", (int)gl_context));
        WriteLog("Created WebGL1 context");
    }
    else {
        WriteLog("Created WebGL2 context");
    }

    EMSCRIPTEN_RESULT r = emscripten_webgl_make_context_current(gl_context);
    RUNTIME_ASSERT_STR(r >= 0, _str("Can't set current context, error '{}'", r));

    OGL_vertex_array_object = (attr.majorVersion > 1 || emscripten_webgl_enable_extension(gl_context, "OES_vertex_array_object"));

    GlContext = (SDL_GLContext)gl_context;
#endif

    // Initialize GLEW
    // Todo: remove GLEW and bind OpenGL functions manually
#if !FO_OPENGL_ES
    const auto glew_result = glewInit();
    RUNTIME_ASSERT_STR(glew_result == GLEW_OK, _str("GLEW not initialized, result {}", glew_result));
    OGL_version_2_0 = GLEW_VERSION_2_0 != 0;
    OGL_vertex_buffer_object = GLEW_ARB_vertex_buffer_object != 0;
    OGL_framebuffer_object = GLEW_ARB_framebuffer_object != 0;
    OGL_framebuffer_object_ext = GLEW_EXT_framebuffer_object != 0;
#if FO_MAC
    OGL_vertex_array_object = GLEW_APPLE_vertex_array_object != 0;
#else
    OGL_vertex_array_object = GLEW_ARB_vertex_array_object != 0;
#endif
#endif

    // OpenGL ES extensions
#if FO_OPENGL_ES
    OGL_version_2_0 = true;
    OGL_vertex_buffer_object = true;
    OGL_framebuffer_object = true;
    OGL_framebuffer_object_ext = false;
    OGL_vertex_array_object = false;
#if FO_ANDROID
    OGL_vertex_array_object = SDL_GL_ExtensionSupported("GL_OES_vertex_array_object");
#endif
#if FO_IOS
    OGL_vertex_array_object = true;
#endif
#endif

    // Check OpenGL extensions
#define CHECK_EXTENSION(ext, critical) \
    if (!GL_HAS(ext)) { \
        string msg = (critical) ? "Critical" : "Not critical"; \
        WriteLog("OpenGL extension '" #ext "' not supported. {}", msg); \
        if (critical) { \
            extension_errors++; \
        } \
    }
    uint extension_errors = 0;
    CHECK_EXTENSION(version_2_0, true);
    CHECK_EXTENSION(vertex_buffer_object, true);
    CHECK_EXTENSION(vertex_array_object, false);
    CHECK_EXTENSION(framebuffer_object, false);
    if (!GL_HAS(framebuffer_object)) {
        CHECK_EXTENSION(framebuffer_object_ext, true);
    }
    RUNTIME_ASSERT(!extension_errors);
#undef CHECK_EXTENSION

    // Map framebuffer_object_ext to framebuffer_object
#if !FO_OPENGL_ES
    if (GL_HAS(framebuffer_object_ext) && !GL_HAS(framebuffer_object)) {
        WriteLog("Map framebuffer_object_ext pointers");
        OGL_framebuffer_object = true;
        glGenFramebuffers = glGenFramebuffersEXT;
        glGenRenderbuffers = glGenRenderbuffersEXT;
        glBindFramebuffer = glBindFramebufferEXT;
        glBindRenderbuffer = glBindRenderbufferEXT;
        glDeleteFramebuffers = glDeleteFramebuffersEXT;
        glDeleteRenderbuffers = glDeleteRenderbuffersEXT;
        glFramebufferTexture2D = glFramebufferTexture2DEXT;
        glFramebufferRenderbuffer = glFramebufferRenderbufferEXT;
        glRenderbufferStorage = glRenderbufferStorageEXT;
        glCheckFramebufferStatus = glCheckFramebufferStatusEXT;
    }
#endif

    // Render states
    GL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
    GL(glDisable(GL_DEPTH_TEST));
    GL(glEnable(GL_BLEND));
    GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    GL(glDisable(GL_CULL_FACE));
    GL(glActiveTexture(GL_TEXTURE0));
    GL(glPixelStorei(GL_PACK_ALIGNMENT, 1));
    GL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
#if !FO_OPENGL_ES
    GL(glEnable(GL_TEXTURE_2D));
    GL(glDisable(GL_LIGHTING));
    GL(glDisable(GL_COLOR_MATERIAL));
    GL(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
#endif

    GL(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &BaseFrameBufObj));

    // Calculate atlas size
    GLint max_texture_size;
    GL(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size));
    GLint max_viewport_size[2];
    GL(glGetIntegerv(GL_MAX_VIEWPORT_DIMS, max_viewport_size));
    auto atlas_w = std::min(max_texture_size, AppRender::MAX_ATLAS_SIZE);
    auto atlas_h = atlas_w;
    atlas_w = std::min(max_viewport_size[0], atlas_w);
    atlas_h = std::min(max_viewport_size[1], atlas_h);
    RUNTIME_ASSERT_STR(atlas_w >= AppRender::MIN_ATLAS_SIZE, _str("Min texture width must be at least {}", AppRender::MIN_ATLAS_SIZE));
    RUNTIME_ASSERT_STR(atlas_h >= AppRender::MIN_ATLAS_SIZE, _str("Min texture height must be at least {}", AppRender::MIN_ATLAS_SIZE));
    const_cast<int&>(AppRender::MAX_ATLAS_WIDTH) = atlas_w;
    const_cast<int&>(AppRender::MAX_ATLAS_HEIGHT) = atlas_h;

    // Check max bones
#if FO_ENABLE_3D
#if !FO_OPENGL_ES
    GLint max_uniform_components;
    GL(glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &max_uniform_components));
    if (max_uniform_components < 1024) {
        WriteLog("Warning! GL_MAX_VERTEX_UNIFORM_COMPONENTS is {}", max_uniform_components);
    }
#endif
#endif

    GL(glViewport(0, 0, settings.ScreenWidth, settings.ScreenHeight));

    MatrixHelper::MatrixOrtho(ProjectionMatrixColMaj[0], 0.0f, static_cast<float>(settings.ScreenWidth), static_cast<float>(settings.ScreenHeight), 0.0f, -10.0f, 10.0f);
    ProjectionMatrixColMaj.Transpose(); // Convert to column major order

    constexpr uint dummy_pixel[1] = {0xFFFF00FF};
    DummyTexture = CreateTexture(1, 1, false, false);
    DummyTexture->UpdateTextureRegion({0, 0, 1, 1}, dummy_pixel);
}

void OpenGL_Renderer::Present()
{
    STACK_TRACE_ENTRY();

#if !FO_WEB
    SDL_GL_SwapWindow(SdlWindow);
#endif

    if (const auto err = glGetError(); err != GL_NO_ERROR) {
        throw RenderingException("OpenGL error", err);
    }
}

auto OpenGL_Renderer::CreateTexture(int width, int height, bool linear_filtered, bool with_depth) -> RenderTexture*
{
    STACK_TRACE_ENTRY();

    auto&& opengl_tex = std::make_unique<OpenGL_Texture>(width, height, linear_filtered, with_depth);

    GL(glGenFramebuffers(1, &opengl_tex->FramebufObj));
    GL(glBindFramebuffer(GL_FRAMEBUFFER, opengl_tex->FramebufObj));
    GL(glGenTextures(1, &opengl_tex->TexId));

    GL(glBindTexture(GL_TEXTURE_2D, opengl_tex->TexId));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear_filtered ? GL_LINEAR : GL_NEAREST));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear_filtered ? GL_LINEAR : GL_NEAREST));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP));
    GL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
    GL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, opengl_tex->TexId, 0));

    if (with_depth) {
        GLint cur_rb;
        GL(glGetIntegerv(GL_RENDERBUFFER_BINDING, &cur_rb));
        GL(glGenRenderbuffers(1, &opengl_tex->DepthBuffer));
        GL(glBindRenderbuffer(GL_RENDERBUFFER, opengl_tex->DepthBuffer));
        GL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height));
        GL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, opengl_tex->DepthBuffer));
        GL(glBindRenderbuffer(GL_RENDERBUFFER, cur_rb));
    }

    GLenum status;
    GL(status = glCheckFramebufferStatus(GL_FRAMEBUFFER));
    RUNTIME_ASSERT_STR(status == GL_FRAMEBUFFER_COMPLETE, _str("Framebuffer not created, status {:#X}", status));

    GL(glBindFramebuffer(GL_FRAMEBUFFER, BaseFrameBufObj));

    return opengl_tex.release();
}

auto OpenGL_Renderer::CreateDrawBuffer(bool is_static) -> RenderDrawBuffer*
{
    STACK_TRACE_ENTRY();

    auto&& opengl_dbuf = std::make_unique<OpenGL_DrawBuffer>(is_static);

    return opengl_dbuf.release();
}

auto OpenGL_Renderer::CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> RenderEffect*
{
    STACK_TRACE_ENTRY();

    auto&& opengl_effect = std::make_unique<OpenGL_Effect>(usage, name, loader);

    for (size_t pass = 0; pass < opengl_effect->_passCount; pass++) {
        string ext = "glsl";
        if constexpr (FO_OPENGL_ES) {
            ext = "glsl-es";
        }
        if (ForceGlslEsProfile) {
            ext = "glsl-es";
        }

        const string vert_fname = _str("{}.{}.vert.{}", _str(name).eraseFileExtension(), pass + 1, ext);
        string vert_content = loader(vert_fname);
        RUNTIME_ASSERT(!vert_content.empty());
        const string frag_fname = _str("{}.{}.frag.{}", _str(name).eraseFileExtension(), pass + 1, ext);
        string frag_content = loader(frag_fname);
        RUNTIME_ASSERT(!frag_content.empty());

        // Create shaders
        GLuint vs;
        GL(vs = glCreateShader(GL_VERTEX_SHADER));
        const GLchar* vs_str = vert_content.c_str();
        GL(glShaderSource(vs, 1, &vs_str, nullptr));

        GLuint fs;
        GL(fs = glCreateShader(GL_FRAGMENT_SHADER));
        const GLchar* fs_str = frag_content.c_str();
        GL(glShaderSource(fs, 1, &fs_str, nullptr));

        // Info parser
        const auto get_shader_compile_log = [](GLuint shader) -> string {
            string result = "(no info)";
            int len = 0;
            GL(glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len));
            if (len > 0) {
                auto* str = new GLchar[len];
                int chars = 0;
                GL(glGetShaderInfoLog(shader, len, &chars, str));
                result.assign(str, len);
                delete[] str;
            }
            return result;
        };

        const auto get_program_compile_log = [](GLuint program) -> string {
            string result = "(no info)";
            int len = 0;
            GL(glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len));
            if (len > 0) {
                auto* str = new GLchar[len];
                int chars = 0;
                GL(glGetProgramInfoLog(program, len, &chars, str));
                result.assign(str, len);
                delete[] str;
            }
            return result;
        };

        // Compile vs
        GLint compiled;
        GL(glCompileShader(vs));
        GL(glGetShaderiv(vs, GL_COMPILE_STATUS, &compiled));
        if (compiled == 0) {
            const auto vert_log = get_shader_compile_log(vs);
            GL(glDeleteShader(vs));
            GL(glDeleteShader(fs));
            throw EffectLoadException("Vertex shader not compiled", vert_fname, vert_content, vert_log);
        }

        // Compile fs
        GL(glCompileShader(fs));
        GL(glGetShaderiv(fs, GL_COMPILE_STATUS, &compiled));
        if (compiled == 0) {
            const auto frag_log = get_shader_compile_log(fs);
            GL(glDeleteShader(vs));
            GL(glDeleteShader(fs));
            throw EffectLoadException("Fragment shader not compiled", frag_fname, frag_content, frag_log);
        }

        // Make program
        GLuint program;
        GL(program = glCreateProgram());
        GL(glAttachShader(program, vs));
        GL(glAttachShader(program, fs));

        GL(glLinkProgram(program));
        GLint linked;
        GL(glGetProgramiv(program, GL_LINK_STATUS, &linked));
        if (linked == 0) {
            const auto program_log = get_program_compile_log(program);
            const auto vert_log = get_shader_compile_log(vs);
            const auto frag_log = get_shader_compile_log(fs);
            GL(glDetachShader(program, vs));
            GL(glDetachShader(program, fs));
            GL(glDeleteShader(vs));
            GL(glDeleteShader(fs));
            GL(glDeleteProgram(program));
            throw EffectLoadException("Failed to link shader program", vert_fname, frag_fname, vert_content, frag_content, vert_log, frag_log, program_log);
        }

        opengl_effect->Program[pass] = program;

        GL(opengl_effect->Location_ProjectionMatrix[pass] = glGetUniformLocation(program, "ProjectionMatrix"));
        GL(opengl_effect->Location_ZoomFactor[pass] = glGetUniformLocation(program, "ZoomFactor"));
        GL(opengl_effect->Location_ColorMap[pass] = glGetUniformLocation(program, "ColorMap"));
        GL(opengl_effect->Location_ColorMapSize[pass] = glGetUniformLocation(program, "ColorMapSize"));
        GL(opengl_effect->Location_EggMap[pass] = glGetUniformLocation(program, "EggMap"));
        GL(opengl_effect->Location_EggMapSize[pass] = glGetUniformLocation(program, "EggMapSize"));
        GL(opengl_effect->Location_SpriteBorder[pass] = glGetUniformLocation(program, "SpriteBorder"));
        GL(opengl_effect->Location_GroundPosition[pass] = glGetUniformLocation(program, "GroundPosition"));
        GL(opengl_effect->Location_LightColor[pass] = glGetUniformLocation(program, "LightColor"));
        GL(opengl_effect->Location_WorldMatrices[pass] = glGetUniformLocation(program, "WorldMatrices"));

#if FO_ENABLE_3D
        if (!opengl_effect->ModelBuf) {
            if (usage == EffectUsage::Model) {
                opengl_effect->ModelBuf = RenderEffect::ModelBuffer();
            }
        }
#endif

        if (!opengl_effect->MapSpriteBuf) {
            if (opengl_effect->Location_ZoomFactor[pass] != -1 || opengl_effect->Location_EggMapSize[pass] != -1) {
                opengl_effect->MapSpriteBuf = RenderEffect::MapSpriteBuffer();
            }
        }

        if (!opengl_effect->MainTexBuf) {
            if (opengl_effect->Location_ColorMapSize[pass] != -1) {
                opengl_effect->MainTexBuf = RenderEffect::MainTexBuffer();
            }
        }

        // Bind data
        /*
        GL(effect_pass.Time = glGetUniformLocation(program, "Time"));
        effect_pass.TimeCurrent = 0.0f;
        effect_pass.TimeLastTick = Timer::RealtimeTick();
        GL(effect_pass.TimeGame = glGetUniformLocation(program, "TimeGame"));
        effect_pass.TimeGameCurrent = 0.0f;
        effect_pass.TimeGameLastTick = Timer::RealtimeTick();
        effect_pass.IsTime = (effect_pass.Time != -1 || effect_pass.TimeGame != -1);
        GL(effect_pass.Random1 = glGetUniformLocation(program, "Random1Effect"));
        GL(effect_pass.Random2 = glGetUniformLocation(program, "Random2Effect"));
        GL(effect_pass.Random3 = glGetUniformLocation(program, "Random3Effect"));
        GL(effect_pass.Random4 = glGetUniformLocation(program, "Random4Effect"));
        effect_pass.IsRandom = (effect_pass.Random1 != -1 || effect_pass.Random2 != -1 || effect_pass.Random3 != -1 || effect_pass.Random4 != -1);
        effect_pass.IsTextures = false;
        for (int i = 0; i < EFFECT_TEXTURES; i++) {
            GL(effect_pass.Textures[i] = glGetUniformLocation(program, _str("Texture{}", i).c_str()));
            if (effect_pass.Textures[i] != -1) {
                effect_pass.IsTextures = true;
                GL(effect_pass.TexturesSize[i] = glGetUniformLocation(program, _str("Texture%dSize", i).c_str()));
                GL(effect_pass.TexturesAtlasOffset[i] = glGetUniformLocation(program, _str("Texture{}AtlasOffset", i).c_str()));
            }
        }
        effect_pass.IsScriptValues = false;
        for (int i = 0; i < EFFECT_SCRIPT_VALUES; i++) {
            GL(effect_pass.ScriptValues[i] = glGetUniformLocation(program, _str("EffectValue{}", i).c_str()));
            if (effect_pass.ScriptValues[i] != -1)
                effect_pass.IsScriptValues = true;
        }
        GL(effect_pass.AnimPosProc = glGetUniformLocation(program, "AnimPosProc"));
        GL(effect_pass.AnimPosTime = glGetUniformLocation(program, "AnimPosTime"));
        effect_pass.IsAnimPos = (effect_pass.AnimPosProc != -1 || effect_pass.AnimPosTime != -1);
        effect_pass.IsNeedProcess = (effect_pass.IsTime || effect_pass.IsRandom || effect_pass.IsTextures || effect_pass.IsScriptValues || effect_pass.IsAnimPos || effect_pass.IsChangeStates);

        // Set defaults
        if (defaults) {
            GL(glUseProgram(program));
            for (uint d = 0; d < defaults_count; d++) {
                EffectDefault& def = defaults[d];

                GLint location = -1;
                GL(location = glGetUniformLocation(program, def.Name));
                if (!IS_EFFECT_VALUE(location))
                    continue;

                switch (def.Type) {
                case EffectDefault::String:
                    break;
                case EffectDefault::Float:
                    GL(glUniform1fv(location, def.Size / sizeof(float), (GLfloat*)def.Data));
                    break;
                case EffectDefault::Int:
                    GL(glUniform1iv(location, def.Size / sizeof(int), (GLint*)def.Data));
                    break;
                default:
                    break;
                }
            }
            GL(glUseProgram(0));
        }

        effect_pass.Program = program;
        effect->Passes.push_back(effect_pass);*/
    }

    return opengl_effect.release();

    /*
#define IS_EFFECT_VALUE(pos) ((pos) != -1)
#define SET_EFFECT_VALUE(eff, pos, value) GL(glUniform1f(pos, value))
     // Parse effect commands
    vector<StrVec> commands;
    while (true)
    {
        string line = file.GetNonEmptyLine();
        if (line.empty())
            break;

        if (_str(line).startsWith("Effect "))
        {
            StrVec tokens = _str(line.substr(_str("Effect ").length())).split(' ');
            if (!tokens.empty())
                commands.push_back(tokens);
        }
        else if (_str(line).startsWith("#version"))
        {
            break;
        }
    }

    // Find passes count
    bool fail = false;
    uint passes = 1;
    for (size_t i = 0; i < commands.size(); i++)
        if (commands[i].size() >= 2 && commands[i][0] == "Passes")
            passes = GenericUtils::ResolveGenericValue(commands[i][1], fail);

    // New effect
    auto effect = std::make_unique<Effect>();
    effect->Name = loaded_fname;
    effect->Defines = defines;
    for (uint pass = 0; pass < passes; pass++)
    {
        EffectPass effect_pass;
        auto& cd = effect_pass.ConstantsDesc;

        size_t offs = offsetof(EffectPass, Constants);
        cd.emplace_back(ShaderConstant::Sampler, "ColorMap", offs + offsetof(EffectPass, ColorMap));
        cd.emplace_back(ShaderConstant::Sampler, "EggMap", offs + offsetof(EffectPass, EggMap));
        cd.emplace_back(ShaderConstant::Float4, "ColorMapSize", offs + offsetof(EffectPass::ConstBuffer,
    ColorMapSize)); cd.emplace_back(ShaderConstant::Float, "ColorMapSamples", offs +
    offsetof(EffectPass::ConstBuffer, ColorMapSamples)); cd.emplace_back(ShaderConstant::Float4, "EggMapSize", offs
    + offsetof(EffectPass::ConstBuffer, EggMapSize)); cd.emplace_back(ShaderConstant::Float4, "SpriteBorder", offs +
    offsetof(EffectPass::ConstBuffer, SpriteBorder)); cd.emplace_back(ShaderConstant::Float, "ZoomFactor", offs +
    offsetof(EffectPass::ConstBuffer, ZoomFactor)); cd.emplace_back(ShaderConstant::Matrix, "ProjMatrix", offs +
    offsetof(EffectPass::ConstBuffer, ProjMatrix)); cd.emplace_back(ShaderConstant::Float4, "LightColor", offs +
    offsetof(EffectPass::ConstBuffer, LightColor)); cd.emplace_back(ShaderConstant::Float, "Time", offs +
    offsetof(EffectPass::ConstBuffer, Time)); cd.emplace_back(ShaderConstant::Float, "GameTime", offs +
    offsetof(EffectPass::ConstBuffer, GameTime)); cd.emplace_back(ShaderConstant::Float, "Random1", offs +
    offsetof(EffectPass::ConstBuffer, Random1)); cd.emplace_back(ShaderConstant::Float, "Random2", offs +
    offsetof(EffectPass::ConstBuffer, Random2)); cd.emplace_back(ShaderConstant::Float, "Random3", offs +
    offsetof(EffectPass::ConstBuffer, Random3)); cd.emplace_back(ShaderConstant::Float, "Random4", offs +
    offsetof(EffectPass::ConstBuffer, Random4)); cd.emplace_back(ShaderConstant::Float, "AnimPosProc", offs +
    offsetof(EffectPass::ConstBuffer, AnimPosProc)); cd.emplace_back(ShaderConstant::Float, "AnimPosTime", offs +
    offsetof(EffectPass::ConstBuffer, AnimPosTime));

        for (uint i = 0; i < EFFECT_SCRIPT_VALUES; i++)
        {
            cd.emplace_back(ShaderConstant::Float, _str("ScriptValue{}", i),
                offs + offsetof(EffectPass::ConstBuffer, ScriptValue) + i * sizeof(float));
        }

        for (uint i = 0; i < EFFECT_TEXTURES; i++)
        {
            cd.emplace_back(
                ShaderConstant::Sampler, _str("Texture{}", i), offs + offsetof(EffectPass, TexMaps) + i *
    sizeof(Texture*));

            size_t tex_offs = offsetof(EffectPass, ConstantsTex) + i * sizeof(EffectPass::ConstBufferTex);
            cd.emplace_back(ShaderConstant::Float4, _str("Texture{}Size", i),
                tex_offs + offsetof(EffectPass::ConstBufferTex, TextureSize));
            cd.emplace_back(ShaderConstant::Float2, _str("Texture{}AtlasOffset", i),
                tex_offs + offsetof(EffectPass::ConstBufferTex, TextureAtlasOffset));
        }

        effect_pass.ConstantsOffset = offsetof(EffectPass, Constants);

        if (!use_in_2d)
        {
            cd.emplace_back(ShaderConstant::Float4, "GroundPosition",
                offsetof(EffectPass, Constants3D) + offsetof(EffectPass::ConstBuffer3D, GroundPosition));
            cd.emplace_back(ShaderConstant::MatrixList, "WorldMatrices",
                offsetof(EffectPass, Constants3D) + offsetof(EffectPass::ConstBuffer3D, WorldMatrices));

            effect_pass.Constants3DOffset = offsetof(EffectPass, Constants3D);
        }

        effect->Passes[pass] = std::move(effect_pass);
        effect->PassCount++;
    }

    if (!LoadEffectPass(effect.get(), fname, file, pass, use_in_2d, defines, defaults, defaults_count))
        return nullptr;

    // Process commands
    for (size_t i = 0; i < commands.size(); i++)
    {
        StrVec& tokens = commands[i];
        if (tokens[0] == "Pass" && tokens.size() >= 3)
        {
            uint pass = GenericUtils::ResolveGenericValue(tokens[1], fail);
            if (pass < passes)
            {
                EffectPass& effect_pass = effect->Passes[pass];
                if (tokens[2] == "BlendFunc" && tokens.size() >= 5)
                {
                    effect_pass.IsNeedProcess = effect_pass.IsChangeStates = true;
                    effect_pass.BlendFuncParam1 = get_gl_blend_func(tokens[3]);
                    effect_pass.BlendFuncParam2 = get_gl_blend_func(tokens[4]);
                    if (effect_pass.BlendFuncParam1 == -1 || effect_pass.BlendFuncParam2 == -1)
                        fail = true;
                }
                else if (tokens[2] == "BlendEquation" && tokens.size() >= 4)
                {
                    effect_pass.IsNeedProcess = effect_pass.IsChangeStates = true;
                    effect_pass.BlendEquation = get_gl_blend_equation(tokens[3]);
                    if (effect_pass.BlendEquation == -1)
                        fail = true;
                }
                else if (tokens[2] == "Shadow")
                {
                    effect_pass.IsShadow = true;
                }
                else
                {
                    fail = true;
                }
            }
            else
            {
                fail = true;
            }
        }
    }
    if (fail)
    {
        WriteLog("Invalid commands in effect '{}'", fname);
        return nullptr;
    }*/
}

void OpenGL_Renderer::SetRenderTarget(RenderTexture* tex)
{
    STACK_TRACE_ENTRY();

    int w;
    int h;

    if (tex != nullptr) {
        const auto* opengl_tex = static_cast<OpenGL_Texture*>(tex);
        GL(glBindFramebuffer(GL_FRAMEBUFFER, opengl_tex->FramebufObj));
        w = opengl_tex->Width;
        h = opengl_tex->Height;
    }
    else {
        GL(glBindFramebuffer(GL_FRAMEBUFFER, BaseFrameBufObj));
        w = App->Settings.ScreenWidth;
        h = App->Settings.ScreenHeight;
    }

    GL(glViewport(0, 0, w, h));

    MatrixHelper::MatrixOrtho(ProjectionMatrixColMaj[0], 0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, -10.0f, 10.0f);
    ProjectionMatrixColMaj.Transpose(); // Convert to column major order

    /*
     *int w, h;
    bool screen_size;
    if (!rtStack.empty()) {
        RenderTarget* rt = rtStack.back();
        w = rt->TargetTexture->Width;
        h = rt->TargetTexture->Height;
        screen_size = (rt->ScreenSize && !rt->Width && !rt->Height);
    }
    else {
        GetWindowSize(w, h);
        screen_size = true;
    }

    if (screen_size && settings.Fullscreen) {
        // Preserve aspect ratio in fullscreen mode
        float native_aspect = (float)w / h;
        float aspect = (float)settings.ScreenWidth / settings.ScreenHeight;
        int new_w = (int)roundf((aspect <= native_aspect ? (float)h * aspect : (float)h * native_aspect));
        int new_h = (int)roundf((aspect <= native_aspect ? (float)w / native_aspect : (float)w / aspect));
        GL(glViewport((w - new_w) / 2, (h - new_h) / 2, new_w, new_h));
    }
    else {
        GL(glViewport(0, 0, w, h));
    }

    if (screen_size) {
        AppWindow::MatrixOrtho(projectionMatrixCM[0], 0.0f, (float)settings.ScreenWidth, (float)settings.ScreenHeight, 0.0f, -1.0f, 1.0f);
    }
    else {
        AppWindow::MatrixOrtho(projectionMatrixCM[0], 0.0f, (float)w, (float)h, 0.0f, -1.0f, 1.0f);
    }
    projectionMatrixCM.Transpose(); // Convert to column major order
    */
}

void OpenGL_Renderer::ClearRenderTarget(optional<uint> color, bool depth, bool stencil)
{
    STACK_TRACE_ENTRY();

    GLbitfield clear_flags = 0;

    if (color.has_value()) {
        const auto color_value = color.value();
        const auto a = static_cast<float>((color_value >> 24) & 0xFF) / 255.0f;
        const auto r = static_cast<float>((color_value >> 16) & 0xFF) / 255.0f;
        const auto g = static_cast<float>((color_value >> 8) & 0xFF) / 255.0f;
        const auto b = static_cast<float>((color_value >> 0) & 0xFF) / 255.0f;
        GL(glClearColor(r, g, b, a));
        clear_flags |= GL_COLOR_BUFFER_BIT;
    }

    if (depth) {
        GL(glClearDepthf(1.0f));
        clear_flags |= GL_DEPTH_BUFFER_BIT;
    }

    if (stencil) {
        GL(glClearStencil(0));
        clear_flags |= GL_STENCIL_BUFFER_BIT;
    }

    if (clear_flags != 0) {
        GL(glClear(clear_flags));
    }
}

void OpenGL_Renderer::EnableScissor(int x, int y, int width, int height)
{
    STACK_TRACE_ENTRY();

    GL(glEnable(GL_SCISSOR_TEST));
    GL(glScissor(x, y, width, height));
}

void OpenGL_Renderer::DisableScissor()
{
    STACK_TRACE_ENTRY();

    GL(glDisable(GL_SCISSOR_TEST));
}

static void EnableVertAtribs(EffectUsage usage)
{
    STACK_TRACE_ENTRY();

#if FO_ENABLE_3D
    if (usage == EffectUsage::Model) {
        GL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, Position))));
        GL(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, Normal))));
        GL(glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, TexCoord))));
        GL(glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, TexCoordBase))));
        GL(glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, Tangent))));
        GL(glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, Bitangent))));
        GL(glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, BlendWeights))));
        GL(glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, BlendIndices))));
        GL(glVertexAttribPointer(8, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, Color))));
        for (uint i = 0; i <= 8; i++) {
            GL(glEnableVertexAttribArray(i));
        }

        return;
    }
#endif

    GL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), reinterpret_cast<const GLvoid*>(offsetof(Vertex2D, PosX))));
    GL(glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex2D), reinterpret_cast<const GLvoid*>(offsetof(Vertex2D, Color))));
    GL(glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), reinterpret_cast<const GLvoid*>(offsetof(Vertex2D, TexU))));
    GL(glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), reinterpret_cast<const GLvoid*>(offsetof(Vertex2D, EggTexU))));
    for (uint i = 0; i <= 3; i++) {
        GL(glEnableVertexAttribArray(i));
    }
}

static void DisableVertAtribs(EffectUsage usage)
{
    STACK_TRACE_ENTRY();

#if FO_ENABLE_3D
    if (usage == EffectUsage::Model) {
        for (uint i = 0; i <= 7; i++) {
            GL(glDisableVertexAttribArray(i));
        }

        return;
    }
#endif

    for (uint i = 0; i <= 3; i++) {
        GL(glDisableVertexAttribArray(i));
    }
}

OpenGL_DrawBuffer::OpenGL_DrawBuffer(bool is_static) : RenderDrawBuffer(is_static)
{
    STACK_TRACE_ENTRY();

    GL(glGenBuffers(1, &VertexBufObj));
    GL(glGenBuffers(1, &IndexBufObj));
}

OpenGL_DrawBuffer::~OpenGL_DrawBuffer()
{
    STACK_TRACE_ENTRY();

    if (VertexBufObj != 0u) {
        glDeleteBuffers(1, &VertexBufObj);
    }
    if (IndexBufObj != 0u) {
        glDeleteBuffers(1, &IndexBufObj);
    }
    if (VertexArrObj != 0u) {
        glDeleteVertexArrays(1, &VertexArrObj);
    }
}

void OpenGL_DrawBuffer::Upload(EffectUsage usage, size_t custom_vertices_size, size_t custom_indices_size)
{
    STACK_TRACE_ENTRY();

    if (IsStatic && !StaticDataChanged) {
        return;
    }

    StaticDataChanged = false;

    const auto buf_type = IsStatic ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW;

    // Fill vertex buffer
    GL(glBindBuffer(GL_ARRAY_BUFFER, VertexBufObj));

    size_t upload_vertices;

#if FO_ENABLE_3D
    if (usage == EffectUsage::Model) {
        RUNTIME_ASSERT(Vertices2D.empty());
        upload_vertices = custom_vertices_size == static_cast<size_t>(-1) ? Vertices3D.size() : custom_vertices_size;
        GL(glBufferData(GL_ARRAY_BUFFER, upload_vertices * sizeof(Vertex3D), Vertices3D.data(), buf_type));
    }
    else {
        RUNTIME_ASSERT(Vertices3D.empty());
        upload_vertices = custom_vertices_size == static_cast<size_t>(-1) ? Vertices2D.size() : custom_vertices_size;
        GL(glBufferData(GL_ARRAY_BUFFER, upload_vertices * sizeof(Vertex2D), Vertices2D.data(), buf_type));
    }
#else
    upload_vertices = custom_vertices_size == static_cast<size_t>(-1) ? Vertices2D.size() : custom_vertices_size;
    GL(glBufferData(GL_ARRAY_BUFFER, upload_vertices * sizeof(Vertex2D), Vertices2D.data(), buf_type));
#endif

    GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    // Auto generate indices
    bool need_upload_indices = false;
    if (!IsStatic) {
        switch (usage) {
#if FO_ENABLE_3D
        case EffectUsage::Model:
#endif
        case EffectUsage::ImGui: {
            // Nothing, indices generated manually
            need_upload_indices = true;
        } break;
        case EffectUsage::QuadSprite: {
            // Sprite quad
            RUNTIME_ASSERT(upload_vertices % 4 == 0);
            auto& indices = Indices;
            const auto need_size = upload_vertices / 4 * 6;
            if (indices.size() < need_size) {
                const auto prev_size = indices.size();
                indices.resize(need_size);
                for (size_t i = prev_size / 6, j = indices.size() / 6; i < j; i++) {
                    indices[i * 6 + 0] = static_cast<ushort>(i * 4 + 0);
                    indices[i * 6 + 1] = static_cast<ushort>(i * 4 + 1);
                    indices[i * 6 + 2] = static_cast<ushort>(i * 4 + 3);
                    indices[i * 6 + 3] = static_cast<ushort>(i * 4 + 1);
                    indices[i * 6 + 4] = static_cast<ushort>(i * 4 + 2);
                    indices[i * 6 + 5] = static_cast<ushort>(i * 4 + 3);
                }

                need_upload_indices = true;
            }
        } break;
        case EffectUsage::Primitive: {
            // One to one
            auto& indices = Indices;
            if (indices.size() < upload_vertices) {
                const auto prev_size = indices.size();
                indices.resize(upload_vertices);
                for (size_t i = prev_size; i < indices.size(); i++) {
                    indices[i] = static_cast<ushort>(i);
                }

                need_upload_indices = true;
            }
        } break;
        }
    }
    else {
        need_upload_indices = true;
    }

    // Fill index buffer
    if (need_upload_indices) {
        const auto upload_indices = custom_indices_size == static_cast<size_t>(-1) ? Indices.size() : custom_indices_size;

        GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBufObj));
        GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, upload_indices * sizeof(ushort), Indices.data(), buf_type));
        GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
    }

    // Vertex array
    if (VertexArrObj == 0u && GL_HAS(vertex_array_object)) {
        GL(glGenVertexArrays(1, &VertexArrObj));
        GL(glBindVertexArray(VertexArrObj));
        GL(glBindBuffer(GL_ARRAY_BUFFER, VertexBufObj));
        GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBufObj));
        EnableVertAtribs(usage);
        GL(glBindVertexArray(0));
        GL(glBindBuffer(GL_ARRAY_BUFFER, 0));
        GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
    }
}

static auto ConvertBlendFunc(BlendFuncType name) -> GLenum
{
    STACK_TRACE_ENTRY();

    switch (name) {
#define CHECK_ENTRY(name, glname) \
    case BlendFuncType::name: \
        return glname
        CHECK_ENTRY(Zero, GL_ZERO);
        CHECK_ENTRY(One, GL_ONE);
        CHECK_ENTRY(SrcColor, GL_SRC_COLOR);
        CHECK_ENTRY(InvSrcColor, GL_ONE_MINUS_SRC_COLOR);
        CHECK_ENTRY(DstColor, GL_DST_COLOR);
        CHECK_ENTRY(InvDstColor, GL_ONE_MINUS_DST_COLOR);
        CHECK_ENTRY(SrcAlpha, GL_SRC_ALPHA);
        CHECK_ENTRY(InvSrcAlpha, GL_ONE_MINUS_SRC_ALPHA);
        CHECK_ENTRY(DstAlpha, GL_DST_ALPHA);
        CHECK_ENTRY(InvDstAlpha, GL_ONE_MINUS_DST_ALPHA);
        CHECK_ENTRY(ConstantColor, GL_CONSTANT_COLOR);
        CHECK_ENTRY(InvConstantColor, GL_ONE_MINUS_CONSTANT_COLOR);
        CHECK_ENTRY(SrcAlphaSaturate, GL_SRC_ALPHA_SATURATE);
#undef CHECK_ENTRY
    }
    throw UnreachablePlaceException(LINE_STR);
}

static auto ConvertBlendEquation(BlendEquationType name) -> GLenum
{
    STACK_TRACE_ENTRY();

    switch (name) {
#define CHECK_ENTRY(name, glname) \
    case BlendEquationType::name: \
        return glname
        CHECK_ENTRY(FuncAdd, GL_FUNC_ADD);
        CHECK_ENTRY(FuncSubtract, GL_FUNC_SUBTRACT);
        CHECK_ENTRY(FuncReverseSubtract, GL_FUNC_REVERSE_SUBTRACT);
        CHECK_ENTRY(Max, GL_MAX);
        CHECK_ENTRY(Min, GL_MIN);
#undef CHECK_ENTRY
    }
    throw UnreachablePlaceException(LINE_STR);
}

OpenGL_Effect::~OpenGL_Effect()
{
    STACK_TRACE_ENTRY();

    for (size_t i = 0; i < _passCount; i++) {
        if (Program[i] != 0u) {
            glDeleteProgram(Program[i]);
        }
    }
}

void OpenGL_Effect::DrawBuffer(RenderDrawBuffer* dbuf, size_t start_index, size_t indices_to_draw, RenderTexture* custom_tex)
{
    STACK_TRACE_ENTRY();

    const auto* opengl_dbuf = static_cast<OpenGL_DrawBuffer*>(dbuf);

    GLenum draw_mode = GL_TRIANGLES;

    if (Usage == EffectUsage::Primitive) {
        switch (opengl_dbuf->PrimType) {
        case RenderPrimitiveType::PointList:
            draw_mode = GL_POINTS;
            break;
        case RenderPrimitiveType::LineList:
            draw_mode = GL_LINES;
            break;
        case RenderPrimitiveType::LineStrip:
            draw_mode = GL_LINE_STRIP;
            break;
        case RenderPrimitiveType::TriangleList:
            draw_mode = GL_TRIANGLES;
            break;
        case RenderPrimitiveType::TriangleStrip:
            draw_mode = GL_TRIANGLE_STRIP;
            break;
        case RenderPrimitiveType::TriangleFan:
            draw_mode = GL_TRIANGLE_FAN;
            break;
        }

#if !FO_OPENGL_ES
        if (MapSpriteBuf && MapSpriteBuf->ZoomFactor != 1.0f) {
            if (opengl_dbuf->PrimType == RenderPrimitiveType::PointList) {
                GL(glEnable(GL_POINT_SMOOTH));
            }
            else if (opengl_dbuf->PrimType == RenderPrimitiveType::LineList || opengl_dbuf->PrimType == RenderPrimitiveType::LineStrip) {
                GL(glEnable(GL_LINE_SMOOTH));
            }
        }
#endif
    }

    if (DisableBlending) {
        GL(glDisable(GL_BLEND));
    }

#if FO_ENABLE_3D
    if (Usage == EffectUsage::Model) {
        GL(glEnable(GL_DEPTH_TEST));

        if (!DisableCulling) {
            GL(glEnable(GL_CULL_FACE));
        }
    }
#endif

    if (opengl_dbuf->VertexArrObj != 0u) {
        GL(glBindVertexArray(opengl_dbuf->VertexArrObj));
    }
    else {
        GL(glBindBuffer(GL_ARRAY_BUFFER, opengl_dbuf->VertexBufObj));
        GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, opengl_dbuf->IndexBufObj));
        EnableVertAtribs(Usage);
    }

    const auto* main_tex = static_cast<OpenGL_Texture*>(custom_tex != nullptr ? custom_tex : (CustomTex[0] != nullptr ? CustomTex[0] : (MainTex != nullptr ? MainTex : DummyTexture)));
    const auto* egg_tex = EggTex != nullptr ? EggTex : DummyTexture;
    const auto draw_count = static_cast<GLsizei>(indices_to_draw == static_cast<size_t>(-1) ? opengl_dbuf->Indices.size() : indices_to_draw);
    const auto* start_pos = reinterpret_cast<const GLvoid*>(start_index * sizeof(ushort));

    for (size_t pass = 0; pass < _passCount; pass++) {
#if FO_ENABLE_3D
        if (DisableShadow && _isShadow[pass]) {
            continue;
        }
#endif

        GL(glUseProgram(Program[pass]));

        if (Location_ProjectionMatrix[pass] != -1) {
            if (ProjBuf) {
                GL(glUniformMatrix4fv(Location_ProjectionMatrix[pass], 1, GL_FALSE, ProjBuf->ProjMatrix));
            }
            else {
                GL(glUniformMatrix4fv(Location_ProjectionMatrix[pass], 1, GL_FALSE, ProjectionMatrixColMaj[0]));
            }
        }

        if (Location_ColorMap[pass] != -1) {
            GL(glBindTexture(GL_TEXTURE_2D, main_tex->TexId));
            GL(glUniform1i(Location_ColorMap[pass], 0));

            if (Location_ColorMapSize[pass] != -1) {
                GL(glUniform4fv(Location_ColorMapSize[pass], 1, main_tex->SizeData));
            }
        }

        if (Location_EggMap[pass] != -1) {
            GL(glActiveTexture(GL_TEXTURE1));
            GL(glBindTexture(GL_TEXTURE_2D, static_cast<const OpenGL_Texture*>(egg_tex)->TexId));
            GL(glUniform1i(Location_EggMap[pass], 1));
            GL(glActiveTexture(GL_TEXTURE0));

            if (Location_EggMapSize[pass] != -1) {
                GL(glUniform4fv(Location_EggMapSize[pass], 1, egg_tex->SizeData));
            }
        }

        if (Location_ZoomFactor[pass] != -1) {
            RUNTIME_ASSERT(MapSpriteBuf->ZoomFactor != 0.0f);
            GL(glUniform1f(Location_ZoomFactor[pass], MapSpriteBuf->ZoomFactor));
        }

        if (Location_SpriteBorder[pass] != -1) {
            GL(glUniform4fv(Location_SpriteBorder[pass], 1, BorderBuf->SpriteBorder));
        }

#if FO_ENABLE_3D
        if (Location_GroundPosition[pass] != -1) {
            GL(glUniform4fv(Location_GroundPosition[pass], 1, ModelBuf->GroundPosition));
        }
        if (Location_LightColor[pass] != -1) {
            GL(glUniform4fv(Location_LightColor[pass], 1, ModelBuf->LightColor));
        }
        if (Location_WorldMatrices[pass] != -1) {
            RUNTIME_ASSERT(MatrixCount != 0);
            GL(glUniformMatrix4fv(Location_WorldMatrices[pass], static_cast<GLsizei>(MatrixCount), GL_FALSE, &ModelBuf->WorldMatrices[0]));
        }
#endif

        if (_srcBlendFunc[pass] != BlendFuncType::SrcAlpha || _destBlendFunc[pass] != BlendFuncType::InvSrcAlpha) {
            GL(glBlendFunc(ConvertBlendFunc(_srcBlendFunc[pass]), ConvertBlendFunc(_destBlendFunc[pass])));
        }
        if (_blendEquation[pass] != BlendEquationType::FuncAdd) {
            GL(glBlendEquation(ConvertBlendEquation(_blendEquation[pass])));
        }
        if (!_depthWrite[pass]) {
            GL(glDepthMask(GL_FALSE));
        }

        // Todo: bind time, random, anim

        GL(glDrawElements(draw_mode, draw_count, GL_UNSIGNED_SHORT, start_pos));

        if (_srcBlendFunc[pass] != BlendFuncType::SrcAlpha || _destBlendFunc[pass] != BlendFuncType::InvSrcAlpha) {
            GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        }
        if (_blendEquation[pass] != BlendEquationType::FuncAdd) {
            GL(glBlendEquation(GL_FUNC_ADD));
        }
        if (!_depthWrite[pass]) {
            GL(glDepthMask(GL_TRUE));
        }
    }

    GL(glUseProgram(0));

    if (opengl_dbuf->VertexArrObj != 0u) {
        GL(glBindVertexArray(0));
    }
    else {
        DisableVertAtribs(Usage);
    }

    if (DisableBlending) {
        GL(glEnable(GL_BLEND));
    }

#if FO_ENABLE_3D
    if (Usage == EffectUsage::Model) {
        GL(glDisable(GL_DEPTH_TEST));

        if (!DisableCulling) {
            GL(glDisable(GL_CULL_FACE));
        }
    }
#endif

    if (Usage == EffectUsage::Primitive) {
#if !FO_OPENGL_ES
        if (MapSpriteBuf && MapSpriteBuf->ZoomFactor != 1.0f) {
            if (opengl_dbuf->PrimType == RenderPrimitiveType::PointList) {
                GL(glDisable(GL_POINT_SMOOTH));
            }
            else if (opengl_dbuf->PrimType == RenderPrimitiveType::LineList || opengl_dbuf->PrimType == RenderPrimitiveType::LineStrip) {
                GL(glDisable(GL_LINE_SMOOTH));
            }
        }
#endif
    }
}

#endif
