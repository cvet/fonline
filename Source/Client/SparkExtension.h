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

#include "Rendering.h"

FO_DISABLE_WARNINGS_PUSH()
#include "SPARK.h"
FO_DISABLE_WARNINGS_POP()

FO_BEGIN_NAMESPACE();
class ParticleManager;
FO_END_NAMESPACE();

namespace SPK::FO
{
    FO_USING_NAMESPACE();

    class SparkRenderBuffer final : public RenderBuffer
    {
    public:
        explicit SparkRenderBuffer(size_t vertices);

        void PositionAtStart();
        void SetNextVertex(const Vector3D& pos, const Color& color);
        void SetNextTexCoord(float32 tu, float32 tv);
        void Render(size_t vertices, RenderEffect* effect) const;

    private:
        mutable unique_ptr<RenderDrawBuffer> _renderBuf {};
        size_t _curVertexIndex {};
        size_t _curTexCoordIndex {};
    };

    class SparkQuadRenderer final : public Renderer, public QuadRenderBehavior, public Oriented3DRenderBehavior
    {
        SPK_IMPLEMENT_OBJECT(SparkQuadRenderer)

        SPK_START_DESCRIPTION
        SPK_PARENT_ATTRIBUTES(Renderer)
        SPK_ATTRIBUTE("draw size", ATTRIBUTE_TYPE_INT32S)
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

        void Setup(string_view path, ParticleManager& particle_mngr);

        auto GetDrawWidth() const -> int32;
        auto GetDrawHeight() const -> int32;
        void SetDrawSize(int32 width, int32 height);

        auto GetEffectName() const -> const string&;
        void SetEffectName(const string& effect_name);

        auto GetTextureName() const -> const string&;
        void SetTextureName(const string& tex_name);

    private:
        SparkQuadRenderer() :
            Renderer(false)
        {
        }
        explicit SparkQuadRenderer(bool needs_dataset);
        SparkQuadRenderer(const SparkQuadRenderer& renderer) = default;

        void AddPosAndColor(const Particle& particle, SparkRenderBuffer& render_buffer) const;
        void AddTexture2D(const Particle& particle, SparkRenderBuffer& render_buffer) const;
        void AddTexture2DAtlas(const Particle& particle, SparkRenderBuffer& render_buffer) const;

        void Render2D(const Particle& particle, SparkRenderBuffer& render_buffer) const; // Rendering for particles with texture 2D or no texture
        void Render2DRot(const Particle& particle, SparkRenderBuffer& render_buffer) const; // Rendering for particles with texture 2D or no texture and rotation
        void Render2DAtlas(const Particle& particle, SparkRenderBuffer& render_buffer) const; // Rendering for particles with texture 2D atlas
        void Render2DAtlasRot(const Particle& particle, SparkRenderBuffer& render_buffer) const; // Rendering for particles with texture 2D atlas and rotation

        string _path {};
        raw_ptr<ParticleManager> _particleMngr {};
        mutable raw_ptr<RenderEffect> _effect {};
        raw_ptr<RenderTexture> _texture {};
        frect32 _textureAtlasOffset {};

        int32 _drawWidth {};
        int32 _drawHeight {};

        string _effectName {};
        string _textureName {};

        mutable mat44 _modelView {};
        mutable mat44 _invModelView {};

        using RenderParticleFunc = void (SparkQuadRenderer::*)(const Particle&, SparkRenderBuffer& renderBuffer) const;
        mutable RenderParticleFunc _renderParticle {};

    private:
        void innerImport(const IO::Descriptor& descriptor) override;
        void innerExport(IO::Descriptor& descriptor) const override;
        RenderBuffer* attachRenderBuffer(const Group& group) const override;
        void render(const Group& group, const DataSet* dataSet, RenderBuffer* renderBuffer) const override;
        void computeAABB(Vector3D& aabbMin, Vector3D& aabbMax, const Group& group, const DataSet* dataSet) const override;
    };
}
