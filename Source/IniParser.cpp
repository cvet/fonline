#include "StdAfx.h"
#include "IniParser.h"
#include "Crypt.h"
#include "FileManager.h"

#define STR_PRIVATE_APP_BEGIN    '['
#define STR_PRIVATE_APP_END      ']'
#define STR_PRIVATE_KEY_CHAR     '='

IniParser::IniParser()
{
    bufPtr = NULL;
    bufLen = 0;
    lastApp[ 0 ] = 0;
    lastAppPos = 0;
}

IniParser::~IniParser()
{
    UnloadFile();
}

bool IniParser::IsLoaded()
{
    return bufPtr != NULL;
}

bool IniParser::LoadFile( const char* fname, int path_type )
{
    UnloadFile();

    FileManager fm;
    if( !fm.LoadFile( fname, path_type ) )
        return false;

    bufPtr = (char*) fm.ReleaseBuffer();
    Str::Replacement( bufPtr, '\r', ' ' );
    bufLen = Str::Length( bufPtr );
    return true;
}

void IniParser::LoadFilePtr( const char* buf, uint len )
{
    UnloadFile();

    bufPtr = new char[ len + 1 ];
    memcpy( bufPtr, buf, len );
    bufPtr[ len ] = 0;
    Str::Replacement( bufPtr, '\r', ' ' );
    bufLen = Str::Length( bufPtr );
}

bool IniParser::AppendToBegin( const char* fname, int path_type )
{
    FileManager fm;
    if( !fm.LoadFile( fname, path_type ) )
        return false;
    char* buf = (char*) fm.ReleaseBuffer();
    Str::Replacement( buf, '\r', ' ' );
    uint  len = Str::Length( buf );

    char* grow_buf = new char[ bufLen + len + 1 ];
    memcpy( grow_buf, buf, len );
    if( bufLen )
        memcpy( grow_buf + len, bufPtr, bufLen );
    grow_buf[ bufLen + len ] = 0;

    SAFEDELA( bufPtr );
    SAFEDELA( buf );
    bufPtr = grow_buf;
    bufLen += len;
    lastApp[ 0 ] = 0;
    lastAppPos = 0;
    return true;
}

bool IniParser::AppendToEnd( const char* fname, int path_type )
{
    FileManager fm;
    if( !fm.LoadFile( fname, path_type ) )
        return false;
    char* buf = (char*) fm.ReleaseBuffer();
    Str::Replacement( buf, '\r', ' ' );
    uint  len = Str::Length( buf );

    char* grow_buf = new char[ bufLen + len + 1 ];
    if( bufLen )
        memcpy( grow_buf, bufPtr, bufLen );
    memcpy( grow_buf + bufLen, buf, len );
    grow_buf[ bufLen + len ] = 0;

    SAFEDELA( bufPtr );
    SAFEDELA( buf );
    bufPtr = grow_buf;
    bufLen += len;
    lastApp[ 0 ] = 0;
    lastAppPos = 0;
    return true;
}

bool IniParser::AppendPtrToBegin( const char* buf, uint len )
{
    char* grow_buf = new char[ bufLen + len + 1 ];
    memcpy( grow_buf, buf, len );
    grow_buf[ len ] = 0;
    Str::Replacement( grow_buf, '\r', ' ' );
    len = Str::Length( grow_buf );
    if( bufLen )
        memcpy( grow_buf + len, bufPtr, bufLen );
    grow_buf[ bufLen + len ] = 0;

    SAFEDELA( bufPtr );
    bufPtr = grow_buf;
    bufLen += len;
    lastApp[ 0 ] = 0;
    lastAppPos = 0;
    return true;
}

bool IniParser::SaveFile( const char* fname, int path_type )
{
    FileManager fm;
    fm.SetData( bufPtr, bufLen );
    return fm.SaveOutBufToFile( fname, path_type );
}

void IniParser::UnloadFile()
{
    SAFEDELA( bufPtr );
    bufLen = 0;
    lastApp[ 0 ] = 0;
    lastAppPos = 0;
}

char* IniParser::GetBuffer()
{
    return bufPtr;
}

