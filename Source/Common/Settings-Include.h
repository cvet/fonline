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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
FIXED_SETTING(string, Common, CommandLine); // Command line used to start the game (read only)
FIXED_SETTING(vector<string>, Common, CommandLineArgs); // Command line arguments used to start the game (read only)
FIXED_SETTING(string, Common, GameName, "FOnline"); // Game name, used in logs and window title
FIXED_SETTING(string, Common, GameVersion, "0.0.0"); // Game version, used in logs and window title
FIXED_SETTING(string, Common, GitBranch); // Git branch name (if present)
FIXED_SETTING(string, Common, GitCommit); // Git commit hash (if present)
FIXED_SETTING(string, Common, UnpackagedSubConfig); // Config applied in unpackaged builds
FIXED_SETTING(bool, Common, DebugBuild); // If true, debug build is used, otherwise release build (read only)
FIXED_SETTING(bool, Common, Packaged); // If yes, then the packaging was done (read only)
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(ScriptSettings, virtual BaseSettings);
FIXED_SETTING(int32, Script, OverrunReportTime); // Time in milliseconds to report script overrun, 0 to disable
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(NetworkSettings, virtual BaseSettings);
FIXED_SETTING(int32, Network, ServerPort, 4000); // Server port number
FIXED_SETTING(bool, Network, DebuggerEnabled, false); // If true, AngelScript debugger endpoint is enabled
FIXED_SETTING(string, Network, DebuggerBindHost, "0.0.0.0"); // Debugger TCP bind host
FIXED_SETTING(int32, Network, NetBufferSize, 4096); // Network buffer size
FIXED_SETTING(bool, Network, NetDebugHashes, false); // Debug network hashes resolution
FIXED_SETTING(int32, Network, UpdateFileSendSize, 1000000); // Update file send size
FIXED_SETTING(bool, Network, SecuredWebSockets, false); // If true, secured WebSockets are enabled
FIXED_SETTING(bool, Network, DisableTcpNagle, true); // If true, TCP Nagle algorithm is disabled
FIXED_SETTING(bool, Network, DisableZlibCompression, false); // If true, Zlib compression is disabled
FIXED_SETTING(int32, Network, ArtificalLags, 0); // Artificial lags in milliseconds
FIXED_SETTING(string, Network, CompatibilityVersion); // Compatibility version (read only)
FIXED_SETTING(string, Network, ForceCompatibilityVersion, ""); // Custom compatability version
SETTING_GROUP_END();

