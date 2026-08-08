---
layout: default
title: Script Methods Map
locale: en
document_id: script-methods-map
permalink: /Docs/en/reference/script-api/method-ownership.html
---

# Script Methods Map

> Engine-owned documentation. This page maps native `///@ ExportMethod` files in `Source/Scripting/` to their script-facing responsibilities. It complements [Scripting](../../explanation/scripting-runtime/); it is not a full generated API reference.

## Purpose

Use this page when adding, moving, or reviewing script-visible native methods. The goal is to keep method ownership obvious before editing codegen inputs:

- choose the correct runtime side (`Common`, `Server`, `Client`, or `Mapper`);
- choose the correct receiver/entity family (`Game`, `Entity`, `Critter`, `Map`, `Item`, `Location`, `Player`, `ImGui`);
- preserve authoritative server boundaries and client/view-only boundaries;
- update tests and docs when exported method groups move.

## Source inventory

The authoritative file list and per-file `///@ ExportMethod` declaration counts are generated from `Source/Scripting/*ScriptMethods.cpp` into [source-inventory.json](../../../generated/source-inventory.json). Full parsed method records live in the [canonical API model](../../../generated/api.json), and the [generated methods reference](../../../generated/api/methods.md) renders their overload IDs, signatures, defaults, nullability, runtime sides, effective receivers, stability, and source locations. This page owns the human explanation of method families and side/receiver boundaries; it does not duplicate generated totals or signatures.

Regenerate the inventory after adding, removing, or moving an export:

```bash
python BuildTools/docs_api.py --write
python BuildTools/docs_api.py --check
python BuildTools/docs_reference.py --write
python BuildTools/docs_reference.py --check
python BuildTools/docs_inventory.py --write
python BuildTools/docs_inventory.py --check
```

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
- Model animation tuple, alias, bake, common lookup, and client-instance boundaries: [Model Animation](../../how-to/content/model-animation.md).

### `Source/Scripting/CommonImGuiScriptMethods.cpp`

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

- Prefix: `Server_Game_*`
- Ownership: authoritative game/server global operations.
- Typical responsibilities:
  - create, load, unload, and destroy critters/entities;
  - get/move/destroy items and item collections;
  - create locations/maps and query world entities;
  - call server-side global utility operations that require `ServerEngine` ownership.
- Related docs: [Server Runtime](../../explanation/runtime/server.md), [Entity Model](../../explanation/entity-and-property-model/), [Persistence](../../explanation/persistence/).
- Tests to inspect: `Source/Tests/Test_ServerScriptMethods.cpp` and server runtime tests.

### `Source/Scripting/ServerEntityScriptMethods.cpp`

- Prefix: `Server_Entity_*`
- Ownership: server-side base entity operations.
- Typical responsibilities:
  - persistence toggles such as `IsPersistent` / `MakePersistent`;
  - entity time-event start/count/stop/repeat/data helpers.
- These operations are server-only because persistence and authoritative entity scheduling belong to the server runtime.

### `Source/Scripting/ServerCritterScriptMethods.cpp`

- Prefix: `Server_Critter_*`
- Ownership: authoritative critter operations.
- Typical responsibilities:
  - script setup and init callbacks;
  - movement state, movement UIDs, map/global transfer;
  - player/control relationship;
  - alive/knockout/dead state helpers;
  - visibility, direction, item inventory operations, and view refresh.
- Related docs: [Server Runtime](../../explanation/runtime/server.md), [Maps and Movement](../../explanation/maps-and-movement.md).

### `Source/Scripting/ServerMapScriptMethods.cpp`

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

- Prefix: `Server_Item_*`
- Ownership: authoritative item operations.
- Typical responsibilities:
  - item script setup;
  - add/query inner items;
  - resolve item map position or owning critter;
  - refresh item visibility.

### `Source/Scripting/ServerLocationScriptMethods.cpp`

- Prefix: `Server_Location_*`
- Ownership: authoritative location operations.
- Typical responsibilities:
  - location script setup;
  - add/query maps by pid/index/id;
  - return map collections;
  - regenerate locations.

### `Source/Scripting/ServerPlayerScriptMethods.cpp`

- Prefix: `Server_Player_*`
- Ownership: connected player/session operations.
- Typical responsibilities:
  - host/port lookup;
  - disconnect and naming;
  - switch controlled critter;
  - query controlled critter;
  - view/reset/unload map operations.
- Related docs: [Server Runtime](../../explanation/runtime/server.md) for player/connection flow.

## Client methods

### `Source/Scripting/ClientGlobalScriptMethods.cpp`

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
- Related docs: [Client Runtime](../../explanation/runtime/client.md), [Frontend and Rendering](../../explanation/rendering/).

### `Source/Scripting/ClientEntityScriptMethods.cpp`

- Prefix: `Client_Entity_*`
- Ownership: client-side base entity time-event helpers.
- Typical responsibilities:
  - start/count/stop/repeat time events;
  - set time-event data.
