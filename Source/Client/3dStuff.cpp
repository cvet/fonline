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

#include "3dStuff.h"

#if FO_ENABLE_3D

#include "Settings.h"

FO_BEGIN_NAMESPACE();

void MeshData::Load(DataReader& reader, HashResolver& hash_resolver)
{
    FO_STACK_TRACE_ENTRY();

    uint32 len = 0;
    reader.ReadPtr(&len, sizeof(len));
    Vertices.resize(len);
    reader.ReadPtr(Vertices.data(), len * sizeof(Vertex3D));

    reader.ReadPtr(&len, sizeof(len));
    Indices.resize(len);
    reader.ReadPtr(Indices.data(), len * sizeof(vindex_t));

    reader.ReadPtr(&len, sizeof(len));
    DiffuseTexture.resize(len);
    reader.ReadPtr(DiffuseTexture.data(), len);

    reader.ReadPtr(&len, sizeof(len));
    SkinBones.resize(len);
    SkinBoneNames.resize(len);

    string tmp;

    for (uint32 i = 0, j = len; i < j; i++) {
        reader.ReadPtr(&len, sizeof(len));
        tmp.resize(len);
        reader.ReadPtr(tmp.data(), len);
        SkinBoneNames[i] = hash_resolver.ToHashedString(tmp);
    }

    reader.ReadPtr(&len, sizeof(len));
    SkinBoneOffsets.resize(len);
    reader.ReadPtr(SkinBoneOffsets.data(), len * sizeof(mat44));
}

void ModelBone::Load(DataReader& reader, HashResolver& hash_resolver)
{
    FO_STACK_TRACE_ENTRY();

    uint32 len = 0;
    reader.ReadPtr(&len, sizeof(len));
    string tmp;
    tmp.resize(len);
    reader.ReadPtr(tmp.data(), len);
    Name = hash_resolver.ToHashedString(tmp);

    reader.ReadPtr(&TransformationMatrix, sizeof(TransformationMatrix));
    reader.ReadPtr(&GlobalTransformationMatrix, sizeof(GlobalTransformationMatrix));

    if (reader.Read<uint8>() != 0) {
        AttachedMesh = SafeAlloc::MakeUnique<MeshData>();
        AttachedMesh->Load(reader, hash_resolver);
        AttachedMesh->Owner = this;
    }
    else {
        AttachedMesh = nullptr;
    }

    reader.ReadPtr(&len, sizeof(len));

    for (uint32 i = 0; i < len; i++) {
        auto child = SafeAlloc::MakeUnique<ModelBone>();
        child->Load(reader, hash_resolver);
        Children.emplace_back(std::move(child));
    }

    CombinedTransformationMatrix = mat44();
}

// ReSharper disable once CppMemberFunctionMayBeConst
void ModelBone::FixAfterLoad(ModelBone* root_bone)
{
    FO_STACK_TRACE_ENTRY();

    if (AttachedMesh) {
        for (size_t i = 0; i < AttachedMesh->SkinBoneNames.size(); i++) {
            if (AttachedMesh->SkinBoneNames[i]) {
                AttachedMesh->SkinBones[i] = root_bone->Find(AttachedMesh->SkinBoneNames[i]);
            }
            else {
                AttachedMesh->SkinBones[i] = AttachedMesh->Owner;
            }
        }
    }

    for (auto& child : Children) {
        child->FixAfterLoad(root_bone);
    }
}

auto ModelBone::Find(hstring bone_name) const noexcept -> const ModelBone*
{
    FO_STACK_TRACE_ENTRY();

    if (Name == bone_name) {
        return this;
    }

    for (const auto& child : Children) {
        const auto* bone = child->Find(bone_name);

        if (bone != nullptr) {
            return bone;
        }
    }

    return nullptr;
}

auto ModelBone::Find(hstring bone_name) noexcept -> ModelBone*
{
    FO_STACK_TRACE_ENTRY();

    if (Name == bone_name) {
        return this;
    }

    for (auto& child : Children) {
        auto* bone = child->Find(bone_name);

        if (bone != nullptr) {
            return bone;
        }
    }

    return nullptr;
}

ModelManager::ModelManager(RenderSettings& settings, FileSystem& resources, EffectManager& effect_mngr, GameTimer& game_time, HashResolver& hash_resolver, NameResolver& name_resolver, AnimationResolver& anim_name_resolver, TextureLoader tex_loader) :
    _settings {&settings},
    _resources {&resources},
    _effectMngr {&effect_mngr},
    _gameTime {&game_time},
    _hashResolver {&hash_resolver},
    _nameResolver {&name_resolver},
    _animNameResolver {&anim_name_resolver},
    _textureLoader {tex_loader},
    _geometry(settings),
    _particleMngr(settings, effect_mngr, resources, game_time, std::move(tex_loader))
{
    FO_STACK_TRACE_ENTRY();

    _moveTransitionTime = numeric_cast<float32>(_settings->Animation3dSmoothTime) / 1000.0f;
    _moveTransitionTime = std::max(_moveTransitionTime, 0.001f);

    if (_settings->Animation3dFPS != 0) {
        _animUpdateThreshold = iround<int32>(1000.0f / numeric_cast<float32>(_settings->Animation3dFPS));
    }

    _headBone = GetBoneHashedString(settings.HeadBone);

    for (const auto& bone_name : settings.LegBones) {
        _legBones.emplace(GetBoneHashedString(bone_name));
    }
}

auto ModelManager::GetBoneHashedString(string_view name) const -> hstring
{
    FO_STACK_TRACE_ENTRY();

    return _hashResolver->ToHashedString(name);
}

auto ModelManager::LoadModel(string_view fname) -> ModelBone*
{
    FO_STACK_TRACE_ENTRY();

    // Find already loaded
    auto name_hashed = _hashResolver->ToHashedString(fname);

    for (auto& root_bone : _loadedModels) {
        if (root_bone->Name == name_hashed) {
            return root_bone.get();
        }
    }

    // Add to already processed
    if (_processedFiles.count(name_hashed) != 0) {
        return nullptr;
    }

    _processedFiles.emplace(name_hashed);

    // File maybe not exists, it's not an error
    if (!_resources->IsFileExists(fname)) {
        return nullptr;
    }

    // Load file data
    const auto file = _resources->ReadFile(fname);
    FO_RUNTIME_ASSERT(file);

    // Load bones
    auto root_bone = SafeAlloc::MakeUnique<ModelBone>();
    auto reader = DataReader({file.GetBuf(), file.GetSize()});

    root_bone->Load(reader, *_hashResolver);
    root_bone->FixAfterLoad(root_bone.get());

    // Load animations
    const auto anim_count = reader.Read<uint32>();

    for (uint32 i = 0; i < anim_count; i++) {
        auto anim = SafeAlloc::MakeUnique<ModelAnimation>();
        anim->Load(reader, *_hashResolver);
        _loadedAnims.push_back(std::move(anim));
    }

    reader.VerifyEnd();

    // Add to collection
    root_bone->Name = name_hashed;
    _loadedModels.emplace_back(std::move(root_bone));
    return _loadedModels.back().get();
}

auto ModelManager::LoadAnimation(string_view anim_fname, string_view anim_name) -> ModelAnimation*
{
    FO_STACK_TRACE_ENTRY();

    // Find in already loaded
    const auto take_first = anim_name == "Base";
    const auto name_hashed = _hashResolver->ToHashedString(anim_fname);

    for (auto& anim : _loadedAnims) {
        if (strex(anim->GetFileName()).compare_ignore_case(anim_fname) && (take_first || strex(anim->GetName()).compare_ignore_case(anim_name))) {
            return anim.get();
        }
    }

    // Check maybe file already processed and nothing founded
    if (_processedFiles.count(name_hashed) != 0) {
        return nullptr;
    }

    // File not processed, load and recheck animations
    const auto* root_bone = LoadModel(anim_fname);

    if (root_bone != nullptr) {
        return LoadAnimation(anim_fname, anim_name);
    }

    _processedFiles.emplace(name_hashed);
    return nullptr;
}

auto ModelManager::LoadTexture(string_view texture_name, string_view model_path) -> MeshTexture*
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    // Skip empty
    if (texture_name.empty()) {
        return nullptr;
    }

    // Create new
    const string tex_path = strex(model_path).extract_dir().combine_path(texture_name);
    auto&& [tex, tex_data] = _textureLoader(tex_path);

    if (tex == nullptr) {
        return nullptr;
    }

    auto* mesh_tex = SafeAlloc::MakeRaw<MeshTexture>();
    mesh_tex->Name = _hashResolver->ToHashedString(texture_name);
    mesh_tex->MainTex = tex;
    mesh_tex->AtlasOffsetData = tex_data;

    return mesh_tex;
}

auto ModelManager::CreateModel(string_view name) -> unique_ptr<ModelInstance>
{
    FO_STACK_TRACE_ENTRY();

    auto* model_info = GetInformation(name);

    if (model_info == nullptr) {
        return nullptr;
    }

    auto* model = model_info->CreateInstance();

    if (model == nullptr) {
        return nullptr;
    }

    // Create mesh instances
    model->_allMeshes.resize(model_info->_hierarchy->_allDrawBones.size());
    model->_allMeshesDisabled.resize(model->_allMeshes.size());

    for (size_t i = 0, j = model_info->_hierarchy->_allDrawBones.size(); i < j; i++) {
        model->_allMeshes[i] = SafeAlloc::MakeUnique<MeshInstance>();
        auto* mesh_instance = model->_allMeshes[i].get();
        auto* mesh = model_info->_hierarchy->_allDrawBones[i]->AttachedMesh.get();
        mesh_instance->Mesh = mesh;
        const auto* tex_name = !mesh->DiffuseTexture.empty() ? mesh->DiffuseTexture.c_str() : nullptr;
        mesh_instance->CurTexures[0] = mesh_instance->DefaultTexures[0] = (tex_name != nullptr ? model_info->_hierarchy->GetTexture(tex_name) : nullptr);
        mesh_instance->CurEffect = mesh_instance->DefaultEffect = (!mesh->EffectName.empty() ? model_info->_hierarchy->GetEffect(mesh->EffectName) : nullptr);
    }

    model->PlayAnim(CritterStateAnim::None, CritterActionAnim::None, nullptr, 0.0f, ModelAnimFlags::Init);

    return unique_ptr<ModelInstance>(model);
}

void ModelManager::PreloadModel(string_view name)
{
    FO_STACK_TRACE_ENTRY();

    const auto* model_info = GetInformation(name);
    ignore_unused(model_info);
}

auto ModelManager::GetInformation(string_view name) -> ModelInformation*
{
    FO_STACK_TRACE_ENTRY();

    // Try to find instance
    for (auto& model_info : _allModelInfos) {
        if (model_info->_fileName == name) {
            return model_info.get();
        }
    }

    // Create new instance
    auto model_info = SafeAlloc::MakeUnique<ModelInformation>(*this);

    if (!model_info->Load(name)) {
        return nullptr;
    }

    _allModelInfos.push_back(std::move(model_info));
    return _allModelInfos.back().get();
}

auto ModelManager::GetHierarchy(string_view name) -> ModelHierarchy*
{
    FO_STACK_TRACE_ENTRY();

    for (auto& model_hierarchy : _hierarchyFiles) {
        if (model_hierarchy->_fileName == name) {
            return model_hierarchy.get();
        }
    }

    // Load
    auto* root_bone = LoadModel(name);

    if (root_bone == nullptr) {
        WriteLog("Unable to load model hierarchy file '{}'", name);
        return nullptr;
    }

    auto model_hierarchy = SafeAlloc::MakeUnique<ModelHierarchy>(*this);
    model_hierarchy->_fileName = name;
    model_hierarchy->_rootBone = root_bone;
    model_hierarchy->SetupBones();

    _hierarchyFiles.emplace_back(std::move(model_hierarchy));
    return _hierarchyFiles.back().get();
}

static void MultMatricesf(const float32 a[16], const float32 b[16], float32 r[16]) noexcept;
static void MultMatrixVecf(const float32 matrix[16], const float32 in[4], float32 out[4]) noexcept;
static auto InvertMatrixf(const float32 m[16], float32 inv_out[16]) noexcept -> bool;

auto ModelManager::MatrixProject(float32 objx, float32 objy, float32 objz, const float32 model_matrix[16], const float32 proj_matrix[16], const int32 viewport[4], float32* winx, float32* winy, float32* winz) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    float32 in[4];
    in[0] = objx;
    in[1] = objy;
    in[2] = objz;
    in[3] = 1.0f;

    float32 out[4];
    MultMatrixVecf(model_matrix, in, out);
    MultMatrixVecf(proj_matrix, out, in);

    if (in[3] == 0.0f) {
        return false;
    }

    in[0] /= in[3];
    in[1] /= in[3];
    in[2] /= in[3];

    in[0] = in[0] * 0.5f + 0.5f;
    in[1] = in[1] * 0.5f + 0.5f;
    in[2] = in[2] * 0.5f + 0.5f;

    in[0] = in[0] * numeric_cast<float32>(viewport[2] + viewport[0]);
    in[1] = in[1] * numeric_cast<float32>(viewport[3] + viewport[1]);

    *winx = in[0];
    *winy = in[1];
    *winz = in[2];

    return true;
}

