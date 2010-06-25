#include "StdAfx.h"
#include "CritterCl.h"
#include "CritterType.h"
#include "ResourceManager.h"

#ifdef FONLINE_CLIENT
#include "SoundManager.h"
#include "Script.h"
#endif

SpriteManager* CritterCl::SprMngr=NULL;
AnimMap CritterCl::crAnim;
AnyFrames* CritterCl::DefaultAnim=NULL;
int CritterCl::ParamsChangeScript[MAX_PARAMS]={0};
int CritterCl::ParamsGetScript[MAX_PARAMS]={0};
bool CritterCl::ParamsRegEnabled[MAX_PARAMS]={0};
int CritterCl::ParamsReg[MAX_PARAMS]={0};
DWORD CritterCl::ParametersMin[MAX_PARAMETERS_ARRAYS]={0};
DWORD CritterCl::ParametersMax[MAX_PARAMETERS_ARRAYS]={MAX_PARAMS-1};
bool CritterCl::ParametersOffset[MAX_PARAMETERS_ARRAYS]={false};
bool CritterCl::SlotEnabled[0x100]={true,true,true,true,false};

CritterCl::CritterCl():
CrDir(0),SprId(0),Id(0),Pid(0),NameColor(0),ContourColor(0),
ItemSlotMain(&DefItemSlotMain),ItemSlotArmor(&DefItemSlotArmor),ItemSlotExt(&DefItemSlotExt),
begSpr(0),endSpr(0),curSpr(0),animStartTick(0),
SprOx(0),SprOy(0),StartTick(0),TickCount(0),ApRegenerationTick(0),
tickTextDelay(0),textOnHeadColor(COLOR_TEXT),Human(false),Alpha(0),
fadingEnable(false),FadingTick(0),fadeUp(false),finishing(false),finishingTime(0),
staySprDir(0),staySprTick(0),needReSet(false),reSetTick(0),CurMoveStep(0),
Visible(true),SprDrawValid(false),IsNotValid(false),RefCounter(1),NameRefCounter(0x80000000),LexemsRefCounter(0x80000000),
OxExtI(0),OyExtI(0),OxExtF(0),OyExtF(0),OxExtSpeed(0),OyExtSpeed(0),OffsExtNextTick(0),
Anim3d(NULL),Anim3dStay(NULL),Layers3d(NULL)
{
	Name="";
	StringCopy(Pass,"");
	ZeroMemory(Params,sizeof(Params));
	ZeroMemory(&DefItemSlotMain,sizeof(Item));
	ZeroMemory(&DefItemSlotExt,sizeof(Item));
	ZeroMemory(&DefItemSlotArmor,sizeof(Item));
	tickFun=Timer::FastTick()+Random(STAY_WAIT_SHOW_TIME/2,STAY_WAIT_SHOW_TIME);
	for(int i=0;i<MAX_PARAMETERS_ARRAYS;i++) ThisPtr[i]=this;
}

CritterCl::~CritterCl()
{
	SAFEDEL(Anim3d);
	SAFEDEL(Anim3dStay);
	if(Layers3d)
	{
#ifdef FONLINE_CLIENT
		((asIScriptArray*)Layers3d)->Release();
#else
		SAFEDELA(Layers3d);
#endif
	}
}

void CritterCl::Init()
{
	textOnHeadColor=COLOR_CRITTER_NAME;
	AnimateStay();
	SpriteInfo* si=SprMngr->GetSpriteInfo(SprId);
	if(si) textRect(0,0,si->Width,si->Height);
	SetFade(true);
}

void CritterCl::InitForRegistration()
{
	Name="";
	StringCopy(Pass,"");
	BaseType=0;

	ZeroMemory(Params,sizeof(Params));
	ZeroMemory(ParamsReg,sizeof(ParamsReg));
	ParamsReg[ST_STRENGTH]=5;
	ParamsReg[ST_PERCEPTION]=5;
	ParamsReg[ST_ENDURANCE]=5;
	ParamsReg[ST_CHARISMA]=5;
	ParamsReg[ST_INTELLECT]=5;
	ParamsReg[ST_AGILITY]=5;
	ParamsReg[ST_LUCK]=5;
	ParamsReg[ST_AGE]=Random(AGE_MIN,AGE_MAX);
	ParamsReg[ST_GENDER]=GENDER_MALE;
	GenParams();
}

void CritterCl::Finish()
{
	SetFade(false);
	finishing=true;
	finishingTime=FadingTick;
}

void CritterCl::GenParams()
{
#ifdef FONLINE_CLIENT
	if(Script::PrepareContext(ClientFunctions.PlayerGeneration,CALL_FUNC_STR,"Registration"))
	{
		asIScriptArray* arr=Script::CreateArray("int[]");
		if(!arr) return;
		arr->Resize(MAX_PARAMS);
		for(int i=0;i<MAX_PARAMS;i++) (*(int*)arr->GetElementPointer(i))=ParamsReg[i];
		Script::SetArgObject(arr);
		if(Script::RunPrepared() && arr->GetElementCount()==MAX_PARAMS)
			for(int i=0;i<MAX_PARAMS;i++) Params[i]=(*(int*)arr->GetElementPointer(i));
		arr->Release();
	}
#endif
}

void CritterCl::SetFade(bool fade_up)
{
	DWORD tick=Timer::FastTick();
	FadingTick=tick+FADING_PERIOD-(FadingTick>tick?FadingTick-tick:0);
	fadeUp=fade_up;
	fadingEnable=true;
}

BYTE CritterCl::GetFadeAlpha()
{
	DWORD tick=Timer::FastTick();
	int fading_proc=100-Procent(FADING_PERIOD,FadingTick>tick?FadingTick-tick:0);
	fading_proc=CLAMP(fading_proc,0,100);
	if(fading_proc>=100)
	{
		fading_proc=100;
		fadingEnable=false;
	}

	return fadeUp==true?(fading_proc*0xFF)/100:((100-fading_proc)*0xFF)/100;
}

void CritterCl::AddItem(Item* item)
{
	item->Accessory=ITEM_ACCESSORY_CRITTER;
	item->ACC_CRITTER.Id=this->Id;

	bool anim_stay=true;
	switch(item->ACC_CRITTER.Slot)
	{
	case SLOT_HAND1: ItemSlotMain=item; break;
	case SLOT_HAND2: ItemSlotExt=item; break;
	case SLOT_ARMOR: ItemSlotArmor=item; break;
	default:
		if(item==ItemSlotMain) ItemSlotMain=&DefItemSlotMain;
		else if(item==ItemSlotExt) ItemSlotExt=&DefItemSlotExt;
		else if(item==ItemSlotArmor) ItemSlotArmor=&DefItemSlotArmor;
		else anim_stay=false;
		break;
	}

	InvItems.insert(ItemPtrMapVal(item->GetId(),item));
	if(anim_stay && !IsAnim()) AnimateStay();
}

void CritterCl::EraseItem(Item* item, bool animate)
{
	if(!item) return;

	if(ItemSlotMain==item) ItemSlotMain=&DefItemSlotMain;
	if(ItemSlotExt==item) ItemSlotExt=&DefItemSlotExt;
	if(ItemSlotArmor==item) ItemSlotArmor=&DefItemSlotArmor;
	item->Accessory=0;
	InvItems.erase(item->GetId());
	item->Release();
	if(animate && !IsAnim()) AnimateStay();
}

void CritterCl::EraseAllItems()
{
	ItemPtrMap items=InvItems;
	for(ItemPtrMapIt it=items.begin(),end=items.end();it!=end;++it) EraseItem((*it).second,false);
	InvItems.clear();
}

Item* CritterCl::GetItem(DWORD item_id)
{
	ItemPtrMapIt it=InvItems.find(item_id);
	return (it!=InvItems.end())?(*it).second:NULL;
}

Item* CritterCl::GetItemByPid(WORD item_pid)
{
	for(ItemPtrMapIt it=InvItems.begin(),end=InvItems.end();it!=end;++it)
		if((*it).second->GetProtoId()==item_pid) return (*it).second;
	return NULL;
}

Item* CritterCl::GetAmmo(DWORD caliber)
{
	for(ItemPtrMapIt it=InvItems.begin(),end=InvItems.end();it!=end;++it)
		if((*it).second->GetType()==ITEM_TYPE_AMMO && (*it).second->Proto->Ammo.Caliber==caliber) return (*it).second;
	return NULL;
}

Item* CritterCl::GetItemSlot(int slot)
{
	for(ItemPtrMapIt it=InvItems.begin(),end=InvItems.end();it!=end;++it)
		if((*it).second->ACC_CRITTER.Slot==slot) return (*it).second;
	return NULL;
}

void CritterCl::GetItemsSlot(int slot, ItemPtrVec& items)
{
	for(ItemPtrMapIt it=InvItems.begin(),end=InvItems.end();it!=end;++it)
	{
		Item* item=(*it).second;
		if(slot==-1 || item->ACC_CRITTER.Slot==slot) items.push_back(item);
	}
}

void CritterCl::GetItemsType(int type, ItemPtrVec& items)
{
	for(ItemPtrMapIt it=InvItems.begin(),end=InvItems.end();it!=end;++it)
	{
		Item* item=(*it).second;
		if(item->GetType()==type) items.push_back(item);
	}
}

DWORD CritterCl::CountItemPid(WORD item_pid)
{
	DWORD result=0;
	for(ItemPtrMapIt it=InvItems.begin(),end=InvItems.end();it!=end;++it)
		if((*it).second->GetProtoId()==item_pid) result+=(*it).second->GetCount();
	return result;
}

