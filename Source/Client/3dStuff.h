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

#define ANIMATION_STAY (0x01)
#define ANIMATION_ONE_TIME (0x02)
#define ANIMATION_PERIOD(proc) (0x04 | ((proc) << 16))
#define ANIMATION_NO_SMOOTH (0x08)
#define ANIMATION_INIT (0x10)
#define ANIMATION_COMBAT (0x20)

struct Bone;
class Animation3d;
class Animation3dEntity;
class Animation3dXFile;

struct MeshTexture : public NonCopyable
{
    string Name {};
    string ModelPath {};
    RenderTexture* MainTex {};
    float AtlasOffsetData[4] {};
};

struct MeshData : public NonCopyable
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

struct MeshInstance : public NonCopyable
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

struct Bone : public NonCopyable
{
    void Load(DataReader& reader);
    void FixAfterLoad(Bone* root_bone);
    Bone* Find(hash name_hash);
    static hash GetHash(const string& name);

    hash NameHash {};
    Matrix TransformationMatrix {};
    Matrix GlobalTransformationMatrix {};
    unique_ptr<MeshData> AttachedMesh {};
    vector<unique_ptr<Bone>> Children {};
    Matrix CombinedTransformationMatrix {};
};

struct CutData : public NonCopyable
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

class Animation3dManager : public NonMovable
{
    friend class Animation3d;
    friend class Animation3dEntity;
    friend class Animation3dXFile;

public:
    using MeshTextureCreator = std::function<void(MeshTexture*)>;

    Animation3dManager(RenderSettings& sett, FileManager& file_mngr, EffectManager& effect_mngr,
        ClientScriptSystem& script_sys, GameTimer& game_time, MeshTextureCreator mesh_tex_creator);
    AnimSet* LoadAnimation(const string& anim_fname, const string& anim_name);
    MeshTexture* LoadTexture(const string& texture_name, const string& model_path);
    void DestroyTextures();
    void SetScreenSize(int width, int height);
    Animation3d* GetAnimation(const string& name, bool is_child);
    void PreloadEntity(const string& name);
    Vector Convert2dTo3d(int x, int y);
    Point Convert3dTo2d(Vector pos);

private:
    Bone* LoadModel(const string& fname);
    Animation3dEntity* GetEntity(const string& name);
    Animation3dXFile* GetXFile(const string& xname);
    void VecProject(const Vector& v, Vector& out);
    void VecUnproject(const Vector& v, Vector& out);
    void ProjectPosition(Vector& v);

    RenderSettings& settings;
    FileManager& fileMngr;
    EffectManager& effectMngr;
    ClientScriptSystem& scriptSys;
    GameTimer& gameTime;
    MeshTextureCreator meshTexCreator {};
    set<hash> processedFiles {};
    vector<unique_ptr<Bone, std::function<void(Bone*)>>> loadedModels {};
    vector<unique_ptr<AnimSet>> loadedAnimSets {};
    vector<unique_ptr<MeshTexture>> loadedMeshTextures {};
    vector<unique_ptr<Animation3dEntity>> allEntities {};
    vector<unique_ptr<Animation3dXFile>> xFiles {};
    int modeWidth {};
    int modeHeight {};
    float modeWidthF {};
    float modeHeightF {};
    Matrix matrixProjRM {}; // Row or column major order
    Matrix matrixEmptyRM {};
    Matrix matrixProjCM {};
    Matrix matrixEmptyCM {};
    float moveTransitionTime {0.25f};
    float globalSpeedAdjust {1.0f};
    uint animDelay {};
    Color lightColor {};
};

class Animation3d : public NonCopyable
{
    friend class Animation3dManager;
    friend class Animation3dEntity;
    friend class Animation3dXFile;

public:
    Animation3d(Animation3dManager& anim3d_mngr);
    ~Animation3d();
    void StartMeshGeneration();
    bool SetAnimation(uint anim1, uint anim2, int* layers, int flags);
    bool IsAnimation(uint anim1, uint anim2);
    bool CheckAnimation(uint& anim1, uint& anim2);
    int GetAnim1();
    int GetAnim2();
    void SetDir(int dir);
    void SetDirAngle(int dir_angle);
    void SetRotation(float rx, float ry, float rz);
    void SetScale(float sx, float sy, float sz);
    void SetSpeed(float speed);
    void SetTimer(bool use_game_timer);
    void EnableShadow(bool enabled) { shadowDisabled = !enabled; }
    bool NeedDraw();
    void Draw(int x, int y);
    bool IsAnimationPlaying();
    void GetRenderFramesData(float& period, int& proc_from, int& proc_to, int& dir);
    void GetDrawSize(uint& draw_width, uint& draw_height);
    bool GetBonePos(hash name_hash, int& x, int& y);

