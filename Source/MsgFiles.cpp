#include "StdAfx.h"
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
    "FOCRAFT.MSG",
    "FOINTERNAL.MSG",
};

FOMsg::FOMsg()
{
    Clear();
}

FOMsg& FOMsg::operator+=( const FOMsg& r )
{
    for( auto it = r.strData.begin(), end = r.strData.end(); it != end; ++it )
    {
        EraseStr( ( *it ).first );
        AddStr( ( *it ).first, ( *it ).second );
    }
    return *this;
}

void FOMsg::AddStr( uint num, const char* str )
{
    if( !str || !Str::Length( str ) )
        strData.insert( PAIR( num, " " ) );
    else
        strData.insert( PAIR( num, str ) );
}

void FOMsg::AddStr( uint num, const string& str )
{
    if( !str.length() )
        strData.insert( PAIR( num, " " ) );
    else
        strData.insert( PAIR( num, str ) );
}

void FOMsg::AddBinary( uint num, const uchar* binary, uint len )
{
    CharVec str;
    str.reserve( len * 2 + 1 );
    for( uint i = 0; i < len; i++ )
    {
        char c = (char) binary[ i ];
        if( c == 0 || c == '}' )
        {
            str.push_back( 2 );
            str.push_back( c + 1 );
        }
        else
        {
            str.push_back( 1 );
            str.push_back( c );
        }
    }
    str.push_back( 0 );

    AddStr( num, (char*) &str[ 0 ] );
}

uint FOMsg::AddStr( const char* str )
{
    uint i = Random( 100000000, 999999999 );
    if( strData.count( i ) )
        return AddStr( str );
    AddStr( i, str );
    return i;
}

const char* FOMsg::GetStr( uint num )
{
    uint str_count = (uint) strData.count( num );
    auto it = strData.find( num );

    switch( str_count )
    {
    case 0:
        return "error";
    case 1:
        break;
    default:
        for( int i = 0, j = Random( 0, str_count ) - 1; i < j; i++ )
            ++it;
        break;
    }

    return ( *it ).second.c_str();
}

const char* FOMsg::GetStr( uint num, uint skip )
{
    uint str_count = (uint) strData.count( num );
    auto it = strData.find( num );

    if( skip >= str_count )
        return "error";
    for( uint i = 0; i < skip; i++ )
        ++it;

    return ( *it ).second.c_str();
}

uint FOMsg::GetStrNumUpper( uint num )
{
    auto it = strData.upper_bound( num );
    if( it == strData.end() )
        return 0;
    return ( *it ).first;
}

uint FOMsg::GetStrNumLower( uint num )
{
    auto it = strData.lower_bound( num );
    if( it == strData.end() )
        return 0;
    return ( *it ).first;
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

    return atoi( ( *it ).second.c_str() );
}

const uchar* FOMsg::GetBinary( uint num, uint& len )
{
    if( !Count( num ) )
        return NULL;

    static THREAD UCharVec* binary = NULL;
    if( !binary )
        binary = new UCharVec();

    const char* str = GetStr( num );
    binary->clear();
    for( int i = 0, j = Str::Length( str ); i < j - 1; i += 2 )
    {
        if( str[ i ] == 1 )
            binary->push_back( str[ i + 1 ] );
        else if( str[ i ] == 2 )
            binary->push_back( str[ i + 1 ] - 1 );
        else
            return NULL;
    }

    len = (uint) binary->size();
    return &( *binary )[ 0 ];
}

int FOMsg::Count( uint num )
{
    return !num ? 0 : (uint) strData.count( num );
}

void FOMsg::EraseStr( uint num )
{
    while( true )
    {
        auto it = strData.find( num );
        if( it != strData.end() )
            strData.erase( it );
        else
            break;
    }
}

uint FOMsg::GetSize()
{
    return (uint) strData.size() - 1;
}

UIntStrMulMap& FOMsg::GetData()
{
    return strData;
}

