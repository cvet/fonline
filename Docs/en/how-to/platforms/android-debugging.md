---
layout: default
title: Android Build, Packaging, and Device Debugging
locale: en
document_id: android-debugging
permalink: /Docs/en/how-to/platforms/android-debugging.html
---

# Android Build, Packaging, and Device Debugging

This guide is the Engine-owned procedure for building an Android client, generating or assembling an APK, installing it over Wi-Fi ADB, connecting it to a development server, and separating build failures from package, device, and runtime failures. It follows the current BuildTools implementation, Android project template, support model, package grammar, settings, and updater boundary. An embedding project owns its application identity, release policy, device fleet, server profile, store delivery, and acceptance evidence.

## Contract status

This is the production contract for the reusable Android client path at the current Engine revision. Engine source, checked models, and tests are normative. Last Frontier and FOnline TLA are pinned discovery and compatibility evidence only; their task names, application ids, package declarations, and CI coverage do not extend Engine support.

The page is independently usable from an embedding-project checkout in which the Engine lives at `Engine/`. Replace `<ProjectDevName>`, `<Config>`, `<application-id>`, and package paths with project-owned values. Project evidence is pinned in `BuildTools/ExternalProjectEvidence.json`; all reusable claims here are re-derived from Engine-owned source.

Android has four separate evidence layers:

1. the C++ client cross-build;
2. the generated Gradle project and APK;
3. installation and runtime on a device or emulator;
4. project release qualification and store publication.

Success at one layer does not qualify the next one.

## Scope and authority

The owning Engine sources are:

- `BuildTools/buildtools.py` and `BuildTools/prepare-workspace.sh` for platform ids, host preparation, SDK/NDK pins, native builds, and the local package helper;
- `BuildTools/PackageInterface.json`, `BuildTools/package.py`, and `BuildTools/cmake/stages/Packages.cmake` for package grammar, ABI staging, Gradle generation, APK selection, and artifact cleanup;
- `BuildTools/android-project/` for the Gradle wrapper, Android plugin, manifest, SDL activity base, resources, and Java template;
- `BuildTools/android_device.py` for Wi-Fi ADB discovery, endpoint selection, install, launch, stop, and filtered logs;
- `Source/Common/Settings.inc` for registered Android package settings;
- `Source/Client/Updater.*` for the Android native-update boundary;
- `ThirdParty/android-sdk`, `ThirdParty/android-ndk`, and `ThirdParty/android-api` for pinned toolchain inputs;
- `BuildTools/SupportMatrix.json` and `.github/workflows/validate.yml` for the evidence-scoped support labels.

The embedding project owns resource baking, its config and server, Android identity and signing material, extra SDK integration, package declarations, CI, devices, store compliance, and visible/runtime acceptance.

## Support and qualification matrix

| BuildTools platform | Android ABI | Engine support level | Current Engine gate |
|---|---|---|---|
| `android-arm32` | `armeabi-v7a` | build-gated | Ubuntu cross-build of client shared-library/package inputs |
| `android-arm64` | `arm64-v8a` | build-gated | Ubuntu cross-build of client shared-library/package inputs |
| `android-x86` | `x86` | source-capable | available validation target, no required Engine CI lane |

The public Android surface is the client shared library and package input. It does not imply Android server, Mapper, Baker, viewer, service, emulator, physical-device, signing, store, graphics-driver, input, audio, lifecycle, or networking qualification. `android-arm64` is the normal physical-device choice, but that is a workflow recommendation rather than a broader support promise.

The current Engine build gates use an Ubuntu 24.04 cross-build host. Other hosts may run parts of CMake, `package.py`, or Gradle, but they are not qualified by this matrix. Use [Support Matrix](../../reference/platforms/support-matrix.md) as the support owner and record project-specific extensions separately.

## Prepare the host and workspace

The Linux host path needs Python, the common build prerequisites, Java 17, and writable workspace storage. On a fresh host, install the Android package group and prepare the pinned SDK/NDK in one route:

