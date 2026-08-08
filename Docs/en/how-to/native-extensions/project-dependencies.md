---
layout: default
title: Project-Local Dependencies
document_id: project-local-dependencies
locale: en
permalink: /Docs/en/how-to/native-extensions/project-dependencies.html
---

# Project-Local Dependencies

This guide owns the reusable contract for libraries, SDKs, frameworks, tools,
and runtime payloads added by a game repository that embeds FOnline. It covers
dependencies that the game needs but the reusable Engine does not own.

Use [ThirdParty Maintenance](../../contributing/third-party/index.md) for source vendored in
`Engine/ThirdParty/`. Use [Native Extensions](../native-extensions.md) for the C++
bridge that consumes a project dependency. The embedding project must keep its
exact inventory, product-specific integrations, credentials, providers, and
release policy in its own repository.

## Dependency decision

Use this sequence for every project-local dependency:

1. Classify the owner first: Engine, embedding project, revisioned companion,
   project build tool, or operating-system prerequisite. Registration through
   Engine helpers does not transfer project ownership.
2. Select and pin the delivery model, then record version, provenance,
   integrity, license, supported platforms/toolchains, update path, and rollback
   pin in the project.
3. Create the project target after Engine third-party targets exist and before
   `BuildCoreLibraries()` consumes role lists. Link it with
   `AddProjectLibraries` only to the required `COMMON`, `SERVER`, `CLIENT`,
   `MAPPER`, or `BAKER` roles.
4. Distinguish requested, compiled, and initialized-at-runtime states. Keep
   allocator ownership, exceptions, CRT/toolchain, architecture, generated
   headers, and C ABI boundaries explicit rather than treating header presence
   as runtime support.
5. Treat a development copy and a release payload separately. Declare runtime
   files through package declarations, including target paths, notices, runtime
   file hashes, and signatures or signing ownership. Then start and probe the
   packaged artifact from an isolated directory on every claimed platform.

A complete release-delivery record names each evidence class separately:
package declarations, licenses and notices, runtime-file hashes, signatures or
signing ownership, and an isolated start of the packaged artifact. One of these
is not shorthand for the others.

Do not stop at a successful include or link. Acceptance must prove the intended
requested, compiled, and initialized states; shared-library ABI and allocator
ownership; package payload and integrity; and isolated runtime behavior.

## Contract Status

The project-facing CMake interface is `experimental` and revision-pinned. The
current Engine provides `AddProjectLibraries` for role-scoped linking, but does
not promise binary compatibility for a project dependency or native extension
across Engine revisions. Reconfigure and rebuild them together after changing
the Engine pin.

The Engine owns:

- the `COMMON`, `SERVER`, `CLIENT`, `MAPPER`, and `BAKER` link roles;
- configure-time validation and routing performed by `AddProjectLibraries`;
- the core-library graph that consumes each role list;
- package declarations and generic package assembly mechanics;
- the documented allocator, pointer, exception, and native-extension rules.

The embedding project owns:

- dependency selection, version, source, integrity, license, and support term;
- the CMake target, feature gate, role assignment, and unsupported stub;
- generated headers, build tools, platform prerequisites, and ABI compatibility;
- runtime libraries, data files, notices, signing, and package acceptance;
- vulnerability response, update cadence, rollback, tests, and release evidence.

Registration never transfers ownership to the Engine. A library becomes an
Engine dependency only through an explicit Engine change that moves the
implementation, tests, maintenance record, and supported-platform obligation.

## Choose The Owner First

Classify a dependency before adding files or CMake:

| Need | Owner and location | Rule |
| --- | --- | --- |
| Reusable Engine runtime, format, renderer, or tool capability | Engine, normally `Engine/ThirdParty/<name>/` | Follow the Engine vendoring and public-contract review. |
| Game-only native bridge, service client, proprietary SDK, or content runtime | Embedding project, commonly `SourceExt/<name>/` or `Dependencies/<name>/` | Keep its implementation, policy, and release evidence project-owned. |
| Reusable but optional integration that should not be Engine core | Revisioned companion repository | Publish an exact Engine compatibility range, its own tests, and a minimal embedding example. |
| Build-time generator or audit not linked into Engine roles | Project tooling tree | Pin its runtime/packages separately and do not add it to an Engine role. |
| Operating-system framework or host library | Project platform configuration | Name the supported hosts and fail configure when a required prerequisite is absent. |

Do not duplicate an Engine dependency in the project merely to reach its
headers or target. If a project deliberately needs a different build or
version, document symbol isolation, allocator/ABI boundaries, platform scope,
and why the Engine copy cannot be reused.

## Select A Delivery Model

Use the smallest model that gives deterministic builds and lawful delivery:

