; Rules added since the last shipped analyzer release. Roslyn's own RS2008 gate requires every diagnostic
; id to be tracked here, which doubles as the changelog for the sync-cover contract.

### New Rules

Rule ID | Category | Severity | Notes
--------|----------|----------|------------------------------------------------------------------------
FOSYNC001 | Synchronization | Warning | [RequiresCover] names an entity that is not a parameter of the annotated method.
FOSYNC002 | Synchronization | Warning | A call to a [RequiresCover] method neither acquires cover nor propagates the obligation.
