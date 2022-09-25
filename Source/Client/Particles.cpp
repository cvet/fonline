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

// Todo: rewrite spark GL renderer to engine-based renderer

#include "Particles.h"
#include "FileSystem.h"

#include "SPARK.h"

struct ParticleManager::ParticleManagerData
{
    std::unordered_map<string, SPK::Ref<SPK::System>> BaseSystems {};
};

struct ParticleSystem::ParticleSystemData
{
    SPK::Ref<SPK::System> System {};
    SPK::Ref<SPK::System> BaseSystem {};
};

ParticleManager::ParticleManager(RenderSettings& settings, FileSystem& file_sys) : _settings {settings}, _fileSys {file_sys}
{
    static bool init = false;
    if (!init) {
        // SPK::GL::GLQuadRenderer::setTextureLoader(LoadParticlesTexture);
        // SPK::IO::IOManager::get().registerObject<SPK::GL::GLLineRenderer>();
        // SPK::IO::IOManager::get().registerObject<SPK::GL::GLLineTrailRenderer>();
        // SPK::IO::IOManager::get().registerObject<SPK::GL::GLPointRenderer>();
        // SPK::IO::IOManager::get().registerObject<SPK::GL::GLQuadRenderer>();
        init = true;
    }

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

    auto particles = unique_ptr<ParticleSystem>(new ParticleSystem(*this));
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

    /*SPK::GL::GLRenderer::saveGLStates();

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(proj[0]);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(-view_offset.x, -view_offset.y, -view_offset.z);*/

    _data->System->renderParticles();

    /*SPK::GL::GLRenderer::restoreGLStates();*/
}

/*static GLuint LoadParticlesTexture(const string& fname)
{
    FileManager file;
    if (!file.LoadFile("Particles/" + fname))
        return 0;

    uint w, h;
    uchar* data = GraphicLoader::LoadTGA(file.GetBuf(), file.GetFsize(), w, h);
    if (!data) {
        return 0;
    }

    GLuint index;
    glGenTextures(1, &index);
    glBindTexture(GL_TEXTURE_2D, index);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, false);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, false);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    delete[] data;

    return index;
}*/
