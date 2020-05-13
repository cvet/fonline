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

#include "ModelBaker.h"
#include "Application.h"
#include "Log.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Testing.h"

#include "fbxsdk.h"

// Linker errors workaround
#define MeshData BakerMeshData
#define Bone BakerBone
#define AnimSet BakerAnimSet

struct MeshData : public NonCopyable
{
    void Save(DataWriter& writer)
    {
        uint len = (uint)Vertices.size();
        writer.WritePtr(&len, sizeof(len));
        writer.WritePtr(&Vertices[0], len * sizeof(Vertices[0]));
        len = (uint)Indices.size();
        writer.WritePtr(&len, sizeof(len));
        writer.WritePtr(&Indices[0], len * sizeof(Indices[0]));
        len = (uint)DiffuseTexture.length();
        writer.WritePtr(&len, sizeof(len));
        writer.WritePtr(&DiffuseTexture[0], len);
        len = (uint)SkinBones.size();
        writer.WritePtr(&len, sizeof(len));
        writer.WritePtr(&SkinBones[0], len * sizeof(SkinBones[0]));
        len = (uint)SkinBoneOffsets.size();
        writer.WritePtr(&len, sizeof(len));
        writer.WritePtr(&SkinBoneOffsets[0], len * sizeof(SkinBoneOffsets[0]));
    }

    Vertex3DVec Vertices {};
    UShortVec Indices {};
    string DiffuseTexture {};
    HashVec SkinBones {};
    MatrixVec SkinBoneOffsets {};
    string EffectName {};
};

struct Bone : public NonCopyable
{
    static hash GetHash(const string& name) { return _str(name).toHash(); }

    Bone* Find(hash name_hash)
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

    void Save(DataWriter& writer)
    {
        writer.WritePtr(&NameHash, sizeof(NameHash));
        writer.WritePtr(&TransformationMatrix, sizeof(TransformationMatrix));
        writer.WritePtr(&GlobalTransformationMatrix, sizeof(GlobalTransformationMatrix));
        writer.Write<uchar>(AttachedMesh != nullptr ? 1 : 0);
        if (AttachedMesh)
            AttachedMesh->Save(writer);
        uint len = (uint)Children.size();
        writer.WritePtr(&len, sizeof(len));
        for (auto& child : Children)
            child->Save(writer);
    }

    hash NameHash {};
    Matrix TransformationMatrix {};
    Matrix GlobalTransformationMatrix {};
    unique_ptr<MeshData> AttachedMesh {};
    vector<unique_ptr<Bone>> Children {};
    Matrix CombinedTransformationMatrix {};
};

struct AnimSet : public NonCopyable
{
    struct BoneOutput
    {
        hash NameHash {};
        FloatVec ScaleTime {};
        VectorVec ScaleValue {};
        FloatVec RotationTime {};
        QuaternionVec RotationValue {};
        FloatVec TranslationTime {};
        VectorVec TranslationValue {};
    };

    void Save(DataWriter& writer)
    {
        uint len = (uint)AnimFileName.length();
        writer.WritePtr(&len, sizeof(len));
        writer.WritePtr(&AnimFileName[0], len);
        len = (uint)AnimName.length();
        writer.WritePtr(&len, sizeof(len));
        writer.WritePtr(&AnimName[0], len);
        writer.WritePtr(&DurationTicks, sizeof(DurationTicks));
        writer.WritePtr(&TicksPerSecond, sizeof(TicksPerSecond));
        len = (uint)BonesHierarchy.size();
        writer.WritePtr(&len, sizeof(len));
        for (uint i = 0, j = (uint)BonesHierarchy.size(); i < j; i++)
        {
            len = (uint)BonesHierarchy[i].size();
            writer.WritePtr(&len, sizeof(len));
            writer.WritePtr(&BonesHierarchy[i][0], len * sizeof(BonesHierarchy[0][0]));
        }
        len = (uint)BoneOutputs.size();
        writer.WritePtr(&len, sizeof(len));
        for (auto it = BoneOutputs.begin(); it != BoneOutputs.end(); ++it)
        {
            AnimSet::BoneOutput& o = *it;
            writer.WritePtr(&o.NameHash, sizeof(o.NameHash));
            len = (uint)o.ScaleTime.size();
            writer.WritePtr(&len, sizeof(len));
            writer.WritePtr(&o.ScaleTime[0], len * sizeof(o.ScaleTime[0]));
            writer.WritePtr(&o.ScaleValue[0], len * sizeof(o.ScaleValue[0]));
            len = (uint)o.RotationTime.size();
            writer.WritePtr(&len, sizeof(len));
            writer.WritePtr(&o.RotationTime[0], len * sizeof(o.RotationTime[0]));
            writer.WritePtr(&o.RotationValue[0], len * sizeof(o.RotationValue[0]));
            len = (uint)o.TranslationTime.size();
            writer.WritePtr(&len, sizeof(len));
            writer.WritePtr(&o.TranslationTime[0], len * sizeof(o.TranslationTime[0]));
            writer.WritePtr(&o.TranslationValue[0], len * sizeof(o.TranslationValue[0]));
        }
    }

