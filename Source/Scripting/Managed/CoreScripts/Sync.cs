#nullable enable

using System.Collections.Generic;
using System.Threading.Tasks;

namespace FOnline
{
    public static partial class Sync
    {
#if SERVER

        // Game.Sync(...) is the native lock primitive — it only acquires locks, replacing the SyncContext
        // lock set with exactly the listed entities. It has no return value: if a pending lock wait is
        // aborted by server shutdown (after the Server.ShutdownGraceMs drain window expires) it throws
        // EntityLockWaitAbortedException, which is not caught here — the exception unwinds the async job and
        // stops its work during teardown. These Sync::Lock(...) helpers add the liveness policy on top: they
        // check IsDestroyed before and after acquiring and return `false` so the caller can early-out without
        // touching dead handles. A destroyed entity is not always fatal — e.g. a critter's presumed map can
        // change and the old one be destroyed, in which case the caller re-reads the map and locks again
        // rather than treating `false` as a hard stop.
        //
        // Empty inputs are treated as "nothing to lock" and return `true`. All call sites can use a
        // uniform pattern:
        //
        //     if (!Sync::Lock(npc, map)) return;
        //     if (!Sync::Lock(npc, map, nearbyCritters)) return;

        // Lifecycle: strict — a destroyed/destroying entity returns false before or after acquisition; it is never skipped.
        [Async]
        public static async Task<bool> Lock(Entity entity)
        {
            if (entity.IsDestroyed || entity.IsDestroying) {
                return false;
            }

            Game.Sync(entity);

            return !entity.IsDestroyed && !entity.IsDestroying;
        }

        // Lifecycle: strict — either destroyed/destroying entity makes the call return false; neither one is skipped.
        [Async]
        public static async Task<bool> Lock(Entity firstEntity, Entity secondEntity)
        {
            if (firstEntity.IsDestroyed || firstEntity.IsDestroying || secondEntity.IsDestroyed || secondEntity.IsDestroying) {
                return false;
            }

            Game.Sync(firstEntity, secondEntity);

            return !firstEntity.IsDestroyed && !firstEntity.IsDestroying && !secondEntity.IsDestroyed && !secondEntity.IsDestroying;
        }

        // Lifecycle: strict — any destroyed/destroying entity makes the call return false; no partial set is accepted.
        [Async]
        public static async Task<bool> Lock(Entity firstEntity, Entity secondEntity, Entity thirdEntity)
        {
            if (firstEntity.IsDestroyed || firstEntity.IsDestroying || secondEntity.IsDestroyed || secondEntity.IsDestroying || thirdEntity.IsDestroyed ||
                thirdEntity.IsDestroying) {
                return false;
            }

            Game.Sync(firstEntity, secondEntity, thirdEntity);

            return !firstEntity.IsDestroyed && !firstEntity.IsDestroying && !secondEntity.IsDestroyed && !secondEntity.IsDestroying && !thirdEntity.IsDestroyed &&
                   !thirdEntity.IsDestroying;
        }

        // Lifecycle: an empty array succeeds without changing cover; any destroyed/destroying member returns false and is not skipped.
        [Async]
        public static async Task<bool> Lock(List<Entity> entities)
        {
            if (entities.Count == 0) {
                return true;
            }

            for (int i = 0; i < entities.Count; i++) {
                if (entities[i].IsDestroyed || entities[i].IsDestroying) {
                    return false;
                }
            }

            Game.Sync(entities);

            for (int i = 0; i < entities.Count; i++) {
                if (entities[i].IsDestroyed || entities[i].IsDestroying) {
                    return false;
                }
            }

            return true;
        }

        // Snapshot the current lock cover (the entities whose locks are held) so a caller can
        // run a helper that REPLACES the lock set (Modifiers::FireCritterEvent, async sub-helpers, …) and
        // then re-establish exactly the same cover with Restore(). This is the script-side equivalent of the
        // engine's per-callback nested SyncContext: it keeps a caller's downstream entity access valid across
        // re-entrant work that internally re-Sync's.
        // Lifecycle: Snapshot does not filter handles or include Game.Lock singleton entries; Restore reports whether every captured handle survived.
        public static List<Entity> Snapshot()
        {
            return Game.GetHeldSyncEntities();
        }

        // Lifecycle: strict query — destroyed/destroying entities return false before the native coverage probe.
        public static bool IsCovered(Entity entity)
        {
            return !entity.IsDestroyed && !entity.IsDestroying && Game.IsEntityLocked(entity);
        }

        // Lifecycle: restores every live entry and returns true only if the entire input stayed live; an empty snapshot releases all cover and succeeds.
        [Async]
        public static async Task<bool> Restore(List<Entity> entities)
        {
            List<Entity> candidates = new List<Entity>(entities);
            bool allEntitiesLive = true;

            while (true) {
                List<Entity> survivors = new List<Entity>();

                for (int i = 0; i < candidates.Count; i++) {
                    if (!candidates[i].IsDestroyed && !candidates[i].IsDestroying) {
                        survivors.Add(candidates[i]);
                    }
                }

                if (survivors.Count != candidates.Count) {
                    allEntitiesLive = false;
                }

                if (survivors.Count == 0) {
                    Release();
                    return allEntitiesLive;
                }

                if (await Lock(survivors)) {
                    return allEntitiesLive;
                }

                allEntitiesLive = false;
                candidates = survivors;
            }
        }

        // Widens the current cover with the given entities instead of replacing it: everything already held
        // stays held, the extras are added. Fast path: when every extra is already covered, no lock
        // transition happens at all (cheap probe), which makes Widen safe to call on hot paths and in loops.
        // Destroyed/destroying extras are dropped by Restore's filtering — the caller re-checks lifecycle
        // after the call if it continues using them (yield boundary).
        // Lifecycle: strict for requested extras; stale extras fail the call, stale retained cover is pruned, and live survivors remain covered.
        [Async]
        public static async Task<bool> Widen(List<Entity> extras)
        {
            for (int i = 0; i < extras.Count; i++) {
                if (extras[i].IsDestroyed || extras[i].IsDestroying) {
                    return false;
                }
            }

            List<Entity> cover = Snapshot();
            bool changed = false;

            for (int i = 0; i < cover.Count; i++) {
                if (cover[i].IsDestroyed || cover[i].IsDestroying) {
                    changed = true;
                    break;
                }
            }

            for (int i = 0; i < extras.Count; i++) {
                if (!ContainsReference(cover, extras[i])) {
                    cover.Add(extras[i]);
                    changed = true;
                }
            }

            if (changed) {
                await Restore(cover);
            }

            List<Entity> widenedCover = Snapshot();
            for (int i = 0; i < extras.Count; i++) {
                if (extras[i].IsDestroyed || extras[i].IsDestroying || !ContainsReference(widenedCover, extras[i])) {
                    return false;
                }
            }

            return true;
        }

        // Lifecycle: strict for the requested extra; a stale extra fails without a native lookup, while stale retained cover is pruned for live input.
        [Async]
        public static async Task<bool> Widen(Entity extra)
        {
            return await Widen(new List<Entity> {extra});
        }

        // SyncScope: widens current cover with every live extra while intentionally skipping stale requests.
        // Lifecycle: best-effort; unlike strict Widen, this operation does not prove requested handles live.
        [Async]
        public static async Task WidenBestEffort(List<Entity> extras)
        {
            List<Entity> cover = Snapshot();
            bool changed = false;

            for (int i = 0; i < extras.Count; i++) {
                Entity extra = extras[i];
                if (extra.IsDestroyed || extra.IsDestroying) {
                    continue;
                }

                if (!ContainsReference(cover, extra)) {
                    cover.Add(extra);
                    changed = true;
                }
            }

            if (changed) {
                await Restore(cover);
            }
        }

        // SyncScope: single-entity best-effort widening overload.
        // Lifecycle: best-effort — a destroyed/destroying extra is intentionally omitted; an explicitly held live extra is a no-op.
        [Async]
        public static async Task WidenBestEffort(Entity extra)
        {
            if (extra.IsDestroyed || extra.IsDestroying) {
                return;
            }

            List<Entity> cover = Snapshot();
            if (ContainsReference(cover, extra)) {
                return;
            }

            cover.Add(extra);
            await Restore(cover);
        }

        private static bool ContainsReference(List<Entity> entities, Entity entity)
        {
            for (int i = 0; i < entities.Count; i++) {
                if (entities[i] == entity) {
                    return true;
                }
            }

            return false;
        }

        // SyncScope: widens cover with cr and its current map when mapped; retries if cr migrates during acquisition.
        // Lifecycle: a stale cr/current map returns false; a map destroyed during escalation is retried through the current cr-to-map link.
        [Async]
        public static Task<bool> WidenCritterWithMap(Critter cr) =>
            WidenCritterWithMap(new List<Entity>(), cr);

