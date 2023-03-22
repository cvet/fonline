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

#pragma once

#include "Common.h"

#include "Settings.h"

DECLARE_EXCEPTION(EffectLoadException);
DECLARE_EXCEPTION(RenderingException);

using WindowInternalHandle = void;

constexpr size_t EFFECT_MAX_PASSES = 6;
constexpr size_t EFFECT_SCRIPT_VALUES = 16;

#if FO_ENABLE_3D
constexpr size_t MODEL_MAX_TEXTURES = 8;
constexpr size_t MODEL_MAX_BONES = 54;
constexpr size_t BONES_PER_VERTEX = 4;
#endif

using RenderEffectLoader = std::function<string(string_view)>;

enum class RenderType
{
    Null,
#if FO_HAVE_OPENGL
    OpenGL,
#endif
#if FO_HAVE_DIRECT_3D
    Direct3D,
#endif
#if FO_HAVE_METAL
    Metal,
#endif
#if FO_HAVE_VULKAN
    Vulkan,
#endif
#if FO_HAVE_GNM
    GNM,
#endif
};

enum class EffectUsage
{
    ImGui,
    QuadSprite,
    Primitive,
#if FO_ENABLE_3D
    Model,
#endif
};

enum class RenderPrimitiveType
{
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
};

enum class BlendFuncType
{
    Zero,
    One,
    SrcColor,
    InvSrcColor,
    DstColor,
    InvDstColor,
    SrcAlpha,
    InvSrcAlpha,
    DstAlpha,
    InvDstAlpha,
    ConstantColor,
    InvConstantColor,
    SrcAlphaSaturate,
};

enum class BlendEquationType
{
    FuncAdd,
    FuncSubtract,
    FuncReverseSubtract,
    Max,
    Min,
};

struct Vertex2D
{
    float PosX;
    float PosY;
    uint Color;
    float TexU;
    float TexV;
    float EggTexU;
    float EggTexV;
};
static_assert(std::is_standard_layout_v<Vertex2D>);
static_assert(sizeof(Vertex2D) == 28);

#if FO_ENABLE_3D
struct Vertex3D
{
    vec3 Position;
    vec3 Normal;
    float TexCoord[2];
    float TexCoordBase[2];
    vec3 Tangent;
    vec3 Bitangent;
    float BlendWeights[BONES_PER_VERTEX];
    float BlendIndices[BONES_PER_VERTEX];
    uint Color;
};
static_assert(std::is_standard_layout_v<Vertex3D>);
static_assert(sizeof(Vertex3D) == 100);
#endif

class RenderTexture
{
public:
    RenderTexture(const RenderTexture&) = delete;
    RenderTexture(RenderTexture&&) noexcept = delete;
    auto operator=(const RenderTexture&) = delete;
    auto operator=(RenderTexture&&) noexcept = delete;
    virtual ~RenderTexture() = default;

    [[nodiscard]] virtual auto GetTexturePixel(int x, int y) const -> uint = 0;
    [[nodiscard]] virtual auto GetTextureRegion(int x, int y, int width, int height) const -> vector<uint> = 0;

    virtual void UpdateTextureRegion(const IRect& r, const uint* data) = 0;

    const int Width;
    const int Height;
    const float SizeData[4]; // Width, Height, TexelWidth, TexelHeight
    const bool LinearFiltered;
    const bool WithDepth;
    const bool FlippedHeight;

protected:
    RenderTexture(int width, int height, bool linear_filtered, bool with_depth, bool flipped_height);
};

class RenderDrawBuffer
{
public:
    RenderDrawBuffer(const RenderDrawBuffer&) = delete;
    RenderDrawBuffer(RenderDrawBuffer&&) noexcept = delete;
    auto operator=(const RenderDrawBuffer&) = delete;
    auto operator=(RenderDrawBuffer&&) noexcept = delete;
    virtual ~RenderDrawBuffer() = default;

    virtual void Upload(EffectUsage usage, size_t custom_vertices_size = static_cast<size_t>(-1), size_t custom_indices_size = static_cast<size_t>(-1)) = 0;

    const bool IsStatic;

    vector<Vertex2D> Vertices2D {};
    vector<uint16> Indices {};
    bool StaticDataChanged {};
    RenderPrimitiveType PrimType {};
    bool PrimZoomed {};
#if FO_ENABLE_3D
    vector<Vertex3D> Vertices3D {};
#endif

protected:
    explicit RenderDrawBuffer(bool is_static);
};

class RenderEffect
{
public:
    struct ProjBuffer
    {
        float ProjMatrix[16] {}; // mat44
    };

    struct MainTexBuffer
    {
        float MainTexSize[4] {}; // vec4
    };

    struct ContourBuffer
    {
        float SpriteBorder[4] {}; // vec4
    };

    struct TimeBuffer
    {
        float FrameTime[4] {}; // vec4
        float GameTime[4] {}; // vec4
    };

    struct RandomValueBuffer
    {
        float RandomValue[4] {}; // vec4
    };

