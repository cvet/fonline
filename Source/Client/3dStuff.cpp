//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "GenericUtils.h"
#include "Log.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Testing.h"
#include "Timer.h"

void MeshData::Load(DataReader& reader)
{
    uint len = 0;
    reader.ReadPtr(&len, sizeof(len));
    Vertices.resize(len);
    reader.ReadPtr(&Vertices[0], len * sizeof(Vertices[0]));
    reader.ReadPtr(&len, sizeof(len));
    Indices.resize(len);
    reader.ReadPtr(&Indices[0], len * sizeof(Indices[0]));
    reader.ReadPtr(&len, sizeof(len));
    DiffuseTexture.resize(len);
    reader.ReadPtr(&DiffuseTexture[0], len);
    reader.ReadPtr(&len, sizeof(len));
    SkinBones.resize(len);
    reader.ReadPtr(&SkinBones[0], len * sizeof(SkinBones[0]));
    reader.ReadPtr(&len, sizeof(len));
    SkinBoneOffsets.resize(len);
    reader.ReadPtr(&SkinBoneOffsets[0], len * sizeof(SkinBoneOffsets[0]));
    SkinBones.resize(SkinBoneOffsets.size());
}

void Bone::Load(DataReader& reader)
{
    reader.ReadPtr(&NameHash, sizeof(NameHash));
    reader.ReadPtr(&TransformationMatrix, sizeof(TransformationMatrix));
    reader.ReadPtr(&GlobalTransformationMatrix, sizeof(GlobalTransformationMatrix));
    if (reader.Read<uchar>())
    {
        AttachedMesh = std::make_unique<MeshData>();
        AttachedMesh->Load(reader);
        AttachedMesh->Owner = this;
    }
    else
    {
        AttachedMesh = nullptr;
    }
    uint len = 0;
    reader.ReadPtr(&len, sizeof(len));
    for (uint i = 0; i < len; i++)
    {
        auto child = std::make_unique<Bone>();
        child->Load(reader);
        Children.push_back(std::move(child));
    }
    CombinedTransformationMatrix = Matrix();
}

void Bone::FixAfterLoad(Bone* root_bone)
{
    if (AttachedMesh)
    {
        MeshData* mesh = AttachedMesh.get();
        for (size_t i = 0, j = mesh->SkinBoneNameHashes.size(); i < j; i++)
        {
            if (mesh->SkinBoneNameHashes[i])
                mesh->SkinBones[i] = root_bone->Find(mesh->SkinBoneNameHashes[i]);
            else
                mesh->SkinBones[i] = mesh->Owner;
        }
    }

    for (auto& child : Children)
        child->FixAfterLoad(root_bone);
}

Bone* Bone::Find(hash name_hash)
{
    if (NameHash == name_hash)
        return this;

    for (auto& child : Children)
    {
        Bone* bone = child->Find(name_hash);
        if (bone)
            return bone;
    }
    return nullptr;
}

hash Bone::GetHash(const string& name)
{
    return _str(name).toHash();
}

Animation3dManager::Animation3dManager(RenderSettings& sett, FileManager& file_mngr, EffectManager& effect_mngr,
    ScriptSystem& script_sys, MeshTextureCreator mesh_tex_creator) :
    settings {sett},
    fileMngr {file_mngr},
    effectMngr {effect_mngr},
    scriptSys {script_sys},
    meshTexCreator {mesh_tex_creator}
{
    RUNTIME_ASSERT(settings.Enable3dRendering);

    // Check effects
    effectMngr.Load3dEffects();

    // Smoothing
    moveTransitionTime = settings.Animation3dSmoothTime / 1000.0f;
    if (moveTransitionTime < 0.001f)
        moveTransitionTime = 0.001f;

    // 2D rendering time
    if (settings.Animation3dFPS)
        animDelay = 1000 / settings.Animation3dFPS;
}

Bone* Animation3dManager::LoadModel(const string& fname)
{
    // Find already loaded
    hash name_hash = _str(fname).toHash();
    for (auto& root_bone : loadedModels)
        if (root_bone->NameHash == name_hash)
            return root_bone.get();

    // Add to already processed
    if (processedFiles.count(name_hash))
        return nullptr;
    processedFiles.emplace(name_hash);

    // Load file data
    File file = fileMngr.ReadFile(fname);
    if (!file)
    {
        WriteLog("3d file '{}' not found.\n", fname);
        return nullptr;
    }

    // Load bones
    auto root_bone = std::make_unique<Bone>();
    DataReader reader {file.GetBuf()};
    root_bone->Load(reader);
    root_bone->FixAfterLoad(root_bone.get());

    // Load animations
    uint anim_sets_count = file.GetBEUInt();
    for (uint i = 0; i < anim_sets_count; i++)
    {
        auto anim_set = std::make_unique<AnimSet>();
        anim_set->Load(reader);
        loadedAnimSets.push_back(std::move(anim_set));
    }

    // Add to collection
    root_bone->NameHash = name_hash;
    loadedModels.emplace_back(root_bone.release(), [this, &root_bone](Bone*) {
        processedFiles.erase(root_bone->NameHash);
        // loadedModels.erase(root_bone);
    });
    return loadedModels.back().get();
}

AnimSet* Animation3dManager::LoadAnimation(const string& anim_fname, const string& anim_name)
{
    // Find in already loaded
    bool take_first = (anim_name == "Base");
    hash name_hash = _str(anim_fname).toHash();
    for (size_t i = 0; i < loadedAnimSets.size(); i++)
    {
        AnimSet* anim = loadedAnimSets[i].get();
        if (_str(anim->GetFileName()).compareIgnoreCase(anim_fname) &&
            (take_first || _str(anim->GetName()).compareIgnoreCase(anim_name)))
            return anim;
    }

    // Check maybe file already processed and nothing founded
    if (processedFiles.count(name_hash))
        return nullptr;
    processedFiles.emplace(name_hash);

    // File not processed, load and recheck animations
    if (LoadModel(anim_fname))
        return LoadAnimation(anim_fname, anim_name);

    return nullptr;
}

MeshTexture* Animation3dManager::LoadTexture(const string& texture_name, const string& model_path)
{
    // Skip empty
    if (texture_name.empty())
        return nullptr;

    // Try find already loaded texture
    for (auto& mesh_tex : loadedMeshTextures)
        if (_str(mesh_tex->Name).compareIgnoreCase(texture_name))
            return mesh_tex->MainTex ? mesh_tex.get() : nullptr;

    // Create new
    MeshTexture* mesh_tex = new MeshTexture();
    mesh_tex->Name = texture_name;
    mesh_tex->ModelPath = model_path;
    meshTexCreator(mesh_tex);
    loadedMeshTextures.emplace_back(mesh_tex);
    return mesh_tex->MainTex ? mesh_tex : nullptr;
}

void Animation3dManager::DestroyTextures()
{
    loadedMeshTextures.clear();
}

void Animation3dManager::SetScreenSize(int width, int height)
{
    if (width == modeWidth && height == modeHeight)
        return;

    // Build orthogonal projection
    modeWidth = width;
    modeHeight = height;
    modeWidthF = (float)modeWidth;
    modeHeightF = (float)modeHeight;

    // Projection
    float k = (float)modeHeight / 768.0f;
    MatrixHelper::MatrixOrtho(
        matrixProjRM[0], 0.0f, 18.65f * k * modeWidthF / modeHeightF, 0.0f, 18.65f * k, -10.0f, 10.0f);
    matrixProjCM = matrixProjRM;
    matrixProjCM.Transpose();
    matrixEmptyRM = Matrix();
    matrixEmptyCM = matrixEmptyRM;
    matrixEmptyCM.Transpose();

    // View port
    // GL(glViewport(0, 0, modeWidth, modeHeight));
}

Animation3d* Animation3dManager::GetAnimation(const string& name, bool is_child)
{
    Animation3dEntity* entity = GetEntity(name);
    if (!entity)
        return nullptr;
    Animation3d* anim3d = entity->CloneAnimation();
    if (!anim3d)
        return nullptr;

    // Create mesh instances
    anim3d->allMeshes.resize(entity->xFile->allDrawBones.size());
    anim3d->allMeshesDisabled.resize(anim3d->allMeshes.size());
    for (size_t i = 0, j = entity->xFile->allDrawBones.size(); i < j; i++)
    {
        MeshInstance* mesh_instance = anim3d->allMeshes[i];
        MeshData* mesh = entity->xFile->allDrawBones[i]->AttachedMesh.get();
        memzero(mesh_instance, sizeof(MeshInstance));
        mesh_instance->Mesh = mesh;
        const char* tex_name = (mesh->DiffuseTexture.length() ? mesh->DiffuseTexture.c_str() : nullptr);
        mesh_instance->CurTexures[0] = mesh_instance->DefaultTexures[0] =
            (tex_name ? entity->xFile->GetTexture(tex_name) : nullptr);
        mesh_instance->CurEffect = mesh_instance->DefaultEffect =
            (!mesh->EffectName.empty() ? entity->xFile->GetEffect(mesh->EffectName) : nullptr);
    }

    // Set default data
    anim3d->SetAnimation(0, 0, nullptr, ANIMATION_INIT);
    return anim3d;
}

void Animation3dManager::PreloadEntity(const string& name)
{
    GetEntity(name);
}

Animation3dEntity* Animation3dManager::GetEntity(const string& name)
{
    // Try find instance
    for (auto& entity : allEntities)
        if (entity->fileName == name)
            return entity.get();

    // Create new instance
    auto entity = std::make_unique<Animation3dEntity>(*this);
    if (!entity->Load(name))
        return nullptr;

    allEntities.push_back(std::move(entity));
    return allEntities.back().get();
}

Animation3dXFile* Animation3dManager::GetXFile(const string& xname)
{
    for (auto& x : xFiles)
        if (x->fileName == xname)
            return x.get();

    // Load
    Bone* root_bone = LoadModel(xname);
    if (!root_bone)
    {
        WriteLog("Unable to load 3d file '{}'.\n", xname);
        return nullptr;
    }

    Animation3dXFile* xfile = new Animation3dXFile(*this);
    xfile->fileName = xname;
    xfile->rootBone = unique_ptr<Bone>(root_bone);
    xfile->SetupBones();

    xFiles.emplace_back(xfile);
    return xfile;
}

Vector Animation3dManager::Convert2dTo3d(int x, int y)
{
    Vector pos;
    VecUnproject(Vector((float)x, (float)y, 0.0f), pos);
    pos.z = 0.0f;
    return pos;
}

