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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#if !defined(SETTING) || !defined(SETTING_GROUP) || !defined(SETTING_GROUP_END)
#error SETTING or SETTING_GROUP or SETTING_GROUP_END not defined
#endif

///@ ExportSettings Common
SETTING_GROUP(Common, virtual BaseSettings)
SETTING(string, Common, GameName, "FOnline"); // Game name, used in logs and window title
SETTING(string, Common, GameVersion, "0.0.0"); // Game version, used in logs and window title
SETTING(string, Common, UnpackagedSubConfig); // Config applied in unpackaged builds
SETTING(bool, Common, Packaged); // If yes, then the packaging was done (read only)
SETTING(bool, Common, AsyncLogWrite, false); // If true, log messages are written to file and console from a dedicated background thread
SETTING(vector<string>, Common, SecretSettingTokens, "secret", "token", "password", "apikey"); // Case-insensitive substrings that mark a setting name as a credential whose value is masked as "***" in command-line override logs
SETTING(string, Common, UserWritablePath); // Read only. Writable data root for everything written at runtime - log, cache, resource overlay, self-updated binaries, server database. Resolved at startup before anything is read from disk and therefore NOT settable from a config file: the command line names it, otherwise an INSTALLED marker beside the executable selects the per-OS user data dir plus the project name (Windows %LOCALAPPDATA% else %APPDATA%; macOS/iOS ~/Library/Application Support; Linux/Android $XDG_DATA_HOME else ~/.local/share), and without a marker everything stays relative to the working directory. Read-only base resources stay where they are with this dir layered on top
SETTING_GROUP_END(Common);

///@ ExportSettings Common
SETTING_GROUP(AngelScript, virtual BaseSettings)
SETTING(int32_t, AngelScript, OverrunReportTime); // Time in milliseconds to report AngelScript execution or entity-lock wait overrun, 0 to disable
SETTING(bool, AngelScript, DebuggerEnabled, false); // If true, AngelScript debugger endpoint is enabled
SETTING(string, AngelScript, DebuggerBindHost, "127.0.0.1"); // AngelScript debugger TCP bind host
SETTING(vector<string>, AngelScript, MutableGlobalsAllowedNamespaces); // AngelScript namespaces (matched as prefixes) whose module-level globals may be mutable (everywhere else const-only)
SETTING(vector<string>, AngelScript, AttributedFunctionDirectCallAllowedNamespaces); // AngelScript namespaces (matched as prefixes) whose functions may call attributed functions ([[Event]], [[TimeEvent]], etc.) directly instead of going through the runtime dispatcher
SETTING(vector<string>, AngelScript, ExtraDirectCallBlockingAttributes); // Project-defined attribute names that should also block direct AngelScript-to-AngelScript calls in addition to the engine's built-in dispatcher-bound list
SETTING_GROUP_END(AngelScript);

///@ ExportSettings Common
SETTING_GROUP(ManagedScript, virtual BaseSettings)
SETTING(int32_t, ManagedScript, OverrunReportTime); // Time in milliseconds to report managed script execution or entity-lock wait overrun, 0 to disable
SETTING(bool, ManagedScript, InteropProbeOnStart, false); // If true, every native-to-managed transport the runtime can use is checked once scripts start, one INTEROP-TRANSPORT log line per check; qualifies a client no test suite runs on, such as a browser or a device
SETTING(bool, ManagedScript, DeepTrackEntityWrappers, false); // If true, every managed entity wrapper is also tracked weakly so shutdown can name the wrappers that still hold a native entity reference; costs a dictionary write per wrapper, while the live wrapper count shutdown reports is always kept
SETTING(vector<string>, ManagedScript, Assemblies, "FOnline"); // Managed script assemblies to build
SETTING(string, ManagedScript, ProjectName, "FOnline"); // Managed project base name
SETTING(string, ManagedScript, TargetFramework, "net10.0"); // Managed project target framework
SETTING(string, ManagedScript, MsBuild, "dotnet msbuild"); // Managed msbuild command
SETTING(vector<string>, ManagedScript, Dirs, "Engine/Source/Scripting/Managed/CoreScripts", "Scripts"); // Managed script source directories, top-level .cs files of each are compiled in place
SETTING(string, ManagedScript, GeneratedDir); // Directory for generated managed scripts and project files (empty targets the build GeneratedSource/Managed directory)
SETTING(vector<string>, ManagedScript, ExtraSources); // Extra managed source files as 'assembly,target,path' entries
SETTING(vector<string>, ManagedScript, ExtraReferences); // Extra managed assembly references as 'assembly,target,reference' entries
SETTING(vector<string>, ManagedScript, Analyzers); // Roslyn analyzer project paths referenced by the generated managed project, so their diagnostics run in the script build
SETTING(vector<string>, ManagedScript, AnalyzerPackages); // Roslyn analyzer NuGet packages as 'name,version' entries, version is mandatory and must be exact
SETTING(vector<string>, ManagedScript, AdditionalFiles); // Analyzer configuration file paths passed to the script compilation as AdditionalFiles
SETTING(string, ManagedScript, AnalysisLevel); // Analysis level of the generated managed project, empty keeps the SDK default
SETTING(string, ManagedScript, AnalysisMode); // Analysis mode of the generated managed project, empty keeps the SDK default
SETTING(bool, ManagedScript, BakerDryRun, false); // If true, managed baker writes stub markers instead of compiling assemblies
SETTING_GROUP_END(ManagedScript);

