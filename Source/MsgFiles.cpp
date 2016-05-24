#include "Common.h"
#include "MsgFiles.h"
#include "Crypt.h"
#include "FileManager.h"

const char* TextMsgFileName[ TEXTMSG_COUNT ] =
{
    "FOTEXT.MSG",
    "FODLG.MSG",
    "FOOBJ.MSG",
    "FOGAME.MSG",
    "FOGM.MSG",
    "FOCOMBAT.MSG",
    "FOQUEST.MSG",
    "FOHOLO.MSG",
    "FOINTERNAL.MSG",
    "FOLOCATIONS.MSG",
};

FOMsg::FOMsg()
{
    Clear();
}

FOMsg::FOMsg( const FOMsg& other )
{
    Clear();
    for( auto it = other.strData.begin(), end = other.strData.end(); it != end; ++it )
        AddStr( it->first, it->second );
}

FOMsg::~FOMsg()
{
    Clear();
}

FOMsg& FOMsg::operator=( const FOMsg& other )
{
    Clear();
    for( auto it = other.strData.begin(), end = other.strData.end(); it != end; ++it )
        AddStr( it->first, it->second );
    return *this;
}

FOMsg& FOMsg::operator+=( const FOMsg& other )
{
    for( auto it = other.strData.begin(), end = other.strData.end(); it != end; ++it )
    {
        EraseStr( it->first );
        AddStr( it->first, it->second );
    }
    return *this;
}

void FOMsg::AddStr( uint num, const char* str )
{
    strData.insert( PAIR( num, Str::Duplicate( str ) ) );
}

void FOMsg::AddStr( uint num, const string& str )
{
    strData.insert( PAIR( num, Str::Duplicate( str.c_str() ) ) );
}

void FOMsg::AddBinary( uint num, const uchar* binary, uint len )
{
    CharVec str;
    str.resize( len * 2 + 1 );
    size_t  str_cur = 0;
    for( uint i = 0; i < len; i++ )
    {
        Str::HexToStr( binary[ i ], &str[ str_cur ] );
        str_cur += 2;
    }

    AddStr( num, (char*) &str[ 0 ] );
}

const char* FOMsg::GetStr( uint num )
{
    uint str_count = (uint) strData.count( num );
    auto it = strData.find( num );

    switch( str_count )
    {
    case 0:
        return MSG_ERROR_MESSAGE;
    case 1:
        break;
    default:
        for( int i = 0, j = Random( 0, str_count ) - 1; i < j; i++ )
            ++it;
        break;
    }

    return it->second;
}

const char* FOMsg::GetStr( uint num, uint skip )
{
    uint str_count = (uint) strData.count( num );
    auto it = strData.find( num );

    if( skip >= str_count )
        return MSG_ERROR_MESSAGE;
    for( uint i = 0; i < skip; i++ )
        ++it;

    return it->second;
}

uint FOMsg::GetStrNumUpper( uint num )
{
    auto it = strData.upper_bound( num );
    if( it == strData.end() )
        return 0;
    return it->first;
}

uint FOMsg::GetStrNumLower( uint num )
{
    auto it = strData.lower_bound( num );
    if( it == strData.end() )
        return 0;
    return it->first;
}

int FOMsg::GetInt( uint num )
{
    uint str_count = (uint) strData.count( num );
    auto it = strData.find( num );

    switch( str_count )
    {
    case 0:
        return -1;
    case 1:
        break;
    default:
        for( int i = 0, j = Random( 0, str_count ) - 1; i < j; i++ )
            ++it;
        break;
    }

    return Str::AtoI( it->second );
}

uint FOMsg::GetBinary( uint num, UCharVec& data  )
{
    data.clear();

    if( !Count( num ) )
        return 0;

    const char* str = GetStr( num );
    uint        len = Str::Length( str ) / 2;
    data.resize( len );
    for( uint i = 0; i < len; i++ )
        data[ i ] = Str::StrToHex( &str[ i * 2 ] );
    return len;
}

uint FOMsg::Count( uint num )
{
    return (uint) strData.count( num );
}

void FOMsg::EraseStr( uint num )
{
    while( true )
    {
        auto it = strData.find( num );
        if( it != strData.end() )
        {
            SAFEDELA( it->second );
            strData.erase( it );
        }
        else
        {
            break;
        }
    }
}

uint FOMsg::GetSize()
{
    return (uint) strData.size();
}

bool FOMsg::IsIntersects( const FOMsg& other )
{
    for( auto it = strData.begin(), end = strData.end(); it != end; ++it )
        if( other.strData.count( it->first ) )
            return true;
    return false;
}

