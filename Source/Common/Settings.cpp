#include "Settings.h"
#include "FileSystem.h"
#include "StringUtils.h"
#include "WinApi_Include.h"

#include "imgui.h"

Settings GameOpt;

static void SetEntry(string& entry, const string& value)
{
    entry = value;
}
static void SetEntry(uchar& entry, const string& value)
{
    entry = _str(value).toInt();
}
static void SetEntry(short& entry, const string& value)
{
    entry = _str(value).toInt();
}
static void SetEntry(int& entry, const string& value)
{
    entry = _str(value).toInt();
}
static void SetEntry(uint& entry, const string& value)
{
    entry = _str(value).toInt();
}
static void SetEntry(bool& entry, const string& value)
{
    entry = _str(value).toBool();
}
static void SetEntry(float& entry, const string& value)
{
    entry = _str(value).toFloat();
}
static void SetEntry(vector<string>& entry, const string& value)
{
}

static void DrawEntry(const char* name, const string& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEntry(const char* name, const uchar& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEntry(const char* name, const short& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEntry(const char* name, const int& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEntry(const char* name, const uint& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEntry(const char* name, const bool& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEntry(const char* name, const float& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEntry(const char* name, const vector<string>& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, "n/a").c_str());
}

static void DrawEditableEntry(const char* name, string& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEditableEntry(const char* name, uchar& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEditableEntry(const char* name, short& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEditableEntry(const char* name, int& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEditableEntry(const char* name, uint& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEditableEntry(const char* name, bool& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEditableEntry(const char* name, float& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEditableEntry(const char* name, vector<string>& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, "n/a").c_str());
}

static void InitSettings(Settings& settings, StrMap& init_values)
{
#define SETTING(type, name, ...) \
    if (init_values.count(#name)) \
    SetEntry(settings.name, init_values[#name])
#define CONST_SETTING(type, name, ...) \
    if (init_values.count(#name)) \
    SetEntry(const_cast<type&>(settings.name), init_values[#name])
#include "Settings_Include.h"

#ifdef FO_WEB
    const_cast<bool&>(settings.WebBuild) = true;
#else
    const_cast<bool&>(settings.WebBuild) = false;
#endif
#ifdef FO_WINDOWS
    const_cast<bool&>(settings.WindowsBuild) = true;
#else
    const_cast<bool&>(settings.WindowsBuild) = false;
#endif
#ifdef FO_LINUX
    const_cast<bool&>(settings.LinuxBuild) = true;
#else
    const_cast<bool&>(settings.LinuxBuild) = false;
#endif
#ifdef FO_MAC
    const_cast<bool&>(settings.MacOsBuild) = true;
#else
    const_cast<bool&>(settings.MacOsBuild) = false;
#endif
#ifdef FO_ANDROID
    const_cast<bool&>(settings.AndroidBuild) = true;
#else
    const_cast<bool&>(settings.AndroidBuild) = false;
#endif
#ifdef FO_IOS
    const_cast<bool&>(settings.IOsBuild) = true;
#else
    const_cast<bool&>(settings.IOsBuild) = false;
#endif
    const_cast<bool&>(settings.DesktopBuild) = (settings.WindowsBuild || settings.LinuxBuild || settings.MacOsBuild);
    const_cast<bool&>(settings.TabletBuild) = (settings.AndroidBuild || settings.IOsBuild);

#ifdef FO_WINDOWS
    if (GetSystemMetrics(SM_TABLETPC) != 0)
    {
        const_cast<bool&>(settings.DesktopBuild) = false;
        const_cast<bool&>(settings.TabletBuild) = true;
    }
#endif
}

static void DrawSettings(Settings& settings, bool editable)
{
#define SETTING(type, name, ...) \
    if (editable) \
        DrawEditableEntry(#name, settings.name); \
    else \
        DrawEntry(#name, settings.name)
#define CONST_SETTING(type, name, ...) DrawEntry(#name, settings.name)
#include "Settings_Include.h"
}

void Settings::Init(int argc, char** argv)
{
    // Parse command line args
    StrMap commands;
    StrVec configs;
    for (int i = 0; i < argc; i++)
    {
        // Skip path
        if (i == 0 && argv[0][0] != '-')
            continue;

        // Find work path entry
        if (Str::Compare(argv[i], "-WorkDir") && i < argc - 1)
            const_cast<string&>(WorkDir) = argv[i + 1];
        if (Str::Compare(argv[i], "-ServerDir") && i < argc - 1)
            const_cast<string&>(ServerDir) = argv[i + 1];

        // Find config path entry
        if (Str::Compare(argv[i], "-AddConfig") && i < argc - 1)
            configs.push_back(argv[i + 1]);

        // Make single line
        const_cast<string&>(CommandLine) += argv[i];
        if (i < argc - 1)
            const_cast<string&>(CommandLine) += " ";

        // Map commands
        if (argv[i][0] == '-')
        {
            string key = _str("{}", argv[i]).trim().substringAfter('0');
            if (i < argc - 1 && argv[i + 1][0] != '-')
                commands[key] = argv[i + 1][0];
            else
                commands[key] = "1";
        }
    }

    InitSettings(*this, commands);

    // Store start directory
    if (!WorkDir.empty())
    {
#ifdef FO_WINDOWS
        SetCurrentDirectoryW(_str(WorkDir).toWideChar().c_str());
#else
        int r = chdir(WorkDir.c_str());
        UNUSED_VARIABLE(r);
#endif
    }

    // Injected config
    static char InternalConfig[5022] = {
        "###InternalConfig###\0"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"};

    ConfigFile config = ConfigFile(InternalConfig);

    // File configs
    configs.insert(configs.begin(), CONFIG_NAME);
    for (string& config_path : configs)
    {
        DiskFile config_file = DiskFileSystem::OpenFile(config_path, false);
        if (config_file)
        {
            char* buf = new char[config_file.GetSize()];
            if (config_file.Read(buf, config_file.GetSize()))
            {
                config.AppendData(buf);
                config.AppendData("\n");
            }
            delete[] buf;
        }
    }

    // Command line settings
    for (int i = 0; i < argc; i++)
    {
        string arg = argv[i];
        if (arg.length() < 2 || arg[0] != '-')
            continue;

        string arg_value;
        while (i < argc - 1 && argv[i + 1][0] != '-')
        {
            if (arg_value.length() > 0)
                arg_value += " ";
            arg_value += argv[i + 1];
            i++;
        }

        config.SetStr("", arg.substr(1), arg_value);
    }

    // Fix Mono path
    // GameOpt.MonoPath = _str(GameOpt.WorkDir).combinePath(GameOpt.MonoPath).normalizeLineEndings();
    // DiskFileSystem::ResolvePath(GameOpt.MonoPath);
}

void Settings::Draw(bool editable)
{
    DrawSettings(*this, editable);
}