///@ ExportSettings Common
SETTING_GROUP(Network, virtual BaseSettings)
SETTING(int32_t, Network, ServerPort, 4000); // Server port number
SETTING(int32_t, Network, WebSocketPort, 4001); // WebSockets port for server listen and client connect
SETTING(int32_t, Network, NetBufferSize, 4096); // Network buffer size
SETTING(int32_t, Network, UdpPortOffset, 0); // UDP port offset relative to ServerPort
SETTING(int32_t, Network, UdpPacketSize, 1200); // Target UDP payload budget in bytes
SETTING(int32_t, Network, UdpWindowSize, 65536); // Reliable UDP send window size in bytes
SETTING(int32_t, Network, UdpResendTimeout, 150); // UDP packet resend timeout in milliseconds
SETTING(int32_t, Network, UdpConnectRetry, 500); // UDP connect retry timeout in milliseconds
SETTING(int32_t, Network, UdpConnectTimeout, 4000); // UDP connect timeout in milliseconds before failing transport bootstrap
SETTING(int32_t, Network, UdpSendUpdateInterval, 16); // UDP send tick interval in milliseconds
SETTING(int32_t, Network, UdpRedundancy, 1); // Number of previous UDP packets redundantly resent with new data
SETTING(int32_t, Network, UpdateFileMaxPortionSize, 1000000); // Maximum update file portion size in bytes
SETTING(bool, Network, SecuredWebSockets, false); // If true, secured WebSockets are enabled
SETTING(bool, Network, DisableTcpNagle, true); // If true, TCP Nagle algorithm is disabled
SETTING(bool, Network, DisableZlibCompression, false); // If true, Zlib compression is disabled
SETTING(bool, Network, DisableInterthreadCommunication, false); // If true, server does not register the in-process interthread listener; clients in the same process fall through to TCP/UDP
SETTING(int32_t, Network, ArtificalLags, 0); // Artificial lags in milliseconds, applied to both directions
SETTING(int32_t, Network, ArtificalLagsJitter, 0); // Extra per-batch random delay in milliseconds on top of the artificial lag, emulating stalls
SETTING(string, Network, CompatibilityVersion); // Compatibility version (read only)
SETTING(string, Network, ForceCompatibilityVersion, ""); // Custom compatability version
SETTING(string, Network, ForceMetadataVersion, ""); // Custom metadata version, for simulating a client baked apart from the server
SETTING_GROUP_END(Network);

