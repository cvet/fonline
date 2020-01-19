#pragma once

#include "Common.h"

#include "CacheStorage.h"
#include "FileSystem.h"
#include "MsgStr_Include.h"

#define TEXTMSG_TEXT (0)
#define TEXTMSG_DLG (1)
#define TEXTMSG_ITEM (2)
#define TEXTMSG_GAME (3)
#define TEXTMSG_GM (4)
#define TEXTMSG_COMBAT (5)
#define TEXTMSG_QUEST (6)
#define TEXTMSG_HOLO (7)
#define TEXTMSG_INTERNAL (8)
#define TEXTMSG_LOCATIONS (9)
#define TEXTMSG_COUNT (10)
extern string TextMsgFileName[TEXTMSG_COUNT];

#define DEFAULT_LANGUAGE "russ"
#define MSG_ERROR_MESSAGE "error"

typedef multimap<uint, string> FOMsgMap;

class FOMsg
{
public:
    FOMsg();
    FOMsg(const FOMsg& other);
    ~FOMsg();
    FOMsg& operator=(const FOMsg& other);
    FOMsg& operator+=(const FOMsg& other);

    void AddStr(uint num, const string& str);
    void AddBinary(uint num, const uchar* binary, uint len);

    string GetStr(uint num);
    string GetStr(uint num, uint skip);
    uint GetStrNumUpper(uint num);
    uint GetStrNumLower(uint num);
    int GetInt(uint num);
    uint GetBinary(uint num, UCharVec& data);
    uint Count(uint num);
    void EraseStr(uint num);
    uint GetSize();
    bool IsIntersects(const FOMsg& other);

    // Serialization
    void GetBinaryData(UCharVec& data);
    bool LoadFromBinaryData(const UCharVec& data);
    bool LoadFromString(const string& str);
    void LoadFromMap(const StrMap& kv);
    void Clear();

private:
    // String values
    FOMsgMap strData;

public:
    static int GetMsgType(const string& type_name);
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

    bool IsAllMsgLoaded;
    FOMsg Msg[TEXTMSG_COUNT];

    void LoadFromFiles(FileManager& file_mngr, const string& lang_name);
    void LoadFromCache(CacheStorage& cache, const string& lang_name);
    string GetMsgCacheName(int msg_num);

    LanguagePack();
    bool operator==(const uint& other) { return Name == other; }
};
using LangPackVec = vector<LanguagePack>;
