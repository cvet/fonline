# FOnline Engine : Build tools

## Build scripts

For build just run from repository root one of the following scripts:  
*Todo: finalize API and declare here*

Validation scenarios can be run one at a time or batched in a single command:

```bash
Engine/BuildTools/validate.sh unit-tests
Engine/BuildTools/validate.sh android-arm64-client linux-client linux-server
```

BuildTools Python regression tests live under `Engine/BuildTools/tests/` and can be run directly:

```bash
pytest -q Engine/BuildTools/tests
```

## CMake layout

All internal CMake modules now live under `Engine/BuildTools/cmake`.
The public entry point kept at the `Engine/BuildTools` root is `Init.cmake`; staged CMake implementation lives under `Engine/BuildTools/cmake/stages/` and helpers under `Engine/BuildTools/cmake/helpers/`.
The validation project scaffold continues to live under `Engine/BuildTools/validation-project`.

## Build environment variables

Build scripts (sh/bat) can be called both from current directory (e.g. `./linux.sh`) or repository root (e.g. `BuildTools/linux.sh`).  
Following environment variables may be set before starting build scripts:

BuildTools consumes only project-specific overrides with the `FO_` prefix. Any declared `FO_*` CMake option can be overridden from an environment variable with the same name. Standard tool variables such as `ANDROID_HOME`, `ANDROID_SDK_ROOT`, and `ANDROID_NDK_ROOT` are reserved for spawned external tools and are not read as BuildTools inputs.

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

#### FO_ANDROID_HOME / FO_ANDROID_SDK_ROOT / FO_ANDROID_NDK_ROOT

Optional explicit Android SDK and NDK overrides for BuildTools. If not set, BuildTools uses prepared workspace locations and then the system `/usr/lib/android-sdk` fallback on Linux when available.

#### FO_CLANG_FORMAT

Optional explicit path to the `clang-format` binary used by `buildtools.py format-source`. The binary must still satisfy the version-20 gate. When unset, BuildTools searches the system `PATH` for `clang-format-20` then `clang-format`. Embedding projects use this to point the engine source formatter at a bundled binary instead of a system install (Last Frontier passes its bundled `Tools/Formatter/clang-format-20.exe` on Windows).

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
- `xwin`
- `msan-libcxx`

Linux system package installation is explicit and separate from workspace preparation:

- `common-packages`
- `linux-packages`
- `web-packages`
- `android-packages`
- `windows-cross-packages`
- `msi-packages`
- `all-packages`

