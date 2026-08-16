# Script Methods Map

> Engine-owned documentation. This page maps native `///@ ExportMethod` files in `Source/Scripting/` to their script-facing responsibilities. It complements [Scripting.md](Scripting.md); it is not a full generated API reference.

## Purpose

Use this page when adding, moving, or reviewing script-visible native methods. The goal is to keep method ownership obvious before editing codegen inputs:

- choose the correct runtime side (`Common`, `Server`, `Client`, or `Mapper`);
- choose the correct receiver/entity family (`Game`, `Entity`, `Critter`, `Map`, `Item`, `Location`, `Player`, `ImGui`);
- preserve authoritative server boundaries and client/view-only boundaries;
- update tests and docs when exported method groups move.

## Source paths inspected

All current native script method files were inspected:

- `Source/Scripting/ClientCritterScriptMethods.cpp`
- `Source/Scripting/ClientEntityScriptMethods.cpp`
- `Source/Scripting/ClientGlobalScriptMethods.cpp`
- `Source/Scripting/ClientImGuiScriptMethods.cpp`
- `Source/Scripting/ClientItemScriptMethods.cpp`
- `Source/Scripting/ClientLocationScriptMethods.cpp`
- `Source/Scripting/ClientMapScriptMethods.cpp`
- `Source/Scripting/ClientPlayerScriptMethods.cpp`
- `Source/Scripting/CommonGlobalScriptMethods.cpp`
- `Source/Scripting/CommonImGuiScriptMethods.cpp`
- `Source/Scripting/MapperGlobalScriptMethods.cpp`
- `Source/Scripting/ServerCritterScriptMethods.cpp`
- `Source/Scripting/ServerEntityScriptMethods.cpp`
- `Source/Scripting/ServerGlobalScriptMethods.cpp`
- `Source/Scripting/ServerItemScriptMethods.cpp`
- `Source/Scripting/ServerLocationScriptMethods.cpp`
- `Source/Scripting/ServerMapScriptMethods.cpp`
- `Source/Scripting/ServerPlayerScriptMethods.cpp`

The current set contains **932** `///@ ExportMethod` declarations across these files.

## Naming and ownership conventions

Exported functions are C++ functions with `FO_SCRIPT_API` and a generated-script name derived from the side/type prefix. Common patterns:

- `Common_Game_*` — side-neutral global utility exposed through the game/global object.
- `Common_ImGui_*` — side-neutral ImGui wrapper methods.
- `Server_Game_*`, `Server_Map_*`, `Server_Critter_*`, etc. — authoritative server methods.
- `Client_Game_*`, `Client_Map_*`, `Client_Critter_*`, etc. — client/view/frontend methods.
- `Mapper_Game_*` — mapper/editor automation methods.

The prefix is part of the ownership contract. Do not move a method to a more convenient file if that changes who owns the state it mutates.

## Common methods

### `Source/Scripting/CommonGlobalScriptMethods.cpp`

- Exported methods: 77
- Prefix: `Common_Game_*`
- Ownership: cross-side global helpers that do not require authoritative server-only state or client-only rendering state.
- Typical responsibilities:
  - logging and debugger break helpers;
  - quit/invoke helpers;
  - resource and config reads, plus the typed duration facade over complete baker-provided model animation metadata (`Game.GetModelAnimDuration`);
  - random, time, UTF-8, clipboard, open-link helpers;
  - geometry helpers such as distance, direction, line angle, intervals, trace line;
  - common serialization and formatting helpers.
- Tests to inspect: `Source/Tests/Test_CommonScriptMethods.cpp`, `Source/Tests/Test_ScriptBuiltins.cpp`.

### `Source/Scripting/CommonImGuiScriptMethods.cpp`

- Exported methods: 235
- Prefixes: `Common_Game_ImGui`, `Common_ImGui_*`
- Ownership: script-visible ImGui wrappers shared by tools/frontends that expose ImGui.
- Typical responsibilities:
  - window begin/end and style stack operations;
  - layout, text, widgets, popups, menus, tables, child windows;
  - value editors and controls;
  - draw-list or UI helper wrappers where implemented.
- Keep UI wrapper semantics here; runtime-specific UI policy belongs to client/mapper scripts or embedding-project scripts.

## Server methods

### `Source/Scripting/ServerGlobalScriptMethods.cpp`

