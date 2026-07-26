---
title: Native Binding Rules
document_id: generated-native-extension-bindings
locale: en
generated: true
---

# Native Binding Rules

> Generated reference. Do not edit directly. Update `BuildTools/NativeExtensionInterface.json`, then run `python BuildTools/docs_native_extension.py --write`.

[Index](index.md) | [Roles](roles.md) | [Hooks](hooks.md) | [Bindings](bindings.md) | [Canonical JSON](../native-extension.json) | [Guide](../../NativeExtensions.md)

These rules are the reusable boundary. Project dependency setup, native state, settings, and packaging remain project-owned.

| Stable ID | Rule | Requirement | Why |
| --- | --- | --- | --- |
| <a id="entry-native-extension-binding-registration-order-29d16ead8d"></a><code>native-extension.binding.registration-order</code> | Registration order | Call AddEngineSources after AddThirdPartyLibraries and before the RegisterEngineSources stage entrypoint. | Project files must enter role source lists and FO_SOURCE_META_FILES before codegen and core libraries are configured. |
| <a id="entry-native-extension-binding-namespace-6dffbfe254"></a><code>native-extension.binding.namespace</code> | Engine namespace | Declare metadata exports inside FO_BEGIN_NAMESPACE/FO_END_NAMESPACE and define them with FO_NAMESPACE. | The same source must compile with the configured engine namespace enabled or disabled. |
| <a id="entry-native-extension-binding-script-export-49a22373ae"></a><code>native-extension.binding.script-export</code> | Script export frontier | Use FO_SCRIPT_API with a supported ///@ metadata tag; do not add stack-trace entry macros to exported bodies. | Codegen parses the declaration and emits the native/script registration boundary. |
| <a id="entry-native-extension-binding-pointer-contract-e9dcb3730c"></a><code>native-extension.binding.pointer-contract</code> | Pointer contract | Use ptr&lt;T&gt;/nptr&lt;T&gt; for engine handle borrows and the engine owning-pointer vocabulary for ownership. | Codegen rejects raw handle pointers and nullability must agree across native and script declarations. |
| <a id="entry-native-extension-binding-compatibility-54a8201a60"></a><code>native-extension.binding.compatibility</code> | Compatibility | Rebuild and rebake every project-side native metadata change against the pinned engine revision. | Registered metadata and most hook presence feed generated compatibility state; binary compatibility across independent revisions is not promised. |
| <a id="entry-native-extension-binding-dependencies-503563d0e1"></a><code>native-extension.binding.dependencies</code> | Dependencies | Declare project libraries, include paths, compile definitions, platform guards, and package payloads in the embedding project. | AddEngineSources owns source routing only; project dependency and distribution policy is not inferred by the engine. |

## Minimal exported method

```cpp
#include "Common.h"
#include "Server.h"

FO_USING_NAMESPACE();
FO_BEGIN_NAMESPACE
///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_ProjectValue(ptr<ServerEngine> server);
FO_END_NAMESPACE

int32_t FO_NAMESPACE Server_Game_ProjectValue(ptr<ServerEngine> server)
{
    ignore_unused(server);
    return 1;
}
```

The exported body intentionally has no stack-trace entry macro. Ordinary non-exported native functions keep the normal engine stack-trace convention.
