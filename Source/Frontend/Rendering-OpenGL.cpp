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

#include "Rendering.h"

#if FO_HAVE_OPENGL

#include "Application.h"
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

#if !FO_DEBUG
#define GL(expr) expr
#else
#define GL(expr) \
    do { \
        expr; \
        if (RenderDebug) { \
            GLenum err__ = glGetError(); \
            RUNTIME_ASSERT_STR(err__ == GL_NO_ERROR, _str(#expr " error {:#X}", err__)); \
        } \
    } while (0)
#endif

#define GL_HAS(extension) (OGL_##extension)

static bool RenderDebug {};
static SDL_Window* SdlWindow {};
static SDL_GLContext GlContext {};
static GLint BaseFrameBufObj {};

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

    [[nodiscard]] auto GetTextureRegion(int x, int y, uint w, uint h) -> vector<uint> override
    {
        RUNTIME_ASSERT(w && h);
        const auto size = w * h;
        vector<uint> result(size);

        GLint prev_fbo;
        GL(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo));
        GL(glBindFramebuffer(GL_FRAMEBUFFER, FramebufObj));
        GL(glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, &result[0]));
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
    explicit OpenGL_DrawBuffer(bool is_static) : RenderDrawBuffer(is_static) { }
    ~OpenGL_DrawBuffer() override;

    GLuint VertexArrObj {};
    GLuint VertexBufObj {};
    GLuint IndexBufObj {};
};

class OpenGL_Effect final : public RenderEffect
{
    friend class OpenGL_Renderer;

public:
    OpenGL_Effect(EffectUsage usage, string_view name, string_view defines, const RenderEffectLoader& loader) : RenderEffect(usage, name, defines, loader) { }
    ~OpenGL_Effect() override;
    void DrawBuffer(RenderDrawBuffer* dbuf, size_t start_index = 0, optional<size_t> indices_to_draw = std::nullopt, RenderTexture* custom_tex = nullptr) override;

    GLuint Program[EFFECT_MAX_PASSES] {};

    GLint Location_ZoomFactor;
    GLint Location_ColorMap;
    GLint Location_ColorMapSize;
    GLint Location_ColorMapSamples;
    GLint Location_EggMap;
    GLint Location_EggMapSize;
    GLint Location_SpriteBorder;
    GLint Location_ProjectionMatrix;
    GLint Location_GroundPosition;
    GLint Location_LightColor;
    GLint Location_WorldMatrices;
};

void OpenGL_Renderer::Init(GlobalSettings& settings, SDL_Window* window)
{
    RenderDebug = settings.RenderDebug;
    SdlWindow = window;

#if !FO_WEB
    GlContext = SDL_GL_CreateContext(window);
    RUNTIME_ASSERT_STR(GlContext, _str("OpenGL context not created, error '{}'", SDL_GetError()));

    const auto make_current = SDL_GL_MakeCurrent(window, GlContext);
    RUNTIME_ASSERT_STR(make_current >= 0, _str("Can't set current context, error '{}'", SDL_GetError()));

    // SDL_GL_SetSwapInterval(settings.VSync ? 1 : 0);
    SDL_GL_SetSwapInterval(1);

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
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE gl_context = emscripten_webgl_create_context(nullptr, &attr);
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
    if (GL_HAS(framebuffer_object_ext) && !GL_HAS(framebuffer_object)) {
        OGL_framebuffer_object = true;
        // glGenFramebuffers = glGenFramebuffersEXT;
        // glBindFramebuffer = glBindFramebufferEXT;
        // glFramebufferTexture2D = glFramebufferTexture2DEXT;
        // Todo: map all framebuffer ext functions
    }

    // Render states
    GL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
    GL(glDisable(GL_DEPTH_TEST));
    GL(glEnable(GL_BLEND));
    GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    GL(glDisable(GL_CULL_FACE));
    GL(glEnable(GL_TEXTURE_2D));
    GL(glActiveTexture(GL_TEXTURE0));
    GL(glPixelStorei(GL_PACK_ALIGNMENT, 1));
    GL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
#if !FO_OPENGL_ES
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
    auto atlas_w = std::min(static_cast<uint>(max_texture_size), Application::AppRender::MAX_ATLAS_SIZE);
    auto atlas_h = atlas_w;
    atlas_w = std::min(static_cast<uint>(max_viewport_size[0]), atlas_w);
    atlas_h = std::min(static_cast<uint>(max_viewport_size[1]), atlas_h);
    RUNTIME_ASSERT_STR(atlas_w >= Application::AppRender::MIN_ATLAS_SIZE, _str("Min texture width must be at least {}", Application::AppRender::MIN_ATLAS_SIZE));
    RUNTIME_ASSERT_STR(atlas_h >= Application::AppRender::MIN_ATLAS_SIZE, _str("Min texture height must be at least {}", Application::AppRender::MIN_ATLAS_SIZE));
    const_cast<uint&>(Application::AppRender::MAX_ATLAS_WIDTH) = atlas_w;
    const_cast<uint&>(Application::AppRender::MAX_ATLAS_HEIGHT) = atlas_h;

    // Check max bones
    if (settings.Enable3dRendering) {
#if !FO_OPENGL_ES
        GLint max_uniform_components;
        GL(glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &max_uniform_components));
        if (max_uniform_components < 1024) {
            WriteLog("Warning! GL_MAX_VERTEX_UNIFORM_COMPONENTS is {}", max_uniform_components);
        }
#endif
    }

    GL(glViewport(0, 0, settings.ScreenWidth, settings.ScreenHeight));
}

