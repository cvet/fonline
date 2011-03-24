#ifndef __TEXT__
#define __TEXT__

#include "Defines.h"
#include "Log.h"
#include "Crypt.h"
#include "MsgStr.h"
#include <strstream>

#define TEXTMSG_TEXT            (0)
#define TEXTMSG_DLG             (1)
#define TEXTMSG_ITEM            (2)
#define TEXTMSG_GAME            (3)
#define TEXTMSG_GM              (4)
#define TEXTMSG_COMBAT          (5)
#define TEXTMSG_QUEST           (6)
#define TEXTMSG_HOLO            (7)
#define TEXTMSG_CRAFT           (8)
#define TEXTMSG_INTERNAL        (9)
#define TEXTMSG_COUNT           (10)

#define DEFAULT_LANGUAGE        "russ"
#define MAX_FOTEXT              (2048)

extern char TextMsgFileName[TEXTMSG_COUNT][16];

#define USER_HOLO_TEXTMSG_FILE  "FOHOLOEXT.MSG"
#define USER_HOLO_START_NUM     (100000)
#define USER_HOLO_MAX_TITLE_LEN (40)
#define USER_HOLO_MAX_LEN       (2000)

namespace Str
{

/************************************************************************/
/* Format string                                                        */
/************************************************************************/

// Strings
void ChangeValue(char* str, int value);
void EraseInterval(char* str, int len);
void Insert(char* to, const char* from);
void Copy(char* to, const char* from);
void EraseWords(char* str, char begin, char end);
void EraseChars(char* str, char ch);
void CopyWord(char* to, const char* from, char end, bool include_end = false);
char* Upr(char* str);
char* Lwr(char* str);
void CopyBack(char* str);
void Replacement(char* str, char ch);
void Replacement(char* str, char from, char to);
void Replacement(char* str, char from1, char from2, char to);
void SkipLine(char*& str);
void GoTo(char*& str, char ch, bool skip_char = false);
const char* ItoA(int i);
const char* DWtoA(uint dw);
const char* Format(const char* fmt, ...);
const char* SFormat(char* stream, const char* fmt, ...);
char* EraseFrontBackSpecificChars(char* str);

// Msg files
int GetMsgType(const char* type_name);

// Just big buffer, 1mb
char* GetBigBuf();

// Numbers
bool IsNumber(const char* str);
bool StrToInt(const char* str, int& i);

// Name hash
uint GetHash(const char* str);
const char* GetName(uint hash);
void AddNameHash(const char* name);

// Parse str
const char* ParseLineDummy(const char* str);
template<typename Cont, class Func>
void ParseLine(const char* str, char divider, Cont& result, Func f)
{
	result.clear();
	char buf[MAX_FOTEXT]={0};
	for(uint buf_pos=0;;str++)
	{
		if(*str==divider || *str=='\0' || buf_pos>=sizeof(buf)-1)
		{
			if(buf_pos)
			{
				buf[buf_pos]=0;
				result.push_back(typename Cont::value_type(f(buf)));
				buf[0]=0;
				buf_pos=0;
			}

			if(!*str) break;
			else continue;
		}

		buf[buf_pos]=*str;
		buf_pos++;
	}
}
}

/************************************************************************/
/* Ini parser                                                           */
/************************************************************************/

class IniParser
{
private:
	char* bufPtr;
	uint bufLen;
	char lastApp[128];
	uint lastAppPos;
	StrSet cachedApps;
	StrSet cachedKeys;

