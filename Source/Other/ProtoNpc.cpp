#include "StdAfx.h"
#include "ProtoNpc.h"

#pragma MESSAGE("Расширить FavoriteItemPid.\n")

/*
bool CProtoNpc::LoadFromFile(FileManager* fm)
{
	ZeroMemory(&Info,sizeof(Info));
	return false;

	//TODO:
	return false;

	char ret_str[DIALOG_STR_BUF_SIZE];
	PrID id = GetPrivateProfileIntA("info","id", 0, file_name);
	if (CheckCritProtoID(id) == FALSE) 
	{
		WriteLog("Неправильный ID прототипа крита %u\n", id);
		return FALSE;
	}

	GetId() = id;
	// Получаем тип
	CrTYPE base_type = GetPrivateProfileIntA("info","base_type", 255, file_name);
	if (CheckCritBaseType(base_type) == FALSE)
	{
		WriteLog("Неправильный base_type прототипа крита %u\n", id);
		return FALSE;
	}
	Info.base_type = base_type;
	
	int true_char;

	GetPrivateProfileStringA("info","name", "err", ret_str, MAX_NAME, file_name);
	true_char = CheckStr(ret_str);
	strcpy(GetName(), ret_str);
	GetPrivateProfileStringA("info","cases0", "err", ret_str, MAX_NAME, file_name);
	true_char = true_char || CheckStr(ret_str);
	strcpy(Info.cases[0], ret_str);
	GetPrivateProfileStringA("info","cases1", "err", ret_str, MAX_NAME, file_name);
	true_char = true_char || CheckStr(ret_str);
	strcpy(Info.cases[1], ret_str);
	GetPrivateProfileStringA("info","cases2", "err", ret_str, MAX_NAME, file_name);
	true_char = true_char || CheckStr(ret_str);
	strcpy(Info.cases[2], ret_str);
	GetPrivateProfileStringA("info","cases3", "err", ret_str, MAX_NAME, file_name);
	true_char = true_char || CheckStr(ret_str);
	strcpy(Info.cases[3], ret_str);
	GetPrivateProfileStringA("info","cases4", "err", ret_str, MAX_NAME, file_name);
	true_char = true_char || CheckStr(ret_str);
	strcpy(Info.cases[4], ret_str);

	GetPrivateProfileStringA("info","script", "error", ret_str, MAX_SCRIPT_NAME, file_name); //!Cvet++
	if(strcmp(ret_str,"error"))
	{
		true_char = true_char || CheckStr(ret_str);
		strcpy(ScriptName, ret_str);
	} //!Cvet --

	if (true_char) WriteLog("Обнаружены некорректные символы в имени крита. Заменены пробелами\n");

	GetPrivateProfileStringA("info", "cond", "err", ret_str, MAX_NAME, file_name);
	if (GetCritCond(ret_str) < 0)
	{
		WriteLog("Неправильный параметр cond прототипа крита %u\n", id);
		return FALSE;
	}

	//параметры
	char key[100];
//	char ret_str[1000];
	int val;
	int z = 0;

	StrByteMap::iterator p = StatNames.begin();
	for (z = 0; z < MAX_STATS && p != StatNames.end(); z++, p++)
	{
		sprintf(key, "%s", p->first.c_str());
		//GetPrivateProfileStringA("stats", key, "0", ret_str, 10, file_name);
//		val = atoi(ret_str);
		val = GetPrivateProfileIntA("stats", key, 0, file_name);
		if (val > STAT_VALUE_MAX) val = STAT_VALUE_MAX;
		if (val < STAT_VALUE_MIN) val = STAT_VALUE_MIN;
		PARSE_ASSIGN(p->second] = val;
	}

	p = SkillNames.begin();
	for (z = 0; z < MAX_SKILLS && p != SkillNames.end(); z++, p++)
	{
		sprintf(key, "%s", p->first.c_str());
//		GetPrivateProfileStringA("skills", key, "0", ret_str, 10, file_name);
//		val = atoi(ret_str);
		val = GetPrivateProfileIntA("skills", key, 0, file_name);
		if (val > SKILL_VALUE_MAX) val = SKILL_VALUE_MAX;
		if (val < SKILL_VALUE_MIN) val = SKILL_VALUE_MIN;
		PARSE_ASSIGN(p->second] = val;
	}

	p = PerkNames.begin();
	for (z = 0; z < MAX_PERKS && p != PerkNames.end(); z++, p++)
	{
		sprintf(key, "%s", p->first.c_str());
//		GetPrivateProfileStringA("perks", key, "0", ret_str, 10, file_name);
//		val = atoi(ret_str);
		val = GetPrivateProfileIntA("stats", key, 0, file_name);
		if (val > PERK_VALUE_MAX) val = PERK_VALUE_MAX;
		if (val < PERK_VALUE_MIN) val = PERK_VALUE_MIN;
		PARSE_ASSIGN(p->second] = val;
	}

	char buf[DIALOG_STR_BUF_SIZE];
	buf[0] = 0;

	// Выходной поток в строку
	dlg_buffer[0] = 0;
	ostrstream out(dlg_buffer, MAX_DLG_LEN_IN_BYTES);
	// Входной поток из файла
//	ifstream   in;
//	in.open((const char * )file_name);
	input = fopen(file_name,"r");
	if (!input) 
	{
		WriteLog("Не могу открыть файл %s\n", file_name);
		return FALSE;
	} 

//	in.width(DIALOG_STR_BUF_SIZE);
	while (!feof(input))
	{
		fscanf(input, "%s", buf);
		if (stricmp(buf, "[vars]") == 0) 	break;
	}
	int len, l;
	// Выходной поток в любом случае должен содержать 
	// обрамление области переменных
//	out << "[vars]\n";
	while (!feof(input))
	{
		fscanf(input, "%s", buf);
		if (stricmp(buf, "[end_vars]") == 0) break;
		len = strlen(buf);
		l = out.pcount();
		// Вычитаем длину строки "\n[end_vars]"
		if (l + (len + 1)  +  11 > MAX_DLG_LEN_IN_BYTES)
		{
			WriteLog("В буфере недостаточно места для записи переменных \n");
			fclose(input);
			return FALSE;
		}
		out << " " << buf;
	}
//	out << "\n[end_vars]";
	fclose(input);

	// Декодируем шаблоны переменных из потоки in_buf
	if (!LoadVarsFromStr(dlg_buffer))
	{
		WriteLog("Ошибка при чтении шаблонов переменных \n");
		return FALSE;
	}

	// Загружаем диалоги из файла
	char ch;
	dlg_buffer[0] = 0;
	FILE * cf = fopen(file_name, "r");
	if (!cf)
	{
		WriteLog("Ошибка при открытии файла %s\n", file_name);
		return FALSE;
	}
	
	while (!feof(cf))
	{
		fscanf(cf, "%c", &ch);
		if (ch == '&') break; 
	}

	if (ch != '&')
	{
		WriteLog("Не найдена область с диалогами у прототипа %u\n", id);
		fclose(cf);
		return TRUE;
	}
/ *
	dlg_buffer[0] = ch;
	//strcat(dlg_buffer, "[dialogs]\n&");
	// Нужно считать диалоги в буффер.
	len = fread(&dlg_buffer[1], sizeof(char), MAX_DLG_LEN_IN_BYTES - 1, cf);
	dlg_buffer[len] = 0;

	if (!LoadDialogsFromStr(dlg_buffer)) 
	{
		WriteLog("ОШИБКА при распаковки диалогов прототипа крита %u\n", GetId());
		fclose(input);
		return FALSE;
	}
* /
	WriteLog("Прототип крита %u  успешно загружен из файла %s \n", id , file_name);
	fclose(input);
	return TRUE;  
}
*/

