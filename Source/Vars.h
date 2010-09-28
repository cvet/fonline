#ifndef __VARS__
#define __VARS__

#include "Common.h"

#define VAR_NAME_LEN            (256)
#define VAR_DESC_LEN            (2048)
#define VAR_FNAME_VARS          "_vars.fos"
#define VAR_DESC_MARK           "**********"
#define VAR_CALC_QUEST(tid,val) ((tid)*1000+(val))

// Types
#define VAR_GLOBAL              (0)
#define VAR_LOCAL               (1)
#define VAR_UNICUM              (2)

// Flags
#define VAR_FLAG_QUEST          (0x1)
#define VAR_FLAG_RANDOM         (0x2)
#define VAR_FLAG_NO_CHECK       (0x4)

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

	bool IsError(){return (!TempId || !Name.size() || (IsNoBorders() && (MinVal>MaxVal || StartVal<MinVal || StartVal>MaxVal)));}
	bool IsQuest(){return FLAG(Flags,VAR_FLAG_QUEST);}
	bool IsRandom(){return FLAG(Flags,VAR_FLAG_RANDOM);}
	bool IsNoBorders(){return FLAG(Flags,VAR_FLAG_NO_CHECK);}
	TemplateVar():Type(VAR_GLOBAL),TempId(0),StartVal(0),MinVal(0),MaxVal(0),Flags(0){}
};

typedef vector<TemplateVar*> TempVarVec;
typedef vector<TemplateVar*>::iterator TempVarVecIt;
typedef map<WORD,TemplateVar*,less<WORD>> TempVarMap;
typedef map<WORD,TemplateVar*,less<WORD>>::iterator TempVarMapIt;
typedef map<WORD,TemplateVar*,less<WORD>>::value_type TempVarMapVal;

#ifdef FONLINE_SERVER
class Critter;

class GameVar
{
public:
	ULONGLONG VarId;
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
	DWORD GetMasterId(){return (VarId>>24)&0xFFFFFF;}
	DWORD GetSlaveId(){return VarId&0xFFFFFF;}
	void SetVarId(DWORD master_id, DWORD slave_id){VarId=((ULONGLONG)VarTemplate->TempId<<48)|((ULONGLONG)master_id<<24)|slave_id;}

	void AddRef(){RefCount++;}
	void Release(){if(!--RefCount) delete this;}

	GameVar(ULONGLONG id, TemplateVar* var_template, int val):VarId(id),VarTemplate(var_template),QuestVarIndex(0),Type(var_template->Type),VarValue(val),RefCount(1){MEMORY_PROCESS(MEMORY_VAR,sizeof(GameVar));}
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

typedef map<ULONGLONG,GameVar*,less<ULONGLONG>> VarsMap;
typedef map<ULONGLONG,GameVar*,less<ULONGLONG>>::iterator VarsMapIt;
typedef map<ULONGLONG,GameVar*,less<ULONGLONG>>::value_type VarsMapVal;
typedef vector<GameVar*> VarsVec;
typedef vector<GameVar*>::iterator VarsVecIt;
#endif // FONLINE_SERVER

class VarManager
{
private:
	bool isInit;
	string varsPath;
	TempVarMap tempVars;
	StrWordMap varsNames;
	Mutex varsLocker;

	bool LoadTemplateVars(FILE* f, TempVarVec& vars); // Return count error

public:
	bool Init(const char* fpath);
	void Finish();
	bool IsInit(){return isInit;}

	bool UpdateVarsTemplate(); // Return count error
	bool AddTemplateVar(TemplateVar* var);
	void EraseTemplateVar(WORD temp_id);
	TemplateVar* GetTempVar(WORD temp_id);
	WORD GetTempVarId(const string& var_name);
	void SaveTemplateVars();
	StrWordMap& GetVarNames(){return varsNames;}

#ifdef FONLINE_SERVER
public:
	void SaveVarsDataFile(void(*save_func)(void*,size_t));
	bool LoadVarsDataFile(FILE* f);
	bool CheckVar(const string& var_name, DWORD master_id, DWORD slave_id, char oper, int val);
	bool CheckVar(WORD template_id, DWORD master_id, DWORD slave_id, char oper, int val);
	GameVar* ChangeVar(const string& var_name, DWORD master_id, DWORD slave_id, char oper, int val);
	GameVar* ChangeVar(WORD template_id, DWORD master_id, DWORD slave_id, char oper, int val);
	bool IsAviableVar(const string& name){return varsNames.find(name)!=varsNames.end();}
	GameVar* GetVar(const string& name, DWORD master_id, DWORD slave_id,  bool create);
	GameVar* GetVar(WORD temp_id, DWORD master_id, DWORD slave_id,  bool create);
	void SwapVars(DWORD id1, DWORD id2);
	DWORD DeleteVars(DWORD id);
	DWORD ClearUnusedVars(DwordSet& ids1, DwordSet& ids2);
	void GetQuestVars(DWORD master_id, DwordVec& vars);
	VarsVec& GetQuestVars(){return allQuestVars;}
	VarsMap& GetVars(){return allVars;}
	DWORD GetVarsCount(){return allVars.size();}

private:
	VarsMap allVars;
	VarsVec allQuestVars;

	bool CheckVar(GameVar* var, char oper, int val);
	void ChangeVar(GameVar* var, char oper, int val);
	GameVar* CreateVar(ULONGLONG id, TemplateVar* tvar);
#endif // FONLINE_SERVER
};

extern VarManager VarMngr;

#endif // __VARS__