auto ModelManager::MatrixUnproject(float32 winx, float32 winy, float32 winz, const float32 model_matrix[16], const float32 proj_matrix[16], const int32 viewport[4], float32* objx, float32* objy, float32* objz) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    float32 final_matrix[16];
    MultMatricesf(model_matrix, proj_matrix, final_matrix);

    if (!InvertMatrixf(final_matrix, final_matrix)) {
        return false;
    }

    float32 in[4];
    in[0] = winx;
    in[1] = winy;
    in[2] = winz;
    in[3] = 1.0f;

    in[0] = (in[0] - numeric_cast<float32>(viewport[0])) / numeric_cast<float32>(viewport[2]);
    in[1] = (in[1] - numeric_cast<float32>(viewport[1])) / numeric_cast<float32>(viewport[3]);

    in[0] = in[0] * 2 - 1;
    in[1] = in[1] * 2 - 1;
    in[2] = in[2] * 2 - 1;

    float32 out[4];
    MultMatrixVecf(final_matrix, in, out);

    if (out[3] == 0.0f) {
        return false;
    }

    out[0] /= out[3];
    out[1] /= out[3];
    out[2] /= out[3];
    *objx = out[0];
    *objy = out[1];
    *objz = out[2];

    return true;
}

static void MultMatricesf(const float32 a[16], const float32 b[16], float32 r[16]) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    for (auto i = 0; i < 4; i++) {
        for (auto j = 0; j < 4; j++) {
            r[i * 4 + j] = a[i * 4 + 0] * b[0 * 4 + j] + a[i * 4 + 1] * b[1 * 4 + j] + a[i * 4 + 2] * b[2 * 4 + j] + a[i * 4 + 3] * b[3 * 4 + j];
        }
    }
}

static void MultMatrixVecf(const float32 matrix[16], const float32 in[4], float32 out[4]) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    for (auto i = 0; i < 4; i++) {
        out[i] = in[0] * matrix[0 * 4 + i] + in[1] * matrix[1 * 4 + i] + in[2] * matrix[2 * 4 + i] + in[3] * matrix[3 * 4 + i];
    }
}

static auto InvertMatrixf(const float32 m[16], float32 inv_out[16]) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    float32 inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

    auto det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (det == 0.0f) {
        return false;
    }

    det = 1.0f / det;

    for (auto i = 0; i < 16; i++) {
        inv_out[i] = inv[i] * det;
    }

    return true;
}

ModelInstance::ModelInstance(ModelManager& model_mngr, ModelInformation* info) :
    _modelMngr(&model_mngr),
    _modelInfo {info}
{
    FO_STACK_TRACE_ENTRY();

    _speedAdjustBase = 1.0f;
    _speedAdjustCur = 1.0f;
    _speedAdjustLink = 1.0f;
    _lookDirAngle = GameSettings::HEXAGONAL_GEOMETRY ? 150.0f : 135.0f;
    _moveDirAngle = _lookDirAngle;
    _targetMoveDirAngle = _moveDirAngle;
    _childChecker = true;
    mat44::RotationX(_modelMngr->_settings->MapCameraAngle * DEG_TO_RAD_FLOAT, _matRot);
    _forceDraw = true;
    _lastDrawTime = GetTime();
    SetupFrame(_modelInfo->_drawSize);
}

void ModelInstance::SetupFrame(isize32 draw_size)
{
    _frameSize.width = draw_size.width * FRAME_SCALE;
    _frameSize.height = draw_size.height * FRAME_SCALE;

    // Projection
    const auto frame_ratio = numeric_cast<float32>(_frameSize.width) / numeric_cast<float32>(_frameSize.height);
    const auto proj_height = numeric_cast<float32>(_frameSize.height) * (1.0f / _modelMngr->_settings->ModelProjFactor);
    const auto proj_width = proj_height * frame_ratio;

    _frameProj = App->Render.CreateOrthoMatrix(0.0f, proj_width, 0.0f, proj_height, -10.0f, 10.0f);
    _frameProjColMaj = _frameProj;
    _frameProjColMaj.Transpose();
}

auto ModelInstance::Convert3dTo2d(vec3 pos) const -> ipos32
{
    FO_STACK_TRACE_ENTRY();

    const int32 viewport[4] = {0, 0, _frameSize.width, _frameSize.height};
    vec3 out;
    ModelManager::MatrixProject(pos.x, pos.y, pos.z, mat44().Transpose()[0], _frameProjColMaj[0], viewport, &out.x, &out.y, &out.z);
    return {iround<int32>(out.x / const_numeric_cast<float32>(FRAME_SCALE)), iround<int32>(out.y / const_numeric_cast<float32>(FRAME_SCALE))};
}

auto ModelInstance::Convert2dTo3d(ipos32 pos) const -> vec3
{
    FO_STACK_TRACE_ENTRY();

    const int32 viewport[4] = {0, 0, _frameSize.width, _frameSize.height};
    const auto xf = numeric_cast<float32>(pos.x) * numeric_cast<float32>(FRAME_SCALE);
    const auto yf = numeric_cast<float32>(pos.y) * numeric_cast<float32>(FRAME_SCALE);
    vec3 out;
    ModelManager::MatrixUnproject(xf, numeric_cast<float32>(_frameSize.height) - yf, 0.0f, mat44().Transpose()[0], _frameProjColMaj[0], viewport, &out.x, &out.y, &out.z);
    out.z = 0.0f;
    return out;
}

void ModelInstance::StartMeshGeneration()
{
    FO_STACK_TRACE_ENTRY();

    if (!_allowMeshGeneration) {
        _allowMeshGeneration = true;
        GenerateCombinedMeshes();
    }
}

void ModelInstance::PrewarmParticles()
{
    FO_STACK_TRACE_ENTRY();

    for (auto& model_particle : _modelParticles) {
        model_particle.Particle->Prewarm();
    }
}

auto ModelInstance::PlayAnim(CritterStateAnim state_anim, CritterActionAnim action_anim, const int32* layers, float32 ntime, ModelAnimFlags flags) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto prev_state_anim = _curStateAnim;
    const auto prev_action_anim = _curActionAnim;

    _curStateAnim = state_anim;
    _curActionAnim = action_anim;

    // Restore rotation
    if (const auto no_rotate = IsEnumSet(flags, ModelAnimFlags::NoRotate); no_rotate != _noRotate) {
        _noRotate = no_rotate;

        if (_noRotate) {
            _deferredLookDirAngle = _lookDirAngle;
        }
        else if (!is_float_equal(_deferredLookDirAngle, _lookDirAngle)) {
            _lookDirAngle = _deferredLookDirAngle;
            RefreshMoveAnimation();
        }
    }

    // Get animation index
    const auto anim_pair = std::make_pair(state_anim, action_anim);
    float32 speed = 1.0f;
    int32 anim_index = 0;

    if (!IsEnumSet(flags, ModelAnimFlags::Init)) {
        anim_index = _modelInfo->GetAnimationIndex(state_anim, action_anim, &speed);
    }

    // Check animation changes
    int32 new_layers[MODEL_LAYERS_COUNT];

    if (layers != nullptr) {
        MemCopy(new_layers, layers, sizeof(_curLayers));
    }
    else {
        MemCopy(new_layers, _curLayers, sizeof(_curLayers));
    }

    // Animation layers
    if (const auto it = _modelInfo->_animLayerValues.find(anim_pair); it != _modelInfo->_animLayerValues.end()) {
        for (auto&& [layer_index, value] : it->second) {
            new_layers[layer_index] = value;
        }
    }

    const bool layers_changed = !MemCompare(new_layers, _curLayers, sizeof(new_layers));

    // Try skip redundant calls
    const bool may_skip_redundant = !IsEnumSet(flags, ModelAnimFlags::Init) && !IsEnumSet(flags, ModelAnimFlags::PlayOnce);

    if (may_skip_redundant && prev_state_anim == _curStateAnim && prev_action_anim == _curActionAnim && !layers_changed) {
        return false;
    }

    MemCopy(_curLayers, new_layers, sizeof(_curLayers));

    bool mesh_changed = false;
    vector<hstring> fast_transition_bones;

    if (layers_changed || IsEnumSet(flags, ModelAnimFlags::Init)) {
        // Store data to compare later
        const auto old_cuts = _allCuts;

        for (size_t i = 0; i < _allMeshes.size(); i++) {
            _allMeshesDisabled[i] = _allMeshes[i]->Disabled;
        }

        // Set anim data
        SetAnimData(_modelInfo->_animDataDefault, true);

        if (_parentBone) {
            SetAnimData(_animLink, false);
        }

        // Mark animations as unused
        for (auto& child : _children) {
            child->_childChecker = false;
        }

        // Get unused layers and meshes
        bool unused_layers[MODEL_LAYERS_COUNT] = {};

        for (int32 i = 0; i < numeric_cast<int32>(MODEL_LAYERS_COUNT); i++) {
            if (new_layers[i] == 0) {
                continue;
            }

            for (const auto& link : _modelInfo->_animData) {
                if (link.Layer == i && link.LayerValue == new_layers[i] && link.ChildName.empty()) {
                    for (const auto j : link.DisabledLayer) {
                        unused_layers[j] = true;
                    }
                    for (const auto disabled_mesh_name : link.DisabledMesh) {
                        for (auto& mesh : _allMeshes) {
                            if (!disabled_mesh_name || disabled_mesh_name == mesh->Mesh->Owner->Name) {
                                mesh->Disabled = true;
                            }
                        }
                    }
                }
            }
        }

        if (_parentBone) {
            for (const auto j : _animLink.DisabledLayer) {
                unused_layers[j] = true;
            }
            for (auto disabled_mesh_name : _animLink.DisabledMesh) {
                for (auto& mesh : _allMeshes) {
                    if (!disabled_mesh_name || disabled_mesh_name == mesh->Mesh->Owner->Name) {
                        mesh->Disabled = true;
                    }
                }
            }
        }

        // Append animations
        set<uint32> keep_alive_particles;

        for (int32 i = 0; i < numeric_cast<int32>(MODEL_LAYERS_COUNT); i++) {
            if (unused_layers[i] || new_layers[i] == 0) {
                continue;
            }

            for (auto& link : _modelInfo->_animData) {
                if (link.Layer == i && link.LayerValue == new_layers[i]) {
                    if (link.ChildName.empty()) {
                        SetAnimData(link, false);
                        continue;
                    }

                    if (link.IsParticles) {
                        bool available = false;

                        for (const auto& model_particle : _modelParticles) {
                            if (model_particle.Id == link.Id) {
                                available = true;
                                break;
                            }
                        }

                        if (!available) {
                            if (const auto* to_bone = FindBone(link.LinkBone)) {
                                if (auto particle = _modelMngr->_particleMngr.CreateParticle(link.ChildName)) {
                                    _modelParticles.push_back({link.Id, std::move(particle), to_bone, vec3(link.MoveX, link.MoveY, link.MoveZ), link.RotY});
                                }
                            }
                        }

                        keep_alive_particles.insert(link.Id);
                    }
                    else {
                        bool available = false;

                        for (auto& child : _children) {
                            if (child->_animLink.Id == link.Id) {
                                child->_childChecker = true;
                                available = true;
                                break;
                            }
                        }

                        if (!available) {
                            // Link to main bone
                            if (link.LinkBone) {
                                auto* to_bone = _modelInfo->_hierarchy->_rootBone->Find(link.LinkBone);

                                if (to_bone != nullptr) {
                                    auto model = _modelMngr->CreateModel(link.ChildName);

                                    if (model) {
                                        mesh_changed = true;
                                        model->_parent = this;
                                        model->_parentBone = to_bone;
                                        model->_animLink = link;
                                        model->SetAnimData(link, false);
                                        _children.emplace_back(std::move(model));
                                    }

                                    if (_modelInfo->_fastTransitionBones.count(link.LinkBone) != 0) {
                                        fast_transition_bones.emplace_back(link.LinkBone);
                                    }
                                }
                            }
                            // Link all bones
                            else {
                                auto model = _modelMngr->CreateModel(link.ChildName);

                                if (model) {
                                    for (auto& child_bone : model->_modelInfo->_hierarchy->_allBones) {
                                        auto* root_bone = _modelInfo->_hierarchy->_rootBone->Find(child_bone->Name);

                                        if (root_bone != nullptr) {
                                            model->_linkBones.emplace_back(root_bone);
                                            model->_linkBones.emplace_back(child_bone);
                                        }
                                    }

                                    mesh_changed = true;
                                    model->_parent = this;
                                    model->_parentBone = _modelInfo->_hierarchy->_rootBone;
                                    model->_animLink = link;
                                    model->SetAnimData(link, false);
                                    _children.emplace_back(std::move(model));
                                }
                            }
                        }
                    }
                }
            }
        }

        // Erase unused stuff
        for (auto it = _children.begin(); it != _children.end();) {
            const auto& child = *it;

            if (!child->_childChecker) {
                mesh_changed = true;
                it = _children.erase(it);
            }
            else {
                ++it;
            }
        }

        for (auto it = _modelParticles.begin(); it != _modelParticles.end();) {
            if (it->Id != 0 && keep_alive_particles.count(it->Id) == 0) {
                it = _modelParticles.erase(it);
            }
            else {
                ++it;
            }
        }

        // Check for mesh changes
        if (!mesh_changed) {
            for (size_t i = 0; i < _allMeshes.size() && !mesh_changed; i++) {
                mesh_changed = _allMeshes[i]->LastEffect != _allMeshes[i]->CurEffect;
            }
        }
        if (!mesh_changed) {
            for (size_t i = 0; i < _allMeshes.size() && !mesh_changed; i++) {
                for (size_t k = 0; k < MODEL_MAX_TEXTURES && !mesh_changed; k++) {
                    mesh_changed = _allMeshes[i]->LastTexures[k] != _allMeshes[i]->CurTexures[k];
                }
            }
        }
        if (!mesh_changed) {
            for (size_t i = 0; i < _allMeshes.size() && !mesh_changed; i++) {
                mesh_changed = _allMeshesDisabled[i] != _allMeshes[i]->Disabled;
            }
        }

        // Affect cut
        if (!mesh_changed) {
            mesh_changed = _allCuts != old_cuts;
        }
    }

    RefreshMoveAnimation();

    if (_bodyAnimController && anim_index >= 0) {
        const auto new_track = _curTrack == 0 ? 1 : 0;
        _animDuration = _bodyAnimController->GetAnimationDuration(anim_index);

        // Turn off fast transition bones on other tracks
        if (!fast_transition_bones.empty()) {
            _bodyAnimController->ResetBonesTransition(new_track, fast_transition_bones);
        }

        _bodyAnimController->ResetEvents();

        const auto no_smooth = IsEnumSet(flags, ModelAnimFlags::NoSmooth) || IsEnumSet(flags, ModelAnimFlags::Freeze) || IsEnumSet(flags, ModelAnimFlags::Init);
        const auto smooth_time = no_smooth ? 0.0f : _modelMngr->_moveTransitionTime;
        const auto anim_start_time = std::min(_animDuration * ntime, _animDuration - 0.001f);
        const auto anim_duration = IsEnumSet(flags, ModelAnimFlags::Freeze) || IsEnumSet(flags, ModelAnimFlags::Init) ? 0.0f : _animDuration - anim_start_time;

        // Disable current track
        if (no_smooth) {
            _bodyAnimController->SetTrackEnable(_curTrack, false);
        }
        else {
            _bodyAnimController->AddEventEnable(_curTrack, false, smooth_time);
            _bodyAnimController->AddEventSpeed(_curTrack, 0.0f, 0.0f, smooth_time);
            _bodyAnimController->AddEventWeight(_curTrack, 0.0f, 0.0f, smooth_time);
        }

        // Enable the new track
        _curTrack = new_track;
        _speedAdjustCur = speed;

        _bodyAnimController->SetTrackEnable(new_track, true);
        _bodyAnimController->SetTrackAnimation(new_track, anim_index, nullptr);
        _bodyAnimController->SetTrackPosition(new_track, anim_start_time);
        _bodyAnimController->AddEventSpeed(new_track, 1.0f, 0.0f, 0.0f);

        if (IsEnumSet(flags, ModelAnimFlags::PlayOnce) || IsEnumSet(flags, ModelAnimFlags::Freeze) || IsEnumSet(flags, ModelAnimFlags::Init)) {
            _bodyAnimController->AddEventSpeed(new_track, 0.0f, anim_duration, 0.0f);
        }

        _bodyAnimController->AddEventWeight(new_track, 1.0f, 0.0f, smooth_time);
        _bodyAnimController->AdvanceTime(0.0f);

        if ((_isMoving || _turnAnimPlaying) && _moveAnimController) {
            _moveAnimController->AdvanceTime(0.0f);
        }

        // Force redraw
        _forceDraw = true;
    }

    // Set animation for children
    for (auto& child : _children) {
        if (child->PlayAnim(state_anim, action_anim, layers, ntime, flags)) {
            mesh_changed = true;
        }
    }

    // Regenerate mesh for drawing
    if (!_parentBone && mesh_changed) {
        GenerateCombinedMeshes();
    }

    return mesh_changed;
}