///@ ExportSettings Server
SETTING_GROUP(ServerNetwork, virtual NetworkSettings)
SETTING(bool, ServerNetwork, DisableNetworking, false); // If true, server networking listeners are not started
SETTING(bool, ServerNetwork, EnableUdp, false); // If true, UDP listener is enabled for native clients
SETTING(bool, ServerNetwork, RejectUdpConnections, false); // If true, UDP listener silently drops incoming Connect packets so clients fall back to TCP (debug aid)
SETTING(bool, ServerNetwork, UpdateFilesInMemory, false); // If true, updater files are served from memory, otherwise from disk
SETTING(int32_t, ServerNetwork, ClientPingTime, 10000); // Client ping time in milliseconds
SETTING(int32_t, ServerNetwork, InactivityDisconnectTime, 0); // Inactivity disconnect time in milliseconds
SETTING(int32_t, ServerNetwork, LoginTimeout, 0); // Maximum pre-login connection lifetime without handshake/auth/update progress in milliseconds (0 = unlimited)
SETTING(int32_t, ServerNetwork, MaxMessageSize, 1048576); // Max single inbound message size in bytes (0 = unlimited); rejected at the header
SETTING(int32_t, ServerNetwork, MaxBufferedInputSize, 4194304); // Max total unread input bytes retained per connection (0 = unlimited)
SETTING(int32_t, ServerNetwork, MaxRemoteCallPayloadSize, 1048576); // Runtime cap for a decoded remote-call payload (0 = structural per-call cap only)
SETTING(int32_t, ServerNetwork, MaxMessagesPerProcessPass, 256); // Max messages drained per connection per worker job pass (0 = unlimited)
SETTING(int32_t, ServerNetwork, MaxUdpReorderAhead, 1024); // Max UDP packets buffered ahead of the next expected sequence (0 = unlimited)
SETTING(int32_t, ServerNetwork, MaxConnections, 0); // Max simultaneous connections incl. unlogined (0 = unlimited); over this, new connections are rejected at accept
SETTING(int32_t, ServerNetwork, MaxPlayers, 0); // Max simultaneous logined players (0 = unlimited); over this, new connections are rejected at accept
SETTING(int32_t, ServerNetwork, NewConnectionRatePerSec, 0); // Max new connections accepted per source host per second (0 = unlimited); bursts above this are dropped at accept
SETTING(int32_t, ServerNetwork, ListenRetryTime, 30000); // Time in milliseconds to keep retrying a listener whose port is still held by the previous process (0 = do not retry)
SETTING(int32_t, ServerNetwork, ListenRetryDelay, 500); // Delay in milliseconds between listener bind attempts
SETTING(string, ServerNetwork, WssPrivateKey, ""); // WebSocket Secure private key
SETTING(string, ServerNetwork, WssCertificate, ""); // WebSocket Secure certificate
SETTING_GROUP_END(ServerNetwork);

///@ ExportSettings Client
SETTING_GROUP(ClientNetwork, virtual NetworkSettings)
SETTING(string, ClientNetwork, ServerHost, "127.0.0.1"); // Server host address
SETTING(string, ClientNetwork, WebSocketHost, "localhost"); // WebSockets server host for client connect
SETTING(int32_t, ClientNetwork, PingPeriod, 2000); // Ping period in milliseconds
SETTING(bool, ClientNetwork, UseUdp, false); // If true, UDP native transport is used instead of TCP
SETTING(int32_t, ClientNetwork, ProxyType, 0); // Proxy type
SETTING(string, ClientNetwork, ProxyHost, ""); // Proxy host address
SETTING(int32_t, ClientNetwork, ProxyPort, 8080); // Proxy port number
SETTING(string, ClientNetwork, ProxyUser, ""); // Proxy username
SETTING(string, ClientNetwork, ProxyPass, ""); // Proxy password
SETTING(bool, ClientNetwork, DebugNet, false); // If true, network debugging is enabled
SETTING_GROUP_END(ClientNetwork);

///@ ExportSettings Client
SETTING_GROUP(Audio, virtual BaseSettings)
SETTING(bool, Audio, DisableAudio, false); // If true, audio is disabled
SETTING(int32_t, Audio, SoundVolume, 100); // Sound volume percentage
SETTING(int32_t, Audio, MusicVolume, 100); // Music volume percentage
SETTING(vector<string>, Audio, SoundFileExtensions, "wav", "ogg"); // Source extensions the sound-name index accepts; every baked payload is Ogg Vorbis whatever the extension says
SETTING(int32_t, Audio, MixRate, 44100); // Sample rate the engine mixes at before handing the stream to the device, which resamples if it runs at another one
SETTING_GROUP_END(Audio);

///@ ExportSettings Client
SETTING_GROUP(View, virtual BaseSettings)
SETTING(int32_t, View, ScreenWidth, 1024); // Screen width in pixels
SETTING(int32_t, View, ScreenHeight, 768); // Screen height in pixels
SETTING(int32_t, View, MonitorWidth); // Monitor width (read only)
SETTING(int32_t, View, MonitorHeight); // Monitor height (read only)
SETTING(bool, View, HideNativeCursor, false); // If true, native cursor is hidden
SETTING(int32_t, View, FadingDuration, 1000); // Fading duration in milliseconds
SETTING(bool, View, MapZoomEnabled); // Map view zooming
SETTING(bool, View, MapDirectDraw); // Draw map directly to main framebuffer (speed up rendering but disables zooming and make scrolling jumpy)
SETTING(float32_t, View, MapRenderTargetScale, 1.0f); // Fixed map/light/indoor-mask render-target scale relative to the logical screen; clamped to the device texture limit
SETTING(bool, View, DisableLighting); // Disables lighting for more performance
SETTING(bool, View, DisableFog); // Disables fog for more performance
SETTING(vector<int32_t>, View, GlobalDayColorTime, 300, 600, 1140, 1380); // Project-wide default DayColorTime in minutes (4 keys); maps may override per-map via Map.DayColorTime
SETTING(vector<uint8_t>, View, GlobalDayColor, 18, 128, 103, 51, 18, 128, 95, 40, 53, 128, 86, 29); // Project-wide default DayColor (12 bytes, channel-major: R0..R3 G0..G3 B0..B3); maps may override per-map via Map.DayColor
SETTING(bool, View, DisableIndoorMask, false); // If true, MapView skips the _rtIndoorMask render pass (saves a sprite-draw + RT swap per frame). Use for projects without weather, or to debug-see weather through hidden roofs.
SETTING_GROUP_END(View);

