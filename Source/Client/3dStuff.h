#pragma once

#include "Common.h"
#include "GraphicApi.h"
#include "GraphicStructures.h"

#define ANIMATION_STAY (0x01)
#define ANIMATION_ONE_TIME (0x02)
#define ANIMATION_PERIOD(proc) (0x04 | ((proc) << 16))
#define ANIMATION_NO_SMOOTH (0x08)
#define ANIMATION_INIT (0x10)
#define ANIMATION_COMBAT (0x20)

class AnimController;
class AnimSet;
using AnimSetVec = vector<AnimSet*>;
class Animation3d;
using Animation3dVec = vector<Animation3d*>;
class Animation3dEntity;
using Animation3dEntityVec = vector<Animation3dEntity*>;
class Animation3dXFile;
using Animation3dXFileVec = vector<Animation3dXFile*>;
class EffectManager;

struct AnimParams
{
    uint Id {};
    int Layer {};
    int LayerValue {};
    uint LinkBoneHash {};
    char* ChildFName {};
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

    int* DisabledLayers {};
    uint DisabledLayersCount {};

    uint* DisabledMesh {};
    uint DisabledMeshCount {};

    char** TextureName {};
    uint* TextureMesh {};
    int* TextureNum {};
    uint TextureCount {};

    EffectInstance* EffectInst {};
    uint* EffectMesh {};
    uint EffectCount {};

    CutData** Cut {};
    uint CutCount {};
};
using AnimParamsVec = vector<AnimParams>;

struct AnimationCallback
{
    uint Anim1 {};
    uint Anim2 {};
    float NormalizedTime {};
    std::function<void()> Callback {};
};
using AnimationCallbackVec = vector<AnimationCallback>;

class Animation3dManager : public NonCopyable
{
    friend class Animation3d;
    friend class Animation3dEntity;
    friend class Animation3dXFile;

public:
    using MeshTextureCreator = std::function<void(MeshTexture*)>;

    Animation3dManager(EffectManager& effect_mngr, MeshTextureCreator mesh_tex_creator);
    ~Animation3dManager();

    Bone* LoadModel(const string& fname);
    void DestroyModel(Bone* root_bone);
    AnimSet* LoadAnimation(const string& anim_fname, const string& anim_name);
    MeshTexture* LoadTexture(const string& texture_name, const string& model_path);
    void DestroyTextures();
    void SetScreenSize(int width, int height);
    Animation3d* GetAnimation(const string& name, bool is_child);
    void PreloadEntity(const string& name);
    Vector Convert2dTo3d(int x, int y);
    Point Convert3dTo2d(Vector pos);

private:
    Animation3dEntity* GetEntity(const string& name);
    Animation3dXFile* GetXFile(const string& xname);
    void VecProject(const Vector& v, Vector& out);
    void VecUnproject(const Vector& v, Vector& out);
    void ProjectPosition(Vector& v);

    EffectManager& effectMngr;
    MeshTextureCreator meshTexCreator {};
    StrVec processedFiles {};
    BoneVec loadedModels {};
    StrVec loadedModelNames {};
    AnimSetVec loadedAnimSets {};
    MeshTextureVec loadedMeshTextures {};
    Animation3dVec loadedAnimations {};
    Animation3dEntityVec allEntities {};
    Animation3dXFileVec xFiles {};
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
    MatrixVec worldMatrices {};
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
    AtlasType SprAtlasType {};
    AnimationCallbackVec AnimationCallbacks {};

private:
    void GenerateCombinedMeshes();
    void FillCombinedMeshes(Animation3d* base, Animation3d* cur);
    void CombineMesh(MeshInstance* mesh_instance, int anim_layer);
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
    MeshInstanceVec allMeshes {};
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
    CutDataVec allCuts {};

    // Derived animations
    Animation3dVec childAnimations {};
    Animation3d* parentAnimation {};
    Bone* parentBone {};
    Matrix parentMatrix {};
    BoneVec linkBones {};
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
    ~Animation3dEntity();

private:
    int GetAnimationIndex(uint& anim1, uint& anim2, float* speed, bool combat_first);
    int GetAnimationIndexEx(uint anim1, uint anim2, float* speed);
    bool Load(const string& name);
    Animation3d* CloneAnimation();

    Animation3dManager& anim3dMngr;
    string fileName {};
    string pathName {};
    Animation3dXFile* xFile {};
    AnimController* animController {};
    uint numAnimationSets {};
    IntMap anim1Equals {};
    IntMap anim2Equals {};
    IntMap animIndexes {};
    IntFloatMap animSpeed {};
    UIntIntPairVecMap animLayerValues {};
    HashSet fastTransitionBones {};
    AnimParams animDataDefault {};
    AnimParamsVec animData {};
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
    ~Animation3dXFile();

private:
    void SetupBones();
    void SetupAnimationOutput(AnimController* anim_controller);
    MeshTexture* GetTexture(const string& tex_name);
    Effect* GetEffect(EffectInstance* effect_inst);

    Animation3dManager& anim3dMngr;
    string fileName {};
    Bone* rootBone {};
    BoneVec allBones {};
    BoneVec allDrawBones {};
};