Workspace features such as `linux`, `web`, `android-arm64`, and `windows-cross` do not install apt packages. On a fresh host, pass the matching `*-packages` feature first. `all-packages` installs every group above (including `msi-packages`, the `wixl` MSI-installer toolset). Because apt lives only on the host-provisioning path, no `prepare-workspace` part installs system packages, and parallel CI jobs never contend for the apt lock.

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
python3 Engine/BuildTools/buildtools.py prepare-workspace msan-libcxx
python3 Engine/BuildTools/buildtools.py prepare-workspace toolset emscripten android-ndk dotnet --check
python3 Engine/BuildTools/buildtools.py prepare-host-workspace linux web-packages web dotnet
```

`msan-libcxx` is Linux-only and intentionally excluded from the default `all`
workspace feature because it downloads matching LLVM sources and builds
`libc++`, `libc++abi`, and `libunwind` with MemorySanitizer instrumentation. The
runtime build also passes `BuildTools/sanitizers/msan-runtime-ignorelist.txt` so
libunwind does not self-report on ABI register snapshots during C++ exception or
sanitizer-report unwinding. The `unit-tests-san-memory` validator prepares it
automatically before configuring `San_Memory`; use the explicit workspace command
only when pre-warming a CI host or debugging the runtime build.

Linux hosts can prepare the Windows cross-compilation SDK/CRT through the same wrapper:

```bash
bash Engine/BuildTools/prepare-workspace.sh windows-cross-packages windows-cross
bash Engine/BuildTools/prepare-workspace.sh windows-cross
python3 Engine/BuildTools/buildtools.py prepare-workspace xwin
python3 Engine/BuildTools/buildtools.py build win64 client Release
python3 Engine/BuildTools/buildtools.py build win32 client Release
```

The `windows-cross-packages` feature installs/checks Linux prerequisites. The `windows-cross` wrapper feature and direct `prepare-workspace xwin` command are workspace-only: they use the xwin version pinned in `Engine/ThirdParty/xwin`, prepare both `x86` and `x86_64` SDK/CRT trees into `Workspace/xwin`, and intentionally skip system package installation for pre-provisioned CI hosts. `buildtools.py` splats the primary architecture first, then merges secondary architecture library directories from isolated splats to avoid the `xwin 0.6.6-rc.2` shared-symlink race in one multi-arch invocation. Each splat passes `--http-retry 5` so transient Microsoft CDN body-read failures are retried before failing the workspace preparation.

For `win32`, `buildtools.py` passes `CMAKE_SYSTEM_PROCESSOR=x86`; the toolchain keeps the xwin `x86` library paths and forces `clang-cl --target=i686-pc-windows-msvc` so CMake compiler probes do not emit x64 objects for an x86 link.

## Windows web debug workflow

The local Windows web debug flow uses these shared commands:

- `buildtools.py build web client RelWithDebInfo`
- `buildtools.py package-web-debug`

For an optimized browser build use:

- `buildtools.py build web client Release`

The packaged browser build is emitted into `Workspace/web-debug/<ProjectDevName>-Client-LocalTest-Web` and can be served by the generated `web-server.py` helper.

## Android debug workflow

The local Android debug flow uses these shared commands:

Supported Android platform identifiers are `android-arm32`, `android-arm64`, and `android-x86`.

Device deployment is built around ADB over Wi-Fi. The target device must have wireless debugging enabled and be paired with this host if Android requests pairing.

- `buildtools.py build android-arm64 client RelWithDebInfo`
- `buildtools.py package-android-debug LF android-arm64 LocalTest`
- `buildtools.py package-android-debug LF android-arm64 RemoteSceneLaunch`
- `android_device.py --workspace-root Workspace connect`

The Android SDK and NDK workspace parts must be prepared first. Use `android-packages` only on a fresh Linux host that still needs system packages:

- `bash Engine/BuildTools/prepare-workspace.sh android-arm64`
- `bash Engine/BuildTools/prepare-workspace.sh android-packages android-arm64`

The packaged Android build is emitted into `Workspace/android-debug/<ProjectDevName>-Client-LocalTest-Android` as a ready-to-build Gradle project. Build and deploy:

```bash
python3 Engine/BuildTools/android_device.py --workspace-root Workspace connect
cd Workspace/android-debug/<ProjectDevName>-Client-LocalTest-Android
./gradlew assembleDebug
python3 Engine/BuildTools/android_device.py --workspace-root Workspace install --apk Workspace/android-debug/<ProjectDevName>-Client-LocalTest-Android/app/build/outputs/apk/debug/app-debug.apk
python3 Engine/BuildTools/android_device.py --workspace-root Workspace launch --activity com.example.game/.FOnlineActivity

