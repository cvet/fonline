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
    auto it = r.strData.begin();
    auto end = r.strData.end();
    it++;     // skip FOMSG_ERRNUM
    for( ; it != end; ++it )
    {
        EraseStr( ( *it ).first );
        AddStr( ( *it ).first, ( *it ).second );
    }
    CalculateHash();
    return *this;
}

void FOMsg::AddStr( uint num, const char* str )
{
    if( num == FOMSG_ERRNUM )
        return;
    if( !str || !Str::Length( str ) )
        strData.insert( PAIR( num, " " ) );
    else
        strData.insert( PAIR( num, str ) );
}

void FOMsg::AddStr( uint num, const string& str )
{
    if( num == FOMSG_ERRNUM )
        return;
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
        return ( *strData.begin() ).second.c_str();       // give FOMSG_ERRNUM
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
        return ( *strData.begin() ).second.c_str();                   // give FOMSG_ERRNUM
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
        binary = new (nothrow) UCharVec();

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
    if( num == FOMSG_ERRNUM )
        return;

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

void FOMsg::CalculateHash()
{
    strDataHash = 0;
    #ifdef FONLINE_SERVER
    toSend.clear();
    #endif
    auto it = strData.begin();
    auto end = strData.end();
    it++;     // skip FOMSG_ERRNUM
    for( ; it != end; ++it )
    {
        uint    num = ( *it ).first;
        string& str = ( *it ).second;
        uint    str_len = (uint) str.size();

        #ifdef FONLINE_SERVER
        toSend.resize( toSend.size() + sizeof( num ) + sizeof( str_len ) + str_len );
        memcpy( &toSend[ toSend.size() - ( sizeof( num ) + sizeof( str_len ) + str_len ) ], &num, sizeof( num ) );
        memcpy( &toSend[ toSend.size() - ( sizeof( str_len ) + str_len ) ], &str_len, sizeof( str_len ) );
        memcpy( &toSend[ toSend.size() - str_len ], (void*) str.c_str(), str_len );
        #endif

        Crypt.Crc32( (uchar*) &num, sizeof( num ), strDataHash );
        Crypt.Crc32( (uchar*) &str_len, sizeof( str_len ), strDataHash );
        Crypt.Crc32( (uchar*) str.c_str(), str_len, strDataHash );
    }
}

uint FOMsg::GetHash()
{
    return strDataHash;
}

UIntStrMulMap& FOMsg::GetData()
{
    return strData;
}

#ifdef FONLINE_SERVER
const char* FOMsg::GetToSend()
{
    return &toSend[ 0 ];
}

uint FOMsg::GetToSendLen()
{
    return (uint) toSend.size();
}
#endif

#ifdef FONLINE_CLIENT
int FOMsg::LoadMsgStream( CharVec& stream )
{
    Clear();

    if( !stream.size() )
        return 0;

    uint   pos = 0;
    uint   num = 0;
    uint   len = 0;
    string str;
    while( true )
    {
        if( pos + sizeof( num ) > stream.size() )
            break;
        memcpy( &num, &stream[ pos ], sizeof( num ) );
        pos += sizeof( num );

        if( pos + sizeof( len ) > stream.size() )
            break;
        memcpy( &len, &stream[ pos ], sizeof( len ) );
        pos += sizeof( len );

        if( pos + len > stream.size() )
            break;
        str.resize( len );
        memcpy( &str[ 0 ], &stream[ pos ], len ); // !!!
        pos += len;

        AddStr( num, str );
    }
    return GetSize();
}
#endif

int FOMsg::LoadMsgFile( const char* fname, int path_type )
{
    Clear();

    #ifdef FONLINE_CLIENT
    uint  buf_len;
    char* buf = (char*) Crypt.GetCache( fname, buf_len );
    if( !buf )
        return -1;
    #else
    FileManager fm;
    if( !fm.LoadFile( fname, path_type ) )
        return -2;
    uint  buf_len = fm.GetFsize();
    char* buf = (char*) fm.ReleaseBuffer();
    #endif

    int result = LoadMsgFileBuf( buf, buf_len );
    delete[] buf;
    return result;
}

int FOMsg::LoadMsgFileBuf( char* data, uint data_len )
{
    Clear();

    #ifdef FONLINE_CLIENT
    char* buf = (char*) Crypt.Uncompress( (uchar*) data, data_len, 10 );
    if( !buf )
        return -3;
    #else
    char* buf = data;
    uint  last_num = 0;
    #endif

    char* pbuf = buf;
    for( uint i = 0; *pbuf && i < data_len; i++ )
    {
        // Find '{' in begin of line
        if( *pbuf != '{' )
        {
            Str::SkipLine( pbuf );
            continue;
        }

        // Skip '{'
        pbuf++;
        if( !*pbuf )
            break;

        // atoi
        uint num_info = Str::AtoUI( pbuf );
        if( !num_info )
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

        #ifndef FONLINE_CLIENT
        if( num_info < last_num )
        {
            WriteLogF( _FUNC_, " - Error string id, cur<%u>, last<%u>\n", num_info, last_num );
            return -4;
        }
        last_num = num_info;
        #endif

        AddStr( num_info, _pbuf );
        pbuf++;
    }

    #ifdef FONLINE_CLIENT
    delete[] buf;
    #endif
    CalculateHash();
    return GetSize();
}

int FOMsg::SaveMsgFile( const char* fname, int path_type )
{
    #ifndef FONLINE_CLIENT
    FileManager fm;
    #endif

    auto it = strData.begin();
    it++;     // skip FOMSG_ERRNUM

    string str;
    for( ; it != strData.end(); it++ )
    {
        str += "{";
        str += Str::UItoA( ( *it ).first );
        str += "}{}{";
        str += ( *it ).second;
        str += "}\n";
    }

    char* buf = (char*) str.c_str();
    uint  buf_len = (uint) str.length();

    #ifdef FONLINE_CLIENT
    buf = (char*) Crypt.Compress( (uchar*) buf, buf_len );
    if( !buf )
        return -2;
    Crypt.SetCache( fname, (uchar*) buf, buf_len );
    delete[] buf;
    #else
    fm.SetData( buf, buf_len );
    if( !fm.SaveOutBufToFile( fname, path_type ) )
        return -2;
    #endif

    return 1;
}

void FOMsg::Clear()
{
    strData.clear();
    strData.insert( PAIR( FOMSG_ERRNUM, string( "error" ) ) );

    #ifdef FONLINE_SERVER
    toSend.clear();
    #endif

    strDataHash = 0;
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

bool LanguagePack::Init( const char* lang, int path_type )
{
    memcpy( NameStr, lang, 4 );
    PathType = path_type;
    if( LoadAll() < 0 )
        return false;
    return true;
}

int LanguagePack::LoadAll()
{
    // Loading All MSG files
    if( !Name )
    {
        WriteLogF( _FUNC_, " - Lang Pack is not initialized.\n" );
        return -1;
    }

    int count_fail = 0;
    for( int i = 0; i < TEXTMSG_COUNT; i++ )
    {
        if( Msg[ i ].LoadMsgFile( Str::FormatBuf( "%s" DIR_SLASH_S "%s", NameStr, TextMsgFileName[ i ] ), PathType ) < 0 )
        {
            count_fail++;
            WriteLogF( _FUNC_, " - Unable to load MSG<%s>.\n", TextMsgFileName[ i ] );
        }
    }

    return -count_fail;
}
