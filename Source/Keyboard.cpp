#include "Common.h"
#include <strstream>

static uchar  KeysMap[ 0x200 ] = { 0 };
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

    bool IsInvalidChar( const string& str, uint flags );
}

void Keyb::Init()
{
    // User keys mapping
    for( uint i = 0; i < 0x100; i++ )
        KeysMapUser[ i ] = i;

    istringstream str( GameOpt.KeyboardRemap );
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

void Keyb::Lost()
{
    CtrlDwn = false;
    AltDwn = false;
    ShiftDwn = false;
}

void Keyb::GetChar( uchar dik, const string& dik_text, string& str, uint* position, uint max, int flags )
{
    if( AltDwn )
        return;

    bool  ctrl_shift = ( CtrlDwn || ShiftDwn );

    uint  dik_text_len_utf8 = Str::LengthUTF8( dik_text.c_str() );
    uint  dik_text_len = Str::Length( dik_text.c_str() );
    uint  str_len_utf8 = Str::LengthUTF8( str.c_str() );
    uint  str_len = Str::Length( str.c_str() );

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
        SDL_SetClipboardText( str.c_str() );
        if( dik == DIK_X )
        {
            str.clear();
            pos = 0;
        }
    }
    else if( CtrlDwn && !ShiftDwn && dik == DIK_V )
    {
        const char* cb_text = SDL_GetClipboardText();
        MainWindowKeyboardEvents.push_back( SDL_KEYDOWN );
        MainWindowKeyboardEvents.push_back( 511 );
        MainWindowKeyboardEventsText.push_back( cb_text );
        MainWindowKeyboardEvents.push_back( SDL_KEYUP );
        MainWindowKeyboardEvents.push_back( 511 );
        MainWindowKeyboardEventsText.push_back( cb_text );
    }
    else if( dik == DIK_CLIPBOARD_PASTE )
    {
        string text = dik_text;
        EraseInvalidChars( text, flags );
        if( !text.empty() )
        {
            uint text_len_utf8 = Str::LengthUTF8( text.c_str() );
            uint erase_len_utf8 = 0;
            if( str_len_utf8 + text_len_utf8 > max )
                erase_len_utf8 = str_len_utf8 + text_len_utf8 - max;

            uint text_pos = (uint) text.length();
            while( erase_len_utf8 )
            {
                text_pos--;
                while( text_pos && ( text[ text_pos ] & 0xC0 ) == 0x80 )
                    text_pos--;
                erase_len_utf8--;
            }
            text[ text_pos ] = '\0';

            str.insert( pos, text );
            pos += (uint) text.length();
        }
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

        str.insert( pos, dik_text );
        pos += dik_text_len;
    }
}

void Keyb::EraseInvalidChars( string& str, int flags )
{
    for( size_t i = 0; i < str.length();)
    {
        uint length;
        int  ch = Str::DecodeUTF8( str.substr( i ), &length );
        if( ch < 0 || IsInvalidChar( str.substr( i ), flags ) )
            str.erase( i, length );
        else
            i += length;
    }
}

bool Keyb::IsInvalidChar( const string& str, uint flags )
{
    if( !Str::IsValidUTF8( str ) )
        return false;
    if( flags & KIF_NO_SPEC_SYMBOLS && str.find_first_of( "\n\r\t" ) != string::npos )
        return true;
    if( flags & KIF_ONLY_NUMBERS && str.find_first_not_of( "0123456789" ) != string::npos )
        return true;
    if( flags & KIF_FILE_NAME )
    {
        for( char ch : str )
        {
            switch( ch )
            {
            case '\\' :
            case '/'  :
            case ':'  :
            case '*'  :
            case '?'  :
            case '"'  :
            case '<'  :
            case '>'  :
            case '|'  :
            case '\n' :
            case '\r' :
            case '\t' :
                return true;
            default:
                break;
            }
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
