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
#include "ClientScripting.h"
#include "EffectManager.h"
#include "FileSystem.h"
#include "GeometryHelper.h"
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

struct Bone;
class Animation3d;
class Animation3dEntity;
class Animation3dXFile;

struct MeshTexture
{
    string Name {};
    string ModelPath {};
    RenderTexture* MainTex {};
    float AtlasOffsetData[4] {};
};

struct MeshData
{
    void Load(DataReader& reader);

    Bone* Owner {};
    Vertex3DVec Vertices {};
    UShortVec Indices {};
    string DiffuseTexture {};
    HashVec SkinBoneNameHashes {};
    MatrixVec SkinBoneOffsets {};
    vector<Bone*> SkinBones {};
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

struct Bone
{
    void Load(DataReader& reader);
    void FixAfterLoad(Bone* root_bone);
    auto Find(hash name_hash) -> Bone*;
    static auto GetHash(const string& name) -> hash;

    hash NameHash {};
    Matrix TransformationMatrix {};
    Matrix GlobalTransformationMatrix {};
    unique_ptr<MeshData> AttachedMesh {};
    vector<unique_ptr<Bone>> Children {};
    Matrix CombinedTransformationMatrix {};
};

struct CutData
{
    struct Shape
    {
        Matrix GlobalTransformationMatrix {};
        float SphereRadius {};
    };

    vector<Shape> Shapes {};
    IntVec Layers {};
    uint UnskinBone {};
    Shape UnskinShape {};
    bool RevertUnskinShape {};
};

struct AnimParams
{
    uint Id {};
    int Layer {};
    int LayerValue {};
    uint LinkBoneHash {};
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
    vector<uint> DisabledMesh {};
    vector<tuple<string, uint, int>> TextureInfo {}; // Name, mesh, num
    vector<tuple<string, uint>> EffectInfo {}; // Name, mesh
    vector<CutData*> CutInfo {};
};

struct AnimationCallback
{
    uint Anim1 {};
    uint Anim2 {};
    float NormalizedTime {};
    std::function<void()> Callback {};
};

class Animation3dManager final
{
    friend class Animation3d;
    friend class Animation3dEntity;
    friend class Animation3dXFile;

public:
    using MeshTextureCreator = std::function<void(MeshTexture*)>;

    Animation3dManager() = delete;
    Animation3dManager(RenderSettings& settings, FileManager& file_mngr, EffectManager& effect_mngr, ClientScriptSystem& script_sys, GameTimer& game_time, MeshTextureCreator mesh_tex_creator);
    Animation3dManager(const Animation3dManager&) = delete;
    Animation3dManager(Animation3dManager&&) noexcept = delete;
    auto operator=(const Animation3dManager&) = delete;
    auto operator=(Animation3dManager&&) noexcept = delete;
    ~Animation3dManager() = default;

    auto LoadAnimation(const string& anim_fname, const string& anim_name) -> AnimSet*;
    auto LoadTexture(const string& texture_name, const string& model_path) -> MeshTexture*;
    void DestroyTextures();
    void SetScreenSize(int width, int height);
    auto GetAnimation(const string& name, bool is_child) -> Animation3d*;
    void PreloadEntity(const string& name);
    auto Convert2dTo3d(int x, int y) -> Vector;
    auto Convert3dTo2d(Vector pos) -> Point;

private:
    auto LoadModel(const string& fname) -> Bone*;
    auto GetEntity(const string& name) -> Animation3dEntity*;
    auto GetXFile(const string& xname) -> Animation3dXFile*;
    void VecProject(const Vector& v, Vector& out);
    void VecUnproject(const Vector& v, Vector& out);
    void ProjectPosition(Vector& v);

