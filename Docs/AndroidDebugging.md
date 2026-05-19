# Android Debugging

> Engine-owned documentation. Paths under `../` are relative to the FOnline engine root. Paths under `../../` point to an embedding game project such as Last Frontier when this engine is used as a submodule.

Practical reference for building, packaging, installing, and remote-scene debugging the Android client.

## Workflow Overview

Android debug work is split into four layers:

1. prepare the Android SDK/NDK workspace parts;
2. build the Android client native library;
3. package a Gradle project with baked resources;
4. build/install/launch the APK on a Wi-Fi ADB device.

The current supported platform identifiers are `android-arm32`, `android-arm64`, and `android-x86`. Most local tasks use `android-arm64`. Official package targets currently emit Android APKs only for the production-like `Daily`, `Staging`, and `Prod` packages; `AndroidTest` remains a raw LocalTest client package.

The high-level command flow from the repo root is:

```bash
bash ../BuildTools/prepare-workspace.sh android-packages android-arm64 # fresh Linux host
# or, when system packages are already present:
bash ../BuildTools/prepare-workspace.sh android-arm64
python3 ../BuildTools/buildtools.py build android-arm64 client RelWithDebInfo
python3 ../BuildTools/buildtools.py package-android-debug LF android-arm64 LocalTest
python3 ../BuildTools/android_device.py --workspace-root Workspace connect
cd Workspace/android-debug/LF-Client-LocalTest-Android
./gradlew assembleDebug
python3 ../BuildTools/android_device.py --workspace-root Workspace install --apk Workspace/android-debug/LF-Client-LocalTest-Android/app/build/outputs/apk/debug/app-debug.apk
python3 ../BuildTools/android_device.py --workspace-root Workspace launch --activity com.lastfrontier.app/.FOnlineActivity
python3 ../BuildTools/android_device.py --workspace-root Workspace logcat
```

## VS Code Task Flow

The active Linux VS Code Android launch tasks are thin wrappers around the same BuildTools entry points:

- `Android :: Prepare Workspace`
- `Android :: Build Client`
- `Android :: Build Debug Package`
- `Android :: Build APK`
- `Android :: Connect Device`
- `Android :: Install APK`
- `Android :: Launch App`
- `Android :: Logs App`

`Android :: Prepare Launch [linux]` runs the standard sequence for a `LocalTest` APK.

`Android :: Launch Remote Scene [linux]` runs the remote-scene sequence: select scene, prepare workspace, bake resources, build `LF_ServerHeadless`, build/package/install `RemoteSceneLaunch`, stop older scene servers on the gameplay ports, start a fresh headless scene server, stop the app, and launch it with a server-host override.

## Remote Scene Launch

Android scene debugging uses the `RemoteSceneLaunch` subconfig rather than the embedded-client `SceneLaunch` path.

Important differences:

- the server is `LF_ServerHeadless` on the host;
- the Android client is a separate device process;
- `Server.AutoStartClientOnServer` stays disabled;
- `android_device.py launch-game` passes `ClientNetwork.ServerHost` as an Android activity extra;
- if `--server-host` is omitted, `android_device.py` auto-detects the host LAN IP by checking the route to the selected Wi-Fi device.

Use `Android :: Launch Remote Scene [linux]` after compatibility-affecting changes. Launching only the already installed APK can leave an older client talking to a newer scene server and produce `Client outdated` style failures.

## Official Package Targets

The local debug flow above uses `package-android-debug` and then Gradle `assembleDebug` from `Workspace/android-debug/...`. The CI/release package flow is different: `MakePackage-Daily`, `MakePackage-Staging`, and `MakePackage-Prod` are generated from `../../CMakeLists.txt` `DefinePackage(...)` entries and include `BINARY Client Android arm64 Apk` alongside the Windows, Web, and Linux server artifacts.

CI prepares Android prerequisites only for those three package types before running the matching `../BuildTools/toolset.sh MakePackage-<type>` target. The generated package lands under `Workspace/output/LF-<type>` and includes the installable Android arm64 APK produced through the shared `../BuildTools/package.py` Android packager. The official packager invokes Gradle with `--no-daemon` so concurrent package jobs on the same self-hosted runner do not reuse or kill each other's Gradle daemon.


## Source paths inspected

- `../BuildTools/buildtools.py`
- `../BuildTools/android_device.py`
- `../BuildTools/package.py`
- `../BuildTools/android-project/`
- `../BuildTools/android-project/app/src/main/java-template/FOnlineActivity.java`
- `../ThirdParty/android-sdk`
- `../ThirdParty/android-ndk`
- `../../.vscode/tasks.json`
- `../../CMakeLists.txt`
- `../../.github/workflows/ci.yml`
- `../../LastFrontier.fomain`

Use this split when debugging Android output:

- **LocalTest/RemoteSceneLaunch device debugging** -> inspect `package-android-debug`, `Workspace/android-debug/LF-Client-*-Android`, Wi-Fi ADB, and the VS Code Android tasks.
- **Daily/Staging/Prod APK artifact issue** -> inspect `../../CMakeLists.txt` package definitions, `.github/workflows/ci.yml` package matrix, `MakePackage-<type>`, and `Workspace/output/LF-<type>` rather than the local debug Gradle directory first.
- **AndroidTest package issue** -> remember it is defined as `BINARY Client Android arm64 Raw`, not an APK-producing package target.

## Runtime Resource Copy

Android packaging moves baked client resources into the Gradle project under `app/src/main/assets/Resources`.