Point Animation3dManager::Convert3dTo2d(Vector pos)
{
    Vector coords;
    VecProject(pos, coords);
    return Point((int)coords.x, (int)coords.y);
}

void Animation3dManager::VecProject(const Vector& v, Vector& out)
{
    int viewport[4] = {0, 0, modeWidth, modeHeight};
    float x = 0.0f, y = 0.0f, z = 0.0f;
    MatrixHelper::MatrixProject(v.x, v.y, v.z, matrixEmptyCM[0], matrixProjCM[0], viewport, &x, &y, &z);
    out.x = x;
    out.y = y;
    out.z = z;
}

void Animation3dManager::VecUnproject(const Vector& v, Vector& out)
{
    int viewport[4] = {0, 0, modeWidth, modeHeight};
    float x = 0.0f, y = 0.0f, z = 0.0f;
    MatrixHelper::MatrixUnproject(v.x, modeHeightF - v.y, v.z, matrixEmptyCM[0], matrixProjCM[0], viewport, &x, &y, &z);
    out.x = x;
    out.y = y;
    out.z = z;
}

void Animation3dManager::ProjectPosition(Vector& v)
{
    v *= matrixProjRM;
    v.x = ((v.x - 1.0f) * 0.5f + 1.0f) * modeWidthF;
    v.y = ((1.0f - v.y) * 0.5f) * modeHeightF;
}

Animation3d::Animation3d(Animation3dManager& anim3d_mngr) : anim3dMngr(anim3d_mngr)
{
    speedAdjustBase = 1.0f;
    speedAdjustCur = 1.0f;
    speedAdjustLink = 1.0f;
    dirAngle = (anim3dMngr.settings.MapHexagonal ? 150.0f : 135.0f);
    childChecker = true;
    useGameTimer = true;
    Matrix::RotationX(anim3dMngr.settings.MapCameraAngle * PI_VALUE / 180.0f, matRot);
}

Animation3d::~Animation3d()
{
    delete animController;

    for (auto anim : childAnimations)
        delete anim;
    for (auto* mesh : allMeshes)
        delete mesh;
    for (auto* mesh : combinedMeshes)
        delete mesh;
}

void Animation3d::StartMeshGeneration()
{
    if (!allowMeshGeneration)
    {
        allowMeshGeneration = true;
        GenerateCombinedMeshes();
    }
}

bool Animation3d::SetAnimation(uint anim1, uint anim2, int* layers, int flags)
{
    curAnim1 = anim1;
    curAnim2 = anim2;

    // Get animation index
    int anim_pair = (anim1 << 16) | anim2;
    float speed = 1.0f;
    int index = 0;
    float period_proc = 0.0f;
    if (!FLAG(flags, ANIMATION_INIT))
    {
        if (!anim1)
        {
            index = animEntity->renderAnim;
            period_proc = (float)anim2 / 10.0f;
        }
        else
        {
            index = animEntity->GetAnimationIndex(anim1, anim2, &speed, FLAG(flags, ANIMATION_COMBAT));
        }
    }

    if (FLAG(flags, ANIMATION_PERIOD(0)))
        period_proc = (float)(flags >> 16);
    period_proc = CLAMP(period_proc, 0.0f, 99.9f);

    // Check animation changes
    int new_layers[LAYERS3D_COUNT];
    if (layers)
        memcpy(new_layers, layers, sizeof(int) * LAYERS3D_COUNT);
    else
        memcpy(new_layers, currentLayers, sizeof(int) * LAYERS3D_COUNT);

    // Animation layers
    auto it = animEntity->animLayerValues.find(anim_pair);
    if (it != animEntity->animLayerValues.end())
        for (auto it_ = it->second.begin(); it_ != it->second.end(); ++it_)
            new_layers[it_->first] = it_->second;

    // Check for change
    bool layer_changed = (FLAG(flags, ANIMATION_INIT) ? true : false);
    if (!layer_changed)
    {
        for (int i = 0; i < LAYERS3D_COUNT; i++)
        {
            if (new_layers[i] != currentLayers[i])
            {
                layer_changed = true;
                break;
            }
        }
    }

    // Is not one time play and same anim
    if (!FLAG(flags, ANIMATION_INIT | ANIMATION_ONE_TIME) && currentLayers[LAYERS3D_COUNT] == anim_pair &&
        !layer_changed)
        return false;

    memcpy(currentLayers, new_layers, sizeof(int) * LAYERS3D_COUNT);
    currentLayers[LAYERS3D_COUNT] = anim_pair;

    bool mesh_changed = false;
    HashVec fast_transition_bones;

    if (layer_changed)
    {
        // Store previous cuts
        vector<CutData*> old_cuts = allCuts;

        // Store disabled meshes
        for (size_t i = 0, j = allMeshes.size(); i < j; i++)
            allMeshesDisabled[i] = allMeshes[i]->Disabled;

        // Set base data
        SetAnimData(animEntity->animDataDefault, true);

        // Append linked data
        if (parentBone)
            SetAnimData(animLink, false);

        // Mark animations as unused
        for (auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it)
            (*it)->childChecker = false;

        // Get unused layers and meshes
        bool unused_layers[LAYERS3D_COUNT];
        memzero(unused_layers, sizeof(unused_layers));
        for (int i = 0; i < LAYERS3D_COUNT; i++)
        {
            if (new_layers[i] == 0)
                continue;

            for (auto it_ = animEntity->animData.begin(), end_ = animEntity->animData.end(); it_ != end_; ++it_)
            {
                AnimParams& link = *it_;
                if (link.Layer == i && link.LayerValue == new_layers[i] && link.ChildName.empty())
                {
                    for (size_t j = 0; j < link.DisabledLayer.size(); j++)
                    {
                        unused_layers[link.DisabledLayer[j]] = true;
                    }
                    for (size_t j = 0; j < link.DisabledMesh.size(); j++)
                    {
                        uint disabled_mesh_name_hash = link.DisabledMesh[j];
                        for (auto* mesh : allMeshes)
                            if (!disabled_mesh_name_hash || disabled_mesh_name_hash == mesh->Mesh->Owner->NameHash)
                                mesh->Disabled = true;
                    }
                }
            }
        }

        if (parentBone)
        {
            for (size_t j = 0; j < animLink.DisabledLayer.size(); j++)
            {
                unused_layers[animLink.DisabledLayer[j]] = true;
            }
            for (size_t j = 0; j < animLink.DisabledMesh.size(); j++)
            {
                uint disabled_mesh_name_hash = animLink.DisabledMesh[j];
                for (auto* mesh : allMeshes)
                    if (!disabled_mesh_name_hash || disabled_mesh_name_hash == mesh->Mesh->Owner->NameHash)
                        mesh->Disabled = true;
            }
        }

        // Append animations
        for (int i = 0; i < LAYERS3D_COUNT; i++)
        {
            if (unused_layers[i] || new_layers[i] == 0)
                continue;

            for (AnimParams& link : animEntity->animData)
            {
                if (link.Layer == i && link.LayerValue == new_layers[i])
                {
                    if (link.ChildName.empty())
                    {
                        SetAnimData(link, false);
                        continue;
                    }

                    bool available = false;
                    for (auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it)
                    {
                        Animation3d* anim3d = *it;
                        if (anim3d->animLink.Id == link.Id)
                        {
                            anim3d->childChecker = true;
                            available = true;
                            break;
                        }
                    }

                    if (!available)
                    {
                        Animation3d* anim3d = nullptr;

                        // Link to main bone
                        if (link.LinkBoneHash)
                        {
                            Bone* to_bone = animEntity->xFile->rootBone->Find(link.LinkBoneHash);
                            if (to_bone)
                            {
                                anim3d = anim3dMngr.GetAnimation(link.ChildName, true);
                                if (anim3d)
                                {
                                    mesh_changed = true;
                                    anim3d->parentAnimation = this;
                                    anim3d->parentBone = to_bone;
                                    anim3d->animLink = link;
                                    anim3d->SetAnimData(link, false);
                                    childAnimations.push_back(anim3d);
                                }

                                if (animEntity->fastTransitionBones.count(link.LinkBoneHash))
                                    fast_transition_bones.push_back(link.LinkBoneHash);
                            }
                        }
                        // Link all bones
                        else
                        {
                            anim3d = anim3dMngr.GetAnimation(link.ChildName, true);
                            if (anim3d)
                            {
                                for (auto it = anim3d->animEntity->xFile->allBones.begin(),
                                          end = anim3d->animEntity->xFile->allBones.end();
                                     it != end; ++it)
                                {
                                    Bone* child_bone = *it;
                                    Bone* root_bone = animEntity->xFile->rootBone->Find(child_bone->NameHash);
                                    if (root_bone)
                                    {
                                        anim3d->linkBones.push_back(root_bone);
                                        anim3d->linkBones.push_back(child_bone);
                                    }
                                }

                                mesh_changed = true;
                                anim3d->parentAnimation = this;
                                anim3d->parentBone = animEntity->xFile->rootBone.get();
                                anim3d->animLink = link;
                                anim3d->SetAnimData(link, false);
                                childAnimations.push_back(anim3d);
                            }
                        }
                    }
                }
            }
        }

        // Erase unused animations
        for (auto it = childAnimations.begin(); it != childAnimations.end();)
        {
            Animation3d* anim3d = *it;
            if (!anim3d->childChecker)
            {
                mesh_changed = true;
                delete anim3d;
                it = childAnimations.erase(it);
            }
            else
            {
                ++it;
            }
        }

        // Compare changed effect
        if (!mesh_changed)
        {
            for (size_t i = 0, j = allMeshes.size(); i < j && !mesh_changed; i++)
                mesh_changed = (allMeshes[i]->LastEffect != allMeshes[i]->CurEffect);
        }

        // Compare changed textures
        if (!mesh_changed)
        {
            for (size_t i = 0, j = allMeshes.size(); i < j && !mesh_changed; i++)
                for (size_t k = 0; k < EFFECT_TEXTURES && !mesh_changed; k++)
                    mesh_changed = (allMeshes[i]->LastTexures[k] != allMeshes[i]->CurTexures[k]);
        }

        // Compare disabled meshes
        if (!mesh_changed)
        {
            for (size_t i = 0, j = allMeshes.size(); i < j && !mesh_changed; i++)
                mesh_changed = (allMeshesDisabled[i] != allMeshes[i]->Disabled);
        }

        // Affect cut
        if (!mesh_changed)
            mesh_changed = (allCuts != old_cuts);
    }

    if (animController && index >= 0)
    {
        // Get the animation set from the controller
        AnimSet* set = animController->GetAnimationSet(index);

        // Alternate tracks
        uint new_track = (currentTrack == 0 ? 1 : 0);
        float period = set->GetDuration();
        animPosPeriod = period;
        if (FLAG(flags, ANIMATION_INIT))
            period = 0.0002f;

        // Assign to our track
        animController->SetTrackAnimationSet(new_track, set);

        // Turn off fast transition bones on other tracks
        if (!fast_transition_bones.empty())
            animController->ResetBonesTransition(new_track, fast_transition_bones);

        // Prepare to new tracking
        animController->Reset();

        // Smooth time
        float smooth_time =
            (FLAG(flags, ANIMATION_NO_SMOOTH | ANIMATION_STAY | ANIMATION_INIT) ? 0.0001f :
                                                                                  anim3dMngr.moveTransitionTime);
        float start_time = period * period_proc / 100.0f;
        if (FLAG(flags, ANIMATION_STAY))
            period = start_time + 0.0002f;

        // Add an event key to disable the currently playing track smooth_time seconds in the future
        animController->AddEventEnable(currentTrack, false, smooth_time);
        // Add an event key to change the speed right away so the animation completes in smooth_time seconds
        animController->AddEventSpeed(currentTrack, 0.0f, 0.0f, smooth_time);
        // Add an event to change the weighting of the current track (the effect it has blended with the second track)
        animController->AddEventWeight(currentTrack, 0.0f, 0.0f, smooth_time);

        // Enable the new track
        animController->SetTrackEnable(new_track, true);
        animController->SetTrackPosition(new_track, 0.0f);
        // Add an event key to set the speed of the track
        animController->AddEventSpeed(new_track, 1.0f, 0.0f, smooth_time);
        if (FLAG(flags, ANIMATION_ONE_TIME | ANIMATION_STAY | ANIMATION_INIT))
            animController->AddEventSpeed(new_track, 0.0f, period - 0.0001f, 0.0);
        // Add an event to change the weighting of the current track (the effect it has blended with the first track)
        // As you can see this will go from 0 effect to total effect(1.0f) in smooth_time seconds and the first track
        // goes from total to 0.0f in the same time.
        animController->AddEventWeight(new_track, 1.0f, 0.0f, smooth_time);

        if (start_time != 0.0)
            animController->AdvanceTime(start_time);
        else
            animController->AdvanceTime(0.0001f);

        // Remember current track
        currentTrack = new_track;

        // Speed
        speedAdjustCur = speed;

        // End time
        uint tick = GetTick();
        if (FLAG(flags, ANIMATION_ONE_TIME))
            endTick = tick + uint(period / GetSpeed() * 1000.0f);
        else
            endTick = 0;

        // Force redraw
        lastDrawTick = 0;
    }

    // Set animation for children
    for (auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it)
    {
        Animation3d* child = *it;
        if (child->SetAnimation(anim1, anim2, layers, flags))
            mesh_changed = true;
    }

    // Regenerate mesh for drawing
    if (!parentBone && mesh_changed)
        GenerateCombinedMeshes();

    return mesh_changed;
}

