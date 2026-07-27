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

#include "EffekseerExtension.h"

#if FO_EFFEKSEER_PARTICLES

#include "Application.h"
#include "EffectManager.h"
#include "FileSystem.h"

FO_DISABLE_WARNINGS_PUSH()
#include "Effekseer.h"
FO_DISABLE_WARNINGS_POP()

FO_BEGIN_NAMESPACE

constexpr int32_t EFFEKSEER_INSTANCE_MAX = 16384;
constexpr size_t EFFEKSEER_SPRITE_INSTANCE_MAX = 16000;
constexpr size_t EFFEKSEER_CHUNK_VERTEX_MAX = 64000;
constexpr float32_t EFFEKSEER_FRAMES_PER_SECOND = 60.0f;
constexpr float32_t EFFEKSEER_PREWARM_SECONDS = 1.0f;

struct EffekseerRuntimeState;

static auto EffekseerMalloc(uint32_t size) -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

    return SafeAlloc::MallocRaw(size).get();
}

static void EffekseerFree(void* mem, uint32_t size)
{
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(size);
    SafeAlloc::FreeRaw(mem);
}

static auto EffekseerAlignedMalloc(uint32_t size, uint32_t alignment) -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

    return SafeAlloc::MallocAlignedRaw(size, alignment).get();
}

static void EffekseerAlignedFree(void* mem, uint32_t size)
{
    FO_NO_STACK_TRACE_ENTRY();

    // Effekseer hands back the size but not the alignment, which is why the aligned tier releases a block
    // without needing it
    ignore_unused(size);
    SafeAlloc::FreeAlignedRaw(mem);
}

void InitializeEffekseerMemory() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    static std::once_flag once;
    std::call_once(once, [] {
        Effekseer::SetMallocFunc(&EffekseerMalloc);
        Effekseer::SetFreeFunc(&EffekseerFree);
        Effekseer::SetAlignedMallocFunc(&EffekseerAlignedMalloc);
        Effekseer::SetAlignedFreeFunc(&EffekseerAlignedFree);
    });
}

static void LogEffekseerRejection(string_view path, string_view reason)
{
    FO_STACK_TRACE_ENTRY();

    WriteLog(LogType::Warning, "Effekseer particle '{}' rejected: {}", path, reason);
}

static auto ToUtf8(const char16_t* value) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (value == nullptr) {
        return {};
    }

    size_t source_length = std::char_traits<char16_t>::length(value);
    vector<char> result(source_length * 3 + 1);
    int32_t converted_length = Effekseer::ConvertUtf16ToUtf8(result.data(), numeric_cast<int32_t>(result.size()), value);
    return string(result.data(), numeric_cast<size_t>(converted_length));
}

static auto ToUtf16(string_view value) -> vector<char16_t>
{
    FO_STACK_TRACE_ENTRY();

    string source {value};
    vector<char16_t> result(source.size() + 1);
    (void)Effekseer::ConvertUtf8ToUtf16(result.data(), numeric_cast<int32_t>(result.size()), source.c_str());
    return result;
}

static auto ToEffekseerMatrix43(const mat44& matrix) -> Effekseer::Matrix43
{
    FO_STACK_TRACE_ENTRY();

    Effekseer::Matrix43 result {};
    // GLM indexes column-major matrices as [column][row], while Effekseer
    // stores the equivalent row-vector transform as [row][column]. Keeping
    // the same two indices therefore performs the intended convention swap.
    for (glm::length_t row = 0; row < 4; row++) {
        for (glm::length_t column = 0; column < 3; column++) {
            result.Value[row][column] = matrix[row][column];
        }
    }
    return result;
}

static auto ToEffekseerMatrix44(const mat44& matrix) -> Effekseer::Matrix44
{
    FO_STACK_TRACE_ENTRY();

    Effekseer::Matrix44 result {};
    // See ToEffekseerMatrix43: equal indices transpose the mathematical
    // convention because GLM's first index denotes a column.
    for (glm::length_t row = 0; row < 4; row++) {
        for (glm::length_t column = 0; column < 4; column++) {
            result.Values[row][column] = matrix[row][column];
        }
    }
    return result;
}

static auto ToVec3(const Effekseer::SIMD::Vec3f& value) -> vec3
{
    FO_STACK_TRACE_ENTRY();

    return {value.GetX(), value.GetY(), value.GetZ()};
}

static auto ToColor(const Effekseer::Color& value) -> ucolor
{
    FO_STACK_TRACE_ENTRY();

    return {value.R, value.G, value.B, value.A};
}

static auto IsFinite(const Effekseer::SIMD::Vec2f& value) -> bool
{
    FO_STACK_TRACE_ENTRY();

    return std::isfinite(value.GetX()) && std::isfinite(value.GetY());
}

static auto IsFinite(const Effekseer::SIMD::Vec3f& value) -> bool
{
    FO_STACK_TRACE_ENTRY();

    return std::isfinite(value.GetX()) && std::isfinite(value.GetY()) && std::isfinite(value.GetZ());
}

static auto IsFinite(const Effekseer::SIMD::Mat43f& value) -> bool
{
    FO_STACK_TRACE_ENTRY();

    return std::isfinite(value.X.GetX()) && std::isfinite(value.X.GetY()) && std::isfinite(value.X.GetZ()) && std::isfinite(value.X.GetW()) && std::isfinite(value.Y.GetX()) && std::isfinite(value.Y.GetY()) && std::isfinite(value.Y.GetZ()) && std::isfinite(value.Y.GetW()) && std::isfinite(value.Z.GetX()) && std::isfinite(value.Z.GetY()) && std::isfinite(value.Z.GetZ()) && std::isfinite(value.Z.GetW());
}

class FOnlineEffekseerTexture final : public Effekseer::Backend::Texture
{
public:
    FOnlineEffekseerTexture(nptr<RenderTexture> texture, frect32 atlas_rect) :
        RenderTextureRef {texture},
        AtlasRect {atlas_rect}
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(RenderTextureRef, "Effekseer texture wrapper requires a render texture");
        param_.Size = {RenderTextureRef->Size.width, RenderTextureRef->Size.height, 1};
    }

    nptr<RenderTexture> RenderTextureRef {};
    frect32 AtlasRect {};
};

class FOnlineEffekseerTextureLoader final : public Effekseer::TextureLoader
{
public:
    explicit FOnlineEffekseerTextureLoader(ParticleTextureLoader texture_loader) :
        _textureLoader {std::move(texture_loader)}
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(_textureLoader, "Effekseer runtime requires a texture loader");
    }

    auto Load(const char16_t* path, Effekseer::TextureType texture_type) -> Effekseer::TextureRef override
    {
        FO_STACK_TRACE_ENTRY();

        // A distortion map is an ordinary image in the atlas; what differs is how the shader reads it, not how it loads.
        if (texture_type != Effekseer::TextureType::Color && texture_type != Effekseer::TextureType::Distortion) {
            WriteLog(LogType::Warning, "Effekseer texture '{}' rejected: only color and distortion textures are supported", ToUtf8(path));
            return nullptr;
        }

        string texture_path = strex(ToUtf8(path)).format_path().str();
        auto [render_texture, atlas_rect] = _textureLoader(texture_path);
        if (!render_texture) {
            WriteLog(LogType::Warning, "Effekseer texture '{}' is missing", texture_path);
            return nullptr;
        }

        Effekseer::Backend::TextureRef backend_texture = Effekseer::MakeRefPtr<FOnlineEffekseerTexture>(render_texture, atlas_rect);
        Effekseer::TextureRef texture = Effekseer::MakeRefPtr<Effekseer::Texture>();
        texture->SetBackend(backend_texture);
        return texture;
    }

private:
    ParticleTextureLoader _textureLoader;
};

// Effekseer's own core parses the .efkmodel payload, so the loader only has to hand it the baked bytes. Models are
// raw-copied resources, which keeps their vertex data identical to what the Editor exported.
class FOnlineEffekseerModelLoader final : public Effekseer::ModelLoader
{
public:
    explicit FOnlineEffekseerModelLoader(ptr<FileSystem> resources) :
        _resources {resources}
    {
        FO_STACK_TRACE_ENTRY();
    }

    auto Load(const char16_t* path) -> Effekseer::ModelRef override
    {
        FO_STACK_TRACE_ENTRY();

        string model_path = strex(ToUtf8(path)).format_path().str();
        File file = _resources->ReadFile(model_path);

        if (!file) {
            WriteLog(LogType::Warning, "Effekseer model '{}' is missing", model_path);
            return nullptr;
        }

        const_span<uint8_t> data = file.GetDataSpan();

        if (data.empty() || data.size() > numeric_cast<size_t>(std::numeric_limits<int32_t>::max())) {
            WriteLog(LogType::Warning, "Effekseer model '{}' has an unusable size", model_path, data.size());
            return nullptr;
        }

        return Effekseer::MakeRefPtr<Effekseer::Model>(data.data(), numeric_cast<int32_t>(data.size()));
    }

private:
    ptr<FileSystem> _resources;
};

class DetectingGpuParticleFactory final : public Effekseer::GpuParticleFactory
{
public:
    void Reset()
    {
        FO_STACK_TRACE_ENTRY();

        _createResourceCount = 0;
    }

    [[nodiscard]] auto WasRequested() const -> bool
    {
        FO_STACK_TRACE_ENTRY();

        return _createResourceCount != 0;
    }

    auto CreateResource(const Effekseer::GpuParticles::ParamSet& parameter_set, const Effekseer::Effect* effect) -> Effekseer::GpuParticles::ResourceRef override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(parameter_set, effect);
        _createResourceCount++;
        return nullptr;
    }

private:
    size_t _createResourceCount {};
};

struct EffekseerParticleRuntimeSystem::Impl
{
    Impl(shared_ptr<EffekseerRuntimeState> runtime, Effekseer::EffectRef effect, string path, vec3 position_min, vec3 position_max, float32_t billboard_radius) :
        Runtime {std::move(runtime)},
        Effect {std::move(effect)},
        Path {std::move(path)},
        BakedPositionMin {position_min},
        BakedPositionMax {position_max},
        BakedBillboardRadius {billboard_radius}
    {
        FO_STACK_TRACE_ENTRY();
    }

    void Fail(string_view reason)
    {
        FO_STACK_TRACE_ENTRY();

        if (!Failed) {
            Failed = true;
            LogEffekseerRejection(Path, reason);
        }
    }

    shared_ptr<EffekseerRuntimeState> Runtime;
    Effekseer::EffectRef Effect;
    string Path;
    Effekseer::Handle Handle {-1};
    mat44 RootMatrix {1.0f};
    mat44 ViewProjMatrix {1.0f};
    mat44 ViewMatrix {1.0f};
    mat44 BoundsMatrix {1.0f};
    vec3 BakedPositionMin {};
    vec3 BakedPositionMax {};
    float32_t BakedBillboardRadius {};
    std::mt19937 RandomGenerator {MakeSeededRandomGenerator()};
    bool Failed {};
};

struct EffekseerDrawBinding
{
    void Bind(ptr<EffekseerParticleRuntimeSystem::Impl> system)
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(!CurrentSystem, "Effekseer renderer is already bound to a particle system");
        CurrentSystem = system;
    }

    void Unbind()
    {
        FO_STACK_TRACE_ENTRY();

        CurrentSystem = nullptr;
    }

    void Fail(string_view reason)
    {
        FO_STACK_TRACE_ENTRY();

        if (CurrentSystem) {
            CurrentSystem->Fail(reason);
        }
    }

    nptr<EffekseerParticleRuntimeSystem::Impl> CurrentSystem {};
};

struct EffekseerSpriteNodeSnapshot
{
    Effekseer::BillboardType Billboard {};
    Effekseer::ZSortType ZSort {};
    Effekseer::AlphaBlendType AlphaBlend {};
    Effekseer::TextureFilterType TextureFilter {};
    Effekseer::TextureWrapType TextureWrap {};
    int32_t TextureIndex {-1};
    // A distortion node refracts the scene behind it instead of drawing its own colour, and scales the displacement
    // its texture describes by this intensity.
    bool Distortion {};
    float32_t DistortionIntensity {};
    bool ZTest {};
    bool ZWrite {};
};

struct EffekseerSpriteInstanceSnapshot
{
    Effekseer::SIMD::Mat43f SRTMatrix43 {};
    array<Effekseer::Color, 4> Colors {};
    array<Effekseer::SIMD::Vec2f, 4> Positions {Effekseer::SIMD::Vec2f {0.0f, 0.0f}, Effekseer::SIMD::Vec2f {0.0f, 0.0f}, Effekseer::SIMD::Vec2f {0.0f, 0.0f}, Effekseer::SIMD::Vec2f {0.0f, 0.0f}};
    Effekseer::RectF UV {};
    Effekseer::SIMD::Vec3f Direction {};
    float32_t CameraDepth {};
};

struct EffekseerRingNodeSnapshot
{
    Effekseer::BillboardType Billboard {};
    Effekseer::ZSortType ZSort {};
    Effekseer::AlphaBlendType AlphaBlend {};
    Effekseer::TextureFilterType TextureFilter {};
    Effekseer::TextureWrapType TextureWrap {};
    int32_t TextureIndex {-1};
    int32_t VertexCount {};
    float32_t StartingFade {};
    float32_t EndingFade {};
    bool ZTest {};
    bool ZWrite {};
};

// What the shared strip geometry needs from a Ribbon or Track node. Both families deliver the same material and depth
// intent, so each renderer snapshots this and keeps only its own extras beside it.
struct EffekseerStripNodeSnapshot
{
    Effekseer::AlphaBlendType AlphaBlend {};
    Effekseer::TextureFilterType TextureFilter {};
    Effekseer::TextureWrapType TextureWrap {};
    int32_t TextureIndex {-1};
    bool ZTest {};
    bool ZWrite {};
};

// One instance's contribution to a strip: the three world positions across the band's width, the colours at them, and
// the texture rectangle the segment starting at this instance stretches from.
struct EffekseerStripWidthTriple
{
    vec3 LeftPosition {};
    vec3 CenterPosition {};
    vec3 RightPosition {};
    Effekseer::Color LeftColor {};
    Effekseer::Color CenterColor {};
    Effekseer::Color RightColor {};
    Effekseer::RectF UV {};
};

// How an emitter node's sampling, blend and depth intent lands on the renderer surface: sampling and blending pick the
// effect, the depth flags pick a variant of that effect's depth state, and tiling changes what the caller must feed the
// shader.
struct EffekseerNodeRenderState
{
    nptr<RenderEffect> Effect {};
    bool DisableBlending {};
    bool ClampInShader {};
    DepthVariantType DepthVariant {};
};

// The particle colour effects a node can draw through, indexed by how it blends. Every one of them maps the texture
// coordinate into the atlas sub-rectangle in the fragment shader, because the texture lives in a shared atlas: hardware
// wrapping and hardware clamping would both reach into neighbouring atlas entries. The node's wrap mode therefore only
// selects how the shader addresses the coordinate, not which effect draws it.
class EffekseerParticleEffects
{
public:
    explicit EffekseerParticleEffects(ptr<EffectManager> effect_mngr);

    // Returns nothing for a sampling or blend mode the renderer has no equivalent for, so the caller keeps failing
    // closed at the one place that can retire the handle. A distortion draw refracts the scene instead of drawing its
    // own colour, which is a different shader family with its own, narrower set of blend modes.
    [[nodiscard]] auto Resolve(Effekseer::AlphaBlendType blend, Effekseer::TextureWrapType wrap, bool z_test, bool z_write) -> optional<EffekseerNodeRenderState>;
    [[nodiscard]] auto ResolveDistortion(Effekseer::AlphaBlendType blend, Effekseer::TextureWrapType wrap, bool z_test, bool z_write) -> optional<EffekseerNodeRenderState>;

private:
    static constexpr size_t BLEND_MODES = 3; // 0 = blend, 1 = add, 2 = subtract
    static constexpr size_t DISTORTION_BLEND_MODES = 2; // 0 = blend, 1 = add

    [[nodiscard]] static auto ResolveDepthVariant(bool z_test, bool z_write) -> DepthVariantType;
    [[nodiscard]] static auto ResolveWrap(Effekseer::TextureWrapType wrap, EffekseerNodeRenderState& state) -> bool;

    nptr<RenderEffect> _effects[BLEND_MODES] {};
    nptr<RenderEffect> _distortionEffects[DISTORTION_BLEND_MODES] {};
};

EffekseerParticleEffects::EffekseerParticleEffects(ptr<EffectManager> effect_mngr)
{
    FO_STACK_TRACE_ENTRY();

    static constexpr string_view effect_names[BLEND_MODES] = {
        "Effects/Particles_ColorMulAtlas.fofx",
        "Effects/Particles_ColorAddAtlas.fofx",
        "Effects/Particles_ColorSubAtlas.fofx",
    };

    for (size_t blend = 0; blend < BLEND_MODES; blend++) {
        _effects[blend] = effect_mngr->LoadEffect(EffectUsage::QuadSprite, effect_names[blend]);
        FO_VERIFY_AND_THROW(_effects[blend], "Particle colour effect is missing", effect_names[blend]);
    }

#if FO_ENABLE_3D
    // The distortion family carries the particle's own plane per vertex, which is the model vertex layout.
    static constexpr string_view distortion_effect_names[DISTORTION_BLEND_MODES] = {
        "Effects/Particles_DistortionAtlas.fofx",
        "Effects/Particles_DistortionAddAtlas.fofx",
    };

    for (size_t blend = 0; blend < DISTORTION_BLEND_MODES; blend++) {
        _distortionEffects[blend] = effect_mngr->LoadEffect(EffectUsage::Model, distortion_effect_names[blend]);
        FO_VERIFY_AND_THROW(_distortionEffects[blend], "Particle distortion effect is missing", distortion_effect_names[blend]);
    }
#endif
}

auto EffekseerParticleEffects::ResolveDepthVariant(bool z_test, bool z_write) -> DepthVariantType
{
    FO_STACK_TRACE_ENTRY();

    if (z_test) {
        return z_write ? DepthVariantType::TestWrite : DepthVariantType::TestNoWrite;
    }

    return z_write ? DepthVariantType::NoTestWrite : DepthVariantType::NoTestNoWrite;
}

auto EffekseerParticleEffects::ResolveWrap(Effekseer::TextureWrapType wrap, EffekseerNodeRenderState& state) -> bool
{
    FO_STACK_TRACE_ENTRY();

    switch (wrap) {
    case Effekseer::TextureWrapType::Clamp:
        state.ClampInShader = true;
        return true;
    case Effekseer::TextureWrapType::Repeat:
        return true;
    default:
        return false;
    }
}

auto EffekseerParticleEffects::ResolveDistortion(Effekseer::AlphaBlendType blend, Effekseer::TextureWrapType wrap, bool z_test, bool z_write) -> optional<EffekseerNodeRenderState>
{
    FO_STACK_TRACE_ENTRY();

    EffekseerNodeRenderState state;
    size_t blend_index = 0;

    switch (blend) {
    case Effekseer::AlphaBlendType::Blend:
        break;
    case Effekseer::AlphaBlendType::Add:
        blend_index = 1;
        break;
    case Effekseer::AlphaBlendType::Opacity:
        state.DisableBlending = true;
        break;
    default:
        return std::nullopt;
    }

    if (!ResolveWrap(wrap, state)) {
        return std::nullopt;
    }
    if (!_distortionEffects[blend_index]) {
        return std::nullopt;
    }

    state.Effect = _distortionEffects[blend_index];
    state.DepthVariant = ResolveDepthVariant(z_test, z_write);

    return state;
}

