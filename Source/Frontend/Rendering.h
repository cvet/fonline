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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(EffectLoadException);
FO_DECLARE_EXCEPTION(RenderingException);

using WindowInternalHandle = void;

constexpr size_t EFFECT_MAX_PASSES = FO_EFFECT_MAX_PASSES;
constexpr size_t EFFECT_SCRIPT_VALUES = FO_EFFECT_SCRIPT_VALUES;
constexpr float32_t ORTHO_DEPTH_DEFAULT_NEAR = -10.0f;
constexpr float32_t ORTHO_DEPTH_DEFAULT_FAR = 10.0f;

#if FO_ENABLE_3D
constexpr size_t MODEL_LAYERS_COUNT = FO_MODEL_LAYERS_COUNT;
constexpr size_t MODEL_MAX_TEXTURES = FO_MODEL_MAX_TEXTURES;
constexpr size_t MODEL_MAX_BONES = FO_MODEL_MAX_BONES;
constexpr size_t MODEL_BONES_PER_VERTEX = FO_MODEL_BONES_PER_VERTEX;
#endif

#if FO_RENDER_32BIT_INDEX
using vindex_t = uint32_t;
#else
using vindex_t = uint16_t;
#endif

using RenderEffectLoader = function<string(string_view)>;

enum class RenderType : uint8_t
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
#if FO_HAVE_SDL_GPU
    SDLGpu,
#endif
};

enum class EffectUsage : uint8_t
{
    ImGui,
    QuadSprite,
    Primitive,
#if FO_ENABLE_3D
    Model,
#endif
};

///@ ExportEnum
enum class RenderPrimitiveType : uint8_t
{
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
};

enum class BlendFuncType : uint8_t
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

enum class BlendEquationType : uint8_t
{
    FuncAdd,
    FuncSubtract,
    FuncReverseSubtract,
    Max,
    Min,
};

enum class DepthFuncType : uint8_t
{
    Always, // Default (zero-init): write depth, never reject — opaque geometry
    Never,
    Less,
    LessEqual,
    Equal,
    GreaterEqual,
    Greater,
    NotEqual,
};

struct Vertex2D
{
    float32_t PosX {};
    float32_t PosY {};
    float32_t PosZ {};
    ucolor Color {};
    float32_t TexU {};
    float32_t TexV {};
    float32_t EggFlags[2] {};
};
static_assert(std::is_standard_layout_v<Vertex2D>);
static_assert(sizeof(Vertex2D) == 32);

#if FO_ENABLE_3D
struct Vertex3D
{
    vec3 Position {};
    vec3 Normal {};
    float32_t TexCoord[2] {};
    float32_t TexCoordBase[2] {};
    vec3 Tangent {};
    vec3 Bitangent {};
    float32_t BlendWeights[MODEL_BONES_PER_VERTEX] {};
    float32_t BlendIndices[MODEL_BONES_PER_VERTEX] {};
    ucolor Color {};
};
static_assert(std::is_standard_layout_v<Vertex3D>);
static_assert(sizeof(Vertex3D) == 68 + 8 * MODEL_BONES_PER_VERTEX);
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

    virtual void UpdateTextureRegion(ipos32 pos, isize32 size, const_span<ucolor> data, bool use_dest_pitch = false) = 0;

    const isize32 Size;
    const float32_t SizeData[4]; // Width, Height, TexelWidth, TexelHeight
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
    explicit RenderDrawBuffer(bool is_static) noexcept;
};

class RenderEffect
{
public:
    // Uniform-buffer payloads consumed by the per-effect GLSL/HLSL/MSL passes.
    // All members are float32 in std140 layout (vec4-aligned). For each buffer
    // the comment lists the corresponding GLSL declaration and which channel
    // holds what; producers cited in parentheses.

    // GLSL: uniform ProjBuf { mat4 ProjMatrix; };
    // Column-major 4x4 view-proj matrix.
    // (Renderer::ProjMatrix for 2D, ModelInstance frame/direct projections for 3D.)
    struct ProjBuffer
    {
        float32_t ProjMatrix[16] {};
    };

