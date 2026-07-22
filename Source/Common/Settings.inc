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
FIXED_SETTING(bool, Common, AsyncLogWrite, false); // If true, log messages are written to file and console from a dedicated background thread
FIXED_SETTING(vector<string>, Common, SecretSettingTokens, "secret", "token", "password", "apikey"); // Case-insensitive substrings that mark a setting name as a credential whose value is masked as "***" in command-line override logs
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(ScriptSettings, virtual BaseSettings);
FIXED_SETTING(int32_t, Script, OverrunReportTime); // Time in milliseconds to report script overrun, 0 to disable
FIXED_SETTING(bool, Script, DebuggerEnabled, false); // If true, AngelScript debugger endpoint is enabled
FIXED_SETTING(string, Script, DebuggerBindHost, "0.0.0.0"); // Debugger TCP bind host
FIXED_SETTING(vector<string>, Script, MutableGlobalsAllowedNamespaces); // Script namespaces (matched as prefixes) whose module-level globals may be mutable (everywhere else const-only)
FIXED_SETTING(vector<string>, Script, AttributedFunctionDirectCallAllowedNamespaces); // Script namespaces (matched as prefixes) whose functions may call attributed functions ([[Event]], [[TimeEvent]], etc.) directly instead of going through the runtime dispatcher
FIXED_SETTING(vector<string>, Script, ExtraDirectCallBlockingAttributes); // Project-defined attribute names that should also block direct script-to-script calls (in addition to the engine's built-in list — [[Event]], [[TimeEvent]], etc.). Used for project-specific dispatcher-bound attributes like Last Frontier's [[DialogResult]] / [[DialogDemand]].
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(NetworkSettings, virtual BaseSettings);
FIXED_SETTING(int32_t, Network, ServerPort, 4000); // Server port number
FIXED_SETTING(int32_t, Network, WebSocketPort, 4001); // WebSockets port for server listen and client connect
FIXED_SETTING(int32_t, Network, NetBufferSize, 4096); // Network buffer size
FIXED_SETTING(int32_t, Network, UdpPortOffset, 0); // UDP port offset relative to ServerPort
FIXED_SETTING(int32_t, Network, UdpPacketSize, 1200); // Target UDP payload budget in bytes
FIXED_SETTING(int32_t, Network, UdpWindowSize, 65536); // Reliable UDP send window size in bytes
FIXED_SETTING(int32_t, Network, UdpResendTimeout, 150); // UDP packet resend timeout in milliseconds
FIXED_SETTING(int32_t, Network, UdpConnectRetry, 500); // UDP connect retry timeout in milliseconds
FIXED_SETTING(int32_t, Network, UdpConnectTimeout, 4000); // UDP connect timeout in milliseconds before failing transport bootstrap
FIXED_SETTING(int32_t, Network, UdpSendUpdateInterval, 16); // UDP send tick interval in milliseconds
FIXED_SETTING(int32_t, Network, UdpRedundancy, 1); // Number of previous UDP packets redundantly resent with new data
FIXED_SETTING(int32_t, Network, UpdateFileMaxPortionSize, 1000000); // Maximum update file portion size in bytes
FIXED_SETTING(bool, Network, SecuredWebSockets, false); // If true, secured WebSockets are enabled
FIXED_SETTING(bool, Network, DisableTcpNagle, true); // If true, TCP Nagle algorithm is disabled
FIXED_SETTING(bool, Network, DisableZlibCompression, false); // If true, Zlib compression is disabled
FIXED_SETTING(bool, Network, DisableInterthreadCommunication, false); // If true, server does not register the in-process interthread listener; clients in the same process fall through to TCP/UDP
FIXED_SETTING(int32_t, Network, ArtificalLags, 0); // Artificial lags in milliseconds
FIXED_SETTING(string, Network, CompatibilityVersion); // Compatibility version (read only)
FIXED_SETTING(string, Network, ForceCompatibilityVersion, ""); // Custom compatability version
SETTING_GROUP_END();

