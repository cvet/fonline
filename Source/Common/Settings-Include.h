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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
FIXED_SETTING(vector<string>, ApplyConfig);
FIXED_SETTING(vector<string>, AppliedConfigs); // Auto
FIXED_SETTING(string, CommandLine); // Auto
FIXED_SETTING(vector<string>, CommandLineArgs); // Auto
FIXED_SETTING(string, GameName, "FOnline");
FIXED_SETTING(string, GameVersion, "0.0.0");
FIXED_SETTING(string, ApplySubConfig);
FIXED_SETTING(string, DebuggingSubConfig);
FIXED_SETTING(vector<int32>, DummyIntVec); // Auto
FIXED_SETTING(string, ImGuiColorStyle); // Light, Classic, Dark
FIXED_SETTING(uint32, ScriptOverrunReportTime, 100);
FIXED_SETTING(bool, DebugBuild, false); // Auto
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(DataSettings, virtual DummySettings);
FIXED_SETTING(string, BaseServerResourcesName, "ServerResources");
FIXED_SETTING(string, BaseClientResourcesName, "Resources");
FIXED_SETTING(string, ServerResources, "Baking");
FIXED_SETTING(string, ClientResources, "Baking");
FIXED_SETTING(vector<string>, ServerResourceEntries); // Auto
FIXED_SETTING(vector<string>, ClientResourceEntries); // Auto
FIXED_SETTING(vector<string>, MapperResourceEntries); // Auto
FIXED_SETTING(string, EmbeddedResources, "@Disabled");
FIXED_SETTING(bool, DataSynchronization, true);
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(CommonGameplaySettings, virtual DummySettings);
FIXED_SETTING(uint32, MinNameLength, 4);
FIXED_SETTING(uint32, MaxNameLength, 12);
FIXED_SETTING(uint32, TalkDistance, 3);
FIXED_SETTING(uint32, LookChecks, 0);
FIXED_SETTING(vector<uint32>, LookDir, 0, 20, 40, 60, 60);
FIXED_SETTING(vector<uint32>, LookSneakDir, 90, 60, 30, 0, 0);
FIXED_SETTING(uint32, LookMinimum, 6);
FIXED_SETTING(bool, CritterBlockHex, false);
FIXED_SETTING(uint32, MaxAddUnstackableItems, 10);
FIXED_SETTING(int32, MaxPathFindLength, 400);
SETTING_GROUP_END();

///@ ExportSettings Server
SETTING_GROUP(ServerGameplaySettings, virtual CommonGameplaySettings);
FIXED_SETTING(uint32, RegistrationTimeout, 5);
FIXED_SETTING(uint32, NpcMaxTalkers, 1);
FIXED_SETTING(uint32, DlgTalkMaxTime, 0);
FIXED_SETTING(uint32, DlgBarterMaxTime, 0);
FIXED_SETTING(bool, NoAnswerShuffle, false);
FIXED_SETTING(uint32, SneakDivider, 6);
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(NetworkSettings, virtual DummySettings);
FIXED_SETTING(uint32, ServerPort, 4000);
FIXED_SETTING(uint32, NetBufferSize, 4096);
FIXED_SETTING(uint32, UpdateFileSendSize, 1000000);
FIXED_SETTING(bool, SecuredWebSockets, false);
FIXED_SETTING(bool, DisableTcpNagle, true);
FIXED_SETTING(bool, DisableZlibCompression, false);
FIXED_SETTING(uint32, ArtificalLags, 0);
SETTING_GROUP_END();