void ModelInstance::MoveModel(ipos32 offset)
{
    FO_STACK_TRACE_ENTRY();

    const vec3 pos_zero = Convert2dTo3d({0, 0});
    const vec3 pos = Convert2dTo3d(offset);
    const vec3 diff = pos - pos_zero;

    _moveOffset += diff;
    _forceDraw = true;
}

void ModelInstance::UpdatePose(bool staying_pose, bool moving, int32 moving_speed)
{
    FO_STACK_TRACE_ENTRY();

    _isStayingPose = staying_pose;
    _isMoving = staying_pose && moving;

    if (_isMoving) {
        if (moving_speed < _modelMngr->_settings->RunAnimStartSpeed) {
            _isRunning = false;
            _movingSpeedFactor = numeric_cast<float32>(moving_speed) / numeric_cast<float32>(_modelMngr->_settings->WalkAnimBaseSpeed);
        }
        else {
            _isRunning = true;
            _movingSpeedFactor = numeric_cast<float32>(moving_speed) / numeric_cast<float32>(_modelMngr->_settings->RunAnimBaseSpeed);
        }
    }

    if (!_isStayingPose) {
        if (_curMovingAnimIndex != -1) {
            _moveAnimController->ResetEvents();
            _moveAnimController->SetTrackEnable(_curMoveTrack, false);
            _curMovingAnim = CritterActionAnim::None;
            _curMovingAnimIndex = -1;
        }

        _turnAnimPlaying = false;
    }

    RefreshMoveAnimation();
}

void ModelInstance::RefreshMoveAnimation()
{
    FO_STACK_TRACE_ENTRY();

    if (!_moveAnimController) {
        return;
    }
    if (!_isStayingPose) {
        return;
    }

    auto state_anim = CritterStateAnim::None;
    auto action_anim = CritterActionAnim::Idle;

    if (_isMoving) {
        const auto angle_diff = GeometryHelper::GetDirAngleDiff(_targetMoveDirAngle, _lookDirAngle);

        if ((!_isMovingBack && angle_diff <= 95.0f) || (_isMovingBack && angle_diff <= 85.0f)) {
            _isMovingBack = false;
            action_anim = _isRunning ? CritterActionAnim::Run : CritterActionAnim::Walk;
        }
        else {
            _isMovingBack = true;
            action_anim = _isRunning ? CritterActionAnim::RunBack : CritterActionAnim::WalkBack;
        }

        _turnAnimPlaying = false;
    }
    else {
        if (_isMovingBack) {
            _moveDirAngle = _targetMoveDirAngle = _lookDirAngle;
        }

        state_anim = _curStateAnim;
        _isMovingBack = false;

        const auto angle_diff = GeometryHelper::GetDirAngleDiffSided(_targetMoveDirAngle, _lookDirAngle);

        if (std::abs(angle_diff) > _modelMngr->_settings->CritterTurnAngle) {
            _targetMoveDirAngle = _lookDirAngle;

            if (_turnAnimPlaying) {
                return;
            }

            action_anim = angle_diff < 0.0f ? CritterActionAnim::TurnRight : CritterActionAnim::TurnLeft;
            _turnAnimPlaying = true;
        }
        else if (_turnAnimPlaying) {
            return;
        }
    }

    if (_animInitCallback) {
        _animInitCallback(state_anim, action_anim);
    }

    _curMovingAnim = action_anim;

    float32 speed = 1.0f;
    const auto anim_index = _modelInfo->GetAnimationIndex(state_anim, action_anim, &speed);

    if (anim_index == _curMovingAnimIndex) {
        return;
    }

    _curMovingAnimIndex = anim_index;

    if (_isMoving) {
        speed *= _movingSpeedFactor;
    }

    constexpr float32 smooth_time = 0.001f;

    if (anim_index != -1) {
        const auto new_track = _curMoveTrack == 0 ? 1 : 0;

        _moveAnimController->ResetEvents();

        _moveAnimController->AddEventEnable(_curMoveTrack, false, smooth_time);
        _moveAnimController->AddEventSpeed(_curMoveTrack, 0.0f, 0.0f, smooth_time);
        _moveAnimController->AddEventWeight(_curMoveTrack, 0.0f, 0.0f, smooth_time);

        _moveAnimController->SetTrackEnable(new_track, true);
        _moveAnimController->SetTrackAnimation(new_track, anim_index, &_modelMngr->_legBones);
        _moveAnimController->SetTrackPosition(new_track, 0.0f);

        _moveAnimController->AddEventSpeed(new_track, speed, 0.0f, smooth_time);
        _moveAnimController->AddEventWeight(new_track, 1.0f, 0.0f, smooth_time);

        if (_turnAnimPlaying) {
            const auto anim_duration = _moveAnimController->GetAnimationDuration(anim_index);
            _moveAnimController->AddEventEnable(new_track, false, anim_duration / speed);
        }

        _curMoveTrack = new_track;
    }
    else {
        _moveAnimController->ResetEvents();

        _moveAnimController->AddEventEnable(_curMoveTrack, false, smooth_time);
        _moveAnimController->AddEventSpeed(_curMoveTrack, 0.0f, 0.0f, smooth_time);
        _moveAnimController->AddEventWeight(_curMoveTrack, 0.0f, 0.0f, smooth_time);
    }
}

void ModelInstance::SetAnimInitCallback(function<void(CritterStateAnim&, CritterActionAnim&)> anim_init)
{
    FO_STACK_TRACE_ENTRY();

    _animInitCallback = std::move(anim_init);
}

auto ModelInstance::HasAnimation(CritterStateAnim state_anim, CritterActionAnim action_anim) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto index = std::make_pair(state_anim, action_anim);
    const auto it = _modelInfo->_animIndexes.find(index);

    return it != _modelInfo->_animIndexes.end();
}

auto ModelInstance::ResolveAnimation(CritterStateAnim& state_anim, CritterActionAnim& action_anim) -> bool
{
    FO_STACK_TRACE_ENTRY();

    return _modelInfo->GetAnimationIndex(state_anim, action_anim, nullptr) != -1;
}

auto ModelInstance::GetMovingAnim() const noexcept -> CritterActionAnim
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_curMovingAnimIndex != -1) {
        return _curMovingAnim;
    }
    else {
        return _isRunning ? CritterActionAnim::Run : CritterActionAnim::Walk;
    }
}

auto ModelInstance::IsAnimationPlaying() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (_bodyAnimController) {
        const auto track0_playing = _bodyAnimController->GetTrackEnable(0) && _bodyAnimController->GetTrackSpeed(0) > 0.0f;
        const auto track1_playing = _bodyAnimController->GetTrackEnable(1) && _bodyAnimController->GetTrackSpeed(1) > 0.0f;
        return track0_playing || track1_playing;
    }
    else {
        return false;
    }
}

auto ModelInstance::GetDrawSize() const -> isize32
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_frameSize.width % FRAME_SCALE == 0);
    FO_RUNTIME_ASSERT(_frameSize.height % FRAME_SCALE == 0);

    return {_frameSize.width / FRAME_SCALE, _frameSize.height / FRAME_SCALE};
}

auto ModelInstance::GetViewSize() const -> isize32
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto draw_width_scale = numeric_cast<float32>(_frameSize.width / FRAME_SCALE) / numeric_cast<float32>(_modelInfo->_drawSize.width);
    const auto draw_height_scale = numeric_cast<float32>(_frameSize.height / FRAME_SCALE) / numeric_cast<float32>(_modelInfo->_drawSize.height);
    const auto view_width = iround<int32>(numeric_cast<float32>(_modelInfo->_viewSize.width) * draw_width_scale);
    const auto view_height = iround<int32>(numeric_cast<float32>(_modelInfo->_viewSize.height) * draw_height_scale);

    return {view_width, view_height};
}

auto ModelInstance::GetSpeed() const -> float32
{
    FO_STACK_TRACE_ENTRY();

    return _speedAdjustCur * _speedAdjustBase * _speedAdjustLink * _modelMngr->_globalSpeedAdjust;
}

auto ModelInstance::GetTime() const -> nanotime
{
    FO_STACK_TRACE_ENTRY();

    return _modelMngr->_gameTime->GetFrameTime();
}

void ModelInstance::SetAnimData(ModelAnimationData& data, bool clear)
{
    FO_STACK_TRACE_ENTRY();

    // Transformations
    if (clear) {
        _matScaleBase = mat44();
        _matRotBase = mat44();
        _matTransBase = mat44();
    }

    mat44 mat_tmp;
    if (data.ScaleX != 0.0f) {
        _matScaleBase = _matScaleBase * mat44::Scaling(vec3(data.ScaleX, 1.0f, 1.0f), mat_tmp);
    }
    if (data.ScaleY != 0.0f) {
        _matScaleBase = _matScaleBase * mat44::Scaling(vec3(1.0f, data.ScaleY, 1.0f), mat_tmp);
    }
    if (data.ScaleZ != 0.0f) {
        _matScaleBase = _matScaleBase * mat44::Scaling(vec3(1.0f, 1.0f, data.ScaleZ), mat_tmp);
    }
    if (data.RotX != 0.0f) {
        _matRotBase = _matRotBase * mat44::RotationX(-data.RotX * DEG_TO_RAD_FLOAT, mat_tmp);
    }
    if (data.RotY != 0.0f) {
        _matRotBase = _matRotBase * mat44::RotationY(data.RotY * DEG_TO_RAD_FLOAT, mat_tmp);
    }
    if (data.RotZ != 0.0f) {
        _matRotBase = _matRotBase * mat44::RotationZ(data.RotZ * DEG_TO_RAD_FLOAT, mat_tmp);
    }
    if (data.MoveX != 0.0f) {
        _matTransBase = _matTransBase * mat44::Translation(vec3(data.MoveX, 0.0f, 0.0f), mat_tmp);
    }
    if (data.MoveY != 0.0f) {
        _matTransBase = _matTransBase * mat44::Translation(vec3(0.0f, data.MoveY, 0.0f), mat_tmp);
    }
    if (data.MoveZ != 0.0f) {
        _matTransBase = _matTransBase * mat44::Translation(vec3(0.0f, 0.0f, -data.MoveZ), mat_tmp);
    }

    // Speed
    if (clear) {
        _speedAdjustLink = 1.0f;
    }
    if (data.SpeedAjust != 0.0f) {
        _speedAdjustLink *= data.SpeedAjust;
    }

    // Textures
    if (clear) {
        // Enable all meshes, set default texture
        for (auto& mesh : _allMeshes) {
            mesh->Disabled = false;

            for (size_t i = 0; i < MODEL_MAX_TEXTURES; i++) {
                mesh->LastTexures[i] = mesh->CurTexures[i];
                mesh->CurTexures[i] = mesh->DefaultTexures[i];
            }
        }
    }

    if (!data.TextureInfo.empty()) {
        for (auto&& [tex_name, mesh_name, tex_num] : data.TextureInfo) {
            MeshTexture* texture = nullptr;

            // Evaluate texture
            if (strex(tex_name).starts_with("Parent")) { // Parent_MeshName
                if (_parent) {
                    const auto* parent_mesh_name = tex_name.c_str() + 6;

                    if (parent_mesh_name[0] == '_') {
                        parent_mesh_name++;
                    }

                    const auto parent_mesh_name_hashed = *parent_mesh_name != 0 ? _modelMngr->GetBoneHashedString(parent_mesh_name) : hstring();

                    for (auto& mesh : _parent->_allMeshes) {
                        if (!parent_mesh_name_hashed || parent_mesh_name_hashed == mesh->Mesh->Owner->Name) {
                            texture = mesh->CurTexures[tex_num].get();
                            break;
                        }
                    }
                }
            }
            else {
                texture = _modelInfo->_hierarchy->GetTexture(tex_name);
            }

            // Assign it
            for (auto& mesh : _allMeshes) {
                if (!mesh_name || mesh_name == mesh->Mesh->Owner->Name) {
                    mesh->CurTexures[tex_num] = texture;
                }
            }
        }
    }

    // Effects
    if (clear) {
        for (auto& mesh : _allMeshes) {
            mesh->LastEffect = mesh->CurEffect;
            mesh->CurEffect = mesh->DefaultEffect;
        }
    }

    if (!data.EffectInfo.empty()) {
        for (const auto& eff_info : data.EffectInfo) {
            RenderEffect* effect = nullptr;

            // Get effect
            if (strex(std::get<0>(eff_info)).starts_with("Parent")) { // Parent_MeshName
                if (_parent) {
                    const auto* mesh_name = std::get<0>(eff_info).c_str() + 6;

                    if (mesh_name[0] == '_') {
                        mesh_name++;
                    }

                    const auto mesh_name_hashed = *mesh_name != 0 ? _modelMngr->GetBoneHashedString(mesh_name) : hstring();

                    for (auto& mesh : _parent->_allMeshes) {
                        if (!mesh_name_hashed || mesh_name_hashed == mesh->Mesh->Owner->Name) {
                            effect = mesh->CurEffect.get();
                            break;
                        }
                    }
                }
            }
            else {
                effect = _modelInfo->_hierarchy->GetEffect(std::get<0>(eff_info));
            }

            // Assign it
            const auto mesh_name = std::get<1>(eff_info);
            for (auto& mesh : _allMeshes) {
                if (!mesh_name || mesh_name == mesh->Mesh->Owner->Name) {
                    mesh->CurEffect = effect;
                }
            }
        }
    }

    // Cut
    if (clear) {
        _allCuts.clear();
    }
    for (auto& cut_info : data.CutInfo) {
        _allCuts.emplace_back(cut_info);
    }
}