///@ ExportSettings Server
SETTING_GROUP(ServerNetworkSettings, virtual NetworkSettings);
FIXED_SETTING(bool, ServerNetwork, DisableNetworking, false); // If true, server networking listeners are not started
FIXED_SETTING(bool, ServerNetwork, EnableUdp, false); // If true, UDP listener is enabled for native clients
FIXED_SETTING(bool, ServerNetwork, RejectUdpConnections, false); // If true, UDP listener silently drops incoming Connect packets so clients fall back to TCP (debug aid)
FIXED_SETTING(bool, ServerNetwork, UpdateFilesInMemory, false); // If true, updater files are served from memory, otherwise from disk
FIXED_SETTING(int32_t, ServerNetwork, ClientPingTime, 10000); // Client ping time in milliseconds
FIXED_SETTING(int32_t, ServerNetwork, InactivityDisconnectTime, 0); // Inactivity disconnect time in milliseconds
FIXED_SETTING(int32_t, ServerNetwork, LoginTimeout, 0); // Maximum pre-login connection lifetime without handshake/auth/update progress in milliseconds (0 = unlimited)
FIXED_SETTING(int32_t, ServerNetwork, MaxMessageSize, 1048576); // Max single inbound message size in bytes (0 = unlimited); rejected at the header
FIXED_SETTING(int32_t, ServerNetwork, MaxMessagesPerProcessPass, 256); // Max messages drained per connection per worker job pass (0 = unlimited)
FIXED_SETTING(int32_t, ServerNetwork, MaxUdpReorderAhead, 1024); // Max UDP packets buffered ahead of the next expected sequence (0 = unlimited)
FIXED_SETTING(int32_t, ServerNetwork, MaxConnections, 0); // Max simultaneous connections incl. unlogined (0 = unlimited); over this, new connections are rejected at accept
FIXED_SETTING(int32_t, ServerNetwork, MaxPlayers, 0); // Max simultaneous logined players (0 = unlimited); over this, new connections are rejected at accept
FIXED_SETTING(int32_t, ServerNetwork, NewConnectionRatePerSec, 0); // Max new connections accepted per source host per second (0 = unlimited); bursts above this are dropped at accept
FIXED_SETTING(string, ServerNetwork, WssPrivateKey, ""); // WebSocket Secure private key
FIXED_SETTING(string, ServerNetwork, WssCertificate, ""); // WebSocket Secure certificate
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(ClientNetworkSettings, virtual NetworkSettings);
FIXED_SETTING(string, ClientNetwork, ServerHost, "127.0.0.1"); // Server host address
FIXED_SETTING(string, ClientNetwork, WebSocketHost, "localhost"); // WebSockets server host for client connect
FIXED_SETTING(int32_t, ClientNetwork, PingPeriod, 2000); // Ping period in milliseconds
VARIABLE_SETTING(bool, ClientNetwork, UseUdp, false); // If true, UDP native transport is used instead of TCP
VARIABLE_SETTING(int32_t, ClientNetwork, ProxyType, 0); // Proxy type
VARIABLE_SETTING(string, ClientNetwork, ProxyHost, ""); // Proxy host address
VARIABLE_SETTING(int32_t, ClientNetwork, ProxyPort, 8080); // Proxy port number
VARIABLE_SETTING(string, ClientNetwork, ProxyUser, ""); // Proxy username
VARIABLE_SETTING(string, ClientNetwork, ProxyPass, ""); // Proxy password
VARIABLE_SETTING(int32_t, ClientNetwork, Ping); // Network ping (read only)
VARIABLE_SETTING(bool, ClientNetwork, DebugNet, false); // If true, network debugging is enabled
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(AudioSettings, virtual BaseSettings);
VARIABLE_SETTING(bool, Audio, DisableAudio, false); // If true, audio is disabled
VARIABLE_SETTING(int32_t, Audio, SoundVolume, 100); // Sound volume percentage
VARIABLE_SETTING(int32_t, Audio, MusicVolume, 100); // Music volume percentage
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(ViewSettings, virtual BaseSettings);
VARIABLE_SETTING(int32_t, View, ScreenWidth, 1024); // Screen width in pixels
VARIABLE_SETTING(int32_t, View, ScreenHeight, 768); // Screen height in pixels
FIXED_SETTING(int32_t, View, MonitorWidth); // Monitor width (read only)
FIXED_SETTING(int32_t, View, MonitorHeight); // Monitor height (read only)
FIXED_SETTING(bool, View, HideNativeCursor, false); // If true, native cursor is hidden
FIXED_SETTING(int32_t, View, FadingDuration, 1000); // Fading duration in milliseconds
FIXED_SETTING(bool, View, MapZoomEnabled); // Map view zooming
FIXED_SETTING(bool, View, MapDirectDraw); // Draw map directly to main framebuffer (speed up rendering but disables zooming and make scrolling jumpy)
FIXED_SETTING(bool, View, DisableLighting); // Disables lighting for more performance
FIXED_SETTING(bool, View, DisableFog); // Disables fog for more performance
FIXED_SETTING(vector<int32_t>, View, GlobalDayColorTime, 300, 600, 1140, 1380); // Project-wide default DayColorTime in minutes (4 keys); maps may override per-map via Map.DayColorTime
FIXED_SETTING(vector<uint8_t>, View, GlobalDayColor, 18, 128, 103, 51, 18, 128, 95, 40, 53, 128, 86, 29); // Project-wide default DayColor (12 bytes, channel-major: R0..R3 G0..G3 B0..B3); maps may override per-map via Map.DayColor
FIXED_SETTING(bool, View, DisableIndoorMask, false); // If true, MapView skips the _rtIndoorMask render pass (saves a sprite-draw + RT swap per frame). Use for projects without weather, or to debug-see weather through hidden roofs.
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(GeometrySettings, virtual BaseSettings);
FIXED_SETTING(bool, Geometry, MapHexagonal); // Hexagonal map grid (read only)
FIXED_SETTING(bool, Geometry, MapSquare); // Square map grid (read only)
FIXED_SETTING(int32_t, Geometry, MapDirCount); // Directions count (read only)
FIXED_SETTING(int32_t, Geometry, MapHexWidth, 32); // Hex/square width
FIXED_SETTING(int32_t, Geometry, MapHexHeight, 16); // Hex/square height
FIXED_SETTING(int32_t, Geometry, MapHexLineHeight, 12); // Hex/square line height
FIXED_SETTING(int32_t, Geometry, MapTileStep, 2); // Tile step value
FIXED_SETTING(int32_t, Geometry, MapRoofElevation, 98); // Roof tile 3D elevation, applied to every roof tile (a roof is just a tile lifted in 3D so it draws above the walls). Replaces the old 2D MapRoofOffs* raise; the flat tile/roof XY offset now lives on the BaseTile prototype Offset
FIXED_SETTING(float32_t, Geometry, MapCameraAngle, 25.6589f); // Angle for critters moving/rendering
FIXED_SETTING(bool, Geometry, MapFreeMovement, false); // If true, free movement on the map is enabled
FIXED_SETTING(bool, Geometry, MapSmoothPath, true); // Enable pathfinding path smoothing
FIXED_SETTING(string, Geometry, MapDataPrefix, "Geometry"); // Path and prefix for names used for geometry sprites
FIXED_SETTING(bool, Geometry, CritterBlockHex, false); // If true, critters block hexes
FIXED_SETTING(int32_t, Geometry, MaxPathFindLength, 400); // Maximum pathfinding length
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(SpriteMeshSettings, virtual BaseSettings);
FIXED_SETTING(bool, SpriteMesh, Enabled, false); // If true, polygonal sprite meshes are generated during image baking
FIXED_SETTING(int32_t, SpriteMesh, AlphaThreshold, 0); // Minimum alpha excluded from sprite mesh geometry (0-254)
FIXED_SETTING(int32_t, SpriteMesh, MaxTriangles, 4096); // Maximum triangle budget searched across whole-sprite, per-component, and detailed mesh candidates
FIXED_SETTING(float32_t, SpriteMesh, AreaSavingsWeight, 32.0f); // Score awarded for removing a full frame area; one point offsets one submitted triangle
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(RenderSettings, virtual ViewSettings, virtual GeometrySettings);
FIXED_SETTING(int32_t, Render, Animation3dSmoothTime, 150); // 3D animation smooth time in milliseconds
FIXED_SETTING(int32_t, Render, Animation3dFPS, 30); // 3D animation frames per second
FIXED_SETTING(string, Render, HeadBone); // Head bone name (Todo: move HeadBone to fo3d settings)
FIXED_SETTING(vector<string>, Render, LegBones); // Leg bone names (Todo: move LegBones to fo3d settings)
VARIABLE_SETTING(bool, Render, WindowCentered, true); // If true, window is centered
VARIABLE_SETTING(bool, Render, WindowResizable, false); // If true, window is resizable
VARIABLE_SETTING(bool, Render, HeadlessWindow, false); // If true, window is created hidden for off-screen rendering
VARIABLE_SETTING(bool, Render, NullRenderer, false); // If true, null renderer is used
VARIABLE_SETTING(bool, Render, ForceOpenGL, false); // If true, OpenGL is forced
VARIABLE_SETTING(bool, Render, ForceDirect3D, false); // If true, Direct3D is forced
VARIABLE_SETTING(bool, Render, ForceMetal, false); // If true, Metal is forced
VARIABLE_SETTING(bool, Render, ForceVulkan, false); // If true, Vulkan is forced
VARIABLE_SETTING(bool, Render, ForceSDLGpu, false); // If true, SDL_GPU backend is forced
VARIABLE_SETTING(bool, Render, ForceGlslEsProfile, false); // If true, GLSL ES profile is forced
FIXED_SETTING(string, Render, Direct3DVertexShaderProfile, "vs_4_0_level_9_3"); // Direct3D vertex shader compile profile
FIXED_SETTING(string, Render, Direct3DPixelShaderProfile, "ps_4_0_level_9_3"); // Direct3D pixel shader compile profile
FIXED_SETTING(string, Render, SDLGpuDriver, ""); // Preferred SDL_GPU driver name (vulkan / metal / direct3d12), empty lets SDL pick
VARIABLE_SETTING(bool, Render, RenderDebug, false); // If true, render debugging is enabled
VARIABLE_SETTING(bool, Render, DrawWireframe, false); // If true, rendered sprite triangle edges are overlaid for geometry debugging
VARIABLE_SETTING(bool, Render, VSync, false); // If true, vertical synchronization is enabled
VARIABLE_SETTING(bool, Render, AlwaysOnTop, false); // If true, window is always on top
VARIABLE_SETTING(bool, Render, Fullscreen, false); // If true, fullscreen mode is enabled
VARIABLE_SETTING(int32_t, Render, Brightness, 0); // Brightness value
VARIABLE_SETTING(int32_t, Render, Sleep, -1); // Sleep duration in milliseconds (-1 to disable)
VARIABLE_SETTING(int32_t, Render, FixedFPS, 100); // Fixed frames per second (0 to disable)
FIXED_SETTING(float32_t, Render, CritterTurnAngle, 100.0f); // Critter turn angle
FIXED_SETTING(float32_t, Render, CritterBodyTurnFactor, 0.6f); // Critter body turn factor
FIXED_SETTING(float32_t, Render, CritterHeadTurnFactor, 0.4f); // Critter head turn factor
FIXED_SETTING(int32_t, Render, WalkAnimBaseSpeed, 60); // Walk animation base speed
FIXED_SETTING(int32_t, Render, RunAnimStartSpeed, 80); // Run animation start speed
FIXED_SETTING(int32_t, Render, RunAnimBaseSpeed, 120); // Run animation base speed
FIXED_SETTING(float32_t, Render, ModelProjFactor, 40.0f); // Screen px per 3D world unit (1 unit = 1 hex = 1 m); scales 3D models and in-scene particles
FIXED_SETTING(bool, Render, ModelDirectDraw, false); // If true, map 3D models render directly into the scene depth buffer; otherwise they render as cached atlas sprites
FIXED_SETTING(int32_t, Render, MapMaxElevation, 4096); // Max abs sprite elevation (px) used to size the per-map scene depth range; smaller = more depth-buffer precision (less z-fighting), but sprites beyond it would be depth-clipped
FIXED_SETTING(int32_t, Render, EggEllipseWidthExt, 0); // Transparency egg ellipse extra width in pixels added to logical sprite/view width
FIXED_SETTING(int32_t, Render, EggEllipseHeightExt, 0); // Transparency egg ellipse extra height in pixels added to logical sprite/view height
FIXED_SETTING(float32_t, Render, EggTransparencyTransitionFactor, 0.9f); // Fraction of egg mask kept fully transparent before linear fade to opaque
FIXED_SETTING(bool, Render, AtlasLinearFiltration, false); // If true, atlas linear filtration is enabled
FIXED_SETTING(int32_t, Render, DefaultParticleDrawWidth, 128); // Default particle draw width
FIXED_SETTING(int32_t, Render, DefaultParticleDrawHeight, 128); // Default particle draw height
FIXED_SETTING(bool, Render, RecreateClientOnError, false); // If true, client is recreated on error
FIXED_SETTING(string, Render, ImGuiColorStyle); // ImGui theme: Light, Classic, Dark
FIXED_SETTING(string, Render, ImGuiDefaultEffect, "Effects/ImGui_Default.fofx"); // Shader effect for ImGui
FIXED_SETTING(int32_t, Render, ImGuiFontTextureSize, 256); // Minimum ImGui texture size
FIXED_SETTING(int32_t, Render, SpriteHitValue, 127); // If alpha is greather then hit test is passed
FIXED_SETTING(string, Render, CritterStubSpriteName, ""); // Fallback critter sprite, empty value uses built-in runtime dummy sprite
FIXED_SETTING(string, Render, ItemStubSpriteName, ""); // Fallback item sprite, empty value uses built-in runtime dummy sprite
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(WebSettings, virtual RenderSettings);
FIXED_SETTING(bool, Web, AutoResize, true); // If true, web screen resolution is recalculated on browser window resize
FIXED_SETTING(string, Web, LoadingImage, ""); // Loading image path for web page, relative to main fomain file
FIXED_SETTING(string, Web, BackgroundColor, "rgb(0, 0, 0)"); // Background color for the web page around the game screen
FIXED_SETTING(int32_t, Web, ScreenHeightPercent, 75); // Web canvas height as a percentage of page height
FIXED_SETTING(float32_t, Web, AspectFactor, 0.75f); // Web canvas height-to-width factor, width = height / factor
FIXED_SETTING(float32_t, Web, HorizontalPosFactor, 0.5f); // Horizontal position factor in free page width, 0 = left, 0.5 = center, 1 = right
FIXED_SETTING(float32_t, Web, VerticalPosFactor, 0.25f); // Vertical position factor in free page height, 0 = top, 0.5 = center, 1 = bottom
FIXED_SETTING(int32_t, Web, MinWidth, 1024); // Minimum adaptive web width in pixels
FIXED_SETTING(int32_t, Web, MinHeight, 768); // Minimum adaptive web height in pixels
FIXED_SETTING(int32_t, Web, MaxWidth, 1920); // Maximum adaptive web width in pixels
FIXED_SETTING(int32_t, Web, MaxHeight, 1080); // Maximum adaptive web height in pixels
SETTING_GROUP_END();