        // SyncScope: widens cover with strictRoots + cr + its current map when mapped; every retry explicitly re-proves all roots.
        // Lifecycle: a stale explicit root/cr/current map returns false; a changed cr-to-map link is retried.
        [Async]
        public static async Task<bool> WidenCritterWithMap(List<Entity> strictRoots, Critter cr)
        {
            List<Entity> roots = new List<Entity>(strictRoots);
            if (!roots.Contains(cr)) {
                roots.Add(cr);
            }

            while (true) {
                if (!await Widen(roots)) {
                    return false;
                }

                Map? map = cr.GetMap();
                if (map == null) {
                    return true;
                }

                ident mapId = map.Id;
                List<Entity> scope = new List<Entity>(roots);
                if (!scope.Contains(map)) {
                    scope.Add(map);
                }
                if (!await Widen(scope)) {
                    if (!await Widen(roots)) {
                        return false;
                    }
                    if (cr.MapId != mapId) {
                        continue;
                    }

                    return false;
                }
                if (cr.MapId != mapId) {
                    continue;
                }

                return true;
            }
        }

        // SyncScope: widens cover with both critters and their distinct current maps; retries if either critter migrates.
        // Lifecycle: a stale critter or initially resolved map returns false; acquisition races retry against both current map links.
        [Async]
        public static async Task<bool> WidenCrittersWithMap(Critter first, Critter second)
        {
            while (true) {
                if (!await Widen(new List<Entity> {first, second})) {
                    return false;
                }

                Map? firstMap = first.GetMap();
                Map? secondMap = second.GetMap();
                ident firstMapId = firstMap != null ? firstMap.Id : new ident(0);
                ident secondMapId = secondMap != null ? secondMap.Id : new ident(0);
                List<Entity> scope = new List<Entity> {first, second};

                if (firstMap != null) {
                    scope.Add(firstMap);
                }
                if (secondMap != null && secondMapId != firstMapId) {
                    scope.Add(secondMap);
                }

                if (!await Widen(scope)) {
                    if (!await Widen(new List<Entity> {first, second})) {
                        return false;
                    }
                    if (first.MapId != firstMapId || second.MapId != secondMapId) {
                        continue;
                    }

                    return false;
                }
                if (first.MapId != firstMapId || second.MapId != secondMapId) {
                    continue;
                }

                return true;
            }
        }

        // SyncScope: widens cover with cr, its current map, and that map's current location.
        // Lifecycle: a stale cr or resolved map/location returns false; acquisition races retry against the current parent chain.
        [Async]
        public static async Task<bool> WidenCritterWithMapAndLocation(Critter cr)
        {
            while (true) {
                if (!await Widen(cr)) {
                    return false;
                }

                Map? map = cr.GetMap();
                if (map == null) {
                    return true;
                }

                ident mapId = map.Id;
                if (!await Widen(new List<Entity> {cr, map})) {
                    if (!await Widen(cr)) {
                        return false;
                    }
                    if (cr.MapId != mapId) {
                        continue;
                    }

                    return false;
                }
                if (cr.MapId != mapId) {
                    continue;
                }

                Location loc = map.GetLocation();
                ident locId = loc.Id;
                if (!await Widen(new List<Entity> {cr, map, loc})) {
                    if (!await WidenCritterWithMap(cr)) {
                        return false;
                    }
                    if (cr.MapId != mapId || map.GetLocation().Id != locId) {
                        continue;
                    }

                    return false;
                }
                if (cr.MapId != mapId || map.GetLocation().Id != locId) {
                    continue;
                }

                return true;
            }
        }

        // Exit-restore for cover-neutral helpers: re-establishes the caller's snapshot cover plus the
        // helper's own entities after re-entrant work replaced the lock set. An EMPTY snapshot means the
        // caller ran in the unrestricted (empty-context) mode — release back to it instead of leaving a
        // partial lock set that would reject the caller's follow-up accesses.
        // Lifecycle: an empty snapshot releases (also drains Game.Lock) without adding first/second; otherwise stale snapshot/fixed entries are dropped best-effort.
        [Async]
        public static async Task RestoreCallerCover(List<Entity> snapshot, Entity first, Entity second)
        {
            if (snapshot.Count == 0) {
                Release();
                return;
            }

            snapshot.Add(first);
            snapshot.Add(second);
            _ = await Restore(snapshot);
        }

        // Lifecycle: an empty array succeeds without changing cover; any destroyed/destroying critter returns false and is not skipped.
        [Async]
        public static async Task<bool> Lock(List<Critter> critters)
        {
            if (critters.Count == 0) {
                return true;
            }

            List<Entity> entities = new List<Entity>(critters.Count);
            for (int i = 0; i < critters.Count; i++) {
                if (critters[i].IsDestroyed) {
                    return false;
                }

                entities.Add(critters[i]);
            }
            return await Lock(entities);
        }

        // Lifecycle: an empty array succeeds without changing cover; any destroyed/destroying item returns false and is not skipped.
        [Async]
        public static async Task<bool> Lock(List<Item> items)
        {
            if (items.Count == 0) {
                return true;
            }

            List<Entity> entities = new List<Entity>(items.Count);
            for (int i = 0; i < items.Count; i++) {
                if (items[i].IsDestroyed) {
                    return false;
                }

                entities.Add(items[i]);
            }
            return await Lock(entities);
        }

        // Lifecycle: an empty array succeeds without changing cover; any destroyed/destroying map returns false and is not skipped.
        [Async]
        public static async Task<bool> Lock(List<Map> maps)
        {
            if (maps.Count == 0) {
                return true;
            }

            List<Entity> entities = new List<Entity>(maps.Count);
            for (int i = 0; i < maps.Count; i++) {
                if (maps[i].IsDestroyed) {
                    return false;
                }

                entities.Add(maps[i]);
            }
            return await Lock(entities);
        }

        // Lifecycle: an empty array succeeds without changing cover; any destroyed/destroying location returns false and is not skipped.
        [Async]
        public static async Task<bool> Lock(List<Location> locations)
        {
            if (locations.Count == 0) {
                return true;
            }

            List<Entity> entities = new List<Entity>(locations.Count);
            for (int i = 0; i < locations.Count; i++) {
                if (locations[i].IsDestroyed) {
                    return false;
                }

                entities.Add(locations[i]);
            }
            return await Lock(entities);
        }

        // Each Game.Sync replaces the SyncContext lock set entirely, so a script that wants both fixed
        // entities (typically the [[TimeEvent]] owner and a related entity like a map) AND a list of
        // neighbours synced has to list everything in one call. These overloads bundle the fixed leading
        // entities with a typed array into a single Entity[] sync request and run the alive check across
        // the union.

        // Lifecycle: strict — a stale fixed entity or critter returns false; an empty critter array locks only the fixed entity.
        [Async]
        public static async Task<bool> Lock(Entity entity, List<Critter> critters)
        {
            if (entity.IsDestroyed) {
                return false;
            }

            List<Entity> entities = new List<Entity>(critters.Count + 1);
            entities.Add(entity);
            for (int i = 0; i < critters.Count; i++) {
                if (critters[i].IsDestroyed) {
                    return false;
                }

                entities.Add(critters[i]);
            }
            return await Lock(entities);
        }

        // Lifecycle: strict — a stale fixed entity or critter returns false; an empty array locks the two fixed entities.
        [Async]
        public static async Task<bool> Lock(Entity firstEntity, Entity secondEntity, List<Critter> critters)
        {
            if (firstEntity.IsDestroyed || secondEntity.IsDestroyed) {
                return false;
            }

            List<Entity> entities = new List<Entity>(critters.Count + 2);
            entities.Add(firstEntity);
            entities.Add(secondEntity);
            for (int i = 0; i < critters.Count; i++) {
                if (critters[i].IsDestroyed) {
                    return false;
                }

                entities.Add(critters[i]);
            }
            return await Lock(entities);
        }

        // Lifecycle: strict — a stale fixed entity or item returns false; an empty item array locks only the fixed entity.
        [Async]
        public static async Task<bool> Lock(Entity entity, List<Item> items)
        {
            if (entity.IsDestroyed) {
                return false;
            }

            List<Entity> entities = new List<Entity>(items.Count + 1);
            entities.Add(entity);
            for (int i = 0; i < items.Count; i++) {
                if (items[i].IsDestroyed) {
                    return false;
                }

                entities.Add(items[i]);
            }
            return await Lock(entities);
        }

        // Lifecycle: strict — a stale fixed entity or item returns false; an empty array locks the two fixed entities.
        [Async]
        public static async Task<bool> Lock(Entity firstEntity, Entity secondEntity, List<Item> items)
        {
            if (firstEntity.IsDestroyed || secondEntity.IsDestroyed) {
                return false;
            }

            List<Entity> entities = new List<Entity>(items.Count + 2);
            entities.Add(firstEntity);
            entities.Add(secondEntity);
            for (int i = 0; i < items.Count; i++) {
                if (items[i].IsDestroyed) {
                    return false;
                }

                entities.Add(items[i]);
            }
            return await Lock(entities);
        }

        // Lifecycle: performs no entity checks and reports no status; an empty cover is not unrestricted, so later entity access must sync again.
        public static void Release()
        {
            Game.SyncRelease();
        }

        // Lifecycle: a stale cr/current map returns false; a map destroyed during escalation is retried through the current cr->map link.
        [Async]
        public static async Task<bool> LockCritterWithMap(Critter cr)
        {
            while (true) {
                if (cr.IsDestroyed) {
                    return false;
                }

                if (!await Lock(cr)) {
                    return false;
                }

                Map? map = cr.GetMap();

                if (map == null) {
                    return true;
                }

                if (map.IsDestroyed) {
                    return false;
                }

                ident mapId = map.Id;

                if (!await Lock(cr, map)) {
                    continue;
                }

                // Critter migrated to another map between read and lock; retry.
                if (cr.MapId != mapId) {
                    continue;
                }

                return true;
            }
        }

