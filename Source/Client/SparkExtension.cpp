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

// Todo: improve particles in 2D

#include "SparkExtension.h"
#include "StringUtils.h"
#include "VisualParticles.h"

namespace SPK::FO
{
    SparkRenderBuffer::SparkRenderBuffer(size_t vertices)
    {
        RUNTIME_ASSERT(vertices > 0);
        RUNTIME_ASSERT(vertices % 4 == 0);

        _renderBuf.reset(App->Render.CreateDrawBuffer(false));
#if FO_ENABLE_3D
        _renderBuf->Vertices3D.resize(vertices);
#endif

        auto& indices = _renderBuf->Indices;
        indices.resize(vertices / 4 * 6);
        RUNTIME_ASSERT(indices.size() <= 0xFFFF);
        for (size_t i = 0; i < indices.size() / 6; i++) {
            indices[i * 6 + 0] = static_cast<ushort>(i * 4 + 0);
            indices[i * 6 + 1] = static_cast<ushort>(i * 4 + 1);
            indices[i * 6 + 2] = static_cast<ushort>(i * 4 + 2);
            indices[i * 6 + 3] = static_cast<ushort>(i * 4 + 2);
            indices[i * 6 + 4] = static_cast<ushort>(i * 4 + 3);
            indices[i * 6 + 5] = static_cast<ushort>(i * 4 + 0);
        }
    }

    void SparkRenderBuffer::PositionAtStart()
    {
        _curVertexIndex = 0;
        _curTexCoordIndex = 0;
    }

    void SparkRenderBuffer::SetNextVertex(const Vector3D& pos, const Color& color)
    {
#if FO_ENABLE_3D
        auto& v = _renderBuf->Vertices3D[_curVertexIndex++];

        v.Position = vec3(pos.x, pos.y, pos.z);
        v.Color = COLOR_RGBA(color.a, color.b, color.g, color.r);
#endif
    }

    void SparkRenderBuffer::SetNextTexCoord(float tu, float tv)
    {
#if FO_ENABLE_3D
        auto& v = _renderBuf->Vertices3D[_curTexCoordIndex++];

        v.TexCoord[0] = tu;
        v.TexCoord[1] = tv;
#endif
    }

    void SparkRenderBuffer::Render(size_t vertices, RenderEffect* effect) const
    {
        if (vertices == 0) {
            return;
        }

#if FO_ENABLE_3D
        _renderBuf->Upload(EffectUsage::Model, vertices, vertices / 4 * 6);
#endif
        effect->DrawBuffer(_renderBuf.get(), 0, vertices / 4 * 6);
    }

    SparkQuadRenderer::SparkQuadRenderer(bool needs_dataset) : Renderer(needs_dataset) { }

    auto SparkQuadRenderer::Create() -> Ref<SparkQuadRenderer> { return SPK_NEW(SparkQuadRenderer); }

    void SparkQuadRenderer::Setup(string_view path, ParticleManager& particle_mngr)
    {
        RUNTIME_ASSERT(!_particleMngr);

        _path = path;
        _particleMngr = &particle_mngr;

        if (!_effectName.empty()) {
            SetEffectName(_effectName);
        }
        if (!_textureName.empty()) {
            SetTextureName(_textureName);
        }
    }

    void SparkQuadRenderer::AddPosAndColor(const Particle& particle, SparkRenderBuffer& render_buffer) const
    {
        render_buffer.SetNextVertex(particle.position() + quadSide() + quadUp(), particle.getColor()); // top right vertex
        render_buffer.SetNextVertex(particle.position() - quadSide() + quadUp(), particle.getColor()); // top left vertex
        render_buffer.SetNextVertex(particle.position() - quadSide() - quadUp(), particle.getColor()); // bottom left vertex
        render_buffer.SetNextVertex(particle.position() + quadSide() - quadUp(), particle.getColor()); // bottom right vertex
    }

    void SparkQuadRenderer::AddTexture2D(const Particle& particle, SparkRenderBuffer& render_buffer) const
    {
        render_buffer.SetNextTexCoord(_textureAtlasOffsets[0] + 1.0f * _textureAtlasOffsets[2], _textureAtlasOffsets[1] + 0.0f * _textureAtlasOffsets[3]);
        render_buffer.SetNextTexCoord(_textureAtlasOffsets[0] + 0.0f * _textureAtlasOffsets[2], _textureAtlasOffsets[1] + 0.0f * _textureAtlasOffsets[3]);
        render_buffer.SetNextTexCoord(_textureAtlasOffsets[0] + 0.0f * _textureAtlasOffsets[2], _textureAtlasOffsets[1] + 1.0f * _textureAtlasOffsets[3]);
        render_buffer.SetNextTexCoord(_textureAtlasOffsets[0] + 1.0f * _textureAtlasOffsets[2], _textureAtlasOffsets[1] + 1.0f * _textureAtlasOffsets[3]);
    }

