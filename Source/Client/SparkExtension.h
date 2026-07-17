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

#if FO_SPARK_PARTICLES

#include "ParticleRuntime.h"
#include "Rendering.h"

FO_DISABLE_WARNINGS_PUSH()
#include "SPARK.h"
FO_DISABLE_WARNINGS_POP()

namespace SPK::FO
{
    class SparkQuadRenderer;
}

FO_BEGIN_NAMESPACE
class IAppRender;
class RenderEffect;
class RenderTexture;
class RenderDrawBuffer;
class SparkParticleRuntimeBackend;

class SparkParticleRuntimeSystem final : public ParticleRuntimeSystem
{
    friend class SparkParticleRuntimeBackend;
    friend class SafeAlloc;

public:
    SparkParticleRuntimeSystem(const SparkParticleRuntimeSystem&) = delete;
    SparkParticleRuntimeSystem(SparkParticleRuntimeSystem&&) noexcept = delete;
    auto operator=(const SparkParticleRuntimeSystem&) = delete;
    auto operator=(SparkParticleRuntimeSystem&&) noexcept = delete;
    ~SparkParticleRuntimeSystem() override;

    [[nodiscard]] auto IsActive() const -> bool override;
    [[nodiscard]] auto GetDrawSize(isize32 default_size) const -> isize32 override;
    [[nodiscard]] auto GetDrawInScene() const -> bool override;

    void Setup(const ParticleRuntimeSetup& setup) override;
    [[nodiscard]] auto Prewarm() -> float32_t override;
    void Respawn(optional<int32_t> seed) override;
    void Update(float32_t delta_seconds) override;
    void RefreshRenderTransform() override;
    void Draw() override;

    [[nodiscard]] auto GetEditableBaseSystem() -> SPK::Ref<SPK::System>;
    void ReplaceBaseSystem(SPK::Ref<SPK::System> system);

private:
    SparkParticleRuntimeSystem(ptr<SparkParticleRuntimeBackend> runtime, string_view path, SPK::Ref<SPK::System> base_system);

    struct Impl;
    unique_ptr<Impl> _impl;
};

class SparkParticleRuntimeBackend final : public ParticleRuntimeBackend
{
    friend class SparkParticleRuntimeSystem;
    friend class SPK::FO::SparkQuadRenderer;

public:
    explicit SparkParticleRuntimeBackend(const ParticleRuntimeServices& services);
    SparkParticleRuntimeBackend(const SparkParticleRuntimeBackend&) = delete;
    SparkParticleRuntimeBackend(SparkParticleRuntimeBackend&&) noexcept = delete;
    auto operator=(const SparkParticleRuntimeBackend&) = delete;
    auto operator=(SparkParticleRuntimeBackend&&) noexcept = delete;
    ~SparkParticleRuntimeBackend() override;

    [[nodiscard]] auto GetExtensions() const -> vector<string> override;
    [[nodiscard]] auto SupportsSeededRespawn() const -> bool override;
    void InvalidateResource(string_view path) override;
    [[nodiscard]] auto Create(string_view path) -> unique_nptr<ParticleRuntimeSystem> override;

private:
    struct Impl;
    unique_ptr<Impl> _impl;
};
FO_END_NAMESPACE

namespace SPK::FO
{
    FO_USING_NAMESPACE();

    class SparkRenderBuffer final : public RenderBuffer
    {
    public:
        SparkRenderBuffer(size_t vertices, ptr<FO_NAMESPACE IAppRender> render);

        void PositionAtStart();
        void SetNextVertex(const Vector3D& pos, const Color& color);
        void SetNextTexCoord(float32_t tu, float32_t tv);
        void Render(size_t vertices, ptr<RenderEffect> effect) const;

    private:
        mutable unique_ptr<RenderDrawBuffer> _renderBuf;
        nptr<FO_NAMESPACE IAppRender> _render {};
        size_t _curVertexIndex {};
        size_t _curTexCoordIndex {};
    };

    void EnsureSparkParticleObjectsRegistered();

    class SparkQuadRenderer final : public Renderer, public QuadRenderBehavior, public Oriented3DRenderBehavior
    {
        SPK_IMPLEMENT_OBJECT(SparkQuadRenderer)

