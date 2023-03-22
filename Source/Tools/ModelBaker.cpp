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

#include "ModelBaker.h"

#if FO_ENABLE_3D

#include "Application.h"
#include "Log.h"
#include "StringUtils.h"

#if FO_HAVE_FBXSDK
#ifdef __clang__
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#pragma clang diagnostic ignored "-Wnull-dereference"
#pragma clang diagnostic ignored "-Wuninitialized-const-reference"
#pragma clang diagnostic ignored "-Wdeprecated-builtins"
#endif
#include "fbxsdk.h"
#endif

// Linker errors workaround
// ReSharper disable CppInconsistentNaming
#define MeshData BakerMeshData
#define Bone BakerBone
#define AnimSet BakerAnimSet
// ReSharper restore CppInconsistentNaming

struct MeshData
{
    void Save(DataWriter& writer) const
    {
        STACK_TRACE_ENTRY();

        auto len = static_cast<uint>(Vertices.size());
        writer.WritePtr(&len, sizeof(len));
        writer.WritePtr(Vertices.data(), len * sizeof(Vertices[0]));
        len = static_cast<uint>(Indices.size());
        writer.WritePtr(&len, sizeof(len));
        writer.WritePtr(Indices.data(), len * sizeof(Indices[0]));
        len = static_cast<uint>(DiffuseTexture.length());
        writer.WritePtr(&len, sizeof(len));
        writer.WritePtr(DiffuseTexture.data(), len);
        len = static_cast<uint>(SkinBones.size());
        writer.WritePtr(&len, sizeof(len));
        for (const auto& bone_name : SkinBones) {
            len = static_cast<uint>(bone_name.length());
            writer.WritePtr(&len, sizeof(len));
            writer.WritePtr(bone_name.data(), len);
        }
        len = static_cast<uint>(SkinBoneOffsets.size());
        writer.WritePtr(&len, sizeof(len));
        writer.WritePtr(SkinBoneOffsets.data(), len * sizeof(SkinBoneOffsets[0]));
    }

    vector<Vertex3D> Vertices {};
    vector<uint16> Indices {};
    string DiffuseTexture {};
    vector<string> SkinBones {};
    vector<mat44> SkinBoneOffsets {};
    string EffectName {};
};

struct Bone
{
    auto Find(string name) -> Bone*
    {
        STACK_TRACE_ENTRY();

        if (Name == name) {
            return this;
        }

        for (auto&& child : Children) {
            Bone* bone = child->Find(name);
            if (bone != nullptr) {
                return bone;
            }
        }
        return nullptr;
    }

    void Save(DataWriter& writer) const
    {
        STACK_TRACE_ENTRY();

        writer.Write<uint>(static_cast<uint>(Name.length()));
        writer.WritePtr(Name.data(), Name.length());
        writer.WritePtr(&TransformationMatrix, sizeof(TransformationMatrix));
        writer.WritePtr(&GlobalTransformationMatrix, sizeof(GlobalTransformationMatrix));
        writer.Write<uint8>(AttachedMesh != nullptr ? 1 : 0);
        if (AttachedMesh) {
            AttachedMesh->Save(writer);
        }
        writer.Write<uint>(static_cast<uint>(Children.size()));
        for (auto&& child : Children) {
            child->Save(writer);
        }
    }

    string Name {};
    mat44 TransformationMatrix {};
    mat44 GlobalTransformationMatrix {};
    unique_ptr<MeshData> AttachedMesh {};
    vector<unique_ptr<Bone>> Children {};
    mat44 CombinedTransformationMatrix {};
};

struct AnimSet
{
    struct BoneOutput
    {
        string Name {};
        vector<float> ScaleTime {};
        vector<vec3> ScaleValue {};
        vector<float> RotationTime {};
        vector<quaternion> RotationValue {};
        vector<float> TranslationTime {};
        vector<vec3> TranslationValue {};
    };