///@ ExportSettings Server
SETTING_GROUP(AndroidSettings, virtual BaseSettings);
FIXED_SETTING(string, Android, PackageName, "com.fonline.app"); // Android application package name for packaging tools
FIXED_SETTING(int32_t, Android, VersionCode, 1); // Android application version code for packaging tools
FIXED_SETTING(int32_t, Android, MinSdk, 23); // Android minimum supported SDK level for packaging tools
FIXED_SETTING(int32_t, Android, TargetSdk, 35); // Android target SDK level for packaging tools
FIXED_SETTING(int32_t, Android, CompileSdk, 35); // Android compile SDK level for packaging tools
FIXED_SETTING(string, Android, ScreenOrientation, "landscape"); // Android activity screen orientation for packaging tools
FIXED_SETTING(string, Android, Icon, "Engine/Resources/Radiation.png"); // Android launcher icon PNG source path for packaging tools
FIXED_SETTING(string, Android, Keystore, ""); // Android release signing keystore path for packaging tools
FIXED_SETTING(string, Android, KeystorePassword, ""); // Android release signing keystore password for packaging tools
FIXED_SETTING(string, Android, KeyAlias, ""); // Android release signing key alias for packaging tools
FIXED_SETTING(string, Android, KeyPassword, ""); // Android release signing key password for packaging tools
SETTING_GROUP_END();

