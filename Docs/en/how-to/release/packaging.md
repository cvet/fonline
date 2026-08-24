---
layout: default
title: Packaging and Release
locale: en
document_id: packaging-and-release
permalink: /Docs/en/how-to/release/packaging.html
---

# Packaging and Release

This guide turns an embedding project's compiled FOnline applications and baked resources into reviewable release artifacts. It owns the reusable Engine procedure and its evidence boundaries. A game repository still owns its package matrix, product configuration, credentials, deployment topology, databases, stores, rollout, and rollback decision.

Use the generated [package interface](../../reference/packages/index.md) for the exact current grammar, target/platform compatibility, pack tokens, payloads, and command-line arguments. Use the [Support Matrix](../../reference/platforms/support-matrix.md) before calling any resulting artifact supported.

## Package decision

For each declared application variant, build the exact target, run
`ForceBakeResources` for its release configuration, then invoke the generated
`MakePackage` target and inspect the isolated artifact. The current
output-producing packs are `Raw`, `Zip`, `SingleZip`, `Tar`, `TarGz`, `Root`,
`Wix`, and `Apk`; valid packs still depend on target and platform. An
implemented payload or pack is capability, not a support claim. The embedding project owns its package
matrix, release policy, signing credentials, distribution, acceptance, rollout,
and rollback. Engine package capability proves none of those project decisions.
In particular, `Apk` copies the selected signed **or debug** APK; the pack token
does not prove release signing. Describe a tested capability and its evidence,
not an Engine guarantee for every host or project.
Do not say the output packs can be combined with any target/platform: use only
an implemented compatible row from the generated matrix. `Debug+Apk` selects
Gradle `assembleDebug`, whose development artifact is signed with the Gradle
debug key; it is debug-signed, not unsigned and not production release-signed.

The current accepted targets are `Server`, `Client`, `Mapper`, `Baker`,
`AnimationViewer`, and `ParticleViewer`. Payload packaging is implemented for
`Windows`, `Linux`, `Android`, and `Web`; `macOS` and `iOS` are accepted parser
dimensions whose package implementations are `unsupported`. The implemented
payloads are native PE plus companion files, native ELF plus companion files,
an Android Gradle client project with ABI libraries and assets, and a browser
JavaScript/Wasm client with preloaded resources.

Name the artifact produced by the chosen compatible output pack: `Raw` retains
the staged directory; `Zip` emits a ZIP; `SingleZip` appends to one package-wide
ZIP; `Tar` emits a tar archive; `TarGz` emits a gzip-compressed tar archive;
`Root` merges the staged directory into the package output root; `Wix` emits an
MSI; and `Apk` copies the selected signed or debug APK. These are tested
packager capabilities, not universal Engine guarantees or project release
evidence.

## Know what the Engine proves

Keep four claims separate:

1. **Build capability**: an application target can compile for a host/target profile.
2. **Package capability**: `BuildTools/package.py` can assemble that target/platform/payload combination.
3. **Project qualification**: the embedding game repeatedly installs, starts, exercises, and diagnoses the artifact in its release environment.
4. **Published release**: an identified artifact passed project approval, signing, distribution, rollout, and recovery gates.

The current packager implements Windows, Linux, Android, and Web payloads. It rejects macOS and iOS packaging. The required workflow nevertheless build-gates narrower macOS and iOS client inputs. A successful Apple build is therefore not an Engine-produced application bundle, signed archive, notarized package, device pass, or store submission.

The packager can emit a Windows service binary or Linux daemon beside the ordinary server. It does not install the service, create an operating-system account, provision a database, write a systemd or recovery policy, open firewall ports, rotate logs, or prove long-running operation.

## Prepare a release-owned package matrix

Record one row for every artifact the game intends to ship. Do not infer rows from all parser choices.

| Field | Example | Required evidence |
|---|---|---|
| Package ID | `ReleaseWindows` | Stable `DefinePackage` name |
| Engine revision | full commit SHA | Clean, recursively initialized checkout |
| Game revision | full commit SHA or signed tag | Source used for the artifact |
| Package config | `PublicRelease` | Baked client/server config exists |
| Target | `Client`, `Server`, or tool | Application binary was built |
| Platform and architecture | `Windows win64` | Compatible package capability |
| Pack tokens | `Raw+Zip+Wix` | Valid generated-interface combination |
| Build host/toolchain | pinned runner image and versions | Reproducible environment record |
| Acceptance lane | named CI job or reviewed procedure | Install/start/runtime result |
| Distribution | archive, installer, store, container, depot | Project-owned publication route |
| Signing identity | certificate/key alias without secret data | Signature verification result |
| Rollback unit | previous immutable artifact/config/data set | Rehearsed restore route |