        // Lifecycle: a stale critter or initially resolved map returns false; acquisition races retry against both current map links.
        [Async]
        public static async Task<bool> LockCrittersWithMap(Critter first, Critter second)
        {
            while (true) {
                if (first.IsDestroyed || first.IsDestroying || second.IsDestroyed || second.IsDestroying) {
                    return false;
                }

                if (!await Lock(first, second)) {
                    return false;
                }

                Map? firstMap = first.GetMap();
                Map? secondMap = second.GetMap();

                if (firstMap != null && (firstMap.IsDestroyed || firstMap.IsDestroying)) {
                    return false;
                }
                if (secondMap != null && (secondMap.IsDestroyed || secondMap.IsDestroying)) {
                    return false;
                }

                ident firstMapId = firstMap != null ? firstMap.Id : new ident(0);
                ident secondMapId = secondMap != null ? secondMap.Id : new ident(0);
                List<Entity> scope = new List<Entity> {first, second};

                if (firstMap != null) {
                    scope.Add(firstMap);
                }
                if (secondMap != null && secondMapId != firstMapId) {
                    scope.Add(secondMap);
                }

                if (!await Lock(scope)) {
                    continue;
                }

                if (first.MapId != firstMapId || second.MapId != secondMapId) {
                    continue;
                }

                return true;
            }
        }

        // Lifecycle: a stale cr or resolved map/location returns false; acquisition races retry against the current parent chain.
        [Async]
        public static async Task<bool> LockCritterWithMapAndLocation(Critter cr)
        {
            while (true) {
                if (cr.IsDestroyed) {
                    return false;
                }

                if (!await Lock(cr)) {
                    return false;
                }

                Map? map = cr.GetMap();

                if (map == null) {
                    return true;
                }

                if (map.IsDestroyed) {
                    return false;
                }

                ident mapId = map.Id;

                // cr + map must be held before map.GetLocation() can be read against the map's lock.
                if (!await Lock(cr, map)) {
                    continue;
                }

                if (cr.MapId != mapId) {
                    continue;
                }

                Location loc = map.GetLocation();
                if (loc.IsDestroyed) {
                    return false;
                }

                ident locId = loc.Id;

                // Escalate to cr + map + location so the location's own properties are accessible.
                if (!await Lock(cr, map, loc)) {
                    continue;
                }

                // Critter migrated while widening the lock set; retry.
                if (cr.MapId != mapId) {
                    continue;
                }

                if (map.GetLocation().Id != locId) {
                    continue;
                }

                return true;
            }
        }

        // Lifecycle: a stale cr/member/destination chain returns false; a changed source graph is retried because cr may have migrated or changed groups.
        [Async]
        public static async Task<bool> LockForTransferToMap(Critter cr, Map destMap)
        {
            while (true) {
                if (cr.IsDestroyed || cr.IsDestroying || destMap.IsDestroyed || destMap.IsDestroying) {
                    return false;
                }

                if (!await Lock(cr, destMap)) {
                    return false;
                }

                Map? srcMap = cr.GetMap();
                Location destLoc = destMap.GetLocation();
                if (destLoc.IsDestroyed || destLoc.IsDestroying) {
                    return false;
                }

                List<Entity> scope = new List<Entity> {cr, destMap, destLoc};
                if (srcMap != null) {
                    scope.Add(srcMap);
                }

                if (!await Lock(scope)) {
                    continue;
                }

                ident srcMapId = new ident(0);
                if (srcMap != null) {
                    if (srcMap.IsDestroyed || srcMap.IsDestroying) {
                        continue;
                    }

                    srcMapId = srcMap.Id;
                }

                if (cr.MapId != srcMapId || destMap.GetLocation().Id != destLoc.Id) {
                    continue;
                }

                if (srcMap != null) {
                    Location srcLoc = srcMap.GetLocation();
                    if (srcLoc.IsDestroyed || srcLoc.IsDestroying) {
                        continue;
                    }

                    ident srcLocId = srcLoc.Id;
                    if (srcLocId != destLoc.Id) {
                        scope.Add(srcLoc);

                        if (!await Lock(scope)) {
                            continue;
                        }

                        if (cr.MapId != srcMapId || destMap.GetLocation().Id != destLoc.Id || srcMap.GetLocation().Id != srcLocId) {
                            continue;
                        }
                    }
                }

                return true;
            }
        }

        // Lifecycle: strict — a mapped root, stable destroyed/destroying member, or exhausted retry budget returns false.
        [Async]
        public static Task<bool> WidenCritterWithGlobalMapGroup(Critter cr) =>
            WidenCritterWithGlobalMapGroup(new List<Entity>(), cr);

        // SyncScope: retry yields may drop incidental caller cover; every acquisition re-proves strictRoots + cr,
        // and success also covers every member from the stable native global-group snapshot.
        // Lifecycle: strict - a stale explicit root/member, mapped cr, or exhausted retry budget returns false.
        [Async]
        public static async Task<bool> WidenCritterWithGlobalMapGroup(List<Entity> strictRoots, Critter cr)
        {
            List<Entity> roots = new List<Entity>(strictRoots);
            if (!roots.Contains(cr)) {
                roots.Add(cr);
            }

            for (int attempt = 0; attempt < GlobalMapGroupCoverAttempts; attempt++) {
                if (!await Widen(roots)) {
                    return false;
                }
                if (cr.MapId.value != 0) {
                    return false;
                }

                uint tripId = cr.GlobalMapTripId;
                ulong revision = 0;
                List<ident> memberIds = cr.GetGlobalMapCritterIds(ref revision);
                if (!memberIds.Contains(cr.Id)) {
                    return false;
                }

                List<Entity> scope = new List<Entity>(roots);
                bool allMembersResolved = true;
                for (int i = 0; i < memberIds.Count; i++) {
                    Critter? member = Game.GetCritter(memberIds[i]);
                    if (member == null) {
                        allMembersResolved = false;
                        break;
                    }
                    if (!scope.Contains(member)) {
                        scope.Add(member);
                    }
                }

                if (!allMembersResolved || !await Widen(scope)) {
                    if (!await Widen(roots)) {
                        return false;
                    }
                    if (cr.MapId.value != 0 || cr.GlobalMapTripId != tripId) {
                        await Game.YieldAsync(0);
                        continue;
                    }

                    ulong currentRevision = 0;
                    List<ident> currentMemberIds = cr.GetGlobalMapCritterIds(ref currentRevision);
                    if (currentRevision != revision || !HasSameIdentMembership(memberIds, currentMemberIds)) {
                        await Game.YieldAsync(0);
                        continue;
                    }
                    return false;
                }

                if (cr.MapId.value != 0 || cr.GlobalMapTripId != tripId) {
                    await Game.YieldAsync(0);
                    continue;
                }

                ulong stableRevision = 0;
                List<ident> stableMemberIds = cr.GetGlobalMapCritterIds(ref stableRevision);
                if (stableRevision == revision && HasSameIdentMembership(memberIds, stableMemberIds)) {
                    return true;
                }

                await Game.YieldAsync(0);
            }

            return false;
        }

        // Monotonic counterpart of LockForTransferToMap for helpers that must retain caller-owned roots.
        // Lifecycle: a stale cr/member/destination chain returns false; a changed source graph is retried against the current parent/group links.
        [Async]
        public static Task<bool> WidenForTransferToMap(Critter cr, Map destMap) =>
            WidenForTransferToMap(new List<Entity>(), cr, destMap);

