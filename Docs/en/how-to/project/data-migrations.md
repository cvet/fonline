---
layout: default
title: Migrate Saved Data and Content in a Game Project
locale: en
document_id: project-data-migrations
permalink: /Docs/en/how-to/project/data-migrations.html
---

# Migrate Saved Data and Content in a Game Project

A live game keeps data written by older builds. This guide explains which names and values count as persisted identities, how to declare `///@ MigrationRule` entries when one of them changes, how to organize the rules and migrator functions in a project for both script backends, how to prove a migration on an isolated copy of production-shaped data, and how Engine updates and project migrations interleave.

The Engine implements the rule grammar and the loading hooks. A game owns the decisions: which saves it supports, how long, and what a transformation means for its players.

## Source paths inspected

- `Source/Tools/MetadataBaker.cpp`
- `Source/Common/EngineBase.cpp`
- `Source/Common/Properties.cpp`
- `Source/Common/PropertiesSerializer.cpp`
- `Source/Common/ProtoManager.cpp`
- `Source/Common/Common.h`
- `Source/Common/EntityProperties.h`
- `Source/Scripting/Managed/CoreScripts/DatabaseDocument.cs`
- `Source/Scripting/Managed/CoreScripts/Native.DatabaseMigrations.cs`
- `Source/Scripting/AngelScript/AngelScriptDocumentMigrations.cpp`
- `Source/Tests/Test_MetadataBaker.cpp`
- `Source/Tests/Test_AngelScriptDocumentMigrations.cpp`
- `BuildTools/codegen.py`
- `AGENTS.md`
- `Docs/en/explanation/persistence/index.md`
- `Docs/en/reference/script-api/migrations.md`
- `Docs/en/reference/metadata/index.md`
- `Docs/en/how-to/scripting/style-and-refactoring.md`
- `Docs/en/how-to/scripting/managed-csharp.md`
- `Docs/en/how-to/content/prototype-format.md`
- `Docs/en/how-to/release/versioning.md`
- `Docs/en/how-to/release/backup-and-recovery.md`
- `Docs/en/how-to/migration/engine-upgrade.md`
- `Docs/en/explanation/runtime/client-updater.md`
- `Docs/en/reference/changelog.md`

## Know what is a persisted identity

An identity is any name or value that stored data, authored content or a deployed client can still refer to after the code that defined it has changed.