auto EffekseerParticleEffects::Resolve(Effekseer::AlphaBlendType blend, Effekseer::TextureWrapType wrap, bool z_test, bool z_write) -> optional<EffekseerNodeRenderState>
{
    FO_STACK_TRACE_ENTRY();

    EffekseerNodeRenderState state;
    size_t blend_index = 0;

    switch (blend) {
    case Effekseer::AlphaBlendType::Blend:
        break;
    case Effekseer::AlphaBlendType::Add:
        blend_index = 1;
        break;
    case Effekseer::AlphaBlendType::Sub:
        blend_index = 2;
        break;
    case Effekseer::AlphaBlendType::Opacity:
        // An opaque particle is the ordinary shader with blending switched off, which is a per-draw flag already.
        state.DisableBlending = true;
        break;
    default:
        return std::nullopt;
    }

    if (!ResolveWrap(wrap, state)) {
        return std::nullopt;
    }

    state.Effect = _effects[blend_index];
    state.DepthVariant = ResolveDepthVariant(z_test, z_write);

    return state;
}

struct EffekseerRingInstanceSnapshot
{
    Effekseer::SIMD::Mat43f SRTMatrix43 {};
    Effekseer::SIMD::Vec2f OuterLocation {};
    Effekseer::SIMD::Vec2f InnerLocation {};
    float32_t ViewingAngleStart {};
    float32_t ViewingAngleEnd {};
    float32_t CenterRatio {};
    Effekseer::Color OuterColor {};
    Effekseer::Color CenterColor {};
    Effekseer::Color InnerColor {};
    Effekseer::RectF UV {};
    Effekseer::SIMD::Vec3f Direction {};
    float32_t CameraDepth {};
};

// std::stable_sort over these snapshot vectors would instantiate std::aligned_storage with the
// snapshot's extended alignment (their Effekseer SIMD members are alignas(16)) for its temporary
// buffer, which MSVC's <type_traits> rejects. Sort a lightweight index permutation by camera depth
// and materialize the reordered instances instead; the stable order keeps the particle draw order
// deterministic.
template<typename T>
static void StableSortSnapshotsByCameraDepth(vector<T>& instances, bool reverse_order)
{
    FO_STACK_TRACE_ENTRY();

    vector<size_t> draw_order(instances.size());

    for (size_t index = 0; index < draw_order.size(); index++) {
        draw_order[index] = index;
    }

    std::stable_sort(draw_order.begin(), draw_order.end(), [&instances, reverse_order](size_t left, size_t right) { //
        return reverse_order ? instances[left].CameraDepth > instances[right].CameraDepth : instances[left].CameraDepth < instances[right].CameraDepth;
    });

    vector<T> sorted_instances;
    sorted_instances.reserve(instances.size());

    for (size_t index : draw_order) {
        sorted_instances.emplace_back(instances[index]);
    }

    instances = std::move(sorted_instances);
}

static auto ValidateSpriteNodeParameter(const Effekseer::SpriteRenderer::NodeParameter& parameter) -> string_view
{
    FO_STACK_TRACE_ENTRY();

    if (parameter.EffectPointer == nullptr || parameter.BasicParameterPtr == nullptr || parameter.DepthParameterPtr == nullptr) {
        return "sprite renderer received incomplete node parameters";
    }
    if (!parameter.IsRightHand) {
        return "left-handed sprite nodes are unsupported";
    }
    if (parameter.EnableViewOffset) {
        return "view offset is unsupported";
    }
    if (parameter.Billboard != Effekseer::BillboardType::Billboard && parameter.Billboard != Effekseer::BillboardType::RotatedBillboard && parameter.Billboard != Effekseer::BillboardType::YAxisFixed && parameter.Billboard != Effekseer::BillboardType::DirectionalBillboard && parameter.Billboard != Effekseer::BillboardType::Fixed) {
        return "unknown sprite billboard mode";
    }
    if (parameter.ZSort != Effekseer::ZSortType::None && parameter.ZSort != Effekseer::ZSortType::NormalOrder && parameter.ZSort != Effekseer::ZSortType::ReverseOrder) {
        return "unknown sprite Z-sort mode";
    }

    const Effekseer::NodeRendererBasicParameter& basic = *parameter.BasicParameterPtr;
    bool distortion = basic.MaterialType == Effekseer::RendererMaterialType::BackDistortion;

    if ((basic.MaterialType != Effekseer::RendererMaterialType::Default && !distortion) || basic.MaterialRenderDataPtr != nullptr) {
        return "only the Default and distortion materials are supported";
    }
    if (distortion && !FO_ENABLE_3D) {
        return "distortion needs the model vertex layout, which this build does not have";
    }
    if (distortion && (!std::isfinite(basic.DistortionIntensity) || basic.TextureIndexes[0] < 0)) {
        return "distortion nodes need a finite intensity and a distortion texture";
    }
    if (basic.AlphaBlend == Effekseer::AlphaBlendType::Mul) {
        return "multiply blending is unsupported";
    }
    if (basic.TextureIndexes[0] < -1) {
        return "sprite node has an invalid color texture index";
    }
    if (basic.TextureFilters[0] != Effekseer::TextureFilterType::Nearest && basic.TextureFilters[0] != Effekseer::TextureFilterType::Linear) {
        return "sprite node uses an unknown texture filter";
    }
    if (basic.TextureWraps[0] != Effekseer::TextureWrapType::Clamp && basic.TextureWraps[0] != Effekseer::TextureWrapType::Repeat) {
        return "mirrored texture wrapping is unsupported";
    }
    for (size_t texture_slot = 1; texture_slot < basic.TextureIndexes.size(); texture_slot++) {
        if (basic.TextureIndexes[texture_slot] >= 0) {
            return "advanced texture slots are unsupported";
        }
    }
    if (basic.GetIsRenderedWithAdvancedRenderer() || basic.TextureBlendType != -1 || basic.EmissiveScaling != 1.0f || basic.SoftParticleDistanceFar != 0.0f || basic.SoftParticleDistanceNear != 0.0f || basic.SoftParticleDistanceNearOffset != 0.0f) {
        return "advanced material parameters are unsupported";
    }
    if (parameter.DepthParameterPtr->DepthOffset != 0.0f || parameter.DepthParameterPtr->IsDepthOffsetScaledWithCamera || parameter.DepthParameterPtr->IsDepthOffsetScaledWithParticleScale || parameter.DepthParameterPtr->SuppressionOfScalingByDepth != 1.0f || parameter.DepthParameterPtr->DepthClipping != std::numeric_limits<float32_t>::max()) {
        return "advanced depth parameters are unsupported";
    }

    return {};
}

static auto ValidateRingNodeParameter(const Effekseer::RingRenderer::NodeParameter& parameter) -> string_view
{
    FO_STACK_TRACE_ENTRY();

    if (parameter.EffectPointer == nullptr || parameter.BasicParameterPtr == nullptr || parameter.DepthParameterPtr == nullptr) {
        return "ring renderer received incomplete node parameters";
    }
    if (!parameter.IsRightHand) {
        return "left-handed ring nodes are unsupported";
    }
    if (parameter.EnableViewOffset) {
        return "view offset is unsupported";
    }
    if (parameter.Billboard != Effekseer::BillboardType::Billboard && parameter.Billboard != Effekseer::BillboardType::RotatedBillboard && parameter.Billboard != Effekseer::BillboardType::YAxisFixed && parameter.Billboard != Effekseer::BillboardType::DirectionalBillboard && parameter.Billboard != Effekseer::BillboardType::Fixed) {
        return "unknown ring billboard mode";
    }
    if (parameter.DepthParameterPtr->ZSort != Effekseer::ZSortType::None && parameter.DepthParameterPtr->ZSort != Effekseer::ZSortType::NormalOrder && parameter.DepthParameterPtr->ZSort != Effekseer::ZSortType::ReverseOrder) {
        return "unknown ring Z-sort mode";
    }
    if (parameter.VertexCount <= 0 || parameter.VertexCount > numeric_cast<int32_t>(EFFEKSEER_CHUNK_VERTEX_MAX / 8)) {
        return "ring vertex count exceeds the supported geometry budget";
    }
    if (!std::isfinite(parameter.StartingFade) || !std::isfinite(parameter.EndingFade)) {
        return "ring fade angles must be finite";
    }

    const Effekseer::NodeRendererBasicParameter& basic = *parameter.BasicParameterPtr;

    if (basic.MaterialType != Effekseer::RendererMaterialType::Default || basic.MaterialRenderDataPtr != nullptr) {
        return "only the Default material is supported";
    }
    if (basic.AlphaBlend == Effekseer::AlphaBlendType::Mul) {
        return "multiply blending is unsupported";
    }
    if (basic.TextureIndexes[0] < -1) {
        return "ring node has an invalid color texture index";
    }
    if (basic.TextureIndexes[0] >= 0 && basic.TextureFilters[0] != Effekseer::TextureFilterType::Nearest && basic.TextureFilters[0] != Effekseer::TextureFilterType::Linear) {
        return "ring node uses an unknown texture filter";
    }
    if (basic.TextureIndexes[0] >= 0 && basic.TextureWraps[0] != Effekseer::TextureWrapType::Clamp && basic.TextureWraps[0] != Effekseer::TextureWrapType::Repeat) {
        return "mirrored texture wrapping is unsupported";
    }
    for (size_t texture_slot = 1; texture_slot < basic.TextureIndexes.size(); texture_slot++) {
        if (basic.TextureIndexes[texture_slot] >= 0) {
            return "advanced texture slots are unsupported";
        }
    }
    if (basic.GetIsRenderedWithAdvancedRenderer() || basic.TextureBlendType != -1 || basic.EmissiveScaling != 1.0f || basic.SoftParticleDistanceFar != 0.0f || basic.SoftParticleDistanceNear != 0.0f || basic.SoftParticleDistanceNearOffset != 0.0f) {
        return "advanced material parameters are unsupported";
    }
    if (parameter.DepthParameterPtr->DepthOffset != 0.0f || parameter.DepthParameterPtr->IsDepthOffsetScaledWithCamera || parameter.DepthParameterPtr->IsDepthOffsetScaledWithParticleScale || parameter.DepthParameterPtr->SuppressionOfScalingByDepth != 1.0f || parameter.DepthParameterPtr->DepthClipping != std::numeric_limits<float32_t>::max()) {
        return "advanced depth parameters are unsupported";
    }

    return {};
}

// Eye space is right-handed and looks down -Z, so this third column of the view rotation is the camera's
// backward direction. Upstream Effekseer stores exactly this raw vector as its renderer "front direction"
// (LookAtRH puts normalize(eye - at) into Values[..][2] and SetCameraParameterInternal never negates it),
// so the reference billboard bases, the sprite Z-sort key, and DrawParameter::CameraFrontDirection are all
// calibrated to the backward vector; the Manager.h "normalize(focus - position)" comment does not match the
// renderer implementation.
static auto ExtractCameraBackward(const mat44& view_matrix) -> vec3
{
    FO_STACK_TRACE_ENTRY();

    vec3 backward {view_matrix[0][2], view_matrix[1][2], view_matrix[2][2]};

    return glm::dot(backward, backward) > 0.0f ? glm::normalize(backward) : vec3 {0.0f, 0.0f, 1.0f};
}

static auto CalculateBillboardBasis(Effekseer::BillboardType billboard, const Effekseer::SIMD::Mat43f& srt_matrix, const Effekseer::SIMD::Vec3f& direction, const vec3& camera_backward) -> glm::mat3
{
    FO_STACK_TRACE_ENTRY();

    Effekseer::SIMD::Vec3f scale;
    Effekseer::SIMD::Mat43f rotation;
    Effekseer::SIMD::Vec3f translation;
    srt_matrix.GetSRT(scale, rotation, translation);
    ignore_unused(scale, translation);

    vec3 up {0.0f, 1.0f, 0.0f};
    vec3 front = camera_backward;
    vec3 right {};

    if (billboard == Effekseer::BillboardType::YAxisFixed) {
        up = {rotation.X.GetY(), rotation.Y.GetY(), rotation.Z.GetY()};
        up = glm::dot(up, up) > 0.0f ? glm::normalize(up) : vec3 {0.0f, 1.0f, 0.0f};
    }
    else if (billboard == Effekseer::BillboardType::DirectionalBillboard) {
        up = ToVec3(direction);
        up = glm::dot(up, up) > 0.0f ? glm::normalize(up) : vec3 {0.0f, 1.0f, 0.0f};
    }

    right = glm::cross(up, front);
    if (glm::dot(right, right) <= std::numeric_limits<float32_t>::epsilon()) {
        if (billboard == Effekseer::BillboardType::YAxisFixed || billboard == Effekseer::BillboardType::DirectionalBillboard) {
            vec3 fallback_axis = std::abs(up.y) < 0.999f ? vec3 {0.0f, 1.0f, 0.0f} : vec3 {1.0f, 0.0f, 0.0f};
            right = glm::cross(up, fallback_axis);
        }
        else {
            vec3 fallback_up = std::abs(front.y) < 0.999f ? vec3 {0.0f, 1.0f, 0.0f} : vec3 {1.0f, 0.0f, 0.0f};
            right = glm::cross(fallback_up, front);
        }
    }
    right = glm::normalize(right);

    if (billboard == Effekseer::BillboardType::YAxisFixed || billboard == Effekseer::BillboardType::DirectionalBillboard) {
        front = glm::normalize(glm::cross(right, up));
    }
    else {
        up = glm::normalize(glm::cross(front, right));
    }

    if (billboard == Effekseer::BillboardType::RotatedBillboard) {
        float32_t rotation_xy_length = std::sqrt(std::max(0.0f, rotation.Y.GetX() * rotation.Y.GetX() + rotation.Y.GetY() * rotation.Y.GetY()));
        float32_t sine = rotation_xy_length > 0.001f ? rotation.Y.GetX() / rotation_xy_length : 0.0f;
        float32_t cosine = rotation_xy_length > 0.001f ? rotation.Y.GetY() / rotation_xy_length : 1.0f;
        vec3 rotated_right = right * cosine + up * sine;
        vec3 rotated_up = up * cosine - right * sine;
        right = rotated_right;
        up = rotated_up;
    }

    return {right, up, front};
}

static auto CalculateParticlePosition(Effekseer::BillboardType billboard, const Effekseer::SIMD::Mat43f& srt_matrix, const Effekseer::SIMD::Vec3f& direction, const vec3& local_position, const vec3& camera_backward) -> vec3
{
    FO_STACK_TRACE_ENTRY();

    if (billboard == Effekseer::BillboardType::Fixed) {
        Effekseer::SIMD::Vec3f local {local_position.x, local_position.y, local_position.z};
        return ToVec3(Effekseer::SIMD::Vec3f::Transform(local, srt_matrix));
    }

    Effekseer::SIMD::Vec3f scale;
    Effekseer::SIMD::Mat43f rotation;
    Effekseer::SIMD::Vec3f translation;
    srt_matrix.GetSRT(scale, rotation, translation);
    ignore_unused(rotation);

    glm::mat3 basis = CalculateBillboardBasis(billboard, srt_matrix, direction, camera_backward);
    vec3 scaled_local {local_position.x * scale.GetX(), local_position.y * scale.GetY(), local_position.z * scale.GetZ()};
    return ToVec3(translation) + basis * scaled_local;
}

// A node's colour texture as the renderer needs it: the atlas texture, the sub-rectangle the node's image occupies in
// it, and whether the shader must snap the sampled coordinate to texel centres. Filtering is a per-node property in
// Effekseer but a per-atlas one here (Render.AtlasLinearFiltration applies to every atlas), and a bilinear fetch at a
// texel centre returns exactly that texel, so a node asking for point sampling from a linearly filtered atlas gets true
// point sampling without a second atlas.
struct EffekseerNodeTexture
{
    ptr<RenderTexture> Texture;
    frect32 AtlasRect;
    bool PointSampled;
};

// Resolves the colour texture slot that every node family shares. An untextured node draws its authored vertex colours,
// so a private white pixel stands in for the atlas and the whole texture is the sampled rectangle. Fails the handle and
// returns nothing when the slot cannot be served, so every family keeps failing closed through one path.
static auto ResolveEffekseerNodeTexture(ptr<EffekseerParticleRuntimeSystem::Impl> system, int32_t texture_index, Effekseer::TextureFilterType filter, ptr<RenderTexture> white_texture, bool distortion = false) -> optional<EffekseerNodeTexture>
{
    FO_STACK_TRACE_ENTRY();

    if (texture_index < 0) {
        return EffekseerNodeTexture {.Texture = white_texture, .AtlasRect = {0.0f, 0.0f, 1.0f, 1.0f}, .PointSampled = false};
    }

    if (texture_index >= (distortion ? system->Effect->GetDistortionImageCount() : system->Effect->GetColorImageCount())) {
        system->Fail("particle node texture index is out of range");
        return std::nullopt;
    }

    Effekseer::TextureRef node_texture = distortion ? system->Effect->GetDistortionImage(texture_index) : system->Effect->GetColorImage(texture_index);

    if (!node_texture || !node_texture->GetBackend()) {
        system->Fail("particle node texture is not loaded");
        return std::nullopt;
    }

    Effekseer::RefPtr<FOnlineEffekseerTexture> texture = node_texture->GetBackend().DownCast<FOnlineEffekseerTexture>();

    if (!texture || !texture->RenderTextureRef) {
        system->Fail("particle node texture was not loaded by the FOnline texture loader");
        return std::nullopt;
    }

    ptr<RenderTexture> render_texture = texture->RenderTextureRef.as_ptr();

    return EffekseerNodeTexture {.Texture = render_texture, .AtlasRect = texture->AtlasRect, .PointSampled = filter == Effekseer::TextureFilterType::Nearest && render_texture->LinearFiltered};
}

// The particle's own plane, as the distortion shader needs it: a displacement of (1, 0) in the distortion map moves
// the sampled background along the tangent, and (0, 1) along the binormal. A billboard takes them from the basis it
// faces the camera with; a fixed-orientation quad takes them from its own rotation.
static auto CalculateParticleTangentFrame(Effekseer::BillboardType billboard, const Effekseer::SIMD::Mat43f& srt_matrix, const Effekseer::SIMD::Vec3f& direction, const vec3& camera_backward) -> pair<vec3, vec3>
{
    FO_STACK_TRACE_ENTRY();

    const auto normalize_axis = [](const vec3& axis, const vec3& fallback) -> vec3 { return glm::dot(axis, axis) > 0.0f ? glm::normalize(axis) : fallback; };

    if (billboard == Effekseer::BillboardType::Fixed) {
        Effekseer::SIMD::Vec3f scale;
        Effekseer::SIMD::Mat43f rotation;
        Effekseer::SIMD::Vec3f translation;
        srt_matrix.GetSRT(scale, rotation, translation);
        ignore_unused(scale, translation);

        vec3 tangent {rotation.X.GetX(), rotation.X.GetY(), rotation.X.GetZ()};
        vec3 binormal {rotation.Y.GetX(), rotation.Y.GetY(), rotation.Y.GetZ()};

        return {normalize_axis(tangent, vec3 {1.0f, 0.0f, 0.0f}), normalize_axis(binormal, vec3 {0.0f, 1.0f, 0.0f})};
    }

    glm::mat3 basis = CalculateBillboardBasis(billboard, srt_matrix, direction, camera_backward);

    return {normalize_axis(basis[0], vec3 {1.0f, 0.0f, 0.0f}), normalize_axis(basis[1], vec3 {0.0f, 1.0f, 0.0f})};
}

