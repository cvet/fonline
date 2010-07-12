#ifndef __3D_STUFF_H__
#define __3D_STUFF_H__
// Some code used from http://www.toymaker.info/Games/html/load_x_hierarchy.html

#include <Timer.h>
#include <Defines.h>
#include <3dStuff\MeshStructures.h>
#include <FlexRect.h>
#include <common.h>

#define LAYERS3D_COUNT             (30)

#define ANIMATION_STAY             (0x01)
#define ANIMATION_ONE_TIME         (0x02)
#define ANIMATION_LAST_FRAME       (0x04)

class Animation3d;
typedef vector<Animation3d*> Animation3dVec;
typedef vector<Animation3d*>::iterator Animation3dVecIt;
class Animation3dEntity;
typedef vector<Animation3dEntity*> Animation3dEntityVec;
typedef vector<Animation3dEntity*>::iterator Animation3dEntityVecIt;
class Animation3dXFile;
typedef vector<Animation3dXFile*> Animation3dXFileVec;
typedef vector<Animation3dXFile*>::iterator Animation3dXFileVecIt;
typedef vector<D3DXMESHCONTAINER_EXTENDED*> MeshContainerVec;
typedef vector<D3DXMESHCONTAINER_EXTENDED*>::iterator MeshContainerVecIt;
typedef vector<D3DXFRAME_EXTENDED*> FrameVec;
typedef vector<D3DXFRAME_EXTENDED*>::iterator FrameVecIt;
typedef vector<D3DXMATRIX> MatrixVec;
typedef vector<D3DXMATRIX>::iterator MatrixVecIt;
class AnimTexture;
typedef vector<AnimTexture*> AnimTextureVec;
typedef vector<AnimTexture*>::iterator AnimTextureVecIt;

class AnimTexture
{
public:
	char* Name;
	IDirect3DTexture9* Data;

	AnimTexture(char* name, IDirect3DTexture9* data){Name=name;Data=data;}
	~AnimTexture(){SAFEDELA(Name);SAFEREL(Data);}
};

struct AnimLink
{
	DWORD Id;
	int Layer;
	int LayerValue;
	string LinkBone;
	string ChildFName;
	float RotX,RotY,RotZ;
	float MoveX,MoveY,MoveZ;
	IntVec DisabledLayers;
	IntVec DisabledSubsets;
	string RootTextureName;
	string TextureName;
	int TextureSubset;
};
typedef vector<AnimLink> AnimLinkVec;
typedef vector<AnimLink>::iterator AnimLinkVecIt;

struct MeshOptions
{
	D3DXMESHCONTAINER_EXTENDED* MeshPtr;
	DWORD SubsetsCount;
	bool* DisabledSubsets;
	IDirect3DTexture9** TexSubsets;
	IDirect3DTexture9** DefaultTexSubsets;
};
typedef vector<MeshOptions> MeshOptionsVec;
typedef vector<MeshOptions>::iterator MeshOptionsVecIt;

class Animation3d
{
private:
	friend Animation3dEntity;
	friend Animation3dXFile;
	static Animation3dVec generalAnimations;

	Animation3dEntity* animEntity;
	ID3DXAnimationController* animController;
	int currentAnimation[LAYERS3D_COUNT+1]; // +1 for actions
	DWORD numAnimationSets;
	DWORD currentTrack;
	DWORD lastTick;
	DWORD endTick;
	D3DXMATRIX matRot,matScale;
	float speedAdjust;
	bool shadowDisabled;
	float dirAngle;
	DWORD sprId;
	INTPOINT drawXY,bordersXY;
	float drawScale;
	D3DXVECTOR4 groundPos;
	bool bordersDisabled;
	INTRECT baseBorders,fullBorders;
	DWORD calcBordersTick;
	bool noDraw;
	MeshOptionsVec meshOpt;

	// Derived animations
	Animation3dVec childAnimations;
	Animation3d* parentAnimation;
	D3DXFRAME_EXTENDED* parentFrame;
	D3DXMATRIX parentMatrix;
	FrameVec linkFrames;
	MatrixVec linkMatricles;
	//MatrixVec 
	AnimLink animLink;
	bool childChecker;

