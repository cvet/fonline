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
SETTING_GROUP(CommonSettings, virtual BaseSettings);
FIXED_SETTING(string, CommandLine); // Command line used to start the game (read only)
FIXED_SETTING(vector<string>, CommandLineArgs); // Command line arguments used to start the game (read only)
FIXED_SETTING(string, GameName, "FOnline"); // Game name, used in logs and window title
FIXED_SETTING(string, GameVersion, "0.0.0"); // Game version, used in logs and window title
FIXED_SETTING(string, GitBranch, ""); // Git branch name (if present)
FIXED_SETTING(string, UnpackagedSubConfig); // Config applied in unpackaged builds
FIXED_SETTING(int32, ScriptOverrunReportTime); // Time in milliseconds to report script overrun, 0 to disable
FIXED_SETTING(bool, DebugBuild); // If true, debug build is used, otherwise release build (read only)
FIXED_SETTING(bool, Packaged); // If yes, then the packaging was done (read only)
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(CommonGameplaySettings, virtual BaseSettings);
FIXED_SETTING(int32, MinNameLength, 4); // Minimum length for player name
FIXED_SETTING(int32, MaxNameLength, 12); // Maximum length for player name
FIXED_SETTING(uint32, LookChecks, 0); // Look checks
FIXED_SETTING(vector<int32>, LookDir, 0, 20, 40, 60, 60); // Look direction values
FIXED_SETTING(vector<int32>, LookSneakDir, 90, 60, 30, 0, 0); // Sneak look direction values
FIXED_SETTING(int32, LookMinimum, 6); // Minimum look value
FIXED_SETTING(bool, CritterBlockHex, false); // If true, critters block hexes
FIXED_SETTING(int32, MaxAddUnstackableItems, 10); // Maximum number of unstackable items to add
FIXED_SETTING(int32, MaxPathFindLength, 400); // Maximum pathfinding length
SETTING_GROUP_END();

///@ ExportSettings Server
SETTING_GROUP(ServerGameplaySettings, virtual CommonGameplaySettings);
FIXED_SETTING(int32, RegistrationTimeout, 5); // Registration timeout in seconds
FIXED_SETTING(int32, SneakDivider, 6); // Sneak divider value
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(NetworkSettings, virtual BaseSettings);
FIXED_SETTING(int32, ServerPort, 4000); // Server port number
FIXED_SETTING(int32, NetBufferSize, 4096); // Network buffer size
FIXED_SETTING(bool, NetDebugHashes, false); // Debug network hashes resolution
FIXED_SETTING(int32, UpdateFileSendSize, 1000000); // Update file send size
FIXED_SETTING(bool, SecuredWebSockets, false); // If true, secured WebSockets are enabled
FIXED_SETTING(bool, DisableTcpNagle, true); // If true, TCP Nagle algorithm is disabled
FIXED_SETTING(bool, DisableZlibCompression, false); // If true, Zlib compression is disabled
FIXED_SETTING(int32, ArtificalLags, 0); // Artificial lags in milliseconds
SETTING_GROUP_END();

