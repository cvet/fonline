using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;

namespace FOnline
{
    public static class Initializator
    {
        private const string ManagedModuleInitOwnerMarker = "FO_MANAGED_MODULE_INIT_OWNER";

        static void InitializeEarly()
        {
            ScriptFuncRegistration.RegisterEngineAttributeFuncs();
            RemoteCallScriptFuncs.RegisterRemoteCalls();
            RunScriptFuncRegistrars();
        }

        // Invokes project-supplied [ScriptFuncRegistrar] methods (e.g. dialog demand/result registrars) in the
        // registration phase so their script funcs reach the engine's cross-backend registry both at runtime and
        // inside bake-time validation engines, which restore the script subsystem from the compiled assembly the
        // same way the AngelScript bake restores it from bytecode.
        private static void RunScriptFuncRegistrars()
        {
            Assembly assembly = Assembly.GetExecutingAssembly();

            foreach (Type type in assembly.GetTypes())
            {
                foreach (MethodInfo method in type.GetMethods(
                    BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly))
                {
                    if (Attribute.GetCustomAttribute(method, typeof(ScriptFuncRegistrarAttribute)) == null)
                    {
                        continue;
                    }

                    if (method.GetParameters().Length != 0 || method.ReturnType != typeof(void))
                    {
                        throw new InvalidOperationException(
                            "ScriptFuncRegistrar method must be static void with no parameters: " +
                            type.FullName +
                            "." +
                            method.Name);
                    }

                    method.Invoke(null, null);
                }
            }
        }

        static void Initialize()
        {
            List<Tuple<int, MethodInfo>> moduleInits = new List<Tuple<int, MethodInfo>>();

            Assembly assembly = Assembly.GetExecutingAssembly();

            foreach (Type type in assembly.GetTypes())
            {
                try
                {
                    RuntimeHelpers.RunClassConstructor(type.TypeHandle);
                }
                catch (Exception ex)
                {
                    Game.Log("Failed to run type: " + type.Name);
                    Game.Log(ex.ToString());
                }

                foreach (MethodInfo method in type.GetMethods(
                    BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic))
                {
                    ModuleInitAttribute attr = (ModuleInitAttribute)Attribute.GetCustomAttribute(
                        method,
                        typeof(ModuleInitAttribute));

                    if (attr == null)
                    {
                        continue;
                    }
                    if (HasCoexistingAngelScriptModule(type, allowManagedModuleInitOwner: true))
                    {
                        continue;
                    }

                    if (method.GetParameters().Length != 0 ||
                        (method.ReturnType != typeof(void) && !typeof(Task).IsAssignableFrom(method.ReturnType)))
                    {
                        throw new InvalidOperationException(
                            "ModuleInit method must be static void or Task with no parameters: " +
                            type.FullName +
                            "." +
                            method.Name);
                    }

                    moduleInits.Add(Tuple.Create(attr.Priority, method));
                }
            }

            foreach (Tuple<int, MethodInfo> moduleInit in moduleInits.OrderBy(entry => entry.Item1))
            {
                MethodInfo method = moduleInit.Item2;
                object result = method.Invoke(null, null);

                if (method.ReturnType == typeof(void))
                {
                    continue;
                }

                Task task = (Task)result;
                if (task == null)
                {
                    throw new InvalidOperationException(
                        "ModuleInit Task method returned null: " +
                        method.DeclaringType.FullName +
                        "." +
                        method.Name);
                }

                task.GetAwaiter().GetResult();
            }
        }

        internal static bool HasCoexistingAngelScriptModule(Type type, bool allowManagedModuleInitOwner = false)
        {
            Type moduleType = type;
            while (moduleType.DeclaringType != null)
            {
                moduleType = moduleType.DeclaringType;
            }

            string moduleName = moduleType.Name;
            int genericSuffix = moduleName.IndexOf('`');
            if (genericSuffix != -1)
            {
                moduleName = moduleName.Substring(0, genericSuffix);
            }

            string[] scriptDirs =
            {
                "Scripts",
                Path.Combine("Scripts", "Quests"),
                Path.Combine("Scripts", "Scenes"),
                Path.Combine("Scripts", "Tests"),
                Path.Combine("Scripts", "Debug"),
                Path.Combine("Engine", "Source", "Scripting", "AngelScript", "CoreScripts"),
            };

            for (int i = 0; i < scriptDirs.Length; i++)
            {
                string scriptPath = Path.Combine(scriptDirs[i], moduleName + ".fos");
                if (File.Exists(scriptPath))
                {
                    if (allowManagedModuleInitOwner && IsManagedModuleInitOwner(scriptPath))
                    {
                        continue;
                    }

                    return true;
                }
            }

            return false;
        }

        private static bool IsManagedModuleInitOwner(string scriptPath)
        {
            try
            {
                return File.ReadAllText(scriptPath).Contains(ManagedModuleInitOwnerMarker, StringComparison.Ordinal);
            }
            catch (Exception ex)
            {
                Game.Log("Failed to inspect AngelScript module ownership marker: " + scriptPath);
                Game.Log(ex.ToString());
                return false;
            }
        }
    }
}