DWORD CritterCl::CountItemType(BYTE type)
{
	DWORD res=0;
	for(ItemPtrMapIt it=InvItems.begin(),end=InvItems.end();it!=end;++it)
		if((*it).second->GetType()==type) res+=(*it).second->GetCount();
	return res;
}

bool CritterCl::CheckKey(DWORD door_id)
{
	if(!door_id) return true;

//Fingers, Eyes
//#define _CritFingersDoorId #(critterId) (0x80000000|(critterId))
//#define _CritEyesDoorId #(critterId) (0x40000000|(critterId))
	if((0x80000000|GetId())==door_id) return !IsDmgTwoArm();
	if((0x40000000|GetId())==door_id) return !IsDmgEye();

//Keys items
	for(ItemPtrMapIt it=InvItems.begin(),end=InvItems.end();it!=end;++it)
	{
		Item* key=(*it).second;
		if(key->IsKey() && key->KeyDoorId()==door_id) return true;
	}
	return false;
}

bool CritterCl::MoveItem(DWORD item_id, BYTE to_slot, DWORD count)
{
	Item* item=GetItem(item_id);
	if(!item) return false;

	BYTE from_slot=item->ACC_CRITTER.Slot;
	bool is_castling=((to_slot==SLOT_HAND1 && from_slot==SLOT_HAND2) || (to_slot==SLOT_HAND2 && from_slot==SLOT_HAND1));

	if(item->ACC_CRITTER.Slot==to_slot) return false;

	if(to_slot==SLOT_GROUND)
	{
		Action(ACTION_DROP_ITEM,from_slot,item);
		if(count<item->GetCount()) item->Count_Sub(count);
		else EraseItem(item,true);
		return true;
	}

	if(!SlotEnabled[to_slot]) return false;
	if(to_slot!=SLOT_INV && !is_castling && GetItemSlot(to_slot)) return false;
	if(to_slot>SLOT_ARMOR && to_slot!=item->Proto->Slot) return false;
	if(to_slot==SLOT_HAND1 && item->IsWeapon() && !CritType::IsAnim1(GetCrType(),item->Proto->Weapon.Anim1)) return false;
	if(to_slot==SLOT_ARMOR && (!item->IsArmor() || item->Proto->Slot || !CritType::IsCanArmor(GetCrType()))) return false;

	if(is_castling && ItemSlotMain->GetId() && ItemSlotExt->GetId())
	{
		Item* tmp_item=ItemSlotExt;
		ItemSlotExt=ItemSlotMain;
		ItemSlotMain=tmp_item;
		ItemSlotMain->ACC_CRITTER.Slot=SLOT_HAND1;
		ItemSlotExt->ACC_CRITTER.Slot=SLOT_HAND2;
		Action(ACTION_SHOW_ITEM,from_slot,ItemSlotMain);
	}
	else
	{
		BYTE act=ACTION_MOVE_ITEM;
		if(to_slot==SLOT_HAND1) act=ACTION_SHOW_ITEM;
		else if(from_slot==SLOT_HAND1) act=ACTION_HIDE_ITEM;

		if(to_slot!=SLOT_HAND1) Action(act,from_slot,item);

		if(from_slot==SLOT_HAND1) ItemSlotMain=&DefItemSlotMain;
		else if(from_slot==SLOT_HAND2) ItemSlotExt=&DefItemSlotExt;
		else if(from_slot==SLOT_ARMOR) ItemSlotArmor=&DefItemSlotArmor;
		if(to_slot==SLOT_HAND1) ItemSlotMain=item;
		else if(to_slot==SLOT_HAND2) ItemSlotExt=item;
		else if(to_slot==SLOT_ARMOR) ItemSlotArmor=item;
		item->ACC_CRITTER.Slot=to_slot;

		if(to_slot==SLOT_HAND1) Action(act,from_slot,item);
	}

	return true;
}

bool CritterCl::IsCanSortItems()
{
	DWORD inv_items=0;
	for(ItemPtrMapIt it=InvItems.begin(),end=InvItems.end();it!=end;++it)
	{
		if((*it).second->ACC_CRITTER.Slot!=SLOT_INV) continue;
		inv_items++;
		if(inv_items>1) return true;
	}
	return false;
}

Item* CritterCl::GetItemHighSortValue()
{
	Item* result=NULL;
	for(ItemPtrMapIt it=InvItems.begin(),end=InvItems.end();it!=end;++it)
	{
		Item* item=(*it).second;
		if(!result) result=item;
		else if(item->GetSortValue()>result->GetSortValue()) result=item;
	}
	return result;
}

Item* CritterCl::GetItemLowSortValue()
{
	Item* result=NULL;
	for(ItemPtrMapIt it=InvItems.begin(),end=InvItems.end();it!=end;++it)
	{
		Item* item=(*it).second;
		if(!result) result=item;
		else if(item->GetSortValue()<result->GetSortValue()) result=item;
	}
	return result;
}

void CritterCl::GetInvItems(ItemVec& items)
{
	items.clear();
	for(ItemPtrMapIt it=InvItems.begin(),end=InvItems.end();it!=end;++it)
	{
		Item* item=(*it).second;
		if(item->ACC_CRITTER.Slot==SLOT_INV) items.push_back(*item);
	}

	Item::SortItems(items);
}

DWORD CritterCl::GetItemsCount()
{
	DWORD count=0;
	for(ItemPtrMapIt it=InvItems.begin(),end=InvItems.end();it!=end;++it)
		count+=(*it).second->GetCount();
	return count;
}

DWORD CritterCl::GetItemsCountInv()
{
	DWORD res=0;
	for(ItemPtrMapIt it=InvItems.begin(),end=InvItems.end();it!=end;++it)
		if((*it).second->ACC_CRITTER.Slot==SLOT_INV) res++;
	return res;
}

DWORD CritterCl::GetItemsWeight()
{
	DWORD res=0;
	for(ItemPtrMapIt it=InvItems.begin(),end=InvItems.end();it!=end;++it)
		res+=(*it).second->GetWeight();
	return res;
}

DWORD CritterCl::GetItemsWeightKg()
{
	return GetItemsWeight()/1000;
}

DWORD CritterCl::GetItemsVolume()
{
	WORD res=0;
	for(ItemPtrMapIt it=InvItems.begin();it!=InvItems.end();++it)
		res+=(*it).second->GetVolume();
	return res;
}

int CritterCl::GetFreeWeight()
{
	int cur_weight=GetItemsWeight();
	int max_weight=GetParam(ST_CARRY_WEIGHT);
	return max_weight-cur_weight;
}

int CritterCl::GetFreeVolume()
{
	int cur_volume=GetItemsVolume();
	int max_volume=GetMaxVolume();
	return max_volume-cur_volume;
}

Item* CritterCl::GetSlotUse(BYTE num_slot, BYTE& use)
{
	Item* item=NULL;
	switch(num_slot)
	{
	case SLOT_HAND1:
		item=ItemSlotMain;
		use=GetUse();
		break;
	case SLOT_HAND2:
		item=ItemSlotExt;
		if(item->IsWeapon()) use=USE_PRIMARY;
		else if(item->IsCanUse() || item->IsCanUseOnSmth()) use=USE_USE;
		else use=0xF;
		break;
	default:
		break;	
	}
	return item;
}

int CritterCl::GetAttackMaxDist()
{
	BYTE use;
	Item* weap=GetSlotUse(SLOT_HAND1,use);
	if(!weap->IsWeapon()) return 0;
	int dist=weap->Proto->Weapon.MaxDist[use];
	if(weap->Proto->Weapon.Skill[use]==SKILL_OFFSET(SK_THROWING)) dist=min(dist,3*min(10,GetParam(ST_STRENGTH)+2*GetPerk(PE_HEAVE_HO)));
	if(weap->WeapIsHtHAttack(use) && IsPerk(MODE_RANGE_HTH)) dist++;
	return dist;
}

int CritterCl::GetUsePic(BYTE num_slot)
{
	static DWORD use_on_pic=Str::GetHash("art\\intrface\\useon.frm");
	static DWORD use_pic=Str::GetHash("art\\intrface\\uset.frm");
	static DWORD reload_pic=Str::GetHash("art\\intrface\\reload.frm");

	BYTE use;
	Item* item=GetSlotUse(num_slot,use);
	if(!item) return 0;
	if(item->IsWeapon())
	{
		if(use==USE_RELOAD) return reload_pic;
		if(use==USE_USE) return use_on_pic;
		if(use>=MAX_USES) return 0;
		return item->Proto->Weapon.PicHash[use];
	}
	if(item->IsCanUseOnSmth()) return use_on_pic;
	if(item->IsCanUse()) return use_pic;
	return 0;
}

bool CritterCl::IsItemAim(BYTE num_slot)
{
	BYTE use;
	Item* item=GetSlotUse(num_slot,use);
	if(!item) return false;
	if(item->IsWeapon() && use<MAX_USES) return item->Proto->Weapon.Aim[use]!=0;
	return false;
}

bool CritterCl::CheckFind(int find_type)
{
	if(IsNpc())
	{
		if(FLAG(find_type,FIND_ONLY_PLAYERS)) return false;
	}
	else
	{
		if(FLAG(find_type,FIND_ONLY_NPC)) return false;
	}
	return FLAG(find_type,FIND_ALL) ||
		(IsLife() && FLAG(find_type,FIND_LIFE)) ||
		(IsKnockout() && FLAG(find_type,FIND_KO)) ||
		(IsDead() && FLAG(find_type,FIND_DEAD));
}

int CritterCl::GetLook()
{
	int look=GameOpt.LookNormal+GetParam(ST_PERCEPTION)*3+GetParam(ST_BONUS_LOOK);
	if(look<GameOpt.LookMinimum) look=GameOpt.LookMinimum;
	return look;
}

