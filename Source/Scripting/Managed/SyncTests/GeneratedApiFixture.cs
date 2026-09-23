namespace FOnline;

using System;
using System.Collections.Generic;
using System.Threading.Tasks;

public readonly record struct ident(uint value);
public readonly record struct hstring(string Value);
public enum ItemOwnership
{
    Nowhere,
    CritterInventory,
    MapHex,
    ItemContainer
}
public enum CritterFindType
{
    Any,
    Players
}

public class Entity
{
    public ident Id { get; init; }
    public bool IsDestroyed { get; set; }
    public bool IsDestroying { get; set; }
    public bool IsAlwaysCovered { get; init; }
}

public class Critter : Entity
{
    public Map? Map { get; set; }
    public ident? MapIdOverride { get; set; }
    public ident MapId => MapIdOverride ?? Map?.Id ?? new ident(0);
    public uint GlobalMapTripId { get; set; }
    public bool IsAttached { get; set; }
    public ident AttachMaster { get; set; }
    public Player? Player { get; set; }
    public List<ident> Members { get; set; } = new();
    public List<Critter> Attached { get; set; } = new();
    public Map? GetMap() => Map;
    public Player? GetPlayer() => Player;
    public List<ident> GetGlobalMapCritterIds(ref ulong revision) => new(Members);
    public List<Critter> GetAttachedCritters() => new(Attached);
    public Func<hstring, List<Item>>? OnGetItems { get; set; }
    public List<Item> GetItems(hstring protoId) => OnGetItems?.Invoke(protoId) ?? throw new NotSupportedException();
}

public class Player : Entity
{
    public Critter? ControlledCritter { get; set; }
    public Critter? GetControlledCritter() => ControlledCritter;
    public Map? GetViewMapTarget() => null;
}

public class Map : Entity
{
    public Location Location { get; set; } = new();
    public Location GetLocation() => Location;
    public List<Player> GetSpectatorPlayers() => new();
    public List<Critter> GetCritters(CritterFindType find) => throw new NotSupportedException();
}

public class Location : Entity
{
    public List<Map> GetMaps() => new();
}

public class Item : Entity
{
    public Entity? Holder { get; set; }
    public Entity? FindHolder() => Holder;
    public ItemOwnership Ownership { get; set; }
    public ident CritterId { get; set; }
}

public class ProtoItem : Entity
{
    public hstring ProtoId { get; set; }
}

public static class Game
{
    public static List<Entity> Held { get; } = new();
    public static Action? OnAcquire { get; set; }
    public static bool IsEntityLocked(Entity entity) => Held.Contains(entity);
    public static List<Entity> GetHeldSyncEntities() => new(Held);
    public static Critter? GetCritter(ident id) => null;
    public static void Sync(Entity entity) => Sync(new List<Entity> { entity });
    public static void Sync(List<Entity> entities)
    {
        Held.Clear();
        SyncWiden(entities);
    }
    public static void SyncWiden(List<Entity> entities)
    {
        Held.AddRange(entities);
        OnAcquire?.Invoke();
    }
    public static void SyncRelease() => Held.Clear();
}

public static class ScriptTask
{
    public static Func<Task>? OnDelay { get; set; }
    public static Task Delay(int delay) => OnDelay?.Invoke() ?? Task.CompletedTask;
}

internal static class Native
{
    internal static List<Exception> Exceptions { get; } = new();
    internal static void ReportException(Exception exception) => Exceptions.Add(exception);
}

public static class Invariant
{
    public static void Verify(bool condition, string message, params object[] context)
    {
        if (!condition) {
            throw new InvalidOperationException(message);
        }
    }
}