    struct ScriptValueBuffer
    {
        float ScriptValue[EFFECT_SCRIPT_VALUES] {}; // float
    };

#if FO_ENABLE_3D
    struct ModelBuffer
    {
        float LightColor[4] {}; // vec4
        float GroundPosition[4] {}; // vec4
        float WorldMatrices[16 * MODEL_MAX_BONES] {}; // mat44
    };

    struct ModelTexBuffer
    {
        float TexAtlasOffset[4 * MODEL_MAX_TEXTURES] {}; // vec4
        float TexSize[4 * MODEL_MAX_TEXTURES] {}; // vec4
    };

    struct ModelAnimBuffer
    {
        float AnimNormalizedTime[4] {}; // vec4
        float AnimAbsoluteTime[4] {}; // vec4
    };
#endif

    static_assert(sizeof(ProjBuffer) % 16 == 0 && sizeof(ProjBuffer) == 64);
    static_assert(sizeof(MainTexBuffer) % 16 == 0 && sizeof(MainTexBuffer) == 16);
    static_assert(sizeof(ContourBuffer) % 16 == 0 && sizeof(ContourBuffer) == 16);
    static_assert(sizeof(TimeBuffer) % 16 == 0 && sizeof(TimeBuffer) == 32);
    static_assert(sizeof(RandomValueBuffer) % 16 == 0 && sizeof(RandomValueBuffer) == 16);
    static_assert(sizeof(ScriptValueBuffer) % 16 == 0 && sizeof(ScriptValueBuffer) == 64);
#if FO_ENABLE_3D
    static_assert(sizeof(ModelBuffer) % 16 == 0 && sizeof(ModelBuffer) == 3488);
    static_assert(sizeof(ModelTexBuffer) % 16 == 0 && sizeof(ModelTexBuffer) == 256);
    static_assert(sizeof(ModelAnimBuffer) % 16 == 0 && sizeof(ModelAnimBuffer) == 32);
#endif
    // Total size: 3984
    // Need fit to 4096, that value guaranteed by GL_MAX_VERTEX_UNIFORM_COMPONENTS (1024 * sizeof(float))

    RenderEffect(const RenderEffect&) = delete;
    RenderEffect(RenderEffect&&) noexcept = delete;
    auto operator=(const RenderEffect&) = delete;
    auto operator=(RenderEffect&&) noexcept = delete;
    virtual ~RenderEffect() = default;

    [[nodiscard]] auto CanBatch(const RenderEffect* other) const -> bool;

    const string Name;
    const EffectUsage Usage;
    RenderTexture* MainTex {};
    RenderTexture* EggTex {};
    bool DisableBlending {};
#if FO_ENABLE_3D
    RenderTexture* ModelTex[MODEL_MAX_TEXTURES] {};
    bool DisableShadow {};
    bool DisableCulling {};
    size_t MatrixCount {};
#endif

    const bool NeedMainTex {};
    const bool NeedEggTex {};
    const bool NeedProjBuf {};
    const bool NeedMainTexBuf {};
    const bool NeedContourBuf {};
    const bool NeedTimeBuf {};
    const bool NeedRandomValueBuf {};
    const bool NeedScriptValueBuf {};
#if FO_ENABLE_3D
    const bool NeedModelBuf {};
    const bool NeedAnyModelTex {};
    const bool NeedModelTex[MODEL_MAX_TEXTURES] {};
    const bool NeedModelTexBuf {};
    const bool NeedModelAnimBuf {};
#endif

    optional<ProjBuffer> ProjBuf {};
    optional<MainTexBuffer> MainTexBuf {};
    optional<ContourBuffer> ContourBuf {};
    optional<TimeBuffer> TimeBuf {};
    optional<RandomValueBuffer> RandomValueBuf {};
    optional<ScriptValueBuffer> ScriptValueBuf {};
#if FO_ENABLE_3D
    optional<ModelBuffer> ModelBuf {};
    optional<ModelTexBuffer> ModelTexBuf {};
    optional<ModelAnimBuffer> ModelAnimBuf {};
#endif

    virtual void DrawBuffer(RenderDrawBuffer* dbuf, size_t start_index = 0, size_t indices_to_draw = static_cast<size_t>(-1), RenderTexture* custom_tex = nullptr) = 0;

protected:
    RenderEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader);

    string _effectName {};
    hstring _name {};
    size_t _passCount {};
    BlendFuncType _srcBlendFunc[EFFECT_MAX_PASSES] {};
    BlendFuncType _destBlendFunc[EFFECT_MAX_PASSES] {};
    BlendEquationType _blendEquation[EFFECT_MAX_PASSES] {};
    bool _depthWrite[EFFECT_MAX_PASSES] {};
#if FO_ENABLE_3D
    bool _isShadow[EFFECT_MAX_PASSES] {};
