#include <stdafx.h>
#include "Perks.h"
#include <FOdefines.h>

#ifdef FONLINE_SERVER
#include <Critter.h>
#else
#include <..\client\CritterCl.h>
#include <FOScript.h>
#endif


bool Perks::Disabled[0x100]={0};
int Perks::ScriptBindId[0x100]={0};
DWORD Perks::Range[0x100]={0};
DWORD Perks::Level[0x100]={0};

void Perks::Clear()
{
	for(int i=0;i<0x100;i++)
	{
		Disabled[i]=false;
		ScriptBindId[i]=0;
		Range[i]=0;
		Level[i]=0;
	}
}

bool Perks::Check(CCritter* cr, BYTE perk, DWORD flags)
{
	if(Disabled[perk]) return false;

#ifdef FONLINE_SERVER
	int* param=cr->Info.Params;
#else
	int* param=cr->Params;
#endif
	
#define PROCESS_PERK0(perk_,range,level) case perk_: return(param[perk_]<range && param[ST_LEVEL]>=level)
#define PROCESS_PERK1(perk_,range,level,demand0) case perk_: return(param[perk_]<range && param[ST_LEVEL]>=level && demand0)
#define PROCESS_PERK2(perk_,range,level,demand0,demand1) case perk_: return(param[perk_]<range && param[ST_LEVEL]>=level && demand0 && demand1)
#define PROCESS_PERK3(perk_,range,level,demand0,demand1,demand2) case perk_: return(param[perk_]<range && param[ST_LEVEL]>=level && demand0 && demand1 && demand2)
#define PROCESS_PERK4(perk_,range,level,demand0,demand1,demand2,demand3) case perk_: return(param[perk_]<range && param[ST_LEVEL]>=level && demand0 && demand1 && demand2 && demand3)

	if(FLAG(flags,PERK_PERK))
	{
		if(perk>=PERK_SCRIPT_BEGIN && perk<=PERK_SCRIPT_END)
		{
			if(param[perk]>=Range[perk] || param[ST_LEVEL]<Level[perk]) return false;
			if(ScriptBindId[perk]<=0 || !Script::PrepareContext(ScriptBindId[perk],CALL_FUNC_STR,cr->GetInfo())) return false;
			Script::SetArgObject(cr);
			return Script::RunPrepared() && Script::GetReturnedBool();
		}

		switch(perk)
		{
		// Perks
		PROCESS_PERK1(PE_AWARENESS,           1,  3, param[ST_PERCEPTION]>=5    );
		PROCESS_PERK1(PE_BONUS_HTH_ATTACKS,   1, 15, param[ST_AGILITY]>=6       );
		PROCESS_PERK2(PE_BONUS_HTH_DAMAGE,    3,  3, param[ST_AGILITY]>=6,      param[ST_STRENGTH]>=6      );
		PROCESS_PERK1(PE_BONUS_MOVE,          2,  6, param[ST_AGILITY]>=5       );
		PROCESS_PERK2(PE_BONUS_RANGED_DAMAGE, 2,  6, param[ST_AGILITY]>=6,      param[ST_LUCK]>=6          );
		PROCESS_PERK3(PE_BONUS_RATE_OF_FIRE,  1, 15, param[ST_AGILITY]>=7,      param[ST_INTELLECT]>=6,    param[ST_PERCEPTION]>=6);
		PROCESS_PERK1(PE_EARLIER_SEQUENCE,    3,  3, param[ST_PERCEPTION]>=6    );
		PROCESS_PERK1(PE_FASTER_HEALING,      3,  3, param[ST_ENDURANCE]>=6     );
		PROCESS_PERK1(PE_MORE_CRITICALS,      3,  6, param[ST_LUCK]>=6          );
		PROCESS_PERK2(PE_RAD_RESISTANCE,      2,  6, param[ST_ENDURANCE]>=6,    param[ST_INTELLECT]>=4     );
		PROCESS_PERK2(PE_TOUGHNESS,           3,  3, param[ST_ENDURANCE]>=6,    param[ST_LUCK]>=6          );
		PROCESS_PERK2(PE_STRONG_BACK,         3,  3, param[ST_STRENGTH]>=6,     param[ST_ENDURANCE]>=6     );
		PROCESS_PERK2(PE_SHARPSHOOTER,        1,  9, param[ST_PERCEPTION]>=7,   param[ST_INTELLECT]>=6     );
		PROCESS_PERK2(PE_SILENT_RUNNING,      1,  6, param[ST_AGILITY]>=6,      param[SK_SNEAK]>=50        );
		PROCESS_PERK3(PE_SURVIVALIST,         3,  3, param[ST_ENDURANCE]>=6,    param[ST_INTELLECT]>=6,    param[SK_OUTDOORSMAN]>=40);
		PROCESS_PERK2(PE_MASTER_TRADER,       1,  9, param[ST_CHARISMA]>=7,     param[SK_BARTER]>=60       );
		PROCESS_PERK1(PE_EDUCATED,            3,  6, param[ST_INTELLECT]>=6     );
		PROCESS_PERK4(PE_HEALER,              2,  3, param[ST_PERCEPTION]>=7,   param[ST_AGILITY]>=6,      param[ST_INTELLECT]>=5,        param[SK_FIRST_AID]>=40);
		PROCESS_PERK3(PE_BETTER_CRITICALS,    1,  9, param[ST_PERCEPTION]>=6,   param[ST_LUCK]>=6,         param[ST_AGILITY]>=4);
		PROCESS_PERK3(PE_SLAYER,              1, 24, param[ST_AGILITY]>=8,      param[ST_STRENGTH]>=8,     param[SK_UNARMED]>=80);
		PROCESS_PERK3(PE_SNIPER,              1, 24, param[ST_AGILITY]>=8,      param[ST_PERCEPTION]>=8,   param[SK_SMALL_GUNS]>=80);
		PROCESS_PERK3(PE_SILENT_DEATH,        1, 18, param[ST_AGILITY]>=10,     param[SK_SNEAK]>=80,       param[SK_UNARMED]>=80);
		PROCESS_PERK1(PE_ACTION_BOY,          2, 12, param[ST_AGILITY]>=5       );
		PROCESS_PERK1(PE_LIFEGIVER,           2, 12, param[ST_ENDURANCE]>=4      );
		PROCESS_PERK1(PE_DODGER,              1,  9, param[ST_AGILITY]>=6       );
		PROCESS_PERK1(PE_SNAKEATER,           2,  6, param[ST_ENDURANCE]>=3     );
		PROCESS_PERK2(PE_MR_FIXIT,            1, 12, param[SK_REPAIR]>=40,      param[SK_SCIENCE]>=40      );
		PROCESS_PERK2(PE_MEDIC,               1, 12, param[SK_FIRST_AID]>=40,   param[SK_DOCTOR]>=40       );
		PROCESS_PERK2(PE_MASTER_THIEF,        1, 12, param[SK_STEAL]>=50,       param[SK_LOCKPICK]>=50     );
		PROCESS_PERK1(PE_SPEAKER,             1,  9, param[SK_SPEECH]>=50       );
		PROCESS_PERK0(PE_HEAVE_HO,            3,  6  );
		PROCESS_PERK2(PE_PICKPOCKET,          1, 15, param[ST_AGILITY]>=8,      param[SK_STEAL]>=80);
		PROCESS_PERK1(PE_GHOST,               1,  6, param[SK_SNEAK]>=60        );
		PROCESS_PERK0(PE_EXPLORER,            1,  9  );
		PROCESS_PERK2(PE_PATHFINDER,          2,  6, param[ST_ENDURANCE]>=6,    param[SK_OUTDOORSMAN]>=40  );
		PROCESS_PERK1(PE_SCOUT,               1,  3, param[ST_PERCEPTION]>=7    );
		PROCESS_PERK1(PE_RANGER,              1,  6, param[ST_PERCEPTION]>=6    );
		PROCESS_PERK1(PE_QUICK_POCKETS,       1,  3, param[ST_AGILITY]>=5       );
		PROCESS_PERK1(PE_SMOOTH_TALKER,       3,  3, param[ST_INTELLECT]>=4     );
		PROCESS_PERK1(PE_SWIFT_LEARNER,       3,  3, param[ST_INTELLECT]>=4     );
		PROCESS_PERK1(PE_ADRENALINE_RUSH,     1,  6, param[ST_STRENGTH]<10      );
		PROCESS_PERK1(PE_CAUTIOUS_NATURE,     1,  3, param[ST_PERCEPTION]>=6    );
		PROCESS_PERK1(PE_COMPREHENSION,       1,  3, param[ST_INTELLECT]>=6     );
		PROCESS_PERK2(PE_DEMOLITION_EXPERT,   1,  9, param[ST_AGILITY]>=4,      param[SK_TRAPS]>=75        );
		PROCESS_PERK1(PE_GAMBLER,             1,  6, param[SK_GAMBLING]>=50     );
		PROCESS_PERK1(PE_GAIN_STRENGTH,       1, 12, param[ST_STRENGTH]<10      );
		PROCESS_PERK1(PE_GAIN_PERCEPTION,     1, 12, param[ST_PERCEPTION]<10    );
		PROCESS_PERK1(PE_GAIN_ENDURANCE,      1, 12, param[ST_ENDURANCE]<10     );
		PROCESS_PERK1(PE_GAIN_CHARISMA,       1, 12, param[ST_CHARISMA]<10      );
		PROCESS_PERK1(PE_GAIN_INTELLIGENCE,   1, 12, param[ST_INTELLECT]<10     );
		PROCESS_PERK1(PE_GAIN_AGILITY,        1, 12, param[ST_AGILITY]<10       );
		PROCESS_PERK1(PE_GAIN_LUCK,           1, 12, param[ST_LUCK]<10          );
		PROCESS_PERK2(PE_HARMLESS,            1,  6, param[SK_STEAL]>=50,       param[ST_KARMA]>=50        );
		PROCESS_PERK0(PE_HERE_AND_NOW,        1,  9  );
		PROCESS_PERK1(PE_HTH_EVADE,           1, 12, param[SK_UNARMED]>=75      );
		PROCESS_PERK2(PE_KAMA_SUTRA_MASTER,   1,  3, param[ST_ENDURANCE]>=5,    param[ST_AGILITY]>=5       );
		PROCESS_PERK1(PE_KARMA_BEACON,        1,  9, param[ST_CHARISMA]>=6      );
		PROCESS_PERK2(PE_LIGHT_STEP,          1,  9, param[ST_AGILITY]>=5,      param[ST_LUCK]>=5          );
		PROCESS_PERK1(PE_LIVING_ANATOMY,      1, 12, param[SK_DOCTOR]>=60       );
		PROCESS_PERK1(PE_MAGNETIC_PERSONALITY,1,  3, param[ST_CHARISMA]<9       );
		PROCESS_PERK2(PE_NEGOTIATOR,          1,  6, param[SK_SPEECH]>=50,      param[SK_BARTER]>=50       );
		PROCESS_PERK0(PE_PACK_RAT,            1,  6  );
		PROCESS_PERK1(PE_PYROMANIAC,          1,  9, param[SK_BIG_GUNS]>=75     );
		PROCESS_PERK1(PE_QUICK_RECOVERY,      1,  3, param[ST_AGILITY]>=5       );
		PROCESS_PERK1(PE_SALESMAN,            1,  6, param[SK_BARTER]>=50       );
		PROCESS_PERK1(PE_STONEWALL,           1,  3, param[ST_STRENGTH]>=6      );
		PROCESS_PERK0(PE_THIEF,               1,  3  );
		PROCESS_PERK1(PE_WEAPON_HANDLING,     1, 12, param[ST_AGILITY]>=5       );
		default: break;
		}
	}
	if(FLAG(flags,PERK_ADDICTION))
	{
		switch(perk)
		{
		PROCESS_PERK0(PE_NUKA_COLA_ADDICTION, 1,  1  );
		PROCESS_PERK0(PE_BUFFOUT_ADDICTION,   1,  1  );
		PROCESS_PERK0(PE_MENTATS_ADDICTION,   1,  1  );
		PROCESS_PERK0(PE_PSYCHO_ADDICTION,    1,  1  );
		PROCESS_PERK0(PE_RADAWAY_ADDICTION,   1,  1  );
		PROCESS_PERK0(PE_JET_ADDICTION,       1,  1  );
		PROCESS_PERK0(PE_TRAGIC_ADDICTION,    1,  1  );
		default: break;
		}
	}
	if(FLAG(flags,PERK_ARMOR))
	{
		switch(perk)
		{
		PROCESS_PERK0(PE_POWERED_ARMOR,       1,  1  );
		PROCESS_PERK0(PE_COMBAT_ARMOR,        1,  1  );
		PROCESS_PERK0(PE_ARMOR_ADVANCED_I,    1,  1  );
		PROCESS_PERK0(PE_ARMOR_ADVANCED_II,   1,  1  );
		default: break;
		}
	}
	if(FLAG(flags,PERK_KARMA))
	{
		switch(perk)
		{
		PROCESS_PERK0(PE_BERSERKER_KARMA,     1,  1  );
		PROCESS_PERK0(PE_CHAMPION_KARMA,      1,  1  );
		PROCESS_PERK0(PE_CHILDKILLER_KARMA,   1,  1  );
		PROCESS_PERK0(PE_SEXPERT_KARMA,       1,  1  );
		PROCESS_PERK0(PE_PRIZEFIGHTER_KARMA,  1,  1  );
		PROCESS_PERK0(PE_GIGOLO_KARMA,        1,  1  );
		PROCESS_PERK0(PE_GRAVE_DIGGER_KARMA,  1,  1  );
		PROCESS_PERK0(PE_MARRIED_KARMA,       1,  1  );
		PROCESS_PERK0(PE_PORN_STAR_KARMA,     1,  1  );
		PROCESS_PERK0(PE_SLAVER_KARMA,        1,  1  );
		PROCESS_PERK0(PE_VIRGIN_WASTES_KARMA, 1,  1  );
		PROCESS_PERK0(PE_MAN_SALVATORE_KARMA, 1,  1  );
		PROCESS_PERK0(PE_MAN_BISHOP_KARMA,    1,  1  );
		PROCESS_PERK0(PE_MAN_MORDINO_KARMA,   1,  1  );
		PROCESS_PERK0(PE_MAN_WRIGHT_KARMA,    1,  1  );
		PROCESS_PERK0(PE_SEPARATED_KARMA,     1,  1  );
		default: break;
		}
	}
	if(FLAG(flags,PERK_DAMAGE))
	{
		switch(perk)
		{
		PROCESS_PERK0(PE_DAMAGE_POISONED,     1,  1  );
		PROCESS_PERK0(PE_DAMAGE_RADIATED,     1,  1  );
		PROCESS_PERK0(PE_DAMAGE_EYE,          1,  1  );
		PROCESS_PERK0(PE_DAMAGE_RIGHT_ARM,    1,  1  );
		PROCESS_PERK0(PE_DAMAGE_LEFT_ARM,     1,  1  );
		PROCESS_PERK0(PE_DAMAGE_RIGHT_LEG,    1,  1  );
		PROCESS_PERK0(PE_DAMAGE_LEFT_LEG,     1,  1  );
		default: break;
		}
	}
	if(FLAG(flags,PERK_TAG_SKILL))
	{
		switch(perk)
		{
		case PE_TAG_SKILL1:
		case PE_TAG_SKILL2:
		case PE_TAG_SKILL3:
			if(param[perk]==0xFF) return true;
			break;
		default:
			break;
		}
	}
	if(FLAG(flags,PERK_MODE))
	{
		switch(perk)
		{
		PROCESS_PERK0(PE_HIDE_MODE,        1,  1  );
		PROCESS_PERK0(PE_NOSTEAL_MODE,     1,  1  );
		PROCESS_PERK0(PE_NOBARTER_MODE,    1,  1  );
		PROCESS_PERK0(PE_NOENEMYSTACK_MODE,1,  1  );
		PROCESS_PERK0(PE_NOPVP_MODE,       1,  1  );
		default: break;
		}
	}
	if(FLAG(flags,PERK_TRAIT))
	{
		switch(perk)
		{
		PROCESS_PERK0(PE_FAST_METABOLISM, 1,  1  );
		PROCESS_PERK0(PE_BRUISER,         1,  1  );
		PROCESS_PERK0(PE_SMALL_FRAME,     1,  1  );
		PROCESS_PERK0(PE_ONE_HANDER,      1,  1  );
		PROCESS_PERK0(PE_FINESSE,         1,  1  );
		PROCESS_PERK0(PE_KAMIKAZE,        1,  1  );
		PROCESS_PERK0(PE_HEAVY_HANDED,    1,  1  );
		PROCESS_PERK0(PE_FAST_SHOT,       1,  1  );
		PROCESS_PERK0(PE_BLOODY_MESS,     1,  1  );
		PROCESS_PERK0(PE_JINXED,          1,  1  );
		PROCESS_PERK0(PE_GOOD_NATURED,    1,  1  );
		PROCESS_PERK0(PE_CHEM_RELIANT,    1,  1  );
		PROCESS_PERK0(PE_CHEM_RESISTANT,  1,  1  );
		PROCESS_PERK0(PE_NIGHT_PERSON,    1,  1  );
		PROCESS_PERK0(PE_SKILLED,         1,  1  );
		PROCESS_PERK0(PE_SEX_APPEAL,      1,  1  );
		//PROCESS_PERK0(PE_GIFTED,          1,  1  );
		default: break;
		}
	}

	return false;
}

