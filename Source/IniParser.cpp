#include "Common.h"
#include "IniParser.h"
#include "Crypt.h"
#include "FileManager.h"

IniParser::IniParser()
{
    //
}

IniParser::IniParser( const char* str )
{
    AppendStr( str );
}

IniParser::IniParser( const char* fname, int path_type )
{
    AppendFile( fname, path_type );
}

void IniParser::AppendStr( const char* buf )
{
    ParseStr( buf );
}

bool IniParser::AppendFile( const char* fname, int path_type )
{
    FileManager fm;
    if( !fm.LoadFile( fname, path_type ) )
        return false;

    ParseStr( fm.GetCStr() );
    return true;
}

void IniParser::ParseStr( const char* str )
{
    StrMap* cur_app;
    auto    it_app = appKeyValues.find( "" );
    if( it_app == appKeyValues.end() )
    {
        auto it = appKeyValues.insert( PAIR( string( "" ), StrMap() ) );
        appKeyValuesOrder.push_back( it );
        cur_app = &it->second;
    }
    else
    {
        cur_app = &it_app->second;
    }

    string app_content;
    if( collectContent )
        app_content.reserve( 0xFFFF );

    istrstream istr( str );
    char       line[ MAX_FOTEXT ];
    char       buf[ MAX_FOTEXT ];
    while( istr.getline( line, MAX_FOTEXT ) )
    {
        // New section
        if( line[ 0 ] == '[' )
        {
            // Parse name
            const char* begin = &line[ 1 ];
            const char* end = Str::Substring( begin, "]" );
            if( !end )
                continue;

            Str::CopyCount( buf, begin, (uint) ( end - begin ) );
            Str::Trim( buf );
            if( !buf[ 0 ] )
                continue;

            // Store current section content
            if( collectContent )
            {
                ( *cur_app )[ "" ] = app_content;
                app_content.clear();
            }

            // Add new section
            auto it = appKeyValues.insert( PAIR( string( buf ), StrMap() ) );
            appKeyValuesOrder.push_back( it );
            cur_app = &it->second;
        }
        // Section content
        else
        {
            // Store raw content
            if( collectContent )
                app_content.append( line ).append( "\n" );

            // Cut comments
            Str::Trim( line );
            char* comment = Str::Substring( line, "#" );
            if( comment )
                *comment = 0;

            // Parse key value
            char* begin = &line[ 0 ];
            char* value = nullptr;

            // Text format {}{}{}
            if( begin[ 0 ] == '{' )
            {
                char* pbuf = begin;

                // Skip '{'
                pbuf++;
                if( !*pbuf )
                    continue;

                // Goto '}'
                const char* num1_begin = pbuf;
                Str::GoTo( pbuf, '}', false );
                if( !*pbuf )
                    continue;
                *pbuf = 0;
                pbuf++;

                // Parse number
                uint num1 = (uint) ( Str::IsNumber( num1_begin ) ? Str::AtoI( num1_begin ) : Str::GetHash( num1_begin ) );

                // Skip '{'
                Str::GoTo( pbuf, '{', true );
                if( !*pbuf )
                    continue;

                // Goto '}'
                const char* num2_begin = pbuf;
                Str::GoTo( pbuf, '}', false );
                if( !*pbuf )
                    continue;

                *pbuf = 0;
                pbuf++;

                // Parse number
                uint num2 = ( num2_begin[ 0 ] ? ( Str::IsNumber( num2_begin ) ? Str::AtoI( num2_begin ) : Str::GetHash( num2_begin ) ) : 0 );

                // Goto '{'
                Str::GoTo( pbuf, '{', true );
                if( !*pbuf )
                    continue;
                // Find '}'
                char* _pbuf = pbuf;
                Str::GoTo( pbuf, '}', false );
                if( !*pbuf )
                    continue;

                *pbuf = 0;
                pbuf++;
                uint num = num1 + num2;
                if( !num )
                    continue;

                Str::Copy( buf, Str::UItoA( num ) );
                value = _pbuf;
            }
            else
            {
                // Key value format
                char* separator = Str::Substring( begin, "=" );
                if( !separator )
                    continue;

                // Parse key value
                Str::CopyCount( buf, begin, (uint) ( separator - begin ) );
                value = separator + 1;
                Str::Trim( buf );
                Str::Trim( value );
            }

            // Store entire
            if( buf[ 0 ] )
                ( *cur_app )[ buf ] = value;
        }
    }

    // Store current section content
    if( collectContent )
        ( *cur_app )[ "" ] = app_content;
}