///@ ExportSettings Server
SETTING_GROUP(PackagingSettings, virtual AndroidSettings, virtual WebSettings);
FIXED_SETTING(string, Packaging, CodeSigningHook, ""); // Windows code signing hook executable path for packaging tools
FIXED_SETTING(string, Packaging, AppIcon, ""); // Windows application ICO source path for packaging tools
FIXED_SETTING(string, Packaging, MsiUpgradeCode, ""); // Windows MSI installer upgrade GUID for packaging tools, must never change once an MSI has shipped
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(TimerSettings, virtual BaseSettings);
FIXED_SETTING(int32_t, Timer, DeltaTimeCap, 100); // Frame delta time cap in milliseconds
SETTING_GROUP_END();

///@ ExportSettings Common
SETTING_GROUP(BakingSettings, virtual ScriptSettings, virtual SpriteMeshSettings, virtual BaseSettings);
FIXED_SETTING(bool, Baking, ForceBaking, false); // If true, baking of all packs are forced
FIXED_SETTING(bool, Baking, SingleThreadBaking, false); // If true, single-threaded baking is enabled
FIXED_SETTING(bool, Baking, IgnoreMissingBakerWarning, false); // If true, missing baker warning is suppressed when stale baked resources already exist
FIXED_SETTING(vector<string>, Baking, RawCopyFileExtensions, "fofnt", "bmfc", "fnt", "acm", "ogg", "wav", "ogv", "json", "ini"); // Raw copy file extensions
FIXED_SETTING(vector<string>, Baking, ProtoFileExtensions, "fopro", "fomap"); // Authored proto/map container extensions: drives proto baking, map-container discovery (files with [ProtoMap] anchors) and mapper file browsing
FIXED_SETTING(vector<string>, Baking, BakeLanguages, "engl"); // Bake languages
FIXED_SETTING(string, Baking, BakeOutput, "Baking"); // Bake output directory
FIXED_SETTING(string, Baking, ServerResources, "ServerResources"); // Server resources directory
FIXED_SETTING(string, Baking, ClientResources, "Resources"); // Client resources directory
FIXED_SETTING(string, Baking, PlatformBinaries, "PlatformBinaries"); // Per-platform client runtime binaries directory served by the updater
FIXED_SETTING(string, Baking, CacheResources, "Cache"); // Cache resources directory
FIXED_SETTING(vector<string>, Baking, ServerResourceEntries); // Server resource entries (read only)
FIXED_SETTING(vector<string>, Baking, ClientResourceEntries); // Client resource entries (read only)
FIXED_SETTING(vector<string>, Baking, MapperResourceEntries); // Mapper resource entries (read only)
FIXED_SETTING(int32_t, Baking, ZipCompressLevel, 1); // Zip deflate level (0-9)
SETTING_GROUP_END();

