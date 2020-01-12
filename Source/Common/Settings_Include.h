#ifndef SETTING
#error SETTING not defined
#define SETTING(type, name, init, desc)
#include "Common.h"
#endif

SETTING(string, WorkDir, "", "");
SETTING(string, CommandLine, "", "");
SETTING(string, WindowName, "FOnline", "");

SETTING(uint, YearStartFTLo, 0, "");
SETTING(uint, YearStartFTHi, 0, "");
SETTING(uint, FullSecondStart, 0, "");
SETTING(uint, FullSecond, 0, "");
SETTING(uint, GameTimeTick, 0, "");

SETTING(bool, DisableTcpNagle, false, "");
SETTING(bool, DisableZlibCompression, false, "");
SETTING(uint, FloodSize, 2048, "");
SETTING(bool, NoAnswerShuffle, false, "");
SETTING(bool, DialogDemandRecheck, false, "");
SETTING(uint, SneakDivider, 6, "");
SETTING(uint, LookMinimum, 6, "");
SETTING(int, DeadHitPoints, -6, "");
SETTING(uint, Breaktime, 1200, "");
SETTING(uint, TimeoutTransfer, 3, "");
SETTING(uint, TimeoutBattle, 10, "");
SETTING(bool, RunOnCombat, false, "");
SETTING(bool, RunOnTransfer, false, "");
SETTING(uint, GlobalMapWidth, 28, "");
SETTING(uint, GlobalMapHeight, 30, "");
SETTING(uint, GlobalMapZoneLength, 50, "");
SETTING(uint, BagRefreshTime, 60, "");
SETTING(uint, WhisperDist, 2, "");
SETTING(uint, ShoutDist, 200, "");
SETTING(int, LookChecks, 0, "");
SETTING_ARR(uint, LookDir[5]); //, ({0, 20, 40, 60, 60}), "");
SETTING_ARR(uint, LookSneakDir[5]); //, ({90, 60, 30, 0, 0}), "");
SETTING(uint, RegistrationTimeout, 5, "");
SETTING(uint, AccountPlayTime, 0, "");
SETTING(uint, ScriptRunSuspendTimeout, 30000, "");
SETTING(uint, ScriptRunMessageTimeout, 10000, "");
SETTING(uint, TalkDistance, 3, "");
SETTING(uint, NpcMaxTalkers, 1, "");
SETTING(uint, MinNameLength, 4, "");
SETTING(uint, MaxNameLength, 12, "");
SETTING(uint, DlgTalkMinTime, 0, "");
SETTING(uint, DlgBarterMinTime, 0, "");
SETTING(uint, MinimumOfflineTime, 180000, "");
SETTING(bool, ForceRebuildResources, false, "");

SETTING(bool, MapHexagonal, true, "");
SETTING(int, MapHexWidth, 32, "");
SETTING(int, MapHexHeight, 16, "");
SETTING(int, MapHexLineHeight, 12, "");
SETTING(int, MapTileOffsX, -8, "");
SETTING(int, MapTileOffsY, 32, "");
SETTING(int, MapTileStep, 2, "");
SETTING(int, MapRoofOffsX, -8, "");
SETTING(int, MapRoofOffsY, -66, "");
SETTING(int, MapRoofSkipSize, 2, "");
SETTING(float, MapCameraAngle, 25.7f, "");
SETTING(bool, MapSmoothPath, true, "");
SETTING(string, MapDataPrefix, "art/geometry/fallout_", "");