bool IniParser::SaveFile( const char* fname, int path_type )
{
    string str;
    str.reserve( 10000000 );
    for( auto& app_it : appKeyValuesOrder )
    {
        auto& app = *app_it;
        str.append( string( "[" ).append( app.first ).append( "]" ).append( "\n" ) );
        for( const auto& kv : app.second )
            if( !kv.first.empty() )
                str.append( string( kv.first ).append( " = " ).append( kv.second ).append( "\n" ) );
        str.append( "\n" );
    }

    char  path[ MAX_FOTEXT ];
    FileManager::GetWritePath( fname, path_type, path );
    void* f = FileOpen( path, true );
    if( !f )
        return false;
    bool result = FileWrite( f, (void*) str.c_str(), (uint) str.length() );
    FileClose( f );
    return result;
}

void IniParser::Clear()
{
    appKeyValues.clear();
    appKeyValuesOrder.clear();
}

bool IniParser::IsLoaded()
{
    return !appKeyValues.empty();
}

string* IniParser::GetRawValue( const char* app_name, const char* key_name )
{
    auto it_app = appKeyValues.find( app_name );
    if( it_app == appKeyValues.end() )
        return nullptr;

    auto it_key = it_app->second.find( key_name );
    if( it_key == it_app->second.end() )
        return nullptr;

    return &it_key->second;
}

const char* IniParser::GetStr( const char* app_name, const char* key_name, const char* def_val /* = nullptr */ )
{
    string* str = GetRawValue( app_name, key_name );
    return str ? str->c_str() : def_val;
}

int IniParser::GetInt( const char* app_name, const char* key_name, int def_val /* = 0 */  )
{
    string* str = GetRawValue( app_name, key_name );
    if( str && str->length() == 4 && Str::CompareCase( str->c_str(), "true" ) )
        return 1;
    if( str && str->length() == 5 && Str::CompareCase( str->c_str(), "false" ) )
        return 0;
    return str ? Str::AtoI( str->c_str() ) : def_val;
}

void IniParser::SetStr( const char* app_name, const char* key_name, const char* val )
{
    auto it_app = appKeyValues.find( app_name );
    if( it_app == appKeyValues.end() )
    {
        StrMap key_values;
        key_values[ key_name ] = val;
        auto   it = appKeyValues.insert( PAIR( string( app_name ), key_values ) );
        appKeyValuesOrder.push_back( it );
    }
    else
    {
        it_app->second[ key_name ] = val;
    }
}

void IniParser::SetInt( const char* app_name, const char* key_name, int val )
{
    SetStr( app_name, key_name, Str::ItoA( val ) );
}

StrMap& IniParser::GetApp( const char* app_name )
{
    auto it = appKeyValues.find( app_name );
    return it->second;
}

void IniParser::GetApps( const char* app_name, PStrMapVec& key_values )
{
    size_t count = appKeyValues.count( app_name );
    auto   it = appKeyValues.find( app_name );
    key_values.reserve( key_values.size() + count );
    for( size_t i = 0; i < count; i++, it++ )
        key_values.push_back( &it->second );
}

StrMap& IniParser::SetApp( const char* app_name )
{
    auto it = appKeyValues.insert( PAIR( string( app_name ), StrMap() ) );
    appKeyValuesOrder.push_back( it );
    return it->second;
}

bool IniParser::IsApp( const char* app_name )
{
    auto it_app = appKeyValues.find( app_name );
    return it_app != appKeyValues.end();
}