///@ ExportSettings Server
SETTING_GROUP(ServerNetworkSettings, virtual NetworkSettings);
FIXED_SETTING(int32, ClientPingTime, 10000); // Client ping time in milliseconds
FIXED_SETTING(int32, InactivityDisconnectTime, 0); // Inactivity disconnect time in milliseconds
FIXED_SETTING(string, WssPrivateKey, ""); // WebSocket Secure private key
FIXED_SETTING(string, WssCertificate, ""); // WebSocket Secure certificate
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(ClientNetworkSettings, virtual NetworkSettings);
FIXED_SETTING(string, ServerHost, "localhost"); // Server host address
FIXED_SETTING(int32, PingPeriod, 2000); // Ping period in milliseconds
VARIABLE_SETTING(int32, ProxyType, 0); // Proxy type
VARIABLE_SETTING(string, ProxyHost, ""); // Proxy host address
VARIABLE_SETTING(int32, ProxyPort, 8080); // Proxy port number
VARIABLE_SETTING(string, ProxyUser, ""); // Proxy username
VARIABLE_SETTING(string, ProxyPass, ""); // Proxy password
VARIABLE_SETTING(int32, Ping); // Network ping (read only)
VARIABLE_SETTING(bool, DebugNet, false); // If true, network debugging is enabled
FIXED_SETTING(bool, BypassCompatibilityCheck, false); // If true, compatibility check is bypassed
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(AudioSettings, virtual BaseSettings);
VARIABLE_SETTING(bool, DisableAudio, false); // If true, audio is disabled
VARIABLE_SETTING(int32, SoundVolume, 100); // Sound volume percentage
VARIABLE_SETTING(int32, MusicVolume, 100); // Music volume percentage
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(ViewSettings, virtual BaseSettings);
VARIABLE_SETTING(int32, ScreenWidth, 1024); // Screen width in pixels
VARIABLE_SETTING(int32, ScreenHeight, 768); // Screen height in pixels
FIXED_SETTING(int32, MonitorWidth); // Monitor width (read only)
FIXED_SETTING(int32, MonitorHeight); // Monitor height (read only)
VARIABLE_SETTING(int32, ScreenHudHeight, 0); // Screen HUD height in pixels
VARIABLE_SETTING(bool, ShowCorners, false); // If true, corners are shown
VARIABLE_SETTING(bool, ShowSpriteBorders, false); // If true, sprite borders are shown
FIXED_SETTING(bool, HideNativeCursor, false); // If true, native cursor is hidden
FIXED_SETTING(int32, FadingDuration, 1000); // Fading duration in milliseconds
FIXED_SETTING(bool, MapZoomEnabled); // Map view zooming
FIXED_SETTING(bool, MapDirectDraw); // Draw map directly to main framebuffer (speed up rendering but disables zooming and make scrolling jumpy)
FIXED_SETTING(bool, DisableLighting); // Disables lighting for more performance
FIXED_SETTING(bool, DisableFog); // Disables fog for more performance
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(GeometrySettings, virtual BaseSettings);
FIXED_SETTING(bool, MapHexagonal); // Hexagonal map grid (read only)
FIXED_SETTING(bool, MapSquare); // Square map grid (read only)
FIXED_SETTING(int32, MapDirCount); // Directions count (read only)
FIXED_SETTING(int32, MapHexWidth, 32); // Hex/square width
FIXED_SETTING(int32, MapHexHeight, 16); // Hex/square height
FIXED_SETTING(int32, MapHexLineHeight, 12); // Hex/square line height
FIXED_SETTING(int32, MapTileStep, 2); // Tile step value
FIXED_SETTING(int32, MapTileOffsX, -8); // Tile default X offset
FIXED_SETTING(int32, MapTileOffsY, 32); // Tile default Y offset
FIXED_SETTING(int32, MapRoofOffsX, -8); // Roof default X offset
FIXED_SETTING(int32, MapRoofOffsY, -66); // Roof default Y offset
FIXED_SETTING(float32, MapCameraAngle, 25.6589f); // Angle for critters moving/rendering
FIXED_SETTING(bool, MapFreeMovement, false); // If true, free movement on the map is enabled
FIXED_SETTING(bool, MapSmoothPath, true); // Enable pathfinding path smoothing
FIXED_SETTING(string, MapDataPrefix, "Geometry"); // Path and prefix for names used for geometry sprites
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(RenderSettings, virtual ViewSettings, virtual GeometrySettings);
FIXED_SETTING(int32, Animation3dSmoothTime, 150); // 3D animation smooth time in milliseconds
FIXED_SETTING(int32, Animation3dFPS, 30); // 3D animation frames per second
FIXED_SETTING(string, HeadBone); // Head bone name (Todo: move HeadBone to fo3d settings)
FIXED_SETTING(vector<string>, LegBones); // Leg bone names (Todo: move LegBones to fo3d settings)
VARIABLE_SETTING(bool, WindowCentered, true); // If true, window is centered
VARIABLE_SETTING(bool, WindowResizable, false); // If true, window is resizable
VARIABLE_SETTING(bool, NullRenderer, false); // If true, null renderer is used
VARIABLE_SETTING(bool, ForceOpenGL, false); // If true, OpenGL is forced
VARIABLE_SETTING(bool, ForceDirect3D, false); // If true, Direct3D is forced
VARIABLE_SETTING(bool, ForceMetal, false); // If true, Metal is forced
VARIABLE_SETTING(bool, ForceGlslEsProfile, false); // If true, GLSL ES profile is forced
VARIABLE_SETTING(bool, RenderDebug, false); // If true, render debugging is enabled
VARIABLE_SETTING(bool, VSync, false); // If true, vertical synchronization is enabled
VARIABLE_SETTING(bool, AlwaysOnTop, false); // If true, window is always on top
VARIABLE_SETTING(vector<float32>, EffectValues, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0); // Effect values
VARIABLE_SETTING(bool, Fullscreen, false); // If true, fullscreen mode is enabled
VARIABLE_SETTING(int32, Brightness, 0); // Brightness value
VARIABLE_SETTING(int32, Sleep, -1); // Sleep duration in milliseconds (-1 to disable)
VARIABLE_SETTING(int32, FixedFPS, 100); // Fixed frames per second (0 to disable)
FIXED_SETTING(int32, FogExtraLength, 0); // Extra fog length
FIXED_SETTING(float32, CritterTurnAngle, 100.0f); // Critter turn angle
FIXED_SETTING(float32, CritterBodyTurnFactor, 0.6f); // Critter body turn factor
FIXED_SETTING(float32, CritterHeadTurnFactor, 0.4f); // Critter head turn factor
FIXED_SETTING(int32, DefaultModelViewWidth, 0); // Default model view width
FIXED_SETTING(int32, DefaultModelViewHeight, 0); // Default model view height
FIXED_SETTING(int32, DefaultModelDrawWidth, 128); // Default model draw width
FIXED_SETTING(int32, DefaultModelDrawHeight, 128); // Default model draw height
FIXED_SETTING(int32, WalkAnimBaseSpeed, 60); // Walk animation base speed
FIXED_SETTING(int32, RunAnimStartSpeed, 80); // Run animation start speed
FIXED_SETTING(int32, RunAnimBaseSpeed, 120); // Run animation base speed
FIXED_SETTING(float32, ModelProjFactor, 40.0f); // Model projection factor
FIXED_SETTING(bool, AtlasLinearFiltration, false); // If true, atlas linear filtration is enabled
FIXED_SETTING(int32, DefaultParticleDrawWidth, 128); // Default particle draw width
FIXED_SETTING(int32, DefaultParticleDrawHeight, 128); // Default particle draw height
FIXED_SETTING(bool, RecreateClientOnError, false); // If true, client is recreated on error
FIXED_SETTING(string, ImGuiColorStyle); // ImGui theme: Light, Classic, Dark
FIXED_SETTING(string, ImGuiDefaultEffect, "Effects/ImGui_Default.fofx"); // Shader effect for ImGui
FIXED_SETTING(int32, ImGuiFontTextureSize, 256); // Minimum ImGui texture size
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(TimerSettings, virtual BaseSettings);
FIXED_SETTING(int32, DebuggingDeltaTimeCap, 100); // Debugging delta time cap in milliseconds
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(BakingSettings, virtual BaseSettings);
FIXED_SETTING(bool, ForceBaking, false); // If true, baking of all packs are forced
FIXED_SETTING(bool, SingleThreadBaking, false); // If true, single-threaded baking is enabled
FIXED_SETTING(vector<string>, RawCopyFileExtensions, "fopts", "fofnt", "bmfc", "fnt", "acm", "ogg", "wav", "ogv", "json", "ini"); // Raw copy file extensions
FIXED_SETTING(vector<string>, ProtoFileExtensions, "fopro", "fomap"); // Proto file extensions
FIXED_SETTING(vector<string>, BakeLanguages, "engl"); // Bake languages
FIXED_SETTING(string, BakeOutput, "Baking"); // Bake output directory
FIXED_SETTING(string, ServerResources, "ServerResources"); // Server resources directory
FIXED_SETTING(string, ClientResources, "Resources"); // Client resources directory
FIXED_SETTING(string, CacheResources, "Cache"); // Cache resources directory
FIXED_SETTING(vector<string>, ServerResourceEntries); // Server resource entries (read only)
FIXED_SETTING(vector<string>, ClientResourceEntries); // Client resource entries (read only)
FIXED_SETTING(vector<string>, MapperResourceEntries); // Mapper resource entries (read only)
FIXED_SETTING(int32, EmbeddedBufSize, 1000000); // Embedded resources buffer size, need for preserve data in executable
FIXED_SETTING(int32, ZipCompressLevel, 1); // Zip deflate level (0-9)
SETTING_GROUP_END();

