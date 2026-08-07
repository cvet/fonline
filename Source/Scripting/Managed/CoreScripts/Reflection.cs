#nullable enable

namespace FOnline
{
    // Managed equivalent of the AngelScript `reflection::typeof<T>(src).instantiate(src, dst)` idiom used by the
    // engine GUI core script (Gui.fos) and game combat code: a polymorphic shallow copy. The port tool maps
    // `reflection::typeof<T>(src).instantiate(src, dst)` -> `dst = Reflection.Clone(src)` and
    // `reflection::typeof<T>(x).nameWithoutNamespace` -> `typeof(T).Name`. Clone uses the runtime type of the
    // source so a subclass (e.g. a Gui Panel/Screen cloned through an Object handle) is reconstructed correctly.
    public static class Reflection
    {
        public static T Clone<T>(T src)
            where T : notnull
        {
            System.Type type = src.GetType();
            object dst = System.Activator.CreateInstance(type)!;

            // Walk the hierarchy so inherited private fields are copied too — needed for the Gui
            // Object/Panel/Screen subclasses.
            System.Type? walk = type;
            while (walk != null && walk != typeof(object))
            {
                foreach (System.Reflection.FieldInfo field in walk.GetFields(
                    System.Reflection.BindingFlags.Instance |
                    System.Reflection.BindingFlags.Public |
                    System.Reflection.BindingFlags.NonPublic |
                    System.Reflection.BindingFlags.DeclaredOnly))
                {
                    field.SetValue(dst, field.GetValue(src));
                }

                walk = walk.BaseType;
            }

            return (T)dst;
        }
    }
}