// Client and Mapper
SETTING(bool, Quit, false, "");
SETTING(int, WaitPing, 0, "");
SETTING(bool, OpenGLRendering, true, "");
SETTING(bool, OpenGLDebug, false, "");
SETTING(bool, AssimpLogging, false, "");
SETTING(int, MouseX, 0, "");
SETTING(int, MouseY, 0, "");
SETTING(int, LastMouseX, 0, "");
SETTING(int, LastMouseY, 0, "");
SETTING(int, ScrOx, 0, "");
SETTING(int, ScrOy, 0, "");
SETTING(bool, ShowTile, true, "");
SETTING(bool, ShowRoof, true, "");
SETTING(bool, ShowItem, true, "");
SETTING(bool, ShowScen, true, "");
SETTING(bool, ShowWall, true, "");
SETTING(bool, ShowCrit, true, "");
SETTING(bool, ShowFast, true, "");
SETTING(bool, ShowPlayerNames, false, "");
SETTING(bool, ShowNpcNames, false, "");
SETTING(bool, ShowCritId, false, "");
SETTING(bool, ScrollKeybLeft, false, "");
SETTING(bool, ScrollKeybRight, false, "");
SETTING(bool, ScrollKeybUp, false, "");
SETTING(bool, ScrollKeybDown, false, "");
SETTING(bool, ScrollMouseLeft, false, "");
SETTING(bool, ScrollMouseRight, false, "");
SETTING(bool, ScrollMouseUp, false, "");
SETTING(bool, ScrollMouseDown, false, "");
SETTING(bool, ShowGroups, true, "");
SETTING(bool, HelpInfo, false, "");
SETTING(bool, DebugInfo, false, "");
SETTING(bool, DebugNet, false, "");
SETTING(bool, Enable3dRendering, false, "");
SETTING(bool, FullScreen, false, "");
SETTING(bool, VSync, false, "");
SETTING(int, Light, 0, "");
SETTING(string, Host, "localhost", "");
SETTING(uint, Port, 4000, "");
SETTING(uint, ProxyType, 0, "");
SETTING(string, ProxyHost, "", "");
SETTING(uint, ProxyPort, 0, "");
SETTING(string, ProxyUser, "", "");
SETTING(string, ProxyPass, "", "");
SETTING(uint, ScrollDelay, 10, "");
SETTING(int, ScrollStep, 1, "");
SETTING(bool, ScrollCheck, true, "");
SETTING(int, FixedFPS, 100, "");
SETTING(uint, FPS, 0, "");
SETTING(uint, PingPeriod, 2000, "");
SETTING(uint, Ping, 0, "");
SETTING(bool, MsgboxInvert, false, "");
SETTING(bool, MessNotify, true, "");
SETTING(bool, SoundNotify, true, "");
SETTING(bool, AlwaysOnTop, false, "");
SETTING(uint, TextDelay, 3000, "");
SETTING(uint, DamageHitDelay, 0, "");
SETTING(int, ScreenWidth, 1024, "");
SETTING(int, ScreenHeight, 768, "");
SETTING(int, MultiSampling, -1, "");
SETTING(bool, MouseScroll, true, "");
SETTING(uint, DoubleClickTime, 0, "");
SETTING(uchar, RoofAlpha, 200, "");
SETTING(bool, HideCursor, false, "");
SETTING(bool, ShowMoveCursor, false, "");
SETTING(bool, DisableMouseEvents, false, "");
SETTING(bool, DisableKeyboardEvents, false, "");
SETTING(bool, HidePassword, true, "");
SETTING(string, PlayerOffAppendix, "_off", "");
SETTING(uint, Animation3dSmoothTime, 150, "");
SETTING(uint, Animation3dFPS, 30, "");
SETTING(int, RunModMul, 1, "");
SETTING(int, RunModDiv, 3, "");
SETTING(int, RunModAdd, 0, "");
SETTING(bool, MapZooming, false, "");
SETTING(float, SpritesZoom, 1.0f, "");
SETTING(float, SpritesZoomMax, MAX_ZOOM, "");
SETTING(float, SpritesZoomMin, MIN_ZOOM, "");
SETTING_ARR(float, EffectValues[EFFECT_SCRIPT_VALUES]); //{}, "");
SETTING(string, KeyboardRemap, "", "");
SETTING(uint, CritterFidgetTime, 50000, "");
SETTING(uint, Anim2CombatBegin, 0, "");
SETTING(uint, Anim2CombatIdle, 0, "");
SETTING(uint, Anim2CombatEnd, 0, "");
SETTING(uint, RainTick, 60, "");
SETTING(short, RainSpeedX, 0, "");
SETTING(short, RainSpeedY, 15, "");
SETTING(uint, ConsoleHistorySize, 20, "");
SETTING(int, SoundVolume, 100, "");
SETTING(int, MusicVolume, 100, "");
SETTING(uint, ChosenLightColor, 0, "");
SETTING(uchar, ChosenLightDistance, 4, "");
SETTING(int, ChosenLightIntensity, 2500, "");
SETTING(uchar, ChosenLightFlags, 0, "");

// Mapper
SETTING(string, ServerDir, "", "");
SETTING(bool, ShowCorners, false, "");
SETTING(bool, ShowSpriteCuts, false, "");
SETTING(bool, ShowDrawOrder, false, "");
SETTING(bool, SplitTilesCollection, true, "");

// Build platform
SETTING(bool, WebBuild, false, "");
SETTING(bool, WindowsBuild, false, "");
SETTING(bool, LinuxBuild, false, "");
SETTING(bool, MacOsBuild, false, "");
SETTING(bool, AndroidBuild, false, "");
SETTING(bool, IOsBuild, false, "");
SETTING(bool, DesktopBuild, false, "");
SETTING(bool, TabletBuild, false, "");

#undef SETTING
#undef SETTING_ARR