void ModelInstance::SetDir(uint8 dir, bool smooth_rotation)
{
    FO_STACK_TRACE_ENTRY();

    const auto dir_angle = GeometryHelper::DirToAngle(dir);

    SetMoveDirAngle(dir_angle, smooth_rotation);
    SetLookDirAngle(dir_angle);
}

void ModelInstance::SetLookDirAngle(int32 dir_angle)
{
    FO_STACK_TRACE_ENTRY();

    const auto new_angle = numeric_cast<float32>(180 - dir_angle);

    if (!_noRotate) {
        if (!is_float_equal(new_angle, _lookDirAngle)) {
            _lookDirAngle = new_angle;
            RefreshMoveAnimation();
        }
    }
    else {
        _deferredLookDirAngle = new_angle;
    }
}

void ModelInstance::SetMoveDirAngle(int32 dir_angle, bool smooth_rotation)
{
    FO_STACK_TRACE_ENTRY();

    const auto new_angle = numeric_cast<float32>(180 - dir_angle);

    if (!is_float_equal(new_angle, _targetMoveDirAngle) || (!smooth_rotation && !is_float_equal(new_angle, _moveDirAngle))) {
        _targetMoveDirAngle = new_angle;

        if (!smooth_rotation) {
            _moveDirAngle = _targetMoveDirAngle;
            _turnAnimPlaying = false;
        }

        RefreshMoveAnimation();
    }

    if (!_modelInfo->_rotationBone) {
        SetLookDirAngle(dir_angle);
    }
}

void ModelInstance::SetRotation(float32 rx, float32 ry, float32 rz)
{
    FO_STACK_TRACE_ENTRY();

    mat44 my;
    mat44 mx;
    mat44 mz;
    mat44::RotationX(rx, mx);
    mat44::RotationY(ry, my);
    mat44::RotationZ(rz, mz);

    _matRot = mx * my * mz;
}

void ModelInstance::SetScale(float32 sx, float32 sy, float32 sz)
{
    FO_STACK_TRACE_ENTRY();

    mat44::Scaling(vec3(sx, sy, sz), _matScale);
}

void ModelInstance::SetSpeed(float32 speed)
{
    FO_STACK_TRACE_ENTRY();

    _speedAdjustBase = speed;
}

void ModelInstance::GenerateCombinedMeshes()
{
    FO_STACK_TRACE_ENTRY();

    // Generation disabled
    if (!_allowMeshGeneration) {
        return;
    }

    // Clean up buffers
    for (auto& combined_mesh : _combinedMeshes) {
        combined_mesh->EncapsulatedMeshCount = 0;
        combined_mesh->CurBoneMatrix = 0;
        combined_mesh->Meshes.clear();
        combined_mesh->MeshIndices.clear();
        combined_mesh->MeshVertices.clear();
        combined_mesh->MeshAnimLayers.clear();
        combined_mesh->MeshBuf->Vertices3D.clear();
        combined_mesh->MeshBuf->VertCount = 0;
        combined_mesh->MeshBuf->Indices.clear();
        combined_mesh->MeshBuf->IndCount = 0;
    }

    _actualCombinedMeshesCount = 0;

    // Combine meshes recursively
    FillCombinedMeshes(this);

    // Cut
    _disableCulling = false;
    CutCombinedMeshes(this);

    // Finalize meshes
    for (size_t i = 0; i < _actualCombinedMeshesCount; i++) {
        _combinedMeshes[i]->MeshBuf->StaticDataChanged = true;
    }
}

void ModelInstance::FillCombinedMeshes(const ModelInstance* cur)
{
    FO_STACK_TRACE_ENTRY();

    // Combine meshes
    for (const auto& mesh : cur->_allMeshes) {
        CombineMesh(mesh.get(), cur->_parentBone ? cur->_animLink.Layer : 0);
    }

    // Fill child
    for (const auto& child : cur->_children) {
        FillCombinedMeshes(child.get());
    }
}

void ModelInstance::CombineMesh(const MeshInstance* mesh_instance, int32 anim_layer)
{
    FO_STACK_TRACE_ENTRY();

    // Skip disabled meshes
    if (mesh_instance->Disabled) {
        return;
    }

    // Try to encapsulate mesh instance to current combined mesh
    for (size_t i = 0; i < _actualCombinedMeshesCount; i++) {
        if (CanBatchCombinedMesh(_combinedMeshes[i].get(), mesh_instance)) {
            BatchCombinedMesh(_combinedMeshes[i].get(), mesh_instance, anim_layer);
            return;
        }
    }

    // Create new combined mesh
    if (_actualCombinedMeshesCount >= _combinedMeshes.size()) {
        auto combined_mesh = SafeAlloc::MakeUnique<CombinedMesh>();
        combined_mesh->MeshBuf = App->Render.CreateDrawBuffer(true);
        combined_mesh->SkinBones.resize(MODEL_MAX_BONES);
        combined_mesh->SkinBoneOffsets.resize(MODEL_MAX_BONES);
        _combinedMeshes.emplace_back(std::move(combined_mesh));
    }

    BatchCombinedMesh(_combinedMeshes[_actualCombinedMeshesCount].get(), mesh_instance, anim_layer);
    _actualCombinedMeshesCount++;
}

auto ModelInstance::CanBatchCombinedMesh(const CombinedMesh* combined_mesh, const MeshInstance* mesh_instance) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (combined_mesh->EncapsulatedMeshCount == 0) {
        return true;
    }
    if (combined_mesh->DrawEffect != mesh_instance->CurEffect) {
        return false;
    }
    for (size_t i = 0; i < MODEL_MAX_TEXTURES; i++) {
        if (combined_mesh->Textures[i] && mesh_instance->CurTexures[i] && combined_mesh->Textures[i]->MainTex != mesh_instance->CurTexures[i]->MainTex) {
            return false;
        }
    }
    return combined_mesh->CurBoneMatrix + mesh_instance->Mesh->SkinBones.size() <= combined_mesh->SkinBones.size();
}

void ModelInstance::BatchCombinedMesh(CombinedMesh* combined_mesh, const MeshInstance* mesh_instance, int32 anim_layer)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    const auto& mesh_data = mesh_instance->Mesh;
    auto& vertices = combined_mesh->MeshBuf->Vertices3D;
    auto& indices = combined_mesh->MeshBuf->Indices;
    const auto vertices_old_size = vertices.size();
    const auto indices_old_size = indices.size();

    // Set or add data
    if (combined_mesh->EncapsulatedMeshCount == 0) {
        vertices = mesh_data->Vertices;
        indices = mesh_data->Indices;
        combined_mesh->DrawEffect = mesh_instance->CurEffect;
        std::ranges::for_each(combined_mesh->SkinBones, [](auto&& bone) { bone = nullptr; });
        std::ranges::for_each(combined_mesh->Textures, [](auto&& tex) { tex = nullptr; });
        combined_mesh->CurBoneMatrix = 0;
    }
    else {
        vertices.insert(vertices.end(), mesh_data->Vertices.begin(), mesh_data->Vertices.end());
        indices.insert(indices.end(), mesh_data->Indices.begin(), mesh_data->Indices.end());

        // Add indices offset
        const auto index_offset = numeric_cast<vindex_t>(vertices.size() - mesh_data->Vertices.size());
        const auto start_index = indices.size() - mesh_data->Indices.size();
        for (auto i = start_index, j = indices.size(); i < j; i++) {
            indices[i] += index_offset;
        }

        // Add bones matrices offset
        const auto bone_index_offset = numeric_cast<float32>(combined_mesh->CurBoneMatrix);
        const auto start_vertex = vertices.size() - mesh_data->Vertices.size();
        for (auto i = start_vertex, j = vertices.size(); i < j; i++) {
            for (auto& blend_index : vertices[i].BlendIndices) {
                blend_index += bone_index_offset;
            }
        }
    }

    // Set mesh transform and anim layer
    combined_mesh->Meshes.emplace_back(mesh_data);
    combined_mesh->MeshVertices.emplace_back(numeric_cast<uint32>(vertices.size() - vertices_old_size));
    combined_mesh->MeshIndices.emplace_back(numeric_cast<uint32>(indices.size() - indices_old_size));
    combined_mesh->MeshAnimLayers.emplace_back(anim_layer);

    // Add bones matrices
    for (size_t i = 0; i < mesh_data->SkinBones.size(); i++) {
        combined_mesh->SkinBones[combined_mesh->CurBoneMatrix + i] = mesh_data->SkinBones[i];
        combined_mesh->SkinBoneOffsets[combined_mesh->CurBoneMatrix + i] = mesh_data->SkinBoneOffsets[i];
    }

    combined_mesh->CurBoneMatrix += mesh_data->SkinBones.size();

    // Add textures
    for (size_t i = 0; i < MODEL_MAX_TEXTURES; i++) {
        if (!combined_mesh->Textures[i] && mesh_instance->CurTexures[i]) {
            combined_mesh->Textures[i] = mesh_instance->CurTexures[i];
        }
    }

    // Fix texture coords
    // Todo: move texcoord offset calculation to gpu
    if (mesh_instance->CurTexures[0]) {
        const auto* mesh_tex = mesh_instance->CurTexures[0].get();

        for (auto i = vertices_old_size, j = vertices.size(); i < j; i++) {
            vertices[i].TexCoord[0] = (vertices[i].TexCoord[0] * mesh_tex->AtlasOffsetData.width) + mesh_tex->AtlasOffsetData.x;
            vertices[i].TexCoord[1] = (vertices[i].TexCoord[1] * mesh_tex->AtlasOffsetData.height) + mesh_tex->AtlasOffsetData.y;
        }
    }

    // Increment mesh count
    combined_mesh->EncapsulatedMeshCount++;

    combined_mesh->MeshBuf->VertCount = combined_mesh->MeshBuf->Vertices3D.size();
    combined_mesh->MeshBuf->IndCount = combined_mesh->MeshBuf->Indices.size();
}

void ModelInstance::CutCombinedMeshes(const ModelInstance* cur)
{
    FO_STACK_TRACE_ENTRY();

    // Cut meshes
    if (!cur->_allCuts.empty()) {
        for (const auto& cut : cur->_allCuts) {
            for (size_t i = 0; i < _actualCombinedMeshesCount; i++) {
                CutCombinedMesh(_combinedMeshes[i].get(), cut.get());
            }
        }

        _disableCulling = true;
    }

    // Fill child
    for (const auto& child : cur->_children) {
        CutCombinedMeshes(child.get());
    }
}