- Exported methods: 122
- Prefix: `Server_Game_*`
- Ownership: authoritative game/server global operations.
- Typical responsibilities:
  - create, load, unload, and destroy critters/entities;
  - get/move/destroy items and item collections;
  - create locations/maps and query world entities;
  - call server-side global utility operations that require `ServerEngine` ownership.
- Related docs: [ServerRuntime.md](ServerRuntime.md), [EntityModel.md](EntityModel.md), [Persistence.md](Persistence.md).
- Tests to inspect: `Source/Tests/Test_ServerScriptMethods.cpp` and server runtime tests.

### `Source/Scripting/ServerEntityScriptMethods.cpp`

- Exported methods: 35
- Prefix: `Server_Entity_*`
- Ownership: server-side base entity operations.
- Typical responsibilities:
  - persistence toggles such as `IsPersistent` / `MakePersistent`;
  - entity time-event start/count/stop/repeat/data helpers.
- These operations are server-only because persistence and authoritative entity scheduling belong to the server runtime.

### `Source/Scripting/ServerCritterScriptMethods.cpp`

- Exported methods: 59
- Prefix: `Server_Critter_*`
- Ownership: authoritative critter operations.
- Typical responsibilities:
  - script setup and init callbacks;
  - movement state, movement UIDs, map/global transfer;
  - player/control relationship;
  - alive/knockout/dead state helpers;
  - visibility, direction, item inventory operations, and view refresh.
- Related docs: [ServerRuntime.md](ServerRuntime.md), [MapsMovementGeometry.md](MapsMovementGeometry.md).

### `Source/Scripting/ServerMapScriptMethods.cpp`

- Exported methods: 73
- Prefix: `Server_Map_*`
- Ownership: authoritative map operations.
- Typical responsibilities:
  - script setup and location lookup;
  - item creation/query by id, hex, radius, or collection;
  - static item lookup;
  - critter lookup by id, hex, radius, path, and visibility conditions;
  - map geometry, path, and movement-related queries.
- Keep state-changing world operations here rather than in common/client helpers.

### `Source/Scripting/ServerItemScriptMethods.cpp`

- Exported methods: 9
- Prefix: `Server_Item_*`
- Ownership: authoritative item operations.
- Typical responsibilities:
  - item script setup;
  - add/query inner items;
  - resolve item map position or owning critter;
  - refresh item visibility.

### `Source/Scripting/ServerLocationScriptMethods.cpp`

- Exported methods: 10
- Prefix: `Server_Location_*`
- Ownership: authoritative location operations.
- Typical responsibilities:
  - location script setup;
  - add/query maps by pid/index/id;
  - return map collections;
  - regenerate locations.

### `Source/Scripting/ServerPlayerScriptMethods.cpp`

- Exported methods: 11
- Prefix: `Server_Player_*`
- Ownership: connected player/session operations.
- Typical responsibilities:
  - host/port lookup;
  - disconnect and naming;
  - switch controlled critter;
  - query controlled critter;
  - view/reset/unload map operations.
- Related docs: [ServerRuntime.md](ServerRuntime.md) for player/connection flow.

## Client methods

### `Source/Scripting/ClientGlobalScriptMethods.cpp`

- Exported methods: 112
- Prefix: `Client_Game_*`
- Ownership: client-side global/runtime/frontend helpers.
- Typical responsibilities:
  - current map/location/player and mouse/gamepad/window state;
  - fullscreen/minimize/connection status;
  - distance helpers and visible entity queries;
  - atlas/resource/debug helpers;
  - resolution/minimap/render-facing helpers, including animation-wide `DrawRect` and stable logical `ViewRect` bounds of a `DrawCritter3d` instance;
  - effect selection and single/ranged script-value buffer writes;
  - sound, music, video, sprite, and UI-adjacent helpers where exposed.
- Related docs: [ClientRuntime.md](ClientRuntime.md), [FrontendAndRendering.md](FrontendAndRendering.md).

### `Source/Scripting/ClientEntityScriptMethods.cpp`

- Exported methods: 33
- Prefix: `Client_Entity_*`
- Ownership: client-side base entity time-event helpers.
- Typical responsibilities:
  - start/count/stop/repeat time events;
  - set time-event data.
- Mirrors part of the server entity utility surface, but acts on client-owned/view entities.

### `Source/Scripting/ClientCritterScriptMethods.cpp`