```bash
bash Engine/BuildTools/prepare-workspace.sh android-packages android-arm64
```

When system packages already exist, prepare only the workspace-local SDK and NDK:

```bash
python3 Engine/BuildTools/buildtools.py prepare-workspace android-sdk android-ndk
```

`prepare-workspace.sh android-arm64` is the equivalent host wrapper for the second route. The current descriptors select Android SDK command-line tools `14742923`, NDK `r29`, and native API level `23`. SDK preparation accepts licenses and installs `platform-tools`, build-tools `34.0.0`, and platform `android-35` under `Workspace/android-sdk`; NDK preparation installs under `Workspace/android-ndk`.

BuildTools resolves SDK/NDK locations in this order:

- prepared `FO_WORKSPACE` locations;
- Linux system SDK `/usr/lib/android-sdk` when neither SDK variable is authored;
- explicit `FO_ANDROID_HOME` / `FO_ANDROID_SDK_ROOT` and `FO_ANDROID_NDK_ROOT` values where applicable.

Use `python3 Engine/BuildTools/buildtools.py env --summary` to inspect the effective paths and pins. Do not infer a usable toolchain from an installed Android Studio UI alone.

### SDK-level relationships

The package defaults are `MinSdk = 23`, `TargetSdk = 35`, and `CompileSdk = 35`; the native build uses the independent `ThirdParty/android-api` pin. BuildTools currently substitutes these values but does not validate their numeric relationship. A project should enforce:

```text
native API <= MinSdk <= TargetSdk <= CompileSdk
```

The prepared SDK contains only the declared required platform/build-tools set. If a project changes `Android.CompileSdk`, it must provision that platform explicitly. A successful C++ cross-build does not prove that Gradle can resolve a changed compile SDK or that a store accepts the target SDK.

## Build and stage a local debug package

Run the embedding project's fresh resource bake first. Then build the native client and generate one Gradle tree per requested sub-config:

```bash
# Run the embedding project's fresh BakeResources route first.
python3 Engine/BuildTools/buildtools.py build android-arm64 client RelWithDebInfo
python3 Engine/BuildTools/buildtools.py package-android-debug <ProjectDevName> android-arm64 <Config>
```

`package-android-debug` also accepts multiple config names in one invocation. It maps the BuildTools platform to an Android ABI and invokes `package.py` as `Client`, `Android`, `Raw`, with the selected config. It does not run Gradle.

The generated project is:

```text
Workspace/android-debug/<ProjectDevName>-Client-<Config>-Android
```

Packaging performs these steps:

1. require baked client resources and the matching build hash;
2. copy the Engine Android project template into a clean output tree;
3. copy `lib<ProjectDevName>_Client.so` into `app/libs/<abi>/libmain.so` and patch its embedded resources, config, build name, and packaged marker;
4. move baked client resource ZIPs under `app/src/main/assets/Resources`;
5. overlay Android values from the authored root and selected sub-config;
6. patch Gradle, manifest, activity, strings, icon, optional Java sources, SDK, and NDK fields.

The generated tree is disposable build output. Change the Engine template or an authored project setting instead of maintaining edits inside `Workspace/android-debug`.

### Native and Gradle debug meanings

The command above builds a `RelWithDebInfo` native library. `assembleDebug` below selects the Android/Gradle debug build type and debug signing key. These are independent choices. The local helper intentionally asks `package.py` for `Raw`, not its `Debug` binary-selection token, so do not describe the native library as a Debug C++ build unless it was actually built and packaged that way.

## Assemble and inspect the debug APK

Build the generated project:

```bash
cd Workspace/android-debug/<ProjectDevName>-Client-<Config>-Android
./gradlew --no-daemon assembleDebug
```

The normal result is `app/build/outputs/apk/debug/app-debug.apk`. The current template uses Gradle 8.12 and Android Gradle Plugin 8.7.3, filters native libraries to the packaged ABI list, leaves ZIP assets uncompressed, and uses legacy JNI library packaging. The manifest requires OpenGL ES 3.0 and declares Internet, network-state, and vibration permissions; touchscreen, gamepad, Bluetooth, USB-host, and PC-style pointer features are optional.

