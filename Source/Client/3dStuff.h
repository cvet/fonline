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

#pragma once

#include "Common.h"

#include "3dAnimation.h"
#include "Application.h"
#include "EffectManager.h"
#include "FileSystem.h"
#include "Settings.h"
#include "Timer.h"

constexpr uint ANIMATION_STAY = 0x01;
constexpr uint ANIMATION_ONE_TIME = 0x02;
constexpr auto ANIMATION_PERIOD(uint proc) -> uint
{
    return 0x04 | proc << 16;
}
constexpr uint ANIMATION_NO_SMOOTH = 0x08;
constexpr uint ANIMATION_INIT = 0x10;
constexpr uint ANIMATION_COMBAT = 0x20;

struct ModelBone;
class ModelInstance;
class ModelInformation;
class ModelHierarchy;

struct MeshTexture
{
    string Name {};
    string ModelPath {};
    RenderTexture* MainTex {};
    float AtlasOffsetData[4] {};
};

struct MeshData
{
    void Load(DataReader& reader, NameResolver& name_resolver);

    ModelBone* Owner {};
    vector<Vertex3D> Vertices {};
    vector<ushort> Indices {};
    string DiffuseTexture {};
    vector<hstring> SkinBoneNames {};
    vector<mat44> SkinBoneOffsets {};
    vector<ModelBone*> SkinBones {};
    string EffectName {};
};

struct MeshInstance
{
    MeshData* Mesh {};
    bool Disabled {};
    MeshTexture* CurTexures[EFFECT_TEXTURES] {};
    MeshTexture* DefaultTexures[EFFECT_TEXTURES] {};
    MeshTexture* LastTexures[EFFECT_TEXTURES] {};
    RenderEffect* CurEffect {};
    RenderEffect* DefaultEffect {};
    RenderEffect* LastEffect {};
};
static_assert(std::is_standard_layout_v<MeshInstance>);

struct ModelBone
{
    void Load(DataReader& reader, NameResolver& name_resolver);
    void FixAfterLoad(ModelBone* root_bone);
    auto Find(hstring bone_name) -> ModelBone*;

    hstring Name {};
    mat44 TransformationMatrix {};
    mat44 GlobalTransformationMatrix {};
    unique_ptr<MeshData> AttachedMesh {};
    vector<unique_ptr<ModelBone>> Children {};
    mat44 CombinedTransformationMatrix {};
};

struct ModelCutData
{
    struct Shape
    {
        mat44 GlobalTransformationMatrix {};
        float SphereRadius {};
    };

    vector<Shape> Shapes {};
    vector<int> Layers {};
    hstring UnskinBone {};
    Shape UnskinShape {};
    bool RevertUnskinShape {};
};

struct ModelAnimationData
{
    uint Id {};
    int Layer {};
    int LayerValue {};
    hstring LinkBone {};
    string ChildName {};
    float RotX {};
    float RotY {};
    float RotZ {};
    float MoveX {};
    float MoveY {};
    float MoveZ {};
    float ScaleX {};
    float ScaleY {};
    float ScaleZ {};
    float SpeedAjust {};
    vector<int> DisabledLayer {};
    vector<hstring> DisabledMesh {};
    vector<tuple<string, hstring, int>> TextureInfo {}; // Name, mesh, num
    vector<tuple<string, hstring>> EffectInfo {}; // Name, mesh
    vector<ModelCutData*> CutInfo {};
};

struct ModelAnimationCallback
{
    uint Anim1 {};
    uint Anim2 {};
    float NormalizedTime {};
    std::function<void()> Callback {};
};

class ModelManager final
{
    friend class ModelInstance;
    friend class ModelInformation;
    friend class ModelHierarchy;

public:
    using MeshTextureCreator = std::function<void(MeshTexture*)>;

    ModelManager() = delete;
    ModelManager(RenderSettings& settings, FileSystem& file_sys, EffectManager& effect_mngr, GameTimer& game_time, NameResolver& name_resolver, AnimationResolver& anim_name_resolver, MeshTextureCreator mesh_tex_creator);
    ModelManager(const ModelManager&) = delete;
    ModelManager(ModelManager&&) noexcept = delete;
    auto operator=(const ModelManager&) = delete;
    auto operator=(ModelManager&&) noexcept = delete;
    ~ModelManager() = default;

    [[nodiscard]] auto GetBoneHashedString(string_view name) const -> hstring;
    [[nodiscard]] auto Convert2dTo3d(int x, int y) const -> vec3;
    [[nodiscard]] auto Convert3dTo2d(vec3 pos) const -> IPoint;

    [[nodiscard]] auto GetModel(string_view name, bool is_child) -> ModelInstance*;
    [[nodiscard]] auto LoadAnimation(string_view anim_fname, string_view anim_name) -> ModelAnimation*;
    [[nodiscard]] auto LoadTexture(string_view texture_name, string_view model_path) -> MeshTexture*;

