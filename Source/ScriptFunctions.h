// Common functions for server, client, mapper
// Works in scripts compiler
#ifndef FONLINE_SCRIPT_COMPILER
# include "curl/curl.h"
# include "SHA/sha1.h"
# include "SHA/sha2.h"
#endif

template< class T >
static Entity* EntityDownCast( T* a )
{
    if( !a )
        return nullptr;
    Entity* b = (Entity*) a;
    b->AddRef();
    return b;
}

template< class T >
static T* EntityUpCast( Entity* a )
{
    if( !a )
        return nullptr;
    #define CHECK_CAST( cast_class, entity_type )                                         \
        if( std::is_same< T, cast_class >::value && (EntityType) a->Type == entity_type ) \
        {                                                                                 \
            T* b = (T*) a;                                                                \
            b->AddRef();                                                                  \
            return b;                                                                     \
        }
    #if defined ( FONLINE_SERVER )
    CHECK_CAST( Location, EntityType::Location );
    CHECK_CAST( Map, EntityType::Map );
    CHECK_CAST( Critter, EntityType::Npc );
    CHECK_CAST( Critter, EntityType::Client );
    CHECK_CAST( Item, EntityType::Item );
    CHECK_CAST( Item, EntityType::ItemHex );
    #elif defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )
    CHECK_CAST( Location, EntityType::Location );
    CHECK_CAST( Map, EntityType::Map );
    CHECK_CAST( CritterCl, EntityType::CritterCl );
    CHECK_CAST( Item, EntityType::Item );
    CHECK_CAST( Item, EntityType::ItemHex );
    #endif
    #undef CHECK_CAST
    return nullptr;
}

void Global_Assert( bool condition )
{
    if( !condition )
        Script::RaiseException( "Assertion failed" );
}

void Global_ThrowException( ScriptString* message )
{
    Script::RaiseException( "%s", message->c_str() );
}

int Global_Random( int min, int max )
{
    static Randomizer script_randomizer;
    return script_randomizer.Random( min, max );
}

void Global_Log( ScriptString& text )
{
    #ifndef FONLINE_SCRIPT_COMPILER
    Script::Log( text.c_str() );
    #else
    printf( "%s\n", text.c_str() );
    #endif
}

bool Global_StrToInt( ScriptString* text, int& result )
{
    if( !text || !text->length() || !Str::IsNumber( text->c_str() ) )
        return false;
    result = Str::AtoI( text->c_str() );
    return true;
}

bool Global_StrToFloat( ScriptString* text, float& result )
{
    if( !text || !text->length() )
        return false;
    result = (float) strtod( text->c_str(), nullptr );
    return true;
}

uint Global_GetDistantion( ushort hx1, ushort hy1, ushort hx2, ushort hy2 )
{
    return DistGame( hx1, hy1, hx2, hy2 );
}

uchar Global_GetDirection( ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy )
{
    return GetFarDir( from_hx, from_hy, to_hx, to_hy );
}

uchar Global_GetOffsetDir( ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, float offset )
{
    return GetFarDir( from_hx, from_hy, to_hx, to_hy, offset );
}

uint Global_GetTick()
{
    return Timer::FastTick();
}

uint Global_GetAngelScriptProperty( int property )
{
    return (uint) asGetActiveContext()->GetEngine()->GetEngineProperty( (asEEngineProp) property );
}

bool Global_SetAngelScriptProperty( int property, uint value )
{
    return asGetActiveContext()->GetEngine()->SetEngineProperty( (asEEngineProp) property, value ) >= 0;
}

hash Global_GetStrHash( ScriptString* str )
{
    if( !str )
        return 0;
    return Str::GetHash( str->c_str() );
}

ScriptString* Global_GetHashStr( hash h )
{
    if( !h )
        return ScriptString::Create();
    const char* str = Str::GetName( h );
    return ScriptString::Create( str ? str : "" );
}

uint Global_DecodeUTF8( ScriptString& text, uint& length )
{
    return Str::DecodeUTF8( text.c_str(), &length );
}

ScriptString* Global_EncodeUTF8( uint ucs )
{
    char buf[ 5 ];
    uint len = Str::EncodeUTF8( ucs, buf );
    buf[ len ] = 0;
    return ScriptString::Create( buf );
}

ScriptString* Global_GetFilePath( int path_type )
{
    char path[ MAX_FOPATH ];
    FileManager::GetDataPath( "", path_type, path );
    FileManager::FormatPath( path );

    return ScriptString::Create( path );
}

uint Global_GetFolderFileNames( ScriptString& path, ScriptString* ext, bool include_subdirs, ScriptArray* result )
{
    StrVec files;
    FileManager::GetFolderFileNames( path.c_str(), include_subdirs, ext ? ext->c_str() : nullptr, files );

    if( result )
    {
        for( uint i = 0, j = (uint) files.size(); i < j; i++ )
            result->InsertLast( ScriptString::Create( files[ i ] ) );
    }

    return (uint) files.size();
}

bool Global_DeleteFile( ScriptString& filename )
{
    return FileManager::DeleteFile( filename.c_str() );
}

void Global_CreateDirectoryTree( ScriptString& path )
{
    char tmp[ MAX_FOPATH ];
    Str::Copy( tmp, path.c_str() );
    Str::Append( tmp, "/" );
    FileManager::FormatPath( tmp );
    CreateDirectoryTree( tmp );
}

void Global_Yield( uint time )
{
    #ifndef FONLINE_SCRIPT_COMPILER
    Script::SuspendCurrentContext( time );
    #endif
}

