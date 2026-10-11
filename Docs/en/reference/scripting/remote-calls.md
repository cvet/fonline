---
layout: default
title: Remote Calls
locale: en
document_id: remote-calls
permalink: /Docs/en/reference/scripting/remote-calls.html
---

# Remote Calls

> Engine-owned reference. This page defines the reusable FOnline remote-call contract. Each embedding project owns its concrete calls, authorization rules, generated call catalog, and compatibility policy.

## Purpose

Use remote calls for one-way script messages between an authenticated client runtime and the server, or from the server to a connected player's client. A remote call has four parts:

1. one common `///@ RemoteCall` declaration;
2. an outbound method generated on the sending side;
3. a side-specific AngelScript `[[ServerRemoteCall]]` / `[[ClientRemoteCall]]` or Managed C# `[ServerRemoteCall]` / `[ClientRemoteCall]` handler;
4. baked metadata that gives both runtimes the same name, argument order, types, nullability, and source-file hint.

Remote calls are not engine events, ordinary function calls, or request/response functions. They return `void`; a response is another explicitly declared remote call.

## Source paths inspected

- `Source/Tools/MetadataBaker.cpp`
- `Source/Common/EngineBase.cpp`
- `Source/Common/MetadataRegistration.cpp`
- `Source/Common/Settings.inc`
- `Source/Common/NetBuffer.cpp`
- `Source/Server/ClientDataValidation.h`
- `Source/Server/ClientDataValidation.cpp`
- `Source/Server/Server.cpp`
- `Source/Scripting/AngelScript/AngelScriptRemoteCalls.cpp`
- `Source/Scripting/AngelScript/AngelScriptAttributes.cpp`
- `Source/Scripting/Managed/ManagedScriptBackend.cpp`
- `Source/Scripting/Managed/CoreScripts/RemoteCall.cs`
- `Source/Scripting/Managed/CoreScripts/RemoteCallScriptFuncs.cs`
- `Source/Tools/ManagedScriptBaker.cpp`
- `Source/Tests/Test_MetadataBaker.cpp`
- `Source/Tests/Test_AngelScriptAttributes.cpp`
- `Source/Tests/Test_ManagedScriptBaker.cpp`
- `Source/Scripting/Managed/Tests/BootstrapScenarios.cs`
- `Source/Tests/Test_ClientDataValidation.cpp`
- `Source/Tests/Test_NetBuffer.cpp`
- `BuildTools/docs_metadata.py`
- `BuildTools/tests/test_docs_metadata.py`
- `Examples/MinimalProject/Scripts/Starter.fos`
- `Examples/MinimalProject/run_starter_smoke.py`

## Direction and runtime surface

The declaration target names the receiving side.

| Target | Sending side | Outbound surface | Receiving handler |
| --- | --- | --- | --- |
| `Server` | client | `player.ServerCall.Name(args...)` | `[[ServerRemoteCall]] void Namespace::Name(Player player, args...)` |
| `Client` | server | `player.ClientCall.Name(args...)` or `cr.PlayerClientCall.Name(args...)` | `[[ClientRemoteCall]] void Namespace::Name(args...)` |

For a `Server` target, the receiving handler gets the calling `Player` as its first argument. That argument is supplied by the engine and is not written in the `///@ RemoteCall` declaration. A `Client` target handler receives only the declared arguments.

`Player.ClientCall` targets that player's client. `Critter.PlayerClientCall` routes through the player associated with the critter. The caller object is part of routing; it is not serialized as a declared argument.

The mapper side does not support remote calls. `[[AdminRemoteCall]]` belongs to the separate admin-command path and is not declared as `///@ RemoteCall Admin` or emitted in project remote-call catalogs.

The transport contract is shared, while authoring syntax and current type coverage differ:

| Concern | AngelScript | Managed C# |
| --- | --- | --- |
| Inbound marker | `[[ServerRemoteCall]]` / `[[ClientRemoteCall]]` | `[ServerRemoteCall]` / `[ClientRemoteCall]` |
| Outbound call | generated `player.ServerCall.Name(...)`, `player.ClientCall.Name(...)`, or `cr.PlayerClientCall.Name(...)` | the same generated property and typed method names |
| Handler lookup | declaration file stem selects the matching namespace | assembly discovery selects an attributed static method by call name; no file-stem namespace rule |
| Handler return | `void`; `[[Async]]` uses AngelScript suspension | `void`, `Task`, or `Task<T>`; task completion/fault is observed and a `Task<T>` value is ignored because the wire call has no result |
| Current collection coverage | arrays, dictionaries, and dictionaries of arrays supported by the AngelScript serializer | scalars and arrays only; dictionaries are rejected until the Managed bridge adds them |