bool Animation3d::IsAnimation(uint anim1, uint anim2)
{
    int ii = (anim1 << 16) | anim2;
    auto it = animEntity->animIndexes.find(ii);
    return it != animEntity->animIndexes.end();
}

bool Animation3d::CheckAnimation(uint& anim1, uint& anim2)
{
    if (animEntity->GetAnimationIndex(anim1, anim2, nullptr, false) == -1)
    {
        anim1 = ANIM1_UNARMED;
        anim2 = ANIM2_IDLE;
        return false;
    }
    return true;
}

int Animation3d::GetAnim1()
{
    return currentLayers[LAYERS3D_COUNT] >> 16;
}

int Animation3d::GetAnim2()
{
    return currentLayers[LAYERS3D_COUNT] & 0xFFFF;
}

bool Animation3d::IsAnimationPlaying()
{
    return GetTick() < endTick;
}

void Animation3d::GetRenderFramesData(float& period, int& proc_from, int& proc_to, int& dir)
{
    period = 0.0f;
    proc_from = animEntity->renderAnimProcFrom;
    proc_to = animEntity->renderAnimProcTo;
    dir = animEntity->renderAnimDir;

    if (animController)
    {
        AnimSet* set = animController->GetAnimationSet(animEntity->renderAnim);
        if (set)
            period = set->GetDuration();
    }
}

void Animation3d::GetDrawSize(uint& draw_width, uint& draw_height)
{
    draw_width = animEntity->drawWidth;
    draw_height = animEntity->drawHeight;
    int s = (int)ceilf(MAX(MAX(matScale.a1, matScale.b2), matScale.c3));
    draw_width *= s;
    draw_height *= s;
}

float Animation3d::GetSpeed()
{
    return speedAdjustCur * speedAdjustBase * speedAdjustLink * anim3dMngr.globalSpeedAdjust;
}

uint Animation3d::GetTick()
{
    if (useGameTimer)
        return Timer::GameTick();
    return Timer::FastTick();
}

void Animation3d::SetAnimData(AnimParams& data, bool clear)
{
    // Transformations
    if (clear)
    {
        matScaleBase = Matrix();
        matRotBase = Matrix();
        matTransBase = Matrix();
    }

    Matrix mat_tmp;
    if (data.ScaleX != 0.0f)
        matScaleBase = matScaleBase * Matrix::Scaling(Vector(data.ScaleX, 1.0f, 1.0f), mat_tmp);
    if (data.ScaleY != 0.0f)
        matScaleBase = matScaleBase * Matrix::Scaling(Vector(1.0f, data.ScaleY, 1.0f), mat_tmp);
    if (data.ScaleZ != 0.0f)
        matScaleBase = matScaleBase * Matrix::Scaling(Vector(1.0f, 1.0f, data.ScaleZ), mat_tmp);
    if (data.RotX != 0.0f)
        matRotBase = matRotBase * Matrix::RotationX(-data.RotX * PI_VALUE / 180.0f, mat_tmp);
    if (data.RotY != 0.0f)
        matRotBase = matRotBase * Matrix::RotationY(data.RotY * PI_VALUE / 180.0f, mat_tmp);
    if (data.RotZ != 0.0f)
        matRotBase = matRotBase * Matrix::RotationZ(data.RotZ * PI_VALUE / 180.0f, mat_tmp);
    if (data.MoveX != 0.0f)
        matTransBase = matTransBase * Matrix::Translation(Vector(data.MoveX, 0.0f, 0.0f), mat_tmp);
    if (data.MoveY != 0.0f)
        matTransBase = matTransBase * Matrix::Translation(Vector(0.0f, data.MoveY, 0.0f), mat_tmp);
    if (data.MoveZ != 0.0f)
        matTransBase = matTransBase * Matrix::Translation(Vector(0.0f, 0.0f, -data.MoveZ), mat_tmp);

    // Speed
    if (clear)
        speedAdjustLink = 1.0f;
    if (data.SpeedAjust != 0.0f)
        speedAdjustLink *= data.SpeedAjust;

    // Textures
    if (clear)
    {
        // Enable all meshes, set default texture
        for (auto* mesh : allMeshes)
        {
            mesh->Disabled = false;
            memcpy(mesh->LastTexures, mesh->CurTexures, sizeof(mesh->LastTexures));
            memcpy(mesh->CurTexures, mesh->DefaultTexures, sizeof(mesh->CurTexures));
        }
    }

    if (!data.TextureInfo.empty())
    {
        for (auto& tex_info : data.TextureInfo)
        {
            MeshTexture* texture = nullptr;

            // Get texture
            if (_str(std::get<0>(tex_info)).startsWith("Parent")) // Parent_MeshName
            {
                if (parentAnimation)
                {
                    const char* mesh_name = std::get<0>(tex_info).c_str() + 6;
                    if (*mesh_name && *mesh_name == '_')
                        mesh_name++;
                    uint mesh_name_hash = (*mesh_name ? Bone::GetHash(mesh_name) : 0);
                    for (auto* mesh : parentAnimation->allMeshes)
                    {
                        if (!mesh_name_hash || mesh_name_hash == mesh->Mesh->Owner->NameHash)
                        {
                            texture = mesh->CurTexures[std::get<2>(tex_info)];
                            break;
                        }
                    }
                }
            }
            else
            {
                texture = animEntity->xFile->GetTexture(std::get<0>(tex_info));
            }

            // Assign it
            int texture_num = std::get<2>(tex_info);
            uint mesh_name_hash = std::get<1>(tex_info);
            for (auto* mesh : parentAnimation->allMeshes)
                if (!mesh_name_hash || mesh_name_hash == mesh->Mesh->Owner->NameHash)
                    mesh->CurTexures[texture_num] = texture;
        }
    }

    // Effects
    if (clear)
    {
        for (auto* mesh : allMeshes)
        {
            mesh->LastEffect = mesh->CurEffect;
            mesh->CurEffect = mesh->DefaultEffect;
        }
    }

    if (!data.EffectInfo.empty())
    {
        for (auto& eff_info : data.EffectInfo)
        {
            RenderEffect* effect = nullptr;

            // Get effect
            if (_str(std::get<0>(eff_info)).startsWith("Parent")) // Parent_MeshName
            {
                if (parentAnimation)
                {
                    const char* mesh_name = std::get<0>(eff_info).c_str() + 6;
                    if (*mesh_name && *mesh_name == '_')
                        mesh_name++;
                    uint mesh_name_hash = (*mesh_name ? Bone::GetHash(mesh_name) : 0);
                    for (auto* mesh : parentAnimation->allMeshes)
                    {
                        if (!mesh_name_hash || mesh_name_hash == mesh->Mesh->Owner->NameHash)
                        {
                            effect = mesh->CurEffect;
                            break;
                        }
                    }
                }
            }
            else
            {
                effect = animEntity->xFile->GetEffect(std::get<0>(eff_info));
            }

            // Assign it
            uint mesh_name_hash = std::get<1>(eff_info);
            for (auto* mesh : allMeshes)
                if (!mesh_name_hash || mesh_name_hash == mesh->Mesh->Owner->NameHash)
                    mesh->CurEffect = effect;
        }
    }

    // Cut
    if (clear)
        allCuts.clear();
    for (auto& cut_info : data.CutInfo)
        allCuts.push_back(cut_info);
}

void Animation3d::SetDir(int dir)
{
    float angle = (float)(anim3dMngr.settings.MapHexagonal ? 150 - dir * 60 : 135 - dir * 45);
    if (angle != dirAngle)
        dirAngle = angle;
}