///@ ExportSettings Common
SETTING_GROUP(Geometry, virtual BaseSettings)
SETTING(int32_t, Geometry, MapHexWidth, 32); // Hex/square width
SETTING(int32_t, Geometry, MapHexHeight, 16); // Hex/square height
SETTING(int32_t, Geometry, MapHexLineHeight, 12); // Hex/square line height
SETTING(int32_t, Geometry, MapTileStep, 2); // Tile step value
SETTING(int32_t, Geometry, MapRoofElevation, 98); // Roof tile 3D elevation, applied to every roof tile (a roof is just a tile lifted in 3D so it draws above the walls). Replaces the old 2D MapRoofOffs* raise; the flat tile/roof XY offset now lives on the BaseTile prototype Offset
SETTING(float32_t, Geometry, MapCameraAngle, 25.6589f); // Angle for critters moving/rendering
SETTING(bool, Geometry, MapFreeMovement, false); // If true, free movement on the map is enabled
SETTING(bool, Geometry, MapSmoothPath, true); // Enable pathfinding path smoothing
SETTING(string, Geometry, MapDataPrefix, "Geometry"); // Path and prefix for names used for geometry sprites
SETTING(bool, Geometry, CritterBlockHex, false); // If true, critters block hexes
SETTING(int32_t, Geometry, MaxPathFindLength, 400); // Maximum pathfinding length
SETTING_GROUP_END(Geometry);

///@ ExportSettings Common
SETTING_GROUP(SpriteMesh, virtual BaseSettings)
SETTING(bool, SpriteMesh, Enabled, false); // If true, polygonal sprite meshes are generated during image baking
SETTING(int32_t, SpriteMesh, AlphaThreshold, 0); // Minimum alpha excluded from sprite mesh geometry (0-254)
SETTING(int32_t, SpriteMesh, MaxTriangles, 4096); // Maximum triangle budget searched across whole-sprite, per-component, and detailed mesh candidates
SETTING(float32_t, SpriteMesh, AreaSavingsWeight, 32.0f); // Score awarded for removing a full frame area; one point offsets one submitted triangle
SETTING_GROUP_END(SpriteMesh);