DWORD CritterCl::GetTalkDistance()
{
	int dist=GetParam(ST_TALK_DISTANCE);
	if(dist<=0) dist=GameOpt.TalkDistance;
	return dist;
}

int CritterCl::GetParam(DWORD index)
{
#ifdef FONLINE_CLIENT
	if(ParamsGetScript[index] && Script::PrepareContext(ParamsGetScript[index],CALL_FUNC_STR,GetInfo()))
	{
		Script::SetArgObject(this);
		Script::SetArgDword(index-(ParametersOffset[index]?ParametersMin[index]:0));
		if(Script::RunPrepared()) return Script::GetReturnedDword();
	}
#endif

	switch(index)
	{
	case ST_STRENGTH: return GetStrength();
	case ST_PERCEPTION: return GetPerception();
	case ST_ENDURANCE: return GetEndurance();
	case ST_CHARISMA: return GetCharisma();
	case ST_INTELLECT: return GetIntellegence();
	case ST_AGILITY: return GetAgility();
	case ST_LUCK: return GetLuck();
	case ST_MAX_LIFE: return GetMaxLife();
	case ST_SEQUENCE: return GetSequence();
	case ST_ACTION_POINTS: return GetMaxAp();
	case ST_ARMOR_CLASS: return GetAc();
	case ST_MELEE_DAMAGE: return GetMeleeDmg();
	case ST_CARRY_WEIGHT: return GetMaxWeight();
	case ST_HEALING_RATE: return GetHealingRate();
	case ST_CRITICAL_CHANCE: return GetCriticalChance();
	case ST_MAX_CRITICAL: return GetMaxCritical();
	case ST_RADIATION_RESISTANCE: return GetRadiationResist();
	case ST_POISON_RESISTANCE: return GetPoisonResist();
	case ST_CURRENT_AP: return GetAp();
	default: break;
	}

	if(index>=ST_NORMAL_RESIST && index<=ST_EXPLODE_RESIST) return GetDamageResistance(index-ST_NORMAL_RESIST+1);
	if(index>=ST_NORMAL_ABSORB && index<=ST_EXPLODE_ABSORB) return GetDamageThreshold(index-ST_NORMAL_ABSORB+1);
	if(index>=TIMEOUT_BEGIN && index<=TIMEOUT_END) return GetTimeout(index);
	if(index>=REPUTATION_BEGIN && index<=REPUTATION_END) return GetReputation(index);
	return Params[index];
}

void CritterCl::ChangeParam(DWORD index)
{
	if(!ParamsIsChanged[index] && ParamLocked!=index)
	{
		ParamsChanged.push_back(index);
		ParamsChanged.push_back(Params[index]);
		ParamsIsChanged[index]=true;
	}
}

IntVec CallChange_;
void CritterCl::ProcessChangedParams()
{
	if(ParamsChanged.size())
	{
		CallChange_.clear();
		for(size_t i=0,j=ParamsChanged.size();i<j;i+=2)
		{
			int index=ParamsChanged[i];
			int old_val=ParamsChanged[i+1];
			if(ParamsChangeScript[index] && Params[index]!=old_val)
			{
				CallChange_.push_back(ParamsChangeScript[index]);
				CallChange_.push_back(index);
				CallChange_.push_back(old_val);
			}
			ParamsIsChanged[index]=false;
		}
		ParamsChanged.clear();

		if(CallChange_.size())
		{
			for(size_t i=0,j=CallChange_.size();i<j;i+=3)
			{
				DWORD index=CallChange_[i+1];
				ParamLocked=index;
#ifdef FONLINE_CLIENT
				if(Script::PrepareContext(CallChange_[i],CALL_FUNC_STR,GetInfo()))
				{
					Script::SetArgObject(this);
					Script::SetArgDword(index-(ParametersOffset[index]?ParametersMin[index]:0));
					Script::SetArgDword(CallChange_[i+2]);
					Script::RunPrepared();
				}
#endif
				ParamLocked=-1;
			}
		}
	}
}

int CritterCl::GetReputation(DWORD index)
{
	return Params[index]!=0x80000000?Params[index]:0;
}

int CritterCl::GetAc()
{
	int val=Params[ST_ARMOR_CLASS]+Params[ST_ARMOR_CLASS_EXT]+GetAgility()+Params[ST_TURN_BASED_AC];
	Item* armor=ItemSlotArmor;
	if(!armor->GetId() || !armor->IsArmor()) armor=NULL;
	int wearProc=(armor?100-armor->GetWearProc():0);
	if(armor) val+=armor->Proto->Armor.AC*wearProc/100;
	return CLAMP(val,0,90);
}

int CritterCl::GetDamageResistance(int dmg_type)
{
	Item* armor=ItemSlotArmor;
	if(!armor->GetId() || !armor->IsArmor()) armor=NULL;
	int wearProc=(armor?100-armor->GetWearProc():0);

	int val=0;
	switch(dmg_type)
	{
	case DAMAGE_TYPE_UNCALLED: break;
	case DAMAGE_TYPE_NORMAL:   val=Params[ST_NORMAL_RESIST] +Params[ST_NORMAL_RESIST_EXT] +(armor?armor->Proto->Armor.DRNormal*wearProc/100:0); break;
	case DAMAGE_TYPE_LASER:    val=Params[ST_LASER_RESIST]  +Params[ST_LASER_RESIST_EXT]  +(armor?armor->Proto->Armor.DRLaser*wearProc/100:0); break;
	case DAMAGE_TYPE_FIRE:     val=Params[ST_FIRE_RESIST]   +Params[ST_FIRE_RESIST_EXT]   +(armor?armor->Proto->Armor.DRFire*wearProc/100:0); break;
	case DAMAGE_TYPE_PLASMA:   val=Params[ST_PLASMA_RESIST] +Params[ST_PLASMA_RESIST_EXT] +(armor?armor->Proto->Armor.DRPlasma*wearProc/100:0); break;
	case DAMAGE_TYPE_ELECTR:   val=Params[ST_ELECTRO_RESIST]+Params[ST_ELECTRO_RESIST_EXT]+(armor?armor->Proto->Armor.DRElectr*wearProc/100:0); break;
	case DAMAGE_TYPE_EMP:      val=Params[ST_EMP_RESIST]    +Params[ST_EMP_RESIST_EXT]    +(armor?armor->Proto->Armor.DREmp:0); break;
	case DAMAGE_TYPE_EXPLODE:  val=Params[ST_EXPLODE_RESIST]+Params[ST_EXPLODE_RESIST_EXT]+(armor?armor->Proto->Armor.DRExplode*wearProc/100:0); break;
	default: break;
	}

	if(dmg_type==DAMAGE_TYPE_EMP) return CLAMP(val,0,999);
	return CLAMP(val,0,90);
}

int CritterCl::GetDamageThreshold(int dmg_type)
{
	Item* armor=ItemSlotArmor;
	if(!armor->GetId() || !armor->IsArmor()) armor=NULL;
	int wearProc=(armor?100-armor->GetWearProc():0);

	int val=0;
	switch(dmg_type)
	{
	case DAMAGE_TYPE_UNCALLED: break;
	case DAMAGE_TYPE_NORMAL:   val=Params[ST_NORMAL_ABSORB] +Params[ST_NORMAL_ABSORB_EXT] +(armor?armor->Proto->Armor.DTNormal*wearProc/100:0); break;
	case DAMAGE_TYPE_LASER:    val=Params[ST_LASER_ABSORB]  +Params[ST_LASER_ABSORB_EXT]  +(armor?armor->Proto->Armor.DTLaser*wearProc/100:0); break;
	case DAMAGE_TYPE_FIRE:     val=Params[ST_FIRE_ABSORB]   +Params[ST_FIRE_ABSORB_EXT]   +(armor?armor->Proto->Armor.DTFire*wearProc/100:0); break;
	case DAMAGE_TYPE_PLASMA:   val=Params[ST_PLASMA_ABSORB] +Params[ST_PLASMA_ABSORB_EXT] +(armor?armor->Proto->Armor.DTPlasma*wearProc/100:0); break;
	case DAMAGE_TYPE_ELECTR:   val=Params[ST_ELECTRO_ABSORB]+Params[ST_ELECTRO_ABSORB_EXT]+(armor?armor->Proto->Armor.DTElectr*wearProc/100:0); break;
	case DAMAGE_TYPE_EMP:      val=Params[ST_EMP_ABSORB]    +Params[ST_EMP_ABSORB_EXT]    +(armor?armor->Proto->Armor.DTEmp:0); break;
	case DAMAGE_TYPE_EXPLODE:  val=Params[ST_EXPLODE_ABSORB]+Params[ST_EXPLODE_ABSORB_EXT]+(armor?armor->Proto->Armor.DTExplode*wearProc/100:0); break;
	default: break;
	}

	return CLAMP(val,0,999);
}

int CritterCl::GetNightPersonBonus()
{
	if(GameOpt.Hour<6 || GameOpt.Hour>18) return 1;
	if(GameOpt.Hour==6 && GameOpt.Minute==0) return 1;
	if(GameOpt.Hour==18 && GameOpt.Minute>0) return 1;
	return -1;
}

