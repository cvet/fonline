#include "Settings.h"
#include "IniFile.h"
#include "StringUtils.h"
#include "imgui.h"

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

static void Process(Settings& settings, StrMap* init, bool draw_editable)
{
#define PROCESS_ENTRY(entry) \
    if (init && init->count(#entry)) \
        SetEntry(settings.entry, (*init)[#entry]); \
    else if (!init && !draw_editable) \
        DrawEntry(#entry, settings.entry); \
    else if (!init && draw_editable) \
    DrawEditableEntry(#entry, settings.entry)
    PROCESS_ENTRY(WorkDir);
    PROCESS_ENTRY(CommandLine);
    PROCESS_ENTRY(FullSecondStart);
    PROCESS_ENTRY(FullSecond);
    PROCESS_ENTRY(GameTimeTick);
    PROCESS_ENTRY(YearStartFTHi);
    PROCESS_ENTRY(YearStartFTLo);
    PROCESS_ENTRY(DisableTcpNagle);
    PROCESS_ENTRY(DisableZlibCompression);
    PROCESS_ENTRY(FloodSize);
    PROCESS_ENTRY(NoAnswerShuffle);
    PROCESS_ENTRY(DialogDemandRecheck);
    PROCESS_ENTRY(SneakDivider);
    PROCESS_ENTRY(LookMinimum);
    PROCESS_ENTRY(DeadHitPoints);
    PROCESS_ENTRY(Breaktime);
    PROCESS_ENTRY(TimeoutTransfer);
    PROCESS_ENTRY(TimeoutBattle);
    PROCESS_ENTRY(RunOnCombat);
    PROCESS_ENTRY(RunOnTransfer);
    PROCESS_ENTRY(GlobalMapWidth);
    PROCESS_ENTRY(GlobalMapHeight);
    PROCESS_ENTRY(GlobalMapZoneLength);
    PROCESS_ENTRY(BagRefreshTime);
    PROCESS_ENTRY(WhisperDist);
    PROCESS_ENTRY(ShoutDist);
    PROCESS_ENTRY(LookChecks);
    // PROCESS_ENTRY(LookDir);
    // PROCESS_ENTRY(LookSneakDir);
    PROCESS_ENTRY(RegistrationTimeout);
    PROCESS_ENTRY(AccountPlayTime);
    PROCESS_ENTRY(ScriptRunSuspendTimeout);
    PROCESS_ENTRY(ScriptRunMessageTimeout);
    PROCESS_ENTRY(TalkDistance);
    PROCESS_ENTRY(NpcMaxTalkers);
    PROCESS_ENTRY(MinNameLength);
    PROCESS_ENTRY(MaxNameLength);
    PROCESS_ENTRY(DlgTalkMinTime);
    PROCESS_ENTRY(DlgBarterMinTime);
    PROCESS_ENTRY(MinimumOfflineTime);
    PROCESS_ENTRY(ForceRebuildResources);
    PROCESS_ENTRY(MapHexagonal);
    PROCESS_ENTRY(MapHexWidth);
    PROCESS_ENTRY(MapHexHeight);
    PROCESS_ENTRY(MapHexLineHeight);
    PROCESS_ENTRY(MapTileOffsX);
    PROCESS_ENTRY(MapTileOffsY);
    PROCESS_ENTRY(MapTileStep);
    PROCESS_ENTRY(MapRoofOffsX);
    PROCESS_ENTRY(MapRoofOffsY);
    PROCESS_ENTRY(MapRoofSkipSize);
    PROCESS_ENTRY(MapCameraAngle);
    PROCESS_ENTRY(MapSmoothPath);
    PROCESS_ENTRY(MapDataPrefix);
    PROCESS_ENTRY(Quit);
    PROCESS_ENTRY(WaitPing);
    PROCESS_ENTRY(OpenGLRendering);
    PROCESS_ENTRY(OpenGLDebug);
    PROCESS_ENTRY(AssimpLogging);
    PROCESS_ENTRY(MouseX);
    PROCESS_ENTRY(MouseY);
    PROCESS_ENTRY(LastMouseX);
    PROCESS_ENTRY(LastMouseY);
    PROCESS_ENTRY(ScrOx);
    PROCESS_ENTRY(ScrOy);
    PROCESS_ENTRY(ShowTile);
    PROCESS_ENTRY(ShowRoof);
    PROCESS_ENTRY(ShowItem);
    PROCESS_ENTRY(ShowScen);
    PROCESS_ENTRY(ShowWall);
    PROCESS_ENTRY(ShowCrit);
    PROCESS_ENTRY(ShowFast);
    PROCESS_ENTRY(ShowPlayerNames);
    PROCESS_ENTRY(ShowNpcNames);
    PROCESS_ENTRY(ShowCritId);
    PROCESS_ENTRY(ScrollKeybLeft);
    PROCESS_ENTRY(ScrollKeybRight);
    PROCESS_ENTRY(ScrollKeybUp);
    PROCESS_ENTRY(ScrollKeybDown);
    PROCESS_ENTRY(ScrollMouseLeft);
    PROCESS_ENTRY(ScrollMouseRight);
    PROCESS_ENTRY(ScrollMouseUp);
    PROCESS_ENTRY(ScrollMouseDown);
    PROCESS_ENTRY(ShowGroups);
    PROCESS_ENTRY(HelpInfo);
    PROCESS_ENTRY(DebugInfo);
    PROCESS_ENTRY(DebugNet);
    PROCESS_ENTRY(Enable3dRendering);
    PROCESS_ENTRY(FullScreen);
    PROCESS_ENTRY(VSync);
    PROCESS_ENTRY(Light);
    PROCESS_ENTRY(Host);
    PROCESS_ENTRY(Port);
    PROCESS_ENTRY(ProxyType);
    PROCESS_ENTRY(ProxyHost);
    PROCESS_ENTRY(ProxyPort);
    PROCESS_ENTRY(ProxyUser);
    PROCESS_ENTRY(ProxyPass);
    PROCESS_ENTRY(ScrollDelay);
    PROCESS_ENTRY(ScrollStep);
    PROCESS_ENTRY(ScrollCheck);
    PROCESS_ENTRY(FixedFPS);
    PROCESS_ENTRY(FPS);
    PROCESS_ENTRY(PingPeriod);
    PROCESS_ENTRY(Ping);
    PROCESS_ENTRY(MsgboxInvert);
    PROCESS_ENTRY(MessNotify);
    PROCESS_ENTRY(SoundNotify);
    PROCESS_ENTRY(AlwaysOnTop);
    PROCESS_ENTRY(TextDelay);
    PROCESS_ENTRY(DamageHitDelay);
    PROCESS_ENTRY(ScreenWidth);
    PROCESS_ENTRY(ScreenHeight);
    PROCESS_ENTRY(MultiSampling);
    PROCESS_ENTRY(MouseScroll);
    PROCESS_ENTRY(DoubleClickTime);
    PROCESS_ENTRY(RoofAlpha);
    PROCESS_ENTRY(HideCursor);
    PROCESS_ENTRY(ShowMoveCursor);
    PROCESS_ENTRY(DisableMouseEvents);
    PROCESS_ENTRY(DisableKeyboardEvents);
    PROCESS_ENTRY(HidePassword);
    PROCESS_ENTRY(PlayerOffAppendix);
    PROCESS_ENTRY(Animation3dSmoothTime);
    PROCESS_ENTRY(Animation3dFPS);
    PROCESS_ENTRY(RunModMul);
    PROCESS_ENTRY(RunModDiv);
    PROCESS_ENTRY(RunModAdd);
    PROCESS_ENTRY(MapZooming);
    PROCESS_ENTRY(SpritesZoom);
    PROCESS_ENTRY(SpritesZoomMax);
    PROCESS_ENTRY(SpritesZoomMin);
    // PROCESS_ENTRY(EffectValues);
    PROCESS_ENTRY(KeyboardRemap);
    PROCESS_ENTRY(CritterFidgetTime);
    PROCESS_ENTRY(Anim2CombatBegin);
    PROCESS_ENTRY(Anim2CombatIdle);
    PROCESS_ENTRY(Anim2CombatEnd);
    PROCESS_ENTRY(RainTick);
    PROCESS_ENTRY(RainSpeedX);
    PROCESS_ENTRY(RainSpeedY);
    PROCESS_ENTRY(ConsoleHistorySize);
    PROCESS_ENTRY(SoundVolume);
    PROCESS_ENTRY(MusicVolume);
    PROCESS_ENTRY(ChosenLightColor);
    PROCESS_ENTRY(ChosenLightDistance);
    PROCESS_ENTRY(ChosenLightIntensity);
    PROCESS_ENTRY(ChosenLightFlags);
    PROCESS_ENTRY(ServerDir);
    PROCESS_ENTRY(ShowCorners);
    PROCESS_ENTRY(ShowSpriteCuts);
    PROCESS_ENTRY(ShowDrawOrder);
    PROCESS_ENTRY(SplitTilesCollection);
#undef PROCESS_ENTRY

    // ProcessEntry
    if (init)
    {
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

    Process(*this, &commands, false);

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
    Process(*this, nullptr, editable);
}