    void Save(DataWriter& writer) const
    {
        STACK_TRACE_ENTRY();

        auto len = static_cast<uint>(AnimFileName.length());
        writer.WritePtr(&len, sizeof(len));
        writer.WritePtr(AnimFileName.data(), len);
        len = static_cast<uint>(AnimName.length());
        writer.WritePtr(&len, sizeof(len));
        writer.WritePtr(AnimName.data(), len);
        writer.WritePtr(&DurationTicks, sizeof(DurationTicks));
        writer.WritePtr(&TicksPerSecond, sizeof(TicksPerSecond));
        len = static_cast<uint>(BonesHierarchy.size());
        writer.WritePtr(&len, sizeof(len));
        for (const auto& i : BonesHierarchy) {
            len = static_cast<uint>(i.size());
            writer.WritePtr(&len, sizeof(len));
            for (const auto& bone_name : i) {
                len = static_cast<uint>(bone_name.length());
                writer.WritePtr(&len, sizeof(len));
                writer.WritePtr(bone_name.data(), len);
            }
        }
        len = static_cast<uint>(BoneOutputs.size());
        writer.WritePtr(&len, sizeof(len));
        for (const auto& o : BoneOutputs) {
            len = static_cast<uint>(o.Name.length());
            writer.WritePtr(&len, sizeof(len));
            writer.WritePtr(o.Name.data(), len);
            len = static_cast<uint>(o.ScaleTime.size());
            writer.WritePtr(&len, sizeof(len));
            writer.WritePtr(o.ScaleTime.data(), len * sizeof(o.ScaleTime[0]));
            writer.WritePtr(o.ScaleValue.data(), len * sizeof(o.ScaleValue[0]));
            len = static_cast<uint>(o.RotationTime.size());
            writer.WritePtr(&len, sizeof(len));
            writer.WritePtr(o.RotationTime.data(), len * sizeof(o.RotationTime[0]));
            writer.WritePtr(o.RotationValue.data(), len * sizeof(o.RotationValue[0]));
            len = static_cast<uint>(o.TranslationTime.size());
            writer.WritePtr(&len, sizeof(len));
            writer.WritePtr(o.TranslationTime.data(), len * sizeof(o.TranslationTime[0]));
            writer.WritePtr(o.TranslationValue.data(), len * sizeof(o.TranslationValue[0]));
        }
    }

    string AnimFileName {};
    string AnimName {};
    float DurationTicks {};
    float TicksPerSecond {};
    vector<BoneOutput> BoneOutputs {};
    vector<vector<string>> BonesHierarchy {};
};

ModelBaker::ModelBaker(BakerSettings& settings, FileCollection files, BakeCheckerCallback bake_checker, WriteDataCallback write_data) :
    BaseBaker(settings, std::move(files), std::move(bake_checker), std::move(write_data))
{
    STACK_TRACE_ENTRY();

#if FO_HAVE_FBXSDK
    _fbxManager = FbxManager::Create();
    if (_fbxManager == nullptr) {
        throw ModelBakerException("Unable to create FBX Manager");
    }

    // Create an IOSettings object. This object holds all import/export settings.
    FbxIOSettings* ios = FbxIOSettings::Create(_fbxManager, IOSROOT);
    _fbxManager->SetIOSettings(ios);

    // Load plugins from the executable directory (optional)
    _fbxManager->LoadPluginsDirectory(FbxGetApplicationDirectory().Buffer());
#endif
}

ModelBaker::~ModelBaker()
{
    STACK_TRACE_ENTRY();

#if FO_HAVE_FBXSDK
    _fbxManager->Destroy();
#endif
}

void ModelBaker::AutoBake()
{
    STACK_TRACE_ENTRY();

    _errors = 0;

    _files.ResetCounter();
    while (_files.MoveNext()) {
        auto file_header = _files.GetCurFileHeader();

        string ext = _str(file_header.GetPath()).getFileExtension();
        if (!(ext == "fo3d" || ext == "fbx" || ext == "dae" || ext == "obj")) {
            continue;
        }

        if (_bakeChecker && !_bakeChecker(file_header)) {
            continue;
        }

        auto file = _files.GetCurFile();

        try {
            if (ext == "fo3d") {
                _writeData(file_header.GetPath(), file.GetData());
            }
            else {
                auto data = BakeFile(file_header.GetPath(), file);
                _writeData(file_header.GetPath(), data);
            }
        }
        catch (const ModelBakerException& ex) {
            ReportExceptionAndContinue(ex);
            _errors++;
        }
        catch (const FileSystemExeption& ex) {
            ReportExceptionAndContinue(ex);
            _errors++;
        }
    }

    if (_errors > 0) {
        throw ModelBakerException("Errors during effects bakering", _errors);
    }
}