class FOnlineEffekseerSpriteRenderer final : public Effekseer::SpriteRenderer
{
public:
    FOnlineEffekseerSpriteRenderer(ptr<EffectManager> effect_mngr, ptr<IAppRender> render, ptr<RenderSettings> settings, shared_ptr<EffekseerDrawBinding> binding, ParticleSceneBackgroundProvider scene_background_provider) :
        _binding {std::move(binding)},
        _sceneBackgroundProvider {std::move(scene_background_provider)},
        _particleEffects {effect_mngr},
        _drawBuffer {render->CreateDrawBuffer(false)},
#if FO_ENABLE_3D
        _distortionDrawBuffer {render->CreateDrawBuffer(false)},
#endif
        _whiteTexture {render->CreateTexture({1, 1}, true, false)},
        _effectMngr {effect_mngr},
        _render {render},
        _settings {settings}
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(_binding, "Effekseer sprite renderer requires draw binding");

#if FO_ENABLE_3D
        _distortionDrawBuffer->PrimType = RenderPrimitiveType::TriangleList;
#endif

        constexpr ucolor white_pixel {255, 255, 255, 255};
        _whiteTexture->UpdateTextureRegion({}, {1, 1}, {&white_pixel, 1});
        _drawBuffer->PrimType = RenderPrimitiveType::TriangleList;
    }

    void BeginRendering(const NodeParameter& parameter, int32_t count, void* user_data) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(user_data);
        _instances.clear();
        _node.reset();
        _declaredInstanceCount = 0;

        if (!_binding->CurrentSystem) {
            return;
        }
        if (count < 0 || numeric_cast<size_t>(count) > EFFEKSEER_SPRITE_INSTANCE_MAX) {
            _binding->Fail("sprite node exceeds the supported instance count");
            return;
        }
        if (string_view reason = ValidateSpriteNodeParameter(parameter); !reason.empty()) {
            _binding->Fail(reason);
            return;
        }
        if (parameter.EffectPointer != _binding->CurrentSystem->Effect.Get()) {
            _binding->Fail("sprite renderer received an unexpected effect pointer");
            return;
        }

        _declaredInstanceCount = numeric_cast<size_t>(count);
        _node = EffekseerSpriteNodeSnapshot {
            .Billboard = parameter.Billboard,
            .ZSort = parameter.ZSort,
            .AlphaBlend = parameter.BasicParameterPtr->AlphaBlend,
            .TextureFilter = parameter.BasicParameterPtr->TextureFilters[0],
            .TextureWrap = parameter.BasicParameterPtr->TextureWraps[0],
            .TextureIndex = parameter.BasicParameterPtr->TextureIndexes[0],
            .Distortion = parameter.BasicParameterPtr->MaterialType == Effekseer::RendererMaterialType::BackDistortion,
            .DistortionIntensity = parameter.BasicParameterPtr->DistortionIntensity,
            .ZTest = parameter.ZTest,
            .ZWrite = parameter.ZWrite,
        };
        _instances.reserve(_declaredInstanceCount);
    }

    void Rendering(const NodeParameter& parameter, const InstanceParameter& instance, void* user_data) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(parameter, user_data);
        if (!_binding->CurrentSystem || !_node || _binding->CurrentSystem->Failed) {
            return;
        }
        if (_instances.size() >= EFFEKSEER_SPRITE_INSTANCE_MAX) {
            _binding->Fail("sprite callback exceeds the supported instance count");
            return;
        }
        if (_instances.size() >= _declaredInstanceCount) {
            _binding->Fail("sprite callback emitted more instances than declared");
            return;
        }
        if (!std::isfinite(instance.AlphaThreshold) || instance.AlphaThreshold != 0.0f) {
            _binding->Fail("alpha cutoff instance data is unsupported");
            return;
        }
        if (!IsFinite(instance.SRTMatrix43)) {
            _binding->Fail("sprite callback emitted a non-finite transform");
            return;
        }
        for (const auto& position : instance.Positions) {
            if (!IsFinite(position)) {
                _binding->Fail("sprite callback emitted a non-finite local position");
                return;
            }
        }
        if (_node->Billboard == Effekseer::BillboardType::DirectionalBillboard && !IsFinite(instance.Direction)) {
            _binding->Fail("directional sprite callback emitted a non-finite direction");
            return;
        }

        float32_t uv_left = instance.UV.X;
        float32_t uv_right = instance.UV.X + instance.UV.Width;
        float32_t uv_top = instance.UV.Y;
        float32_t uv_bottom = instance.UV.Y + instance.UV.Height;
        if (!std::isfinite(uv_left) || !std::isfinite(uv_right) || !std::isfinite(uv_top) || !std::isfinite(uv_bottom)) {
            _binding->Fail("sprite callback emitted non-finite texture coordinates");
            return;
        }

        vec3 position = ToVec3(instance.SRTMatrix43.GetTranslation());
        vec3 camera_backward = ExtractCameraBackward(_binding->CurrentSystem->ViewMatrix);

        // Same key as the reference SpriteRendererBase: dot of the raw translation with the backward
        // vector, so NormalOrder ascending renders back-to-front.
        float32_t camera_depth = glm::dot(position, camera_backward);
        if (!std::isfinite(camera_depth)) {
            _binding->Fail("sprite callback emitted a non-finite camera depth");
            return;
        }

        Effekseer::SIMD::Vec3f direction {0.0f, 1.0f, 0.0f};
        if (_node->Billboard == Effekseer::BillboardType::DirectionalBillboard) {
            direction = instance.Direction;
        }

        _instances.emplace_back(EffekseerSpriteInstanceSnapshot {
            .SRTMatrix43 = instance.SRTMatrix43,
            .Colors = {instance.Colors[0], instance.Colors[1], instance.Colors[2], instance.Colors[3]},
            .Positions = {instance.Positions[0], instance.Positions[1], instance.Positions[2], instance.Positions[3]},
            .UV = instance.UV,
            .Direction = direction,
            .CameraDepth = camera_depth,
        });
    }

    void EndRendering(const NodeParameter& parameter, void* user_data) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(parameter, user_data);

        if (!_binding->CurrentSystem || !_node || _binding->CurrentSystem->Failed) {
            return;
        }
        if (_instances.size() != _declaredInstanceCount) {
            _binding->Fail("sprite callback instance count does not match its declaration");
            _instances.clear();
            _node.reset();
            _declaredInstanceCount = 0;
            return;
        }
        if (_instances.empty()) {
            _node.reset();
            _declaredInstanceCount = 0;
            return;
        }

        if (_node->ZSort == Effekseer::ZSortType::NormalOrder) {
            StableSortSnapshotsByCameraDepth(_instances, false);
        }
        else if (_node->ZSort == Effekseer::ZSortType::ReverseOrder) {
            StableSortSnapshotsByCameraDepth(_instances, true);
        }

        Render(_binding->CurrentSystem.as_ptr());
        _instances.clear();
        _node.reset();
        _declaredInstanceCount = 0;
    }

private:
    void Render(ptr<EffekseerParticleRuntimeSystem::Impl> system)
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(_node, "Effekseer sprite render called without a node snapshot");

        optional<EffekseerNodeTexture> texture = ResolveEffekseerNodeTexture(system, _node->TextureIndex, _node->TextureFilter, _whiteTexture.get(), _node->Distortion);

        if (!texture) {
            return;
        }

#if FO_ENABLE_3D
        if (_node->Distortion) {
            RenderDistortion(system, *texture);
            return;
        }
#endif

        optional<EffekseerNodeRenderState> render_state = _particleEffects.Resolve(_node->AlphaBlend, _node->TextureWrap, _node->ZTest, _node->ZWrite);

        if (!render_state) {
            system->Fail("node sampling or blend mode has no renderer equivalent");
            return;
        }

        size_t vertex_count = _instances.size() * 4;
        size_t index_count = _instances.size() * 6;
        _drawBuffer->VertCount = 0;
        _drawBuffer->IndCount = 0;
        _drawBuffer->CheckAllocBuf(vertex_count, index_count);

        vec3 camera_backward = ExtractCameraBackward(system->ViewMatrix);

        for (size_t instance_index = 0; instance_index < _instances.size(); instance_index++) {
            const EffekseerSpriteInstanceSnapshot& instance = _instances[instance_index];
            // The shader maps this into the atlas sub-rectangle, so the vertex carries the emitter's own coordinate.
            float32_t uv_left = instance.UV.X;
            float32_t uv_right = instance.UV.X + instance.UV.Width;
            float32_t uv_top = instance.UV.Y;
            float32_t uv_bottom = instance.UV.Y + instance.UV.Height;
            const float32_t texture_u[4] = {uv_left, uv_right, uv_left, uv_right};
            const float32_t texture_v[4] = {uv_bottom, uv_bottom, uv_top, uv_top};

            for (size_t vertex_offset = 0; vertex_offset < 4; vertex_offset++) {
                vec3 local_position {instance.Positions[vertex_offset].GetX(), instance.Positions[vertex_offset].GetY(), 0.0f};
                vec3 position = CalculateParticlePosition(_node->Billboard, instance.SRTMatrix43, instance.Direction, local_position, camera_backward);

                if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
                    system->Fail("sprite geometry produced a non-finite vertex");
                    return;
                }

                Vertex2D& vertex = _drawBuffer->Vertices[instance_index * 4 + vertex_offset];
                vertex.PosX = position.x;
                vertex.PosY = position.y;
                vertex.PosZ = position.z;
                vertex.Color = ToColor(instance.Colors[vertex_offset]);
                vertex.TexU = texture_u[vertex_offset];
                vertex.TexV = texture_v[vertex_offset];
                vertex.EggFlags[0] = 0.0f;
                vertex.EggFlags[1] = 0.0f;
            }

            size_t vertex_base = instance_index * 4;
            size_t index_base = instance_index * 6;
            _drawBuffer->Indices[index_base + 0] = numeric_cast<vindex_t>(vertex_base + 0);
            _drawBuffer->Indices[index_base + 1] = numeric_cast<vindex_t>(vertex_base + 1);
            _drawBuffer->Indices[index_base + 2] = numeric_cast<vindex_t>(vertex_base + 2);
            _drawBuffer->Indices[index_base + 3] = numeric_cast<vindex_t>(vertex_base + 2);
            _drawBuffer->Indices[index_base + 4] = numeric_cast<vindex_t>(vertex_base + 1);
            _drawBuffer->Indices[index_base + 5] = numeric_cast<vindex_t>(vertex_base + 3);
        }

        _drawBuffer->VertCount = vertex_count;
        _drawBuffer->IndCount = index_count;
        _drawBuffer->Upload(EffectUsage::QuadSprite, vertex_count, index_count);

        ptr<RenderEffect> effect = render_state->Effect.as_ptr();
        effect->DisableBlending = render_state->DisableBlending;
        effect->DepthVariant = render_state->DepthVariant;
        effect->CullMode = CullModeType::None;
        effect->ProjBuf = RenderEffect::ProjBuffer();
        MemCopy(effect->ProjBuf->ProjMatrix, glm::value_ptr(system->ViewProjMatrix), sizeof(effect->ProjBuf->ProjMatrix));
        effect->MainTex = texture->Texture;
        effect->ParticleSamplingBuf = RenderEffect::ParticleSamplingBuffer();
        effect->ParticleSamplingBuf->ParticleSampling[0] = texture->PointSampled ? 1.0f : 0.0f;
        effect->ParticleSamplingBuf->ParticleSampling[1] = render_state->ClampInShader ? 1.0f : 0.0f;

        // The fragment addresses the raw emitter coordinate inside this rectangle.
        effect->SpriteBorderBuf = RenderEffect::SpriteBorderBuffer();
        effect->SpriteBorderBuf->SpriteBorder[0] = texture->AtlasRect.x;
        effect->SpriteBorderBuf->SpriteBorder[1] = texture->AtlasRect.y;
        effect->SpriteBorderBuf->SpriteBorder[2] = texture->AtlasRect.x + texture->AtlasRect.width;
        effect->SpriteBorderBuf->SpriteBorder[3] = texture->AtlasRect.y + texture->AtlasRect.height;

        effect->DrawBuffer(_drawBuffer, 0, index_count);

        if (_settings->DrawWireframe) {
            DrawParticleBufferWireframe(_effectMngr, _render, _wireframeBuf, *_drawBuffer, index_count, system->ViewProjMatrix);
        }
    }

#if FO_ENABLE_3D
    // A distortion quad is the same geometry as an ordinary one, but it also has to tell the shader which way its own
    // plane points, so the displacement its texture describes is applied in the particle's frame rather than the
    // screen's. That is what the model vertex layout carries, so this path fills Vertices3D instead of Vertices.
    void RenderDistortion(ptr<EffekseerParticleRuntimeSystem::Impl> system, const EffekseerNodeTexture& texture)
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(_node, "Effekseer sprite distortion render called without a node snapshot");

        optional<EffekseerNodeRenderState> render_state = _particleEffects.ResolveDistortion(_node->AlphaBlend, _node->TextureWrap, _node->ZTest, _node->ZWrite);

        if (!render_state) {
            system->Fail("distortion node sampling or blend mode has no renderer equivalent");
            return;
        }

        // Without a scene to refract there is nothing to draw: an offscreen atlas has no background behind it.
        nptr<const RenderTexture> background = _sceneBackgroundProvider ? _sceneBackgroundProvider() : nullptr;

        if (!background) {
            system->Fail("distortion nodes need a scene background, which this draw target has none of");
            return;
        }

        size_t vertex_count = _instances.size() * 4;
        size_t index_count = _instances.size() * 6;
        _distortionDrawBuffer->VertCount = 0;
        _distortionDrawBuffer->IndCount = 0;
        _distortionDrawBuffer->Vertices3D.resize(std::max(_distortionDrawBuffer->Vertices3D.size(), vertex_count));
        _distortionDrawBuffer->Indices.resize(std::max(_distortionDrawBuffer->Indices.size(), index_count));

        vec3 camera_backward = ExtractCameraBackward(system->ViewMatrix);

        for (size_t instance_index = 0; instance_index < _instances.size(); instance_index++) {
            const EffekseerSpriteInstanceSnapshot& instance = _instances[instance_index];
            auto [tangent, binormal] = CalculateParticleTangentFrame(_node->Billboard, instance.SRTMatrix43, instance.Direction, camera_backward);
            float32_t uv_left = instance.UV.X;
            float32_t uv_right = instance.UV.X + instance.UV.Width;
            float32_t uv_top = instance.UV.Y;
            float32_t uv_bottom = instance.UV.Y + instance.UV.Height;
            const float32_t texture_u[4] = {uv_left, uv_right, uv_left, uv_right};
            const float32_t texture_v[4] = {uv_bottom, uv_bottom, uv_top, uv_top};

            for (size_t vertex_offset = 0; vertex_offset < 4; vertex_offset++) {
                vec3 local_position {instance.Positions[vertex_offset].GetX(), instance.Positions[vertex_offset].GetY(), 0.0f};
                vec3 position = CalculateParticlePosition(_node->Billboard, instance.SRTMatrix43, instance.Direction, local_position, camera_backward);

                if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
                    system->Fail("distortion geometry produced a non-finite vertex");
                    return;
                }

                Vertex3D& vertex = _distortionDrawBuffer->Vertices3D[instance_index * 4 + vertex_offset];
                vertex = Vertex3D {};
                vertex.Position = position;
                vertex.Tangent = tangent;
                vertex.Bitangent = binormal;
                vertex.TexCoord[0] = texture_u[vertex_offset];
                vertex.TexCoord[1] = texture_v[vertex_offset];
                vertex.Color = ToColor(instance.Colors[vertex_offset]);
            }

            size_t vertex_base = instance_index * 4;
            size_t index_base = instance_index * 6;
            _distortionDrawBuffer->Indices[index_base + 0] = numeric_cast<vindex_t>(vertex_base + 0);
            _distortionDrawBuffer->Indices[index_base + 1] = numeric_cast<vindex_t>(vertex_base + 1);
            _distortionDrawBuffer->Indices[index_base + 2] = numeric_cast<vindex_t>(vertex_base + 2);
            _distortionDrawBuffer->Indices[index_base + 3] = numeric_cast<vindex_t>(vertex_base + 2);
            _distortionDrawBuffer->Indices[index_base + 4] = numeric_cast<vindex_t>(vertex_base + 1);
            _distortionDrawBuffer->Indices[index_base + 5] = numeric_cast<vindex_t>(vertex_base + 3);
        }

        _distortionDrawBuffer->VertCount = vertex_count;
        _distortionDrawBuffer->IndCount = index_count;
        _distortionDrawBuffer->Upload(EffectUsage::Model, vertex_count, index_count);

        ptr<RenderEffect> effect = render_state->Effect.as_ptr();
        effect->DisableBlending = render_state->DisableBlending;
        effect->DepthVariant = render_state->DepthVariant;
        effect->CullMode = CullModeType::None;
        effect->ProjBuf = RenderEffect::ProjBuffer();
        MemCopy(effect->ProjBuf->ProjMatrix, glm::value_ptr(system->ViewProjMatrix), sizeof(effect->ProjBuf->ProjMatrix));
        effect->MainTex = texture.Texture;
        effect->BackgroundTex = background;
        effect->ParticleSamplingBuf = RenderEffect::ParticleSamplingBuffer();
        effect->ParticleSamplingBuf->ParticleSampling[0] = texture.PointSampled ? 1.0f : 0.0f;
        effect->ParticleSamplingBuf->ParticleSampling[1] = render_state->ClampInShader ? 1.0f : 0.0f;
        effect->ParticleSamplingBuf->ParticleSampling[2] = _node->DistortionIntensity;
        // The snapshot keeps whatever orientation its source render target has, so the shader flips the screen-space
        // lookup for a flipped one instead of the copy being re-oriented.
        effect->ParticleSamplingBuf->ParticleSampling[3] = background->FlippedHeight ? 1.0f : 0.0f;

        // The fragment addresses the raw emitter coordinate inside this rectangle.
        effect->SpriteBorderBuf = RenderEffect::SpriteBorderBuffer();
        effect->SpriteBorderBuf->SpriteBorder[0] = texture.AtlasRect.x;
        effect->SpriteBorderBuf->SpriteBorder[1] = texture.AtlasRect.y;
        effect->SpriteBorderBuf->SpriteBorder[2] = texture.AtlasRect.x + texture.AtlasRect.width;
        effect->SpriteBorderBuf->SpriteBorder[3] = texture.AtlasRect.y + texture.AtlasRect.height;

        effect->DrawBuffer(_distortionDrawBuffer, 0, index_count);
        effect->BackgroundTex = nullptr;
    }
