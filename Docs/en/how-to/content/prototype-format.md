---
layout: default
title: Prototype Format
document_id: prototype-format-guide
locale: en
permalink: /Docs/en/how-to/content/prototype-format.html
---

# Prototype Format

FOnline prototypes are metadata-backed, named property sets baked separately for the server, client, and mapper. They define reusable entity defaults and project fixed definitions; they are not runtime save records, map placement records, or a game-specific content taxonomy.

Use this guide for the authoring model, inheritance, references, migrations, and validation workflow. Use the generated [syntax reference](../../reference/prototype-format/syntax.md), [built-in property catalog](../../reference/prototype-format/properties.md), [validation rules](../../reference/prototype-format/validation.md), and [canonical JSON model](../../../generated/prototype-format.json) for exact declarations at the current engine revision.

## Contract status

The prototype-format surface is `experimental` and revision-pinned. The engine owns:

- file selection through `Baking.ProtoFileExtensions`;
- section resolution, identity, inheritance, and side-specific binary output;
- metadata property lookup and strict text-value conversion;
- built-in `HasProtos` entity declarations and properties;
- engine migration lookup and prototype-reference validation.

An embedding project owns:

- additional prototype extensions and content directory layout;
- project entity declarations, `FixedType` declarations, properties, enums, and script callbacks;
- concrete IDs, field combinations, gameplay semantics, balance, and localization;
- migrations for project content and persisted project data;
- semantic validators, tests, release policy, and public examples.

The generated property catalog says whether the engine parser can load a key. It does not say that assigning the key in a prototype is meaningful or safe for a particular game system. Project documentation and tests must define those semantic constraints.

## Source paths inspected

- `BuildTools/PrototypeFormatInterface.json`
- `Source/Common/Settings.inc`
- `Source/Common/ConfigFile.cpp`
- `Source/Common/Properties.h`
- `Source/Common/Properties.cpp`
- `Source/Common/PropertiesSerializer.cpp`
- `Source/Common/EntityProperties.h`
- `Source/Common/EntityProtos.cpp`
- `Source/Common/ProtoManager.cpp`
- `Source/Common/ScriptSystem.h`
- `Source/Server/EntityManager.cpp`
- `Source/Tools/Baker.cpp`
- `Source/Tools/ProtoBaker.cpp`
- `Source/Tools/ProtoTextBaker.cpp`
- `Source/Scripting/ServerCritterScriptMethods.cpp`
- `Source/Scripting/ServerItemScriptMethods.cpp`
- `Source/Scripting/ServerLocationScriptMethods.cpp`
- `Source/Scripting/ServerMapScriptMethods.cpp`
- `Source/Tests/Test_ConfigFile.cpp`
- `Source/Tests/Test_Properties.cpp`
- `Source/Tests/Test_EntityProtos.cpp`
- `Source/Tests/Test_ProtoManager.cpp`
- `Source/Tests/Test_ProtoBaker.cpp`
- `Source/Tests/Test_ServerMapOperations.cpp`

## From source file to baked prototype

`ProtoBaker` receives the complete resource-pack file list and keeps files whose extension occurs in `Baking.ProtoFileExtensions`. The engine default is `fopro`; a project may add `fomap` for top-level map prototypes and other type-oriented authoring suffixes.

The extension does not choose the prototype type. Each section does:

- `[Proto<Type>]` resolves to entity metadata named `<Type>` and requires `HasProtos`;
- `[<FixedType>]` resolves to metadata declared with `///@ FixedType`;
- every top-level `[ProtoMap]` anchor in a map container resolves to `Map`.

Nested `[$Name/Critter]` / `[$Name/Item]` sections do not enter this stage. They belong to the map baker and the separate map-format contract. The legacy `[Header]` / `[Tiles]` / `[Objects]` layout is not an accepted authoring format.

The baker first collects every declaration and registers an empty prototype for every resolved type/ID. It finalizes registration before applying property text, so a property may refer to a prototype declared later or in another file of the same pack input. Source order is not a dependency mechanism.

Finally, the same source set is applied to server, client, and mapper metadata and emitted as `<pack>.fopro-bin-<side>`.

## Configuration syntax

Prototype text uses the shared configuration parser:

```ini
# A configuration comment
[ProtoItem]
$Name = BaseContainer
Stackable = false

[ProtoItem]
$Name = SecureContainer
$Parent = BaseContainer
NoBlock = false
```

Use `key = value` for replacement and `key += value` only where appending text is part of the property's documented representation. A final backslash continues a logical line only when the character before it is a space or tab; the parser trims both physical lines and joins them with one space. `#` starts a comment outside quoted/escaped content.