void FOMsg::GetBinaryData( UCharVec& data )
{
    // Fill raw data
    uint count = (uint) strData.size();
    data.resize( sizeof( count ) );
    memcpy( &data[ 0 ], &count, sizeof( count ) );
    for( auto it = strData.begin(), end = strData.end(); it != end; ++it )
    {
        uint        num = it->first;
        const char* str = it->second;
        uint        str_len = ( str ? Str::Length( str ) : 0 );

        data.resize( data.size() + sizeof( num ) + sizeof( str_len ) + str_len );
        memcpy( &data[ data.size() - ( sizeof( num ) + sizeof( str_len ) + str_len ) ], &num, sizeof( num ) );
        memcpy( &data[ data.size() - ( sizeof( str_len ) + str_len ) ], &str_len, sizeof( str_len ) );
        if( str_len )
            memcpy( &data[ data.size() - str_len ], (void*) str, str_len );
    }

    // Compress
    Crypt.Compress( data );
}

bool FOMsg::LoadFromBinaryData( const UCharVec& data )
{
    Clear();

    // Uncompress
    UCharVec data_copy = data;
    if( !Crypt.Uncompress( data_copy, 10 ) )
        return false;

    // Read count of strings
    const uchar* buf = &data_copy[ 0 ];
    uint         count;
    memcpy( &count, buf, sizeof( count ) );
    buf += sizeof( count );

    // Read strings
    uint   num;
    uint   str_len;
    string str;
    for( uint i = 0; i < count; i++ )
    {
        memcpy( &num, buf, sizeof( num ) );
        buf += sizeof( num );

        memcpy( &str_len, buf, sizeof( str_len ) );
        buf += sizeof( str_len );

        str.resize( str_len );
        if( str_len )
            memcpy( &str[ 0 ], buf, str_len );
        buf += str_len;

        AddStr( num, str );
    }

    return true;
}

bool FOMsg::LoadFromFile( const char* fname, int path_type )
{
    Clear();

    FileManager file;
    if( !file.LoadFile( fname, path_type ) )
        return false;

    return LoadFromString( (char*) file.GetBuf(), file.GetFsize() );
}

bool FOMsg::LoadFromString( const char* str, uint str_len )
{
    bool  fail = false;

    char* str_copy = new char[ str_len + 1 ];
    memcpy( str_copy, str, str_len );
    str_copy[ str_len ] = 0;

    for( char* pbuf = str_copy; *pbuf;)
    {
        // Comments
        if( *pbuf == '#' || *pbuf == ';' )
        {
            Str::SkipLine( pbuf );
            continue;
        }

        // Find '{'
        if( *pbuf != '{' )
        {
            pbuf++;
            continue;
        }

        // Skip '{'
        pbuf++;
        if( !*pbuf )
            break;

        // Goto '}'
        const char* num1_begin = pbuf;
        Str::GoTo( pbuf, '}', false );
        if( !*pbuf )
            break;
        *pbuf = 0;
        pbuf++;

        // Parse number
        uint num1 = (uint) ( Str::IsNumber( num1_begin ) ? Str::AtoI( num1_begin ) : Str::GetHash( num1_begin ) );

        // Skip '{'
        Str::GoTo( pbuf, '{', true );
        if( !*pbuf )
            break;

        // Goto '}'
        const char* num2_begin = pbuf;
        Str::GoTo( pbuf, '}', false );
        if( !*pbuf )
            break;
        *pbuf = 0;
        pbuf++;

        // Parse number
        uint num2 = ( num2_begin[ 0 ] ? ( Str::IsNumber( num2_begin ) ? Str::AtoI( num2_begin ) : Str::GetHash( num2_begin ) ) : 0 );

        // Goto '{'
        Str::GoTo( pbuf, '{', true );
        if( !*pbuf )
            break;

        // Find '}'
        char* _pbuf = pbuf;
        Str::GoTo( pbuf, '}', false );
        if( !*pbuf )
            break;
        *pbuf = 0;
        pbuf++;

        uint num = num1 + num2;
        if( num )
            AddStr( num, _pbuf );
    }

    SAFEDELA( str_copy );
    return !fail;
}

void FOMsg::LoadFromMap( const StrMap& kv )
{
    for( auto& kv_ : kv )
    {
        uint num = Str::AtoUI( kv_.first.c_str() );
        if( num )
            AddStr( num, kv_.second );
    }
}