#endif

    shared_ptr<EffekseerDrawBinding> _binding;
    ParticleSceneBackgroundProvider _sceneBackgroundProvider;
    EffekseerParticleEffects _particleEffects;
    unique_ptr<RenderDrawBuffer> _drawBuffer;
#if FO_ENABLE_3D
    unique_ptr<RenderDrawBuffer> _distortionDrawBuffer;
#endif
    unique_ptr<RenderTexture> _whiteTexture;
    unique_nptr<RenderDrawBuffer> _wireframeBuf {};
    ptr<EffectManager> _effectMngr;
    ptr<IAppRender> _render;
    ptr<RenderSettings> _settings;
    optional<EffekseerSpriteNodeSnapshot> _node {};
    size_t _declaredInstanceCount {};
    vector<EffekseerSpriteInstanceSnapshot> _instances {};
};

class FOnlineEffekseerRingRenderer final : public Effekseer::RingRenderer
{
public:
    FOnlineEffekseerRingRenderer(ptr<EffectManager> effect_mngr, ptr<IAppRender> render, ptr<RenderSettings> settings, shared_ptr<EffekseerDrawBinding> binding) :
        _binding {std::move(binding)},
        _particleEffects {effect_mngr},
        _drawBuffer {render->CreateDrawBuffer(false)},
        _effectMngr {effect_mngr},
        _render {render},
        _settings {settings},
        _whiteTexture {render->CreateTexture({1, 1}, true, false)}
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(_binding, "Effekseer ring renderer requires draw binding");

        constexpr ucolor white_pixel {255, 255, 255, 255};
        _whiteTexture->UpdateTextureRegion({}, {1, 1}, {&white_pixel, 1});
        _drawBuffer->PrimType = RenderPrimitiveType::TriangleList;
    }

    void BeginRendering(const NodeParameter& parameter, int32_t count, void* user_data) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(user_data);
        ResetState();

        if (!_binding->CurrentSystem) {
            return;
        }
        if (count < 0 || count > EFFEKSEER_INSTANCE_MAX) {
            _binding->Fail("ring node exceeds the supported instance count");
            return;
        }
        if (string_view reason = ValidateRingNodeParameter(parameter); !reason.empty()) {
            _binding->Fail(reason);
            return;
        }
        if (parameter.EffectPointer != _binding->CurrentSystem->Effect.Get()) {
            _binding->Fail("ring renderer received an unexpected effect pointer");
            return;
        }

        _declaredInstanceCount = numeric_cast<size_t>(count);
        _node = EffekseerRingNodeSnapshot {
            .Billboard = parameter.Billboard,
            .ZSort = parameter.DepthParameterPtr->ZSort,
            .AlphaBlend = parameter.BasicParameterPtr->AlphaBlend,
            .TextureFilter = parameter.BasicParameterPtr->TextureFilters[0],
            .TextureWrap = parameter.BasicParameterPtr->TextureWraps[0],
            .TextureIndex = parameter.BasicParameterPtr->TextureIndexes[0],
            .VertexCount = parameter.VertexCount,
            .StartingFade = parameter.StartingFade,
            .EndingFade = parameter.EndingFade,
            .ZTest = parameter.ZTest,
            .ZWrite = parameter.ZWrite,
        };
        _instances.reserve(_declaredInstanceCount);
    }

    void Rendering(const NodeParameter& parameter, const InstanceParameter& instance, void* user_data) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(parameter, user_data);

        if (!_binding->CurrentSystem || !_node || _binding->CurrentSystem->Failed) {
            return;
        }
        if (_instances.size() >= numeric_cast<size_t>(EFFEKSEER_INSTANCE_MAX)) {
            _binding->Fail("ring callback exceeds the supported instance count");
            return;
        }
        if (_instances.size() >= _declaredInstanceCount) {
            _binding->Fail("ring callback emitted more instances than declared");
            return;
        }
        if (!std::isfinite(instance.AlphaThreshold) || instance.AlphaThreshold != 0.0f) {
            _binding->Fail("alpha cutoff instance data is unsupported");
            return;
        }
        if (!IsFinite(instance.SRTMatrix43) || !IsFinite(instance.OuterLocation) || !IsFinite(instance.InnerLocation)) {
            _binding->Fail("ring callback emitted non-finite geometry data");
            return;
        }
        if (!std::isfinite(instance.ViewingAngleStart) || !std::isfinite(instance.ViewingAngleEnd) || !std::isfinite(instance.CenterRatio)) {
            _binding->Fail("ring callback emitted non-finite shape data");
            return;
        }
        if (_node->Billboard == Effekseer::BillboardType::DirectionalBillboard && !IsFinite(instance.Direction)) {
            _binding->Fail("directional ring callback emitted a non-finite direction");
            return;
        }

        float32_t uv_left = instance.UV.X;
        float32_t uv_right = instance.UV.X + instance.UV.Width;
        float32_t uv_top = instance.UV.Y;
        float32_t uv_bottom = instance.UV.Y + instance.UV.Height;

        if (!std::isfinite(uv_left) || !std::isfinite(uv_right) || !std::isfinite(uv_top) || !std::isfinite(uv_bottom)) {
            _binding->Fail("ring callback emitted non-finite texture coordinates");
            return;
        }

        vec3 position = ToVec3(instance.SRTMatrix43.GetTranslation());
        vec3 camera_backward = ExtractCameraBackward(_binding->CurrentSystem->ViewMatrix);
        float32_t camera_depth = glm::dot(position, camera_backward);

        if (!std::isfinite(camera_depth)) {
            _binding->Fail("ring callback emitted a non-finite camera depth");
            return;
        }

        Effekseer::SIMD::Vec3f direction {0.0f, 1.0f, 0.0f};

        if (_node->Billboard == Effekseer::BillboardType::DirectionalBillboard) {
            direction = instance.Direction;
        }

        _instances.emplace_back(EffekseerRingInstanceSnapshot {
            .SRTMatrix43 = instance.SRTMatrix43,
            .OuterLocation = instance.OuterLocation,
            .InnerLocation = instance.InnerLocation,
            .ViewingAngleStart = instance.ViewingAngleStart,
            .ViewingAngleEnd = instance.ViewingAngleEnd,
            .CenterRatio = instance.CenterRatio,
            .OuterColor = instance.OuterColor,
            .CenterColor = instance.CenterColor,
            .InnerColor = instance.InnerColor,
            .UV = instance.UV,
            .Direction = direction,
            .CameraDepth = camera_depth,
        });
    }

    void EndRendering(const NodeParameter& parameter, void* user_data) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(parameter, user_data);

        if (!_binding->CurrentSystem || !_node || _binding->CurrentSystem->Failed) {
            return;
        }
        if (_instances.size() != _declaredInstanceCount) {
            _binding->Fail("ring callback instance count does not match its declaration");
            ResetState();
            return;
        }
        if (_instances.empty()) {
            ResetState();
            return;
        }

        if (_node->ZSort == Effekseer::ZSortType::NormalOrder) {
            StableSortSnapshotsByCameraDepth(_instances, false);
        }
        else if (_node->ZSort == Effekseer::ZSortType::ReverseOrder) {
            StableSortSnapshotsByCameraDepth(_instances, true);
        }

        Render(_binding->CurrentSystem.as_ptr());
        ResetState();
    }

private:
    void ResetState()
    {
        FO_STACK_TRACE_ENTRY();

        _instances.clear();
        _node.reset();
        _declaredInstanceCount = 0;
    }

    void Render(ptr<EffekseerParticleRuntimeSystem::Impl> system)
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(_node, "Effekseer ring render called without a node snapshot");

        optional<EffekseerNodeTexture> texture = ResolveEffekseerNodeTexture(system, _node->TextureIndex, _node->TextureFilter, _whiteTexture.get());

        if (!texture) {
            return;
        }

        size_t vertices_per_instance = numeric_cast<size_t>(_node->VertexCount) * 8;
        size_t instances_per_draw = EFFEKSEER_CHUNK_VERTEX_MAX / vertices_per_instance;
        FO_VERIFY_AND_THROW(instances_per_draw != 0, "Effekseer ring geometry budget cannot fit one instance");

        for (size_t first_instance = 0; first_instance < _instances.size() && !system->Failed; first_instance += instances_per_draw) {
            size_t instance_count = std::min(instances_per_draw, _instances.size() - first_instance);
            RenderChunk(system, *texture, first_instance, instance_count);
        }
    }

    void RenderChunk(ptr<EffekseerParticleRuntimeSystem::Impl> system, const EffekseerNodeTexture& texture, size_t first_instance, size_t instance_count)
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(_node, "Effekseer ring chunk render called without a node snapshot");

        optional<EffekseerNodeRenderState> render_state = _particleEffects.Resolve(_node->AlphaBlend, _node->TextureWrap, _node->ZTest, _node->ZWrite);

        if (!render_state) {
            system->Fail("node sampling or blend mode has no renderer equivalent");
            return;
        }

        constexpr float32_t degrees_to_radians = 3.141592f / 180.0f;

        size_t segment_count = numeric_cast<size_t>(_node->VertexCount);
        size_t vertex_count = instance_count * segment_count * 8;
        size_t index_count = instance_count * segment_count * 12;
        _drawBuffer->VertCount = 0;
        _drawBuffer->IndCount = 0;
        _drawBuffer->CheckAllocBuf(vertex_count, index_count);

        vec3 camera_backward = ExtractCameraBackward(system->ViewMatrix);

        for (size_t chunk_instance_index = 0; chunk_instance_index < instance_count; chunk_instance_index++) {
            const EffekseerRingInstanceSnapshot& instance = _instances[first_instance + chunk_instance_index];
            float32_t inverse_segment_count = 1.0f / numeric_cast<float32_t>(segment_count);
            float32_t circle_angle = instance.ViewingAngleEnd - instance.ViewingAngleStart;
            float32_t step_angle_degrees = circle_angle * inverse_segment_count;
            float32_t step_angle = step_angle_degrees * degrees_to_radians;
            float32_t begin_angle = (instance.ViewingAngleStart + 90.0f) * degrees_to_radians;

            float32_t outer_radius = instance.OuterLocation.GetX();
            float32_t inner_radius = instance.InnerLocation.GetX();
            float32_t center_radius = inner_radius + (outer_radius - inner_radius) * instance.CenterRatio;
            float32_t outer_height = instance.OuterLocation.GetY();
            float32_t inner_height = instance.InnerLocation.GetY();
            float32_t center_height = inner_height + (outer_height - inner_height) * instance.CenterRatio;

            Effekseer::Color outer_color = instance.OuterColor;
            Effekseer::Color center_color = instance.CenterColor;
            Effekseer::Color inner_color = instance.InnerColor;

            if (_node->StartingFade > 0.0f) {
                outer_color.A = 0;
                center_color.A = 0;
                inner_color.A = 0;
            }

            float32_t step_cosine = std::cos(step_angle);
            float32_t step_sine = std::sin(step_angle);
            float32_t cosine = std::cos(begin_angle);
            float32_t sine = std::sin(begin_angle);
            float32_t current_angle_degrees = 0.0f;
            float32_t current_u = instance.UV.X;
            float32_t step_u = instance.UV.Width * inverse_segment_count;
            float32_t outer_v = instance.UV.Y;
            float32_t center_v = instance.UV.Y + instance.UV.Height * 0.5f;
            float32_t inner_v = instance.UV.Y + instance.UV.Height;

            for (size_t segment_index = 0; segment_index < segment_count; segment_index++) {
                float32_t next_cosine = cosine * step_cosine - sine * step_sine;
                float32_t next_sine = sine * step_cosine + cosine * step_sine;

                current_angle_degrees += step_angle_degrees;
                current_angle_degrees = std::min(current_angle_degrees, circle_angle);
                float32_t next_alpha = 1.0f;

                if (current_angle_degrees < _node->StartingFade) {
                    next_alpha = current_angle_degrees / _node->StartingFade;
                }
                else if (current_angle_degrees > circle_angle - _node->EndingFade) {
                    next_alpha = 1.0f - (current_angle_degrees - (circle_angle - _node->EndingFade)) / _node->EndingFade;
                }

                next_alpha = std::isfinite(next_alpha) ? std::clamp(next_alpha, 0.0f, 1.0f) : 0.0f;

                Effekseer::Color next_outer_color = instance.OuterColor;
                Effekseer::Color next_center_color = instance.CenterColor;
                Effekseer::Color next_inner_color = instance.InnerColor;

                if (next_alpha != 1.0f) {
                    // RingRendererBase intentionally truncates these products instead of rounding.
                    next_outer_color.A = iround<uint8_t>(std::trunc(numeric_cast<float32_t>(next_outer_color.A) * next_alpha));
                    next_center_color.A = iround<uint8_t>(std::trunc(numeric_cast<float32_t>(next_center_color.A) * next_alpha));
                    next_inner_color.A = iround<uint8_t>(std::trunc(numeric_cast<float32_t>(next_inner_color.A) * next_alpha));
                }

                float32_t next_u = current_u + step_u;
                const vec3 local_positions[8] = {
                    {cosine * outer_radius, sine * outer_radius, outer_height},
                    {cosine * center_radius, sine * center_radius, center_height},
                    {next_cosine * outer_radius, next_sine * outer_radius, outer_height},
                    {next_cosine * center_radius, next_sine * center_radius, center_height},
                    {cosine * center_radius, sine * center_radius, center_height},
                    {cosine * inner_radius, sine * inner_radius, inner_height},
                    {next_cosine * center_radius, next_sine * center_radius, center_height},
                    {next_cosine * inner_radius, next_sine * inner_radius, inner_height},
                };
                const Effekseer::Color colors[8] = {outer_color, center_color, next_outer_color, next_center_color, center_color, inner_color, next_center_color, next_inner_color};
                const float32_t texture_u[8] = {current_u, current_u, next_u, next_u, current_u, current_u, next_u, next_u};
                const float32_t texture_v[8] = {outer_v, center_v, outer_v, center_v, center_v, inner_v, center_v, inner_v};

                size_t segment_base = (chunk_instance_index * segment_count + segment_index) * 8;

                for (size_t vertex_offset = 0; vertex_offset < 8; vertex_offset++) {
                    vec3 position = CalculateParticlePosition(_node->Billboard, instance.SRTMatrix43, instance.Direction, local_positions[vertex_offset], camera_backward);

                    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
                        system->Fail("ring geometry produced a non-finite vertex");
                        return;
                    }

                    Vertex2D& vertex = _drawBuffer->Vertices[segment_base + vertex_offset];
                    vertex.PosX = position.x;
                    vertex.PosY = position.y;
                    vertex.PosZ = position.z;
                    vertex.Color = ToColor(colors[vertex_offset]);
                    vertex.TexU = texture_u[vertex_offset];
                    vertex.TexV = texture_v[vertex_offset];
                    vertex.EggFlags[0] = 0.0f;
                    vertex.EggFlags[1] = 0.0f;
                }

                size_t index_base = (chunk_instance_index * segment_count + segment_index) * 12;
                _drawBuffer->Indices[index_base + 0] = numeric_cast<vindex_t>(segment_base + 0);
                _drawBuffer->Indices[index_base + 1] = numeric_cast<vindex_t>(segment_base + 1);
                _drawBuffer->Indices[index_base + 2] = numeric_cast<vindex_t>(segment_base + 2);
                _drawBuffer->Indices[index_base + 3] = numeric_cast<vindex_t>(segment_base + 2);
                _drawBuffer->Indices[index_base + 4] = numeric_cast<vindex_t>(segment_base + 1);
                _drawBuffer->Indices[index_base + 5] = numeric_cast<vindex_t>(segment_base + 3);
                _drawBuffer->Indices[index_base + 6] = numeric_cast<vindex_t>(segment_base + 4);
                _drawBuffer->Indices[index_base + 7] = numeric_cast<vindex_t>(segment_base + 5);
                _drawBuffer->Indices[index_base + 8] = numeric_cast<vindex_t>(segment_base + 6);
                _drawBuffer->Indices[index_base + 9] = numeric_cast<vindex_t>(segment_base + 6);
                _drawBuffer->Indices[index_base + 10] = numeric_cast<vindex_t>(segment_base + 5);
                _drawBuffer->Indices[index_base + 11] = numeric_cast<vindex_t>(segment_base + 7);

                cosine = next_cosine;
                sine = next_sine;
                current_u = next_u;
                outer_color = next_outer_color;
                center_color = next_center_color;
                inner_color = next_inner_color;
            }
        }

        _drawBuffer->VertCount = vertex_count;
        _drawBuffer->IndCount = index_count;
        _drawBuffer->Upload(EffectUsage::QuadSprite, vertex_count, index_count);

        ptr<RenderEffect> effect = render_state->Effect.as_ptr();
        effect->DisableBlending = render_state->DisableBlending;
        effect->DepthVariant = render_state->DepthVariant;
        effect->CullMode = CullModeType::None;
        effect->ProjBuf = RenderEffect::ProjBuffer();
        MemCopy(effect->ProjBuf->ProjMatrix, glm::value_ptr(system->ViewProjMatrix), sizeof(effect->ProjBuf->ProjMatrix));
        effect->MainTex = texture.Texture;
        effect->ParticleSamplingBuf = RenderEffect::ParticleSamplingBuffer();
        effect->ParticleSamplingBuf->ParticleSampling[0] = texture.PointSampled ? 1.0f : 0.0f;
        effect->ParticleSamplingBuf->ParticleSampling[1] = render_state->ClampInShader ? 1.0f : 0.0f;

        // The fragment addresses the raw emitter coordinate inside this rectangle.
        effect->SpriteBorderBuf = RenderEffect::SpriteBorderBuffer();
        effect->SpriteBorderBuf->SpriteBorder[0] = texture.AtlasRect.x;
        effect->SpriteBorderBuf->SpriteBorder[1] = texture.AtlasRect.y;
        effect->SpriteBorderBuf->SpriteBorder[2] = texture.AtlasRect.x + texture.AtlasRect.width;
        effect->SpriteBorderBuf->SpriteBorder[3] = texture.AtlasRect.y + texture.AtlasRect.height;

        effect->DrawBuffer(_drawBuffer, 0, index_count);

        if (_settings->DrawWireframe) {
            DrawParticleBufferWireframe(_effectMngr, _render, _wireframeBuf, *_drawBuffer, index_count, system->ViewProjMatrix);
        }
    }

    shared_ptr<EffekseerDrawBinding> _binding;
    EffekseerParticleEffects _particleEffects;
    unique_ptr<RenderDrawBuffer> _drawBuffer;
    unique_nptr<RenderDrawBuffer> _wireframeBuf {};
    ptr<EffectManager> _effectMngr;
    ptr<IAppRender> _render;
    ptr<RenderSettings> _settings;
    unique_ptr<RenderTexture> _whiteTexture;
    optional<EffekseerRingNodeSnapshot> _node {};
    size_t _declaredInstanceCount {};
    vector<EffekseerRingInstanceSnapshot> _instances {};
};

