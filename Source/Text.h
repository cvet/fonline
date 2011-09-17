#ifndef ___TEXT___
#define ___TEXT___

#include "Common.h"
#include <strstream>

#define MAX_FOTEXT      ( 2048 )
#define BIG_BUF_SIZE    ( 0x100000 )       // 1mb

namespace Str
{
    void Copy( char* to, size_t size, const char* from );
    template< int Size >
    inline void Copy( char(&to)[ Size ], const char* from ) { return Copy( to, Size, from ); }
    void        Append( char* to, size_t size, const char* from );
    template< int Size >
    inline void Append( char(&to)[ Size ], const char* from ) { return Append( to, Size, from ); }

    char* Duplicate( const char* str );
    char* Lower( char* str );
    char* Upper( char* str );

    char*       Substring( char* str, const char* sub_str );
    const char* Substring( const char* str, const char* sub_str );

    uint Length( const char* str );
    bool Compare( const char* str1, const char* str2 );
    bool CompareCase( const char* str1, const char* str2 );
    bool CompareCount( const char* str1, const char* str2, uint max_count );
    bool CompareCaseCount( const char* str1, const char* str2, uint max_count );

    char*       Format( char* buf, const char* format, ... );
    const char* FormatBuf( const char* format, ... );

    void  ChangeValue( char* str, int value );
    void  EraseInterval( char* str, int len );
    void  Insert( char* to, const char* from );
    void  EraseWords( char* str, char begin, char end );
    void  EraseChars( char* str, char ch );
    void  CopyWord( char* to, const char* from, char end, bool include_end = false );
    void  CopyBack( char* str );
    void  Replacement( char* str, char ch );
    void  Replacement( char* str, char from, char to );
    void  Replacement( char* str, char from1, char from2, char to );
    char* EraseFrontBackSpecificChars( char* str );

    void SkipLine( char*& str );
    void GoTo( char*& str, char ch, bool skip_char = false );

    bool        IsNumber( const char* str );
    const char* ItoA( int i );
    const char* UItoA( uint dw );
    int         AtoI( const char* str );
    uint        AtoUI( const char* str );

    char* GetBigBuf();     // Just big buffer, 1mb

    // Name hashes
    uint        GetHash( const char* str );
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
