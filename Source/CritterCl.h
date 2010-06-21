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
	CritterCl();
	~CritterCl();
	void Init();
	void InitForRegistration();
	void Finish();
	void GenParams();
	void SetBaseType(DWORD type);
	void SetDir(BYTE dir);
	BYTE GetDir(){return CrDir;}
	void Animate(DWORD anim1, DWORD anim2, Item* item);
	void AnimateStay();
	void Action(int action, int action_ext, Item* item, bool local_call = true);
	void Process();
	const char* GetInfo(){return Name.c_str();}
	DWORD GetId(){return Id;}
	WORD GetHexX(){return HexX;}
	WORD GetHexY(){return HexY;}
	DWORD GetPos(){return HEX_POS(HexX,HexY);}
	Item* GetSlotUse(BYTE num_slot, BYTE& use);
	int GetAttackMaxDist();
	int GetUsePic(BYTE num_slot);
	bool IsItemAim(BYTE num_slot);
	bool IsDmgEye(){return IsPerk(DAMAGE_EYE);}
	bool IsDmgLeg(){return IsPerk(DAMAGE_RIGHT_LEG) || IsPerk(DAMAGE_LEFT_LEG);}
	bool IsDmgTwoLeg(){return IsPerk(DAMAGE_RIGHT_LEG) && IsPerk(DAMAGE_LEFT_LEG);}
	bool IsDmgArm(){return IsPerk(DAMAGE_RIGHT_ARM) || IsPerk(DAMAGE_LEFT_ARM);}
	bool IsDmgTwoArm(){return IsPerk(DAMAGE_RIGHT_ARM) && IsPerk(DAMAGE_LEFT_ARM);}
	int GetLook();
	DWORD GetTalkDistance();
	int GetCond(){return Cond;}
	int GetCondExt(){return CondExt;}
	void DrawStay(INTRECT r);
	const char* GetName(){return Name.c_str();}
	const char* GetPass(){return Pass;}
	ProtoItem* GetProtoMain(){return ItemSlotMain->Proto;}
	ProtoItem* GetProtoExt(){return ItemSlotExt->Proto;}
	ProtoItem* GetProtoArm(){return ItemSlotArmor->Proto;}
	const char* GetMoneyStr();
	Item* GetAmmoAvialble(Item* weap);
	DWORD GetTimeout(int timeout);
	bool IsTurnBased(){return TB_BATTLE_TIMEOUT_CHECK(GetTimeout(TO_BATTLE));}
	bool IsLastHexes();
	void FixLastHexes();
	WORD PopLastHexX();
	WORD PopLastHexY();
	bool IsNpc(){return FLAG(Flags,FCRIT_NPC);}
	bool IsPlayer(){return FLAG(Flags,FCRIT_PLAYER);}
	bool IsChosen(){return Human;}
	bool IsGmapRule(){return FLAG(Flags,FCRIT_RULEGROUP)!=0;}
	bool IsOnline(){return FLAG(Flags,FCRIT_DISCONNECT)==0;}
	bool IsOffline(){return FLAG(Flags,FCRIT_DISCONNECT)!=0;}
	bool IsLife(){return Cond==COND_LIFE;}
	bool IsLifeNone(){return Cond==COND_LIFE && CondExt==COND_LIFE_NONE;}
	bool IsKnockout(){return Cond==COND_KNOCKOUT;}
	bool IsDead(){return Cond==COND_DEAD;}
	bool IsToTalk(){return IsNpc() && IsLifeNone() && Params[ST_DIALOG_ID];}
	bool IsOverweight(){return GetItemsWeight()>GetParam(ST_CARRY_WEIGHT);}
	bool IsDoubleOverweight(){return GetItemsWeight()>GetParam(ST_CARRY_WEIGHT)*2;}
	bool IsInjured(){return IsDmgArm() || IsDmgLeg() || IsDmgEye();}
	bool CheckFind(int find_type);
	bool IsCombatMode(){return GetTimeout(TO_BATTLE)!=0;}

	DWORD Id;
	WORD Pid;
	WORD HexX,HexY;
	BYTE CrDir;
	int Params[MAX_PARAMS];
	string Name;
	int NameRefCounter;
	string Lexems;
	int LexemsRefCounter;
	string Avatar;
	char Pass[MAX_NAME+1];
	bool Human;
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
	DWORD NameColor;
	DWORD ContourColor;
	WordVec LastHexX,LastHexY;
	BYTE Cond;
	BYTE CondExt;
	DWORD Flags;
	DWORD BaseType,BaseTypeAlias;
	DWORD ApRegenerationTick;

	// Parameters
	int GetParam(DWORD index);
	void ChangeParam(DWORD index);
	void ProcessChangedParams();
	DWORD GetMaxVolume(){return CRITTER_INV_VOLUME;}
	int GetSkill(DWORD index){return GetParam(index);}
	bool IsTagSkill(DWORD index){return Params[TAG_SKILL1]==index || Params[TAG_SKILL2]==index || Params[TAG_SKILL3]==index || Params[TAG_SKILL4]==index;}
	bool IsPerk(DWORD index){return GetParam(index)!=0;}
	int GetPerk(DWORD index){return GetParam(index);}
	int GetReputation(DWORD index);
	bool IsAddicted(){for(int i=ADDICTION_BEGIN;i<=ADDICTION_END;i++) if(GetParam(i)!=0) return true; return false;}
	DWORD GetMaxWeightKg(){return GetParam(ST_CARRY_WEIGHT)/1000;}
	DWORD GetCrType();
	DWORD GetCrTypeAlias();
	BYTE GetUse(){return ItemSlotMain->Data.Rate&0xF;}
	BYTE GetFullRate(){return ItemSlotMain->Data.Rate;}
	bool NextRateItem(bool prev);
	BYTE GetAim(){return (ItemSlotMain->Data.Rate>>4)&0xF;}
	bool IsAim(){return GetAim()>0;}
	void SetAim(BYTE hit_location);
	DWORD GetUseApCost(Item* item, BYTE rate);
	ProtoItem* GetUnarmedItem(BYTE tree, BYTE priority);

	// Migrate to dll in future
	int GetStrength(){int val=Params[ST_STRENGTH]+Params[ST_STRENGTH_EXT]; if(Params[PE_ADRENALINE_RUSH] && GetTimeout(TO_BATTLE) && Params[ST_CURRENT_HP]<=(Params[ST_MAX_LIFE]+Params[ST_STRENGTH]+Params[ST_ENDURANCE]*2)/2) val++; return CLAMP(val,1,10);}
	int GetPerception(){int val=(IsDmgEye()?1:Params[ST_PERCEPTION]+Params[ST_PERCEPTION_EXT]); if(Params[TRAIT_NIGHT_PERSON]) val+=GetNightPersonBonus(); return CLAMP(val,1,10);}
	int GetEndurance(){int val=Params[ST_ENDURANCE]+Params[ST_ENDURANCE_EXT]; return CLAMP(val,1,10);}
	int GetCharisma(){int val=Params[ST_CHARISMA]+Params[ST_CHARISMA_EXT]; return CLAMP(val,1,10);}
	int GetIntellegence(){int val=Params[ST_INTELLECT]+Params[ST_INTELLECT_EXT]; if(Params[TRAIT_NIGHT_PERSON]) val+=GetNightPersonBonus(); return CLAMP(val,1,10);}
	int GetAgility(){int val=Params[ST_AGILITY]+Params[ST_AGILITY_EXT]; return CLAMP(val,1,10);}
	int GetLuck(){int val=Params[ST_LUCK]+Params[ST_LUCK_EXT]; return CLAMP(val,1,10);}
	int GetHp(){return Params[ST_CURRENT_HP];}
	int GetMaxLife(){int val=Params[ST_MAX_LIFE]+Params[ST_MAX_LIFE_EXT]+Params[ST_STRENGTH]+Params[ST_ENDURANCE]*2; return CLAMP(val,1,9999);}
	int GetMaxAp(){int val=Params[ST_ACTION_POINTS]+Params[ST_ACTION_POINTS_EXT]+GetAgility()/2; return CLAMP(val,1,9999);}
	int GetAp(){int val=Params[ST_CURRENT_AP]; val/=AP_DIVIDER; return CLAMP(val,-9999,9999);}
	int GetRealAp(){return Params[ST_CURRENT_AP];}
	void SubAp(int val){ChangeParam(ST_CURRENT_AP);Params[ST_CURRENT_AP]-=val*AP_DIVIDER;ApRegenerationTick=0;}
	int GetMaxMoveAp(){int val=Params[ST_MAX_MOVE_AP]; return CLAMP(val,0,9999);}
	int GetMoveAp(){int val=Params[ST_MOVE_AP]; return CLAMP(val,0,9999);}
	int GetAllAp(){return GetAp()+GetMoveAp();}
	void SubMoveAp(int val){ChangeParam(ST_CURRENT_AP);Params[ST_MOVE_AP]-=val;}
	DWORD GetMaxWeight(){int val=max(Params[ST_CARRY_WEIGHT]+Params[ST_CARRY_WEIGHT_EXT],0); val+=CONVERT_GRAMM(25+GetStrength()*(25-Params[TRAIT_SMALL_FRAME]*10)); return (DWORD)val;}
	int GetSequence(){int val=Params[ST_SEQUENCE]+Params[ST_SEQUENCE_EXT]+GetPerception()*2; return CLAMP(val,0,9999);}
	int GetMeleeDmg(){int strength=GetStrength(); int val=Params[ST_MELEE_DAMAGE]+Params[ST_MELEE_DAMAGE_EXT]+(strength>6?strength-5:1); return CLAMP(val,1,9999);}
	int GetHealingRate(){int e=GetEndurance(); int val=Params[ST_HEALING_RATE]+Params[ST_HEALING_RATE_EXT]+max(1,e/3); return CLAMP(val,-9999,9999);}
	int GetCriticalChance(){int val=Params[ST_CRITICAL_CHANCE]+Params[ST_CRITICAL_CHANCE_EXT]+GetLuck(); return CLAMP(val,0,100);}
	int GetMaxCritical(){int val=Params[ST_MAX_CRITICAL]+Params[ST_MAX_CRITICAL_EXT]; return CLAMP(val,-100,100);}
	int GetAc();
	int GetDamageResistance(int dmg_type);
	int GetDamageThreshold(int dmg_type);
	int GetRadiationResist(){int val=Params[ST_RADIATION_RESISTANCE]+Params[ST_RADIATION_RESISTANCE_EXT]+GetEndurance()*2; return CLAMP(val,0,95);}
	int GetPoisonResist(){int val=Params[ST_POISON_RESISTANCE]+Params[ST_POISON_RESISTANCE_EXT]+GetEndurance()*5; return CLAMP(val,0,95);}
	int GetNightPersonBonus();

	// Items
	ItemPtrMap InvItems;
	Item DefItemSlotMain;
	Item DefItemSlotExt;
	Item DefItemSlotArmor;
	Item* ItemSlotMain;
	Item* ItemSlotExt;
	Item* ItemSlotArmor;

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
	bool CheckKey(DWORD door_id);
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
	Item* GetRadio();

	// Moving
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
	bool IsNeedReSet(){return needReSet && Timer::FastTick()>=reSetTick;}
	void ReSetOk(){needReSet=false;}

	// Time