#ifndef FONLINE_SCRIPT_COMPILER
static size_t WriteMemoryCallback( char* ptr, size_t size, size_t nmemb, void* userdata )
{
    string& result = *(string*) userdata;
    size_t  len = size * nmemb;
    if( len )
    {
        result.resize( result.size() + len );
        memcpy( &result[ result.size() - len ], ptr, len );
    }
    return len;
}
#endif

void Global_YieldWebRequest( ScriptString& url, ScriptDict* post, bool& success, ScriptString& result )
{
    #ifndef FONLINE_SCRIPT_COMPILER
    success = false;
    result = "";

    asIScriptContext* ctx = Script::SuspendCurrentContext( uint( -1 ) );
    if( !ctx )
        return;

    struct RequestData
    {
        asIScriptContext* Context;
        Thread*           WorkThread;
        ScriptString*     Url;
        ScriptDict*       Post;
        bool*             Success;
        ScriptString*     Result;
    };

    RequestData* request_data = new RequestData();
    request_data->Context = ctx;
    request_data->WorkThread = new Thread();
    request_data->Url = &url;
    request_data->Post = post;
    request_data->Success = &success;
    request_data->Result = &result;

    auto request_func = [] (void* data)
    {
        static bool curl_inited = false;
        if( !curl_inited )
        {
            curl_inited = true;
            curl_global_init( CURL_GLOBAL_ALL );
        }

        RequestData* request_data = (RequestData*) data;

        bool         success = false;
        string       result;

        CURL*        curl = curl_easy_init();
        if( curl )
        {
            curl_easy_setopt( curl, CURLOPT_URL, request_data->Url->c_str() );
            curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback );
            curl_easy_setopt( curl, CURLOPT_WRITEDATA, &result );

            string post;
            if( request_data->Post )
            {
                for( uint i = 0, j = request_data->Post->GetSize(); i < j; i++ )
                {
                    ScriptString* key = (ScriptString*) request_data->Post->GetKey( i );
                    ScriptString* value = (ScriptString*) request_data->Post->GetValue( i );
                    char*         escaped_key = curl_easy_escape( curl, key->c_str(), key->length() );
                    char*         escaped_value = curl_easy_escape( curl, value->c_str(), value->length() );
                    if( i > 0 )
                        post += "&";
                    post += escaped_key;
                    post += "=";
                    post += escaped_value;
                    curl_free( escaped_key );
                    curl_free( escaped_value );
                }
                curl_easy_setopt( curl, CURLOPT_POSTFIELDS, post.c_str() );
            }

            CURLcode curl_res = curl_easy_perform( curl );
            if( curl_res == CURLE_OK )
            {
                success = true;
            }
            else
            {
                result = "curl_easy_perform() failed: ";
                result += curl_easy_strerror( curl_res );
            }
            curl_easy_cleanup( curl );
        }
        else
        {
            result = "curl_easy_init fail";
        }

        *request_data->Success = success;
        *request_data->Result = result;
        Script::ResumeContext( request_data->Context );
        request_data->WorkThread->Release();
        delete request_data->WorkThread;
        delete request_data;
    };
    request_data->WorkThread->Start( request_func, "WebRequest", request_data );
    #endif
}

ScriptString* Global_SHA1( ScriptString& text )
{
    #ifndef FONLINE_SCRIPT_COMPILER
    SHA1_CTX ctx;
    SHA1_Init( &ctx );
    SHA1_Update( &ctx, (uchar*) text.c_str(), text.length() );
    uchar digest[ SHA1_DIGEST_SIZE ];
    SHA1_Final( &ctx, digest );

    static const char* nums = "0123456789abcdef";
    char               hex_digest[ SHA1_DIGEST_SIZE * 2 ];
    for( uint i = 0; i < sizeof( hex_digest ); i++ )
        hex_digest[ i ] = nums[ i % 2 ? digest[ i / 2 ] & 0xF : digest[ i / 2 ] >> 4 ];
    return ScriptString::Create( hex_digest, sizeof( hex_digest ) );
    #else
    return nullptr;
    #endif
}

ScriptString* Global_SHA2( ScriptString& text )
{
    #ifndef FONLINE_SCRIPT_COMPILER
    const uint digest_size = 32;
    uchar      digest[ digest_size ];
    sha256( (uchar*) text.c_str(), text.length(), digest );

    static const char* nums = "0123456789abcdef";
    char               hex_digest[ digest_size * 2 ];
    for( uint i = 0; i < sizeof( hex_digest ); i++ )
        hex_digest[ i ] = nums[ i % 2 ? digest[ i / 2 ] & 0xF : digest[ i / 2 ] >> 4 ];
    return ScriptString::Create( hex_digest, sizeof( hex_digest ) );
    #else
    return nullptr;
    #endif
}

void Global_OpenLink( ScriptString& link )
{
    #ifdef FO_WINDOWS
    ShellExecute( nullptr, "open", link.c_str(), nullptr, nullptr, SW_SHOWNORMAL );
    #else
    system( ( string( "xdg-open " ) + link.c_std_str() ).c_str() );
    #endif
}

Item* Global_GetProtoItem( hash pid )
{
    #ifndef FONLINE_SCRIPT_COMPILER
    ProtoItem* proto = ProtoMngr.GetProtoItem( pid );
    return proto != nullptr ? new Item( 0, proto ) : nullptr;
    #else
    return nullptr;
    #endif
}
