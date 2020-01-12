#pragma once

#include "Common.h"
#include "FileUtils.h"
#include "GraphicApi.h"
#include "Settings.h"
#include "Timer.h"

#include "assimp/scene.h"
#include "assimp/types.h"

#define ATTRIB_OFFSET(s, m) ((void*)(size_t)((int)(size_t)(&reinterpret_cast<s*>(100000)->m) - 100000))
#define EFFECT_TEXTURES (10)
#define BONES_PER_VERTEX (4)
#define MAX_BONES_PER_MODEL (60)
#define ANY_FRAMES_POOL_SIZE (2000)
#define MAX_FRAMES (50)
#define IS_EFFECT_VALUE(pos) ((pos) != -1)
#define SET_EFFECT_VALUE(eff, pos, value) GL(glUniform1f(pos, value))
#define MAX_STORED_PIXEL_PICKS (100)

using Matrix = aiMatrix4x4;
using Vector = aiVector3D;
using Quaternion = aiQuaternion;
using Color = aiColor4D;
using VectorVec = vector<Vector>;
using QuaternionVec = vector<Quaternion>;
using MatrixVec = vector<Matrix>;
using MatrixPtrVec = vector<Matrix*>;

class Animation3d;
struct Bone;
using BoneVec = vector<Bone*>;

extern bool Is3dExtensionSupported(const string& ext);

struct Vertex2D
{
    float X, Y;
    uint Diffuse;
    float TU, TV;
    float TUEgg, TVEgg;
};
static_assert(std::is_standard_layout_v<Vertex2D>);
static_assert(sizeof(Vertex2D) == 28);
using Vertex2DVec = vector<Vertex2D>;

struct Vertex3D
{
    Vector Position;
    Vector Normal;
    float TexCoord[2];
    float TexCoordBase[2];
    Vector Tangent;
    Vector Bitangent;
    float BlendWeights[BONES_PER_VERTEX];
    float BlendIndices[BONES_PER_VERTEX];
};
static_assert(std::is_standard_layout_v<Vertex3D>);
static_assert(sizeof(Vertex3D) == 96);
using Vertex3DVec = vector<Vertex3D>;
using Vertex3DVecVec = vector<Vertex3DVec>;

struct AnyFrames : public NonCopyable
{
    uint GetSprId(uint num_frm) { return Ind[num_frm % CntFrm]; }
    short GetNextX(uint num_frm) { return NextX[num_frm % CntFrm]; }
    short GetNextY(uint num_frm) { return NextY[num_frm % CntFrm]; }
    uint GetCnt() { return CntFrm; }
    uint GetCurSprId() { return CntFrm > 1 ? Ind[((Timer::GameTick() % Ticks) * 100 / Ticks) * CntFrm / 100] : Ind[0]; }
    uint GetCurSprIndex() { return CntFrm > 1 ? ((Timer::GameTick() % Ticks) * 100 / Ticks) * CntFrm / 100 : 0; }
    int DirCount() { return HaveDirs ? DIRS_COUNT : 1; }
    AnyFrames* GetDir(int dir) { return dir == 0 || !HaveDirs ? this : Dirs[dir - 1]; }

    uint Ind[MAX_FRAMES] {}; // Sprite Ids
    short NextX[MAX_FRAMES] {};
    short NextY[MAX_FRAMES] {};
    uint CntFrm {};
    uint Ticks {}; // Time of playing animation
    uint Anim1 {};
    uint Anim2 {};
    hash NameHash {};
    bool HaveDirs {};
    AnyFrames* Dirs[7] {}; // 7 additional for square hexes, 5 for hexagonal
};
static_assert(std::is_standard_layout_v<AnyFrames>);
using AnimMap = map<uint, AnyFrames*>;
using AnimVec = vector<AnyFrames*>;

struct PrepPoint
{
    int PointX {};
    int PointY {};
    uint PointColor {};
    short* PointOffsX {};
    short* PointOffsY {};
};
using PointVec = vector<PrepPoint>;
using PointVecVec = vector<PointVec>;

struct EffectDefault : public NonCopyable
{
    enum EType
    {
        String,
        Float,
        Int
    };