///@ ExportSettings Common
SETTING_GROUP(Render, virtual ViewSettings, virtual GeometrySettings)
SETTING(int32_t, Render, Animation3dSmoothTime, 150); // 3D animation smooth time in milliseconds
SETTING(int32_t, Render, Animation3dFPS, 30); // 3D animation frames per second
SETTING(string, Render, HeadBone); // Head bone name (Todo: move HeadBone to fo3d settings)
SETTING(vector<string>, Render, LegBones); // Leg bone names (Todo: move LegBones to fo3d settings)
SETTING(vector<string>, Render, ModelLayerProperties); // Critter properties feeding model layers, as "<PropertyName>=<LayerIndex>" pairs; tools use them to dress a prototype the way the game does
SETTING(bool, Render, WindowCentered, true); // If true, window is centered
SETTING(bool, Render, WindowResizable, false); // If true, window is resizable
SETTING(bool, Render, HeadlessWindow, false); // If true, window is created hidden for off-screen rendering
SETTING(bool, Render, NullRenderer, false); // If true, null renderer is used
SETTING(bool, Render, ForceOpenGL, false); // If true, OpenGL is forced
SETTING(bool, Render, ForceDirect3D, false); // If true, Direct3D is forced
SETTING(bool, Render, ForceMetal, false); // If true, Metal is forced
SETTING(bool, Render, ForceVulkan, false); // If true, Vulkan is forced
SETTING(bool, Render, ForceSDLGpu, false); // If true, SDL_GPU backend is forced
SETTING(bool, Render, ForceGlslEsProfile, false); // If true, GLSL ES profile is forced
SETTING(bool, Render, AllowSoftwareRenderer, false); // If true, a software rasterizer may stand in when no hardware device is available
SETTING(string, Render, Direct3DVertexShaderProfile, "vs_4_0_level_9_3"); // Direct3D vertex shader compile profile
SETTING(string, Render, Direct3DPixelShaderProfile, "ps_4_0_level_9_3"); // Direct3D pixel shader compile profile
SETTING(string, Render, SDLGpuDriver, ""); // Preferred SDL_GPU driver name (vulkan / metal / direct3d12), empty lets SDL pick
SETTING(bool, Render, RenderDebug, false); // If true, render debugging is enabled
SETTING(bool, Render, DrawWireframe, false); // If true, rendered sprite and particle triangle edges are overlaid for geometry debugging
SETTING(bool, Render, VSync, false); // If true, vertical synchronization is enabled
SETTING(bool, Render, AlwaysOnTop, false); // If true, window is always on top
SETTING(bool, Render, Fullscreen, false); // If true, fullscreen mode is enabled
SETTING(int32_t, Render, Brightness, 0); // Brightness value
SETTING(int32_t, Render, Sleep, -1); // Sleep duration in milliseconds (-1 to disable)
SETTING(int32_t, Render, FixedFPS, 100); // Fixed frames per second (0 to disable)
SETTING(float32_t, Render, CritterTurnAngle, 100.0f); // Critter turn angle
SETTING(float32_t, Render, CritterBodyTurnFactor, 0.6f); // Critter body turn factor
SETTING(float32_t, Render, CritterHeadTurnFactor, 0.4f); // Critter head turn factor
SETTING(int32_t, Render, WalkAnimBaseSpeed, 60); // Walk animation base speed
SETTING(int32_t, Render, RunAnimStartSpeed, 80); // Run animation start speed
SETTING(int32_t, Render, RunAnimBaseSpeed, 120); // Run animation base speed
SETTING(float32_t, Render, ModelProjFactor, 40.0f); // Screen px per 3D world unit (1 unit = 1 hex = 1 m); scales 3D models and in-scene particles
SETTING(int32_t, Render, ModelSpriteMaxTextureWidth, 2048); // Max GPU scratch-texture width a 3D model sprite may allocate; larger envelopes clamp and draw cropped
SETTING(int32_t, Render, ModelSpriteMaxTextureHeight, 2048); // Max GPU scratch-texture height a 3D model sprite may allocate; larger envelopes clamp and draw cropped
SETTING(bool, Render, ModelDirectDraw, false); // If true, map 3D models render directly into the scene depth buffer; otherwise they render as cached atlas sprites
SETTING(int32_t, Render, MapMaxElevation, 4096); // Max abs sprite elevation (px) used to size the per-map scene depth range; smaller = more depth-buffer precision (less z-fighting), but sprites beyond it would be depth-clipped
SETTING(int32_t, Render, EggEllipseWidthExt, 0); // Transparency egg ellipse extra width in pixels added to logical sprite/view width
SETTING(int32_t, Render, EggEllipseHeightExt, 0); // Transparency egg ellipse extra height in pixels added to logical sprite/view height
SETTING(float32_t, Render, EggTransparencyTransitionFactor, 0.9f); // Fraction of egg mask kept fully transparent before linear fade to opaque
SETTING(bool, Render, AtlasLinearFiltration, false); // If true, atlas linear filtration is enabled
SETTING(int32_t, Render, DefaultParticleDrawWidth, 128); // Default particle draw width
SETTING(int32_t, Render, DefaultParticleDrawHeight, 128); // Default particle draw height
SETTING(bool, Render, RecreateClientOnError, false); // If true, client is recreated on error
SETTING(string, Render, ImGuiColorStyle); // ImGui theme: Light, Classic, Dark
SETTING(string, Render, ImGuiDefaultEffect, "Effects/ImGui_Default.fofx"); // Shader effect for ImGui
SETTING(int32_t, Render, ImGuiFontTextureSize, 256); // Minimum ImGui texture size
SETTING(int32_t, Render, SpriteHitValue, 127); // If alpha is greather then hit test is passed
SETTING(string, Render, CritterStubSpriteName, ""); // Fallback critter sprite, empty value uses built-in runtime dummy sprite
SETTING(string, Render, ItemStubSpriteName, ""); // Fallback item sprite, empty value uses built-in runtime dummy sprite
SETTING_GROUP_END(Render);

