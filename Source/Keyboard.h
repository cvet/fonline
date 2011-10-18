#ifndef __KEYBOARD__
#define __KEYBOARD__

// Languages
#define LANG_RUS               ( 0 )
#define LANG_ENG               ( 1 )

// Keyboard input flags
#define KIF_NO_SPEC_SYMBOLS    ( 1 )  // Ignore \n \r \t
#define KIF_ONLY_NUMBERS       ( 2 )  // Only 0..9
#define KIF_FILE_NAME          ( 4 )  // Ignore \/:*?\"<>|

namespace Keyb
{
    extern int  Lang;
    extern bool ShiftDwn;
    extern bool CtrlDwn;
    extern bool AltDwn;
    extern bool KeyPressed[ 0x100 ];

    void  InitKeyb();
    void  Finish();
    void  Lost();
    void  GetChar( uchar dik, string& str, int* position, int max, int flags );
    void  GetChar( uchar dik, char* str, int* position, int max, int flags );
    void  PuntoSwitch( char* str );
    void  EraseInvalidChars( char* str, int flags );
    bool  IsInvalidSymbol( char c, uint flags );
    uchar MapKey( ushort code );
}

// Key codes
#ifndef MAKE_KEY_CODE
# define MAKE_KEY_CODE( name, index, code )    const uchar name = index
#endif
MAKE_KEY_CODE( DIK_ESCAPE, 0x01, 0xFF1B );
MAKE_KEY_CODE( DIK_1, 0x02, '1' );
MAKE_KEY_CODE( DIK_2, 0x03, '2' );
MAKE_KEY_CODE( DIK_3, 0x04, '3' );
MAKE_KEY_CODE( DIK_4, 0x05, '4' );
MAKE_KEY_CODE( DIK_5, 0x06, '5' );
MAKE_KEY_CODE( DIK_6, 0x07, '6' );
MAKE_KEY_CODE( DIK_7, 0x08, '7' );
MAKE_KEY_CODE( DIK_8, 0x09, '8' );
MAKE_KEY_CODE( DIK_9, 0x0A, '9' );
MAKE_KEY_CODE( DIK_0, 0x0B, '0' );
MAKE_KEY_CODE( DIK_MINUS, 0x0C, '-' );              /* - on main keyboard */
MAKE_KEY_CODE( DIK_EQUALS, 0x0D, '=' );
MAKE_KEY_CODE( DIK_BACK, 0x0E, 0xFF08 );            /* backspace */
MAKE_KEY_CODE( DIK_TAB, 0x0F, 0xFF09 );
MAKE_KEY_CODE( DIK_Q, 0x10, 'q' );
MAKE_KEY_CODE( DIK_W, 0x11, 'w' );
MAKE_KEY_CODE( DIK_E, 0x12, 'e' );
MAKE_KEY_CODE( DIK_R, 0x13, 'r' );
MAKE_KEY_CODE( DIK_T, 0x14, 't' );
MAKE_KEY_CODE( DIK_Y, 0x15, 'y' );
MAKE_KEY_CODE( DIK_U, 0x16, 'u' );
MAKE_KEY_CODE( DIK_I, 0x17, 'i' );
MAKE_KEY_CODE( DIK_O, 0x18, 'o' );
MAKE_KEY_CODE( DIK_P, 0x19, 'p' );
MAKE_KEY_CODE( DIK_LBRACKET, 0x1A, '[' );
MAKE_KEY_CODE( DIK_RBRACKET, 0x1B, ']' );
MAKE_KEY_CODE( DIK_RETURN, 0x1C, 0xFF0D );             /* Enter on main keyboard */
MAKE_KEY_CODE( DIK_LCONTROL, 0x1D, 0xFFE3 );
MAKE_KEY_CODE( DIK_A, 0x1E, 'a' );
MAKE_KEY_CODE( DIK_S, 0x1F, 's' );
MAKE_KEY_CODE( DIK_D, 0x20, 'd' );
MAKE_KEY_CODE( DIK_F, 0x21, 'f' );
MAKE_KEY_CODE( DIK_G, 0x22, 'g' );
MAKE_KEY_CODE( DIK_H, 0x23, 'h' );
MAKE_KEY_CODE( DIK_J, 0x24, 'j' );
MAKE_KEY_CODE( DIK_K, 0x25, 'k' );
MAKE_KEY_CODE( DIK_L, 0x26, 'l' );
MAKE_KEY_CODE( DIK_SEMICOLON, 0x27, ';' );
MAKE_KEY_CODE( DIK_APOSTROPHE, 0x28, '\'' );
MAKE_KEY_CODE( DIK_GRAVE, 0x29, '`' );              /* accent grave */
MAKE_KEY_CODE( DIK_LSHIFT, 0x2A, 0xFFE1 );
MAKE_KEY_CODE( DIK_BACKSLASH, 0x2B, '\\' );
MAKE_KEY_CODE( DIK_Z, 0x2C, 'z' );
MAKE_KEY_CODE( DIK_X, 0x2D, 'x' );
MAKE_KEY_CODE( DIK_C, 0x2E, 'c' );
MAKE_KEY_CODE( DIK_V, 0x2F, 'v' );
MAKE_KEY_CODE( DIK_B, 0x30, 'b' );
MAKE_KEY_CODE( DIK_N, 0x31, 'n' );
MAKE_KEY_CODE( DIK_M, 0x32, 'm' );
MAKE_KEY_CODE( DIK_COMMA, 0x33, ',' );
MAKE_KEY_CODE( DIK_PERIOD, 0x34, '.' );             /* . on main keyboard */
MAKE_KEY_CODE( DIK_SLASH, 0x35, '/' );              /* / on main keyboard */
MAKE_KEY_CODE( DIK_RSHIFT, 0x36, 0xFFE2 );
MAKE_KEY_CODE( DIK_MULTIPLY, 0x37, 0xFF80 + '*' );  /* * on numeric keypad */
MAKE_KEY_CODE( DIK_LMENU, 0x38, 0xFFE9 );           /* left Alt */
MAKE_KEY_CODE( DIK_SPACE, 0x39, ' ' );
MAKE_KEY_CODE( DIK_CAPITAL, 0x3A, 0xFFE5 );
MAKE_KEY_CODE( DIK_F1, 0x3B, 0xFFBD + 1 );
MAKE_KEY_CODE( DIK_F2, 0x3C, 0xFFBD + 2 );
MAKE_KEY_CODE( DIK_F3, 0x3D, 0xFFBD + 3 );
MAKE_KEY_CODE( DIK_F4, 0x3E, 0xFFBD + 4 );
MAKE_KEY_CODE( DIK_F5, 0x3F, 0xFFBD + 5 );
MAKE_KEY_CODE( DIK_F6, 0x40, 0xFFBD + 6 );
MAKE_KEY_CODE( DIK_F7, 0x41, 0xFFBD + 7 );
MAKE_KEY_CODE( DIK_F8, 0x42, 0xFFBD + 8 );
MAKE_KEY_CODE( DIK_F9, 0x43, 0xFFBD + 9 );
MAKE_KEY_CODE( DIK_F10, 0x44, 0xFFBD + 10 );
MAKE_KEY_CODE( DIK_NUMLOCK, 0x45, 0xFF7F );
MAKE_KEY_CODE( DIK_SCROLL, 0x46, 0xFF14 );             /* Scroll Lock */
MAKE_KEY_CODE( DIK_NUMPAD7, 0x47, 0xFF80 + '7' );
MAKE_KEY_CODE( DIK_NUMPAD8, 0x48, 0xFF80 + '8' );
MAKE_KEY_CODE( DIK_NUMPAD9, 0x49, 0xFF80 + '9' );
MAKE_KEY_CODE( DIK_SUBTRACT, 0x4A, 0xFF80 + '-' );           /* - on numeric keypad */
MAKE_KEY_CODE( DIK_NUMPAD4, 0x4B, 0xFF80 + '4' );
MAKE_KEY_CODE( DIK_NUMPAD5, 0x4C, 0xFF80 + '5' );
MAKE_KEY_CODE( DIK_NUMPAD6, 0x4D, 0xFF80 + '6' );
MAKE_KEY_CODE( DIK_ADD, 0x4E, 0xFF80 + '+' );                /* + on numeric keypad */
MAKE_KEY_CODE( DIK_NUMPAD1, 0x4F, 0xFF80 + '1' );
MAKE_KEY_CODE( DIK_NUMPAD2, 0x50, 0xFF80 + '2' );
MAKE_KEY_CODE( DIK_NUMPAD3, 0x51, 0xFF80 + '3' );
MAKE_KEY_CODE( DIK_NUMPAD0, 0x52, 0xFF80 + '0' );
MAKE_KEY_CODE( DIK_DECIMAL, 0x53, 0xFF80 + '.' );      /* . on numeric keypad */
MAKE_KEY_CODE( DIK_F11, 0x57, 0xFFBD + 11 );
MAKE_KEY_CODE( DIK_F12, 0x58, 0xFFBD + 12 );
MAKE_KEY_CODE( DIK_NUMPADENTER, 0x9C, 0xFF8D );        /* Enter on numeric keypad */
MAKE_KEY_CODE( DIK_RCONTROL, 0x9D, 0xFFE4 );
MAKE_KEY_CODE( DIK_DIVIDE, 0xB5, 0xFF80 + '/' );       /* / on numeric keypad */
MAKE_KEY_CODE( DIK_SYSRQ, 0xB7, 0xFF61 );
MAKE_KEY_CODE( DIK_RMENU, 0xB8, 0xFFEA );              /* right Alt */
MAKE_KEY_CODE( DIK_PAUSE, 0xC5, 0xFF13 );              /* Pause */
MAKE_KEY_CODE( DIK_HOME, 0xC7, 0xFF50 );               /* Home on arrow keypad */
MAKE_KEY_CODE( DIK_UP, 0xC8, 0xFF52 );                 /* UpArrow on arrow keypad */
MAKE_KEY_CODE( DIK_PRIOR, 0xC9, 0xFF55 );              /* PgUp on arrow keypad */
MAKE_KEY_CODE( DIK_LEFT, 0xCB, 0xFF51 );               /* LeftArrow on arrow keypad */
MAKE_KEY_CODE( DIK_RIGHT, 0xCD, 0xFF53 );              /* RightArrow on arrow keypad */
MAKE_KEY_CODE( DIK_END, 0xCF, 0xFF57 );                /* End on arrow keypad */
MAKE_KEY_CODE( DIK_DOWN, 0xD0, 0xFF54 );               /* DownArrow on arrow keypad */
MAKE_KEY_CODE( DIK_NEXT, 0xD1, 0xFF56 );               /* PgDn on arrow keypad */
MAKE_KEY_CODE( DIK_INSERT, 0xD2, 0xFF63 );             /* Insert on arrow keypad */
MAKE_KEY_CODE( DIK_DELETE, 0xD3, 0xFFFF );             /* Delete on arrow keypad */
MAKE_KEY_CODE( DIK_LWIN, 0xDB, 0xFFE7 );               /* Left Windows key */
MAKE_KEY_CODE( DIK_RWIN, 0xDC, 0xFFE8 );               /* Right Windows key */

#endif // __KEYBOARD__
