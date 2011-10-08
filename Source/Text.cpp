#include "StdAfx.h"
#include "Text.h"
#include "Crypt.h"

void Str::Copy( char* to, size_t size, const char* from )
{
    if( !from )
    {
        to[ 0 ] = 0;
        return;
    }

    size_t from_len = Str::Length( from );
    if( !from_len )
    {
        to[ 0 ] = 0;
        return;
    }

    if( from_len >= size )
    {
        memcpy( to, from, size - 1 );
        to[ size - 1 ] = 0;
    }
    else
    {
        memcpy( to, from, from_len );
        to[ from_len ] = 0;
    }
}

void Str::Append( char* to, size_t size, const char* from )
{
    size_t from_len = Str::Length( from );
    if( !from_len )
        return;

    size_t to_len = Str::Length( to );
    size_t to_free = size - to_len;
    if( (int) to_free <= 1 )
        return;

    char* ptr = to + to_len;

    if( from_len >= to_free )
    {
        memcpy( ptr, from, to_free - 1 );
        to[ size - 1 ] = 0;
    }
    else
    {
        memcpy( ptr, from, from_len );
        ptr[ from_len ] = 0;
    }
}

char* Str::Duplicate( const char* str )
{
    size_t len = Str::Length( str );
    char*  dup = new ( nothrow ) char[ len + 1 ];
    if( !dup )
        return NULL;
    if( len )
        memcpy( dup, str, len );
    dup[ len ] = 0;
    return dup;
}

char* Str::Lower( char* str )
{
    #if defined ( FO_WINDOWS )
    _strlwr( str );
    #else // FO_LINUX
    for( ; *str; ++str )
        *str = tolower( *str );
    #endif
    return str;
}

char* Str::Upper( char* str )
{
    #if defined ( FO_WINDOWS )
    _strupr( str );
    #else // FO_LINUX
    for( ; *str; ++str )
        *str = toupper( *str );
    #endif
    return str;
}

char* Str::Substring( char* str, const char* sub_str )
{
    return strstr( str, sub_str );
}

const char* Str::Substring( const char* str, const char* sub_str )
{
    return strstr( str, sub_str );
}

uint Str::Length( const char* str )
{
    return (uint) strlen( str );
}

bool Str::Compare( const char* str1, const char* str2 )
{
    return strcmp( str1, str2 ) == 0;
}

bool Str::CompareCase( const char* str1, const char* str2 )
{
    #if defined ( FO_WINDOWS )
    return _stricmp( str1, str2 ) == 0;
    #else // FO_LINUX
    return strcasecmp( str1, str2 ) == 0;
    #endif
}

bool Str::CompareCount( const char* str1, const char* str2, uint max_count )
{
    return strncmp( str1, str2, max_count ) == 0;
}

bool Str::CompareCaseCount( const char* str1, const char* str2, uint max_count )
{
    #if defined ( FO_WINDOWS )
    return _strnicmp( str1, str2, max_count ) == 0;
    #else
    return strncasecmp( str1, str2, max_count ) == 0;
    #endif
}

char* Str::Format( char* buf, const char* format, ... )
{
    va_list list;
    va_start( list, format );
    #if defined ( FO_MSVC )
    vsprintf_s( buf, MAX_FOTEXT, format, list );
    #else
    vsprintf( buf, format, list );
    #endif
    va_end( list );
    return buf;
}

const char* Str::FormatBuf( const char* format, ... )
{
    static THREAD char buf[ 0x4000 ];
    va_list            list;
    va_start( list, format );
    #if defined ( FO_MSVC )
    vsprintf_s( buf, MAX_FOTEXT, format, list );
    #else
    vsprintf( buf, format, list );
    #endif
    va_end( list );
    return buf;
}

void Str::ChangeValue( char* str, int value )
{
    for( int i = 0; str[ i ]; ++i )
        str[ i ] += value;
}

void Str::EraseInterval( char* str, int len )
{
    if( !str || len <= 0 )
        return;

    char* str2 = str + len;
    while( *str2 )
    {
        *str = *str2;
        ++str;
        ++str2;
    }

    *str = 0;
}

void Str::Insert( char* to, const char* from )
{
    if( !to || !from )
        return;

    int flen = Str::Length( from );
    if( !flen )
        return;

    char* end_to = to;
    while( *end_to )
        ++end_to;

    for( ; end_to >= to; --end_to )
        *( end_to + flen ) = *end_to;

    while( *from )
    {
        *to = *from;
        ++to;
        ++from;
    }
}

void Str::EraseWords( char* str, char begin, char end )
{
    if( !str )
        return;

    for( int i = 0; str[ i ]; ++i )
    {
        if( str[ i ] == begin )
        {
            for( int k = i + 1; ; ++k )
            {
                if( !str[ k ] || str[ k ] == end )
                {
                    Str::EraseInterval( &str[ i ], k - i + 1 );
                    break;
                }
            }
            i--;
        }
    }
}

void Str::EraseChars( char* str, char ch )
{
    if( !str )
        return;

    while( *str )
    {
        if( *str == ch )
            CopyBack( str );
        else
            ++str;
    }
}

void Str::CopyWord( char* to, const char* from, char end, bool include_end /* = false */ )
{
    if( !from || !to )
        return;

    *to = 0;

    while( *from && *from != end )
    {
        *to = *from;
        to++;
        from++;
    }

    if( include_end && *from )
    {
        *to = *from;
        to++;
    }

    *to = 0;
}