        // SyncScope: retry yields may drop incidental caller cover; success re-proves strictRoots + cr +
        // its complete stable source graph + destMap/location.
        // Lifecycle: a stale explicit root/source member/destination chain returns false; changed source
        // or parent graphs are retried.
        // Lifecycle: a stale explicit root/source member/destination chain returns false; changed source or parent graphs are retried.
        [Async]
        public static async Task<bool> WidenForTransferToMap(
            List<Entity> strictRoots,
            Critter cr,
            Map destMap)
        {
            List<Entity> roots = new List<Entity>(strictRoots);
            if (!roots.Contains(cr)) {
                roots.Add(cr);
            }
            if (!roots.Contains(destMap)) {
                roots.Add(destMap);
            }

            for (int attempt = 0; attempt < GlobalMapGroupCoverAttempts; attempt++) {
                if (!await Widen(roots)) {
                    return false;
                }

                Map? srcMap = cr.GetMap();
                Location destLoc = destMap.GetLocation();
                ident srcMapId = srcMap != null ? srcMap.Id : new ident(0);
                ident destLocId = destLoc.Id;

                List<Entity> scope = new List<Entity>(roots);
                if (!scope.Contains(destLoc)) {
                    scope.Add(destLoc);
                }
                uint srcTripId = 0;
                ulong srcRevision = 0;
                List<ident> srcMemberIds = new List<ident>();

                if (srcMap != null) {
                    scope.Add(srcMap);
                }
                else {
                    if (cr.MapId.value != 0) {
                        await Game.YieldAsync(0);
                        continue;
                    }

                    srcTripId = cr.GlobalMapTripId;
                    srcMemberIds = cr.GetGlobalMapCritterIds(ref srcRevision);
                    if (!srcMemberIds.Contains(cr.Id)) {
                        return false;
                    }

                    bool allMembersResolved = true;
                    for (int i = 0; i < srcMemberIds.Count; i++) {
                        Critter? member = Game.GetCritter(srcMemberIds[i]);
                        if (member == null) {
                            allMembersResolved = false;
                            break;
                        }
                        if (!scope.Contains(member)) {
                            scope.Add(member);
                        }
                    }
                    if (!allMembersResolved) {
                        if (!await Widen(roots)) {
                            return false;
                        }
                        if (!IsGlobalMapGroupSnapshotCurrent(cr, srcTripId, srcRevision, srcMemberIds)) {
                            await Game.YieldAsync(0);
                            continue;
                        }
                        return false;
                    }
                }

                if (!await Widen(scope)) {
                    if (!await Widen(roots)) {
                        return false;
                    }
                    if (cr.MapId != srcMapId || destMap.GetLocation().Id != destLocId) {
                        await Game.YieldAsync(0);
                        continue;
                    }
                    if (srcMap == null && !IsGlobalMapGroupSnapshotCurrent(cr, srcTripId, srcRevision, srcMemberIds)) {
                        await Game.YieldAsync(0);
                        continue;
                    }
                    return false;
                }

                if (cr.MapId != srcMapId || destMap.GetLocation().Id != destLocId) {
                    await Game.YieldAsync(0);
                    continue;
                }
                if (srcMap == null &&
                    (!IsGlobalMapGroupSnapshotCurrent(cr, srcTripId, srcRevision, srcMemberIds) ||
                     !IsIdentMembershipCovered(srcMemberIds))) {
                    await Game.YieldAsync(0);
                    continue;
                }

                if (srcMap != null) {
                    Location srcLoc = srcMap.GetLocation();
                    ident srcLocId = srcLoc.Id;
                    if (srcLocId != destLocId) {
                        scope.Add(srcLoc);
                        if (!await Widen(scope)) {
                            if (!await Widen(roots)) {
                                return false;
                            }
                            if (cr.MapId != srcMapId || destMap.GetLocation().Id != destLocId) {
                                continue;
                            }

                            List<Entity> mapScope = new List<Entity>(roots);
                            if (!mapScope.Contains(destLoc)) {
                                mapScope.Add(destLoc);
                            }
                            if (!mapScope.Contains(srcMap)) {
                                mapScope.Add(srcMap);
                            }
                            if (!await Widen(mapScope)) {
                                if (!await Widen(roots)) {
                                    return false;
                                }
                                if (cr.MapId != srcMapId || destMap.GetLocation().Id != destLocId) {
                                    continue;
                                }
                                return false;
                            }
                            if (cr.MapId != srcMapId ||
                                destMap.GetLocation().Id != destLocId ||
                                srcMap.GetLocation().Id != srcLocId) {
                                await Game.YieldAsync(0);
                                continue;
                            }
                            return false;
                        }

                        if (cr.MapId != srcMapId ||
                            destMap.GetLocation().Id != destLocId ||
                            srcMap.GetLocation().Id != srcLocId) {
                            await Game.YieldAsync(0);
                            continue;
                        }
                    }
                }

                return true;
            }

            return false;
        }

        // Lifecycle: a stale player/map or resolved location returns false; final acquisition/relink races retry the map->location chain.
        [Async]
        public static async Task<bool> LockForViewMap(Player player, Map map)
        {
            while (true) {
                if (!await Lock(player, map)) {
                    return false;
                }

                Location loc = map.GetLocation();
                if (loc.IsDestroyed || loc.IsDestroying) {
                    return false;
                }

                ident locId = loc.Id;
                if (!await Lock(player, map, loc)) {
                    continue;
                }

                if (map.GetLocation().Id != locId) {
                    continue;
                }

                return true;
            }
        }

        // Lifecycle: a stale player/cr or resolved map/location returns false; acquisition races retry against the current cr parent chain.
        [Async]
        public static async Task<bool> LockPlayerAndCritterWithMapAndLocation(Player player, Critter cr)
        {
            while (true) {
                if (player.IsDestroyed || player.IsDestroying || cr.IsDestroyed || cr.IsDestroying) {
                    return false;
                }

                if (!await Lock(player, cr)) {
                    return false;
                }

                Map? map = cr.GetMap();
                if (map == null) {
                    return true;
                }

                if (map.IsDestroyed || map.IsDestroying) {
                    return false;
                }

                ident mapId = map.Id;
                if (!await Lock(new List<Entity> {player, cr, map}) || cr.MapId != mapId) {
                    continue;
                }

                Location loc = map.GetLocation();
                if (loc.IsDestroyed || loc.IsDestroying) {
                    return false;
                }

                ident locId = loc.Id;
                if (!await Lock(new List<Entity> {player, cr, map, loc})) {
                    continue;
                }

                if (cr.MapId != mapId || map.GetLocation().Id != locId) {
                    continue;
                }

                return true;
            }
        }

        // SyncScope: replaces cover with player + cr and the stable initial-info dependency graph: map/location when mapped, or every current global-map group member.
        // Lifecycle: strict - any stale dependency returns false; parent/group changes during acquisition are retried before returning success.
        [Async]
        public static async Task<bool> LockPlayerCritterInitialInfoGraph(Player player, Critter cr)
        {
            return await LockCrittersInitialInfoGraphs(new List<Entity> {player}, new List<Critter> {cr});
        }

        // SyncScope: replaces cover with player + every critter and the union of all stable mapped or global initial-info graphs.
        // Lifecycle: strict - every root, map/location, and global-group member is requested by the final exact acquisition; graph changes are retried.
        [Async]
        public static async Task<bool> LockPlayerCrittersInitialInfoGraphs(Player player, List<Critter> critters)
        {
            return await LockCrittersInitialInfoGraphs(new List<Entity> {player}, critters);
        }

        // SyncScope: replaces cover with both sessions, the stable controlled-critter initial-info graph,
        // and the spectator view target. A graph race returns false to the caller's retry budget.
        // Lifecycle: strict — a stale dependency or concurrent player graph change returns false to the caller's single retry budget; a stable asymmetric player/cr link is an invariant failure.
        [Async]
        public static async Task<bool> LockPlayerReconnectGraph(Player unloginedPlayer, Player player)
        {
            if (!await Lock(unloginedPlayer, player)) {
                return false;
            }

            Critter? controlledCr = player.GetControlledCritter();
            Map? viewMap = player.GetViewMapTarget();
            List<Entity> roots = new List<Entity> {unloginedPlayer, player};
            if (controlledCr != null) {
                roots.Add(controlledCr);
            }
            if (viewMap != null) {
                roots.Add(viewMap);
            }

            if (!await Lock(roots)) {
                return false;
            }
            if (player.GetControlledCritter() != controlledCr || player.GetViewMapTarget() != viewMap) {
                return false;
            }
            if (controlledCr == null) {
                return true;
            }

            Critter cr = controlledCr;
            Map? map = cr.GetMap();
            if (map != null) {
                List<Entity> mapScope = new List<Entity>(roots);
                if (!mapScope.Contains(map)) {
                    mapScope.Add(map);
                }
                if (!await Lock(mapScope)) {
                    return false;
                }
                if (player.GetControlledCritter() != cr || player.GetViewMapTarget() != viewMap || cr.MapId != map.Id) {
                    return false;
                }

                Location location = map.GetLocation();
                mapScope.Add(location);
                if (!await Lock(mapScope)) {
                    return false;
                }
                if (player.GetControlledCritter() != cr || player.GetViewMapTarget() != viewMap || cr.MapId != map.Id || map.GetLocation().Id != location.Id) {
                    return false;
                }

                Game.Verify(cr.GetPlayer() == player, "Authoritative player's controlled critter has an asymmetric player link", player.Id, cr.Id);
                return true;
            }
            if (cr.MapId != new ident(0)) {
                return false;
            }

            uint tripId = cr.GlobalMapTripId;
            ulong revision = 0;
            List<ident> memberIds = cr.GetGlobalMapCritterIds(ref revision);
            if (!memberIds.Contains(cr.Id)) {
                return false;
            }

            List<Entity> groupScope = new List<Entity>(roots);
            for (int i = 0; i < memberIds.Count; i++) {
                Critter? member = Game.GetCritter(memberIds[i]);
                if (member == null) {
                    return false;
                }
                if (!groupScope.Contains(member)) {
                    groupScope.Add(member);
                }
            }
            if (!await Lock(groupScope)) {
                return false;
            }
            if (player.GetControlledCritter() != cr || player.GetViewMapTarget() != viewMap || !IsGlobalMapGroupSnapshotCurrent(cr, tripId, revision, memberIds) ||
                !IsIdentMembershipCovered(memberIds)) {
                return false;
            }

            Game.Verify(cr.GetPlayer() == player, "Authoritative player's controlled critter has an asymmetric player link", player.Id, cr.Id);
            return true;
        }

