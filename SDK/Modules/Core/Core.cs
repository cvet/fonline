using System;
using System.Runtime.CompilerServices;

namespace FOnlineEngine
{
    [AttributeUsage(AttributeTargets.All)]
    public class NotImplementedAttribute : Attribute
    {
    }

    public struct Hash
    {
    }

    public static class Engine
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        extern public static void Log(string message);
    }
}
