---
title: Generated Native Extension Interface
document_id: generated-native-extension-index
locale: en
generated: true
---

# Generated Native Extension Interface

> Generated reference. Do not edit directly. Update `BuildTools/NativeExtensionInterface.json`, then run `python BuildTools/docs_native_extension.py --write`.

[Index](index.md) | [Roles](roles.md) | [Hooks](hooks.md) | [Bindings](bindings.md) | [Canonical JSON](../native-extension.json) | [Guide](../../NativeExtensions.md)

This interface describes engine-owned composition and hook contracts for project-native C++ sources. It does not document any particular game's extension implementation.

## Contract status

| Field | Value |
| --- | --- |
| Stability | <code>experimental</code> |
| Support policy | Source-compatible use is documented for a pinned engine revision; independently built binary compatibility is not promised. |
| Source manifest | [BuildTools/NativeExtensionInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/NativeExtensionInterface.json) |
| Contract digest | <code>7309539ef4c0941c34b105b6bbbe58e553bbeec9d2b609e81b23bdae63208a02</code> |

| Reference | Entries | Purpose |
| --- | --- | --- |
| [Roles](roles.md) | 5 | CMake source routing, libraries, headers, and consumers. |
| [Hooks](hooks.md) | 8 | Optional declarations, call sites, defaults, and compatibility state. |
| [Bindings](bindings.md) | 6 | Registration, namespace, pointer, and dependency rules. |

## Boundary

Included:

- role-scoped project C++ source registration through AddEngineSources
- metadata/codegen participation of registered project sources
- optional EngineHook declarations, signatures, owning roles, call sites, and generated fallback behavior
- native binding and namespace rules required by generated script exports

Excluded:

- project-local extension implementations, dependencies, settings, persistence, and release policy
- third-party SDK ABI and project-specific native library packaging
- client host/runtime updater ABI and compatibility across independently built engine revisions
- native scripting backend internals controlled by FO_NATIVE_SCRIPTING