///@ ExportSettings Server
SETTING_GROUP(ServerNetworkSettings, virtual NetworkSettings);
FIXED_SETTING(bool, ServerNetwork, DisableNetworking, false); // If true, server networking listeners are not started
FIXED_SETTING(int32, ServerNetwork, ClientPingTime, 10000); // Client ping time in milliseconds
FIXED_SETTING(int32, ServerNetwork, InactivityDisconnectTime, 0); // Inactivity disconnect time in milliseconds
FIXED_SETTING(string, ServerNetwork, WssPrivateKey, ""); // WebSocket Secure private key
FIXED_SETTING(string, ServerNetwork, WssCertificate, ""); // WebSocket Secure certificate
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(ClientNetworkSettings, virtual NetworkSettings);
FIXED_SETTING(string, ClientNetwork, ServerHost, "localhost"); // Server host address
FIXED_SETTING(int32, ClientNetwork, PingPeriod, 2000); // Ping period in milliseconds
VARIABLE_SETTING(int32, ClientNetwork, ProxyType, 0); // Proxy type
VARIABLE_SETTING(string, ClientNetwork, ProxyHost, ""); // Proxy host address
VARIABLE_SETTING(int32, ClientNetwork, ProxyPort, 8080); // Proxy port number
VARIABLE_SETTING(string, ClientNetwork, ProxyUser, ""); // Proxy username
VARIABLE_SETTING(string, ClientNetwork, ProxyPass, ""); // Proxy password
VARIABLE_SETTING(int32, ClientNetwork, Ping); // Network ping (read only)
VARIABLE_SETTING(bool, ClientNetwork, DebugNet, false); // If true, network debugging is enabled
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(AudioSettings, virtual BaseSettings);
VARIABLE_SETTING(bool, Audio, DisableAudio, false); // If true, audio is disabled
VARIABLE_SETTING(int32, Audio, SoundVolume, 100); // Sound volume percentage
VARIABLE_SETTING(int32, Audio, MusicVolume, 100); // Music volume percentage
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(ViewSettings, virtual BaseSettings);
VARIABLE_SETTING(int32, View, ScreenWidth, 1024); // Screen width in pixels
VARIABLE_SETTING(int32, View, ScreenHeight, 768); // Screen height in pixels
FIXED_SETTING(int32, View, MonitorWidth); // Monitor width (read only)
FIXED_SETTING(int32, View, MonitorHeight); // Monitor height (read only)
VARIABLE_SETTING(int32, View, ScreenHudHeight, 0); // Screen HUD height in pixels
VARIABLE_SETTING(bool, View, ShowCorners, false); // If true, corners are shown
VARIABLE_SETTING(bool, View, ShowSpriteBorders, false); // If true, sprite borders are shown
FIXED_SETTING(bool, View, HideNativeCursor, false); // If true, native cursor is hidden
FIXED_SETTING(int32, View, FadingDuration, 1000); // Fading duration in milliseconds
FIXED_SETTING(bool, View, MapZoomEnabled); // Map view zooming
FIXED_SETTING(bool, View, MapDirectDraw); // Draw map directly to main framebuffer (speed up rendering but disables zooming and make scrolling jumpy)
FIXED_SETTING(bool, View, DisableLighting); // Disables lighting for more performance
FIXED_SETTING(bool, View, DisableFog); // Disables fog for more performance
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(GeometrySettings, virtual BaseSettings);
FIXED_SETTING(bool, Geometry, MapHexagonal); // Hexagonal map grid (read only)
FIXED_SETTING(bool, Geometry, MapSquare); // Square map grid (read only)
FIXED_SETTING(int32, Geometry, MapDirCount); // Directions count (read only)
FIXED_SETTING(int32, Geometry, MapHexWidth, 32); // Hex/square width
FIXED_SETTING(int32, Geometry, MapHexHeight, 16); // Hex/square height
FIXED_SETTING(int32, Geometry, MapHexLineHeight, 12); // Hex/square line height
FIXED_SETTING(int32, Geometry, MapTileStep, 2); // Tile step value
FIXED_SETTING(int32, Geometry, MapTileOffsX, -8); // Tile default X offset
FIXED_SETTING(int32, Geometry, MapTileOffsY, 32); // Tile default Y offset
FIXED_SETTING(int32, Geometry, MapRoofOffsX, -8); // Roof default X offset
FIXED_SETTING(int32, Geometry, MapRoofOffsY, -66); // Roof default Y offset
FIXED_SETTING(float32, Geometry, MapCameraAngle, 25.6589f); // Angle for critters moving/rendering
FIXED_SETTING(bool, Geometry, MapFreeMovement, false); // If true, free movement on the map is enabled
FIXED_SETTING(bool, Geometry, MapSmoothPath, true); // Enable pathfinding path smoothing
FIXED_SETTING(string, Geometry, MapDataPrefix, "Geometry"); // Path and prefix for names used for geometry sprites
FIXED_SETTING(bool, Geometry, CritterBlockHex, false); // If true, critters block hexes
FIXED_SETTING(int32, Geometry, MaxPathFindLength, 400); // Maximum pathfinding length
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(RenderSettings, virtual ViewSettings, virtual GeometrySettings);
FIXED_SETTING(int32, Render, Animation3dSmoothTime, 150); // 3D animation smooth time in milliseconds
FIXED_SETTING(int32, Render, Animation3dFPS, 30); // 3D animation frames per second
FIXED_SETTING(string, Render, HeadBone); // Head bone name (Todo: move HeadBone to fo3d settings)
FIXED_SETTING(vector<string>, Render, LegBones); // Leg bone names (Todo: move LegBones to fo3d settings)
VARIABLE_SETTING(bool, Render, WindowCentered, true); // If true, window is centered
VARIABLE_SETTING(bool, Render, WindowResizable, false); // If true, window is resizable
VARIABLE_SETTING(bool, Render, NullRenderer, false); // If true, null renderer is used
VARIABLE_SETTING(bool, Render, ForceOpenGL, false); // If true, OpenGL is forced
VARIABLE_SETTING(bool, Render, ForceDirect3D, false); // If true, Direct3D is forced
VARIABLE_SETTING(bool, Render, ForceMetal, false); // If true, Metal is forced
VARIABLE_SETTING(bool, Render, ForceGlslEsProfile, false); // If true, GLSL ES profile is forced
VARIABLE_SETTING(bool, Render, RenderDebug, false); // If true, render debugging is enabled
VARIABLE_SETTING(bool, Render, VSync, false); // If true, vertical synchronization is enabled
VARIABLE_SETTING(bool, Render, AlwaysOnTop, false); // If true, window is always on top
VARIABLE_SETTING(vector<float32>, Render, EffectValues, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0); // Effect values
VARIABLE_SETTING(bool, Render, Fullscreen, false); // If true, fullscreen mode is enabled
VARIABLE_SETTING(int32, Render, Brightness, 0); // Brightness value
VARIABLE_SETTING(int32, Render, Sleep, -1); // Sleep duration in milliseconds (-1 to disable)
VARIABLE_SETTING(int32, Render, FixedFPS, 100); // Fixed frames per second (0 to disable)
FIXED_SETTING(int32, Render, FogExtraLength, 0); // Extra fog length
FIXED_SETTING(int32, Render, FogTransitionDuration, 200); // Fog visual transition duration in milliseconds
FIXED_SETTING(float32, Render, CritterTurnAngle, 100.0f); // Critter turn angle
FIXED_SETTING(float32, Render, CritterBodyTurnFactor, 0.6f); // Critter body turn factor
FIXED_SETTING(float32, Render, CritterHeadTurnFactor, 0.4f); // Critter head turn factor
FIXED_SETTING(int32, Render, DefaultModelViewWidth, 0); // Default model view width
FIXED_SETTING(int32, Render, DefaultModelViewHeight, 0); // Default model view height
FIXED_SETTING(int32, Render, DefaultModelDrawWidth, 128); // Default model draw width
FIXED_SETTING(int32, Render, DefaultModelDrawHeight, 128); // Default model draw height
FIXED_SETTING(int32, Render, WalkAnimBaseSpeed, 60); // Walk animation base speed
FIXED_SETTING(int32, Render, RunAnimStartSpeed, 80); // Run animation start speed
FIXED_SETTING(int32, Render, RunAnimBaseSpeed, 120); // Run animation base speed
FIXED_SETTING(float32, Render, ModelProjFactor, 40.0f); // Model projection factor
FIXED_SETTING(int32, Render, EggEllipseWidthExt, 0); // Transparency egg ellipse extra width in pixels added to logical sprite/view width
FIXED_SETTING(int32, Render, EggEllipseHeightExt, 0); // Transparency egg ellipse extra height in pixels added to logical sprite/view height
FIXED_SETTING(float32, Render, EggTransparencyTransitionFactor, 0.9f); // Fraction of egg mask kept fully transparent before linear fade to opaque
FIXED_SETTING(bool, Render, AtlasLinearFiltration, false); // If true, atlas linear filtration is enabled
FIXED_SETTING(int32, Render, DefaultParticleDrawWidth, 128); // Default particle draw width
FIXED_SETTING(int32, Render, DefaultParticleDrawHeight, 128); // Default particle draw height
FIXED_SETTING(bool, Render, RecreateClientOnError, false); // If true, client is recreated on error
FIXED_SETTING(bool, Render, UseDummyEffects, false); // If true, missing client effects use built-in dummy definitions intended for testing with null renderer
FIXED_SETTING(string, Render, ImGuiColorStyle); // ImGui theme: Light, Classic, Dark
FIXED_SETTING(string, Render, ImGuiDefaultEffect, "Effects/ImGui_Default.fofx"); // Shader effect for ImGui
FIXED_SETTING(int32, Render, ImGuiFontTextureSize, 256); // Minimum ImGui texture size
FIXED_SETTING(int32, Render, SpriteHitValue, 127); // If alpha is greather then hit test is passed
FIXED_SETTING(string, Render, CritterStubSpriteName, ""); // Fallback critter sprite, empty value uses built-in runtime dummy sprite
FIXED_SETTING(string, Render, ItemStubSpriteName, ""); // Fallback item sprite, empty value uses built-in runtime dummy sprite
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(WebSettings, virtual RenderSettings);
FIXED_SETTING(bool, Web, AutoResize, true); // If true, web screen resolution is recalculated on browser window resize
FIXED_SETTING(string, Web, LoadingImage, ""); // Loading image path for web page, relative to main fomain file
FIXED_SETTING(string, Web, BackgroundColor, "rgb(0, 0, 0)"); // Background color for the web page around the game screen
FIXED_SETTING(int32, Web, ScreenHeightPercent, 75); // Web canvas height as a percentage of page height
FIXED_SETTING(float32, Web, AspectFactor, 0.75f); // Web canvas height-to-width factor, width = height / factor
FIXED_SETTING(float32, Web, HorizontalPosFactor, 0.5f); // Horizontal position factor in free page width, 0 = left, 0.5 = center, 1 = right
FIXED_SETTING(float32, Web, VerticalPosFactor, 0.25f); // Vertical position factor in free page height, 0 = top, 0.5 = center, 1 = bottom
FIXED_SETTING(int32, Web, MinWidth, 1024); // Minimum adaptive web width in pixels
FIXED_SETTING(int32, Web, MinHeight, 768); // Minimum adaptive web height in pixels
FIXED_SETTING(int32, Web, MaxWidth, 1920); // Maximum adaptive web width in pixels
FIXED_SETTING(int32, Web, MaxHeight, 1080); // Maximum adaptive web height in pixels
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(AndroidSettings, virtual BaseSettings);
FIXED_SETTING(string, Android, PackageName, "com.fonline.app"); // Android application package name for packaging tools
FIXED_SETTING(int32, Android, VersionCode, 1); // Android application version code for packaging tools
FIXED_SETTING(int32, Android, MinSdk, 23); // Android minimum supported SDK level for packaging tools
FIXED_SETTING(int32, Android, TargetSdk, 35); // Android target SDK level for packaging tools
FIXED_SETTING(int32, Android, CompileSdk, 35); // Android compile SDK level for packaging tools
FIXED_SETTING(string, Android, ScreenOrientation, "landscape"); // Android activity screen orientation for packaging tools
FIXED_SETTING(string, Android, Icon, "Engine/Resources/Radiation.png"); // Android launcher icon PNG source path for packaging tools
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(TimerSettings, virtual BaseSettings);
FIXED_SETTING(int32, Timer, DebuggingDeltaTimeCap, 100); // Debugging delta time cap in milliseconds
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(BakingSettings, virtual BaseSettings);
FIXED_SETTING(bool, Baking, ForceBaking, false); // If true, baking of all packs are forced
FIXED_SETTING(bool, Baking, SingleThreadBaking, false); // If true, single-threaded baking is enabled
FIXED_SETTING(bool, Baking, IgnoreMissingBakerWarning, false); // If true, missing baker warning is suppressed when stale baked resources already exist
FIXED_SETTING(vector<string>, Baking, RawCopyFileExtensions, "fopts", "fofnt", "bmfc", "fnt", "acm", "ogg", "wav", "ogv", "json", "ini"); // Raw copy file extensions
FIXED_SETTING(vector<string>, Baking, ProtoFileExtensions, "fopro", "fomap"); // Proto file extensions
FIXED_SETTING(vector<string>, Baking, BakeLanguages, "engl"); // Bake languages
FIXED_SETTING(string, Baking, BakeOutput, "Baking"); // Bake output directory
FIXED_SETTING(string, Baking, ServerResources, "ServerResources"); // Server resources directory
FIXED_SETTING(string, Baking, ClientResources, "Resources"); // Client resources directory
FIXED_SETTING(string, Baking, CacheResources, "Cache"); // Cache resources directory
FIXED_SETTING(vector<string>, Baking, ServerResourceEntries); // Server resource entries (read only)
FIXED_SETTING(vector<string>, Baking, ClientResourceEntries); // Client resource entries (read only)
FIXED_SETTING(vector<string>, Baking, MapperResourceEntries); // Mapper resource entries (read only)
FIXED_SETTING(int32, Baking, ZipCompressLevel, 1); // Zip deflate level (0-9)
SETTING_GROUP_END();

