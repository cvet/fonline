# FOnline Engine : Build tools

## Build scripts

For build just run from repository root one of the following scripts:  
*Todo: finalize API and declare here*

## CMake layout

All internal CMake modules now live under `Engine/BuildTools/cmake`.
The public entry points kept at the `Engine/BuildTools` root are `Init.cmake`, `StartGeneration.cmake`, and `FinalizeGeneration.cmake`.
The validation project scaffold continues to live under `Engine/BuildTools/validation-project`.

## Build environment variables

Build scripts (sh/bat) can be called both from current directory (e.g. `./linux.sh`) or repository root (e.g. `BuildTools/linux.sh`).  
Following environment variables may be set before starting build scripts:

#### FO_ENGINE_ROOT

Path to root directory of FOnline repository.  
If not specified then taked one level outside directory of running script file (i.e. outside `BuildTools`, at repository root).  

*Default: `$(dirname ./script.sh)/../`*  
*Example: `export FO_ENGINE_ROOT=/mnt/d/fonline`*

#### FO_WORKSPACE

Path to directory where all intermediate build files will be stored.  
Default behaviour is build in current directory plus `Workspace`.  

*Default: `$PWD/Workspace`*  
*Example: `export FO_ENGINE_ROOT=/mnt/d/fonline-workspace`*

## Shared workspace preparation

Shared workspace parts are prepared through `buildtools.py` and wrapped by the platform-specific scripts.

- Linux: `Engine/BuildTools/prepare-workspace.sh`
- macOS: `Engine/BuildTools/prepare-mac-workspace.sh`
- Windows: `Engine/BuildTools/prepare-win-workspace.ps1`

At the moment the shared flow covers:

- `toolset`
- `emscripten`
- `android-ndk`
- `dotnet`

Host prerequisite checks are also available through the main tool:

- `buildtools.py host-check linux`
- `buildtools.py host-check macos`
- `buildtools.py host-check windows`

Host wrapper scripts now delegate to the unified workspace preparation command:

- `buildtools.py prepare-host-workspace linux ...`
- `buildtools.py prepare-host-workspace windows ...`
- `buildtools.py prepare-host-workspace macos ...`

Emscripten version is pinned by `Engine/ThirdParty/emscripten` and installed into `Workspace/emsdk`.

Examples:

```bash
python3 Engine/BuildTools/buildtools.py prepare-workspace toolset
python3 Engine/BuildTools/buildtools.py prepare-workspace emscripten
python3 Engine/BuildTools/buildtools.py prepare-workspace android-ndk dotnet
python3 Engine/BuildTools/buildtools.py prepare-workspace toolset emscripten android-ndk dotnet --check
python3 Engine/BuildTools/buildtools.py prepare-host-workspace linux web dotnet
```

## Windows web debug workflow

The local Windows web debug flow uses these shared commands:

- `buildtools.py build web client RelWithDebInfo`
- `buildtools.py package-web-debug`

For an optimized browser build use:

- `buildtools.py build web client Release`

The packaged browser build is emitted into `Workspace/web-debug/LF-Client-LocalTest-Web` and can be served by the generated `web-server.py` helper.

## Android debug workflow

The local Android debug flow uses these shared commands:

Supported Android platform identifiers are `android-arm32`, `android-arm64`, and `android-x86`.

Device deployment is built around ADB over Wi-Fi. The target device must have wireless debugging enabled and be paired with this host if Android requests pairing.

- `buildtools.py build android-arm64 client RelWithDebInfo`
- `buildtools.py package-android-debug LF android-arm64 LocalTest`
- `buildtools.py package-android-debug LF android-arm64 RemoteSceneLaunch`
- `android_device.py --workspace-root Workspace connect`

The Android SDK and NDK workspace parts must be prepared first:

- `bash Engine/BuildTools/prepare-workspace.sh android-arm64`

The packaged Android build is emitted into `Workspace/android-debug/LF-Client-LocalTest-Android` as a ready-to-build Gradle project. Build and deploy:

```bash
python3 Engine/BuildTools/android_device.py --workspace-root Workspace connect
cd Workspace/android-debug/LF-Client-LocalTest-Android
./gradlew assembleDebug
python3 Engine/BuildTools/android_device.py --workspace-root Workspace install --apk Workspace/android-debug/LF-Client-LocalTest-Android/app/build/outputs/apk/debug/app-debug.apk
python3 Engine/BuildTools/android_device.py --workspace-root Workspace launch --activity com.example.game/.FOnlineActivity

# Remote scene launch from host to Android device
python3 Engine/BuildTools/buildtools.py package-android-debug LF android-arm64 RemoteSceneLaunch
cd Workspace/android-debug/LF-Client-RemoteSceneLaunch-Android
./gradlew assembleDebug
python3 Engine/BuildTools/android_device.py --workspace-root Workspace install --apk Workspace/android-debug/LF-Client-RemoteSceneLaunch-Android/app/build/outputs/apk/debug/app-debug.apk
python3 Engine/BuildTools/android_device.py --workspace-root Workspace launch-game --activity com.example.game/.FOnlineActivity
```

`launch-game` auto-detects the host LAN IP that reaches the selected Wi-Fi Android device and passes it as a runtime `ClientNetwork.ServerHost` override, which makes the packaged `RemoteSceneLaunch` client connect back to the host server without editing baked config files.

At runtime, `FOnlineActivity` stages `assets/Resources` into the app files directory on first launch after install or update and then starts the engine with absolute `Baking.ClientResources` and `Baking.CacheResources` overrides that point to that runtime location.

Android SDK command-line tools version is pinned by `Engine/ThirdParty/android-sdk` and installed into `Workspace/android-sdk`.

Android NDK version is pinned by `Engine/ThirdParty/android-ndk` and installed into `Workspace/android-ndk`.

The Gradle project template lives in `Engine/BuildTools/android-project/` and uses `$PLACEHOLDER$` tokens patched by `package.py` during packaging. Configuration values come from the project main config `Android.*` settings, including the launcher icon PNG source and the Android signing settings.

Android release APK packaging signs the artifact. Configure signing through `Android.Keystore`, `Android.KeystorePassword`, `Android.KeyAlias`, and `Android.KeyPassword` in the project main config. If they are empty, packaging falls back to the Gradle debug signing key so generated package APKs remain installable on development devices. If needed, these settings can use `$ENV{...}` expressions.

`android_device.py` first tries `adb mdns services`, shows any discovered Android Wi-Fi endpoints as a numbered list, caches the selected endpoint in `Workspace/android-debug/device-endpoint.txt`, and falls back to manual `IP[:port]` entry when discovery returns nothing.
