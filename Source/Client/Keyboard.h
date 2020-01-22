#pragma once

#include "Common.h"

#include "KeyCodes_Include.h"
#include "Settings.h"

// Keyboard input flags
#define KIF_NO_SPEC_SYMBOLS (1) // Ignore \n \r \t
#define KIF_ONLY_NUMBERS (2) // Only 0..9
#define KIF_FILE_NAME (4) // Ignore \/:*?\"<>|

class SpriteManager;

class Keyboard : public NonCopyable
{
public:
    Keyboard(InputSettings& sett, SpriteManager& spr_mngr);
    ~Keyboard();
    void Lost();
    void GetChar(uchar dik, const string& dik_text, string& str, uint* position, uint max, int flags);
    void EraseInvalidChars(string& str, int flags);
    uchar MapKey(ushort code);
    ushort UnmapKey(uchar key);

    bool ShiftDwn = false;
    bool CtrlDwn = false;
    bool AltDwn = false;
    bool KeyPressed[0x100] = {};

private:
    bool IsInvalidChar(const char* str, uint flags, uint& length);

    InputSettings& settings;
    SpriteManager& sprMngr;
    uchar KeysMap[0x200] = {};
    ushort KeysMapRevert[0x100] = {};
    uchar KeysMapUser[0x100] = {};
};
