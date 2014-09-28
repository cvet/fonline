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
    void        Append( char* to, uint size, const char* from );
    template< int Size >
    inline void Append( char(&to)[ Size ], const char* from ) { return Append( to, Size, from ); }

    char* Duplicate( const char* str );

    void Lower( char* str );
    uint LowerUTF8( uint ucs );
    void LowerUTF8( char* str );
    void Upper( char* str );
    uint UpperUTF8( uint ucs );
    void UpperUTF8( char* str );

    char*       Substring( char* str, const char* sub_str );
    const char* Substring( const char* str, const char* sub_str );

    bool IsValidUTF8( uint ucs );
    bool IsValidUTF8( const char* str );
    uint DecodeUTF8( const char* str, uint* length );
    uint EncodeUTF8( uint ucs, char* buf );

    uint Length( const char* str );
    uint LengthUTF8( const char* str );
    bool Compare( const char* str1, const char* str2 );
    bool CompareCase( const char* str1, const char* str2 );
    bool CompareCaseUTF8( const char* str1, const char* str2 );
    bool CompareCount( const char* str1, const char* str2, uint max_count );
    bool CompareCaseCount( const char* str1, const char* str2, uint max_count );
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
    void  Replacement( char* str, char from, char to );
    void  Replacement( char* str, char from1, char from2, char to );
    char* EraseFrontBackSpecificChars( char* str );

    void SkipLine( char*& str );
    void GoTo( char*& str, char ch, bool skip_char = false );

    bool        IsNumber( const char* str );
    const char* ItoA( int i );
    const char* I64toA( int64 i );
    const char* UItoA( uint dw );
    int         AtoI( const char* str );
    int64       AtoI64( const char* str );
    uint        AtoUI( const char* str );

    void  HexToStr( uchar hex, char* str ); // 2 bytes string
    uchar StrToHex( const char* str );

    char* GetBigBuf();                      // Just big buffer, 1mb

    // Name hashes
    uint        FormatForHash( char* name );
    uint        GetHash( const char* name );
    const char* GetName( uint hash );
    void        AddNameHash( const char* name );

    // Parse str
    const char* ParseLineDummy( const char* str );
    template< typename Cont, class Func >
    void ParseLine( const char* str, char divider, Cont& result, Func f )
    {
        result.clear();
        char buf[ MAX_FOTEXT ];
        for( uint buf_pos = 0; ; str++ )
        {
            if( *str == divider || *str == 0 || buf_pos >= sizeof( buf ) - 1 )
            {
                if( buf_pos )
                {
                    buf[ buf_pos ] = 0;
                    EraseFrontBackSpecificChars( buf );
                    if( buf[ 0 ] )
                        result.push_back( typename Cont::value_type( f( buf ) ) );
                    buf_pos = 0;
                }

                if( *str == 0 )
                    break;
            }
            else
            {
                buf[ buf_pos ] = *str;
                buf_pos++;
            }
        }
    }
}

#endif // ___TEXT___