// The direction a strip spreads its width along: the band's own axis crossed with the view direction, so the band keeps
// facing the camera while staying anchored to what the family considers the band axis - the emitter's up axis for a
// viewpoint-dependent ribbon, the direction of travel for a track.
static auto CalculateStripWidthAxis(const vec3& band_axis, const vec3& view_direction) -> vec3
{
    FO_STACK_TRACE_ENTRY();

    vec3 width_axis = glm::cross(band_axis, view_direction);

    if (glm::dot(width_axis, width_axis) <= std::numeric_limits<float32_t>::epsilon()) {
        // The band runs straight at the camera, so every perpendicular direction is equally correct.
        vec3 fallback = std::abs(band_axis.y) < 0.999f ? vec3 {0.0f, 1.0f, 0.0f} : vec3 {1.0f, 0.0f, 0.0f};
        width_axis = glm::cross(band_axis, fallback);
    }

    return glm::dot(width_axis, width_axis) > 0.0f ? glm::normalize(width_axis) : vec3 {1.0f, 0.0f, 0.0f};
}

// A track's colour and width fade toward their middle value across the strip. TrackRendererBase clamps the interpolated
// product into the byte range and truncates it rather than rounding, so this reproduces that exactly.
static auto LerpTrackColor(const Effekseer::Color& from, const Effekseer::Color& to, float32_t factor) -> Effekseer::Color
{
    FO_STACK_TRACE_ENTRY();

    const auto lerp_channel = [factor](uint8_t from_channel, uint8_t to_channel) -> uint8_t {
        float32_t value = numeric_cast<float32_t>(from_channel) + (numeric_cast<float32_t>(to_channel) - numeric_cast<float32_t>(from_channel)) * factor;

        return iround<uint8_t>(std::trunc(std::clamp(value, 0.0f, 255.0f)));
    };

    Effekseer::Color color;
    color.R = lerp_channel(from.R, to.R);
    color.G = lerp_channel(from.G, to.G);
    color.B = lerp_channel(from.B, to.B);
    color.A = lerp_channel(from.A, to.A);

    return color;
}

// The node-level contract Ribbon and Track share. Everything the strip geometry does not implement fails closed here,
// before a single vertex is built: the corpus census found spline smoothing, tiled strip UVs, trail smoothing, view
// offset and left-handed strips entirely unused, so implementing them would be speculation rather than support.
static auto ValidateStripNodeParameter(const Effekseer::NodeRendererBasicParameter* basic, const Effekseer::NodeRendererDepthParameter* depth, const Effekseer::NodeRendererTextureUVTypeParameter* texture_uv, int32_t spline_division, bool enable_view_offset, bool is_right_hand) -> string_view
{
    FO_STACK_TRACE_ENTRY();

    if (basic == nullptr || depth == nullptr || texture_uv == nullptr) {
        return "strip renderer received incomplete node parameters";
    }
    if (!is_right_hand) {
        return "left-handed strip nodes are unsupported";
    }
    if (enable_view_offset) {
        return "view offset is unsupported";
    }
    if (spline_division != 1) {
        return "spline-smoothed strips are unsupported";
    }
    if (texture_uv->Type != Effekseer::TextureUVType::Strech) {
        return "tiled strip texture coordinates are unsupported";
    }
    if (basic->MaterialType != Effekseer::RendererMaterialType::Default || basic->MaterialRenderDataPtr != nullptr) {
        return "only the Default material is supported";
    }
    if (basic->AlphaBlend == Effekseer::AlphaBlendType::Mul) {
        return "multiply blending is unsupported";
    }
    if (basic->TextureIndexes[0] < -1) {
        return "strip node has an invalid color texture index";
    }
    if (basic->TextureIndexes[0] >= 0 && basic->TextureFilters[0] != Effekseer::TextureFilterType::Nearest && basic->TextureFilters[0] != Effekseer::TextureFilterType::Linear) {
        return "strip node uses an unknown texture filter";
    }
    if (basic->TextureIndexes[0] >= 0 && basic->TextureWraps[0] != Effekseer::TextureWrapType::Clamp && basic->TextureWraps[0] != Effekseer::TextureWrapType::Repeat) {
        return "mirrored texture wrapping is unsupported";
    }

    for (size_t texture_slot = 1; texture_slot < basic->TextureIndexes.size(); texture_slot++) {
        if (basic->TextureIndexes[texture_slot] >= 0) {
            return "advanced texture slots are unsupported";
        }
    }

    if (basic->GetIsRenderedWithAdvancedRenderer() || basic->TextureBlendType != -1 || basic->EmissiveScaling != 1.0f || basic->SoftParticleDistanceFar != 0.0f || basic->SoftParticleDistanceNear != 0.0f || basic->SoftParticleDistanceNearOffset != 0.0f) {
        return "advanced material parameters are unsupported";
    }
    if (depth->DepthOffset != 0.0f || depth->IsDepthOffsetScaledWithCamera || depth->IsDepthOffsetScaledWithParticleScale || depth->SuppressionOfScalingByDepth != 1.0f || depth->DepthClipping != std::numeric_limits<float32_t>::max()) {
        return "advanced depth parameters are unsupported";
    }
    // A strip is stitched by instance index, so a Z-sorted node would hand its instances over in an order that no longer
    // describes the chain. Nothing in the corpus asks for it and upstream silently builds a scrambled band.
    if (depth->ZSort != Effekseer::ZSortType::None) {
        return "Z-sorted strip nodes are unsupported";
    }

    return {};
}

// Effekseer picks which faces to discard per node; the renderer carries the same choice per draw.
static auto ConvertEffekseerCulling(Effekseer::CullingType culling) -> optional<CullModeType>
{
    FO_STACK_TRACE_ENTRY();

    switch (culling) {
    case Effekseer::CullingType::Front:
        return CullModeType::Front;
    case Effekseer::CullingType::Back:
        return CullModeType::Back;
    case Effekseer::CullingType::Double:
        return CullModeType::None;
    default:
        return std::nullopt;
    }
}

static auto ValidateModelNodeParameter(const Effekseer::ModelRenderer::NodeParameter& parameter) -> string_view
{
    FO_STACK_TRACE_ENTRY();

    if (parameter.EffectPointer == nullptr || parameter.BasicParameterPtr == nullptr || parameter.DepthParameterPtr == nullptr) {
        return "model renderer received incomplete node parameters";
    }
    if (!parameter.IsRightHand) {
        return "left-handed model nodes are unsupported";
    }
    if (parameter.EnableViewOffset) {
        return "view offset is unsupported";
    }
    if (parameter.IsProceduralMode || parameter.IsExternalMode || parameter.ExternalModel != nullptr) {
        return "procedural and externally supplied models are unsupported";
    }
    if (parameter.EnableFalloff) {
        return "falloff is unsupported";
    }
    if (parameter.Billboard != Effekseer::BillboardType::Billboard && parameter.Billboard != Effekseer::BillboardType::RotatedBillboard && parameter.Billboard != Effekseer::BillboardType::YAxisFixed && parameter.Billboard != Effekseer::BillboardType::DirectionalBillboard && parameter.Billboard != Effekseer::BillboardType::Fixed) {
        return "unknown model billboard mode";
    }
    if (parameter.Magnification != 1.0f || parameter.Maginification != 1.0f) {
        return "magnified model nodes are unsupported";
    }

    const Effekseer::NodeRendererBasicParameter& basic = *parameter.BasicParameterPtr;

    if (basic.MaterialType != Effekseer::RendererMaterialType::Default || basic.MaterialRenderDataPtr != nullptr) {
        return "only the Default material is supported";
    }
    if (basic.AlphaBlend == Effekseer::AlphaBlendType::Mul) {
        return "multiply blending is unsupported";
    }
    if (basic.TextureIndexes[0] < -1) {
        return "model node has an invalid color texture index";
    }
    if (basic.TextureIndexes[0] >= 0 && basic.TextureFilters[0] != Effekseer::TextureFilterType::Nearest && basic.TextureFilters[0] != Effekseer::TextureFilterType::Linear) {
        return "model node uses an unknown texture filter";
    }
    if (basic.TextureIndexes[0] >= 0 && basic.TextureWraps[0] != Effekseer::TextureWrapType::Clamp && basic.TextureWraps[0] != Effekseer::TextureWrapType::Repeat) {
        return "mirrored texture wrapping is unsupported";
    }

    for (size_t texture_slot = 1; texture_slot < basic.TextureIndexes.size(); texture_slot++) {
        if (basic.TextureIndexes[texture_slot] >= 0) {
            return "advanced texture slots are unsupported";
        }
    }

    if (basic.GetIsRenderedWithAdvancedRenderer() || basic.TextureBlendType != -1 || basic.EmissiveScaling != 1.0f || basic.SoftParticleDistanceFar != 0.0f || basic.SoftParticleDistanceNear != 0.0f || basic.SoftParticleDistanceNearOffset != 0.0f) {
        return "advanced material parameters are unsupported";
    }
    if (parameter.DepthParameterPtr->DepthOffset != 0.0f || parameter.DepthParameterPtr->IsDepthOffsetScaledWithCamera || parameter.DepthParameterPtr->IsDepthOffsetScaledWithParticleScale || parameter.DepthParameterPtr->SuppressionOfScalingByDepth != 1.0f || parameter.DepthParameterPtr->DepthClipping != std::numeric_limits<float32_t>::max()) {
        return "advanced depth parameters are unsupported";
    }
    if (parameter.DepthParameterPtr->ZSort != Effekseer::ZSortType::None) {
        return "Z-sorted model nodes are unsupported";
    }

    return {};
}

// The geometry Ribbon and Track share. Consecutive width triples become two quad strips - the left and right half of the
// band - and the texture is stretched along the whole chain, so the segment between instance k and k+1 samples the V
// range [k, k+1] / (count - 1). Ribbon and Track differ only in how one triple is produced, so texture resolution, the
// atlas addressing flags, chunking against the vertex budget and the draw tail all live here once.
class EffekseerStripGeometry final
{
public:
    EffekseerStripGeometry(ptr<EffectManager> effect_mngr, ptr<IAppRender> render, ptr<RenderSettings> settings) :
        _effectMngr {effect_mngr},
        _render {render},
        _settings {settings},
        _particleEffects {effect_mngr},
        _drawBuffer {render->CreateDrawBuffer(false)},
        _whiteTexture {render->CreateTexture({1, 1}, true, false)}
    {
        FO_STACK_TRACE_ENTRY();

        constexpr ucolor white_pixel {255, 255, 255, 255};
        _whiteTexture->UpdateTextureRegion({}, {1, 1}, {&white_pixel, 1});
        _drawBuffer->PrimType = RenderPrimitiveType::TriangleList;
    }

    // The chain holds the instances actually delivered; declared_instance_count is the node's own instance count, which
    // owns the texture stretch so the mapping stays the emitter's regardless of how many instances were alive.
    void Draw(ptr<EffekseerParticleRuntimeSystem::Impl> system, const EffekseerStripNodeSnapshot& node, const vector<EffekseerStripWidthTriple>& chain, size_t declared_instance_count)
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(chain.size() >= 2, "Effekseer strip draw requires at least one segment", chain.size());
        FO_VERIFY_AND_THROW(declared_instance_count >= chain.size(), "Effekseer strip declared fewer instances than it delivered", declared_instance_count, chain.size());

        optional<EffekseerNodeTexture> texture = ResolveEffekseerNodeTexture(system, node.TextureIndex, node.TextureFilter, _whiteTexture.get());

        if (!texture) {
            return;
        }

        constexpr size_t segment_vertices = 8;
        size_t segments_per_draw = EFFEKSEER_CHUNK_VERTEX_MAX / segment_vertices;
        size_t segment_count = chain.size() - 1;

        for (size_t first_segment = 0; first_segment < segment_count && !system->Failed; first_segment += segments_per_draw) {
            DrawChunk(system, node, chain, declared_instance_count, *texture, first_segment, std::min(segments_per_draw, segment_count - first_segment));
        }
    }

private:
    void DrawChunk(ptr<EffekseerParticleRuntimeSystem::Impl> system, const EffekseerStripNodeSnapshot& node, const vector<EffekseerStripWidthTriple>& chain, size_t declared_instance_count, const EffekseerNodeTexture& texture, size_t first_segment, size_t segment_count)
    {
        FO_STACK_TRACE_ENTRY();

        optional<EffekseerNodeRenderState> render_state = _particleEffects.Resolve(node.AlphaBlend, node.TextureWrap, node.ZTest, node.ZWrite);

        if (!render_state) {
            system->Fail("node sampling or blend mode has no renderer equivalent");
            return;
        }

        size_t vertex_count = segment_count * 8;
        size_t index_count = segment_count * 12;
        _drawBuffer->VertCount = 0;
        _drawBuffer->IndCount = 0;
        _drawBuffer->CheckAllocBuf(vertex_count, index_count);

        float32_t stretch_divisor = numeric_cast<float32_t>(declared_instance_count - 1);

        for (size_t chunk_segment_index = 0; chunk_segment_index < segment_count; chunk_segment_index++) {
            size_t segment_index = first_segment + chunk_segment_index;
            const EffekseerStripWidthTriple& near_side = chain[segment_index];
            const EffekseerStripWidthTriple& far_side = chain[segment_index + 1];

            float32_t u_left = near_side.UV.X;
            float32_t u_center = near_side.UV.X + near_side.UV.Width * 0.5f;
            float32_t u_right = near_side.UV.X + near_side.UV.Width;
            float32_t v_near = near_side.UV.Y + numeric_cast<float32_t>(segment_index) / stretch_divisor * near_side.UV.Height;
            float32_t v_far = near_side.UV.Y + numeric_cast<float32_t>(segment_index + 1) / stretch_divisor * near_side.UV.Height;

            const vec3 positions[8] = {
                near_side.LeftPosition,
                near_side.CenterPosition,
                far_side.LeftPosition,
                far_side.CenterPosition,
                near_side.CenterPosition,
                near_side.RightPosition,
                far_side.CenterPosition,
                far_side.RightPosition,
            };
            const Effekseer::Color colors[8] = {
                near_side.LeftColor,
                near_side.CenterColor,
                far_side.LeftColor,
                far_side.CenterColor,
                near_side.CenterColor,
                near_side.RightColor,
                far_side.CenterColor,
                far_side.RightColor,
            };
            const float32_t texture_u[8] = {u_left, u_center, u_left, u_center, u_center, u_right, u_center, u_right};
            const float32_t texture_v[8] = {v_near, v_near, v_far, v_far, v_near, v_near, v_far, v_far};

            size_t vertex_base = chunk_segment_index * 8;

            for (size_t vertex_offset = 0; vertex_offset < 8; vertex_offset++) {
                const vec3& position = positions[vertex_offset];

                if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) || !std::isfinite(texture_u[vertex_offset]) || !std::isfinite(texture_v[vertex_offset])) {
                    system->Fail("strip geometry produced a non-finite vertex");
                    return;
                }

                Vertex2D& vertex = _drawBuffer->Vertices[vertex_base + vertex_offset];
                vertex.PosX = position.x;
                vertex.PosY = position.y;
                vertex.PosZ = position.z;
                vertex.Color = ToColor(colors[vertex_offset]);
                vertex.TexU = texture_u[vertex_offset];
                vertex.TexV = texture_v[vertex_offset];
                vertex.EggFlags[0] = 0.0f;
                vertex.EggFlags[1] = 0.0f;
            }

            size_t index_base = chunk_segment_index * 12;
            _drawBuffer->Indices[index_base + 0] = numeric_cast<vindex_t>(vertex_base + 0);
            _drawBuffer->Indices[index_base + 1] = numeric_cast<vindex_t>(vertex_base + 1);
            _drawBuffer->Indices[index_base + 2] = numeric_cast<vindex_t>(vertex_base + 2);
            _drawBuffer->Indices[index_base + 3] = numeric_cast<vindex_t>(vertex_base + 2);
            _drawBuffer->Indices[index_base + 4] = numeric_cast<vindex_t>(vertex_base + 1);
            _drawBuffer->Indices[index_base + 5] = numeric_cast<vindex_t>(vertex_base + 3);
            _drawBuffer->Indices[index_base + 6] = numeric_cast<vindex_t>(vertex_base + 4);
            _drawBuffer->Indices[index_base + 7] = numeric_cast<vindex_t>(vertex_base + 5);
            _drawBuffer->Indices[index_base + 8] = numeric_cast<vindex_t>(vertex_base + 6);
            _drawBuffer->Indices[index_base + 9] = numeric_cast<vindex_t>(vertex_base + 6);
            _drawBuffer->Indices[index_base + 10] = numeric_cast<vindex_t>(vertex_base + 5);
            _drawBuffer->Indices[index_base + 11] = numeric_cast<vindex_t>(vertex_base + 7);
        }

        _drawBuffer->VertCount = vertex_count;
        _drawBuffer->IndCount = index_count;
        _drawBuffer->Upload(EffectUsage::QuadSprite, vertex_count, index_count);

        ptr<RenderEffect> effect = render_state->Effect.as_ptr();
        effect->DisableBlending = render_state->DisableBlending;
        effect->DepthVariant = render_state->DepthVariant;
        effect->CullMode = CullModeType::None;
        effect->ProjBuf = RenderEffect::ProjBuffer();
        MemCopy(effect->ProjBuf->ProjMatrix, glm::value_ptr(system->ViewProjMatrix), sizeof(effect->ProjBuf->ProjMatrix));
        effect->MainTex = texture.Texture;
        effect->ParticleSamplingBuf = RenderEffect::ParticleSamplingBuffer();
        effect->ParticleSamplingBuf->ParticleSampling[0] = texture.PointSampled ? 1.0f : 0.0f;
        effect->ParticleSamplingBuf->ParticleSampling[1] = render_state->ClampInShader ? 1.0f : 0.0f;

        // The fragment addresses the raw emitter coordinate inside this rectangle.
        effect->SpriteBorderBuf = RenderEffect::SpriteBorderBuffer();
        effect->SpriteBorderBuf->SpriteBorder[0] = texture.AtlasRect.x;
        effect->SpriteBorderBuf->SpriteBorder[1] = texture.AtlasRect.y;
        effect->SpriteBorderBuf->SpriteBorder[2] = texture.AtlasRect.x + texture.AtlasRect.width;
        effect->SpriteBorderBuf->SpriteBorder[3] = texture.AtlasRect.y + texture.AtlasRect.height;

        effect->DrawBuffer(_drawBuffer, 0, index_count);

        if (_settings->DrawWireframe) {
            DrawParticleBufferWireframe(_effectMngr, _render, _wireframeBuf, *_drawBuffer, index_count, system->ViewProjMatrix);
        }
    }

    ptr<EffectManager> _effectMngr;
    ptr<IAppRender> _render;
    ptr<RenderSettings> _settings;
    EffekseerParticleEffects _particleEffects;
    unique_ptr<RenderDrawBuffer> _drawBuffer;
    unique_ptr<RenderTexture> _whiteTexture;
    unique_nptr<RenderDrawBuffer> _wireframeBuf {};
};

