# Android Debugging

> Engine-owned documentation for the reusable Android build, package, device, and runtime path. Project-specific package names, launch profiles, CI jobs, and VS Code tasks belong in the embedding project's documentation.

Use this guide to build an Android client, generate a Gradle project, install it over Wi-Fi ADB, pass a development server address, and isolate failures by layer.

## Ownership boundary

The engine owns:

- Android platform identifiers and SDK/NDK workspace preparation;
- native client build orchestration;
- Gradle project generation and the `FOnlineActivity` template;
- Android package configuration keys;
- APK resource staging and runtime overrides;
- the Wi-Fi ADB helper used to discover, install, launch, stop, and inspect an app.

An embedding project owns:

- its development name, application id, display name, icon, and signing credentials;
- the sub-config used by the packaged client;
- server startup and scene-selection policy;
- package definitions and CI matrices;
- editor tasks that compose the engine commands below.

Do not copy a game's binary names or task graph into an engine procedure. In the examples, replace `<ProjectDevName>`, `<Config>`, and `<application-id>` with values from the embedding project.

## Supported targets and prerequisites

The shared BuildTools accept `android-arm32`, `android-arm64`, and `android-x86`; `android-arm64` is the normal device target. Android packaging is driven from a Linux host with Java, Gradle prerequisites, the Android SDK, and the Android NDK.

From an embedding-project root where the engine checkout is `Engine/`, prepare the SDK/NDK workspace parts:

```bash
# Fresh host: install system packages and prepare the pinned SDK/NDK.
bash Engine/BuildTools/prepare-workspace.sh android-packages android-arm64

# Host packages already exist: prepare only the workspace parts.
bash Engine/BuildTools/prepare-workspace.sh android-arm64
```

The SDK and NDK versions are pinned by `ThirdParty/android-sdk` and `ThirdParty/android-ndk`. The prepared copies live under `Workspace/android-sdk` and `Workspace/android-ndk`.

## Build and package a debug client

Build the native client and generate a Gradle project from a project sub-config:

```bash
python3 Engine/BuildTools/buildtools.py build android-arm64 client RelWithDebInfo
python3 Engine/BuildTools/buildtools.py package-android-debug <ProjectDevName> android-arm64 <Config>
```

The generated project is written to:

```text
Workspace/android-debug/<ProjectDevName>-Client-<Config>-Android
```

Build its debug APK:

```bash
cd Workspace/android-debug/<ProjectDevName>-Client-<Config>-Android
./gradlew assembleDebug
```

The result is `app/build/outputs/apk/debug/app-debug.apk` below that generated project. Treat the generated Gradle tree as build output; change the engine template or project configuration instead of maintaining edits there.

## Connect, install, and launch

Enable wireless debugging on the Android device and pair it with the host if Android requests pairing. The helper first tries `adb mdns services`, lets the user select a discovered endpoint, caches it in `Workspace/android-debug/device-endpoint.txt`, and falls back to manual `IP[:port]` input.

```bash
python3 Engine/BuildTools/android_device.py --workspace-root Workspace connect
python3 Engine/BuildTools/android_device.py --workspace-root Workspace install \
  --apk Workspace/android-debug/<ProjectDevName>-Client-<Config>-Android/app/build/outputs/apk/debug/app-debug.apk
python3 Engine/BuildTools/android_device.py --workspace-root Workspace launch \
  --activity <application-id>/.FOnlineActivity
python3 Engine/BuildTools/android_device.py --workspace-root Workspace logcat
```

Use `stop` before replacing or relaunching an app whose old process is still alive. Run each subcommand with `--help` for its activity, package, endpoint, and log-filter arguments.

## Connect the device client to a host server

The `launch-game` subcommand launches the activity and adds a `ClientNetwork.ServerHost` runtime override. If `--server-host` is omitted, the helper derives the host LAN address from the route to the selected Wi-Fi device.

```bash
python3 Engine/BuildTools/android_device.py --workspace-root Workspace launch-game \
  --activity <application-id>/.FOnlineActivity
```

The embedding project's `<Config>` must already describe a network client compatible with the server being run on the host. The project also owns server startup, ports, authentication, and startup-scene selection. Repackage after compatibility-affecting engine or script API changes; relaunching an old APK against a new server is not a valid test.

## Runtime resource staging

`BuildTools/package.py` places baked client resources under `app/src/main/assets/Resources`. On first launch after install or update, `FOnlineActivity` copies those assets into the application's files directory and starts the engine with absolute overrides for:

- `Baking.ClientResources`;
- `Baking.CacheResources`.

An `.asset_revision` derived from Android package metadata determines when assets must be copied again. If an APK starts but content is stale, confirm that the expected APK was installed and inspect logcat for resource-copy failures before debugging gameplay code.