public:
	DWORD TickCount;
	DWORD StartTick;

	void TickStart(DWORD ms){TickCount=ms; StartTick=Timer::FastTick();}
	void TickNull(){TickCount=0;}
	bool IsFree(){return (Timer::FastTick()-StartTick>=TickCount);}

	// Animation
public:
	static SpriteManager* SprMngr;
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
		CritterAnim(AnyFrames* anim, DWORD tick, int beg_frm, int end_frm, bool move_text, BYTE ind_crtype, BYTE ind_anim1, BYTE ind_anim2, Item* item):
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
	bool finishing;
	DWORD finishingTime;

public:
	bool IsFinishing(){return finishing;}
	bool IsFinish(){return (finishing && Timer::FastTick()>finishingTime);}

	// Fade
private:
	bool fadingEnable;
	bool fadeUp;

	void SetFade(bool fade_up);
	BYTE GetFadeAlpha();

public:
	DWORD FadingTick;
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

public:
	int GetApCostCritterMove(bool is_run){return IsTurnBased()?GameOpt.TbApCostCritterMove*AP_DIVIDER*(IsDmgTwoLeg()?4:(IsDmgLeg()?2:1)):(GetTimeout(TO_BATTLE)?(is_run?GameOpt.RtApCostCritterRun:GameOpt.RtApCostCritterWalk):0);}
	int GetApCostMoveItemContainer(){return IsTurnBased()?GameOpt.TbApCostMoveItemContainer:GameOpt.RtApCostMoveItemContainer;}
	int GetApCostMoveItemInventory(){int val=IsTurnBased()?GameOpt.TbApCostMoveItemInventory:GameOpt.RtApCostMoveItemInventory; if(IsPerk(PE_QUICK_POCKETS)) val/=2; return val;}
	int GetApCostPickItem(){return IsTurnBased()?GameOpt.TbApCostPickItem:GameOpt.RtApCostPickItem;}
	int GetApCostDropItem(){return IsTurnBased()?GameOpt.TbApCostDropItem:GameOpt.RtApCostDropItem;}
	int GetApCostReload(){return IsTurnBased()?GameOpt.TbApCostReloadWeapon:GameOpt.RtApCostReloadWeapon;}
	int GetApCostPickCritter(){return IsTurnBased()?GameOpt.TbApCostPickCritter:GameOpt.RtApCostPickCritter;}
	int GetApCostUseItem(){return IsTurnBased()?GameOpt.TbApCostUseItem:GameOpt.RtApCostUseItem;}
	int GetApCostUseSkill(){return IsTurnBased()?GameOpt.TbApCostUseSkill:GameOpt.RtApCostUseSkill;}

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