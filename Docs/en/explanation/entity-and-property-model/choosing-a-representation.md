---
layout: default
title: Choose How to Represent Game Data
locale: en
document_id: choosing-a-data-representation
permalink: /Docs/en/explanation/entity-and-property-model/choosing-a-representation.html
---

# Choose How to Represent Game Data

The Engine offers several ways to give a game concept a shape: a startup setting, a FixedType catalog row, a prototype property or component, a RefType record, a ValueType layout, or a whole entity. They differ in who can change the data, whether it is saved, whether it reaches clients, whether it has an identity, and what a later change costs. This page is a decision guide for choosing one before the first declaration is written; the metadata reference owns the syntax.

## Source paths inspected

- `BuildTools/codegen.py`
- `BuildTools/tests/check_resource_usage_pipeline.py`
- `Source/Common/Settings.inc`
- `Source/Common/Geometry.h`
- `Source/Common/Properties.h`
- `Source/Tests/Test_MetadataBaker.cpp`
- `Source/Tools/ManagedScriptBaker.cpp`
- `Source/Scripting/AngelScript/AngelScriptEntity.cpp`
- `Examples/MinimalProject/Scripts/Starter.fos`
- `Examples/MinimalProject/FOnlineStarter.fomain`
- `Docs/en/explanation/entity-and-property-model/index.md`
- `Docs/en/explanation/persistence/index.md`
- `Docs/en/reference/metadata/index.md`
- `Docs/en/reference/settings/configuration-and-data-sources.md`
- `Docs/en/reference/script-api/settings.md`
- `Docs/en/how-to/content/prototype-format.md`
- `Docs/en/how-to/scripting/managed-csharp.md`
- `Docs/en/how-to/scripting/lifecycle-and-concurrency.md`

## The choices at a glance

| Need | Representation | Declared with |
| --- | --- | --- |
| A tunable scalar fixed for a server or client run | Setting | `///@ Setting <Side> <type> Group.Name` plus a value in the `.fomain` |
| A table of authored rows that scripts and other data refer to by ID | FixedType | `///@ FixedType <Side> Name`, `///@ Property` fields, rows in a prototype container file |
| A default or a capability of a kind of entity | Prototype property or property component | `///@ Property <Entity> ...`, optionally with `Component`, authored in `[Proto<Entity>]` |
| Per-entity state that changes and may be saved | Mutable property | `///@ Property <Entity> <Side> <type> Name Mutable [Persistent] [sync flag]` |
| A structured value that changes as a unit and has no identity | RefType held by a property | `///@ RefType <Side> Name` plus `///@ Property Name ...` fields |
| A small fixed-size value copied as bytes | ValueType | `///@ ValueType <Side> Name Layout = type - Field + ...` |
| A native twin of such a value | `ExportValueType` | `///@ ExportValueType Layout = ...` on a C++ struct |
| A thing with its own ID, lifetime, events or holder membership | Entity | `///@ Entity <Side> Name [flags]` plus `///@ EntityHolder` |

`<Side>` is `Common`, `Server` or `Client` depending on the declaration; the baker rejects a target that does not apply (for example `Mapper` on a RefType or on a property). Read the exact grammar in the [metadata reference](../../reference/metadata/index.md) and in the baker tests listed above before relying on a flag combination.

## Consequences of each choice

### Setting

- **Persistence:** none. The value is read from the authored config at startup, and `MetadataBaker` refuses a declaration whose value is missing from the config.
- **Replication:** none at runtime. Each side reads the value in its own baked profile; the metadata record carries only the name and type, and the engine never applies values from metadata.
- **Mutability:** built-in settings are read-only after the startup snapshot. Managed `Settings.Group.Name` properties are get-only; AngelScript custom settings keep their getters and setters, and `SetRuntimeSetting` still accepts project-declared settings. Treat every declared setting as configuration anyway; live state belongs on an entity, a property or the owning runtime service.
- **Identity:** the dotted `Group.Name` key. The baker rejects a declaration without a group. The config parser treats an unknown dotted name as a project custom setting, so a mistyped built-in key can look valid; cover every setting that matters with a focused test, as the [project configuration guide](../../how-to/build/project-configuration.md) advises.
- **Migration:** there is no migration rule kind for settings. A rename is a coordinated change of the declaration and every config that sets it.

### FixedType catalog row

