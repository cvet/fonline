#pragma once

#include "Common.h"

class Settings
{
public:
#define SETTING(type, name, ...) type name = {__VA_ARGS__}
#define CONST_SETTING(type, name, ...) const type name = {__VA_ARGS__}
#include "Settings_Include.h"

    void Init(int argc, char** argv);
    void Draw(bool editable);
};

extern Settings GameOpt;
