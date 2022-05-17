//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#if !defined(FIXED_SETTING) || !defined(VARIABLE_SETTING) || !defined(SETTING_GROUP) || !defined(SETTING_GROUP_END)
#error SETTING or VAR_SETTING or SETTING_GROUP or SETTING_GROUP_END not defined
#endif

///@ ExportSettings Common
SETTING_GROUP(CommonSettings, virtual DummySettings);
FIXED_SETTING(string, CommandLine, "");
FIXED_SETTING(vector<string>, CommandLineArgs);
VARIABLE_SETTING(bool, Quit, false); // Todo: rework global Quit setting
FIXED_SETTING(vector<int>, DummyIntVec);
FIXED_SETTING(string, ImGuiColorStyle, "Light"); // Light, Classic, Dark
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(FileSystemSettings, virtual DummySettings);
FIXED_SETTING(string, ResourcesDir, "Resources");
FIXED_SETTING(vector<string>, ResourceEntries);
FIXED_SETTING(string, EmbeddedResources, "$Embedded");
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(CommonGameplaySettings, virtual DummySettings);
FIXED_SETTING(uint, MinNameLength, 4);
FIXED_SETTING(uint, MaxNameLength, 12);
FIXED_SETTING(vector<string>, Languages, "engl");
FIXED_SETTING(uint, TalkDistance, 3);
FIXED_SETTING(bool, RunOnCombat, false);
FIXED_SETTING(bool, RunOnTransfer, false);
FIXED_SETTING(uint, TimeoutTransfer, 3);
FIXED_SETTING(uint, TimeoutBattle, 10);
FIXED_SETTING(uint, GlobalMapWidth, 28);
FIXED_SETTING(uint, GlobalMapHeight, 30);
FIXED_SETTING(uint, GlobalMapZoneLength, 50);
FIXED_SETTING(uint, LookChecks, 0);
FIXED_SETTING(vector<uint>, LookDir, 0, 20, 40, 60, 60);
FIXED_SETTING(vector<uint>, LookSneakDir, 90, 60, 30, 0, 0);
FIXED_SETTING(uint, LookMinimum, 6);
SETTING_GROUP_END();

///@ ExportSettings Server
SETTING_GROUP(ServerGameplaySettings, virtual CommonGameplaySettings)
FIXED_SETTING(uint, MinimumOfflineTime, 180000);
FIXED_SETTING(uint, RegistrationTimeout, 5);
FIXED_SETTING(uint, NpcMaxTalkers, 1);
FIXED_SETTING(uint, DlgTalkMinTime, 0);
FIXED_SETTING(uint, DlgBarterMinTime, 0);
FIXED_SETTING(uint, WhisperDist, 2);
FIXED_SETTING(uint, ShoutDist, 200);
FIXED_SETTING(bool, NoAnswerShuffle, false);
FIXED_SETTING(uint, SneakDivider, 6);
FIXED_SETTING(int, DeadHitPoints, -6);
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(NetworkSettings, virtual DummySettings);
FIXED_SETTING(uint, ServerPort, 4000);
FIXED_SETTING(bool, SecuredWebSockets, false);
FIXED_SETTING(bool, DisableTcpNagle, true);
FIXED_SETTING(bool, DisableZlibCompression, false);
FIXED_SETTING(uint, FloodSize, 2048);
SETTING_GROUP_END();

