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
constexpr size_t EFFEKSEER_RING_VERTEX_MAX = 64000;
constexpr float32_t EFFEKSEER_FRAMES_PER_SECOND = 60.0f;
constexpr float32_t EFFEKSEER_PREWARM_SECONDS = 1.0f;

struct EffekseerRuntimeState;

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

        if (texture_type != Effekseer::TextureType::Color) {
            WriteLog(LogType::Warning, "Effekseer texture '{}' rejected: only color textures are supported", ToUtf8(path));
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
    Impl(shared_ptr<EffekseerRuntimeState> runtime, Effekseer::EffectRef effect, string path) :
        Runtime {std::move(runtime)},
        Effect {std::move(effect)},
        Path {std::move(path)}
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
    int32_t TextureIndex {-1};
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
    int32_t TextureIndex {-1};
    int32_t VertexCount {};
    float32_t StartingFade {};
    float32_t EndingFade {};
};

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

static auto ValidateSpriteNodeParameter(const Effekseer::SpriteRenderer::NodeParameter& parameter) -> string_view
{
    FO_STACK_TRACE_ENTRY();

    if (parameter.EffectPointer == nullptr || parameter.BasicParameterPtr == nullptr || parameter.DepthParameterPtr == nullptr) {
        return "sprite renderer received incomplete node parameters";
    }
    if (!parameter.ZTest || parameter.ZWrite) {
        return "sprite node must use ZTest=on and ZWrite=off";
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
    if (basic.MaterialType != Effekseer::RendererMaterialType::Default || basic.MaterialRenderDataPtr != nullptr) {
        return "only the Default material is supported";
    }
    if (basic.AlphaBlend != Effekseer::AlphaBlendType::Blend && basic.AlphaBlend != Effekseer::AlphaBlendType::Add) {
        return "only Blend and Add blending are supported";
    }
    if (basic.TextureIndexes[0] < 0) {
        return "sprite node has no color texture";
    }
    if (basic.TextureFilters[0] != Effekseer::TextureFilterType::Nearest && basic.TextureFilters[0] != Effekseer::TextureFilterType::Linear) {
        return "sprite node uses an unknown texture filter";
    }
    if (basic.TextureWraps[0] != Effekseer::TextureWrapType::Clamp) {
        return "only Clamp texture wrapping is supported";
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
    if (!parameter.ZTest || parameter.ZWrite) {
        return "ring node must use ZTest=on and ZWrite=off";
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
    if (parameter.VertexCount <= 0 || parameter.VertexCount > numeric_cast<int32_t>(EFFEKSEER_RING_VERTEX_MAX / 8)) {
        return "ring vertex count exceeds the supported geometry budget";
    }
    if (!std::isfinite(parameter.StartingFade) || !std::isfinite(parameter.EndingFade)) {
        return "ring fade angles must be finite";
    }

    const Effekseer::NodeRendererBasicParameter& basic = *parameter.BasicParameterPtr;

    if (basic.MaterialType != Effekseer::RendererMaterialType::Default || basic.MaterialRenderDataPtr != nullptr) {
        return "only the Default material is supported";
    }
    if (basic.AlphaBlend != Effekseer::AlphaBlendType::Blend && basic.AlphaBlend != Effekseer::AlphaBlendType::Add) {
        return "only Blend and Add blending are supported";
    }
    if (basic.TextureIndexes[0] < -1) {
        return "ring node has an invalid color texture index";
    }
    if (basic.TextureIndexes[0] >= 0 && basic.TextureFilters[0] != Effekseer::TextureFilterType::Nearest && basic.TextureFilters[0] != Effekseer::TextureFilterType::Linear) {
        return "ring node uses an unknown texture filter";
    }
    if (basic.TextureIndexes[0] >= 0 && basic.TextureWraps[0] != Effekseer::TextureWrapType::Clamp) {
        return "only Clamp texture wrapping is supported";
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

class FOnlineEffekseerSpriteRenderer final : public Effekseer::SpriteRenderer
{
public:
    FOnlineEffekseerSpriteRenderer(ptr<EffectManager> effect_mngr, ptr<IAppRender> render, shared_ptr<EffekseerDrawBinding> binding) :
        _binding {std::move(binding)},
        _multiplyEffect {effect_mngr->LoadEffect(EffectUsage::QuadSprite, "Effects/Particles_ColorMul.fofx")},
        _addEffect {effect_mngr->LoadEffect(EffectUsage::QuadSprite, "Effects/Particles_ColorAdd.fofx")},
        _drawBuffer {render->CreateDrawBuffer(false)}
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(_binding, "Effekseer sprite renderer requires draw binding");
        FO_VERIFY_AND_THROW(_multiplyEffect, "Effekseer multiply particle effect is missing");
        FO_VERIFY_AND_THROW(_addEffect, "Effekseer additive particle effect is missing");
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
            .TextureIndex = parameter.BasicParameterPtr->TextureIndexes[0],
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
        if (std::min(uv_left, uv_right) < 0.0f || std::max(uv_left, uv_right) > 1.0f || std::min(uv_top, uv_bottom) < 0.0f || std::max(uv_top, uv_bottom) > 1.0f) {
            _binding->Fail("clamped UV outside the base texture range is unsupported");
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
            std::stable_sort(_instances.begin(), _instances.end(), [](const EffekseerSpriteInstanceSnapshot& left, const EffekseerSpriteInstanceSnapshot& right) { return left.CameraDepth < right.CameraDepth; });
        }
        else if (_node->ZSort == Effekseer::ZSortType::ReverseOrder) {
            std::stable_sort(_instances.begin(), _instances.end(), [](const EffekseerSpriteInstanceSnapshot& left, const EffekseerSpriteInstanceSnapshot& right) { return left.CameraDepth > right.CameraDepth; });
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
        if (_node->TextureIndex < 0 || _node->TextureIndex >= system->Effect->GetColorImageCount()) {
            system->Fail("sprite color texture index is out of range");
            return;
        }

        Effekseer::TextureRef color_texture = system->Effect->GetColorImage(_node->TextureIndex);
        if (!color_texture || !color_texture->GetBackend()) {
            system->Fail("sprite color texture is not loaded");
            return;
        }
        Effekseer::RefPtr<FOnlineEffekseerTexture> texture = color_texture->GetBackend().DownCast<FOnlineEffekseerTexture>();
        if (!texture || !texture->RenderTextureRef) {
            system->Fail("sprite color texture was not loaded by the FOnline texture loader");
            return;
        }
        bool requested_linear_filter = _node->TextureFilter == Effekseer::TextureFilterType::Linear;
        if (texture->RenderTextureRef->LinearFiltered != requested_linear_filter) {
            system->Fail(requested_linear_filter ? "sprite requests linear filtering but its FOnline atlas is nearest-filtered" : "sprite requests nearest filtering but its FOnline atlas is linear-filtered");
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
            float32_t uv_left = texture->AtlasRect.x + instance.UV.X * texture->AtlasRect.width;
            float32_t uv_right = texture->AtlasRect.x + (instance.UV.X + instance.UV.Width) * texture->AtlasRect.width;
            float32_t uv_top = texture->AtlasRect.y + instance.UV.Y * texture->AtlasRect.height;
            float32_t uv_bottom = texture->AtlasRect.y + (instance.UV.Y + instance.UV.Height) * texture->AtlasRect.height;
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

        ptr<RenderEffect> effect = _node->AlphaBlend == Effekseer::AlphaBlendType::Add ? _addEffect.as_ptr() : _multiplyEffect.as_ptr();
        effect->ProjBuf = RenderEffect::ProjBuffer();
        MemCopy(effect->ProjBuf->ProjMatrix, glm::value_ptr(system->ViewProjMatrix), sizeof(effect->ProjBuf->ProjMatrix));
        effect->MainTex = texture->RenderTextureRef;
        effect->DrawBuffer(_drawBuffer, 0, index_count);
    }

    shared_ptr<EffekseerDrawBinding> _binding;
    nptr<RenderEffect> _multiplyEffect {};
    nptr<RenderEffect> _addEffect {};
    unique_ptr<RenderDrawBuffer> _drawBuffer;
    optional<EffekseerSpriteNodeSnapshot> _node {};
    size_t _declaredInstanceCount {};
    vector<EffekseerSpriteInstanceSnapshot> _instances {};
};

class FOnlineEffekseerRingRenderer final : public Effekseer::RingRenderer
{
public:
    FOnlineEffekseerRingRenderer(ptr<EffectManager> effect_mngr, ptr<IAppRender> render, shared_ptr<EffekseerDrawBinding> binding) :
        _binding {std::move(binding)},
        _multiplyEffect {effect_mngr->LoadEffect(EffectUsage::QuadSprite, "Effects/Particles_ColorMul.fofx")},
        _addEffect {effect_mngr->LoadEffect(EffectUsage::QuadSprite, "Effects/Particles_ColorAdd.fofx")},
        _drawBuffer {render->CreateDrawBuffer(false)},
        _whiteTexture {render->CreateTexture({1, 1}, true, false)}
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(_binding, "Effekseer ring renderer requires draw binding");
        FO_VERIFY_AND_THROW(_multiplyEffect, "Effekseer multiply particle effect is missing");
        FO_VERIFY_AND_THROW(_addEffect, "Effekseer additive particle effect is missing");

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
            .TextureIndex = parameter.BasicParameterPtr->TextureIndexes[0],
            .VertexCount = parameter.VertexCount,
            .StartingFade = parameter.StartingFade,
            .EndingFade = parameter.EndingFade,
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
        if (_node->TextureIndex >= 0 && (std::min(uv_left, uv_right) < 0.0f || std::max(uv_left, uv_right) > 1.0f || std::min(uv_top, uv_bottom) < 0.0f || std::max(uv_top, uv_bottom) > 1.0f)) {
            _binding->Fail("clamped UV outside the base texture range is unsupported");
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
            std::stable_sort(_instances.begin(), _instances.end(), [](const EffekseerRingInstanceSnapshot& left, const EffekseerRingInstanceSnapshot& right) { return left.CameraDepth < right.CameraDepth; });
        }
        else if (_node->ZSort == Effekseer::ZSortType::ReverseOrder) {
            std::stable_sort(_instances.begin(), _instances.end(), [](const EffekseerRingInstanceSnapshot& left, const EffekseerRingInstanceSnapshot& right) { return left.CameraDepth > right.CameraDepth; });
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

        nptr<RenderTexture> render_texture = _whiteTexture.as_nptr();
        frect32 atlas_rect {0.0f, 0.0f, 1.0f, 1.0f};

        if (_node->TextureIndex >= 0) {
            if (_node->TextureIndex >= system->Effect->GetColorImageCount()) {
                system->Fail("ring color texture index is out of range");
                return;
            }

            Effekseer::TextureRef color_texture = system->Effect->GetColorImage(_node->TextureIndex);

            if (!color_texture || !color_texture->GetBackend()) {
                system->Fail("ring color texture is not loaded");
                return;
            }

            Effekseer::RefPtr<FOnlineEffekseerTexture> texture = color_texture->GetBackend().DownCast<FOnlineEffekseerTexture>();

            if (!texture || !texture->RenderTextureRef) {
                system->Fail("ring color texture was not loaded by the FOnline texture loader");
                return;
            }

            bool requested_linear_filter = _node->TextureFilter == Effekseer::TextureFilterType::Linear;

            if (texture->RenderTextureRef->LinearFiltered != requested_linear_filter) {
                system->Fail(requested_linear_filter ? "ring requests linear filtering but its FOnline atlas is nearest-filtered" : "ring requests nearest filtering but its FOnline atlas is linear-filtered");
                return;
            }

            render_texture = texture->RenderTextureRef;
            atlas_rect = texture->AtlasRect;
        }

        size_t vertices_per_instance = numeric_cast<size_t>(_node->VertexCount) * 8;
        size_t instances_per_draw = EFFEKSEER_RING_VERTEX_MAX / vertices_per_instance;
        FO_VERIFY_AND_THROW(instances_per_draw != 0, "Effekseer ring geometry budget cannot fit one instance");

        for (size_t first_instance = 0; first_instance < _instances.size() && !system->Failed; first_instance += instances_per_draw) {
            size_t instance_count = std::min(instances_per_draw, _instances.size() - first_instance);
            RenderChunk(system, render_texture.as_ptr(), atlas_rect, first_instance, instance_count);
        }
    }

    void RenderChunk(ptr<EffekseerParticleRuntimeSystem::Impl> system, ptr<RenderTexture> render_texture, frect32 atlas_rect, size_t first_instance, size_t instance_count)
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(_node, "Effekseer ring chunk render called without a node snapshot");
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
                    vertex.TexU = atlas_rect.x + texture_u[vertex_offset] * atlas_rect.width;
                    vertex.TexV = atlas_rect.y + texture_v[vertex_offset] * atlas_rect.height;
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

        ptr<RenderEffect> effect = _node->AlphaBlend == Effekseer::AlphaBlendType::Add ? _addEffect.as_ptr() : _multiplyEffect.as_ptr();
        effect->ProjBuf = RenderEffect::ProjBuffer();
        MemCopy(effect->ProjBuf->ProjMatrix, glm::value_ptr(system->ViewProjMatrix), sizeof(effect->ProjBuf->ProjMatrix));
        effect->MainTex = render_texture;
        effect->DrawBuffer(_drawBuffer, 0, index_count);
    }

    shared_ptr<EffekseerDrawBinding> _binding;
    nptr<RenderEffect> _multiplyEffect {};
    nptr<RenderEffect> _addEffect {};
    unique_ptr<RenderDrawBuffer> _drawBuffer;
    unique_ptr<RenderTexture> _whiteTexture;
    optional<EffekseerRingNodeSnapshot> _node {};
    size_t _declaredInstanceCount {};
    vector<EffekseerRingInstanceSnapshot> _instances {};
};

template<typename Renderer>
class RejectingEffekseerRenderer;

#define FO_DEFINE_REJECTING_EFFEKSEER_RENDERER(RendererType, Description) \
    template<> \
    class RejectingEffekseerRenderer<Effekseer::RendererType> final : public Effekseer::RendererType \
    { \
    public: \
        explicit RejectingEffekseerRenderer(shared_ptr<EffekseerDrawBinding> binding) : \
            _binding {std::move(binding)} \
        { \
            FO_STACK_TRACE_ENTRY(); \
        } \
        void BeginRendering(const NodeParameter& parameter, int32_t count, void* user_data) override \
        { \
            FO_STACK_TRACE_ENTRY(); \
\
            ignore_unused(parameter, count, user_data); \
            _binding->Fail(Description); \
        } \
\
    private: \
        shared_ptr<EffekseerDrawBinding> _binding; \
    }

FO_DEFINE_REJECTING_EFFEKSEER_RENDERER(RibbonRenderer, "Ribbon nodes are unsupported");
FO_DEFINE_REJECTING_EFFEKSEER_RENDERER(TrackRenderer, "Track nodes are unsupported");
FO_DEFINE_REJECTING_EFFEKSEER_RENDERER(ModelRenderer, "Model nodes are unsupported");

#undef FO_DEFINE_REJECTING_EFFEKSEER_RENDERER

static auto ValidateStaticSpriteParameter(string_view path, const Effekseer::EffectBasicRenderParameter& parameter, ptr<Effekseer::Effect> effect) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (parameter.MaterialType != Effekseer::RendererMaterialType::Default || parameter.MaterialIndex != -1) {
        LogEffekseerRejection(path, "only the Default material is supported");
        return false;
    }
    if (parameter.AlphaBlend != Effekseer::AlphaBlendType::Blend && parameter.AlphaBlend != Effekseer::AlphaBlendType::Add) {
        LogEffekseerRejection(path, "only Blend and Add blending are supported");
        return false;
    }
    if (!parameter.ZTest || parameter.ZWrite) {
        LogEffekseerRejection(path, "sprite nodes must use ZTest=on and ZWrite=off");
        return false;
    }

    // Modern Effekseer exports retain a non-zero distortion intensity even while the Default
    // material leaves distortion disabled. The dormant value has no renderer effect.
    if (parameter.Distortion || parameter.EnableFalloff || parameter.TextureBlendType != -1 || parameter.FlipbookParams.EnableInterpolation || parameter.EmissiveScaling != 1.0f || parameter.EdgeParam.Threshold != 0.0f || parameter.SoftParticleDistanceFar != 0.0f || parameter.SoftParticleDistanceNear != 0.0f || parameter.SoftParticleDistanceNearOffset != 0.0f) {
        LogEffekseerRejection(path, "advanced material, distortion, soft-particle, or flipbook features are unsupported");
        return false;
    }
    if (parameter.TextureIndexes[0] < 0 || parameter.TextureIndexes[0] >= effect->GetColorImageCount()) {
        LogEffekseerRejection(path, "sprite color texture index is missing or out of range");
        return false;
    }
    if (parameter.TextureFilters[0] != Effekseer::TextureFilterType::Nearest && parameter.TextureFilters[0] != Effekseer::TextureFilterType::Linear) {
        LogEffekseerRejection(path, "sprite color texture uses an unknown filter");
        return false;
    }
    if (parameter.TextureWraps[0] != Effekseer::TextureWrapType::Clamp) {
        LogEffekseerRejection(path, "only Clamp texture wrapping is supported");
        return false;
    }
    for (size_t texture_slot = 1; texture_slot < parameter.TextureIndexes.size(); texture_slot++) {
        if (parameter.TextureIndexes[texture_slot] >= 0) {
            LogEffekseerRejection(path, "advanced texture slots are unsupported");
            return false;
        }
    }

    Effekseer::TextureRef texture = effect->GetColorImage(parameter.TextureIndexes[0]);
    if (!texture || !texture->GetBackend()) {
        LogEffekseerRejection(path, "sprite color texture failed to load");
        return false;
    }
    return true;
}

static auto ValidateStaticRingParameter(string_view path, const Effekseer::EffectBasicRenderParameter& parameter, ptr<Effekseer::Effect> effect) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (parameter.MaterialType != Effekseer::RendererMaterialType::Default || parameter.MaterialIndex != -1) {
        LogEffekseerRejection(path, "only the Default material is supported");
        return false;
    }
    if (parameter.AlphaBlend != Effekseer::AlphaBlendType::Blend && parameter.AlphaBlend != Effekseer::AlphaBlendType::Add) {
        LogEffekseerRejection(path, "only Blend and Add blending are supported");
        return false;
    }
    if (!parameter.ZTest || parameter.ZWrite) {
        LogEffekseerRejection(path, "ring nodes must use ZTest=on and ZWrite=off");
        return false;
    }
    if (parameter.Distortion || parameter.EnableFalloff || parameter.TextureBlendType != -1 || parameter.FlipbookParams.EnableInterpolation || parameter.EmissiveScaling != 1.0f || parameter.EdgeParam.Threshold != 0.0f || parameter.SoftParticleDistanceFar != 0.0f || parameter.SoftParticleDistanceNear != 0.0f || parameter.SoftParticleDistanceNearOffset != 0.0f) {
        LogEffekseerRejection(path, "advanced material, distortion, soft-particle, or flipbook features are unsupported");
        return false;
    }
    if (parameter.TextureIndexes[0] < -1 || parameter.TextureIndexes[0] >= effect->GetColorImageCount()) {
        LogEffekseerRejection(path, "ring color texture index is out of range");
        return false;
    }
    if (parameter.TextureIndexes[0] >= 0 && parameter.TextureFilters[0] != Effekseer::TextureFilterType::Nearest && parameter.TextureFilters[0] != Effekseer::TextureFilterType::Linear) {
        LogEffekseerRejection(path, "ring color texture uses an unknown filter");
        return false;
    }
    if (parameter.TextureIndexes[0] >= 0 && parameter.TextureWraps[0] != Effekseer::TextureWrapType::Clamp) {
        LogEffekseerRejection(path, "only Clamp texture wrapping is supported");
        return false;
    }
    for (size_t texture_slot = 1; texture_slot < parameter.TextureIndexes.size(); texture_slot++) {
        if (parameter.TextureIndexes[texture_slot] >= 0) {
            LogEffekseerRejection(path, "advanced texture slots are unsupported");
            return false;
        }
    }
    if (parameter.TextureIndexes[0] >= 0) {
        Effekseer::TextureRef texture = effect->GetColorImage(parameter.TextureIndexes[0]);

        if (!texture || !texture->GetBackend()) {
            LogEffekseerRejection(path, "ring color texture failed to load");
            return false;
        }
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

    if (node_type != Effekseer::EffectNodeType::Root && node_type != Effekseer::EffectNodeType::NoneType && node_type != Effekseer::EffectNodeType::Sprite && node_type != Effekseer::EffectNodeType::Ring) {
        LogEffekseerRejection(path, "only Root, None, Sprite, and Ring nodes are supported");
        return false;
    }
    if (node_type == Effekseer::EffectNodeType::Sprite && !ValidateStaticSpriteParameter(path, node->GetBasicRenderParameter(), effect)) {
        return false;
    }
    if (node_type == Effekseer::EffectNodeType::Ring && !ValidateStaticRingParameter(path, node->GetBasicRenderParameter(), effect)) {
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
    if (effect->GetNormalImageCount() != 0 || effect->GetDistortionImageCount() != 0 || effect->GetWaveCount() != 0 || effect->GetModelCount() != 0 || effect->GetMaterialCount() != 0 || effect->GetCurveCount() != 0 || effect->GetProceduralModelCount() != 0) {
        LogEffekseerRejection(path, "normal/distortion textures, sounds, models, custom materials, and external curves are unsupported");
        return false;
    }
    return ValidateEffectNode(path, effect->GetRoot(), effect);
}

struct EffekseerRuntimeState
{
    EffekseerRuntimeState(ptr<EffectManager> effect_mngr, ptr<IAppRender> render, ParticleTextureLoader texture_loader) :
        Binding {SafeAlloc::MakeShared<EffekseerDrawBinding>()},
        Setting {Effekseer::Setting::Create()},
        Manager {Effekseer::Manager::Create(EFFEKSEER_INSTANCE_MAX)},
        TextureLoader {Effekseer::MakeRefPtr<FOnlineEffekseerTextureLoader>(std::move(texture_loader))},
        GpuParticleFactory {Effekseer::MakeRefPtr<DetectingGpuParticleFactory>()},
        SpriteRenderer {Effekseer::MakeRefPtr<FOnlineEffekseerSpriteRenderer>(effect_mngr, render, Binding)},
        RibbonRenderer {Effekseer::MakeRefPtr<RejectingEffekseerRenderer<Effekseer::RibbonRenderer>>(Binding)},
        RingRenderer {Effekseer::MakeRefPtr<FOnlineEffekseerRingRenderer>(effect_mngr, render, Binding)},
        TrackRenderer {Effekseer::MakeRefPtr<RejectingEffekseerRenderer<Effekseer::TrackRenderer>>(Binding)},
        ModelRenderer {Effekseer::MakeRefPtr<RejectingEffekseerRenderer<Effekseer::ModelRenderer>>(Binding)}
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(Setting, "Failed to create Effekseer setting");
        FO_VERIFY_AND_THROW(Manager, "Failed to create Effekseer manager");
        Setting->SetCoordinateSystem(Effekseer::CoordinateSystem::RH);
        Setting->SetTextureLoader(TextureLoader);
        Setting->SetGpuParticleFactory(GpuParticleFactory);
        Manager->SetSetting(Setting);
        Manager->SetSpriteRenderer(SpriteRenderer);
        Manager->SetRibbonRenderer(RibbonRenderer);
        Manager->SetRingRenderer(RingRenderer);
        Manager->SetTrackRenderer(TrackRenderer);
        Manager->SetModelRenderer(ModelRenderer);
    }

    shared_ptr<EffekseerDrawBinding> Binding;
    Effekseer::SettingRef Setting;
    Effekseer::ManagerRef Manager;
    Effekseer::TextureLoaderRef TextureLoader;
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
        Runtime {SafeAlloc::MakeShared<EffekseerRuntimeState>(services.EffectMngr, services.Render, services.TextureLoader)},
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

auto EffekseerParticleRuntimeSystem::GetDrawSize(isize32 default_size) const -> isize32
{
    FO_STACK_TRACE_ENTRY();

    return default_size;
}

auto EffekseerParticleRuntimeSystem::GetDrawInScene() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return true;
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
    if (data.size() > numeric_cast<size_t>(std::numeric_limits<int32_t>::max())) {
        LogEffekseerRejection(path, "binary is too large");
        return {};
    }

    string material_path = strex(path).extract_dir().format_path().str();
    vector<char16_t> material_path_utf16 = ToUtf16(material_path);
    _impl->Runtime->GpuParticleFactory->Reset();
    Effekseer::EffectRef effect = Effekseer::Effect::Create(_impl->Runtime->Manager, data.data(), numeric_cast<int32_t>(data.size()), 1.0f, material_path_utf16.data());
    if (!effect) {
        LogEffekseerRejection(path, "Effekseer core rejected the binary");
        return {};
    }
    if (!ValidateEffect(path, effect.Get(), _impl->Runtime->GpuParticleFactory->WasRequested())) {
        return {};
    }

    auto system = SafeAlloc::MakeUnique<EffekseerParticleRuntimeSystem>(SafeAlloc::MakeUnique<EffekseerParticleRuntimeSystem::Impl>(_impl->Runtime, std::move(effect), string {path}));
    system->Respawn(0);
    if (!system->IsActive()) {
        return {};
    }
    return system;
}

FO_END_NAMESPACE

#endif