| Identity | Where it persists | Rule support |
|---|---|---|
| Property name, including a component-qualified name such as `Bulletin.Region` | Keys of persistent entity documents | `Property` rules: `Rename`, `Remove`, `Transform` |
| Property type or layout | Stored values under that key | None; treat as a replacement (below) |
| Prototype id | `_Proto` of stored entities, content references, parent lists | `Proto` rules: `Rename`, `Remove`, `Transform` |
| Enum value name | Stored by name in scalar enum properties and enum dictionary keys | `Enum` rule |
| Id or text key copied into a persistent `string` or `hstring` value | Document values | `Property Transform` function |
| Entity type name, custom collection name (`DataBase.CustomCollections`) | Collection names in the database | None; a project-owned data conversion |
| Remote-call, event and setting names | Not persisted, but part of the baked metadata that clients must match | Rebake; see [Metadata version](../../reference/metadata/index.md#metadata-version) |

The runtime consults exactly three rule kinds when loading: `Property`, `Proto` and `Enum` (the lookups are in `Properties.cpp`, `ProtoManager.cpp` and `PropertiesSerializer.cpp`). A fourth kind, `Version`, is the Engine's own compatibility marker. Anything outside the table needs a project conversion that is tested like a migration but is not expressed as a rule.

## Declare a rule for every rename, removal and move

Renaming a property, renaming or removing a prototype id, renaming or removing an enum value, or moving a persisted field (for example from a flat `BulletinRegion` into a component field `Bulletin.Region`) is a compatibility change, not a refactor. Land the rule in the same change as the rename. Without one the old value either fails to resolve at load or is silently ignored.

Verified syntax, one rule per line, each tag parsed by `MetadataBaker` from any script source in the metadata pack:

```text
///@ MigrationRule Property <Owner> Rename <OldName> <NewName>
///@ MigrationRule Property <Owner> Remove <OldName>
///@ MigrationRule Property <Owner> Transform <Property> <Function>
///@ MigrationRule Proto <Type> Rename <OldId> <NewId>
///@ MigrationRule Proto <Type> Remove <OldId>
///@ MigrationRule Proto <Type> Transform <OldId> <Function>
///@ MigrationRule Enum <EnumName> <OldValueName> <NewValueName>
```

Constraints enforced by the Engine:

- `Rename` and `Transform` rules have five parts after the tag word; `Remove` has four, and an extra argument is rejected. Dotted names are allowed (`Weapon.AmmoPid`). A legacy prototype id containing whitespace must be double-quoted, as in `Proto Modifier Rename "Local knowledge" LocalKnowledge`; replacements cannot contain whitespace.
- A property `Rename` needs the destination to exist. A property `Transform` needs an existing `Persistent` property. A rule whose source name is still a live declared property on that owner is rejected at bake time, which prevents retiring a name that is still in use.
- `Property Remove` is a permanent name tombstone: baking rejects a property still declared under the removed name, and loading skips the old value. Obsolete top-level keys may remain in the database.
- One source has one action. Rename chains resolve transitively with cycle detection.
- `Transform` functions are synchronous and are bound at startup; an unbound function refuses document migration instead of skipping it. They run while the stored document is loaded and edit a detached value before the document is decoded into the entity's properties and before any event or script sees the restored entity; every callback for a document reads the same original document. Return `true` to store the result, `false` to keep the original encoded value exactly.
- Rules for a server-only owner are omitted from the other roles' metadata.
- The `Enum` form remaps by name when a stored enum value no longer resolves; without a rule an unknown name throws `EnumResolveException`.

The generated [migration rule reference](../../reference/script-api/migrations.md) lists the rules the Engine itself ships; the grammar owners are the [metadata reference](../../reference/metadata/index.md), [prototype format](../content/prototype-format.md#migrations) and, for callbacks, [Managed C#](../scripting/managed-csharp.md#document-property-migrations) and [AngelScript](../scripting/style-and-refactoring.md#document-migrations).

## Organize rules in one central module

The rule tags are position independent: `MetadataBaker` reads them from every script file that its pack includes, and the Engine's own tests bake `.cs` and `.fos` sources together. That frees a project to choose a layout; the following one keeps review simple.

- One central migration file is the index. It holds the flat, mechanical rules (renames, removals, enum remaps) in domain order and explains the conventions in its header comment.
- One part per domain (inventory, quests, world state) holds that domain's `Transform` rules next to the migrator functions they name, so a change to a domain's data shape and its conversion are reviewed together.
- Rules are append-only. A rule is deleted only through the retention policy below.

AngelScript: one file and namespace per part, following the file-stem namespace convention in [style and refactoring](../scripting/style-and-refactoring.md). Qualify function names as `Namespace::Function`.

```angelscript
// Migration.fos
namespace Migration
{
    ///@ MigrationRule Property Critter Rename OldStep Step
    ///@ MigrationRule Property Critter Remove ObsoleteFlag
    ///@ MigrationRule Proto Item Rename OldContainer NewContainer
    ///@ MigrationRule Enum QuestState Aborted Failed
}
```

```angelscript
// MigrationQuests.fos
namespace MigrationQuests
{
    ///@ Property Critter Common int32 QuestStage Mutable Persistent PublicSync
    ///@ MigrationRule Property Critter Transform QuestStage MigrationQuests::RewriteQuestStage

    // The legacy build stored -1 for "not started"; the current build stores 0
    [[PropertyMigrator]]
    bool RewriteQuestStage(int&inout value, const DatabaseDocument&inout document)
    {
        if (value != -1) {
            return false;
        }

        value = 0;

        return true;
    }
}
```

Managed C#: one `static partial class` split over top-level files in a script directory (`ManagedScript.Dirs` scans top-level `.cs` files). The binder searches the entry assembly for a unique static `[PropertyMigrator]` method with the exact signature `bool Name(ref T value, DatabaseDocument document)`, where `T` is the generated property type.

```csharp
// Migration.Quests.cs
namespace MyGame;

using FOnline;

///@ Property Critter Common int32 QuestStage Mutable Persistent PublicSync
///@ MigrationRule Property Critter Transform QuestStage GameMigrations.RewriteQuestStage

public static partial class GameMigrations
{
    // The legacy build stored -1 for "not started"; the current build stores 0
    [PropertyMigrator]
    public static bool RewriteQuestStage(ref int value, DatabaseDocument document)
    {
        if (value != -1) {
            return false;
        }

        value = 0;

        return true;
    }
}
```

A project uses one gameplay backend; the two forms above are alternatives, not a pair. The signature, including the unused document parameter, is fixed by the binder.

Write each migrator so that it is idempotent by inspecting the value it converts: a converted value no longer has the legacy shape, so a second load returns `false` and keeps it. Do not depend on a marker field set by the migrator. Only the migrated property's value is committed, and the document is read-only context.

## Keep gameplay code current-only

After a rename, game scripts, content and settings use only the new name. The old name lives in exactly two places: the rule and, if present, the migrator. Do not add runtime branches such as "if the old key exists, read it instead"; the loader has already normalized the document before scripts or load events observe it. Following the Engine's own contract (see [versioning](../release/versioning.md#current-api-and-database-migrations)), an unmigrated consumer should fail explicitly at compile, bake or validation time, not drift.

Search the project for the old identifier in scripts, content, `.fomain` files and tests before declaring the change complete, and update the authored references; a rule does not repair content that still names the retired id.

## Never reuse a retired identity

A retired property name, prototype id or enum value must not be given a new meaning, because stored data and old content would reinterpret it silently. The Engine enforces this only for properties (a live declaration under a removed or renamed-away name is rejected). For every other identity it is a project discipline, and it must outlive the rule: when a rename rule is eventually deleted, the retired name still stays reserved. The Engine's migration guard, `BuildTools/migration_guard.py`, implements that ledger: it compares the prototype ids, property names and enum values a project declares with a committed manifest, requires a rule for every identity that left, and fails on a reintroduced retired name, so reuse fails in review and in CI rather than in production ([Guard Identity Migrations](migration-guard.md)). Identities it does not see, such as catalog ids stored inside property values or text keys copied into saves, need project registries beside it.

A property type change is a replacement, not a rename. Give the new meaning a distinct name, retire the old one with `Remove`, and prove the value carry-over on isolated data. Prototype migration does not repair an incompatible property payload.

## Test a migration

1. Obtain production-shaped data without touching production. Restore a verified backup into an isolated namespace, following [Restore safely](../release/backup-and-recovery.md#restore-safely), and keep an unmodified baseline copy before the first migrating start.
2. Point one headless server at the copy using a dedicated sub-config: networking disabled and `Server.DbStorage` set to `JSON <directory>` or `DbSQLite <directory>` for the copy. Never reuse a connection string that names real data.
3. Start the candidate build. Startup loads persistent entities before `Start server complete!`; treat any startup exception as a failed migration. The [persistence guide](../../explanation/persistence/index.md) describes how entity state reaches documents.
4. Run a semantic probe: counts and critical fields of representative entities, a migrated and an unmigrated value of the same property, an enum value that was renamed, a retired prototype. Assert on loaded values, not on raw stored keys, because obsolete keys may remain.
5. Stop gracefully, start again on the same directory and repeat the probe. Persistence is proved by the restart, and a second start must change nothing.
6. Test sparse and old shapes deliberately: documents with the property missing, with only the old key, with both keys (the existing canonical key wins), with an empty or default value, and with a retired prototype id.
7. For a Managed `Transform`, also write fast fixtures with `new DatabaseDocument(entityType, originalValues)` and `Migrate<T>(property, ref value)` or `MigrateProto(ref value)`; they run the registered callback without creating an entity and without committing, so they complement but do not replace the restart test.

Run the test lane described in [Add and Run Tests in a Game Project](project-tests.md): the migration check is a script or process test with markers and an exit code, and it must not run while a bake of the same output is in progress.

## Decide retention and removal

- Keep a rename or enum rule for the supported save horizon: the oldest build whose saves, backups or restores the game still promises to load. The Engine's upgrade guide says the same ([protect persisted state](../migration/engine-upgrade.md#protect-persisted-state)).
- Keep `Property Remove` rules permanently as tombstones.
- Delete a rename or enum rule only when the horizon has passed and a scan of every supported copy (live data, retained backups, restore drills) shows no document still carrying the old name. Record the retired name in the identity ledger in the same change.
- Record the horizon and the removal decision in project documentation. The Engine does not choose retention, schedules or recovery objectives.

## Interleave Engine updates and project migrations

Engine changes can rename or reshape Engine-owned properties and ship their own rules in Engine source (for example the `Property Item Rename ScriptId InitScript` family in `EntityProperties.h`). Project rules cover project-declared names only. When you move the Engine pin:

1. Read each changelog entry from the old to the new version, oldest first. Every entry has a `### Migration` section; execute it completely before the next ([apply explicit migration notes](../migration/engine-upgrade.md#apply-explicit-migration-notes)). An entry that requires a stored-property or reference conversion names the exact `MigrationRule` operation and the isolated-data proof.
2. Put any project-side conversion that an entry demands into the project migration module, in a separate change from unrelated game rules, so a failed load is attributable to one cause.
3. Run the migration test above with the Engine update and the project rules together on a clone, then again with the Engine update alone.
4. Rebake metadata, configuration and scripts together so servers and clients come from one bake.

## Know who changes which compatibility marker

Only what the Engine documents is stated here.

- `FO_COMPATIBILITY_VERSION` is a runtime contract digest that includes the separate manual migration marker `///@ MigrationRule Version` in `Source/Common/Common.h` ([versioning identities](../release/versioning.md#identities-and-consumers)). Codegen hashes every `MigrationRule` tag it parses from native sources into that digest. The Engine's own rule is that a serialized change updates `MigrationRule` entries, while only a network or runtime compatibility change raises the central marker (`AGENTS.md`); a data migration by itself is not such a change. Engine maintainers raise the Engine marker. The Engine documents no project-owned manual marker.
- The baked metadata version is computed from every parsed tag, including migration rules, properties, enums and settings. It is not configured by hand, and changing any tag changes it. A server and every client must run one bake; the updater and handshake reject a mismatch ([Metadata version](../../reference/metadata/index.md#metadata-version), [handshake](../../explanation/runtime/client-updater.md#handshake)). A rule declared in project scripts therefore changes the metadata version, not the binary compatibility digest.
- `Common.GameVersion` and the project version are project-owned; see [Version and Release a Game Project](versioning-and-releases.md).

Deploy a data migration as a unit: matching server package, baked resources, configuration and a verified backup, with the rollback point decided in advance. A migration that is one-way needs a stated rule for refusing old binaries, as in [Backup and Recovery](../release/backup-and-recovery.md).

## Validation checklist

- Every rename, removal or move of a persisted identity has a rule in the same change.
- The rule grammar bakes without errors for every role, and the destination or target exists.
- Game code, content and settings contain only current names.
- The retired identity is recorded and cannot be reintroduced: `migration_guard.py --check` passes and the manifest diff is in the same change (see the [migration guard checklist](migration-guard.md#validation-checklist)).
- The migration ran on an isolated copy, survived a restart unchanged, and passed sparse and old-shape fixtures.
- The supported save horizon and the removal decision are written down.
- Engine update notes were applied oldest first, and metadata, configuration and scripts were rebaked together.
