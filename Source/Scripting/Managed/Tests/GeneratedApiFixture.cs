namespace FOnline;

using System;

// The fixture records registration and fallback calls; unrelated native operations fail explicitly
internal static class Native
{
    public static readonly System.Collections.Generic.List<string> RegisteredFunctions = new();
    public static readonly System.Collections.Generic.List<string> RegisteredRemoteCalls = new();
    public static void RegisterGlobalScriptFunc(string name, string attribute, string[] parameters, string result,
                                                Delegate handler) => RegisteredFunctions.Add(name);
    public static void RegisterRemoteCallHandler(string name, int parameters,
                                                 Delegate handler) => RegisteredRemoteCalls.Add(name);
    public static int FallbackCalls;
    public static bool InvokeScriptFunc(string name, object?[] args)
    {
        FallbackCalls++;
        return false;
    }
    public static void Log(string text)
    {
    }
    public static object GetProperty(string owner, string property, IntPtr entity) => throw new NotSupportedException();
    public static void SetProperty(string owner, string property, IntPtr entity,
                                   object value) => throw new NotSupportedException();
    public static short HdirToMdir(sbyte value) => throw new NotSupportedException();
    public static sbyte MdirHex(short value) => throw new NotSupportedException();
    public static short MdirRotateHex(short value, int steps) => throw new NotSupportedException();
    public static short MdirReverse(short value) => throw new NotSupportedException();
}

public class Critter
{
}
public static partial class Game
{
    public static void Log(string text) => Native.Log(text);
}

public enum GameProperty
{
    Value
}
public enum ModifierEvent
{
    Value
}
public enum ModifierScope
{
    Value
}
public enum CritterProperty
{
    Strength = 7
}
public enum TextPackName
{
    Game
}
public readonly struct hstring
{
    private readonly string text;
    public hstring(string text)
    {
        this.text = text;
    }
    public static hstring FromString(string text) => new hstring(text);
    public override string ToString() => text;
}

public partial struct timespan
{
    public long value;
    public timespan(long value)
    {
        this.value = value;
    }
}
public partial struct synctime
{
    public long value;
    public synctime(long value)
    {
        this.value = value;
    }
}
public partial struct nanotime
{
    public long value;
    public nanotime(long value)
    {
        this.value = value;
    }
}
public partial struct ident
{
    public long value;
}
public partial struct ucolor
{
    public uint value;
}
public static class Settings
{
    public static int Geometry_MapDirCount { get; set; } = 6;
}
public partial struct hdir
{
    public sbyte value;
}
public partial struct mdir
{
    public short angle;
}
public partial struct ipos8
{
    public sbyte x, y;
}
public partial struct ipos16
{
    public short x, y;
}
public partial struct frect
{
    public float x, y, width, height;
}
public partial struct ipos
{
    public int x, y;
    public ipos(int x, int y)
    {
        this.x = x;
        this.y = y;
    }
}
public partial struct fpos
{
    public float x, y;
    public fpos(float x, float y)
    {
        this.x = x;
        this.y = y;
    }
}
public partial struct isize
{
    public int width, height;
}
public partial struct fsize
{
    public float width, height;
}
public partial struct irect
{
    public int x, y, width, height;
}
public partial struct mpos
{
    public short x, y;
}
public partial struct msize
{
    public short width, height;
}
public partial struct TextPackKey
{
    public TextPackName Collection;
    public hstring Key1, Key2, Key3;
    public TextPackKey(TextPackName collection, hstring key1, hstring key2, hstring key3)
    {
        Collection = collection;
        Key1 = key1;
        Key2 = key2;
        Key3 = key3;
    }
}