	void GotoEol(uint& iter);
	bool GotoApp(const char* app_name, uint& iter);
	bool GotoKey(const char* key_name, uint& iter);
	bool GetPos(const char* app_name, const char* key_name, uint& iter);

public:
	IniParser();
	~IniParser();
	bool IsLoaded();
	bool LoadFile(const char* fname, int path_type);
	bool LoadFilePtr(const char* buf, uint len);
	bool AppendToBegin(const char* fname, int path_type);
	bool AppendToEnd(const char* fname, int path_type);
	bool AppendPtrToBegin(const char* buf, uint len);
	void UnloadFile();
	char* GetBuffer();
	int GetInt(const char* app_name, const char* key_name, int def_val);
	int GetInt(const char* key_name, int def_val);
	bool GetStr(const char* app_name, const char* key_name, const char* def_val, char* ret_buf, char end = 0);
	bool GetStr(const char* key_name, const char* def_val, char* ret_buf, char end = 0);
	bool IsApp(const char* app_name);
	bool IsKey(const char* app_name, const char* key_name);
	bool IsKey(const char* key_name);

	char* GetApp(const char* app_name);
	bool GotoNextApp(const char* app_name);
	void GetAppLines(StrVec& lines);

	void CacheApps();
	bool IsCachedApp(const char* app_name);
	void CacheKeys();
	bool IsCachedKey(const char* key_name);
	StrSet& GetCachedKeys();
};

/************************************************************************/
/* FOMsg                                                                */
/************************************************************************/

#define FOMSG_ERRNUM            (0)
#define FOMSG_VERNUM            (1)

class FOMsg
{
public:
	FOMsg(); // Insert FOMSG_ERRNUM into strData
	FOMsg& operator+=(const FOMsg& r);

	// Add String value in strData and Nums
	void AddStr(uint num, const char* str);
	void AddStr(uint num, const string& str);
	void AddBinary(uint num, const uchar* binary, uint len);

	// Generate random number with interval from 10000000 to 99999999
	uint AddStr(const char* str);
	const char* GetStr(uint num); // Gets const pointer on String value by num in strData
	const char* GetStr(uint num, uint skip); // Gets const pointer on String value by num with skip in strData
	uint GetStrNumUpper(uint num); // Gets const pointer on String value by upper num in strData
	uint GetStrNumLower(uint num); // Gets const pointer on String value by lower num in strData
	int GetInt(uint num); // Gets integer value of string
	const uchar* GetBinary(uint num, uint& len);
	int Count(uint num); // Return count of string exist
	void EraseStr(uint num); // Delete string
	uint GetSize(); // Gets Size of All Strings, without only FOMSG_ERRNUM
	void CalculateHash(); // Calculate toSend data and hash
	uint GetHash(); // Gets Hash code of MSG in toSend
	UIntStrMulMap& GetData(); // Gets strData

#ifdef FONLINE_SERVER
	const char* GetToSend(); // Gets toSend data
	uint GetToSendLen(); // Gets toSend Lenght
#endif

#ifdef FONLINE_CLIENT
	// Load MSG from stream, old data is clear
	int LoadMsgStream(CharVec& stream);
#endif

	// Load MSG from file, old data is clear
	int LoadMsgFile(const char* fname, int path_type);
	int LoadMsgFileBuf(char* data, uint data_len);
	// Save strData in file, if file is not empty his clear
	int SaveMsgFile(const char* fname, int path_type);
	// Clearing MSG
	void Clear();

private:

#ifdef FONLINE_SERVER
	// Data to send client
	CharVec toSend;
#endif

	// Hash of toSend
	uint strDataHash;
	// Numbers and String Values
	UIntStrMulMap strData;
};
typedef vector<FOMsg*> FOMsgVec;

class LanguagePack
{
public:
	union
	{
		uint Name;
		char NameStr[5];
	};

	int PathType;
	FOMsg Msg[TEXTMSG_COUNT];

	bool Init(const char* lang, int path_type);
	int LoadAll();

	LanguagePack(){ZeroMemory(NameStr,sizeof(NameStr));}
	bool operator==(const uint& r){return Name==r;}
};

typedef vector<LanguagePack> LangPackVec;
typedef vector<LanguagePack>::iterator LangPackVecIt;

#endif // __TEXT__