///@ ExportSettings Server
SETTING_GROUP(CritterSettings, virtual TimerSettings, virtual NetworkSettings, virtual GeometrySettings);
FIXED_SETTING(int32_t, Critter, MaxAddUnstackableItems, 10); // Maximum number of unstackable items to add
FIXED_SETTING(vector<bool>, Critter, CritterSlotEnabled, true, true); // Critter slot enabled flags
FIXED_SETTING(vector<bool>, Critter, CritterSlotSendData, false, true); // Critter slot send data flags
FIXED_SETTING(vector<bool>, Critter, CritterSlotMultiItem, true, false); // Critter slot multi-item flags
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(CritterViewSettings, virtual ViewSettings, virtual GeometrySettings, virtual TimerSettings);
FIXED_SETTING(int32_t, CritterView, NameOffset, 0); // Name offset value
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(HexSettings, virtual ViewSettings, virtual GeometrySettings, virtual CritterViewSettings);
FIXED_SETTING(int32_t, Hex, ScrollFixedDt, 10); // Scroll fixed delta time in milliseconds
FIXED_SETTING(int32_t, Hex, ScrollSpeed, 1200); // Scroll speed in pixels per second
FIXED_SETTING(int32_t, Hex, ZoomSpeed, 100); // Speed of zooming
VARIABLE_SETTING(bool, Hex, FullscreenMouseScroll, true); // If true, fullscreen mouse scroll is enabled
VARIABLE_SETTING(bool, Hex, WindowedMouseScroll, false); // If true, windowed mouse scroll is enabled
VARIABLE_SETTING(bool, Hex, ScrollKeybLeft, false); // Keyboard map scroll left (read only)
VARIABLE_SETTING(bool, Hex, ScrollKeybRight, false); // Keyboard map scroll right (read only)
VARIABLE_SETTING(bool, Hex, ScrollKeybUp, false); // Keyboard map scroll up (read only)
VARIABLE_SETTING(bool, Hex, ScrollKeybDown, false); // Keyboard map scroll down (read only)
VARIABLE_SETTING(bool, Hex, ScrollMouseLeft, false); // Mouse map scroll left (read only)
VARIABLE_SETTING(bool, Hex, ScrollMouseRight, false); // Mouse map scroll right (read only)
VARIABLE_SETTING(bool, Hex, ScrollMouseUp, false); // Mouse map scroll up (read only)
VARIABLE_SETTING(bool, Hex, ScrollMouseDown, false); // Mouse map scroll down (read only)
VARIABLE_SETTING(uint8_t, Hex, RoofAlpha, 200); // Roof alpha value
VARIABLE_SETTING(bool, Hex, ShowTile, true); // If true, tiles are shown
VARIABLE_SETTING(bool, Hex, ShowRoof, true); // If true, roofs are shown
VARIABLE_SETTING(bool, Hex, ShowItem, true); // If true, items are shown
VARIABLE_SETTING(bool, Hex, ShowScen, true); // If true, scenery is shown
VARIABLE_SETTING(bool, Hex, ShowWall, true); // If true, walls are shown
VARIABLE_SETTING(bool, Hex, ShowCrit, true); // If true, critters are shown
VARIABLE_SETTING(bool, Hex, ShowFast, true); // If true, fast mode is enabled
FIXED_SETTING(int32_t, Hex, ScrollBlockSize, 1); // Blocked hexes around scroll block line
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
FIXED_SETTING(int32_t, DataBase, ReconnectRetryPeriod, 1000); // Database backend retry period in milliseconds while oplog mode is active
FIXED_SETTING(int32_t, DataBase, PanicOpLogSizeThreshold, 10000000); // Pending database changes file size threshold in bytes after which panic is triggered
FIXED_SETTING(int32_t, DataBase, PanicShutdownTimeout, 5000); // Graceful shutdown timeout in milliseconds after database panic callback before forced termination
FIXED_SETTING(bool, DataBase, UnQLiteOmitJournaling, true); // If true, UnQLite opens collections without journaling
FIXED_SETTING(int32_t, DataBase, JsonIndent, 4); // JSON backend indentation, 0 for compact output
FIXED_SETTING(string, DataBase, MongoEscapeChar, ":"); // Single character used to replace dots in Mongo document keys
FIXED_SETTING(vector<string>, DataBase, CustomCollections); // Space-separated user-defined database collections in format CollectionName:Int or CollectionName:Str
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(InputSettings, virtual BaseSettings);
VARIABLE_SETTING(bool, Input, DisableGamepad, false); // If true, gamepad support is disabled
VARIABLE_SETTING(int32_t, Input, DoubleClickTime, 500); // Double-click time in milliseconds
VARIABLE_SETTING(int32_t, Input, ConsoleHistorySize, 100); // Console history size
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(MapperSettings, virtual BakingSettings);
FIXED_SETTING(string, Mapper, StartMap, ""); // Start map name
VARIABLE_SETTING(int32_t, Mapper, StartHexX, -1); // Start hex X coordinate
VARIABLE_SETTING(int32_t, Mapper, StartHexY, -1); // Start hex Y coordinate
VARIABLE_SETTING(bool, Mapper, SplitTilesCollection, true); // If true, tiles collection is split
VARIABLE_SETTING(string, Mapper, ParticlePreviewEffect, ""); // Particle resource to preview after opening the start map
VARIABLE_SETTING(int32_t, Mapper, ParticlePreviewSeed, 0); // Seed used when the selected runtime supports seeded startup preview
VARIABLE_SETTING(float32_t, Mapper, ParticlePreviewScale, 1.0f); // Scale for the startup particle preview
VARIABLE_SETTING(bool, Mapper, ParticlePreviewPrewarm, false); // If true, prewarm the startup particle preview
SETTING_GROUP_END();

