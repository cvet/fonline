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
    extern int  KeysMap[ 0x100 ];

    void InitKeyb();
    void ClearKeyb();
    void Lost();
    void GetChar( uchar dik, string& str, int* position, int max, int flags );
    void GetChar( uchar dik, char* str, int* position, int max, int flags );
    void PuntoSwitch( char* str );
    void EraseInvalidChars( char* str, int flags );
    bool IsInvalidSymbol( char c, uint flags );
}

#endif // __KEYBOARD__