///@ ExportSettings Server
SETTING_GROUP(CritterSettings, virtual ServerGameplaySettings, virtual TimerSettings, virtual NetworkSettings, virtual GeometrySettings);
FIXED_SETTING(vector<bool>, CritterSlotEnabled, true, true); // Critter slot enabled flags
FIXED_SETTING(vector<bool>, CritterSlotSendData, false, true); // Critter slot send data flags
FIXED_SETTING(vector<bool>, CritterSlotMultiItem, true, false); // Critter slot multi-item flags
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(CritterViewSettings, virtual ViewSettings, virtual GeometrySettings, virtual TimerSettings);
FIXED_SETTING(int32, NameOffset, 0); // Name offset value
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(HexSettings, virtual ViewSettings, virtual GeometrySettings, virtual CritterViewSettings);
FIXED_SETTING(int32, ScrollFixedDt, 10); // Scroll fixed delta time in milliseconds
FIXED_SETTING(int32, ScrollSpeed, 1200); // Scroll speed in pixels per second
FIXED_SETTING(int32, ZoomSpeed, 100); // Speed of zooming
FIXED_SETTING(ucolor, ChosenLightColor, ucolor::clear); // Chosen light color
FIXED_SETTING(uint8, ChosenLightDistance, 4); // Chosen light distance
FIXED_SETTING(int32, ChosenLightIntensity, 20); // Chosen light intensity
FIXED_SETTING(uint8, ChosenLightFlags, 0); // Chosen light flags
VARIABLE_SETTING(bool, FullscreenMouseScroll, true); // If true, fullscreen mouse scroll is enabled
VARIABLE_SETTING(bool, WindowedMouseScroll, false); // If true, windowed mouse scroll is enabled
VARIABLE_SETTING(bool, ScrollCheck, true); // If true, scroll check is enabled
VARIABLE_SETTING(bool, ScrollKeybLeft, false); // Keyboard map scroll left (read only)
VARIABLE_SETTING(bool, ScrollKeybRight, false); // Keyboard map scroll right (read only)
VARIABLE_SETTING(bool, ScrollKeybUp, false); // Keyboard map scroll up (read only)
VARIABLE_SETTING(bool, ScrollKeybDown, false); // Keyboard map scroll down (read only)
VARIABLE_SETTING(bool, ScrollMouseLeft, false); // Mouse map scroll left (read only)
VARIABLE_SETTING(bool, ScrollMouseRight, false); // Mouse map scroll right (read only)
VARIABLE_SETTING(bool, ScrollMouseUp, false); // Mouse map scroll up (read only)
VARIABLE_SETTING(bool, ScrollMouseDown, false); // Mouse map scroll down (read only)
VARIABLE_SETTING(uint8, RoofAlpha, 200); // Roof alpha value
VARIABLE_SETTING(bool, ShowTile, true); // If true, tiles are shown
VARIABLE_SETTING(bool, ShowRoof, true); // If true, roofs are shown
VARIABLE_SETTING(bool, ShowItem, true); // If true, items are shown
VARIABLE_SETTING(bool, ShowScen, true); // If true, scenery is shown
VARIABLE_SETTING(bool, ShowWall, true); // If true, walls are shown
VARIABLE_SETTING(bool, ShowCrit, true); // If true, critters are shown
VARIABLE_SETTING(bool, ShowFast, true); // If true, fast mode is enabled
FIXED_SETTING(int32, ScrollBlockSize, 1); // Blocked hexes around scroll block line
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(PlatformSettings, virtual BaseSettings);
FIXED_SETTING(bool, WebBuild); // Web build (read only)
FIXED_SETTING(bool, WindowsBuild); // Windows build (read only)
FIXED_SETTING(bool, LinuxBuild); // Linux build (read only)
FIXED_SETTING(bool, MacOsBuild); // macOS build (read only)
FIXED_SETTING(bool, AndroidBuild); // Android build (read only)
FIXED_SETTING(bool, IOsBuild); // iOS build (read only)
FIXED_SETTING(bool, DesktopBuild); // Desktop build (read only)
FIXED_SETTING(bool, TabletBuild); // Tablet build (read only)
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(InputSettings, virtual BaseSettings);
VARIABLE_SETTING(int32, DoubleClickTime, 500); // Double-click time in milliseconds
VARIABLE_SETTING(int32, ConsoleHistorySize, 100); // Console history size
VARIABLE_SETTING(ipos32, MousePos); // Mouse position (read only)
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(MapperSettings, virtual BakingSettings);
FIXED_SETTING(string, StartMap, ""); // Start map name
VARIABLE_SETTING(int32, StartHexX, -1); // Start hex X coordinate
VARIABLE_SETTING(int32, StartHexY, -1); // Start hex Y coordinate
VARIABLE_SETTING(bool, SplitTilesCollection, true); // If true, tiles collection is split
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(ClientSettings, virtual CommonSettings, virtual BakingSettings, virtual CommonGameplaySettings, virtual ClientNetworkSettings, virtual AudioSettings, virtual ViewSettings, virtual RenderSettings, virtual GeometrySettings, virtual TimerSettings, virtual HexSettings, virtual PlatformSettings, virtual InputSettings, virtual CritterViewSettings, virtual MapperSettings);
FIXED_SETTING(int32, UpdaterInfoDelay, 1000); // Updater info delay in milliseconds
FIXED_SETTING(int32, UpdaterInfoPos, 0); // Updater info position (<1 - top, 0 - center, >1 - bottom)
FIXED_SETTING(string, DefaultSplash); // Default splash screen
FIXED_SETTING(string, DefaultSplashPack); // Default splash pack
VARIABLE_SETTING(string, Language, "engl"); // Language setting
VARIABLE_SETTING(bool, WinNotify, true); // If true, Windows notifications are enabled
VARIABLE_SETTING(bool, SoundNotify, false); // If true, sound notifications are enabled
VARIABLE_SETTING(bool, HelpInfo, false); // If true, help information is shown
SETTING_GROUP_END();