///@ ExportSettings Server
SETTING_GROUP(CritterSettings, virtual TimerSettings, virtual NetworkSettings, virtual GeometrySettings);
FIXED_SETTING(int32, Critter, MaxAddUnstackableItems, 10); // Maximum number of unstackable items to add
FIXED_SETTING(vector<bool>, Critter, CritterSlotEnabled, true, true); // Critter slot enabled flags
FIXED_SETTING(vector<bool>, Critter, CritterSlotSendData, false, true); // Critter slot send data flags
FIXED_SETTING(vector<bool>, Critter, CritterSlotMultiItem, true, false); // Critter slot multi-item flags
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(CritterViewSettings, virtual ViewSettings, virtual GeometrySettings, virtual TimerSettings);
FIXED_SETTING(int32, CritterView, NameOffset, 0); // Name offset value
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(HexSettings, virtual ViewSettings, virtual GeometrySettings, virtual CritterViewSettings);
FIXED_SETTING(int32, Hex, ScrollFixedDt, 10); // Scroll fixed delta time in milliseconds
FIXED_SETTING(int32, Hex, ScrollSpeed, 1200); // Scroll speed in pixels per second
FIXED_SETTING(int32, Hex, ZoomSpeed, 100); // Speed of zooming
FIXED_SETTING(ucolor, Hex, ChosenLightColor, ucolor::clear); // Chosen light color
FIXED_SETTING(uint8, Hex, ChosenLightDistance, 4); // Chosen light distance
FIXED_SETTING(int32, Hex, ChosenLightIntensity, 20); // Chosen light intensity
FIXED_SETTING(uint8, Hex, ChosenLightFlags, 0); // Chosen light flags
VARIABLE_SETTING(bool, Hex, FullscreenMouseScroll, true); // If true, fullscreen mouse scroll is enabled
VARIABLE_SETTING(bool, Hex, WindowedMouseScroll, false); // If true, windowed mouse scroll is enabled
VARIABLE_SETTING(bool, Hex, ScrollCheck, true); // If true, scroll check is enabled
VARIABLE_SETTING(bool, Hex, ScrollKeybLeft, false); // Keyboard map scroll left (read only)
VARIABLE_SETTING(bool, Hex, ScrollKeybRight, false); // Keyboard map scroll right (read only)
VARIABLE_SETTING(bool, Hex, ScrollKeybUp, false); // Keyboard map scroll up (read only)
VARIABLE_SETTING(bool, Hex, ScrollKeybDown, false); // Keyboard map scroll down (read only)
VARIABLE_SETTING(bool, Hex, ScrollMouseLeft, false); // Mouse map scroll left (read only)
VARIABLE_SETTING(bool, Hex, ScrollMouseRight, false); // Mouse map scroll right (read only)
VARIABLE_SETTING(bool, Hex, ScrollMouseUp, false); // Mouse map scroll up (read only)
VARIABLE_SETTING(bool, Hex, ScrollMouseDown, false); // Mouse map scroll down (read only)
VARIABLE_SETTING(uint8, Hex, RoofAlpha, 200); // Roof alpha value
VARIABLE_SETTING(bool, Hex, ShowTile, true); // If true, tiles are shown
VARIABLE_SETTING(bool, Hex, ShowRoof, true); // If true, roofs are shown
VARIABLE_SETTING(bool, Hex, ShowItem, true); // If true, items are shown
VARIABLE_SETTING(bool, Hex, ShowScen, true); // If true, scenery is shown
VARIABLE_SETTING(bool, Hex, ShowWall, true); // If true, walls are shown
VARIABLE_SETTING(bool, Hex, ShowCrit, true); // If true, critters are shown
VARIABLE_SETTING(bool, Hex, ShowFast, true); // If true, fast mode is enabled
FIXED_SETTING(int32, Hex, ScrollBlockSize, 1); // Blocked hexes around scroll block line
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(PlatformSettings, virtual BaseSettings);
FIXED_SETTING(bool, Platform, WebBuild); // Web build (read only)
FIXED_SETTING(bool, Platform, WindowsBuild); // Windows build (read only)
FIXED_SETTING(bool, Platform, LinuxBuild); // Linux build (read only)
FIXED_SETTING(bool, Platform, MacOsBuild); // macOS build (read only)
FIXED_SETTING(bool, Platform, AndroidBuild); // Android build (read only)
FIXED_SETTING(bool, Platform, IOsBuild); // iOS build (read only)
FIXED_SETTING(bool, Platform, DesktopBuild); // Desktop build (read only)
FIXED_SETTING(bool, Platform, TabletBuild); // Tablet build (read only)
SETTING_GROUP_END();

