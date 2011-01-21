#ifndef __CRITTER_CL__
#define __CRITTER_CL__

#include "NetProtocol.h"
#include "Common.h"
#include "SpriteManager.h"
#include "BufferManager.h"
#include "Item.h"
#include "ItemManager.h"
#include "3dStuff\Animation.h"

#define FIRST_FRAME               (0)
#define LAST_FRAME                (255)
#define STAY_WAIT_SHOW_TIME       (50000)
#define ANIM_MAP_ID(t,a1,a2,d)    (((t)<<19)|((a1)<<11)|((a2)<<3)|(d))

class CritterCl
{
public:
	DWORD Id;
	WORD Pid;
	WORD HexX,HexY;
	BYTE CrDir;
	int Params[MAX_PARAMS];
	DWORD NameColor;
	DWORD ContourColor;
	WordVec LastHexX,LastHexY;
	BYTE Cond;
	BYTE CondExt;
	DWORD Flags;
	DWORD BaseType,BaseTypeAlias;
	DWORD ApRegenerationTick;
	short Multihex;

	string Name;
	int NameRefCounter;
	string NameOnHead;
	int NameOnHeadRefCounter;
	string Lexems;
	int LexemsRefCounter;
	string Avatar;
	char Pass[MAX_NAME+1];

	ItemPtrVec InvItems;
	Item* DefItemSlotHand;
	Item* DefItemSlotArmor;
	Item* ItemSlotMain;
	Item* ItemSlotExt;
	Item* ItemSlotArmor;

	static bool ParamsRegEnabled[MAX_PARAMS];
	static int ParamsReg[MAX_PARAMS];
	CritterCl* ThisPtr[MAX_PARAMETERS_ARRAYS];
	static int ParamsChangeScript[MAX_PARAMS];
	static int ParamsGetScript[MAX_PARAMS];
	static DWORD ParametersMin[MAX_PARAMETERS_ARRAYS];
	static DWORD ParametersMax[MAX_PARAMETERS_ARRAYS];
	static bool ParametersOffset[MAX_PARAMETERS_ARRAYS];
	bool ParamsIsChanged[MAX_PARAMS];
	IntVec ParamsChanged;
	int ParamLocked;
	static bool SlotEnabled[0x100];

	CritterCl();
	~CritterCl();
	void Init();
	void InitForRegistration();
	void Finish();
	void GenParams();

	DWORD GetId(){return Id;}
	const char* GetInfo(){return Name.c_str();}
	WORD GetHexX(){return HexX;}
	WORD GetHexY(){return HexY;}
	bool IsLastHexes();
	void FixLastHexes();
	WORD PopLastHexX();
	WORD PopLastHexY();
	void SetBaseType(DWORD type);
	void SetDir(BYTE dir);
	BYTE GetDir(){return CrDir;}
	DWORD GetCrTypeAlias();

	void Animate(DWORD anim1, DWORD anim2, Item* item);
	void AnimateStay();
	void Action(int action, int action_ext, Item* item, bool local_call = true);
	void Process();

	int GetCond(){return Cond;}
	int GetCondExt(){return CondExt;}
	void DrawStay(INTRECT r);
	const char* GetName(){return Name.c_str();}
	const char* GetPass(){return Pass;}

	bool IsNpc(){return FLAG(Flags,FCRIT_NPC);}
	bool IsPlayer(){return FLAG(Flags,FCRIT_PLAYER);}
	bool IsChosen(){return FLAG(Flags,FCRIT_CHOSEN);}
	bool IsGmapRule(){return FLAG(Flags,FCRIT_RULEGROUP);}
	bool IsOnline(){return !FLAG(Flags,FCRIT_DISCONNECT);}
	bool IsOffline(){return FLAG(Flags,FCRIT_DISCONNECT);}
	bool IsLife(){return Cond==COND_LIFE;}
	bool IsLifeNone(){return Cond==COND_LIFE && CondExt==COND_LIFE_NONE;}
	bool IsKnockout(){return Cond==COND_KNOCKOUT;}
	bool IsDead(){return Cond==COND_DEAD;}
	bool CheckFind(int find_type);
	bool IsToTalk(){return IsNpc() && IsLifeNone() && Params[ST_DIALOG_ID];}
	bool IsCombatMode(){return GetParam(TO_BATTLE)!=0;}
	bool IsTurnBased(){return TB_BATTLE_TIMEOUT_CHECK(GetParam(TO_BATTLE));}

	DWORD GetLook();
	DWORD GetTalkDistance();
	DWORD GetAttackDist();
	DWORD GetUseDist();
	DWORD GetMultihex();

	int GetParam(DWORD index);
	bool IsRawParam(DWORD index){return Params[index]!=0;}
	int GetRawParam(DWORD index){return Params[index];}
	void ChangeParam(DWORD index);
	void ProcessChangedParams();