#ifdef FONLINE_SERVER
void Perks::Up(CCritter* cr, BYTE perk, DWORD perk_flags, bool force)
{
	if(!force && !Check(cr,perk,perk_flags)) return;

	if(perk>=PERK_SCRIPT_BEGIN && perk<=PERK_SCRIPT_END)
	{
		cr->Info.Params[perk]++;
		cr->SendParam(perk);
		return;
	}

#define PERKUP_STAT(param,val) cr->Info.Params[param]##val; cr->SendParam(param)
#define PERKUP_SKILL(param,val) cr->Info.Params[param]##val; if(cr->Info.Params[param]>MAX_SKILL_VAL) cr->Info.Params[param]=MAX_SKILL_VAL; cr->SendParam(param)

	switch(perk)
	{
	// Perks
	case PE_AWARENESS: /*Examining a target shows hitpoints, weapon and ammunition count*/ break;
	case PE_BONUS_HTH_ATTACKS: /*-1 Ap on HtH attacks*/ break;
	case PE_BONUS_HTH_DAMAGE: PERKUP_STAT(ST_MELEE_DAMAGE,+=2); break;
	case PE_BONUS_MOVE: PERKUP_STAT(ST_MAX_MOVE_AP,+=2); break;
	case PE_BONUS_RANGED_DAMAGE: /*In calc damage +2 per range*/ break;
	case PE_BONUS_RATE_OF_FIRE: /*-1 Ap on ranged attacks*/ break;
	case PE_EARLIER_SEQUENCE: PERKUP_STAT(ST_SEQUENCE,+=2); break;
	case PE_FASTER_HEALING: PERKUP_STAT(ST_HEALING_RATE,+=2); break;
	case PE_MORE_CRITICALS: PERKUP_STAT(ST_CRITICAL_CHANCE,+=5); break;
	case PE_RAD_RESISTANCE: PERKUP_STAT(ST_RADIATION_RESISTANCE,+=15); break;
	case PE_TOUGHNESS: PERKUP_STAT(ST_NORMAL_RESIST,+=10); break;
	case PE_STRONG_BACK: PERKUP_STAT(ST_CARRY_WEIGHT,+=CONVERT_GRAMM(50)); break;
	case PE_SHARPSHOOTER: PERKUP_STAT(ST_BONUS_LOOK,+=6); /*+2 perception on ranged attack*/ break;
	case PE_SILENT_RUNNING: /*No steal off while running*/ break;
	case PE_SURVIVALIST: PERKUP_SKILL(SK_OUTDOORSMAN,+=25); break;
	case PE_MASTER_TRADER: /*Barter k is zero on buy*/ break;
	case PE_EDUCATED: /*+2 skill points per range*/ break;
	case PE_HEALER: /*Additional Hp bonus in scripts*/ break;
	case PE_BETTER_CRITICALS: /*In criticals damage +20%*/ break;
	case PE_SLAYER: /*All unarmed and melee attacks is critical*/ break;
	case PE_SNIPER: /*All gun attacks is critical*/ break;
	case PE_SILENT_DEATH: /*Attack from sneak from back x2 damage*/ break;
	case PE_ACTION_BOY: PERKUP_STAT(ST_ACTION_POINTS,+=1); break;
	case PE_LIFEGIVER: PERKUP_STAT(ST_MAX_LIFE,+=4); break;
	case PE_DODGER: PERKUP_STAT(ST_ARMOR_CLASS,+=5); break;
	case PE_SNAKEATER: PERKUP_STAT(ST_POISON_RESISTANCE,+=25); break;
	case PE_MR_FIXIT: PERKUP_SKILL(SK_REPAIR,+=10); PERKUP_SKILL(SK_SCIENCE,+=10); break;
	case PE_MEDIC: PERKUP_SKILL(SK_FIRST_AID,+=20); PERKUP_SKILL(SK_DOCTOR,+=20); break;
	case PE_MASTER_THIEF: PERKUP_SKILL(SK_STEAL,+=20); PERKUP_SKILL(SK_LOCKPICK,+=20); break;
	case PE_SPEAKER: PERKUP_SKILL(SK_SPEECH,+=40); break;
	case PE_HEAVE_HO: /*+2 strength for throwing weapon*/ break;
	case PE_PICKPOCKET: /*Ignore size and facing while stealing*/ break;
	case PE_GHOST: /*+20% sneak on night*/ break;
	case PE_EXPLORER: /*Higher chance of finding special places and people in random encounters*/ break;
	case PE_PATHFINDER: /*+25% speed on global per range*/ break;
	case PE_SCOUT: /*This will increase the amount of the map you can see while exploring and make finding the special random encounters a little easier.*/ break;
	case PE_RANGER: PERKUP_SKILL(SK_OUTDOORSMAN,+=15); /*Fewer hostile random encounters*/ break;
	case PE_QUICK_POCKETS: /*Inventory items move ap cost div 2*/ break;
	case PE_SMOOTH_TALKER: /*+2 intellect on dialogs checks*/ break;
	case PE_SWIFT_LEARNER: /*+5% add experience per range*/ break;
	case PE_ADRENALINE_RUSH: /*+1 strength on <50% hp in battle*/ break;
	case PE_CAUTIOUS_NATURE: /*+3 perception on encounter generate*/ break;
	case PE_COMPREHENSION: /*50% more points on books reading*/ break;
	case PE_DEMOLITION_EXPERT: /*Used in explosion script*/ break;
	case PE_GAMBLER: PERKUP_SKILL(SK_GAMBLING,+=40); break;
	case PE_GAIN_STRENGTH: PERKUP_STAT(ST_STRENGTH,++); break;
	case PE_GAIN_PERCEPTION: PERKUP_STAT(ST_PERCEPTION,++); break;
	case PE_GAIN_ENDURANCE: PERKUP_STAT(ST_ENDURANCE,++); break;
	case PE_GAIN_CHARISMA: PERKUP_STAT(ST_CHARISMA,++); break;
	case PE_GAIN_INTELLIGENCE: PERKUP_STAT(ST_INTELLECT,++); break;
	case PE_GAIN_AGILITY: PERKUP_STAT(ST_AGILITY,++); break;
	case PE_GAIN_LUCK: PERKUP_STAT(ST_LUCK,++); break;
	case PE_HARMLESS: PERKUP_SKILL(SK_STEAL,+=40); break;
	case PE_HERE_AND_NOW: /*+1 level*/ cr->AddExperience(NextLevel(cr->GetParam(ST_LEVEL))-cr->GetParam(ST_EXPERIENCE),true); break;
	case PE_HTH_EVADE: /*+3 ac per 1 ap on end turn*/ break;
	case PE_KAMA_SUTRA_MASTER: /*In dialogs*/ break;
	case PE_KARMA_BEACON: /*Karma x2*/ break;
	case PE_LIGHT_STEP: /*50% less chance of setting off a trap*/ break;
	case PE_LIVING_ANATOMY: PERKUP_SKILL(SK_DOCTOR,+=20); /*Also +5 dmg*/ break;
	case PE_MAGNETIC_PERSONALITY: /*+1 people in group*/ break;
	case PE_NEGOTIATOR: PERKUP_SKILL(SK_SPEECH,+=20); PERKUP_SKILL(SK_BARTER,+=20); break;
	case PE_PACK_RAT: PERKUP_STAT(ST_CARRY_WEIGHT,+=CONVERT_GRAMM(50)); break;
	case PE_PYROMANIAC: /*+5 damage on fire attack*/ break;
	case PE_QUICK_RECOVERY: /* 1 ap for stending up*/ break;
	case PE_SALESMAN: PERKUP_SKILL(SK_BARTER,+=40); break;
	case PE_STONEWALL: /*Reduction in chance to be knocked down during combat*/ break;
	case PE_THIEF: PERKUP_SKILL(SK_SNEAK,+=10); PERKUP_SKILL(SK_LOCKPICK,+=10); PERKUP_SKILL(SK_STEAL,+=10); PERKUP_SKILL(SK_TRAPS,+=10); break;
	case PE_WEAPON_HANDLING: /*In use weapon strength +3*/ break;
	// Addictions
	case PE_NUKA_COLA_ADDICTION: /*Addiction*/ break;
	case PE_BUFFOUT_ADDICTION: /*Addiction*/ break;
	case PE_MENTATS_ADDICTION: /*Addiction*/ break;
	case PE_PSYCHO_ADDICTION: /*Addiction*/ break;
	case PE_RADAWAY_ADDICTION: /*Addiction*/ break;
	case PE_JET_ADDICTION: /*Addiction*/ break;
	case PE_TRAGIC_ADDICTION: /*Addiction*/ break;
	// Armor
	case PE_POWERED_ARMOR: /*Armor*/ break;
	case PE_COMBAT_ARMOR: /*Armor*/ break;
	case PE_ARMOR_ADVANCED_I: /*Armor*/ break;
	case PE_ARMOR_ADVANCED_II: /*Armor*/ break;
	// Karma
	case PE_BERSERKER_KARMA: /*Karma*/ break;
	case PE_CHAMPION_KARMA: /*Karma*/ break;
	case PE_CHILDKILLER_KARMA: /*Karma*/ break;
	case PE_SEXPERT_KARMA: /*Karma*/ break;
	case PE_PRIZEFIGHTER_KARMA: /*Karma*/ break;
	case PE_GIGOLO_KARMA: /*Karma*/ break;
	case PE_GRAVE_DIGGER_KARMA: /*Karma*/ break;
	case PE_MARRIED_KARMA: /*Karma*/ break;
	case PE_PORN_STAR_KARMA: /*Karma*/ break;
	case PE_SLAVER_KARMA: /*Karma*/ break;
	case PE_VIRGIN_WASTES_KARMA: /*Karma*/ break;
	case PE_MAN_SALVATORE_KARMA: /*Karma*/ break;
	case PE_MAN_BISHOP_KARMA: /*Karma*/ break;
	case PE_MAN_MORDINO_KARMA: /*Karma*/ break;
	case PE_MAN_WRIGHT_KARMA: /*Karma*/ break;
	case PE_SEPARATED_KARMA: /*Karma*/ break;
	default: if(force) break; return;
	}

	cr->Info.Params[perk]++;
	cr->SendParam(perk);
}

