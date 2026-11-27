; Rules added since the last shipped analyzer release. Roslyn's own RS2008 gate requires every diagnostic
; id to be tracked here, which doubles as the changelog for the engine script contracts.

### New Rules

Rule ID | Category | Severity | Notes
--------|----------|----------|------------------------------------------------------------------------
FOSYNC001 | Synchronization | Warning | A cover annotation sits on a value that is neither an entity nor a collection of entities.
FOSYNC002 | Synchronization | Warning | An argument for a [RequiresCover] parameter is neither covered by the caller, received from a [ProvidesCover] source, nor re-declared.
FOSYNC003 | Synchronization | Warning | An execution-context entry point does not declare [RequiresCover] on the entity the engine already synchronized for it.
FOSYNC004 | Synchronization | Warning | Cover state is probed (Sync.IsCovered / Game.IsEntityLocked) instead of acquired.
FOSYNC005 | Synchronization | Warning | A raw synchronization primitive is used outside its wrapper: Game.Sync / SyncRelease outside Sync, Game.Lock / Unlock outside GameLock.
FOSYNC009 | Synchronization | Warning | Cover for a value is not re-proved after an await that released it.
FOSYNC011 | Synchronization | Warning | A Sync helper changes the held cover -- through the primitive or another changing helper -- without declaring the effect it has.
FOSYNC010 | Synchronization | Warning | The boolean answer of a cover acquisition is discarded instead of read.
FOSYNC015 | Synchronization | Warning | A widening acquisition names only entities the caller already covers, with no snapshot able to observe the difference.
FOENTRY001 | Dispatch | Warning | A method carrying a dispatch attribute is invoked directly instead of through its dispatcher.
FOENTRY002 | Dispatch | Warning | A method the engine dispatches or looks up through an attribute is not declared internal.
FOENTRY003 | Dispatch | Warning | An fonline_entry option names an attribute type the compilation does not contain.
FOVERIFY001 | Verification | Warning | An Invariant.Verify message restates its condition instead of describing the invariant.
FOVERIFY002 | Verification | Warning | Invariant.Verify is called with a constant false condition instead of Invariant.Failed.
FOVERIFY003 | Verification | Warning | Invariant.Verify or Invariant.VerifyNotNull evaluates acting code instead of a predicate.
FOSTATIC001 | Lifetime | Warning | A script static holds a reference the engine cannot clear when scripting shuts down.