bool FOMsg::SaveToFile( const char* fname, int path_type )
{
    string str;
    for( auto it = strData.begin(), end = strData.end(); it != end; it++ )
    {
        str += "{";
        str += Str::UItoA( it->first );
        str += "}{}{";
        str += it->second;
        str += "}\n";
    }

    char*       buf = (char*) str.c_str();
    uint        buf_len = (uint) str.length();

    FileManager fm;
    fm.SetData( buf, buf_len );
    if( !fm.SaveOutBufToFile( fname, path_type ) )
        return false;

    return true;
}

void FOMsg::Clear()
{
    for( auto it = strData.begin(), end = strData.end(); it != end; ++it )
        SAFEDELA( it->second );
    strData.clear();
}

int FOMsg::GetMsgType( const char* type_name )
{
    if( Str::CompareCase( type_name, "text" ) )
        return TEXTMSG_TEXT;
    else if( Str::CompareCase( type_name, "dlg" ) )
        return TEXTMSG_DLG;
    else if( Str::CompareCase( type_name, "item" ) )
        return TEXTMSG_ITEM;
    else if( Str::CompareCase( type_name, "obj" ) )
        return TEXTMSG_ITEM;
    else if( Str::CompareCase( type_name, "game" ) )
        return TEXTMSG_GAME;
    else if( Str::CompareCase( type_name, "gm" ) )
        return TEXTMSG_GM;
    else if( Str::CompareCase( type_name, "combat" ) )
        return TEXTMSG_COMBAT;
    else if( Str::CompareCase( type_name, "quest" ) )
        return TEXTMSG_QUEST;
    else if( Str::CompareCase( type_name, "holo" ) )
        return TEXTMSG_HOLO;
    else if( Str::CompareCase( type_name, "internal" ) )
        return TEXTMSG_INTERNAL;
    return -1;
}

LanguagePack::LanguagePack()
{
    memzero( NameStr, sizeof( NameStr ) );
    IsAllMsgLoaded = false;
}

bool LanguagePack::LoadFromFiles( const char* lang_name )
{
    memcpy( NameStr, lang_name, 4 );
    bool            fail = false;

    FilesCollection msg_files( "msg" );
    while( msg_files.IsNextFile() )
    {
        const char*  name, * path;
        FileManager& msg_file = msg_files.GetNextFile( &name, &path );

        // Check pattern '...Texts/lang/file'
        StrVec dirs;
        Str::ParseLine( path, '/', dirs, Str::ParseLineDummy );
        if( dirs.size() >= 3 && dirs[ dirs.size() - 3 ] == "Texts" && dirs[ dirs.size() - 2 ] == lang_name )
        {
            for( int i = 0; i < TEXTMSG_COUNT; i++ )
            {
                char name_[ MAX_FOTEXT ];
                Str::Copy( name_, TextMsgFileName[ i ] );
                FileManager::EraseExtension( name_ );
                if( Str::CompareCase( name_, name ) )
                {
                    if( !Msg[ i ].LoadFromString( (char*) msg_file.GetBuf(), msg_file.GetFsize() ) )
                    {
                        WriteLogF( _FUNC_, " - Invalid MSG file '%s'.\n", path );
                        fail = true;
                    }
                    break;
                }
            }
        }
    }

    if( Msg[ TEXTMSG_GAME ].GetSize() == 0 )
        WriteLogF( _FUNC_, " - Unable to load '%s' from file.\n", TextMsgFileName[ TEXTMSG_GAME ] );

    IsAllMsgLoaded = ( Msg[ TEXTMSG_GAME ].GetSize() > 0 && !fail );
    return IsAllMsgLoaded;
}

bool LanguagePack::LoadFromCache( const char* lang_name )
{
    memcpy( NameStr, lang_name, 4 );

    int errors = 0;
    for( int i = 0; i < TEXTMSG_COUNT; i++ )
    {
        char   cache_name[ MAX_FOTEXT ];
        uint   buf_len;
        uchar* buf = Crypt.GetCache( GetMsgCacheName( i, cache_name ), buf_len );
        if( buf )
        {
            UCharVec data;
            data.resize( buf_len );
            memcpy( &data[ 0 ], buf, buf_len );
            SAFEDELA( buf );

            if( !Msg[ i ].LoadFromBinaryData( data ) )
                errors++;
        }
        else
        {
            errors++;
        }
    }

    if( errors )
        WriteLogF( _FUNC_, " - Cached language '%s' not found.\n", NameStr );

    IsAllMsgLoaded = errors == 0;
    return IsAllMsgLoaded;
}

char* LanguagePack::GetMsgCacheName( int msg_num, char* result )
{
    return Str::Format( result, CACHE_MSG_PREFIX "\\%s\\%s", NameStr, TextMsgFileName[ msg_num ] );
}
