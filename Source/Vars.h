#ifndef __VARS__
#define __VARS__

#include "Common.h"

#define VAR_NAME_LEN             (256)
#define VAR_DESC_LEN             (2048)
#define VAR_FNAME_VARS           "_vars.fos"
#define VAR_DESC_MARK            "**********"
#define VAR_CALC_QUEST(tid,val)  ((tid)*1000+(val))

// Types
#define VAR_GLOBAL               (0)
#define VAR_LOCAL                (1)
#define VAR_UNICUM               (2)
#define VAR_LOCAL_LOCATION       (3)
#define VAR_LOCAL_MAP            (4)
#define VAR_LOCAL_ITEM           (5)

// Flags
#define VAR_FLAG_QUEST           (0x1)
#define VAR_FLAG_RANDOM          (0x2)
#define VAR_FLAG_NO_CHECK        (0x4)

// Typedefs
class TemplateVar;
class GameVar;
typedef vector<TemplateVar*> TempVarVec;
typedef vector<TemplateVar*>::iterator TempVarVecIt;
typedef map<DWORD,GameVar*> VarsMap32;
typedef map<DWORD,GameVar*>::iterator VarsMap32It;
typedef map<DWORD,GameVar*>::value_type VarsMap32Val;
typedef map<ULONGLONG,GameVar*> VarsMap64;
typedef map<ULONGLONG,GameVar*>::iterator VarsMap64It;
typedef map<ULONGLONG,GameVar*>::value_type VarsMap64Val;
typedef vector<GameVar*> VarsVec;
typedef vector<GameVar*>::iterator VarsVecIt;


class TemplateVar
{
public:
	int Type;
	WORD TempId;
	string Name;
	string Desc;
	int StartVal;
	int MinVal;
	int MaxVal;
	DWORD Flags;

	VarsMap32 Vars;
	VarsMap64 VarsUnicum;

	bool IsNotUnicum(){return Type!=VAR_UNICUM;}
	bool IsError(){return (!TempId || !Name.size() || (IsNoBorders() && (MinVal>MaxVal || StartVal<MinVal || StartVal>MaxVal)) || (IsQuest() && Type!=VAR_LOCAL));}
	bool IsQuest(){return FLAG(Flags,VAR_FLAG_QUEST);}
	bool IsRandom(){return FLAG(Flags,VAR_FLAG_RANDOM);}
	bool IsNoBorders(){return FLAG(Flags,VAR_FLAG_NO_CHECK);}
	TemplateVar():Type(VAR_GLOBAL),TempId(0),StartVal(0),MinVal(0),MaxVal(0),Flags(0){}
};

#ifdef FONLINE_SERVER
class Critter;

class GameVar
{
public:
	DWORD MasterId;
	DWORD SlaveId;
	int VarValue;
	TemplateVar* VarTemplate;
	DWORD QuestVarIndex;
	WORD Type;
	short RefCount;
	SyncObject Sync;

	GameVar& operator+=(const int _right);
	GameVar& operator-=(const int _right);
	GameVar& operator*=(const int _right);
	GameVar& operator/=(const int _right);
	GameVar& operator=(const int _right);
	GameVar& operator+=(const GameVar& _right);
	GameVar& operator-=(const GameVar& _right);
	GameVar& operator*=(const GameVar& _right);
	GameVar& operator/=(const GameVar& _right);
	GameVar& operator=(const GameVar& _right);
	int operator+(const int _right){return VarValue+_right;}
	int operator-(const int _right){return VarValue-_right;}
	int operator*(const int _right){return VarValue*_right;}
	int operator/(const int _right){return VarValue/_right;}
	int operator+(const GameVar& _right){return VarValue+_right.VarValue;}
	int operator-(const GameVar& _right){return VarValue-_right.VarValue;}
	int operator*(const GameVar& _right){return VarValue*_right.VarValue;}
	int operator/(const GameVar& _right){return VarValue/_right.VarValue;}
	bool operator>(const int _right) const {return VarValue>_right;}
	bool operator<(const int _right) const {return VarValue<_right;}
	bool operator==(const int _right) const {return VarValue==_right;}
	bool operator>=(const int _right) const {return VarValue>=_right;}
	bool operator<=(const int _right) const {return VarValue<=_right;}
	bool operator!=(const int _right) const {return VarValue!=_right;}
	bool operator>(const GameVar& _right) const {return VarValue>_right.VarValue;}
	bool operator<(const GameVar& _right) const {return VarValue<_right.VarValue;}
	bool operator==(const GameVar& _right) const {return VarValue==_right.VarValue;}
	bool operator>=(const GameVar& _right) const {return VarValue>=_right.VarValue;}
	bool operator<=(const GameVar& _right) const {return VarValue<=_right.VarValue;}
	bool operator!=(const GameVar& _right) const {return VarValue!=_right.VarValue;}

