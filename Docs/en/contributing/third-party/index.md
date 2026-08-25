---
layout: default
title: ThirdParty Maintenance
document_id: third-party-maintenance
locale: en
permalink: /Docs/en/contributing/third-party/
---

# ThirdParty Maintenance

This document owns the reusable engine workflow for vendored dependencies under
`ThirdParty/`. Project-specific bundled libraries belong to the embedding
project; follow [Project-Local Dependencies](../../how-to/native-extensions/project-dependencies.md) for their
selection, integration, delivery, and maintenance contract, then keep exact
project inventory and release evidence there.

## Ownership

`ThirdParty/<Library>/` directories are engine-owned vendored source trees used
by `BuildTools/cmake/stages/ThirdParty.cmake` and related CMake helpers.

Plain files directly under `ThirdParty/`, such as `emscripten`,
`android-sdk`, `dotnet-runtime`, and `xwin`, are version pins consumed by
`BuildTools/buildtools.py` and workspace/package preparation scripts. Do not add
a root-level `ThirdParty/FONLINE_PRUNED_FILES.md`; prune notes belong inside
actual vendored library directories.

Keep library licenses, notices, readmes, and changelogs unless there is a clear
legal or packaging reason to do otherwise. Removing unused examples, tests,
docs, CI files, helper scripts, editor settings, package metadata, and release
automation is acceptable when the engine does not build or distribute those
parts.

The polygonal sprite baker currently consumes the pruned `clipper2` and
`earcut` trees directly: Clipper2 owns contour offset/union/intersection work,
while earcut triangulates outer rings and holes. Update either dependency with
`Source/Tools/SpriteMeshing.cpp`, `Source/Tests/Test_ImageBaker.cpp`, a clean
resource bake, and the sprite-mesh report/atlas diagnostics in scope. A
compile-only check does not prove deterministic geometry or visible-pixel
coverage.

## Update Workflow

The whole `ThirdParty/` surface is refreshed in one sweep: every maintained
vendored library and every root version pin is checked against upstream, and
each dependency that moves gets its own commit. Prefer the freshest **stable**
release; take a master/branch head or an upstream-designated tag only when that
is the library's own release convention (no stable releases, or the project is
deliberately tracked at a branch).

1. Identify the upstream release, tag, archive, or package version from the
   project homepage, official release feed, or package metadata. Verify the
   candidate against the project's own release channel, not raw git tags — a
   repository may carry version tags that were never released (no archive, no
   announcement); such tags are not update targets.
2. Stage the upstream source outside the repository, usually under
   `Workspace/ThirdPartyUpdate/`, so the original download remains available
   while reviewing the vendored diff.
3. Copy the new source into the matching `ThirdParty/<Library>/` directory.
   Prefer preserving the upstream layout for files that the engine build
   consumes.
4. Reapply pruning from `ThirdParty/<Library>/FONLINE_PRUNED_FILES.md`.
   Update that file whenever a new upstream directory or file is intentionally
   removed.
5. Keep the files needed by the engine build, public headers, source files,
   license/notices, readmes, changelogs, and CMake files that are still part of
   the build graph.
6. Mark every engine-local edit inside a vendored file with `(FOnline Patch)`
   and a short reason. Prefer one-line comments beside the changed condition or
   setting.
7. Update `ThirdParty/README.md` with the new version. If the update changes
   platform workspace pins, update the matching plain file under `ThirdParty/`.
8. Run at least `git diff --check` on the touched dependency. Then validate the
   smallest embedding-project configure/build/test path that exercises the
   dependency. For build-critical libraries (allocator, shader toolchain,
   serialization), prefer a functional pass over the real data path - e.g. a
   full resource bake or the engine unit-test suite - not just compile-and-link.
   If a vendored C update raises its language requirement, declare the standard
   on that dependency target (`C_STANDARD` / `C_STANDARD_REQUIRED`) and include
   native MSVC in validation; a Windows cross build through clang does not prove
   that MSVC accepts C11 headers such as `<stdatomic.h>`. Current MSVC also
   requires `/experimental:c11atomics` on the affected C target. Mark vendored
   include directories as `SYSTEM` when first-party translation units consume
   them. When a linked vendored CMake target publishes ordinary
   `INTERFACE_INCLUDE_DIRECTORIES`, mark that target `SYSTEM` too; otherwise its
   transitive path can take precedence over a direct system path and let
   dependency-header warnings bypass third-party warning policy.
9. Commit each dependency or version pin separately. Use a direct message such
   as `Update SDL to 3.4.10`.

