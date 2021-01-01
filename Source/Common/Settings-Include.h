//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

// ReSharper disable CppMissingIncludeGuard

#if !defined(SETTING) || !defined(VAR_SETTING) || !defined(SETTING_GROUP) || !defined(SETTING_GROUP_END)
#error SETTING or VAR_SETTING or SETTING_GROUP or SETTING_GROUP_END not defined
#endif

SETTING_GROUP(CommonSettings, virtual DummySettings);
SETTING(string, WorkDir, "");
SETTING(string, CommandLine, "");
SETTING(vector<string>, CommandLineArgs);
SETTING(uint, MinNameLength, 4);
SETTING(uint, MaxNameLength, 12);
SETTING(uint, Breaktime, 1200);
SETTING(string, Language, "engl");
VAR_SETTING(bool, Quit, false); // Todo: rework global Quit setting
SETTING_GROUP_END();

SETTING_GROUP(CommonGameplaySettings, virtual DummySettings);
SETTING(uint, TalkDistance, 3);
SETTING(bool, RunOnCombat, false);
SETTING(bool, RunOnTransfer, false);
SETTING(uint, TimeoutTransfer, 3);
SETTING(uint, TimeoutBattle, 10);
SETTING(uint, GlobalMapWidth, 28);
SETTING(uint, GlobalMapHeight, 30);
SETTING(uint, GlobalMapZoneLength, 50);
SETTING(uint, LookChecks, 0);
SETTING(vector<uint>, LookDir, 0, 20, 40, 60, 60);
SETTING(vector<uint>, LookSneakDir, 90, 60, 30, 0, 0);
SETTING_GROUP_END();

SETTING_GROUP(ServerGameplaySettings, virtual CommonGameplaySettings)
SETTING(uint, MinimumOfflineTime, 180000);
SETTING(uint, RegistrationTimeout, 5);
SETTING(uint, NpcMaxTalkers, 1);
SETTING(uint, DlgTalkMinTime, 0);
SETTING(uint, DlgBarterMinTime, 0);
SETTING(uint, WhisperDist, 2);
SETTING(uint, ShoutDist, 200);
SETTING(bool, NoAnswerShuffle, false);
SETTING(uint, SneakDivider, 6);
SETTING(uint, LookMinimum, 6);
SETTING(int, DeadHitPoints, -6);
SETTING_GROUP_END();

SETTING_GROUP(NetworkSettings, virtual DummySettings);
SETTING(uint, Port, 4000);
SETTING(bool, SecuredWebSockets, false);
SETTING(bool, DisableTcpNagle, true);
SETTING(bool, DisableZlibCompression, false);
SETTING(uint, FloodSize, 2048);
SETTING_GROUP_END();

SETTING_GROUP(ServerNetworkSettings, virtual NetworkSettings);
SETTING(string, WssPrivateKey, "");
SETTING(string, WssCertificate, "");
SETTING_GROUP_END();

SETTING_GROUP(ClientNetworkSettings, virtual NetworkSettings);
SETTING(string, Host, "localhost");
SETTING(uint, ProxyType, 0);
SETTING(string, ProxyHost, "");
SETTING(uint, ProxyPort, 8080);
SETTING(string, ProxyUser, "");
SETTING(string, ProxyPass, "");
SETTING(uint, PingPeriod, 2000);
VAR_SETTING(bool, WaitPing, false);
VAR_SETTING(uint, Ping, 0);
VAR_SETTING(bool, DebugNet, false);
SETTING_GROUP_END();

SETTING_GROUP(AudioSettings, virtual DummySettings);
SETTING(bool, DisableAudio, false);
VAR_SETTING(uint, SoundVolume, 100);
VAR_SETTING(uint, MusicVolume, 100);
SETTING_GROUP_END();

SETTING_GROUP(ViewSettings, virtual DummySettings);
VAR_SETTING(int, ScreenWidth, 1024);
VAR_SETTING(int, ScreenHeight, 768);
VAR_SETTING(float, SpritesZoom, 1.0f);
VAR_SETTING(int, ScrOx, 0);
VAR_SETTING(int, ScrOy, 0);
VAR_SETTING(bool, ShowCorners, false);
VAR_SETTING(bool, ShowDrawOrder, false);
VAR_SETTING(bool, ShowSpriteBorders, false);
SETTING_GROUP_END();