	int GetValue(){return VarValue;}
	int GetMin(){return VarTemplate->MinVal;}
	int GetMax(){return VarTemplate->MaxVal;}
	bool IsQuest(){return VarTemplate->IsQuest();}
	DWORD GetQuestStr(){return VAR_CALC_QUEST(VarTemplate->TempId,VarValue);}
	bool IsRandom(){return VarTemplate->IsRandom();}
	TemplateVar* GetTemplateVar(){return VarTemplate;}
	ULONGLONG GetUid(){return (((ULONGLONG)SlaveId)<<32)|((ULONGLONG)MasterId);}
	DWORD GetMasterId(){return MasterId;}
	DWORD GetSlaveId(){return SlaveId;}

	void AddRef(){RefCount++;}
	void Release(){if(!--RefCount) delete this;}

	GameVar(DWORD master_id, DWORD slave_id, TemplateVar* var_template, int val):
		MasterId(master_id),SlaveId(slave_id),VarTemplate(var_template),QuestVarIndex(0),
		Type(var_template->Type),VarValue(val),RefCount(1){MEMORY_PROCESS(MEMORY_VAR,sizeof(GameVar));}
	~GameVar(){MEMORY_PROCESS(MEMORY_VAR,-(int)sizeof(GameVar));}
	private: GameVar(){}
};

// Wrapper
int GameVarAddInt(GameVar& var, const int _right);
int GameVarSubInt(GameVar& var, const int _right);
int GameVarMulInt(GameVar& var, const int _right);
int GameVarDivInt(GameVar& var, const int _right);
int GameVarAddGameVar(GameVar& var, GameVar& _right);
int GameVarSubGameVar(GameVar& var, GameVar& _right);
int GameVarMulGameVar(GameVar& var, GameVar& _right);
int GameVarDivGameVar(GameVar& var, GameVar& _right);
bool GameVarEqualInt(const GameVar& var, const int _right);
int GameVarCmpInt(const GameVar& var, const int _right);
bool GameVarEqualGameVar(const GameVar& var, const GameVar& _right);
int GameVarCmpGameVar(const GameVar& var, const GameVar& _right);
#endif // FONLINE_SERVER

class VarManager
{
private:
	bool isInit;
	string varsPath;
	TempVarVec tempVars;
	StrWordMap varsNames;
	Mutex varsLocker;

	bool LoadTemplateVars(const char* str, TempVarVec& vars); // Return count error

public:
	bool Init(const char* fpath);
	void Finish();
	void Clear();
	bool IsInit(){return isInit;}

	bool UpdateVarsTemplate();
	bool AddTemplateVar(TemplateVar* var);
	void EraseTemplateVar(WORD temp_id);
	TemplateVar* GetTemplateVar(WORD temp_id);
	WORD GetTemplateVarId(const char* var_name);
	bool IsTemplateVarAviable(const char* var_name);
	void SaveTemplateVars();
	TempVarVec& GetTemplateVars(){return tempVars;}

#ifdef FONLINE_SERVER
public:
	void SaveVarsDataFile(void(*save_func)(void*,size_t));
	bool LoadVarsDataFile(FILE* f, int version);
	bool CheckVar(const char* var_name, DWORD master_id, DWORD slave_id, char oper, int val);
	bool CheckVar(WORD temp_id, DWORD master_id, DWORD slave_id, char oper, int val);
	GameVar* ChangeVar(const char* var_name, DWORD master_id, DWORD slave_id, char oper, int val);
	GameVar* ChangeVar(WORD temp_id, DWORD master_id, DWORD slave_id, char oper, int val);
	GameVar* GetVar(const char* var_name, DWORD master_id, DWORD slave_id,  bool create);
	GameVar* GetVar(WORD temp_id, DWORD master_id, DWORD slave_id,  bool create);
	void SwapVars(DWORD id1, DWORD id2);
	DWORD ClearUnusedVars(DwordSet& ids1, DwordSet& ids2, DwordSet& ids_locs, DwordSet& ids_maps, DwordSet& ids_items);
	void GetQuestVars(DWORD master_id, DwordVec& vars);
	VarsVec& GetQuestVars(){return allQuestVars;}
	DWORD GetVarsCount(){return varsCount;}

private:
	VarsVec allQuestVars;
	DWORD varsCount;

	bool CheckVar(GameVar* var, char oper, int val);
	void ChangeVar(GameVar* var, char oper, int val);
	GameVar* CreateVar(DWORD master_id, TemplateVar* tvar);
	GameVar* CreateVarUnicum(ULONGLONG id, DWORD master_id, DWORD slave_id, TemplateVar* tvar);
#endif // FONLINE_SERVER
};

extern VarManager VarMngr;

#endif // __VARS__