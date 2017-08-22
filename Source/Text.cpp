#include "Common.h"
#include "Text.h"
#include "Crypt.h"
#include "FLTK/src/xutf8/headers/case.h"
#include <sstream>
#include "FileSystem.h"

_str& _str::trim()
{
    // Left trim
    size_t l = s.find_first_not_of( " \n\r\t" );
    if( l == string::npos )
    {
        s.erase();
    }
    else if( l > 0 )
    {
        s.erase( 0, l );

        // Right trim
        size_t r = s.find_last_not_of( " \n\r\t" );
        if( r < s.length() - 1 )
            s.erase( r + 1 );
    }
    return *this;
}

_str& _str::replace( char from, char to )
{
    std::replace( s.begin(), s.end(), from, to );
    return *this;
}

_str& _str::lower()
{
    std::transform( s.begin(), s.end(), s.begin(), tolower );
    return *this;
}

_str& _str::upper()
{
    std::transform( s.begin(), s.end(), s.begin(), toupper );
    return *this;
}

bool _str::isInt()
{
    if( s.empty() || ( !isdigit( s[ 0 ] ) && s[ 0 ] != '-' && s[ 0 ] != '+' ) )
        return false;

    char* p;
    strtol( s.c_str(), &p, 10 );
    return *p == 0;
}

int _str::toInt()
{
    return strtol( s.c_str(), nullptr, 10 );
}

_str& _str::formatPath()
{
    trim();
    normalizePathSlashes();

    // Erase first './'
    while( s[ 0 ] == '.' && s[ 1 ] == '/' )
        s.erase( 0, 2 );

    // Skip first '../'
    uint back_count = 0;
    while( s.length() >= 3 && s[ 0 ] == '.' &&
           s[ 1 ] == '.' && s[ 2 ] == '/' )
    {
        back_count++;
        s.erase( 0, 3 );
    }

    // Replace '/./' to '/'
    while( true )
    {
        size_t pos = s.find( "/./" );
        if( pos == string::npos )
            break;

        s.replace( pos, 3, "/" );
    }

    // Replace '//' to '/'
    while( true )
    {
        size_t pos = s.find( "//" );
        if( pos == string::npos )
            break;

        s.replace( pos, 2, "/" );
    }

    // Replace 'folder/../' to '/'
    while( true )
    {
        size_t pos = s.find( "/../" );
        if( pos == string::npos || pos == 0 )
            break;

        size_t pos2 = s.rfind( '/', pos - 1 );
        if( pos2 == string::npos )
            break;

        s.erase( pos2 + 1, pos - pos2 - 1 + 3 );
    }

    // Apply skipped '../'
    for( uint i = 0; i < back_count; i++ )
        s.insert( 0, "../" );

    return *this;
}

_str& _str::extractDir()
{
    formatPath();
    size_t pos = s.find_last_of( '/' );
    if( pos != string::npos )
        s = s.substr( 0, pos + 1 );
    else if( !s.empty() && s.back() != '/' )
        s += "/";
    return *this;
}

_str& _str::extractFileName()
{
    formatPath();
    size_t pos = s.find_last_of( '/' );
    if( pos != string::npos )
        s = s.substr( pos + 1 );
    return *this;
}

_str& _str::getFileExtension()
{
    size_t dot = s.find_last_of( '.' );
    s = ( dot != string::npos ? s.substr( dot + 1 ) : "" );
    lower();
    return *this;
}

_str& _str::eraseFileExtension()
{
    size_t dot = s.find_last_of( '.' );
    if( dot != string::npos )
        s = s.substr( 0, dot );
    return *this;
}

_str& _str::combinePath( const string& path )
{
    extractDir();
    if( !s.empty() && s.back() != '/' && ( path.empty() || path.front() != '/' ) )
        s += "/";
    s += path;
    formatPath();
    return *this;
}

_str& _str::forwardPath( const string& relative_dir )
{
    string dir = _str( *this ).extractDir();
    string name = _str( *this ).extractFileName();
    s = dir + relative_dir + name;
    formatPath();
    return *this;
}