    void DestroyTextures();
    void SetScreenSize(int width, int height);
    void PreloadModel(string_view name);

private:
    [[nodiscard]] auto VecProject(const vec3& v) const -> vec3;
    [[nodiscard]] auto VecUnproject(const vec3& v) const -> vec3;

    [[nodiscard]] auto LoadModel(string_view fname) -> ModelBone*;
    [[nodiscard]] auto GetInformation(string_view name) -> ModelInformation*;
    [[nodiscard]] auto GetHierarchy(string_view xname) -> ModelHierarchy*;

    RenderSettings& _settings;
    FileSystem& _fileSys;
    EffectManager& _effectMngr;
    GameTimer& _gameTime;
    NameResolver& _nameResolver;
    AnimationResolver& _animNameResolver;
    MeshTextureCreator _meshTexCreator {};
    set<hstring> _processedFiles {};
    vector<unique_ptr<ModelBone, std::function<void(ModelBone*)>>> _loadedModels {};
    vector<unique_ptr<ModelAnimation>> _loadedAnimSets {};
    vector<unique_ptr<MeshTexture>> _loadedMeshTextures {};
    vector<unique_ptr<ModelInformation>> _allModelInfos {};
    vector<unique_ptr<ModelHierarchy>> _xFiles {};
    int _modeWidth {};
    int _modeHeight {};
    float _modeWidthF {};
    float _modeHeightF {};
    mat44 _matrixProjRMaj {}; // Row or column major order
    mat44 _matrixEmptyRMaj {};
    mat44 _matrixProjCMaj {};
    mat44 _matrixEmptyCMaj {};
    float _moveTransitionTime {0.25f};
    float _globalSpeedAdjust {1.0f};
    uint _animDelay {};
    color4 _lightColor {};
};

class ModelInstance final
{
    friend class ModelManager;
    friend class ModelInformation;
    friend class ModelHierarchy;

public:
    ModelInstance() = delete;
    explicit ModelInstance(ModelManager& model_mngr);
    ModelInstance(const ModelInstance&) = default;
    ModelInstance(ModelInstance&&) noexcept = delete;
    auto operator=(const ModelInstance&) = delete;
    auto operator=(ModelInstance&&) noexcept = delete;
    ~ModelInstance();

    [[nodiscard]] auto HasAnimation(uint anim1, uint anim2) const -> bool;
    [[nodiscard]] auto GetAnim1() const -> int; // Todo: GetAnim1/GetAnim2 int to uint return type
    [[nodiscard]] auto GetAnim2() const -> int;
    [[nodiscard]] auto EvaluateAnimation() const -> optional<tuple<uint, uint>>;
    [[nodiscard]] auto NeedDraw() const -> bool;
    [[nodiscard]] auto IsAnimationPlaying() const -> bool;
    [[nodiscard]] auto GetRenderFramesData() const -> tuple<float, int, int, int>;
    [[nodiscard]] auto GetDrawSize() const -> tuple<uint, uint>;
    [[nodiscard]] auto GetBonePos(hstring bone_name) const -> optional<tuple<int, int>>;

    void StartMeshGeneration();
    auto SetAnimation(uint anim1, uint anim2, int* layers, uint flags) -> bool;
    void SetDir(uchar dir);
    void SetDirAngle(int dir_angle);
    void SetRotation(float rx, float ry, float rz);
    void SetScale(float sx, float sy, float sz);
    void SetSpeed(float speed);
    void SetTimer(bool use_game_timer);
    void EnableShadow(bool enabled) { _shadowDisabled = !enabled; }
    void Draw(int x, int y, float scale);

    uint SprId {};
    int SprAtlasType {}; // Todo: fix AtlasType referencing in 3dStuff
    vector<ModelAnimationCallback> AnimationCallbacks {};

private:
    struct CombinedMesh
    {
        RenderEffect* DrawEffect {};
        RenderDrawBuffer* DrawMesh {};
        int EncapsulatedMeshCount {};
        vector<MeshData*> Meshes {};
        vector<uint> MeshVertices {};
        vector<uint> MeshIndices {};
        vector<int> MeshAnimLayers {};
        size_t CurBoneMatrix {};
        vector<ModelBone*> SkinBones {};
        vector<mat44> SkinBoneOffsets {};
        MeshTexture* Textures[EFFECT_TEXTURES] {};
    };

    [[nodiscard]] auto CanBatchCombinedMesh(CombinedMesh* combined_mesh, MeshInstance* mesh_instance) const -> bool;
    [[nodiscard]] auto GetSpeed() const -> float;
    [[nodiscard]] auto GetTick() const -> uint;

    void GenerateCombinedMeshes();
    void FillCombinedMeshes(ModelInstance* base, ModelInstance* cur);
    void CombineMesh(MeshInstance* mesh_instance, int anim_layer);
    void ClearCombinedMesh(CombinedMesh* combined_mesh);
    void BatchCombinedMesh(CombinedMesh* combined_mesh, MeshInstance* mesh_instance, int anim_layer);
    void CutCombinedMeshes(ModelInstance* base, ModelInstance* cur);
    void CutCombinedMesh(CombinedMesh* combined_mesh, ModelCutData* cut);
    void ProcessAnimation(float elapsed, int x, int y, float scale);
    void UpdateBoneMatrices(ModelBone* bone, const mat44* parent_matrix);
    void DrawCombinedMeshes();
    void DrawCombinedMesh(CombinedMesh* combined_mesh, bool shadow_disabled);
    void SetAnimData(ModelAnimationData& data, bool clear);