#if FO_HAVE_FBXSDK
class FbxStreamImpl : public FbxStream
{
public:
    FbxStreamImpl()
    {
        STACK_TRACE_ENTRY();

        _file = nullptr;
        _curState = FbxStream::eClosed;
    }

    auto Open(void* stream) -> bool override
    {
        STACK_TRACE_ENTRY();

        _file = static_cast<File*>(stream);
        _file->SetCurPos(0);
        _curState = FbxStream::eOpen;
        return true;
    }

    auto Close() -> bool override
    {
        STACK_TRACE_ENTRY();

        _file->SetCurPos(0);
        _file = nullptr;
        _curState = FbxStream::eClosed;
        return true;
    }

    auto ReadString(char* buffer, int max_size, bool stop_at_first_white_space) -> char* override
    {
        STACK_TRACE_ENTRY();

        const auto* str = reinterpret_cast<const char*>(_file->GetCurBuf());
        auto len = 0;
        while ((*str != 0) && len < max_size - 1) {
            str++;
            len++;
            if (*str == '\n' || (stop_at_first_white_space && *str == ' ')) {
                break;
            }
        }
        if (len != 0) {
            _file->CopyData(buffer, len);
        }
        buffer[len] = 0;
        return buffer;
    }

    void Seek(const FbxInt64& offset, const FbxFile::ESeekPos& seek_pos) override
    {
        STACK_TRACE_ENTRY();

        if (seek_pos == FbxFile::eBegin) {
            _file->SetCurPos(static_cast<uint>(offset));
        }
        else if (seek_pos == FbxFile::eCurrent) {
            _file->GoForward(static_cast<uint>(offset));
        }
        else if (seek_pos == FbxFile::eEnd) {
            _file->SetCurPos(_file->GetSize() - static_cast<uint>(offset));
        }
    }

    auto Read(void* data, int size) const -> int override
    {
        STACK_TRACE_ENTRY();

        _file->CopyData(data, size);
        return size;
    }

    auto GetState() -> EState override
    {
        STACK_TRACE_ENTRY();

        return _curState;
    }

    auto Flush() -> bool override
    {
        STACK_TRACE_ENTRY();

        return true;
    }

    auto Write(const void* /*data*/, int /*size*/) -> int override
    {
        STACK_TRACE_ENTRY();

        return 0;
    }

    [[nodiscard]] auto GetReaderID() const -> int override
    {
        STACK_TRACE_ENTRY();

        return 0;
    }

    [[nodiscard]] auto GetWriterID() const -> int override
    {
        STACK_TRACE_ENTRY();

        return -1;
    }

    [[nodiscard]] auto GetPosition() const -> long override
    {
        STACK_TRACE_ENTRY();

        return static_cast<long>(_file->GetCurPos());
    }

    void SetPosition(long position) override
    {
        STACK_TRACE_ENTRY();

        _file->SetCurPos(static_cast<uint>(position));
    }

    [[nodiscard]] auto GetError() const -> int override
    {
        STACK_TRACE_ENTRY();

        return 0;
    }

    void ClearError() override
    {
        STACK_TRACE_ENTRY();

        //
    }

private:
    File* _file {};
    EState _curState {};
};

static auto ConvertFbxPass1(FbxNode* fbx_node, vector<FbxNode*>& fbx_all_nodes) -> Bone*;
static void ConvertFbxPass2(Bone* root_bone, Bone* bone, FbxNode* fbx_node);
static auto ConvertFbxMatrix(const FbxAMatrix& m) -> mat44;