    // GLSL: uniform MainTexBuf { vec4 MainTexSize; };
    //   .x = width px, .y = height px, .z = 1/width, .w = 1/height.
    // (RenderTexture::SizeData, copied per-draw.)
    struct MainTexBuffer
    {
        float32_t MainTexSize[4] {};
    };

    // GLSL: uniform EggBuf { vec4 EggData[3]; };
    // Two transparent-egg slots + global parameters:
    //   EggData[0] = slot 0: .xy = center - drawOffset (px), .zw = radius (px, 0 = inactive)
    //   EggData[1] = slot 1: same layout
    //   EggData[2].x = EggTransparencyTransitionFactor (0..0.9999)
    //   EggData[2].yzw reserved.
    // (SpriteManager::DrawWithEgg.)
    struct EggBuffer
    {
        float32_t EggData[12] {};
    };

    // GLSL: uniform SpriteBorderBuf { vec4 SpriteBorder; };
    //   .x = left, .y = top, .z = right, .w = bottom (atlas UV rect of the sprite).
    // (SpriteManager fills from the current draw item's atlas rect.)
    struct SpriteBorderBuffer
    {
        float32_t SpriteBorder[4] {};
    };

    // GLSL: uniform TimeBuf { vec4 FrameTime; vec4 GameTime; };
    //   FrameTime.x = real-frame time in seconds since the first rendered frame, wrapped at 8192 s.
    //   GameTime.x  = game-frame time in seconds, same wrap (same source today, may diverge).
    //   Both .yzw are reserved (zero).
    // The values are periodic animation phases: fp32 shader math degrades at large magnitudes, so
    // consumers must tolerate the once-per-period pop and wrap unbounded phase math locally.
    // (EffectManager::PerFrameEffectUpdate.)
    struct TimeBuffer
    {
        float32_t FrameTime[4] {};
        float32_t GameTime[4] {};
    };

    // GLSL: uniform RandomValueBuf { vec4 RandomValue; };
    //   .xyzw = four independent random floats in [0,1], refreshed each frame.
    // (EffectManager::PerFrameEffectUpdate.)
    struct RandomValueBuffer
    {
        float32_t RandomValue[4] {};
    };

    // GLSL: uniform ScriptValueBuf { vec4 ScriptValue[EFFECT_SCRIPT_VALUES / 4]; };
    // Flat float pool indexed by ExportMethod Game.SetEffectScriptValue(idx, value).
    // Slot meanings are project-defined; see Scripts/EffectSlots.fos in this game.
    struct ScriptValueBuffer
    {
        float32_t ScriptValue[EFFECT_SCRIPT_VALUES] {};
    };

    // GLSL: uniform CameraBuf { vec4 MapAnchorScreenPos; vec4 ChunkScreenAnchor; };
    // Both fields are an **affine basis** for converting the shader's `TexCoord` into a
    // semantic UV, evaluated as `uv = .xy + TexCoord * .zw`. The scale `.zw` is needed
    // because `_rtMap` is allocated larger than `_screenSize` (one hex of padding so
    // sprites that overlap screen edges still render fully), and the FlushMap blit's
    // `source_rect` samples only the screen-sized content region, so `TexCoord` does not
    // reach 1.0 at the chunk's right edge — it tops out at `~screen_w/rt_w` (~0.95 with
    // default hex padding). A `TexCoord - anchor` form would therefore leave a hex-sized
    // gap between adjacent chunks' world UV at the seam; the affine form cancels both the
    // rt-vs-screen scale mismatch and the sub-pixel `source_x = fmod(scrollOffset, 1.0)`
    // offset, so adjacent chunks land on identical UV at the boundary (no seam).
    //   MapAnchorScreenPos = world-anchored basis.  .xy uses the **pre-zoom** anchor
    //                        (`anchor_hex - scrollOffset`, no zoom multiply), which makes
    //                        the resulting `world_uv` **zoom-invariant**: the same world
    //                        position produces the same UV at every zoom, so weather noise
    //                        stays pinned to the world during zoom in/out instead of
    //                        drifting (older screen-anchor form scaled with zoom and
    //                        visibly slid the noise pattern on zoom). .zw = rt/screen
    //                        size ratio. Used by snow/rain/fog/dust noise.
    //   ChunkScreenAnchor  = screen-anchored basis. `screen_uv` stays continuous across
    //                        the **full screen** regardless of chunking — required for
    //                        effects pinned to the screen frame (vignette, sun bleach,
    //                        film grain, heat warp). .xy + .zw = (step_xy + 1) * draw_scale.
    // Populated by MapView::DrawMap right before the FlushMap pass so weather/dust/rain
    // shaders can sample world-anchored coords without per-frame scripts.
    struct CameraBuffer
    {
        float32_t MapAnchorScreenPos[4] {};
        float32_t ChunkScreenAnchor[4] {};
    };

#if FO_ENABLE_3D
    // GLSL: uniform ModelBuf { vec4 LightColor; vec4 GroundPosition; mat4 WorldMatrices[MAX_BONES]; };
    //   LightColor       = RGBA tint of the current map light.
    //   GroundPosition.xyz = world-space ground position; .w reserved (0).
    //   WorldMatrices    = per-bone skinning matrices, count == effect->MatrixCount.
    // (3dStuff::Combined3dMeshDraw.)
    struct ModelBuffer
    {
        float32_t LightColor[4] {};
        float32_t GroundPosition[4] {};
        float32_t WorldMatrices[16 * MODEL_MAX_BONES] {};
    };

