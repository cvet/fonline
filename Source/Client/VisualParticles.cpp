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

#include "VisualParticles.h"
#include "Application.h"
#include "Rendering.h"

#include "SPARK.h"

struct ParticleManager::ParticleManagerData
{
    mat44 ProjMat {};
    mat44 ViewMat {};
    std::unordered_map<string, SPK::Ref<SPK::System>> BaseSystems {};
};

struct ParticleSystem::ParticleSystemData
{
    SPK::Ref<SPK::System> System {};
    SPK::Ref<SPK::System> BaseSystem {};
};

namespace SPK::FO
{
    class SparkRenderBuffer final : public RenderBuffer
    {
    public:
        explicit SparkRenderBuffer(size_t vertices);

        void PositionAtStart();
        void SetNextVertex(const Vector3D& pos, const Color& color);
        void SetNextTexCoord(float tu, float tv);
        void Render(size_t vertices, RenderEffect* effect) const;

    private:
        unique_ptr<RenderDrawBuffer> _renderBuf {};
        size_t _curVertexIndex {};
        size_t _curTexCoordIndex {};
    };

    SparkRenderBuffer::SparkRenderBuffer(size_t vertices)
    {
        RUNTIME_ASSERT(vertices > 0);
        RUNTIME_ASSERT(vertices % 4 == 0);

        _renderBuf.reset(App->Render.CreateDrawBuffer(false));
        _renderBuf->Vertices3D.resize(vertices);

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
        auto& v = _renderBuf->Vertices3D[_curVertexIndex++];

        v.Position = vec3(pos.x, pos.y, pos.z);
        v.Color = COLOR_RGBA(color.a, color.r, color.g, color.b);
    }

    void SparkRenderBuffer::SetNextTexCoord(float tu, float tv)
    {
        auto& v = _renderBuf->Vertices3D[_curTexCoordIndex++];

        v.TexCoord[0] = tu;
        v.TexCoord[1] = tv;
    }

    void SparkRenderBuffer::Render(size_t vertices, RenderEffect* effect) const
    {
        if (vertices == 0) {
            return;
        }

        _renderBuf->Upload(EffectUsage::Model, vertices, vertices / 4 * 6);
        effect->DrawBuffer(_renderBuf.get(), 0, vertices / 4 * 6);
    }

    class SparkQuadRenderer final : public Renderer, public QuadRenderBehavior, public Oriented3DRenderBehavior
    {
        SPK_IMPLEMENT_OBJECT(SparkQuadRenderer)

        SPK_START_DESCRIPTION
        SPK_PARENT_ATTRIBUTES(Renderer)
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

        auto GetEffectName() const -> const std::string&;
        void SetEffectName(const std::string& name);

        auto GetTextureName() const -> const std::string&;
        void SetTextureName(const std::string& name);

    private:
        SparkQuadRenderer() : Renderer(false) { }
        explicit SparkQuadRenderer(bool needs_dataset);
        SparkQuadRenderer(const SparkQuadRenderer& renderer) = default;

        void AddPosAndColor(const Particle& particle, SparkRenderBuffer& render_buffer) const;
        void AddTexture2DAtlas(const Particle& particle, SparkRenderBuffer& render_buffer) const;

        void Render2D(const Particle& particle, SparkRenderBuffer& render_buffer) const; // Rendering for particles with texture 2D or no texture
        void Render2DRot(const Particle& particle, SparkRenderBuffer& render_buffer) const; // Rendering for particles with texture 2D or no texture and rotation
        void Render2DAtlas(const Particle& particle, SparkRenderBuffer& render_buffer) const; // Rendering for particles with texture 2D atlas
        void Render2DAtlasRot(const Particle& particle, SparkRenderBuffer& render_buffer) const; // Rendering for particles with texture 2D atlas and rotation

        std::string _path {};
        ParticleManager* _particleMngr {};
        RenderEffect* _effect {};
        RenderTexture* _texture {};
        FRect _textureAtlasOffsets {};

        std::string _effectName {};
        std::string _textureName {};

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