_str& _str::resolvePath()
{
    ResolvePathInplace( s );
    return *this;
}

_str& _str::normalizePathSlashes()
{
    NormalizePathSlashesInplace( s );
    return *this;
}

#ifdef FO_WINDOWS
_str& _str::parseWideChar( const wchar_t* str )
{
    int len = (int) wcslen( str );
    if( len )
    {
        char* buf = (char*) alloca( UTF8_BUF_SIZE( len ) );
        int   r = WideCharToMultiByte( CP_UTF8, 0, str, len, buf, len * 4, nullptr, nullptr );
        s += string( buf, r );
    }
    return *this;
}

std::wstring _str::toWideChar()
{
    if( s.empty() )
        return L"";
    wchar_t* buf = (wchar_t*) alloca( s.length() * sizeof( wchar_t ) * 2 );
    int      len = MultiByteToWideChar( CP_UTF8, 0, s.c_str(), (int) s.length(), buf, (int) s.length() );
    return std::wstring( buf, len );
}
#endif

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

uint Str::UpperUTF8( uint ucs )
{
    // Taken from FLTK
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

const char* Str::Substring( const string& str, const char* sub_str )
{
    return strstr( str.c_str(), sub_str );
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

bool Str::Compare( const string& str1, const string& str2 )
{
    if( str1.length() != str2.length() )
        return false;
    return str1 == str2;
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

bool Str::CompareCase( const string& str1, const string& str2 )
{
    if( str1.length() != str2.length() )
        return false;

    for( size_t i = 0; i < str1.length(); i++ )
        if( tolower( str1[ i ] ) != tolower( str2[ i ] ) )
            return false;
    return true;
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

bool Str::CompareCount( const string& str1, const string& str2, uint max_count )
{
    if( str1.length() != str2.length() )
        return false;

    if( max_count > str1.length() )
        max_count = (uint) str1.length();
    for( uint i = 0; i < max_count; i++ )
        if( str1[ i ] != str2[ i ] )
            return false;
    return true;
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

bool Str::CompareCaseCount( const string& str1, const string& str2, uint max_count )
{
    if( str1.length() != str2.length() )
        return false;

    if( max_count > str1.length() )
        max_count = (uint) str1.length();
    for( uint i = 0; i < max_count; i++ )
        if( tolower( str1[ i ] ) != tolower( str2[ i ] ) )
            return false;
    return true;
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

void Str::ReplaceText( string& str, const string& from, const string& to )
{
    size_t start_pos = 0;
    while( ( start_pos = str.find( from, start_pos ) ) != std::string::npos )
    {
        str.replace( start_pos, from.length(), to );
        start_pos += to.length();
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

void Str::Replacement( string& str, char from, char to )
{
    std::replace( str.begin(), str.end(), from, to );
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

void Str::Replacement( string& str, char from1, char from2, char to )
{
    ReplaceText( str, string( { from1, from2 } ), string( { to } ) );
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

const char* Str::BtoA( bool value )
{
    static THREAD char str[ 128 ];
    sprintf( str, "%s", value ? "true" : "false" );
    return str;
}

const char* Str::ItoA( int value )
{
    static THREAD char str[ 128 ];
    sprintf( str, "%d", value );
    return str;
}

const char* Str::UItoA( uint value )
{
    static THREAD char str[ 128 ];
    sprintf( str, "%u", value );
    return str;
}

const char* Str::I64toA( int64 value )
{
    static THREAD char str[ 128 ];
    sprintf( str, "%lld", value );
    return str;
}

const char* Str::UI64toA( uint64 value )
{
    static THREAD char str[ 128 ];
    sprintf( str, "%llu", value );
    return str;
}

const char* Str::FtoA( float value )
{
    static THREAD char str[ 128 ];
    sprintf( str, "%f", value );
    return str;
}

const char* Str::DFtoA( double value )
{
    static THREAD char str[ 128 ];
    sprintf( str, "%lf", value );
    return str;
}

bool Str::AtoB( const char* str )
{
    if( Str::CompareCase( str, "True" ) )
        return true;
    if( Str::CompareCase( str, "False" ) )
        return false;
    return Str::AtoI( str ) != 0;
}

int Str::AtoI( const char* str )
{
    return (int) AtoI64( str );
}

uint Str::AtoUI( const char* str )
{
    return (uint) AtoI64( str );
}

int64 Str::AtoI64( const char* str )
{
    if( str[ 0 ] && str[ 0 ] == '0' && ( str[ 1 ] == 'x' || str[ 1 ] == 'X' ) )
        return strtoll( str + 2, nullptr, 16 );
    return strtoll( str, nullptr, 10 );
}

uint64 Str::AtoUI64( const char* str )
{
    if( str[ 0 ] && str[ 0 ] == '0' && ( str[ 1 ] == 'x' || str[ 1 ] == 'X' ) )
        return strtoull( str + 2, nullptr, 16 );
    return strtoull( str, nullptr, 10 );
}

float Str::AtoF( const char* str )
{
    return (float) atof( str );
}

double Str::AtoDF( const char* str )
{
    return atof( str );
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

#ifndef NO_THREADING
static Mutex                    HashNamesLocker;
#endif
static map< hash, const char* > HashNames;

#define HASH_IMPL( var, name )    hash var = Str::GetHash( name )
HASH_IMPL( SP_SCEN_IBLOCK, "MinimapInvisibleBlock" );
HASH_IMPL( SP_SCEN_TRIGGER, "Trigger" );
HASH_IMPL( SP_WALL_BLOCK_LIGHT, "BlockLight" );
HASH_IMPL( SP_WALL_BLOCK, "Block" );
HASH_IMPL( SP_GRID_EXITGRID, "ExitGrid" );
HASH_IMPL( SP_GRID_ENTIRE, "Entrance" );
HASH_IMPL( SP_MISC_SCRBLOCK, "ScrollBlock" );

hash Str::GetHash( const string& name )
{
    if( name.empty() )
        return 0;

    string fixed_name = _str( name ).normalizePathSlashes().trim();
    if( fixed_name.empty() )
        return 0;

    // Calculate hash
    hash h = Crypt.MurmurHash2( (const uchar*) fixed_name.c_str(), (uint) fixed_name.length() );
    if( !h )
        return 0;

    // Add hash
    #ifndef NO_THREADING
    SCOPE_LOCK( HashNamesLocker );
    #endif

    auto ins = HashNames.insert( PAIR( h, (const char*) nullptr ) );
    if( ins.second )
        ins.first->second = Str::Duplicate( fixed_name.c_str() );
    else if( !Str::Compare( ins.first->second, fixed_name ) )
        WriteLog( "Hash collision detected for names '{}' and '{}', hash {:#X}.\n", fixed_name, ins.first->second, h );

    return h;
}

const char* Str::GetName( hash h )
{
    #ifndef NO_THREADING
    SCOPE_LOCK( HashNamesLocker );
    #endif

    auto it = HashNames.find( h );
    if( !h || it == HashNames.end() )
        return nullptr;
    return it->second;
}

void Str::SaveHashes( StrMap& hashes )
{
    #ifndef NO_THREADING
    SCOPE_LOCK( HashNamesLocker );
    #endif

    for( auto& kv : HashNames )
        hashes[ UItoA( kv.first ) ] = kv.second;
}

void Str::LoadHashes( StrMap& hashes )
{
    #ifndef NO_THREADING
    SCOPE_LOCK( HashNamesLocker );
    #endif

    for( auto& kv : hashes )
        GetHash( kv.second );
}

StrVec Str::Split( const string& line, char divider )
{
    StrVec            result;
    std::stringstream ss( line );
    string            item;
    while( std::getline( ss, item, divider ) )
    {
        item = _str( item ).trim();
        if( !item.empty() )
            result.push_back( std::move( item ) );
    }
    return result;
}

IntVec Str::SplitToInt( const string& line, char divider )
{
    IntVec            result;
    std::stringstream ss( line );
    string            item;
    while( std::getline( ss, item, divider ) )
    {
        item = _str( item ).trim();
        if( !item.empty() )
            result.push_back( AtoI( item.c_str() ) );
    }
    return result;
}