The template sets `lint.abortOnError = false`. APK assembly is therefore not a lint, policy, privacy, vulnerability, or store-readiness gate. Add project-owned lint and release checks instead of interpreting `assembleDebug` as production acceptance.

Inspect the generated artifact before installation:

- application id, version code/name, min/target SDK, ABI, activity, permissions, and signature;
- exactly one intended `libmain.so` per declared ABI;
- `assets/Resources/Metadata.zip` and expected resource packs;
- absence of keystore passwords, private credentials, local paths, stale configs, and unlicensed SDK payloads.

Use Android SDK tools such as `apkanalyzer`, `aapt2`, and `apksigner` from the prepared SDK when these checks are part of a release lane.

## Android package configuration

The fixed package settings are registered in `Source/Common/Settings.inc` and exported on the server/config-baking side:

| Setting | Default | Contract |
|---|---|---|
| `Android.PackageName` | `com.fonline.app` | Gradle namespace/application id and generated Java package. |
| `Android.VersionCode` | `1` | Integer Android package version code. |
| `Android.MinSdk` | `23` | Gradle minimum SDK; keep at or above the native API pin. |
| `Android.TargetSdk` | `35` | Gradle target SDK; store policy remains project-owned. |
| `Android.CompileSdk` | `35` | Gradle compile SDK; provision the matching SDK platform. |
| `Android.ScreenOrientation` | `landscape` | Manifest activity orientation value. |
| `Android.Icon` | `Engine/Resources/Radiation.png` | Existing PNG copied to every legacy mipmap density. |
| `Android.Keystore` | empty | Release keystore path read from baked target config and resolved relative to project config. |
| `Android.KeystorePassword` | empty | Release store password. |
| `Android.KeyAlias` | empty | Release key alias. |
| `Android.KeyPassword` | empty | Release key password. |

The version name is not independently configurable in the current packager. It is the first eight characters of the package build hash, or `1.0` when no hash is supplied. If a product requires a semantic Android version name, extend and test the package contract before publishing rather than editing generated Gradle files.

The icon check verifies an existing file and PNG signature only. The same bytes are copied to every current mipmap directory; there is no resizing, adaptive-icon generation, foreground/background split, or visual validation. Supply a project-owned source that remains acceptable at every launcher size and add store/device inspection.

### Package-only extension families

The authored root and selected sub-config may also contain:

| Prefix | Behavior |
|---|---|
| `Android.ManifestMetaData.<name>` | Emit one escaped, non-empty `<meta-data>` value under `<application>`. |
| `Android.GradleMavenRepository.<name>` | Emit a non-empty Maven repository URL. |
| `Android.GradleDependency.<name>` | Insert a non-empty project-owned Gradle dependency statement. |
| `Android.JavaSource.<name>` | Copy a `.java` file into the generated application package and replace `$PACKAGE$` / `$CONFIG$`. |

Keys are processed in sorted order. Java source basenames should be unique; the packager rejects `FOnlineActivity.java` but does not provide a general collision or class-package validator. Dependency statements are trusted project input. Review repositories, dependency locks, licenses, transitive manifests, permissions, exported components, supply-chain provenance, and Java source before release.

The current manifest template has no generic config family for extra permissions, services, providers, intent filters, network-security policy, or adaptive icons. Such changes require a reviewed template/package-contract extension or a project-owned deterministic packaging step with tests. Do not rely on hand edits to a generated tree.

### Config precedence and secrets

`package.py` reads Android settings from the baked effective target config. It does not overlay authored `Android.*` keys or resolve `$TARGET_ENV{...}` / `$TARGET_FILE{...}` on the packaging host. A concrete signing password therefore enters baked config, while a retained target directive remains a literal string and cannot supply signing credentials.

The following runtime-directive syntax is intentionally **not** a working package-secret handoff:

```ini
Android.Keystore = $TARGET_ENV{MYGAME_ANDROID_KEYSTORE_PATH}
Android.KeystorePassword = $TARGET_ENV{MYGAME_ANDROID_STORE_PASSWORD}
Android.KeyAlias = $TARGET_ENV{MYGAME_ANDROID_KEY_ALIAS}
Android.KeyPassword = $TARGET_ENV{MYGAME_ANDROID_KEY_PASSWORD}
```

Never put real signing values in authored or baked config, documentation, fixtures, logs, generated trees, archives, or manifests. The current Engine has no host-only Android signing-secret input; leave the tuple empty for development output or sign in a protected project-owned stage. [Security and Secrets](../release/security-and-secrets.md) owns provisioning, redaction, rotation, CI trust, and incident handling.

## Runtime bootstrap and resource staging

The generated `FOnlineActivity` extends SDL's `SDLActivity`, loads only `libmain.so`, prepares resources before SDL startup, and constructs the native argument vector itself. It always adds:

- `--ApplySubConfig <Config>`;
- `--Baking.ClientResources <files-dir>/Resources`;
- `--Baking.CacheResources <files-dir>/Cache`.

If the launch Intent contains a non-empty string extra named `server_host`, the activity also adds `--ClientNetwork.ServerHost <value>`. It does not accept a generic command-line extra. Add a reviewed typed bridge before claiming other runtime overrides.

Packaged assets are copied from `assets/Resources` into the app-private files directory when either condition is true:

- `.asset_revision` differs from `PackageInfo.lastUpdateTime`;
- `<files-dir>/Resources/Metadata.zip` is missing.

The activity deletes the old resource directory, copies the asset tree recursively, and writes the new revision only after a successful copy. Enumeration, directory, copy, delete, or revision-write failures throw and stop startup. The cache directory is created separately and is not cleared by an APK update. If resource formats or cache keys change incompatibly, the project must own explicit invalidation and migration evidence.

`adb install -r` preserves app data, including the runtime cache and copied resources. Clearing app storage or uninstalling removes that data. Distinguish a stale APK, retained cache, failed asset copy, updater state, and server incompatibility before changing gameplay code.

Android may update writable resources through the normal client resource path, but `Updater::CanSelfUpdateNativeModules()` returns false for Android. A native compatibility mismatch cannot be repaired by downloading a new shared library in place; build, package, install, and restart a compatible APK.

## Discover and select a Wi-Fi ADB device

Enable Developer Options and Wireless debugging, pair the device with the host using Android/ADB when required, unlock it, and accept the authorization prompt. `android_device.py` discovers connect services but does not perform the pairing transaction.

The helper resolves `adb` from `Workspace/android-sdk/platform-tools` first, then `PATH`. Its endpoint order is:

1. explicit `--device IP[:port]`;
2. cached `Workspace/android-debug/device-endpoint.txt` when it still connects;
3. one already-connected network serial;
4. `adb mdns services` entries containing `_adb-tls-connect._tcp` or `_adb._tcp`;
5. interactive manual input.

An endpoint without a port becomes `:5555`. In a non-interactive job, supply `--device` unless a cached or single connected endpoint is guaranteed. The helper is Wi-Fi-oriented; use raw ADB or add a tested helper path for USB serials.

List connect endpoints or select one:

```bash
python3 Engine/BuildTools/android_device.py --workspace-root Workspace discover
python3 Engine/BuildTools/android_device.py --workspace-root Workspace connect --device 192.0.2.10:37199
```

`discover` lists services but does not connect. A stale cached endpoint is retried and then bypassed; it is overwritten only after a later endpoint succeeds.

## Install, launch, stop, and collect logs

Use the fully qualified activity component derived from `Android.PackageName`:

```bash
python3 Engine/BuildTools/android_device.py --workspace-root Workspace install \
  --apk Workspace/android-debug/<ProjectDevName>-Client-<Config>-Android/app/build/outputs/apk/debug/app-debug.apk \
  --device 192.0.2.10:37199
python3 Engine/BuildTools/android_device.py --workspace-root Workspace launch \
  --activity <application-id>/.FOnlineActivity \
  --device 192.0.2.10:37199
python3 Engine/BuildTools/android_device.py --workspace-root Workspace stop \
  --package <application-id> \
  --device 192.0.2.10:37199
python3 Engine/BuildTools/android_device.py --workspace-root Workspace logcat \
  --device 192.0.2.10:37199
```

`install` runs `adb install -r`: it replaces the APK while preserving application data and does not uninstall, clear storage, grant permissions, or change signing compatibility. `INSTALL_FAILED_USER_RESTRICTED` receives device-side guidance; other ADB failures are returned as-is.

`launch` runs `am start -n`; `stop` runs `am force-stop`. `logcat` streams the fixed filters `SDL:V`, `FOnline:V`, `LF:V`, and all error-priority messages. It neither clears the buffer nor filters by package/PID and exposes no custom filter option. Use raw `adb logcat` for complete history, timestamps, PID filters, tombstones, or buffer clearing.

## Connect to a development server

Start a project-owned server that matches the packaged config and listens on an address reachable from the device. Then use `launch-game`:

```bash
python3 Engine/BuildTools/android_device.py --workspace-root Workspace launch-game \
  --activity <application-id>/.FOnlineActivity \
  --device 192.0.2.10:37199 \
  --server-host 192.0.2.20
```

The helper force-stops the package, starts the activity, and sends `--es server_host <host>`. If `--server-host` is omitted, it opens a UDP route to the selected device endpoint and uses the resulting non-loopback local address. Auto-detection does not prove that the server is listening there, that the firewall permits traffic, or that project ports and protocol versions match.

The activity converts only that typed extra to `--ClientNetwork.ServerHost`. The project owns server startup, ports, TLS/proxy policy, account/auth flow, startup scene, public-network threat controls, and compatibility. Reinstall after native or script-contract changes; connecting an old APK to a new incompatible server is not a valid Android acceptance test.

## Integrate release packaging

The reusable package declaration is a project-owned `DefinePackage(...)` entry. For example:

```cmake
DefinePackage(AndroidCandidate
    CONFIG PublicGame
    BINARY Client Android arm64 Raw+Apk)
```

`Apk` runs Gradle inside the generated tree and copies the selected APK beside it as `<target-output-name>.apk`. `Raw` retains the Gradle tree; without `Raw`, finalization deletes it after producing the requested artifact. `Debug+Apk` selects `assembleDebug`; `Apk` without `Debug` selects `assembleRelease`. APK Gradle caches are isolated under a package-specific `.gradle-user-home` below the output parent.

Android packaging supports only the `Client` target and requires resources. A package declaration is not sufficient by itself: CI must build every requested ABI, produce a matching build hash and fresh baked config/resources, prepare SDK/NDK/Java, invoke the package target, and retain logs and artifact evidence.

### Release signing behavior

If any signing field is non-empty, all four fields are required and the keystore file must exist. The keystore path and alias are patched into Gradle; passwords are passed only through `FO_ANDROID_RELEASE_STORE_PASSWORD` and `FO_ANDROID_RELEASE_KEY_PASSWORD`. Gradle then signs `assembleRelease` with that release config.

If all signing fields are empty, the template signs the release build with Gradle's debug signing config so the APK remains installable. `package.py` rejects an unsigned release APK, but it cannot turn a debug-key signature into production identity. A debug-key APK is always a development artifact. A production lane must require the intended certificate, verify its digest after final-byte assembly, prove version-code monotonicity and update compatibility, and fail closed when release credentials or verification are absent.

Use [Packaging and Release](../release/packaging.md) for manifest, provenance, publication, staged rollout, and rollback requirements.

## Device and release acceptance matrix