    RenderSettings& _settings;
    FileManager& _fileMngr;
    EffectManager& _effectMngr;
    ClientScriptSystem& _scriptSys;
    GameTimer& _gameTime;
    MeshTextureCreator _meshTexCreator {};
    set<hash> _processedFiles {};
    vector<unique_ptr<Bone, std::function<void(Bone*)>>> _loadedModels {};
    vector<unique_ptr<AnimSet>> _loadedAnimSets {};
    vector<unique_ptr<MeshTexture>> _loadedMeshTextures {};
    vector<unique_ptr<Animation3dEntity>> _allEntities {};
    vector<unique_ptr<Animation3dXFile>> _xFiles {};
    int _modeWidth {};
    int _modeHeight {};
    float _modeWidthF {};
    float _modeHeightF {};
    Matrix _matrixProjRMaj {}; // Row or column major order
    Matrix _matrixEmptyRMaj {};
    Matrix _matrixProjCMaj {};
    Matrix _matrixEmptyCMaj {};
    float _moveTransitionTime {0.25f};
    float _globalSpeedAdjust {1.0f};
    uint _animDelay {};
    Color _lightColor {};
};

class Animation3d final
{
    friend class Animation3dManager;
    friend class Animation3dEntity;
    friend class Animation3dXFile;

public:
    Animation3d() = delete;
    explicit Animation3d(Animation3dManager& anim3d_mngr);
    Animation3d(const Animation3d&) = default;
    Animation3d(Animation3d&&) noexcept = delete;
    auto operator=(const Animation3d&) = delete;
    auto operator=(Animation3d&&) noexcept = delete;
    ~Animation3d();

    void StartMeshGeneration();
    auto SetAnimation(uint anim1, uint anim2, int* layers, uint flags) -> bool;
    [[nodiscard]] auto IsAnimation(uint anim1, uint anim2) const -> bool;
    auto EvaluateAnimation(uint& anim1, uint& anim2) const -> bool;
    // Todo: GetAnim1/GetAnim2 int to uint return type
    [[nodiscard]] auto GetAnim1() const -> int;
    [[nodiscard]] auto GetAnim2() const -> int;
    void SetDir(uchar dir);
    void SetDirAngle(int dir_angle);
    void SetRotation(float rx, float ry, float rz);
    void SetScale(float sx, float sy, float sz);
    void SetSpeed(float speed);
    void SetTimer(bool use_game_timer);
    void EnableShadow(bool enabled) { _shadowDisabled = !enabled; }
    [[nodiscard]] auto NeedDraw() const -> bool;
    void Draw(int x, int y);
    [[nodiscard]] auto IsAnimationPlaying() const -> bool;
    void GetRenderFramesData(float& period, int& proc_from, int& proc_to, int& dir) const;
    void GetDrawSize(uint& draw_width, uint& draw_height) const;
    auto GetBonePos(hash name_hash, int& x, int& y) const -> bool;

    uint SprId {};
    int SprAtlasType {}; // Todo: fix AtlasType referencing in 3dStuff
    vector<AnimationCallback> AnimationCallbacks {};

private:
    struct CombinedMesh
    {
        RenderEffect* DrawEffect {};
        RenderMesh* DrawMesh {};
        int EncapsulatedMeshCount {};
        vector<MeshData*> Meshes {};
        UIntVec MeshVertices {};
        UIntVec MeshIndices {};
        IntVec MeshAnimLayers {};
        size_t CurBoneMatrix {};
        vector<Bone*> SkinBones {};
        MatrixVec SkinBoneOffsets {};
        MeshTexture* Textures[EFFECT_TEXTURES] {};
    };
    using CombinedMeshVec = vector<CombinedMesh*>;

    void GenerateCombinedMeshes();
    void FillCombinedMeshes(Animation3d* base, Animation3d* cur);
    void CombineMesh(MeshInstance* mesh_instance, int anim_layer);
    void ClearCombinedMesh(CombinedMesh* combined_mesh);
    [[nodiscard]] auto CanBatchCombinedMesh(CombinedMesh* combined_mesh, MeshInstance* mesh_instance) const -> bool;
    void BatchCombinedMesh(CombinedMesh* combined_mesh, MeshInstance* mesh_instance, int anim_layer);
    void CutCombinedMeshes(Animation3d* base, Animation3d* cur);
    void CutCombinedMesh(CombinedMesh* combined_mesh, CutData* cut);
    void ProcessAnimation(float elapsed, int x, int y, float scale);
    void UpdateBoneMatrices(Bone* bone, const Matrix* parent_matrix);
    void DrawCombinedMeshes();
    void DrawCombinedMesh(CombinedMesh* combined_mesh, bool shadow_disabled);
    [[nodiscard]] auto GetSpeed() const -> float;
    [[nodiscard]] auto GetTick() const -> uint;
    void SetAnimData(AnimParams& data, bool clear);