///@ ExportSettings Server
SETTING_GROUP(ServerNetworkSettings, virtual NetworkSettings);
FIXED_SETTING(string, WssPrivateKey, "");
FIXED_SETTING(string, WssCertificate, "");
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(ClientNetworkSettings, virtual NetworkSettings);
FIXED_SETTING(string, ServerHost, "localhost");
FIXED_SETTING(uint, PingPeriod, 2000);
VARIABLE_SETTING(uint, ProxyType, 0);
VARIABLE_SETTING(string, ProxyHost, "");
VARIABLE_SETTING(uint, ProxyPort, 8080);
VARIABLE_SETTING(string, ProxyUser, "");
VARIABLE_SETTING(string, ProxyPass, "");
VARIABLE_SETTING(uint, Ping, 0);
VARIABLE_SETTING(bool, DebugNet, false);
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(AudioSettings, virtual DummySettings);
VARIABLE_SETTING(bool, DisableAudio, false);
VARIABLE_SETTING(uint, SoundVolume, 100);
VARIABLE_SETTING(uint, MusicVolume, 100);
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(ViewSettings, virtual DummySettings);
VARIABLE_SETTING(int, ScreenWidth, 1024);
VARIABLE_SETTING(int, ScreenHeight, 768);
VARIABLE_SETTING(float, SpritesZoom, 1.0f);
VARIABLE_SETTING(int, ScrOx, 0);
VARIABLE_SETTING(int, ScrOy, 0);
VARIABLE_SETTING(bool, ShowCorners, false);
VARIABLE_SETTING(bool, ShowDrawOrder, false);
VARIABLE_SETTING(bool, ShowSpriteBorders, false);
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(GeometrySettings, virtual DummySettings);
FIXED_SETTING(bool, MapHexagonal, true); // true - hexagonal, false - square
FIXED_SETTING(int, MapDirCount);
FIXED_SETTING(int, MapHexWidth, 32); // hex/square width
FIXED_SETTING(int, MapHexHeight, 16); // hex/square height
FIXED_SETTING(int, MapHexLineHeight, 12); // hex/square line height
FIXED_SETTING(int, MapTileOffsX, -8); // tile default offsets
FIXED_SETTING(int, MapTileOffsY, 32); // tile default offsets
FIXED_SETTING(int, MapTileStep, 2);
FIXED_SETTING(int, MapRoofOffsX, -8); // roof default offsets
FIXED_SETTING(int, MapRoofOffsY, -66); // roof default offsets
FIXED_SETTING(int, MapRoofSkipSize, 2); // default length (in hexes/squares) of roof tiles
FIXED_SETTING(float, MapCameraAngle, 21.671f); // angle for 3d critters rendering
FIXED_SETTING(bool, MapSmoothPath, true); // enable pathfinding path smoothing
FIXED_SETTING(string, MapDataPrefix, "art/geometry/fallout_"); // path and prefix for names used for geometry sprites
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(RenderSettings, virtual ViewSettings, virtual GeometrySettings);
FIXED_SETTING(string, WindowName, "FOnline");
VARIABLE_SETTING(bool, WindowCentered, true);
VARIABLE_SETTING(bool, NullRenderer, false);
VARIABLE_SETTING(bool, ForceOpenGL, false);
VARIABLE_SETTING(bool, ForceDirect3D, false);
VARIABLE_SETTING(bool, ForceMetal, false);
VARIABLE_SETTING(bool, ForceGNM, false);
VARIABLE_SETTING(bool, RenderDebug, false);
FIXED_SETTING(uint, Animation3dSmoothTime, 150);
FIXED_SETTING(uint, Animation3dFPS, 30);
VARIABLE_SETTING(bool, VSync, false);
VARIABLE_SETTING(bool, AlwaysOnTop, false);
VARIABLE_SETTING(vector<float>, EffectValues, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
VARIABLE_SETTING(bool, FullScreen, false);
VARIABLE_SETTING(int, Brightness, 20);
VARIABLE_SETTING(uint, FPS, 0);
VARIABLE_SETTING(int, FixedFPS, 100);
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(TimerSettings, virtual DummySettings);
FIXED_SETTING(int, StartYear, 2000);
SETTING_GROUP_END();

SETTING_GROUP(ScriptSettings, virtual TimerSettings);
FIXED_SETTING(string, ASServer);
FIXED_SETTING(string, ASClient);
FIXED_SETTING(string, ASSingle);
FIXED_SETTING(string, ASMapper);
SETTING_GROUP_END();

SETTING_GROUP(BakerSettings, virtual DummySettings);
VARIABLE_SETTING(vector<string>, BakeResourceEntries);
VARIABLE_SETTING(vector<string>, BakeContentEntries);
VARIABLE_SETTING(vector<string>, BakeExtraFileExtensions, "fofnt", "bmfc", "fnt", "acm", "ogg", "wav", "ogv");
SETTING_GROUP_END();

///@ ExportSettings Server
SETTING_GROUP(CritterSettings, virtual ServerGameplaySettings, virtual TimerSettings, virtual NetworkSettings, virtual GeometrySettings);
FIXED_SETTING(vector<bool>, CritterSlotEnabled, true, true, true);
FIXED_SETTING(vector<bool>, CritterSlotSendData, true, false, true);
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(CritterViewSettings, virtual ViewSettings, virtual GeometrySettings, virtual TimerSettings);
FIXED_SETTING(uint, CritterFidgetTime, 50000);
FIXED_SETTING(uint, Anim2CombatBegin, 0);
FIXED_SETTING(uint, Anim2CombatIdle, 0);
FIXED_SETTING(uint, Anim2CombatEnd, 0);
FIXED_SETTING(string, PlayerOffAppendix, "_off");
VARIABLE_SETTING(bool, ShowPlayerNames, false);
VARIABLE_SETTING(bool, ShowNpcNames, false);
VARIABLE_SETTING(bool, ShowCritId, false);
VARIABLE_SETTING(bool, ShowGroups, true);
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(HexSettings, virtual ViewSettings, virtual GeometrySettings, virtual CritterViewSettings);
FIXED_SETTING(float, SpritesZoomMax, MAX_ZOOM);
FIXED_SETTING(float, SpritesZoomMin, MIN_ZOOM);
FIXED_SETTING(uint, ScrollDelay, 10);
FIXED_SETTING(int, ScrollStep, 12);
FIXED_SETTING(uint, RainTick, 60);
FIXED_SETTING(short, RainSpeedX, 0);
FIXED_SETTING(short, RainSpeedY, 15);
FIXED_SETTING(uint, ChosenLightColor, 0);
FIXED_SETTING(uchar, ChosenLightDistance, 4);
FIXED_SETTING(int, ChosenLightIntensity, 2500);
FIXED_SETTING(uchar, ChosenLightFlags, 0);
VARIABLE_SETTING(bool, MouseScroll, true);
VARIABLE_SETTING(bool, ScrollCheck, true);
VARIABLE_SETTING(bool, ScrollKeybLeft, false);
VARIABLE_SETTING(bool, ScrollKeybRight, false);
VARIABLE_SETTING(bool, ScrollKeybUp, false);
VARIABLE_SETTING(bool, ScrollKeybDown, false);
VARIABLE_SETTING(bool, ScrollMouseLeft, false);
VARIABLE_SETTING(bool, ScrollMouseRight, false);
VARIABLE_SETTING(bool, ScrollMouseUp, false);
VARIABLE_SETTING(bool, ScrollMouseDown, false);
VARIABLE_SETTING(uchar, RoofAlpha, 200);
VARIABLE_SETTING(bool, ShowTile, true);
VARIABLE_SETTING(bool, ShowRoof, true);
VARIABLE_SETTING(bool, ShowItem, true);
VARIABLE_SETTING(bool, ShowScen, true);
VARIABLE_SETTING(bool, ShowWall, true);
VARIABLE_SETTING(bool, ShowCrit, true);
VARIABLE_SETTING(bool, ShowFast, true);
VARIABLE_SETTING(bool, HideCursor, false);
VARIABLE_SETTING(bool, ShowMoveCursor, false);
SETTING_GROUP_END();

///@ ExportSettings Common Auto
SETTING_GROUP(PlatformSettings, virtual DummySettings);
FIXED_SETTING(bool, WebBuild);
FIXED_SETTING(bool, WindowsBuild);
FIXED_SETTING(bool, LinuxBuild);
FIXED_SETTING(bool, MacOsBuild);
FIXED_SETTING(bool, AndroidBuild);
FIXED_SETTING(bool, IOsBuild);
FIXED_SETTING(bool, DesktopBuild);
FIXED_SETTING(bool, TabletBuild);
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(InputSettings, virtual DummySettings);
VARIABLE_SETTING(uint, DoubleClickTime, 500);
VARIABLE_SETTING(uint, ConsoleHistorySize, 100);
VARIABLE_SETTING(bool, DisableMouseEvents, false);
VARIABLE_SETTING(bool, DisableKeyboardEvents, false);
VARIABLE_SETTING(int, MouseX, 0);
VARIABLE_SETTING(int, MouseY, 0);
SETTING_GROUP_END();

///@ ExportSettings Mapper
SETTING_GROUP(MapperSettings, virtual DummySettings);
FIXED_SETTING(string, MapsDir, "");
FIXED_SETTING(string, StartMap, "");
VARIABLE_SETTING(int, StartHexX, -1);
VARIABLE_SETTING(int, StartHexY, -1);
VARIABLE_SETTING(bool, SplitTilesCollection, true);
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(ClientSettings, virtual CommonSettings, virtual FileSystemSettings, virtual CommonGameplaySettings, virtual ClientNetworkSettings, virtual ScriptSettings, virtual AudioSettings, virtual ViewSettings, virtual RenderSettings, virtual GeometrySettings, virtual TimerSettings, virtual HexSettings, virtual PlatformSettings, virtual InputSettings, virtual CritterViewSettings, virtual MapperSettings);
FIXED_SETTING(string, AutoLogin, "");
FIXED_SETTING(uint, TextDelay, 3000);
VARIABLE_SETTING(string, Language, "engl");
VARIABLE_SETTING(bool, WinNotify, true);
VARIABLE_SETTING(bool, SoundNotify, false);
VARIABLE_SETTING(bool, HelpInfo, false);
SETTING_GROUP_END();

///@ ExportSettings Server
SETTING_GROUP(ServerSettings, virtual CommonSettings, virtual FileSystemSettings, virtual ServerNetworkSettings, virtual ScriptSettings, virtual AudioSettings, virtual RenderSettings, virtual GeometrySettings, virtual PlatformSettings, virtual TimerSettings, virtual ServerGameplaySettings, virtual CritterSettings);
FIXED_SETTING(uint, AdminPanelPort, 0);
FIXED_SETTING(string, DbStorage, "Memory");
FIXED_SETTING(string, DbHistory, "None");
FIXED_SETTING(bool, NoStart, false);
FIXED_SETTING(int, GameSleep, 0);
SETTING_GROUP_END();

#undef FIXED_SETTING
#undef VARIABLE_SETTING
#undef SETTING_GROUP
#undef SETTING_GROUP_END