- **Persistence:** none. Rows are baked into the pack's per-side prototype output; they are rebuilt by the next bake, not read from the database.
- **Replication:** none at runtime. The `<Side>` of the type and of each field decides which baked outputs contain it. A server-only field is skipped in client output.
- **Mutability:** authored constants. A mutable, non-persistent (temporary) property fails to load from prototype text, so rows carry only authored values.
- **Identity:** `$Name` (or the file name), unique per type, hashed to an `hstring`, free of `/`, `$` and whitespace. References to a row from other properties must name an existing row, or be empty on a nullable property. `$Parent` inheritance works within the same type.
- **Migration:** references resolve through `MigrationRule Proto ...` rules, so renaming or removing a row ID is a compatibility change, not a file move.
- **Access:** both backends generate `Game.Get<Name>(id)` and `Game.Check<Name>(id)` for each FixedType; the getter throws on an unknown ID, so probe with the check only when absence is legitimate.
- **Authoring file:** the container extension comes from `Baking.ProtoFileExtensions` (default `fopro`). The Engine's own resource-usage fixture sets `fopro foinfo` and authors rows in a `.foinfo` file. Engine bakers also write baked `.foinfo` outputs (sprite and model-animation indexes); those are not authored catalogs.

### Prototype property and property component

- **Persistence:** an immutable property lives in the prototype and is not saved per entity. A property saved with the entity must be `Mutable` and `Persistent`; the baker rejects `Persistent` without `Mutable`, on a client-only property, or on a virtual property.
- **Replication:** per property. A `Common` mutable property needs a sync type (`PublicSync` or `OwnerSync`, or `NoSync`); a client-modifiable property must be synced. Immutable `Common` values and `NoSync` values are not transferred in initial snapshots, so shared structure must come from the prototype.
- **Mutability:** immutable (authored), mutable and persisted, or mutable and temporary (runtime-only).
- **Identity:** the owning entity's. A component adds none: it groups qualified fields (`Bulletin.Region`) under a plain bool marker on the same owner, with no extra entity, holder or storage.
- **Migration:** a persisted name change needs `MigrationRule Property <Owner> Rename`; a deleted name needs `Remove`; moving flat fields into a component is a rename to the qualified name plus a save-compatibility test. See [Prototype Format](../../how-to/content/prototype-format.md#property-components).

### RefType

- **Persistence:** decided by the property that holds the value. The value is written as a whole, so removing a field takes effect on the next save of that value.
- **Replication:** decided by the holding property's sync flags. RefType fields themselves accept no flag except `Component`.
- **Mutability:** a read returns a detached copy (documented for Managed C#; AngelScript getters also copy the raw property data before conversion). Change a field by reading the value, editing the copy and assigning it back to the property.
- **Identity:** none. A RefType has no ID, no holder, no events and no lock. It needs at least one field, and its entity-typed fields may only be fixed or prototype references.
- **Migration:** field renames and removals use property migration rules, and old fields are skipped when a stored value is read.

### ValueType and `ExportValueType`

- **Persistence and replication:** as the property that uses the type; the value is copied as bytes.
- **Mutability:** a value, copied on assignment.
- **Identity:** none. Every field must be a primitive, enum, `hstring` or a single-field value type, offsets must be multiples of the field size, and the total must have no tail padding. Strings, collections, multi-field nested records and identity-bearing data do not qualify.
- **Migration:** the layout is part of the metadata contract and changes the metadata version, so server and clients must come from one bake. The Engine documents no in-place layout migration; plan a change as a new type or property plus a migration of the old one.
- **Native twin:** `ExportValueType` marks a C++ struct whose layout is mirrored in scripts; the native type must be trivially copyable with the same size. Use `///@ ValueType` for script-only data.

### Entity

- **Persistence:** through holder entries. A `Persistent` entry stores the children of persistent holders or `Game`; other entries are temporary.
- **Replication:** per holder entry (`NoSync`, `OwnerSync`, `PublicSync`); the baker requires a sync flag on a `Common` holder declaration and forbids one on a `Server` declaration.
- **Mutability:** a full lifecycle: create, destroy, events and time events.
- **Identity:** an entity ID, and on the server an entity lock that scripts must cover.
- **Migration:** there is no entity-level migration rule kind. Rules cover `Property`, `Proto` and `Enum`, so an entity's persisted data migrates through its property and prototype rules.

## Decide in this order

1. Does the data change while the game runs? If not, it is a setting (one scalar for the whole run), a prototype property (a default per kind of entity) or a FixedType row (a named record other data refers to).
2. Does it have its own identity, events, time events or lifetime, or must scripts look it up by ID and destroy it? Make it an entity. Otherwise keep it as properties of an existing entity.
3. Is it one value? Use a property with the right flags. Is it several values that change together and are never referenced individually? Use a RefType. Is it a few fixed-size numbers copied around? Use a ValueType.
4. Is it an optional role with a shared presence test and several related fields? Use a component with an immutable marker rather than parallel flat properties.
5. Write the persistence and sync flags last, and test a restart and a reconnect with the real declarations.

## Anti-patterns

| Anti-pattern | Why it fails | Use instead |
| --- | --- | --- |
| A setting used as a counter or a per-entity value | Settings are startup configuration; the Engine keeps live state on its owner | A mutable property on the entity, or a runtime service |
| A table of rows built into a setting list or a script-global dictionary | No IDs, no reference validation, no migration, separate from the bake | A FixedType catalog |
| A script-global dictionary keyed by entity ID | Separates data from lifetime; leaks across ID reuse; needs its own cleanup; becomes a shared concurrency boundary | A property on the entity |
| Flat `BulletinRegion`, `BulletinStep` properties plus presence inferred from a non-empty field | No single presence test; validators and consumers guess | A component with a bool marker |
| An entity created for a plain value | Adds ID, lifecycle, lock and holder bookkeeping | A property or RefType |
| A RefType for something looked up by ID, subscribed to, or destroyed | RefTypes have none of that | An entity or FixedType |
| A frequently changing large collection inside one RefType | The value is read as a copy and written as a whole | Smaller properties, or an entity per element if it needs identity |
| Strings or arrays inside a ValueType | The baker rejects them | A RefType |
| Editing a ValueType layout in place | Changes the byte layout and the metadata version for every side | A new type or property and a migration |
| Mutating a RefType from a property read | The read is a detached copy, so the change is lost | Read, edit, assign back |

## Minimal declarations

These lines use grammar that the baker tests and the example project exercise. The first block is the declaration side, the second is the matching config.

```angelscript
namespace RepresentationSample
{

///@ Setting Server bool Starter.ExitAfterStart

///@ FixedType Server LootTable
///@ Property LootTable Server int32 Rolls

///@ Property Critter Common bool Bulletin Component
///@ Property Critter Server hstring Bulletin.Region

///@ RefType Common RouteSnapshot
///@ Property RouteSnapshot Common int32[] Steps
///@ Property RouteSnapshot Common string Note
///@ Property Critter Server RouteSnapshot Snapshot Mutable Persistent

///@ ValueType Common ThresholdPair Layout = int32 - Low + int32 - High

}
```

```ini
Starter.ExitAfterStart = False

[LootTable]
$Name = Crate
Rolls = 3
```

The first `ini` line is the config value `MetadataBaker` requires for the setting, as `Examples/MinimalProject/FOnlineStarter.fomain` does; `[LootTable]` is a row in a prototype container file. A managed script reads the setting as `Settings.Starter.ExitAfterStart`:

```csharp
if (Settings.Starter.ExitAfterStart)
{
    Game.RequestQuit(true);
}
```

`Examples/MinimalProject/Scripts/Starter.fos` shows the same setting read from AngelScript. The Engine ships no managed starter example yet, so this C# fragment is a usage shape for the generated `Settings` class, not a project to build.

## Changing a representation later

Moving data between representations changes persisted names or layouts, so treat it as a migration:

1. List what is stored: property names, RefType fields, FixedType IDs, enum names and entity holder entries.
2. Add the matching `MigrationRule` for each renamed or removed name, and keep removed names as permanent tombstones. The [metadata reference](../../reference/metadata/index.md#dynamic-metadata) and the [Managed C# document migration section](../../how-to/scripting/managed-csharp.md#document-property-migrations) own the grammar.
3. Deploy server and client metadata from one bake; a metadata mismatch is a defect, not a supported mode ([metadata version](../../reference/metadata/index.md#metadata-version)).
4. Test with a save written by the previous declarations and with a restart, not only with new data.

## Validation checklist

- [ ] The chosen representation answers: mutable or not, saved or not, sent to clients or not, identity or not.
- [ ] Every declaration bakes without baker errors for server, client and mapper roles.
- [ ] Persistent data survives a server restart; synced data reaches a reconnecting client.
- [ ] Renamed or removed names have migration rules and a test against an old save.
- [ ] No script-global map holds per-entity state.
- [ ] Setting values exist in the config for every declared setting.