auto ModelBaker::BakeFile(string_view fname, File& file) -> vector<uint8>
{
    STACK_TRACE_ENTRY();

    // Result bone
    Bone* root_bone = nullptr;
    vector<AnimSet*> loaded_animations;

    // Create an FBX scene
    FbxScene* fbx_scene = FbxScene::Create(_fbxManager, "Root Scene");
    if (fbx_scene == nullptr) {
        throw ModelBakerException("Unable to create FBX scene");
    }

    // Create an importer
    FbxImporter* fbx_importer = FbxImporter::Create(_fbxManager, "");
    if (fbx_importer == nullptr) {
        throw ModelBakerException("Unable to create FBX importer");
    }

    // Initialize the importer
    FbxStreamImpl fbx_stream;
    if (!fbx_importer->Initialize(&fbx_stream, &file, -1, _fbxManager->GetIOSettings())) {
        string error_desc = fbx_importer->GetStatus().GetErrorString();
        if (fbx_importer->GetStatus().GetCode() == FbxStatus::eInvalidFileVersion) {
            int sdk_major;
            int sdk_minor;
            int sdk_revision;
            FbxManager::GetFileFormatVersion(sdk_major, sdk_minor, sdk_revision);
            int file_major;
            int file_minor;
            int file_revision;
            fbx_importer->GetFileVersion(file_major, file_minor, file_revision);
            error_desc += _str(" (minimum version {}.{}.{}, file version {}.{}.{})", sdk_major, sdk_minor, sdk_revision, file_major, file_minor, file_revision);
        }

        throw ModelBakerException("Call to FbxImporter::Initialize() failed", fname, error_desc);
    }

    // Import the scene
    if (!fbx_importer->Import(fbx_scene)) {
        throw ModelBakerException("Can't import scene", fname);
    }

    // Load hierarchy
    vector<FbxNode*> fbx_all_nodes;
    root_bone = ConvertFbxPass1(fbx_scene->GetRootNode(), fbx_all_nodes);
    ConvertFbxPass2(root_bone, root_bone, fbx_scene->GetRootNode());

    // Extract animations
    if (fbx_scene->GetCurrentAnimationStack() != nullptr) {
        auto* fbx_anim_evaluator = fbx_scene->GetAnimationEvaluator();
        auto fbx_anim_stack_criteria = FbxCriteria::ObjectType(fbx_scene->GetCurrentAnimationStack()->GetClassId());
        for (auto i = 0, j = fbx_scene->GetSrcObjectCount(fbx_anim_stack_criteria); i < j; i++) {
            auto* fbx_anim_stack = static_cast<FbxAnimStack*>(fbx_scene->GetSrcObject(fbx_anim_stack_criteria, i));
            fbx_scene->SetCurrentAnimationStack(fbx_anim_stack);

            auto* take_info = fbx_importer->GetTakeInfo(i);
            auto frames_count = static_cast<int>(take_info->mLocalTimeSpan.GetDuration().GetFrameCount()) + 1;
            auto frame_rate = static_cast<float>(frames_count - 1) / static_cast<float>(take_info->mLocalTimeSpan.GetDuration().GetSecondDouble());
            auto frame_offset = static_cast<int>(take_info->mLocalTimeSpan.GetStart().GetFrameCount());

            vector<float> st;
            vector<vec3> sv;
            vector<float> rt;
            vector<quaternion> rv;
            vector<float> tt;
            vector<vec3> tv;
            st.reserve(frames_count);
            sv.reserve(frames_count);
            rt.reserve(frames_count);
            rv.reserve(frames_count);
            tt.reserve(frames_count);
            tv.reserve(frames_count);

            auto* anim_set = new AnimSet();
            for (auto& fbx_all_node : fbx_all_nodes) {
                st.clear();
                sv.clear();
                rt.clear();
                rv.clear();
                tt.clear();
                tv.clear();

                FbxTime cur_time;
                for (auto f = 0; f < frames_count; f++) {
                    const auto time = static_cast<float>(f);
                    cur_time.SetFrame(frame_offset + f);

                    const auto& fbx_m = fbx_anim_evaluator->GetNodeLocalTransform(fbx_all_node, cur_time);
                    const auto& fbx_s = fbx_m.GetS();
                    const auto& fbx_q = fbx_m.GetQ();
                    const auto& fbx_t = fbx_m.GetT();
                    const auto& s = vec3(static_cast<float>(fbx_s[0]), static_cast<float>(fbx_s[1]), static_cast<float>(fbx_s[2]));
                    const auto& r = quaternion(static_cast<float>(fbx_q[3]), static_cast<float>(fbx_q[0]), static_cast<float>(fbx_q[1]), static_cast<float>(fbx_q[2]));
                    const auto& t = vec3(static_cast<float>(fbx_t[0]), static_cast<float>(fbx_t[1]), static_cast<float>(fbx_t[2]));

                    // Manage duplicates
                    if (f < 2 || sv.back() != s || sv[sv.size() - 2] != s) {
                        st.push_back(time);
                        sv.push_back(s);
                    }
                    else {
                        st.back() = time;
                    }
                    if (f < 2 || rv.back() != r || rv[rv.size() - 2] != r) {
                        rt.push_back(time);
                        rv.push_back(r);
                    }
                    else {
                        rt.back() = time;
                    }
                    if (f < 2 || tv.back() != t || tv[tv.size() - 2] != t) {
                        tt.push_back(time);
                        tv.push_back(t);
                    }
                    else {
                        tt.back() = time;
                    }
                }

                vector<string> hierarchy;
                auto* fbx_node = fbx_all_node;
                while (fbx_node != nullptr) {
                    hierarchy.insert(hierarchy.begin(), fbx_node->GetName());
                    fbx_node = fbx_node->GetParent();
                }

                anim_set->BoneOutputs.push_back(AnimSet::BoneOutput());
                AnimSet::BoneOutput& o = anim_set->BoneOutputs.back();
                o.Name = hierarchy.back();
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
            anim_set->DurationTicks = static_cast<float>(frames_count);
            anim_set->TicksPerSecond = frame_rate;

            loaded_animations.push_back(anim_set);
        }
    }

    // Release importer and scene
    fbx_importer->Destroy(true);
    fbx_scene->Destroy(true);

    vector<uint8> data;
    auto writer = DataWriter(data);

    root_bone->Save(writer);
    writer.Write<uint>(static_cast<uint>(loaded_animations.size()));
    for (auto& loaded_animation : loaded_animations) {
        loaded_animation->Save(writer);
    }

    delete root_bone;
    for (auto* loaded_animation : loaded_animations) {
        delete loaded_animation;
    }

    return data;
}

static void FixTexCoord(float& x, float& y)
{
    STACK_TRACE_ENTRY();

    if (x < 0.0f) {
        x = 1.0f - std::fmod(-x, 1.0f);
    }
    else if (x > 1.0f) {
        x = std::fmod(x, 1.0f);
    }
    if (y < 0.0f) {
        y = 1.0f - std::fmod(-y, 1.0f);
    }
    else if (y > 1.0f) {
        y = std::fmod(y, 1.0f);
    }
}

static auto ConvertFbxPass1(FbxNode* fbx_node, vector<FbxNode*>& fbx_all_nodes) -> Bone*
{
    STACK_TRACE_ENTRY();

    fbx_all_nodes.push_back(fbx_node);

    Bone* bone = new Bone();
    bone->Name = fbx_node->GetName();
    bone->TransformationMatrix = ConvertFbxMatrix(fbx_node->EvaluateLocalTransform());
    bone->GlobalTransformationMatrix = ConvertFbxMatrix(fbx_node->EvaluateGlobalTransform());
    bone->CombinedTransformationMatrix = mat44();
    bone->Children.resize(fbx_node->GetChildCount());

    for (auto i = 0; i < fbx_node->GetChildCount(); i++) {
        bone->Children[i] = unique_ptr<Bone>(ConvertFbxPass1(fbx_node->GetChild(i), fbx_all_nodes));
    }
    return bone;
}

template<class T, class T2>
static auto FbxGetElement(T* elements, int index, int* vertices) -> T2
{
    STACK_TRACE_ENTRY();

    if (elements->GetMappingMode() == FbxGeometryElement::eByPolygonVertex) {
        if (elements->GetReferenceMode() == FbxGeometryElement::eDirect) {
            return elements->GetDirectArray().GetAt(index);
        }
        if (elements->GetReferenceMode() == FbxGeometryElement::eIndexToDirect) {
            return elements->GetDirectArray().GetAt(elements->GetIndexArray().GetAt(index));
        }
    }
    else if (elements->GetMappingMode() == FbxGeometryElement::eByControlPoint) {
        if (elements->GetReferenceMode() == FbxGeometryElement::eDirect) {
            return elements->GetDirectArray().GetAt(vertices[index]);
        }
        if (elements->GetReferenceMode() == FbxGeometryElement::eIndexToDirect) {
            return elements->GetDirectArray().GetAt(elements->GetIndexArray().GetAt(vertices[index]));
        }
    }

    WriteLog("Unknown mapping mode {} or reference mode {}", static_cast<int>(elements->GetMappingMode()), static_cast<int>(elements->GetReferenceMode()));
    return elements->GetDirectArray().GetAt(0);
}

static void ConvertFbxPass2(Bone* root_bone, Bone* bone, FbxNode* fbx_node)
{
    STACK_TRACE_ENTRY();

    auto* fbx_mesh = fbx_node->GetMesh();
    if ((fbx_mesh != nullptr) && fbx_node->Show && fbx_mesh->GetPolygonVertexCount() == fbx_mesh->GetPolygonCount() * 3 && fbx_mesh->GetPolygonCount() > 0) {
        bone->AttachedMesh = std::make_unique<MeshData>();
        MeshData* mesh = bone->AttachedMesh.get();

        // Generate tangents
        fbx_mesh->GenerateTangentsDataForAllUVSets();

        // Vertices
        auto* vertices = fbx_mesh->GetPolygonVertices();
        auto vertices_count = fbx_mesh->GetPolygonVertexCount();
        auto* vertices_data = fbx_mesh->GetControlPoints();
        mesh->Vertices.resize(vertices_count);

        auto* fbx_normals = fbx_mesh->GetElementNormal();
        auto* fbx_tangents = fbx_mesh->GetElementTangent();
        auto* fbx_binormals = fbx_mesh->GetElementBinormal();
        auto* fbx_uvs = fbx_mesh->GetElementUV();
        for (auto i = 0; i < vertices_count; i++) {
            auto& v = mesh->Vertices[i];
            auto& fbx_v = vertices_data[vertices[i]];

            std::memset(&v, 0, sizeof(v));
            v.Position = vec3(static_cast<float>(fbx_v.mData[0]), static_cast<float>(fbx_v.mData[1]), static_cast<float>(fbx_v.mData[2]));

            if (fbx_normals != nullptr) {
                const auto& fbx_normal = FbxGetElement<FbxGeometryElementNormal, FbxVector4>(fbx_normals, i, vertices);
                v.Normal = vec3(static_cast<float>(fbx_normal[0]), static_cast<float>(fbx_normal[1]), static_cast<float>(fbx_normal[2]));
            }
            if (fbx_tangents != nullptr) {
                const auto& fbx_tangent = FbxGetElement<FbxGeometryElementTangent, FbxVector4>(fbx_tangents, i, vertices);
                v.Tangent = vec3(static_cast<float>(fbx_tangent[0]), static_cast<float>(fbx_tangent[1]), static_cast<float>(fbx_tangent[2]));
            }
            if (fbx_binormals != nullptr) {
                const auto& fbx_binormal = FbxGetElement<FbxGeometryElementBinormal, FbxVector4>(fbx_binormals, i, vertices);
                v.Bitangent = vec3(static_cast<float>(fbx_binormal[0]), static_cast<float>(fbx_binormal[1]), static_cast<float>(fbx_binormal[2]));
            }
            if (fbx_uvs != nullptr) {
                const auto& fbx_uv = FbxGetElement<FbxGeometryElementUV, FbxVector2>(fbx_uvs, i, vertices);
                v.TexCoord[0] = static_cast<float>(fbx_uv[0]);
                v.TexCoord[1] = 1.0f - static_cast<float>(fbx_uv[1]);
                FixTexCoord(v.TexCoord[0], v.TexCoord[1]);
                v.TexCoordBase[0] = v.TexCoord[0];
                v.TexCoordBase[1] = v.TexCoord[1];
            }

            v.BlendIndices[0] = -1.0f;
            v.BlendIndices[1] = -1.0f;
            v.BlendIndices[2] = -1.0f;
            v.BlendIndices[3] = -1.0f;
        }

        // Faces
        mesh->Indices.resize(vertices_count);
        for (auto i = 0; i < vertices_count; i++) {
            mesh->Indices[i] = static_cast<uint16>(i);
        }

        // Material
        auto* fbx_material = fbx_node->GetMaterial(0);
        auto prop_diffuse = (fbx_material != nullptr ? fbx_material->FindProperty("DiffuseColor") : FbxProperty());
        if (prop_diffuse.IsValid() && prop_diffuse.GetSrcObjectCount() > 0) {
            for (auto i = 0, j = prop_diffuse.GetSrcObjectCount(); i < j; i++) {
                if (string(prop_diffuse.GetSrcObject(i)->GetClassId().GetName()) == "FbxFileTexture") {
                    auto* fbx_file_texture = static_cast<FbxFileTexture*>(prop_diffuse.GetSrcObject(i));
                    mesh->DiffuseTexture = _str(fbx_file_texture->GetFileName()).extractFileName();
                    break;
                }
            }
        }

        // Skinning
        auto* fbx_skin = static_cast<FbxSkin*>(fbx_mesh->GetDeformer(0, FbxDeformer::eSkin));
        if (fbx_skin != nullptr) {
            // 3DS Max specific - Geometric transform
            mat44 ms;
            mat44 mr;
            mat44 mt;
            auto gt = fbx_node->GetGeometricTranslation(FbxNode::eSourcePivot);
            auto gr = fbx_node->GetGeometricRotation(FbxNode::eSourcePivot);
            auto gs = fbx_node->GetGeometricScaling(FbxNode::eSourcePivot);
            mat44::Translation(vec3(static_cast<float>(gt[0]), static_cast<float>(gt[1]), static_cast<float>(gt[2])), mt);
            mr.FromEulerAnglesXYZ(vec3(static_cast<float>(gr[0]), static_cast<float>(gr[1]), static_cast<float>(gr[2])));
            mat44::Scaling(vec3(static_cast<float>(gs[0]), static_cast<float>(gs[1]), static_cast<float>(gs[2])), ms);

            // Process skin bones
            auto num_bones = fbx_skin->GetClusterCount();
            mesh->SkinBones.resize(num_bones);
            mesh->SkinBoneOffsets.resize(num_bones);
            RUNTIME_ASSERT(num_bones <= MODEL_MAX_BONES);
            for (auto i = 0; i < num_bones; i++) {
                auto* fbx_cluster = fbx_skin->GetCluster(i);

                // Matrices
                FbxAMatrix link_matrix;
                fbx_cluster->GetTransformLinkMatrix(link_matrix);
                FbxAMatrix cur_matrix;
                fbx_cluster->GetTransformMatrix(cur_matrix);
                Bone* skin_bone = root_bone->Find(fbx_cluster->GetLink()->GetName());
                if (skin_bone == nullptr) {
                    WriteLog("Skin bone '{}' for mesh '{}' not found", fbx_cluster->GetLink()->GetName(), fbx_node->GetName());
                    skin_bone = bone;
                }
                mesh->SkinBones[i] = skin_bone->Name;
                mesh->SkinBoneOffsets[i] = ConvertFbxMatrix(link_matrix).Inverse() * ConvertFbxMatrix(cur_matrix) * mt * mr * ms;

                // Blend data
                auto bone_index = static_cast<float>(i);
                auto num_weights = fbx_cluster->GetControlPointIndicesCount();
                auto* indices = fbx_cluster->GetControlPointIndices();
                auto* weights = fbx_cluster->GetControlPointWeights();
                auto mesh_vertices_count = fbx_mesh->GetPolygonVertexCount();
                auto* mesh_vertices = fbx_mesh->GetPolygonVertices();
                for (auto j = 0; j < num_weights; j++) {
                    for (auto k = 0; k < mesh_vertices_count; k++) {
                        if (mesh_vertices[k] != indices[j]) {
                            continue;
                        }

                        auto& v = mesh->Vertices[k];
                        uint index = 0;
                        if (v.BlendIndices[0] < 0.0f) {
                            index = 0;
                        }
                        else if (v.BlendIndices[1] < 0.0f) {
                            index = 1;
                        }
                        else if (v.BlendIndices[2] < 0.0f) {
                            index = 2;
                        }
                        else {
                            index = 3;
                        }

                        v.BlendIndices[index] = bone_index;
                        v.BlendWeights[index] = static_cast<float>(weights[j]);
                    }
                }
            }
        }
        else {
            // 3DS Max specific - Geometric transform
            mat44 ms;
            mat44 mr;
            mat44 mt;
            auto gt = fbx_node->GetGeometricTranslation(FbxNode::eSourcePivot);
            auto gr = fbx_node->GetGeometricRotation(FbxNode::eSourcePivot);
            auto gs = fbx_node->GetGeometricScaling(FbxNode::eSourcePivot);
            mat44::Translation(vec3(static_cast<float>(gt[0]), static_cast<float>(gt[1]), static_cast<float>(gt[2])), mt);
            mr.FromEulerAnglesXYZ(vec3(static_cast<float>(gr[0]), static_cast<float>(gr[1]), static_cast<float>(gr[2])));
            mat44::Scaling(vec3(static_cast<float>(gs[0]), static_cast<float>(gs[1]), static_cast<float>(gs[2])), ms);

            mesh->SkinBones.resize(1);
            mesh->SkinBoneOffsets.resize(1);
            mesh->SkinBones[0] = string();
            mesh->SkinBoneOffsets[0] = mt * mr * ms;
            for (auto& v : mesh->Vertices) {
                v.BlendIndices[0] = 0.0f;
                v.BlendWeights[0] = 1.0f;
            }
        }

        // Drop not filled indices
        for (auto& v : mesh->Vertices) {
            auto w = 0.0f;
            size_t last_bone = 0;
            for (size_t b = 0; b < BONES_PER_VERTEX; b++) {
                if (v.BlendIndices[b] < 0.0f) {
                    v.BlendIndices[b] = v.BlendWeights[b] = 0.0f;
                }
                else {
                    last_bone = b;
                }
                w += v.BlendWeights[b];
            }
            v.BlendWeights[last_bone] += 1.0f - w;
        }
    }

    for (auto i = 0; i < fbx_node->GetChildCount(); i++) {
        ConvertFbxPass2(root_bone, bone->Children[i].get(), fbx_node->GetChild(i));
    }
}

static auto ConvertFbxMatrix(const FbxAMatrix& m) -> mat44
{
    STACK_TRACE_ENTRY();

    return mat44(static_cast<float>(m.Get(0, 0)), static_cast<float>(m.Get(1, 0)), static_cast<float>(m.Get(2, 0)), static_cast<float>(m.Get(3, 0)), //
        static_cast<float>(m.Get(0, 1)), static_cast<float>(m.Get(1, 1)), static_cast<float>(m.Get(2, 1)), static_cast<float>(m.Get(3, 1)), //
        static_cast<float>(m.Get(0, 2)), static_cast<float>(m.Get(1, 2)), static_cast<float>(m.Get(2, 2)), static_cast<float>(m.Get(3, 2)), //
        static_cast<float>(m.Get(0, 3)), static_cast<float>(m.Get(1, 3)), static_cast<float>(m.Get(2, 3)), static_cast<float>(m.Get(3, 3)));
}

#else
auto ModelBaker::BakeFile(string_view fname, File& file) -> vector<uint8>
{
    STACK_TRACE_ENTRY();

    throw NotSupportedException("ModelBaker::BakeFile");
}
#endif

#endif
