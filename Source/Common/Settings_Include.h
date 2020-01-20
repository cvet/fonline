#if !defined(SETTING) || !defined(CONST_SETTING)
#error SETTING or CONST_SETTING not defined
#define SETTING(type, name, ...)
#define CONST_SETTING(type, name, ...)
#include "Common.h"
#endif

// Common
CONST_SETTING(string, WorkDir, "");
CONST_SETTING(vector<string>, ProjectFiles);
CONST_SETTING(string, CommandLine, "");
CONST_SETTING(string, WindowName, "FOnline");
CONST_SETTING(string, MonoPath, "");
CONST_SETTING(bool, OpenGLRendering, true);
CONST_SETTING(bool, OpenGLDebug, false);
CONST_SETTING(bool, Enable3dRendering, false);
CONST_SETTING(bool, VSync, false);
CONST_SETTING(uint, ScriptRunSuspendTimeout, 30000);
CONST_SETTING(uint, ScriptRunMessageTimeout, 10000);
CONST_SETTING(uint, TalkDistance, 3);
CONST_SETTING(uint, MinNameLength, 4);
CONST_SETTING(uint, MaxNameLength, 12);
CONST_SETTING(bool, DisableTcpNagle, false);
CONST_SETTING(bool, DisableZlibCompression, false);
CONST_SETTING(uint, GlobalMapWidth, 28);
CONST_SETTING(uint, GlobalMapHeight, 30);
CONST_SETTING(uint, GlobalMapZoneLength, 50);
CONST_SETTING(bool, RunOnCombat, false);
CONST_SETTING(bool, RunOnTransfer, false);
CONST_SETTING(uint, Breaktime, 1200);
CONST_SETTING(uint, TimeoutTransfer, 3);
CONST_SETTING(uint, TimeoutBattle, 10);
SETTING(bool, Quit, false);
SETTING(uint, YearStartFTLo, 0);
SETTING(uint, YearStartFTHi, 0);
SETTING(uint, FullSecondStart, 0);
SETTING(uint, FullSecond, 0);
SETTING(uint, GameTimeTick, 0);

// Client
CONST_SETTING(string, RemoteHost, "localhost");
CONST_SETTING(uint, RemotePort, 4000);
CONST_SETTING(uint, ProxyType, 0);
CONST_SETTING(string, ProxyHost, "");
CONST_SETTING(uint, ProxyPort, 8080);
CONST_SETTING(string, ProxyUser, "");
CONST_SETTING(string, ProxyPass, "");
CONST_SETTING(uint, PingPeriod, 2000);
CONST_SETTING(uint, TextDelay, 3000);
CONST_SETTING(bool, WinNotify, true);
CONST_SETTING(bool, SoundNotify, false);
CONST_SETTING(string, PlayerOffAppendix, "_off");
CONST_SETTING(uint, ChosenLightColor, 0);
CONST_SETTING(uchar, ChosenLightDistance, 4);
CONST_SETTING(int, ChosenLightIntensity, 2500);
CONST_SETTING(uchar, ChosenLightFlags, 0);
SETTING(bool, WaitPing, false);
SETTING(uint, FPS, 0);
SETTING(uint, Ping, 0);

// Mapper
CONST_SETTING(string, ServerDir, "");
CONST_SETTING(string, AutoLogin, "");
CONST_SETTING(string, StartMap, "");
CONST_SETTING(int, StartHexX, -1);
CONST_SETTING(int, StartHexY, -1);
CONST_SETTING(int, FixedFPS, 100);
CONST_SETTING(bool, SplitTilesCollection, true);
SETTING(bool, ShowCorners, false);
SETTING(bool, ShowSpriteCuts, false);
SETTING(bool, ShowDrawOrder, false);