    // GLSL: uniform ModelTexBuf { vec4 TexAtlasOffset[MAX_TEXTURES]; vec4 TexSize[MAX_TEXTURES]; };
    //   TexAtlasOffset[i] = .xy atlas UV origin, .zw atlas UV scale of texture i.
    //   TexSize[i]        = .xy width/height px, .zw 1/width 1/height (matches MainTexBuf format).
    // Used by 3D shaders that sample multiple model textures (normal map etc.).
    struct ModelTexBuffer
    {
        float32_t TexAtlasOffset[4 * MODEL_MAX_TEXTURES] {};
        float32_t TexSize[4 * MODEL_MAX_TEXTURES] {};
    };

    // GLSL: uniform ModelAnimBuf { vec4 AnimNormalizedTime; vec4 AnimAbsoluteTime; };
    //   AnimNormalizedTime.x = current anim progress in [0,1].
    //   AnimAbsoluteTime.x   = absolute anim time in seconds (looped).
    //   Both .yzw reserved.
    // (3dStuff sets these when an effect declares ModelAnimBuf.)
    struct ModelAnimBuffer
    {
        float32_t AnimNormalizedTime[4] {};
        float32_t AnimAbsoluteTime[4] {};
    };
#endif

    static_assert(sizeof(ProjBuffer) % 16 == 0 && sizeof(ProjBuffer) == 64);
    static_assert(sizeof(MainTexBuffer) % 16 == 0 && sizeof(MainTexBuffer) == 16);
    static_assert(sizeof(EggBuffer) % 16 == 0 && sizeof(EggBuffer) == 48);
    static_assert(sizeof(SpriteBorderBuffer) % 16 == 0 && sizeof(SpriteBorderBuffer) == 16);
    static_assert(sizeof(TimeBuffer) % 16 == 0 && sizeof(TimeBuffer) == 32);
    static_assert(sizeof(RandomValueBuffer) % 16 == 0 && sizeof(RandomValueBuffer) == 16);
    static_assert(sizeof(ScriptValueBuffer) % 16 == 0 && sizeof(ScriptValueBuffer) == EFFECT_SCRIPT_VALUES * sizeof(float32_t));
    static_assert(sizeof(CameraBuffer) % 16 == 0 && sizeof(CameraBuffer) == 32);
#if FO_ENABLE_3D
    static_assert(sizeof(ModelBuffer) % 16 == 0 && sizeof(ModelBuffer) == 32 + 64 * MODEL_MAX_BONES);
    static_assert(sizeof(ModelTexBuffer) % 16 == 0 && sizeof(ModelTexBuffer) == 32 * MODEL_MAX_TEXTURES);
    static_assert(sizeof(ModelAnimBuffer) % 16 == 0 && sizeof(ModelAnimBuffer) == 32);
#endif