- Exported methods: 38
- Prefix: `Client_Critter_*`
- Ownership: client-side visible critter/view operations.
- Typical responsibilities:
  - display name, online/alive/movement/model/visibility state;
  - animation availability/playback/stop/refresh and per-`(state, action)` duration for the currently loaded model (`Critter.GetModelAnimDuration`);
  - inventory queries on visible client-side critters;
  - text position, particles, animation callbacks, bone positions;
  - local movement helpers.
- Do not add authoritative inventory or transfer policy here; that belongs to server methods.

### `Source/Scripting/ClientMapScriptMethods.cpp`

- Exported methods: 59
- Prefix: `Client_Map_*`
- Ownership: client-side map/view/rendering operations.
- Typical responsibilities:
  - draw map sprites and entity sprites;
  - rebuild fog and day colors;
  - screen/scroll state;
  - visible item/critter lookup by id, hex, radius, path, and collections;
  - path and line tracing queries;
  - coordinate conversion between map and screen.
- Related docs: [ClientRuntime.md](ClientRuntime.md), [MapsMovementGeometry.md](MapsMovementGeometry.md), [FrontendAndRendering.md](FrontendAndRendering.md).

### `Source/Scripting/ClientItemScriptMethods.cpp`

- Exported methods: 16
- Prefix: `Client_Item_*`
- Ownership: client-side visible item/view operations.
- Typical responsibilities:
  - visibility and clone helpers;
  - map position and movement state;
  - animation playback/time/direction;
  - inner item queries;
  - alpha/finish helpers.

### `Source/Scripting/ClientImGuiScriptMethods.cpp`

- Exported methods: 2
- Prefix: `Client_ImGui_*`
- Ownership: client-specific ImGui image/image-button helpers.
- Typical responsibilities:
  - image widgets backed by client/frontend texture or sprite resources.
- General ImGui wrappers live in `CommonImGuiScriptMethods.cpp`.

### `Source/Scripting/ClientLocationScriptMethods.cpp`

- Exported methods: 0
- Ownership: reserved/empty client-side location method group.
- Keep this file as a routing marker unless client location methods are actually added.

### `Source/Scripting/ClientPlayerScriptMethods.cpp`

- Exported methods: 0
- Ownership: reserved/empty client-side player method group.
- Player/session authority remains server-owned; add client methods here only for genuinely client-owned player view behavior.

## Mapper methods

### `Source/Scripting/MapperGlobalScriptMethods.cpp`

- Exported methods: 50
- Prefix: `Mapper_Game_*`
- Ownership: mapper/editor automation.
- Typical responsibilities:
  - create a blank map (`NewMap` / `NewMapFromText`, wrapping the internal `LoadMapFromText`);
  - add/delete/move/select entities;
  - set any per-instance entity property by name/text (`SetEntityProperty`, via the inspector apply path);
  - add tiles;
  - load/unload/save/show maps, plus a sandboxed save into a sub-directory (`SaveMapToPath`);
  - query loaded map files;
  - resize maps;
  - manage mapper tabs and tab pid filters.
- Related docs: [MapperTools.md](MapperTools.md).

## Adding or moving exported methods

Use this checklist before editing a `*ScriptMethods.cpp` file:

1. Identify the side that owns the state: common utility, server authority, client view/frontend, or mapper editor.
2. Identify the receiver family: global/game, entity, critter, map, item, location, player, ImGui, or another registered type.
3. Add `///@ ExportMethod` and `FO_SCRIPT_API` in the owning file. This annotation is the source of truth for all script backends: AngelScript registration and the WASM bridge both consume the generated metadata instead of separate backend-specific declarations. Use trailing C++ default parameters for optional suffix arguments instead of duplicating overloads whose bodies only supply fallback values. Codegen normalizes engine value-type defaults such as `isize32 {}` or `ucolor {}` into AngelScript defaults such as `isize()` or `ucolor()`, and preserves nested `ScriptFunc<...>` callback signatures so they can flow into the shared metadata and WASM callback-name ABI.
4. Spell a scalar pointer parameter/return `nptr<T>` only when it genuinely accepts or returns null; otherwise use the non-null `ptr<T>`. See [Nullability.md](Nullability.md).
5. Regenerate code so method descriptors and wrappers reflect the new signature.
6. Add or update the smallest relevant script method tests.
7. Update this page if a file is added/removed or a group meaning changes.

