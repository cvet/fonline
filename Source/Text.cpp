#include "StdAfx.h"
#include "Text.h"
#include "Crypt.h"
#include "FL/case.h"

void Str::Copy( char* to, uint size, const char* from )
{
    if( !from )
    {
        to[ 0 ] = 0;
        return;
    }

    uint from_len = Length( from );
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

void Str::Append( char* to, uint size, const char* from )
{
    uint from_len = Length( from );
    if( !from_len )
        return;

    uint to_len = Length( to );
    uint to_free = size - to_len;
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
    uint  len = Length( str );
    char* dup = new char[ len + 1 ];
    if( !dup )
        return NULL;
    if( len )
        memcpy( dup, str, len );
    dup[ len ] = 0;
    return dup;
}

void Str::Lower( char* str )
{
    while( *str )
    {
        if( *str >= 'A' && *str <= 'Z' )
            *str += 0x20;
        str++;
    }
}

uint Str::LowerUTF8( uint ucs )
{
    // Taked from FLTK
    uint ret;
    if( ucs <= 0x02B6 )
    {
        if( ucs >= 0x0041 )
        {
            ret = ucs_table_0041[ ucs - 0x0041 ];
            if( ret > 0 )
                return ret;
        }
        return ucs;
    }
    if( ucs <= 0x0556 )
    {
        if( ucs >= 0x0386 )
        {
            ret = ucs_table_0386[ ucs - 0x0386 ];
            if( ret > 0 )
                return ret;
        }
        return ucs;
    }
    if( ucs <= 0x10C5 )
    {
        if( ucs >= 0x10A0 )
        {
            ret = ucs_table_10A0[ ucs - 0x10A0 ];
            if( ret > 0 )
                return ret;
        }
        return ucs;
    }
    if( ucs <= 0x1FFC )
    {
        if( ucs >= 0x1E00 )
        {
            ret = ucs_table_1E00[ ucs - 0x1E00 ];
            if( ret > 0 )
                return ret;
        }
        return ucs;
    }
    if( ucs <= 0x2133 )
    {
        if( ucs >= 0x2102 )
        {
            ret = ucs_table_2102[ ucs - 0x2102 ];
            if( ret > 0 )
                return ret;
        }
        return ucs;
    }
    if( ucs <= 0x24CF )
    {
        if( ucs >= 0x24B6 )
        {
            ret = ucs_table_24B6[ ucs - 0x24B6 ];
            if( ret > 0 )
                return ret;
        }
        return ucs;
    }
    if( ucs <= 0x33CE )
    {
        if( ucs >= 0x33CE )
        {
            ret = ucs_table_33CE[ ucs - 0x33CE ];
            if( ret > 0 )
                return ret;
        }
        return ucs;
    }
    if( ucs <= 0xFF3A )
    {
        if( ucs >= 0xFF21 )
        {
            ret = ucs_table_FF21[ ucs - 0xFF21 ];
            if( ret > 0 )
                return ret;
        }
        return ucs;
    }
    return ucs;
}

void Str::LowerUTF8( char* str )
{
    for( uint length; *str; str += length )
    {
        uint ucs = DecodeUTF8( str, &length );
        ucs = LowerUTF8( ucs );
        char buf[ 4 ];
        uint length_ = EncodeUTF8( ucs, buf );
        if( length_ == length )
            memcpy( str, buf, length );
    }
}

void Str::Upper( char* str )
{
    while( *str )
    {
        if( *str >= 'a' && *str <= 'z' )
            *str -= 0x20;
        str++;
    }
}

uint Str::UpperUTF8( uint ucs )
{
    // Taked from FLTK
    static unsigned short* table = NULL;
    if( !table )
    {
        table = (unsigned short*) malloc( sizeof( unsigned short ) * 0x10000 );
        for( uint i = 0; i < 0x10000; i++ )
        {
            table[ i ] = (unsigned short) i;
        }
        for( uint i = 0; i < 0x10000; i++ )
        {
            uint l = LowerUTF8( i );
            if( l != i )
                table[ l ] = (unsigned short) i;
        }
    }
    if( ucs >= 0x10000 )
        return ucs;
    return table[ ucs ];
}

void Str::UpperUTF8( char* str )
{
    for( uint length; *str; str += length )
    {
        uint ucs = DecodeUTF8( str, &length );
        ucs = UpperUTF8( ucs );
        char buf[ 4 ];
        uint length_ = EncodeUTF8( ucs, buf );
        if( length_ == length )
            memcpy( str, buf, length );
    }
}

char* Str::Substring( char* str, const char* sub_str )
{
    return strstr( str, sub_str );
}

const char* Str::Substring( const char* str, const char* sub_str )
{
    return strstr( str, sub_str );
}

bool Str::IsValidUTF8( uint ucs )
{
    return ucs != 0xFFFD /* Unicode REPLACEMENT CHARACTER */ && ucs <= 0x10FFFF;
}

bool Str::IsValidUTF8( const char* str )
{
    while( *str )
    {
        uint length;
        if( !IsValidUTF8( DecodeUTF8( str, &length ) ) )
            return false;
        str += length;
    }
    return true;
}

uint Str::DecodeUTF8( const char* str, uint* length )
{
    // Taked from FLTK
    unsigned char c = *(unsigned char*) str;
    if( c < 0x80 )
    {
        if( length )
            *length = 1;
        return c;
    }
    else if( c < 0xc2 )
    {
        goto FAIL;
    }
    if( ( str[ 1 ] & 0xc0 ) != 0x80 )
    {
        goto FAIL;
    }
    if( c < 0xe0 )
    {
        if( length )
            *length = 2;
        return ( ( str[ 0 ] & 0x1f ) << 6 ) +
               ( ( str[ 1 ] & 0x3f ) );
    }
    else if( c == 0xe0 )
    {
        if( ( (unsigned char*) str )[ 1 ] < 0xa0 )
            goto FAIL;
        goto UTF8_3;
    }
    else if( c < 0xf0 )
    {
UTF8_3:
        if( ( str[ 2 ] & 0xc0 ) != 0x80 )
            goto FAIL;
        if( length )
            *length = 3;
        return ( ( str[ 0 ] & 0x0f ) << 12 ) +
               ( ( str[ 1 ] & 0x3f ) << 6 ) +
               ( ( str[ 2 ] & 0x3f ) );
    }
    else if( c == 0xf0 )
    {
        if( ( (unsigned char*) str )[ 1 ] < 0x90 )
            goto FAIL;
        goto UTF8_4;
    }
    else if( c < 0xf4 )
    {
UTF8_4:
        if( ( str[ 2 ] & 0xc0 ) != 0x80 || ( str[ 3 ] & 0xc0 ) != 0x80 )
            goto FAIL;
        if( length )
            *length = 4;
        return ( ( str[ 0 ] & 0x07 ) << 18 ) +
               ( ( str[ 1 ] & 0x3f ) << 12 ) +
               ( ( str[ 2 ] & 0x3f ) << 6 ) +
               ( ( str[ 3 ] & 0x3f ) );
    }
    else if( c == 0xf4 )
    {
        if( ( (unsigned char*) str )[ 1 ] > 0x8f )
            goto FAIL;                                /* after 0x10ffff */
        goto UTF8_4;
    }
    else
    {
FAIL:
        if( length )
            *length = 1;
        return 0xfffd; /* Unicode REPLACEMENT CHARACTER */
    }
}

uint Str::EncodeUTF8( uint ucs, char* buf )
{
    // Taked from FLTK
    if( ucs < 0x000080U )
    {
        buf[ 0 ] = ucs;
        return 1;
    }
    else if( ucs < 0x000800U )
    {
        buf[ 0 ] = 0xc0 | ( ucs >> 6 );
        buf[ 1 ] = 0x80 | ( ucs & 0x3F );
        return 2;
    }
    else if( ucs < 0x010000U )
    {
        buf[ 0 ] = 0xe0 | ( ucs >> 12 );
        buf[ 1 ] = 0x80 | ( ( ucs >> 6 ) & 0x3F );
        buf[ 2 ] = 0x80 | ( ucs & 0x3F );
        return 3;
    }
    else if( ucs <= 0x0010ffffU )
    {
        buf[ 0 ] = 0xf0 | ( ucs >> 18 );
        buf[ 1 ] = 0x80 | ( ( ucs >> 12 ) & 0x3F );
        buf[ 2 ] = 0x80 | ( ( ucs >> 6 ) & 0x3F );
        buf[ 3 ] = 0x80 | ( ucs & 0x3F );
        return 4;
    }
    else
    {
        /* encode 0xfffd: */
        buf[ 0 ] = 0xefU;
        buf[ 1 ] = 0xbfU;
        buf[ 2 ] = 0xbdU;
        return 3;
    }
}

uint Str::Length( const char* str )
{
    const char* str_ = str;
    while( *str )
        str++;
    return (uint) ( str - str_ );
}

uint Str::LengthUTF8( const char* str )
{
    uint len = 0;
    while( *str )
        len += ( ( *str++ & 0xC0 ) != 0x80 );
    return len;
}

bool Str::Compare( const char* str1, const char* str2 )
{
    while( *str1 && *str2 )
    {
        if( *str1 != *str2 )
            return false;
        str1++, str2++;
    }
    return *str1 == 0 && *str2 == 0;
}

bool Str::CompareCase( const char* str1, const char* str2 )
{
    while( *str1 && *str2 )
    {
        char c1 = *str1;
        char c2 = *str2;
        if( c1 >= 'A' && c1 <= 'Z' )
            c1 += 0x20;
        if( c2 >= 'A' && c2 <= 'Z' )
            c2 += 0x20;
        if( c1 != c2 )
            return false;
        str1++, str2++;
    }
    return *str1 == 0 && *str2 == 0;
}

bool Str::CompareCaseUTF8( const char* str1, const char* str2 )
{
    static char str1_buf[ MAX_FOTEXT ];
    static char str2_buf[ MAX_FOTEXT ];
    Copy( str1_buf, str1 );
    Copy( str2_buf, str2 );
    LowerUTF8( str1_buf );
    LowerUTF8( str2_buf );
    return Compare( str1_buf, str2_buf );
}

bool Str::CompareCount( const char* str1, const char* str2, uint max_count )
{
    while( *str1 && *str2 && max_count )
    {
        if( *str1 != *str2 )
            return false;
        str1++, str2++;
        max_count--;
    }
    return max_count == 0;
}

bool Str::CompareCaseCount( const char* str1, const char* str2, uint max_count )
{
    while( *str1 && *str2 && max_count )
    {
        char c1 = *str1;
        char c2 = *str2;
        if( c1 >= 'A' && c1 <= 'Z' )
            c1 += 0x20;
        if( c2 >= 'A' && c2 <= 'Z' )
            c2 += 0x20;
        if( c1 != c2 )
            return false;
        str1++, str2++;
        max_count--;
    }
    return max_count == 0;
}

bool Str::CompareCaseCountUTF8( const char* str1, const char* str2, uint max_count )
{
    static char str1_buf[ MAX_FOTEXT ];
    static char str2_buf[ MAX_FOTEXT ];
    Copy( str1_buf, str1 );
    Copy( str2_buf, str2 );
    LowerUTF8( str1_buf );
    LowerUTF8( str2_buf );
    return CompareCount( str1_buf, str2_buf, max_count );
}

char* Str::Format( char* buf, const char* format, ... )
{
    va_list list;
    va_start( list, format );
    vsprintf( buf, format, list );
    va_end( list );
    return buf;
}

const char* Str::FormatBuf( const char* format, ... )
{
    static THREAD char buf[ 0x4000 ];
    va_list            list;
    va_start( list, format );
    vsprintf( buf, format, list );
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

    int flen = Length( from );
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
                    EraseInterval( &str[ i ], k - i + 1 );
                    break;
                }
            }
            i--;
        }
    }
}