        SPK_START_DESCRIPTION
        SPK_PARENT_ATTRIBUTES(Renderer)
        SPK_ATTRIBUTE("draw size", ATTRIBUTE_TYPE_INT32S)
        SPK_ATTRIBUTE("draw in scene", ATTRIBUTE_TYPE_BOOL)
        SPK_ATTRIBUTE("effect", ATTRIBUTE_TYPE_STRING)
        SPK_ATTRIBUTE("blend mode", ATTRIBUTE_TYPE_STRING)
        SPK_ATTRIBUTE("texture", ATTRIBUTE_TYPE_STRING)
        SPK_ATTRIBUTE("scale", ATTRIBUTE_TYPE_FLOATS)
        SPK_ATTRIBUTE("atlas dimensions", ATTRIBUTE_TYPE_UINT32S)
        SPK_ATTRIBUTE("look orientation", ATTRIBUTE_TYPE_STRING)
        SPK_ATTRIBUTE("up orientation", ATTRIBUTE_TYPE_STRING)
        SPK_ATTRIBUTE("locked axis", ATTRIBUTE_TYPE_STRING)
        SPK_ATTRIBUTE("locked look vector", ATTRIBUTE_TYPE_VECTOR)
        SPK_ATTRIBUTE("locked up vector", ATTRIBUTE_TYPE_VECTOR)
        SPK_END_DESCRIPTION

    public:
        static auto Create() -> Ref<SparkQuadRenderer>;
        ~SparkQuadRenderer() override = default;

        [[nodiscard]] auto Setup(string_view path, ptr<FO_NAMESPACE SparkParticleRuntimeBackend> runtime) -> bool;

        auto GetDrawWidth() const -> int32_t;
        auto GetDrawHeight() const -> int32_t;
        void SetDrawSize(int32_t width, int32_t height);

        auto GetDrawInScene() const -> bool;
        void SetDrawInScene(bool draw_in_scene);

        auto GetEffectName() const -> string_view;
        void SetEffectName(string_view effect_name);

        auto GetTextureName() const -> string_view;
        void SetTextureName(string_view tex_name);

    private:
        SparkQuadRenderer() :
            Renderer(false)
        {
        }
        explicit SparkQuadRenderer(bool needs_dataset);
        SparkQuadRenderer(const SparkQuadRenderer& renderer) = default;

        string _path {};
        nptr<FO_NAMESPACE SparkParticleRuntimeBackend> _runtime {};

        int32_t _drawWidth {};
        int32_t _drawHeight {};
        bool _drawInScene {};

        string _effectName {};
        string _textureName {};

        void AddPosAndColor(const Particle& particle, ptr<SparkRenderBuffer> render_buffer) const;
        void AddTexture2D(const Particle& particle, ptr<SparkRenderBuffer> render_buffer) const;
        void AddTexture2DAtlas(const Particle& particle, ptr<SparkRenderBuffer> render_buffer) const;

        void Render2D(const Particle& particle, ptr<SparkRenderBuffer> render_buffer) const; // Rendering for particles with texture 2D or no texture
        void Render2DRot(const Particle& particle, ptr<SparkRenderBuffer> render_buffer) const; // Rendering for particles with texture 2D or no texture and rotation
        void Render2DAtlas(const Particle& particle, ptr<SparkRenderBuffer> render_buffer) const; // Rendering for particles with texture 2D atlas
        void Render2DAtlasRot(const Particle& particle, ptr<SparkRenderBuffer> render_buffer) const; // Rendering for particles with texture 2D atlas and rotation

        mutable nptr<RenderEffect> _effect {};
        nptr<RenderTexture> _texture {};
        frect32 _textureAtlasOffset {};

        mutable mat44 _modelView {};
        mutable mat44 _invModelView {};

        using RenderParticleFunc = void (SparkQuadRenderer::*)(const Particle&, ptr<SparkRenderBuffer> renderBuffer) const;
        mutable RenderParticleFunc _renderParticle {};

    private:
        void innerImport(const IO::Descriptor& descriptor) override;
        void innerExport(IO::Descriptor& descriptor) const override;
        RenderBuffer* attachRenderBuffer(const Group& group) const override;
        void render(const Group& group, const DataSet* dataSet, RenderBuffer* renderBuffer) const override;
        void computeAABB(Vector3D& aabbMin, Vector3D& aabbMax, const Group& group, const DataSet* dataSet) const override;
    };
}

#endif