Treat the exact game and Engine revisions, package declaration, package config, dependency pins, SDK/tool versions, and build image as one input set. Changing any member creates a different release candidate.

## Declare packages

Call `DefinePackage(...)` after project sources are registered and before `BuildPackages()`. Keep separate package IDs when their build hosts, credentials, acceptance lanes, or publication destinations differ.

```cmake
DefinePackage(ReleaseWindows
    CONFIG PublicRelease
    BINARY Client Windows win64 Raw+Zip+Wix
    BINARY Server Windows win64 Headless+Service+Raw+Zip)

DefinePackage(ReleaseLinux
    CONFIG PublicRelease
    BINARY Server Linux x64 Headless+Daemon+Raw+TarGz)

DefinePackage(ReleaseWeb
    CONFIG PublicRelease
    BINARY Client Web wasm Raw+Zip+WebServer)

DefinePackage(ReleaseAndroidArm64
    CONFIG PublicRelease
    BINARY Client Android arm64 Raw+Apk)
```

This is a grammar example, not a support claim or a universal production matrix. Remove every row that the game does not build and qualify.

Each `BINARY` clause has this shape:

```text
BINARY <target> <platform> <architecture> <pack+tokens> [POSTFIX <variant>]
```

`CONFIG` selects a baked sub-config. For resource-bearing client and server packages, baking must have produced `Baking/Configs/<config>.fomain-client` or `...-server` through the `Config` baker. The package target does not silently create missing application binaries or baked resources.

Use `POSTFIX` when a separately built binary variant has `FO_BINARY_OUTPUT_POSTFIX`. The declaration value must match the build output. This keeps package input selection, packaged runtime identity, and server-staged updater payload names aligned. Do not use a package name as an implicit variant selector.

Use `INCLUDE <source-glob> <target-path>` only for reviewed, distributable files already present under the build output. The packager rejects escaping paths and updates the package root plus `SingleZip` when present. License notices, attribution, and third-party payload approval remain project responsibilities.

## Build, bake, then package

Run the stages explicitly and stop on the first failure:

1. Start from a clean, recursively initialized game checkout at the release revision and verify the exact Engine SHA.
2. Prepare the host using the pinned Engine workspace command and the embedding project's preset or CI image.
3. Configure the project with release-owned cache values. Do not reuse an unexplained developer cache.
4. Build every application variant named by the package declaration.
5. Force-bake the release resources and configs when the candidate must not depend on incremental state.
6. Invoke `MakePackage-<package-id>` only after its binary and baking inputs exist.
7. Preserve the complete package log and fail on assertions, warnings treated as errors, signing failures, missing symbols, missing configs, or missing resource packs.

The project chooses concrete target names. A typical multi-config sequence is:

```bash
cmake --preset release-host
cmake --build Build/release-host --config Release --target <application-targets>
cmake --build Build/release-host --config Release --target ForceBakeResources
cmake --build Build/release-host --config Release --target MakePackage-ReleaseWindows
```

Do not run several platform entries from one package ID unless all of their compiled inputs are available in the same `FO_OUTPUT_PATH`. Separate package IDs make cross-build ownership and failures easier to audit.

The packager patches reserved data regions after linking. It embeds resources and the selected baked config, writes the packaged build name, and may adjust PE PDB paths. It does not generate or execute code in those reserved regions. Signing, when configured, happens after patching and before archives or installers are emitted.

## Run the Engine packaging fixture

`Examples/PackagingMatrix` is the executable Engine-owned baseline for native package mechanics. It is intentionally separate from the readable starter and multiplayer tutorials because `ConfigBaker` requires every server/client runtime setting to be initialized. Its checked-in `FOnlinePackagingMatrix.fomain` is deterministically generated from `Source/Common/Settings.inc`; `generate_config.py --check` fails when settings and the fixture diverge.

In a standalone checkout of `Examples/PackagingMatrix` with its `Engine`
submodule initialized, configure the host build and build the fixture-owned
`RunPackagingChecks` target. This target is not registered as a required
Engine `BuildTools validate` lane.

```bash
cmake --build <packaging-matrix-build-dir> --config Release --target RunPackagingChecks
```

Each route builds the client, headless client, server, headless server, host service/daemon role, and baker; force-bakes resources plus server/client `PackageSmoke` configs; creates raw payloads and ZIP or TAR.GZ archives; compares archive members with staged payloads; and starts the packaged headless client against the packaged server through the real updater handshake. Both processes must observe `Common.Packaged`, consume the embedded fixture setting, emit success markers, and stop with code zero.