    ModelManager& _modelMngr;
    uint _curAnim1 {};
    uint _curAnim2 {};
    vector<CombinedMesh*> _combinedMeshes {};
    size_t _combinedMeshesSize {};
    bool _disableCulling {};
    vector<MeshInstance*> _allMeshes {};
    vector<bool> _allMeshesDisabled {};
    ModelInformation* _modelInfo {};
    ModelAnimationController* _animController {};
    int _currentLayers[LAYERS3D_COUNT + 1] {}; // +1 for actions
    uint _currentTrack {};
    uint _lastDrawTick {};
    uint _endTick {};
    mat44 _matRot {};
    mat44 _matScale {};
    mat44 _matScaleBase {};
    mat44 _matRotBase {};
    mat44 _matTransBase {};
    float _speedAdjustBase {};
    float _speedAdjustCur {};
    float _speedAdjustLink {};
    bool _shadowDisabled {};
    float _dirAngle {};
    vec3 _groundPos {};
    bool _useGameTimer {};
    float _animPosProc {};
    float _animPosTime {};
    float _animPosPeriod {};
    bool _allowMeshGeneration {};
    vector<ModelCutData*> _allCuts {};
    bool _nonConstHelper {};

    // Derived animations
    vector<ModelInstance*> _children {};
    ModelInstance* _parent {};
    ModelBone* _parentBone {};
    mat44 _parentMatrix {};
    vector<ModelBone*> _linkBones {};
    vector<mat44> _linkMatricles {};
    ModelAnimationData _animLink {};
    bool _childChecker {};
};

class ModelInformation final
{
    friend class ModelManager;
    friend class ModelInstance;
    friend class ModelHierarchy;

public:
    ModelInformation() = delete;
    explicit ModelInformation(ModelManager& model_mngr);
    ModelInformation(const ModelInformation&) = delete;
    ModelInformation(ModelInformation&&) noexcept = default;
    auto operator=(const ModelInformation&) = delete;
    auto operator=(ModelInformation&&) noexcept = delete;
    ~ModelInformation() = default;

private:
    [[nodiscard]] auto GetAnimationIndex(uint& anim1, uint& anim2, float* speed, bool combat_first) const -> int;
    [[nodiscard]] auto GetAnimationIndexEx(uint anim1, uint anim2, float* speed) const -> int;
    [[nodiscard]] auto CreateCutShape(MeshData* mesh) const -> ModelCutData::Shape;

    [[nodiscard]] auto Load(string_view name) -> bool;
    [[nodiscard]] auto CreateInstance() -> ModelInstance*;

    ModelManager& _modelMngr;
    string _fileName {};
    string _pathName {};
    ModelHierarchy* _hierarchy {};
    unique_ptr<ModelAnimationController> _animController {};
    uint _numAnimationSets {};
    map<int, int> _anim1Equals {};
    map<int, int> _anim2Equals {};
    map<int, int> _animIndexes {};
    map<int, float> _animSpeed {};
    map<uint, vector<pair<int, int>>> _animLayerValues {};
    set<hstring> _fastTransitionBones {};
    ModelAnimationData _animDataDefault {};
    vector<ModelAnimationData> _animData {};
    int _renderAnim {};
    int _renderAnimProcFrom {};
    int _renderAnimProcTo {100};
    int _renderAnimDir {};
    bool _shadowDisabled {};
    uint _drawWidth {DEFAULT_3D_DRAW_WIDTH};
    uint _drawHeight {DEFAULT_3D_DRAW_HEIGHT};
};

class ModelHierarchy final
{
    friend class ModelManager;
    friend class ModelInstance;
    friend class ModelInformation;

public:
    ModelHierarchy() = delete;
    explicit ModelHierarchy(ModelManager& model_mngr);
    ModelHierarchy(const ModelHierarchy&) = delete;
    ModelHierarchy(ModelHierarchy&&) noexcept = default;
    auto operator=(const ModelHierarchy&) = delete;
    auto operator=(ModelHierarchy&&) noexcept = delete;
    ~ModelHierarchy() = default;

private:
    void SetupBones();
    void SetupAnimationOutput(ModelAnimationController* anim_controller);
    auto GetTexture(string_view tex_name) -> MeshTexture*;
    auto GetEffect(string_view name) -> RenderEffect*;

    ModelManager& _modelMngr;
    string _fileName {};
    unique_ptr<ModelBone> _rootBone {};
    vector<ModelBone*> _allBones {};
    vector<ModelBone*> _allDrawBones {};
    bool _nonConstHelper {};
};
