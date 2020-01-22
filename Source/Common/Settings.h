#pragma once

#include "Common.h"

// Todo: remove VAR_SETTING

struct DummySettings
{
};

#define SETTING_GROUP(name, ...) \
    struct name : __VA_ARGS__ \
    {
#define SETTING_GROUP_END() }
#define SETTING(type, name, ...) const type name = {__VA_ARGS__}
#define VAR_SETTING(type, name, ...) type name = {__VA_ARGS__}
#include "Settings_Include.h"

struct GlobalSettings : ClientSettings, MapperSettings, ServerSettings, EditorSettings
{
public:
    GlobalSettings() = default;
    void ParseArgs(int argc, char** argv);
    void Draw(bool editable);

private:
    void ResetStrongConstants();
    void SetValue(const string& setting_name, string value);
};
