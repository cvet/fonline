; Rules added since the last shipped analyzer release. Roslyn's own RS2008 gate requires every diagnostic
; id to be tracked here, which doubles as the changelog for the sync-cover contract.

### New Rules

Rule ID | Category | Severity | Notes
--------|----------|----------|------------------------------------------------------------------------
FOSYNC001 | Synchronization | Warning | A cover annotation sits on a value that is neither an entity nor a collection of entities.
FOSYNC002 | Synchronization | Warning | An argument for a [RequiresCover] parameter is neither covered by the caller, received from a [ProvidesCover] source, nor re-declared.
FOSYNC003 | Synchronization | Warning | An execution-context entry point does not declare [RequiresCover] on the entity the engine already synchronized for it.
FOSYNC004 | Synchronization | Warning | Cover state is probed (Sync.IsCovered / Game.IsEntityLocked) instead of acquired.
FOSYNC005 | Synchronization | Warning | A raw entity-cover primitive (Game.Sync / SyncRelease / TrySyncEntity) is used outside the Sync module.
FOSYNC006 | Synchronization | Warning | A singleton bucket lock (Game.Lock) is not released on every path out of its scope.
FOSYNC007 | Synchronization | Warning | A singleton bucket lock is held across an await, where the continuation may resume on another thread.
FOSYNC009 | Synchronization | Warning | Cover for a value is not re-proved after an await that released it.