Both backends use the same baked `RemoteCallDesc`, native hostile-input validation, byte format, per-call limits, routing, and authority boundary. Backend parity means one network contract with explicitly documented capability differences, not two incompatible protocols.

## Declaration grammar

Declare each contract once in script source visible to metadata baking:

```angelscript
///@ RemoteCall Server RequestRename(string name) MaxBytes 256
///@ RemoteCall Client RenameResult(bool accepted, string reason) MaxBytes 512
```

The grammar is:

```text
///@ RemoteCall (Server|Client) Name([Type[?] argument [, ...]]) [MaxBytes N] [MaxCollectionSize N]
```

Rules enforced by `MetadataBaker` include:

- the target is exactly `Server` or `Client`;
- the name is followed by parentheses;
- every argument type resolves through engine metadata;
- every argument has a name;
- arguments are comma-separated;
- `?` is recorded separately from the resolved type.
- after the closing parenthesis, only `MaxBytes` and `MaxCollectionSize` are accepted, each at most once and followed by a non-negative integer; `0` or an omitted option means no per-call structural limit.

Names are registered in inbound and outbound maps and must be unique within each direction on a runtime side. Treat the call name, target, argument order, type, and nullability as one network contract.

## AngelScript file and namespace contract

The baker stores only the declaration source file name, such as `AccountUi.fos`, as the subsystem hint. The AngelScript binder removes the extension and resolves the receiving function in the matching namespace:

```angelscript
// AccountUi.fos
namespace AccountUi
{

///@ RemoteCall Server RequestRename(string name)
///@ RemoteCall Client RenameResult(bool accepted, string reason)

#if SERVER

[[ServerRemoteCall]]
void RequestRename(Player player, string name)
{
    // Authorize and validate before changing server state.
}

#endif

#if CLIENT

[[ClientRemoteCall]]
void RenameResult(bool accepted, string reason)
{
    // Update client presentation only.
}

#endif

}
```

For this file, the required namespace is `AccountUi`. Renaming or moving a script without keeping the file stem, namespace, declarations, and handlers aligned causes binding to fail. The runtime reports the expected declaration when an inbound handler is missing.

Handlers must use the attribute for their receiving side. The validator rejects a handler with the opposite attribute, an attributed function with no matching inbound declaration, and a normal direct call to a remote-call handler. Send through the generated caller surface instead:

```angelscript
// Client to server.
player.ServerCall.RequestRename("Ranger");

// Server to one player's client.
player.ClientCall.RenameResult(true, "");
```

## Managed C# binding contract

Place the same declaration tag in a `.cs` source included by the Managed resource pack. The Managed baker emits a typed `RemoteCaller` method for every outbound metadata record and adds the appropriate `ServerCall`, `ClientCall`, or `PlayerClientCall` property to generated entity types. Inbound methods are discovered across the project assembly; they must be static, carry the receiving-side attribute, have the exact call name and arguments, and add the engine-supplied `Player` first on the server:

```csharp
namespace Game.Scripts;

using System.Threading.Tasks;
using FOnline;

///@ RemoteCall Server RequestRename(string name) MaxBytes 256
///@ RemoteCall Client RenameResult(bool accepted, string reason) MaxBytes 512

public static class AccountUi
{
    [ServerRemoteCall]
    public static async Task RequestRename(Player player, string name)
    {
        // Authorize, validate, and reacquire cover after every await.
        await Game.YieldAsync(1);
    }

    [ClientRemoteCall]
    public static void RenameResult(bool accepted, string reason)
    {
        // Update client presentation only.
    }
}
```

Send through the generated typed surface:

```csharp
player.ServerCall.RequestRename("Ranger");
player.ClientCall.RenameResult(true, "");
```

`FOnline.RemoteCall.Send(...)` remains a low-level named entry point used by the generated surface and diagnostics; project gameplay should prefer the generated method so C# compilation checks the argument list. `RemoteCall.Loopback(...)` is diagnostic/test-only and does not prove a real peer, authentication, or transport path.

## Arguments and serialization

