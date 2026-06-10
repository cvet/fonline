# Entity Model

This document explains the reusable runtime entity model: entity type descriptors, generated property accessors, prototype entities, inner-entity ownership, entity events, and the property storage model that other runtime systems build on.

Use it when changing `Source/Common/Entity.*`, `EntityProperties.*`, `EntityProtos.*`, `Properties.*`, `PropertiesSerializator.*`, `ProtoManager.*`, metadata annotations, or code that persists/synchronizes entity state.

## Ownership model

The engine owns the entity runtime and metadata/property mechanics. An embedding game project owns concrete prototype files, content IDs, scripts, and gameplay rules that use those mechanics.

Keep this document focused on reusable engine behavior. Put project-specific item/critter/map/location definitions and balancing notes in the embedding project's docs.

## Runtime entity types

`Source/Common/Entity.h` declares the core entity taxonomy through `///@ ExportEntity` annotations:

- `Game` — global engine/game state entity.
- `Player` — player-side entity/view contract.
- `Location` — world location entity with prototypes and time events.
- `Map` — map entity/view contract with prototypes and time events.
- `Critter` — character/NPC/player body entity with prototypes and time events.
- `Item` — item entity/view contract with prototypes, statics, abstract variants, and time events.

`EntityTypeDesc` stores metadata discovered/generated for each entity type:

- whether the type is exported or global;
- whether it has prototypes, statics, abstract entities, or holder entries;
- the `PropertyRegistrator` used by that type;
- exported methods and events;
- holder-entry sync/persistence policy.

For generated metadata and registration flow, see [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md).

## Entity base class

`Entity` is the shared base for all runtime/prototype entities. It owns:

- a `Properties` instance;
- optional event callback lists;
- optional time-event data;
- optional inner-entity holder entries;
- destroying/destroyed state flags;
- intrusive-style reference counting through `AddRef()` / `Release()`.

Important accessors and mutation paths include:

- identity/type: `GetName()`, `GetId()`, `IsGlobal()`, `GetTypeName()`, `GetTypeNamePlural()`;
- property access: `GetProperties()`, `GetPropertiesForEdit()`, `GetValueAsInt()`, `GetValueAsAny()`, `SetValueAsInt()`, `SetValueAsAny()`;
- raw data snapshots: `StoreData()`, `RestoreData()`, `SetValueFromData()`;
- lifecycle state: `IsDestroying()`, `IsDestroyed()`, `MarkAsDestroying()`, `MarkAsDestroyed()`;
- ownership graph: `AddInnerEntity()`, `RemoveInnerEntity()`, `ClearInnerEntities()`;
- event dispatch: `SubscribeEvent()`, `UnsubscribeEvent()`, `FireEvent()`.

Do not bypass `Properties` when changing entity state. Property callbacks, overlay data, sync flags, persistence flags, and script-visible accessors depend on the property layer seeing the mutation.

## Generated property wrappers

`FO_ENTITY_PROPERTY(type, Name)` expands into a small typed accessor surface:

- `GetPropertyName()` returns the registered `Property*` by generated registration index;
- `GetName()` reads the typed value from `Properties`;
- `SetName()` writes through `Properties::SetValue()`;
- `IsNonEmptyName()` checks whether raw property data exists.

`Source/Common/EntityProperties.h` defines the generated property wrapper classes:

- `GameProperties`
- `PlayerProperties`
- `ItemProperties`
- `CritterProperties`
- `MapProperties`
- `LocationProperties`

`EntityProperties` itself contributes common persistent fields:

- `CustomHolderId`
- `CustomHolderEntry`
- `ExplicitlyPersistent`

The generated wrapper classes are thin over `Properties`; the real storage, type information, sync/persistence flags, callbacks, and serialization decisions live in `Property`, `Properties`, and `PropertyRegistrator`.

## Property runtime

`Source/Common/Properties.h` defines four central pieces:

- `PropertyRawData` — temporary typed/raw buffer used by getters/setters and raw restore paths.
- `Property` — metadata for one property: name/component, base type, collection shape, sync flags, mutability, persistence, nullability, callbacks, and registration index.
- `Properties` — per-entity value storage, base/overlay relation, raw snapshot/restore, text import/export, typed get/set helpers, and hash resolution.
- `PropertyRegistrator` — per-entity-type registry that creates properties from metadata tokens and tracks lookup, groups, components, data layout, and public/protected/private data spaces.

Property flags are load-bearing:

- `Common`, `ServerOnly`, `ClientOnly` route side visibility.
- `Synced`, `OwnerSync`, `PublicSync`, `NoSync` route network replication behavior.
- `ModifiableByClient` and `ModifiableByAnyClient` gate client-originated changes.
- `Virtual`, `Mutable`, `Persistent`, `Historical`, `Nullable`, and `Temporary` influence storage, callbacks, persistence, and script contracts.

When changing property metadata, update runtime docs and script/nullability docs together if the change affects script-visible signatures. See [Nullability.md](Nullability.md).

## Base properties and overlays

A `Properties` instance can have base properties. This is used heavily by prototype-derived runtime entities:

