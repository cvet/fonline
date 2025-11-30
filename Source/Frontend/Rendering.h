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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

FO_BEGIN_NAMESPACE();

FO_DECLARE_EXCEPTION(EffectLoadException);
FO_DECLARE_EXCEPTION(RenderingException);

using WindowInternalHandle = void;

constexpr size_t EFFECT_MAX_PASSES = 6;
constexpr size_t EFFECT_SCRIPT_VALUES = 16;

#if FO_ENABLE_3D
constexpr size_t MODEL_MAX_TEXTURES = 8;
constexpr size_t MODEL_MAX_BONES = 54;
constexpr size_t BONES_PER_VERTEX = 4;
#endif

#if FO_RENDER_32BIT_INDEX
using vindex_t = uint32;
#else
using vindex_t = uint16;
#endif

using RenderEffectLoader = function<string(string_view)>;

enum class RenderType : uint8
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
};

enum class EffectUsage : uint8
{
    ImGui,
    QuadSprite,
    Primitive,
#if FO_ENABLE_3D
    Model,
#endif
};

///@ ExportEnum
enum class RenderPrimitiveType : uint8
{
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
};

enum class BlendFuncType : uint8
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

enum class BlendEquationType : uint8
{
    FuncAdd,
    FuncSubtract,
    FuncReverseSubtract,
    Max,
    Min,
};

struct Vertex2D
{
    float32 PosX {};
    float32 PosY {};
    float32 PosZ {};
    ucolor Color {};
    float32 TexU {};
    float32 TexV {};
    float32 EggTexU {};
    float32 EggTexV {};
};
static_assert(std::is_standard_layout_v<Vertex2D>);
static_assert(sizeof(Vertex2D) == 32);

#if FO_ENABLE_3D
struct Vertex3D
{
    vec3 Position {};
    vec3 Normal {};
    float32 TexCoord[2] {};
    float32 TexCoordBase[2] {};
    vec3 Tangent {};
    vec3 Bitangent {};
    float32 BlendWeights[BONES_PER_VERTEX] {};
    float32 BlendIndices[BONES_PER_VERTEX] {};
    ucolor Color {};
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

    [[nodiscard]] virtual auto GetTexturePixel(ipos32 pos) const -> ucolor = 0;
    [[nodiscard]] virtual auto GetTextureRegion(ipos32 pos, isize32 size) const -> vector<ucolor> = 0;

    virtual void UpdateTextureRegion(ipos32 pos, isize32 size, const ucolor* data, bool use_dest_pitch = false) = 0;

    const isize32 Size;
    const float32 SizeData[4]; // Width, Height, TexelWidth, TexelHeight
    const bool LinearFiltered;
    const bool WithDepth;

    bool FlippedHeight {};

protected:
    RenderTexture(isize32 size, bool linear_filtered, bool with_depth);
};

class RenderDrawBuffer
{
public:
    RenderDrawBuffer(const RenderDrawBuffer&) = delete;
    RenderDrawBuffer(RenderDrawBuffer&&) noexcept = delete;
    auto operator=(const RenderDrawBuffer&) = delete;
    auto operator=(RenderDrawBuffer&&) noexcept = delete;
    virtual ~RenderDrawBuffer() = default;

    virtual void Upload(EffectUsage usage, optional<size_t> custom_vertices_size = std::nullopt, optional<size_t> custom_indices_size = std::nullopt) = 0;

    void CheckAllocBuf(size_t vcount, size_t icount);

    const bool IsStatic;

    vector<Vertex2D> Vertices {};
    size_t VertCount {};
    vector<vindex_t> Indices {};
    size_t IndCount {};
    bool StaticDataChanged {};
    RenderPrimitiveType PrimType {};
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
        float32 ProjMatrix[16] {}; // mat44
    };

    struct MainTexBuffer
    {
        float32 MainTexSize[4] {}; // vec4
    };

    struct ContourBuffer
    {
        float32 SpriteBorder[4] {}; // vec4
    };

    struct TimeBuffer
    {
        float32 FrameTime[4] {}; // vec4
        float32 GameTime[4] {}; // vec4
    };

    struct RandomValueBuffer
    {
        float32 RandomValue[4] {}; // vec4
    };

    struct ScriptValueBuffer
    {
        float32 ScriptValue[EFFECT_SCRIPT_VALUES] {}; // float32
    };