void OpenGL_Renderer::Present()
{
#if !FO_WEB
    SDL_GL_SwapWindow(SdlWindow);
#endif

    if (RenderDebug && glGetError() != GL_NO_ERROR) {
        throw RenderingException("Unknown place of OpenGL error");
    }
}

auto OpenGL_Renderer::CreateTexture(uint width, uint height, bool linear_filtered, bool with_depth) -> RenderTexture*
{
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
    auto&& opengl_dbuf = std::make_unique<OpenGL_DrawBuffer>(is_static);

    return opengl_dbuf.release();
}

OpenGL_DrawBuffer::~OpenGL_DrawBuffer()
{
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

auto OpenGL_Renderer::CreateEffect(EffectUsage usage, string_view name, string_view defines, const RenderEffectLoader& loader) -> RenderEffect*
{
    auto&& opengl_effect = std::make_unique<OpenGL_Effect>(usage, name, defines, loader);

    for (size_t pass = 0; pass < opengl_effect->_passCount; pass++) {
        string ext = "glsl";
        if constexpr (FO_OPENGL_ES) {
            ext = "glsl-es";
        }

        const string vert_fname = _str("{}.{}.vert.{}", name, pass + 1, ext);
        string vert_content = loader(vert_fname);
        RUNTIME_ASSERT(!vert_content.empty());
        const string frag_fname = _str("{}.{}.frag.{}", name, pass + 1, ext);
        string frag_content = loader(frag_fname);
        RUNTIME_ASSERT(!frag_content.empty());

        // Todo: additional defines to shaders
        // defines
        // string internal_defines = _str("#define PASS{}\n#define MAX_BONES {}\n", pass, MaxBones);

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
                glGetShaderInfoLog(shader, len, &chars, str);
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
                glGetProgramInfoLog(program, len, &chars, str);
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

        GL(opengl_effect->Location_ProjectionMatrix = glGetUniformLocation(program, "ProjectionMatrix"));
        GL(opengl_effect->Location_ZoomFactor = glGetUniformLocation(program, "ZoomFactor"));
        GL(opengl_effect->Location_ColorMap = glGetUniformLocation(program, "ColorMap"));
        GL(opengl_effect->Location_ColorMapSize = glGetUniformLocation(program, "ColorMapSize"));
        GL(opengl_effect->Location_ColorMapSamples = glGetUniformLocation(program, "ColorMapSamples"));
        GL(opengl_effect->Location_EggMap = glGetUniformLocation(program, "EggMap"));
        GL(opengl_effect->Location_EggMapSize = glGetUniformLocation(program, "EggMapSize"));
        GL(opengl_effect->Location_SpriteBorder = glGetUniformLocation(program, "SpriteBorder"));
        GL(opengl_effect->Location_GroundPosition = glGetUniformLocation(program, "GroundPosition"));
        GL(opengl_effect->Location_LightColor = glGetUniformLocation(program, "LightColor"));
        GL(opengl_effect->Location_WorldMatrices = glGetUniformLocation(program, "WorldMatrices"));

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
    if (tex != nullptr) {
        const auto* opengl_tex = static_cast<OpenGL_Texture*>(tex);
        GL(glBindFramebuffer(GL_FRAMEBUFFER, opengl_tex->FramebufObj));
        GL(glViewport(0, 0, opengl_tex->Width, opengl_tex->Height));
    }
    else {
        GL(glBindFramebuffer(GL_FRAMEBUFFER, BaseFrameBufObj));
        GL(glViewport(0, 0, App->Settings.ScreenWidth, App->Settings.ScreenHeight));
    }

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

    if (screen_size && settings.FullScreen) {
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
        Application::AppWindow::MatrixOrtho(projectionMatrixCM[0], 0.0f, (float)settings.ScreenWidth, (float)settings.ScreenHeight, 0.0f, -1.0f, 1.0f);
    }
    else {
        Application::AppWindow::MatrixOrtho(projectionMatrixCM[0], 0.0f, (float)w, (float)h, 0.0f, -1.0f, 1.0f);
    }
    projectionMatrixCM.Transpose(); // Convert to column major order
    */
}

void OpenGL_Renderer::ClearRenderTarget(optional<uint> color, bool depth, bool stencil)
{
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
        clear_flags |= GL_DEPTH_BUFFER_BIT;
    }

    if (stencil) {
        clear_flags |= GL_STENCIL_BUFFER_BIT;
    }

    if (clear_flags != 0) {
        GL(glClear(clear_flags));
    }
}

void OpenGL_Renderer::EnableScissor(int x, int y, uint w, uint h)
{
    GL(glEnable(GL_SCISSOR_TEST));
    GL(glScissor(x, y, w, h));
}

void OpenGL_Renderer::DisableScissor()
{
    GL(glDisable(GL_SCISSOR_TEST));
}

OpenGL_Effect::~OpenGL_Effect()
{
    for (size_t i = 0; i < _passCount; i++) {
        if (Program[i] != 0u) {
            glDeleteProgram(Program[i]);
        }
    }
}

/*void OpenGL_Effect::DrawQuads(const Vertex2DVec& vbuf, const vector<ushort>& ibuf, size_t pos, size_t count, RenderTexture* tex)
{
    // EnableVertexArray(quadsVertexArray, 4 * curDrawQuad);

    for (size_t pass = 0; pass < _passCount; pass++) {
        GL(glUseProgram(Program[pass]));

        / *
        if (IS_EFFECT_VALUE(effect_pass.ZoomFactor))
            GL(glUniform1f(effect_pass.ZoomFactor, settings.SpritesZoom));
        if (IS_EFFECT_VALUE(effect_pass.ProjectionMatrix))
            GL(glUniformMatrix4fv(effect_pass.ProjectionMatrix, 1, GL_FALSE, projectionMatrixCM[0]));
        if (IS_EFFECT_VALUE(effect_pass.ColorMap) && dip.SourceTexture)
        {
            if (dip.SourceTexture->Samples == 0.0f)
            {
                GL(glBindTexture(GL_TEXTURE_2D, dip.SourceTexture->Id));
            }
            else
            {
                GL(glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, dip.SourceTexture->Id));
                if (IS_EFFECT_VALUE(effect_pass.ColorMapSamples))
                    GL(glUniform1f(effect_pass.ColorMapSamples, dip.SourceTexture->Samples));
            }
            GL(glUniform1i(effect_pass.ColorMap, 0));
            if (IS_EFFECT_VALUE(effect_pass.ColorMapSize))
                GL(glUniform4fv(effect_pass.ColorMapSize, 1, dip.SourceTexture->SizeData));
        }
        if (IS_EFFECT_VALUE(effect_pass.EggMap) && sprEgg)
        {
            GL(glActiveTexture(GL_TEXTURE1));
            GL(glBindTexture(GL_TEXTURE_2D, sprEgg->Atlas->TextureOwner->Id));
            GL(glActiveTexture(GL_TEXTURE0));
            GL(glUniform1i(effect_pass.EggMap, 1));
            if (IS_EFFECT_VALUE(effect_pass.EggMapSize))
                GL(glUniform4fv(effect_pass.EggMapSize, 1, sprEgg->Atlas->TextureOwner->SizeData));
        }
        if (IS_EFFECT_VALUE(effect_pass.SpriteBorder))
            GL(glUniform4f(effect_pass.SpriteBorder, dip.SpriteBorder.L, dip.SpriteBorder.T, dip.SpriteBorder.R,
                dip.SpriteBorder.B));

        if (effect_pass.IsNeedProcess)
            effectMngr.EffectProcessVariables(effect_pass, true);
        * /

        GL(glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(count), GL_UNSIGNED_SHORT, reinterpret_cast<const GLvoid*>(pos * sizeof(ushort))));

        // if (effect_pass.IsNeedProcess)
        //     effectMngr.EffectProcessVariables(effect_pass, false);
    }

    GL(glUseProgram(0));

    // DisableVertexArray(quadsVertexArray);
}

void OpenGL_Effect::DrawPrimitive(RenderPrimitiveType prim, const Vertex2DVec& vbuf, const vector<ushort>& ibuf)
{
    / // Enable smooth
#if !FO_OPENGL_ES
    if (zoom && *zoom != 1.0f) {
        GL(glEnable(prim == PRIMITIVE_POINTLIST ? GL_POINT_SMOOTH : GL_LINE_SMOOTH));
    }
#endif

    // Draw
    EnableVertexArray(pointsVertexArray, count);
    EnableScissor();

    for (size_t pass = 0; pass < draw_effect->Passes.size(); pass++) {
        EffectPass& effect_pass = draw_effect->Passes[pass];

        GL(glUseProgram(effect_pass.Program));

        if (IS_EFFECT_VALUE(effect_pass.ProjectionMatrix))
            GL(glUniformMatrix4fv(effect_pass.ProjectionMatrix, 1, GL_FALSE, projectionMatrixCM[0]));

        if (effect_pass.IsNeedProcess)
            effectMngr.EffectProcessVariables(effect_pass, true);

        GL(glDrawElements(prim_type, count, GL_UNSIGNED_SHORT, (void*)0));

        if (effect_pass.IsNeedProcess)
            effectMngr.EffectProcessVariables(effect_pass, false);
    }

    GL(glUseProgram(0));
    DisableVertexArray(pointsVertexArray);
    DisableScissor();

// Disable smooth
#if !FO_OPENGL_ES
    if (zoom && *zoom != 1.0f) {
        GL(glDisable(prim == PRIMITIVE_POINTLIST ? GL_POINT_SMOOTH : GL_LINE_SMOOTH));
    }
#endif
}*/

static void EnableVertAtribs(EffectUsage usage)
{
    if (usage == EffectUsage::Model) {
        GL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, Position))));
        GL(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, Normal))));
        GL(glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, TexCoord))));
        GL(glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, TexCoordBase))));
        GL(glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, Tangent))));
        GL(glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, Bitangent))));
        GL(glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, BlendWeights))));
        GL(glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, BlendIndices))));
        for (uint i = 0; i <= 7; i++) {
            GL(glEnableVertexAttribArray(i));
        }
    }
    else {
        GL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), reinterpret_cast<const GLvoid*>(offsetof(Vertex2D, X))));
        GL(glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex2D), reinterpret_cast<const GLvoid*>(offsetof(Vertex2D, Diffuse))));
        GL(glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), reinterpret_cast<const GLvoid*>(offsetof(Vertex2D, TU))));
        GL(glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), reinterpret_cast<const GLvoid*>(offsetof(Vertex2D, TUEgg))));
        for (uint i = 0; i <= 3; i++) {
            GL(glEnableVertexAttribArray(i));
        }
    }
}