void IniParser::GotoEol( uint& iter )
{
    for( ; iter < bufLen; ++iter )
        if( bufPtr[ iter ] == '\n' )
            return;
}

bool IniParser::GotoApp( const char* app_name, uint& iter )
{
    // Skip UTF-8 BOM signature
    if( iter == 0 && bufLen >= 3 && (uchar) bufPtr[ 0 ] == 0xEF && (uchar) bufPtr[ 1 ] == 0xBB && (uchar) bufPtr[ 2 ] == 0xBF )
        iter += 3;

    uint j;
    for( ; iter < bufLen; ++iter )
    {
        // Skip white spaces and tabs
        while( iter < bufLen && ( bufPtr[ iter ] == ' ' || bufPtr[ iter ] == '\t' ) )
            ++iter;
        if( iter >= bufLen )
            break;

        if( bufPtr[ iter ] == STR_PRIVATE_APP_BEGIN )
        {
            ++iter;
            for( j = 0; app_name[ j ]; ++iter, ++j )
            {
                if( iter >= bufLen )
                    return false;
                if( app_name[ j ] != bufPtr[ iter ] )
                    goto label_NextLine;
            }

            if( bufPtr[ iter ] != STR_PRIVATE_APP_END )
                goto label_NextLine;

            GotoEol( iter );
            ++iter;

            Str::Copy( lastApp, app_name );
            lastAppPos = iter;
            return true;
        }

label_NextLine:
        GotoEol( iter );
    }
    return false;
}

bool IniParser::GotoKey( const char* key_name, uint& iter )
{
    // Skip UTF-8 BOM signature
    if( iter == 0 && bufLen >= 3 && (uchar) bufPtr[ 0 ] == 0xEF && (uchar) bufPtr[ 1 ] == 0xBB && (uchar) bufPtr[ 2 ] == 0xBF )
        iter += 3;

    uint j;
    for( ; iter < bufLen; ++iter )
    {
        if( bufPtr[ iter ] == STR_PRIVATE_APP_BEGIN )
            return false;

        // Skip white spaces and tabs
        while( iter < bufLen && ( bufPtr[ iter ] == ' ' || bufPtr[ iter ] == '\t' ) )
            ++iter;
        if( iter >= bufLen )
            break;

        // Compare first character
        if( bufPtr[ iter ] == key_name[ 0 ] )
        {
            // Compare others
            ++iter;
            for( j = 1; key_name[ j ]; ++iter, ++j )
            {
                if( iter >= bufLen )
                    return false;
                if( key_name[ j ] != bufPtr[ iter ] )
                    goto label_NextLine;
            }

            // Skip white spaces and tabs
            while( iter < bufLen && ( bufPtr[ iter ] == ' ' || bufPtr[ iter ] == '\t' ) )
                ++iter;
            if( iter >= bufLen )
                break;

            // Check for '='
            if( bufPtr[ iter ] != STR_PRIVATE_KEY_CHAR )
                goto label_NextLine;
            ++iter;

            return true;
        }

label_NextLine:
        GotoEol( iter );
    }

    return false;
}

bool IniParser::GetPos( const char* app_name, const char* key_name, uint& iter )
{
    // Check
    if( !key_name || !key_name[ 0 ] )
        return false;

    // Find
    iter = 0;
    if( app_name && lastAppPos && Str::Compare( app_name, lastApp ) )
    {
        iter = lastAppPos;
    }
    else if( app_name )
    {
        if( !GotoApp( app_name, iter ) )
            return false;
    }

    if( !GotoKey( key_name, iter ) )
        return false;
    return true;
}

int IniParser::GetInt( const char* app_name, const char* key_name, int def_val )
{
    if( !bufPtr )
        return def_val;

    // Get pos
    uint iter;
    if( !GetPos( app_name, key_name, iter ) )
        return def_val;

    // Read number
    uint j;
    char num[ 64 ];
    for( j = 0; iter < bufLen; ++iter, ++j )
    {
        if( j >= 63 || bufPtr[ iter ] == '\n' || bufPtr[ iter ] == '#' || bufPtr[ iter ] == ';' )
            break;
        num[ j ] = bufPtr[ iter ];
    }

    if( !j )
        return def_val;
    num[ j ] = 0;
    Str::Trim( num );
    if( Str::CompareCase( num, "true" ) )
        return 1;
    if( Str::CompareCase( num, "false" ) )
        return 0;
    return atoi( num );
}

