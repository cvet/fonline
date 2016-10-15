#ifndef ___INI_PARSER___
#define ___INI_PARSER___

#include "Common.h"

#define CONFIG_NAME    "FOnline.cfg"

class IniParser
{
private:
    typedef multimap< string, StrMap >          ValuesMap;
    typedef vector< ValuesMap::const_iterator > ValuesMapItVec;
    ValuesMap      appKeyValues;
    ValuesMapItVec appKeyValuesOrder;
    bool           collectContent = false;

    void    ParseStr( const char* str );
    string* GetRawValue( const char* app_name, const char* key_name );

public:
    IniParser();
    IniParser( const char* str );
    IniParser( const char* fname, int path_type );
    void CollectContent() { collectContent = true; }
    void AppendStr( const char* buf );
    bool AppendFile( const char* fname, int path_type );
    bool SaveFile( const char* fname, int path_type );
    void Clear();
    bool IsLoaded();

    const char* GetStr( const char* app_name, const char* key_name, const char* def_val = nullptr );
    int         GetInt( const char* app_name, const char* key_name, int def_val = 0 );

    void SetStr( const char* app_name, const char* key_name, const char* val );
    void SetInt( const char* app_name, const char* key_name, int val );

    StrMap& GetApp( const char* app_name );
    void    GetApps( const char* app_name, PStrMapVec& key_values );
    StrMap& SetApp( const char* app_name );

    bool IsApp( const char* app_name );
    bool IsKey( const char* app_name, const char* key_name );

    void          GetAppNames( StrSet& apps );
    void          GotoNextApp( const char* app_name );
    const StrMap* GetAppKeyValues( const char* app_name );
    const char*   GetAppContent( const char* app_name );
};

#endif // ___INI_PARSER___
