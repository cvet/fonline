namespace FOnline;

using System;
using System.Globalization;

// The hand-written part of the Game entity API; everything else on `Game` is generated per target
public static partial class Game
{
    public static int GetAsInt(GameProperty prop)
    {
        return Convert.ToInt32(Native.GetProperty("Game", prop.ToString(), IntPtr.Zero), CultureInfo.InvariantCulture);
    }

    public static void SetAsInt(GameProperty prop, int value)
    {
        Native.SetProperty("Game", prop.ToString(), IntPtr.Zero, value);
    }
}