- Mirrors part of the server entity utility surface, but acts on client-owned/view entities.

### `Source/Scripting/ClientCritterScriptMethods.cpp`

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

- Prefix: `Client_Map_*`
- Ownership: client-side map/view/rendering operations.
- Typical responsibilities:
  - draw map sprites and entity sprites;
  - rebuild fog and day colors;
  - screen/scroll state;
  - visible item/critter lookup by id, hex, radius, path, and collections;
  - path and line tracing queries;
  - coordinate conversion between map and screen.
- Related docs: [Client Runtime](../../explanation/runtime/client.md), [Maps and Movement](../../explanation/maps-and-movement.md), [Frontend and Rendering](../../explanation/rendering/).

### `Source/Scripting/ClientItemScriptMethods.cpp`

- Prefix: `Client_Item_*`
- Ownership: client-side visible item/view operations.
- Typical responsibilities:
  - visibility and clone helpers;
  - map position and movement state;
  - animation playback/time/direction;
  - inner item queries;
  - alpha/finish helpers.

### `Source/Scripting/ClientImGuiScriptMethods.cpp`

- Prefix: `Client_ImGui_*`
- Ownership: client-specific ImGui image/image-button helpers.
- Typical responsibilities:
  - image widgets backed by client/frontend texture or sprite resources.
- General ImGui wrappers live in `CommonImGuiScriptMethods.cpp`.

### `Source/Scripting/ClientLocationScriptMethods.cpp`

- Ownership: reserved/empty client-side location method group.
- Keep this file as a routing marker unless client location methods are actually added.

### `Source/Scripting/ClientPlayerScriptMethods.cpp`

- Ownership: reserved/empty client-side player method group.
- Player/session authority remains server-owned; add client methods here only for genuinely client-owned player view behavior.

## Mapper methods

### `Source/Scripting/MapperGlobalScriptMethods.cpp`

- Prefix: `Mapper_Game_*`
- Ownership: mapper/editor automation.
- Typical responsibilities:
  - create a blank map (`NewMap` / `NewMapFromText`, wrapping the internal `LoadMapFromText`);
  - add/delete/move/select entities;
  - set any per-instance entity property by name/text (`SetEntityProperty`, via the inspector apply path);
  - add tiles;
  - load/unload/save/show maps, plus a sandboxed save into a sub-directory (`SaveMapToPath`);
  - save the map-only render target synchronously with `SaveMapperScreenshot`, or request a deferred full-window capture including application ImGui composition with `RequestMapperWindowScreenshot`;
  - query loaded map files;
  - resize maps;
  - manage mapper tabs and tab pid filters.
- Related docs: [Mapper Tools](../../how-to/tools/mapper.md).

## Adding or moving exported methods

Use this checklist before editing a `*ScriptMethods.cpp` file:

1. Identify the side that owns the state: common utility, server authority, client view/frontend, or mapper editor.
2. Identify the receiver family: global/game, entity, critter, map, item, location, player, ImGui, or another registered type.
3. Add `///@ ExportMethod` and `FO_SCRIPT_API` in the owning file. Use trailing C++ default parameters for optional suffix arguments instead of duplicating overloads whose bodies only supply fallback values. Codegen normalizes engine value-type defaults such as `isize32 {}` or `ucolor {}` into AngelScript defaults such as `isize()` or `ucolor()`.
4. Spell a scalar pointer parameter/return `nptr<T>` only when it genuinely accepts or returns null; otherwise use the non-null `ptr<T>`. See [Nullability.md](../../../Nullability.md).
5. Regenerate code so method descriptors and wrappers reflect the new signature.
6. Add or update the smallest relevant script method tests.
7. Review or add `///@ ApiContract` only when the method's support status has an owner-backed disposition; reachability is not a stability promise.
8. Regenerate `Docs/generated/api.json`, generated reference pages, and `Docs/generated/source-inventory.json`; update this page only if a file or responsibility group changes meaning.

## Validation checklist

1. Run `python BuildTools/tests/test_docs_api.py`, `python BuildTools/docs_api.py --write`, `python BuildTools/docs_reference.py --write`, and then both `--check` commands after API or contract changes.
2. Run `python BuildTools/docs_inventory.py --write` and then `--check` after export-file changes.
3. Run code generation and compile generated wrappers.
4. Run the relevant method tests:
   - common methods: `Source/Tests/Test_CommonScriptMethods.cpp`;
   - server methods: `Source/Tests/Test_ServerScriptMethods.cpp` plus relevant server runtime tests;
   - entity/handle behavior: `Source/Tests/Test_ScriptEntityOps.cpp`;
   - builtins/types: `Source/Tests/Test_ScriptBuiltins.cpp`.
5. Run nullable analyzers if any pointer signature changed.
6. Validate runtime behavior on the owning side; compilation alone does not prove that a method belongs on that side.