`ProtoBaker` interprets `$Name` and `$Parent`, and property application skips every `$`-prefixed key. `$Text ...` belongs to the separate `ProtoTextBaker` contract; do not assume that an arbitrary `$` key has meaning merely because property loading ignores it. Keys beginning with `_` are also ignored by property application and should be reserved for project tooling with an explicit project contract. Every other key must resolve to metadata.

## Identity

`$Name = <PrototypeId>` sets the ID. Without it, the source file name without its extension is used.

An ID must not contain `/` or `$`; both characters are reserved for nested-section addressing and are rejected before hashing and duplicate detection.

Identity is scoped by resolved type. `Item/Foo` and `Critter/Foo` can coexist, but two `Item/Foo` declarations anywhere in the same pack input fail. IDs are hashed and passed through `Proto` migration rules before duplicate detection.

Prefer one primary prototype per file and keep the file name equal to the ID. This preserves the useful default and makes source search, review, migration diffs, and generated examples predictable. Use explicit `$Name` whenever a file contains multiple sections or the file name is not the intended ID.

Directories and extensions are organization and discovery choices only. Never infer type or gameplay behavior from either one.

## Inheritance

`$Parent = ParentA ParentB` lists space-separated parents of the same resolved type. Parents may live in other files, but they must be present in the current pack input.

The baker merges:

1. ancestors depth first;
2. direct parents from left to right;
3. the child last.

Later values replace earlier values. Control keys beginning with `$` are not
copied as properties. Among direct parents, the rightmost parent wins where both
contribute the same key, and the child wins over all parents. An ancestor reached
through several paths contributes only at its first reach;
`Baking.AllowRepeatedProtoParents` (default `true`) skips later reaches, while
`false` rejects the inheritance diamond. `ProtoBaker` and `ProtoTextBaker` use
the same walk, so properties and `$Text` cannot diverge.

Keep inheritance shallow and capability-oriented. Prefer a small base that represents a stable authored concept over long visual or balance chains. Multiple inheritance is useful for orthogonal defaults, but overlapping parent fields make the result order-sensitive and should be made explicit in the child.

Parent graphs must be acyclic. Both prototype bakers track the active parent
path and reject self-cycles, two-node cycles, and longer cycles regardless of
the repeated-parent setting. Keep project-side structural validation for faster
author feedback, but baking is the authoritative rejection boundary.

## Property applicability

Property loading is side-aware:

The unknown-property check is unconditional and occurs before side
applicability can be considered. Side-specific skipping applies only after the
metadata property is known; it never turns an unknown property into a valid one.

- an unknown property fails;
- a property disabled on the current side fails, except that a client-only property is skipped in server output and a server-only property is skipped in client/mapper output;
- a virtual property fails;
- a temporary property fails;
- a valid property is parsed through its metadata type.

A property is temporary when it is mutable or core-owned and is not persistent. The generated [property catalog](../../reference/prototype-format/properties.md) applies this exact rule to current built-in metadata and lists the active sides.

Project metadata is not present in the engine-only catalog. A production embedding project should generate a companion catalog from its combined engine/project metadata and publish it in project documentation.

Parser applicability is only the first gate. A project should separately classify fields as:

- authored defaults intended for prototypes;
- runtime state that should be created or mutated by gameplay;
- structural links managed by another authoring tool;
- derived/cache state that must never be authored;
- project-required fields and valid field combinations.

## Text values and references

`PropertiesSerializer` is the value authority. It rejects malformed collections, integer overflow, invalid enum values, non-finite floating-point values, and unresolved references. Booleans accept their declared text form or numeric `0`/`1`; numeric and enum acceptance must not be inferred from loose configuration parsing.

`FixedType` and prototype-reference properties resolve through metadata and migration rules. A reference must name an existing target unless the property is nullable and the authored value is empty.

Do not document guessed array/dictionary punctuation for a project property. Read its generated type declaration and prove the concrete value with the baker or a focused parser test.

## Init scripts

The built-in `Item`, `Critter`, `Map`, and `Location` types expose a server-side, mutable, persistent `InitScript` property. A non-empty authored value names a global function:

```text
void Function(Item item, bool firstTime)
void Function(Critter critter, bool firstTime)
void Function(Map map, bool firstTime)
void Function(Location location, bool firstTime)
```

The property's `ScriptFuncType` metadata selects the exact signature. During server baking, `BaseBaker::ValidateProperties()` resolves every non-empty callback and rejects a missing function or mismatched signature. No callback attribute is required. Delegates are not valid persisted names.