///@ ExportSettings Client
SETTING_GROUP(Web, virtual RenderSettings)
SETTING(bool, Web, AutoResize, true); // If true, web screen resolution is recalculated on browser window resize
SETTING(string, Web, LoadingImage, ""); // Loading image path for web page, relative to main fomain file
SETTING(string, Web, BackgroundColor, "rgb(0, 0, 0)"); // Background color for the web page around the game screen
SETTING(int32_t, Web, ScreenHeightPercent, 75); // Web canvas height as a percentage of page height
SETTING(float32_t, Web, AspectFactor, 0.75f); // Web canvas height-to-width factor, width = height / factor
SETTING(float32_t, Web, HorizontalPosFactor, 0.5f); // Horizontal position factor in free page width, 0 = left, 0.5 = center, 1 = right
SETTING(float32_t, Web, VerticalPosFactor, 0.25f); // Vertical position factor in free page height, 0 = top, 0.5 = center, 1 = bottom
SETTING(int32_t, Web, MinWidth, 1024); // Minimum adaptive web width in pixels
SETTING(int32_t, Web, MinHeight, 768); // Minimum adaptive web height in pixels
SETTING(int32_t, Web, MaxWidth, 1920); // Maximum adaptive web width in pixels
SETTING(int32_t, Web, MaxHeight, 1080); // Maximum adaptive web height in pixels
SETTING_GROUP_END(Web);

///@ ExportSettings Server
SETTING_GROUP(Android, virtual BaseSettings)
SETTING(string, Android, PackageName, "com.fonline.app"); // Android application package name for packaging tools
SETTING(int32_t, Android, VersionCode, 1); // Android application version code for packaging tools
SETTING(int32_t, Android, MinSdk, 23); // Android minimum supported SDK level for packaging tools
SETTING(int32_t, Android, TargetSdk, 35); // Android target SDK level for packaging tools
SETTING(int32_t, Android, CompileSdk, 35); // Android compile SDK level for packaging tools
SETTING(string, Android, ScreenOrientation, "landscape"); // Android activity screen orientation for packaging tools
SETTING(string, Android, Icon, "Engine/Resources/Radiation.png"); // Android launcher icon PNG source path for packaging tools
SETTING(string, Android, Keystore, ""); // Android release signing keystore path for packaging tools
SETTING(string, Android, KeystorePassword, ""); // Android release signing keystore password for packaging tools
SETTING(string, Android, KeyAlias, ""); // Android release signing key alias for packaging tools
SETTING(string, Android, KeyPassword, ""); // Android release signing key password for packaging tools
SETTING_GROUP_END(Android);

///@ ExportSettings Server
SETTING_GROUP(Packaging, virtual AndroidSettings, virtual WebSettings)
SETTING(string, Packaging, CodeSigningHook, ""); // Windows code signing hook executable path for packaging tools
SETTING(string, Packaging, AppIcon, ""); // Windows application ICO source path for packaging tools
SETTING(string, Packaging, MsiUpgradeCode, ""); // Windows MSI installer upgrade GUID for packaging tools, must never change once an MSI has shipped
SETTING_GROUP_END(Packaging);

///@ ExportSettings Common
SETTING_GROUP(Timer, virtual BaseSettings)
SETTING(int32_t, Timer, DeltaTimeCap, 100); // Frame delta time cap in milliseconds
SETTING_GROUP_END(Timer);

///@ ExportSettings Common
SETTING_GROUP(Baking, virtual AngelScriptSettings, virtual ManagedScriptSettings, virtual SpriteMeshSettings, virtual BaseSettings)
SETTING(bool, Baking, ForceBaking, false); // If true, baking of all packs are forced
SETTING(bool, Baking, SingleThreadBaking, false); // If true, single-threaded baking is enabled
SETTING(bool, Baking, IgnoreMissingBakerWarning, false); // If true, missing baker warning is suppressed when stale baked resources already exist
SETTING(vector<string>, Baking, IgnoreInputDirs); // Resource pack input directories left out of every pack, relative to the config declaring the pack; each must be an input of some pack
SETTING(bool, Baking, PreciseModelBounds, false); // If true, model bounds measure every skinned vertex; if false, per-bone envelope boxes produce a conservative superset far faster
SETTING(float32_t, Baking, ModelAttachmentMinExtent, 0.01f); // Smallest world extent a directly attached model may reach; below it the export sits in a smaller unit space than the engine
SETTING(float32_t, Baking, ModelAttachmentMaxExtent, 50.0f); // Largest world extent a directly attached model may reach; above it the export sits in a larger unit space and the client cannot build its sprite frame
SETTING(vector<string>, Baking, RawCopyFileExtensions, "fofnt", "bmfc", "fnt", "ogv", "json", "ini"); // Raw copy file extensions
SETTING(float32_t, Baking, AudioVorbisQuality, 0.5f); // Vorbis VBR quality audio baking encodes with, from -0.1 (smallest) to 1.0 (largest)
SETTING(vector<string>, Baking, ProtoFileExtensions, "fopro"); // Authored proto container extensions
SETTING(bool, Baking, AllowRepeatedProtoParents, true); // If false, a prototype reaching one ancestor through several $Parent entries fails baking; when allowed, a repeated ancestor contributes its fields only where it is first reached
SETTING(vector<string>, Baking, BakeLanguages, "engl"); // Bake languages
SETTING(string, Baking, BakeOutput, "Baking"); // Bake output directory
SETTING(string, Baking, ServerResources, "ServerResources"); // Server resources directory
SETTING(string, Baking, ClientResources, "Resources"); // Client resources directory
SETTING(string, Baking, PlatformBinaries, "PlatformBinaries"); // Per-platform client runtime binaries directory served by the updater
SETTING(string, Baking, CacheResources, "Cache"); // Cache resources directory
SETTING(int32_t, Baking, ZipCompressLevel, 1); // Zip deflate level (0-9)
SETTING(vector<string>, Baking, BootstrapGameSettings); // Game settings an embedding project consumes before metadata is available (ApplicationInitHook and earlier); written to the binary config in full instead of as a sub-config delta
SETTING_GROUP_END(Baking);