Vendored CMake logic is part of the toolchain contract. In particular,
AngelScript's x64 assembly selection must test
`CMAKE_ASM_MASM_COMPILER` for the MSVC/MASM path and
`CMAKE_ASM_COMPILER` for other assemblers after the language is enabled; a
generic `CMAKE_ASM_COMPILER_WORKS` probe is not a portable substitute on modern
CMake and Visual Studio generators. Keep local corrections marked
`(FOnline Patch)`, run
`python -m unittest BuildTools.tests.test_angelscript_cmake`, and complete at
least one configure plus assemble/link path on the affected host.

## Root Version Pins

The plain files directly under `ThirdParty/` (`emscripten`, `android-ndk`,
`android-sdk`, `android-api`, `dotnet-runtime`, `iOS-sdk`, `xwin`) pin toolchain
versions that `BuildTools/buildtools.py` downloads during workspace and package
preparation. They are part of the regular update sweep: bump the pin file and
the matching `ThirdParty/README.md` entry in one commit per pin, and validate
through the platform build that consumes it (web, Android, iOS, or Windows
cross build). Two pins are policy values rather than freshness targets:
`android-api` is the deliberate minimum-supported-device floor, and `iOS-sdk`
follows the embedding project's shipping requirements — change these only as an
explicit product decision, not as part of a mechanical refresh.

## Heavily Patched Forks

A dependency whose vendored copy carries extensive semantic `(FOnline Patch)`
edits (the current example is AngelScript, which embeds the engine's nullable
`T?` type system, VM stack-alignment layout, an added bytecode instruction, and
a modern-threads mode on top of an upstream fork) is **not** covered by the
mechanical copy-then-prune workflow above. Re-vendoring it means reconciling
every patched region against the new upstream — treat that as a dedicated task
with its own plan and full script/VM regression validation, and skip such
dependencies during a routine refresh sweep.

For a branch-tracked or WIP fork, record the exact upstream commit alongside
the snapshot date in `ThirdParty/README.md`; the upstream version string alone
does not identify a reproducible source tree.

## Adding A New Engine Dependency

Add reusable dependencies to the engine only when they are genuinely engine
surface area. Project-only native bridges and SDKs belong to the embedding
project and use the ownership gate in
[Project-Local Dependencies](../../how-to/native-extensions/project-dependencies.md).

For a new engine dependency:

- add the vendored source under `ThirdParty/<Library>/`;
- add `ThirdParty/<Library>/FONLINE_PRUNED_FILES.md`;
- add a version entry to `ThirdParty/README.md`;
- wire the dependency in `BuildTools/cmake/stages/ThirdParty.cmake` or a nearby
  reusable helper;
- register any `find_package()` interception needed to prevent accidental host
  library use;
- mark local vendored-file edits as `(FOnline Patch)`;
- **check whether the library exposes an allocator hook, and either wire it to
  `SafeAlloc` or record why not.** Libraries that allocate through C `malloc`
  land in the CRT heap rather than rpmalloc, outside the engine
  out-of-memory contract and invisible to allocator statistics and Tracy. Hooks
  come in several shapes — a runtime setter (`SDL_SetMemoryFunctions`,
  `asSetGlobalMemoryFunctions`, `Effekseer::SetMallocFunc`), a struct passed at
  init (`png_create_read_struct_2`, `bson_mem_set_vtable`), or a compile-time
  symbol the consumer defines (`UFBX_EXTERNAL_MALLOC`). Read the hook's
  *implementation*, not just its declaration: LibreSSL still exports
  `CRYPTO_set_mem_functions`, but its body is an inert `return 0;`. Check also
  whether a vtable is copied or retained by pointer, and whether an aligned
  allocation is released through the same free callback as an unaligned one —
  both have bitten this codebase. See
  [Essentials](../../reference/native/essentials.md#third-party-allocators);
- validate at least one configure/build path that consumes the dependency.

## Pruning Notes

Every library directory should document removed upstream paths in
`FONLINE_PRUNED_FILES.md`. Keep the note mechanical and easy to reapply:

```text
FOnline ThirdParty pruning notes

This vendored copy is intentionally trimmed for the engine build. When updating
from upstream, remove these paths again after copying the new version.

Removed paths:
- docs/
- examples/
- tests/
```

Do not list retained files as multiline bullets in a way that could be confused
with removed paths by simple maintenance scripts. If retained files need to be
called out, write them inline.

## Local Patch Marker

Use `(FOnline Patch)` only for edits made to third-party files. The marker must
explain why the upstream line is different, for example:

```cmake
option(ZLIB_BUILD_SHARED "Enable zlib shared library" OFF) # (FOnline Patch) engine links zlib statically.
```

Avoid reformatting large upstream files just to add the marker. Keep the local
delta small enough that the next update can reapply it by inspection.