void Perks::Down(CCritter* cr, BYTE perk)
{
	if(!cr->Info.Params[perk]) return;

	if(perk>=PERK_SCRIPT_BEGIN && perk<=PERK_SCRIPT_END)
	{
		cr->Info.Params[perk]--;
		cr->SendParam(perk);
		return;
	}

#define PERKDOWN_STAT(param,val) cr->Info.Params[param]##val; cr->SendParam(param)
#define PERKDOWN_SKILL(param,val) cr->Info.Params[param]##val; cr->SendParam(param)

	switch(perk)
	{
	// Perks
	case PE_AWARENESS: break;
	case PE_BONUS_HTH_ATTACKS: break;
	case PE_BONUS_HTH_DAMAGE: PERKDOWN_STAT(ST_MELEE_DAMAGE,-=2);	break;
	case PE_BONUS_MOVE: PERKDOWN_STAT(ST_MAX_MOVE_AP,-=2); break;
	case PE_BONUS_RANGED_DAMAGE: break;
	case PE_BONUS_RATE_OF_FIRE: break;
	case PE_EARLIER_SEQUENCE: PERKDOWN_STAT(ST_SEQUENCE,-=2); break;
	case PE_FASTER_HEALING: PERKDOWN_STAT(ST_HEALING_RATE,-=2); break;
	case PE_MORE_CRITICALS: PERKDOWN_STAT(ST_CRITICAL_CHANCE,-=5); break;
	case PE_RAD_RESISTANCE: PERKDOWN_STAT(ST_RADIATION_RESISTANCE,-=15); break;
	case PE_TOUGHNESS: PERKDOWN_STAT(ST_NORMAL_RESIST,-=10); break;
	case PE_STRONG_BACK: PERKDOWN_STAT(ST_CARRY_WEIGHT,-=CONVERT_GRAMM(50)); break;
	case PE_SHARPSHOOTER: PERKDOWN_STAT(ST_BONUS_LOOK,-=6); break;
	case PE_SILENT_RUNNING: break;
	case PE_SURVIVALIST: PERKDOWN_SKILL(SK_OUTDOORSMAN,-=25); break;
	case PE_MASTER_TRADER: break;
	case PE_EDUCATED: break;
	case PE_HEALER: break;
	case PE_BETTER_CRITICALS: break;
	case PE_SLAYER: break;
	case PE_SNIPER: break;
	case PE_SILENT_DEATH: break;
	case PE_ACTION_BOY: PERKDOWN_STAT(ST_ACTION_POINTS,-=1); break;
	case PE_LIFEGIVER: PERKDOWN_STAT(ST_MAX_LIFE,-=4); break;
	case PE_DODGER: PERKDOWN_STAT(ST_ARMOR_CLASS,-=5); break;
	case PE_SNAKEATER: PERKDOWN_STAT(ST_POISON_RESISTANCE,-=25); break;
	case PE_MR_FIXIT: PERKDOWN_SKILL(SK_REPAIR,-=20); PERKDOWN_SKILL(SK_SCIENCE,-=20); break;
	case PE_MEDIC: PERKDOWN_SKILL(SK_FIRST_AID,-=20); PERKDOWN_SKILL(SK_DOCTOR,-=20); break;
	case PE_MASTER_THIEF: PERKDOWN_SKILL(SK_STEAL,-=20); PERKDOWN_SKILL(SK_LOCKPICK,-=20); break;
	case PE_SPEAKER: PERKDOWN_SKILL(SK_SPEECH,-=40); break;
	case PE_HEAVE_HO: break;
	case PE_PICKPOCKET: break;
	case PE_GHOST: break;
	case PE_EXPLORER: break;
	case PE_PATHFINDER: break;
	case PE_SCOUT: break;
	case PE_RANGER: break;
	case PE_QUICK_POCKETS: break;
	case PE_SMOOTH_TALKER: break;
	case PE_SWIFT_LEARNER: break;
	case PE_ADRENALINE_RUSH: break;
	case PE_CAUTIOUS_NATURE: break;
	case PE_COMPREHENSION: break;
	case PE_DEMOLITION_EXPERT: break;
	case PE_GAMBLER: PERKDOWN_SKILL(SK_GAMBLING,-=40); break;
	case PE_GAIN_STRENGTH: PERKDOWN_STAT(ST_STRENGTH,--); break;
	case PE_GAIN_PERCEPTION: PERKDOWN_STAT(ST_PERCEPTION,--); break;
	case PE_GAIN_ENDURANCE: PERKDOWN_STAT(ST_ENDURANCE,--); break;
	case PE_GAIN_CHARISMA: PERKDOWN_STAT(ST_CHARISMA,--); break;
	case PE_GAIN_INTELLIGENCE: PERKDOWN_STAT(ST_INTELLECT,--); break;
	case PE_GAIN_AGILITY: PERKDOWN_STAT(ST_AGILITY,--); break;
	case PE_GAIN_LUCK: PERKDOWN_STAT(ST_LUCK,--); break;
	case PE_HARMLESS: PERKDOWN_SKILL(SK_STEAL,-=40); break;
	case PE_HERE_AND_NOW: break;
	case PE_HTH_EVADE: break;
	case PE_KAMA_SUTRA_MASTER: break;
	case PE_KARMA_BEACON: break;
	case PE_LIGHT_STEP: break;
	case PE_LIVING_ANATOMY: PERKDOWN_SKILL(SK_DOCTOR,-=20); break;
	case PE_MAGNETIC_PERSONALITY: break;
	case PE_NEGOTIATOR: PERKDOWN_SKILL(SK_SPEECH,-=20); PERKDOWN_SKILL(SK_BARTER,-=20); break;
	case PE_PACK_RAT: PERKDOWN_STAT(ST_CARRY_WEIGHT,-=CONVERT_GRAMM(50)); break;
	case PE_PYROMANIAC: break;
	case PE_QUICK_RECOVERY: break;
	case PE_SALESMAN: PERKDOWN_SKILL(SK_BARTER,-=40); break;
	case PE_STONEWALL: break;
	case PE_THIEF: PERKDOWN_SKILL(SK_SNEAK,-=10); PERKDOWN_SKILL(SK_LOCKPICK,-=10); PERKDOWN_SKILL(SK_STEAL,-=10); PERKDOWN_SKILL(SK_TRAPS,-=10); break;
	case PE_WEAPON_HANDLING: break;
	// Addictions
	case PE_NUKA_COLA_ADDICTION: break;
	case PE_BUFFOUT_ADDICTION: break;
	case PE_MENTATS_ADDICTION: break;
	case PE_PSYCHO_ADDICTION: break;
	case PE_RADAWAY_ADDICTION: break;
	case PE_JET_ADDICTION: break;
	case PE_TRAGIC_ADDICTION: break;
	// Armor
	case PE_POWERED_ARMOR: break;
	case PE_COMBAT_ARMOR: break;
	case PE_ARMOR_ADVANCED_I: break;
	case PE_ARMOR_ADVANCED_II: break;
	// Karma
	case PE_BERSERKER_KARMA: /*Karma*/ break;
	case PE_CHAMPION_KARMA: /*Karma*/ break;
	case PE_CHILDKILLER_KARMA: /*Karma*/ break;
	case PE_SEXPERT_KARMA: /*Karma*/ break;
	case PE_PRIZEFIGHTER_KARMA: /*Karma*/ break;
	case PE_GIGOLO_KARMA: /*Karma*/ break;
	case PE_GRAVE_DIGGER_KARMA: /*Karma*/ break;
	case PE_MARRIED_KARMA: /*Karma*/ break;
	case PE_PORN_STAR_KARMA: /*Karma*/ break;
	case PE_SLAVER_KARMA: /*Karma*/ break;
	case PE_VIRGIN_WASTES_KARMA: /*Karma*/ break;
	case PE_MAN_SALVATORE_KARMA: /*Karma*/ break;
	case PE_MAN_BISHOP_KARMA: /*Karma*/ break;
	case PE_MAN_MORDINO_KARMA: /*Karma*/ break;
	case PE_MAN_WRIGHT_KARMA: /*Karma*/ break;
	case PE_SEPARATED_KARMA: /*Karma*/ break;
	default: return;
	}

	cr->Info.Params[perk]--;
	cr->SendParam(perk);
}

void Perks::CheckValid(CCritter* cr)
{
	// Check armor perks
	for(int i=0;i<ArmorPerksCount;i++)
	{
		BYTE perk=ArmorPerks[i];
		if(cr->Info.Params[perk] && cr->ObjSlotArm->Proto->ARMOR.Perk!=perk) Perks::Down(cr,perk);
	}
}
#endif