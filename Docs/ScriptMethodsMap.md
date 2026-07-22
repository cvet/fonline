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
3. Add `///@ ExportMethod` and `FO_SCRIPT_API` in the owning file. Use trailing C++ default parameters for optional suffix arguments instead of duplicating overloads whose bodies only supply fallback values. Codegen normalizes engine value-type defaults such as `isize32 {}` or `ucolor {}` into AngelScript defaults such as `isize()` or `ucolor()`.
4. Spell a scalar pointer parameter/return `nptr<T>` only when it genuinely accepts or returns null; otherwise use the non-null `ptr<T>`. See [Nullability.md](Nullability.md).
5. Regenerate code so method descriptors and wrappers reflect the new signature.
6. Add or update the smallest relevant script method tests.
7. Update this page if a file is added/removed or a group meaning changes.

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