void Str::CopyBack( char* str )
{
    while( *str )
    {
        *str = *( str + 1 );
        str++;
    }
}

void Str::Replacement( char* str, char ch )
{
    while( *str )
    {
        if( *str == ch )
            CopyBack( str );
        else
            ++str;
    }
}

void Str::Replacement( char* str, char from, char to )
{
    while( *str )
    {
        if( *str == from )
            *str = to;
        ++str;
    }
}

void Str::Replacement( char* str, char from1, char from2, char to )
{
    while( *str && *( str + 1 ) )
    {
        if( *str == from1 && *( str + 1 ) == from2 )
        {
            CopyBack( str );
            *str = to;
        }
        ++str;
    }
}

char* Str::EraseFrontBackSpecificChars( char* str )
{
    char* front = str;
    while( *front && ( *front == ' ' || *front == '\t' || *front == '\n' || *front == '\r' ) )
        front++;
    if( front != str )
    {
        char* str_ = str;
        while( *front )
            *str_++ = *front++;
        *str_ = 0;
    }

    char* back = str;
    while( *back )
        back++;
    back--;
    while( back >= str && ( *back == ' ' || *back == '\t' || *back == '\n' || *back == '\r' ) )
        back--;
    *( back + 1 ) = 0;

    return str;
}

void Str::SkipLine( char*& str )
{
    while( *str && *str != '\n' )
        ++str;
    if( *str )
        ++str;
}

void Str::GoTo( char*& str, char ch, bool skip_char /* = false */ )
{
    while( *str && *str != ch )
        ++str;
    if( skip_char && *str )
        ++str;
}

bool Str::IsNumber( const char* str )
{
    // Check number it or not
    bool is_number = true;
    uint pos = 0;
    uint len = Str::Length( str );
    for( uint i = 0, j = len; i < j; i++, pos++ )
        if( str[ i ] != ' ' && str[ i ] != '\t' )
            break;
    if( pos >= len )
        is_number = false;            // Empty string
    for( uint i = pos, j = len; i < j; i++, pos++ )
    {
        if( !( ( str[ i ] >= '0' && str[ i ] <= '9' ) || ( i == 0 && str[ i ] == '-' ) ) )
        {
            is_number = false;           // Found not a number
            break;
        }
    }
    if( is_number && str[ pos - 1 ] == '-' )
        return false;
    return is_number;
}

const char* Str::ItoA( int i )
{
    static THREAD char str[ 128 ];
    sprintf( str, "%d", i );
    return str;
}

const char* Str::I64toA( int64 i )
{
    static THREAD char str[ 128 ];
    #ifdef FO_WINDOWS
    sprintf( str, "%I64d", i );
    #else // FO_LINUX
    sprintf( str, "%lld", i );
    #endif
    return str;
}

const char* Str::UItoA( uint dw )
{
    static THREAD char str[ 128 ];
    sprintf( str, "%u", dw );
    return str;
}

int Str::AtoI( const char* str )
{
    if( str[ 0 ] && str[ 0 ] == '0' && ( str[ 1 ] == 'x' || str[ 1 ] == 'X' ) )
        return strtol( str + 2, NULL, 16 );
    return atoi( str );
}

int64 Str::AtoI64( const char* str )
{
    if( str[ 0 ] && str[ 0 ] == '0' && ( str[ 1 ] == 'x' || str[ 1 ] == 'X' ) )
        return strtol( str + 2, NULL, 16 );
    #ifdef FO_WINDOWS
    return _atoi64( str );
    #else // FO_LINUX
    return strtoll( str, NULL, 10 );
    #endif
}

uint Str::AtoUI( const char* str )
{
    if( str[ 0 ] && str[ 0 ] == '0' && ( str[ 1 ] == 'x' || str[ 1 ] == 'X' ) )
        return strtoul( str + 2, NULL, 16 );
    return strtoul( str, NULL, 10 );
}

static char BigBuf[ BIG_BUF_SIZE ] = { 0 };
char* Str::GetBigBuf()
{
    return BigBuf;
}

static UIntStrMap NamesHash;
uint Str::GetHash( const char* str )
{
    if( !str )
        return 0;

    char str_[ MAX_FOPATH ];
    Str::Copy( str_, str );
    Lower( str_ );
    uint len = 0;
    for( char* s = str_; *s; s++, len++ )
        if( *s == '/' )
            *s = '\\';

    EraseFrontBackSpecificChars( str_ );

    return Crypt.Crc32( (uchar*) str_, len );
}

const char* Str::GetName( uint hash )
{
    if( !hash )
        return NULL;

    auto it = NamesHash.find( hash );
    return it != NamesHash.end() ? ( *it ).second.c_str() : NULL;
}

void Str::AddNameHash( const char* name )
{
    uint hash = GetHash( name );

    auto it = NamesHash.find( hash );
    if( it == NamesHash.end() )
        NamesHash.insert( PAIR( hash, name ) );
    else if( !Str::CompareCase( name, ( *it ).second.c_str() ) )
        WriteLogF( _FUNC_, " - Found equal hash for different names, name1<%s>, name2<%s>, hash<%u>.\n", name, ( *it ).second.c_str(), hash );
}

const char* Str::ParseLineDummy( const char* str )
{
    return str;
}