void CritterCl::DrawStay(INTRECT r)
{
	if(Timer::FastTick()-staySprTick>500)
	{
		staySprDir++;
		if(staySprDir>5) staySprDir=0;
		staySprTick=Timer::FastTick();
	}

	int dir=(!IsLife()?CrDir:staySprDir);
	DWORD anim1=GetAnim1();
	DWORD anim2=GetAnim2();

	if(!Anim3d)
	{
		DWORD crtype=GetCrType();
		AnyFrames* frm=LoadAnim(crtype,anim1,anim2,dir);
		if(frm)
		{
			DWORD spr_id=(IsLife()?frm->Ind[0]:frm->Ind[frm->CntFrm-1]);
			SprMngr->DrawSpriteSize(spr_id,r.L,r.T,r.W(),r.H(),false,true);
		}
	}
	else if(Anim3dStay)
	{
		Anim3dStay->SetDir(dir);
		Anim3dStay->SetAnimation(anim1,anim2,GetLayers3dData(),IsLife()?0:ANIMATION_STAY|ANIMATION_LAST_FRAME);
		SprMngr->Draw3dSize(FLTRECT(r.L,r.T,r.R,r.B),false,true,Anim3dStay,&FLTRECT(r.L,r.T,r.R,r.B),COLOR_IFACE);
	}
}

const char* CritterCl::GetMoneyStr()
{
	static char money_str[64];
	DWORD money_count=CountItemPid(PID_BOTTLE_CAPS);
	sprintf(money_str,"%u$",money_count);
	return money_str;
}

bool CritterCl::NextRateItem(bool prev)
{
	bool result=false;
	BYTE old_rate=ItemSlotMain->Data.Rate;
	if(!ItemSlotMain->IsWeapon())
	{
		if(ItemSlotMain->IsCanUse() || ItemSlotMain->IsCanUseOnSmth())
			ItemSlotMain->Data.Rate=USE_USE;
		else
			ItemSlotMain->Data.Rate=USE_NONE;
	}
	else
	{
		// Unarmed
		if(!ItemSlotMain->GetId())
		{
			ProtoItem* old_unarmed=ItemSlotMain->Proto;
			ProtoItem* unarmed=ItemSlotMain->Proto;
			BYTE tree=ItemSlotMain->Proto->Weapon.UnarmedTree;
			BYTE priority=ItemSlotMain->Proto->Weapon.UnarmedPriority;
			while(true)
			{
				if(prev)
				{
					if(IsItemAim(SLOT_HAND1) && IsAim())
					{
						SetAim(HIT_LOCATION_NONE);
						break;
					}
					if(!priority)
					{
						// Find prev tree
						if(tree)
						{
							tree--;
						}
						else
						{
							// Find last tree
							for(int i=0;i<200;i++)
							{
								ProtoItem* ua=GetUnarmedItem(i,0);
								if(ua) unarmed=ua;
								else break;
							}
							tree=unarmed->Weapon.UnarmedTree;
						}

						// Find last priority
						for(int i=0;i<200;i++)
						{
							ProtoItem* ua=GetUnarmedItem(tree,i);
							if(ua) unarmed=ua;
							else break;
						}
					}
					else
					{
						priority--;
						unarmed=GetUnarmedItem(tree,priority);
					}
					ItemSlotMain->Init(unarmed);
					if(IsItemAim(SLOT_HAND1) && !IsPerk(TRAIT_FAST_SHOT) && CritType::IsCanAim(GetCrType()))
					{
						SetAim(HIT_LOCATION_TORSO);
						break;
					}
				}
				else
				{
					if(IsItemAim(SLOT_HAND1) && !IsAim() && !IsPerk(TRAIT_FAST_SHOT) && CritType::IsCanAim(GetCrType()))
					{
						SetAim(HIT_LOCATION_TORSO);
						break;
					}
					priority++;
					unarmed=GetUnarmedItem(tree,priority);
					if(!unarmed)
					{
						// Find next tree
						tree++;
						unarmed=GetUnarmedItem(tree,0);
						if(!unarmed) unarmed=GetUnarmedItem(0,0);
					}
					ItemSlotMain->Init(unarmed);
					SetAim(HIT_LOCATION_NONE);
				}
				break;
			}
			result=(old_unarmed!=unarmed);
		}
		// Armed
		else
		{
			while(true)
			{
				if(prev)
				{
					if(IsItemAim(SLOT_HAND1) && IsAim())
					{
						SetAim(HIT_LOCATION_NONE);
						break;
					}
					if(!ItemSlotMain->Data.Rate) ItemSlotMain->Data.Rate=(ItemSlotMain->IsCanUseOnSmth()?USE_USE:USE_RELOAD);
					else ItemSlotMain->Data.Rate--;
					if(IsItemAim(SLOT_HAND1) && !IsPerk(TRAIT_FAST_SHOT) && CritType::IsCanAim(GetCrType()))
					{
						SetAim(HIT_LOCATION_TORSO);
						break;
					}
				}
				else
				{
					if(IsItemAim(SLOT_HAND1) && !IsAim() && !IsPerk(TRAIT_FAST_SHOT) && CritType::IsCanAim(GetCrType()))
					{
						SetAim(HIT_LOCATION_TORSO);
						break;
					}
					ItemSlotMain->Data.Rate++;
					SetAim(HIT_LOCATION_NONE);
				}

				switch(GetUse())
				{
				case USE_PRIMARY: if(ItemSlotMain->Proto->Weapon.CountAttack & 0x1) break; continue;
				case USE_SECONDARY: if(ItemSlotMain->Proto->Weapon.CountAttack & 0x2) break; continue;
				case USE_THIRD: if(ItemSlotMain->Proto->Weapon.CountAttack & 0x4) break; continue;
				case USE_RELOAD: if(ItemSlotMain->Proto->Weapon.VolHolder) break; continue;
				case USE_USE: if(ItemSlotMain->IsCanUseOnSmth()) break; continue;
				default: ItemSlotMain->Data.Rate=USE_PRIMARY; break;
				}
				break;
			}
		}
	}
	ItemSlotMain->SetRate(ItemSlotMain->Data.Rate);
	return ItemSlotMain->Data.Rate!=old_rate || result;
}

void CritterCl::SetAim(BYTE hit_location)
{
	UNSETFLAG(ItemSlotMain->Data.Rate,0xF0);
	if(!IsItemAim(SLOT_HAND1)) return;
	SETFLAG(ItemSlotMain->Data.Rate,hit_location<<4);
}

DWORD CritterCl::GetUseApCost(Item* item, BYTE rate)
{
	int use=(rate&0xF);
	int aim=(rate>>4);

	if(use==USE_USE) return GetApCostUseItem();
	else if(use==USE_RELOAD) return GetApCostReload()-(item->IsWeapon() && item->WeapIsFastReload()?1:0);
	else if(use>=USE_PRIMARY && use<=USE_THIRD && item->IsWeapon())
	{
		int ap_cost=item->Proto->Weapon.Time[use];
		if(aim) ap_cost+=GameAimApCost(aim);
		if(IsPerk(PE_BONUS_HTH_ATTACKS) && item->WeapIsHtHAttack(use)) ap_cost--;
		if(IsPerk(PE_BONUS_RATE_OF_FIRE) && item->WeapIsRangedAttack(use)) ap_cost--;
		if(IsPerk(TRAIT_FAST_SHOT) && !item->WeapIsHtHAttack(use)) ap_cost--;
		return ap_cost>0?ap_cost:0;
	}
	return 0;
}

ProtoItem* CritterCl::GetUnarmedItem(BYTE tree, BYTE priority)
{
	ProtoItem* best_unarmed=NULL;
	for(int i=ITEM_SLOT_BEGIN;i<=ITEM_SLOT_END;i++)
	{
		ProtoItem* unarmed=ItemMngr.GetProtoItem(i);
		if(!unarmed || !unarmed->IsWeapon() || !unarmed->Weapon.IsUnarmed) continue;
		if(unarmed->Weapon.UnarmedTree!=tree || unarmed->Weapon.UnarmedPriority!=priority) continue;
		if(GetParam(ST_STRENGTH)<unarmed->Weapon.MinSt || GetParam(ST_AGILITY)<unarmed->Weapon.UnarmedMinAgility) break;
		if(GetParam(ST_LEVEL)<unarmed->Weapon.UnarmedMinLevel || GetSkill(SK_UNARMED)<unarmed->Weapon.UnarmedMinUnarmed) break;
		best_unarmed=unarmed;
	}
	return best_unarmed;
}

Item* CritterCl::GetAmmoAvialble(Item* weap)
{
	Item* ammo=GetItemByPid(weap->Data.TechInfo.AmmoPid);
	if(!ammo && weap->WeapIsEmpty()) ammo=GetItemByPid(weap->Proto->Weapon.DefAmmo);
	if(!ammo && weap->WeapIsEmpty()) ammo=GetAmmo(weap->Proto->Weapon.Caliber);
	return ammo;
}

DWORD CritterCl::GetTimeout(int timeout)
{
	DWORD to=(Params[timeout]>GameOpt.FullMinute?Params[timeout]-GameOpt.FullMinute:0);
	if(timeout==TO_REMOVE_FROM_GAME && to<(Params[TO_BATTLE]>GameOpt.FullMinute?Params[TO_BATTLE]-GameOpt.FullMinute:0)) to=Params[TO_BATTLE];
	DWORD sec=to*60/(GameOpt.TimeMultiplier?GameOpt.TimeMultiplier:1);
	DWORD sec_sub=(Timer::FastTick()-GameOpt.FullMinuteTick)/1000;
	if(sec_sub>sec) sec=0;
	else sec-=sec_sub;
	if(timeout==TO_REMOVE_FROM_GAME && sec<CLIENT_KICK_TIME/1000) sec=CLIENT_KICK_TIME/1000;
	return sec;
}

bool CritterCl::IsLastHexes()
{
	return !LastHexX.empty() && !LastHexY.empty();
}

void CritterCl::FixLastHexes()
{
	if(IsLastHexes() && LastHexX[LastHexX.size()-1]==HexX && LastHexY[LastHexY.size()-1]==HexY) return;
	LastHexX.push_back(HexX);
	LastHexY.push_back(HexY);
}