///@ ExportSettings Client
SETTING_GROUP(ClientSettings, virtual CommonSettings, virtual ScriptSettings, virtual BakingSettings, virtual ClientNetworkSettings, virtual AudioSettings, virtual ViewSettings, virtual RenderSettings, virtual GeometrySettings, virtual TimerSettings, virtual HexSettings, virtual PlatformSettings, virtual InputSettings, virtual CritterViewSettings, virtual MapperSettings, virtual WebSettings);
FIXED_SETTING(int32_t, Client, UpdaterInfoDelay, 1000); // Updater info delay in milliseconds
FIXED_SETTING(int32_t, Client, UpdaterInfoPos, 0); // Updater info position (<1 - top, 0 - center, >1 - bottom)
FIXED_SETTING(string, Client, DefaultSplash); // Default splash screen
FIXED_SETTING(string, Client, DefaultSplashPack); // Default splash pack
FIXED_SETTING(bool, Client, ClientPropertiesPackData, true); // If true, client entities with prototypes use overlay property storage
FIXED_SETTING(bool, Client, ForceEmbeddedRuntime, false); // If true, the client host runs the embedded engine instead of loading the bundled runtime DLL first (an explicit --ClientLibPath still loads a DLL); read from the command line at host startup, before settings are otherwise resolved
VARIABLE_SETTING(string, Client, UserWritablePath); // Writable data root for an INSTALLED client (cache, logs, self-update overlay) when the install dir is read-only. Empty = portable (write next to the exe, the default). "*" = the per-OS user data dir from the environment (Windows %LOCALAPPDATA% else %APPDATA%; macOS ~/Library/Application Support; Linux/Android $XDG_DATA_HOME else ~/.local/share). Otherwise an explicit absolute path. Resolved to an absolute, created directory at startup; read-only base resources stay in the install dir with this dir layered on top. The installer sets this; see Docs/ClientUpdater.md
VARIABLE_SETTING(string, Client, Language, "engl"); // Language setting
VARIABLE_SETTING(bool, Client, WinNotify, true); // If true, Windows notifications are enabled
VARIABLE_SETTING(bool, Client, SoundNotify, false); // If true, sound notifications are enabled
VARIABLE_SETTING(bool, Client, HelpInfo, false); // If true, help information is shown
SETTING_GROUP_END();

