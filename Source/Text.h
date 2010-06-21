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

// Adding value to all characters in string
void ChangeValue(char* str, int value);
// Erase selected string
void EraseInterval(char* str, int len);
// Insert selected string
void Insert(char* to_str, const char* from_str);
void Copy(char* to_str, const char* from_str);
//void StrEraseWords(char* str, char* word);
// Erase all words from beginn_char to first end_char
void EraseWords(char* str, char begin_char, char end_char);
void EraseChars(char* str, char c);
// Copy word to to_str from from_str to first end_char
void CopyWord(char* to_str, const char* from_str, char end_char, bool include_end_char=false);
// Parse characters in upper
char* Upr(char* str);
// Parse characters in lower
char* Lwr(char* str);
//BOOL StrCmp(const char* str1, const char* str2);
//BOOL StriCmp(const char* str1, const char* str2);
//char* StrCpy(char* dest, const char* src);
//char* StrCat(char* dest, const char* src);
void CopyBack(char* str);
void Replacement(char* str, char ch);
void Replacement(char* str, char from, char to);
void Replacement(char* str, char from1, char from2, char to);
void SkipLine(char*& str);
void GoTo(char*& str, char ch, bool skip_char = false);
const char* ItoA(int i);
const char* DWtoA(DWORD dw);
const char* Format(const char* fmt, ...);
const char* SFormat(char* stream, const char* fmt, ...);
// Parse str
const char* ParseLineDummy(const char* str);
template<typename Cont, class Func>
void ParseLine(const char* str, char divider, Cont& result, Func f)
{
	result.clear();
	char buf[512]={0};
	for(int buf_pos=0;;str++)
	{
		if(*str==divider || *str=='\0' || buf_pos>=sizeof(buf)-1)
		{
			if(buf_pos)
			{
				buf[buf_pos]=0;
				result.push_back(Cont::value_type(f(buf)));
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
// Msg files
int GetMsgType(const char* type_name);
// Copy of data
char* DuplicateString(const char* str);
// Just big buffer, 65536 bytes
char* GetBigBuf();
// Number or not
bool IsNumber(const char* str);
bool StrToInt(const char* str, int& i);
DWORD GetHash(const char* str);
}

/************************************************************************/
/* Ini parser                                                           */
/************************************************************************/

class IniParser
{
private:
	char* bufPtr;
	DWORD bufLen;
	char lastApp[128];
	DWORD lastAppPos;
	StringSet cachedKeys;

	void GotoEol(DWORD& iter);
	bool GotoApp(const char* app_name, DWORD& iter);
	bool GotoKey(const char* key_name, DWORD& iter);
	bool GetPos(const char* app_name, const char* key_name, DWORD& iter);

public:
	IniParser();
	~IniParser();
	bool IsLoaded();
	bool LoadFile(const char* fname);
	bool LoadFile(const BYTE* buf, DWORD len);
	bool AppendToBegin(const BYTE* buf, DWORD len);
	bool AppendToEnd(const BYTE* buf, DWORD len);
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

	void CacheKeys();
	bool IsCachedKey(const char* key_name);
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
	void AddStr(DWORD num, const char* str);
	void AddStr(DWORD num, const string& str);
	void AddBinary(DWORD num, const BYTE* binary, DWORD len);

	// Generate random number with interval from 10000000 to 99999999
	DWORD AddStr(const char* str);
	const char* GetStr(DWORD num); // Gets const pointer on String value by num in strData
	const char* GetStr(DWORD num, DWORD skip); // Gets const pointer on String value by num with skip in strData
	DWORD GetStrNumUpper(DWORD num); // Gets const pointer on String value by upper num in strData
	DWORD GetStrNumLower(DWORD num); // Gets const pointer on String value by lower num in strData
	int GetInt(DWORD num); // Gets integer value of string
	const BYTE* GetBinary(DWORD num, DWORD& len);
	int Count(DWORD num); // Return count of string exist
	void EraseStr(DWORD num); // Delete string
	DWORD GetSize(); // Gets Size of All Strings, without only FOMSG_ERRNUM
	void CalculateHash(); // Calculate toSend data and hash
	DWORD GetHash(); // Gets Hash code of MSG in toSend
	StringMulMap& GetData(); // Gets strData

#ifdef FONLINE_SERVER
	const char* GetToSend(); // Gets toSend data
	DWORD GetToSendLen(); // Gets toSend Lenght
#endif

#ifdef FONLINE_CLIENT
	// Load MSG from stream, old data is clear
	int LoadMsgStream(CharVec& stream);
#endif

	// Load MSG from file, old data is clear
	int LoadMsgFile(const char* path);
	int LoadMsgFile(char* data, DWORD data_len);
	// Save strData in file, if file is not empty his clear
	int SaveMsgFile(const char* path);
	// Clearing MSG
	void Clear();

private:

#ifdef FONLINE_SERVER
	// Data to send client
	CharVec toSend;
#endif

	// Hash of toSend
	DWORD strDataHash;
	// Numbers and String Values
	StringMulMap strData;
};
typedef vector<FOMsg*> FOMsgVec;

class LanguagePack
{
public:
	DWORD Name;
	char Zero;
	string Path;
	FOMsg Msg[TEXTMSG_COUNT];

	bool Init(const char* path, DWORD name);
	void ChangeName(DWORD new_name);
	int LoadAll();
	const char* GetName();
	const char* GetPath();

	LanguagePack():Name(0),Zero(0){}
	bool operator==(const DWORD& r){return Name==r;}
};

typedef vector<LanguagePack> LangPackVec;
typedef vector<LanguagePack>::iterator LangPackVecIt;

#endif // __TEXT__