WORD CritterCl::PopLastHexX()
{
	WORD hx=LastHexX[LastHexX.size()-1];
	LastHexX.pop_back();
	return hx;
}

WORD CritterCl::PopLastHexY()
{
	WORD hy=LastHexY[LastHexY.size()-1];
	LastHexY.pop_back();
	return hy;
}

void CritterCl::Move(BYTE dir)
{
	if(dir>5 || !CritType::IsCanRotate(GetCrType())) dir=0;
	CrDir=dir;

	DWORD crtype=GetCrType();
	DWORD time_move=0;

	if(!IsRunning)
	{
		time_move=CritType::GetTimeWalk(GetCrType());
		if(!time_move) time_move=400;
		//if(IsTurnBased()) time_move/=2;
	}
	else
	{
		time_move=CritType::GetTimeRun(GetCrType());
		if(!time_move) time_move=200;
	}
	TickStart(IsDmgTwoLeg()?GameOpt.Breaktime:time_move);
	animStartTick=Timer::FastTick();

	if(!Anim3d)
	{
		DWORD anim1=(IsRunning?ANIM1_UNARMED:GetAnim1());
		DWORD anim2=(IsRunning?ANIM2_2D_RUN:ANIM2_2D_WALK);
		AnyFrames* anim=LoadAnim(crtype,anim1,anim2,dir);
		int step=0;
		curSpr=endSpr;

		if(!IsRunning)
		{
			int s0=CritType::GetWalkFrmCnt(crtype,0);
			int s1=CritType::GetWalkFrmCnt(crtype,1);
			int s2=CritType::GetWalkFrmCnt(crtype,2);
			int s3=CritType::GetWalkFrmCnt(crtype,3);

			if(curSpr==s0-1 && s1)
			{
				begSpr=s0;
				endSpr=s1-1;
				step=2;
			}
			else if(curSpr==s1-1 && s2)
			{
				begSpr=s1;
				endSpr=s2-1;
				step=3;
			}
			else if(curSpr==s2-1 && s3)
			{
				begSpr=s2;
				endSpr=s3-1;
				step=4;
			}
			else
			{
				begSpr=0;
				endSpr=s0-1;
				step=1;
			}
		}
		else
		{
			switch(curSpr)
			{
			default:
			case 0:
				begSpr=0;
				endSpr=1;
				step=1;
				break;
			case 1:
				begSpr=2;
				endSpr=3;
				step=2;
				break;
			case 3:
				begSpr=4;
				endSpr=6;
				step=3;
				break;
			case 6:
				begSpr=7;
				endSpr=anim->CntFrm-1;
				step=4;
				break;
			}
		}
		curSpr=begSpr;

		SetOffs(0,0,true);
		for(int i=0;i<=begSpr;++i)
			ChangeOffs(anim->NextX[i],anim->NextY[i],true);

		for(int i=0;i<step;i++)
		{
			switch(dir)
			{
			case 0: ChangeOffs(-16,12,true); break;
			case 1: ChangeOffs(-32,0,true); break;
			case 2: ChangeOffs(-16,-12,true); break;
			case 3: ChangeOffs(16,-12,true); break;
			case 4: ChangeOffs(32,0,true); break;
			case 5: ChangeOffs(16,12,true); break;
			default: break;
			}
		}

		if(curSpr>=anim->CntFrm) curSpr=0;
		SprId=anim->Ind[curSpr];
		ClearAnim();
		animSequence.push_back(CritterAnim(anim,time_move,begSpr,anim->CntFrm-1,true,crtype,anim1,anim2,NULL));
	}
	else
	{
		BYTE anim1=GetAnim1();
		BYTE anim2=(IsRunning?ANIM2_3D_RUN:ANIM2_3D_WALK);
		if(IsDmgLeg()) anim2=ANIM2_3D_LIMP;

		Anim3d->SetDir(dir);

		ClearAnim();
		animSequence.push_back(CritterAnim(NULL,time_move,dir+1,0,true,crtype,anim1,anim2,NULL));
		NextAnim(false);

		switch(dir)
		{
		case 0: SetOffs(-16,12,true); break;
		case 1: SetOffs(-32,0,true); break;
		case 2: SetOffs(-16,-12,true); break;
		case 3: SetOffs(16,-12,true); break;
		case 4: SetOffs(32,0,true); break;
		case 5: SetOffs(16,12,true); break;
		default: break;
		}
	}
}

void CritterCl::Action(int action, int action_ext, Item* item, bool local_call /* = true */)
{
#ifdef FONLINE_CLIENT
	if(Script::PrepareContext(ClientFunctions.CritterAction,CALL_FUNC_STR,"CritterAction"))
	{
		if(item) item=item->Clone();

		Script::SetArgBool(local_call);
		Script::SetArgObject(this);
		Script::SetArgDword(action);
		Script::SetArgDword(action_ext);
		Script::SetArgObject(item);
		Script::RunPrepared();

		SAFEREL(item);
	}
#endif

	switch(action)
	{
	case ACTION_KNOCKOUT:
		if(action_ext==0)
		{
			Cond=COND_KNOCKOUT;
			CondExt=COND_KNOCKOUT_FRONT;
		}
		else
		{
			Cond=COND_KNOCKOUT;
			CondExt=COND_KNOCKOUT_BACK;
		}
		break;
	case ACTION_STANDUP:
		Cond=COND_LIFE;
		CondExt=COND_LIFE_NONE;
		break;
	case ACTION_DEAD:
		{
			Cond=COND_DEAD;
			CondExt=action_ext;
			CritterAnim* anim=GetCurAnim();
			needReSet=true;
			reSetTick=Timer::FastTick()+(anim && anim->Anim?anim->Anim->Ticks:1000);
		}
		break;
	case ACTION_CONNECT:
		UNSETFLAG(Flags,FCRIT_DISCONNECT);
		break;
	case ACTION_DISCONNECT:
		SETFLAG(Flags,FCRIT_DISCONNECT);
		break;
	case ACTION_RESPAWN:
		Cond=COND_LIFE;
		CondExt=COND_LIFE_NONE;
		Alpha=0;
		SetFade(true);
		AnimateStay();
		needReSet=true;
		reSetTick=Timer::FastTick(); // Fast
		break;
	case ACTION_REFRESH:
		if(!IsAnim()) AnimateStay();
		break;
	default:
		break;
	}
}

void CritterCl::NextAnim(bool erase_front)
{
	if(animSequence.empty()) return;
	if(erase_front)
	{
		SAFEREL((*animSequence.begin()).ActiveItem);
		animSequence.erase(animSequence.begin());
	}
	if(animSequence.empty()) return;

	CritterAnim* anim=&animSequence[0];
	animStartTick=Timer::FastTick();
	SetOffs(0,0,anim->MoveText);

	ProcessAnim(!Anim3d,anim->IndAnim1,anim->IndAnim2,anim->ActiveItem);

	if(!Anim3d)
	{
		begSpr=anim->BeginFrm;
		endSpr=anim->EndFrm;
		curSpr=begSpr;
		if(curSpr>=anim->Anim->CntFrm) curSpr=0;
		SprId=anim->Anim->Ind[curSpr];

		for(int i=0;i<=begSpr;++i)
			ChangeOffs(anim->Anim->NextX[i],anim->Anim->NextY[i],anim->MoveText);
	}
	else	
	{
		Anim3d->SetAnimation(anim->IndAnim1,anim->IndAnim2,GetLayers3dData(),anim->BeginFrm?0:ANIMATION_ONE_TIME);
	}

#ifdef FONLINE_CLIENT
	SndMngr.PlayAction(CritType::GetName(anim->IndCrType),anim->IndAnim1,anim->IndAnim2);
#endif
}

void CritterCl::Animate(DWORD anim1, DWORD anim2, Item* item)
{
	DWORD crtype=GetCrType();
	BYTE dir=GetDir();
	int num_frame=0;
	if(!anim1) anim1=GetAnim1();

	if(item) item=item->Clone();

	if(!Anim3d)
	{
		AnyFrames* anim=LoadAnim(crtype,anim1,anim2,dir);
		if(num_frame>=anim->CntFrm) num_frame=0;

		bool move_text=((anim1==ANIM1_DEAD || anim1==ANIM1_KNOCKOUT/* || anim1==ANIM1_RIP*/) ||
			(anim2!=ANIM2_2D_SHOW && anim2!=ANIM2_2D_HIDE && anim2!=ANIM2_2D_PREPARE_WEAPON && anim2!=ANIM2_2D_TURNOFF_WEAPON &&
			anim2!=ANIM2_2D_DODGE_WEAPON && anim2!=ANIM2_2D_DODGE_EMPTY && anim2!=ANIM2_2D_USE && anim2!=ANIM2_2D_PICKUP &&
			anim2!=ANIM2_2D_DAMAGE_FRONT && anim2!=ANIM2_2D_DAMAGE_BACK && anim2!=ANIM2_2D_STAY));

		animSequence.push_back(CritterAnim(anim,anim->Ticks,num_frame,anim->CntFrm-1,move_text,crtype,anim1,anim2,item));
	}
	else
	{
		animSequence.push_back(CritterAnim(NULL,0,0,0,true,crtype,anim1,anim2,item));
	}

	if(animSequence.size()==1) NextAnim(false);
}