///@ ExportSettings Server
SETTING_GROUP(DataBaseSettings, virtual BaseSettings);
FIXED_SETTING(bool, DataBase, OpLogEnabled, true); // If true, pending database changes are mirrored to oplog for recovery; if false, any database write failure causes immediate panic
FIXED_SETTING(string, DataBase, OpLogPath, "DbPendingChanges.oplog"); // Pending database changes oplog path; committed replay progress path is derived as this value plus -committed
FIXED_SETTING(int32, DataBase, ReconnectRetryPeriod, 1000); // Database backend retry period in milliseconds while oplog mode is active
FIXED_SETTING(int32, DataBase, PanicOpLogSizeThreshold, 10000000); // Pending database changes file size threshold in bytes after which panic is triggered
FIXED_SETTING(int32, DataBase, PanicShutdownTimeout, 5000); // Graceful shutdown timeout in milliseconds after database panic callback before forced termination
FIXED_SETTING(bool, DataBase, UnQLiteOmitJournaling, true); // If true, UnQLite opens collections without journaling
FIXED_SETTING(int32, DataBase, JsonIndent, 4); // JSON backend indentation, 0 for compact output
FIXED_SETTING(string, DataBase, MongoEscapeChar, ":"); // Single character used to replace dots in Mongo document keys
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(InputSettings, virtual BaseSettings);
VARIABLE_SETTING(int32, Input, DoubleClickTime, 500); // Double-click time in milliseconds
VARIABLE_SETTING(int32, Input, ConsoleHistorySize, 100); // Console history size
VARIABLE_SETTING(ipos32, Input, MousePos); // Mouse position (read only)
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(MapperSettings, virtual BakingSettings);
FIXED_SETTING(string, Mapper, StartMap, ""); // Start map name
VARIABLE_SETTING(int32, Mapper, StartHexX, -1); // Start hex X coordinate
VARIABLE_SETTING(int32, Mapper, StartHexY, -1); // Start hex Y coordinate
VARIABLE_SETTING(bool, Mapper, SplitTilesCollection, true); // If true, tiles collection is split
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(ClientSettings, virtual CommonSettings, virtual ScriptSettings, virtual BakingSettings, virtual ClientNetworkSettings, virtual AudioSettings, virtual ViewSettings, virtual RenderSettings, virtual GeometrySettings, virtual TimerSettings, virtual HexSettings, virtual PlatformSettings, virtual InputSettings, virtual CritterViewSettings, virtual MapperSettings, virtual WebSettings, virtual AndroidSettings);
FIXED_SETTING(int32, Client, UpdaterInfoDelay, 1000); // Updater info delay in milliseconds
FIXED_SETTING(int32, Client, UpdaterInfoPos, 0); // Updater info position (<1 - top, 0 - center, >1 - bottom)
FIXED_SETTING(string, Client, DefaultSplash); // Default splash screen
FIXED_SETTING(string, Client, DefaultSplashPack); // Default splash pack
FIXED_SETTING(bool, Client, ClientPropertiesPackData, true); // If true, client entities with prototypes use overlay property storage
VARIABLE_SETTING(string, Client, Language, "engl"); // Language setting
VARIABLE_SETTING(bool, Client, WinNotify, true); // If true, Windows notifications are enabled
VARIABLE_SETTING(bool, Client, SoundNotify, false); // If true, sound notifications are enabled
VARIABLE_SETTING(bool, Client, HelpInfo, false); // If true, help information is shown
SETTING_GROUP_END();