///@ ExportSettings Server
SETTING_GROUP(ServerNetworkSettings, virtual NetworkSettings);
FIXED_SETTING(uint32, ClientPingTime, 10000);
FIXED_SETTING(uint32, InactivityDisconnectTime, 0);
FIXED_SETTING(string, WssPrivateKey, "");
FIXED_SETTING(string, WssCertificate, "");
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(ClientNetworkSettings, virtual NetworkSettings);
FIXED_SETTING(string, ServerHost, "localhost");
FIXED_SETTING(uint32, PingPeriod, 2000);
VARIABLE_SETTING(uint32, ProxyType, 0);
VARIABLE_SETTING(string, ProxyHost, "");
VARIABLE_SETTING(uint32, ProxyPort, 8080);
VARIABLE_SETTING(string, ProxyUser, "");
VARIABLE_SETTING(string, ProxyPass, "");
VARIABLE_SETTING(uint32, Ping); // Auto
VARIABLE_SETTING(bool, DebugNet, false);
FIXED_SETTING(bool, BypassCompatibilityCheck, false);
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(AudioSettings, virtual DummySettings);
VARIABLE_SETTING(bool, DisableAudio, false);
VARIABLE_SETTING(uint32, SoundVolume, 100);
VARIABLE_SETTING(uint32, MusicVolume, 100);
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(ViewSettings, virtual DummySettings);
VARIABLE_SETTING(int32, ScreenWidth, 1024);
VARIABLE_SETTING(int32, ScreenHeight, 768);
FIXED_SETTING(int32, MonitorWidth); // Auto
FIXED_SETTING(int32, MonitorHeight); // Auto
VARIABLE_SETTING(int32, ScreenHudHeight, 0);
VARIABLE_SETTING(ipos, ScreenOffset); // Auto
VARIABLE_SETTING(bool, ShowCorners, false);
VARIABLE_SETTING(bool, ShowDrawOrder, false);
VARIABLE_SETTING(bool, ShowSpriteBorders, false);
FIXED_SETTING(bool, HideNativeCursor, false);
FIXED_SETTING(uint32, FadingDuration, 1000);
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(GeometrySettings, virtual DummySettings);
FIXED_SETTING(bool, MapHexagonal); // Auto
FIXED_SETTING(bool, MapSquare); // Auto
FIXED_SETTING(int32, MapDirCount); // Auto
FIXED_SETTING(int32, MapHexWidth, 32); // Hex/square width
FIXED_SETTING(int32, MapHexHeight, 16); // Hex/square height
FIXED_SETTING(int32, MapHexLineHeight, 12); // Hex/square line height
FIXED_SETTING(int32, MapTileStep, 2);
FIXED_SETTING(int32, MapTileOffsX, -8); // Tile default offsets
FIXED_SETTING(int32, MapTileOffsY, 32); // Tile default offsets
FIXED_SETTING(int32, MapRoofOffsX, -8); // Roof default offsets
FIXED_SETTING(int32, MapRoofOffsY, -66); // Roof default offsets
FIXED_SETTING(float32, MapCameraAngle, 25.6589f); // Angle for critters moving/rendering
FIXED_SETTING(bool, MapFreeMovement, false);
FIXED_SETTING(bool, MapSmoothPath, true); // Enable pathfinding path smoothing
FIXED_SETTING(string, MapDataPrefix, "Geometry"); // Path and prefix for names used for geometry sprites
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(RenderSettings, virtual ViewSettings, virtual GeometrySettings);
FIXED_SETTING(uint32, Animation3dSmoothTime, 150);
FIXED_SETTING(uint32, Animation3dFPS, 30);
FIXED_SETTING(string, HeadBone); // Todo: move HeadBone to fo3d settings
FIXED_SETTING(vector<string>, LegBones); // Todo: move LegBones to fo3d settings
VARIABLE_SETTING(bool, WindowCentered, true);
VARIABLE_SETTING(bool, WindowResizable, false);
VARIABLE_SETTING(bool, NullRenderer, false);
VARIABLE_SETTING(bool, ForceOpenGL, false);
VARIABLE_SETTING(bool, ForceDirect3D, false);
VARIABLE_SETTING(bool, ForceMetal, false);
VARIABLE_SETTING(bool, ForceGlslEsProfile, false);
VARIABLE_SETTING(bool, RenderDebug, false);
VARIABLE_SETTING(bool, VSync, false);
VARIABLE_SETTING(bool, AlwaysOnTop, false);
VARIABLE_SETTING(vector<float32>, EffectValues, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
VARIABLE_SETTING(bool, Fullscreen, false);
VARIABLE_SETTING(int32, Brightness, 0);
VARIABLE_SETTING(int32, Sleep, -1); // -1 to disable, Sleep has priority over FixedFPS if both enabled
VARIABLE_SETTING(int32, FixedFPS, 100); // 0 to disable, Sleep has priority over FixedFPS if both enabled
FIXED_SETTING(int32, FogExtraLength, 0);
FIXED_SETTING(float32, CritterTurnAngle, 100.0f);
FIXED_SETTING(float32, CritterBodyTurnFactor, 0.6f);
FIXED_SETTING(float32, CritterHeadTurnFactor, 0.4f);
FIXED_SETTING(int32, DefaultModelViewWidth, 0);
FIXED_SETTING(int32, DefaultModelViewHeight, 0);
FIXED_SETTING(int32, DefaultModelDrawWidth, 128);
FIXED_SETTING(int32, DefaultModelDrawHeight, 128);
FIXED_SETTING(int32, WalkAnimBaseSpeed, 60);
FIXED_SETTING(int32, RunAnimStartSpeed, 80);
FIXED_SETTING(int32, RunAnimBaseSpeed, 120);
FIXED_SETTING(float32, ModelProjFactor, 40.0f);
FIXED_SETTING(bool, AtlasLinearFiltration, false);
FIXED_SETTING(int32, DefaultParticleDrawWidth, 128);
FIXED_SETTING(int32, DefaultParticleDrawHeight, 128);
FIXED_SETTING(bool, RecreateClientOnError, false);
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(TimerSettings, virtual DummySettings);
FIXED_SETTING(uint32, DebuggingDeltaTimeCap, 100);
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(BakerSettings, virtual DummySettings);
FIXED_SETTING(bool, ForceBaking, false);
FIXED_SETTING(bool, SingleThreadBaking, false);
FIXED_SETTING(int32, MaxBakeOrder, 10);
FIXED_SETTING(vector<string>, RawCopyFileExtensions, "fopts", "fofnt", "bmfc", "fnt", "acm", "ogg", "wav", "ogv", "json", "ini");
FIXED_SETTING(vector<string>, ProtoFileExtensions, "fopro", "fomap");
FIXED_SETTING(vector<string>, BakeLanguages, "engl");
FIXED_SETTING(string, BakeOutput);
SETTING_GROUP_END();

///@ ExportSettings Server
SETTING_GROUP(CritterSettings, virtual ServerGameplaySettings, virtual TimerSettings, virtual NetworkSettings, virtual GeometrySettings);
FIXED_SETTING(vector<bool>, CritterSlotEnabled, true, true);
FIXED_SETTING(vector<bool>, CritterSlotSendData, false, true);
FIXED_SETTING(vector<bool>, CritterSlotMultiItem, true, false);
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(CritterViewSettings, virtual ViewSettings, virtual GeometrySettings, virtual TimerSettings);
FIXED_SETTING(uint32, CritterFidgetTime, 50000);
FIXED_SETTING(CritterActionAnim, CombatAnimBegin, CritterActionAnim::None);
FIXED_SETTING(CritterActionAnim, CombatAnimIdle, CritterActionAnim::None);
FIXED_SETTING(CritterActionAnim, CombatAnimEnd, CritterActionAnim::None);
FIXED_SETTING(int32, NameOffset, 0);
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(HexSettings, virtual ViewSettings, virtual GeometrySettings, virtual CritterViewSettings);
FIXED_SETTING(float32, SpritesZoomMax, MAX_ZOOM);
FIXED_SETTING(float32, SpritesZoomMin, MIN_ZOOM);
FIXED_SETTING(uint32, ScrollDelay, 10);
FIXED_SETTING(int32, ScrollStep, 12);
FIXED_SETTING(uint32, RainTick, 60);
FIXED_SETTING(int16, RainSpeedX, 0);
FIXED_SETTING(int16, RainSpeedY, 15);
FIXED_SETTING(ucolor, ChosenLightColor, ucolor::clear);
FIXED_SETTING(uint8, ChosenLightDistance, 4);
FIXED_SETTING(int32, ChosenLightIntensity, 2500);
FIXED_SETTING(uint8, ChosenLightFlags, 0);
VARIABLE_SETTING(bool, FullscreenMouseScroll, true);
VARIABLE_SETTING(bool, WindowedMouseScroll, false);
VARIABLE_SETTING(bool, ScrollCheck, true);
VARIABLE_SETTING(bool, ScrollKeybLeft, false); // Auto
VARIABLE_SETTING(bool, ScrollKeybRight, false); // Auto
VARIABLE_SETTING(bool, ScrollKeybUp, false); // Auto
VARIABLE_SETTING(bool, ScrollKeybDown, false); // Auto
VARIABLE_SETTING(bool, ScrollMouseLeft, false); // Auto
VARIABLE_SETTING(bool, ScrollMouseRight, false); // Auto
VARIABLE_SETTING(bool, ScrollMouseUp, false); // Auto
VARIABLE_SETTING(bool, ScrollMouseDown, false); // Auto
VARIABLE_SETTING(uint8, RoofAlpha, 200);
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

///@ ExportSettings Common
SETTING_GROUP(PlatformSettings, virtual DummySettings);
FIXED_SETTING(bool, WebBuild); // Auto
FIXED_SETTING(bool, WindowsBuild); // Auto
FIXED_SETTING(bool, LinuxBuild); // Auto
FIXED_SETTING(bool, MacOsBuild); // Auto
FIXED_SETTING(bool, AndroidBuild); // Auto
FIXED_SETTING(bool, IOsBuild); // Auto
FIXED_SETTING(bool, DesktopBuild); // Auto
FIXED_SETTING(bool, TabletBuild); // Auto
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(InputSettings, virtual DummySettings);
VARIABLE_SETTING(uint32, DoubleClickTime, 500);
VARIABLE_SETTING(uint32, ConsoleHistorySize, 100);
VARIABLE_SETTING(ipos, MousePos); // Auto
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(MapperSettings, virtual DataSettings);
FIXED_SETTING(string, StartMap, "");
VARIABLE_SETTING(int32, StartHexX, -1);
VARIABLE_SETTING(int32, StartHexY, -1);
VARIABLE_SETTING(bool, SplitTilesCollection, true);
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(ClientSettings, virtual CommonSettings, virtual DataSettings, virtual CommonGameplaySettings, virtual ClientNetworkSettings, virtual AudioSettings, virtual ViewSettings, virtual RenderSettings, virtual GeometrySettings, virtual TimerSettings, virtual HexSettings, virtual PlatformSettings, virtual InputSettings, virtual CritterViewSettings, virtual MapperSettings);
FIXED_SETTING(uint32, UpdaterInfoDelay, 1000);
FIXED_SETTING(int32, UpdaterInfoPos, 0); // <1 - top, 0 - center, >1 - bottom
FIXED_SETTING(string, DefaultSplash);
FIXED_SETTING(string, DefaultSplashPack);
VARIABLE_SETTING(string, Language, "engl");
VARIABLE_SETTING(bool, WinNotify, true);
VARIABLE_SETTING(bool, SoundNotify, false);
VARIABLE_SETTING(bool, HelpInfo, false);
SETTING_GROUP_END();

///@ ExportSettings Server
SETTING_GROUP(ServerSettings, virtual CommonSettings, virtual DataSettings, virtual ServerNetworkSettings, virtual AudioSettings, virtual RenderSettings, virtual GeometrySettings, virtual PlatformSettings, virtual TimerSettings, virtual ServerGameplaySettings, virtual CritterSettings);
FIXED_SETTING(vector<string>, AccessAdmin);
FIXED_SETTING(vector<string>, AccessClient);
FIXED_SETTING(vector<string>, AccessModer);
FIXED_SETTING(vector<string>, AccessTester);
FIXED_SETTING(uint32, AdminPanelPort);
FIXED_SETTING(string, DbStorage, "Memory");
FIXED_SETTING(bool, NoStart, false);
FIXED_SETTING(bool, CollapseLogOnStart, false);
FIXED_SETTING(int32, ServerSleep, -1);
FIXED_SETTING(int32, LoopsPerSecondCap, 1000);
FIXED_SETTING(uint32, LockMaxWaitTime, 100);
FIXED_SETTING(uint32, DataBaseCommitPeriod, 10);
FIXED_SETTING(uint32, DataBaseMaxCommitJobs, 100);
FIXED_SETTING(uint32, LoopAverageTimeInterval, 1000);
FIXED_SETTING(bool, WriteHealthFile, false);
FIXED_SETTING(bool, ProtoMapStaticGrid, false);
FIXED_SETTING(bool, MapInstanceStaticGrid, false);
FIXED_SETTING(int64, EntityStartId, 10000000001);
SETTING_GROUP_END();

#undef FIXED_SETTING
#undef VARIABLE_SETTING
#undef SETTING_GROUP
#undef SETTING_GROUP_END
