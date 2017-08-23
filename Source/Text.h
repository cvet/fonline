#ifndef ___TEXT___
#define ___TEXT___

#include "Common.h"
#include <strstream>

#define MAX_FOTEXT      UTF8_BUF_SIZE( 2048 )
#define BIG_BUF_SIZE    ( 0x100000 )       // 1mb

namespace Str
{
    void Copy( char* to, uint size, const char* from );
    template< int Size >
    inline void Copy( char(&to)[ Size ], const char* from ) { return Copy( to, Size, from ); }
    template< int Size >
    inline void CopyCount( char(&to)[ Size ], const char* from, uint count ) { return Copy( to, Size < count + 1 ? Size : count + 1, from ); }
    void        Append( char* to, uint size, const char* from );
    template< int Size >
    inline void Append( char(&to)[ Size ], const char* from ) { return Append( to, Size, from ); }
    template< int Size >
    inline void AppendCount( char(&to)[ Size ], const char* from, uint count ) { return Append( to, Size < count + 1 ? Size : count + 1, from ); }

    char* Duplicate( const char* str );

    uint LowerUTF8( uint ucs );
    void LowerUTF8( char* str );
    uint UpperUTF8( uint ucs );
    void UpperUTF8( char* str );

    char*       Substring( char* str, const char* sub_str );
    const char* Substring( const char* str, const char* sub_str );
    const char* Substring( const string& str, const char* sub_str );
    char*       LastSubstring( char* str, const char* sub_str );
    const char* LastSubstring( const char* str, const char* sub_str );

    bool IsValidUTF8( uint ucs );
    bool IsValidUTF8( const char* str );
    bool IsValidUTF8( const string& str );
    uint DecodeUTF8( const char* str, uint* length );
    uint DecodeUTF8( const string& str, uint* length );
    uint EncodeUTF8( uint ucs, char* buf );

    uint Length( const char* str );
    uint LengthUTF8( const char* str );
    bool Compare( const char* str1, const char* str2 );
    bool Compare( const string& str1, const string& str2 );
    bool CompareCase( const char* str1, const char* str2 );
    bool CompareCase( const string& str1, const string& str2 );
    bool CompareCaseUTF8( const char* str1, const char* str2 );
    bool CompareCount( const char* str1, const char* str2, uint max_count );
    bool CompareCount( const string& str1, const string& str2, uint max_count );
    bool CompareCaseCount( const char* str1, const char* str2, uint max_count );
    bool CompareCaseCount( const string& str1, const string& str2, uint max_count );
    bool CompareCaseCountUTF8( const char* str1, const char* str2, uint max_count );

    void ChangeValue( char* str, int value );
    void EraseInterval( char* str, uint len );
    void Insert( char* to, const char* from, uint from_len = 0 );
    void EraseWords( char* str, char begin, char end );
    void EraseWords( char* str, const char* word );
    void EraseChars( char* str, char ch );
    void CopyWord( char* to, const char* from, char end, bool include_end = false );
    void CopyBack( char* str );
    void ReplaceText( char* str, const char* from, const char* to );
    void ReplaceText( string& str, const string& from, const string& to );
    void Replacement( char* str, char from, char to );
    void Replacement( string& str, char from, char to );
    void Replacement( char* str, char from1, char from2, char to );
    void Replacement( string& str, char from1, char from2, char to );

    void SkipLine( char*& str );
    void GoTo( char*& str, char ch, bool skip_char = false );

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

    bool compareIgnoreCase( const string& r );

    _str& trim();
    _str& replace( char from, char to );
    _str& replace( char from1, char from2, char to );
    _str& replace( const string& from, const string& to );
    _str& lower();
    _str& upper();

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
    _str& extractFileName();
    _str& getFileExtension();   // Extension without dot
    _str& eraseFileExtension(); // Erase extension with dot
    _str& combinePath( const string& path );
    _str& forwardPath( const string& relative_dir );
    _str& resolvePath();
    _str& normalizePathSlashes();

    #ifdef FO_WINDOWS
    _str&        parseWideChar( const wchar_t* str );
    std::wstring toWideChar();
    #endif

    hash        toHash();
    _str&       parseHash( hash h );
    static void saveHashes( StrMap& hashes );
    static void loadHashes( StrMap& hashes );
};

namespace fmt
{
    template< typename ArgFormatter >
    void format_arg( BasicFormatter< char, ArgFormatter >& f, const char*& format_str, const _str& s )
    {
        f.writer().write( "{}", s.str() );
    }
}

#endif // ___TEXT___