The shared wire format handles metadata-resolved primitives, integer-backed enums, `string`, `hstring`, registered reference types, fixed-layout structs, arrays, dictionaries, and dictionaries of arrays. The AngelScript bridge covers that complete list. The current Managed bridge accepts scalar values and arrays, and accepts only dynamic managed ref types; it rejects dictionaries, dictionary-of-array shapes, and native-only ref wrappers. A type being known to metadata is therefore not by itself a complete backend or transport test: compile/bake both sides and exercise the call through every shipped scripting backend and intended network path.

Use [Nullability.md](../../../Nullability.md) for the `T?` rules. The declaration is authoritative: the receiving handler must reproduce each argument's type and nullability exactly. Do not use nullable syntax to stand in for an optional field with domain-specific meaning; define that meaning explicitly in the protocol.

Keep payloads bounded and purpose-specific. In particular:

- prefer compact identifiers and re-resolve server-owned entities on receipt;
- impose project-owned length/count limits on strings and collections;
- avoid sending large state snapshots when an identifier and revision are sufficient;
- add an explicit request or correlation ID when more than one response may be outstanding;
- introduce a new call or a versioned payload when old and new clients cannot interpret the same signature.

For every untrusted client-to-server call, author `MaxBytes` as the largest legitimate serialized body and `MaxCollectionSize` as the largest legitimate size of any declared collection. The collection ceiling applies independently to arrays, dictionaries, and both the outer dictionary and nested arrays of a dictionary-of-arrays. A call without a declared limit retains `0` in metadata; this is an explicit unlimited structural value, not a recommended production default.

## Authority and failure handling

A client-to-server remote call is a request, not proof that an action is allowed. The server handler should derive caller identity from the engine-supplied `Player`, then validate authentication state, ownership, entity validity, current gameplay state, resource costs, rate limits, and any project-specific authorization before mutation.

Do not accept a player or owner identifier from the payload as a substitute for the calling `Player`. Resolve payload identifiers under the normal synchronization/entity-access contract and re-check state after any yield before using captured entities. Keep client handlers presentation-oriented; authoritative state remains on the server.

On the server, `ServerEngine::Process_RemoteCall()` rejects a negative or current-frame-exceeding payload size and resolves the declared call before allocating or copying its body. Unknown calls therefore cannot force body allocation. The effective byte ceiling is the smaller nonzero value of the call's `MaxBytes` and the server-wide `ServerNetwork.MaxRemoteCallPayloadSize` (default 1 MiB). The per-call value defines legitimate protocol shape; the global value remains a hostile-input safety ceiling.

The server then runs `ValidateInboundRemoteCallData()` before acquiring the calling `Player` cover or invoking the handler. The validator walks the metadata shape, checks enum/hash/reference data, rejects negative or over-limit collection counts, proves minimum remaining bytes before iterating a collection, and requires complete payload consumption. The AngelScript decoder independently enforces the same collection ceiling before reserve or construction, including nested dictionary arrays. The Managed decoder uses the shared `RemoteCallWire` reader, repeats the payload and array ceiling checks before building a managed `List<T>`, requires complete consumption, and invokes the attributed handler inside the backend context. These transport checks do not replace domain validation. Log failures with enough call/caller context to diagnose them without logging secrets or full untrusted payloads.

## Baked metadata

`MetadataBaker` emits one binary metadata file per target. A remote-call record contains:

```text
name, source-file hint, In|Out, type, nullable marker, argument name, ..., Limits, max-bytes, max-collection-size
```

For a `Server` target, server metadata records `In` and client metadata records `Out`. For a `Client` target, those directions are reversed. Every record has the mandatory three-token `Limits` trailer, including `Limits 0 0` when the declaration omits both options; dynamic registration rejects the older trailer-less shape. Registration turns valid records into `RemoteCallDesc` entries. AngelScript registers caller methods and binds the file/namespace handlers; the Managed baker generates typed caller methods and the Managed runtime binds attributed assembly methods by name.

The baked format retains the source file name but not a repository-relative path or declaration line. Documentation generated from `.fometa` must therefore expose that field as a source hint, not fabricate full provenance.

## Generate a project catalog

The engine-owned generator consumes the parser-owned baked files. It does not parse `.fos` or `.cs` with a second grammar:

```bash
python Engine/BuildTools/docs_metadata.py \
  --root . \
  --metadata Baking/Metadata/Metadata.fometa-server \
  --metadata Baking/Metadata/Metadata.fometa-client \
  --write
```

By default it writes these project-owned artifacts:

- `Docs/generated/project-remote-calls.json` for tools and AI retrieval;
- `Docs/generated/project-remote-calls.md` for GitHub Pages and human browsing.

