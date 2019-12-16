#pragma once

#include "Common.h"

class Settings
{
public:
    string WorkDir = "";
    string CommandLine = "";

    uint YearStartFTLo = 0;
    uint YearStartFTHi = 0;
    uint FullSecondStart = 0;
    uint FullSecond = 0;
    uint GameTimeTick = 0;

    bool DisableTcpNagle = 0;
    bool DisableZlibCompression = 0;
    uint FloodSize = 2048;
    bool NoAnswerShuffle = false;
    bool DialogDemandRecheck = false;
    uint SneakDivider = 6;
    uint LookMinimum = 6;
    int DeadHitPoints = -6;
    uint Breaktime = 1200;
    uint TimeoutTransfer = 3;
    uint TimeoutBattle = 10;
    bool RunOnCombat = false;
    bool RunOnTransfer = false;
    uint GlobalMapWidth = 28;
    uint GlobalMapHeight = 30;
    uint GlobalMapZoneLength = 50;
    uint BagRefreshTime = 60;
    uint WhisperDist = 2;
    uint ShoutDist = 200;
    int LookChecks = 0;
    uint LookDir[5] = {0, 20, 40, 60, 60};
    uint LookSneakDir[5] = {90, 60, 30, 0, 0};
    uint RegistrationTimeout = 5;
    uint AccountPlayTime = 0;
    uint ScriptRunSuspendTimeout = 30000;
    uint ScriptRunMessageTimeout = 10000;
    uint TalkDistance = 3;
    uint NpcMaxTalkers = 1;
    uint MinNameLength = 4;
    uint MaxNameLength = 12;
    uint DlgTalkMinTime = 0;
    uint DlgBarterMinTime = 0;
    uint MinimumOfflineTime = 180000;
    bool ForceRebuildResources = false;

    bool MapHexagonal = true;
    int MapHexWidth = 32;
    int MapHexHeight = 16;
    int MapHexLineHeight = 12;
    int MapTileOffsX = -8;
    int MapTileOffsY = 32;
    int MapTileStep = 2;
    int MapRoofOffsX = -8;
    int MapRoofOffsY = -66;
    int MapRoofSkipSize = 2;
    float MapCameraAngle = 25.7f;
    bool MapSmoothPath = true;
    string MapDataPrefix = "art/geometry/fallout_";

    // Client and Mapper
    bool Quit = false;
    int WaitPing = 0;
    bool OpenGLRendering = true;
    bool OpenGLDebug = false;
    bool AssimpLogging = false;
    int MouseX = 0;
    int MouseY = 0;
    int LastMouseX = 0;
    int LastMouseY = 0;
    int ScrOx = 0;
    int ScrOy = 0;
    bool ShowTile = true;
    bool ShowRoof = true;
    bool ShowItem = true;
    bool ShowScen = true;
    bool ShowWall = true;
    bool ShowCrit = true;
    bool ShowFast = true;
    bool ShowPlayerNames = false;
    bool ShowNpcNames = false;
    bool ShowCritId = false;
    bool ScrollKeybLeft = false;
    bool ScrollKeybRight = false;
    bool ScrollKeybUp = false;
    bool ScrollKeybDown = false;
    bool ScrollMouseLeft = false;
    bool ScrollMouseRight = false;
    bool ScrollMouseUp = false;
    bool ScrollMouseDown = false;
    bool ShowGroups = true;
    bool HelpInfo = false;
    bool DebugInfo = false;
    bool DebugNet = false;
    bool Enable3dRendering = false;
    bool FullScreen = false;
    bool VSync = false;
    int Light = 0;
    string Host = "localhost";
    uint Port = 4000;
    uint ProxyType = 0;
    string ProxyHost = "";
    uint ProxyPort = 0;
    string ProxyUser = "";
    string ProxyPass = "";
    uint ScrollDelay = 10;
    int ScrollStep = 1;
    bool ScrollCheck = true;
    int FixedFPS = 100;
    uint FPS = 0;
    uint PingPeriod = 2000;
    uint Ping = 0;
    bool MsgboxInvert = false;
    bool MessNotify = true;
    bool SoundNotify = true;
    bool AlwaysOnTop = false;
    uint TextDelay = 3000;
    uint DamageHitDelay = 0;
    int ScreenWidth = 1024;
    int ScreenHeight = 768;
    int MultiSampling = -1;
    bool MouseScroll = true;
    uint DoubleClickTime = 0;
    uchar RoofAlpha = 200;
    bool HideCursor = false;
    bool ShowMoveCursor = false;
    bool DisableMouseEvents = false;
    bool DisableKeyboardEvents = false;
    bool HidePassword = true;
    string PlayerOffAppendix = "_off";
    uint Animation3dSmoothTime = 150;
    uint Animation3dFPS = 30;
    int RunModMul = 1;
    int RunModDiv = 3;
    int RunModAdd = 0;
    bool MapZooming = false;
    float SpritesZoom = 1.0f;
    float SpritesZoomMax = MAX_ZOOM;
    float SpritesZoomMin = MIN_ZOOM;
    float EffectValues[EFFECT_SCRIPT_VALUES];
    string KeyboardRemap = "";
    uint CritterFidgetTime = 50000;
    uint Anim2CombatBegin = 0;
    uint Anim2CombatIdle = 0;
    uint Anim2CombatEnd = 0;
    uint RainTick = 60;
    short RainSpeedX = 0;
    short RainSpeedY = 15;
    uint ConsoleHistorySize = 20;
    int SoundVolume = 100;
    int MusicVolume = 100;
    uint ChosenLightColor = 0;
    uchar ChosenLightDistance = 4;
    int ChosenLightIntensity = 2500;
    uchar ChosenLightFlags = 0;

    // Mapper
    string ServerDir = "";
    bool ShowCorners = false;
    bool ShowSpriteCuts = false;
    bool ShowDrawOrder = false;
    bool SplitTilesCollection = true;

    // Build platform
    bool WebBuild = false;
    bool WindowsBuild = false;
    bool LinuxBuild = false;
    bool MacOsBuild = false;
    bool AndroidBuild = false;
    bool IOsBuild = false;
    bool DesktopBuild = false;
    bool TabletBuild = false;

    void Init(int argc, char** argv);
    void Draw(bool editable);
};

extern Settings GameOpt;
class IniFile;
extern IniFile* MainConfig;
extern StrVec ProjectFiles;