// A ribbon is a band threaded through its instances: each one contributes a left and a right edge offset, and the
// segment between two consecutive instances is drawn as two quads meeting at the band's centre line. Unless the node is
// viewpoint dependent the edges are simply transformed by the instance matrix; when it is, the band twists around the
// emitter's own up axis so it keeps facing the camera.
class FOnlineEffekseerRibbonRenderer final : public Effekseer::RibbonRenderer
{
public:
    FOnlineEffekseerRibbonRenderer(ptr<EffectManager> effect_mngr, ptr<IAppRender> render, ptr<RenderSettings> settings, shared_ptr<EffekseerDrawBinding> binding) :
        _binding {std::move(binding)},
        _geometry {effect_mngr, render, settings}
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(_binding, "Effekseer ribbon renderer requires draw binding");
    }

    void BeginRendering(const NodeParameter& parameter, int32_t count, void* user_data) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(count, user_data);
        ResetGroup();
        _node.reset();

        if (!_binding->CurrentSystem) {
            return;
        }
        if (string_view reason = ValidateStripNodeParameter(parameter.BasicParameterPtr, parameter.DepthParameterPtr, parameter.TextureUVTypeParameterPtr, parameter.SplineDivision, parameter.EnableViewOffset, parameter.IsRightHand); !reason.empty()) {
            _binding->Fail(reason);
            return;
        }
        if (parameter.EffectPointer != _binding->CurrentSystem->Effect.Get()) {
            _binding->Fail("ribbon renderer received an unexpected effect pointer");
            return;
        }

        _node = EffekseerStripNodeSnapshot {
            .AlphaBlend = parameter.BasicParameterPtr->AlphaBlend,
            .TextureFilter = parameter.BasicParameterPtr->TextureFilters[0],
            .TextureWrap = parameter.BasicParameterPtr->TextureWraps[0],
            .TextureIndex = parameter.BasicParameterPtr->TextureIndexes[0],
            .ZTest = parameter.ZTest,
            .ZWrite = parameter.ZWrite,
        };
        _viewpointDependent = parameter.ViewpointDependent;
    }

    // A node draws one strip per instance group, so the chain restarts here rather than in BeginRendering.
    void BeginRenderingGroup(const NodeParameter& parameter, int32_t count, void* user_data) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(parameter, user_data);
        ResetGroup();

        if (!_binding->CurrentSystem || !_node || _binding->CurrentSystem->Failed) {
            return;
        }
        if (count < 0 || count > EFFEKSEER_INSTANCE_MAX) {
            _binding->Fail("ribbon group exceeds the supported instance count");
            return;
        }

        _declaredInstanceCount = numeric_cast<size_t>(count);
        _chain.reserve(_declaredInstanceCount);
    }

    void Rendering(const NodeParameter& parameter, const InstanceParameter& instance, void* user_data) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(parameter, user_data);

        if (!_binding->CurrentSystem || !_node || _binding->CurrentSystem->Failed || _declaredInstanceCount == 0) {
            return;
        }
        if (_chain.size() >= _declaredInstanceCount) {
            _binding->Fail("ribbon callback emitted more instances than its group declared");
            return;
        }
        // The chain is stitched in index order, so an instance arriving out of order would silently describe a different
        // band instead of the emitter's.
        if (instance.InstanceIndex < 0 || numeric_cast<size_t>(instance.InstanceIndex) != _chain.size() || instance.InstanceCount != numeric_cast<int32_t>(_declaredInstanceCount)) {
            _binding->Fail("ribbon callback emitted an instance out of strip order");
            return;
        }
        if (!std::isfinite(instance.AlphaThreshold) || instance.AlphaThreshold != 0.0f) {
            _binding->Fail("alpha cutoff instance data is unsupported");
            return;
        }
        // Positions[2] and [3] are read only by the spline path, which is rejected, and the emitter leaves them
        // uninitialised - so validating them would reject perfectly drawable content.
        if (!IsFinite(instance.SRTMatrix43) || !std::isfinite(instance.Positions[0]) || !std::isfinite(instance.Positions[1])) {
            _binding->Fail("ribbon callback emitted non-finite geometry data");
            return;
        }
        if (!std::isfinite(instance.UV.X) || !std::isfinite(instance.UV.Y) || !std::isfinite(instance.UV.Width) || !std::isfinite(instance.UV.Height)) {
            _binding->Fail("ribbon callback emitted non-finite texture coordinates");
            return;
        }

        _chain.emplace_back(MakeWidthTriple(instance));
    }

    void EndRenderingGroup(const NodeParameter& parameter, int32_t count, void* user_data) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(parameter, count, user_data);

        // A band needs two instances to span a segment; a shorter group draws nothing, exactly as upstream leaves it.
        if (_binding->CurrentSystem && _node && !_binding->CurrentSystem->Failed && _chain.size() >= 2) {
            _geometry.Draw(_binding->CurrentSystem.as_ptr(), *_node, _chain, _declaredInstanceCount);
        }

        ResetGroup();
    }

    void EndRendering(const NodeParameter& parameter, void* user_data) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(parameter, user_data);
        ResetGroup();
        _node.reset();
    }

private:
    [[nodiscard]] auto MakeWidthTriple(const InstanceParameter& instance) const -> EffekseerStripWidthTriple
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(_binding->CurrentSystem, "Effekseer ribbon triple built without a bound system");

        float32_t left_offset = instance.Positions[0];
        float32_t right_offset = instance.Positions[1];
        float32_t center_offset = (left_offset + right_offset) * 0.5f;

        EffekseerStripWidthTriple triple {
            .LeftColor = instance.Colors[0],
            .CenterColor = Effekseer::Color::Lerp(instance.Colors[0], instance.Colors[1], 0.5f),
            .RightColor = instance.Colors[1],
            .UV = instance.UV,
        };

        if (_viewpointDependent) {
            Effekseer::SIMD::Vec3f scale;
            Effekseer::SIMD::Mat43f rotation;
            Effekseer::SIMD::Vec3f translation;
            instance.SRTMatrix43.GetSRT(scale, rotation, translation);

            vec3 up {rotation.X.GetY(), rotation.Y.GetY(), rotation.Z.GetY()};
            vec3 view_direction = -ExtractCameraBackward(_binding->CurrentSystem->ViewMatrix);
            vec3 width_axis = CalculateStripWidthAxis(up, view_direction);
            vec3 center = ToVec3(translation);

            triple.LeftPosition = center - width_axis * (left_offset * scale.GetX());
            triple.CenterPosition = center - width_axis * (center_offset * scale.GetX());
            triple.RightPosition = center - width_axis * (right_offset * scale.GetX());
        }
        else {
            triple.LeftPosition = ToVec3(Effekseer::SIMD::Vec3f::Transform(Effekseer::SIMD::Vec3f {left_offset, 0.0f, 0.0f}, instance.SRTMatrix43));
            triple.CenterPosition = ToVec3(Effekseer::SIMD::Vec3f::Transform(Effekseer::SIMD::Vec3f {center_offset, 0.0f, 0.0f}, instance.SRTMatrix43));
            triple.RightPosition = ToVec3(Effekseer::SIMD::Vec3f::Transform(Effekseer::SIMD::Vec3f {right_offset, 0.0f, 0.0f}, instance.SRTMatrix43));
        }

        return triple;
    }

    void ResetGroup()
    {
        FO_STACK_TRACE_ENTRY();

        _chain.clear();
        _declaredInstanceCount = 0;
    }

    shared_ptr<EffekseerDrawBinding> _binding;
    EffekseerStripGeometry _geometry;
    optional<EffekseerStripNodeSnapshot> _node {};
    bool _viewpointDependent {};
    size_t _declaredInstanceCount {};
    vector<EffekseerStripWidthTriple> _chain {};
};

// One instance of a track before the strip is known: its width and colours fade toward the middle of the whole trail, and
// the direction the band spreads along comes from where the neighbouring instances are, so a triple can only be built
// once the group has arrived in full.
struct EffekseerTrackInstanceSnapshot
{
    Effekseer::SIMD::Mat43f SRTMatrix43 {};
    Effekseer::Color ColorLeft {};
    Effekseer::Color ColorCenter {};
    Effekseer::Color ColorRight {};
    Effekseer::Color ColorLeftMiddle {};
    Effekseer::Color ColorCenterMiddle {};
    Effekseer::Color ColorRightMiddle {};
    float32_t SizeFor {};
    float32_t SizeMiddle {};
    float32_t SizeBack {};
    Effekseer::RectF UV {};
};

// A track is a trail behind a moving emitter: every instance is one cross-section of it, centred on the instance and
// spread across the direction of travel so the band faces the camera. Width and colour interpolate from the head and
// tail values toward the middle ones across the length of the trail.
class FOnlineEffekseerTrackRenderer final : public Effekseer::TrackRenderer
{
public:
    FOnlineEffekseerTrackRenderer(ptr<EffectManager> effect_mngr, ptr<IAppRender> render, ptr<RenderSettings> settings, shared_ptr<EffekseerDrawBinding> binding) :
        _binding {std::move(binding)},
        _geometry {effect_mngr, render, settings}
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(_binding, "Effekseer track renderer requires draw binding");
    }

    void BeginRendering(const NodeParameter& parameter, int32_t count, void* user_data) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(count, user_data);
        ResetGroup();
        _node.reset();

        if (!_binding->CurrentSystem) {
            return;
        }
        if (string_view reason = ValidateStripNodeParameter(parameter.BasicParameterPtr, parameter.DepthParameterPtr, parameter.TextureUVTypeParameterPtr, parameter.SplineDivision, parameter.EnableViewOffset, parameter.IsRightHand); !reason.empty()) {
            _binding->Fail(reason);
            return;
        }
        if (parameter.MaterialType != Effekseer::RendererMaterialType::Default || parameter.MaterialRenderDataPtr != nullptr) {
            _binding->Fail("only the Default material is supported");
            return;
        }
        if (parameter.SmoothingType != Effekseer::TrailSmoothingType::Off) {
            _binding->Fail("smoothed track nodes are unsupported");
            return;
        }
        if (parameter.EffectPointer != _binding->CurrentSystem->Effect.Get()) {
            _binding->Fail("track renderer received an unexpected effect pointer");
            return;
        }

        _node = EffekseerStripNodeSnapshot {
            .AlphaBlend = parameter.BasicParameterPtr->AlphaBlend,
            .TextureFilter = parameter.BasicParameterPtr->TextureFilters[0],
            .TextureWrap = parameter.BasicParameterPtr->TextureWraps[0],
            .TextureIndex = parameter.BasicParameterPtr->TextureIndexes[0],
            .ZTest = parameter.ZTest,
            .ZWrite = parameter.ZWrite,
        };
    }

    void BeginRenderingGroup(const NodeParameter& parameter, int32_t count, void* user_data) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(parameter, user_data);
        ResetGroup();

        if (!_binding->CurrentSystem || !_node || _binding->CurrentSystem->Failed) {
            return;
        }
        if (count < 0 || count > EFFEKSEER_INSTANCE_MAX) {
            _binding->Fail("track group exceeds the supported instance count");
            return;
        }

        _declaredInstanceCount = numeric_cast<size_t>(count);
        _instances.reserve(_declaredInstanceCount);
    }

    void Rendering(const NodeParameter& parameter, const InstanceParameter& instance, void* user_data) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(parameter, user_data);

        if (!_binding->CurrentSystem || !_node || _binding->CurrentSystem->Failed || _declaredInstanceCount == 0) {
            return;
        }
        if (_instances.size() >= _declaredInstanceCount) {
            _binding->Fail("track callback emitted more instances than its group declared");
            return;
        }
        if (instance.InstanceIndex < 0 || numeric_cast<size_t>(instance.InstanceIndex) != _instances.size() || instance.InstanceCount != numeric_cast<int32_t>(_declaredInstanceCount)) {
            _binding->Fail("track callback emitted an instance out of strip order");
            return;
        }
        if (!std::isfinite(instance.AlphaThreshold) || instance.AlphaThreshold != 0.0f) {
            _binding->Fail("alpha cutoff instance data is unsupported");
            return;
        }
        if (!IsFinite(instance.SRTMatrix43) || !std::isfinite(instance.SizeFor) || !std::isfinite(instance.SizeMiddle) || !std::isfinite(instance.SizeBack)) {
            _binding->Fail("track callback emitted non-finite geometry data");
            return;
        }
        if (!std::isfinite(instance.UV.X) || !std::isfinite(instance.UV.Y) || !std::isfinite(instance.UV.Width) || !std::isfinite(instance.UV.Height)) {
            _binding->Fail("track callback emitted non-finite texture coordinates");
            return;
        }

        _instances.emplace_back(EffekseerTrackInstanceSnapshot {
            .SRTMatrix43 = instance.SRTMatrix43,
            .ColorLeft = instance.ColorLeft,
            .ColorCenter = instance.ColorCenter,
            .ColorRight = instance.ColorRight,
            .ColorLeftMiddle = instance.ColorLeftMiddle,
            .ColorCenterMiddle = instance.ColorCenterMiddle,
            .ColorRightMiddle = instance.ColorRightMiddle,
            .SizeFor = instance.SizeFor,
            .SizeMiddle = instance.SizeMiddle,
            .SizeBack = instance.SizeBack,
            .UV = instance.UV,
        });
    }

    void EndRenderingGroup(const NodeParameter& parameter, int32_t count, void* user_data) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(parameter, count, user_data);

        if (_binding->CurrentSystem && _node && !_binding->CurrentSystem->Failed && _instances.size() >= 2) {
            _geometry.Draw(_binding->CurrentSystem.as_ptr(), *_node, MakeWidthChain(), _declaredInstanceCount);
        }

        ResetGroup();
    }

    void EndRendering(const NodeParameter& parameter, void* user_data) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(parameter, user_data);
        ResetGroup();
        _node.reset();
    }

private:
    [[nodiscard]] auto MakeWidthChain() const -> vector<EffekseerStripWidthTriple>
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(_binding->CurrentSystem, "Effekseer track chain built without a bound system");
        FO_VERIFY_AND_THROW(_instances.size() >= 2, "Effekseer track chain requires at least one segment", _instances.size());

        vec3 view_direction = ExtractCameraBackward(_binding->CurrentSystem->ViewMatrix);
        float32_t fade_divisor = numeric_cast<float32_t>(_declaredInstanceCount - 1);
        vector<EffekseerStripWidthTriple> chain;
        chain.reserve(_instances.size());
        vec3 previous_axis {};

        for (size_t index = 0; index < _instances.size(); index++) {
            const EffekseerTrackInstanceSnapshot& instance = _instances[index];
            Effekseer::SIMD::Vec3f scale;
            Effekseer::SIMD::Mat43f rotation;
            Effekseer::SIMD::Vec3f translation;
            instance.SRTMatrix43.GetSRT(scale, rotation, translation);
            ignore_unused(rotation);

            // The trail runs from one instance to the next, and an interior cross-section splits the difference between
            // the segment it ends and the one it begins so the band does not kink at the joint.
            vec3 forward_axis = index + 1 < _instances.size() ? NormalizeTrailAxis(_instances[index + 1], instance) : previous_axis;
            vec3 band_axis = index != 0 ? (forward_axis + previous_axis) * 0.5f : forward_axis;
            previous_axis = forward_axis;

            // The head half of the trail fades from its front size and colour toward the middle ones, the tail half from
            // its back values, so both ends meet in the middle of the strip.
            float32_t fade_position = numeric_cast<float32_t>(index) / fade_divisor;
            bool head_half = index < _declaredInstanceCount / 2;
            float32_t fade = head_half ? fade_position * 2.0f : 1.0f - (fade_position * 2.0f - 1.0f);
            float32_t edge_size = head_half ? instance.SizeFor : instance.SizeBack;
            float32_t half_width = (edge_size + (instance.SizeMiddle - edge_size) * fade) * 0.5f * scale.GetX();

            vec3 width_axis = CalculateStripWidthAxis(band_axis, view_direction);
            vec3 center = ToVec3(translation);

            chain.emplace_back(EffekseerStripWidthTriple {
                .LeftPosition = center + width_axis * half_width,
                .CenterPosition = center,
                .RightPosition = center - width_axis * half_width,
                .LeftColor = LerpTrackColor(instance.ColorLeft, instance.ColorLeftMiddle, fade),
                .CenterColor = LerpTrackColor(instance.ColorCenter, instance.ColorCenterMiddle, fade),
                .RightColor = LerpTrackColor(instance.ColorRight, instance.ColorRightMiddle, fade),
                .UV = instance.UV,
            });
        }

        return chain;
    }

    [[nodiscard]] static auto NormalizeTrailAxis(const EffekseerTrackInstanceSnapshot& to, const EffekseerTrackInstanceSnapshot& from) -> vec3
    {
        FO_STACK_TRACE_ENTRY();

        vec3 axis = ToVec3(to.SRTMatrix43.GetTranslation()) - ToVec3(from.SRTMatrix43.GetTranslation());

        return glm::dot(axis, axis) > 0.0f ? glm::normalize(axis) : vec3 {};
    }

    void ResetGroup()
    {
        FO_STACK_TRACE_ENTRY();

        _instances.clear();
        _declaredInstanceCount = 0;
    }

    shared_ptr<EffekseerDrawBinding> _binding;
    EffekseerStripGeometry _geometry;
    optional<EffekseerStripNodeSnapshot> _node {};
    size_t _declaredInstanceCount {};
    vector<EffekseerTrackInstanceSnapshot> _instances {};
};

// What a model node needs beyond the material every family shares: which mesh it draws, how its instances are oriented,
// and which faces the rasterizer discards.
struct EffekseerModelNodeSnapshot
{
    Effekseer::AlphaBlendType AlphaBlend {};
    Effekseer::TextureFilterType TextureFilter {};
    Effekseer::TextureWrapType TextureWrap {};
    int32_t TextureIndex {-1};
    int32_t ModelIndex {-1};
    Effekseer::BillboardType Billboard {};
    CullModeType CullMode {};
    bool ZTest {};
    bool ZWrite {};
};

struct EffekseerModelInstanceSnapshot
{
    Effekseer::SIMD::Mat43f SRTMatrix43 {};
    Effekseer::RectF UV {};
    Effekseer::Color AllColor {};
    Effekseer::SIMD::Vec3f Direction {};
    int32_t Frame {};
};

// A model node draws a mesh per instance instead of a generated quad: the .efkmodel supplies positions, texture
// coordinates and vertex colours, and each instance contributes its own transformed copy. The instance transform is
// folded into the vertices here, the same way every other family bakes its geometry into world space, so the mesh needs
// no per-draw matrix of its own and batches with the rest of the particle draws.
class FOnlineEffekseerModelRenderer final : public Effekseer::ModelRenderer
{
public:
    FOnlineEffekseerModelRenderer(ptr<EffectManager> effect_mngr, ptr<IAppRender> render, ptr<RenderSettings> settings, shared_ptr<EffekseerDrawBinding> binding) :
        _binding {std::move(binding)},
        _particleEffects {effect_mngr},
        _drawBuffer {render->CreateDrawBuffer(false)},
        _effectMngr {effect_mngr},
        _render {render},
        _settings {settings},
        _whiteTexture {render->CreateTexture({1, 1}, true, false)}
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(_binding, "Effekseer model renderer requires draw binding");