Each call receives a stable ID of the form `script.remote-call.<target>.<name>` and the status `project-owned`. This status is an ownership boundary, not a promise that a game maintains backward compatibility for the call.

The generator strictly decodes the binary container, validates UTF-8 and the mandatory limits trailer, rejects duplicate inputs/calls, and requires matching server/client signatures and structural limits for every call. The JSON model and Markdown table expose the selected script backend, backend-correct handler attribute syntax, `MaxBytes`, and `MaxCollectionSize` alongside each call. A Managed handler is shown as an unqualified lookup signature because baked metadata does not preserve its declaring C# type or whether the implementation returns `void` or `Task`; the source hint remains the evidence needed to inspect that declaration. The generator also records SHA-256 hashes for the inputs. Use the same input paths with `--check` in project CI after baking:

```bash
python Engine/BuildTools/docs_metadata.py \
  --root . \
  --metadata Baking/Metadata/Metadata.fometa-server \
  --metadata Baking/Metadata/Metadata.fometa-client \
  --check
```

`--allow-unpaired` exists for diagnostics when only one side is available. Do not use it for a published production catalog: an unpaired record cannot prove that sender and receiver agree.

## Compatibility and release practice

Project-authored remote calls are baked project metadata and are intentionally outside the engine-native [generated API model](../../../generated/api.json). A game should check in its generated catalog, diff it during review, and deploy matching client/server project revisions.

Renaming a call, changing its target, reordering arguments, changing an argument type/nullability, or changing either structural limit is a network-contract change. Record the change in the embedding project's release notes and compatibility policy. Engine-level changes to remote-call transport, metadata shape, or registration must also follow the engine compatibility-version rule in [AGENTS.md](../../../../AGENTS.md#validation-routing).

## Troubleshooting

- `Remote call function not found`: in AngelScript check file stem versus namespace, side guards, handler name, and exact signature; in Managed C# check that the source entered the target assembly and that a matching attributed static method was discovered.
- Attribute validation failure: use only the receiving-side `[[ServerRemoteCall]]` / `[[ClientRemoteCall]]` or `[ServerRemoteCall]` / `[ClientRemoteCall]` marker.
- Managed collection/ref-type rejection: reduce the declaration to scalars, arrays, and dynamic managed ref types, or keep that call on AngelScript until the Managed bridge supports the required shape.
- Duplicate registration: call names collide within one inbound or outbound side; rename one contract.
- Unpaired documentation record: server/client metadata came from different or incomplete bakes, or one file is stale.
- Missing or mismatched limits record: rebake both targets with the same Engine/project revision; every record must end with `Limits <max-bytes> <max-collection-size>` and both sides must agree.
- Source link unavailable in generated output: `.fometa` stores only the source file hint; inspect the project source inventory instead of guessing a path.
- Runtime serialization failure: reduce the contract to known supported types and add a focused end-to-end transport test.

## Validation checklist

1. Keep each declaration common to the metadata inputs for both sides.
2. Implement the exact inbound signature under the correct side and attribute.
3. Run the project's normal resource bake; require warning-free AngelScript compilation and/or a clean Managed C# compile plus analyzer pass for every backend the project enables.
4. Generate and review the paired JSON/Markdown catalog from that bake.
5. Run `BuildTools/docs_metadata.py --check` with the same inputs in CI.
6. Exercise client-to-server authorization and server-to-client presentation through a real network or project integration test.
7. Test rejected permissions, stale identifiers, malformed domain values, global/per-call payload limits, and outer plus nested collection limits.
8. Give every incompatible call change an explicit release/compatibility disposition.

The engine-owned AngelScript [minimal project](../../../../Examples/MinimalProject/README.md) proves declaration parsing, both inbound handler bindings, paired baked metadata, stable catalog IDs, and server lifecycle startup. `Test_ManagedScriptBaker` plus the Managed CoreScripts tests prove generated caller shape and managed handler registration/loopback. Neither replaces a game's real multiplayer transport and authorization tests.

## See also

- [Scripting](../../explanation/scripting-runtime/) - scripting runtime and callback ownership.
- [Nullability.md](../../../Nullability.md) - script nullability and remote-call signature matching.
- [GeneratedApiAndMetadata.md](../metadata/index.md) - codegen versus baked metadata ownership.
- [Networking](../../explanation/authority-and-networking/) - transport and connection model.
- [Baking Pipeline](../../explanation/content-pipeline/baking.md) - resource-pack and baker execution.