1. **Vendored source** is preferred when the project must build the library on
   all supported hosts, apply a small reviewed patch, or avoid host-version
   drift. Pin the upstream version and archive hash, preserve required notices,
   and record pruning.
2. **Imported SDK target** fits a proprietary or prebuilt SDK. Pin the SDK
   release, architecture, compiler/runtime compatibility, acquisition source,
   redistributable files, and license terms. Do not commit material that the
   license forbids distributing.
3. **System or platform library** fits an OS API or a deliberately supported
   host prerequisite. Keep the use behind explicit platform checks and prove
   the minimum supported host. A successful developer-machine lookup is not a
   portable dependency contract.
4. **Package-manager or fetched source** is acceptable only with an immutable
   version/commit and integrity lock. Release and CI builds must not silently
   select a newer package or depend on an unreviewed network response.
5. **Runtime-only payload** fits a shared library, helper executable, model, or
   data file that is not compiled. It still needs a version, provenance,
   platform/architecture mapping, license decision, package rule, and launch
   acceptance test.

Do not use an unpinned branch, floating package range, ambient include path, or
unregistered `find_package()` result as production input.

## Keep A Dependency Record

Every project-local dependency should have one authoritative record in the
embedding repository. It may be a table, manifest, or dependency-owned README,
but it must answer:

| Field | Required content |
| --- | --- |
| Identity | Upstream name, project target name, owner, and support contact. |
| Version | Exact release/tag/commit and the in-source or package metadata used to verify it. |
| Provenance | Official source URL or private artifact identity plus archive/commit hash. |
| Delivery | Vendored, imported SDK, system, fetched, tool-only, or runtime-only. |
| License | License identifier, retained files, attribution, redistribution and source-offer obligations. |
| Integration | Consuming Engine roles, native bridge, feature flag, generated files, and allocator hook. |
| Support | Platforms, architectures, toolchains, configurations, and unsupported behavior. |
| Package | Runtime files, target paths, signing owner, and acceptance probe. |
| Security | Advisory source, review cadence, secret boundary, and emergency disable/remove path. |
| Update | Local patches, pruning record, compatibility-coupled assets/data, tests, and rollback pin. |

The dependency source is authoritative for the version when it exposes one.
Keep human inventories synchronized with that value; do not make a prose-only
version string the build's source of truth.

## Integrate At The Project Boundary

Create project dependency targets after the Engine ThirdParty stage has
created reusable Engine targets and before `BuildCoreLibraries()` consumes the
role lists. Register project source before `RegisterEngineSources()` as usual:

```cmake
StartProjectGeneration()
RegisterProjectOptions()

# Register handlers needed by Engine or project third-party CMake before the
# ThirdParty stage installs its find_package() interceptor.
RegisterFindPackageHandler(OptionalBackend NotFoundFindPackage)
AddThirdPartyLibraries()

add_subdirectory(Dependencies/ProjectCodec EXCLUDE_FROM_ALL)

# A project wrapper gives one stable target for upstream target-name changes,
# include classification, compile definitions, and transitive requirements.
add_library(ProjectCodec INTERFACE)
target_link_libraries(ProjectCodec INTERFACE upstream_codec)
target_include_directories(ProjectCodec SYSTEM INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/ProjectCodec/include")

AddProjectLibraries(
    ROLES CLIENT BAKER
    LIBRARIES ProjectCodec)

AddEngineSources(CLIENT SourceExt/ProjectCodecBridge.cpp)
RegisterEngineSources()
SetupCodeGeneration()
BuildCoreLibraries()
```

`AddProjectLibraries` accepts one or more roles and one or more CMake targets or
explicit platform-library items. It deduplicates each item within a role,
rejects unknown roles, rejects missing lists, and rejects registration after
the CoreLibs stage. The allowed roles come from the same project-interface
contract as `AddEngineSources`; use the generated
[CMake helper reference](../../reference/cmake/helpers.md) for the exact signature.

Do not append to `FO_COMMON_LIBS`, `FO_SERVER_LIBS`, `FO_CLIENT_LIBS`,
`FO_MAPPER_LIBS`, or `FO_BAKER_LIBS` directly. Those lists are implementation
state, while `AddProjectLibraries` is the selected project-facing interface.
Likewise, a helper's presence under `BuildTools/cmake` does not make it public;
only commands in `BuildTools/cmake/ProjectInterface.json` are supported for an
embedding project.

## Route To The Narrowest Role

Dependency roles follow the native source roles:

| Role | Link owner | Typical consumers | Use for |
| --- | --- | --- | --- |
| `COMMON` | `CommonLib` | Every enabled runtime/tool role | A genuinely common process/config primitive. Avoid placing a client or server SDK here for convenience. |
| `SERVER` | `ServerLib` | Server and native tests that include it | Authority, persistence, server transport, or backend SDK code. |
| `CLIENT` | `ClientLib` | Client plus current server-controller, Mapper, viewer, Baker, ASCompiler, and test paths | Rendering, input, client transport, or client SDK code. Account for the wider current consumer graph. |
| `MAPPER` | `MapperLib` | Mapper and tests that include it | Mapper-only editor integration. |
| `BAKER` | `BakerLib` | Baker, Mapper, viewers, ASCompiler, and tests | Resource import, validation, conversion, or authoring support. |

Split a dependency wrapper when different roles need different headers,
features, or runtime payloads. Do not route a library through `COMMON` just to
repair a missing symbol.

## Control Package Discovery

The ThirdParty stage intercepts `find_package()` so nested third-party CMake
cannot silently use arbitrary host libraries. Register every expected package
name before `AddThirdPartyLibraries()`:

- map a package to a vendored/imported target in a project handler;
- use `NotFoundFindPackage` for a supported optional backend that must remain
  disabled;
- use `PassThroughFindPackage` only for an intentional host prerequisite whose
  installation and minimum version are documented;
- let an unregistered lookup fail configure and then make the ownership
  decision explicitly.

Do not disable the interceptor or add a broad fallback. A dependency's nested
optional probes are part of its supply-chain and support surface.

## Isolate Headers, Warnings, And Generated Files

Expose dependency headers through its target, preferably with
`target_include_directories(... SYSTEM ...)`, instead of a repository-wide
include path. Keep first-party bridge headers non-system so project warnings
remain errors.

Compile definitions and generated headers belong on the narrow wrapper target
that needs them. Ensure generators run before consuming targets, emit into the
build tree, and participate in clean builds. Do not register a vendored source
tree with `AddEngineSources`: every registered file enters metadata/codegen
inspection, which is intended for project extension declarations, not arbitrary
third-party code.

If warning-clean upstream source is impractical, keep any warning adjustment on
the third-party target. Do not lower warnings globally or suppress diagnostics
in the project bridge.

## Define The Platform Contract

For every optional or platform-specific dependency:

1. expose a project feature option with a deterministic default;
2. check platform, architecture, headers, import/static library, and runtime
   payload at configure time;
3. define one availability macro from the final result;
4. compile a no-dependency stub when a shared script/native symbol must remain;
5. make unsupported runtime use explicit rather than returning false success;
6. validate at least one enabled and one disabled build;
7. keep the package matrix aligned with the compiled availability.

Separate three states: requested, compiled, and initialized at runtime. The
presence of headers does not prove that the matching runtime library loads,
credentials are provisioned, or the external service is reachable.

## Package Runtime Payloads

Static source dependencies may add no runtime file, but they can still add
license obligations. Shared/imported SDKs usually require a platform- and
architecture-specific library beside the application. Tools may require helper
executables or data packs.

Use the package declarations described in
[Packaging and Release](../release/packaging.md) to include each required file
and notice. A development post-build copy helps local launching but is not a
release package rule. Package acceptance must start the artifact from an
isolated directory and exercise the feature far enough to detect a missing or
wrong-architecture payload.

Record runtime file hashes in release provenance. Apply signing/notarization at
the release-owned boundary and verify signatures after package assembly. Never
place private SDK credentials, service tokens, signing keys, or license-server
secrets in source, CMake cache defaults, generated metadata, examples, logs, or
documentation.

## Respect ABI, Allocation, And Lifetime

Build source dependencies with a compatible compiler, architecture, C/C++
runtime, exception, RTTI, and configuration policy. For a prebuilt SDK, use
only the vendor-supported combination and fail configure for unsupported
combinations.

Do not transfer ownership of Engine containers, strings, exceptions, or owning
pointers across an undocumented shared-library ABI. Convert at the bridge,
keep allocator ownership on the side that allocated the object, and expose a
small SDK-native or C ABI where possible.