        // SyncScope: replaces cover with strictRoots plus every critter and the union of all stable mapped or global initial-info graphs.
        // Lifecycle: strict - every explicit root, critter, map/location, and global-group member must be live in the final exact acquisition.
        [Async]
        public static async Task<bool> LockCrittersInitialInfoGraphs(List<Entity> strictRoots, List<Critter> critters)
        {
            List<Entity> roots = new List<Entity>(strictRoots);

            for (int i = 0; i < critters.Count; i++) {
                if (!roots.Contains(critters[i])) {
                    roots.Add(critters[i]);
                }
            }

            for (int attempt = 0; attempt < GlobalMapGroupCoverAttempts; attempt++) {
                if (!await Lock(roots)) {
                    return false;
                }

                List<Entity> mapScope = new List<Entity>(roots);
                List<Map> maps = new List<Map>();
                List<int> mapSlots = new List<int>();
                bool graphResolved = true;

                for (int i = 0; i < critters.Count; i++) {
                    Map? map = critters[i].GetMap();

                    if (map == null) {
                        mapSlots.Add(-1);

                        if (critters[i].MapId != new ident(0)) {
                            graphResolved = false;
                            break;
                        }

                        continue;
                    }

                    mapSlots.Add(maps.Count);
                    maps.Add(map);
                }

                if (!graphResolved) {
                    await Game.YieldAsync(0);
                    continue;
                }

                for (int i = 0; i < maps.Count; i++) {
                    if (!mapScope.Contains(maps[i])) {
                        mapScope.Add(maps[i]);
                    }
                }

                if (!await Lock(mapScope)) {
                    await Game.YieldAsync(0);
                    continue;
                }

                for (int i = 0; i < critters.Count; i++) {
                    int mapSlot = mapSlots[i];
                    ident expectedMapId = mapSlot >= 0 ? maps[mapSlot].Id : new ident(0);

                    if (critters[i].MapId != expectedMapId) {
                        graphResolved = false;
                        break;
                    }
                }

                if (!graphResolved) {
                    await Game.YieldAsync(0);
                    continue;
                }

                List<Entity> scope = new List<Entity>(mapScope);
                List<uint> tripIds = new List<uint>();
                List<ulong> revisions = new List<ulong>();
                List<List<ident>> groupMemberIds = new List<List<ident>>();

                // Resolve each map's owning location by iterating the maps subset directly: the canonical
                // index keeps every location read provably inside the freshly locked map scope.
                List<Location> mapLocations = new List<Location>();

                for (int i = 0; i < maps.Count; i++) {
                    Location mapLocation = maps[i].GetLocation();
                    mapLocations.Add(mapLocation);

                    if (!scope.Contains(mapLocation)) {
                        scope.Add(mapLocation);
                    }
                }

                for (int i = 0; i < critters.Count; i++) {
                    if (mapSlots[i] >= 0) {
                        tripIds.Add(0);
                        revisions.Add(0);
                        groupMemberIds.Add(new List<ident>());
                        continue;
                    }

                    uint tripId = critters[i].GlobalMapTripId;
                    ulong revision = 0;
                    List<ident> memberIds = critters[i].GetGlobalMapCritterIds(ref revision);
                    tripIds.Add(tripId);
                    revisions.Add(revision);
                    groupMemberIds.Add(memberIds);

                    if (!memberIds.Contains(critters[i].Id)) {
                        graphResolved = false;
                        break;
                    }

                    for (int memberIndex = 0; memberIndex < memberIds.Count; memberIndex++) {
                        Critter? member = Game.GetCritter(memberIds[memberIndex]);

                        if (member == null) {
                            graphResolved = false;
                            break;
                        }
                        if (!scope.Contains(member)) {
                            scope.Add(member);
                        }
                    }

                    if (!graphResolved) {
                        break;
                    }
                }

                if (!graphResolved) {
                    await Game.YieldAsync(0);
                    continue;
                }

                // Re-assert the maps subset as the final scope mutation so the replacing lock provably
                // covers every maps[slot] handle revalidated below (runtime no-op: all maps are present).
                for (int i = 0; i < maps.Count; i++) {
                    if (!scope.Contains(maps[i])) {
                        scope.Add(maps[i]);
                    }
                }

                if (!await Lock(scope)) {
                    await Game.YieldAsync(0);
                    continue;
                }

                // Per-map revalidation first (canonical index over the proven maps subset), then the
                // per-critter map-link and global-group snapshot checks.
                for (int i = 0; i < maps.Count; i++) {
                    if (maps[i].GetLocation().Id != mapLocations[i].Id) {
                        graphResolved = false;
                        break;
                    }
                }

                if (graphResolved) {
                    for (int i = 0; i < critters.Count; i++) {
                        int mapSlot = mapSlots[i];

                        if (mapSlot >= 0) {
                            if (critters[i].MapId != maps[mapSlot].Id) {
                                graphResolved = false;
                                break;
                            }
                        }
                        else if (!IsGlobalMapGroupSnapshotCurrent(critters[i], tripIds[i], revisions[i], groupMemberIds[i]) ||
                                 !IsIdentMembershipCovered(groupMemberIds[i])) {
                            graphResolved = false;
                            break;
                        }
                    }
                }

                if (graphResolved) {
                    return true;
                }

                await Game.YieldAsync(0);
            }

            return false;
        }

        // Lifecycle: a stale item/direct holder or missing holder returns false; direct reparent races retry against the current holder.
        [Async]
        public static async Task<bool> LockItemWithHolder(Item item)
        {
            while (true) {
                if (!await Lock(item)) {
                    return false;
                }

                Entity? holder = item.FindHolder();
                if (holder == null) {
                    return false;
                }

                if (!await Lock(item, holder)) {
                    return false;
                }

                Entity? holderAfter = item.FindHolder();
                if (holderAfter == null) {
                    return false;
                }
                if (holderAfter != holder) {
                    continue;
                }

                Critter? holderCr = holder as Critter;
                if (holderCr == null) {
                    return true;
                }

                Map? map = holderCr.GetMap();
                if (map == null) {
                    return true;
                }
                if (map.IsDestroyed || map.IsDestroying) {
                    return false;
                }

                ident mapId = map.Id;
                if (!await Lock(new List<Entity> {item, holderCr, map})) {
                    continue;
                }

                if (item.Ownership != ItemOwnership.CritterInventory || item.CritterId != holderCr.Id || holderCr.MapId != mapId) {
                    continue;
                }

                return true;
            }
        }

        // An attached critter (a rider, a towed vehicle, a carried body) is only meaningful together with the
        // rest of its attachment component, and every node of that component is placed either on a map or in a
        // global-map group. These helpers acquire the whole transitive component plus each node's placement,
        // then re-prove that neither the attachment links nor the placements moved while the cover was being
        // taken — the retry loop exists because widening yields, and a yield is exactly when a rider can
        // dismount or a group can split.
        private const int GlobalMapGroupCoverAttempts = 128;

        // SyncScope: retry yields may drop incidental caller cover; success covers cr's complete stable transitive attachment component and every component node's map or global-map group.
        // Lifecycle: strict — a stale component node/placement dependency or exhausted retry budget returns false.
        [Async]
        public static async Task<bool> WidenCritterAttachmentGraph(Critter cr)
        {
            return await WidenCritterAttachmentGraphsImpl(new List<Entity>(), new List<Critter> {cr});
        }

        // SyncScope: retry yields may drop incidental caller cover; success re-proves strictRoots plus cr's complete stable transitive attachment component and all placements.
        // Lifecycle: strict — every explicit root, component node, map, and global-group member must be live in the final acquisition.
        [Async]
        public static async Task<bool> WidenCritterAttachmentGraphWithRoots(List<Entity> strictRoots, Critter cr)
        {
            return await WidenCritterAttachmentGraphsImpl(strictRoots, new List<Critter> {cr});
        }

        // SyncScope: retry yields may drop incidental caller cover; success covers the union of both complete stable transitive attachment components and all placements.
        // Lifecycle: strict — a stale component node/placement dependency or exhausted retry budget returns false.
        [Async]
        public static async Task<bool> WidenCritterAttachmentGraphs(Critter first, Critter second)
        {
            return await WidenCritterAttachmentGraphsImpl(new List<Entity>(), new List<Critter> {first, second});
        }

        // SyncScope: retry yields may drop incidental caller cover; success re-proves strictRoots plus both complete stable transitive attachment components and all placements.
        // Lifecycle: strict — every explicit root, component node, map, and global-group member must be live in the final acquisition.
        [Async]
        public static async Task<bool> WidenCritterAttachmentGraphsWithRoots(List<Entity> strictRoots, Critter first, Critter second)
        {
            return await WidenCritterAttachmentGraphsImpl(strictRoots, new List<Critter> {first, second});
        }

