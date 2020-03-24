using FOnline;
using System;

namespace TheGame
{
    public static class Test
    {
        static void ModuleInit()
        {
#if SERVER
            Console.WriteLine("SERVER");
#elif CLIENT
            Console.WriteLine("CLIENT");
#elif MAPPER
            Console.WriteLine("MAPPER");
#else
            Console.WriteLine("unknown");
#endif
        }
    }
}