///@ ExportSettings Server
SETTING_GROUP(Critter, virtual TimerSettings, virtual NetworkSettings, virtual GeometrySettings)
SETTING(vector<bool>, Critter, CritterSlotEnabled, true, true); // Critter slot enabled flags
SETTING(vector<bool>, Critter, CritterSlotSendData, false, true); // Critter slot send data flags
SETTING(vector<bool>, Critter, CritterSlotMultiItem, true, false); // Critter slot multi-item flags
SETTING_GROUP_END(Critter);

///@ ExportSettings Client
SETTING_GROUP(CritterView, virtual ViewSettings, virtual GeometrySettings, virtual TimerSettings)
SETTING(int32_t, CritterView, NameOffset, 0); // Name offset value
SETTING_GROUP_END(CritterView);

///@ ExportSettings Client
SETTING_GROUP(Hex, virtual ViewSettings, virtual GeometrySettings, virtual CritterViewSettings)
SETTING(int32_t, Hex, ScrollFixedDt, 10); // Scroll fixed delta time in milliseconds
SETTING(int32_t, Hex, ScrollSpeed, 1200); // Scroll speed in pixels per second
SETTING(int32_t, Hex, ZoomSpeed, 100); // Speed of zooming
SETTING(bool, Hex, FullscreenMouseScroll, true); // If true, fullscreen mouse scroll is enabled
SETTING(bool, Hex, WindowedMouseScroll, false); // If true, windowed mouse scroll is enabled
SETTING(int32_t, Hex, ScrollBlockSize, 1); // Blocked hexes around scroll block line
SETTING_GROUP_END(Hex);

///@ ExportSettings Server
SETTING_GROUP(DataBase, virtual CommonSettings)
SETTING(bool, DataBase, OpLogEnabled, true); // If true, pending database changes are mirrored to oplog for recovery; if false, any database write failure causes immediate panic
SETTING(string, DataBase, OpLogPath, "DbPendingChanges.oplog"); // Pending database changes oplog path; committed replay progress path is derived as this value plus -committed
SETTING(int32_t, DataBase, ReconnectRetryPeriod, 1000); // Database backend retry period in milliseconds while oplog mode is active
SETTING(int32_t, DataBase, PanicOpLogSizeThreshold, 10000000); // Pending database changes file size threshold in bytes after which panic is triggered
SETTING(int32_t, DataBase, PanicShutdownTimeout, 5000); // Graceful shutdown timeout in milliseconds after database panic callback before forced termination
SETTING(int32_t, DataBase, JsonIndent, 4); // JSON backend indentation, 0 for compact output
SETTING(string, DataBase, MongoEscapeChar, ":"); // Single character used to replace dots in Mongo document keys
SETTING(vector<string>, DataBase, CustomCollections); // Space-separated user-defined database collections in format CollectionName:Int or CollectionName:Str
SETTING_GROUP_END(DataBase);

///@ ExportSettings Client
SETTING_GROUP(Input, virtual BaseSettings)
SETTING(bool, Input, DisableGamepad, false); // If true, gamepad support is disabled
SETTING(int32_t, Input, ConsoleHistorySize, 100); // Console history size
SETTING_GROUP_END(Input);

///@ ExportSettings Client
SETTING_GROUP(Mapper, virtual BakingSettings)
SETTING(string, Mapper, StartMap, ""); // Start map name
SETTING(int32_t, Mapper, StartHexX, -1); // Start hex X coordinate
SETTING(int32_t, Mapper, StartHexY, -1); // Start hex Y coordinate
SETTING(string, Mapper, ParticlePreviewEffect, ""); // Particle resource to preview after opening the start map
SETTING(int32_t, Mapper, ParticlePreviewSeed, 0); // Seed used when the selected runtime supports seeded startup preview
SETTING(float32_t, Mapper, ParticlePreviewScale, 1.0f); // Scale for the startup particle preview
SETTING(bool, Mapper, ParticlePreviewPrewarm, false); // If true, prewarm the startup particle preview
SETTING_GROUP_END(Mapper);

