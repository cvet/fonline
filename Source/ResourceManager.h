#ifndef __RESOURCE_MANAGER__
#define __RESOURCE_MANAGER__

#include "Defines.h"
#include "SpriteManager.h"
#include "FileManager.h"

#define RES_NONE           (0)
#define RES_IFACE          (1)
#define RES_CRITTERS       (2)
#define RES_ITEMS          (3)
#define RES_SCRIPT         (4)
#define RES_SPLASH         (5)
#define RES_GLOBAL_MAP     (6)
#define RES_IFACE_EXT      (7)
#define RES_AVATAR         (8)

class SpriteManager;
struct SpriteInfo;
struct AnyFrames;

struct LoadedAnim
{
	int ResType;
	AnyFrames* Anim;
	LoadedAnim(int res_type, AnyFrames* anim):ResType(res_type),Anim(anim){}
};
typedef map<DWORD,LoadedAnim,less<DWORD>> LoadedAnimMap;
typedef map<DWORD,LoadedAnim,less<DWORD>>::iterator LoadedAnimMapIt;
typedef map<DWORD,LoadedAnim,less<DWORD>>::value_type LoadedAnimMapVal;


class ResourceManager
{
private:
	SpriteManager* sprMngr;
	FileManager fileMngr;

	PtrVec processedDats;
	DwordStrMap namesHash;
	LoadedAnimMap loadedAnims;
	AnimMap critterFrames;
	StrVec splashNames;
	StrMap soundNames;

	void AddNamesHash(StrVec& names);

public:
	void Refresh(SpriteManager* spr_mngr);
	void Finish();
	void FreeResources(int type);

	const char* GetName(DWORD name_hash);

	DWORD GetSprId(DWORD name_hash, int dir);
	SpriteInfo* GetSprInfo(DWORD name_hash, int dir);
	AnyFrames* GetAnim(DWORD name_hash, int dir);

	DWORD GetIfaceSprId(DWORD name_hash);
	DWORD GetInvSprId(DWORD name_hash);
	DWORD GetSkDxSprId(DWORD name_hash);

	SpriteInfo* GetIfaceSprInfo(DWORD name_hash);
	SpriteInfo* GetInvSprInfo(DWORD name_hash);
	SpriteInfo* GetSkDxSprInfo(DWORD name_hash);

	AnyFrames* GetIfaceAnim(DWORD name_hash);
	AnyFrames* GetInvAnim(DWORD name_hash);

	AnyFrames* GetCritAnim(DWORD crtype, BYTE anim1, BYTE anim2, BYTE dir);

	DWORD GetAvatarSprId(const char* fname);
	DWORD GetRandomSplash();

	StrMap& GetSoundNames(){return soundNames;}
};

extern ResourceManager ResMngr;


#define SKILLDEX_PARAM(index)          (index)
#define SKILLDEX_PERKS                 (1000)
#define SKILLDEX_KILLS                 (1001)
#define SKILLDEX_KARMA                 (1002)
#define SKILLDEX_TRAITS                (1003)
#define SKILLDEX_REPUTATION            (1004)
#define SKILLDEX_SKILLS                (1005)
#define SKILLDEX_NEXT_LEVEL            (1006)
#define SKILLDEX_DRUG_ADDICT           (1007)
#define SKILLDEX_ALCOHOL_ADDICT        (1008)
#define SKILLDEX_REPUTATION_RATIO(val) (2100+((val)>=GameOpt.ReputationLoved?0:((val)>=GameOpt.ReputationLiked?1:((val)>=GameOpt.ReputationAccepted?2:((val)>=GameOpt.ReputationNeutral?3:((val)>=GameOpt.ReputationAntipathy?4:((val)>=GameOpt.ReputationHated?5:6)))))))

#endif // __RESOURCE_MANAGER__