///@ ExportSettings Server
SETTING_GROUP(ServerSettings, virtual CommonSettings, virtual ScriptSettings, virtual BakingSettings, virtual ServerNetworkSettings, virtual AudioSettings, virtual RenderSettings, virtual GeometrySettings, virtual PlatformSettings, virtual TimerSettings, virtual CritterSettings, virtual DataBaseSettings, virtual WebSettings, virtual PackagingSettings);
FIXED_SETTING(string, Server, DbStorage, "Memory"); // Database storage type
FIXED_SETTING(bool, Server, ServerPropertiesPackData, true); // If true, server entities with prototypes use overlay property storage
FIXED_SETTING(bool, Server, NoStart, false); // If true, server start is disabled
FIXED_SETTING(int32_t, Server, AutoStartClientOnServer, 0); // Number of embedded clients to auto-spawn on server startup; 0 disables the feature
VARIABLE_SETTING(int32_t, Server, ServerWidth, 800); // Server window width in pixels (test bench host before any client is spawned)
VARIABLE_SETTING(int32_t, Server, ServerHeight, 600); // Server window height in pixels (test bench host before any client is spawned)
FIXED_SETTING(bool, Server, CollapseLogOnStart, false); // If true, log is collapsed on start
FIXED_SETTING(int32_t, Server, MaxServerLogLines, 1000); // Maximum server log lines in UI
FIXED_SETTING(int32_t, Server, LockMaxWaitTime, 100); // Maximum lock wait time in milliseconds
FIXED_SETTING(int32_t, Server, WorkerThreads, 0); // Worker thread count for entity processing (0 = auto)
FIXED_SETTING(bool, Server, WriteHealthFile, false); // If true, health file is written
FIXED_SETTING(bool, Server, ProtoMapStaticGrid, false); // If true, proto map static grid is enabled
FIXED_SETTING(bool, Server, MapInstanceStaticGrid, false); // If true, map instance static grid is enabled
FIXED_SETTING(int64_t, Server, EntityStartId, 10000000001); // Entity start ID
FIXED_SETTING(int64_t, Server, EntityIdReserveBatch, 1000); // Entity IDs reserved per persisted-counter bump, so a new entity does not force a DB write of the last-id marker every time
FIXED_SETTING(int32_t, Server, SyncPeriodMs, 10); // Sync-point job period in milliseconds (100 FPS by default)
FIXED_SETTING(int32_t, Server, FrameTimePeriodNs, 900); // Frame-time update job period in nanoseconds
FIXED_SETTING(int32_t, Server, ConnectionProcessPeriodMs, 100); // Player / unlogined-player connection job re-poll period between data-arrival wakes, in milliseconds
FIXED_SETTING(int32_t, Server, CritterMovingPeriodMs, 50); // Critter movement-step job period in milliseconds
FIXED_SETTING(int32_t, Server, HealthFilePeriodMs, 300); // Health-file refresh job period in milliseconds
FIXED_SETTING(int32_t, Server, ShutdownGraceMs, 5000); // Grace period in milliseconds for in-flight worker jobs to drain on shutdown before parked entity-lock waiters are force-aborted
SETTING_GROUP_END();

#undef FIXED_SETTING
#undef VARIABLE_SETTING
#undef SETTING_GROUP
#undef SETTING_GROUP_END