`CallInit` marks the entity initialized and fires the corresponding `Game.On*Init` event before invoking `InitScript`. A callback that destroys the entity prevents the later steps. A function name that cannot be resolved at runtime is a hard `ScriptException`; the engine does not silently skip it. The initialized flag is already set, so this remains the documented entity-lifecycle `Basic` guarantee rather than a rollback. An exception thrown by the script body itself is reported by `ScriptFunc::Call` and converted to a `false` result instead of propagating from the script body.

`firstTime` is `true` for newly created entities and for the immediate call made by `SetupScript` / `SetupScriptEx`; restored world entities receive `false`. Those runtime methods invoke the callback first and persist its name only after a successful call. The typed overload rejects delegates, and both overloads throw when the function cannot be resolved or the call reports failure.

Initialization ordering is not a project dependency mechanism. A newly created location initializes its child maps before the location; world loading initializes each location first and then recursively initializes its maps, critters, and items. Author callbacks so they depend only on the entity and explicitly established relationships available at that boundary.

The callback starts with the initialized entity covered. It must acquire or widen synchronization before accessing unrelated entities. See [Script Lifecycle and Concurrency](../scripting/lifecycle-and-concurrency.md#entity-initscript-callbacks) for the runtime and concurrency contract.

## Migrations

Prototype IDs can appear in declarations, parent lists, property references, runtime lookups, and persisted entities. Renaming or removing an ID is therefore a compatibility change, not a file move.

Declare:

```cpp
///@ MigrationRule Proto Item OldContainer NewContainer
///@ MigrationRule Proto Item RemovedContainer __remove__
```

The owning project decides where project metadata declarations live and how long rules are retained. A rename target must exist at the receiving revision. A removal is valid only when loading policy can safely discard the reference or entity; otherwise migrate to a compatible replacement.

When changing a property name or type, follow the property/persistence migration policy of the owning metadata declaration. Prototype migration does not repair an incompatible property payload.

## Authoring practices

1. Start from the generated section and property references for the pinned engine revision.
2. Add project metadata and content rules in project-owned documentation instead of copying engine internals.
3. Keep IDs descriptive and stable; do not encode temporary folder structure, balance numbers, or release names into them.
4. Keep parent chains shallow, avoid overlapping multiple parents, and run a cycle validator.
5. Author only semantic defaults. Let runtime systems own transient position, ownership, cache, and relationship state unless the project explicitly documents otherwise.
6. Treat side-only behavior deliberately. A key being skipped from one output is not proof that the other side can function without a corresponding project contract.
7. Use prototype references instead of duplicated free-form IDs where metadata supports them, and validate every referenced target.
8. Add migrations in the same change as an ID/property compatibility change.
9. Run the real bake and the consuming subsystem's semantic tests; successful parsing alone does not prove usable content.
10. Keep each project extension focused on one content family for review and tooling, while remembering that the section and metadata, not the extension or directory, select the type.

## Validation workflow

For an engine change:

```bash
python BuildTools/tests/test_docs_prototype_format.py
python BuildTools/docs_prototype_format.py --check
python BuildTools/docs_contract_diff.py --baseline-git-ref origin/master --allow-missing-baseline --write --enforce
```

Also run the focused native tests for the changed parser, metadata, property, or baker boundary.

For a project content change:

1. regenerate any project-owned metadata/property reference;
2. run the project's normal resource bake;
3. run structural validators for duplicate IDs, inheritance cycles, references, migrations, and required fields;
4. run focused tests for the consuming gameplay/editor system;
5. inspect every side that consumes a side-specific property.

## Updating an engine revision

When an embedding project advances its Engine revision:

1. diff the old/new [canonical prototype-format models](../../../generated/prototype-format.json);
2. inspect `ProtoBaker`, `ConfigFile`, property serialization, metadata declarations, settings, and tests changed in the revision range;
3. regenerate the engine and project references;
4. update project format/semantic docs for added, removed, renamed, retyped, or side-changed properties;
5. add migrations and release notes for compatibility changes;
6. rebake all resource packs and run focused runtime/editor tests;
7. do not reuse baked prototype binaries from the previous revision.

## See also

- [Baking Pipeline](../../explanation/content-pipeline/baking.md) - baker orchestration and output ownership.
- [Entity Model](../../explanation/entity-and-property-model/) - entity metadata and runtime identity.
- [Script Lifecycle and Concurrency](../scripting/lifecycle-and-concurrency.md) - callback invocation, synchronization, failure, and teardown rules.
- [GeneratedApiAndMetadata.md](../../reference/metadata/index.md) - source metadata and generated API models.
- [Generated Contract Change Management](../../contributing/contract-change-management.md) - aggregate contract diff and dispositions.
- [Embedding Project](../build/embedding-project.md) - reusable engine/project ownership boundary.