The verifier writes `FOPKG-PackageSmoke/packaging-manifest.json` with the exact Engine revision, archive hashes and sizes, complete payload inventories, role presence, and runtime results. Preserve the manifest and archives in the embedding project's release lane when this evidence is required; the current Engine workflow does not publish them.

This fixture provides opt-in evidence for the Windows x64 or Ubuntu/Linux x64 Engine package path on the host where it is run. It does not qualify another game's package declaration, signing, installer, store, deployment host, database, renderer, or rollback. Copy the evidence pattern into the embedding project's required release lane and keep its concrete acceptance there.

### Public multiplayer package acceptance

`Examples/MinimalMultiplayer` applies that pattern to a readable game source.
Its `Tutorial` package force-bakes the automated gameplay configuration and
emits native raw plus ZIP/tar.gz client/server payloads. The example-owned
verifier checks archive/payload parity, records SHA-256 for every archive and
payload file, and runs the packaged headless server/client map and item
interaction through the shared gameplay process runner.

The checked-in `.fomain` is generated from current `Settings.inc` defaults plus
reviewed tutorial overrides and sections. `CheckTutorialConfig` runs before
baking, so a new or changed saved setting fails on stale source instead of
appearing later as an incomplete packaged config.

The `windows-package` and `linux-package` presets in `Examples/MinimalMultiplayer`
build the fixture-owned `RunTutorialPackageChecks` target on demand. Preserve
the archives, package manifest, and runtime report in a project workflow when
they are required; the current Engine workflow does not run these presets.
This evidence remains narrower than a product release: the
archives are unsigned, headless, audio-disabled fixtures with no installer,
store, public deployment, durable backend, upgrade, or rollback claim.

The Windows x64 lane passed locally on Engine `fac978a67`: two archives matched
their raw payloads, the 28-file client and 37-file server inventories were
hashed, and the packaged interaction scenario passed. This is local host
evidence only. Linux support and immutable example-release evidence require a
green landed job and a reviewed external repository commit/tag.

## Select artifacts by platform

### Windows client

- `Raw` retains the staged portable directory.
- `Zip` emits a portable archive from that directory.
- `Wix` emits an MSI and requires the WiX/wixl path used by `BuildTools/msicreator`.
- `OGL` adds the separately built OpenGL runtime variant.
- `Lib` selects the library form where the target supports it.
- `POSTFIX` keeps independently built variants, such as a depot-specific client, from colliding.

An MSI is not proof that the client is signed, trusted by endpoint protection, upgrade-compatible, or accepted by a distribution channel. Verify those properties on the final emitted artifact.

### Linux client or server

- `Raw`, `Zip`, `Tar`, and `TarGz` are available output forms.
- `Headless` includes the headless variant in addition to the ordinary target.
- `Daemon` includes the Linux daemon server variant.
- `TotalProfiling` and `OnDemandProfiling` add separately compiled profiling variants where valid.

Preserve executable modes when a downstream publication system unpacks and repacks an archive. Qualify the actual Linux distribution, runtime libraries, filesystem paths, process account, signals, logs, and service manager used by the game.

### Web client

The Web client payload contains JavaScript, patched Wasm, an HTML shell, preloaded `Resources.data` / `Resources.js`, and optionally the local `WebServer` helper. The Engine-owned Content Showcase command `python validate.py --web-runtime` can provide opt-in evidence for native-host baking, exact raw/ZIP package inventory, localhost HTTP delivery, a native server connection, required lifecycle markers, a real WebGL 2 context, and compositor pixels for one deterministic fixture under pinned Chromium. It is not a required Engine workflow lane and does not prove an embedding game's public browser deployment.

Follow [Web Build, Packaging, and Browser Debugging](../platforms/web-debugging.md) for local staging. A release lane must additionally verify HTTPS hosting, MIME types, cache policy, cross-origin isolation or other required headers, WebSocket reachability, browser compatibility, storage persistence, audio activation, loading failure UX, and at least one visible representative scene.

### Android client

The Android payload is a generated Gradle project with one `libmain.so` per selected ABI and baked resources under application assets. `Apk` runs the Gradle assembly and copies the resulting APK beside the staged project.

Follow [Android Build, Packaging, and Device Debugging](../platforms/android-debugging.md) for the pinned SDK/NDK workspace, ABI mapping, device connection, resource staging, and configuration fields. Android ARM32 and ARM64 are build-gated; Android x86 remains source-capable. A game must own emulator/device gates, GPU/input/audio/network/background behavior, signing identity, versioning, store policy, and rollout.

An APK produced without release keystore settings uses the Gradle development key and is not a production release artifact.