        // SyncScope: internal union builder for stable transitive attachment components and each node's current map or complete global-map group.
        // Lifecycle: strict — all explicit roots, discovered component nodes, and placement members must remain live through the final snapshot check.
        [Async]
        public static async Task<bool> WidenCritterAttachmentGraphsImpl(List<Entity> strictRoots, List<Critter> attachmentRoots)
        {
            List<Entity> roots = new List<Entity>(strictRoots);

            for (int i = 0; i < attachmentRoots.Count; i++) {
                if (!roots.Contains(attachmentRoots[i])) {
                    roots.Add(attachmentRoots[i]);
                }
            }

            for (int attempt = 0; attempt < GlobalMapGroupCoverAttempts; attempt++) {
                if (!await Widen(roots)) {
                    return false;
                }

                List<Critter> graph = new List<Critter>(attachmentRoots);
                bool retry = false;

                while (true) {
                    List<Entity> graphScope = new List<Entity>(roots);

                    for (int i = 0; i < graph.Count; i++) {
                        if (!graphScope.Contains(graph[i])) {
                            graphScope.Add(graph[i]);
                        }
                    }
                    if (!await Widen(graphScope)) {
                        retry = true;
                        break;
                    }

                    // Two-phase expansion: read every covered node first, collect discoveries separately,
                    // and append them to the component only after the reads. The component list therefore
                    // stays a proven subset of the widened scope for the whole read pass.
                    List<Critter> discovered = new List<Critter>();
                    int graphLength = graph.Count;

                    for (int i = 0; i < graphLength; i++) {
                        Critter node = graph[i];

                        if (node.IsAttached) {
                            Critter? master = Game.GetCritter(node.AttachMaster);

                            if (master == null) {
                                return false;
                            }
                            if (!graph.Contains(master) && !discovered.Contains(master)) {
                                discovered.Add(master);
                            }
                        }

                        List<Critter> children = node.GetAttachedCritters();

                        for (int childIndex = 0; childIndex < children.Count; childIndex++) {
                            if (!graph.Contains(children[childIndex]) && !discovered.Contains(children[childIndex])) {
                                discovered.Add(children[childIndex]);
                            }
                        }
                    }

                    if (discovered.Count == 0) {
                        break;
                    }

                    for (int i = 0; i < discovered.Count; i++) {
                        graph.Add(discovered[i]);
                    }
                }

                if (retry) {
                    await Game.YieldAsync(0);
                    continue;
                }

                List<ident> masterIds = new List<ident>();
                List<List<ident>> childIds = new List<List<ident>>();
                List<Map> placementMaps = new List<Map>();
                List<int> mapSlots = new List<int>();
                List<uint> tripIds = new List<uint>();
                List<ulong> revisions = new List<ulong>();
                List<List<ident>> groupMemberIds = new List<List<ident>>();
                List<Entity> scope = new List<Entity>(roots);

                for (int i = 0; i < graph.Count; i++) {
                    Critter node = graph[i];

                    if (!scope.Contains(node)) {
                        scope.Add(node);
                    }

                    masterIds.Add(node.IsAttached ? node.AttachMaster : new ident(0));
                    List<Critter> children = node.GetAttachedCritters();
                    List<ident> nodeChildIds = new List<ident>();

                    for (int childIndex = 0; childIndex < children.Count; childIndex++) {
                        nodeChildIds.Add(children[childIndex].Id);
                    }

                    childIds.Add(nodeChildIds);

                    Map? map = node.GetMap();

                    if (map != null) {
                        mapSlots.Add(placementMaps.Count);
                        placementMaps.Add(map);
                        tripIds.Add(0);
                        revisions.Add(0);
                        groupMemberIds.Add(new List<ident>());

                        if (!scope.Contains(map)) {
                            scope.Add(map);
                        }

                        continue;
                    }

                    if (node.MapId != new ident(0)) {
                        retry = true;
                        break;
                    }

                    mapSlots.Add(-1);
                    uint tripId = node.GlobalMapTripId;
                    ulong revision = 0;
                    List<ident> memberIds = new List<ident>();

                    if (tripId != 0) {
                        memberIds = node.GetGlobalMapCritterIds(ref revision);

                        if (!memberIds.Contains(node.Id)) {
                            return false;
                        }

                        for (int memberIndex = 0; memberIndex < memberIds.Count; memberIndex++) {
                            Critter? member = Game.GetCritter(memberIds[memberIndex]);

                            if (member == null) {
                                retry = true;
                                break;
                            }
                            if (!scope.Contains(member)) {
                                scope.Add(member);
                            }
                        }
                        if (retry) {
                            break;
                        }
                    }

                    tripIds.Add(tripId);
                    revisions.Add(revision);
                    groupMemberIds.Add(memberIds);
                }

                if (retry || !await Widen(scope)) {
                    await Game.YieldAsync(0);
                    continue;
                }

                for (int i = 0; i < graph.Count; i++) {
                    Critter node = graph[i];
                    ident currentMasterId = node.IsAttached ? node.AttachMaster : new ident(0);

                    if (currentMasterId != masterIds[i]) {
                        retry = true;
                        break;
                    }

                    List<Critter> children = node.GetAttachedCritters();
                    List<ident> currentChildIds = new List<ident>();

                    for (int childIndex = 0; childIndex < children.Count; childIndex++) {
                        currentChildIds.Add(children[childIndex].Id);
                    }

                    if (!HasSameIdentMembership(childIds[i], currentChildIds)) {
                        retry = true;
                        break;
                    }

                    int mapSlot = mapSlots[i];

                    if (mapSlot >= 0) {
                        Map? currentMap = node.GetMap();

                        if (currentMap == null || currentMap != placementMaps[mapSlot] || node.MapId != placementMaps[mapSlot].Id) {
                            retry = true;
                            break;
                        }
                    }
                    else if (tripIds[i] == 0) {
                        if (node.MapId != new ident(0) || node.GlobalMapTripId != 0) {
                            retry = true;
                            break;
                        }
                    }
                    else if (!IsGlobalMapGroupSnapshotCurrent(node, tripIds[i], revisions[i], groupMemberIds[i]) || !IsIdentMembershipCovered(groupMemberIds[i])) {
                        retry = true;
                        break;
                    }
                }

                if (!retry) {
                    return true;
                }

                await Game.YieldAsync(0);
            }

            return false;
        }

        // SyncScope: checks that every id from a previously stabilized native membership snapshot resolves inside the current cover.
        // Lifecycle: inspection-only — a missing handle or uncovered member returns false; the preceding strict widen proves covered-member liveness.
        public static bool IsIdentMembershipCovered(List<ident> memberIds)
        {
            for (int i = 0; i < memberIds.Count; i++) {
                Critter? member = Game.GetCritter(memberIds[i]);

                if (member == null || !IsCovered(member)) {
                    return false;
                }
            }

            return true;
        }

        // SyncScope: compares two native membership snapshots by immutable entity id without changing the current cover.
        // Lifecycle: scalar-only — does not resolve or access entity handles.
        public static bool HasSameIdentMembership(List<ident> first, List<ident> second)
        {
            if (first.Count != second.Count) {
                return false;
            }

            for (int i = 0; i < first.Count; i++) {
                if (!second.Contains(first[i])) {
                    return false;
                }
            }

            return true;
        }

        // SyncScope: compares a covered global-map critter with a previously captured native membership snapshot.
        // Lifecycle: inspection-only — the caller must hold cr while this function reads its group state.
        public static bool IsGlobalMapGroupSnapshotCurrent(Critter cr, uint tripId, ulong revision, List<ident> memberIds)
        {
            if (cr.MapId != new ident(0) || cr.GlobalMapTripId != tripId) {
                return false;
            }

            ulong currentRevision = 0;
            List<ident> currentMemberIds = cr.GetGlobalMapCritterIds(ref currentRevision);
            return currentRevision == revision && HasSameIdentMembership(memberIds, currentMemberIds);
        }

        // Destroying an entity touches more than the entity itself: the native preflight walks the target's
        // descendants and validates its immediate holder/parent upward, and map/location destruction notifies
        // every independent spectator Player. These helpers acquire that whole package and then re-prove the
        // relationships they snapshotted, so a caller can destroy without racing a reparent, a map migration or
        // a spectator join/leave. Each retries a bounded number of times and returns false once the budget is
        // spent, rather than looping forever against a target that keeps moving.
        // Public because callers that build their own cover ladder around these helpers bound their retries by
        // the same budget (see Lockers.WidenLockerInteractionEntities), matching the AngelScript original.
        public const int ItemDestroyGraphCoverAttempts = 128;
        public const int MapDestroyGraphCoverAttempts = 128;

        // SyncScope: retry yields may drop incidental caller cover; success covers cr plus its source map, complete stable global group, or only cr while still parentless.
        // Lifecycle: strict — a stale critter/placement dependency or exhausted global-group retry budget returns false.
        [Async]
        public static async Task<bool> WidenCritterForDestroy(Critter cr)
        {
            return await WidenCritterAttachmentGraphWithRoots(new List<Entity>(), cr);
        }

        // SyncScope: retry yields may drop incidental caller cover; success re-proves strictRoots + cr and its source map, complete stable global group, or parentless own lock.
        // Lifecycle: strict — a stale explicit root/cr/placement dependency or exhausted global-group retry budget returns false.
        [Async]
        public static async Task<bool> WidenCritterForDestroy(List<Entity> strictRoots, Critter cr)
        {
            return await WidenCritterAttachmentGraphWithRoots(strictRoots, cr);
        }

        // SyncScope: retry yields may drop incidental caller cover; success covers cr + its stable source map or global group and globalCr + every stable target-group member.
        // Lifecycle: strict — a stale dependency or exhausted retry budget returns false.
        [Async]
        public static Task<bool> WidenForTransferToGlobalGroup(Critter cr, Critter globalCr) =>
            WidenForTransferToGlobalGroup(new List<Entity>(), cr, globalCr);