void CritterCl::AnimateStay()
{
	BYTE num_frame=LAST_FRAME;
	if(Cond==COND_LIFE && CondExt==COND_LIFE_NONE) num_frame=FIRST_FRAME;

	DWORD crtype=GetCrType();
	DWORD anim1=GetAnim1();
	DWORD anim2=GetAnim2();

	ProcessAnim(!Anim3d,anim1,anim2,NULL);

	if(!Anim3d)
	{
		AnyFrames* frm=LoadAnim(crtype,anim1,anim2,CrDir);
		if(num_frame==LAST_FRAME) num_frame=frm->CntFrm-1;
		else if(num_frame==FIRST_FRAME) num_frame=0;

		if(num_frame>=frm->CntFrm) num_frame=0;
		SprId=frm->Ind[num_frame];

		SetOffs(0,0,true);
		for(int i=0;i<=num_frame;++i)
			ChangeOffs(frm->NextX[i],frm->NextY[i],true);

		curSpr=0;
		begSpr=0;
		endSpr=0;
	}
	else
	{
		if(Cond==COND_LIFE || Cond==COND_KNOCKOUT) Anim3d->SetAnimation(anim1,anim2,GetLayers3dData(),GameOpt.Animation3dFPS?ANIMATION_STAY:0);
		else Anim3d->SetAnimation(anim1,anim2,GetLayers3dData(),ANIMATION_STAY|ANIMATION_LAST_FRAME);
	}
}

AnyFrames* CritterCl::GetAnimSpr(DWORD crtype, DWORD anim1, DWORD anim2, BYTE dir)
{
	AnimMapIt it=crAnim.find(ANIM_MAP_ID(crtype,anim1,anim2,dir));
	return it!=crAnim.end()?(*it).second:NULL;
}

bool CritterCl::LoadAnimSpr(DWORD crtype, DWORD anim1, DWORD anim2, BYTE dir)
{
	AnimMapIt it=crAnim.find(ANIM_MAP_ID(crtype,anim1,anim2,dir));
	if(it!=crAnim.end()) return (*it).second!=NULL;

	AnyFrames* frames=ResMngr.GetCritAnim(crtype,anim1,anim2,dir);
	crAnim.insert(AnimMapVal(ANIM_MAP_ID(crtype,anim1,anim2,dir),frames));
	if(!frames) return false;

	if(anim2==ANIM2_2D_WALK) frames->Ticks=CritType::GetTimeWalk(crtype);
	if(anim2==ANIM2_2D_RUN) frames->Ticks=CritType::GetTimeRun(crtype);

//*****************************************
#define LOADSPR_ADDOFFS(a1,a2) \
	do{\
		if(!LoadAnimSpr(crtype,a1,a2,dir)) break;\
		AnyFrames* stay_frm=GetAnimSpr(crtype,a1,a2,dir);\
		AnyFrames* frm=frames;\
		SpriteInfo* stay_si=SprMngr->GetSpriteInfo(stay_frm->Ind[0]);\
		if(!stay_si) break;\
		for(int i=0;i<frm->CntFrm;i++)\
		{\
			SpriteInfo* si=SprMngr->GetSpriteInfo(frm->Ind[i]);\
			if(!si) continue;\
			si->OffsX+=stay_si->OffsX;\
			si->OffsY+=stay_si->OffsY;\
		}\
	}while(0)
#define LOADSPR_ADDOFFS_NEXT(a1,a2) \
	do{\
		if(!LoadAnimSpr(crtype,a1,a2,dir)) break;\
		AnyFrames* stay_frm=GetAnimSpr(crtype,a1,a2,dir);\
		AnyFrames* frm=frames;\
		SpriteInfo* stay_si=SprMngr->GetSpriteInfo(stay_frm->Ind[0]);\
		if(!stay_si) break;\
		short ox=0;\
		short oy=0;\
		for(int i=0;i<stay_frm->CntFrm;i++)\
		{\
			ox+=stay_frm->NextX[i];\
			oy+=stay_frm->NextY[i];\
		}\
		for(int i=0;i<frm->CntFrm;i++)\
		{\
			SpriteInfo* si=SprMngr->GetSpriteInfo(frm->Ind[i]);\
			if(!si) continue;\
			si->OffsX+=ox;\
			si->OffsY+=oy;\
		}\
	}while(0)
//*****************************************

	if(anim1==ANIM1_AIM) return true;

	// Empty offsets
	if(anim1==ANIM1_UNARMED)
	{
		if(anim2==ANIM2_2D_STAY || anim2==ANIM2_2D_WALK || anim2==ANIM2_2D_RUN) return true;
		LOADSPR_ADDOFFS(ANIM1_UNARMED,ANIM2_2D_STAY);
	}

	// Weapon offsets
	if(anim1>=ANIM1_KNIFE && anim1<=ANIM1_ROCKET_LAUNCHER)
	{
		if(anim2==ANIM2_2D_SHOW) LOADSPR_ADDOFFS(ANIM1_UNARMED,ANIM2_2D_STAY);
		else if(anim2==ANIM2_2D_WALK) return true;
		else if(anim2==ANIM2_2D_STAY)
		{
			LOADSPR_ADDOFFS(ANIM1_UNARMED,ANIM2_2D_STAY);
			LOADSPR_ADDOFFS_NEXT(anim1,ANIM2_2D_SHOW);
		}
		else if(anim2==ANIM2_2D_SHOOT || anim2==ANIM2_2D_BURST || anim2==ANIM2_2D_FLAME)
		{
			LOADSPR_ADDOFFS(anim1,ANIM2_2D_PREPARE_WEAPON);
			LOADSPR_ADDOFFS_NEXT(anim1,ANIM2_2D_PREPARE_WEAPON);
		}
		else if(anim2==ANIM2_2D_TURNOFF_WEAPON)
		{
			if(anim1==ANIM1_MINIGUN)
			{
				LOADSPR_ADDOFFS(anim1,ANIM2_2D_BURST);
				LOADSPR_ADDOFFS_NEXT(anim1,ANIM2_2D_BURST);
			}
			else
			{
				LOADSPR_ADDOFFS(anim1,ANIM2_2D_SHOOT);
				LOADSPR_ADDOFFS_NEXT(anim1,ANIM2_2D_SHOOT);
			}
		}
		else LOADSPR_ADDOFFS(anim1,ANIM2_2D_STAY);
	}

	// Dead & Ko offsets
	if(anim1==ANIM1_DEAD)
	{
		if(anim2==ANIM2_2D_DEAD_FRONT2)
		{
			LOADSPR_ADDOFFS(ANIM1_DEAD,ANIM2_2D_DEAD_FRONT);
			LOADSPR_ADDOFFS_NEXT(ANIM1_DEAD,ANIM2_2D_DEAD_FRONT);
		}
		else if(anim2==ANIM2_2D_DEAD_BACK2)
		{
			LOADSPR_ADDOFFS(ANIM1_DEAD,ANIM2_2D_DEAD_BACK);
			LOADSPR_ADDOFFS_NEXT(ANIM1_DEAD,ANIM2_2D_DEAD_BACK);
		}
		else
		{
			LOADSPR_ADDOFFS(ANIM1_UNARMED,ANIM2_2D_STAY);
		}
	}

	// Ko rise offsets
	if(anim1==ANIM1_KNOCKOUT)
	{
		BYTE anim2_=ANIM2_2D_KNOCK_FRONT;
		if(anim2==ANIM2_2D_STANDUP_BACK) anim2_=ANIM2_2D_KNOCK_BACK;
		LOADSPR_ADDOFFS(ANIM1_DEAD,anim2_);
		LOADSPR_ADDOFFS_NEXT(ANIM1_DEAD,anim2_);
	}

// 	// Rip offsets
// 	if(anim1==ANIM1_RIP)
// 	{
// 		LOADSPR_ADDOFFS(ANIM1_DEAD,anim2);
// 		LOADSPR_ADDOFFS_NEXT(ANIM1_DEAD,anim2);
// 	}

	return true;
}

AnyFrames* CritterCl::LoadAnim(DWORD& crtype, DWORD& anim1, DWORD& anim2, BYTE dir)
{
	// Check
	if(crtype>=MAX_CRIT_TYPES || anim1>ABC_SIZE || anim2>ABC_SIZE || dir>5) return CritterCl::DefaultAnim;
	if(!CritType::IsCanRotate(crtype)) dir=0;

	bool with_alias=false;
	if(anim1==ANIM1_UNARMED)
	{
		switch(anim2)
		{
		case ANIM2_2D_THROW_WEAPON: anim2=ANIM2_2D_THROW_EMPTY; break;
		case ANIM2_2D_DODGE_WEAPON: anim2=ANIM2_2D_DODGE_EMPTY; break;
		default: break;
		}
	}
	else if(anim1==ANIM1_DEAD) // Also Knockout
	{
		switch(anim2)
		{
		case ANIM2_2D_DEAD_PULSE:
		case ANIM2_2D_DEAD_PULSE_DUST:
		case ANIM2_2D_DEAD_BURN:
		case ANIM2_2D_DEAD_BURN2:
		case ANIM2_2D_DEAD_BURN_RUN: with_alias=true; break;
		default: break;
		}
	}

	// Base
	if(!LoadAnimSpr(crtype,anim1,anim2,dir))
	{
		// Alias
		if(with_alias && LoadAnimSpr(CritType::GetAlias(crtype),anim1,anim2,dir))
		{
			crtype=CritType::GetAlias(crtype);
			return GetAnimSpr(crtype,anim1,anim2,dir);
		}

		// Default animations
		if(anim1==ANIM1_UNARMED)
		{
			switch(anim2)
			{
			case ANIM2_2D_STAY: return CritterCl::DefaultAnim;
			case ANIM2_2D_RUN: anim2=ANIM2_2D_WALK; break;
			case ANIM2_2D_KICK: anim2=ANIM2_2D_PUNCH; break;
			default: anim2=ANIM2_2D_STAY; break;
			}
		}
		else if(anim1==ANIM1_DEAD) // Also Knockout
		{
			switch(anim2)
			{
			case ANIM2_2D_DEAD_FRONT: anim1=ANIM1_UNARMED; anim2=ANIM2_2D_USE; break;
			case ANIM2_2D_DEAD_BACK2: anim2=ANIM2_2D_DEAD_FRONT2; break;
			case ANIM2_2D_DEAD_PULSE_DUST: anim2=ANIM2_2D_DEAD_PULSE; break;
			case ANIM2_2D_DEAD_BURN2: anim2=ANIM2_2D_DEAD_BURN; break;
			case ANIM2_2D_DEAD_BURN_RUN: anim2=ANIM2_2D_DEAD_BURN2; break;
			default:
				anim1=ANIM1_DEAD;
				anim2=ANIM2_2D_DEAD_FRONT;
				break;
			}
		}
		else
		{
			anim1=ANIM1_UNARMED;
			anim2=ANIM2_2D_USE;
		}
		return LoadAnim(crtype,anim1,anim2,dir);
	}
	return GetAnimSpr(crtype,anim1,anim2,dir);
}