///@ ExportSettings Client
SETTING_GROUP(Client, virtual CommonSettings, virtual AngelScriptSettings, virtual ManagedScriptSettings, virtual BakingSettings, virtual ClientNetworkSettings, virtual AudioSettings, virtual ViewSettings, virtual RenderSettings, virtual GeometrySettings, virtual TimerSettings, virtual HexSettings, virtual InputSettings, virtual CritterViewSettings, virtual MapperSettings, virtual WebSettings)
SETTING(int32_t, Client, UpdaterInfoDelay, 1000); // Updater info delay in milliseconds
SETTING(int32_t, Client, UpdaterInfoPos, 0); // Updater info position (<1 - top, 0 - center, >1 - bottom)
SETTING(string, Client, DefaultSplash); // Default splash screen
SETTING(string, Client, DefaultSplashPack); // Default splash pack
SETTING(bool, Client, ClientPropertiesPackData, true); // If true, client entities with prototypes use overlay property storage
SETTING(bool, Client, ForceEmbeddedRuntime, false); // If true, the client host runs the embedded engine instead of loading the bundled runtime DLL first (an explicit --ClientLibPath still loads a DLL); read from the command line at host startup, before settings are otherwise resolved
SETTING(string, Client, Language, "engl"); // Language setting
SETTING_GROUP_END(Client);

///@ ExportSettings Server
SETTING_GROUP(Server, virtual CommonSettings, virtual AngelScriptSettings, virtual ManagedScriptSettings, virtual BakingSettings, virtual ServerNetworkSettings, virtual AudioSettings, virtual RenderSettings, virtual GeometrySettings, virtual TimerSettings, virtual CritterSettings, virtual DataBaseSettings, virtual WebSettings, virtual PackagingSettings)
SETTING(string, Server, DbStorage, "Memory"); // Database storage type
SETTING(bool, Server, ServerPropertiesPackData, true); // If true, server entities with prototypes use overlay property storage
SETTING(bool, Server, NoStart, false); // If true, server start is disabled
SETTING(int32_t, Server, AutoStartClientOnServer, 0); // Number of embedded clients to auto-spawn on server startup; 0 disables the feature
SETTING(int32_t, Server, ServerWidth, 800); // Server window width in pixels (test bench host before any client is spawned)
SETTING(int32_t, Server, ServerHeight, 600); // Server window height in pixels (test bench host before any client is spawned)
SETTING(bool, Server, CollapseLogOnStart, false); // If true, log is collapsed on start
SETTING(int32_t, Server, MaxServerLogLines, 1000); // Maximum server log lines in UI
SETTING(int32_t, Server, LockMaxWaitTime, 100); // Maximum lock wait time in milliseconds
SETTING(int32_t, Server, WorkerThreads, 0); // Worker thread count for entity processing (0 = auto)
SETTING(bool, Server, SingleThreadedLogic, false); // If true, all game logic runs on one worker thread and scripts need no explicit entity synchronization
SETTING(bool, Server, WriteHealthFile, false); // If true, health file is written
SETTING(bool, Server, ProtoMapStaticGrid, false); // If true, proto map static grid is enabled
SETTING(bool, Server, MapInstanceStaticGrid, false); // If true, map instance static grid is enabled
SETTING(int64_t, Server, EntityStartId, 10000000001); // Entity start ID
SETTING(int64_t, Server, EntityIdReserveBatch, 1000); // Entity IDs reserved per persisted-counter bump, so a new entity does not force a DB write of the last-id marker every time
SETTING(int32_t, Server, SyncPeriodMs, 10); // Sync-point job period in milliseconds (100 FPS by default)
SETTING(int32_t, Server, FrameTimePeriodNs, 900); // Frame-time update job period in nanoseconds
SETTING(int32_t, Server, ConnectionProcessPeriodMs, 100); // Player / unlogined-player connection job re-poll period between data-arrival wakes, in milliseconds
SETTING(int32_t, Server, CritterMovingPeriodMs, 50); // Critter movement-step job period in milliseconds
SETTING(int32_t, Server, HealthFilePeriodMs, 300); // Health-file refresh job period in milliseconds
SETTING(int32_t, Server, ShutdownGraceMs, 5000); // Grace period in milliseconds for in-flight worker jobs to drain on shutdown before parked entity-lock waiters are force-aborted
SETTING_GROUP_END(Server);

#undef SETTING
#undef SETTING_GROUP
#undef SETTING_GROUP_END
