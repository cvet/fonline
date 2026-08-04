---
permalink: /Examples/NativeExtensionSample/README.html
locale: en
document_id: native-extension-sample-readme
---

# FOnline Native Extension Sample

This engine-owned project demonstrates one complete project-native C++ path without game-specific services or third-party SDKs. It is separate from the minimal starter so the native ownership and compatibility boundary remains visible.

The sample proves:

- `SourceExt/ServerExtension.cpp` is registered in the narrow `SERVER` role;
- `ServerInitHook` installs state owned by one `ServerEngine` instance and releases it through the engine allocator;
- `Server_Game_NativeExtensionValue` becomes `Game.NativeExtensionValue()` in server AngelScript;
- `NativeExtensionCore` is linked through `AddProjectLibraries`, with a compile-time check that rejects incorrect role routing;
- `FONATIVE_NativeExtensionCoreTest` tests the engine-independent fixed-width value boundary;
- `run_native_extension_smoke.py` proves the lifecycle hook, generated script binding, state read, and clean server shutdown.

## Run The Checks

Initialize `Engine/` at the exact revision recorded by the repository, then run:

```bash
python validate.py
```

Equivalent commands are:

```bash
# Windows
cmake --preset windows
cmake --build --preset windows-check

# Linux
cmake --preset linux
cmake --build --preset linux-check
```

Linux hosts first install the prerequisites owned by the pinned Engine revision:

```bash
Engine/BuildTools/prepare-workspace.sh linux-packages linux
```

Success requires both the focused native marker and the runtime markers:

```text
native_extension_core_test_passed
native_extension_hook_initialized
native_extension_value=42
native_extension_smoke_passed
```

## Ownership And ABI Boundary

The project compiles the extension and Engine from source in one build. It does not promise a stable binary ABI across Engine revisions. Update the Engine gitlink, reconfigure, rebuild, rebake, and rerun both pinned/current compatibility lanes as one review.

`NativeExtensionCore` exposes only fixed-width values to its focused test. Engine handles stay in the registered extension translation unit and use `ptr<T>` borrows. Per-server state lives in `ServerEngine.UserData`; there is no file-scope mutable state. A real project with several native subsystems should place one project-owned aggregate in that slot instead of letting independent extensions compete for it.

The sample contains no distributable assets, runtime libraries, credentials, or service integration. Add those only with explicit provenance, platform support, package acceptance, and security ownership.

## Related Documentation

- [Native Extensions](../../Docs/en/how-to/native-extensions.md)
- [Project-Local Dependencies](../../Docs/en/how-to/native-extensions/project-dependencies.md)
- [Engine Upgrade Guide](../../Docs/en/how-to/migration/engine-upgrade.md)
- [Public Example Repositories](../../Docs/en/how-to/build/public-example-repositories.md)
