#include "Settings.h"
#include "IniFile.h"
#include "StringUtils.h"
#include "WinApi_Include.h"

#include "imgui.h"
#ifndef FO_WINDOWS
#include <unistd.h>
#endif

Settings GameOpt;
IniFile* MainConfig;
StrVec ProjectFiles;

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

static void InitSettings(Settings& settings, StrMap& init_values)
{
#define SETTING(type, name, init, desc) \
    if (init_values.count(#name)) \
    SetEntry(settings.name, init_values[#name])
#define SETTING_ARR(type, name)
#include "Settings_Include.h"

#ifdef FO_WEB
    settings.WebBuild = true;
#else
    settings.WebBuild = false;
#endif
#ifdef FO_WINDOWS
    settings.WindowsBuild = true;
#else
    settings.WindowsBuild = false;
#endif
#ifdef FO_LINUX
    settings.LinuxBuild = true;
#else
    settings.LinuxBuild = false;
#endif
#ifdef FO_MAC
    settings.MacOsBuild = true;
#else
    settings.MacOsBuild = false;
#endif
#ifdef FO_ANDROID
    settings.AndroidBuild = true;
#else
    settings.AndroidBuild = false;
#endif
#ifdef FO_IOS
    settings.IOsBuild = true;
#else
    settings.IOsBuild = false;
#endif
    settings.DesktopBuild = (settings.WindowsBuild || settings.LinuxBuild || settings.MacOsBuild);
    settings.TabletBuild = (settings.AndroidBuild || settings.IOsBuild);

#ifdef FO_WINDOWS
    if (GetSystemMetrics(SM_TABLETPC) != 0)
    {
        settings.DesktopBuild = false;
        settings.TabletBuild = true;
    }
#endif
}

static void DrawSettings(Settings& settings, bool editable)
{
#define SETTING(type, name, init, desc) \
    if (editable) \
        DrawEditableEntry(#name, settings.name); \
    else \
        DrawEntry(#name, settings.name)
#define SETTING_ARR(type, name)
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
            WorkDir = argv[i + 1];
        if (Str::Compare(argv[i], "-ServerDir") && i < argc - 1)
            ServerDir = argv[i + 1];

        // Find config path entry
        if (Str::Compare(argv[i], "-AddConfig") && i < argc - 1)
            configs.push_back(argv[i + 1]);

        // Make single line
        CommandLine += argv[i];
        if (i < argc - 1)
            CommandLine += " ";

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
#ifdef FO_WINDOWS
    wchar_t dir_buf[TEMP_BUF_SIZE];
    GetCurrentDirectoryW(TEMP_BUF_SIZE, dir_buf);
    WorkDir = _str().parseWideChar(dir_buf);
#else
    char dir_buf[TEMP_BUF_SIZE];
    char* r = getcwd(dir_buf, sizeof(dir_buf));
    UNUSED_VARIABLE(r);
    WorkDir = dir_buf;
#endif

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
    MainConfig = new IniFile();
    MainConfig->AppendStr(InternalConfig);

    // File configs
    string config_dir;
    MainConfig->AppendFile(CONFIG_NAME);
    for (string& config : configs)
    {
        config_dir = _str(config).extractDir();
        MainConfig->AppendFile(config);
    }

    // Command line config
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

        MainConfig->SetStr("", arg.substr(1), arg_value);
    }

// Cache project files
#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
    string project_files = MainConfig->GetStr("", "ProjectFiles");
    for (string project_path : _str(project_files).split(';'))
    {
        project_path = _str(config_dir).combinePath(project_path).normalizePathSlashes().resolvePath();
        if (!project_path.empty() && project_path.back() != '/' && project_path.back() != '\\')
            project_path += "/";
        ProjectFiles.push_back(_str(project_path).normalizePathSlashes());
    }
#endif

// Defines
#define GETOPTIONS_CMD_LINE_INT(opt, str_id) \
    do \
    { \
        string str = MainConfig->GetStr("", str_id); \
        if (!str.empty()) \
            opt = MainConfig->GetInt("", str_id); \
    } while (0)
#define GETOPTIONS_CMD_LINE_BOOL(opt, str_id) \
    do \
    { \
        string str = MainConfig->GetStr("", str_id); \
        if (!str.empty()) \
            opt = MainConfig->GetInt("", str_id) != 0; \
    } while (0)
#define GETOPTIONS_CMD_LINE_BOOL_ON(opt, str_id) \
    do \
    { \
        string str = MainConfig->GetStr("", str_id); \
        if (!str.empty()) \
            opt = true; \
    } while (0)
#define GETOPTIONS_CMD_LINE_STR(opt, str_id) \
    do \
    { \
        string str = MainConfig->GetStr("", str_id); \
        if (!str.empty()) \
            opt = str; \
    } while (0)
#define GETOPTIONS_CHECK(val_, min_, max_, def_) \
    do \
    { \
        int val__ = (int)val_; \
        if (val__ < min_ || val__ > max_) \
            val_ = def_; \
    } while (0)

    string buf;
#define READ_CFG_STR_DEF(cfg, key, def_val) buf = MainConfig->GetStr("", key, def_val)

    // Cached configuration
    MainConfig->AppendFile(CONFIG_NAME);

    // Language
    READ_CFG_STR_DEF(*MainConfig, "Language", "russ");
    GETOPTIONS_CMD_LINE_STR(buf, "Language");

    // Int / Bool
    OpenGLDebug = MainConfig->GetInt("", "OpenGLDebug", 0) != 0;
    GETOPTIONS_CMD_LINE_BOOL(OpenGLDebug, "OpenGLDebug");
    AssimpLogging = MainConfig->GetInt("", "AssimpLogging", 0) != 0;
    GETOPTIONS_CMD_LINE_BOOL(AssimpLogging, "AssimpLogging");
    FullScreen = MainConfig->GetInt("", "FullScreen", 0) != 0;
    GETOPTIONS_CMD_LINE_BOOL(FullScreen, "FullScreen");
    VSync = MainConfig->GetInt("", "VSync", 0) != 0;
    GETOPTIONS_CMD_LINE_BOOL(VSync, "VSync");
    Light = MainConfig->GetInt("", "Light", 20);
    GETOPTIONS_CMD_LINE_INT(Light, "Light");
    GETOPTIONS_CHECK(Light, 0, 100, 20);
    ScrollDelay = MainConfig->GetInt("", "ScrollDelay", 10);
    GETOPTIONS_CMD_LINE_INT(ScrollDelay, "ScrollDelay");
    GETOPTIONS_CHECK(ScrollDelay, 0, 100, 10);
    ScrollStep = MainConfig->GetInt("", "ScrollStep", 12);
    GETOPTIONS_CMD_LINE_INT(ScrollStep, "ScrollStep");
    GETOPTIONS_CHECK(ScrollStep, 4, 32, 12);
    TextDelay = MainConfig->GetInt("", "TextDelay", 3000);
    GETOPTIONS_CMD_LINE_INT(TextDelay, "TextDelay");
    GETOPTIONS_CHECK(TextDelay, 1000, 30000, 3000);
    DamageHitDelay = MainConfig->GetInt("", "DamageHitDelay", 0);
    GETOPTIONS_CMD_LINE_INT(DamageHitDelay, "OptDamageHitDelay");
    GETOPTIONS_CHECK(DamageHitDelay, 0, 30000, 0);
    ScreenWidth = MainConfig->GetInt("", "ScreenWidth", 0);
    GETOPTIONS_CMD_LINE_INT(ScreenWidth, "ScreenWidth");
    GETOPTIONS_CHECK(ScreenWidth, 100, 30000, 800);
    ScreenHeight = MainConfig->GetInt("", "ScreenHeight", 0);
    GETOPTIONS_CMD_LINE_INT(ScreenHeight, "ScreenHeight");
    GETOPTIONS_CHECK(ScreenHeight, 100, 30000, 600);
    AlwaysOnTop = MainConfig->GetInt("", "AlwaysOnTop", false) != 0;
    GETOPTIONS_CMD_LINE_BOOL(AlwaysOnTop, "AlwaysOnTop");
    FixedFPS = MainConfig->GetInt("", "FixedFPS", 100);
    GETOPTIONS_CMD_LINE_INT(FixedFPS, "FixedFPS");
    GETOPTIONS_CHECK(FixedFPS, -10000, 10000, 100);
    MsgboxInvert = MainConfig->GetInt("", "InvertMessBox", false) != 0;
    GETOPTIONS_CMD_LINE_BOOL(MsgboxInvert, "InvertMessBox");
    MessNotify = MainConfig->GetInt("", "WinNotify", true) != 0;
    GETOPTIONS_CMD_LINE_BOOL(MessNotify, "WinNotify");
    SoundNotify = MainConfig->GetInt("", "SoundNotify", false) != 0;
    GETOPTIONS_CMD_LINE_BOOL(SoundNotify, "SoundNotify");
    Port = MainConfig->GetInt("", "RemotePort", 4010);
    GETOPTIONS_CMD_LINE_INT(Port, "RemotePort");
    GETOPTIONS_CHECK(Port, 0, 0xFFFF, 4000);
    ProxyType = MainConfig->GetInt("", "ProxyType", 0);
    GETOPTIONS_CMD_LINE_INT(ProxyType, "ProxyType");
    GETOPTIONS_CHECK(ProxyType, 0, 3, 0);
    ProxyPort = MainConfig->GetInt("", "ProxyPort", 8080);
    GETOPTIONS_CMD_LINE_INT(ProxyPort, "ProxyPort");
    GETOPTIONS_CHECK(ProxyPort, 0, 0xFFFF, 1080);

    uint dct = 500;
#ifdef FO_WINDOWS
    dct = (uint)GetDoubleClickTime();
#endif
    DoubleClickTime = MainConfig->GetInt("", "DoubleClickTime", dct);
    GETOPTIONS_CMD_LINE_INT(DoubleClickTime, "DoubleClickTime");
    GETOPTIONS_CHECK(DoubleClickTime, 0, 1000, dct);

    GETOPTIONS_CMD_LINE_BOOL_ON(HelpInfo, "HelpInfo");
    GETOPTIONS_CMD_LINE_BOOL_ON(DebugInfo, "DebugInfo");
    GETOPTIONS_CMD_LINE_BOOL_ON(DebugNet, "DebugNet");

    // Str
    READ_CFG_STR_DEF(*MainConfig, "RemoteHost", "localhost");
    GETOPTIONS_CMD_LINE_STR(buf, "RemoteHost");
    Host = buf;
    READ_CFG_STR_DEF(*MainConfig, "ProxyHost", "localhost");
    GETOPTIONS_CMD_LINE_STR(buf, "ProxyHost");
    ProxyHost = buf;
    READ_CFG_STR_DEF(*MainConfig, "ProxyUser", "");
    GETOPTIONS_CMD_LINE_STR(buf, "ProxyUser");
    ProxyUser = buf;
    READ_CFG_STR_DEF(*MainConfig, "ProxyPass", "");
    GETOPTIONS_CMD_LINE_STR(buf, "ProxyPass");
    ProxyPass = buf;
    READ_CFG_STR_DEF(*MainConfig, "KeyboardRemap", "");
    GETOPTIONS_CMD_LINE_STR(buf, "KeyboardRemap");
    KeyboardRemap = buf;
}

void Settings::Draw(bool editable)
{
    DrawSettings(*this, editable);
}