    uint SprId {};
    int SprAtlasType {}; // Todo: fix AtlasType referencing in 3dStuff
    vector<AnimationCallback> AnimationCallbacks {};

private:
    struct CombinedMesh : public NonCopyable
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
    bool CanBatchCombinedMesh(CombinedMesh* combined_mesh, MeshInstance* mesh_instance);
    void BatchCombinedMesh(CombinedMesh* combined_mesh, MeshInstance* mesh_instance, int anim_layer);
    void CutCombinedMeshes(Animation3d* base, Animation3d* cur);
    void CutCombinedMesh(CombinedMesh* combined_mesh, CutData* cut);
    void ProcessAnimation(float elapsed, int x, int y, float scale);
    void UpdateBoneMatrices(Bone* bone, const Matrix* parent_matrix);
    void DrawCombinedMeshes();
    void DrawCombinedMesh(CombinedMesh* combined_mesh, bool shadow_disabled);
    float GetSpeed();
    uint GetTick();
    void SetAnimData(AnimParams& data, bool clear);

    Animation3dManager& anim3dMngr;
    uint curAnim1 {};
    uint curAnim2 {};
    CombinedMeshVec combinedMeshes {};
    size_t combinedMeshesSize {};
    bool disableCulling {};
    vector<MeshInstance*> allMeshes {};
    BoolVec allMeshesDisabled {};
    Animation3dEntity* animEntity {};
    AnimController* animController {};
    int currentLayers[LAYERS3D_COUNT + 1] {}; // +1 for actions
    uint currentTrack {};
    uint lastDrawTick {};
    uint endTick {};
    Matrix matRot {};
    Matrix matScale {};
    Matrix matScaleBase {};
    Matrix matRotBase {};
    Matrix matTransBase {};
    float speedAdjustBase {};
    float speedAdjustCur {};
    float speedAdjustLink {};
    bool shadowDisabled {};
    float dirAngle {};
    Vector groundPos {};
    bool useGameTimer {};
    float animPosProc {};
    float animPosTime {};
    float animPosPeriod {};
    bool allowMeshGeneration {};
    vector<CutData*> allCuts {};

    // Derived animations
    vector<Animation3d*> childAnimations {};
    Animation3d* parentAnimation {};
    Bone* parentBone {};
    Matrix parentMatrix {};
    vector<Bone*> linkBones {};
    MatrixVec linkMatricles {};
    AnimParams animLink {};
    bool childChecker {};
};

class Animation3dEntity : public NonCopyable
{
    friend class Animation3dManager;
    friend class Animation3d;
    friend class Animation3dXFile;

public:
    Animation3dEntity(Animation3dManager& anim3d_mngr);

private:
    int GetAnimationIndex(uint& anim1, uint& anim2, float* speed, bool combat_first);
    int GetAnimationIndexEx(uint anim1, uint anim2, float* speed);
    bool Load(const string& name);
    Animation3d* CloneAnimation();
    CutData::Shape CreateCutShape(MeshData* mesh);

    Animation3dManager& anim3dMngr;
    string fileName {};
    string pathName {};
    Animation3dXFile* xFile {};
    unique_ptr<AnimController> animController {};
    uint numAnimationSets {};
    IntMap anim1Equals {};
    IntMap anim2Equals {};
    IntMap animIndexes {};
    IntFloatMap animSpeed {};
    UIntIntPairVecMap animLayerValues {};
    HashSet fastTransitionBones {};
    AnimParams animDataDefault {};
    vector<AnimParams> animData {};
    int renderAnim {};
    int renderAnimProcFrom {};
    int renderAnimProcTo {100};
    int renderAnimDir {};
    bool shadowDisabled {};
    bool calcualteTangetSpace {};
    uint drawWidth {};
    uint drawHeight {};
};

class Animation3dXFile : public NonCopyable
{
    friend class Animation3dManager;
    friend class Animation3d;
    friend class Animation3dEntity;

public:
    Animation3dXFile(Animation3dManager& anim3d_mngr);

private:
    void SetupBones();
    void SetupAnimationOutput(AnimController* anim_controller);
    MeshTexture* GetTexture(const string& tex_name);
    RenderEffect* GetEffect(const string& name);

    Animation3dManager& anim3dMngr;
    string fileName {};
    unique_ptr<Bone> rootBone {};
    vector<Bone*> allBones {};
    vector<Bone*> allDrawBones {};
};