        constexpr ucolor white_pixel {255, 255, 255, 255};
        _whiteTexture->UpdateTextureRegion({}, {1, 1}, {&white_pixel, 1});
        _drawBuffer->PrimType = RenderPrimitiveType::TriangleList;
    }

    void BeginRendering(const NodeParameter& parameter, int32_t count, void* user_data) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(user_data);
        ResetState();

        if (!_binding->CurrentSystem) {
            return;
        }
        if (count < 0 || count > EFFEKSEER_INSTANCE_MAX) {
            _binding->Fail("model node exceeds the supported instance count");
            return;
        }
        if (string_view reason = ValidateModelNodeParameter(parameter); !reason.empty()) {
            _binding->Fail(reason);
            return;
        }
        if (parameter.EffectPointer != _binding->CurrentSystem->Effect.Get()) {
            _binding->Fail("model renderer received an unexpected effect pointer");
            return;
        }

        optional<CullModeType> cull_mode = ConvertEffekseerCulling(parameter.Culling);

        if (!cull_mode) {
            _binding->Fail("unknown model culling mode");
            return;
        }

        _declaredInstanceCount = numeric_cast<size_t>(count);
        _node = EffekseerModelNodeSnapshot {
            .AlphaBlend = parameter.BasicParameterPtr->AlphaBlend,
            .TextureFilter = parameter.BasicParameterPtr->TextureFilters[0],
            .TextureWrap = parameter.BasicParameterPtr->TextureWraps[0],
            .TextureIndex = parameter.BasicParameterPtr->TextureIndexes[0],
            .ModelIndex = parameter.ModelIndex,
            .Billboard = parameter.Billboard,
            .CullMode = *cull_mode,
            .ZTest = parameter.ZTest,
            .ZWrite = parameter.ZWrite,
        };
        _instances.reserve(_declaredInstanceCount);
    }

    void Rendering(const NodeParameter& parameter, const InstanceParameter& instance, void* user_data) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(parameter, user_data);

        if (!_binding->CurrentSystem || !_node || _binding->CurrentSystem->Failed) {
            return;
        }
        if (_instances.size() >= _declaredInstanceCount) {
            _binding->Fail("model callback emitted more instances than declared");
            return;
        }
        if (!std::isfinite(instance.AlphaThreshold) || instance.AlphaThreshold != 0.0f) {
            _binding->Fail("alpha cutoff instance data is unsupported");
            return;
        }
        if (!IsFinite(instance.SRTMatrix43)) {
            _binding->Fail("model callback emitted non-finite geometry data");
            return;
        }
        if (!std::isfinite(instance.UV.X) || !std::isfinite(instance.UV.Y) || !std::isfinite(instance.UV.Width) || !std::isfinite(instance.UV.Height)) {
            _binding->Fail("model callback emitted non-finite texture coordinates");
            return;
        }
        if (instance.Time < 0) {
            _binding->Fail("model callback emitted a negative animation frame");
            return;
        }

        Effekseer::SIMD::Vec3f direction {0.0f, 1.0f, 0.0f};

        if (_node->Billboard == Effekseer::BillboardType::DirectionalBillboard) {
            if (!IsFinite(instance.Direction)) {
                _binding->Fail("directional model callback emitted a non-finite direction");
                return;
            }

            direction = instance.Direction;
        }

        _instances.emplace_back(EffekseerModelInstanceSnapshot {
            .SRTMatrix43 = instance.SRTMatrix43,
            .UV = instance.UV,
            .AllColor = instance.AllColor,
            .Direction = direction,
            .Frame = instance.Time,
        });
    }

    void EndRendering(const NodeParameter& parameter, void* user_data) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(parameter, user_data);

        if (!_binding->CurrentSystem || !_node || _binding->CurrentSystem->Failed) {
            ResetState();
            return;
        }
        if (!_instances.empty()) {
            Render(_binding->CurrentSystem.as_ptr());
        }

        ResetState();
    }

private:
    void ResetState()
    {
        FO_STACK_TRACE_ENTRY();

        _instances.clear();
        _node.reset();
        _declaredInstanceCount = 0;
    }

    void Render(ptr<EffekseerParticleRuntimeSystem::Impl> system)
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(_node, "Effekseer model render called without a node snapshot");

        if (_node->ModelIndex < 0 || _node->ModelIndex >= system->Effect->GetModelCount()) {
            system->Fail("model index is out of range");
            return;
        }

        Effekseer::ModelRef model = system->Effect->GetModel(_node->ModelIndex);

        if (!model || model->GetFrameCount() <= 0) {
            system->Fail("model resource is not loaded");
            return;
        }

        optional<EffekseerNodeTexture> texture = ResolveEffekseerNodeTexture(system, _node->TextureIndex, _node->TextureFilter, _whiteTexture.get());

        if (!texture) {
            return;
        }

        // Every frame of an animated mesh is a separate vertex set, so the budget is taken from the largest of them.
        size_t max_vertices_per_instance = 0;

        for (int32_t frame = 0; frame < model->GetFrameCount(); frame++) {
            max_vertices_per_instance = std::max(max_vertices_per_instance, numeric_cast<size_t>(model->GetFaceCount(frame)) * 3);
        }

        if (max_vertices_per_instance == 0) {
            system->Fail("model resource has no faces");
            return;
        }
        if (max_vertices_per_instance > EFFEKSEER_CHUNK_VERTEX_MAX) {
            system->Fail("model mesh exceeds the supported geometry budget");
            return;
        }

        size_t instances_per_draw = EFFEKSEER_CHUNK_VERTEX_MAX / max_vertices_per_instance;

        for (size_t first_instance = 0; first_instance < _instances.size() && !system->Failed; first_instance += instances_per_draw) {
            RenderChunk(system, model, *texture, first_instance, std::min(instances_per_draw, _instances.size() - first_instance));
        }
    }

    void RenderChunk(ptr<EffekseerParticleRuntimeSystem::Impl> system, const Effekseer::ModelRef& model, const EffekseerNodeTexture& texture, size_t first_instance, size_t instance_count)
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(_node, "Effekseer model chunk render called without a node snapshot");

        optional<EffekseerNodeRenderState> render_state = _particleEffects.Resolve(_node->AlphaBlend, _node->TextureWrap, _node->ZTest, _node->ZWrite);

        if (!render_state) {
            system->Fail("node sampling or blend mode has no renderer equivalent");
            return;
        }

        size_t vertex_count = 0;

        for (size_t chunk_instance_index = 0; chunk_instance_index < instance_count; chunk_instance_index++) {
            vertex_count += numeric_cast<size_t>(model->GetFaceCount(ResolveFrame(model, _instances[first_instance + chunk_instance_index].Frame))) * 3;
        }

        _drawBuffer->VertCount = 0;
        _drawBuffer->IndCount = 0;
        _drawBuffer->CheckAllocBuf(vertex_count, vertex_count);

        vec3 camera_backward = ExtractCameraBackward(system->ViewMatrix);
        size_t emitted = 0;

        for (size_t chunk_instance_index = 0; chunk_instance_index < instance_count; chunk_instance_index++) {
            const EffekseerModelInstanceSnapshot& instance = _instances[first_instance + chunk_instance_index];
            int32_t frame = ResolveFrame(model, instance.Frame);
            size_t vertex_total = numeric_cast<size_t>(model->GetVertexCount(frame));
            size_t face_total = numeric_cast<size_t>(model->GetFaceCount(frame));

            // An empty frame of an animated mesh draws nothing, and its vertex array is not required to exist.
            if (vertex_total == 0 || face_total == 0) {
                continue;
            }

            const_span<Effekseer::Model::Vertex> vertices {model->GetVertexes(frame), vertex_total};
            const_span<Effekseer::Model::Face> faces {model->GetFaces(frame), face_total};

            for (size_t face_index = 0; face_index < face_total; face_index++) {
                for (size_t corner = 0; corner < 3; corner++) {
                    int32_t vertex_index = faces[face_index].Indexes[corner];

                    if (vertex_index < 0 || numeric_cast<size_t>(vertex_index) >= vertex_total) {
                        system->Fail("model face references a vertex outside the mesh");
                        return;
                    }

                    const Effekseer::Model::Vertex& model_vertex = vertices[numeric_cast<size_t>(vertex_index)];
                    vec3 local_position {model_vertex.Position.X, model_vertex.Position.Y, model_vertex.Position.Z};
                    vec3 position = CalculateParticlePosition(_node->Billboard, instance.SRTMatrix43, instance.Direction, local_position, camera_backward);

                    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
                        system->Fail("model geometry produced a non-finite vertex");
                        return;
                    }

                    // The mesh's own coordinate lands inside the instance's texture rectangle, which the shader then
                    // addresses inside the atlas - so a mesh that tiles its texture keeps tiling.
                    float32_t texture_u = instance.UV.X + model_vertex.UV1.X * instance.UV.Width;
                    float32_t texture_v = instance.UV.Y + model_vertex.UV1.Y * instance.UV.Height;

                    if (!std::isfinite(texture_u) || !std::isfinite(texture_v)) {
                        system->Fail("model geometry produced a non-finite texture coordinate");
                        return;
                    }

                    Vertex2D& vertex = _drawBuffer->Vertices[emitted];
                    vertex.PosX = position.x;
                    vertex.PosY = position.y;
                    vertex.PosZ = position.z;
                    vertex.Color = ToColor(Effekseer::Color::Mul(model_vertex.VColor, instance.AllColor));
                    vertex.TexU = texture_u;
                    vertex.TexV = texture_v;
                    vertex.EggFlags[0] = 0.0f;
                    vertex.EggFlags[1] = 0.0f;
                    _drawBuffer->Indices[emitted] = numeric_cast<vindex_t>(emitted);
                    emitted++;
                }
            }
        }

        FO_VERIFY_AND_THROW(emitted == vertex_count, "Effekseer model chunk emitted an unexpected vertex count", emitted, vertex_count);

        _drawBuffer->VertCount = vertex_count;
        _drawBuffer->IndCount = vertex_count;
        _drawBuffer->Upload(EffectUsage::QuadSprite, vertex_count, vertex_count);

        ptr<RenderEffect> effect = render_state->Effect.as_ptr();
        effect->DisableBlending = render_state->DisableBlending;
        effect->DepthVariant = render_state->DepthVariant;
        effect->CullMode = _node->CullMode;
        effect->ProjBuf = RenderEffect::ProjBuffer();
        MemCopy(effect->ProjBuf->ProjMatrix, glm::value_ptr(system->ViewProjMatrix), sizeof(effect->ProjBuf->ProjMatrix));
        effect->MainTex = texture.Texture;
        effect->ParticleSamplingBuf = RenderEffect::ParticleSamplingBuffer();
        effect->ParticleSamplingBuf->ParticleSampling[0] = texture.PointSampled ? 1.0f : 0.0f;
        effect->ParticleSamplingBuf->ParticleSampling[1] = render_state->ClampInShader ? 1.0f : 0.0f;

        // The fragment addresses the raw emitter coordinate inside this rectangle.
        effect->SpriteBorderBuf = RenderEffect::SpriteBorderBuffer();
        effect->SpriteBorderBuf->SpriteBorder[0] = texture.AtlasRect.x;
        effect->SpriteBorderBuf->SpriteBorder[1] = texture.AtlasRect.y;
        effect->SpriteBorderBuf->SpriteBorder[2] = texture.AtlasRect.x + texture.AtlasRect.width;
        effect->SpriteBorderBuf->SpriteBorder[3] = texture.AtlasRect.y + texture.AtlasRect.height;

        effect->DrawBuffer(_drawBuffer, 0, vertex_count);

        if (_settings->DrawWireframe) {
            DrawParticleBufferWireframe(_effectMngr, _render, _wireframeBuf, *_drawBuffer, vertex_count, system->ViewProjMatrix);
        }
    }

    // An animated mesh cycles through its frames, exactly as the reference renderer indexes them.
    [[nodiscard]] static auto ResolveFrame(const Effekseer::ModelRef& model, int32_t frame) -> int32_t
    {
        FO_STACK_TRACE_ENTRY();

        return frame % model->GetFrameCount();
    }

    shared_ptr<EffekseerDrawBinding> _binding;
    EffekseerParticleEffects _particleEffects;
    unique_ptr<RenderDrawBuffer> _drawBuffer;
    unique_nptr<RenderDrawBuffer> _wireframeBuf {};
    ptr<EffectManager> _effectMngr;
    ptr<IAppRender> _render;
    ptr<RenderSettings> _settings;
    unique_ptr<RenderTexture> _whiteTexture;
    optional<EffekseerModelNodeSnapshot> _node {};
    size_t _declaredInstanceCount {};
    vector<EffekseerModelInstanceSnapshot> _instances {};
};

// Only the sprite family refracts: the distortion shader takes the particle's own plane per vertex, which the sprite
// geometry supplies. Rejecting the other families here rather than at their first draw keeps an effect that cannot be
// drawn from being accepted and then vanishing mid-play.
static auto ValidateStaticNodeMaterial(string_view path, const Effekseer::EffectBasicRenderParameter& parameter, ptr<Effekseer::Effect> effect, bool sprite_family) -> bool
{
    FO_STACK_TRACE_ENTRY();

    bool distortion = parameter.MaterialType == Effekseer::RendererMaterialType::BackDistortion;

    if ((parameter.MaterialType != Effekseer::RendererMaterialType::Default && !distortion) || parameter.MaterialIndex != -1) {
        LogEffekseerRejection(path, "only the Default and distortion materials are supported");
        return false;
    }
    if (distortion && !sprite_family) {
        LogEffekseerRejection(path, "only Sprite nodes can refract the scene");
        return false;
    }
    if (parameter.AlphaBlend == Effekseer::AlphaBlendType::Mul) {
        LogEffekseerRejection(path, "multiply blending is unsupported");
        return false;
    }

    // Modern Effekseer exports retain a non-zero distortion intensity even while the Default material leaves
    // distortion disabled. The dormant value has no renderer effect; only the distortion material reads it.
    if ((parameter.Distortion && !distortion) || parameter.EnableFalloff || parameter.TextureBlendType != -1 || parameter.FlipbookParams.EnableInterpolation || parameter.EmissiveScaling != 1.0f || parameter.EdgeParam.Threshold != 0.0f || parameter.SoftParticleDistanceFar != 0.0f || parameter.SoftParticleDistanceNear != 0.0f || parameter.SoftParticleDistanceNearOffset != 0.0f) {
        LogEffekseerRejection(path, "advanced material, soft-particle, or flipbook features are unsupported");
        return false;
    }

    // A distortion node's texture index addresses the distortion image table, not the colour one.
    int32_t image_count = distortion ? effect->GetDistortionImageCount() : effect->GetColorImageCount();

    if (parameter.TextureIndexes[0] < -1 || parameter.TextureIndexes[0] >= image_count) {
        LogEffekseerRejection(path, "node texture index is out of range");
        return false;
    }
    for (size_t texture_slot = 1; texture_slot < parameter.TextureIndexes.size(); texture_slot++) {
        if (parameter.TextureIndexes[texture_slot] >= 0) {
            LogEffekseerRejection(path, "advanced texture slots are unsupported");
            return false;
        }
    }

    // Sampler and payload checks only mean something for a node that actually samples; an untextured one draws its
    // authored vertex colours through a private white pixel.
    if (parameter.TextureIndexes[0] < 0) {
        return true;
    }

    if (parameter.TextureFilters[0] != Effekseer::TextureFilterType::Nearest && parameter.TextureFilters[0] != Effekseer::TextureFilterType::Linear) {
        LogEffekseerRejection(path, "node texture uses an unknown filter");
        return false;
    }
    if (parameter.TextureWraps[0] != Effekseer::TextureWrapType::Clamp && parameter.TextureWraps[0] != Effekseer::TextureWrapType::Repeat) {
        LogEffekseerRejection(path, "mirrored texture wrapping is unsupported");
        return false;
    }

    Effekseer::TextureRef texture = distortion ? effect->GetDistortionImage(parameter.TextureIndexes[0]) : effect->GetColorImage(parameter.TextureIndexes[0]);

    if (!texture || !texture->GetBackend()) {
        LogEffekseerRejection(path, "node texture failed to load");
        return false;
    }

    return true;
}

static auto ValidateEffectNode(string_view path, nptr<Effekseer::EffectNode> node, ptr<Effekseer::Effect> effect) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!node) {
        LogEffekseerRejection(path, "effect contains a null node");
        return false;
    }

    Effekseer::EffectNodeType node_type = node->GetType();

    if (node_type != Effekseer::EffectNodeType::Root && node_type != Effekseer::EffectNodeType::NoneType && node_type != Effekseer::EffectNodeType::Sprite && node_type != Effekseer::EffectNodeType::Ring && node_type != Effekseer::EffectNodeType::Ribbon && node_type != Effekseer::EffectNodeType::Track && node_type != Effekseer::EffectNodeType::Model) {
        LogEffekseerRejection(path, "only Root, None, Sprite, Ring, Ribbon, Track, and Model nodes are supported");
        return false;
    }

    // A node that draws carries the same material description whatever its shape is, so the material walk is shared and
    // the per-family geometry contract is checked by that family's renderer when it receives its node parameters.
    bool draws = node_type == Effekseer::EffectNodeType::Sprite || node_type == Effekseer::EffectNodeType::Ring || node_type == Effekseer::EffectNodeType::Ribbon || node_type == Effekseer::EffectNodeType::Track || node_type == Effekseer::EffectNodeType::Model;

    if (draws && !ValidateStaticNodeMaterial(path, node->GetBasicRenderParameter(), effect, node_type == Effekseer::EffectNodeType::Sprite)) {
        return false;
    }

    int32_t children_count = node->GetChildrenCount();
    if (children_count < 0) {
        LogEffekseerRejection(path, "effect node reports an invalid child count");
        return false;
    }
    for (int32_t child_index = 0; child_index < children_count; child_index++) {
        if (!ValidateEffectNode(path, node->GetChild(child_index), effect)) {
            return false;
        }
    }
    return true;
}

static auto ValidateEffect(string_view path, ptr<Effekseer::Effect> effect, bool gpu_particles_requested) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (gpu_particles_requested) {
        LogEffekseerRejection(path, "GPU particles are unsupported");
        return false;
    }
    if (effect->GetNormalImageCount() != 0 || effect->GetWaveCount() != 0 || effect->GetMaterialCount() != 0 || effect->GetCurveCount() != 0 || effect->GetProceduralModelCount() != 0) {
        LogEffekseerRejection(path, "normal textures, sounds, custom materials, and external curves are unsupported");
        return false;
    }
    return ValidateEffectNode(path, effect->GetRoot(), effect);
}

