#pragma once

#include "Common.h"

#define MAX_FOTEXT      UTF8_BUF_SIZE( 2048 )
#define BIG_BUF_SIZE    ( 0x100000 )       // 1mb

namespace Str
{
    void Copy( char* to, size_t size, const char* from );
    template< int Size >
    inline void Copy( char(&to)[ Size ], const char* from ) { return Copy( to, Size, from ); }
    void        Append( char* to, size_t size, const char* from );
    template< int Size >
    inline void Append( char(&to)[ Size ], const char* from ) { return Append( to, Size, from ); }

    char* Duplicate( const string& str );
    char* Duplicate( const char* str );

    bool Compare( const char* str1, const char* str2 );

    void  HexToStr( uchar hex, char* str ); // 2 bytes string
    uchar StrToHex( const char* str );
}

class _str
{
    string s;

public:
    _str() {}
    _str( const _str& r ): s( r.s ) {}
    _str( const string& s ): s( s ) {}
    _str( const char* s ): s( s ) {}
    template< typename ... Args > _str( const string& format, Args ... args ): s( fmt::format( format, args ... ) ) {}
    operator string&() { return s; }
    _str          operator+( const char* r ) const             { return _str( s + string( r ) ); }
    friend _str   operator+( const _str& l, const string& r )  { return _str( l.s + r ); }
    friend bool   operator==( const _str& l, const string& r ) { return l.s == r; }
    bool          operator!=( const _str& r ) const            { return s != r.s; }
    friend bool   operator!=( const _str& l, const string& r ) { return l.s != r; }
    const char*   c_str() const                                { return s.c_str(); }
    const string& str() const                                  { return s; }

    uint length();
    bool empty();
    bool compareIgnoreCase( const string& r );
    bool compareIgnoreCaseUtf8( const string& r );
    bool startsWith( char r );
    bool startsWith( const string& r );
    bool endsWith( char r );
    bool endsWith( const string& r );
    bool isValidUtf8();
    uint lengthUtf8();

    _str& substringUntil( char separator );
    _str& substringUntil( string separator );
    _str& substringAfter( char separator );
    _str& substringAfter( string separator );
    _str& trim();
    _str& erase( char what );
    _str& erase( char begin, char end );
    _str& replace( char from, char to );
    _str& replace( char from1, char from2, char to );
    _str& replace( const string& from, const string& to );
    _str& lower();
    _str& lowerUtf8();
    _str& upper();
    _str& upperUtf8();

    StrVec split( char divider );
    IntVec splitToInt( char divider );

    bool   isNumber();
    int    toInt();
    uint   toUInt();
    int64  toInt64();
    uint64 toUInt64();
    float  toFloat();
    double toDouble();
    bool   toBool();

    _str& formatPath();
    _str& extractDir();
    _str& extractLastDir();
    _str& extractFileName();
    _str& getFileExtension();   // Extension without dot
    _str& eraseFileExtension(); // Erase extension with dot
    _str& combinePath( const string& path );
    _str& forwardPath( const string& relative_dir );
    _str& resolvePath();
    _str& normalizePathSlashes();
    _str& normalizeLineEndings();

    #ifdef FO_WINDOWS
    _str&        parseWideChar( const wchar_t* str );
    std::wstring toWideChar();
    #endif

    hash  toHash();
    _str& parseHash( hash h );

    #if defined ( FONLINE_SERVER ) || defined ( FONLINE_EDITOR )
    static void loadHashes();
    #endif
};

namespace utf8
{
    bool IsValid( uint ucs );
    uint Decode( const char* str, uint* length );
    uint Encode( uint ucs, char(&buf)[ 4 ] );
    uint Lower( uint ucs );
    uint Upper( uint ucs );
}

namespace fmt
{
    template< >
    struct formatter< _str >
    {
        template< typename ParseContext >
        constexpr auto parse( ParseContext& ctx )
        {
            return ctx.begin();
        }

        template< typename FormatContext >
        auto format( const _str &s, FormatContext & ctx )
        {
            return format_to( ctx.out(), "{}", s.str() );
        }
    };
}