	bool IsTagSkill(DWORD index){return Params[TAG_SKILL1]==index || Params[TAG_SKILL2]==index || Params[TAG_SKILL3]==index || Params[TAG_SKILL4]==index;}
	DWORD GetMaxWeightKg(){return GetParam(ST_CARRY_WEIGHT)/1000;}
	DWORD GetMaxVolume(){return CRITTER_INV_VOLUME;}
	DWORD GetCrType();
	bool IsDmgLeg(){return IsRawParam(DAMAGE_RIGHT_LEG) || IsRawParam(DAMAGE_LEFT_LEG);}
	bool IsDmgTwoLeg(){return IsRawParam(DAMAGE_RIGHT_LEG) && IsRawParam(DAMAGE_LEFT_LEG);}
	bool IsDmgArm(){return IsRawParam(DAMAGE_RIGHT_ARM) || IsRawParam(DAMAGE_LEFT_ARM);}
	bool IsDmgTwoArm(){return IsRawParam(DAMAGE_RIGHT_ARM) && IsRawParam(DAMAGE_LEFT_ARM);}
	int GetRealAp(){return Params[ST_CURRENT_AP];}
	int GetAllAp(){return GetParam(ST_CURRENT_AP)+GetParam(ST_MOVE_AP);}
	void SubMoveAp(int val){ChangeParam(ST_CURRENT_AP);Params[ST_MOVE_AP]-=val;}
	void SubAp(int val){ChangeParam(ST_CURRENT_AP);Params[ST_CURRENT_AP]-=val*AP_DIVIDER;ApRegenerationTick=0;}

	// Items
public:
	void AddItem(Item* item);
	void EraseItem(Item* item, bool animate);
	void EraseAllItems();
	Item* GetItem(DWORD item_id);
	Item* GetItemByPid(WORD item_pid);
	Item* GetAmmo(DWORD caliber);
	Item* GetItemSlot(int slot);
	void GetItemsSlot(int slot, ItemPtrVec& items);
	void GetItemsType(int slot, ItemPtrVec& items);
	DWORD CountItemPid(WORD item_pid);
	DWORD CountItemType(BYTE type);
	bool MoveItem(DWORD item_id, BYTE to_slot, DWORD count);
	bool IsCanSortItems();
	Item* GetItemHighSortValue();
	Item* GetItemLowSortValue();
	void GetInvItems(ItemVec& items);
	DWORD GetItemsCount();
	DWORD GetItemsCountInv();
	DWORD GetItemsWeight();
	DWORD GetItemsWeightKg();
	DWORD GetItemsVolume();
	int GetFreeWeight();
	int GetFreeVolume();
	bool IsHaveLightSources();
	Item* GetSlotUse(BYTE num_slot, BYTE& use);
	DWORD GetUsePicName(BYTE num_slot);
	bool IsItemAim(BYTE num_slot);
	BYTE GetUse(){return ItemSlotMain->Data.Mode&0xF;}
	BYTE GetFullRate(){return ItemSlotMain->Data.Mode;}
	bool NextRateItem(bool prev);
	BYTE GetAim(){return (ItemSlotMain->Data.Mode>>4)&0xF;}
	bool IsAim(){return GetAim()>0;}
	void SetAim(BYTE hit_location);
	DWORD GetUseApCost(Item* item, BYTE rate);
	ProtoItem* GetUnarmedItem(BYTE tree, BYTE priority);
	ProtoItem* GetProtoMain(){return ItemSlotMain->Proto;}
	ProtoItem* GetProtoExt(){return ItemSlotExt->Proto;}
	ProtoItem* GetProtoArm(){return ItemSlotArmor->Proto;}
	const char* GetMoneyStr();
	Item* GetAmmoAvialble(Item* weap);
	bool IsOverweight(){return GetItemsWeight()>GetParam(ST_CARRY_WEIGHT);}
	bool IsDoubleOverweight(){return GetItemsWeight()>GetParam(ST_CARRY_WEIGHT)*2;}

	// Moving
public:
	bool IsRunning;
	WordPairVec MoveSteps;
	int CurMoveStep;
	bool IsNeedMove(){return MoveSteps.size() && !IsWalkAnim();}
	void ZeroSteps(){MoveSteps.clear(); CurMoveStep=0;}
	void Move(BYTE dir);

	// ReSet
private:
	bool needReSet;
	DWORD reSetTick;

public:
	bool IsNeedReSet(){return needReSet && Timer::GameTick()>=reSetTick;}
	void ReSetOk(){needReSet=false;}

	// Time
public:
	DWORD TickCount;
	DWORD StartTick;

	void TickStart(DWORD ms){TickCount=ms; StartTick=Timer::GameTick();}
	void TickNull(){TickCount=0;}
	bool IsFree(){return (Timer::GameTick()-StartTick>=TickCount);}

	// Animation
public:
	static AnyFrames* DefaultAnim;
	void* Layers3d;
	static void FreeAnimations();
	BYTE GetAnim1();
	BYTE GetAnim2();
	void ProcessAnim(bool is2d, DWORD anim1, DWORD anim2, Item* item);
	int* GetLayers3dData();
	bool IsAnimAviable(DWORD anim1, DWORD anim2);
	static AnyFrames* LoadAnim(DWORD& crtype, DWORD& anim1, DWORD& anim2, BYTE dir);

private:
	static AnimMap crAnim;
	static bool LoadAnimSpr(DWORD crtype, DWORD anim1, DWORD anim2, BYTE dir);
	static AnyFrames* GetAnimSpr(DWORD crtype, DWORD anim1, DWORD anim2, BYTE dir);