        // SyncScope: retry yields may drop incidental caller cover; snapshots both graphs under strictRoots + cr + globalCr and returns only after one final union acquisition still matches them.
        // Lifecycle: strict — every explicit root, source dependency, target-group member, and final union member must be live.
        [Async]
        public static async Task<bool> WidenForTransferToGlobalGroup(List<Entity> strictRoots, Critter cr, Critter globalCr)
        {
            List<Entity> roots = new List<Entity>(strictRoots);
            if (!roots.Contains(cr)) {
                roots.Add(cr);
            }
            if (!roots.Contains(globalCr)) {
                roots.Add(globalCr);
            }

            for (int attempt = 0; attempt < GlobalMapGroupCoverAttempts; attempt++) {
                if (!await Widen(roots)) {
                    return false;
                }
                if (globalCr.MapId.value != 0) {
                    return false;
                }

                uint targetTripId = globalCr.GlobalMapTripId;
                ulong targetRevision = 0;
                List<ident> targetMemberIds = globalCr.GetGlobalMapCritterIds(ref targetRevision);
                if (!targetMemberIds.Contains(globalCr.Id)) {
                    return false;
                }

                List<Critter> targetMembers = new List<Critter>();
                bool allMembersResolved = true;
                for (int i = 0; i < targetMemberIds.Count; i++) {
                    Critter? member = Game.GetCritter(targetMemberIds[i]);
                    if (member == null) {
                        allMembersResolved = false;
                        break;
                    }
                    targetMembers.Add(member);
                }

                Map? sourceMap = cr.GetMap();
                ident sourceMapId = sourceMap != null ? sourceMap.Id : new ident(0);
                uint sourceTripId = sourceMap == null ? cr.GlobalMapTripId : 0;
                ulong sourceRevision = 0;
                List<ident> sourceMemberIds = new List<ident>();
                List<Critter> sourceMembers = new List<Critter>();

                if (sourceMap == null) {
                    sourceMemberIds = cr.GetGlobalMapCritterIds(ref sourceRevision);
                    if (!sourceMemberIds.Contains(cr.Id)) {
                        return false;
                    }
                    for (int i = 0; i < sourceMemberIds.Count; i++) {
                        Critter? member = Game.GetCritter(sourceMemberIds[i]);
                        if (member == null) {
                            allMembersResolved = false;
                            break;
                        }
                        sourceMembers.Add(member);
                    }
                }

                bool targetChanged = !IsGlobalMapGroupSnapshotCurrent(globalCr, targetTripId, targetRevision, targetMemberIds);
                bool sourceChanged = sourceMap != null ? cr.MapId != sourceMapId : !IsGlobalMapGroupSnapshotCurrent(cr, sourceTripId, sourceRevision, sourceMemberIds);

                if (!allMembersResolved) {
                    if (targetChanged || sourceChanged) {
                        await Game.YieldAsync(0);
                        continue;
                    }
                    return false;
                }

                List<Entity> scope = new List<Entity>(roots);
                for (int i = 0; i < targetMembers.Count; i++) {
                    if (!scope.Contains(targetMembers[i])) {
                        scope.Add(targetMembers[i]);
                    }
                }
                if (sourceMap != null) {
                    if (!scope.Contains(sourceMap)) {
                        scope.Add(sourceMap);
                    }
                }
                else {
                    for (int i = 0; i < sourceMembers.Count; i++) {
                        if (!scope.Contains(sourceMembers[i])) {
                            scope.Add(sourceMembers[i]);
                        }
                    }
                }

                if (!await Widen(scope)) {
                    if (!await Widen(roots)) {
                        return false;
                    }
                    targetChanged = !IsGlobalMapGroupSnapshotCurrent(globalCr, targetTripId, targetRevision, targetMemberIds);
                    sourceChanged = sourceMap != null ? cr.MapId != sourceMapId : !IsGlobalMapGroupSnapshotCurrent(cr, sourceTripId, sourceRevision, sourceMemberIds);
                    if (targetChanged || sourceChanged) {
                        await Game.YieldAsync(0);
                        continue;
                    }
                    return false;
                }

                targetChanged = !IsGlobalMapGroupSnapshotCurrent(globalCr, targetTripId, targetRevision, targetMemberIds);
                sourceChanged = sourceMap != null ? cr.MapId != sourceMapId : !IsGlobalMapGroupSnapshotCurrent(cr, sourceTripId, sourceRevision, sourceMemberIds);
                if (targetChanged || sourceChanged) {
                    await Game.YieldAsync(0);
                    continue;
                }
                if (!IsIdentMembershipCovered(targetMemberIds) || (sourceMap == null && !IsIdentMembershipCovered(sourceMemberIds))) {
                    return false;
                }

                return true;
            }

            return false;
        }

        // SyncScope: replaces the caller cover with map's complete stable destroy graph: map + parent location + every independent spectator Player.
        // Lifecycle: strict — a stale dependency or exhausted map/location/spectator membership retry budget returns false.
        [Async]
        public static async Task<bool> LockMapForDestroy(Map map)
        {
            if (!await Lock(map)) {
                return false;
            }

            return await WidenMapForDestroy(map);
        }

        // SyncScope: retry yields may drop incidental caller cover; success covers map + current location + every independent spectator Player, while map ancestry covers its descendants.
        // Lifecycle: strict — a stale dependency or exhausted map/location/spectator membership retry budget returns false.
        [Async]
        public static async Task<bool> WidenMapForDestroy(Map map)
        {
            List<Entity> roots = new List<Entity> {map};

            for (int attempt = 0; attempt < MapDestroyGraphCoverAttempts; attempt++) {
                if (!await Widen(roots)) {
                    return false;
                }

                Location location = map.GetLocation();
                ident locationId = location.Id;
                List<Entity> treeScope = new List<Entity> {map, location};

                if (!await Widen(treeScope)) {
                    if (!await Widen(roots)) {
                        return false;
                    }
                    if (map.GetLocation().Id != locationId) {
                        await Game.YieldAsync(0);
                        continue;
                    }

                    return false;
                }
                if (map.GetLocation().Id != locationId) {
                    await Game.YieldAsync(0);
                    continue;
                }

                List<Player> spectators = map.GetSpectatorPlayers();
                List<Entity> scope = new List<Entity>(treeScope);

                for (int i = 0; i < spectators.Count; i++) {
                    if (!scope.Contains(spectators[i])) {
                        scope.Add(spectators[i]);
                    }
                }

                if (!await Widen(scope)) {
                    if (!await Widen(treeScope)) {
                        return false;
                    }
                    if (map.GetLocation().Id != locationId || !HasSamePlayerMembership(spectators, map.GetSpectatorPlayers())) {
                        await Game.YieldAsync(0);
                        continue;
                    }

                    return false;
                }

                if (map.GetLocation().Id == locationId && HasSamePlayerMembership(spectators, map.GetSpectatorPlayers())) {
                    return true;
                }

                await Game.YieldAsync(0);
            }

            return false;
        }

        // SyncScope: replaces the caller cover with location's complete stable destroy graph: the location tree + every independent spectator Player on its maps.
        // Lifecycle: strict — a stale dependency or exhausted map/spectator membership retry budget returns false.
        [Async]
        public static async Task<bool> LockLocationForDestroy(Location location)
        {
            if (!await Lock(location)) {
                return false;
            }

            return await WidenLocationForDestroy(location);
        }

        // SyncScope: retry yields may drop incidental caller cover; success covers location + every independent spectator Player from current child maps, while location ancestry covers descendants.
        // Lifecycle: strict — a stale dependency or exhausted map/spectator membership retry budget returns false.
        [Async]
        public static async Task<bool> WidenLocationForDestroy(Location location)
        {
            List<Entity> roots = new List<Entity> {location};

            for (int attempt = 0; attempt < MapDestroyGraphCoverAttempts; attempt++) {
                if (!await Widen(roots)) {
                    return false;
                }

                List<Map> maps = location.GetMaps();
                List<List<Player>> spectatorSnapshots = new List<List<Player>>(maps.Count);
                List<Entity> scope = new List<Entity>(roots);

                for (int mapIndex = 0; mapIndex < maps.Count; mapIndex++) {
                    List<Player> spectators = maps[mapIndex].GetSpectatorPlayers();
                    spectatorSnapshots.Add(spectators);

                    for (int playerIndex = 0; playerIndex < spectators.Count; playerIndex++) {
                        if (!scope.Contains(spectators[playerIndex])) {
                            scope.Add(spectators[playerIndex]);
                        }
                    }
                }

                if (!await Widen(scope)) {
                    if (!await Widen(roots)) {
                        return false;
                    }
                    if (!IsLocationDestroySnapshotCurrent(location, maps, spectatorSnapshots)) {
                        await Game.YieldAsync(0);
                        continue;
                    }

                    return false;
                }

                if (IsLocationDestroySnapshotCurrent(location, maps, spectatorSnapshots)) {
                    return true;
                }

                await Game.YieldAsync(0);
            }

            return false;
        }

        // SyncScope: verifies that a covered location still owns the same maps and that every map has the same independent spectator membership.
        // Lifecycle: inspection-only — callers hold the location and the spectator snapshot while checking it.
        public static bool IsLocationDestroySnapshotCurrent(Location location, List<Map> maps, List<List<Player>> spectatorSnapshots)
        {
            List<Map> currentMaps = location.GetMaps();

            if (!HasSameMapMembership(maps, currentMaps)) {
                return false;
            }

            for (int mapIndex = 0; mapIndex < currentMaps.Count; mapIndex++) {
                int snapshotIndex = maps.IndexOf(currentMaps[mapIndex]);

                if (snapshotIndex < 0 || !HasSamePlayerMembership(spectatorSnapshots[snapshotIndex], currentMaps[mapIndex].GetSpectatorPlayers())) {
                    return false;
                }
            }

            return true;
        }