int IniParser::GetInt( const char* key_name, int def_val )
{
    return GetInt( NULL, key_name, def_val );
}

bool IniParser::GetStr( const char* app_name, const char* key_name, const char* def_val, char* ret_buf, char end /* = 0 */ )
{
    uint iter = 0, j = 0;

    // Check
    if( !ret_buf )
        goto label_DefVal;
    if( !bufPtr )
        goto label_DefVal;

    // Get pos
    if( !GetPos( app_name, key_name, iter ) )
        goto label_DefVal;

    // Skip white spaces and tabs
    while( iter < bufLen && ( bufPtr[ iter ] == ' ' || bufPtr[ iter ] == '\t' ) )
        ++iter;
    if( iter >= bufLen )
        goto label_DefVal;

    // Read string
    for( ; iter < bufLen && bufPtr[ iter ]; ++iter )
    {
        if( end )
        {
            if( bufPtr[ iter ] == end )
                break;

            if( bufPtr[ iter ] == '\n' )
            {
                if( j && ret_buf[ j - 1 ] != ' ' )
                    ret_buf[ j++ ] = ' ';
                continue;
            }
            if( bufPtr[ iter ] == ';' || bufPtr[ iter ] == '#' )
            {
                if( j && ret_buf[ j - 1 ] != ' ' )
                    ret_buf[ j++ ] = ' ';
                GotoEol( iter );
                continue;
            }
        }
        else
        {
            if( !bufPtr[ iter ] || bufPtr[ iter ] == '\n' || bufPtr[ iter ] == '#' || bufPtr[ iter ] == ';' )
                break;
        }

        ret_buf[ j ] = bufPtr[ iter ];
        j++;
    }

    // Erase white spaces and tabs from end
    for( ; j; j-- )
        if( ret_buf[ j - 1 ] != ' ' && ret_buf[ j - 1 ] != '\t' )
            break;

    ret_buf[ j ] = 0;
    return true;

label_DefVal:
    ret_buf[ 0 ] = 0;
    if( def_val )
        Str::Copy( ret_buf, MAX_FOTEXT, def_val );
    return false;
}

bool IniParser::GetStr( const char* key_name, const char* def_val, char* ret_buf, char end /* = 0 */ )
{
    return GetStr( NULL, key_name, def_val, ret_buf, end );
}

void IniParser::SetStr( const char* app_name, const char* key_name, const char* val )
{
    uint iter = 0;
    if( bufPtr && GetPos( app_name, key_name, iter ) )
    {
        // Refresh founded field
        while( iter < bufLen && ( bufPtr[ iter ] == ' ' || bufPtr[ iter ] == '\t' ) )
            ++iter;

        uint        start = iter;
        uint        count = 0;
        const char* end_str = Str::Substring( &bufPtr[ start ], "\n" );
        if( !end_str )
            end_str = &bufPtr[ bufLen ];

        const char* comment = Str::Substring( &bufPtr[ start ], "#" );
        if( comment && comment < end_str )
            end_str = comment;
        comment = Str::Substring( &bufPtr[ start ], ";" );
        if( comment && comment < end_str )
            end_str = comment;

        count = (uint) ( (size_t) end_str - (size_t) ( &bufPtr[ start ] ) );

        uint val_len = Str::Length( val );
        if( val_len > count )
        {
            char* grow_buf = new char[ bufLen + val_len - count + 1 ];
            memcpy( grow_buf, bufPtr, bufLen );
            grow_buf[ bufLen ] = 0;
            delete[] bufPtr;
            bufPtr = grow_buf;
        }

        if( count )
            Str::EraseInterval( &bufPtr[ start ], count );
        Str::Insert( &bufPtr[ start ], val );
        bufLen = bufLen + val_len - count;
    }
    else
    {
        // Write new field
        bool new_line = false;
        if( bufPtr && bufPtr[ bufLen - 1 ] != '\n' )
            new_line = true;

        uint  key_name_len = Str::Length( key_name );
        uint  val_len = Str::Length( val );
        uint  new_len = bufLen + ( new_line ? 1 : 0 ) + key_name_len + 3 + val_len + 1; // {[\n]key_name = val\n}
        char* new_buf = new char[ new_len + 1 ];
        if( bufPtr )
            memcpy( new_buf, bufPtr, bufLen );

        uint pos = bufLen;
        if( new_line )
            new_buf[ pos++ ] = '\n';
        memcpy( &new_buf[ pos ], key_name, key_name_len );
        pos += key_name_len;
        new_buf[ pos++ ] = ' ';
        new_buf[ pos++ ] = '=';
        new_buf[ pos++ ] = ' ';
        memcpy( &new_buf[ pos ], val, val_len );
        pos += val_len;
        new_buf[ pos++ ] = '\n';
        new_buf[ pos ] = 0;

        SAFEDELA( bufPtr );
        bufPtr = new_buf;
        bufLen = new_len;
    }
}