static void DisableVertAtribs(EffectUsage usage)
{
    if (usage == EffectUsage::Model) {
        for (uint i = 0; i <= 7; i++) {
            GL(glDisableVertexAttribArray(i));
        }
    }
    else {
        for (uint i = 0; i <= 3; i++) {
            GL(glDisableVertexAttribArray(i));
        }
    }
}

void OpenGL_Effect::DrawBuffer(RenderDrawBuffer* dbuf, size_t start_index, optional<size_t> indices_to_draw, RenderTexture* custom_tex)
{
    auto* opengl_dbuf = static_cast<OpenGL_DrawBuffer*>(dbuf);

    if (opengl_dbuf->VertexBufObj == 0u || dbuf->DataChanged) {
        dbuf->DataChanged = false;

        // Regenerate static buffers
        if (dbuf->IsStatic) {
            if (opengl_dbuf->VertexBufObj != 0u) {
                GL(glDeleteBuffers(1, &opengl_dbuf->VertexBufObj));
                opengl_dbuf->VertexBufObj = 0u;
            }
            if (opengl_dbuf->IndexBufObj != 0u) {
                GL(glDeleteBuffers(1, &opengl_dbuf->IndexBufObj));
                opengl_dbuf->IndexBufObj = 0u;
            }
        }

        if (opengl_dbuf->VertexBufObj == 0u) {
            GL(glGenBuffers(1, &opengl_dbuf->VertexBufObj));
        }
        if (opengl_dbuf->IndexBufObj == 0u) {
            GL(glGenBuffers(1, &opengl_dbuf->IndexBufObj));
        }

        const auto buf_type = dbuf->IsStatic ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW;

        // Fill vertex buffer
        GL(glBindBuffer(GL_ARRAY_BUFFER, opengl_dbuf->VertexBufObj));

        if (Usage == EffectUsage::Model) {
            RUNTIME_ASSERT(dbuf->Vertices2D.empty());
            GL(glBufferData(GL_ARRAY_BUFFER, dbuf->Vertices3D.size() * sizeof(Vertex3D), dbuf->Vertices3D.data(), buf_type));
        }
        else {
            RUNTIME_ASSERT(dbuf->Vertices3D.empty());
            GL(glBufferData(GL_ARRAY_BUFFER, dbuf->Vertices2D.size() * sizeof(Vertex2D), dbuf->Vertices2D.data(), buf_type));
        }

        // Auto generate indices
        auto& indices = dbuf->Indices;
        switch (Usage) {
        case EffectUsage::ImGui:
        case EffectUsage::Model: {
            // Nothing, indices generated manually
        } break;
        case EffectUsage::Font:
        case EffectUsage::MapSprite:
        case EffectUsage::Interface: {
            // Sprite quad
            RUNTIME_ASSERT(dbuf->Vertices2D.size() % 4 == 0);
            indices.resize(dbuf->Vertices2D.size() / 4 * 6);
            for (size_t i = 0; i < indices.size(); i += 6) {
                indices[i * 6 + 0] = static_cast<ushort>(i * 4 + 0);
                indices[i * 6 + 1] = static_cast<ushort>(i * 4 + 1);
                indices[i * 6 + 2] = static_cast<ushort>(i * 4 + 3);
                indices[i * 6 + 3] = static_cast<ushort>(i * 4 + 1);
                indices[i * 6 + 4] = static_cast<ushort>(i * 4 + 2);
                indices[i * 6 + 5] = static_cast<ushort>(i * 4 + 3);
            }
        } break;
        case EffectUsage::Flush:
        case EffectUsage::Contour:
        case EffectUsage::Primitive: {
            // One to one
            indices.resize(dbuf->Vertices2D.size());
            for (size_t i = 0; i < indices.size(); i++) {
                indices[i] = static_cast<ushort>(i);
            }
        } break;
        }

        // Fill index buffer
        GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, opengl_dbuf->IndexBufObj));
        GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, dbuf->Indices.size() * sizeof(ushort), dbuf->Indices.data(), buf_type));

        // Create vertex array object
        if (opengl_dbuf->VertexArrObj == 0u && GL_HAS(vertex_array_object)) {
            GL(glGenVertexArrays(1, &opengl_dbuf->VertexArrObj));
            GL(glBindVertexArray(opengl_dbuf->VertexArrObj));
            EnableVertAtribs(Usage);
            GL(glBindVertexArray(0));
        }
    }

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
        // Todo: smooth only if( zoom && *zoom != 1.0f )
        if (opengl_dbuf->PrimType == RenderPrimitiveType::PointList) {
            GL(glEnable(GL_POINT_SMOOTH));
        }
        else if (opengl_dbuf->PrimType == RenderPrimitiveType::LineList || opengl_dbuf->PrimType == RenderPrimitiveType::LineStrip) {
            GL(glEnable(GL_LINE_SMOOTH));
        }
