using System;
using System.Reflection;

namespace FOnline
{
    public class Entity { }
    public class Item : Entity { }
    public class Critter : Entity { }
    public class Map : Entity { }
    public class Location : Entity { }
    public class MapSprite { }

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