    char* Name {};
    EType Type {};
    uchar* Data {};
    uint Size {};
};

struct EffectInstance : public NonCopyable
{
    string Name {};
    EffectDefault* Defaults {};
    uint DefaultsCount {};
};

struct EffectPass
{
    GLuint Program {};
    bool IsShadow {};

    GLint ZoomFactor {};
    GLint ColorMap {};
    GLint ColorMapSize {};
    GLint ColorMapSamples {};
    GLint EggMap {};
    GLint EggMapSize {};
    GLint SpriteBorder {};
    GLint ProjectionMatrix {};
    GLint GroundPosition {};
    GLint LightColor {};
    GLint WorldMatrices {};

    bool IsNeedProcess {};
    bool IsTime {};
    GLint Time {};
    float TimeCurrent {};
    double TimeLastTick {};
    GLint TimeGame {};
    float TimeGameCurrent {};
    double TimeGameLastTick {};
    bool IsRandom {};
    GLint Random1 {};
    GLint Random2 {};
    GLint Random3 {};
    GLint Random4 {};
    bool IsTextures {};
    GLint Textures[EFFECT_TEXTURES] {};
    GLint TexturesSize[EFFECT_TEXTURES] {};
    GLint TexturesAtlasOffset[EFFECT_TEXTURES] {};
    bool IsScriptValues {};
    GLint ScriptValues[EFFECT_SCRIPT_VALUES] {};
    bool IsAnimPos {};
    GLint AnimPosProc {};
    GLint AnimPosTime {};
    bool IsChangeStates {};
    int BlendFuncParam1 {};
    int BlendFuncParam2 {};
    int BlendEquation {};
};
using EffectPassVec = vector<EffectPass>;

struct Effect : public NonCopyable
{
    uint Id {};
    string Name {};
    string Defines {};
    EffectDefault* Defaults {};
    EffectPassVec Passes {};
};
using EffectVec = vector<Effect*>;

struct Texture : public NonCopyable
{
    Texture() = default;
    ~Texture();
    void UpdateRegion(const Rect& r, const uchar* data);

    string Name {};
    GLuint Id {};
    uint Width {};
    uint Height {};
    float SizeData[4] {}; // Width, Height, TexelWidth, TexelHeight
    float Samples {};
};
using TextureVec = vector<Texture*>;

struct MeshTexture : public NonCopyable
{
    string Name {};
    string ModelPath {};
    GLuint Id {};
    float SizeData[4] {};
    float AtlasOffsetData[4] {};
};
using MeshTextureVec = vector<MeshTexture*>;

struct RenderTarget : public NonCopyable
{
    GLuint FBO {};
    Texture* TargetTexture {};
    GLuint DepthBuffer {};
    Effect* DrawEffect {};
    bool Multisampling {};
    bool ScreenSize {};
    uint Width {};
    uint Height {};
    bool TexLinear {};
    UIntPairVec* LastPixelPicks {};
};
using RenderTargetVec = vector<RenderTarget*>;

struct TextureAtlas : public NonCopyable
{
    class SpaceNode : public NonCopyable
    {
    public:
        SpaceNode(int x, int y, int w, int h);
        ~SpaceNode();
        bool FindPosition(int w, int h, int& x, int& y);

    private:
        bool busy {};
        int posX {};
        int posY {};
        int width {};
        int height {};
        SpaceNode* child1 {};
        SpaceNode* child2 {};
    };

    TextureAtlas() = default;
    ~TextureAtlas();

    int Type {};
    RenderTarget* RT {};
    Texture* TextureOwner {};
    uint Width {};
    uint Height {};

    // Packer 1
    SpaceNode* RootNode {};

    // Packer 2
    uint CurX {};
    uint CurY {};
    uint LineMaxH {};
    uint LineCurH {};
    uint LineW {};
};
using TextureAtlasVec = vector<TextureAtlas*>;