        if (_modelView != _particleMngr->_data->ViewMat) {
            _modelView = _particleMngr->_data->ViewMat;
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
        std::memcpy(_effect->ProjBuf->ProjMatrix, &_particleMngr->_data->ProjMat, sizeof(_effect->ProjBuf->ProjMatrix));
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
    }

    void SparkQuadRenderer::Render2DRot(const Particle& particle, SparkRenderBuffer& render_buffer) const
    {
        rotateAndScaleQuadVectors(particle, scaleX, scaleY);
        AddPosAndColor(particle, render_buffer);
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

    auto SparkQuadRenderer::GetEffectName() const -> const std::string& { return _effectName; }

    void SparkQuadRenderer::SetEffectName(const std::string& name)
    {
        _effectName = name;

        if (!_effectName.empty() && _particleMngr != nullptr) {
            _effect = _particleMngr->_effectMngr.LoadEffect(EffectUsage::Model, _effectName, _path);
        }
        else {
            _effect = nullptr;
        }
    }

    auto SparkQuadRenderer::GetTextureName() const -> const std::string& { return _textureName; }

    void SparkQuadRenderer::SetTextureName(const std::string& name)
    {
        _textureName = name;

        if (!_textureName.empty() && _particleMngr != nullptr) {
            auto&& [tex, tex_data] = _particleMngr->_textureLoader(name, _path);
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
            SetEffectName(attrib->getValue<std::string>());
        }

        if (const auto* attrib = descriptor.getAttributeWithValue("texture"); attrib != nullptr) {
            SetTextureName(attrib->getValue<std::string>());
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
            const auto lookOrient = attrib->getValue<std::string>();
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
            const auto upOrient = attrib->getValue<std::string>();
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
            const auto lockAx = attrib->getValue<std::string>();
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
            descriptor.getAttribute("look orientation")->setValue(std::string("LOOK_CAMERA_PLANE"));
        }
        else if (lookOrientation == LOOK_CAMERA_POINT) {
            descriptor.getAttribute("look orientation")->setValue(std::string("LOOK_CAMERA_POINT"));
        }
        else if (lookOrientation == LOOK_AXIS) {
            descriptor.getAttribute("look orientation")->setValue(std::string("LOOK_AXIS"));
        }
        else if (lookOrientation == LOOK_POINT) {
            descriptor.getAttribute("look orientation")->setValue(std::string("LOOK_POINT"));
        }

        if (upOrientation == UP_CAMERA) {
            descriptor.getAttribute("up orientation")->setValue(std::string("UP_CAMERA"));
        }
        else if (upOrientation == UP_DIRECTION) {
            descriptor.getAttribute("up orientation")->setValue(std::string("UP_DIRECTION"));
        }
        else if (upOrientation == UP_AXIS) {
            descriptor.getAttribute("up orientation")->setValue(std::string("UP_AXIS"));
        }
        else if (upOrientation == UP_POINT) {
            descriptor.getAttribute("up orientation")->setValue(std::string("UP_POINT"));
        }

        if (lockedAxis == LOCK_LOOK) {
            descriptor.getAttribute("locked axis")->setValue(std::string("LOCK_LOOK"));
        }
        else if (lockedAxis == LOCK_UP) {
            descriptor.getAttribute("locked axis")->setValue(std::string("LOCK_UP"));
        }

        descriptor.getAttribute("locked look vector")->setValue(lookVector);

        descriptor.getAttribute("locked up vector")->setValue(upVector);
    }
}

ParticleManager::ParticleManager(RenderSettings& settings, EffectManager& effect_mngr, FileSystem& file_sys, TextureLoader tex_loader) :
    _settings {settings}, //
    _effectMngr {effect_mngr},
    _fileSys {file_sys},
    _textureLoader {std::move(tex_loader)}
{
    static std::once_flag once;
    std::call_once(once, [] { SPK::IO::IOManager::get().registerObject<SPK::FO::SparkQuadRenderer>(); });

    _data = new ParticleManagerData();
}

ParticleManager::~ParticleManager()
{
    delete _data;
}

auto ParticleManager::CreateParticles(string_view name) -> unique_ptr<ParticleSystem>
{
    SPK::Ref<SPK::System> base_system;

    if (const auto it = _data->BaseSystems.find(string(name)); it == _data->BaseSystems.end()) {
        if (const auto file = _fileSys.ReadFile(name)) {
            base_system = SPK::IO::IOManager::get().loadFromBuffer("xml", reinterpret_cast<const char*>(file.GetBuf()), static_cast<unsigned>(file.GetSize()));
        }

        if (base_system) {
            for (size_t i = 0; i < base_system->getNbGroups(); i++) {
                auto&& group = base_system->getGroup(i);
                if (auto&& renderer = SPK::dynamicCast<SPK::FO::SparkQuadRenderer>(group->getRenderer())) {
                    renderer->Setup(name, *this);
                }
            }
        }

        _data->BaseSystems.emplace(name, base_system);
    }
    else {
        base_system = it->second;
    }

    if (!base_system) {
        return nullptr;
    }

    auto&& system = SPK::SPKObject::copy(base_system);
    system->initialize();

    auto&& particles = unique_ptr<ParticleSystem>(new ParticleSystem(*this));
    particles->_data->System = system;
    particles->_data->BaseSystem = base_system;

    return std::move(particles);
}

ParticleSystem::ParticleSystem(ParticleManager& particle_mngr) : _particleMngr {particle_mngr}
{
    _data = new ParticleSystemData();
}

ParticleSystem::~ParticleSystem()
{
    delete _data;
}

bool ParticleSystem::IsActive() const
{
    return _data->System->isActive();
}

void ParticleSystem::Update(float dt, const mat44& world, const vec3& pos_offest, float look_dir, const vec3& view_offset)
{
    NON_CONST_METHOD_HINT();

    if (!_data->System->isActive()) {
        return;
    }

    mat44 pos_offset_mat;
    mat44::Translation(pos_offest, pos_offset_mat);
    mat44 view_offset_mat;
    mat44::Translation(view_offset, view_offset_mat);

    if (!_data->BaseSystem->getTransform().isLocalIdentity()) {
        vec3 result_pos_rot;
        vec3 result_pos_pos;
        vec3 result_pos_scale;
        (view_offset_mat * world * pos_offset_mat).Decompose(result_pos_scale, result_pos_rot, result_pos_pos);

        mat44 result_pos_pos_mat;
        mat44::Translation(result_pos_pos, result_pos_pos_mat);

        mat44 cam_rot_mat;
        mat44::RotationX(_particleMngr._settings.MapCameraAngle * PI_FLOAT / 180.0f, cam_rot_mat);
        mat44 look_dir_mat;
        mat44::RotationY((look_dir - 90.0f) * PI_FLOAT / 180.0f, look_dir_mat);

        mat44 result_pos_mat = result_pos_pos_mat * cam_rot_mat * look_dir_mat;

        _data->System->getTransform().set(result_pos_mat.Transpose()[0]);
    }
    else {
        mat44 result_pos_mat = view_offset_mat * world * pos_offset_mat;

        _data->System->getTransform().set(result_pos_mat.Transpose()[0]);
    }

    if (const auto local_pos = _data->BaseSystem->getTransform().getLocalPos(); local_pos != SPK::Vector3D()) {
        _data->System->getTransform().setPosition(_data->System->getTransform().getLocalPos() + local_pos);
    }

    _data->System->updateTransform();
    _data->System->updateParticles(dt);
}

void ParticleSystem::Draw(const mat44& proj, const vec3& view_offset) const
{
    if (!_data->System->isActive()) {
        return;
    }

    mat44 view;
    mat44::Translation({-view_offset.x, -view_offset.y, -view_offset.z}, view);
    view.Transpose();

    _particleMngr._data->ProjMat = view * proj;
    _particleMngr._data->ViewMat = view;

    _data->System->renderParticles();
}