void FOMsg::GetBinaryData( UCharVec& data )
{
    // Fill raw data
    uint count = (uint) strData.size();
    data.resize( sizeof( count ) );
    memcpy( &data[ 0 ], &count, sizeof( count ) );
    for( auto it = strData.begin(), end = strData.end(); it != end; ++it )
    {
        uint    num = ( *it ).first;
        string& str = ( *it ).second;
        uint    str_len = (uint) str.size();

        data.resize( data.size() + sizeof( num ) + sizeof( str_len ) + str_len );
        memcpy( &data[ data.size() - ( sizeof( num ) + sizeof( str_len ) + str_len ) ], &num, sizeof( num ) );
        memcpy( &data[ data.size() - ( sizeof( str_len ) + str_len ) ], &str_len, sizeof( str_len ) );
        memcpy( &data[ data.size() - str_len ], (void*) str.c_str(), str_len );
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
        memcpy( &str[ 0 ], buf, str_len );
        buf += str_len;

        AddStr( num, str );
    }

    return true;
}

bool FOMsg::LoadFromFile( const char* fname, int path_type )
{
    Clear();

    FileManager fm;
    if( !fm.LoadFile( fname, path_type ) )
        return false;
    uint  buf_len = fm.GetFsize();
    char* buf = (char*) fm.ReleaseBuffer();

    LoadFromString( buf, buf_len );
    SAFEDELA( buf );
    return true;
}

void FOMsg::LoadFromString( const char* str, uint str_len )
{
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

        // Parse number
        uint num = Str::AtoUI( pbuf );
        if( !num )
        {
            Str::SkipLine( pbuf );
            continue;
        }

        // Skip 'Number}{}{'
        Str::GoTo( pbuf, '{', true );
        Str::GoTo( pbuf, '{', true );
        if( !*pbuf )
            break;

        // Find '}'
        char* _pbuf = pbuf;
        Str::GoTo( pbuf, '}' );
        if( !*pbuf )
            break;
        *pbuf = 0;

        AddStr( num, _pbuf );
        pbuf++;
    }

    SAFEDELA( str_copy );
}

bool FOMsg::SaveToFile( const char* fname, int path_type )
{
    string str;
    for( auto it = strData.begin(), end = strData.end(); it != end; it++ )
    {
        str += "{";
        str += Str::UItoA( ( *it ).first );
        str += "}{}{";
        str += ( *it ).second;
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
    else if( Str::CompareCase( type_name, "craft" ) )
        return TEXTMSG_CRAFT;
    else if( Str::CompareCase( type_name, "fix" ) )
        return TEXTMSG_CRAFT;
    else if( Str::CompareCase( type_name, "internal" ) )
        return TEXTMSG_INTERNAL;
    return -1;
}

bool LanguagePack::LoadFromFiles( const char* lang_name )
{
    memcpy( NameStr, lang_name, 4 );

    int count_fail = 0;
    for( int i = 0; i < TEXTMSG_COUNT; i++ )
    {
        if( !Msg[ i ].LoadFromFile( Str::FormatBuf( "%s" DIR_SLASH_S "%s", NameStr, TextMsgFileName[ i ] ), PT_SERVER_TEXTS ) )
        {
            count_fail++;
            WriteLogF( _FUNC_, " - Unable to load MSG<%s> from file.\n", TextMsgFileName[ i ] );
        }
    }

    return count_fail == 0;
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
    {
        WriteLogF( _FUNC_, " - Cached language<%s> not found.\n", NameStr );

        string lang_default = Crypt.GetCache( "lang_default" );
        if( lang_default.size() == 4 && lang_default != NameStr )
            return LoadFromCache( lang_default.c_str() );
    }

    return errors == 0;
}

char* LanguagePack::GetMsgCacheName( int msg_num, char* result )
{
    return Str::Format( result, CACHE_MSG_PREFIX "\\%s\\%s", NameStr, TextMsgFileName[ msg_num ] );
}