#endif
    }

    if (Usage == EffectUsage::Model) {
        GL(glEnable(GL_DEPTH_TEST));

        if (!dbuf->DisableModelCulling) {
            GL(glEnable(GL_CULL_FACE));
        }
    }

    if (opengl_dbuf->VertexArrObj != 0u) {
        GL(glBindVertexArray(opengl_dbuf->VertexArrObj));
        GL(glBindBuffer(GL_ARRAY_BUFFER, opengl_dbuf->VertexBufObj));
        GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, opengl_dbuf->IndexBufObj));
    }
    else {
        GL(glBindBuffer(GL_ARRAY_BUFFER, opengl_dbuf->VertexBufObj));
        GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, opengl_dbuf->IndexBufObj));
        EnableVertAtribs(Usage);
    }

    const auto* opnegl_tex = static_cast<OpenGL_Texture*>(custom_tex != nullptr ? custom_tex : MainTex);
    const auto draw_count = static_cast<GLsizei>(indices_to_draw.value_or(dbuf->Indices.size()));
    const auto* start_pos = reinterpret_cast<const GLvoid*>(start_index * sizeof(ushort));

    // Effect* effect = (combined_mesh->DrawEffect ? combined_mesh->DrawEffect : modelMngr.effectMngr.Effects.Skinned3d);
    // MeshTexture** textures = combined_mesh->Textures;

    for (size_t pass = 0; pass < _passCount; pass++) {
        // if (shadow_disabled && effect_pass.IsShadow)
        //     continue;

        GL(glUseProgram(Program[pass]));

        if (Location_ProjectionMatrix != -1) {
            GL(glUniformMatrix4fv(Location_ProjectionMatrix, 1, GL_FALSE, ProjBuf.ProjMatrix));
        }

        if (Location_ColorMap != -1) {
            if (opnegl_tex == nullptr) {
                throw RenderingException("Trying to draw effect that required texture", Name);
            }

            GL(glBindTexture(GL_TEXTURE_2D, opnegl_tex->TexId));
            GL(glUniform1i(Location_ColorMap, 0));

#ifdef GL_SAMPLER_BINDING
            GL(glBindSampler(0, 0));
#endif
        }

        if (Location_ColorMapSize != -1) {
            GL(glUniform4fv(Location_ColorMapSize, 1, opnegl_tex->SizeData));
        }

        // GL(glActiveTexture(GL_TEXTURE0));

        /*
         if (IS_EFFECT_VALUE(effect_pass.ZoomFactor))
            GL(glUniform1f(effect_pass.ZoomFactor, modelMngr.settings.SpritesZoom));
        if (IS_EFFECT_VALUE(effect_pass.ProjectionMatrix))
            GL(glUniformMatrix4fv(effect_pass.ProjectionMatrix, 1, GL_FALSE, modelMngr.matrixProjCM[0]));
        if (IS_EFFECT_VALUE(effect_pass.ColorMap) && textures[0]) {
            GL(glBindTexture(GL_TEXTURE_2D, textures[0]->Id));
            GL(glUniform1i(effect_pass.ColorMap, 0));
            if (IS_EFFECT_VALUE(effect_pass.ColorMapSize))
                GL(glUniform4fv(effect_pass.ColorMapSize, 1, textures[0]->SizeData));
        }
        if (IS_EFFECT_VALUE(effect_pass.LightColor))
            GL(glUniform4fv(effect_pass.LightColor, 1, (float*)&modelMngr.lightColor));
        if (IS_EFFECT_VALUE(effect_pass.WorldMatrices)) {
            GL(glUniformMatrix4fv(effect_pass.WorldMatrices, (GLsizei)combined_mesh->CurBoneMatrix, GL_FALSE, (float*)&modelMngr.worldMatrices[0]));
        }
        if (IS_EFFECT_VALUE(effect_pass.GroundPosition))
            GL(glUniform3fv(effect_pass.GroundPosition, 1, (float*)&groundPos));

        if (effect_pass.IsNeedProcess)
            modelMngr.effectMngr.EffectProcessVariables(effect_pass, true, animPosProc, animPosTime, textures);
        */

        GL(glDrawElements(draw_mode, draw_count, GL_UNSIGNED_SHORT, start_pos));

        // if (effect_pass.IsNeedProcess)
        //     modelMngr.effectMngr.EffectProcessVariables(effect_pass, false, animPosProc, animPosTime, textures);
    }

    GL(glUseProgram(0));

    if (opengl_dbuf->VertexArrObj != 0u) {
        GL(glBindVertexArray(0));
    }
    else {
        DisableVertAtribs(Usage);
    }

    if (Usage == EffectUsage::Model) {
        GL(glDisable(GL_DEPTH_TEST));

        if (!dbuf->DisableModelCulling) {
            GL(glDisable(GL_CULL_FACE));
        }
    }

    if (Usage == EffectUsage::Primitive) {
#if !FO_OPENGL_ES
        if (opengl_dbuf->PrimType == RenderPrimitiveType::PointList) {
            GL(glDisable(GL_POINT_SMOOTH));
        }
        else if (opengl_dbuf->PrimType == RenderPrimitiveType::LineList || opengl_dbuf->PrimType == RenderPrimitiveType::LineStrip) {
            GL(glDisable(GL_LINE_SMOOTH));
        }
#endif
    }
}

#endif
