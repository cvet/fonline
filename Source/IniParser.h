#ifndef ___INI_PARSER___
#define ___INI_PARSER___

#include "Common.h"

class IniParser
{
private:
    char*  bufPtr;
    uint   bufLen;
    char   lastApp[ 128 ];
    uint   lastAppPos;
    StrSet cachedApps;
    StrSet cachedKeys;

    void GotoEol( uint& iter );
    bool GotoApp( const char* app_name, uint& iter );
    bool GotoKey( const char* key_name, uint& iter );
    bool GetPos( const char* app_name, const char* key_name, uint& iter );

public:
    IniParser();
    ~IniParser();
    bool  IsLoaded();
    bool  LoadFile( const char* fname, int path_type );
    bool  LoadFilePtr( const char* buf, uint len );
    bool  AppendToBegin( const char* fname, int path_type );
    bool  AppendToEnd( const char* fname, int path_type );
    bool  AppendPtrToBegin( const char* buf, uint len );
    bool  SaveFile( const char* fname, int path_type );
    void  UnloadFile();
    char* GetBuffer();
    int   GetInt( const char* app_name, const char* key_name, int def_val );
    int   GetInt( const char* key_name, int def_val );
    bool  GetStr( const char* app_name, const char* key_name, const char* def_val, char* ret_buf, char end = 0 );
    bool  GetStr( const char* key_name, const char* def_val, char* ret_buf, char end = 0 );
    void  SetStr( const char* app_name, const char* key_name, const char* val ); // If key not founded than writes to end of file despite app
    void  SetStr( const char* key_name, const char* val );
    bool  IsApp( const char* app_name );
    bool  IsKey( const char* app_name, const char* key_name );
    bool  IsKey( const char* key_name );

    char* GetApp( const char* app_name );
    bool  GotoNextApp( const char* app_name );
    void  GetAppLines( StrVec& lines );

    void    CacheApps();
    bool    IsCachedApp( const char* app_name );
    void    CacheKeys();
    bool    IsCachedKey( const char* key_name );
    StrSet& GetCachedKeys();

    static const char* GetConfigFileName();
    static IniParser& GetClientConfig();
    static IniParser& GetServerConfig();
    static IniParser& GetMapperConfig();
};

#endif // ___INI_PARSER___