    void SparkQuadRenderer::AddTexture2DAtlas(const Particle& particle, SparkRenderBuffer& render_buffer) const
    {
        computeAtlasCoordinates(particle);

        render_buffer.SetNextTexCoord(_textureAtlasOffsets[0] + textureAtlasU1() * _textureAtlasOffsets[2], _textureAtlasOffsets[1] + textureAtlasV0() * _textureAtlasOffsets[3]);
        render_buffer.SetNextTexCoord(_textureAtlasOffsets[0] + textureAtlasU0() * _textureAtlasOffsets[2], _textureAtlasOffsets[1] + textureAtlasV0() * _textureAtlasOffsets[3]);
        render_buffer.SetNextTexCoord(_textureAtlasOffsets[0] + textureAtlasU0() * _textureAtlasOffsets[2], _textureAtlasOffsets[1] + textureAtlasV1() * _textureAtlasOffsets[3]);
        render_buffer.SetNextTexCoord(_textureAtlasOffsets[0] + textureAtlasU1() * _textureAtlasOffsets[2], _textureAtlasOffsets[1] + textureAtlasV1() * _textureAtlasOffsets[3]);
    }

    RenderBuffer* SparkQuadRenderer::attachRenderBuffer(const Group& group) const { return SPK_NEW(SparkRenderBuffer, group.getCapacity() << 2); }

    void SparkQuadRenderer::render(const Group& group, const DataSet* dataSet, RenderBuffer* renderBuffer) const
    {
        RUNTIME_ASSERT(_particleMngr);

        RUNTIME_ASSERT(renderBuffer);
        auto& buffer = static_cast<SparkRenderBuffer&>(*renderBuffer);
        buffer.PositionAtStart();

        if (_modelView != _particleMngr->_viewMat) {
            _modelView = _particleMngr->_viewMat;
            _invModelView = _modelView;
            _invModelView.Inverse();
        }

        if (!group.isEnabled(PARAM_TEXTURE_INDEX)) {
            if (!group.isEnabled(PARAM_ANGLE)) {
                _renderParticle = &SparkQuadRenderer::Render2D;
            }
            else {
                _renderParticle = &SparkQuadRenderer::Render2DRot;
            }
        }
        else {
            if (!group.isEnabled(PARAM_ANGLE)) {
                _renderParticle = &SparkQuadRenderer::Render2DAtlas;
            }
            else {
                _renderParticle = &SparkQuadRenderer::Render2DAtlasRot;
            }
        }

        const bool globalOrientation = precomputeOrientation3D(group, //
            Vector3D(-_invModelView.c1, -_invModelView.c2, -_invModelView.c3), //
            Vector3D(_invModelView.b1, _invModelView.b2, _invModelView.b3), //
            Vector3D(_invModelView.d1, _invModelView.d2, _invModelView.d3));

        if (globalOrientation) {
            computeGlobalOrientation3D(group);

            for (ConstGroupIterator particleIt(group); !particleIt.end(); ++particleIt) {
                (this->*_renderParticle)(*particleIt, buffer);
            }
        }
        else {
            for (ConstGroupIterator particleIt(group); !particleIt.end(); ++particleIt) {
                computeSingleOrientation3D(*particleIt);
                (this->*_renderParticle)(*particleIt, buffer);
            }
        }

        RUNTIME_ASSERT(_effect != nullptr);
        RUNTIME_ASSERT(_texture != nullptr);
        _effect->ProjBuf = RenderEffect::ProjBuffer();
        std::memcpy(_effect->ProjBuf->ProjMatrix, &_particleMngr->_projMat, sizeof(_effect->ProjBuf->ProjMatrix));
        _effect->MainTex = _texture;

        buffer.Render(group.getNbParticles() << 2, _effect);
    }

