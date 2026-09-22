namespace FOnline.ManagedHost;

using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.Loader;

public static class ManagedLoadContextHost
{
    [CallableByEngine]
    internal static object CreateLoadScope(string contextName, string[] assemblyPaths, string[] entryAssemblyPaths)
    {
        ManagedAssemblyLoadContext context = new ManagedAssemblyLoadContext(contextName, assemblyPaths);
        Assembly[] entryAssemblies = new Assembly[entryAssemblyPaths.Length];

        for (int i = 0; i < entryAssemblyPaths.Length; i++) {
            entryAssemblies[i] = context.LoadFromAssemblyPath(Path.GetFullPath(entryAssemblyPaths[i]));
        }

        return new ManagedLoadScope(context, entryAssemblies);
    }

    [CallableByEngine]
    internal static Assembly[] GetEntryAssemblies(object scope)
    {
        return GetScope(scope).EntryAssemblies;
    }

    [CallableByEngine]
    internal static Assembly LoadDynamicAssembly(object scope, string assemblyName, byte[] image, byte[]? symbols)
    {
        return GetScope(scope).LoadDynamicAssembly(assemblyName, image, symbols);
    }

    [CallableByEngine]
    internal static void ReleaseLoadScope(object scope)
    {
        GetScope(scope).Release();
    }

    private static ManagedLoadScope GetScope(object scope)
    {
        if (scope is not ManagedLoadScope loadScope) {
            throw new ArgumentException("Invalid managed load scope", nameof(scope));
        }

        return loadScope;
    }

    private sealed class ManagedLoadScope
    {
        private ManagedAssemblyLoadContext? Context;

        public ManagedLoadScope(ManagedAssemblyLoadContext context, Assembly[] entryAssemblies)
        {
            Context = context;
            EntryAssemblies = entryAssemblies;
        }

        public Assembly[] EntryAssemblies { get; private set; }

        public Assembly LoadDynamicAssembly(string assemblyName, byte[] image, byte[]? symbols)
        {
            if (Context == null) {
                throw new InvalidOperationException("Managed load scope is released");
            }

            return Context.LoadDynamicAssembly(assemblyName, image, symbols);
        }

        public void Release()
        {
            if (Context == null) {
                return;
            }

            EntryAssemblies = Array.Empty<Assembly>();
            Context = null;
        }
    }

    private sealed class ManagedAssemblyLoadContext : AssemblyLoadContext
    {
        // Code compiled after the bake carries this prefix, so its name can never be taken for a pack assembly or a
        // class library the context has not opened yet
        private const string DynamicAssemblyNamePrefix = "FOnline.Dynamic.";

        private readonly Dictionary<string, string> AssemblyPaths =
            new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        private readonly HashSet<string> DynamicAssemblyNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        public ManagedAssemblyLoadContext(string name, string[] assemblyPaths) : base(name, isCollectible: false)
        {
            for (int i = 0; i < assemblyPaths.Length; i++) {
                string path = Path.GetFullPath(assemblyPaths[i]);

                // The baker packs only managed assemblies named after the assembly they define, so the file name is
                // the simple name. Reading it from metadata would need AssemblyName.GetAssemblyName, which memory-maps
                // the file where WebAssembly has no mmap, or System.Reflection.Metadata with its dependency chain
                string assemblyName = Path.GetFileNameWithoutExtension(path);

                if (string.IsNullOrEmpty(assemblyName)) {
                    throw new InvalidOperationException("Managed assembly has no simple name: " + path);
                }
                if (!AssemblyPaths.TryAdd(assemblyName, path)) {
                    throw new InvalidOperationException("Duplicate managed assembly name: " + assemblyName);
                }
            }
        }

        // The name comes from the image before loading: a stream load never reuses an assembly the context holds, so
        // a second image under a taken name would load beside the first and later references could bind to either
        public Assembly LoadDynamicAssembly(string assemblyName, byte[] image, byte[]? symbols)
        {
            if (!assemblyName.StartsWith(DynamicAssemblyNamePrefix, StringComparison.Ordinal) ||
                assemblyName.Length == DynamicAssemblyNamePrefix.Length) {
                throw new InvalidOperationException("Dynamic managed assembly name must start with " +
                                                    DynamicAssemblyNamePrefix + ": " + assemblyName);
            }

            lock (DynamicAssemblyNames)
            {
                if (AssemblyPaths.ContainsKey(assemblyName) || DynamicAssemblyNames.Contains(assemblyName)) {
                    throw new InvalidOperationException("Duplicate managed assembly name: " + assemblyName);
                }

                using MemoryStream imageStream = new MemoryStream(image, false);

                // An empty PDB is no PDB: a caller relaying one it was never given passes an empty array
                using MemoryStream? symbolsStream =
                    symbols is { Length: > 0 } ? new MemoryStream(symbols, false) : null;
                Assembly assembly = LoadFromStream(imageStream, symbolsStream);

                if (!string.Equals(assembly.GetName().Name, assemblyName, StringComparison.OrdinalIgnoreCase)) {
                    throw new InvalidOperationException("Dynamic managed assembly loaded under another name: " +
                                                        assemblyName);
                }

                DynamicAssemblyNames.Add(assemblyName);
                return assembly;
            }
        }

        // A reference to a dynamic assembly never gets here: the runtime answers it with the assembly already loaded
        protected override Assembly? Load(AssemblyName assemblyName)
        {
            if (assemblyName.Name != null && AssemblyPaths.TryGetValue(assemblyName.Name, out string? path))
            {
                return LoadFromAssemblyPath(path);
            }

            return null;
        }
    }
}

// The host is built from this file alone and loads before any script assembly, so it cannot see the CoreScripts
// marker and carries its own copy with the same meaning: native code resolves the method through Mono metadata,
// so its name and parameter count are part of the native ABI
[AttributeUsage(AttributeTargets.Method)]
internal sealed class CallableByEngineAttribute : Attribute
{
}
