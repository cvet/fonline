---
title: Native Extension Roles
document_id: generated-native-extension-roles
locale: en
generated: true
---

# Native Extension Roles

> Generated reference. Do not edit directly. Update `BuildTools/NativeExtensionInterface.json`, then run `python BuildTools/docs_native_extension.py --write`.

[Index](index.md) | [Roles](roles.md) | [Hooks](hooks.md) | [Bindings](bindings.md) | [Canonical JSON](../native-extension.json) | [Guide](../../NativeExtensions.md)

`AddEngineSources` accepts only these roles. Every resolved source also enters `FO_SOURCE_META_FILES` before code generation.

| Stable ID | Role | Source list | Library | Primary header | Consumers | Script targets | Purpose |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-native-extension-role-common-7fefc75aab"></a><code>native-extension.role.common</code> | <code>COMMON</code> | <code>FO_COMMON_SOURCE</code> | <code>CommonLib</code> | <code>Common.h</code> | <code>client</code>, <code>server</code>, <code>mapper</code>, <code>baker</code>, <code>editor</code>, <code>tests</code> | <code>common</code> | Code shared by every enabled engine role; use only for genuinely common dependencies and hooks. |
| <a id="entry-native-extension-role-server-a000ac35db"></a><code>native-extension.role.server</code> | <code>SERVER</code> | <code>FO_SERVER_SOURCE</code> | <code>ServerLib</code> | <code>Server.h</code> | <code>server</code>, <code>tests</code> | <code>server</code> | Authoritative server-only code and script exports. |
| <a id="entry-native-extension-role-client-a9db128cb5"></a><code>native-extension.role.client</code> | <code>CLIENT</code> | <code>FO_CLIENT_SOURCE</code> | <code>ClientLib</code> | <code>Client.h</code> | <code>client</code>, <code>mapper</code>, <code>tests</code> | <code>client</code>, <code>mapper</code> | Client runtime code; MapperLib links ClientLib and receives client-side metadata registrations. |
| <a id="entry-native-extension-role-mapper-f90930cf33"></a><code>native-extension.role.mapper</code> | <code>MAPPER</code> | <code>FO_MAPPER_SOURCE</code> | <code>MapperLib</code> | <code>Mapper.h</code> | <code>mapper</code>, <code>tests</code> | <code>mapper</code> | Mapper-only native tooling and mapper script exports. |
| <a id="entry-native-extension-role-baker-46164f8207"></a><code>native-extension.role.baker</code> | <code>BAKER</code> | <code>FO_BAKER_SOURCE</code> | <code>BakerLib</code> | <code>Baker.h</code> | <code>baker</code>, <code>editor</code>, <code>tests</code> | - | Resource-baker extensions; EditorLib links BakerLib, but BAKER is not a script export target. |

## Registration shape

```cmake
AddEngineSources(
    COMMON SourceExt/CommonExtension.cpp
    SERVER SourceExt/ServerExtension.cpp
    CLIENT SourceExt/ClientExtension.cpp)
RegisterEngineSources()
```

Paths and globs are resolved relative to the embedding-project contribution root. Unknown roles are configure errors.