void Animation3d::SetDirAngle(int dir_angle)
{
    float angle = (float)dir_angle;
    if (angle != dirAngle)
        dirAngle = angle;
}

void Animation3d::SetRotation(float rx, float ry, float rz)
{
    Matrix my, mx, mz;
    Matrix::RotationX(rx, mx);
    Matrix::RotationY(ry, my);
    Matrix::RotationZ(rz, mz);
    matRot = mx * my * mz;
}

void Animation3d::SetScale(float sx, float sy, float sz)
{
    Matrix::Scaling(Vector(sx, sy, sz), matScale);
}

void Animation3d::SetSpeed(float speed)
{
    speedAdjustBase = speed;
}

void Animation3d::SetTimer(bool use_game_timer)
{
    useGameTimer = use_game_timer;
}

void Animation3d::GenerateCombinedMeshes()
{
    // Generation disabled
    if (!allowMeshGeneration)
        return;

    // Clean up buffers
    for (size_t i = 0, j = combinedMeshesSize; i < j; i++)
        ClearCombinedMesh(combinedMeshes[i]);
    combinedMeshesSize = 0;

    // Combine meshes recursively
    FillCombinedMeshes(this, this);

    // Cut
    disableCulling = false;
    CutCombinedMeshes(this, this);

    // Finalize meshes
    for (size_t i = 0, j = combinedMeshesSize; i < j; i++)
        combinedMeshes[i]->DrawMesh->DataChanged = true;
}

void Animation3d::FillCombinedMeshes(Animation3d* base, Animation3d* cur)
{
    // Combine meshes
    for (size_t i = 0, j = cur->allMeshes.size(); i < j; i++)
        base->CombineMesh(cur->allMeshes[i], cur->parentBone ? cur->animLink.Layer : 0);

    // Fill child
    for (auto it = cur->childAnimations.begin(), end = cur->childAnimations.end(); it != end; ++it)
        FillCombinedMeshes(base, *it);
}

void Animation3d::CombineMesh(MeshInstance* mesh_instance, int anim_layer)
{
    // Skip disabled meshes
    if (mesh_instance->Disabled)
        return;

    // Try encapsulate mesh instance to current combined mesh
    for (size_t i = 0, j = combinedMeshesSize; i < j; i++)
    {
        if (CanBatchCombinedMesh(combinedMeshes[i], mesh_instance))
        {
            BatchCombinedMesh(combinedMeshes[i], mesh_instance, anim_layer);
            return;
        }
    }

    // Create new combined mesh
    if (combinedMeshesSize >= combinedMeshes.size())
    {
        CombinedMesh* combined_mesh = new CombinedMesh();
        combined_mesh->SkinBones.resize(MODEL_MAX_BONES);
        combined_mesh->SkinBoneOffsets.resize(MODEL_MAX_BONES);
    }
    BatchCombinedMesh(combinedMeshes[combinedMeshesSize], mesh_instance, anim_layer);
    combinedMeshesSize++;
}

void Animation3d::ClearCombinedMesh(CombinedMesh* combined_mesh)
{
    combined_mesh->EncapsulatedMeshCount = 0;
    combined_mesh->CurBoneMatrix = 0;
    combined_mesh->Meshes.clear();
    combined_mesh->MeshIndices.clear();
    combined_mesh->MeshVertices.clear();
    combined_mesh->MeshAnimLayers.clear();
    combined_mesh->DrawMesh->Vertices.clear();
    combined_mesh->DrawMesh->Indices.clear();
}

bool Animation3d::CanBatchCombinedMesh(CombinedMesh* combined_mesh, MeshInstance* mesh_instance)
{
    if (combined_mesh->EncapsulatedMeshCount == 0)
        return true;
    if (combined_mesh->DrawEffect != mesh_instance->CurEffect)
        return false;
    for (int i = 0; i < EFFECT_TEXTURES; i++)
        if (combined_mesh->Textures[i] && mesh_instance->CurTexures[i] &&
            combined_mesh->Textures[i]->MainTex != mesh_instance->CurTexures[i]->MainTex)
            return false;
    if (combined_mesh->CurBoneMatrix + mesh_instance->Mesh->SkinBones.size() > combined_mesh->SkinBones.size())
        return false;
    return true;
}

void Animation3d::BatchCombinedMesh(CombinedMesh* combined_mesh, MeshInstance* mesh_instance, int anim_layer)
{
    MeshData* mesh = mesh_instance->Mesh;
    auto& vertices = combined_mesh->DrawMesh->Vertices;
    auto& indices = combined_mesh->DrawMesh->Indices;
    size_t vertices_old_size = vertices.size();
    size_t indices_old_size = indices.size();

    // Set or add data
    if (!combined_mesh->EncapsulatedMeshCount)
    {
        vertices = mesh->Vertices;
        indices = mesh->Indices;
        combined_mesh->DrawEffect = mesh_instance->CurEffect;
        memzero(&combined_mesh->SkinBones[0], combined_mesh->SkinBones.size() * sizeof(combined_mesh->SkinBones[0]));
        memzero(combined_mesh->Textures, sizeof(combined_mesh->Textures));
        combined_mesh->CurBoneMatrix = 0;
    }
    else
    {
        vertices.insert(vertices.end(), mesh->Vertices.begin(), mesh->Vertices.end());
        indices.insert(indices.end(), mesh->Indices.begin(), mesh->Indices.end());

        // Add indices offset
        ushort index_offset = (ushort)(vertices.size() - mesh->Vertices.size());
        size_t start_index = indices.size() - mesh->Indices.size();
        for (size_t i = start_index, j = indices.size(); i < j; i++)
            indices[i] += index_offset;

        // Add bones matrices offset
        float bone_index_offset = (float)combined_mesh->CurBoneMatrix;
        size_t start_vertex = vertices.size() - mesh->Vertices.size();
        for (size_t i = start_vertex, j = vertices.size(); i < j; i++)
            for (size_t b = 0; b < BONES_PER_VERTEX; b++)
                vertices[i].BlendIndices[b] += bone_index_offset;
    }

    // Set mesh transform and anim layer
    combined_mesh->Meshes.push_back(mesh);
    combined_mesh->MeshVertices.push_back((uint)(vertices.size() - vertices_old_size));
    combined_mesh->MeshIndices.push_back((uint)(indices.size() - indices_old_size));
    combined_mesh->MeshAnimLayers.push_back(anim_layer);

    // Add bones matrices
    for (size_t i = 0, j = mesh->SkinBones.size(); i < j; i++)
    {
        combined_mesh->SkinBones[combined_mesh->CurBoneMatrix + i] = mesh->SkinBones[i];
        combined_mesh->SkinBoneOffsets[combined_mesh->CurBoneMatrix + i] = mesh->SkinBoneOffsets[i];
    }
    combined_mesh->CurBoneMatrix += mesh->SkinBones.size();

    // Add textures
    for (int i = 0; i < EFFECT_TEXTURES; i++)
        if (!combined_mesh->Textures[i] && mesh_instance->CurTexures[i])
            combined_mesh->Textures[i] = mesh_instance->CurTexures[i];

    // Fix texture coords
    if (mesh_instance->CurTexures[0])
    {
        MeshTexture* mesh_tex = mesh_instance->CurTexures[0];
        for (size_t i = vertices_old_size, j = vertices.size(); i < j; i++)
        {
            vertices[i].TexCoord[0] =
                (vertices[i].TexCoord[0] * mesh_tex->AtlasOffsetData[2]) + mesh_tex->AtlasOffsetData[0];
            vertices[i].TexCoord[1] =
                (vertices[i].TexCoord[1] * mesh_tex->AtlasOffsetData[3]) + mesh_tex->AtlasOffsetData[1];
        }
    }

    // Increment mesh count
    combined_mesh->EncapsulatedMeshCount++;
}

void Animation3d::CutCombinedMeshes(Animation3d* base, Animation3d* cur)
{
    // Cut meshes
    if (!cur->allCuts.empty())
    {
        for (size_t i = 0, j = cur->allCuts.size(); i < j; i++)
            for (size_t k = 0; k < base->combinedMeshesSize; k++)
                base->CutCombinedMesh(base->combinedMeshes[k], cur->allCuts[i]);
        disableCulling = true;
    }

    // Fill child
    for (auto it = cur->childAnimations.begin(), end = cur->childAnimations.end(); it != end; ++it)
        CutCombinedMeshes(base, *it);
}

