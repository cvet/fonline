# ThirdParty Maintenance

This document owns the reusable engine workflow for vendored dependencies under
`ThirdParty/`. Project-specific bundled libraries belong to the embedding
project and should document their extra rules there.

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

## Update Workflow

1. Identify the upstream release, tag, archive, or package version from the
   project homepage, official release feed, or package metadata.
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
   dependency.
9. Commit each dependency or version pin separately. Use a direct message such
   as `Update SDL to 3.4.10`.

## Adding A New Engine Dependency

Add reusable dependencies to the engine only when they are genuinely engine
surface area. Project-only native bridges and SDKs belong to the embedding
project.

For a new engine dependency:

- add the vendored source under `ThirdParty/<Library>/`;
- add `ThirdParty/<Library>/FONLINE_PRUNED_FILES.md`;
- add a version entry to `ThirdParty/README.md`;
- wire the dependency in `BuildTools/cmake/stages/ThirdParty.cmake` or a nearby
  reusable helper;
- register any `find_package()` interception needed to prevent accidental host
  library use;
- mark local vendored-file edits as `(FOnline Patch)`;
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