- base data provides inherited/default values;
- overlay entries store values that differ from the base or need explicit local data;
- `CompareData()` can ignore temporary properties when comparing snapshots;
- `StoreData()` can include or exclude protected data depending on sync/persistence needs;
- `RemoveSyncedOverlayEntries()` and related overlay helpers keep replicated state compact.

This means a runtime entity is not simply a flat map from property name to value. When debugging, inspect whether the value is coming from base properties, own POD/complex storage, or overlay data.

Proto-derived overlays keep the dense property index lazy: small overlays use a linear scan over their sorted entries, and `_overlayEntryIndex` is built only after the overlay entry count reaches the engine threshold. This avoids allocating an index sized to every registered property for the common low-entry overlay case while keeping dense lookup for larger overlays.

## Prototypes

`Source/Common/EntityProtos.h` defines prototype entities:

- `ProtoEntity` — base entity with `GetProtoId()` and `CollectionName`.
- `EntityWithProto` — mix-in for runtime entities that hold a reference to a `ProtoEntity`.
- `ProtoItem`, `ProtoCritter`, `ProtoMap`, `ProtoLocation` — typed prototype entities with their generated property wrappers.
- `ProtoCustomEntity` — custom prototype entity path.

`Source/Common/ProtoManager.*` owns prototype lookup and loading:

- `GetProtoItem()`, `GetProtoCritter()`, `GetProtoMap()`, `GetProtoLocation()`;
- generic `GetProtoEntity(type, pid)` and `GetProtoEntities(type)`;
- `AddProto()` for adding constructed prototypes;
- `LoadFromResources()` for loading baked/resource-backed prototype data.

Prototype loading is adjacent to resource baking. For baker-side proto handling, see [BakingPipeline.md](BakingPipeline.md).

## Inner entities and holders

Entities can hold other entities under named entries. Holder metadata lives in `EntityTypeDesc::HolderEntryDesc`:

- `TargetType` — what entity type the entry can hold;
- `Sync` — `NoSync`, `OwnerSync`, or `PublicSync`;
- `Persistent` — whether holder membership participates in persistence.

The common persistent fields `CustomHolderId` and `CustomHolderEntry` let custom entities record holder relationships. `EntityManagerApi` provides custom-entity creation, lookup, and destruction hooks:

- `CreateCustomInnerEntity()`
- `CreateCustomEntity()`
- `GetCustomEntity()`
- `DestroyEntity()`

When changing holder behavior, inspect server/client entity managers and persistence paths in addition to `Entity.*`.

## Events and time events

`FO_ENTITY_EVENT(Name, Args...)` creates an `EntityEventWrapper` member. Event callbacks are priority-ordered and return `Entity::EventResult`:

- `ContinueChain`
- `StopChain`

`EntityEventWrapper::Fire()` builds native call data differently for global and non-global entities: non-global events inject the entity as the first argument.

`Entity::TimeEventData` stores scheduled script callbacks, fire time, repeat duration, and script data. Entities that support time events are declared with the `HasTimeEvents` metadata flag in the `ExportEntity` annotations.

## Serialization relationships

Entity state is serialized through property data, not by hand-copying entity fields:

- raw binary property snapshots: `Entity::StoreData()` / `RestoreData()` and `Properties::StoreData()` / `RestoreData()`;
- full property data: `Properties::StoreAllData()` / `RestoreAllData()`;
- text/document conversion: `Properties::SaveToText()`, `ApplyFromText()`, and `PropertiesSerializator.*`.

When text/document loading converts numeric property values, the serializer rejects values that do not fit the target primitive width instead of wrapping or producing infinity.

Persistence backends store `AnyData::Document` records. For database commit/recovery details, see [Persistence.md](Persistence.md).

## Tests to inspect

Relevant tests include:

- `Source/Tests/Test_EntityLifecycle.cpp`
- `Source/Tests/Test_EntityProtos.cpp`
- `Source/Tests/Test_LocationAndEntityMgmt.cpp`
- `Source/Tests/Test_ScriptEntityOps.cpp`
- property/metadata tests such as `Test_Properties.cpp` and `Test_EngineMetadata.cpp` when available in the checkout.

## Change routing

- Entity base, events, holders, time-event storage: `Source/Common/Entity.*`.
- Generated property wrapper classes: `Source/Common/EntityProperties.*`.
- Prototype entity classes: `Source/Common/EntityProtos.*`.
- Prototype lookup/loading: `Source/Common/ProtoManager.*`.
- Property storage and flags: `Source/Common/Properties.*`.
- Property document/text conversion: `Source/Common/PropertiesSerializator.*`.
- Generated metadata and registration: [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md).
- Persistence: [Persistence.md](Persistence.md).
- Network replication and command buffers: [Networking.md](Networking.md).

## Validation checklist

1. Build the smallest target that compiles generated entity/property code.
2. Run entity lifecycle/prototype tests relevant to the changed type.
3. Run property/metadata tests when property flags, registration, or serialization changes.
4. If a property is synced, validate client/server replication paths and update [Networking.md](Networking.md) if behavior changes.
5. If a property is persistent, validate database save/load paths and update [Persistence.md](Persistence.md) if behavior changes.
6. If a property or method is script-visible, validate generated script API and update [Nullability.md](Nullability.md) where applicable.