bool IniParser::IsKey( const char* app_name, const char* key_name )
{
    auto it_app = appKeyValues.find( app_name );
    if( it_app == appKeyValues.end() )
        return false;

    return it_app->second.find( key_name ) != it_app->second.end();
}

void IniParser::GetAppNames( StrSet& apps )
{
    for( const auto& kv : appKeyValues )
        apps.insert( kv.first );
}

void IniParser::GotoNextApp( const char* app_name )
{
    auto it_app = appKeyValues.find( app_name );
    if( it_app == appKeyValues.end() )
        return;

    auto it = std::find( appKeyValuesOrder.begin(), appKeyValuesOrder.end(), it_app );
    RUNTIME_ASSERT( it != appKeyValuesOrder.end() );
    appKeyValuesOrder.erase( it );
    appKeyValues.erase( it_app );
}

const StrMap* IniParser::GetAppKeyValues( const char* app_name )
{
    auto it_app = appKeyValues.find( app_name );
    return it_app != appKeyValues.end() ? &it_app->second : nullptr;
}

const char* IniParser::GetAppContent( const char* app_name )
{
    RUNTIME_ASSERT( collectContent );

    auto it_app = appKeyValues.find( app_name );
    if( it_app == appKeyValues.end() )
        return nullptr;

    auto it_key = it_app->second.find( "" );
    return it_key != it_app->second.end() ? it_key->second.c_str() : nullptr;
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

        // Get module name
        char module_name[ MAX_FOPATH ];
        # ifdef FO_WINDOWS
        char path[ MAX_FOPATH ];
        if( !GetModuleFileName( nullptr, path, sizeof( path ) ) )
            return config_name;
        FileManager::ExtractFileName( path, module_name );
        # else
        // Todo: Linux CommandLineArgValues[0] ?
        return config_name;
        # endif

        // Change extension
        char* ext = (char*) FileManager::GetExtension( module_name );
        if( !ext )
            return config_name;
        Str::Copy( ext, 4, "cfg" );

        // Check availability
        if( !FileExist( module_name ) )
            return config_name;

        // Set as main
        Str::Copy( config_name, module_name );
    }
    #endif

    return config_name;
}

IniParser& IniParser::GetClientConfig()
{
    static IniParser cfg_client;
    cfg_client.Clear();

    // Injected options
    static char InternalClientConfig[ 5032 ] =
    {
        "###InternalClientConfig###5032\0"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
    };

    #ifndef FONLINE_MAPPER
    const char* cfg_name = GetConfigFileName();
    cfg_client.AppendStr( InternalClientConfig );
    cfg_client.AppendFile( cfg_name, PT_ROOT );
    cfg_client.AppendFile( cfg_name, PT_CACHE );
    #else
    IniParser&  cfg_mapper = GetMapperConfig();
    const char* client_name = cfg_mapper.GetStr( "", "ClientName", "FOnline" );
    char        cfg_name[ MAX_FOTEXT ];
    Str::Copy( cfg_name, client_name );
    Str::Append( cfg_name, ".cfg" );
    cfg_client.AppendFile( cfg_name, PT_ROOT );
    cfg_client.AppendFile( cfg_name, PT_CACHE );
    #endif

    return cfg_client;
}

IniParser& IniParser::GetServerConfig()
{
    static IniParser cfg_server;
    cfg_server.Clear();

    #ifndef FONLINE_MAPPER
    cfg_server.AppendFile( GetConfigFileName(), PT_ROOT );
    #else
    IniParser&  cfg_mapper = GetMapperConfig();
    const char* server_name = cfg_mapper.GetStr( "", "ServerName", "FOnline" );
    char        cfg_name[ MAX_FOTEXT ];
    Str::Copy( cfg_name, server_name );
    Str::Append( cfg_name, ".cfg" );
    cfg_server.AppendFile( cfg_name, PT_ROOT );
    #endif

    return cfg_server;
}

IniParser& IniParser::GetMapperConfig()
{
    static IniParser cfg_mapper;
    cfg_mapper.Clear();

    cfg_mapper.AppendFile( GetConfigFileName(), PT_ROOT );

    return cfg_mapper;
}
