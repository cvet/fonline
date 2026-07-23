#nullable enable

using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.Loader;

namespace FOnline.ManagedHost
{
    public static class ManagedLoadContextHost
    {
        public static object CreateLoadScope(
            string contextName,
            string[] assemblyPaths,
            string[] entryAssemblyPaths)
        {
            ManagedAssemblyLoadContext context = new ManagedAssemblyLoadContext(contextName, assemblyPaths);
            Assembly[] entryAssemblies = new Assembly[entryAssemblyPaths.Length];

            for (int i = 0; i < entryAssemblyPaths.Length; i++)
            {
                entryAssemblies[i] = context.LoadFromAssemblyPath(Path.GetFullPath(entryAssemblyPaths[i]));
            }

            return new ManagedLoadScope(context, entryAssemblies);
        }

        public static Assembly[] GetEntryAssemblies(object scope)
        {
            return GetScope(scope).EntryAssemblies;
        }

        public static void ReleaseLoadScope(object scope)
        {
            GetScope(scope).Release();
        }

        private static ManagedLoadScope GetScope(object scope)
        {
            if (scope is not ManagedLoadScope loadScope)
            {
                throw new ArgumentException("Invalid managed load scope", nameof(scope));
            }

            return loadScope;
        }

        private sealed class ManagedLoadScope
        {
            private ManagedAssemblyLoadContext? _context;

            public ManagedLoadScope(ManagedAssemblyLoadContext context, Assembly[] entryAssemblies)
            {
                _context = context;
                EntryAssemblies = entryAssemblies;
            }

            public Assembly[] EntryAssemblies { get; private set; }

            public void Release()
            {
                if (_context == null)
                {
                    return;
                }

                EntryAssemblies = Array.Empty<Assembly>();
                _context = null;
            }
        }

        private sealed class ManagedAssemblyLoadContext : AssemblyLoadContext
        {
            private readonly Dictionary<string, string> _assemblyPaths =
                new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

            public ManagedAssemblyLoadContext(string name, string[] assemblyPaths)
                : base(name, isCollectible: false)
            {
                for (int i = 0; i < assemblyPaths.Length; i++)
                {
                    string path = Path.GetFullPath(assemblyPaths[i]);
                    string? assemblyName;

                    try
                    {
                        assemblyName = AssemblyName.GetAssemblyName(path).Name;
                    }
                    catch (BadImageFormatException)
                    {
                        continue;
                    }

                    if (string.IsNullOrEmpty(assemblyName))
                    {
                        throw new InvalidOperationException("Managed assembly has no simple name: " + path);
                    }
                    if (!_assemblyPaths.TryAdd(assemblyName, path))
                    {
                        throw new InvalidOperationException("Duplicate managed assembly name: " + assemblyName);
                    }
                }
            }

            protected override Assembly? Load(AssemblyName assemblyName)
            {
                if (assemblyName.Name != null && _assemblyPaths.TryGetValue(assemblyName.Name, out string? path))
                {
                    return LoadFromAssemblyPath(path);
                }

                return null;
            }
        }
    }
}
