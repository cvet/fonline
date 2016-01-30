#ifndef ___MSG_FILES___
#define ___MSG_FILES___

#include "Common.h"
#include "MsgStr.h"

#define TEXTMSG_TEXT         ( 0 )
#define TEXTMSG_DLG          ( 1 )
#define TEXTMSG_ITEM         ( 2 )
#define TEXTMSG_GAME         ( 3 )
#define TEXTMSG_GM           ( 4 )
#define TEXTMSG_COMBAT       ( 5 )
#define TEXTMSG_QUEST        ( 6 )
#define TEXTMSG_HOLO         ( 7 )
#define TEXTMSG_CRAFT        ( 8 )
#define TEXTMSG_INTERNAL     ( 9 )
#define TEXTMSG_LOCATIONS    ( 10 )
#define TEXTMSG_COUNT        ( 11 )
extern const char* TextMsgFileName[ TEXTMSG_COUNT ];

#define DEFAULT_LANGUAGE     "russ"
#define MSG_ERROR_MESSAGE    "error"

typedef multimap< uint, const char* > FOMsgMap;

class FOMsg
{
public:
    FOMsg();
    FOMsg( const FOMsg& other );
    ~FOMsg();
    FOMsg& operator=( const FOMsg& other );
    FOMsg& operator+=( const FOMsg& other );

    void AddStr( uint num, const char* str );
    void AddStr( uint num, const string& str );
    void AddBinary( uint num, const uchar* binary, uint len );

    const char* GetStr( uint num );
    const char* GetStr( uint num, uint skip );
    uint        GetStrNumUpper( uint num );
    uint        GetStrNumLower( uint num );
    int         GetInt( uint num );
    uint        GetBinary( uint num, UCharVec& data );
    uint        Count( uint num );
    void        EraseStr( uint num );
    uint        GetSize();
    bool        IsIntersects( const FOMsg& other );

    // Serialization
    void GetBinaryData( UCharVec& data );
    bool LoadFromBinaryData( const UCharVec& data );
    bool LoadFromFile( const char* fname, int path_type );
    bool LoadFromString( const char* str, uint str_len );
    void LoadFromMap( const StrMap& kv );
    bool SaveToFile( const char* fname, int path_type );
    void Clear();

private:
    // String values
    FOMsgMap strData;

public:
    static int GetMsgType( const char* type_name );
};
typedef vector< FOMsg* > FOMsgVec;

class LanguagePack
{
public:
    union
    {
        uint Name;
        char NameStr[ 5 ];
    };

    bool  IsAllMsgLoaded;
    FOMsg Msg[ TEXTMSG_COUNT ];

    bool  LoadFromFiles( const char* lang_name );
    bool  LoadFromCache( const char* lang_name );
    char* GetMsgCacheName( int msg_num, char* result );

    LanguagePack();
    bool operator==( const uint& other ) { return Name == other; }
};
typedef vector< LanguagePack > LangPackVec;

#endif // ___MSG_FILES___