    string AnimFileName {};
    string AnimName {};
    float DurationTicks {};
    float TicksPerSecond {};
    vector<BoneOutput> BoneOutputs {};
    HashVecVec BonesHierarchy {};
};

ModelBaker::ModelBaker(FileCollection& all_files) : allFiles {all_files}
{
    fbxManager = FbxManager::Create();
    if (!fbxManager)
        throw GenericException("Unable to create FBX Manager");

    // Create an IOSettings object. This object holds all import/export settings.
    FbxIOSettings* ios = FbxIOSettings::Create(fbxManager, IOSROOT);
    fbxManager->SetIOSettings(ios);

    // Load plugins from the executable directory (optional)
    fbxManager->LoadPluginsDirectory(FbxGetApplicationDirectory().Buffer());
}

ModelBaker::~ModelBaker()
{
    fbxManager->Destroy();
}

void ModelBaker::AutoBakeModels()
{
    allFiles.ResetCounter();
    while (allFiles.MoveNext())
    {
        FileHeader file_header = allFiles.GetCurFileHeader();
        string relative_path = file_header.GetPath().substr(allFiles.GetPath().length());
        if (bakedFiles.count(relative_path))
            continue;

        string ext = _str(relative_path).getFileExtension();
        if (!Is3dExtensionSupported(ext))
            continue;

        File file = allFiles.GetCurFile();
        UCharVec data = BakeFile(relative_path, file);
        bakedFiles.emplace(relative_path, std::move(data));
    }
}

void ModelBaker::FillBakedFiles(map<string, UCharVec>& baked_files)
{
    for (const auto& kv : bakedFiles)
        baked_files.emplace(kv.first, kv.second);
}

class FbxStreamImpl : public FbxStream
{
public:
    FbxStreamImpl() : FbxStream()
    {
        file = nullptr;
        curState = FbxStream::eClosed;
    }

    virtual bool Open(void* stream) override
    {
        file = (File*)stream;
        file->SetCurPos(0);
        curState = FbxStream::eOpen;
        return true;
    }

    virtual bool Close() override
    {
        file->SetCurPos(0);
        file = nullptr;
        curState = FbxStream::eClosed;
        return true;
    }

    virtual char* ReadString(char* buffer, int max_size, bool stop_at_first_white_space = false) override
    {
        const char* str = (char*)file->GetCurBuf();
        int len = 0;
        while (*str && len < max_size - 1)
        {
            str++;
            len++;
            if (*str == '\n' || (stop_at_first_white_space && *str == ' '))
                break;
        }
        if (len)
            file->CopyMem(buffer, len);
        buffer[len] = 0;
        return buffer;
    }

    virtual void Seek(const FbxInt64& offset, const FbxFile::ESeekPos& seek_pos) override
    {
        if (seek_pos == FbxFile::eBegin)
            file->SetCurPos((uint)offset);
        else if (seek_pos == FbxFile::eCurrent)
            file->GoForward((uint)offset);
        else if (seek_pos == FbxFile::eEnd)
            file->SetCurPos(file->GetFsize() - (uint)offset);
    }