bool LoadFromFileF2(FileManager* fm, FILE* f)
{
#define PARSE_ASSIGN(x) int x_##x=fm->GetBEDWord()
#define PARSE_ASSIGND(x) int x_##x=0; fm->GetBEDWord()
#define PARSE_ASSIGN_SAVE(x) int x_##x=fm->GetBEDWord(); if(x_##x!=0) fprintf(f,#x"=%d\n",x_##x)
#define PARSE_ADD_SAVE(x) x_##x+=fm->GetBEDWord(); if(x_##x!=0) fprintf(f,#x"=%d\n",x_##x)
#define PARSE_ASSIGNV_SAVE(x,v) int x_##x=v; if(x_##x!=0) fprintf(f,#x"=%d\n",x_##x)
#define PARSE_SAVE(x) if(x_##x!=0) fprintf(f,#x"=%d\n",x_##x)

	fprintf(f,"[Critter prototype]\n");
	fm->SetCurPos(0);
	DWORD ProtoId=fm->GetBEDWord()&0xFFFF; //0x00
	fprintf(f,"Pid=%d\n",ProtoId);
	fm->GetBEDWord(); //0x04
	PARSE_ASSIGN(ST_BASE_CRTYPE); x_ST_BASE_CRTYPE&=0xFFFF; PARSE_SAVE(ST_BASE_CRTYPE);
	//Info.BaseType=(fm->GetBEDWord()&0xFFFF); //0x08
	fm->GetBEDWord(); //0x0C
	fm->GetBEDWord(); //0x10
	fm->GetBEDWord(); //0x14
	fm->GetBEDWord(); //0x18
	fm->GetBEDWord(); //0x1C
	fm->GetBEDWord(); //0x20
	PARSE_ASSIGN_SAVE(ST_AI_ID);
	//AiPackNum=fm->GetBEDWord(); //0x24
	fm->GetBEDWord(); //0x28
	DWORD Flags=fm->GetBEDWord(); //0x2C

	PARSE_ASSIGN(ST_STRENGTH);
	PARSE_ASSIGN(ST_PERCEPTION);
	PARSE_ASSIGN(ST_ENDURANCE);
	PARSE_ASSIGN(ST_CHARISMA);
	PARSE_ASSIGN(ST_INTELLECT);
	PARSE_ASSIGN(ST_AGILITY);
	PARSE_ASSIGN(ST_LUCK);
	PARSE_ASSIGND(ST_MAX_LIFE); // Disregarded
	PARSE_ASSIGND(ST_ACTION_POINTS); // Disregarded
	PARSE_ASSIGND(ST_ARMOR_CLASS); // Disregarded
	/*PARSE_ASSIGN(ST_UNARMED_DAMAGE]=0;*/fm->GetBEDWord(); // Disregarded
	PARSE_ASSIGND(ST_MELEE_DAMAGE); // Disregarded
	PARSE_ASSIGND(ST_CARRY_WEIGHT); // Disregarded
	PARSE_ASSIGND(ST_SEQUENCE); // Disregarded
	PARSE_ASSIGND(ST_HEALING_RATE); // Disregarded
	PARSE_ASSIGND(ST_CRITICAL_CHANCE); // Disregarded
	PARSE_ASSIGND(ST_MAX_CRITICAL);
	PARSE_ASSIGN(ST_NORMAL_ABSORB);
	PARSE_ASSIGN(ST_LASER_ABSORB);
	PARSE_ASSIGN(ST_FIRE_ABSORB);
	PARSE_ASSIGN(ST_PLASMA_ABSORB);
	PARSE_ASSIGN(ST_ELECTRO_ABSORB);
	PARSE_ASSIGN(ST_EMP_ABSORB);
	PARSE_ASSIGN(ST_EXPLODE_ABSORB);
	PARSE_ASSIGN(ST_NORMAL_RESIST);
	PARSE_ASSIGN(ST_LASER_RESIST);
	PARSE_ASSIGN(ST_FIRE_RESIST);
	PARSE_ASSIGN(ST_PLASMA_RESIST);
	PARSE_ASSIGN(ST_ELECTRO_RESIST);
	PARSE_ASSIGN(ST_EMP_RESIST);
	PARSE_ASSIGN(ST_EXPLODE_RESIST);
	PARSE_ASSIGND(ST_RADIATION_RESISTANCE); // Disregarded
	PARSE_ASSIGND(ST_POISON_RESISTANCE); // Disregarded
	PARSE_ASSIGN(ST_AGE);
	PARSE_ASSIGN(ST_GENDER);

	// Bonus
	PARSE_ADD_SAVE(ST_STRENGTH);
	PARSE_ADD_SAVE(ST_PERCEPTION);
	PARSE_ADD_SAVE(ST_ENDURANCE);
	PARSE_ADD_SAVE(ST_CHARISMA);
	PARSE_ADD_SAVE(ST_INTELLECT);
	PARSE_ADD_SAVE(ST_AGILITY);
	PARSE_ADD_SAVE(ST_LUCK);
	PARSE_ADD_SAVE(ST_MAX_LIFE);
	PARSE_ADD_SAVE(ST_ACTION_POINTS);
	PARSE_ADD_SAVE(ST_ARMOR_CLASS);
	/*PARSE_ADD_SAVE(ST_UNARMED_DAMAGE);*/ fm->GetBEDWord(); // Unarmed Damage
	PARSE_ADD_SAVE(ST_MELEE_DAMAGE); // Melee Damage
	PARSE_ADD_SAVE(ST_CARRY_WEIGHT);
	PARSE_ADD_SAVE(ST_SEQUENCE);
	PARSE_ADD_SAVE(ST_HEALING_RATE);
	PARSE_ADD_SAVE(ST_CRITICAL_CHANCE);
	PARSE_ADD_SAVE(ST_MAX_CRITICAL);
	PARSE_ADD_SAVE(ST_NORMAL_ABSORB);
	PARSE_ADD_SAVE(ST_LASER_ABSORB);
	PARSE_ADD_SAVE(ST_FIRE_ABSORB);
	PARSE_ADD_SAVE(ST_PLASMA_ABSORB);
	PARSE_ADD_SAVE(ST_ELECTRO_ABSORB);
	PARSE_ADD_SAVE(ST_EMP_ABSORB);
	PARSE_ADD_SAVE(ST_EXPLODE_ABSORB);
	PARSE_ADD_SAVE(ST_NORMAL_RESIST);
	PARSE_ADD_SAVE(ST_LASER_RESIST);
	PARSE_ADD_SAVE(ST_FIRE_RESIST);
	PARSE_ADD_SAVE(ST_PLASMA_RESIST);
	PARSE_ADD_SAVE(ST_ELECTRO_RESIST);
	PARSE_ADD_SAVE(ST_EMP_RESIST);
	PARSE_ADD_SAVE(ST_EXPLODE_RESIST);
	PARSE_ADD_SAVE(ST_RADIATION_RESISTANCE);
	PARSE_ADD_SAVE(ST_POISON_RESISTANCE);
	PARSE_ADD_SAVE(ST_AGE);
	PARSE_ADD_SAVE(ST_GENDER);

	// Skills
	PARSE_ASSIGN_SAVE(SK_SMALL_GUNS);
	PARSE_ASSIGN_SAVE(SK_BIG_GUNS);
	PARSE_ASSIGN_SAVE(SK_ENERGY_WEAPONS);
	PARSE_ASSIGN_SAVE(SK_UNARMED);
	PARSE_ASSIGN_SAVE(SK_MELEE_WEAPONS);
	PARSE_ASSIGN_SAVE(SK_THROWING);
	PARSE_ASSIGN_SAVE(SK_FIRST_AID);
	PARSE_ASSIGN_SAVE(SK_DOCTOR);
	PARSE_ASSIGN_SAVE(SK_SNEAK);
	PARSE_ASSIGN_SAVE(SK_LOCKPICK);
	PARSE_ASSIGN_SAVE(SK_STEAL);
	PARSE_ASSIGN_SAVE(SK_TRAPS);
	PARSE_ASSIGN_SAVE(SK_SCIENCE);
	PARSE_ASSIGN_SAVE(SK_REPAIR);
	PARSE_ASSIGN_SAVE(SK_SPEECH);
	PARSE_ASSIGN_SAVE(SK_BARTER);
	PARSE_ASSIGN_SAVE(SK_GAMBLING);
	PARSE_ASSIGN_SAVE(SK_OUTDOORSMAN);
/*
	PARSE_ADD_SAVE(SK_SMALL_GUNS);//]+=5+4*PARSE_ASSIGN(ST_AGILITY];
	PARSE_ADD_SAVE(SK_BIG_GUNS);//]+=0+2*PARSE_ASSIGN(ST_AGILITY];
	PARSE_ADD_SAVE(SK_ENERGY_WEAPONS);//]+=0+2*PARSE_ASSIGN(ST_AGILITY];
	PARSE_ADD_SAVE(SK_UNARMED);//]+=30+2*(PARSE_ASSIGN(ST_AGILITY]+PARSE_ASSIGN(ST_STRENGTH]);
	PARSE_ADD_SAVE(SK_MELEE_WEAPONS);//]+=20+2*(PARSE_ASSIGN(ST_AGILITY]+PARSE_ASSIGN(ST_STRENGTH]);
	PARSE_ADD_SAVE(SK_THROWING);//]+=0+4*PARSE_ASSIGN(ST_AGILITY];
	PARSE_ADD_SAVE(SK_FIRST_AID);//]+=0+2*(PARSE_ASSIGN(ST_PERCEPTION]+PARSE_ASSIGN(ST_INTELLECT]);
	PARSE_ADD_SAVE(SK_DOCTOR);//]+=5+PARSE_ASSIGN(ST_PERCEPTION]+PARSE_ASSIGN(ST_INTELLECT];
	PARSE_ADD_SAVE(SK_SNEAK);//]+=5+3*PARSE_ASSIGN(ST_AGILITY];
	PARSE_ADD_SAVE(SK_LOCKPICK);//]+=10+PARSE_ASSIGN(ST_PERCEPTION]+PARSE_ASSIGN(ST_AGILITY];
	PARSE_ADD_SAVE(SK_STEAL);//]+=0+3*PARSE_ASSIGN(ST_AGILITY];
	PARSE_ADD_SAVE(SK_TRAPS);//]+=10+PARSE_ASSIGN(ST_PERCEPTION]+PARSE_ASSIGN(ST_AGILITY];
	PARSE_ADD_SAVE(SK_SCIENCE);//]+=0+4*PARSE_ASSIGN(ST_INTELLECT];
	PARSE_ADD_SAVE(SK_REPAIR);//]+=0+3*PARSE_ASSIGN(ST_INTELLECT];
	PARSE_ADD_SAVE(SK_SPEECH);//]+=0+5*PARSE_ASSIGN(ST_CHARISMA];
	PARSE_ADD_SAVE(SK_BARTER);//]+=0+4*PARSE_ASSIGN(ST_CHARISMA];
	PARSE_ADD_SAVE(SK_GAMBLING);//]+=0+5*PARSE_ASSIGN(ST_LUCK];
	PARSE_ADD_SAVE(SK_OUTDOORSMAN);//]+=0+2*(PARSE_ASSIGN(ST_ENDURANCE]+PARSE_ASSIGN(ST_INTELLECT]);
*/
	// Other
	PARSE_ASSIGN_SAVE(ST_LOCOMOTION_TYPE);
	PARSE_ASSIGN_SAVE(ST_KILL_EXPERIENCE);
	PARSE_ASSIGN_SAVE(ST_BODY_TYPE);
	PARSE_ASSIGN(ST_DAMAGE_TYPE); x_ST_DAMAGE_TYPE++; PARSE_SAVE(ST_DAMAGE_TYPE);

//	PARSE_ASSIGN(ST_MAX_LIFE]+=15;
//	PARSE_ASSIGN(ST_CURRENT_HP]=PARSE_ASSIGN(ST_MAX_LIFE]+PARSE_ASSIGN(ST_STRENGTH]+PARSE_ASSIGN(ST_ENDURANCE]*2;
//	PARSE_ASSIGN(ST_CURRENT_AP]=(PARSE_ASSIGN(ST_ACTION_POINTS]+5+PARSE_ASSIGN(ST_AGILITY]/2)*AP_DIVIDER;
//	PARSE_ASSIGN(ST_POISONING_LEVEL]=0;
//	PARSE_ASSIGN(ST_RADIATION_LEVEL]=0;
//	PARSE_ASSIGN(ST_MAX_CRITICAL]=(PARSE_ASSIGN(ST_MAX_CRITICAL]?20:0);

	//PARSE_ASSIGN(ST_MAX_LIFE]-=PARSE_ASSIGN(ST_STRENGTH]+PARSE_ASSIGN(ST_ENDURANCE]*2;
	//PARSE_ASSIGN(ST_ACTION_POINTS]-=5+PARSE_ASSIGN(ST_AGILITY]/2;
	//PARSE_ASSIGN(ST_UNARMED_DAMAGE]-=(PARSE_ASSIGN(ST_STRENGTH]>6?PARSE_ASSIGN(ST_STRENGTH]-5:1);
	//PARSE_ASSIGN(ST_MELEE_DAMAGE]-=(PARSE_ASSIGN(ST_STRENGTH]>6?PARSE_ASSIGN(ST_STRENGTH]-5:1);
	//PARSE_ASSIGN(ST_ARMOR_CLASS]-=PARSE_ASSIGN(ST_AGILITY];
	//PARSE_ASSIGN(ST_SEQUENCE]-=PARSE_ASSIGN(ST_PERCEPTION]*2;
	//PARSE_ASSIGN(ST_HEALING_RATE]-=PARSE_ASSIGN(ST_ENDURANCE];
	//PARSE_ASSIGN(ST_CRITICAL_CHANCE]-=PARSE_ASSIGN(ST_LUCK];
	//PARSE_ASSIGN(ST_RADIATION_RESISTANCE]-=PARSE_ASSIGN(ST_ENDURANCE]*2;
	//PARSE_ASSIGN(ST_POISON_RESISTANCE]-=PARSE_ASSIGN(ST_ENDURANCE]*5;

//	Info.Cond=COND_LIFE;
//	Info.CondExt=COND_LIFE_NONE;

	fm->UnloadFile();

	SETFLAG(Flags,FCRIT_NPC);

#define FCRIT_CAN_BARTER        (0x00000002) // Может торговать. +
#define FCRIT_NO_STEAL          (0x00000020) // Нельзя обворовать. +
#define FCRIT_NO_DROP           (0x00000040) // Нельзя сбрасывать предметы. +
#define FCRIT_NO_LOOSE_LIMBS    (0x00000080) // Не может терять конечности. +
#define FCRIT_DEAD_AGES         (0x00000100) // Мёртвое тело не исчезает. ?
#define FCRIT_NO_HEAL           (0x00000200) // Повреждения не излечиваются с течением времени. ?
#define FCRIT_INVULNERABLE      (0x00000400) // Неуязвимый. ?
#define FCRIT_NO_FLATTEN        (0x00000800) // Не помещать труп на задний план после смерти. +
#define FCRIT_SPECIAL_DEAD      (0x00001000) // Есть особый вид смерти. -
#define FCRIT_RANGE_HTH         (0x00002000) // Возможна рукопашная атака на расстоянии. -
#define FCRIT_NO_KNOCK          (0x00004000) // Не может быть сбит с ног. +
	PARSE_ASSIGNV_SAVE(MODE_NO_BARTER,FLAG(Flags,FCRIT_CAN_BARTER)?0:1);
	PARSE_ASSIGNV_SAVE(MODE_NO_STEAL,FLAG(Flags,FCRIT_NO_STEAL)?1:0);
	PARSE_ASSIGNV_SAVE(MODE_NO_DROP,FLAG(Flags,FCRIT_NO_DROP)?1:0);
	PARSE_ASSIGNV_SAVE(MODE_NO_LOOSE_LIMBS,FLAG(Flags,FCRIT_NO_LOOSE_LIMBS)?1:0);
	PARSE_ASSIGNV_SAVE(MODE_DEAD_AGES,FLAG(Flags,FCRIT_DEAD_AGES)?1:0);
	PARSE_ASSIGNV_SAVE(MODE_NO_HEAL,FLAG(Flags,FCRIT_NO_HEAL)?1:0);
	PARSE_ASSIGNV_SAVE(MODE_INVULNERABLE,FLAG(Flags,FCRIT_INVULNERABLE)?1:0);
	PARSE_ASSIGNV_SAVE(MODE_NO_FLATTEN,FLAG(Flags,FCRIT_NO_FLATTEN)?1:0);
	PARSE_ASSIGNV_SAVE(MODE_SPECIAL_DEAD,FLAG(Flags,FCRIT_SPECIAL_DEAD)?1:0);
	PARSE_ASSIGNV_SAVE(MODE_RANGE_HTH,FLAG(Flags,FCRIT_RANGE_HTH)?1:0);
	PARSE_ASSIGNV_SAVE(MODE_NO_KNOCK,FLAG(Flags,FCRIT_NO_KNOCK)?1:0);

//	static DWORD barter=0,cnt=0;
//	barter+=PARSE_ASSIGN(SK_BARTER];
//	cnt++;
//	WriteLog("Stat barter <%d>, approx<%d>.\n",PARSE_ASSIGN(SK_BARTER],barter/cnt);

//	static DWORD charisma=0,cnt=0;
//	charisma+=PARSE_ASSIGN(ST_ACTION_POINTS]+(5+PARSE_ASSIGN(ST_AGILITY]/2);
//	cnt++;
//	WriteLog("Pid<%u> ap<%d>, approx<%d>.\n",ProtoId,PARSE_ASSIGN(ST_ACTION_POINTS]+(5+PARSE_ASSIGN(ST_AGILITY]/2),charisma/cnt);

//	WriteLog("Skill barter %d<%d>\n",FLAG(Info.Flags,FCRIT_CAN_BARTER)?1:0,PARSE_ASSIGN(SK_BARTER]);
//	WriteLog("Unarmed dmg<%d>\n",PARSE_ASSIGN(ST_UNARMED_DAMAGE]+(PARSE_ASSIGN(ST_STRENGTH]>6?PARSE_ASSIGN(ST_STRENGTH]-5:1));
//	for(int i=0;i<MAX_SKILLS;i++) WriteLog("Skill<%d> == <%d>\n",i,PARSE_ASSIGN(i]);
//	WriteLog("============\n");

	fprintf(f,"\n");
	return true;
}






