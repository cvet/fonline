using System;
using System.Reflection;

namespace FOnline
{
    public static class Initializator
    {
        static void Initialize()
        {
            foreach (Assembly assembly in AppDomain.CurrentDomain.GetAssemblies())
            {
                foreach (Type type in assembly.GetTypes())
                {
                    try
                    {
                        System.Runtime.CompilerServices.RuntimeHelpers.RunClassConstructor(type.TypeHandle);
                    }
                    catch (Exception ex)
                    {
                        Game.Log("Failed to run type: " + type.Name);
                        Game.Log(ex.ToString());
                    }
                }
            }
        }
    }
}
