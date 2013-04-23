#include "StdAfx.h"
#include <strstream>

static uchar  KeysMap[ 0x10000 ] = { 0 };
static ushort KeysMapRevert[ 0x100 ] = { 0 };
static uchar  KeysMapUser[ 0x100 ] = { 0 };
#define MAKE_KEY_CODE( name, index, code ) \
    const uchar name = index;              \
    struct name ## _INIT { name ## _INIT() { KeysMap[ code ] = index; KeysMapRevert[ index ] = code; } } name ## _INIT_;

#include "Keyboard.h"
#include "SpriteManager.h"

namespace Keyb
{
    bool ShiftDwn = false;
    bool CtrlDwn = false;
    bool AltDwn = false;
    bool KeyPressed[ 0x100 ] = { 0 };

    void GetCharInternal( uchar dik, const char* dik_text, char* str, uint* position, uint max, int flags );
    bool IsInvalidChar( const char* str, uint flags );
}

void Keyb::InitKeyb()
{
    // User keys mapping
    for( uint i = 0; i < 0x100; i++ )
        KeysMapUser[ i ] = i;
    istrstream str( GameOpt.KeyboardRemap.c_str() );
    while( !str.eof() )
    {
        int from, to;
        str >> from >> to;
        if( str.fail() )
            break;
        from &= 0xFF;
        to &= 0xFF;
        KeysMapUser[ from ] = to;
    }
}

void Keyb::Finish()
{}

void Keyb::Lost()
{
    CtrlDwn = false;
    AltDwn = false;
    ShiftDwn = false;
    memzero( KeyPressed, sizeof( KeyPressed ) );
}

void Keyb::GetChar( uchar dik, const char* dik_text, string& str, uint* position, uint max, int flags )
{
    char* big_buf = Str::GetBigBuf();
    Str::Copy( big_buf, BIG_BUF_SIZE, str.c_str() );
    GetCharInternal( dik, dik_text, big_buf, position, max, flags );
    str = big_buf;
}

void Keyb::GetChar( uchar dik, const char* dik_text, char* str, uint str_size, uint* position, uint max, int flags )
{
    string str_ = str;
    GetChar( dik, dik_text, str_, position, max, flags );
    Str::Copy( str, str_size, str_.c_str() );
}

void Keyb::GetCharInternal( uchar dik, const char* dik_text, char* str, uint* position, uint max, int flags )
{
    if( AltDwn )
        return;

    bool  ctrl_shift = ( CtrlDwn || ShiftDwn );

    uint  dik_text_len_utf8 = Str::LengthUTF8( dik_text );
    uint  dik_text_len = Str::Length( dik_text );
    uint  str_len_utf8 = Str::LengthUTF8( str );
    uint  str_len = Str::Length( str );

    uint  position_ = str_len;
    uint& pos = ( position ? *position : position_ );
    if( pos > str_len )
        pos = str_len;

    // Controls
    if( dik == DIK_RIGHT && !ctrl_shift )
    {
        if( pos < str_len )
        {
            pos++;
            while( pos < str_len && ( str[ pos ] & 0xC0 ) == 0x80 )
                pos++;
        }
    }
    else if( dik == DIK_LEFT && !ctrl_shift )
    {
        if( pos > 0 )
        {
            pos--;
            while( pos && ( str[ pos ] & 0xC0 ) == 0x80 )
                pos--;
        }
    }
    else if( dik == DIK_BACK && !ctrl_shift )
    {
        if( pos > 0 )
        {
            uint letter_len = 1;
            pos--;
            while( pos && ( str[ pos ] & 0xC0 ) == 0x80 )
                pos--, letter_len++;

            for( uint i = pos; str[ i + letter_len ]; i++ )
                str[ i ] = str[ i + letter_len ];
            str[ str_len - letter_len ] = '\0';
        }
    }
    else if( dik == DIK_DELETE && !ctrl_shift )
    {
        if( pos < str_len )
        {
            uint letter_len = 1;
            uint pos_ = pos + 1;
            while( pos_ < str_len && ( str[ pos_ ] & 0xC0 ) == 0x80 )
                pos_++, letter_len++;

            for( uint i = pos; str[ i + letter_len ]; i++ )
                str[ i ] = str[ i + letter_len ];
            str[ str_len - letter_len ] = '\0';
        }
    }
    else if( dik == DIK_HOME && !ctrl_shift )
    {
        pos = 0;
    }
    else if( dik == DIK_END && !ctrl_shift )
    {
        pos = str_len;
    }
    // Clipboard
    else if( CtrlDwn && !ShiftDwn && str_len > 0 && ( dik == DIK_C || dik == DIK_X ) )
    {
        Fl::copy( str, Str::Length( str ), 1 );
        if( dik == DIK_X )
        {
            *str = '\0';
            pos = 0;
        }
    }
    else if( CtrlDwn && !ShiftDwn && dik == DIK_V )
    {
        Fl::paste( *MainWindow, 1 );
    }
    else if( dik == DIK_CLIPBOARD_PASTE )
    {
        char* text = Str::Duplicate( dik_text );
        EraseInvalidChars( text, flags );
        uint  text_len = Str::Length( text );
        if( text_len != 0 )
        {
            uint text_len_utf8 = Str::LengthUTF8( text );
            uint erase_len_utf8 = 0;
            if( str_len_utf8 + text_len_utf8 > max )
                erase_len_utf8 = str_len_utf8 + text_len_utf8 - max;

            uint text_pos = text_len;
            while( erase_len_utf8 )
            {
                text_pos--;
                while( text_pos && ( text[ text_pos ] & 0xC0 ) == 0x80 )
                    text_pos--;
                erase_len_utf8--;
            }
            text[ text_pos ] = '\0';

            Str::Insert( &str[ pos ], text );
            pos += Str::Length( text );
        }
        delete[] text;
    }
    // Text input
    else
    {
        if( dik_text_len_utf8 == 0 )
            return;
        if( str_len_utf8 + dik_text_len_utf8 > max )
            return;
        if( CtrlDwn )
            return;

        if( IsInvalidChar( dik_text, flags ) )
            return;

        Str::Insert( &str[ pos ], dik_text );
        pos += dik_text_len;
    }
}

void Keyb::EraseInvalidChars( char* str, int flags )
{
    while( *str )
    {
        uint length;
        int  ch = Str::DecodeUTF8( str, &length );
        if( ch < 0 || IsInvalidChar( str, flags ) )
            Str::EraseInterval( str, length );
        str += length;
    }
}

bool Keyb::IsInvalidChar( const char* str, uint flags )
{
    if( !Str::IsValidUTF8( str ) )
        return false;
    if( flags & KIF_NO_SPEC_SYMBOLS && ( *str == '\n' || *str == '\r' || *str == '\t' ) )
        return true;
    if( flags & KIF_ONLY_NUMBERS && !( *str >= '0' && *str <= '9' ) )
        return true;
    if( flags & KIF_FILE_NAME )
    {
        switch( *str )
        {
        case '\\':
        case '/':
        case ':':
        case '*':
        case '?':
        case '"':
        case '<':
        case '>':
        case '|':
        case '\n':
        case '\r':
        case '\t':
            return true;
        default:
            break;
        }
    }
    return !SprMngr.HaveLetter( -1, str );
}

uchar Keyb::MapKey( ushort code )
{
    return KeysMapUser[ KeysMap[ code ] ];
}

ushort Keyb::UnmapKey( uchar key )
{
    return KeysMapRevert[ key ];
}