    Animation3dManager& _anim3dMngr;
    uint _curAnim1 {};
    uint _curAnim2 {};
    CombinedMeshVec _combinedMeshes {};
    size_t _combinedMeshesSize {};
    bool _disableCulling {};
    vector<MeshInstance*> _allMeshes {};
    BoolVec _allMeshesDisabled {};
    Animation3dEntity* _animEntity {};
    AnimController* _animController {};
    int _currentLayers[LAYERS3D_COUNT + 1] {}; // +1 for actions
    uint _currentTrack {};
    uint _lastDrawTick {};
    uint _endTick {};
    Matrix _matRot {};
    Matrix _matScale {};
    Matrix _matScaleBase {};
    Matrix _matRotBase {};
    Matrix _matTransBase {};
    float _speedAdjustBase {};
    float _speedAdjustCur {};
    float _speedAdjustLink {};
    bool _shadowDisabled {};
    float _dirAngle {};
    Vector _groundPos {};
    bool _useGameTimer {};
    float _animPosProc {};
    float _animPosTime {};
    float _animPosPeriod {};
    bool _allowMeshGeneration {};
    vector<CutData*> _allCuts {};

    // Derived animations
    vector<Animation3d*> _childAnimations {};
    Animation3d* _parentAnimation {};
    Bone* _parentBone {};
    Matrix _parentMatrix {};
    vector<Bone*> _linkBones {};
    MatrixVec _linkMatricles {};
    AnimParams _animLink {};
    bool _childChecker {};
};

class Animation3dEntity final
{
    friend class Animation3dManager;
    friend class Animation3d;
    friend class Animation3dXFile;

public:
    Animation3dEntity() = delete;
    explicit Animation3dEntity(Animation3dManager& anim3d_mngr);
    Animation3dEntity(const Animation3dEntity&) = delete;
    Animation3dEntity(Animation3dEntity&&) noexcept = default;
    auto operator=(const Animation3dEntity&) = delete;
    auto operator=(Animation3dEntity&&) noexcept = delete;
    ~Animation3dEntity() = default;

private:
    [[nodiscard]] auto GetAnimationIndex(uint& anim1, uint& anim2, float* speed, bool combat_first) const -> int;
    [[nodiscard]] auto GetAnimationIndexEx(uint anim1, uint anim2, float* speed) const -> int;
    auto Load(const string& name) -> bool;
    auto CloneAnimation() -> Animation3d*;
    auto CreateCutShape(MeshData* mesh) -> CutData::Shape;

    Animation3dManager& _anim3dMngr;
    string _fileName {};
    string _pathName {};
    Animation3dXFile* _xFile {};
    unique_ptr<AnimController> _animController {};
    uint _numAnimationSets {};
    IntMap _anim1Equals {};
    IntMap _anim2Equals {};
    IntMap _animIndexes {};
    IntFloatMap _animSpeed {};
    UIntIntPairVecMap _animLayerValues {};
    HashSet _fastTransitionBones {};
    AnimParams _animDataDefault {};
    vector<AnimParams> _animData {};
    int _renderAnim {};
    int _renderAnimProcFrom {};
    int _renderAnimProcTo {100};
    int _renderAnimDir {};
    bool _shadowDisabled {};
    uint _drawWidth {};
    uint _drawHeight {};
};

class Animation3dXFile final
{
    friend class Animation3dManager;
    friend class Animation3d;
    friend class Animation3dEntity;

public:
    Animation3dXFile() = delete;
    explicit Animation3dXFile(Animation3dManager& anim3d_mngr);
    Animation3dXFile(const Animation3dXFile&) = delete;
    Animation3dXFile(Animation3dXFile&&) noexcept = default;
    auto operator=(const Animation3dXFile&) = delete;
    auto operator=(Animation3dXFile&&) noexcept = delete;
    ~Animation3dXFile() = default;

private:
    void SetupBones();
    void SetupAnimationOutput(AnimController* anim_controller);
    auto GetTexture(const string& tex_name) -> MeshTexture*;
    auto GetEffect(const string& name) -> RenderEffect*;

    Animation3dManager& _anim3dMngr;
    string _fileName {};
    unique_ptr<Bone> _rootBone {};
    vector<Bone*> _allBones {};
    vector<Bone*> _allDrawBones {};
    int _dummy {};
};
