using System;
using System.Linq;
using System.Linq.Expressions;
using System.Reflection;
using System.Threading.Tasks;

namespace FOnline
{
    // Shared helper for the managed backend: register every static method tagged with a given marker attribute as a
    // global script function in the engine's cross-backend registry (Native.RegisterGlobalScriptFunc ->
    // ScriptSystem::FindFunc), under its `Module::Func` name and carrying the marker so HasAttribute(attributeName)
    // holds. This is how managed mirrors AngelScript attribute-bound functions that the engine resolves by name +
    // attribute — e.g. static-item triggers ([[ItemTrigger]], bound by MapManager from the item's TriggerScript
    // property). Engine marker attributes register from Initializator.InitializeEarly because maps bind triggers
    // before normal [ModuleInit] runs; project extensions (e.g. dialogs) call RegisterAttributedScriptFuncs with
    // their own attribute type + marker name.
    public static class ScriptFuncRegistration
    {
        // Engine-owned marker attributes resolved by name through the cross-backend registry. (Anim callbacks are a
        // separate mechanism — a callback handle passed to Critter.AddAnimCallback — not a name lookup, so they are
        // not registered here.)
        internal static void RegisterEngineAttributeFuncs()
        {
            RegisterAttributedScriptFuncs(typeof(ItemTriggerAttribute), "ItemTrigger");
            RegisterAttributedScriptFuncs(typeof(ItemInitAttribute), "ItemInit");
            RegisterAttributedScriptFuncs(typeof(ItemStaticAttribute), "ItemStatic");
            RegisterAttributedScriptFuncs(typeof(CritterInitAttribute), "CritterInit");
            RegisterAttributedScriptFuncs(typeof(MapInitAttribute), "MapInit");
            RegisterAttributedScriptFuncs(typeof(LocationInitAttribute), "LocationInit");
        }

        // Register all static methods in the script assembly tagged with `attributeType` as global script functions
        // carrying `attributeName`. Finds nothing on a side where the tagged methods are not compiled (harmless).
        // Normal attribute-bound funcs skip coexisting AngelScript modules to avoid duplicate live owners; transition
        // bridges may opt in when their exported method names are explicitly kept unique.
        public static void RegisterAttributedScriptFuncs(Type attributeType, string attributeName, bool includeCoexistingAngelScriptModules = false)
        {
            Assembly assembly = typeof(ScriptFuncRegistration).Assembly;

            foreach (Type type in assembly.GetTypes())
            {
                foreach (MethodInfo method in type.GetMethods(
                    BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly))
                {
                    if (Attribute.GetCustomAttribute(method, attributeType) != null)
                    {
                        if (!includeCoexistingAngelScriptModules && Initializator.HasCoexistingAngelScriptModule(type))
                        {
                            continue;
                        }

                        RegisterFunc(type, method, attributeName);
                    }
                }
            }
        }

        private static void RegisterFunc(Type type, MethodInfo method, string attributeName)
        {
            ParameterInfo[] parameters = method.GetParameters();
            string[] paramTypeNames = new string[parameters.Length];
            Type[] delegateParamTypes = new Type[parameters.Length];

            for (int i = 0; i < parameters.Length; i++)
            {
                paramTypeNames[i] = EngineTypeName(parameters[i].ParameterType);
                delegateParamTypes[i] = parameters[i].ParameterType;
            }

            string returnTypeName = EngineTypeName(GetRegisteredReturnType(method.ReturnType));

            // Build a matching Action<...>/Func<...> delegate type so the static method can be wrapped as a Delegate
            // and invoked later via Native.InvokeCallback (DynamicInvoke).
            Type delegateType = method.ReturnType == typeof(void)
                ? Expression.GetActionType(delegateParamTypes)
                : Expression.GetFuncType(delegateParamTypes.Append(method.ReturnType).ToArray());

            Delegate handler = Delegate.CreateDelegate(delegateType, method);
            string fullName = type.Name + "::" + method.Name;

            Native.RegisterGlobalScriptFunc(fullName, attributeName, paramTypeNames, returnTypeName, handler);
        }

        private static Type GetRegisteredReturnType(Type returnType)
        {
            if (returnType == typeof(Task))
            {
                return typeof(void);
            }

            if (returnType.IsGenericType && returnType.GetGenericTypeDefinition() == typeof(Task<>))
            {
                return returnType.GetGenericArguments()[0];
            }

            return returnType;
        }

        // Map a C# parameter/return type to the engine base-type name the metadata uses (see AngelScriptCall.cpp
        // resolve_type). Entity wrappers and value structs are named after the engine type, so their simple name is
        // used directly.
        public static string EngineTypeName(Type type)
        {
            if (type == typeof(void))
            {
                return "void";
            }

            if (type == typeof(bool))
            {
                return "bool";
            }

            if (type == typeof(sbyte))
            {
                return "int8";
            }

            if (type == typeof(byte))
            {
                return "uint8";
            }

            if (type == typeof(short))
            {
                return "int16";
            }

            if (type == typeof(ushort))
            {
                return "uint16";
            }

            if (type == typeof(int))
            {
                return "int32";
            }

            if (type == typeof(uint))
            {
                return "uint32";
            }

            if (type == typeof(long))
            {
                return "int64";
            }

            if (type == typeof(ulong))
            {
                return "uint64";
            }

            if (type == typeof(float))
            {
                return "float32";
            }

            if (type == typeof(double))
            {
                return "float64";
            }

            if (type == typeof(string))
            {
                return "string";
            }

            if (type == typeof(object))
            {
                return "any";
            }

            // Collections map to the engine array type name "element[]" so the registered signature matches the
            // FindFunc<...> an AngelScript array arg produces (the engine builds an Array ComplexTypeDesc for it).
            // Covers List<T> (the managed idiom) and T[]; the engine marshals both ends as the same array.
            if (type.IsArray)
            {
                return EngineTypeName(type.GetElementType()) + "[]";
            }

            if (type.IsGenericType && type.GetGenericTypeDefinition() == typeof(System.Collections.Generic.List<>))
            {
                return EngineTypeName(type.GetGenericArguments()[0]) + "[]";
            }

            return type.Name;
        }
    }
}