struct EffekseerRuntimeState
{
    EffekseerRuntimeState(ptr<EffectManager> effect_mngr, ptr<IAppRender> render, ptr<RenderSettings> settings, ptr<FileSystem> resources, ParticleTextureLoader texture_loader, ParticleSceneBackgroundProvider scene_background_provider) :
        Binding {SafeAlloc::MakeShared<EffekseerDrawBinding>()},
        SceneBackgroundProvider {std::move(scene_background_provider)},
        Setting {Effekseer::Setting::Create()},
        Manager {Effekseer::Manager::Create(EFFEKSEER_INSTANCE_MAX)},
        TextureLoader {Effekseer::MakeRefPtr<FOnlineEffekseerTextureLoader>(std::move(texture_loader))},
        ModelLoader {Effekseer::MakeRefPtr<FOnlineEffekseerModelLoader>(resources)},
        GpuParticleFactory {Effekseer::MakeRefPtr<DetectingGpuParticleFactory>()},
        SpriteRenderer {Effekseer::MakeRefPtr<FOnlineEffekseerSpriteRenderer>(effect_mngr, render, settings, Binding, SceneBackgroundProvider)},
        RibbonRenderer {Effekseer::MakeRefPtr<FOnlineEffekseerRibbonRenderer>(effect_mngr, render, settings, Binding)},
        RingRenderer {Effekseer::MakeRefPtr<FOnlineEffekseerRingRenderer>(effect_mngr, render, settings, Binding)},
        TrackRenderer {Effekseer::MakeRefPtr<FOnlineEffekseerTrackRenderer>(effect_mngr, render, settings, Binding)},
        ModelRenderer {Effekseer::MakeRefPtr<FOnlineEffekseerModelRenderer>(effect_mngr, render, settings, Binding)}
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(Setting, "Failed to create Effekseer setting");
        FO_VERIFY_AND_THROW(Manager, "Failed to create Effekseer manager");
        Setting->SetCoordinateSystem(Effekseer::CoordinateSystem::RH);
        Setting->SetTextureLoader(TextureLoader);
        Setting->SetModelLoader(ModelLoader);
        Setting->SetGpuParticleFactory(GpuParticleFactory);
        Manager->SetSetting(Setting);
        Manager->SetSpriteRenderer(SpriteRenderer);
        Manager->SetRibbonRenderer(RibbonRenderer);
        Manager->SetRingRenderer(RingRenderer);
        Manager->SetTrackRenderer(TrackRenderer);
        Manager->SetModelRenderer(ModelRenderer);
    }

    shared_ptr<EffekseerDrawBinding> Binding;
    // Supplies the scene behind a refracting draw; absent where there is no scene to refract.
    ParticleSceneBackgroundProvider SceneBackgroundProvider;
    Effekseer::SettingRef Setting;
    Effekseer::ManagerRef Manager;
    Effekseer::TextureLoaderRef TextureLoader;
    Effekseer::ModelLoaderRef ModelLoader;
    Effekseer::RefPtr<DetectingGpuParticleFactory> GpuParticleFactory;
    Effekseer::SpriteRendererRef SpriteRenderer;
    Effekseer::RibbonRendererRef RibbonRenderer;
    Effekseer::RingRendererRef RingRenderer;
    Effekseer::TrackRendererRef TrackRenderer;
    Effekseer::ModelRendererRef ModelRenderer;
};

static void RetireEffekseerHandle(ptr<EffekseerParticleRuntimeSystem::Impl> system)
{
    FO_STACK_TRACE_ENTRY();

    if (system->Handle < 0) {
        return;
    }

    if (system->Runtime->Manager->Exists(system->Handle)) {
        system->Runtime->Manager->StopEffect(system->Handle);
    }

    // Effekseer intentionally retires draw sets through two deferred GC queues.
    // Three flips cover an active draw set as well as both queue stages.
    for (size_t flush_index = 0; flush_index < 3; flush_index++) {
        system->Runtime->Manager->BeginUpdate();
        system->Runtime->Manager->EndUpdate();
    }

    system->Handle = -1;
}

struct EffekseerParticleRuntimeBackend::Impl
{
    explicit Impl(const ParticleRuntimeServices& services) :
        Runtime {SafeAlloc::MakeShared<EffekseerRuntimeState>(services.EffectMngr, services.Render, services.Settings, services.Resources, services.TextureLoader, services.SceneBackgroundProvider)},
        Resources {services.Resources}
    {
        FO_STACK_TRACE_ENTRY();
    }

    shared_ptr<EffekseerRuntimeState> Runtime;
    ptr<FileSystem> Resources;
};

EffekseerParticleRuntimeSystem::EffekseerParticleRuntimeSystem(unique_ptr<Impl>&& impl) :
    _impl {std::move(impl)}
{
    FO_STACK_TRACE_ENTRY();
}

EffekseerParticleRuntimeSystem::~EffekseerParticleRuntimeSystem()
{
    FO_STACK_TRACE_ENTRY();

    RetireEffekseerHandle(_impl.as_ptr());
}

auto EffekseerParticleRuntimeSystem::IsActive() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return !_impl->Failed && _impl->Handle >= 0 && _impl->Runtime->Manager->Exists(_impl->Handle);
}

auto EffekseerParticleRuntimeSystem::GetDrawInScene() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return true;
}

auto EffekseerParticleRuntimeSystem::GetBakedBounds() const noexcept -> optional<ParticleBounds3D>
{
    FO_STACK_TRACE_ENTRY();

    return MakeParticleBounds(_impl->BakedPositionMin, _impl->BakedPositionMax, _impl->BakedBillboardRadius);
}

auto EffekseerParticleRuntimeSystem::GetLiveBounds() const noexcept -> optional<ParticleBounds3D>
{
    FO_STACK_TRACE_ENTRY();

    // Frame the effect from its mandatory bake-time extent (a static box measured while baking), and only while it is
    // actually playing - a cheap instance-count check, no per-frame bounds computation. A finished or not-yet-playing
    // effect reserves nothing.
    if (_impl->Failed || _impl->Handle < 0 || !_impl->Runtime->Manager->Exists(_impl->Handle) || _impl->Runtime->Manager->GetInstanceCount(_impl->Handle) == 0) {
        return std::nullopt;
    }

    optional<ParticleBounds3D> baked = MakeParticleBounds(_impl->BakedPositionMin, _impl->BakedPositionMax, _impl->BakedBillboardRadius);

    if (!baked) {
        return std::nullopt;
    }

    return TransformParticleBounds(*baked, _impl->BoundsMatrix);
}

void EffekseerParticleRuntimeSystem::Setup(const ParticleRuntimeSetup& setup)
{
    FO_STACK_TRACE_ENTRY();

    if (setup.LookDirectionAngle != 0.0f) {
        _impl->Fail("look-direction oriented particles are unsupported");
        return;
    }

    mat44 position_offset_matrix = glm::translate(mat44 {1.0f}, setup.PositionOffset);
    mat44 view_offset_matrix = glm::translate(mat44 {1.0f}, setup.ViewOffset);
    _impl->RootMatrix = view_offset_matrix * setup.World * position_offset_matrix;
    _impl->RootMatrix *= glm::scale(mat44 {1.0f}, vec3 {setup.Scale, setup.Scale, setup.Scale});

    mat44 camera_rotation_matrix = setup.TiltInProjection ? mat44 {1.0f} : glm::rotate(mat44 {1.0f}, setup.MapCameraAngle * DEG_TO_RAD_FLOAT, vec3 {1.0f, 0.0f, 0.0f});
    _impl->ViewMatrix = camera_rotation_matrix * glm::translate(mat44 {1.0f}, -setup.ViewOffset);
    _impl->ViewProjMatrix = setup.Projection * _impl->ViewMatrix;

    // Bake-time bounds are stored in effect-local space; fold the effect's world placement (RootMatrix) and the view
    // transform into one matrix so the static box lands where the live particles emit.
    _impl->BoundsMatrix = _impl->ViewMatrix * _impl->RootMatrix;

    if (_impl->Handle >= 0 && _impl->Runtime->Manager->Exists(_impl->Handle)) {
        _impl->Runtime->Manager->SetMatrix(_impl->Handle, ToEffekseerMatrix43(_impl->RootMatrix));
    }
}

auto EffekseerParticleRuntimeSystem::Prewarm() -> float32_t
{
    FO_STACK_TRACE_ENTRY();

    if (_impl->Handle >= 0 && !_impl->Runtime->Manager->Exists(_impl->Handle)) {
        RetireEffekseerHandle(_impl.as_ptr());
    }
    if (!IsActive()) {
        return 0.0f;
    }

    float32_t remaining_frames = EFFEKSEER_PREWARM_SECONDS * EFFEKSEER_FRAMES_PER_SECOND;
    _impl->Runtime->Manager->SetMatrix(_impl->Handle, ToEffekseerMatrix43(_impl->RootMatrix));
    _impl->Runtime->Manager->BeginUpdate();
    while (remaining_frames > 0.0f && _impl->Runtime->Manager->Exists(_impl->Handle)) {
        float32_t step = std::min(remaining_frames, 1.0f);
        _impl->Runtime->Manager->UpdateHandle(_impl->Handle, step);
        remaining_frames -= step;
    }
    _impl->Runtime->Manager->EndUpdate();
    if (!_impl->Runtime->Manager->Exists(_impl->Handle)) {
        RetireEffekseerHandle(_impl.as_ptr());
    }

    return EFFEKSEER_PREWARM_SECONDS;
}

void EffekseerParticleRuntimeSystem::Respawn(optional<int32_t> seed)
{
    FO_STACK_TRACE_ENTRY();

    RetireEffekseerHandle(_impl.as_ptr());
    if (_impl->Failed) {
        return;
    }

    _impl->Handle = _impl->Runtime->Manager->Play(_impl->Effect, 0.0f, 0.0f, 0.0f);
    if (_impl->Handle < 0) {
        _impl->Fail("Effekseer manager failed to play the effect");
        return;
    }

    int32_t resolved_seed = seed ? *seed : std::uniform_int_distribution<int32_t> {std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()}(_impl->RandomGenerator);
    _impl->Runtime->Manager->SetAutoDrawing(_impl->Handle, false);
    _impl->Runtime->Manager->SetRandomSeed(_impl->Handle, resolved_seed);
    _impl->Runtime->Manager->SetMatrix(_impl->Handle, ToEffekseerMatrix43(_impl->RootMatrix));
}

void EffekseerParticleRuntimeSystem::Update(float32_t delta_seconds)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(std::isfinite(delta_seconds) && delta_seconds >= 0.0f, "Invalid Effekseer update delta", delta_seconds);
    if (_impl->Failed || _impl->Handle < 0) {
        return;
    }
    if (!_impl->Runtime->Manager->Exists(_impl->Handle)) {
        RetireEffekseerHandle(_impl.as_ptr());
        return;
    }

    _impl->Runtime->Manager->SetMatrix(_impl->Handle, ToEffekseerMatrix43(_impl->RootMatrix));
    _impl->Runtime->Manager->BeginUpdate();
    _impl->Runtime->Manager->UpdateHandle(_impl->Handle, delta_seconds * EFFEKSEER_FRAMES_PER_SECOND);
    _impl->Runtime->Manager->EndUpdate();
    if (!_impl->Runtime->Manager->Exists(_impl->Handle)) {
        RetireEffekseerHandle(_impl.as_ptr());
        return;
    }
}

void EffekseerParticleRuntimeSystem::RefreshRenderTransform()
{
    FO_STACK_TRACE_ENTRY();

    if (IsActive()) {
        Update(0.0f);
    }
}

void EffekseerParticleRuntimeSystem::Draw()
{
    FO_STACK_TRACE_ENTRY();

    if (!IsActive()) {
        return;
    }

    Effekseer::Manager::DrawParameter draw_parameter;
    draw_parameter.ViewProjectionMatrix = ToEffekseerMatrix44(_impl->ViewProjMatrix);
    mat44 inverse_view = glm::inverse(_impl->ViewMatrix);
    vec3 camera_position = vec3 {inverse_view[3]};
    vec3 camera_backward = ExtractCameraBackward(_impl->ViewMatrix);
    draw_parameter.CameraPosition = {camera_position.x, camera_position.y, camera_position.z};
    draw_parameter.CameraFrontDirection = {camera_backward.x, camera_backward.y, camera_backward.z};

    {
        _impl->Runtime->Binding->Bind(_impl.as_ptr());
        auto unbind_renderer = scope_exit([this]() noexcept { _impl->Runtime->Binding->Unbind(); });
        _impl->Runtime->Manager->DrawHandle(_impl->Handle, draw_parameter);
    }

    if (_impl->Failed) {
        RetireEffekseerHandle(_impl.as_ptr());
    }
}

EffekseerParticleRuntimeBackend::EffekseerParticleRuntimeBackend(const ParticleRuntimeServices& services) :
    _impl {SafeAlloc::MakeUnique<Impl>(services)}
{
    FO_STACK_TRACE_ENTRY();
}

EffekseerParticleRuntimeBackend::~EffekseerParticleRuntimeBackend()
{
    FO_STACK_TRACE_ENTRY();
}

auto EffekseerParticleRuntimeBackend::GetExtensions() const -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    return {"efk"};
}

void EffekseerParticleRuntimeBackend::InvalidateResource(string_view path)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(path);
}

auto EffekseerParticleRuntimeBackend::Create(string_view path) -> unique_nptr<ParticleRuntimeSystem>
{
    FO_STACK_TRACE_ENTRY();

    if (strex(path).get_file_extension() != "efk") {
        LogEffekseerRejection(path, "unsupported file extension");
        return {};
    }

    File file = _impl->Resources->ReadFile(path);

    if (!file) {
        LogEffekseerRejection(path, "resource is missing");
        return {};
    }

    const_span<uint8_t> data = file.GetDataSpan();

    if (data.size() < 4) {
        LogEffekseerRejection(path, "binary is truncated");
        return {};
    }

    constexpr string_view expected_magic = "SKFE";

    for (size_t index = 0; index < expected_magic.size(); index++) {
        if (data[index] != numeric_cast<uint8_t>(expected_magic[index])) {
            LogEffekseerRejection(path, "binary magic does not match the file extension");
            return {};
        }
    }

    // The baker appends a mandatory bounds trailer after the Effekseer payload. Split it off (a missing or malformed
    // trailer is a broken invariant of our baked data and throws) so the effect is loaded from the untouched payload
    // and the precomputed box is available for sprite-frame sizing.
    EffekseerBoundsTrailer bounds_trailer = ReadEffekseerBoundsTrailer(data);

    if (bounds_trailer.PayloadSize > numeric_cast<size_t>(std::numeric_limits<int32_t>::max())) {
        LogEffekseerRejection(path, "binary is too large");
        return {};
    }

    string material_path = strex(path).extract_dir().format_path().str();
    vector<char16_t> material_path_utf16 = ToUtf16(material_path);
    _impl->Runtime->GpuParticleFactory->Reset();
    Effekseer::EffectRef effect = Effekseer::Effect::Create(_impl->Runtime->Manager, data.data(), numeric_cast<int32_t>(bounds_trailer.PayloadSize), 1.0f, material_path_utf16.data());

    if (!effect) {
        LogEffekseerRejection(path, "Effekseer core rejected the binary");
        return {};
    }
    if (!ValidateEffect(path, effect.Get(), _impl->Runtime->GpuParticleFactory->WasRequested())) {
        return {};
    }

    auto system = SafeAlloc::MakeUnique<EffekseerParticleRuntimeSystem>(SafeAlloc::MakeUnique<EffekseerParticleRuntimeSystem::Impl>(_impl->Runtime, std::move(effect), string {path}, bounds_trailer.PositionMin, bounds_trailer.PositionMax, bounds_trailer.BillboardRadius));
    system->Respawn(0);

    if (!system->IsActive()) {
        return {};
    }

    return system;
}

// Bounds trailer, all little-endian: [6 x float32 min/max][uint32 payload size][uint32 magic]. A fixed size lets the
// runtime probe the tail without scanning, and the payload-size cross-check makes a false positive on an untrailered
// binary effectively impossible.
static constexpr size_t EFFEKSEER_BOUNDS_TRAILER_FLOATS = 7; // position box min/max, then the billboard radius
static constexpr size_t EFFEKSEER_BOUNDS_TRAILER_SIZE = EFFEKSEER_BOUNDS_TRAILER_FLOATS * sizeof(float32_t) + 2 * sizeof(uint32_t);

static void WriteLittleEndianUint32(vector<uint8_t>& out, uint32_t value)
{
    FO_NO_STACK_TRACE_ENTRY();

    out.push_back(numeric_cast<uint8_t>(value & 0xFFu));
    out.push_back(numeric_cast<uint8_t>((value >> 8) & 0xFFu));
    out.push_back(numeric_cast<uint8_t>((value >> 16) & 0xFFu));
    out.push_back(numeric_cast<uint8_t>((value >> 24) & 0xFFu));
}

static auto ReadLittleEndianUint32(const_span<uint8_t> data, size_t offset) -> uint32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return uint32_t {data[offset]} | (uint32_t {data[offset + 1]} << 8) | (uint32_t {data[offset + 2]} << 16) | (uint32_t {data[offset + 3]} << 24);
}

void AppendEffekseerBoundsTrailer(vector<uint8_t>& binary, const vec3& min_bounds, const vec3& max_bounds, float32_t billboard_radius)
{
    FO_STACK_TRACE_ENTRY();

    uint32_t payload_size = numeric_cast<uint32_t>(binary.size());
    const float32_t values[EFFEKSEER_BOUNDS_TRAILER_FLOATS] = {min_bounds.x, min_bounds.y, min_bounds.z, max_bounds.x, max_bounds.y, max_bounds.z, billboard_radius};

    for (size_t i = 0; i < EFFEKSEER_BOUNDS_TRAILER_FLOATS; i++) {
        WriteLittleEndianUint32(binary, std::bit_cast<uint32_t>(values[i]));
    }

    WriteLittleEndianUint32(binary, payload_size);
    WriteLittleEndianUint32(binary, EFFEKSEER_BOUNDS_TRAILER_MAGIC);
}

auto ReadEffekseerBoundsTrailer(const_span<uint8_t> binary) -> EffekseerBoundsTrailer
{
    FO_STACK_TRACE_ENTRY();

    // Every baked .efk carries the trailer, so each of these is a violated invariant of our own baked data, not an
    // expected "maybe absent" case: fail loudly instead of skipping.
    FO_VERIFY_AND_THROW(binary.size() >= EFFEKSEER_BOUNDS_TRAILER_SIZE, "Baked Effekseer binary is too small to hold its mandatory bounds trailer", binary.size());
    FO_VERIFY_AND_THROW(ReadLittleEndianUint32(binary, binary.size() - sizeof(uint32_t)) == EFFEKSEER_BOUNDS_TRAILER_MAGIC, "Baked Effekseer binary is missing its mandatory bounds trailer magic", binary.size());

    size_t trailer_offset = binary.size() - EFFEKSEER_BOUNDS_TRAILER_SIZE;
    uint32_t payload_size = ReadLittleEndianUint32(binary, binary.size() - 2 * sizeof(uint32_t));
    FO_VERIFY_AND_THROW(numeric_cast<size_t>(payload_size) == trailer_offset, "Baked Effekseer bounds trailer has an inconsistent payload size", payload_size, trailer_offset);

    float32_t values[EFFEKSEER_BOUNDS_TRAILER_FLOATS];

    for (size_t i = 0; i < EFFEKSEER_BOUNDS_TRAILER_FLOATS; i++) {
        values[i] = std::bit_cast<float32_t>(ReadLittleEndianUint32(binary, trailer_offset + i * sizeof(uint32_t)));
    }

    EffekseerBoundsTrailer trailer;
    trailer.PayloadSize = numeric_cast<size_t>(payload_size);
    trailer.PositionMin = vec3 {values[0], values[1], values[2]};
    trailer.PositionMax = vec3 {values[3], values[4], values[5]};
    trailer.BillboardRadius = values[6];
    return trailer;
}

FO_END_NAMESPACE

#endif
