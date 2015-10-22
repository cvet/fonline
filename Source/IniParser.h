#ifndef ___INI_PARSER___
#define ___INI_PARSER___

#include "Common.h"

class IniParser
{
private:
    typedef multimap< string, StrMap > ValuesMap;
    ValuesMap appKeyValues;

    void    ParseStr( const char* str );
    string* GetRawValue( const char* app_name, const char* key_name );

public:
    IniParser();
    IniParser( const char* str );
    IniParser( const char* fname, int path_type );
    void AppendStr( const char* buf );
    bool AppendFile( const char* fname, int path_type );
    bool SaveFile( const char* fname, int path_type );
    void Clear();
    bool IsLoaded();

    int         GetInt( const char* app_name, const char* key_name, int def_val = 0 );
    const char* GetStr( const char* app_name, const char* key_name, const char* def_val = nullptr );

    void SetStr( const char* app_name, const char* key_name, const char* val );

    bool IsApp( const char* app_name );
    bool IsKey( const char* app_name, const char* key_name );

    void          GetAppNames( StrSet& apps );
    void          GotoNextApp( const char* app_name );
    const StrMap* GetAppKeyValues( const char* app_name );
    const char*   GetAppContent( const char* app_name );

    static const char* GetConfigFileName();
    static IniParser& GetClientConfig();
    static IniParser& GetServerConfig();
    static IniParser& GetMapperConfig();
};

#endif // ___INI_PARSER___
