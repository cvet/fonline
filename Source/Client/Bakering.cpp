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

#include "Bakering.h"
#include "GenericUtils.h"
#include "Log.h"
#include "StringUtils.h"
#include "Testing.h"
#include "Version_Include.h"

void Bakering::SaveBone(Bone* bone, DataWriter& writer)
{
    writer.WritePtr(&bone->NameHash, sizeof(bone->NameHash));
    writer.WritePtr(&bone->TransformationMatrix, sizeof(bone->TransformationMatrix));
    writer.WritePtr(&bone->GlobalTransformationMatrix, sizeof(bone->GlobalTransformationMatrix));
    writer.Write<uchar>(bone->AttachedMesh != nullptr ? 1 : 0);
    if (bone->AttachedMesh)
        SaveMeshData(bone->AttachedMesh.get(), writer);
    uint len = (uint)bone->Children.size();
    writer.WritePtr(&len, sizeof(len));
    for (auto& child : bone->Children)
        SaveBone(child.get(), writer);
}

void Bakering::LoadBone(Bone* bone, DataReader& reader)
{
    reader.ReadPtr(&bone->NameHash, sizeof(bone->NameHash));
    reader.ReadPtr(&bone->TransformationMatrix, sizeof(bone->TransformationMatrix));
    reader.ReadPtr(&bone->GlobalTransformationMatrix, sizeof(bone->GlobalTransformationMatrix));
    if (reader.Read<uchar>())
    {
        bone->AttachedMesh = std::make_unique<MeshData>();
        LoadMeshData(bone->AttachedMesh.get(), reader);
        bone->AttachedMesh->Owner = bone;
    }
    else
    {
        bone->AttachedMesh = nullptr;
    }
    uint len = 0;
    reader.ReadPtr(&len, sizeof(len));
    for (uint i = 0; i < len; i++)
    {
        auto child = std::make_unique<Bone>();
        LoadBone(child.get(), reader);
        bone->Children.push_back(std::move(child));
    }
    bone->CombinedTransformationMatrix = Matrix();
}

void Bakering::FixBoneAfterLoad(Bone* bone, Bone* root_bone)
{
    if (bone->AttachedMesh)
    {
        MeshData* mesh = bone->AttachedMesh.get();
        for (size_t i = 0, j = mesh->SkinBoneNameHashes.size(); i < j; i++)
        {
            if (mesh->SkinBoneNameHashes[i])
                mesh->SkinBones[i] = root_bone->Find(mesh->SkinBoneNameHashes[i]);
            else
                mesh->SkinBones[i] = mesh->Owner;
        }
    }

    for (auto& child : bone->Children)
        FixBoneAfterLoad(child.get(), root_bone);
}

void Bakering::SaveMeshData(MeshData* mesh, DataWriter& writer)
{
    uint len = (uint)mesh->Vertices.size();
    writer.WritePtr(&len, sizeof(len));
    writer.WritePtr(&mesh->Vertices[0], len * sizeof(mesh->Vertices[0]));
    len = (uint)mesh->Indices.size();
    writer.WritePtr(&len, sizeof(len));
    writer.WritePtr(&mesh->Indices[0], len * sizeof(mesh->Indices[0]));
    len = (uint)mesh->DiffuseTexture.length();
    writer.WritePtr(&len, sizeof(len));
    writer.WritePtr(&mesh->DiffuseTexture[0], len);
    len = (uint)mesh->SkinBoneNameHashes.size();
    writer.WritePtr(&len, sizeof(len));
    writer.WritePtr(&mesh->SkinBoneNameHashes[0], len * sizeof(mesh->SkinBoneNameHashes[0]));
    len = (uint)mesh->SkinBoneOffsets.size();
    writer.WritePtr(&len, sizeof(len));
    writer.WritePtr(&mesh->SkinBoneOffsets[0], len * sizeof(mesh->SkinBoneOffsets[0]));
}

void Bakering::LoadMeshData(MeshData* mesh, DataReader& reader)
{
    uint len = 0;
    reader.ReadPtr(&len, sizeof(len));
    mesh->Vertices.resize(len);
    reader.ReadPtr(&mesh->Vertices[0], len * sizeof(mesh->Vertices[0]));
    reader.ReadPtr(&len, sizeof(len));
    mesh->Indices.resize(len);
    reader.ReadPtr(&mesh->Indices[0], len * sizeof(mesh->Indices[0]));
    reader.ReadPtr(&len, sizeof(len));
    mesh->DiffuseTexture.resize(len);
    reader.ReadPtr(&mesh->DiffuseTexture[0], len);
    reader.ReadPtr(&len, sizeof(len));
    mesh->SkinBoneNameHashes.resize(len);
    reader.ReadPtr(&mesh->SkinBoneNameHashes[0], len * sizeof(mesh->SkinBoneNameHashes[0]));
    reader.ReadPtr(&len, sizeof(len));
    mesh->SkinBoneOffsets.resize(len);
    reader.ReadPtr(&mesh->SkinBoneOffsets[0], len * sizeof(mesh->SkinBoneOffsets[0]));
    mesh->SkinBones.resize(mesh->SkinBoneOffsets.size());
}

