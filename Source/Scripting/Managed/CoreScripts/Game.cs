namespace FOnline;

using System;

// The hand-written part of the Game entity API; everything else on `Game` is generated per target
public static partial class Game
{
    public static int GetAsInt(GameProperty prop)
    {
        return Native.GetEntityValueAsInt(IntPtr.Zero, (int)prop);
    }

    public static void SetAsInt(GameProperty prop, int value)
    {
        Native.SetEntityValueAsInt(IntPtr.Zero, (int)prop, value);
    }
}
