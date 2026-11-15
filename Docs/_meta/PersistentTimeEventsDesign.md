# Persistent Time Events Design

> Internal design note. It records what an opt-in persistent time event needs from the Engine and why it was not landed together with `EnsureTimeEvent`.

## Status

- State: design only, no code
- Owner: scripting runtime
- Last updated: 2026-11-15
- Landed instead: `Entity.EnsureTimeEvent` and `Game.EnsureTimeEvent`, a start-if-not-running call that replaces the `CountTimeEvent(callback) == 0` guard of the [timed effect recipe](../en/how-to/mechanics/timed-effect.md)

## Problem

Time events are runtime state. A persistent entity stores its properties in the database, but its time events are lost at shutdown and by `Game.UnloadCritter`, and no database load restores them (`EntityManager::LoadEntities()`, `InitLoadedEntities()`, `Game.LoadCritter`). Only a server snapshot restore brings them back (`ServerEngine::RestoreSnapshotTimeEvents`). Both embedding projects therefore keep a persisted deadline, re-arm in `Game.OnCritterInit` with `firstTime == false` and guard with a count. That works, but every project repeats the pattern and a missed re-arm silently loses a gameplay timer.

## Goal

An opt-in way for a time event on a persistent entity to be saved with the entity and restored when the entity is loaded from the database, before that entity's init events, surviving `Game.UnloadCritter`, with downtime counted against the deadline.

## Design

- **Opt-in surface.** A separate method family, for example `StartPersistentTimeEvent`, not a flag on the existing overloads: each `StartTimeEvent` shape is already twelve overloads per backend and entity kind, and a flag changes the meaning of the returned event. The persistent family accepts only a `[[TimeEvent]]` global function, never a delegate or lambda (a delegate address is valid only in the current process, the same rule that makes `CaptureSerializableTimeEvents` count `DelegateBoundCount`), and only on an entity that is persistent when the call is made; calling it on a temporary entity fails loudly.
- **Where the data lives.** One reserved key of the entity's own database document, written next to its properties, holding an array of records (event id, callback name, callback signature alternative, absolute fire time, repeat duration, payload). A separate collection would need its own cascade on entity deletion and a second read at load; the document key is deleted with the record. The payload is the `vector<any_t>` that snapshot serialization already stores, so the typed encoding is reused.
- **Writes.** Only when a persistent event starts, stops, is modified (`RepeatTimeEvent`, `SetTimeEventData`) or finishes. A repeating event stores its period and the fire time of its next firing; rewriting it on every firing costs one write per period, so a repeating persistent event is rewritten only when its repeat or payload changes and restored by advancing the stored fire time in whole periods.
- **Time base.** The absolute fire time is a `synctime` in `Game.SynchronizedTime` units, the clock the server persists and resumes, so downtime counts. Restore converts it to a delay (`max(fire time - now, 0)`), and the manager's 1 millisecond minimum makes an overdue event fire on the next dispatch.
- **Restore.** Between entity creation and the init events, in the slot that `RestoreSnapshotTimeEvents` uses between `EntityManager::LoadEntities()` and `InitLoadedEntities()`, and equally in `Game.LoadCritter` and the other single-entity load paths. It calls `TimeEventManager::RestartTimeEvent`, so callback identity, signature alternative and the global/entity split are validated by the code that already does it for snapshots.
- **Missing callback.** A restored event whose callback no longer resolves with the recorded signature raises `TimeEventException` and fails that entity's load like a failed `InitScript`, leaving the record for diagnosis. It is never dropped silently.
- **Event ids.** A restored event keeps its id; the counter is raised past the largest persisted id (`ReserveTimeEventIds`) before any new event starts, so a new event cannot collide with a stored one.
- **Snapshot interplay.** A snapshot captures every entity timer, including persistent ones. Capture either skips persistent events (the database owns them) or restore skips an id that the database load has already restarted; the first is simpler and keeps one owner per event.
- **Unload and destroy.** `Game.UnloadCritter` and shutdown clear the runtime events (`MarkAsDestroyed`) and keep the stored record; destroying the entity deletes the record with the document. `StopTimeEvent` by id or callback removes the stored record in the same write.
- **Migration.** The new key is additive: a database written before it simply has no key. Moving back to an Engine without it leaves the key unread. If the encoding of a record ever changes, a `///@ MigrationRule` and a marker bump follow the "Current API, preserved meaning" rule of the repository instructions.
- **Cost.** One document write per persistent start, stop or modification; no per-tick work.

## Why it is not in this change

- The restore touches the shared load path of six entity kinds plus custom entities and the global entity, and needs a decision for each on when its stored events may fire relative to map and location loading.
- It needs an integration test with a real database restart for every kind, not only a unit test, and both scripting backends (the Managed side cannot persist a delegate).
- `EnsureTimeEvent` removes the repeated guard now and leaves the persisted deadline and the `OnCritterInit` re-arm as the documented pattern; nothing in it blocks the persistent family later.

## Open questions

- Whether the global engine entity (`Game.StartPersistentTimeEvent`) belongs in the first version; its record would live with the synchronized-time record rather than an entity document.
- Whether a repeating persistent event should fire once for each missed period or once for all of them after a long downtime; the snapshot restore currently resumes with the remaining delay only.