///@ ExportSettings Server
SETTING_GROUP(ServerSettings, virtual CommonSettings, virtual BakingSettings, virtual ServerNetworkSettings, virtual AudioSettings, virtual RenderSettings, virtual GeometrySettings, virtual PlatformSettings, virtual TimerSettings, virtual ServerGameplaySettings, virtual CritterSettings);
FIXED_SETTING(string, DbStorage, "Memory"); // Database storage type
FIXED_SETTING(bool, NoStart, false); // If true, server start is disabled
FIXED_SETTING(bool, CollapseLogOnStart, false); // If true, log is collapsed on start
FIXED_SETTING(int32, ServerSleep, -1); // Server sleep duration in milliseconds (-1 to disable)
FIXED_SETTING(int32, LoopsPerSecondCap, 1000); // Loops per second cap
FIXED_SETTING(int32, LockMaxWaitTime, 100); // Maximum lock wait time in milliseconds
FIXED_SETTING(int32, DataBaseCommitPeriod, 10); // Database commit period in seconds
FIXED_SETTING(int32, DataBaseMaxCommitJobs, 100); // Maximum database commit jobs
FIXED_SETTING(int32, LoopAverageTimeInterval, 1000); // Loop average time interval in milliseconds
FIXED_SETTING(bool, WriteHealthFile, false); // If true, health file is written
FIXED_SETTING(bool, ProtoMapStaticGrid, false); // If true, proto map static grid is enabled
FIXED_SETTING(bool, MapInstanceStaticGrid, false); // If true, map instance static grid is enabled
FIXED_SETTING(int64, EntityStartId, 10000000001); // Entity start ID
SETTING_GROUP_END();

#undef FIXED_SETTING
#undef VARIABLE_SETTING
#undef SETTING_GROUP
#undef SETTING_GROUP_END