| Route | Minimum project evidence | Failure signal |
|---|---|---|
| ABI and install | APK declares only intended ABI(s); clean install and `-r` update succeed on representative API levels | `INSTALL_FAILED_NO_MATCHING_ABIS`, downgrade, signature, or policy rejection |
| Cold and warm startup | Resource copy, SDL/native load, config application, and first rendered frame | crash before `libmain`, missing `Metadata.zip`, black screen, or wrong config |
| Rendering | Representative map, GUI, fonts, images, sprites/models, effects, and orientation on GLES 3 devices | shader/driver failure, clipping, corruption, unsupported orientation |
| Input | Touch, back/navigation, keyboard/IME where used, and each claimed controller class | trapped input, duplicate events, unusable focus, controller mapping drift |
| Audio and lifecycle | audible sound/music, interruption, background/pause/resume, screen lock, and process recreation | stuck audio, lost device, duplicate runtime, crash or state loss |
| Networking | device-to-server route, reconnect, incompatible-version response, latency/loss behavior, firewall policy | loopback/host mismatch, silent timeout, insecure exposure, update loop |
| Resource/cache update | APK update, retained data, changed assets, missing metadata, and explicit cache invalidation | old assets after update, partial copy, incompatible retained cache |
| Native compatibility | mismatched native generation reports unsupported self-update and recovers by APK replacement | downloaded native module assumed to hot-replace Android binary |
| Permissions and privacy | manifest merge, exported components, runtime permissions, data backup, SDK collection, privacy disclosure | unexpected permission/component, rejected policy, undeclared data flow |
| Release identity | release certificate digest, monotonic version code, signed final APK, install/update from previous supported release | debug key, unsigned artifact, wrong alias, non-upgradable package |
| Distribution | store/internal-track upload, install from real channel, asset limits, target API, rollback/recovery | local sideload succeeds but published channel fails |

The Engine CI matrix intentionally does not supply this device evidence. A project may call a target production-supported only after the relevant rows are versioned, repeatable, and required by its release gate.

## Troubleshooting by layer

| Symptom | Inspect first |
|---|---|
| Host preparation fails | Java 17/system package group, disk permissions, network access, pinned SDK/NDK descriptors, and accepted licenses. Truncated Google CDN archives (`ContentTooShortError`, `Error reading Zip content from a SeekableByteChannel`) are retried by `download_file` / `run_with_retry`; a persistent failure is a host/network problem, not a missing pin |
| CMake cannot configure Android | `FO_ANDROID_NDK_ROOT`, toolchain file, ABI mapping, native API pin, Clang floor, and clean build directory |
| Native build succeeds but package input is missing | `FO_OUTPUT`, target/config/build hash, expected `lib<ProjectDevName>_Client.so`, and matching resource bake |
| Packaging rejects resources | selected sub-config, `Baking.ClientResources`, fresh `Metadata.zip`, and no `NoRes` token |
| Packaging rejects icon or Java source | real PNG signature, project-relative path, `.java` suffix, unique basename, and no `FOnlineActivity.java` override |
| Gradle cannot find SDK/NDK | generated `local.properties`, `ANDROID_HOME` / `ANDROID_SDK_ROOT`, patched NDK path/version, and provisioned compile SDK |
| Gradle dependency resolution fails | generated repositories/dependencies, credentials, dependency locks, proxy/TLS, and repository availability |
| Release build is unsigned or debug-signed | complete signing tuple, environment handoff, keystore path/alias, final `apksigner verify`, and certificate digest |
| Device is not discovered | pairing first, Wireless debugging, `discover`, `adb devices`, same-network reachability, then explicit `--device` |
| Device is `unauthorized` or install is restricted | unlock device, authorize host, accept installer/security prompt, and device/vendor policy |
| Update install fails | application id, version code, signing certificate, ABI, storage, and whether a clean uninstall is acceptable for this test |
| Activity does not start | exact `<application-id>/.FOnlineActivity`, installed package, manifest merge, native library, and full logcat |
| Content is missing or stale | installed APK hash/path, `lastUpdateTime`, `.asset_revision`, `Metadata.zip`, copy exception, retained cache |
| Client cannot reach host | typed `server_host` extra, selected LAN address, server bind address, firewall, ports, and compatibility version |
| Client asks for native update | Android native self-update is unsupported; install a compatible APK instead of retrying the resource updater |
| Resume/orientation fails | SDL lifecycle logs, manifest `configChanges`, orientation setting, renderer/device loss, and project state restoration |