/*
#include "StdAfx.h"
#include <ProtoNpc.h>

bool CProtoNpc::LoadFromFile(FileManager* fm)
{
	ZeroMemory(&Info,sizeof(Info));
	return LoadFromFileF2(fm);

	//TODO:
	return false;
/ *
	char ret_str[DIALOG_STR_BUF_SIZE];
	PrID id = GetPrivateProfileIntA("info","id", 0, file_name);
	if (CheckCritProtoID(id) == FALSE) 
	{
		WriteLog("Неправильный ID прототипа крита %u\n", id);
		return FALSE;
	}

	GetId() = id;
	// Получаем тип 
	CrTYPE base_type = GetPrivateProfileIntA("info","base_type", 255, file_name);
	if (CheckCritBaseType(base_type) == FALSE)
	{
		WriteLog("Неправильный base_type прототипа крита %u\n", id);
		return FALSE;
	}
	Info.base_type = base_type;
	
	int true_char;

	GetPrivateProfileStringA("info","name", "err", ret_str, MAX_NAME, file_name);
	true_char = CheckStr(ret_str);
	strcpy(GetName(), ret_str);
	GetPrivateProfileStringA("info","cases0", "err", ret_str, MAX_NAME, file_name);
	true_char = true_char || CheckStr(ret_str);
	strcpy(Info.cases[0], ret_str);
	GetPrivateProfileStringA("info","cases1", "err", ret_str, MAX_NAME, file_name);
	true_char = true_char || CheckStr(ret_str);
	strcpy(Info.cases[1], ret_str);
	GetPrivateProfileStringA("info","cases2", "err", ret_str, MAX_NAME, file_name);
	true_char = true_char || CheckStr(ret_str);
	strcpy(Info.cases[2], ret_str);
	GetPrivateProfileStringA("info","cases3", "err", ret_str, MAX_NAME, file_name);
	true_char = true_char || CheckStr(ret_str);
	strcpy(Info.cases[3], ret_str);
	GetPrivateProfileStringA("info","cases4", "err", ret_str, MAX_NAME, file_name);
	true_char = true_char || CheckStr(ret_str);
	strcpy(Info.cases[4], ret_str);

	GetPrivateProfileStringA("info","script", "error", ret_str, MAX_SCRIPT_NAME, file_name); //!Cvet++
	if(strcmp(ret_str,"error"))
	{
		true_char = true_char || CheckStr(ret_str);
		strcpy(ScriptName, ret_str);
	} //!Cvet --

	if (true_char) WriteLog("Обнаружены некорректные символы в имени крита. Заменены пробелами\n");

	GetPrivateProfileStringA("info", "cond", "err", ret_str, MAX_NAME, file_name);
	if (GetCritCond(ret_str) < 0)
	{
		WriteLog("Неправильный параметр cond прототипа крита %u\n", id);
		return FALSE;
	}

	//параметры
	char key[100];
//	char ret_str[1000];
	int val;
	int z = 0;

	StrByteMap::iterator p = StatNames.begin();
	for (z = 0; z < MAX_STATS && p != StatNames.end(); z++, p++)
	{
		sprintf(key, "%s", p->first.c_str());
		//GetPrivateProfileStringA("stats", key, "0", ret_str, 10, file_name);
//		val = atoi(ret_str);
		val = GetPrivateProfileIntA("stats", key, 0, file_name);
		if (val > STAT_VALUE_MAX) val = STAT_VALUE_MAX;
		if (val < STAT_VALUE_MIN) val = STAT_VALUE_MIN;
		Info.Params[p->second] = val;
	}

	p = SkillNames.begin();
	for (z = 0; z < MAX_SKILLS && p != SkillNames.end(); z++, p++)
	{
		sprintf(key, "%s", p->first.c_str());
//		GetPrivateProfileStringA("skills", key, "0", ret_str, 10, file_name);
//		val = atoi(ret_str);
		val = GetPrivateProfileIntA("skills", key, 0, file_name);
		if (val > SKILL_VALUE_MAX) val = SKILL_VALUE_MAX;
		if (val < SKILL_VALUE_MIN) val = SKILL_VALUE_MIN;
		Info.Params[p->second] = val;
	}

	p = PerkNames.begin();
	for (z = 0; z < MAX_PERKS && p != PerkNames.end(); z++, p++)
	{
		sprintf(key, "%s", p->first.c_str());
//		GetPrivateProfileStringA("perks", key, "0", ret_str, 10, file_name);
//		val = atoi(ret_str);
		val = GetPrivateProfileIntA("stats", key, 0, file_name);
		if (val > PERK_VALUE_MAX) val = PERK_VALUE_MAX;
		if (val < PERK_VALUE_MIN) val = PERK_VALUE_MIN;
		Info.Params[p->second] = val;
	}

	char buf[DIALOG_STR_BUF_SIZE];
	buf[0] = 0;

	// Выходной поток в строку
	dlg_buffer[0] = 0;
	ostrstream out(dlg_buffer, MAX_DLG_LEN_IN_BYTES);
	// Входной поток из файла
//	ifstream   in;
//	in.open((const char * )file_name);
	input = fopen(file_name,"r");
	if (!input) 
	{
		WriteLog("Не могу открыть файл %s\n", file_name);
		return FALSE;
	} 

//	in.width(DIALOG_STR_BUF_SIZE);
	while (!feof(input))
	{
		fscanf(input, "%s", buf);
		if (stricmp(buf, "[vars]") == 0) 	break;
	}
	int len, l;
	// Выходной поток в любом случае должен содержать 
	// обрамление области переменных
//	out << "[vars]\n";
	while (!feof(input))
	{
		fscanf(input, "%s", buf);
		if (stricmp(buf, "[end_vars]") == 0) break;
		len = strlen(buf);
		l = out.pcount();
		// Вычитаем длину строки "\n[end_vars]"
		if (l + (len + 1)  +  11 > MAX_DLG_LEN_IN_BYTES)
		{
			WriteLog("В буфере недостаточно места для записи переменных \n");
			fclose(input);
			return FALSE;
		}
		out << " " << buf;
	}
//	out << "\n[end_vars]";
	fclose(input);

	// Декодируем шаблоны переменных из потоки in_buf
	if (!LoadVarsFromStr(dlg_buffer))
	{
		WriteLog("Ошибка при чтении шаблонов переменных \n");
		return FALSE;
	}

	// Загружаем диалоги из файла
	char ch;
	dlg_buffer[0] = 0;
	FILE * cf = fopen(file_name, "r");
	if (!cf)
	{
		WriteLog("Ошибка при открытии файла %s\n", file_name);
		return FALSE;
	}
	
	while (!feof(cf))
	{
		fscanf(cf, "%c", &ch);
		if (ch == '&') break; 
	}

	if (ch != '&')
	{
		WriteLog("Не найдена область с диалогами у прототипа %u\n", id);
		fclose(cf);
		return TRUE;
	}
/ *
	dlg_buffer[0] = ch;
	//strcat(dlg_buffer, "[dialogs]\n&");
	// Нужно считать диалоги в буффер.
	len = fread(&dlg_buffer[1], sizeof(char), MAX_DLG_LEN_IN_BYTES - 1, cf);
	dlg_buffer[len] = 0;

	if (!LoadDialogsFromStr(dlg_buffer)) 
	{
		WriteLog("ОШИБКА при распаковки диалогов прототипа крита %u\n", GetId());
		fclose(input);
		return FALSE;
	}
* /
	WriteLog("Прототип крита %u  успешно загружен из файла %s \n", id , file_name);
	fclose(input);
	return TRUE;  * /   
}

bool CProtoNpc::LoadFromFileF2(FileManager* fm)
{
	fm->SetCurPos(0);
	ProtoId=fm->GetBEDWord()&0xFFFF; //0x00
	fm->GetBEDWord(); //0x04
	Info.BaseType=(fm->GetBEDWord()&0xFFFF); //0x08
	fm->GetBEDWord(); //0x0C
	fm->GetBEDWord(); //0x10
	fm->GetBEDWord(); //0x14
	fm->GetBEDWord(); //0x18
	fm->GetBEDWord(); //0x1C
	fm->GetBEDWord(); //0x20
	AiPackNum=fm->GetBEDWord(); //0x24
	fm->GetBEDWord(); //0x28
	Flags=fm->GetBEDWord(); //0x2C

#define PARSE_ASSIGN(x) int x_##x=fm->GetBEDWord()
#define PARSE_ASSIGND(x) int x_##x=0; fm->GetBEDWord()

	PARSE_ASSIGN(ST_STRENGTH]=fm->GetBEDWord();
	PARSE_ASSIGN(ST_PERCEPTION]=fm->GetBEDWord();
	PARSE_ASSIGN(ST_ENDURANCE]=fm->GetBEDWord();
	PARSE_ASSIGN(ST_CHARISMA]=fm->GetBEDWord();
	PARSE_ASSIGN(ST_INTELLECT]=fm->GetBEDWord();
	PARSE_ASSIGN(ST_AGILITY]=fm->GetBEDWord();
	PARSE_ASSIGN(ST_LUCK]=fm->GetBEDWord();

	PARSE_ASSIGND(ST_MAX_LIFE]=0;fm->GetBEDWord(); // Disregarded
	PARSE_ASSIGND(ST_ACTION_POINTS]=0;fm->GetBEDWord(); // Disregarded
	PARSE_ASSIGND(ST_ARMOR_CLASS]=0;fm->GetBEDWord(); // Disregarded
	/ *Info.Params[ST_UNARMED_DAMAGE]=0;* /fm->GetBEDWord(); // Disregarded
	PARSE_ASSIGND(ST_MELEE_DAMAGE]=0;fm->GetBEDWord(); // Disregarded
	PARSE_ASSIGND(ST_CARRY_WEIGHT]=0;fm->GetBEDWord(); // Disregarded
	PARSE_ASSIGND(ST_SEQUENCE]=0;fm->GetBEDWord(); // Disregarded
	PARSE_ASSIGND(ST_HEALING_RATE]=0;fm->GetBEDWord(); // Disregarded
	PARSE_ASSIGND(ST_CRITICAL_CHANCE]=0;fm->GetBEDWord(); // Disregarded
	PARSE_ASSIGND(ST_MAX_CRITICAL]=fm->GetBEDWord();

	PARSE_ASSIGN(ST_NORMAL_ABSORB]=fm->GetBEDWord();
	PARSE_ASSIGN(ST_LASER_ABSORB]=fm->GetBEDWord();
	PARSE_ASSIGN(ST_FIRE_ABSORB]=fm->GetBEDWord();
	PARSE_ASSIGN(ST_PLASMA_ABSORB]=fm->GetBEDWord();
	PARSE_ASSIGN(ST_ELECTRO_ABSORB]=fm->GetBEDWord();
	PARSE_ASSIGN(ST_EMP_ABSORB]=fm->GetBEDWord();
	PARSE_ASSIGN(ST_EXPLODE_ABSORB]=fm->GetBEDWord();

	Info.Params[ST_NORMAL_RESIST]=fm->GetBEDWord();
	Info.Params[ST_LASER_RESIST]=fm->GetBEDWord();
	Info.Params[ST_FIRE_RESIST]=fm->GetBEDWord();
	Info.Params[ST_PLASMA_RESIST]=fm->GetBEDWord();
	Info.Params[ST_ELECTRO_RESIST]=fm->GetBEDWord();
	Info.Params[ST_EMP_RESIST]=fm->GetBEDWord();
	Info.Params[ST_EXPLODE_RESIST]=fm->GetBEDWord();
	Info.Params[ST_RADIATION_RESISTANCE]=0;fm->GetBEDWord(); // Disregarded
	Info.Params[ST_POISON_RESISTANCE]=0;fm->GetBEDWord(); // Disregarded

	Info.Params[ST_AGE]=fm->GetBEDWord();
	Info.Params[ST_GENDER]=fm->GetBEDWord();

	// Bonus
	Info.Params[ST_STRENGTH]+=fm->GetBEDWord();
	Info.Params[ST_PERCEPTION]+=fm->GetBEDWord();
	Info.Params[ST_ENDURANCE]+=fm->GetBEDWord();
	Info.Params[ST_CHARISMA]+=fm->GetBEDWord();
	Info.Params[ST_INTELLECT]+=fm->GetBEDWord();
	Info.Params[ST_AGILITY]+=fm->GetBEDWord();
	Info.Params[ST_LUCK]+=fm->GetBEDWord();

	Info.Params[ST_MAX_LIFE]+=fm->GetBEDWord();
	Info.Params[ST_ACTION_POINTS]+=fm->GetBEDWord();
	Info.Params[ST_CURRENT_AP]+=0;
	Info.Params[ST_ARMOR_CLASS]+=fm->GetBEDWord();
	Info.Params[ST_UNARMED_DAMAGE]+=fm->GetBEDWord(); // Unarmed Damage
	Info.Params[ST_MELEE_DAMAGE]+=fm->GetBEDWord(); // Melee Damage
	Info.Params[ST_CARRY_WEIGHT]+=fm->GetBEDWord();
	Info.Params[ST_SEQUENCE]+=fm->GetBEDWord();
	Info.Params[ST_HEALING_RATE]+=fm->GetBEDWord();
	Info.Params[ST_CRITICAL_CHANCE]+=fm->GetBEDWord();
	Info.Params[ST_MAX_CRITICAL]+=fm->GetBEDWord();

	Info.Params[ST_NORMAL_ABSORB]+=fm->GetBEDWord();
	Info.Params[ST_LASER_ABSORB]+=fm->GetBEDWord();
	Info.Params[ST_FIRE_ABSORB]+=fm->GetBEDWord();
	Info.Params[ST_PLASMA_ABSORB]+=fm->GetBEDWord();
	Info.Params[ST_ELECTRO_ABSORB]+=fm->GetBEDWord();
	Info.Params[ST_EMP_ABSORB]+=fm->GetBEDWord();
	Info.Params[ST_EXPLODE_ABSORB]+=fm->GetBEDWord();

	Info.Params[ST_NORMAL_RESIST]+=fm->GetBEDWord();
	Info.Params[ST_LASER_RESIST]+=fm->GetBEDWord();
	Info.Params[ST_FIRE_RESIST]+=fm->GetBEDWord();
	Info.Params[ST_PLASMA_RESIST]+=fm->GetBEDWord();
	Info.Params[ST_ELECTRO_RESIST]+=fm->GetBEDWord();
	Info.Params[ST_EMP_RESIST]+=fm->GetBEDWord();
	Info.Params[ST_EXPLODE_RESIST]+=fm->GetBEDWord();
	Info.Params[ST_RADIATION_RESISTANCE]+=fm->GetBEDWord();
	Info.Params[ST_POISON_RESISTANCE]+=fm->GetBEDWord();

	Info.Params[ST_AGE]+=fm->GetBEDWord();
	Info.Params[ST_GENDER]+=fm->GetBEDWord();

	// Skills
	Info.Params[SK_SMALL_GUNS]=fm->GetBEDWord();
	Info.Params[SK_BIG_GUNS]=fm->GetBEDWord();
	Info.Params[SK_ENERGY_WEAPONS]=fm->GetBEDWord();
	Info.Params[SK_UNARMED]=fm->GetBEDWord();
	Info.Params[SK_MELEE_WEAPONS]=fm->GetBEDWord();
	Info.Params[SK_THROWING]=fm->GetBEDWord();
	Info.Params[SK_FIRST_AID]=fm->GetBEDWord();
	Info.Params[SK_DOCTOR]=fm->GetBEDWord();
	Info.Params[SK_SNEAK]=fm->GetBEDWord();
	Info.Params[SK_LOCKPICK]=fm->GetBEDWord();
	Info.Params[SK_STEAL]=fm->GetBEDWord();
	Info.Params[SK_TRAPS]=fm->GetBEDWord();
	Info.Params[SK_SCIENCE]=fm->GetBEDWord();
	Info.Params[SK_REPAIR]=fm->GetBEDWord();
	Info.Params[SK_SPEECH]=fm->GetBEDWord();
	Info.Params[SK_BARTER]=fm->GetBEDWord();
	Info.Params[SK_GAMBLING]=fm->GetBEDWord();
	Info.Params[SK_OUTDOORSMAN]=fm->GetBEDWord();

	Info.Params[SK_SMALL_GUNS]+=5+4*Info.Params[ST_AGILITY];
	Info.Params[SK_BIG_GUNS]+=0+2*Info.Params[ST_AGILITY];
	Info.Params[SK_ENERGY_WEAPONS]+=0+2*Info.Params[ST_AGILITY];
	Info.Params[SK_UNARMED]+=30+2*(Info.Params[ST_AGILITY]+Info.Params[ST_STRENGTH]);
	Info.Params[SK_MELEE_WEAPONS]+=20+2*(Info.Params[ST_AGILITY]+Info.Params[ST_STRENGTH]);
	Info.Params[SK_THROWING]+=0+4*Info.Params[ST_AGILITY];
	Info.Params[SK_FIRST_AID]+=0+2*(Info.Params[ST_PERCEPTION]+Info.Params[ST_INTELLECT]);
	Info.Params[SK_DOCTOR]+=5+Info.Params[ST_PERCEPTION]+Info.Params[ST_INTELLECT];
	Info.Params[SK_SNEAK]+=5+3*Info.Params[ST_AGILITY];
	Info.Params[SK_LOCKPICK]+=10+Info.Params[ST_PERCEPTION]+Info.Params[ST_AGILITY];
	Info.Params[SK_STEAL]+=0+3*Info.Params[ST_AGILITY];
	Info.Params[SK_TRAPS]+=10+Info.Params[ST_PERCEPTION]+Info.Params[ST_AGILITY];
	Info.Params[SK_SCIENCE]+=0+4*Info.Params[ST_INTELLECT];
	Info.Params[SK_REPAIR]+=0+3*Info.Params[ST_INTELLECT];
	Info.Params[SK_SPEECH]+=0+5*Info.Params[ST_CHARISMA];
	Info.Params[SK_BARTER]+=0+4*Info.Params[ST_CHARISMA];
	Info.Params[SK_GAMBLING]+=0+5*Info.Params[ST_LUCK];
	Info.Params[SK_OUTDOORSMAN]+=0+2*(Info.Params[ST_ENDURANCE]+Info.Params[ST_INTELLECT]);

	// Other
	fm->GetBEDWord();
	Experience=fm->GetBEDWord();
	ProtoKillType=fm->GetBEDWord();
#pragma MESSAGE("TODO")
	//if(ProtoKillType>=MAX_KILLS) ProtoKillType=0;
	DamageType=fm->GetBEDWord()+1;
	if(DamageType>DAMAGE_TYPE_EXPLODE) DamageType=DAMAGE_TYPE_NORMAL;
	Info.Params[ST_CURRENT_AP]=Info.Params[ST_ACTION_POINTS]*AP_DIVIDER;

	if(fm->GetFsize()>416)
	{
		fm->SetCurPos(416);
		DialogsId=fm->GetBEDWord();
	}

	Info.Params[ST_MAX_LIFE]+=15;
	Info.Params[ST_CURRENT_HP]=Info.Params[ST_MAX_LIFE]+Info.Params[ST_STRENGTH]+Info.Params[ST_ENDURANCE]*2;
	Info.Params[ST_CURRENT_AP]=(Info.Params[ST_ACTION_POINTS]+5+Info.Params[ST_AGILITY]/2)*AP_DIVIDER;
//	Info.Params[ST_POISONING_LEVEL]=0;
//	Info.Params[ST_RADIATION_LEVEL]=0;
	Info.Params[ST_MAX_CRITICAL]=(Info.Params[ST_MAX_CRITICAL]?20:0);

	//Info.Params[ST_MAX_LIFE]-=Info.Params[ST_STRENGTH]+Info.Params[ST_ENDURANCE]*2;
	//Info.Params[ST_ACTION_POINTS]-=5+Info.Params[ST_AGILITY]/2;
	//Info.Params[ST_UNARMED_DAMAGE]-=(Info.Params[ST_STRENGTH]>6?Info.Params[ST_STRENGTH]-5:1);
	//Info.Params[ST_MELEE_DAMAGE]-=(Info.Params[ST_STRENGTH]>6?Info.Params[ST_STRENGTH]-5:1);
	//Info.Params[ST_ARMOR_CLASS]-=Info.Params[ST_AGILITY];
	//Info.Params[ST_SEQUENCE]-=Info.Params[ST_PERCEPTION]*2;
	//Info.Params[ST_HEALING_RATE]-=Info.Params[ST_ENDURANCE];
	//Info.Params[ST_CRITICAL_CHANCE]-=Info.Params[ST_LUCK];
	//Info.Params[ST_RADIATION_RESISTANCE]-=Info.Params[ST_ENDURANCE]*2;
	//Info.Params[ST_POISON_RESISTANCE]-=Info.Params[ST_ENDURANCE]*5;

	Info.Cond=COND_LIFE;
	Info.CondExt=COND_LIFE_NONE;

	fm->UnloadFile();

	SETFLAG(Flags,FCRIT_NPC);

#define FCRIT_CAN_BARTER        (0x00000002) // Может торговать. +
#define FCRIT_NO_STEAL          (0x00000020) // Нельзя обворовать. +
#define FCRIT_NO_DROP           (0x00000040) // Нельзя сбрасывать предметы. +
#define FCRIT_NO_LOOSE_LIMBS    (0x00000080) // Не может терять конечности. +
#define FCRIT_DEAD_AGES         (0x00000100) // Мёртвое тело не исчезает. ?
#define FCRIT_NO_HEAL           (0x00000200) // Повреждения не излечиваются с течением времени. ?
#define FCRIT_INVULNERABLE      (0x00000400) // Неуязвимый. ?
#define FCRIT_NO_FLATTEN        (0x00000800) // Не помещать труп на задний план после смерти. +
#define FCRIT_SPECIAL_DEAD      (0x00001000) // Есть особый вид смерти. -
#define FCRIT_RANGE_HTH         (0x00002000) // Возможна рукопашная атака на расстоянии. -
#define FCRIT_NO_KNOCK          (0x00004000) // Не может быть сбит с ног. +
	Info.Params[MODE_NO_BARTER]=(FLAG(Flags,FCRIT_CAN_BARTER)?0:1);
	Info.Params[MODE_NO_STEAL]=(FLAG(Flags,FCRIT_NO_STEAL)?1:0);
	Info.Params[MODE_NO_DROP]=(FLAG(Flags,FCRIT_NO_DROP)?1:0);
	Info.Params[MODE_NO_LOOSE_LIMBS]=(FLAG(Flags,FCRIT_NO_LOOSE_LIMBS)?1:0);
	Info.Params[MODE_DEAD_AGES]=(FLAG(Flags,FCRIT_DEAD_AGES)?1:0);
	Info.Params[MODE_NO_HEAL]=(FLAG(Flags,FCRIT_NO_HEAL)?1:0);
	Info.Params[MODE_INVULNERABLE]=(FLAG(Flags,FCRIT_INVULNERABLE)?1:0);
	Info.Params[MODE_NO_FLATTEN]=(FLAG(Flags,FCRIT_NO_FLATTEN)?1:0);
	Info.Params[MODE_SPECIAL_DEAD]=(FLAG(Flags,FCRIT_SPECIAL_DEAD)?1:0);
	Info.Params[MODE_RANGE_HTH]=(FLAG(Flags,FCRIT_RANGE_HTH)?1:0);
	Info.Params[MODE_NO_KNOCK]=(FLAG(Flags,FCRIT_NO_KNOCK)?1:0);

//	static DWORD barter=0,cnt=0;
//	barter+=Info.Params[SK_BARTER];
//	cnt++;
//	WriteLog("Stat barter <%d>, approx<%d>.\n",Info.Params[SK_BARTER],barter/cnt);

//	static DWORD charisma=0,cnt=0;
//	charisma+=Info.Params[ST_ACTION_POINTS]+(5+Info.Params[ST_AGILITY]/2);
//	cnt++;
//	WriteLog("Pid<%u> ap<%d>, approx<%d>.\n",ProtoId,Info.Params[ST_ACTION_POINTS]+(5+Info.Params[ST_AGILITY]/2),charisma/cnt);

//	WriteLog("Skill barter %d<%d>\n",FLAG(Info.Flags,FCRIT_CAN_BARTER)?1:0,Info.Params[SK_BARTER]);
//	WriteLog("Unarmed dmg<%d>\n",Info.Params[ST_UNARMED_DAMAGE]+(Info.Params[ST_STRENGTH]>6?Info.Params[ST_STRENGTH]-5:1));
//	for(int i=0;i<MAX_SKILLS;i++) WriteLog("Skill<%d> == <%d>\n",i,Info.Params[i]);
//	WriteLog("============\n");
* /
	isLoaded=true;
	return true;
}*/