struct SpriteInfo : public NonCopyable
{
    TextureAtlas* Atlas {};
    RectF SprRect {};
    short Width {};
    short Height {};
    short OffsX {};
    short OffsY {};
    Effect* DrawEffect {};
    bool UsedForAnim3d {};
    Animation3d* Anim3d {};
    uchar* Data {};
    int DataAtlasType {};
    bool DataAtlasOneImage {};
};
using SprInfoVec = vector<SpriteInfo*>;

struct DipData
{
    Texture* SourceTexture {};
    Effect* SourceEffect {};
    uint SpritesCount {};
    RectF SpriteBorder {};
};
using DipDataVec = vector<DipData>;

struct MeshData : public NonCopyable
{
    void Save(File& file);
    void Load(File& file);

    Bone* Owner {};
    Vertex3DVec Vertices {};
    UShortVec Indices {};
    string DiffuseTexture {};
    UIntVec SkinBoneNameHashes {};
    MatrixVec SkinBoneOffsets {};
    BoneVec SkinBones {};
    EffectInstance DrawEffect {};
};
using MeshDataVec = vector<MeshData*>;

struct MeshInstance : public NonCopyable
{
    MeshData* Mesh {};
    bool Disabled {};
    MeshTexture* CurTexures[EFFECT_TEXTURES] {};
    MeshTexture* DefaultTexures[EFFECT_TEXTURES] {};
    MeshTexture* LastTexures[EFFECT_TEXTURES] {};
    Effect* CurEffect {};
    Effect* DefaultEffect {};
    Effect* LastEffect {};
};
using MeshInstanceVec = vector<MeshInstance*>;

struct CombinedMesh : public NonCopyable
{
    CombinedMesh(uint max_bones);
    ~CombinedMesh();
    void Clear();
    bool CanEncapsulate(MeshInstance* mesh_instance);
    void Encapsulate(MeshInstance* mesh_instance, int anim_layer);
    void Finalize();

    int EncapsulatedMeshCount {};
    Vertex3DVec Vertices {};
    UShortVec Indices {};
    MeshDataVec Meshes {};
    UIntVec MeshVertices {};
    UIntVec MeshIndices {};
    IntVec MeshAnimLayers {};
    size_t CurBoneMatrix {};
    BoneVec SkinBones {};
    MatrixVec SkinBoneOffsets {};
    MeshTexture* Textures[EFFECT_TEXTURES] {};
    Effect* DrawEffect {};
    GLuint VAO {};
    GLuint VBO {};
    GLuint IBO {};
};
using CombinedMeshVec = vector<CombinedMesh*>;

struct Bone : public NonCopyable
{
    Bone() = default;
    ~Bone();
    Bone* Find(uint name_hash);
    void Save(File& file);
    void Load(File& file);
    void FixAfterLoad(Bone* root_bone);
    static uint GetHash(const string& name);

    uint NameHash {};
    Matrix TransformationMatrix {};
    Matrix GlobalTransformationMatrix {};
    MeshData* Mesh {};
    BoneVec Children {};
    Matrix CombinedTransformationMatrix {};
};

struct CutShape
{
    CutShape() = default;
    CutShape(MeshData* mesh);

    Matrix GlobalTransformationMatrix {};
    float SphereRadius {};
};
using CutShapeVec = vector<CutShape>;

struct CutData : public NonCopyable
{
    CutShapeVec Shapes {};
    IntVec Layers {};
    uint UnskinBone {};
    CutShape UnskinShape {};
    bool RevertUnskinShape {};
};
using CutDataVec = vector<CutData*>;

struct MapSprite : public NonCopyable
{
    void AddRef() const;
    void Release() const;
    static MapSprite* Factory();

    mutable int RefCount {1};
    bool Valid {};
    uint SprId {};
    ushort HexX {};
    ushort HexY {};
    hash ProtoId {};
    int FrameIndex {};
    int OffsX {};
    int OffsY {};
    bool IsFlat {};
    bool NoLight {};
    int DrawOrder {};
    int DrawOrderHyOffset {};
    int Corner {};
    bool DisableEgg {};
    uint Color {};
    uint ContourColor {};
    bool IsTweakOffs {};
    short TweakOffsX {};
    short TweakOffsY {};
    bool IsTweakAlpha {};
    uchar TweakAlpha {};
};
