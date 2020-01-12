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
class Animation3d;
using Animation3dVec = vector<Animation3d*>;
class Animation3dEntity;
using Animation3dEntityVec = vector<Animation3dEntity*>;
class Animation3dXFile;
using Animation3dXFileVec = vector<Animation3dXFile*>;
class EffectManager;
class GraphicLoader;

struct AnimParams
{
    uint Id;
    int Layer;
    int LayerValue;
    uint LinkBoneHash;
    char* ChildFName;
    float RotX, RotY, RotZ;
    float MoveX, MoveY, MoveZ;
    float ScaleX, ScaleY, ScaleZ;
    float SpeedAjust;

    int* DisabledLayers;
    uint DisabledLayersCount;

    uint* DisabledMesh;
    uint DisabledMeshCount;

    char** TextureName;
    uint* TextureMesh;
    int* TextureNum;
    uint TextureCount;

    EffectInstance* EffectInst;
    uint* EffectMesh;
    uint EffectCount;

    CutData** Cut;
    uint CutCount;
};
using AnimParamsVec = vector<AnimParams>;

struct AnimationCallback
{
    uint Anim1;
    uint Anim2;
    float NormalizedTime;
    std::function<void()> Callback;
};
using AnimationCallbackVec = vector<AnimationCallback>;

class Animation3dManager : public NonCopyable
{
    friend class Animation3d;
    friend class Animation3dEntity;
    friend class Animation3dXFile;

public:
    Animation3dManager(EffectManager& effect_mngr, GraphicLoader& graphic_loader);
    ~Animation3dManager();

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
    GraphicLoader& graphicLoader;
    Animation3dVec loadedAnimations;
    Animation3dEntityVec allEntities;
    Animation3dXFileVec xFiles;

    int ModeWidth = 0;
    int ModeHeight = 0;
    float ModeWidthF = 0.0f;
    float ModeHeightF = 0.0f;
    Matrix MatrixProjRM; // Row or Column major order
    Matrix MatrixEmptyRM;
    Matrix MatrixProjCM;
    Matrix MatrixEmptyCM;
    float MoveTransitionTime = 0.25f;
    float GlobalSpeedAdjust = 1.0f;
    bool SoftwareSkinning = false;
    uint AnimDelay = 0;
    Color LightColor;
    MatrixVec WorldMatrices;
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

    uint SprId;
    int SprAtlasType;
    AnimationCallbackVec AnimationCallbacks;

private:
    Animation3dManager& anim3dMngr;
    uint curAnim1;
    uint curAnim2;
    CombinedMeshVec combinedMeshes;
    size_t combinedMeshesSize;
    bool disableCulling;
    MeshInstanceVec allMeshes;
    BoolVec allMeshesDisabled;
    Animation3dEntity* animEntity;
    AnimController* animController;
    int currentLayers[LAYERS3D_COUNT + 1]; // +1 for actions
    uint currentTrack;
    uint lastDrawTick;
    uint endTick;
    Matrix matRot, matScale;
    Matrix matScaleBase, matRotBase, matTransBase;
    float speedAdjustBase, speedAdjustCur, speedAdjustLink;
    bool shadowDisabled;
    float dirAngle;
    Vector groundPos;
    bool useGameTimer;
    float animPosProc, animPosTime, animPosPeriod;
    bool allowMeshGeneration;
    CutDataVec allCuts;

    // Derived animations
    Animation3dVec childAnimations;
    Animation3d* parentAnimation;
    Bone* parentBone;
    Matrix parentMatrix;
    BoneVec linkBones;
    MatrixVec linkMatricles;
    AnimParams animLink;
    bool childChecker;

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
    Animation3dManager& anim3dMngr;
    string fileName;
    string pathName;
    Animation3dXFile* xFile;
    AnimController* animController;
    uint numAnimationSets;
    IntMap anim1Equals, anim2Equals;
    IntMap animIndexes;
    IntFloatMap animSpeed;
    UIntIntPairVecMap animLayerValues;
    HashSet fastTransitionBones;
    AnimParams animDataDefault;
    AnimParamsVec animData;
    int renderAnim;
    int renderAnimProcFrom, renderAnimProcTo;
    int renderAnimDir;
    bool shadowDisabled;
    bool calcualteTangetSpace;
    uint drawWidth, drawHeight;

    int GetAnimationIndex(uint& anim1, uint& anim2, float* speed, bool combat_first);
    int GetAnimationIndexEx(uint anim1, uint anim2, float* speed);

    bool Load(const string& name);
    Animation3d* CloneAnimation();
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
    Animation3dManager& anim3dMngr;
    string fileName;
    Bone* rootBone;
    BoneVec allBones;
    BoneVec allDrawBones;

    void SetupBones();
    void SetupAnimationOutput(AnimController* anim_controller);

    MeshTexture* GetTexture(const string& tex_name);
    Effect* GetEffect(EffectInstance* effect_inst);
};