// -2 - ignore
// -1 - inside
// 0 - outside
// 1 - one point
static auto SphereLineIntersection(const Vertex3D& p1, const Vertex3D& p2, const vec3& sp, float32 r, Vertex3D& in) -> int32
{
    FO_STACK_TRACE_ENTRY();

    auto sq = [](float32 f) -> float32 { return f * f; };
    const auto a = sq(p2.Position.x - p1.Position.x) + sq(p2.Position.y - p1.Position.y) + sq(p2.Position.z - p1.Position.z);
    const auto b = 2 * ((p2.Position.x - p1.Position.x) * (p1.Position.x - sp.x) + (p2.Position.y - p1.Position.y) * (p1.Position.y - sp.y) + (p2.Position.z - p1.Position.z) * (p1.Position.z - sp.z));
    const auto c = sq(sp.x) + sq(sp.y) + sq(sp.z) + sq(p1.Position.x) + sq(p1.Position.y) + sq(p1.Position.z) - 2 * (sp.x * p1.Position.x + sp.y * p1.Position.y + sp.z * p1.Position.z) - sq(r);
    const auto i = sq(b) - 4 * a * c;

    if (i > 0.0f) {
        const auto sqrt_i = sqrt(i);
        const auto mu1 = (-b + sqrt_i) / (2 * a);
        const auto mu2 = (-b - sqrt_i) / (2 * a);

        // Line segment doesn't intersect and on outside of sphere, in which case both values of u wll either be less
        // than 0 or greater than 1
        if ((mu1 < 0.0f && mu2 < 0.0f) || (mu1 > 1.0f && mu2 > 1.0f)) {
            return 0;
        }

        // Line segment doesn't intersect and is inside sphere, in which case one value of u will be negative and the
        // other greater than 1
        if ((mu1 < 0.0f && mu2 > 1.0f) || (mu2 < 0.0f && mu1 > 1.0f)) {
            return -1;
        }

        // Line segment intersects at one point, in which case one value of u will be between 0 and 1 and the other not
        if ((mu1 >= 0.0f && mu1 <= 1.0f && (mu2 < 0.0f || mu2 > 1.0f)) || (mu2 >= 0.0f && mu2 <= 1.0f && (mu1 < 0.0f || mu1 > 1.0f))) {
            const auto& mu = ((mu1 >= 0.0f && mu1 <= 1.0f) ? mu1 : mu2);
            in = p1;
            in.Position.x = p1.Position.x + mu * (p2.Position.x - p1.Position.x);
            in.Position.y = p1.Position.y + mu * (p2.Position.y - p1.Position.y);
            in.Position.z = p1.Position.z + mu * (p2.Position.z - p1.Position.z);
            in.TexCoord[0] = p1.TexCoord[0] + mu * (p2.TexCoord[0] - p1.TexCoord[0]);
            in.TexCoord[1] = p1.TexCoord[1] + mu * (p2.TexCoord[1] - p1.TexCoord[1]);
            in.TexCoordBase[0] = p1.TexCoordBase[0] + mu * (p2.TexCoordBase[0] - p1.TexCoordBase[0]);
            in.TexCoordBase[1] = p1.TexCoordBase[1] + mu * (p2.TexCoordBase[1] - p1.TexCoordBase[1]);
            return 1;
        }

        // Line segment intersects at two points, in which case both values of u will be between 0 and 1
        if (mu1 >= 0.0f && mu1 <= 1.0f && mu2 >= 0.0f && mu2 <= 1.0f) {
            // Ignore
            return -2;
        }
    }
    else if (i == 0.0f) {
        // Ignore
        return -2;
    }
    return 0;
}

void ModelInstance::CutCombinedMesh(CombinedMesh* combined_mesh, const ModelCutData* cut)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    auto& vertices = combined_mesh->MeshBuf->Vertices3D;
    auto& indices = combined_mesh->MeshBuf->Indices;

    for (const auto& shape : cut->Shapes) {
        vector<Vertex3D> result_vertices;
        vector<vindex_t> result_indices;
        vector<uint32> result_mesh_vertices;
        vector<uint32> result_mesh_indices;

        result_vertices.reserve(vertices.size());
        result_indices.reserve(indices.size());
        result_mesh_vertices.reserve(combined_mesh->MeshVertices.size());
        result_mesh_indices.reserve(combined_mesh->MeshIndices.size());

        uint32 i_pos = 0;
        uint32 i_count = 0;

        for (size_t k = 0, l = combined_mesh->MeshIndices.size(); k < l; k++) {
            // Move shape to face space
            auto mesh_transform = combined_mesh->Meshes[k]->Owner->GlobalTransformationMatrix;
            auto sm = mesh_transform.Inverse() * shape.GlobalTransformationMatrix;
            vec3 ss;
            vec3 sp;
            quaternion sr;
            sm.Decompose(ss, sr, sp);

            // Check anim layer
            const auto mesh_anim_layer = combined_mesh->MeshAnimLayers[k];
            const auto skip = std::ranges::find(cut->Layers, mesh_anim_layer) == cut->Layers.end();

            // Process faces
            i_count += combined_mesh->MeshIndices[k];
            auto vertices_old_size = result_vertices.size();
            auto indices_old_size = result_indices.size();

            for (; i_pos < i_count; i_pos += 3) {
                // Face points
                const auto& v1 = vertices[indices[i_pos + 0]];
                const auto& v2 = vertices[indices[i_pos + 1]];
                const auto& v3 = vertices[indices[i_pos + 2]];

                // Skip mesh
                if (skip) {
                    result_vertices.emplace_back(v1);
                    result_vertices.emplace_back(v2);
                    result_vertices.emplace_back(v3);
                    result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                    result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                    result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                    continue;
                }

                if (shape.IsSphere) {
                    // Find intersections
                    Vertex3D i1;
                    int32 r1 = SphereLineIntersection(v1, v2, sp, shape.SphereRadius * ss.x, i1);
                    Vertex3D i2;
                    int32 r2 = SphereLineIntersection(v2, v3, sp, shape.SphereRadius * ss.x, i2);
                    Vertex3D i3;
                    int32 r3 = SphereLineIntersection(v3, v1, sp, shape.SphereRadius * ss.x, i3);

                    // Process intersections
                    bool outside = (r1 == 0 && r2 == 0 && r3 == 0);
                    bool ignore = (r1 == -2 || r2 == -2 || r3 == -2);
                    int32 sum = r1 + r2 + r3;

                    if (!ignore && sum == 2) {
                        // 1 1 0, corner in
                        const Vertex3D& vv1 = (r1 == 0 ? v1 : (r2 == 0 ? v2 : v3));
                        const Vertex3D& vv2 = (r1 == 0 ? v2 : (r2 == 0 ? v3 : v1));
                        const Vertex3D& vv3 = (r1 == 0 ? i3 : (r2 == 0 ? i1 : i2));
                        const Vertex3D& vv4 = (r1 == 0 ? i2 : (r2 == 0 ? i3 : i1));

                        // First face
                        result_vertices.emplace_back(vv1);
                        result_vertices.emplace_back(vv2);
                        result_vertices.emplace_back(vv3);
                        result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                        result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                        result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));

                        // Second face
                        result_vertices.emplace_back(vv3);
                        result_vertices.emplace_back(vv2);
                        result_vertices.emplace_back(vv4);
                        result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                        result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                        result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                    }
                    else if (!ignore && sum == 1) {
                        // 1 1 -1, corner out
                        const Vertex3D& vv1 = (r1 == -1 ? i3 : (r2 == -1 ? v1 : i1));
                        const Vertex3D& vv2 = (r1 == -1 ? i2 : (r2 == -1 ? i1 : v2));
                        const Vertex3D& vv3 = (r1 == -1 ? v3 : (r2 == -1 ? i3 : i2));

                        // One face
                        result_vertices.emplace_back(vv1);
                        result_vertices.emplace_back(vv2);
                        result_vertices.emplace_back(vv3);
                        result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                        result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                        result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                    }
                    else if (ignore || outside) {
                        if (ignore && sum == 0) {
                            // 1 1 -2
                            continue;
                        }

                        result_vertices.emplace_back(v1);
                        result_vertices.emplace_back(v2);
                        result_vertices.emplace_back(v3);
                        result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                        result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                        result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                    }
                }
                else {
                    if ((v1.Position.x > shape.BBoxMin.x && v1.Position.y > shape.BBoxMin.y && v1.Position.z > shape.BBoxMin.z && v1.Position.x < shape.BBoxMax.x && v1.Position.y < shape.BBoxMax.y && v1.Position.z > shape.BBoxMax.z) || //
                        (v2.Position.x > shape.BBoxMin.x && v2.Position.y > shape.BBoxMin.y && v2.Position.z > shape.BBoxMin.z && v2.Position.x < shape.BBoxMax.x && v2.Position.y < shape.BBoxMax.y && v2.Position.z > shape.BBoxMax.z) || //
                        (v3.Position.x > shape.BBoxMin.x && v3.Position.y > shape.BBoxMin.y && v3.Position.z > shape.BBoxMin.z && v3.Position.x < shape.BBoxMax.x && v3.Position.y < shape.BBoxMax.y && v3.Position.z > shape.BBoxMax.z)) {
                        continue;
                    }

                    result_vertices.emplace_back(v1);
                    result_vertices.emplace_back(v2);
                    result_vertices.emplace_back(v3);
                    result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                    result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                    result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                }
            }

            result_mesh_vertices.emplace_back(numeric_cast<uint32>(result_vertices.size() - vertices_old_size));
            result_mesh_indices.emplace_back(numeric_cast<uint32>(result_indices.size() - indices_old_size));
        }

        vertices = result_vertices;
        indices = result_indices;
        combined_mesh->MeshVertices = result_mesh_vertices;
        combined_mesh->MeshIndices = result_mesh_indices;
    }

    // Unskin
    if (cut->UnskinBone1 && cut->UnskinBone2) {
        // Find unskin bones
        ModelBone* unskin_bone1 = nullptr;
        float32 unskin_bone1_index = 0.0f;
        ModelBone* unskin_bone2 = nullptr;
        float32 unskin_bone2_index = 0.0f;

        for (size_t i = 0; i < combined_mesh->CurBoneMatrix; i++) {
            if (combined_mesh->SkinBones[i]->Name == cut->UnskinBone1) {
                unskin_bone1 = combined_mesh->SkinBones[i].get();
                unskin_bone1_index = numeric_cast<float32>(i);
            }
            if (combined_mesh->SkinBones[i]->Name == cut->UnskinBone2) {
                unskin_bone2 = combined_mesh->SkinBones[i].get();
                unskin_bone2_index = numeric_cast<float32>(i);
            }
            if (unskin_bone1 != nullptr && unskin_bone2 != nullptr) {
                break;
            }
        }

        // Unskin
        if (unskin_bone1 != nullptr && unskin_bone2 != nullptr) {
            // Process meshes
            size_t v_pos = 0;
            size_t v_count = 0;

            for (size_t i = 0, j = combined_mesh->MeshVertices.size(); i < j; i++) {
                // Check anim layer
                if (std::ranges::find(cut->Layers, combined_mesh->MeshAnimLayers[i]) == cut->Layers.end()) {
                    v_count += combined_mesh->MeshVertices[i];
                    v_pos = v_count;
                    continue;
                }

                // Move shape to face space
                auto mesh_transform = combined_mesh->Meshes[i]->Owner->GlobalTransformationMatrix;
                auto sm = mesh_transform.Inverse() * cut->UnskinShape.GlobalTransformationMatrix;
                vec3 ss;
                vec3 sp;
                quaternion sr;
                sm.Decompose(ss, sr, sp);
                auto sphere_square_radius = powf(cut->UnskinShape.SphereRadius * ss.x, 2.0f);
                auto revert_shape = cut->RevertUnskinShape;

                // Process mesh vertices
                v_count += combined_mesh->MeshVertices[i];

                for (; v_pos < v_count; v_pos++) {
                    auto& v = vertices[v_pos];

                    // Get vertex side
                    auto v_side = ((v.Position - sp).SquareLength() <= sphere_square_radius);
                    if (revert_shape) {
                        v_side = !v_side;
                    }

                    // Check influences
                    for (size_t b = 0; b < BONES_PER_VERTEX; b++) {
                        // No influence
                        auto w = v.BlendWeights[b];

                        if (w < 0.00001f) {
                            continue;
                        }

                        // Last influence, don't reskin
                        if (w > 1.0f - 0.00001f) {
                            break;
                        }

                        // Skip equal influence side
                        bool influence_side = unskin_bone1->Find(combined_mesh->SkinBones[iround<int32>(v.BlendIndices[b])]->Name) != nullptr;

                        if (v_side == influence_side) {
                            continue;
                        }

                        // Last influence, don't reskin
                        if (w > 1.0f - 0.00001f) {
                            v.BlendIndices[b] = (influence_side ? unskin_bone2_index : unskin_bone1_index);
                            v.BlendWeights[b] = 1.0f;
                            break;
                        }

                        // Move influence to other bones
                        v.BlendWeights[b] = 0.0f;
                        for (auto& blend_weight : v.BlendWeights) {
                            blend_weight += blend_weight / (1.0f - w) * w;
                        }
                    }
                }
            }
        }
    }

    combined_mesh->MeshBuf->VertCount = combined_mesh->MeshBuf->Vertices3D.size();
    combined_mesh->MeshBuf->IndCount = combined_mesh->MeshBuf->Indices.size();
}

auto ModelInstance::NeedDraw() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return GetTime() - _lastDrawTime >= std::chrono::milliseconds(_modelMngr->_animUpdateThreshold);
}

void ModelInstance::Draw()
{
    FO_STACK_TRACE_ENTRY();

    const auto time = GetTime();
    const auto dt = 0.001f * (time - _lastDrawTime).to_ms<float32>();

    _lastDrawTime = time;
    _forceDraw = false;

    // Move animation
    const auto w = _frameSize.width / FRAME_SCALE;
    const auto h = _frameSize.height / FRAME_SCALE;
    ProcessAnimation(dt, {w / 2, h - h / 4}, const_numeric_cast<float32>(FRAME_SCALE));

    if (_actualCombinedMeshesCount != 0) {
        for (size_t i = 0; i < _actualCombinedMeshesCount; i++) {
            DrawCombinedMesh(_combinedMeshes[i].get(), _shadowDisabled || _modelInfo->_shadowDisabled);
        }
    }

    DrawAllParticles();
}