// Client & Mapper
CONST_SETTING(string, Language, "engl");
CONST_SETTING(uint, ScrollDelay, 10);
CONST_SETTING(int, ScrollStep, 12);
CONST_SETTING(int, MultiSampling, -1);
CONST_SETTING(uint, DoubleClickTime, 500);
CONST_SETTING(uint, Animation3dSmoothTime, 150);
CONST_SETTING(uint, Animation3dFPS, 30);
CONST_SETTING(string, KeyboardRemap, "");
CONST_SETTING(uint, ConsoleHistorySize, 100);
CONST_SETTING(uint, CritterFidgetTime, 50000);
CONST_SETTING(uint, Anim2CombatBegin, 0);
CONST_SETTING(uint, Anim2CombatIdle, 0);
CONST_SETTING(uint, Anim2CombatEnd, 0);
CONST_SETTING(uint, RainTick, 60);
CONST_SETTING(short, RainSpeedX, 0);
CONST_SETTING(short, RainSpeedY, 15);
CONST_SETTING(float, SpritesZoomMax, MAX_ZOOM);
CONST_SETTING(float, SpritesZoomMin, MIN_ZOOM);
SETTING(uchar, RoofAlpha, 200);
SETTING(bool, FullScreen, false);
SETTING(int, Light, 20);
SETTING(bool, ScrollCheck, true);
SETTING(bool, AlwaysOnTop, false);
SETTING(int, ScreenWidth, 1024);
SETTING(int, ScreenHeight, 768);
SETTING(bool, MouseScroll, true);
SETTING(bool, HideCursor, false);
SETTING(bool, ShowMoveCursor, false);
SETTING(bool, DisableMouseEvents, false);
SETTING(bool, DisableKeyboardEvents, false);
SETTING(int, SoundVolume, 100);
SETTING(int, MusicVolume, 100);
SETTING(float, EffectValues[EFFECT_SCRIPT_VALUES]);
SETTING(bool, MapZooming, false);
SETTING(float, SpritesZoom, 1.0f);
SETTING(bool, ShowTile, true);
SETTING(bool, ShowRoof, true);
SETTING(bool, ShowItem, true);
SETTING(bool, ShowScen, true);
SETTING(bool, ShowWall, true);
SETTING(bool, ShowCrit, true);
SETTING(bool, ShowFast, true);
SETTING(bool, ShowPlayerNames, false);
SETTING(bool, ShowNpcNames, false);
SETTING(bool, ShowCritId, false);
SETTING(bool, ShowGroups, true);
SETTING(bool, HelpInfo, false);
SETTING(bool, DebugInfo, false);
SETTING(bool, DebugNet, false);
SETTING(bool, ScrollKeybLeft, false);
SETTING(bool, ScrollKeybRight, false);
SETTING(bool, ScrollKeybUp, false);
SETTING(bool, ScrollKeybDown, false);
SETTING(bool, ScrollMouseLeft, false);
SETTING(bool, ScrollMouseRight, false);
SETTING(bool, ScrollMouseUp, false);
SETTING(bool, ScrollMouseDown, false);
SETTING(int, MouseX, 0);
SETTING(int, MouseY, 0);
SETTING(int, LastMouseX, 0);
SETTING(int, LastMouseY, 0);
SETTING(int, ScrOx, 0);
SETTING(int, ScrOy, 0);

// Server
CONST_SETTING(uint, ListenPort, 4000);
CONST_SETTING(uint, AdminPanelPort, 0);
CONST_SETTING(bool, AssimpLogging, false);
CONST_SETTING(bool, ForceRebuildResources, false);
CONST_SETTING(string, DbStorage, "Memory");
CONST_SETTING(string, DbHistory, "None");
CONST_SETTING(string, WssCredentials, "");
CONST_SETTING(bool, NoStart, false);
CONST_SETTING(uint, MinimumOfflineTime, 180000);
CONST_SETTING(uint, RegistrationTimeout, 5);
CONST_SETTING(uint, NpcMaxTalkers, 1);
CONST_SETTING(uint, DlgTalkMinTime, 0);
CONST_SETTING(uint, DlgBarterMinTime, 0);
CONST_SETTING(int, LookChecks, 0);
CONST_SETTING(uint, LookDir[5], 0, 20, 40, 60, 60);
CONST_SETTING(uint, LookSneakDir[5], 90, 60, 30, 0, 0);
CONST_SETTING(uint, WhisperDist, 2);
CONST_SETTING(uint, ShoutDist, 200);
CONST_SETTING(int, GameSleep, 0);
CONST_SETTING(uint, FloodSize, 2048);
CONST_SETTING(bool, NoAnswerShuffle, false);
CONST_SETTING(uint, SneakDivider, 6);
CONST_SETTING(uint, LookMinimum, 6);
CONST_SETTING(int, DeadHitPoints, -6);
SETTING(bool, DialogDemandRecheck, false);

// Geometry
CONST_SETTING(bool, MapHexagonal, true);
CONST_SETTING(int, MapHexWidth, 32);
CONST_SETTING(int, MapHexHeight, 16);
CONST_SETTING(int, MapHexLineHeight, 12);
CONST_SETTING(int, MapTileOffsX, -8);
CONST_SETTING(int, MapTileOffsY, 32);
CONST_SETTING(int, MapTileStep, 2);
CONST_SETTING(int, MapRoofOffsX, -8);
CONST_SETTING(int, MapRoofOffsY, -66);
CONST_SETTING(int, MapRoofSkipSize, 2);
CONST_SETTING(float, MapCameraAngle, 25.7f);
CONST_SETTING(bool, MapSmoothPath, true);
CONST_SETTING(string, MapDataPrefix, "art/geometry/fallout_");

// Build platform
CONST_SETTING(bool, WebBuild);
CONST_SETTING(bool, WindowsBuild);
CONST_SETTING(bool, LinuxBuild);
CONST_SETTING(bool, MacOsBuild);
CONST_SETTING(bool, AndroidBuild);
CONST_SETTING(bool, IOsBuild);
CONST_SETTING(bool, DesktopBuild);
CONST_SETTING(bool, TabletBuild);

#undef SETTING
#undef CONST_SETTING