void Bakering::SaveAnimSet(AnimSet* anim_set, DataWriter& writer)
{
    uint len = (uint)anim_set->animFileName.length();
    writer.WritePtr(&len, sizeof(len));
    writer.WritePtr(&anim_set->animFileName[0], len);
    len = (uint)anim_set->animName.length();
    writer.WritePtr(&len, sizeof(len));
    writer.WritePtr(&anim_set->animName[0], len);
    writer.WritePtr(&anim_set->durationTicks, sizeof(anim_set->durationTicks));
    writer.WritePtr(&anim_set->ticksPerSecond, sizeof(anim_set->ticksPerSecond));
    len = (uint)anim_set->bonesHierarchy.size();
    writer.WritePtr(&len, sizeof(len));
    for (uint i = 0, j = (uint)anim_set->bonesHierarchy.size(); i < j; i++)
    {
        len = (uint)anim_set->bonesHierarchy[i].size();
        writer.WritePtr(&len, sizeof(len));
        writer.WritePtr(&anim_set->bonesHierarchy[i][0], len * sizeof(anim_set->bonesHierarchy[0][0]));
    }
    len = (uint)anim_set->boneOutputs.size();
    writer.WritePtr(&len, sizeof(len));
    for (auto it = anim_set->boneOutputs.begin(); it != anim_set->boneOutputs.end(); ++it)
    {
        AnimSet::BoneOutput& o = *it;
        writer.WritePtr(&o.nameHash, sizeof(o.nameHash));
        len = (uint)o.scaleTime.size();
        writer.WritePtr(&len, sizeof(len));
        writer.WritePtr(&o.scaleTime[0], len * sizeof(o.scaleTime[0]));
        writer.WritePtr(&o.scaleValue[0], len * sizeof(o.scaleValue[0]));
        len = (uint)o.rotationTime.size();
        writer.WritePtr(&len, sizeof(len));
        writer.WritePtr(&o.rotationTime[0], len * sizeof(o.rotationTime[0]));
        writer.WritePtr(&o.rotationValue[0], len * sizeof(o.rotationValue[0]));
        len = (uint)o.translationTime.size();
        writer.WritePtr(&len, sizeof(len));
        writer.WritePtr(&o.translationTime[0], len * sizeof(o.translationTime[0]));
        writer.WritePtr(&o.translationValue[0], len * sizeof(o.translationValue[0]));
    }
}

void Bakering::LoadAnimSet(AnimSet* anim_set, DataReader& reader)
{
    uint len = 0;
    reader.ReadPtr(&len, sizeof(len));
    anim_set->animFileName.resize(len);
    reader.ReadPtr(&anim_set->animFileName[0], len);
    reader.ReadPtr(&len, sizeof(len));
    anim_set->animName.resize(len);
    reader.ReadPtr(&anim_set->animName[0], len);
    reader.ReadPtr(&anim_set->durationTicks, sizeof(anim_set->durationTicks));
    reader.ReadPtr(&anim_set->ticksPerSecond, sizeof(anim_set->ticksPerSecond));
    reader.ReadPtr(&len, sizeof(len));
    anim_set->bonesHierarchy.resize(len);
    for (uint i = 0, j = len; i < j; i++)
    {
        reader.ReadPtr(&len, sizeof(len));
        anim_set->bonesHierarchy[i].resize(len);
        reader.ReadPtr(&anim_set->bonesHierarchy[i][0], len * sizeof(anim_set->bonesHierarchy[0][0]));
    }
    reader.ReadPtr(&len, sizeof(len));
    anim_set->boneOutputs.resize(len);
    for (uint i = 0, j = len; i < j; i++)
    {
        AnimSet::BoneOutput& o = anim_set->boneOutputs[i];
        reader.ReadPtr(&o.nameHash, sizeof(o.nameHash));
        reader.ReadPtr(&len, sizeof(len));
        o.scaleTime.resize(len);
        o.scaleValue.resize(len);
        reader.ReadPtr(&o.scaleTime[0], len * sizeof(o.scaleTime[0]));
        reader.ReadPtr(&o.scaleValue[0], len * sizeof(o.scaleValue[0]));
        reader.ReadPtr(&len, sizeof(len));
        o.rotationTime.resize(len);
        o.rotationValue.resize(len);
        reader.ReadPtr(&o.rotationTime[0], len * sizeof(o.rotationTime[0]));
        reader.ReadPtr(&o.rotationValue[0], len * sizeof(o.rotationValue[0]));
        reader.ReadPtr(&len, sizeof(len));
        o.translationTime.resize(len);
        o.translationValue.resize(len);
        reader.ReadPtr(&o.translationTime[0], len * sizeof(o.translationTime[0]));
        reader.ReadPtr(&o.translationValue[0], len * sizeof(o.translationValue[0]));
    }
}