void ModelInstance::ProcessAnimation(float32 elapsed, ipos32 pos, float32 scale)
{
    FO_STACK_TRACE_ENTRY();

    // Update world matrix, only for root
    if (!_parentBone) {
        const auto pos3d = Convert2dTo3d(pos);
        mat44 mat_rot_y;
        mat44 mat_scale;
        mat44 mat_trans;
        mat44::Scaling(vec3(scale, scale, scale), mat_scale);
        mat44::RotationY((_moveDirAngle + (_isMovingBack ? 180.0f : 0.0f)) * DEG_TO_RAD_FLOAT, mat_rot_y);
        mat44::Translation(pos3d, mat_trans);

        _parentMatrix = mat_trans * _matTransBase * _matRot * mat_rot_y * _matRotBase * mat_scale * _matScale * _matScaleBase;
        _groundPos.x = _parentMatrix.a4;
        _groundPos.y = _parentMatrix.b4;
        _groundPos.z = _parentMatrix.c4;
    }

    // Rotate body
    if (!is_float_equal(_moveDirAngle, _targetMoveDirAngle)) {
        const auto diff = GeometryHelper::GetDirAngleDiffSided(_moveDirAngle, _targetMoveDirAngle);
        _moveDirAngle += std::clamp(diff * elapsed * 10.0f, -std::abs(diff), std::abs(diff));
    }

    // Advance animation time
    float32 prev_track_pos = 0.0f;
    float32 new_track_pos = 0.0f;

    if (_bodyAnimController && elapsed >= 0.0f) {
        prev_track_pos = _bodyAnimController->GetTrackPosition(_curTrack);

        _bodyAnimController->AdvanceTime(elapsed * GetSpeed());

        if ((_isMoving || _turnAnimPlaying) && _moveAnimController) {
            _moveAnimController->AdvanceTime(elapsed * GetSpeed());

            if (_turnAnimPlaying && !_moveAnimController->GetTrackEnable(_curMoveTrack)) {
                _turnAnimPlaying = false;
                RefreshMoveAnimation();
            }
        }

        new_track_pos = _bodyAnimController->GetTrackPosition(_curTrack);

        if (_animDuration > 0.0f) {
            _animPosProc = new_track_pos / _animDuration;

            if (_animPosProc >= 1.0f) {
                _animPosProc = std::fmod(_animPosProc, 1.0f);
            }

            _animPosTime = new_track_pos;

            if (_animPosTime >= _animDuration) {
                _animPosTime = std::fmod(_animPosTime, _animDuration);
            }
        }
    }

    // Update matrices
    UpdateBoneMatrices(_modelInfo->_hierarchy->_rootBone.get(), &_parentMatrix);

    // Update linked matrices
    if (_parentBone && !_linkBones.empty()) {
        const auto link_bones_count = _linkBones.size() / 2;

        for (size_t i = 0; i < link_bones_count; i++) {
            _linkBones[i * 2 + 1]->CombinedTransformationMatrix = _linkBones[i * 2]->CombinedTransformationMatrix;
        }
    }

    // Update world matrices for children
    for (auto& child : _children) {
        child->_groundPos = _groundPos;
        child->_parentMatrix = child->_parentBone->CombinedTransformationMatrix * child->_matTransBase * child->_matRotBase * child->_matScaleBase;
    }

    // Particles
    for (auto& model_particle : _modelParticles) {
        if (model_particle.Id == 0) {
            model_particle.Particle->Setup(_frameProj, model_particle.Bone->CombinedTransformationMatrix, model_particle.Move, model_particle.Rot, _moveOffset);
        }
        else {
            model_particle.Particle->Setup(_frameProj, model_particle.Bone->CombinedTransformationMatrix, model_particle.Move, model_particle.Rot + _lookDirAngle, _moveOffset);
        }
    }

    for (auto it = _modelParticles.begin(); it != _modelParticles.end();) {
        if (!it->Particle->IsActive()) {
            it = _modelParticles.erase(it);
        }
        else {
            ++it;
        }
    }

    // Move child animations
    for (auto& child : _children) {
        child->ProcessAnimation(elapsed, pos, 1.0f);
    }

    // Animation callbacks
    if (_bodyAnimController && elapsed >= 0.0f && _animDuration > 0.0f) {
        for (auto& callback : AnimationCallbacks) {
            if ((callback.StateAnim == CritterStateAnim::None || callback.StateAnim == _curStateAnim) && (callback.ActionAnim == CritterActionAnim::None || callback.ActionAnim == _curActionAnim)) {
                const auto fire_track_pos1 = floorf(prev_track_pos / _animDuration) * _animDuration + callback.NormalizedTime * _animDuration;
                const auto fire_track_pos2 = floorf(new_track_pos / _animDuration) * _animDuration + callback.NormalizedTime * _animDuration;

                if ((prev_track_pos < fire_track_pos1 && new_track_pos >= fire_track_pos1) || (prev_track_pos < fire_track_pos2 && new_track_pos >= fire_track_pos2)) {
                    callback.Callback();
                }
            }
        }
    }
}

void ModelInstance::UpdateBoneMatrices(ModelBone* bone, const mat44* parent_matrix)
{
    FO_STACK_TRACE_ENTRY();

    if (_modelInfo->_rotationBone && bone->Name == _modelInfo->_rotationBone && !is_float_equal(_lookDirAngle, _moveDirAngle)) {
        mat44 mat_rot;
        mat44::RotationX((GeometryHelper::GetDirAngleDiffSided(_lookDirAngle + (_isMovingBack ? 180.0f : 0.0f), _moveDirAngle) * -_modelMngr->_settings->CritterBodyTurnFactor) * DEG_TO_RAD_FLOAT, mat_rot);
        bone->CombinedTransformationMatrix = *parent_matrix * mat_rot * bone->TransformationMatrix;
    }
    else if (_modelInfo->_rotationBone && bone->Name == _modelMngr->_headBone && !is_float_equal(_lookDirAngle, _moveDirAngle)) {
        mat44 mat_rot;
        mat44::RotationX((GeometryHelper::GetDirAngleDiffSided(_lookDirAngle + (_isMovingBack ? 180.0f : 0.0f), _moveDirAngle) * -_modelMngr->_settings->CritterHeadTurnFactor) * DEG_TO_RAD_FLOAT, mat_rot);
        bone->CombinedTransformationMatrix = *parent_matrix * mat_rot * bone->TransformationMatrix;
    }
    else {
        bone->CombinedTransformationMatrix = *parent_matrix * bone->TransformationMatrix;
    }

    // Update child
    for (auto it = bone->Children.begin(), end = bone->Children.end(); it != end; ++it) {
        UpdateBoneMatrices(it->get(), &bone->CombinedTransformationMatrix);
    }
}

void ModelInstance::DrawCombinedMesh(CombinedMesh* combined_mesh, bool shadow_disabled)
{
    FO_STACK_TRACE_ENTRY();

    auto* effect = combined_mesh->DrawEffect ? combined_mesh->DrawEffect.get() : _modelMngr->_effectMngr->Effects.SkinnedModel.get();

    auto& proj_buf = effect->ProjBuf = RenderEffect::ProjBuffer();
    MemCopy(proj_buf->ProjMatrix, _frameProjColMaj[0], 16 * sizeof(float32));

    effect->MainTex = combined_mesh->Textures[0] ? combined_mesh->Textures[0]->MainTex : nullptr;

    // Todo: merge all bones in one hierarchy and disable offset copying
    auto& model_buf = effect->ModelBuf = RenderEffect::ModelBuffer();

    auto* wm = model_buf->WorldMatrices;

    for (size_t i = 0; i < combined_mesh->CurBoneMatrix; i++) {
        auto m = combined_mesh->SkinBones[i]->CombinedTransformationMatrix * combined_mesh->SkinBoneOffsets[i];
        m.Transpose(); // Convert to column major order
        MemCopy(wm, m[0], 16 * sizeof(float32));
        wm += 16;
    }

    effect->MatrixCount = combined_mesh->CurBoneMatrix;

    MemCopy(model_buf->GroundPosition, &_groundPos, 3 * sizeof(float32));
    model_buf->GroundPosition[3] = 0.0f;

    MemCopy(model_buf->LightColor, &_modelMngr->_lightColor, 4 * sizeof(float32));

    if (effect->IsNeedModelTexBuf()) {
        auto& custom_tex_buf = effect->ModelTexBuf = RenderEffect::ModelTexBuffer();

        for (size_t i = 0; i < MODEL_MAX_TEXTURES; i++) {
            if (combined_mesh->Textures[i]) {
                effect->ModelTex[i] = combined_mesh->Textures[i]->MainTex;
                MemCopy(&custom_tex_buf->TexAtlasOffset[i * 4 * sizeof(float32)], &combined_mesh->Textures[i]->AtlasOffsetData, 4 * sizeof(float32));
                MemCopy(&custom_tex_buf->TexSize[i * 4 * sizeof(float32)], combined_mesh->Textures[i]->MainTex->SizeData, 4 * sizeof(float32));
            }
            else {
                effect->ModelTex[i] = nullptr;
            }
        }
    }

    if (effect->IsNeedModelAnimBuf()) {
        auto& anim_buf = effect->ModelAnimBuf = RenderEffect::ModelAnimBuffer();

        anim_buf->AnimNormalizedTime[0] = _animPosProc;
        anim_buf->AnimAbsoluteTime[0] = _animPosTime;
    }

    effect->DisableCulling = _disableCulling;
    effect->DisableShadow = shadow_disabled;

    combined_mesh->MeshBuf->Upload(effect->GetUsage());
    effect->DrawBuffer(combined_mesh->MeshBuf.get());
}

void ModelInstance::DrawAllParticles()
{
    FO_STACK_TRACE_ENTRY();

    for (auto& model_particle : _modelParticles) {
        model_particle.Particle->Draw();
    }

    for (auto& child : _children) {
        child->DrawAllParticles();
    }
}

auto ModelInstance::FindBone(hstring bone_name) const noexcept -> const ModelBone*
{
    FO_STACK_TRACE_ENTRY();

    const auto* bone = _modelInfo->_hierarchy->_rootBone->Find(bone_name);

    if (bone == nullptr) {
        for (const auto& child : _children) {
            bone = child->_modelInfo->_hierarchy->_rootBone->Find(bone_name);

            if (bone != nullptr) {
                break;
            }
        }
    }

    return bone;
}

auto ModelInstance::GetBonePos(hstring bone_name) const -> optional<ipos32>
{
    FO_STACK_TRACE_ENTRY();

    const auto* bone = FindBone(bone_name);

    if (bone == nullptr) {
        return std::nullopt;
    }

    vec3 pos;
    quaternion rot;
    bone->CombinedTransformationMatrix.DecomposeNoScaling(rot, pos);

    const auto p = Convert3dTo2d(pos);
    const auto x = p.x - _frameSize.width / FRAME_SCALE / 2;
    const auto y = -(p.y - _frameSize.height / FRAME_SCALE / 4);

    return ipos32 {x, y};
}

auto ModelInstance::GetAnimDuration() const -> timespan
{
    FO_STACK_TRACE_ENTRY();

    return std::chrono::milliseconds(iround<int32>(_animDuration * 1000.0f));
}

void ModelInstance::RunParticle(string_view particle_name, hstring bone_name, vec3 move)
{
    FO_STACK_TRACE_ENTRY();

    if (const auto* to_bone = FindBone(bone_name)) {
        if (auto particle = _modelMngr->_particleMngr.CreateParticle(particle_name)) {
            _modelParticles.emplace_back(ModelParticleSystem {0, std::move(particle), to_bone, move, _lookDirAngle});
        }
    }
}

ModelInformation::ModelInformation(ModelManager& model_mngr) :
    _modelMngr {&model_mngr}
{
    FO_STACK_TRACE_ENTRY();

    _drawSize.width = _modelMngr->_settings->DefaultModelDrawWidth;
    _drawSize.height = _modelMngr->_settings->DefaultModelDrawHeight;
    _viewSize.width = _modelMngr->_settings->DefaultModelViewWidth != 0 ? _modelMngr->_settings->DefaultModelViewWidth : _drawSize.width / 4;
    _viewSize.height = _modelMngr->_settings->DefaultModelViewHeight != 0 ? _modelMngr->_settings->DefaultModelViewHeight : _viewSize.height / 2;
}