### macOS and iOS

`package.py` currently aborts for both `macOS` and `iOS`. Do not add `DefinePackage` rows for them. The support matrix build-gates client inputs at narrower scopes, but the embedding project must supply and maintain application-bundle assembly, resources, entitlements, provisioning, signing, notarization where applicable, device/simulator checks, store metadata, and delivery. Until that route exists and is repeatable, describe Apple targets as build-gated inputs rather than packaged or release-supported products.

### Server, service, and daemon

A server package includes server resources and client update resource packs. When matching client runtime libraries are available, it also stages platform runtime payloads used by the updater. Review [Client Runtime Split and Updater](../../explanation/runtime/client-updater.md) before publishing a server whose clients self-update.

`Service` and `Daemon` are binary variants, not deployment systems. The game must version and test:

- process arguments and environment;
- least-privilege account and filesystem permissions;
- service-manager definition and restart limits;
- network exposure and TLS termination;
- database schema, credentials, migration, backup, and restore;
- logs, metrics, crash reports, health checks, and alerting;
- graceful drain/shutdown and rollback to a compatible binary/config/data set.

Keep those product and infrastructure details in the embedding project. [Release Operations](operations.md) supplies the reusable process, readiness, rollout, shutdown, and rollback runbook without claiming that infrastructure as Engine-owned.

## Reproducibility and provenance

FOnline makes resource-pack ZIP entries deterministic by sorting normalized paths and fixing ZIP timestamps and permissions. Embedded resource ZIP data uses the same rule. The package declaration parser and generated contract are deterministic and checked in CI.

That does not make every complete release bit-for-bit reproducible. Linked binaries, debug symbols, top-level archives, MSI/APK toolchains, signing timestamps, included files, and external SDKs may carry host- or time-dependent data. State the narrower guarantee you have actually tested.

For every candidate, emit or retain an artifact manifest containing at least:

- game and Engine full SHAs;
- dirty-tree status;
- submodule revisions;
- host image, compiler/linker, Python, CMake, SDK/NDK/Gradle/WiX versions;
- package ID, config, target, platform, architecture, and pack tokens;
- ordered artifact paths, sizes, and SHA-256 hashes;
- signing subject/key alias, signature verification result, and timestamp status without credentials;
- test job/run identifiers and acceptance result;
- third-party license/provenance inventory;
- updater generation/runtime ABI when the artifact participates in native updates.

Generate hashes from the files that will actually be published, after signing and final packaging. Store immutable manifests beside immutable artifacts. Comparing two hashes is meaningful only when their declared input and signing policies match.

## Signing and secret boundaries

Windows signing is optional and off by default. `Packaging.CodeSigningHook` points to a project-owned executable through a directly usable non-secret path in project config; the packager does not resolve target directives for it. The packager invokes the hook once per staged `.exe` and `.dll` after binary patching and before archive/MSI creation. A nonzero hook exit fails packaging. The hook owns the signing provider, certificate, timestamp service, retry policy, credential environment, and verification.

Android release fields come from `Android.Keystore`, `Android.KeystorePassword`, `Android.KeyAlias`, and `Android.KeyPassword` in the baked effective target config. Password strings are passed to Gradle through dedicated environment variables rather than written into the generated project, but they have already traversed baked config and target directives are not resolved. The current Engine therefore has no host-only Android signing-secret handoff; keep production credentials out of Engine config and use a protected project-owned signing stage.

Never commit a private key, token, password, keystore password, signing session, production endpoint secret, or decrypted credential to a `.fomain`, package include, log, test fixture, documentation page, artifact manifest, or CI artifact. Use the project's secret manager, limit credentials to the packaging job, redact command output, and verify that fork/untrusted jobs cannot request them. Record identities and verification results, not secret values.

[Security and Secrets](security-and-secrets.md) owns substitution timing, the narrow `Common.SecretSettingTokens` masking boundary, the current package-secret limitations, CI isolation, rotation/revocation, incident routing, and secret-free artifact verification.

Signing proves artifact integrity and publisher identity. It does not prove gameplay correctness, malware absence, store acceptance, updater compatibility, or safe rollback.

## Acceptance matrix

Run the narrow Engine checks first, then the game-owned release checks. A green build alone is insufficient.