Inspect allocator hooks in the dependency implementation, not only its public
declaration. If the library can use Engine allocation, wire a lifecycle-correct
hook and test allocate/reallocate/free symmetry, aligned allocation, and
shutdown order. Otherwise record that it uses a separate heap and keep its
objects out of Engine ownership/statistics assumptions. Follow
[Essentials](../../reference/native/essentials.md#third-party-allocators) and
[Smart Pointers](../../contributing/coding-contracts/smart-pointers.md) at the bridge.

Global SDK state must have explicit process-wide semantics. Per-client,
per-server, or per-test-instance state belongs to an Engine/project instance;
initialize and shut it down through the owning lifecycle path, including failed
partial initialization.

## Review License And Supply-Chain Risk

Before first use and every update:

- obtain the release from the official upstream or approved private source;
- verify the pinned commit/archive and stored hash;
- review release notes, supported toolchains, license changes, and security
  advisories;
- preserve licenses, notices, attribution, changelog, and source-offer material
  required by the distribution model;
- inventory local patches and removed files;
- scan package output for accidental source archives, credentials, debug-only
  helpers, and unapproved runtime files;
- record an emergency disable, downgrade, or removal route.

The Engine does not determine whether a dependency license is compatible with a
game's commercial or distribution model. That is a project release/legal gate,
not a successful-build inference.

## Update Workflow

1. Record the old/new dependency identity and current Engine/project revisions.
2. Stage the candidate outside the authored tree and verify provenance.
3. Review license, advisories, API/ABI changes, build requirements, and
   platform support before replacing files.
4. Reapply documented pruning and the smallest possible local patches. Mark
   project-local edits consistently in the project policy.
5. Update the authoritative version record, integrity hash, notices, feature
   gates, package rules, compatibility-coupled assets/data, and owning docs.
6. Reconfigure every affected platform lane so cached discovery cannot hide a
   missing prerequisite or stale target.
7. Build every consuming Engine role and run focused bridge tests.
8. Assemble an isolated package, verify runtime payload hashes/signatures, and
   exercise enabled and disabled behavior.
9. Record failures, evidence, and the rollback pin. Keep the previous approved
   artifact available until acceptance is complete.

If an Engine update and dependency update happen together, audit them as two
compatibility ranges. Do not attribute a passing final build to either change
without a narrow check or bisectable evidence.

## Validation Matrix

At minimum, capture:

| Boundary | Required proof |
| --- | --- |
| Interface | `cmake -P BuildTools/tests/validate_project_interface.cmake` and current generated CMake reference. |
| Configure | Clean configure for every affected platform/architecture and for required enabled/disabled feature states. |
| Compile/link | Every role named in `AddProjectLibraries`; warning-clean first-party bridge and target-scoped dependency policy. |
| Codegen/script | Regenerated metadata plus resource bake when a native declaration or generated header changes. |
| Runtime | Focused success, unavailable, initialization-failure, and shutdown paths. |
| Package | Isolated launch, runtime payload presence/hash/architecture, notices, and secret-free artifact scan. |
| Upgrade | Old/new Engine pin comparison, dependency compatibility review, and no reused native/generated binary from the old pin. |

The Engine-owned minimal project compiles an `INTERFACE` project dependency
through `AddProjectLibraries(ROLES SERVER ...)`; its server extension fails
compilation if the role-scoped usage requirement is absent. This proves the
public link path without making an external SDK part of the fixture.

## Failure Routing

| Symptom | Inspect first |
| --- | --- |
| Unknown role or late registration | `AddProjectLibraries` call and stage order. |
| Header found locally but not in CI | Wrapper target include scope and accidental ambient include paths. |
| Unregistered `find_package()` failure | Nested dependency probe and the explicit handler decision. |
| Extension compiles but another role fails to link | Narrow role assignment, transitive target requirements, and actual core-library consumer graph. |
| Runtime library missing or wrong architecture | Package declaration, imported target location, post-build/package distinction, and artifact matrix. |
| Crash during allocation or shutdown | Allocator/free pairing, ABI ownership, global-state lifetime, and partial-init cleanup. |
| Feature says available but cannot initialize | Requested/compiled/runtime state separation and credential/service provisioning. |
| Update passes compile but assets fail | Version-coupled authored data, generator/runtime format, and real runtime validation. |

## Source Paths Inspected

- `BuildTools/Init.cmake`
- `BuildTools/cmake/ProjectInterface.json`
- `BuildTools/cmake/helpers/Build.cmake`
- `BuildTools/cmake/helpers/State.cmake`
- `BuildTools/cmake/stages/ThirdParty.cmake`
- `BuildTools/cmake/stages/CoreLibs.cmake`
- `BuildTools/cmake/stages/Packages.cmake`
- `Examples/MinimalProject/CMakeLists.txt`
- `Examples/MinimalProject/StarterServerExtension.cpp`

## See Also

- [Embedding Project](../build/embedding-project.md) - Engine/game repository ownership.
- [Native Extensions](../native-extensions.md) - project C++ roles, hooks, metadata, state, and testing.
- [ThirdParty Maintenance](../../contributing/third-party/index.md) - Engine-owned vendored source and patch workflow.
- [BuildTools Pipeline](../../reference/cmake-and-buildtools/pipeline.md) - public stages and selected helper boundary.
- [Packaging and Release](../release/packaging.md) - package declarations, payloads, signing, and acceptance.
- [Security and Secrets](../release/security-and-secrets.md) - credentials, redaction, trust, rotation, and incident handling.
- [Engine Upgrade Guide](../migration/engine-upgrade.md) - revision update and compatibility reconciliation.