Property exports follow the same source-of-truth rule even though they are not counted in this method map: `///@ ExportProperty` enters the generated property registrars once, `PropertyRegistrar::RegisterProperty()` records the entry in the owning `EngineMetadata` script API inventory, AngelScript registers object accessors from those registrars, and the WASM `fonline.api` bridge derives property get/set imports from the same metadata instead of a backend-specific property list.

WASM entity method/property imports use the same receiver family as AngelScript, projected as a leading `ident`/`i64` handle for the runtime entity; runtime entity method arguments and returns are likewise projected as nullable-aware `ident`/`i64` handles. Proto and fixed-type arguments, returns, and properties use their proto/fixed id hash as a nullable-aware `hstring`/`i64` handle. Fixed-type property getters add a leading `hstring`/`i64` receiver id and fixed-type setters stay unsupported because AngelScript exposes fixed-type properties as read-only. Enum values, simple one-field value types, and compact packed value structs are scalar-compatible for WASM descriptors when their metadata layout can be represented as one `i32`, `i64`, `f32`, or `f64` value. Larger fixed-size value structs that fit the fixed object-value storage and contain only supported scalar/hash/value fields can be used as direct `fonline.api` method arguments, method returns, mutable method refs, and property values, and inside array, dictionary, and dict-of-array buffers.

WASM ref-type imports use the same metadata inventory as AngelScript for visible ref methods and dynamic ref layout fields. The receiver is a leading opaque `i64` pointer named by the ref type in the import suffix, for example `MovingContext_GetSpeed__MovingContext__uint16(ctx: i64) -> i32`. Ordinary ref receiver handles are borrowed. Refcounted `__AddRef` and `__Release` are also imported with the same receiver shape so a `PassOwnership` ref return can be balanced manually, for example `MovingContext___Release__MovingContext__void(ctx: i64) -> void`. Ref factories are imported without a receiver, for example `MovingContext___Factory__void__MovingContext() -> i64`, and return one owned reference that must later be balanced with `__Release`. Dynamic ref fields use property imports such as `RouteSnapshot_get_Note__RouteSnapshot_string` and `RouteSnapshot_set_Note__RouteSnapshot_string`. Non-mutable `RefType[]`, `dict<K,RefType>`, `dict<RefType,V>`, `dict<K,RefType[]>`, and `dict<RefType,V[]>` method imports carry borrowed `i64` handles in supported key/value positions. Mutable `RefType[]&`, `dict<K,RefType>&`, `dict<RefType,V>&`, `dict<K,RefType[]>&`, and `dict<RefType,V[]>&` method arguments copy borrowed handles back through the same collection buffer ABI; property ref keys stay unsupported because property raw storage, serialization, and AngelScript property conversion do not define a fixed borrowed-handle key representation.

Mutable scalar/value-type `&` arguments are raw `ptr,len` in/out buffers. Method/property `string` and `any` values are passed as read-only UTF-8 `ptr,len` input pairs; method returns and property getters append an output `ptr,len` pair and return the required byte length as `i32`. Mutable method `string&`/`any&` arguments use `ptr,byte_len,capacity_byte_len,required_byte_len_ptr`, report the required byte length, and copy back only when the modified UTF-8 bytes fit in capacity. Direct fixed-size object-shaped method/property values such as `irect` use raw layout bytes: arguments and setters are `ptr,byte_len`, returns and getters append output `ptr,byte_len` and return the required byte length as `i32`, and mutable refs use `ptr,byte_len` copy-back. Scalar-compatible and fixed-size object-shaped value-struct `T[]` method arguments, method returns, and property arrays use raw byte-buffer `ptr,byte_len` pairs, so `uint8[]` is the byte-buffer API path and `irect[]` is the larger value-struct buffer path; `string[]` and `any[]` use a counted UTF-8 blob over the same pointer pair. Property array getters append output `ptr,byte_len` and return the required byte length; property array setters validate the blob and write through the normal property setter path. Mutable `T[]&`/`string[]&`/`any[]&` method arguments use `ptr,byte_len,capacity_byte_len,required_byte_len_ptr`; if the modified array does not fit in capacity, the bridge reports the required byte length without partially overwriting the caller buffer. Scalar-compatible/fixed-size value-struct/`string`/`any` `dict<K,V>` method arguments, method returns, and property dictionaries use counted key/value blobs; method dictionary keys and values may also be borrowed ref handles where supported, while `PropertyRegistrar` rejects ref-type keys before any property descriptor is recorded. Too-small return/getter buffers are left untouched and the returned byte length tells the caller how much to allocate. Property dictionary setters validate the counted blob, strip the outer count because property raw dictionary storage is an entry stream, and write through the normal property setter path. Mutable `dict<K,V>&` arguments use `ptr,byte_len,capacity_byte_len,required_byte_len_ptr` with the same no-partial-copy rule; mutable method keys and values may use borrowed `u64` ref handles. Scalar-compatible/fixed-size value-struct/`string`/`any` `dict<K,V[]>` method arguments, returns, mutable arguments, and property dict-of-array get/set imports use the same outer dictionary pointer shape with nested counted array blobs; method `dict<K,RefType[]>` values and `dict<RefType,V[]>` keys use borrowed `u64` handles, and generated native wrappers still receive ordinary `map<K, vector<V>>` values for supported element families.