SETTING_GROUP(EffectSettings, virtual DummySettings);
VAR_SETTING(vector<float>, EffectValues, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
SETTING_GROUP_END();

SETTING_GROUP(GeometrySettings, virtual DummySettings);
SETTING(bool, MapHexagonal, true);
SETTING(uint, MapDirCount);
SETTING(int, MapHexWidth, 32);
SETTING(int, MapHexHeight, 16);
SETTING(int, MapHexLineHeight, 12);
SETTING(int, MapTileOffsX, -8);
SETTING(int, MapTileOffsY, 32);
SETTING(uint, MapTileStep, 2);
SETTING(int, MapRoofOffsX, -8);
SETTING(int, MapRoofOffsY, -66);
SETTING(int, MapRoofSkipSize, 2);
SETTING(float, MapCameraAngle, 25.7f);
SETTING(bool, MapSmoothPath, true);
SETTING(string, MapDataPrefix, "art/geometry/fallout_");
SETTING_GROUP_END();

SETTING_GROUP(RenderSettings, virtual ViewSettings, virtual EffectSettings, virtual GeometrySettings);
SETTING(string, WindowName, "FOnline");
SETTING(bool, WindowCentered, true);
SETTING(bool, ForceOpenGL, false);
SETTING(bool, ForceDirect3D, false);
SETTING(bool, ForceMetal, false);
SETTING(bool, ForceGnm, false);
SETTING(bool, RenderDebug, false);
SETTING(uint, Animation3dSmoothTime, 150);
SETTING(uint, Animation3dFPS, 30);
SETTING(bool, Enable3dRendering, false);
SETTING(bool, VSync, false);
SETTING(bool, AlwaysOnTop, false);
VAR_SETTING(vector<float>, EffectValues, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
VAR_SETTING(bool, FullScreen, false);
VAR_SETTING(int, Brightness, 20);
VAR_SETTING(uint, FPS, 0);
SETTING(int, FixedFPS, 100);
SETTING_GROUP_END();

SETTING_GROUP(TimerSettings, virtual DummySettings);
SETTING(int, StartYear, 2000);
SETTING_GROUP_END();

SETTING_GROUP(ScriptSettings, virtual TimerSettings);
SETTING(string, ASServer);
SETTING(string, ASClient);
SETTING(string, ASMapper);
SETTING_GROUP_END();

SETTING_GROUP(BakerSettings, virtual DummySettings);
SETTING(string, ResourcesOutput, ".");
SETTING(vector<string>, ContentEntry);
SETTING(vector<string>, ResourcesEntry);
SETTING_GROUP_END();

SETTING_GROUP(MapSettings, virtual ServerGameplaySettings, virtual GeometrySettings);
SETTING_GROUP_END();

SETTING_GROUP(CritterSettings, virtual ServerGameplaySettings, virtual TimerSettings, virtual NetworkSettings, virtual GeometrySettings);
SETTING(vector<bool>, CritterSlotEnabled, true, true, true);
SETTING(vector<bool>, CritterSlotSendData, true, false, true);
SETTING_GROUP_END();

SETTING_GROUP(CritterViewSettings, virtual ViewSettings, virtual GeometrySettings, virtual TimerSettings);
SETTING(uint, CritterFidgetTime, 50000);
SETTING(uint, Anim2CombatBegin, 0);
SETTING(uint, Anim2CombatIdle, 0);
SETTING(uint, Anim2CombatEnd, 0);
SETTING(string, PlayerOffAppendix, "_off");
SETTING(bool, ShowPlayerNames, false);
SETTING(bool, ShowNpcNames, false);
SETTING(bool, ShowCritId, false);
SETTING(bool, ShowGroups, true);
SETTING_GROUP_END();

SETTING_GROUP(HexSettings, virtual ViewSettings, virtual GeometrySettings, virtual CritterViewSettings);
SETTING(float, SpritesZoomMax, MAX_ZOOM);
SETTING(float, SpritesZoomMin, MIN_ZOOM);
SETTING(uint, ScrollDelay, 10);
SETTING(int, ScrollStep, 12);
SETTING(uint, RainTick, 60);
SETTING(short, RainSpeedX, 0);
SETTING(short, RainSpeedY, 15);
SETTING(uint, ChosenLightColor, 0);
SETTING(uchar, ChosenLightDistance, 4);
SETTING(int, ChosenLightIntensity, 2500);
SETTING(uchar, ChosenLightFlags, 0);
VAR_SETTING(bool, MouseScroll, true);
VAR_SETTING(bool, ScrollCheck, true);
VAR_SETTING(bool, ScrollKeybLeft, false);
VAR_SETTING(bool, ScrollKeybRight, false);
VAR_SETTING(bool, ScrollKeybUp, false);
VAR_SETTING(bool, ScrollKeybDown, false);
VAR_SETTING(bool, ScrollMouseLeft, false);
VAR_SETTING(bool, ScrollMouseRight, false);
VAR_SETTING(bool, ScrollMouseUp, false);
VAR_SETTING(bool, ScrollMouseDown, false);
VAR_SETTING(uchar, RoofAlpha, 200);
VAR_SETTING(bool, ShowTile, true);
VAR_SETTING(bool, ShowRoof, true);
VAR_SETTING(bool, ShowItem, true);
VAR_SETTING(bool, ShowScen, true);
VAR_SETTING(bool, ShowWall, true);
VAR_SETTING(bool, ShowCrit, true);
VAR_SETTING(bool, ShowFast, true);
VAR_SETTING(bool, HideCursor, false);
VAR_SETTING(bool, ShowMoveCursor, false);
SETTING_GROUP_END();

SETTING_GROUP(PlatformSettings, virtual DummySettings);
SETTING(bool, WebBuild);
SETTING(bool, WindowsBuild);
SETTING(bool, LinuxBuild);
SETTING(bool, MacOsBuild);
SETTING(bool, AndroidBuild);
SETTING(bool, IOsBuild);
SETTING(bool, DesktopBuild);
SETTING(bool, TabletBuild);
SETTING_GROUP_END();

SETTING_GROUP(InputSettings, virtual DummySettings);
SETTING(uint, DoubleClickTime, 500);
SETTING(uint, ConsoleHistorySize, 100);
VAR_SETTING(bool, DisableMouseEvents, false);
VAR_SETTING(bool, DisableKeyboardEvents, false);
VAR_SETTING(int, MouseX, 0);
VAR_SETTING(int, MouseY, 0);
SETTING_GROUP_END();

SETTING_GROUP(ClientSettings, virtual CommonSettings, virtual CommonGameplaySettings, virtual ClientNetworkSettings, virtual ScriptSettings, virtual AudioSettings, virtual ViewSettings, virtual RenderSettings, virtual GeometrySettings, virtual TimerSettings, virtual HexSettings, virtual PlatformSettings, virtual InputSettings, virtual CritterViewSettings);
SETTING(string, AutoLogin, "");
SETTING(uint, TextDelay, 3000);
SETTING(bool, WinNotify, true);
SETTING(bool, SoundNotify, false);
VAR_SETTING(bool, HelpInfo, false);
SETTING_GROUP_END();

SETTING_GROUP(MapperSettings, virtual CommonSettings, virtual ScriptSettings, virtual ViewSettings, virtual RenderSettings, virtual GeometrySettings, virtual TimerSettings, virtual HexSettings, virtual PlatformSettings, virtual InputSettings, virtual CritterSettings, virtual CritterViewSettings);
SETTING(string, ServerDir, "");
SETTING(string, StartMap, "");
SETTING(int, StartHexX, -1);
SETTING(int, StartHexY, -1);
SETTING(bool, SplitTilesCollection, true);
SETTING_GROUP_END();

SETTING_GROUP(ServerSettings, virtual CommonSettings, virtual ServerNetworkSettings, virtual ScriptSettings, virtual AudioSettings, virtual RenderSettings, virtual GeometrySettings, virtual PlatformSettings, virtual TimerSettings, virtual ServerGameplaySettings, virtual MapSettings, virtual CritterSettings);
SETTING(uint, AdminPanelPort, 0);
SETTING(string, DbStorage, "Memory");
SETTING(string, DbHistory, "None");
SETTING(bool, NoStart, false);
SETTING(int, GameSleep, 0);
SETTING_GROUP_END();

#undef SETTING
#undef VAR_SETTING
#undef SETTING_GROUP
#undef SETTING_GROUP_END
