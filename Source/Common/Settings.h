#pragma once

#include "Common.h"

class Settings
{
public:
#define SETTING(type, name, init, desc) type name = init
#define SETTING_ARR(type, name) type name = {}
#include "Settings_Include.h"

    void Init(int argc, char** argv);
    void Draw(bool editable);
};

extern Settings GameOpt;
class IniFile;
extern IniFile* MainConfig;
extern StrVec ProjectFiles;