void CritterCl::FreeAnimations()
{
	ResMngr.FreeResources(RES_CRITTERS);
	for(AnimMapIt it=crAnim.begin(),end=crAnim.end();it!=end;++it) SAFEDEL((*it).second);
	crAnim.clear();
}

bool CritterCl::IsWalkAnim()
{
	if(animSequence.size())
	{
		int anim2=animSequence[0].IndAnim2;
		if(!Anim3d) return anim2==ANIM2_2D_WALK || anim2==ANIM2_2D_RUN;
		else return anim2==ANIM2_3D_WALK || anim2==ANIM2_3D_RUN || anim2==ANIM2_3D_LIMP || anim2==ANIM2_3D_PANIC_RUN;
	}
	return false;
}

void CritterCl::ClearAnim()
{
	for(size_t i=0,j=animSequence.size();i<j;i++) SAFEREL(animSequence[i].ActiveItem);
	animSequence.clear();
}

Item* CritterCl::GetRadio()
{
	if(ItemSlotMain->IsRadio()) return ItemSlotMain;
	if(ItemSlotExt->IsRadio()) return ItemSlotExt;
	return NULL;
}

bool CritterCl::IsHaveLightSources()
{
	for(ItemPtrMapIt it=InvItems.begin(),end=InvItems.end();it!=end;++it)
	{
		Item* item=(*it).second;
		if(item->IsLight()) return true;
	}
	return false;
}

DWORD CritterCl::GetCrType()
{
	return BaseType;
}

DWORD CritterCl::GetCrTypeAlias()
{
	return CritType::GetAlias(GetCrType());
}

BYTE CritterCl::GetAnim1()
{
	if(!Anim3d)
	{
		switch(Cond)
		{
		case COND_LIFE: return ItemSlotMain->IsWeapon()?ItemSlotMain->Proto->Weapon.Anim1:ANIM1_UNARMED;
		case COND_KNOCKOUT: return ANIM1_DEAD;
		case COND_DEAD: return ANIM1_DEAD;
		}
	}
	else
	{
		return ItemSlotMain->IsWeapon()?ItemSlotMain->Proto->Weapon.Anim1:ANIM1_UNARMED;
	}
	return ANIM1_UNARMED;
}

BYTE CritterCl::GetAnim2()
{
	if(!Anim3d)
	{
		switch(Cond)
		{
		case COND_LIFE: return ANIM2_2D_STAY;
		case COND_KNOCKOUT:
			switch(CondExt)
			{
			default:
			case COND_KNOCKOUT_FRONT: return ANIM2_2D_KNOCK_FRONT;
			case COND_KNOCKOUT_BACK: return ANIM2_2D_KNOCK_BACK;
			}
		case COND_DEAD:
			switch(CondExt)
			{
			default:
			case COND_DEAD_FRONT: return ANIM2_2D_DEAD_FRONT2;
			case COND_DEAD_BACK: return ANIM2_2D_DEAD_BACK2;
			case COND_DEAD_BLOODY_SINGLE: return ANIM2_2D_DEAD_BLOODY_SINGLE;
			case COND_DEAD_BLOODY_BURST: return ANIM2_2D_DEAD_BLOODY_BURST;
			case COND_DEAD_BURST: return ANIM2_2D_DEAD_BURST;
			case COND_DEAD_PULSE: return ANIM2_2D_DEAD_PULSE;
			case COND_DEAD_PULSE_DUST: return ANIM2_2D_DEAD_PULSE_DUST;
			case COND_DEAD_LASER: return ANIM2_2D_DEAD_LASER;
			case COND_DEAD_EXPLODE: return ANIM2_2D_DEAD_EXPLODE;
			case COND_DEAD_FUSED: return ANIM2_2D_DEAD_FUSED;
			case COND_DEAD_BURN: return ANIM2_2D_DEAD_BURN;
			case COND_DEAD_BURN2: return ANIM2_2D_DEAD_BURN2;
			case COND_DEAD_BURN_RUN: return ANIM2_2D_DEAD_BURN2;
			}
		default: break;
		}
	}
	else
	{
		switch(Cond)
		{
		case COND_LIFE: return IsCombatMode()?ANIM2_3D_IDLE_COMBAT:ANIM2_3D_IDLE;
		case COND_KNOCKOUT:
			switch(CondExt)
			{
			default:
			case COND_KNOCKOUT_FRONT: return ANIM2_3D_IDLE_PRONE_FRONT;
			case COND_KNOCKOUT_BACK: return ANIM2_3D_IDLE_PRONE_BACK;
			}
		case COND_DEAD:
			switch(CondExt)
			{
			default:
			case COND_DEAD_FRONT: return ANIM2_3D_DEAD_PRONE_FRONT;
			case COND_DEAD_BACK: return ANIM2_3D_DEAD_PRONE_BACK;
			case COND_DEAD_BLOODY_SINGLE: return ANIM2_3D_DEAD_BLOODY_SINGLE;
			case COND_DEAD_BLOODY_BURST: return ANIM2_3D_DEAD_BLOODY_BURST;
			case COND_DEAD_BURST: return ANIM2_3D_DEAD_BURST;
			case COND_DEAD_PULSE: return ANIM2_3D_DEAD_PULSE;
			case COND_DEAD_PULSE_DUST: return ANIM2_3D_DEAD_PULSE_DUST;
			case COND_DEAD_LASER: return ANIM2_3D_DEAD_LASER;
			case COND_DEAD_EXPLODE: return ANIM2_3D_DEAD_EXPLODE;
			case COND_DEAD_FUSED: return ANIM2_3D_DEAD_FUSED;
			case COND_DEAD_BURN: return ANIM2_3D_DEAD_BURN;
			case COND_DEAD_BURN2: return ANIM2_3D_DEAD_BURN;
			case COND_DEAD_BURN_RUN: return ANIM2_3D_DEAD_BURN_RUN;
			}
		default: break;
		}
	}
	return ANIM2_2D_STAY;
}

void CritterCl::ProcessAnim(bool is2d, DWORD anim1, DWORD anim2, Item* item)
{
#ifdef FONLINE_CLIENT
	if(Script::PrepareContext(is2d?ClientFunctions.Animation2dProcess:ClientFunctions.Animation3dProcess,CALL_FUNC_STR,"AnimationProcess"))
	{
		Script::SetArgObject(this);
		Script::SetArgDword(anim1);
		Script::SetArgDword(anim2);
		Script::SetArgObject(item);
		Script::RunPrepared();
	}
#endif
}

int* CritterCl::GetLayers3dData()
{
#ifdef FONLINE_CLIENT
	static int layers[LAYERS3D_COUNT];
	asIScriptArray* arr=(asIScriptArray*)Layers3d;
	memcpy(layers,arr->GetElementPointer(0),sizeof(layers));
	return layers;
#endif

	return (int*)Layers3d;
}

bool CritterCl::IsAnimAviable(DWORD anim1, DWORD anim2)
{
	if(!anim1) anim1=GetAnim1();
	// 3d
	if(Anim3d) return Anim3d->IsAnimation(anim1,anim2);
	// 2d
	DWORD crtype=GetCrType();
	return LoadAnim(crtype,anim1,anim2,GetDir())!=NULL;
}

void CritterCl::SetBaseType(DWORD type)
{
	BaseType=type;
	BaseTypeAlias=CritType::GetAlias(type);

	// Check 3d aviability
	Anim3d=SprMngr->Load3dAnimation(Str::Format("%s.fo3d",CritType::GetName(BaseType)),PT_ART_CRITTERS);
	if(Anim3d)
	{
		Anim3dStay=SprMngr->Load3dAnimation(Str::Format("%s.fo3d",CritType::GetName(BaseType)),PT_ART_CRITTERS);

		Anim3d->SetDir(CrDir);
		SprId=Anim3d->GetSprId();

#ifdef FONLINE_CLIENT
		if(!Layers3d)
		{
			Layers3d=Script::CreateArray("int[]");
			((asIScriptArray*)Layers3d)->Resize(LAYERS3D_COUNT);
		}
		ZeroMemory(((asIScriptArray*)Layers3d)->GetElementPointer(0),LAYERS3D_COUNT*sizeof(int));
#else
		if(!Layers3d) Layers3d=new int[LAYERS3D_COUNT];
		ZeroMemory(Layers3d,LAYERS3D_COUNT*sizeof(int));
#endif
	}
}

void CritterCl::SetDir(BYTE dir)
{
	if(dir>5 || !CritType::IsCanRotate(GetCrType())) dir=0;
	if(CrDir==dir) return;
	CrDir=dir;
	if(Anim3d) Anim3d->SetDir(dir);
	AnimateStay();
}