    void SparkQuadRenderer::computeAABB(Vector3D& aabbMin, Vector3D& aabbMax, const Group& group, const DataSet* dataSet) const
    {
        const float diagonal = group.getGraphicalRadius() * std::sqrt(scaleX * scaleX + scaleY * scaleY);
        const Vector3D diag_v(diagonal, diagonal, diagonal);

        if (group.isEnabled(PARAM_SCALE)) {
            for (ConstGroupIterator it(group); !it.end(); ++it) {
                Vector3D scaledDiagV = diag_v * it->getParamNC(PARAM_SCALE);
                aabbMin.setMin(it->position() - scaledDiagV);
                aabbMax.setMax(it->position() + scaledDiagV);
            }
        }
        else {
            for (ConstGroupIterator it(group); !it.end(); ++it) {
                aabbMin.setMin(it->position());
                aabbMax.setMax(it->position());
            }
            aabbMin -= diag_v;
            aabbMax += diag_v;
        }
    }

    void SparkQuadRenderer::Render2D(const Particle& particle, SparkRenderBuffer& render_buffer) const
    {
        scaleQuadVectors(particle, scaleX, scaleY);
        AddPosAndColor(particle, render_buffer);
        AddTexture2D(particle, render_buffer);
    }

    void SparkQuadRenderer::Render2DRot(const Particle& particle, SparkRenderBuffer& render_buffer) const
    {
        rotateAndScaleQuadVectors(particle, scaleX, scaleY);
        AddPosAndColor(particle, render_buffer);
        AddTexture2D(particle, render_buffer);
    }

    void SparkQuadRenderer::Render2DAtlas(const Particle& particle, SparkRenderBuffer& render_buffer) const
    {
        scaleQuadVectors(particle, scaleX, scaleY);
        AddPosAndColor(particle, render_buffer);
        AddTexture2DAtlas(particle, render_buffer);
    }

    void SparkQuadRenderer::Render2DAtlasRot(const Particle& particle, SparkRenderBuffer& render_buffer) const
    {
        rotateAndScaleQuadVectors(particle, scaleX, scaleY);
        AddPosAndColor(particle, render_buffer);
        AddTexture2DAtlas(particle, render_buffer);
    }

    auto SparkQuadRenderer::GetEffectName() const -> const string& { return _effectName; }

    void SparkQuadRenderer::SetEffectName(const string& name)
    {
        _effectName = name;

        if (!_effectName.empty() && _particleMngr != nullptr) {
#if FO_ENABLE_3D
            _effect = _particleMngr->_effectMngr.LoadEffect(EffectUsage::Model, _effectName);
#endif
        }
        else {
            _effect = nullptr;
        }
    }

    auto SparkQuadRenderer::GetTextureName() const -> const string& { return _textureName; }

    void SparkQuadRenderer::SetTextureName(const string& name)
    {
        _textureName = name;

        if (!_textureName.empty() && _particleMngr != nullptr) {
            const auto tex_path = _str(_path).extractDir().combinePath(_textureName);
            auto&& [tex, tex_data] = _particleMngr->_textureLoader(tex_path);
            _texture = tex;
            _textureAtlasOffsets = tex_data;
        }
        else {
            _texture = nullptr;
        }
    }