    virtual int Read(void* data, int size) const override
    {
        file->CopyMem(data, size);
        return size;
    }

    virtual EState GetState() override { return curState; }
    virtual bool Flush() override { return true; }
    virtual int Write(const void* data, int size) override { return 0; }
    virtual int GetReaderID() const override { return 0; }
    virtual int GetWriterID() const override { return -1; }
    virtual long GetPosition() const override { return file->GetCurPos(); }
    virtual void SetPosition(long position) override { file->SetCurPos((uint)position); }
    virtual int GetError() const override { return 0; }
    virtual void ClearError() override {}

private:
    File* file;
    EState curState;
};

static Bone* ConvertFbxPass1(FbxNode* fbx_node, vector<FbxNode*>& fbx_all_nodes);
static void ConvertFbxPass2(Bone* root_bone, Bone* bone, FbxNode* fbx_node);
static Matrix ConvertFbxMatrix(const FbxAMatrix& m);

UCharVec ModelBaker::BakeFile(const string& fname, File& file)
{
    // Result bone
    Bone* root_bone = nullptr;
    vector<AnimSet*> loaded_animations;

    // Create an FBX scene
    FbxScene* fbx_scene = FbxScene::Create(fbxManager, "Root Scene");
    if (!fbx_scene)
        throw GenericException("Unable to create FBX scene");

    // Create an importer
    FbxImporter* fbx_importer = FbxImporter::Create(fbxManager, "");
    if (!fbx_importer)
        throw GenericException("Unable to create FBX importer");

    // Initialize the importer
    FbxStreamImpl fbx_stream;
    if (!fbx_importer->Initialize(&fbx_stream, &file, -1, fbxManager->GetIOSettings()))
    {
        string error_desc = fbx_importer->GetStatus().GetErrorString();
        if (fbx_importer->GetStatus().GetCode() == FbxStatus::eInvalidFileVersion)
        {
            int sdk_major, sdk_minor, sdk_revision;
            FbxManager::GetFileFormatVersion(sdk_major, sdk_minor, sdk_revision);
            int file_major, file_minor, file_revision;
            fbx_importer->GetFileVersion(file_major, file_minor, file_revision);
            error_desc += _str(" (minimum version {}.{}.{}, file version {}.{}.{})", sdk_major, sdk_minor, sdk_revision,
                file_major, file_minor, file_revision);
        }

        throw GenericException("Call to FbxImporter::Initialize() failed", fname, error_desc);
    }

    // Import the scene
    if (!fbx_importer->Import(fbx_scene))
        throw GenericException("Can't import scene", fname);

    // Load hierarchy
    vector<FbxNode*> fbx_all_nodes;
    root_bone = ConvertFbxPass1(fbx_scene->GetRootNode(), fbx_all_nodes);
    ConvertFbxPass2(root_bone, root_bone, fbx_scene->GetRootNode());

    // Extract animations
    if (fbx_scene->GetCurrentAnimationStack())
    {
        FbxAnimEvaluator* fbx_anim_evaluator = fbx_scene->GetAnimationEvaluator();
        FbxCriteria fbx_anim_stack_criteria =
            FbxCriteria::ObjectType(fbx_scene->GetCurrentAnimationStack()->GetClassId());
        for (int i = 0, j = fbx_scene->GetSrcObjectCount(fbx_anim_stack_criteria); i < j; i++)
        {
            FbxAnimStack* fbx_anim_stack = (FbxAnimStack*)fbx_scene->GetSrcObject(fbx_anim_stack_criteria, i);
            fbx_scene->SetCurrentAnimationStack(fbx_anim_stack);

            FbxTakeInfo* take_info = fbx_importer->GetTakeInfo(i);
            int frames_count = (int)take_info->mLocalTimeSpan.GetDuration().GetFrameCount() + 1;
            float frame_rate =
                (float)(frames_count - 1) / (float)take_info->mLocalTimeSpan.GetDuration().GetSecondDouble();
            int frame_offset = (int)take_info->mLocalTimeSpan.GetStart().GetFrameCount();

            FloatVec st;
            VectorVec sv;
            FloatVec rt;
            QuaternionVec rv;
            FloatVec tt;
            VectorVec tv;
            st.reserve(frames_count);
            sv.reserve(frames_count);
            rt.reserve(frames_count);
            rv.reserve(frames_count);
            tt.reserve(frames_count);
            tv.reserve(frames_count);

            AnimSet* anim_set = new AnimSet();
            for (uint n = 0; n < (uint)fbx_all_nodes.size(); n++)
            {
                st.clear();
                sv.clear();
                rt.clear();
                rv.clear();
                tt.clear();
                tv.clear();

                FbxTime cur_time;
                for (int f = 0; f < frames_count; f++)
                {
                    float time = (float)f;
                    cur_time.SetFrame(frame_offset + f);

                    const FbxAMatrix& fbx_m = fbx_anim_evaluator->GetNodeLocalTransform(fbx_all_nodes[n], cur_time);
                    const FbxVector4& fbx_s = fbx_m.GetS();
                    const FbxQuaternion& fbx_q = fbx_m.GetQ();
                    const FbxVector4& fbx_t = fbx_m.GetT();
                    const Vector& s = Vector((float)fbx_s[0], (float)fbx_s[1], (float)fbx_s[2]);
                    const Quaternion& r =
                        Quaternion((float)fbx_q[3], (float)fbx_q[0], (float)fbx_q[1], (float)fbx_q[2]);
                    const Vector& t = Vector((float)fbx_t[0], (float)fbx_t[1], (float)fbx_t[2]);

                    // Manage duplicates
                    if (f < 2 || sv.back() != s || sv[sv.size() - 2] != s)
                        st.push_back(time), sv.push_back(s);
                    else
                        st.back() = time;
                    if (f < 2 || rv.back() != r || rv[rv.size() - 2] != r)
                        rt.push_back(time), rv.push_back(r);
                    else
                        rt.back() = time;
                    if (f < 2 || tv.back() != t || tv[tv.size() - 2] != t)
                        tt.push_back(time), tv.push_back(t);
                    else
                        tt.back() = time;
                }

                UIntVec hierarchy;
                FbxNode* fbx_node = fbx_all_nodes[n];
                while (fbx_node != nullptr)
                {
                    hierarchy.insert(hierarchy.begin(), Bone::GetHash(fbx_node->GetName()));
                    fbx_node = fbx_node->GetParent();
                }

                anim_set->BoneOutputs.push_back(AnimSet::BoneOutput());
                AnimSet::BoneOutput& o = anim_set->BoneOutputs.back();
                o.NameHash = hierarchy.back();
                o.ScaleTime = st;
                o.ScaleValue = sv;
                o.RotationTime = rt;
                o.RotationValue = rv;
                o.TranslationTime = tt;
                o.TranslationValue = tv;
                anim_set->BonesHierarchy.push_back(hierarchy);
            }

            anim_set->AnimFileName = fname;
            anim_set->AnimName = take_info->mName.Buffer();
            anim_set->DurationTicks = (float)frames_count;
            anim_set->TicksPerSecond = frame_rate;

            loaded_animations.push_back(anim_set);
        }
    }

    // Release importer and scene
    fbx_importer->Destroy(true);
    fbx_scene->Destroy(true);

    UCharVec data;
    DataWriter writer {data};
    root_bone->Save(writer);
    writer.Write((uint)loaded_animations.size());
    for (size_t i = 0; i < loaded_animations.size(); i++)
        loaded_animations[i]->Save(writer);

    delete root_bone;
    for (size_t i = 0; i < loaded_animations.size(); i++)
        delete loaded_animations[i];

    return std::move(data);
}