#if FO_ENABLE_3D
    struct ModelBuffer
    {
        float32 LightColor[4] {}; // vec4
        float32 GroundPosition[4] {}; // vec4
        float32 WorldMatrices[16 * MODEL_MAX_BONES] {}; // mat44
    };

    struct ModelTexBuffer
    {
        float32 TexAtlasOffset[4 * MODEL_MAX_TEXTURES] {}; // vec4
        float32 TexSize[4 * MODEL_MAX_TEXTURES] {}; // vec4
    };

    struct ModelAnimBuffer
    {
        float32 AnimNormalizedTime[4] {}; // vec4
        float32 AnimAbsoluteTime[4] {}; // vec4
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
    // Need fit to 4096, that value guaranteed by GL_MAX_VERTEX_UNIFORM_COMPONENTS (1024 * sizeof(float32))

    RenderEffect(const RenderEffect&) = delete;
    RenderEffect(RenderEffect&&) noexcept = delete;
    auto operator=(const RenderEffect&) = delete;
    auto operator=(RenderEffect&&) noexcept = delete;
    virtual ~RenderEffect() = default;

    [[nodiscard]] auto GetName() const -> const string& { return _name; }
    [[nodiscard]] auto GetUsage() const -> EffectUsage { return _usage; }
    [[nodiscard]] auto GetPassCount() const -> size_t { return _passCount; }

    [[nodiscard]] auto IsNeedMainTex() const -> bool { return _needMainTex; }
    [[nodiscard]] auto IsNeedEggTex() const -> bool { return _needEggTex; }
    [[nodiscard]] auto IsNeedProjBuf() const -> bool { return _needProjBuf; }
    [[nodiscard]] auto IsNeedMainTexBuf() const -> bool { return _needMainTexBuf; }
    [[nodiscard]] auto IsNeedContourBuf() const -> bool { return _needContourBuf; }
    [[nodiscard]] auto IsNeedTimeBuf() const -> bool { return _needTimeBuf; }
    [[nodiscard]] auto IsNeedRandomValueBuf() const -> bool { return _needRandomValueBuf; }
    [[nodiscard]] auto IsNeedScriptValueBuf() const -> bool { return _needScriptValueBuf; }
#if FO_ENABLE_3D
    [[nodiscard]] auto IsNeedModelBuf() const -> bool { return _needModelBuf; }
    [[nodiscard]] auto IsNeedAnyModelTex() const -> bool { return _needAnyModelTex; }
    [[nodiscard]] auto IsNeedModelTex(size_t index) const -> bool { return _needModelTex[index]; }
    [[nodiscard]] auto IsNeedModelTexBuf() const -> bool { return _needModelTexBuf; }
    [[nodiscard]] auto IsNeedModelAnimBuf() const -> bool { return _needModelAnimBuf; }
#endif

    [[nodiscard]] auto CanBatch(const RenderEffect* other) const -> bool;

    // Input data
    raw_ptr<const RenderTexture> MainTex {};
    raw_ptr<const RenderTexture> EggTex {};
    bool DisableBlending {};
#if FO_ENABLE_3D
    raw_ptr<RenderTexture> ModelTex[MODEL_MAX_TEXTURES] {};
    bool DisableShadow {};
    bool DisableCulling {};
    size_t MatrixCount {};
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

    virtual void DrawBuffer(RenderDrawBuffer* dbuf, size_t start_index = 0, optional<size_t> indices_to_draw = std::nullopt, const RenderTexture* custom_tex = nullptr) = 0;

protected:
    RenderEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader);

    string _name;
    EffectUsage _usage;

    bool _needMainTex {};
    bool _needEggTex {};
    bool _needProjBuf {};
    bool _needMainTexBuf {};
    bool _needContourBuf {};
    bool _needTimeBuf {};
    bool _needRandomValueBuf {};
    bool _needScriptValueBuf {};
#if FO_ENABLE_3D
    bool _needModelBuf {};
    bool _needAnyModelTex {};
    bool _needModelTex[MODEL_MAX_TEXTURES] {};
    bool _needModelTexBuf {};
    bool _needModelAnimBuf {};
#endif

    size_t _passCount {};
    BlendFuncType _srcBlendFunc[EFFECT_MAX_PASSES] {};
    BlendFuncType _destBlendFunc[EFFECT_MAX_PASSES] {};
    BlendEquationType _blendEquation[EFFECT_MAX_PASSES] {};
    bool _depthWrite[EFFECT_MAX_PASSES] {};
#if FO_ENABLE_3D
    bool _isShadow[EFFECT_MAX_PASSES] {};
#endif

    int32 _posMainTex[EFFECT_MAX_PASSES] {};
    int32 _posEggTex[EFFECT_MAX_PASSES] {};
    int32 _posProjBuf[EFFECT_MAX_PASSES] {};
    int32 _posMainTexBuf[EFFECT_MAX_PASSES] {};
    int32 _posContourBuf[EFFECT_MAX_PASSES] {};
    int32 _posTimeBuf[EFFECT_MAX_PASSES] {};
    int32 _posRandomValueBuf[EFFECT_MAX_PASSES] {};
    int32 _posScriptValueBuf[EFFECT_MAX_PASSES] {};