## Android package configuration

The generated Gradle project consumes Android settings from the selected project config:

| Setting | Purpose |
|---|---|
| `Android.PackageName` | Java/application id used for installation and activity launch. |
| `Android.VersionCode` | Integer Android package version code. |
| `Android.MinSdk`, `Android.TargetSdk`, `Android.CompileSdk` | Generated Gradle SDK levels. |
| `Android.ScreenOrientation` | Activity screen orientation; defaults to `landscape`. |
| `Android.Icon` | PNG icon copied into generated Android resources. |
| `Android.ManifestMetaData.<name>` | Additional non-empty `<meta-data>` entries under `<application>`. |
| `Android.GradleMavenRepository.<name>` | Package-specific Maven repository. |
| `Android.GradleDependency.<name>` | Package-specific Gradle dependency statement. |
| `Android.JavaSource.<name>` | Additional `.java` source copied into the generated application namespace. |
| `Android.Keystore`, `Android.KeystorePassword`, `Android.KeyAlias`, `Android.KeyPassword` | Release-signing configuration. |

Additional Java sources cannot replace `FOnlineActivity.java`. During packaging, `$PACKAGE$` and `$CONFIG$` placeholders in those sources are replaced with the generated package namespace and selected config.

Release signing secrets may use `$ENV{...}` expressions. `package.py` passes passwords to Gradle through `FO_ANDROID_RELEASE_STORE_PASSWORD` and `FO_ANDROID_RELEASE_KEY_PASSWORD` instead of writing them into the generated project. If signing settings are empty, a development APK uses the Gradle debug key and is not a production release artifact.

## Release package integration

The local `package-android-debug` flow always generates a debuggable Gradle project. Release packaging is selected by the embedding project's `DefinePackage(...)` declarations. A package entry that requests `BINARY Client Android arm64 Apk` produces an APK through the same shared packager.

The project CI job responsible for such a package must prepare the Android SDK/NDK workspace before invoking the package target. Whether a particular release includes Android is project policy, not an engine guarantee.

## Troubleshooting by layer

| Symptom | Check first |
|---|---|
| Workspace preparation fails | Host packages, pinned SDK/NDK descriptors, and `Workspace/android-sdk` / `Workspace/android-ndk`. |
| Native client build fails | The selected Android platform and the normal BuildTools build output before opening Gradle. |
| Gradle cannot find the SDK | The generated project's `local.properties` and the prepared `Workspace/android-sdk`. |
| Gradle dependency resolution fails | The selected config's `Android.GradleMavenRepository.*` and `Android.GradleDependency.*` entries. |
| Packaging rejects icon or signing data | PNG validity, complete signing tuple, non-empty metadata values, and environment-backed secrets. |
| Device discovery fails | Wireless debugging, host/device network reachability, `discover`, then manual `IP[:port]`. |
| Install is cancelled | Device-side confirmation and permission to install the debug package. |
| Activity does not launch | Exact `Android.PackageName`, `.FOnlineActivity`, and the installed package reported by ADB. |
| Client cannot reach the host | `launch-game` host override, selected route, host firewall, listening address, and project-owned server ports. |
| Installed content is stale | APK timestamp/path, `.asset_revision`, and `FOnlineActivity` copy diagnostics in logcat. |

## Source paths inspected

- `BuildTools/buildtools.py`
- `BuildTools/prepare-workspace.sh`
- `BuildTools/android_device.py`
- `BuildTools/package.py`
- `BuildTools/android-project/`
- `BuildTools/android-project/app/src/main/java-template/FOnlineActivity.java`
- `Source/Common/Settings.inc`
- `ThirdParty/android-sdk`
- `ThirdParty/android-ndk`

## Validation checklist

1. `python3 BuildTools/buildtools.py package-android-debug --help` lists all supported Android platforms and positional arguments.
2. `python3 BuildTools/android_device.py --help` lists discovery, connection, install, launch, host-override, stop, and logcat commands.
3. The generated project builds with `./gradlew assembleDebug` using the prepared SDK/NDK.
4. Install and launch are tested with project-owned placeholder values replaced by real configuration.
5. Release-package claims are checked against the embedding project's `DefinePackage(...)` and CI wiring rather than copied into this page.

## See also

- [EmbeddingProject.md](EmbeddingProject.md) for the engine/project ownership boundary.
- [BuildWorkflow.md](BuildWorkflow.md) for build and package entry points.
- [BuildToolsPipeline.md](BuildToolsPipeline.md) for BuildTools orchestration.
- [WebDebugging.md](WebDebugging.md) for the sibling remote-client platform workflow.
