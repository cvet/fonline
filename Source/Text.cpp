#include "Common.h"
#include "Text.h"
#include "Crypt.h"
#include "src/xutf8/headers/case.h"

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
        return nullptr;
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
    static unsigned short* table = nullptr;
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

char* Str::LastSubstring( char* str, const char* sub_str )
{
    uint  len = Length( sub_str );
    char* last = nullptr;
    while( true )
    {
        str = strstr( str, sub_str );
        if( !str )
            break;
        last = str;
        str += len;
    }
    return last;
}

const char* Str::LastSubstring( const char* str, const char* sub_str )
{
    uint        len = Length( sub_str );
    const char* last = nullptr;
    while( true )
    {
        str = strstr( str, sub_str );
        if( !str )
            break;
        last = str;
        str += len;
    }
    return last;
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
    else if( ( str[ 1 ] & 0xc0 ) != 0x80 )
    {
        goto FAIL;
    }
    else if( c < 0xe0 )
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

void Str::EraseInterval( char* str, uint len )
{
    if( !str || !len )
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

void Str::Insert( char* to, const char* from, uint from_len /* = 0 */ )
{
    if( !to || !from )
        return;

    if( !from_len )
        from_len = Length( from );
    if( !from_len )
        return;

    char* end_to = to;
    while( *end_to )
        ++end_to;

    for( ; end_to >= to; --end_to )
        *( end_to + from_len ) = *end_to;

    while( from_len-- )
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

void Str::ReplaceText( char* str, const char* from, const char* to )
{
    uint from_len = Length( from );
    uint to_len = Length( to );
    while( true )
    {
        str = Substring( str, from );
        if( !str )
            break;

        EraseInterval( str, from_len );
        Insert( str, to, to_len );
        str += to_len;
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

char* Str::Trim( char* str, uint* trimmed /* = NULL */ )
{
    char* front = str;
    while( *front && ( *front == ' ' || *front == '\t' || *front == '\n' || *front == '\r' ) )
    {
        front++;
        if( trimmed )
            *trimmed++;
    }

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
    {
        back--;
        if( trimmed )
            *trimmed++;
    }
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
        return (int) strtol( str + 2, nullptr, 16 );
    return atoi( str );
}

int64 Str::AtoI64( const char* str )
{
    if( str[ 0 ] && str[ 0 ] == '0' && ( str[ 1 ] == 'x' || str[ 1 ] == 'X' ) )
        return (int64) strtol( str + 2, nullptr, 16 );
    #ifdef FO_WINDOWS
    return _atoi64( str );
    #else
    return strtoll( str, nullptr, 10 );
    #endif
}

uint Str::AtoUI( const char* str )
{
    if( str[ 0 ] && str[ 0 ] == '0' && ( str[ 1 ] == 'x' || str[ 1 ] == 'X' ) )
        return (uint) strtoul( str + 2, nullptr, 16 );
    return (uint) strtoul( str, nullptr, 10 );
}

void Str::HexToStr( uchar hex, char* str )
{
    for( int i = 0; i < 2; i++ )
    {
        int val = ( i == 0 ? hex >> 4 : hex & 0xF );
        if( val < 10 )
            *str++ = '0' + val;
        else
            *str++ = 'A' + val - 10;
    }
}

uchar Str::StrToHex( const char* str )
{
    uchar result = 0;
    for( int i = 0; i < 2; i++ )
    {
        char c = *str++;
        if( c < 'A' )
            result |= ( c - '0' ) << ( i == 0 ? 4 : 0 );
        else
            result |= ( c - 'A' + 10 ) << ( i == 0 ? 4 : 0 );
    }
    return result;
}

static THREAD char BigBuf[ BIG_BUF_SIZE ] = { 0 };
char* Str::GetBigBuf()
{
    return BigBuf;
}

static Mutex                    HashNamesLocker;
static map< hash, const char* > HashNames;

#define HASH_IMPL( var, name )    hash var = Str::GetHash( name )
HASH_IMPL( ITEM_DEF_SLOT, "default_weapon" );
HASH_IMPL( ITEM_DEF_ARMOR, "default_armor" );
HASH_IMPL( SP_SCEN_IBLOCK, "minimap_invisible_block" );
HASH_IMPL( SP_SCEN_TRIGGER, "trigger" );
HASH_IMPL( SP_WALL_BLOCK_LIGHT, "block_light" );
HASH_IMPL( SP_WALL_BLOCK, "block" );
HASH_IMPL( SP_GRID_EXITGRID, "exit_grid" );
HASH_IMPL( SP_GRID_ENTIRE, "entrance" );
HASH_IMPL( SP_MISC_SCRBLOCK, "scroll_block" );

hash Str::GetHash( const char* name )
{
    if( !name || !name[ 0 ] )
        return 0;

    // Copy and swap '\' to '/'
    char        name_[ MAX_FOTEXT ];
    uint        len = 0;
    const char* from = name;
    char*       to = name_;
    for( ; *from; from++, to++, len++ )
        *to = ( *from != '\\' ? *from : '/' );
    *to = 0;

    // Trim specific chars
    uint trimmed = 0;
    Str::Trim( name_, &trimmed );
    len -= trimmed;
    if( !len )
        return 0;

    // Calculate hash
    hash h = Crypt.MurmurHash2( (const uchar*) name_, len );
    if( !h )
        return 0;

    // Add hash
    SCOPE_LOCK( HashNamesLocker );

    auto ins = HashNames.insert( PAIR( h, nullptr ) );
    if( ins.second )
        ins.first->second = Str::Duplicate( name_ );
    else if( !Str::Compare( ins.first->second, name_ ) )
        WriteLog( "Hash collision detected for names '%s' and '%s', hash %08X.\n", name_, ins.first->second, h );

    return h;
}

const char* Str::GetName( hash h )
{
    SCOPE_LOCK( HashNamesLocker );

    auto it = HashNames.find( h );
    if( it == HashNames.end() )
    {
        static THREAD char error[ MAX_FOTEXT ];
        Format( error, "(unknown hash %08X)", h );
        return error;
    }
    return it->second;
}

void Str::SaveHashes( void ( * save_func )( void*, size_t ) )
{
    SCOPE_LOCK( HashNamesLocker );

    for( auto it = HashNames.begin(); it != HashNames.end(); ++it )
    {
        const char* name = it->second;
        uint        name_len = Str::Length( name );
        if( name_len <= 255 )
        {
            uchar name_len_ = (uchar) name_len;
            save_func( &name_len_, sizeof( name_len_ ) );
            save_func( (void*) name, name_len );
        }
    }
    uchar zero = 0;
    save_func( &zero, sizeof( zero ) );
}

void Str::LoadHashes( void* f, uint version )
{
    SCOPE_LOCK( HashNamesLocker );

    char buf[ 256 ];
    while( true )
    {
        uchar name_len = 0;
        FileRead( f, &name_len, sizeof( name_len ) );
        if( !name_len )
            break;

        FileRead( f, buf, name_len );
        buf[ name_len ] = 0;

        GetHash( buf );
    }
}

const char* Str::ParseLineDummy( const char* str )
{
    return str;
}