auto ModelInformation::Load(string_view name) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const string ext = strex(name).get_file_extension();

    if (ext.empty()) {
        return false;
    }

    // Load fonline 3d file
    if (ext == "fo3d") {
        // Load main fo3d file
        const auto fo3d = _modelMngr->_resources->ReadFile(name);

        if (!fo3d) {
            return false;
        }

        // Parse
        string file_buf = fo3d.GetStr();
        string model;
        string render_fname;
        string render_anim;
        bool disable_animation_interpolation = false;
        bool convert_value_fail = false;

        hstring mesh;
        int32 layer = -1;
        int32 layer_val = 0;

        ModelAnimationData dummy_link {};
        auto* link = &_animDataDefault;

        auto istr = SafeAlloc::MakeUnique<istringstream>(file_buf);
        string token;
        string buf;

        struct AnimEntry
        {
            CritterStateAnim StateAnim;
            CritterActionAnim ActionAnim;
            string FileName;
            string Name;
        };

        vector<AnimEntry> anim_entries;

        // Todo: move fo3d parsing to baking
        while (!istr->eof()) {
            *istr >> token;

            if (istr->eof()) {
                break;
            }

            FO_RUNTIME_ASSERT(!istr->fail());

            auto comment = token.find(';');

            if (comment == string::npos) {
                comment = token.find('#');
            }

            if (comment != string::npos) {
                token = token.substr(0, comment);

                string line;
                std::getline(*istr, line, '\n');
                FO_RUNTIME_ASSERT(!istr->fail());
            }

            if (token.empty()) {
                // None
            }
            else if (token == "Model") {
                *istr >> buf;
                model = strex(name).extract_dir().combine_path(buf);
            }
            else if (token == "Include") {
                // Get swapped words
                vector<string> templates;
                string line;
                std::getline(*istr, line, '\n');
                FO_RUNTIME_ASSERT(!istr->fail());

                templates = strex(line).trim().split(' ');
                FO_RUNTIME_ASSERT(!templates.empty());

                for (size_t i = 1; i < templates.size() - 1; i += 2) {
                    templates[i] = strex("%{}%", templates[i]);
                }

                // Include file path
                const string fname = strex(name).extract_dir().combine_path(templates[0]);
                const auto fo3d_ex = _modelMngr->_resources->ReadFile(fname);

                if (fo3d_ex) {
                    string new_content = fo3d_ex.GetStr();

                    for (size_t i = 1; i < templates.size() - 1; i += 2) {
                        new_content = strex(new_content).replace(templates[i], templates[i + 1]);
                    }

                    const auto offs = numeric_cast<size_t>(static_cast<std::streamoff>(istr->tellg()));
                    const auto rest_content = !istr->eof() ? file_buf.substr(offs) : "";
                    file_buf = strex("{}\n{}", new_content, rest_content);
                    istr = SafeAlloc::MakeUnique<istringstream>(file_buf);
                }
                else {
                    WriteLog("Include file '{}' not found", fname);
                }
            }
            else if (token == "Mesh") {
                *istr >> buf;

                if (buf != "All") {
                    mesh = _modelMngr->GetBoneHashedString(buf);
                }
                else {
                    mesh = {};
                }
            }
            else if (token == "Subset") {
                *istr >> buf;
                WriteLog("Tag 'Subset' obsolete, use 'Mesh' instead");
            }
            else if (token == "Layer" || token == "Value") {
                *istr >> buf;

                if (token == "Layer") {
                    layer = _modelMngr->_nameResolver->ResolveGenericValue(buf, &convert_value_fail);
                }
                else {
                    layer_val = _modelMngr->_nameResolver->ResolveGenericValue(buf, &convert_value_fail);
                }

                link = &dummy_link;
                mesh = {};
            }
            else if (token == "Root") {
                if (layer == -1) {
                    link = &_animDataDefault;
                }
                else if (layer_val == 0) {
                    WriteLog("Wrong layer '{}' zero value", layer);
                    link = &dummy_link;
                }
                else {
                    link = &_animData.emplace_back();
                    link->Id = ++_modelMngr->_linkId;
                    link->Layer = layer;
                    link->LayerValue = layer_val;
                }

                mesh = {};
            }
            else if (token == "Attach") {
                *istr >> buf;

                if (layer >= 0 && layer_val != 0) {
                    link = &_animData.emplace_back();
                    link->Id = ++_modelMngr->_linkId;
                    link->Layer = layer;
                    link->LayerValue = layer_val;

                    string fname = strex(name).extract_dir().combine_path(buf);
                    link->ChildName = fname;
                    link->IsParticles = false;
                }

                mesh = {};
            }
            else if (token == "AttachParticles") {
                *istr >> buf;

                if (layer >= 0 && layer_val != 0) {
                    link = &_animData.emplace_back();
                    link->Id = ++_modelMngr->_linkId;
                    link->Layer = layer;
                    link->LayerValue = layer_val;

                    link->ChildName = buf;
                    link->IsParticles = true;
                }

                mesh = {};
            }
            else if (token == "Link") {
                *istr >> buf;

                if (link->Id != 0) {
                    link->LinkBone = _modelMngr->GetBoneHashedString(buf);
                }
            }
            else if (token == "Cut") {
                *istr >> buf;
                string fname = strex(name).extract_dir().combine_path(buf);
                auto* area = _modelMngr->GetHierarchy(fname);

                if (area != nullptr) {
                    // Add cut
                    auto* cut = SafeAlloc::MakeRaw<ModelCutData>();
                    link->CutInfo.emplace_back(cut);

                    // Layers
                    *istr >> buf;
                    auto cur_layer_names = strex(buf).split('-');

                    for (auto& cut_layer_name : cur_layer_names) {
                        if (cut_layer_name != "All") {
                            const auto cut_layer = _modelMngr->_nameResolver->ResolveGenericValue(cut_layer_name, &convert_value_fail);
                            cut->Layers.emplace_back(cut_layer);
                        }
                        else {
                            for (int32 i = 0; i < numeric_cast<int32>(MODEL_LAYERS_COUNT); i++) {
                                if (i != layer) {
                                    cut->Layers.emplace_back(i);
                                }
                            }
                        }
                    }

                    // Shapes
                    *istr >> buf;
                    auto shapes = strex(buf).split('-');

                    // Unskin bones
                    *istr >> buf;
                    cut->UnskinBone1 = buf != "-" ? _modelMngr->GetBoneHashedString(buf) : hstring();

                    *istr >> buf;
                    cut->UnskinBone2 = buf != "-" ? _modelMngr->GetBoneHashedString(buf) : hstring();

                    // Unskin shape
                    *istr >> buf;
                    hstring unskin_shape_name;
                    cut->RevertUnskinShape = false;

                    if (cut->UnskinBone1 && cut->UnskinBone2) {
                        cut->RevertUnskinShape = !buf.empty() && buf[0] == '~';
                        unskin_shape_name = _modelMngr->GetBoneHashedString(!buf.empty() && buf[0] == '~' ? buf.substr(1) : buf);

                        for (auto& bone : area->_allDrawBones) {
                            if (unskin_shape_name == bone->Name) {
                                cut->UnskinShape = CreateCutShape(bone->AttachedMesh.get());
                            }
                        }
                    }

                    // Parse shapes
                    for (auto& shape : shapes) {
                        auto shape_name = _modelMngr->GetBoneHashedString(shape);

                        if (shape == "All") {
                            shape_name = hstring();
                        }

                        for (auto& bone : area->_allDrawBones) {
                            if ((!shape_name || shape_name == bone->Name) && bone->Name != unskin_shape_name) {
                                cut->Shapes.emplace_back(CreateCutShape(bone->AttachedMesh.get()));
                            }
                        }
                    }
                }
                else {
                    WriteLog("Cut file '{}' not found", fname);
                    *istr >> buf;
                    *istr >> buf;
                    *istr >> buf;
                    *istr >> buf;
                    *istr >> buf;
                }
            }
            else if (token == "RotX") {
                *istr >> link->RotX;
            }
            else if (token == "RotY") {
                *istr >> link->RotY;
            }
            else if (token == "RotZ") {
                *istr >> link->RotZ;
            }
            else if (token == "MoveX") {
                *istr >> link->MoveX;
            }
            else if (token == "MoveY") {
                *istr >> link->MoveY;
            }
            else if (token == "MoveZ") {
                *istr >> link->MoveZ;
            }
            else if (token == "ScaleX") {
                *istr >> link->ScaleX;
            }
            else if (token == "ScaleY") {
                *istr >> link->ScaleY;
            }
            else if (token == "ScaleZ") {
                *istr >> link->ScaleZ;
            }
            else if (token == "Scale") {
                float32 valuef = 0.0f;
                *istr >> valuef;
                link->ScaleX = link->ScaleY = link->ScaleZ = valuef;
            }
            else if (token == "Speed") {
                *istr >> link->SpeedAjust;
            }
            else if (token == "RotX+") {
                float32 valuef = 0.0f;
                *istr >> valuef;
                link->RotX = (link->RotX == 0.0f ? valuef : link->RotX + valuef);
            }
            else if (token == "RotY+") {
                float32 valuef = 0.0f;
                *istr >> valuef;
                link->RotY = (link->RotY == 0.0f ? valuef : link->RotY + valuef);
            }
            else if (token == "RotZ+") {
                float32 valuef = 0.0f;
                *istr >> valuef;
                link->RotZ = (link->RotZ == 0.0f ? valuef : link->RotZ + valuef);
            }
            else if (token == "MoveX+") {
                float32 valuef = 0.0f;
                *istr >> valuef;
                link->MoveX = (link->MoveX == 0.0f ? valuef : link->MoveX + valuef);
            }
            else if (token == "MoveY+") {
                float32 valuef = 0.0f;
                *istr >> valuef;
                link->MoveY = (link->MoveY == 0.0f ? valuef : link->MoveY + valuef);
            }
            else if (token == "MoveZ+") {
                float32 valuef = 0.0f;
                *istr >> valuef;
                link->MoveZ = (link->MoveZ == 0.0f ? valuef : link->MoveZ + valuef);
            }
            else if (token == "ScaleX+") {
                float32 valuef = 0.0f;
                *istr >> valuef;
                link->ScaleX = (link->ScaleX == 0.0f ? valuef : link->ScaleX + valuef);
            }
            else if (token == "ScaleY+") {
                float32 valuef = 0.0f;
                *istr >> valuef;
                link->ScaleY = (link->ScaleY == 0.0f ? valuef : link->ScaleY + valuef);
            }
            else if (token == "ScaleZ+") {
                float32 valuef = 0.0f;
                *istr >> valuef;
                link->ScaleZ = (link->ScaleZ == 0.0f ? valuef : link->ScaleZ + valuef);
            }
            else if (token == "Scale+") {
                float32 valuef = 0.0f;
                *istr >> valuef;
                link->ScaleX = (link->ScaleX == 0.0f ? valuef : link->ScaleX + valuef);
                link->ScaleY = (link->ScaleY == 0.0f ? valuef : link->ScaleY + valuef);
                link->ScaleZ = (link->ScaleZ == 0.0f ? valuef : link->ScaleZ + valuef);
            }
            else if (token == "Speed+") {
                float32 valuef = 0.0f;
                *istr >> valuef;
                link->SpeedAjust = (link->SpeedAjust == 0.0f ? valuef : link->SpeedAjust * valuef);
            }
            else if (token == "RotX*") {
                float32 valuef = 0.0f;
                *istr >> valuef;
                link->RotX = (link->RotX == 0.0f ? valuef : link->RotX * valuef);
            }
            else if (token == "RotY*") {
                float32 valuef = 0.0f;
                *istr >> valuef;
                link->RotY = (link->RotY == 0.0f ? valuef : link->RotY * valuef);
            }
            else if (token == "RotZ*") {
                float32 valuef = 0.0f;
                *istr >> valuef;
                link->RotZ = (link->RotZ == 0.0f ? valuef : link->RotZ * valuef);
            }
            else if (token == "MoveX*") {
                float32 valuef = 0.0f;
                *istr >> valuef;
                link->MoveX = (link->MoveX == 0.0f ? valuef : link->MoveX * valuef);
            }
            else if (token == "MoveY*") {
                float32 valuef = 0.0f;
                *istr >> valuef;
                link->MoveY = (link->MoveY == 0.0f ? valuef : link->MoveY * valuef);
            }
            else if (token == "MoveZ*") {
                float32 valuef = 0.0f;
                *istr >> valuef;
                link->MoveZ = (link->MoveZ == 0.0f ? valuef : link->MoveZ * valuef);
            }
            else if (token == "ScaleX*") {
                float32 valuef = 0.0f;
                *istr >> valuef;
                link->ScaleX = (link->ScaleX == 0.0f ? valuef : link->ScaleX * valuef);
            }
            else if (token == "ScaleY*") {
                float32 valuef = 0.0f;
                *istr >> valuef;
                link->ScaleY = (link->ScaleY == 0.0f ? valuef : link->ScaleY * valuef);
            }
            else if (token == "ScaleZ*") {
                float32 valuef = 0.0f;
                *istr >> valuef;
                link->ScaleZ = (link->ScaleZ == 0.0f ? valuef : link->ScaleZ * valuef);
            }
            else if (token == "Scale*") {
                float32 valuef = 0.0f;
                *istr >> valuef;
                link->ScaleX = (link->ScaleX == 0.0f ? valuef : link->ScaleX * valuef);
                link->ScaleY = (link->ScaleY == 0.0f ? valuef : link->ScaleY * valuef);
                link->ScaleZ = (link->ScaleZ == 0.0f ? valuef : link->ScaleZ * valuef);
            }
            else if (token == "Speed*") {
                float32 valuef = 0.0f;
                *istr >> valuef;
                link->SpeedAjust = (link->SpeedAjust == 0.0f ? valuef : link->SpeedAjust * valuef);
            }
            else if (token == "DisableLayer") {
                *istr >> buf;
                const auto disabled_layers = strex(buf).split('-');

                for (const auto& disabled_layer_name : disabled_layers) {
                    const auto disabled_layer = _modelMngr->_nameResolver->ResolveGenericValue(disabled_layer_name, &convert_value_fail);

                    if (disabled_layer >= 0 && disabled_layer < const_numeric_cast<int32>(MODEL_LAYERS_COUNT)) {
                        link->DisabledLayer.emplace_back(disabled_layer);
                    }
                }
            }
            else if (token == "DisableMesh") {
                *istr >> buf;
                const auto disabled_mesh_names = strex(buf).split('-');

                for (const auto& disabled_mesh_name : disabled_mesh_names) {
                    hstring disabled_mesh;

                    if (disabled_mesh_name != "All") {
                        disabled_mesh = _modelMngr->GetBoneHashedString(disabled_mesh_name);
                    }

                    link->DisabledMesh.emplace_back(disabled_mesh);
                }
            }
            else if (token == "Texture") {
                *istr >> buf;
                auto index = _modelMngr->_nameResolver->ResolveGenericValue(buf, &convert_value_fail);

                *istr >> buf;

                if (index >= 0 && index < const_numeric_cast<int32>(MODEL_MAX_TEXTURES)) {
                    link->TextureInfo.emplace_back(buf, mesh, index);
                }
            }
            else if (token == "Effect") {
                *istr >> buf;

                link->EffectInfo.emplace_back(buf, mesh);
            }
            else if (token == "Anim") {
                *istr >> buf;
                const auto ind1 = _modelMngr->_nameResolver->ResolveGenericValue(buf, &convert_value_fail);
                *istr >> buf;
                const auto ind2 = _modelMngr->_nameResolver->ResolveGenericValue(buf, &convert_value_fail);

                string a1;
                string a2;
                *istr >> a1 >> a2;
                anim_entries.emplace_back(AnimEntry {.StateAnim = static_cast<CritterStateAnim>(ind1), .ActionAnim = static_cast<CritterActionAnim>(ind2), .FileName = a1, .Name = a2});
            }
            else if (token == "AnimSpeed") {
                *istr >> buf;
                const auto ind1 = _modelMngr->_nameResolver->ResolveGenericValue(buf, &convert_value_fail);
                *istr >> buf;
                const auto ind2 = _modelMngr->_nameResolver->ResolveGenericValue(buf, &convert_value_fail);

                float32 valuef = 0.0f;
                *istr >> valuef;
                _animSpeed.emplace(std::make_pair(static_cast<CritterStateAnim>(ind1), static_cast<CritterActionAnim>(ind2)), valuef);
            }
            else if (token == "AnimLayerValue") {
                *istr >> buf;
                const auto ind1 = _modelMngr->_nameResolver->ResolveGenericValue(buf, &convert_value_fail);
                *istr >> buf;
                const auto ind2 = _modelMngr->_nameResolver->ResolveGenericValue(buf, &convert_value_fail);

                *istr >> buf;
                const auto anim_layer = _modelMngr->_nameResolver->ResolveGenericValue(buf, &convert_value_fail);
                *istr >> buf;
                const auto anim_layer_value = _modelMngr->_nameResolver->ResolveGenericValue(buf, &convert_value_fail);

                const auto index = std::make_pair(static_cast<CritterStateAnim>(ind1), static_cast<CritterActionAnim>(ind2));

                if (_animLayerValues.count(index) == 0) {
                    _animLayerValues.emplace(index, vector<pair<int32, int32>>());
                }

                _animLayerValues[index].emplace_back(anim_layer, anim_layer_value);
            }
            else if (token == "FastTransitionBone") {
                *istr >> buf;
                _fastTransitionBones.insert(_modelMngr->GetBoneHashedString(buf));
            }
            else if (token == "StateAnimEqual") {
                auto ind1 = 0;
                auto ind2 = 0;
                *istr >> buf;
                ind1 = _modelMngr->_nameResolver->ResolveGenericValue(buf, &convert_value_fail);
                *istr >> buf;
                ind2 = _modelMngr->_nameResolver->ResolveGenericValue(buf, &convert_value_fail);

                _stateAnimEquals.emplace(static_cast<CritterStateAnim>(ind1), static_cast<CritterStateAnim>(ind2));
            }
            else if (token == "ActionAnimEqual") {
                auto ind1 = 0;
                auto ind2 = 0;
                *istr >> buf;
                ind1 = _modelMngr->_nameResolver->ResolveGenericValue(buf, &convert_value_fail);
                *istr >> buf;
                ind2 = _modelMngr->_nameResolver->ResolveGenericValue(buf, &convert_value_fail);

                _actionAnimEquals.emplace(static_cast<CritterActionAnim>(ind1), static_cast<CritterActionAnim>(ind2));
            }
            else if (token == "DisableShadow") {
                _shadowDisabled = true;
            }
            else if (token == "DrawSize") {
                *istr >> buf;
                _drawSize.width = _modelMngr->_nameResolver->ResolveGenericValue(buf, &convert_value_fail);
                *istr >> buf;
                _drawSize.height = _modelMngr->_nameResolver->ResolveGenericValue(buf, &convert_value_fail);
            }
            else if (token == "ViewSize") {
                *istr >> buf;
                _viewSize.width = _modelMngr->_nameResolver->ResolveGenericValue(buf, &convert_value_fail);
                *istr >> buf;
                _viewSize.height = _modelMngr->_nameResolver->ResolveGenericValue(buf, &convert_value_fail);
            }
            else if (token == "DisableAnimationInterpolation") {
                disable_animation_interpolation = true;
            }
            else if (token == "RotationBone") {
                *istr >> buf;
                _rotationBone = _modelMngr->GetBoneHashedString(buf);
            }
            else {
                WriteLog("Unknown token '{}' in file '{}'", token, name);
            }

            if (istr->fail()) {
                WriteLog("Failed to process token {} in file '{}'", token, name);
                BreakIntoDebugger();
                break;
            }
        }

        // Process pathes
        if (model.empty()) {
            WriteLog("'Model' section not found in file '{}'", name);
            return false;
        }

        // Check for correct param values
        if (convert_value_fail) {
            WriteLog("Invalid param values for file '{}'", name);
            return false;
        }

        // Load x file
        auto* hierarchy = _modelMngr->GetHierarchy(model);

        if (hierarchy == nullptr) {
            return false;
        }

        // Create animation
        _fileName = name;
        _hierarchy = hierarchy;

        // Single frame render
        if (!render_fname.empty() && !render_anim.empty()) {
            anim_entries.emplace_back(AnimEntry {.StateAnim = CritterStateAnim::None, .ActionAnim = CritterActionAnim::None, .FileName = render_fname, .Name = render_anim});
        }

        // Create animation controller
        if (!anim_entries.empty()) {
            _animController = SafeAlloc::MakeUnique<ModelAnimationController>(2);
        }

        // Parse animations
        if (_animController) {
            for (const auto& anim_entry : anim_entries) {
                string anim_path;

                if (anim_entry.FileName == "ModelFile") {
                    anim_path = model;
                }
                else {
                    anim_path = strex(name).extract_dir().combine_path(anim_entry.FileName);
                }

                const auto reversed = anim_entry.Name.starts_with('~');
                const auto anim_name = reversed ? anim_entry.Name.substr(1) : anim_entry.Name;
                auto* anim = _modelMngr->LoadAnimation(anim_path, anim_name);

                if (anim != nullptr) {
                    const auto anim_index = _animController->RegisterAnimation(anim, reversed);
                    _animIndexes.emplace(std::make_pair(anim_entry.StateAnim, anim_entry.ActionAnim), anim_index);
                }
            }

            const auto anim_count = _animController->GetAnimationsCount();

            if (anim_count != 0) {
                // Add animation bones, not included to base hierarchy
                // All bones linked with animation in SetupAnimationOutput
                for (int32 i = 0; i < anim_count; i++) {
                    const auto& bones_hierarchy = _animController->GetAnimationBones(i);

                    for (const auto& bone_hierarchy : bones_hierarchy) {
                        auto* bone = _hierarchy->_rootBone.get();

                        for (size_t b = 1; b < bone_hierarchy.size(); b++) {
                            auto* child = bone->Find(bone_hierarchy[b]);

                            if (child == nullptr) {
                                auto new_child = SafeAlloc::MakeUnique<ModelBone>();
                                child = new_child.get();
                                new_child->Name = bone_hierarchy[b];
                                bone->Children.emplace_back(std::move(new_child));
                            }

                            bone = child;
                        }
                    }
                }

                _animController->SetInterpolation(!disable_animation_interpolation);
                _hierarchy->SetupAnimationOutput(_animController.get());
            }
            else {
                _animController = nullptr;
            }
        }
    }
    else {
        // Load just model
        auto* hierarchy = _modelMngr->GetHierarchy(name);

        if (hierarchy == nullptr) {
            return false;
        }

        // Create animation
        _fileName = name;
        _hierarchy = hierarchy;

        // Todo: process default animations
    }

    return true;
}