static void FixTexCoord(float& x, float& y)
{
    if (x < 0.0f)
        x = 1.0f - fmodf(-x, 1.0f);
    else if (x > 1.0f)
        x = fmodf(x, 1.0f);
    if (y < 0.0f)
        y = 1.0f - fmodf(-y, 1.0f);
    else if (y > 1.0f)
        y = fmodf(y, 1.0f);
}

static Bone* ConvertFbxPass1(FbxNode* fbx_node, vector<FbxNode*>& fbx_all_nodes)
{
    fbx_all_nodes.push_back(fbx_node);

    Bone* bone = new Bone();
    bone->NameHash = Bone::GetHash(fbx_node->GetName());
    bone->TransformationMatrix = ConvertFbxMatrix(fbx_node->EvaluateLocalTransform());
    bone->GlobalTransformationMatrix = ConvertFbxMatrix(fbx_node->EvaluateGlobalTransform());
    bone->CombinedTransformationMatrix = Matrix();
    bone->Children.resize(fbx_node->GetChildCount());

    for (int i = 0; i < fbx_node->GetChildCount(); i++)
        bone->Children[i] = unique_ptr<Bone>(ConvertFbxPass1(fbx_node->GetChild(i), fbx_all_nodes));
    return bone;
}

template<class T, class T2>
static T2 FbxGetElement(T* elements, int index, int* vertices)
{
    if (elements->GetMappingMode() == FbxGeometryElement::eByPolygonVertex)
    {
        if (elements->GetReferenceMode() == FbxGeometryElement::eDirect)
            return elements->GetDirectArray().GetAt(index);
        else if (elements->GetReferenceMode() == FbxGeometryElement::eIndexToDirect)
            return elements->GetDirectArray().GetAt(elements->GetIndexArray().GetAt(index));
    }
    else if (elements->GetMappingMode() == FbxGeometryElement::eByControlPoint)
    {
        if (elements->GetReferenceMode() == FbxGeometryElement::eDirect)
            return elements->GetDirectArray().GetAt(vertices[index]);
        else if (elements->GetReferenceMode() == FbxGeometryElement::eIndexToDirect)
            return elements->GetDirectArray().GetAt(elements->GetIndexArray().GetAt(vertices[index]));
    }

    WriteLog(
        "Unknown mapping mode {} or reference mode {}.\n", elements->GetMappingMode(), elements->GetReferenceMode());
    return elements->GetDirectArray().GetAt(0);
}