# Remote scene launch from host to Android device
python3 Engine/BuildTools/buildtools.py package-android-debug LF android-arm64 RemoteSceneLaunch
cd Workspace/android-debug/<ProjectDevName>-Client-RemoteSceneLaunch-Android
./gradlew assembleDebug
python3 Engine/BuildTools/android_device.py --workspace-root Workspace install --apk Workspace/android-debug/<ProjectDevName>-Client-RemoteSceneLaunch-Android/app/build/outputs/apk/debug/app-debug.apk
python3 Engine/BuildTools/android_device.py --workspace-root Workspace launch-game --activity com.example.game/.FOnlineActivity
```

`launch-game` auto-detects the host LAN IP that reaches the selected Wi-Fi Android device and passes it as a runtime `ClientNetwork.ServerHost` override, which makes the packaged `RemoteSceneLaunch` client connect back to the host server without editing baked config files.

At runtime, `FOnlineActivity` stages `assets/Resources` into the app files directory on first launch after install or update and then starts the engine with absolute `Baking.ClientResources` and `Baking.CacheResources` overrides that point to that runtime location.

Android SDK command-line tools version is pinned by `Engine/ThirdParty/android-sdk` and installed into `Workspace/android-sdk`.

Android NDK version is pinned by `Engine/ThirdParty/android-ndk` and installed into `Workspace/android-ndk`.

The Gradle project template lives in `Engine/BuildTools/android-project/` and uses `$PLACEHOLDER$` tokens patched by `package.py` during packaging. Android configuration values come from the baked target config for the selected package config, so `SubConfig` overrides affect APK metadata. Android SDKs that require application manifest metadata can use `Android.ManifestMetaData.<android:name> = <android:value>` settings; the packager emits them as `<meta-data>` entries inside `<application>`. SDK Gradle setup can use `Android.GradleMavenRepository.<name> = <url>` and `Android.GradleDependency.<name> = <Gradle dependency statement>` to add package-config-specific Maven repositories and `dependencies { ... }` entries. Package-specific Java sources can use `Android.JavaSource.<name> = <path/to/File.java>`; the packager copies each non-empty source into the generated app package namespace and patches `$PACKAGE$` / `$CONFIG$`.

Android release APK packaging signs the artifact. Configure signing through `Android.Keystore`, `Android.KeystorePassword`, `Android.KeyAlias`, and `Android.KeyPassword` in the project main config. `package.py` passes `Android.KeystorePassword` and `Android.KeyPassword` to Gradle through `FO_ANDROID_RELEASE_STORE_PASSWORD` and `FO_ANDROID_RELEASE_KEY_PASSWORD` environment variables instead of writing them into the generated Gradle project. If you build the generated Gradle project manually, set those variables before `./gradlew assembleRelease`; if the signing settings are empty, packaging falls back to the Gradle debug signing key so generated package APKs remain installable on development devices. If needed, these settings can use `$ENV{...}` expressions.

APK packaging runs Gradle with `GRADLE_USER_HOME` under the current workspace output tree instead of the shared `~/.gradle`, so parallel CI package jobs do not contend for global Gradle caches.

`android_device.py` first tries `adb mdns services`, shows any discovered Android Wi-Fi endpoints as a numbered list, caches the selected endpoint in `Workspace/android-debug/device-endpoint.txt`, and falls back to manual `IP[:port]` entry when discovery returns nothing.

## Packaging: post-build binary patching

`package.py` injects a few values into the already-linked binaries instead of recompiling per package. Each
target is a fixed-capacity placeholder reserved at compile time with `FO_KEEP_DATA_SYMBOL` (so the linker keeps
the array), and `patch_data(file, marker, data, max_size)` overwrites it in place: it locates the marker, writes
`data` followed by `#` padding up to `max_size`, and asserts the file size is unchanged. Nothing is relocated,
compressed, encrypted, or generated — the file layout is identical before and after, only reserved data bytes
change. Patched regions, all transparent identity/config **text** (never code):

- `PACKAGED_BUILD_NAME` — marker `###NotPackaged###`, a 128-byte array. The package/build identity string;
  each runtime variant patches its own so `IsPackaged()` and the build name reflect the package.
- `INTERNAL_CONFIG` — markers `###InternalConfig###…` / `###InternalConfigEnd###`, capacity
  `FO_INTERNAL_CONFIG_CAPACITY` (40000). The baked internal config blob.
- Embedded resources — capacity `FO_EMBEDDED_DATA_CAPACITY` (200000).

`package.py` also rewrites the PE PDB path (`patch_pe_pdb_path`) and the Android Gradle `$PLACEHOLDER$` tokens.