    RenderEffect(const RenderEffect&) = delete;
    RenderEffect(RenderEffect&&) noexcept = delete;
    auto operator=(const RenderEffect&) = delete;
    auto operator=(RenderEffect&&) noexcept = delete;
    virtual ~RenderEffect() = default;

    [[nodiscard]] auto GetName() const -> string_view { return _name; }
    [[nodiscard]] auto GetUsage() const -> EffectUsage { return _usage; }
    [[nodiscard]] auto GetPassCount() const -> size_t { return _passCount; }

    [[nodiscard]] auto IsNeedMainTex() const -> bool { return _needMainTex; }
    [[nodiscard]] auto IsNeedIndoorMaskTex() const -> bool { return _needIndoorMaskTex; }
    [[nodiscard]] auto IsNeedProjBuf() const -> bool { return _needProjBuf; }
    [[nodiscard]] auto IsNeedMainTexBuf() const -> bool { return _needMainTexBuf; }
    [[nodiscard]] auto IsNeedEggBuf() const -> bool { return _needEggBuf; }
    [[nodiscard]] auto IsNeedSpriteBorderBuf() const -> bool { return _needSpriteBorderBuf; }
    [[nodiscard]] auto IsNeedTimeBuf() const -> bool { return _needTimeBuf; }
    [[nodiscard]] auto IsNeedRandomValueBuf() const -> bool { return _needRandomValueBuf; }
    [[nodiscard]] auto IsNeedScriptValueBuf() const -> bool { return _needScriptValueBuf; }
    [[nodiscard]] auto IsNeedCameraBuf() const -> bool { return _needCameraBuf; }
#if FO_ENABLE_3D
    [[nodiscard]] auto IsNeedModelBuf() const -> bool { return _needModelBuf; }
    [[nodiscard]] auto IsNeedAnyModelTex() const -> bool { return _needAnyModelTex; }
    [[nodiscard]] auto IsNeedModelTex(size_t index) const -> bool { return _needModelTex[index]; }
    [[nodiscard]] auto IsNeedModelTexBuf() const -> bool { return _needModelTexBuf; }
    [[nodiscard]] auto IsNeedModelAnimBuf() const -> bool { return _needModelAnimBuf; }
#endif

    [[nodiscard]] auto CanBatch(ptr<const RenderEffect> other) const -> bool;

    // Input data
    nptr<const RenderTexture> MainTex {};
    nptr<const RenderTexture> IndoorMaskTex {};
    bool DisableBlending {};
#if FO_ENABLE_3D
    nptr<RenderTexture> ModelTex[MODEL_MAX_TEXTURES] {};
    bool DisableShadow {};
    bool DisableCulling {};
    size_t MatrixCount {};
#endif

    optional<ProjBuffer> ProjBuf {};
    optional<MainTexBuffer> MainTexBuf {};
    optional<EggBuffer> EggBuf {};
    optional<SpriteBorderBuffer> SpriteBorderBuf {};
    optional<TimeBuffer> TimeBuf {};
    optional<RandomValueBuffer> RandomValueBuf {};
    optional<ScriptValueBuffer> ScriptValueBuf {};
    optional<CameraBuffer> CameraBuf {};
#if FO_ENABLE_3D
    optional<ModelBuffer> ModelBuf {};
    optional<ModelTexBuffer> ModelTexBuf {};
    optional<ModelAnimBuffer> ModelAnimBuf {};
#endif

    virtual void DrawBuffer(ptr<RenderDrawBuffer> dbuf, size_t start_index = 0, optional<size_t> indices_to_draw = std::nullopt, nptr<const RenderTexture> custom_tex = nullptr) = 0;

protected:
    RenderEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader);

    string _name;
    EffectUsage _usage;

    bool _needMainTex {};
    bool _needIndoorMaskTex {};
    bool _needEggBuf {};
    bool _needProjBuf {};
    bool _needMainTexBuf {};
    bool _needSpriteBorderBuf {};
    bool _needTimeBuf {};
    bool _needRandomValueBuf {};
    bool _needScriptValueBuf {};
    bool _needCameraBuf {};
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
    DepthFuncType _depthFunc[EFFECT_MAX_PASSES] {};
#if FO_ENABLE_3D
    bool _isShadow[EFFECT_MAX_PASSES] {};