Keep failure evidence per layer: CMake/build log, package log and generated tree, Gradle log and APK inspection, ADB command output, full logcat/tombstone, server log, and exact revisions. Do not collapse all failures into "Android does not work."

## Project evidence and extraction rules

The pinned Last Frontier snapshot demonstrates project-owned Android settings, an ARM64 local VS Code build/bake/package/install/remote-server task graph, raw Android package declarations, and a nightly/manual cross-platform build matrix. Its official production package declarations currently do not emit an Android APK. This is integration evidence, not an Engine release promise.

The pinned FOnline TLA snapshot independently carries Android package settings, ARM32/ARM64/x86 CMake presets, and ARM32/ARM64 CI client builds, but no Android APK/device qualification observed by this audit. It confirms that identity and build wiring belong to the game; it does not justify copying its release practices or claiming x86 support.

Promote a project observation only when the reusable mechanism and focused tests live in Engine. Keep these project-owned:

- application id, display name, icon, version policy, signing identity, SDK integrations, and permissions;
- selected sub-config, server route, ports, authentication, startup scene, and content packs;
- Gradle dependency allowlist, privacy/security review, device matrix, performance budgets, and store policy;
- CI job names, editor tasks, device endpoints, credentials, artifacts, rollout, monitoring, and rollback.

Absence of a device lane in either project is evidence of a gap, not evidence that device behavior is acceptable.

## Maintenance triggers

Re-audit this guide in the same change when any of these move:

- Android platform ids, ABI aliases, support labels, CI matrix, host prerequisites, SDK/NDK/API pins, or environment resolution;
- Android CMake flags, client output name, native library rename, binary patching, package target/pack grammar, output paths, or Gradle invocation;
- package setting declarations/defaults, sub-config precedence, host directives, signing fields, secret handoff, icon/metadata/dependency/Java-source handling;
- Gradle wrapper/plugin, manifest features/permissions/activity, SDL Java base, `FOnlineActivity`, resource revision/copy/cache behavior, or Intent extras;
- ADB discovery markers, endpoint cache/selection, install flags, activity command, server-host detection, force-stop, or logcat filters;
- Android updater capability, native compatibility behavior, renderer/input/audio/lifecycle requirements, or release acceptance policy;
- Last Frontier or TLA evidence revision/path, especially after either project changes Android config, package declarations, CI, or device workflows.

Run the focused Android documentation test, package/security tests, generated documentation gates, and the affected build/package/device lanes. Update project integration docs in the same revision where observable behavior changes.

## Validation routes

From the Engine root, run the source-backed documentation checks:

```bash
python3 BuildTools/tests/test_docs_android_debugging.py
python3 BuildTools/tests/test_package_security.py
python3 BuildTools/tests/test_docs_package.py
python3 BuildTools/tests/test_docs_support_matrix.py
python3 BuildTools/docs_validate.py
```

For an Android change, also run the narrowest affected native validation target (`android-arm32-client`, `android-arm64-client`, or the source-capable x86 target), a fresh resource bake, generated-project assembly, APK inspection, install/update, cold and warm launch, complete log capture, and the relevant rows of the device acceptance matrix. A host-only documentation or package test cannot replace visible/device evidence.

## See also

- [Support Matrix](../../reference/platforms/support-matrix.md)
- [Build Workflow](../build/)
- [Embedding Project](../build/embedding-project.md)
- [BuildTools Pipeline](../../reference/cmake-and-buildtools/pipeline.md)
- [Packaging and Release](../release/packaging.md)
- [Security and Secrets](../release/security-and-secrets.md)
- [Client Runtime Split and Updater](../../explanation/runtime/client-updater.md)
- [Web Build, Packaging, and Browser Debugging](web-debugging.md)