> **Antivirus note:** "a large placeholder region overwritten after the build" superficially resembles a
> packer/dropper stub, but here it is only transparent config/identity text written into reserved data arrays —
> no code is produced, decrypted, or executed. This is documented so a release engineer or AV reviewer can
> confirm it is benign; keep the capacities no larger than needed and whitelist the packaging step. See the
> project's client-AV audit (`Docs/Plans/2026-06-27-client-av-heuristics-audit.md`, finding M2).

## Packaging: Windows code signing

Packaged Windows binaries can be code-signed at release time so antivirus/SmartScreen trust the client (the
self-update flow downloads and executes a runtime DLL, so an unsigned client is the main heuristic trigger).
Signing is **off by default** (current behavior: unsigned) and tool-agnostic:

- Set `Packaging.CodeSigningHook` in the project main config to an **executable script** on the packaging host.
- During `finalize_output`, before any Zip/Tar/Wix/Raw step, `package.py` calls `<hook> <absolute-pe-path>`
  once for **every `*.exe`/`*.dll`** staged under the package tree — launcher exes (incl. the OpenGL variant),
  the runtime DLLs, and the client-runtime **update payloads** (the downloaded-and-executed DLL). Signing last
  means the signature covers the final patched bytes; covering the whole tree means archives, the MSI, and the
  updater payloads are all signed.
- The hook must exit `0`; a non-zero exit **fails the package** so a release that asked to be signed never
  ships unsigned. The hook must be directly executable on the host (shebang + `chmod +x` on Linux; a
  `.cmd`/`.bat`/`.exe` on Windows).
- The hook owns the tool, certificate, timestamp URL, and **secrets** — keep passwords/tokens in environment
  variables, never in the repo or main config. Always **timestamp** (RFC-3161) so signatures outlive the cert.

The certificate must come from a publicly-trusted CA, and since June 2023 its key lives on a hardware token or
cloud HSM (no plain on-disk `.pfx` for public trust). Cheapest practical options and the matching hook body:

```bash
# 1) Azure Trusted Signing — cheapest ($ ~10/mo), cloud, instant SmartScreen reputation. Signs on a Windows
#    host via signtool + the Trusted Signing dlib; Azure creds come from env (AZURE_* / managed identity).
#    sign_windows.cmd  (args: %1 = file)
#    signtool sign /v /fd SHA256 /tr http://timestamp.acs.microsoft.com /td SHA256 ^
#      /dlib "%TRUSTED_SIGNING_DLIB%" /dmdf "%TRUSTED_SIGNING_METADATA_JSON%" "%~1"

# 2) SSL.com eSigner CodeSignTool — cloud, runs on the Linux packaging host (Java). Creds/TOTP from env.
#    sign_windows.sh  (args: $1 = file)
#    CodeSignTool sign -username="$ESIGNER_USER" -password="$ESIGNER_PASS" \
#      -totp_secret="$ESIGNER_TOTP" -input_file_path="$1" -override

# 3) osslsigncode — on the Linux host with a PKCS#11 cloud-HSM cert (e.g. Certum) or a token.
#    sign_windows.sh  (args: $1 = file)
#    tmp="$(mktemp)"; osslsigncode sign -pkcs11module "$PKCS11_MODULE" -key "$PKCS11_KEY_ID" \
#      -certs "$CERT_PEM" -h sha256 -n "Last Frontier" -i https://lastfrontier.ru \
#      -ts http://timestamp.digicert.com -in "$1" -out "$tmp" && mv -f "$tmp" "$1"
```

For Android, signing stays in Gradle (see the Android workflow above); this hook is Windows-only.

## Source formatting

`buildtools.py format-source` formats the engine `Source/` tree with `clang-format`. The binary is resolved by `discover_clang_format()`: the `FO_CLANG_FORMAT` override first (when set), then `clang-format-20`/`clang-format` on `PATH`; the resolved binary must report major version 20. This keeps the command usable both from CI (clang-format-20 on `PATH`) and from an embedding project that supplies a bundled binary through `FO_CLANG_FORMAT`.

## Pipeline documentation

For the maintained staged CMake pipeline guide, see [../Docs/BuildToolsPipeline.md](../Docs/BuildToolsPipeline.md).