	short begSpr;
	short endSpr;
	short curSpr;
	DWORD animStartTick;

	struct CritterAnim
	{
		AnyFrames* Anim;
		DWORD AnimTick;
		int BeginFrm;
		int EndFrm;
		bool MoveText;
		DWORD IndCrType,IndAnim1,IndAnim2;
		Item* ActiveItem;
		CritterAnim(AnyFrames* anim, DWORD tick, int beg_frm, int end_frm, bool move_text, DWORD ind_crtype, DWORD ind_anim1, DWORD ind_anim2, Item* item):
			Anim(anim),AnimTick(tick),BeginFrm(beg_frm),EndFrm(end_frm),MoveText(move_text),IndCrType(ind_crtype),IndAnim1(ind_anim1),IndAnim2(ind_anim2),ActiveItem(item){}
	};
typedef vector<CritterAnim> CritterAnimVec;

	CritterAnimVec animSequence;

	CritterAnim* GetCurAnim(){return IsAnim()?&animSequence[0]:NULL;}
	void NextAnim(bool erase_front);

public:
	Animation3d* Anim3d;
	Animation3d* Anim3dStay;
	bool Visible;
	BYTE Alpha;
	INTRECT DRect;
	bool SprDrawValid;
	Sprite* SprDraw;
	DWORD SprId;
	short SprOx,SprOy;
	// Extra offsets
	short OxExtI,OyExtI;
	float OxExtF,OyExtF;
	float OxExtSpeed,OyExtSpeed;
	DWORD OffsExtNextTick;

	void SetSprRect();
	bool Is3dAnim(){return Anim3d!=NULL;}
	Animation3d* GetAnim3d(){return Anim3d;}
	bool IsAnim(){return animSequence.size()>0;}
	bool IsWalkAnim();
	void ClearAnim();

	void SetOffs(short set_ox, short set_oy, bool move_text);
	void ChangeOffs(short change_ox, short change_oy, bool move_text);
	void AccamulateOffs();
	void AddOffsExt(short ox, short oy);

	// Stay sprite
private:
	int staySprDir;
	DWORD staySprTick;

	// Finish
private:
	DWORD finishingTime;

public:
	bool IsFinishing(){return finishingTime!=0;}
	bool IsFinish(){return (finishingTime && Timer::GameTick()>finishingTime);}

	// Fade
private:
	bool fadingEnable;
	bool fadeUp;

	void SetFade(bool fade_up);
	BYTE GetFadeAlpha();

public:
	DWORD FadingTick;

	// Text
public:
	INTRECT GetTextRect();
	void SetText(const char* str, DWORD color, DWORD text_delay);
	void DrawTextOnHead();

private:
	INTRECT textRect;
	DWORD tickFun;
	string strTextOnHead;
	DWORD tickStartText;
	DWORD tickTextDelay;
	DWORD textOnHeadColor;

	// Ap cost
public:
	int GetApCostCritterMove(bool is_run){return IsTurnBased()?GameOpt.TbApCostCritterMove*AP_DIVIDER*(IsDmgTwoLeg()?4:(IsDmgLeg()?2:1)):(GetParam(TO_BATTLE)?(is_run?GameOpt.RtApCostCritterRun:GameOpt.RtApCostCritterWalk):0);}
	int GetApCostMoveItemContainer(){return IsTurnBased()?GameOpt.TbApCostMoveItemContainer:GameOpt.RtApCostMoveItemContainer;}
	int GetApCostMoveItemInventory(){int val=IsTurnBased()?GameOpt.TbApCostMoveItemInventory:GameOpt.RtApCostMoveItemInventory; if(IsRawParam(PE_QUICK_POCKETS)) val/=2; return val;}
	int GetApCostPickItem(){return IsTurnBased()?GameOpt.TbApCostPickItem:GameOpt.RtApCostPickItem;}
	int GetApCostDropItem(){return IsTurnBased()?GameOpt.TbApCostDropItem:GameOpt.RtApCostDropItem;}
	int GetApCostPickCritter(){return IsTurnBased()?GameOpt.TbApCostPickCritter:GameOpt.RtApCostPickCritter;}
	int GetApCostUseSkill(){return IsTurnBased()?GameOpt.TbApCostUseSkill:GameOpt.RtApCostUseSkill;}

	// Ref counter
public:
	short RefCounter;
	bool IsNotValid;
	void AddRef(){RefCounter++;}
	void Release(){RefCounter--; if(RefCounter<=0) delete this;}
};

typedef map<DWORD,CritterCl*,less<DWORD>> CritMap;
typedef map<DWORD,CritterCl*,less<DWORD>>::iterator CritMapIt;
typedef map<DWORD,CritterCl*,less<DWORD>>::value_type CritMapVal;
typedef vector<CritterCl*> CritVec;
typedef vector<CritterCl*>::iterator CritVecIt;
typedef CritterCl* CritterClPtr;

#endif // __CRITTER_CL__