On first launch after install/update, `FOnlineActivity` copies those assets into the app files directory and starts the engine with absolute overrides for:

- `Baking.ClientResources`
- `Baking.CacheResources`

The activity tracks an `.asset_revision` based on Android package metadata and recopies resources when the installed package changes.

## Practical Debugging Notes

When Android launch behavior fails, isolate the failing layer before rebuilding the whole stack:

- **workspace prepare fails before build** -> check `../ThirdParty/android-sdk`, `../ThirdParty/android-ndk`, and whether `Workspace/android-sdk` / `Workspace/android-ndk` were prepared by `prepare-workspace.sh`.
- **Gradle project exists but APK build fails** -> inspect `Workspace/android-debug/LF-Client-*-Android/local.properties`; the VS Code tasks fall back to `Workspace/android-sdk` and then `/usr/lib/android-sdk` if needed.
- **APK install is canceled on device** -> rerun `android_device.py install` after approving Android wireless debugging or unknown-app-install prompts on the device.
- **device is not found** -> run `android_device.py discover` / `connect`; the helper uses `adb mdns services`, caches `Workspace/android-debug/device-endpoint.txt`, and falls back to manual `IP[:port]` input.
- **RemoteSceneLaunch app cannot connect back to host** -> check the `launch-game` `ClientNetwork.ServerHost` override, the selected Wi-Fi route, and whether host ports `4025`/`4026` are already occupied by a stale server.
- **scene launch opens the wrong scene** -> inspect the selected `startupSceneName`, `LF_ServerHeadless --ApplySubConfig RemoteSceneLaunch --Scene.Startup <SceneId>`, and the installed package path.
- **resources are stale after reinstall** -> verify the APK was rebuilt from the expected `Workspace/android-debug/LF-Client-*-Android` directory and that `FOnlineActivity` recopied assets by checking app logs for resource-copy failures.
- **packaging fails on icon or signing** -> inspect `Android.Icon`, `Android.Keystore`, `Android.KeystorePassword`, `Android.KeyAlias`, and `Android.KeyPassword` in `../../LastFrontier.fomain`; icon input must be a PNG, and partial signing config is invalid.
- **CI package contains Windows/Web/Linux artifacts but no Android APK** -> check whether the package is `Daily`, `Staging`, or `Prod`; CI prepares `android-arm64` only for those matrix types, and only package definitions with `BINARY Client Android arm64 Apk` emit APKs.

## Key Files and Integration Points

If you need to trace the Android debug flow through the live repository, start with these files:

- `README.md` - concise repo-front-door Android command flow that this detailed device/debug guide expands
- `../../CMakeLists.txt` - `AndroidTest`, `Daily`, `Staging`, and `Prod` package definitions, including which package targets emit `Android arm64 Apk` artifacts
- `.github/workflows/ci.yml` - package matrix and Android workspace preparation gate for `Prod`, `Staging`, and `Daily`
- `../../.vscode/tasks.json` - live Android task graph for workspace prep, package build, install, remote-scene server startup, and app launch
- `../BuildTools/buildtools.py` - Android platform identifiers, workspace feature mapping, and `package-android-debug` entry point
- `../BuildTools/android_device.py` - Wi-Fi ADB discovery, connection caching, install, launch, `launch-game`, stop, and logcat helper
- `../BuildTools/package.py` - Android Gradle project generation, resource movement, icon/signing config, and APK build integration used by both debug and package targets
- `../BuildTools/android-project/` - Gradle and activity template patched by the packager
- `../BuildTools/android-project/app/src/main/java-template/FOnlineActivity.java` - runtime resource copy and `ClientNetwork.ServerHost` argument forwarding
- `../../LastFrontier.fomain` - `Android.*`, `LocalTest`, and `RemoteSceneLaunch` config values used by packaging and launch
- `BuildAndLaunch.md` and `Docs/Scenes.md` - companion references for general launch selection and scene-debug behavior

## Validation and Tests

Current checks worth running when Android build, packaging, or launch docs change:

- `../../.vscode/tasks.json` confirms the current task names, package paths, APK paths, and remote-scene sequence.
- `README.md` remains the front-door summary for the concise Android command flow; this guide owns detailed Wi-Fi ADB, Gradle project, package-output, and remote-scene troubleshooting behavior.
- `../../CMakeLists.txt` confirms which official package targets emit Android APKs, especially `Daily`, `Staging`, and `Prod` with `BINARY Client Android arm64 Apk`.
- `.github/workflows/ci.yml` confirms CI prepares `android-arm64` only for the APK-producing package matrix types.
- `../BuildTools/android_device.py` confirms the current helper commands and failure messages around Wi-Fi ADB discovery/installation.
- `../BuildTools/package.py` confirms Android config keys, icon requirements, signing behavior, and resource movement into APK assets.
- `FOnlineActivity.java` confirms runtime resource copy and `ClientNetwork.ServerHost` override handling.

## See Also

- [BuildAndLaunch.md](../../Docs/BuildAndLaunch.md) â€” general build and launch entry points
- [Scenes.md](../../Docs/Scenes.md) â€” `SceneLaunch` vs `RemoteSceneLaunch` behavior
- [WebDebugging.md](WebDebugging.md) â€” sibling remote-scene workflow for browser clients
- [GuiSystem.md](../../Docs/GuiSystem.md) and [Localization.md](../../Docs/Localization.md) â€” generated-screen and text-presentation references when APK/device startup is healthy but UI output is wrong