        // SyncScope: compares two covered map membership snapshots by immutable entity identity without changing cover.
        // Lifecycle: inspection-only — the owning location remains covered while the snapshots are compared.
        public static bool HasSameMapMembership(List<Map> first, List<Map> second)
        {
            if (first.Count != second.Count) {
                return false;
            }

            for (int i = 0; i < first.Count; i++) {
                if (!second.Contains(first[i])) {
                    return false;
                }
            }

            return true;
        }

        // SyncScope: compares two owning spectator snapshots by immutable Player identity without changing cover.
        // Lifecycle: inspection-only — no Player property is read, so a removed snapshot member may remain as a retained owning handle.
        public static bool HasSamePlayerMembership(List<Player> first, List<Player> second)
        {
            if (first.Count != second.Count) {
                return false;
            }

            for (int i = 0; i < first.Count; i++) {
                if (!second.Contains(first[i])) {
                    return false;
                }
            }

            return true;
        }

        // SyncScope: retry yields may drop incidental caller cover; success covers item + stable immediate holder, while the root lock covers its nested subtree by ancestry.
        // Lifecycle: strict — a stale root or owned item with a stale/unresolvable direct holder returns false; a parentless root succeeds and direct reparent races are retried.
        [Async]
        public static async Task<bool> WidenItemForDestroy(Item item)
        {
            return await WidenItemsForDestroy(new List<Entity>(), new List<Item> {item});
        }

        // SyncScope: retry yields may drop incidental caller cover; success re-proves strictRoots plus item and its stable immediate holder.
        // Lifecycle: strict — every explicit root, item, and current direct holder must remain live through the final relationship read.
        [Async]
        public static async Task<bool> WidenItemForDestroy(List<Entity> strictRoots, Item item)
        {
            return await WidenItemsForDestroy(strictRoots, new List<Item> {item});
        }

        // SyncScope: retry yields may drop incidental caller cover; success covers every root item + the union of stable immediate holders, with each nested subtree covered by ancestry.
        // Lifecycle: strict — any stale root or owned item with a stale/unresolvable direct holder returns false; parentless roots succeed, duplicates are deduplicated, and direct reparent races are retried.
        [Async]
        public static async Task<bool> WidenItemsForDestroy(List<Item> items)
        {
            return await WidenItemsForDestroy(new List<Entity>(), items);
        }

        // SyncScope: retry yields may drop incidental caller cover; every attempt re-proves strictRoots, and success also covers every root item + stable immediate holder.
        // Lifecycle: strict — every explicit root and current direct holder of an owned item must remain live through the final relationship read; parentless roots need no holder.
        [Async]
        public static async Task<bool> WidenItemsForDestroy(List<Entity> strictRoots, List<Item> items)
        {
            if (items.Count == 0) {
                return await Widen(strictRoots);
            }

            List<Item> roots = new List<Item>();

            for (int i = 0; i < items.Count; i++) {
                if (!roots.Contains(items[i])) {
                    roots.Add(items[i]);
                }
            }

            List<Entity> baseScope = new List<Entity>(strictRoots);

            for (int i = 0; i < roots.Count; i++) {
                if (!baseScope.Contains(roots[i])) {
                    baseScope.Add(roots[i]);
                }
            }

            List<Entity> scope = new List<Entity>(baseScope);

            for (int attempt = 0; attempt < ItemDestroyGraphCoverAttempts; attempt++) {
                if (!await Widen(scope)) {
                    scope = new List<Entity>(baseScope);

                    if (!await Widen(scope)) {
                        return false;
                    }
                }

                List<Entity> currentScope = new List<Entity>(baseScope);
                bool needsWiden = false;

                for (int i = 0; i < roots.Count && !needsWiden; i++) {
                    Item root = roots[i];

                    if (root.Ownership == ItemOwnership.Nowhere) {
                        continue;
                    }

                    Entity? holder = root.FindHolder();

                    if (holder == null) {
                        return false;
                    }
                    if (!currentScope.Contains(holder)) {
                        currentScope.Add(holder);
                    }
                    if (!scope.Contains(holder)) {
                        needsWiden = true;
                    }
                }

                if (needsWiden) {
                    scope = currentScope;
                    // Managed void callbacks cannot synchronously wait for YieldAsync without blocking the
                    // script pump that completes it. The next Widen performs the required lock transition,
                    // so retry immediately after expanding the requested scope.
                    continue;
                }

                // Native preflight recurses downward through every inner item/entity but validates only the
                // root's immediate holder upward. Removing a root from an item container does not touch that
                // container's own holder, and critter broadcasts use mutex-owned spectator snapshots, so no
                // transitive container chain or holder-critter map belongs in this caller package.
                return true;
            }

            return false;
        }

        // SyncScope: retry yields may drop incidental caller cover; success covers cr plus every current matching direct inventory-item destroy graph and verifies membership stability.
        // Lifecycle: strict — a stale critter/item graph or exhausted retry budget returns false; an empty matching set succeeds with cr explicitly covered.
        [Async]
        public static async Task<bool> WidenCritterItemsForDestroy(Critter cr, hstring protoId)
        {
            return await WidenCritterItemsForDestroy(new List<Entity>(), cr, new List<hstring> {protoId});
        }

        // SyncScope: multi-proto convenience overload; leaves cr and every current matching stable inventory-item destroy graph covered.
        // Lifecycle: strict — identical to the strict-root multi-proto overload.
        [Async]
        public static async Task<bool> WidenCritterItemsForDestroy(Critter cr, List<hstring> protoIds)
        {
            return await WidenCritterItemsForDestroy(new List<Entity>(), cr, protoIds);
        }

        // SyncScope: retry yields may drop incidental caller cover; success re-proves strictRoots + cr and every current matching direct inventory-item destroy graph.
        // Lifecycle: strict — a stale explicit root/cr/item graph or exhausted retry budget returns false; an empty matching set succeeds with every root explicitly covered.
        [Async]
        public static async Task<bool> WidenCritterItemsForDestroy(List<Entity> strictRoots, Critter cr, hstring protoId)
        {
            return await WidenCritterItemsForDestroy(strictRoots, cr, new List<hstring> {protoId});
        }

        // SyncScope: retry yields may drop incidental caller cover; success re-proves strictRoots + cr and every current inventory-item destroy graph matching any requested proto.
        // Lifecycle: strict — a stale explicit root/cr/item graph or exhausted retry budget returns false; an empty matching set succeeds with every root explicitly covered.
        [Async]
        public static async Task<bool> WidenCritterItemsForDestroy(List<Entity> strictRoots, Critter cr, List<hstring> protoIds)
        {
            List<Entity> roots = new List<Entity>(strictRoots);

            if (!roots.Contains(cr)) {
                roots.Add(cr);
            }

            for (int attempt = 0; attempt < ItemDestroyGraphCoverAttempts; attempt++) {
                if (!await Widen(roots)) {
                    return false;
                }

                List<Item> items = CollectItemsByProtos(cr, protoIds);

                if (!await WidenItemsForDestroy(roots, items)) {
                    return false;
                }

                List<Item> currentItems = CollectItemsByProtos(cr, protoIds);

                if (HasSameItemMembership(items, currentItems)) {
                    return true;
                }

                await Game.YieldAsync(0);
            }

            return false;
        }

        // SyncScope: ProtoItem convenience overload for WidenCritterItemsForDestroy; leaves cr and every matching stable item destroy graph covered.
        // Lifecycle: strict — identical to the hstring overload.
        [Async]
        public static async Task<bool> WidenCritterItemsForDestroy(Critter cr, ProtoItem proto)
        {
            return await WidenCritterItemsForDestroy(cr, proto.ProtoId);
        }

        // SyncScope: strict-root ProtoItem convenience overload; leaves every explicit root, cr and each matching stable item destroy graph covered.
        // Lifecycle: strict — identical to the strict-root hstring overload.
        [Async]
        public static async Task<bool> WidenCritterItemsForDestroy(List<Entity> strictRoots, Critter cr, ProtoItem proto)
        {
            return await WidenCritterItemsForDestroy(strictRoots, cr, proto.ProtoId);
        }

        // SyncScope: compares two covered item snapshots by immutable entity identity without changing cover.
        // Lifecycle: inspection-only — callers keep the owning holder locked while relying on membership stability.
        public static bool HasSameItemMembership(List<Item> first, List<Item> second)
        {
            if (first.Count != second.Count) {
                return false;
            }

            for (int i = 0; i < first.Count; i++) {
                if (!second.Contains(first[i])) {
                    return false;
                }
            }

            return true;
        }

        // Deduplicated union of cr's direct inventory items matching any of the requested protos. Read twice per
        // attempt in WidenCritterItemsForDestroy — once to build the cover, once to prove the set did not move.
        private static List<Item> CollectItemsByProtos(Critter cr, List<hstring> protoIds)
        {
            List<Item> items = new List<Item>();

            for (int i = 0; i < protoIds.Count; i++) {
                List<Item> matchingItems = cr.GetItems(protoIds[i]);

                for (int j = 0; j < matchingItems.Count; j++) {
                    if (!items.Contains(matchingItems[j])) {
                        items.Add(matchingItems[j]);
                    }
                }
            }

            return items;
        }

#endif
    }
}