#endif

    const int _posMainTex[EFFECT_MAX_PASSES] {};
    const int _posEggTex[EFFECT_MAX_PASSES] {};
    const int _posProjBuf[EFFECT_MAX_PASSES] {};
    const int _posMainTexBuf[EFFECT_MAX_PASSES] {};
    const int _posContourBuf[EFFECT_MAX_PASSES] {};
    const int _posTimeBuf[EFFECT_MAX_PASSES] {};
    const int _posRandomValueBuf[EFFECT_MAX_PASSES] {};
    const int _posScriptValueBuf[EFFECT_MAX_PASSES] {};
#if FO_ENABLE_3D
    const int _posModelBuf[EFFECT_MAX_PASSES] {};
    const int _posModelTex[EFFECT_MAX_PASSES][MODEL_MAX_TEXTURES] {};
    const int _posModelTexBuf[EFFECT_MAX_PASSES] {};
    const int _posModelAnimBuf[EFFECT_MAX_PASSES] {};
#endif
};

class Renderer
{
public:
    virtual ~Renderer() = default;

    [[nodiscard]] virtual auto CreateTexture(int width, int height, bool linear_filtered, bool with_depth) -> RenderTexture* = 0;
    [[nodiscard]] virtual auto CreateDrawBuffer(bool is_static) -> RenderDrawBuffer* = 0;
    [[nodiscard]] virtual auto CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> RenderEffect* = 0;
    [[nodiscard]] virtual auto CreateOrthoMatrix(float left, float right, float bottom, float top, float nearp, float farp) -> mat44 = 0;
    [[nodiscard]] virtual auto GetViewPort() -> IRect = 0;

    virtual void Init(GlobalSettings& settings, WindowInternalHandle* window) = 0;
    virtual void Present() = 0;
    virtual void SetRenderTarget(RenderTexture* tex) = 0;
    virtual void ClearRenderTarget(optional<uint> color, bool depth = false, bool stencil = false) = 0;
    virtual void EnableScissor(int x, int y, int width, int height) = 0;
    virtual void DisableScissor() = 0;
    virtual void OnResizeWindow(int width, int height) = 0;
};

class Null_Renderer final : public Renderer
{
public:
    [[nodiscard]] auto CreateTexture(int width, int height, bool linear_filtered, bool with_depth) -> RenderTexture* override { return nullptr; }
    [[nodiscard]] auto CreateDrawBuffer(bool is_static) -> RenderDrawBuffer* override { return nullptr; }
    [[nodiscard]] auto CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> RenderEffect* override { return nullptr; }
    [[nodiscard]] auto CreateOrthoMatrix(float left, float right, float bottom, float top, float nearp, float farp) -> mat44 override { return {}; }
    [[nodiscard]] auto GetViewPort() -> IRect override { return {}; }

    void Init(GlobalSettings& settings, WindowInternalHandle* window) override { }
    void Present() override { }
    void SetRenderTarget(RenderTexture* tex) override { }
    void ClearRenderTarget(optional<uint> color, bool depth = false, bool stencil = false) override { }
    void EnableScissor(int x, int y, int width, int height) override { }
    void DisableScissor() override { }
    void OnResizeWindow(int width, int height) override { }
};

#if FO_HAVE_OPENGL

class OpenGL_Renderer final : public Renderer
{
public:
    static constexpr auto RING_BUFFER_LENGTH = 300;

    [[nodiscard]] auto CreateTexture(int width, int height, bool linear_filtered, bool with_depth) -> RenderTexture* override;
    [[nodiscard]] auto CreateDrawBuffer(bool is_static) -> RenderDrawBuffer* override;
    [[nodiscard]] auto CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> RenderEffect* override;
    [[nodiscard]] auto CreateOrthoMatrix(float left, float right, float bottom, float top, float nearp, float farp) -> mat44 override;
    [[nodiscard]] auto GetViewPort() -> IRect override;

    void Init(GlobalSettings& settings, WindowInternalHandle* window) override;
    void Present() override;
    void SetRenderTarget(RenderTexture* tex) override;
    void ClearRenderTarget(optional<uint> color, bool depth = false, bool stencil = false) override;
    void EnableScissor(int x, int y, int width, int height) override;
    void DisableScissor() override;
    void OnResizeWindow(int width, int height) override;
};

#endif

#if FO_HAVE_DIRECT_3D

class Direct3D_Renderer final : public Renderer
{
public:
    [[nodiscard]] auto CreateTexture(int width, int height, bool linear_filtered, bool with_depth) -> RenderTexture* override;
    [[nodiscard]] auto CreateDrawBuffer(bool is_static) -> RenderDrawBuffer* override;
    [[nodiscard]] auto CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> RenderEffect* override;
    [[nodiscard]] auto CreateOrthoMatrix(float left, float right, float bottom, float top, float nearp, float farp) -> mat44 override;
    [[nodiscard]] auto GetViewPort() -> IRect override;

    void Init(GlobalSettings& settings, WindowInternalHandle* window) override;
    void Present() override;
    void SetRenderTarget(RenderTexture* tex) override;
    void ClearRenderTarget(optional<uint> color, bool depth = false, bool stencil = false) override;
    void EnableScissor(int x, int y, int width, int height) override;
    void DisableScissor() override;
    void OnResizeWindow(int width, int height) override;
};

#endif
