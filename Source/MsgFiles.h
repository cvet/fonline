#ifndef ___MSG_FILES___
#define ___MSG_FILES___

#include "Common.h"
#include "MsgStr.h"

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
extern const char* TextMsgFileName[TEXTMSG_COUNT];

#define DEFAULT_LANGUAGE        "russ"

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
	uint GetToSendLen(); // Gets toSend Length
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

public:
	static int GetMsgType(const char* type_name);
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

	LanguagePack(){memset(NameStr,0,sizeof(NameStr));}
	bool operator==(const uint& r){return Name==r;}
};

typedef vector<LanguagePack> LangPackVec;
typedef vector<LanguagePack>::iterator LangPackVecIt;

#endif // ___MSG_FILES___
