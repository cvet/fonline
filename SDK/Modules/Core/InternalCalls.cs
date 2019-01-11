using System.Runtime.CompilerServices;

namespace FOnlineEngine
{
    [NotImplemented]
    internal static class InternalCalls
    {
#if SERVER
        [MethodImpl(MethodImplOptions.InternalCall)]
        extern public static void SaveVariable(long entityId, string varName, byte[] data);
        [MethodImpl(MethodImplOptions.InternalCall)]
        extern public static void SendVariable(long entityId, string varName, bool toAll, byte[] data);
#elif CLIENT
        [MethodImpl(MethodImplOptions.InternalCall)]
        extern public static void SendModifiableVariable(long entityId, string varName, byte[] data);
        [MethodImpl(MethodImplOptions.InternalCall)]
        extern public static Critter GetChosen();
        [MethodImpl(MethodImplOptions.InternalCall)]
        extern public static long GetChosenId();
#endif
    }
}