void Str::EraseWords( char* str, const char* word )
{
    if( !str || !word )
        return;

    char* sub_str = Substring( str, word );
    while( sub_str )
    {
        EraseInterval( sub_str, Length( word ) );
        sub_str = Substring( sub_str, word );
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
        else
        {
            ++str;
        }
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
    uint len = Length( str );
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
    #else
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
    #else
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
uint Str::FormatForHash( char* name )
{
    EraseFrontBackSpecificChars( name );
    Lower( name );
    uint len = 0;
    for( char* s = name; *s; s++, len++ )
        if( *s == '\\' )
            *s = '/';
    return len;
}

uint Str::GetHash( const char* name )
{
    if( !name )
        return 0;

    char name_[ MAX_FOPATH ];
    Copy( name_, name );
    uint len = FormatForHash( name_ );
    return Crypt.Crc32( (uchar*) name_, len );
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
    char name_[ MAX_FOPATH ];
    Copy( name_, name );
    uint len = FormatForHash( name_ );
    uint hash = Crypt.Crc32( (uchar*) name_, len );

    auto it = NamesHash.find( hash );
    if( it == NamesHash.end() )
    {
        NamesHash.insert( PAIR( hash, name ) );
    }
    else
    {
        char name__[ MAX_FOPATH ];
        Copy( name__, ( *it ).second.c_str() );
        FormatForHash( name__ );
        if( !Compare( name_, name__ ) )
            WriteLogF( _FUNC_, " - Found equal hash for different names, name1<%s>, name2<%s>, hash<%u>.\n", name, ( *it ).second.c_str(), hash );
    }
}

const char* Str::ParseLineDummy( const char* str )
{
    return str;
}