// -2 - ignore
// -1 - inside
// 0 - outside
// 1 - one point
static int SphereLineIntersection(const Vertex3D& p1, const Vertex3D& p2, const Vector& sp, float r, Vertex3D& in)
{
    auto sq = [](float f) -> float { return f * f; };
    float a = sq(p2.Position.x - p1.Position.x) + sq(p2.Position.y - p1.Position.y) + sq(p2.Position.z - p1.Position.z);
    float b = 2 *
        ((p2.Position.x - p1.Position.x) * (p1.Position.x - sp.x) +
            (p2.Position.y - p1.Position.y) * (p1.Position.y - sp.y) +
            (p2.Position.z - p1.Position.z) * (p1.Position.z - sp.z));
    float c = sq(sp.x) + sq(sp.y) + sq(sp.z) + sq(p1.Position.x) + sq(p1.Position.y) + sq(p1.Position.z) -
        2 * (sp.x * p1.Position.x + sp.y * p1.Position.y + sp.z * p1.Position.z) - sq(r);
    float i = sq(b) - 4 * a * c;

    if (i > 0.0)
    {
        float sqrt_i = sqrt(i);
        float mu1 = (-b + sqrt_i) / (2 * a);
        float mu2 = (-b - sqrt_i) / (2 * a);

        // Line segment doesn't intersect and on outside of sphere, in which case both values of u wll either be less
        // than 0 or greater than 1
        if ((mu1 < 0.0 && mu2 < 0.0) || (mu1 > 1.0 && mu2 > 1.0))
            return 0;

        // Line segment doesn't intersect and is inside sphere, in which case one value of u will be negative and the
        // other greater than 1
        if ((mu1 < 0.0 && mu2 > 1.0) || (mu2 < 0.0 && mu1 > 1.0))
            return -1;

        // Line segment intersects at one point, in which case one value of u will be between 0 and 1 and the other not
        if ((mu1 >= 0.0 && mu1 <= 1.0 && (mu2 < 0.0 || mu2 > 1.0)) ||
            (mu2 >= 0.0 && mu2 <= 1.0 && (mu1 < 0.0 || mu1 > 1.0)))
        {
            float& mu = ((mu1 >= 0.0 && mu1 <= 1.0) ? mu1 : mu2);
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
        if (mu1 >= 0.0 && mu1 <= 1.0 && mu2 >= 0.0 && mu2 <= 1.0)
        {
            // Ignore
            return -2;
        }
    }
    else if (i == 0.0)
    {
        // Ignore
        return -2;
    }
    return 0;
}

void Animation3d::CutCombinedMesh(CombinedMesh* combined_mesh, CutData* cut)
{
    auto& vertices = combined_mesh->DrawMesh->Vertices;
    auto& indices = combined_mesh->DrawMesh->Indices;
    IntVec& cut_layers = cut->Layers;
    for (size_t i = 0, j = cut->Shapes.size(); i < j; i++)
    {
        Vertex3DVec result_vertices;
        UShortVec result_indices;
        UIntVec result_mesh_vertices;
        UIntVec result_mesh_indices;
        result_vertices.reserve(vertices.size());
        result_indices.reserve(indices.size());
        result_mesh_vertices.reserve(combined_mesh->MeshVertices.size());
        result_mesh_indices.reserve(combined_mesh->MeshIndices.size());
        uint i_pos = 0, i_count = 0;
        for (size_t k = 0, l = combined_mesh->MeshIndices.size(); k < l; k++)
        {
            // Move shape to face space
            Matrix mesh_transform = combined_mesh->Meshes[k]->Owner->GlobalTransformationMatrix;
            Matrix sm = mesh_transform.Inverse() * cut->Shapes[i].GlobalTransformationMatrix;
            Vector ss, sp;
            Quaternion sr;
            sm.Decompose(ss, sr, sp);

            // Check anim layer
            int mesh_anim_layer = combined_mesh->MeshAnimLayers[k];
            bool skip = (std::find(cut_layers.begin(), cut_layers.end(), mesh_anim_layer) == cut_layers.end());

            // Process faces
            i_count += combined_mesh->MeshIndices[k];
            size_t vertices_old_size = result_vertices.size();
            size_t indices_old_size = result_indices.size();
            for (; i_pos < i_count; i_pos += 3)
            {
                // Face points
                const Vertex3D& v1 = vertices[indices[i_pos + 0]];
                const Vertex3D& v2 = vertices[indices[i_pos + 1]];
                const Vertex3D& v3 = vertices[indices[i_pos + 2]];

                // Skip mesh
                if (skip)
                {
                    result_vertices.push_back(v1);
                    result_vertices.push_back(v2);
                    result_vertices.push_back(v3);
                    result_indices.push_back((ushort)result_indices.size());
                    result_indices.push_back((ushort)result_indices.size());
                    result_indices.push_back((ushort)result_indices.size());
                    continue;
                }

                // Find intersections
                Vertex3D i1, i2, i3;
                int r1 = SphereLineIntersection(v1, v2, sp, cut->Shapes[i].SphereRadius * ss.x, i1);
                int r2 = SphereLineIntersection(v2, v3, sp, cut->Shapes[i].SphereRadius * ss.x, i2);
                int r3 = SphereLineIntersection(v3, v1, sp, cut->Shapes[i].SphereRadius * ss.x, i3);

                // Process intersections
                bool outside = (r1 == 0 && r2 == 0 && r3 == 0);
                bool ignore = (r1 == -2 || r2 == -2 || r3 == -2);
                int sum = r1 + r2 + r3;
                if (!ignore && sum == 2) // 1 1 0, corner in
                {
                    const Vertex3D& vv1 = (r1 == 0 ? v1 : (r2 == 0 ? v2 : v3));
                    const Vertex3D& vv2 = (r1 == 0 ? v2 : (r2 == 0 ? v3 : v1));
                    const Vertex3D& vv3 = (r1 == 0 ? i3 : (r2 == 0 ? i1 : i2));
                    const Vertex3D& vv4 = (r1 == 0 ? i2 : (r2 == 0 ? i3 : i1));

                    // First face
                    result_vertices.push_back(vv1);
                    result_vertices.push_back(vv2);
                    result_vertices.push_back(vv3);
                    result_indices.push_back((ushort)result_indices.size());
                    result_indices.push_back((ushort)result_indices.size());
                    result_indices.push_back((ushort)result_indices.size());

                    // Second face
                    result_vertices.push_back(vv3);
                    result_vertices.push_back(vv2);
                    result_vertices.push_back(vv4);
                    result_indices.push_back((ushort)result_indices.size());
                    result_indices.push_back((ushort)result_indices.size());
                    result_indices.push_back((ushort)result_indices.size());
                }
                else if (!ignore && sum == 1) // 1 1 -1, corner out
                {
                    const Vertex3D& vv1 = (r1 == -1 ? i3 : (r2 == -1 ? v1 : i1));
                    const Vertex3D& vv2 = (r1 == -1 ? i2 : (r2 == -1 ? i1 : v2));
                    const Vertex3D& vv3 = (r1 == -1 ? v3 : (r2 == -1 ? i3 : i2));

                    // One face
                    result_vertices.push_back(vv1);
                    result_vertices.push_back(vv2);
                    result_vertices.push_back(vv3);
                    result_indices.push_back((ushort)result_indices.size());
                    result_indices.push_back((ushort)result_indices.size());
                    result_indices.push_back((ushort)result_indices.size());
                }
                else if (ignore || outside)
                {
                    if (ignore && sum == 0) // 1 1 -2
                        continue;

                    result_vertices.push_back(v1);
                    result_vertices.push_back(v2);
                    result_vertices.push_back(v3);
                    result_indices.push_back((ushort)result_indices.size());
                    result_indices.push_back((ushort)result_indices.size());
                    result_indices.push_back((ushort)result_indices.size());
                }
            }

            result_mesh_vertices.push_back((uint)(result_vertices.size() - vertices_old_size));
            result_mesh_indices.push_back((uint)(result_indices.size() - indices_old_size));
        }
        vertices = result_vertices;
        indices = result_indices;
        combined_mesh->MeshVertices = result_mesh_vertices;
        combined_mesh->MeshIndices = result_mesh_indices;
    }

    // Unskin
    if (cut->UnskinBone)
    {
        // Find unskin bone
        Bone* unskin_bone = nullptr;
        for (size_t i = 0; i < combined_mesh->CurBoneMatrix; i++)
        {
            if (combined_mesh->SkinBones[i]->NameHash == cut->UnskinBone)
            {
                unskin_bone = combined_mesh->SkinBones[i];
                break;
            }
        }

        // Unskin
        if (unskin_bone)
        {
            // Process meshes
            size_t v_pos = 0, v_count = 0;
            for (size_t i = 0, j = combined_mesh->MeshVertices.size(); i < j; i++)
            {
                // Check anim layer
                if (std::find(cut_layers.begin(), cut_layers.end(), combined_mesh->MeshAnimLayers[i]) ==
                    cut_layers.end())
                {
                    v_count += combined_mesh->MeshVertices[i];
                    v_pos = v_count;
                    continue;
                }

                // Move shape to face space
                Matrix mesh_transform = combined_mesh->Meshes[i]->Owner->GlobalTransformationMatrix;
                Matrix sm = mesh_transform.Inverse() * cut->UnskinShape.GlobalTransformationMatrix;
                Vector ss, sp;
                Quaternion sr;
                sm.Decompose(ss, sr, sp);
                float sphere_square_radius = powf(cut->UnskinShape.SphereRadius * ss.x, 2.0f);
                bool revert_shape = cut->RevertUnskinShape;

                // Process mesh vertices
                v_count += combined_mesh->MeshVertices[i];
                for (; v_pos < v_count; v_pos++)
                {
                    Vertex3D& v = vertices[v_pos];

                    // Get vertex side
                    bool v_side = ((v.Position - sp).SquareLength() <= sphere_square_radius);
                    if (revert_shape)
                        v_side = !v_side;

                    // Check influences
                    for (int b = 0; b < BONES_PER_VERTEX; b++)
                    {
                        // No influence
                        float w = v.BlendWeights[b];
                        if (w < 0.00001f)
                            continue;

                        // Last influence, don't reskin
                        if (w > 1.0f - 0.00001f)
                            break;

                        // Skip equal influence side
                        bool influence_side =
                            (unskin_bone->Find(combined_mesh->SkinBones[(int)v.BlendIndices[b]]->NameHash) != nullptr);
                        if (v_side == influence_side)
                            continue;

                        // Move influence to other bones
                        v.BlendWeights[b] = 0.0f;
                        for (int b2 = 0; b2 < BONES_PER_VERTEX; b2++)
                            v.BlendWeights[b2] += v.BlendWeights[b2] / (1.0f - w) * w;
                    }
                }
            }
        }
    }
}

bool Animation3d::NeedDraw()
{
    return combinedMeshesSize && (!lastDrawTick || GetTick() - lastDrawTick >= anim3dMngr.animDelay);
}

void Animation3d::Draw(int x, int y)
{
    // Skip drawing if no meshes generated
    if (!combinedMeshesSize)
        return;

    // Move timer
    uint tick = GetTick();
    float elapsed = (lastDrawTick ? 0.001f * (float)(tick - lastDrawTick) : 0.0f);
    lastDrawTick = tick;

    // Move animation
    ProcessAnimation(
        elapsed, x ? x : anim3dMngr.modeWidth / 2, y ? y : anim3dMngr.modeHeight - anim3dMngr.modeHeight / 4, 1.0f);

    // Draw mesh
    DrawCombinedMeshes();
}

void Animation3d::ProcessAnimation(float elapsed, int x, int y, float scale)
{
    // Update world matrix, only for root
    if (!parentBone)
    {
        Vector pos = anim3dMngr.Convert2dTo3d(x, y);
        Matrix mat_rot_y, mat_scale, mat_trans;
        Matrix::Scaling(Vector(scale, scale, scale), mat_scale);
        Matrix::RotationY(dirAngle * PI_VALUE / 180.0f, mat_rot_y);
        Matrix::Translation(pos, mat_trans);
        parentMatrix = mat_trans * matTransBase * matRot * mat_rot_y * matRotBase * mat_scale * matScale * matScaleBase;
        groundPos.x = parentMatrix.a4;
        groundPos.y = parentMatrix.b4;
        groundPos.z = parentMatrix.c4;
    }

    // Advance animation time
    float prev_track_pos;
    float new_track_pos;
    if (animController && elapsed >= 0.0f)
    {
        prev_track_pos = animController->GetTrackPosition(currentTrack);

        elapsed *= GetSpeed();
        animController->AdvanceTime(elapsed);

        new_track_pos = animController->GetTrackPosition(currentTrack);

        if (animPosPeriod > 0.0f)
        {
            animPosProc = new_track_pos / animPosPeriod;
            if (animPosProc >= 1.0f)
                animPosProc = fmod(animPosProc, 1.0f);
            animPosTime = new_track_pos;
            if (animPosTime >= animPosPeriod)
                animPosTime = fmod(animPosTime, animPosPeriod);
        }
    }

    // Update matrices
    UpdateBoneMatrices(animEntity->xFile->rootBone.get(), &parentMatrix);

    // Update linked matrices
    if (parentBone && linkBones.size())
    {
        for (uint i = 0, j = (uint)linkBones.size() / 2; i < j; i++)
            linkBones[i * 2 + 1]->CombinedTransformationMatrix = linkBones[i * 2]->CombinedTransformationMatrix;
    }

    // Update world matrices for children
    for (auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it)
    {
        Animation3d* child = *it;
        child->groundPos = groundPos;
        child->parentMatrix = child->parentBone->CombinedTransformationMatrix * child->matTransBase *
            child->matRotBase * child->matScaleBase;
    }

    // Move child animations
    for (auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it)
        (*it)->ProcessAnimation(elapsed, x, y, 1.0f);

    // Animation callbacks
    if (animController && elapsed >= 0.0f && animPosPeriod > 0.0f)
    {
        for (AnimationCallback& callback : AnimationCallbacks)
        {
            if ((!callback.Anim1 || callback.Anim1 == curAnim1) && (!callback.Anim2 || callback.Anim2 == curAnim2))
            {
                float fire_track_pos1 =
                    floorf(prev_track_pos / animPosPeriod) * animPosPeriod + callback.NormalizedTime * animPosPeriod;
                float fire_track_pos2 =
                    floorf(new_track_pos / animPosPeriod) * animPosPeriod + callback.NormalizedTime * animPosPeriod;
                if ((prev_track_pos < fire_track_pos1 && new_track_pos >= fire_track_pos1) ||
                    (prev_track_pos < fire_track_pos2 && new_track_pos >= fire_track_pos2))
                {
                    callback.Callback();
                }
            }
        }
    }
}

void Animation3d::UpdateBoneMatrices(Bone* bone, const Matrix* parent_matrix)
{
    // If parent matrix exists multiply our bone matrix by it
    bone->CombinedTransformationMatrix = (*parent_matrix) * bone->TransformationMatrix;

    // Update child
    for (auto it = bone->Children.begin(), end = bone->Children.end(); it != end; ++it)
        UpdateBoneMatrices(it->get(), &bone->CombinedTransformationMatrix);
}

void Animation3d::DrawCombinedMeshes()
{
    for (size_t i = 0; i < combinedMeshesSize; i++)
        DrawCombinedMesh(combinedMeshes[i], shadowDisabled || animEntity->shadowDisabled);
}

void Animation3d::DrawCombinedMesh(CombinedMesh* combined_mesh, bool shadow_disabled)
{
    RenderEffect* effect = combined_mesh->DrawEffect;

    memcpy(effect->ProjBuf.ProjMatrix, anim3dMngr.matrixProjCM[0], 16 * sizeof(float));

    effect->MainTex = combined_mesh->Textures[0]->MainTex;
    memcpy(effect->MainTexBuf->MainTexSize, effect->MainTex->SizeData, 4 * sizeof(float));
    effect->MainTexBuf->MainTexSamples = effect->MainTex->Samples;
    effect->MainTexBuf->MainTexSamplesInt = (int)effect->MainTexBuf->MainTexSamples;

    float* wm = effect->ModelBuf->WorldMatrices;
    for (size_t i = 0; i < combined_mesh->CurBoneMatrix; i++)
    {
        Matrix m = combined_mesh->SkinBones[i]->CombinedTransformationMatrix * combined_mesh->SkinBoneOffsets[i];
        m.Transpose(); // Convert to column major order
        memcpy(wm, m[0], 16 * sizeof(float));
        wm += 16;
    }

    memcpy(effect->ModelBuf->GroundPosition, &groundPos[0], 3 * sizeof(float));
    effect->ModelBuf->GroundPosition[3] = 0.0f;

    if (effect->CustomTexBuf)
    {
        for (int i = 0; i < EFFECT_TEXTURES; i++)
        {
            effect->CustomTex[i] = combined_mesh->Textures[i]->MainTex;
            memcpy(&effect->CustomTexBuf->AtlasOffset[i * 4 * sizeof(float)],
                combined_mesh->Textures[i]->AtlasOffsetData, 4 * sizeof(float));
            memcpy(&effect->CustomTexBuf->Size[i * 4 * sizeof(float)], combined_mesh->Textures[i]->MainTex->SizeData,
                4 * sizeof(float));
        }
    }

    if (effect->AnimBuf)
    {
        effect->AnimBuf->NormalizedTime = animPosProc;
        effect->AnimBuf->AbsoluteTime = animPosTime;
    }

    App::Render::DrawMesh(combined_mesh->DrawMesh, effect);
}

bool Animation3d::GetBonePos(hash name_hash, int& x, int& y)
{
    Bone* bone = animEntity->xFile->rootBone->Find(name_hash);
    if (!bone)
    {
        for (Animation3d* child : childAnimations)
        {
            bone = child->animEntity->xFile->rootBone->Find(name_hash);
            if (bone)
                break;
        }
    }

    if (!bone)
        return false;

    Vector pos;
    Quaternion rot;
    bone->CombinedTransformationMatrix.DecomposeNoScaling(rot, pos);

    Point p = anim3dMngr.Convert3dTo2d(pos);
    x = p.X - anim3dMngr.modeWidth / 2;
    y = -(p.Y - anim3dMngr.modeHeight / 4);
    return true;
}

Animation3dEntity::Animation3dEntity(Animation3dManager& anim3d_mngr) : anim3dMngr {anim3d_mngr}
{
    drawWidth = DEFAULT_DRAW_SIZE;
    drawHeight = DEFAULT_DRAW_SIZE;
}

bool Animation3dEntity::Load(const string& name)
{
    string ext = _str(name).getFileExtension();
    if (ext.empty())
        return false;

    // Load fonline 3d file
    if (ext == "fo3d")
    {
        // Load main fo3d file
        File fo3d = anim3dMngr.fileMngr.ReadFile(name);
        if (!fo3d)
            return false;

        // Parse
        string file_buf = fo3d.GetCStr();
        string model;
        string render_fname;
        string render_anim;
        bool disable_animation_interpolation = false;
        bool convert_value_fail = false;

        uint mesh = 0;
        int layer = -1;
        int layer_val = 0;

        AnimParams dummy_link {};
        AnimParams* link = &animDataDefault;
        static uint link_id = 0;

        istringstream* istr = new istringstream(file_buf);
        bool closed = false;
        string token;
        string buf;
        float valuei = 0;
        float valuef = 0.0f;

        struct anim_entry
        {
            int index;
            string fname;
            string name;
        };
        vector<anim_entry> anims;

        while (!istr->eof())
        {
            (*istr) >> token;
            if (istr->fail())
                break;

            size_t comment = token.find(';');
            if (comment == string::npos)
                comment = token.find('#');
            if (comment != string::npos)
            {
                token = token.substr(0, comment);

                string line;
                std::getline(*istr, line, '\n');
                if (token.empty())
                    continue;
            }

            if (closed)
            {
                if (token == "ContinueParsing")
                    closed = false;
                continue;
            }

            if (token == "StopParsing")
            {
                closed = true;
            }
            else if (token == "Model")
            {
                (*istr) >> buf;
                model = _str(name).combinePath(buf);
            }
            else if (token == "Include")
            {
                // Get swapped words
                StrVec templates;
                string line;
                std::getline(*istr, line, '\n');
                templates = _str(line).trim().split(' ');
                if (templates.empty())
                    continue;

                for (size_t i = 1; i < templates.size() - 1; i += 2)
                    templates[i] = _str("%{}%", templates[i]);

                // Include file path
                string fname = _str(name).combinePath(templates[0]);
                File fo3d_ex = anim3dMngr.fileMngr.ReadFile(fname);
                if (!fo3d_ex)
                {
                    WriteLog("Include file '{}' not found.\n", fname);
                    continue;
                }

                // Words swapping
                string new_content = fo3d_ex.GetCStr();
                if (templates.size() > 2)
                    for (size_t i = 1; i < templates.size() - 1; i += 2)
                        new_content = _str(new_content).replace(templates[i], templates[i + 1]);

                // Insert new buffer
                file_buf = _str("{}\n{}", new_content, !istr->eof() ? file_buf.substr((uint)istr->tellg()) : "");

                // Reinitialize stream
                delete istr;
                istr = new istringstream(file_buf);
            }
            else if (token == "Mesh")
            {
                (*istr) >> buf;
                if (buf != "All")
                    mesh = Bone::GetHash(buf);
                else
                    mesh = 0;
            }
            else if (token == "Subset")
            {
                (*istr) >> buf;
                WriteLog("Tag 'Subset' obsolete, use 'Mesh' instead.\n");
            }
            else if (token == "Layer" || token == "Value")
            {
                (*istr) >> buf;
                if (token == "Layer")
                    layer = GenericUtils::ConvertParamValue(buf, convert_value_fail);
                else
                    layer_val = GenericUtils::ConvertParamValue(buf, convert_value_fail);

                link = &dummy_link;
                mesh = 0;
            }
            else if (token == "Root")
            {
                if (layer == -1)
                {
                    link = &animDataDefault;
                }
                else if (!layer_val)
                {
                    WriteLog("Wrong layer '{}' zero value.\n", layer);
                    link = &dummy_link;
                }
                else
                {
                    animData.push_back(AnimParams());
                    link = &animData.back();
                    memzero(link, sizeof(AnimParams));
                    link->Id = ++link_id;
                    link->Layer = layer;
                    link->LayerValue = layer_val;
                }

                mesh = 0;
            }
            else if (token == "Attach")
            {
                (*istr) >> buf;
                if (layer < 0 || !layer_val)
                    continue;

                animData.push_back(AnimParams());
                link = &animData.back();
                memzero(link, sizeof(AnimParams));
                link->Id = ++link_id;
                link->Layer = layer;
                link->LayerValue = layer_val;

                string fname = _str(name).combinePath(buf);
                link->ChildName = fname;

                mesh = 0;
            }
            else if (token == "Link")
            {
                (*istr) >> buf;
                if (link->Id)
                    link->LinkBoneHash = Bone::GetHash(buf);
            }
            else if (token == "Cut")
            {
                (*istr) >> buf;
                string fname = _str(name).combinePath(buf);
                Animation3dXFile* area = anim3dMngr.GetXFile(fname);
                if (area)
                {
                    // Add cut
                    CutData* cut = new CutData();
                    link->CutInfo.push_back(cut);

                    // Layers
                    (*istr) >> buf;
                    StrVec layers = _str(buf).split('-');
                    for (uint m = 0, n = (uint)layers.size(); m < n; m++)
                    {
                        if (layers[m] != "All")
                        {
                            int layer = GenericUtils::ConvertParamValue(layers[m], convert_value_fail);
                            cut->Layers.push_back(layer);
                        }
                        else
                        {
                            for (int i = 0; i < LAYERS3D_COUNT; i++)
                                if (i != layer)
                                    cut->Layers.push_back(i);
                        }
                    }

                    // Shapes
                    (*istr) >> buf;
                    StrVec shapes = _str(buf).split('-');

                    // Unskin bone
                    (*istr) >> buf;
                    cut->UnskinBone = Bone::GetHash(buf);
                    if (buf == "-")
                        cut->UnskinBone = 0;

                    // Unskin shape
                    (*istr) >> buf;
                    uint unskin_shape_name = 0;
                    cut->RevertUnskinShape = false;
                    if (cut->UnskinBone)
                    {
                        cut->RevertUnskinShape = (!buf.empty() && buf[0] == '~');
                        unskin_shape_name = Bone::GetHash(!buf.empty() && buf[0] == '~' ? buf.substr(1) : buf);
                        for (size_t k = 0, l = area->allDrawBones.size(); k < l; k++)
                        {
                            if (unskin_shape_name == area->allDrawBones[k]->NameHash)
                                cut->UnskinShape = CreateCutShape(area->allDrawBones[k]->AttachedMesh.get());
                        }
                    }

                    // Parse shapes
                    for (uint m = 0, n = (uint)shapes.size(); m < n; m++)
                    {
                        hash shape_name = Bone::GetHash(shapes[m]);
                        if (shapes[m] == "All")
                            shape_name = 0;
                        for (size_t k = 0, l = area->allDrawBones.size(); k < l; k++)
                        {
                            if ((!shape_name || shape_name == area->allDrawBones[k]->NameHash) &&
                                area->allDrawBones[k]->NameHash != unskin_shape_name)
                            {
                                cut->Shapes.push_back(CreateCutShape(area->allDrawBones[k]->AttachedMesh.get()));
                            }
                        }
                    }
                }
                else
                {
                    WriteLog("Cut file '{}' not found.\n", fname);
                    (*istr) >> buf;
                    (*istr) >> buf;
                    (*istr) >> buf;
                    (*istr) >> buf;
                }
            }
            else if (token == "RotX")
                (*istr) >> link->RotX;
            else if (token == "RotY")
                (*istr) >> link->RotY;
            else if (token == "RotZ")
                (*istr) >> link->RotZ;
            else if (token == "MoveX")
                (*istr) >> link->MoveX;
            else if (token == "MoveY")
                (*istr) >> link->MoveY;
            else if (token == "MoveZ")
                (*istr) >> link->MoveZ;
            else if (token == "ScaleX")
                (*istr) >> link->ScaleX;
            else if (token == "ScaleY")
                (*istr) >> link->ScaleY;
            else if (token == "ScaleZ")
                (*istr) >> link->ScaleZ;
            else if (token == "Scale")
            {
                (*istr) >> valuef;
                link->ScaleX = link->ScaleY = link->ScaleZ = valuef;
            }
            else if (token == "Speed")
                (*istr) >> link->SpeedAjust;
            else if (token == "RotX+")
            {
                (*istr) >> valuef;
                link->RotX = (link->RotX == 0.0f ? valuef : link->RotX + valuef);
            }
            else if (token == "RotY+")
            {
                (*istr) >> valuef;
                link->RotY = (link->RotY == 0.0f ? valuef : link->RotY + valuef);
            }
            else if (token == "RotZ+")
            {
                (*istr) >> valuef;
                link->RotZ = (link->RotZ == 0.0f ? valuef : link->RotZ + valuef);
            }
            else if (token == "MoveX+")
            {
                (*istr) >> valuef;
                link->MoveX = (link->MoveX == 0.0f ? valuef : link->MoveX + valuef);
            }
            else if (token == "MoveY+")
            {
                (*istr) >> valuef;
                link->MoveY = (link->MoveY == 0.0f ? valuef : link->MoveY + valuef);
            }
            else if (token == "MoveZ+")
            {
                (*istr) >> valuef;
                link->MoveZ = (link->MoveZ == 0.0f ? valuef : link->MoveZ + valuef);
            }
            else if (token == "ScaleX+")
            {
                (*istr) >> valuef;
                link->ScaleX = (link->ScaleX == 0.0f ? valuef : link->ScaleX + valuef);
            }
            else if (token == "ScaleY+")
            {
                (*istr) >> valuef;
                link->ScaleY = (link->ScaleY == 0.0f ? valuef : link->ScaleY + valuef);
            }
            else if (token == "ScaleZ+")
            {
                (*istr) >> valuef;
                link->ScaleZ = (link->ScaleZ == 0.0f ? valuef : link->ScaleZ + valuef);
            }
            else if (token == "Scale+")
            {
                (*istr) >> valuef;
                link->ScaleX = (link->ScaleX == 0.0f ? valuef : link->ScaleX + valuef);
                link->ScaleY = (link->ScaleY == 0.0f ? valuef : link->ScaleY + valuef);
                link->ScaleZ = (link->ScaleZ == 0.0f ? valuef : link->ScaleZ + valuef);
            }
            else if (token == "Speed+")
            {
                (*istr) >> valuef;
                link->SpeedAjust = (link->SpeedAjust == 0.0f ? valuef : link->SpeedAjust * valuef);
            }
            else if (token == "RotX*")
            {
                (*istr) >> valuef;
                link->RotX = (link->RotX == 0.0f ? valuef : link->RotX * valuef);
            }
            else if (token == "RotY*")
            {
                (*istr) >> valuef;
                link->RotY = (link->RotY == 0.0f ? valuef : link->RotY * valuef);
            }
            else if (token == "RotZ*")
            {
                (*istr) >> valuef;
                link->RotZ = (link->RotZ == 0.0f ? valuef : link->RotZ * valuef);
            }
            else if (token == "MoveX*")
            {
                (*istr) >> valuef;
                link->MoveX = (link->MoveX == 0.0f ? valuef : link->MoveX * valuef);
            }
            else if (token == "MoveY*")
            {
                (*istr) >> valuef;
                link->MoveY = (link->MoveY == 0.0f ? valuef : link->MoveY * valuef);
            }
            else if (token == "MoveZ*")
            {
                (*istr) >> valuef;
                link->MoveZ = (link->MoveZ == 0.0f ? valuef : link->MoveZ * valuef);
            }
            else if (token == "ScaleX*")
            {
                (*istr) >> valuef;
                link->ScaleX = (link->ScaleX == 0.0f ? valuef : link->ScaleX * valuef);
            }
            else if (token == "ScaleY*")
            {
                (*istr) >> valuef;
                link->ScaleY = (link->ScaleY == 0.0f ? valuef : link->ScaleY * valuef);
            }
            else if (token == "ScaleZ*")
            {
                (*istr) >> valuef;
                link->ScaleZ = (link->ScaleZ == 0.0f ? valuef : link->ScaleZ * valuef);
            }
            else if (token == "Scale*")
            {
                (*istr) >> valuef;
                link->ScaleX = (link->ScaleX == 0.0f ? valuef : link->ScaleX * valuef);
                link->ScaleY = (link->ScaleY == 0.0f ? valuef : link->ScaleY * valuef);
                link->ScaleZ = (link->ScaleZ == 0.0f ? valuef : link->ScaleZ * valuef);
            }
            else if (token == "Speed*")
            {
                (*istr) >> valuef;
                link->SpeedAjust = (link->SpeedAjust == 0.0f ? valuef : link->SpeedAjust * valuef);
            }
            else if (token == "DisableLayer")
            {
                (*istr) >> buf;
                StrVec layers = _str(buf).split('-');
                for (uint m = 0, n = (uint)layers.size(); m < n; m++)
                {
                    int layer = GenericUtils::ConvertParamValue(layers[m], convert_value_fail);
                    if (layer >= 0 && layer < LAYERS3D_COUNT)
                        link->DisabledLayer.push_back(layer);
                }
            }
            else if (token == "DisableSubset")
            {
                (*istr) >> buf;
                WriteLog("Tag 'DisableSubset' obsolete, use 'DisableMesh' instead.\n");
            }
            else if (token == "DisableMesh")
            {
                (*istr) >> buf;
                StrVec meshes = _str(buf).split('-');
                for (uint m = 0, n = (uint)meshes.size(); m < n; m++)
                {
                    uint mesh_name_hash = 0;
                    if (meshes[m] != "All")
                        mesh_name_hash = Bone::GetHash(meshes[m]);

                    link->DisabledMesh.push_back(mesh_name_hash);
                }
            }
            else if (token == "Texture")
            {
                (*istr) >> buf;
                int index = GenericUtils::ConvertParamValue(buf, convert_value_fail);
                (*istr) >> buf;
                if (index >= 0 && index < EFFECT_TEXTURES)
                    link->TextureInfo.emplace_back(buf, mesh, index);
            }
            else if (token == "Effect")
            {
                (*istr) >> buf;

                link->EffectInfo.emplace_back(buf, mesh);
            }
            else if (token == "Anim" || token == "AnimSpeed" || token == "AnimExt" || token == "AnimSpeedExt")
            {
                // Index animation
                int ind1 = 0, ind2 = 0;
                (*istr) >> buf;
                ind1 = GenericUtils::ConvertParamValue(buf, convert_value_fail);
                (*istr) >> buf;
                ind2 = GenericUtils::ConvertParamValue(buf, convert_value_fail);

                if (token == "Anim" || token == "AnimExt")
                {
                    // Todo: add reverse playing of 3d animation

                    string a1, a2;
                    (*istr) >> a1 >> a2;
                    anims.push_back({(ind1 << 16) | ind2, a1, a2});

                    if (token == "AnimExt")
                    {
                        (*istr) >> a1 >> a2;
                        anims.push_back({(ind1 << 16) | (ind2 | 0x8000), a1, a2});
                    }
                }
                else
                {
                    (*istr) >> valuef;
                    animSpeed.insert(std::make_pair((ind1 << 16) | ind2, valuef));

                    if (token == "AnimSpeedExt")
                    {
                        (*istr) >> valuef;
                        animSpeed.insert(std::make_pair((ind1 << 16) | (ind2 | 0x8000), valuef));
                    }
                }
            }
            else if (token == "AnimLayerValue")
            {
                int ind1 = 0, ind2 = 0;
                (*istr) >> buf;
                ind1 = GenericUtils::ConvertParamValue(buf, convert_value_fail);
                (*istr) >> buf;
                ind2 = GenericUtils::ConvertParamValue(buf, convert_value_fail);

                int layer = 0, value = 0;
                (*istr) >> buf;
                layer = GenericUtils::ConvertParamValue(buf, convert_value_fail);
                (*istr) >> buf;
                value = GenericUtils::ConvertParamValue(buf, convert_value_fail);

                uint index = (ind1 << 16) | ind2;
                if (!animLayerValues.count(index))
                    animLayerValues.insert(std::make_pair(index, IntPairVec()));
                animLayerValues[index].push_back(IntPair(layer, value));
            }
            else if (token == "FastTransitionBone")
            {
                (*istr) >> buf;
                fastTransitionBones.insert(Bone::GetHash(buf));
            }
            else if (token == "AnimEqual")
            {
                (*istr) >> valuei;

                int ind1 = 0, ind2 = 0;
                (*istr) >> buf;
                ind1 = GenericUtils::ConvertParamValue(buf, convert_value_fail);
                (*istr) >> buf;
                ind2 = GenericUtils::ConvertParamValue(buf, convert_value_fail);

                if (valuei == 1)
                    anim1Equals.insert(std::make_pair(ind1, ind2));
                else if (valuei == 2)
                    anim2Equals.insert(std::make_pair(ind1, ind2));
            }
            else if (token == "RenderFrame" || token == "RenderFrames")
            {
                anims.push_back({0, render_fname, render_anim});

                (*istr) >> renderAnimProcFrom;

                // One frame
                renderAnimProcTo = renderAnimProcFrom;

                // Many frames
                if (token == "RenderFrames")
                    (*istr) >> renderAnimProcTo;

                // Check
                renderAnimProcFrom = CLAMP(renderAnimProcFrom, 0, 100);
                renderAnimProcTo = CLAMP(renderAnimProcTo, 0, 100);
            }
            else if (token == "RenderDir")
            {
                (*istr) >> buf;

                renderAnimDir = GenericUtils::ConvertParamValue(buf, convert_value_fail);
            }
            else if (token == "DisableShadow")
            {
                shadowDisabled = true;
            }
            else if (token == "DrawSize")
            {
                int w = 0, h = 0;
                (*istr) >> buf;
                w = GenericUtils::ConvertParamValue(buf, convert_value_fail);
                (*istr) >> buf;
                h = GenericUtils::ConvertParamValue(buf, convert_value_fail);

                drawWidth = w;
                drawHeight = h;
            }
            else if (token == "DisableAnimationInterpolation")
            {
                disable_animation_interpolation = true;
            }
            else
            {
                WriteLog("Unknown token '{}' in file '{}'.\n", token, name);
            }
        }

        // Process pathes
        if (model.empty())
        {
            WriteLog("'Model' section not found in file '{}'.\n", name);
            return false;
        }

        // Check for correct param values
        if (convert_value_fail)
        {
            WriteLog("Invalid param values for file '{}'.\n", name);
            return false;
        }

        // Load x file
        Animation3dXFile* xfile = anim3dMngr.GetXFile(model);
        if (!xfile)
            return false;

        // Create animation
        fileName = name;
        xFile = xfile;

        // Single frame render
        if (render_fname[0] && render_anim[0])
            anims.push_back({-1, render_fname, render_anim});

        // Create animation controller
        if (!anims.empty())
            animController = std::make_unique<AnimController>(2);

        // Parse animations
        if (animController)
        {
            for (const auto& anim : anims)
            {
                string anim_path;
                if (anim.fname == "ModelFile")
                    anim_path = model;
                else
                    anim_path = _str(name).combinePath(anim.fname);

                AnimSet* set = anim3dMngr.LoadAnimation(anim_path, anim.name);
                if (set)
                {
                    animController->RegisterAnimationSet(set);
                    uint set_index = animController->GetNumAnimationSets() - 1;

                    if (anim.index == -1)
                        renderAnim = set_index;
                    else if (anim.index)
                        animIndexes.insert(std::make_pair(anim.index, set_index));
                }
            }

            numAnimationSets = animController->GetNumAnimationSets();
            if (numAnimationSets > 0)
            {
                // Add animation bones, not included to base hierarchy
                // All bones linked with animation in SetupAnimationOutput
                for (uint i = 0; i < numAnimationSets; i++)
                {
                    AnimSet* set = animController->GetAnimationSet(i);
                    UIntVecVec& bones_hierarchy = set->GetBonesHierarchy();
                    for (size_t j = 0; j < bones_hierarchy.size(); j++)
                    {
                        UIntVec& bone_hierarchy = bones_hierarchy[j];
                        Bone* bone = xFile->rootBone.get();
                        for (size_t b = 1; b < bone_hierarchy.size(); b++)
                        {
                            Bone* child = bone->Find(bone_hierarchy[b]);
                            if (!child)
                            {
                                child = new Bone();
                                child->NameHash = bone_hierarchy[b];
                                bone->Children.push_back(unique_ptr<Bone>(child));
                            }
                            bone = child;
                        }
                    }
                }

                animController->SetInterpolation(!disable_animation_interpolation);
                xFile->SetupAnimationOutput(animController.get());
            }
            else
            {
                animController = nullptr;
            }
        }
    }
    // Load just x file
    else
    {
        Animation3dXFile* xfile = anim3dMngr.GetXFile(name);
        if (!xfile)
            return false;

        // Create animation
        fileName = name;
        xFile = xfile;

        // Todo: process default animations
    }

    return true;
}

int Animation3dEntity::GetAnimationIndex(uint& anim1, uint& anim2, float* speed, bool combat_first)
{
    // Find index
    int index = -1;
    if (combat_first)
        index = GetAnimationIndexEx(anim1, anim2 | 0x8000, speed);
    if (index == -1)
        index = GetAnimationIndexEx(anim1, anim2, speed);
    if (!combat_first && index == -1)
        index = GetAnimationIndexEx(anim1, anim2 | 0x8000, speed);
    if (index != -1)
        return index;

    // Find substitute animation
    hash base_model_name = 0;
    uint anim1_base = anim1, anim2_base = anim2;
    while (index == -1)
    {
        hash model_name = base_model_name;
        uint anim1_ = anim1, anim2_ = anim2;
        if (anim3dMngr.scriptSys.RaiseInternalEvent(ClientFunctions.CritterAnimationSubstitute, base_model_name,
                anim1_base, anim2_base, &model_name, &anim1, &anim2) &&
            (anim1 != anim1_ || anim2 != anim2_))
            index = GetAnimationIndexEx(anim1, anim2, speed);
        else
            break;
    }

    return index;
}

int Animation3dEntity::GetAnimationIndexEx(uint anim1, uint anim2, float* speed)
{
    // Check equals
    auto it1 = anim1Equals.find(anim1);
    if (it1 != anim1Equals.end())
        anim1 = (*it1).second;
    auto it2 = anim2Equals.find(anim2 & 0x7FFF);
    if (it2 != anim2Equals.end())
        anim2 = ((*it2).second | (anim2 & 0x8000));

    // Make index
    int ii = (anim1 << 16) | anim2;

    // Speed
    if (speed)
    {
        auto it = animSpeed.find(ii);
        if (it != animSpeed.end())
            *speed = it->second;
        else
            *speed = 1.0f;
    }

    // Find number of animation
    auto it = animIndexes.find(ii);
    if (it != animIndexes.end())
        return it->second;

    return -1;
}

Animation3d* Animation3dEntity::CloneAnimation()
{
    Animation3d* a3d = new Animation3d(anim3dMngr);
    a3d->animEntity = this;
    if (animController)
        a3d->animController = animController->Clone();
    return a3d;
}

CutData::Shape Animation3dEntity::CreateCutShape(MeshData* mesh)
{
    CutData::Shape shape;
    shape.GlobalTransformationMatrix = mesh->Owner->GlobalTransformationMatrix;

    // Calculate sphere radius
    float vmin, vmax;
    for (size_t i = 0, j = mesh->Vertices.size(); i < j; i++)
    {
        Vertex3D v = mesh->Vertices[i];
        if (!i || v.Position.x < vmin)
            vmin = v.Position.x;
        if (!i || v.Position.x > vmax)
            vmax = v.Position.x;
    }

    shape.SphereRadius = (vmax - vmin) / 2.0f;
    return shape;
}

Animation3dXFile::Animation3dXFile(Animation3dManager& anim3d_mngr) : anim3dMngr {anim3d_mngr}
{
}

void SetupBonesExt(multimap<uint, Bone*>& bones, Bone* bone, uint depth)
{
    bones.insert(std::make_pair(depth, bone));
    for (size_t i = 0, j = bone->Children.size(); i < j; i++)
        SetupBonesExt(bones, bone->Children[i].get(), depth + 1);
}

void Animation3dXFile::SetupBones()
{
    multimap<uint, Bone*> bones;
    SetupBonesExt(bones, rootBone.get(), 0);

    for (auto it = bones.begin(), end = bones.end(); it != end; ++it)
    {
        Bone* bone = it->second;
        allBones.push_back(bone);
        if (bone->AttachedMesh)
            allDrawBones.push_back(bone);
    }
}

void SetupAnimationOutputExt(AnimController* anim_controller, Bone* bone)
{
    anim_controller->RegisterAnimationOutput(bone->NameHash, bone->TransformationMatrix);

    for (auto it = bone->Children.begin(), end = bone->Children.end(); it != end; ++it)
        SetupAnimationOutputExt(anim_controller, it->get());
}

void Animation3dXFile::SetupAnimationOutput(AnimController* anim_controller)
{
    SetupAnimationOutputExt(anim_controller, rootBone.get());
}

MeshTexture* Animation3dXFile::GetTexture(const string& tex_name)
{
    MeshTexture* texture = anim3dMngr.LoadTexture(tex_name, fileName);
    if (!texture)
        WriteLog("Can't load texture '{}'.\n", tex_name);
    return texture;
}

RenderEffect* Animation3dXFile::GetEffect(const string& name)
{
    RenderEffect* effect = anim3dMngr.effectMngr.LoadEffect(name, "", fileName);
    if (!effect)
        WriteLog("Can't load effect '{}'.\n", name);
    return effect;
}