void CritterCl::Process()
{
	// Changed parameters
	ProcessChangedParams();

	// Fading
	if(fadingEnable==true) Alpha=GetFadeAlpha();

	// Extra offsets
	if(OffsExtNextTick && Timer::FastTick()>=OffsExtNextTick)
	{
		OffsExtNextTick=Timer::FastTick()+30;
		int dist=DistSqrt(0,0,OxExtI,OyExtI);
		SprOx-=OxExtI;
		SprOy-=OyExtI;
		float mul=dist/10;
		if(mul<1.0f) mul=1.0f;
		OxExtF+=OxExtSpeed*mul;
		OyExtF+=OyExtSpeed*mul;
		OxExtI=(int)OxExtF;
		OyExtI=(int)OyExtF;
		if(DistSqrt(0,0,OxExtI,OyExtI)>dist) // End of work
		{
			OffsExtNextTick=0;
			OxExtI=0;
			OyExtI=0;
		}
		SetOffs(SprOx,SprOy,true);
	}

	// Animation
	if(animSequence.size())
	{
		CritterAnim* anim=&animSequence[0];
		int anim_proc=Procent(anim->AnimTick,Timer::FastTick()-animStartTick);
		if(!Anim3d)
		{
			if(anim_proc<100)
			{
				short cur_spr=begSpr+((endSpr-begSpr+1)*anim_proc)/100;
				if(cur_spr!=curSpr)
				{
					curSpr=cur_spr;
					//TODO: animation cycle
					if(curSpr>=anim->Anim->CntFrm) curSpr=0;
					SprId=anim->Anim->Ind[curSpr];
					ChangeOffs(anim->Anim->NextX[curSpr],anim->Anim->NextY[curSpr],anim->MoveText);
				}
			}
			else
			{
				bool is_move=(anim->IndAnim2==ANIM2_2D_WALK || anim->IndAnim2==ANIM2_2D_RUN);
				NextAnim(true);
				if(is_move && MoveSteps.size()) return;
			}
		}
		else
		{
			if(anim->BeginFrm)
			{
#define WALK_OFFSET(x,y) (x)-((x)*anim_proc/100),(y)-((y)*anim_proc/100),true
				if(anim_proc>=100) anim_proc=100;
				switch(anim->BeginFrm-1)
				{
				case 0: SetOffs(WALK_OFFSET(-16,12)); break;
				case 1: SetOffs(WALK_OFFSET(-32,0)); break;
				case 2: SetOffs(WALK_OFFSET(-16,-12)); break;
				case 3: SetOffs(WALK_OFFSET(16,-12)); break;
				case 4: SetOffs(WALK_OFFSET(32,0)); break;
				case 5: SetOffs(WALK_OFFSET(16,12)); break;
				default: break;
				}

				if(anim_proc>=100)
				{
					NextAnim(true);
					if(MoveSteps.size()) return;
				}
			}
			else if(!Anim3d->IsAnimationPlaying())
			{
				NextAnim(true);
			}
		}

		if(!animSequence.size()) AnimateStay();
	}

	// Battle 3d mode
	if(Anim3d && !animSequence.size() && Cond==COND_LIFE)
	{
		if(IsCombatMode() && Anim3d->GetAnim2()!=ANIM2_3D_IDLE_COMBAT) Animate(0,ANIM2_3D_BEGIN_COMBAT,NULL);
		else if(!IsCombatMode() && Anim3d->GetAnim2()==ANIM2_3D_IDLE_COMBAT) Animate(0,ANIM2_3D_END_COMBAT,NULL);
	}

	// Fidget animation
	if(!animSequence.size() && Cond==COND_LIFE && Timer::FastTick()>tickFun && IsFree() && !MoveSteps.size())
	{
		Action(ACTION_FIDGET,0,NULL);
		tickFun=Timer::FastTick()+Random(STAY_WAIT_SHOW_TIME,STAY_WAIT_SHOW_TIME*2);
	}
}

void CritterCl::ChangeOffs(short change_ox, short change_oy, bool move_text)
{
	SetOffs(SprOx-OxExtI+change_ox,SprOy-OyExtI+change_oy,move_text);
}

void CritterCl::SetOffs(short set_ox, short set_oy, bool move_text)
{
	SprOx=set_ox+OxExtI;
	SprOy=set_oy+OyExtI;
	if(SprDrawValid)
	{
		if(!Anim3d)
		{
			SprMngr->GetDrawCntrRect(SprDraw,&DRect);
			if(move_text) textRect=DRect;
		}
		if(Anim3d) Anim3d->SetDrawPos(SprDraw->ScrX+SprOx+CmnScrOx,SprDraw->ScrY+SprOy+CmnScrOy);
		if(IsChosen()) SprMngr->SetEgg(HexX,HexY,SprDraw);
	}
}

void CritterCl::SetSprRect()
{
	if(SprDrawValid)
	{
		if(!Anim3d)
		{
			INTRECT old=DRect;
			SprMngr->GetDrawCntrRect(SprDraw,&DRect);
			textRect.L+=DRect.L-old.L;
			textRect.R+=DRect.L-old.L;
			textRect.T+=DRect.T-old.T;
			textRect.B+=DRect.T-old.T;
		}
		else
		{
			Anim3d->SetDrawPos(SprDraw->ScrX+SprOx+CmnScrOx,SprDraw->ScrY+SprOy+CmnScrOy);
		}
		if(IsChosen()) SprMngr->SetEgg(HexX,HexY,SprDraw);
	}
}

INTRECT CritterCl::GetTextRect()
{
	if(SprDrawValid)
	{
		if(Anim3d)
		{
			SprMngr->GetDrawCntrRect(SprDraw,&textRect);
			textRect(-CmnScrOx,BORDERS_GROW-CmnScrOy);
		}
		return textRect;
	}
	return INTRECT();
}

/*
short CritterCl::GetSprOffX()
{
	short res=0;
	for(int i=0;i<curSpr;++i)

	SpriteInfo*	
	res=lpSM->->next_x[lpSM->CrAnim[crtype][anim1][anim2]->dir_offs[Ori]];
	
	for(int i=1;i<=num_frame;++i)
		ChangeCur_offs(lpSM->CrAnim[crtype][anim1][anim2]->next_x[lpSM->CrAnim[crtype][anim1][anim2]->dir_offs[Ori]+i],
		lpSM->CrAnim[crtype][anim1][anim2]->next_y[lpSM->CrAnim[crtype][anim1][anim2]->dir_offs[Ori]+i]);	
}

short CritterCl::GetSprOffY()
{

}
*/
void CritterCl::AccamulateOffs()
{
//	if(!cur_anim) return;
//	if(cur_afrm<cnt_per_turn) return;

//	for(int i=cur_afrm-cnt_per_turn;i<=cur_afrm;i++)
//	{
//		if(i<0) i=0;
	//	SetCur_offs(cur_anim->next_x[cur_anim->dir_offs[cur_dir]+cur_afrm],
	//		cur_anim->next_y[cur_anim->dir_offs[cur_dir]+cur_afrm]);
//	}
}

void CritterCl::AddOffsExt(short ox, short oy)
{
	SprOx-=OxExtI;
	SprOy-=OyExtI;
	ox+=OxExtI;
	oy+=OyExtI;
	OxExtI=ox;
	OyExtI=oy;
	OxExtF=(float)ox;
	OyExtF=(float)oy;
	GetStepsXY(OxExtSpeed,OyExtSpeed,0,0,ox,oy);
	OxExtSpeed=-OxExtSpeed;
	OyExtSpeed=-OyExtSpeed;
	OffsExtNextTick=Timer::FastTick()+30;
	SetOffs(SprOx,SprOy,true);
}

void CritterCl::SetText(const char* str, DWORD color, DWORD text_delay)
{
	tickStartText=Timer::FastTick();
	strTextOnHead=str;
	tickTextDelay=text_delay;
	textOnHeadColor=color;
}

void CritterCl::DrawTextOnHead()
{
	if(strTextOnHead.empty())
	{
		if(IsPlayer() && !CmnShowPlayerNames) return;
		if(IsNpc() && !CmnShowNpcNames) return;
	}

	if(SprDrawValid)
	{
		INTRECT tr=GetTextRect();
		int x=(tr.L+tr.W()/2+CmnScrOx)/ZOOM-100;
		int y=(tr.T+CmnScrOy)/ZOOM-70;
		INTRECT r(x,y,x+200,y+70);

		string str;
		DWORD color;
		if(strTextOnHead.empty())
		{
			str=GetName();
			if(CmnShowCritId) str+=Str::Format(" <%u>",IsNpc()?GetId()-NPC_START_ID+1:GetId()-USERS_START_ID+1);
			if(FLAG(Flags,FCRIT_DISCONNECT)) str+=OptPlayerOffAppendix;
			color=(NameColor?NameColor:COLOR_CRITTER_NAME);
		}
		else
		{
			str=strTextOnHead;
			color=textOnHeadColor;
		}

		if(fadingEnable)
		{
			DWORD alpha=GetFadeAlpha();
			SprMngr->DrawStr(r,str.c_str(),FT_CENTERX|FT_BOTTOM|FT_COLORIZE|FT_BORDERED,(alpha<<24)|(color&0xFFFFFF));
		}
		else if(!IsFinishing())
		{
			SprMngr->DrawStr(r,str.c_str(),FT_CENTERX|FT_BOTTOM|FT_COLORIZE|FT_BORDERED,color);
		}
	}

	if(Timer::FastTick()-tickStartText>=tickTextDelay && !strTextOnHead.empty()) strTextOnHead="";
}