#endif

    int32_t _posMainTex[EFFECT_MAX_PASSES] {};
    int32_t _posIndoorMaskTex[EFFECT_MAX_PASSES] {};
    int32_t _posEggBuf[EFFECT_MAX_PASSES] {};
    int32_t _posProjBuf[EFFECT_MAX_PASSES] {};
    int32_t _posMainTexBuf[EFFECT_MAX_PASSES] {};
    int32_t _posSpriteBorderBuf[EFFECT_MAX_PASSES] {};
    int32_t _posTimeBuf[EFFECT_MAX_PASSES] {};
    int32_t _posRandomValueBuf[EFFECT_MAX_PASSES] {};
    int32_t _posScriptValueBuf[EFFECT_MAX_PASSES] {};
    int32_t _posCameraBuf[EFFECT_MAX_PASSES] {};
#if FO_ENABLE_3D
    int32_t _posModelBuf[EFFECT_MAX_PASSES] {};
    int32_t _posModelTex[EFFECT_MAX_PASSES][MODEL_MAX_TEXTURES] {};
    int32_t _posModelTexBuf[EFFECT_MAX_PASSES] {};
    int32_t _posModelAnimBuf[EFFECT_MAX_PASSES] {};
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
    [[nodiscard]] virtual auto CreateOrthoMatrix(float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t nearp, float32_t farp) const -> mat44 = 0;
    [[nodiscard]] virtual auto GetProjMatrix() const -> mat44 = 0;
    [[nodiscard]] virtual auto GetViewPort() const -> irect32 = 0;
    [[nodiscard]] virtual auto IsRenderTargetFlipped() const -> bool = 0;

    virtual void Init(GlobalSettings& settings, nptr<WindowInternalHandle> window) = 0;
    virtual void Present() = 0;
    virtual void SetRenderTarget(nptr<RenderTexture> tex) = 0;
    virtual void SetOrthoDepthRange(float32_t nearp, float32_t farp) noexcept = 0;
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
    [[nodiscard]] auto CreateOrthoMatrix(float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t nearp, float32_t farp) const -> mat44 override;
    [[nodiscard]] auto GetViewPort() const -> irect32 override;
    [[nodiscard]] auto IsRenderTargetFlipped() const -> bool override;
    [[nodiscard]] auto GetProjMatrix() const -> mat44 override { return mat44 {1.0f}; }

    void Init(GlobalSettings& settings, nptr<WindowInternalHandle> window) override;
    void Present() override;
    void SetRenderTarget(nptr<RenderTexture> tex) override;
    void SetOrthoDepthRange(float32_t /*nearp*/, float32_t /*farp*/) noexcept override { }
    void ClearRenderTarget(optional<ucolor> color, bool depth = false, bool stencil = false) override;
    void EnableScissor(irect32 rect) override;
    void DisableScissor() override;
    void OnResizeWindow(isize32 size) override;

private:
    irect32 _viewPortRect {};
    nptr<RenderTexture> _currentRenderTarget {};
    bool _scissorEnabled {};
    irect32 _scissorRect {};
};

#if FO_HAVE_OPENGL

class OpenGL_Renderer final : public Renderer
{
public:
    struct Context;

    static constexpr auto RING_BUFFER_LENGTH = 300;
    OpenGL_Renderer();
    ~OpenGL_Renderer() override;

    [[nodiscard]] auto CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture> override;
    [[nodiscard]] auto CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer> override;
    [[nodiscard]] auto CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect> override;
    [[nodiscard]] auto CreateOrthoMatrix(float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t nearp, float32_t farp) const -> mat44 override;
    [[nodiscard]] auto GetViewPort() const -> irect32 override;
    [[nodiscard]] auto IsRenderTargetFlipped() const -> bool override { return true; }
    [[nodiscard]] auto GetProjMatrix() const -> mat44 override;

    void Init(GlobalSettings& settings, nptr<WindowInternalHandle> window) override;
    void Present() override;
    void SetRenderTarget(nptr<RenderTexture> tex) override;
    void SetOrthoDepthRange(float32_t nearp, float32_t farp) noexcept override;
    void ClearRenderTarget(optional<ucolor> color, bool depth = false, bool stencil = false) override;
    void EnableScissor(irect32 rect) override;
    void DisableScissor() override;
    void OnResizeWindow(isize32 size) override;

private:
    unique_nptr<Context> _ctx {};
};