| Layer | Minimum release evidence |
|---|---|
| Contract | Package model/check tests and no undeclared grammar drift |
| Clean inputs | Exact clean game/Engine revisions and initialized submodules |
| Build | Clean release configure and all declared binary variants |
| Bake | Fresh resources, scripts, metadata, and selected client/server config |
| Package | Expected payload tree and every requested artifact emitted |
| Contents | Allowlist/denylist, symbols policy, licenses, no secrets, no stale files |
| Integrity | Final SHA-256 manifest and signature verification where required |
| Install/start | Real archive/installer/APK/site/service route on representative target |
| Runtime | Login/connect, representative map/content/UI, save/persistence, clean shutdown |
| Platform | Renderer, input, audio, networking, lifecycle, permissions, update behavior |
| Server operations | Service/daemon lifecycle, database migration, backup/restore, observability |
| Compatibility | Network, save/schema, updater generation/runtime ABI, old-client policy |
| Recovery | Previous artifact/config/data restoration under a rehearsed time objective |

Promote a package row to project-qualified only when the named lane is versioned, repeatable, and required for release. Record a failure as a failed candidate; do not overwrite an earlier immutable artifact under the same version.

## Release checklist

1. Freeze the game revision, exact Engine revision, dependency graph, package declarations, and release config.
2. Confirm the target rows are package-capable and their support labels are current.
3. Start from a clean workspace and record toolchain/SDK versions.
4. Build every declared application variant with zero warnings.
5. force-bake and validate the exact release configs and content.
6. Package each host-owned package ID separately and retain complete logs.
7. Audit payload contents, license/provenance records, writable-path behavior, symbols, and secret absence.
8. Sign where required, verify signatures, then hash final artifacts and write the manifest.
9. Install or deploy the emitted artifact through the real distribution path and run the declared acceptance lane.
10. Validate updater, network, save/database, and rollback compatibility against the versions the game still supports.
11. Rehearse or verify [backup/restore](backup-and-recovery.md) and previous-release rollback before broad rollout.
12. Publish immutable artifacts and manifests, monitor the staged rollout, and retain the previous compatible release until acceptance completes.

## Failure routing

| Symptom | Inspect first |
|---|---|
| `Config file not found` | `Config` baker, package `CONFIG`, `Baking/Configs`, and fresh bake output |
| Missing binary input | built target/variant, platform architecture key, `POSTFIX`, and shared `FO_OUTPUT_PATH` |
| Unknown or invalid pack token | generated package matrix rather than remembered combinations |
| Package succeeds but requested artifact is absent | output-producing pack token and final package output path |
| Signing was skipped | project config resolution and `Packaging.CodeSigningHook` or Android keystore fields |
| Signing fails | isolated signing hook/Gradle job, credentials, timestamp service, and final-byte order |
| Web files load but the game does not connect | HTTP/browser diagnostics, WebSocket endpoint, and embedded release config |
| APK installs but resources are missing | generated assets, runtime staging, app update/version behavior, and storage logs |
| Service or daemon exits immediately | package payload first, then project-owned account/config/database/network/log policy |
| Update loops or loads the wrong runtime | package `POSTFIX`, staged runtime name, updater generation/ABI, and server payload inventory |
| Same sources produce different hashes | compare the full input/tool/signing manifest before calling the packager nondeterministic |

## Source paths inspected

- `BuildTools/PackageInterface.json`
- `BuildTools/package.py`
- `BuildTools/cmake/helpers/Build.cmake`
- `BuildTools/cmake/stages/Packages.cmake`
- `BuildTools/tests/test_docs_package.py`
- `BuildTools/tests/validate_package_interface.cmake`
- `BuildTools/tests/test_package_include.py`
- `BuildTools/tests/test_package_security.py`
- `BuildTools/tests/test_package_zip_determinism.py`
- `BuildTools/tests/test_packaging_matrix.py`
- `BuildTools/tests/test_minimal_multiplayer_package.py`
- `BuildTools/msicreator/createmsi.py`
- `BuildTools/check_windows7_imports.py`
- `BuildTools/SupportMatrix.json`
- `.github/workflows/validate.yml`
- `Examples/PackagingMatrix/`
- `Examples/MinimalProject/`
- `Examples/MinimalMultiplayer/`

## See also

- [Generated Package Interface](../../reference/packages/index.md)
- [Build Workflow](../build/)
- [Project-Local Dependencies](../../../ProjectDependencies.md)
- [Support Matrix](../../reference/platforms/support-matrix.md)
- [Project Configuration](../build/project-configuration.md)
- [Generated Content Workflow](../build/generated-content.md)
- [Web Build, Packaging, and Browser Debugging](../platforms/web-debugging.md)
- [Android Build, Packaging, and Device Debugging](../platforms/android-debugging.md)
- [Client Runtime Split and Updater](../../explanation/runtime/client-updater.md)
- [Release Operations](operations.md)
- [Engine Upgrade Guide](../migration/engine-upgrade.md)
