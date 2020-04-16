using FOnline;
using System;

namespace TheGame
{
    public static class Test
    {
        static void Main()
        {
#if SERVER
            Console.WriteLine("SERVER");
#elif CLIENT
            Console.WriteLine("CLIENT");
#elif SINGLE
            Console.WriteLine("SINGLE");
#elif MAPPER
            Console.WriteLine("MAPPER");
#else
            Console.WriteLine("unknown");
#endif
        }
    }
}