///@ ExportSettings Server
SETTING_GROUP(ServerSettings, virtual CommonSettings, virtual ScriptSettings, virtual BakingSettings, virtual ServerNetworkSettings, virtual AudioSettings, virtual RenderSettings, virtual GeometrySettings, virtual PlatformSettings, virtual TimerSettings, virtual CritterSettings, virtual DataBaseSettings, virtual WebSettings);
FIXED_SETTING(string, Server, DbStorage, "Memory"); // Database storage type
FIXED_SETTING(bool, Server, ServerPropertiesPackData, true); // If true, server entities with prototypes use overlay property storage
FIXED_SETTING(bool, Server, NoStart, false); // If true, server start is disabled
FIXED_SETTING(bool, Server, AutoStartClientOnServer, false); // If true, server automatically spawns an embedded client on startup
FIXED_SETTING(bool, Server, CollapseLogOnStart, false); // If true, log is collapsed on start
FIXED_SETTING(int32, Server, MaxServerLogLines, 1000); // Maximum server log lines in UI
FIXED_SETTING(int32, Server, ServerSleep, -1); // Server sleep duration in milliseconds (-1 to disable)
FIXED_SETTING(int32, Server, LoopsPerSecondCap, 1000); // Loops per second cap
FIXED_SETTING(int32, Server, LockMaxWaitTime, 100); // Maximum lock wait time in milliseconds
FIXED_SETTING(int32, Server, LoopAverageTimeInterval, 1000); // Loop average time interval in milliseconds
FIXED_SETTING(bool, Server, WriteHealthFile, false); // If true, health file is written
FIXED_SETTING(bool, Server, ProtoMapStaticGrid, false); // If true, proto map static grid is enabled
FIXED_SETTING(bool, Server, MapInstanceStaticGrid, false); // If true, map instance static grid is enabled
FIXED_SETTING(int64, Server, EntityStartId, 10000000001); // Entity start ID
SETTING_GROUP_END();

#undef FIXED_SETTING
#undef VARIABLE_SETTING
#undef SETTING_GROUP
#undef SETTING_GROUP_END