Callback arguments are passed as UTF-8 registered script function names or `__fonline_callback_N` delegate tokens and resolved through `ScriptSystem`; the named target can use non-scalar and nested callback metadata shapes when its backend supports them. Delegate tokens are call-scoped unless the WASM module calls `fonline.callback_retain(ptr,len)` before the export returns and later balances it with `fonline.callback_release(ptr,len)`. WASM-exported callback targets may use raw scalar signatures or metadata-suffixed signatures such as `Func__TestMode__TestMode`, `Func__ProtoItem__ProtoItem`, `Func__RefCounter__RefCounter`, `Func__string__int32`, `Func__void__string`, `Func__irect__int32`, `Func__void__irect`, `Func__irect_mut__void`, `Func__int32_array__int32`, `Func__void__uint8_array`, `Func__string_string_dict__int32`, `Func__void__string_string_dict`, `Func__string_int32_array_dict__int32`, `Func__string_bool_array_dict__int32`, `Func__string_ucolor_array_dict__int32`, `Func__string_irect_array_dict__int32`, `Func__void__string_irect_array_dict`, `Func__string_irect_array_dict_mut__void`, `Func__string_Rule_array_dict__int32`, `Func__void__string_Rule_array_dict`, `Func__RefCounter_array__int32`, `Func__void__RefCounter_array`, `Func__string_RefCounter_dict__int32`, `Func__void__string_RefCounter_array_dict`, `Func__int32_mut__void`, `Func__Critter_mut__void`, `Func__string_mut__void`, `Func__int32_array_mut__void`, `Func__string_string_dict_mut__void`, `Func__string_Rule_array_dict_mut__void`, `Func__callback_int32_int32_callback__int32`, and `Func__callback_void_callback_int32_int32_callback_callback__int32`. Export-side runtime entity/proto/fixed handles and simple borrowed ref-type handles are `i64`; ref-type export handles are passed through without retain/release. Export-side read-only `string`/`any`, direct fixed-size object-shaped values, non-mutable array, non-mutable dictionary, non-mutable dict-of-array including borrowed ref handles and fixed-size object-shaped value structs, mutable scalar/value-type and runtime entity/proto/fixed handle refs, and callback inputs are copied into temporary module memory as `ptr,len` pairs. Callback inputs carry the registered function name or temporary token as UTF-8 bytes; mutable `string&`/`any&`, mutable array, mutable dictionary, and mutable dict-of-array inputs use `ptr,byte_len,capacity_byte_len,required_byte_len_ptr`; mutable scalar/value-type/handle, mutable direct object-shaped value, mutable text, mutable array, mutable dictionary, and mutable dict-of-array inputs are copied back after the export returns when they fit the provided capacity, and matching text/direct value/array/dictionary/dict-of-array returns use a packed physical `i64` whose low 32 bits are a module pointer and high 32 bits are the byte length copied immediately by the engine. Callback return values, property ref keys, and mutable ref handles still require dedicated ABI work.

## Validation checklist

1. Recount `///@ ExportMethod` declarations after large API changes.
2. Run code generation and compile generated wrappers.
3. Run the relevant method tests:
   - common methods: `Source/Tests/Test_CommonScriptMethods.cpp`;
   - server methods: `Source/Tests/Test_ServerScriptMethods.cpp` plus relevant server runtime tests;
   - entity/handle behavior: `Source/Tests/Test_ScriptEntityOps.cpp`;
   - builtins/types: `Source/Tests/Test_ScriptBuiltins.cpp`.
4. Run nullable analyzers if any pointer signature changed.
5. Validate runtime behavior on the owning side; compilation alone does not prove that a method belongs on that side.