    void SparkQuadRenderer::innerImport(const IO::Descriptor& descriptor)
    {
        Renderer::innerImport(descriptor);

        _effectName = "";
        _textureName = "";

        scaleX = 1.0f;
        scaleY = 1.0f;
        textureAtlasNbX = 1;
        textureAtlasNbY = 1;
        textureAtlasW = 1.0f;
        textureAtlasH = 1.0f;

        lookOrientation = LOOK_CAMERA_PLANE;
        upOrientation = UP_CAMERA;
        lockedAxis = LOCK_UP;
        lookVector.set(0.0f, 0.0f, 1.0f);
        upVector.set(0.0f, 1.0f, 0.0f);

        if (const auto* attrib = descriptor.getAttributeWithValue("effect"); attrib != nullptr) {
            SetEffectName(attrib->getValue<string>());
        }

        if (const auto* attrib = descriptor.getAttributeWithValue("texture"); attrib != nullptr) {
            SetTextureName(attrib->getValue<string>());
        }

        if (const auto* attrib = descriptor.getAttributeWithValue("scale"); attrib != nullptr) {
            const auto tmpScale = attrib->getValues<float>();
            switch (tmpScale.size()) {
            case 1:
                setScale(tmpScale[0], scaleY);
                break;
            case 2:
                setScale(tmpScale[0], tmpScale[1]);
                break;
            default:
                break;
            }
        }

        if (const auto* attrib = descriptor.getAttributeWithValue("atlas dimensions"); attrib != nullptr) {
            const auto tmpAtlasDimensions = attrib->getValues<uint32_t>();
            switch (tmpAtlasDimensions.size()) {
            case 1:
                setAtlasDimensions(tmpAtlasDimensions[0], textureAtlasNbY);
                break;
            case 2:
                setAtlasDimensions(tmpAtlasDimensions[0], tmpAtlasDimensions[1]);
                break;
            default:
                break;
            }
        }

        if (const auto* attrib = descriptor.getAttributeWithValue("look orientation"); attrib != nullptr) {
            const auto lookOrient = attrib->getValue<string>();
            if (lookOrient == "LOOK_CAMERA_PLANE") {
                lookOrientation = LOOK_CAMERA_PLANE;
            }
            else if (lookOrient == "LOOK_CAMERA_POINT") {
                lookOrientation = LOOK_CAMERA_POINT;
            }
            else if (lookOrient == "LOOK_AXIS") {
                lookOrientation = LOOK_AXIS;
            }
            else if (lookOrient == "LOOK_POINT") {
                lookOrientation = LOOK_POINT;
            }
        }

        if (const auto* attrib = descriptor.getAttributeWithValue("up orientation"); attrib != nullptr) {
            const auto upOrient = attrib->getValue<string>();
            if (upOrient == "UP_CAMERA") {
                upOrientation = UP_CAMERA;
            }
            else if (upOrient == "UP_DIRECTION") {
                upOrientation = UP_DIRECTION;
            }
            else if (upOrient == "UP_AXIS") {
                upOrientation = UP_AXIS;
            }
            else if (upOrient == "UP_POINT") {
                upOrientation = UP_POINT;
            }
        }

        if (const auto* attrib = descriptor.getAttributeWithValue("locked axis"); attrib != nullptr) {
            const auto lockAx = attrib->getValue<string>();
            if (lockAx == "LOCK_LOOK") {
                lockedAxis = LOCK_LOOK;
            }
            else if (lockAx == "LOCK_UP") {
                lockedAxis = LOCK_UP;
            }
        }

        if (const auto* attrib = descriptor.getAttributeWithValue("locked look vector"); attrib != nullptr) {
            lookVector = attrib->getValue<Vector3D>();
        }

        if (const auto* attrib = descriptor.getAttributeWithValue("locked up vector"); attrib != nullptr) {
            upVector = attrib->getValue<Vector3D>();
        }
    }

    void SparkQuadRenderer::innerExport(IO::Descriptor& descriptor) const
    {
        Renderer::innerExport(descriptor);

        descriptor.getAttribute("effect")->setValue(_effectName);

        descriptor.getAttribute("texture")->setValue(_textureName);

        const std::vector tmpScale = {scaleX, scaleY};
        descriptor.getAttribute("scale")->setValues(tmpScale.data(), 2);

        const std::vector tmpAtlasDimensions = {static_cast<uint32_t>(textureAtlasNbX), static_cast<uint32_t>(textureAtlasNbY)};
        descriptor.getAttribute("atlas dimensions")->setValues(tmpAtlasDimensions.data(), 2);

        if (lookOrientation == LOOK_CAMERA_PLANE) {
            descriptor.getAttribute("look orientation")->setValue(string("LOOK_CAMERA_PLANE"));
        }
        else if (lookOrientation == LOOK_CAMERA_POINT) {
            descriptor.getAttribute("look orientation")->setValue(string("LOOK_CAMERA_POINT"));
        }
        else if (lookOrientation == LOOK_AXIS) {
            descriptor.getAttribute("look orientation")->setValue(string("LOOK_AXIS"));
        }
        else if (lookOrientation == LOOK_POINT) {
            descriptor.getAttribute("look orientation")->setValue(string("LOOK_POINT"));
        }

        if (upOrientation == UP_CAMERA) {
            descriptor.getAttribute("up orientation")->setValue(string("UP_CAMERA"));
        }
        else if (upOrientation == UP_DIRECTION) {
            descriptor.getAttribute("up orientation")->setValue(string("UP_DIRECTION"));
        }
        else if (upOrientation == UP_AXIS) {
            descriptor.getAttribute("up orientation")->setValue(string("UP_AXIS"));
        }
        else if (upOrientation == UP_POINT) {
            descriptor.getAttribute("up orientation")->setValue(string("UP_POINT"));
        }

        if (lockedAxis == LOCK_LOOK) {
            descriptor.getAttribute("locked axis")->setValue(string("LOCK_LOOK"));
        }
        else if (lockedAxis == LOCK_UP) {
            descriptor.getAttribute("locked axis")->setValue(string("LOCK_UP"));
        }

        descriptor.getAttribute("locked look vector")->setValue(lookVector);

        descriptor.getAttribute("locked up vector")->setValue(upVector);
    }
}