#endif

#if FO_HAVE_DIRECT_3D

class Direct3D_Renderer final : public Renderer
{
public:
    struct Context;

    Direct3D_Renderer();
    ~Direct3D_Renderer() override;

    [[nodiscard]] auto CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture> override;
    [[nodiscard]] auto CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer> override;
    [[nodiscard]] auto CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect> override;
    [[nodiscard]] auto CreateOrthoMatrix(float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t nearp, float32_t farp) const -> mat44 override;
    [[nodiscard]] auto GetViewPort() const -> irect32 override;
    [[nodiscard]] auto IsRenderTargetFlipped() const -> bool override { return false; }
    [[nodiscard]] auto GetProjMatrix() const -> mat44 override;

    void Init(GlobalSettings& settings, nptr<WindowInternalHandle> window) override;
    void Present() override;
    void SetRenderTarget(nptr<RenderTexture> tex) override;
    void SetOrthoDepthRange(float32_t nearp, float32_t farp) noexcept override;
    void ClearRenderTarget(optional<ucolor> color, bool depth = false, bool stencil = false) override;
    void EnableScissor(irect32 rect) override;
    void DisableScissor() override;
    void OnResizeWindow(isize32 size) override;

private:
    unique_nptr<Context> _ctx {};
};

#endif

#if FO_HAVE_VULKAN

class Vulkan_Renderer final : public Renderer
{
public:
    struct Context;

    Vulkan_Renderer();
    ~Vulkan_Renderer() override;

    [[nodiscard]] auto CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture> override;
    [[nodiscard]] auto CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer> override;
    [[nodiscard]] auto CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect> override;
    [[nodiscard]] auto CreateOrthoMatrix(float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t nearp, float32_t farp) const -> mat44 override;
    [[nodiscard]] auto GetViewPort() const -> irect32 override;
    [[nodiscard]] auto IsRenderTargetFlipped() const -> bool override { return false; }
    [[nodiscard]] auto GetProjMatrix() const -> mat44 override;

    void Init(GlobalSettings& settings, nptr<WindowInternalHandle> window) override;
    void Present() override;
    void SetRenderTarget(nptr<RenderTexture> tex) override;
    void SetOrthoDepthRange(float32_t nearp, float32_t farp) noexcept override;
    void ClearRenderTarget(optional<ucolor> color, bool depth = false, bool stencil = false) override;
    void EnableScissor(irect32 rect) override;
    void DisableScissor() override;
    void OnResizeWindow(isize32 size) override;

private:
    unique_nptr<Context> _ctx {};
};

#endif

#if FO_HAVE_SDL_GPU

class SDLGpu_Renderer final : public Renderer
{
public:
    struct Context;

    SDLGpu_Renderer();
    ~SDLGpu_Renderer() override;

    [[nodiscard]] auto CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture> override;
    [[nodiscard]] auto CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer> override;
    [[nodiscard]] auto CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect> override;
    [[nodiscard]] auto CreateOrthoMatrix(float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t nearp, float32_t farp) const -> mat44 override;
    [[nodiscard]] auto GetViewPort() const -> irect32 override;
    [[nodiscard]] auto IsRenderTargetFlipped() const -> bool override { return false; }
    [[nodiscard]] auto GetProjMatrix() const -> mat44 override;

    void Init(GlobalSettings& settings, nptr<WindowInternalHandle> window) override;
    void Present() override;
    void SetRenderTarget(nptr<RenderTexture> tex) override;
    void SetOrthoDepthRange(float32_t nearp, float32_t farp) noexcept override;
    void ClearRenderTarget(optional<ucolor> color, bool depth = false, bool stencil = false) override;
    void EnableScissor(irect32 rect) override;
    void DisableScissor() override;
    void OnResizeWindow(isize32 size) override;

private:
    unique_nptr<Context> _ctx {};
};

#endif

FO_END_NAMESPACE