	bool FrameMove(double elapsed, int x, int y, float scale, bool software_skinning);
	void UpdateFrameMatrices(const D3DXFRAME* frame_base, const D3DXMATRIX* parent_matrix);
	void BuildShadowVolume(D3DXFRAME_EXTENDED* frame);
	bool DrawFrame(LPD3DXFRAME frame, bool with_shadow);
	bool DrawMeshEffect(ID3DXMesh* mesh, DWORD subset, ID3DXEffect* effect, D3DXHANDLE technique);
	bool IsIntersectFrame(LPD3DXFRAME frame, const D3DXVECTOR3& ray_origin, const D3DXVECTOR3& ray_dir);
	bool SetupBordersFrame(LPD3DXFRAME frame, FLTRECT& borders);
	void ProcessBorders();
	double GetSpeed();
	MeshOptions* GetMeshOptions(D3DXMESHCONTAINER_EXTENDED* mesh);
	void SetTexture(const char* tex_name, int subset);

public:
	Animation3d();
	~Animation3d();

	void SetAnimation(int anim1, int anim2, int* layers, int flags);
	bool IsAnimation(int anim1, int anim2);
	int GetAnim1();
	int GetAnim2();
	void SetDir(int dir);
	void SetRotation(float rx, float ry, float rz);
	void SetScale(float sx, float sy, float sz);
	void SetSpeed(float speed);
	void EnableShadow(bool enabled){shadowDisabled=!enabled;}
	bool Draw(int x, int y, float scale, FLTRECT* stencil, DWORD color);
	void SetDrawPos(int x, int y){drawXY.X=x; drawXY.Y=y;}
	bool IsAnimationPlaying();
	bool IsIntersect(int x, int y);
	void SetSprId(DWORD value){sprId=value;}
	DWORD GetSprId(){return sprId;}
	void EnableSetupBorders(bool enabled){bordersDisabled=!enabled;}
	void SetupBorders();
	INTPOINT GetBaseBordersPivot();
	INTPOINT GetFullBordersPivot();
	INTRECT GetBaseBorders();
	INTRECT GetFullBorders();
	INTRECT GetExtraBorders();

	static bool StartUp(LPDIRECT3DDEVICE9 device, bool software_skinning);
	static bool SetScreenSize(int width, int height);
	static void Finish();
	static void BeginScene();
	static void PreRestore();
	static Animation3d* GetAnimation(const char* name, int path_type, bool is_child);
	static void AnimateFaster();
	static void AnimateSlower();
	static FLTPOINT Convert2dTo3d(int x, int y);
	static INTPOINT Convert3dTo2d(float x, float y);
};

class Animation3dEntity
{
private:
	friend Animation3d;
	friend Animation3dXFile;
	static Animation3dEntityVec allEntities;

	string fileName;
	int pathType;
	Animation3dXFile* xFile;
	DWORD numAnimationSets;
	IntMap animIndexes;
	AnimLinkVec animBones;
	float scaleValue;
	float speedAdjust;

	struct EntityTexture
	{
		char* Name;
		IDirect3DTexture9* Texture;
	};

	void ProcessTemplateDefines(char* str, StrVec& def);
	int GetAnimationIndex(const char* anim_name);
	int GetAnimationIndex(int anim1, int anim2);

	bool Load(const char* name, int path_type);
	Animation3d* CloneAnimation();

public:
	Animation3dEntity();
	~Animation3dEntity();
	static Animation3dEntity* GetEntity(const char* name, int path_type);
};

class Animation3dXFile
{
private:
	friend Animation3d;
	friend Animation3dEntity;
	static Animation3dXFileVec xFiles;

	string fileName;
	int pathType;
	LPD3DXFRAME frameRoot;
	FrameVec framesSkinned;
	MeshContainerVec allMeshes;
	ID3DXAnimationController* animController;
	DWORD facesCount;

	static Animation3dXFile* GetXFile(const char* xname, const char* anim_xname, int path_type);
	bool SetupBoneMatrices(D3DXFRAME_EXTENDED* frame, D3DXFRAME_EXTENDED* frame_root);
	void SetupFacesCount(D3DXFRAME_EXTENDED* frame, DWORD& count);
	static void SetupAnimationOutput(D3DXFRAME* frame, ID3DXAnimationController* anim_controller);

	// Textures managing
	static AnimTextureVec Textures;
	IDirect3DTexture9* GetTexture(const char* tex_name);

public:
	Animation3dXFile();
	~Animation3dXFile();
};

#endif // __3D_STUFF_H__