void IniParser::SetStr( const char* key_name, const char* val )
{
    return SetStr( NULL, key_name, val );
}

bool IniParser::IsApp( const char* app_name )
{
    if( !bufPtr )
        return false;
    uint iter = 0;
    return GotoApp( app_name, iter );
}

bool IniParser::IsKey( const char* app_name, const char* key_name )
{
    if( !bufPtr )
        return false;
    uint iter = 0;
    return GetPos( app_name, key_name, iter );
}

bool IniParser::IsKey( const char* key_name )
{
    return IsKey( NULL, key_name );
}

char* IniParser::GetApp( const char* app_name )
{
    if( !bufPtr )
        return NULL;
    if( !app_name )
        return NULL;

    uint iter = 0;
    if( lastAppPos && Str::Compare( app_name, lastApp ) )
        iter = lastAppPos;
    else if( !GotoApp( app_name, iter ) )
        return NULL;

    uint i = iter, len = 0;
    for( ; i < bufLen; i++, len++ )
        if( i > 0 && bufPtr[ i - 1 ] == '\n' && bufPtr[ i ] == STR_PRIVATE_APP_BEGIN )
            break;
    for( ; len; i--, len-- )
        if( i > 0 && bufPtr[ i - 1 ] != '\n' )
            break;

    char* ret_buf = new char[ len + 1 ];
    if( len )
        memcpy( ret_buf, &bufPtr[ iter ], len );
    ret_buf[ len ] = 0;
    return ret_buf;
}

bool IniParser::GotoNextApp( const char* app_name )
{
    if( !bufPtr )
        return false;
    return GotoApp( app_name, lastAppPos );
}

void IniParser::GetAppLines( StrVec& lines )
{
    if( !bufPtr )
        return;
    uint i = lastAppPos, len = 0;
    for( ; i < bufLen; i++, len++ )
        if( i > 0 && bufPtr[ i - 1 ] == '\n' && bufPtr[ i ] == STR_PRIVATE_APP_BEGIN )
            break;
    for( ; len; i--, len-- )
        if( i > 0 && bufPtr[ i - 1 ] != '\n' )
            break;

    istrstream str( &bufPtr[ lastAppPos ], len );
    char       line[ MAX_FOTEXT ];
    while( !str.eof() )
    {
        str.getline( line, sizeof( line ), '\n' );
        lines.push_back( line );
    }
}

void IniParser::CacheApps()
{
    if( !bufPtr )
        return;

    istrstream str( bufPtr, bufLen );
    char       line[ MAX_FOTEXT ];
    while( !str.eof() )
    {
        str.getline( line, sizeof( line ), '\n' );
        if( line[ 0 ] == STR_PRIVATE_APP_BEGIN )
        {
            char* end = Str::Substring( line + 1, "]" );
            if( end )
            {
                int len = (int) ( end - line - 1 );
                if( len > 0 )
                {
                    string app;
                    app.assign( line + 1, len );
                    cachedApps.insert( app );
                }
            }
        }
    }
}