auto ModelInformation::GetAnimationIndex(CritterStateAnim& state_anim, CritterActionAnim& action_anim, float32* speed) -> int32
{
    FO_STACK_TRACE_ENTRY();

    auto anim_index = GetAnimationIndexEx(state_anim, action_anim, speed);

    if (anim_index != -1) {
        return anim_index;
    }

    // Find substitute animation
    const auto base_model_name = _modelMngr->_hashResolver->ToHashedString(_fileName);
    const auto base_state_anim = state_anim;
    const auto base_action_anim = action_anim;

    while (anim_index == -1) {
        auto model_name = base_model_name;
        const auto state_anim_ = state_anim;
        const auto action_anim_ = action_anim;

        if (_modelMngr->_animNameResolver->ResolveCritterAnimationSubstitute(base_model_name, base_state_anim, base_action_anim, model_name, state_anim, action_anim) && (state_anim != state_anim_ || action_anim != action_anim_)) {
            anim_index = GetAnimationIndexEx(state_anim, action_anim, speed);
        }
        else {
            break;
        }
    }

    return anim_index;
}

auto ModelInformation::GetAnimationIndexEx(CritterStateAnim state_anim, CritterActionAnim action_anim, float32* speed) const -> int32
{
    FO_STACK_TRACE_ENTRY();

    if (const auto it1 = _stateAnimEquals.find(state_anim); it1 != _stateAnimEquals.end()) {
        state_anim = it1->second;
    }
    if (const auto it2 = _actionAnimEquals.find(action_anim); it2 != _actionAnimEquals.end()) {
        action_anim = it2->second;
    }

    const auto index = std::make_pair(state_anim, action_anim);

    if (speed != nullptr) {
        const auto it = _animSpeed.find(index);

        if (it != _animSpeed.end()) {
            *speed = it->second;
        }
        else {
            *speed = 1.0f;
        }
    }

    if (const auto it = _animIndexes.find(index); it != _animIndexes.end()) {
        return it->second;
    }

    return -1;
}

auto ModelInformation::CreateInstance() -> ModelInstance*
{
    FO_STACK_TRACE_ENTRY();

    auto* model = SafeAlloc::MakeRaw<ModelInstance>(*_modelMngr, this);

    if (_animController) {
        model->_bodyAnimController = _animController->Copy();

        if (_rotationBone) {
            model->_moveAnimController = _animController->Copy();
        }
    }

    return model;
}

auto ModelInformation::CreateCutShape(MeshData* mesh) const -> ModelCutData::Shape
{
    FO_STACK_TRACE_ENTRY();

    ModelCutData::Shape shape;
    shape.GlobalTransformationMatrix = mesh->Owner->GlobalTransformationMatrix;

    if (mesh->Vertices.size() != 36) {
        shape.IsSphere = true;

        // Evaluate sphere radius
        auto vmin = mesh->Vertices[0].Position.x;
        auto vmax = mesh->Vertices[0].Position.x;

        for (const auto i : iterate_range(mesh->Vertices)) {
            const auto& v = mesh->Vertices[i];
            vmin = std::min(v.Position.x, vmin);
            vmax = std::max(v.Position.x, vmax);
        }

        shape.SphereRadius = (vmax - vmin) / 2.0f;
    }
    else {
        shape.IsSphere = false;

        // Calculate bounding box size
        float32 vmin[3];
        float32 vmax[3];

        for (size_t i = 0, j = mesh->Vertices.size(); i < j; i++) {
            const Vertex3D& v = mesh->Vertices[i];

            if (i == 0) {
                vmin[0] = vmax[0] = v.Position.x;
                vmin[1] = vmax[1] = v.Position.y;
                vmin[2] = vmax[2] = v.Position.z;
            }
            else {
                vmin[0] = std::max(vmin[0], v.Position.x);
                vmin[1] = std::max(vmin[1], v.Position.y);
                vmin[2] = std::max(vmin[2], v.Position.z);
                vmax[0] = std::min(vmax[0], v.Position.x);
                vmax[1] = std::min(vmax[1], v.Position.y);
                vmax[2] = std::min(vmax[2], v.Position.z);
            }
        }

        shape.BBoxMin.x = vmin[0];
        shape.BBoxMin.y = vmin[1];
        shape.BBoxMin.z = vmin[2];
        shape.BBoxMax.x = vmax[0];
        shape.BBoxMax.y = vmax[1];
        shape.BBoxMax.z = vmax[2];
    }

    return shape;
}

ModelHierarchy::ModelHierarchy(ModelManager& model_mngr) :
    _modelMngr {&model_mngr}
{
    FO_STACK_TRACE_ENTRY();
}

static void SetupBonesExt(multimap<uint32, ModelBone*>& bones, ModelBone* bone, uint32 depth)
{
    FO_STACK_TRACE_ENTRY();

    bones.emplace(depth, bone);

    for (auto& child : bone->Children) {
        SetupBonesExt(bones, child.get(), depth + 1);
    }
}

void ModelHierarchy::SetupBones()
{
    FO_STACK_TRACE_ENTRY();

    multimap<uint32, ModelBone*> bones;
    SetupBonesExt(bones, _rootBone.get(), 0);

    for (auto* bone : bones | std::views::values) {
        _allBones.emplace_back(bone);

        if (bone->AttachedMesh) {
            _allDrawBones.emplace_back(bone);
        }
    }
}

static void SetupAnimationOutputExt(ModelAnimationController* anim_controller, ModelBone* bone)
{
    FO_STACK_TRACE_ENTRY();

    anim_controller->RegisterAnimationOutput(bone->Name, bone->TransformationMatrix);

    for (auto& it : bone->Children) {
        SetupAnimationOutputExt(anim_controller, it.get());
    }
}

void ModelHierarchy::SetupAnimationOutput(ModelAnimationController* anim_controller)
{
    FO_STACK_TRACE_ENTRY();

    SetupAnimationOutputExt(anim_controller, _rootBone.get());
}

auto ModelHierarchy::GetTexture(string_view tex_name) -> MeshTexture*
{
    FO_STACK_TRACE_ENTRY();

    auto* texture = _modelMngr->LoadTexture(tex_name, _fileName);

    if (texture == nullptr) {
        WriteLog("Can't load texture '{}'", tex_name);
    }

    return texture;
}

auto ModelHierarchy::GetEffect(string_view name) -> RenderEffect*
{
    FO_STACK_TRACE_ENTRY();

    auto* effect = _modelMngr->_effectMngr->LoadEffect(EffectUsage::Model, name);

    if (effect == nullptr) {
        WriteLog("Can't load effect '{}'", name);
    }

    return effect;
}

FO_END_NAMESPACE();

#endif