static void ConvertFbxPass2(Bone* root_bone, Bone* bone, FbxNode* fbx_node)
{
    FbxMesh* fbx_mesh = fbx_node->GetMesh();
    if (fbx_mesh && fbx_node->Show && fbx_mesh->GetPolygonVertexCount() == fbx_mesh->GetPolygonCount() * 3 &&
        fbx_mesh->GetPolygonCount() > 0)
    {
        bone->AttachedMesh = std::make_unique<MeshData>();
        MeshData* mesh = bone->AttachedMesh.get();

        // Generate tangents
        fbx_mesh->GenerateTangentsDataForAllUVSets();

        // Vertices
        int* vertices = fbx_mesh->GetPolygonVertices();
        int vertices_count = fbx_mesh->GetPolygonVertexCount();
        FbxVector4* vertices_data = fbx_mesh->GetControlPoints();
        mesh->Vertices.resize(vertices_count);

        FbxGeometryElementNormal* fbx_normals = fbx_mesh->GetElementNormal();
        FbxGeometryElementTangent* fbx_tangents = fbx_mesh->GetElementTangent();
        FbxGeometryElementBinormal* fbx_binormals = fbx_mesh->GetElementBinormal();
        FbxGeometryElementUV* fbx_uvs = fbx_mesh->GetElementUV();
        for (int i = 0; i < vertices_count; i++)
        {
            Vertex3D& v = mesh->Vertices[i];
            FbxVector4& fbx_v = vertices_data[vertices[i]];

            memzero(&v, sizeof(v));
            v.Position = Vector((float)fbx_v.mData[0], (float)fbx_v.mData[1], (float)fbx_v.mData[2]);

            if (fbx_normals)
            {
                const FbxVector4& fbx_normal =
                    FbxGetElement<FbxGeometryElementNormal, FbxVector4>(fbx_normals, i, vertices);
                v.Normal = Vector((float)fbx_normal[0], (float)fbx_normal[1], (float)fbx_normal[2]);
            }
            if (fbx_tangents)
            {
                const FbxVector4& fbx_tangent =
                    FbxGetElement<FbxGeometryElementTangent, FbxVector4>(fbx_tangents, i, vertices);
                v.Tangent = Vector((float)fbx_tangent[0], (float)fbx_tangent[1], (float)fbx_tangent[2]);
            }
            if (fbx_binormals)
            {
                const FbxVector4& fbx_binormal =
                    FbxGetElement<FbxGeometryElementBinormal, FbxVector4>(fbx_binormals, i, vertices);
                v.Bitangent = Vector((float)fbx_binormal[0], (float)fbx_binormal[1], (float)fbx_binormal[2]);
            }
            if (fbx_uvs)
            {
                const FbxVector2& fbx_uv = FbxGetElement<FbxGeometryElementUV, FbxVector2>(fbx_uvs, i, vertices);
                v.TexCoord[0] = (float)fbx_uv[0];
                v.TexCoord[1] = 1.0f - (float)fbx_uv[1];
                FixTexCoord(v.TexCoord[0], v.TexCoord[1]);
                v.TexCoordBase[0] = v.TexCoord[0];
                v.TexCoordBase[1] = v.TexCoord[1];
            }
#undef FBX_GET_ELEMENT

            v.BlendIndices[0] = -1.0f;
            v.BlendIndices[1] = -1.0f;
            v.BlendIndices[2] = -1.0f;
            v.BlendIndices[3] = -1.0f;
        }

        // Faces
        mesh->Indices.resize(vertices_count);
        for (int i = 0; i < vertices_count; i++)
            mesh->Indices[i] = i;

        // Material
        FbxSurfaceMaterial* fbx_material = fbx_node->GetMaterial(0);
        FbxProperty prop_diffuse = (fbx_material ? fbx_material->FindProperty("DiffuseColor") : FbxProperty());
        if (prop_diffuse.IsValid() && prop_diffuse.GetSrcObjectCount() > 0)
        {
            for (int i = 0, j = prop_diffuse.GetSrcObjectCount(); i < j; i++)
            {
                if (Str::Compare(prop_diffuse.GetSrcObject(i)->GetClassId().GetName(), "FbxFileTexture"))
                {
                    FbxFileTexture* fbx_file_texture = (FbxFileTexture*)prop_diffuse.GetSrcObject(i);
                    mesh->DiffuseTexture = _str(fbx_file_texture->GetFileName()).extractFileName();
                    break;
                }
            }
        }

        // Skinning
        FbxSkin* fbx_skin = (FbxSkin*)fbx_mesh->GetDeformer(0, FbxDeformer::eSkin);
        if (fbx_skin)
        {
            // 3DS Max specific - Geometric transform
            Matrix ms, mr, mt;
            FbxVector4 gt = fbx_node->GetGeometricTranslation(FbxNode::eSourcePivot);
            FbxVector4 gr = fbx_node->GetGeometricRotation(FbxNode::eSourcePivot);
            FbxVector4 gs = fbx_node->GetGeometricScaling(FbxNode::eSourcePivot);
            Matrix::Translation(Vector((float)gt[0], (float)gt[1], (float)gt[2]), mt);
            mr.FromEulerAnglesXYZ(Vector((float)gr[0], (float)gr[1], (float)gr[2]));
            Matrix::Scaling(Vector((float)gs[0], (float)gs[1], (float)gs[2]), ms);

            // Process skin bones
            int num_bones = fbx_skin->GetClusterCount();
            mesh->SkinBones.resize(num_bones);
            mesh->SkinBoneOffsets.resize(num_bones);
            RUNTIME_ASSERT(num_bones <= MODEL_MAX_BONES);
            for (int i = 0; i < num_bones; i++)
            {
                FbxCluster* fbx_cluster = fbx_skin->GetCluster(i);

                // Matrices
                FbxAMatrix link_matrix;
                fbx_cluster->GetTransformLinkMatrix(link_matrix);
                FbxAMatrix cur_matrix;
                fbx_cluster->GetTransformMatrix(cur_matrix);
                Bone* skin_bone = root_bone->Find(Bone::GetHash(fbx_cluster->GetLink()->GetName()));
                if (!skin_bone)
                {
                    WriteLog("Skin bone '{}' for mesh '{}' not found.\n", fbx_cluster->GetLink()->GetName(),
                        fbx_node->GetName());
                    skin_bone = bone;
                }
                mesh->SkinBones[i] = skin_bone->NameHash;
                mesh->SkinBoneOffsets[i] =
                    ConvertFbxMatrix(link_matrix).Inverse() * ConvertFbxMatrix(cur_matrix) * mt * mr * ms;

                // Blend data
                float bone_index = (float)i;
                int num_weights = fbx_cluster->GetControlPointIndicesCount();
                int* indices = fbx_cluster->GetControlPointIndices();
                double* weights = fbx_cluster->GetControlPointWeights();
                int vertices_count = fbx_mesh->GetPolygonVertexCount();
                int* vertices = fbx_mesh->GetPolygonVertices();
                for (int j = 0; j < num_weights; j++)
                {
                    for (int k = 0; k < vertices_count; k++)
                    {
                        if (vertices[k] != indices[j])
                            continue;

                        Vertex3D& v = mesh->Vertices[k];
                        uint index;
                        if (v.BlendIndices[0] < 0.0f)
                            index = 0;
                        else if (v.BlendIndices[1] < 0.0f)
                            index = 1;
                        else if (v.BlendIndices[2] < 0.0f)
                            index = 2;
                        else
                            index = 3;
                        v.BlendIndices[index] = bone_index;
                        v.BlendWeights[index] = (float)weights[j];
                    }
                }
            }
        }
        else
        {
            // 3DS Max specific - Geometric transform
            Matrix ms, mr, mt;
            FbxVector4 gt = fbx_node->GetGeometricTranslation(FbxNode::eSourcePivot);
            FbxVector4 gr = fbx_node->GetGeometricRotation(FbxNode::eSourcePivot);
            FbxVector4 gs = fbx_node->GetGeometricScaling(FbxNode::eSourcePivot);
            Matrix::Translation(Vector((float)gt[0], (float)gt[1], (float)gt[2]), mt);
            mr.FromEulerAnglesXYZ(Vector((float)gr[0], (float)gr[1], (float)gr[2]));
            Matrix::Scaling(Vector((float)gs[0], (float)gs[1], (float)gs[2]), ms);

            mesh->SkinBones.resize(1);
            mesh->SkinBoneOffsets.resize(1);
            mesh->SkinBones[0] = 0;
            mesh->SkinBoneOffsets[0] = mt * mr * ms;
            for (size_t i = 0, j = mesh->Vertices.size(); i < j; i++)
            {
                Vertex3D& v = mesh->Vertices[i];
                v.BlendIndices[0] = 0.0f;
                v.BlendWeights[0] = 1.0f;
            }
        }

        // Drop not filled indices
        for (size_t i = 0, j = mesh->Vertices.size(); i < j; i++)
        {
            Vertex3D& v = mesh->Vertices[i];
            float w = 0.0f;
            int last_bone = 0;
            for (int b = 0; b < BONES_PER_VERTEX; b++)
            {
                if (v.BlendIndices[b] < 0.0f)
                    v.BlendIndices[b] = v.BlendWeights[b] = 0.0f;
                else
                    last_bone = b;
                w += v.BlendWeights[b];
            }
            v.BlendWeights[last_bone] += 1.0f - w;
        }
    }

    for (int i = 0; i < fbx_node->GetChildCount(); i++)
        ConvertFbxPass2(root_bone, bone->Children[i].get(), fbx_node->GetChild(i));
}

static Matrix ConvertFbxMatrix(const FbxAMatrix& m)
{
    return Matrix((float)m.Get(0, 0), (float)m.Get(1, 0), (float)m.Get(2, 0), (float)m.Get(3, 0), (float)m.Get(0, 1),
        (float)m.Get(1, 1), (float)m.Get(2, 1), (float)m.Get(3, 1), (float)m.Get(0, 2), (float)m.Get(1, 2),
        (float)m.Get(2, 2), (float)m.Get(3, 2), (float)m.Get(0, 3), (float)m.Get(1, 3), (float)m.Get(2, 3),
        (float)m.Get(3, 3));
}