bool IniParser::IsCachedApp( const char* app_name )
{
    return cachedApps.count( string( app_name ) ) != 0;
}

StrSet& IniParser::GetCachedApps()
{
    return cachedApps;
}

void IniParser::CacheKeys()
{
    if( !bufPtr )
        return;

    istrstream str( bufPtr, bufLen );
    char       line[ MAX_FOTEXT ];
    while( !str.eof() )
    {
        str.getline( line, sizeof( line ), '\n' );
        char* key_end = Str::Substring( line, "=" );
        if( key_end )
        {
            *key_end = 0;
            Str::Trim( line );
            if( Str::Length( line ) )
                cachedKeys.insert( line );
        }
    }
}

bool IniParser::IsCachedKey( const char* key_name )
{
    return cachedKeys.count( string( key_name ) ) != 0;
}

StrSet& IniParser::GetCachedKeys()
{
    return cachedKeys;
}

const char* IniParser::GetConfigFileName()
{
    // Default config names
    #if defined ( FONLINE_SERVER )
    static char config_name[ MAX_FOPATH ] = { "FOnlineServer.cfg\0--default-server-config--" };
    #elif defined ( FONLINE_MAPPER )
    static char config_name[ MAX_FOPATH ] = { "Mapper.cfg\0--default-mapper-config--" };
    #else // FONLINE_CLIENT and others
    static char config_name[ MAX_FOPATH ] = { "FOnline.cfg\0--default-client-config--" };
    #endif

    // Extract config name from current exe
    #ifdef FO_WINDOWS
    static bool processed = false;
    if( !processed )
    {
        // Call once
        processed = true;

        // Get full path
        char module_name[ MAX_FOPATH ];
        # ifdef FO_WINDOWS
        if( !GetModuleFileName( NULL, module_name, sizeof( module_name ) ) )
            return config_name;
        # else
        // Todo: Linux CommandLineArgValues[0] ?
        # endif

        // Change extension
        char* ext = (char*) FileManager::GetExtension( module_name );
        if( !ext )
            return config_name;
        Str::Copy( ext, 4, "cfg" );

        // Check availability
        if( !FileExist( module_name ) )
            return config_name;

        // Get file name
        const char* name = NULL;
        for( size_t i = 0, j = Str::Length( module_name ); i < j; i++ )
            if( module_name[ i ] == DIR_SLASH_C )
                name = &module_name[ i + 1 ];
        if( !name )
            return config_name;

        // Set as main
        Str::Copy( config_name, name );
    }
    #endif

    return config_name;
}

IniParser& IniParser::GetClientConfig()
{
    static IniParser cfg_client;

    #ifndef FONLINE_MAPPER
    const char* cfg_name = GetConfigFileName();
    cfg_client.LoadFile( cfg_name, PT_ROOT );
    cfg_client.AppendToBegin( cfg_name, PT_CACHE );
    #else
    IniParser& cfg_mapper = GetMapperConfig();
    char       buf[ MAX_FOPATH ];
    cfg_mapper.GetStr( "ClientName", "FOnline", buf );
    Str::Append( buf, ".cfg" );
    cfg_client.LoadFile( ( GameOpt.ClientPath->c_std_str() + buf ).c_str(), PT_ROOT );
    cfg_client.AppendToBegin( buf, PT_CACHE );
    #endif

    return cfg_client;
}

IniParser& IniParser::GetServerConfig()
{
    static IniParser cfg_server;

    #ifndef FONLINE_MAPPER
    cfg_server.LoadFile( GetConfigFileName(), PT_ROOT );
    #else
    IniParser& cfg_mapper = GetMapperConfig();
    char       buf[ MAX_FOTEXT ];
    cfg_mapper.GetStr( "ServerName", "FOnline", buf );
    cfg_server.LoadFile( ( GameOpt.ServerPath->c_std_str() + buf + ".cfg" ).c_str(), PT_ROOT );
    #endif

    return cfg_server;
}

IniParser& IniParser::GetMapperConfig()
{
    static IniParser cfg_mapper;

    cfg_mapper.LoadFile( GetConfigFileName(), PT_ROOT );

    return cfg_mapper;
}