#if FO_ENABLE_3D
    int32 _posModelBuf[EFFECT_MAX_PASSES] {};
    int32 _posModelTex[EFFECT_MAX_PASSES][MODEL_MAX_TEXTURES] {};
    int32 _posModelTexBuf[EFFECT_MAX_PASSES] {};
    int32 _posModelAnimBuf[EFFECT_MAX_PASSES] {};
#endif
};

class Renderer
{
public:
    Renderer() = default;
    Renderer(const Renderer&) = delete;
    Renderer(Renderer&&) noexcept = delete;
    auto operator=(const Renderer&) -> Renderer& = delete;
    auto operator=(Renderer&&) noexcept -> Renderer& = delete;
    virtual ~Renderer() = default;

    [[nodiscard]] virtual auto CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture> = 0;
    [[nodiscard]] virtual auto CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer> = 0;
    [[nodiscard]] virtual auto CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect> = 0;
    [[nodiscard]] virtual auto CreateOrthoMatrix(float32 left, float32 right, float32 bottom, float32 top, float32 nearp, float32 farp) -> mat44 = 0;
    [[nodiscard]] virtual auto GetViewPort() -> irect32 = 0;
    [[nodiscard]] virtual auto IsRenderTargetFlipped() -> bool = 0;

    virtual void Init(GlobalSettings& settings, WindowInternalHandle* window) = 0;
    virtual void Present() = 0;
    virtual void SetRenderTarget(RenderTexture* tex) = 0;
    virtual void ClearRenderTarget(optional<ucolor> color, bool depth = false, bool stencil = false) = 0;
    virtual void EnableScissor(irect32 rect) = 0;
    virtual void DisableScissor() = 0;
    virtual void OnResizeWindow(isize32 size) = 0;
};

class Null_Renderer final : public Renderer
{
public:
    [[nodiscard]] auto CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture> override;
    [[nodiscard]] auto CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer> override;
    [[nodiscard]] auto CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect> override;
    [[nodiscard]] auto CreateOrthoMatrix(float32 left, float32 right, float32 bottom, float32 top, float32 nearp, float32 farp) -> mat44 override;
    [[nodiscard]] auto GetViewPort() -> irect32 override;
    [[nodiscard]] auto IsRenderTargetFlipped() -> bool override;

    void Init(GlobalSettings& settings, WindowInternalHandle* window) override;
    void Present() override;
    void SetRenderTarget(RenderTexture* tex) override;
    void ClearRenderTarget(optional<ucolor> color, bool depth = false, bool stencil = false) override;
    void EnableScissor(irect32 rect) override;
    void DisableScissor() override;
    void OnResizeWindow(isize32 size) override;
};

#if FO_HAVE_OPENGL

class OpenGL_Renderer final : public Renderer
{
public:
    static constexpr auto RING_BUFFER_LENGTH = 300;

    [[nodiscard]] auto CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture> override;
    [[nodiscard]] auto CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer> override;
    [[nodiscard]] auto CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect> override;
    [[nodiscard]] auto CreateOrthoMatrix(float32 left, float32 right, float32 bottom, float32 top, float32 nearp, float32 farp) -> mat44 override;
    [[nodiscard]] auto GetViewPort() -> irect32 override;
    [[nodiscard]] auto IsRenderTargetFlipped() -> bool override { return true; }

    void Init(GlobalSettings& settings, WindowInternalHandle* window) override;
    void Present() override;
    void SetRenderTarget(RenderTexture* tex) override;
    void ClearRenderTarget(optional<ucolor> color, bool depth = false, bool stencil = false) override;
    void EnableScissor(irect32 rect) override;
    void DisableScissor() override;
    void OnResizeWindow(isize32 size) override;
};

#endif

#if FO_HAVE_DIRECT_3D

class Direct3D_Renderer final : public Renderer
{
public:
    [[nodiscard]] auto CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture> override;
    [[nodiscard]] auto CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer> override;
    [[nodiscard]] auto CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect> override;
    [[nodiscard]] auto CreateOrthoMatrix(float32 left, float32 right, float32 bottom, float32 top, float32 nearp, float32 farp) -> mat44 override;
    [[nodiscard]] auto GetViewPort() -> irect32 override;
    [[nodiscard]] auto IsRenderTargetFlipped() -> bool override { return false; }

    void Init(GlobalSettings& settings, WindowInternalHandle* window) override;
    void Present() override;
    void SetRenderTarget(RenderTexture* tex) override;
    void ClearRenderTarget(optional<ucolor> color, bool depth = false, bool stencil = false) override;
    void EnableScissor(irect32 rect) override;
    void DisableScissor() override;
    void OnResizeWindow(isize32 size) override;
};

#endif

FO_END_NAMESPACE();
