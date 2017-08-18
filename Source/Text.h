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

    string Lower( const string& str );
    string Upper( const string& str );

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
    uint DecodeUTF8( const char* str, uint* length );
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

    char*       Format( char* buf, const char* format, ... );
    const char* FormatBuf( const char* format, ... );

    void  ChangeValue( char* str, int value );
    void  EraseInterval( char* str, uint len );
    void  Insert( char* to, const char* from, uint from_len = 0 );
    void  EraseWords( char* str, char begin, char end );
    void  EraseWords( char* str, const char* word );
    void  EraseChars( char* str, char ch );
    void  CopyWord( char* to, const char* from, char end, bool include_end = false );
    void  CopyBack( char* str );
    void  ReplaceText( char* str, const char* from, const char* to );
    void  Replacement( char* str, char from, char to );
    void  Replacement( char* str, char from1, char from2, char to );
    char* Trim( char* str, uint* trimmed = nullptr );
    void  Trim( string& str, uint* trimmed = nullptr );

    void SkipLine( char*& str );
    void GoTo( char*& str, char ch, bool skip_char = false );

    bool        IsNumber( const char* str );
    const char* BtoA( bool value );
    const char* ItoA( int value );
    const char* UItoA( uint value );
    const char* I64toA( int64 value );
    const char* UI64toA( uint64 value );
    const char* FtoA( float value );
    const char* DFtoA( double value );
    bool        AtoB( const char* str );
    int         AtoI( const char* str );
    uint        AtoUI( const char* str );
    int64       AtoI64( const char* str );
    uint64      AtoUI64( const char* str );
    float       AtoF( const char* str );
    double      AtoDF( const char* str );

    void  HexToStr( uchar hex, char* str ); // 2 bytes string
    uchar StrToHex( const char* str );

    char* GetBigBuf();                      // Just big buffer, 1mb

    // Name hashes
    hash        GetHash( const string& name );
    const char* GetName( hash h );
    void        SaveHashes( StrMap& hashes );
    void        LoadHashes( StrMap& hashes );

    // Split exclude empty entries
    StrVec Split( const string& line, char divider );
    IntVec SplitToInt( const string& line, char divider );
}

class _str // Todo: do it
{
    string s;

public:
    _str( const string& s ): s( s ) {}
    operator string&() { return s; }
    _str        operator+( const char* r )                  { return _str( s + string( r ) ); }
    _str        operator+( const string& r )                { return _str( s + r ); }
    friend _str operator+( const _str& l, const string& r ) { return _str( l.s + r ); }

    _str& lower()
    {
        s = Str::Lower( s );
        return *this;
    }

    _str& upper()
    {
        s = Str::Upper( s );
        return *this;
    }
};

#endif // ___TEXT___
