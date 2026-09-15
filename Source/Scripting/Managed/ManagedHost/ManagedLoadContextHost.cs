namespace FOnline.ManagedHost;

using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Reflection.Metadata;
using System.Reflection.PortableExecutable;
using System.Runtime.Loader;

public static class ManagedLoadContextHost
{
    [CallableByEngine]
    public static object CreateLoadScope(string contextName, string[] assemblyPaths, string[] entryAssemblyPaths)
    {
        ManagedAssemblyLoadContext context = new ManagedAssemblyLoadContext(contextName, assemblyPaths);
        Assembly[] entryAssemblies = new Assembly[entryAssemblyPaths.Length];

        for (int i = 0; i < entryAssemblyPaths.Length; i++) {
            entryAssemblies[i] = context.LoadFromAssemblyPath(Path.GetFullPath(entryAssemblyPaths[i]));
        }

        return new ManagedLoadScope(context, entryAssemblies);
    }

    [CallableByEngine]
    public static Assembly[] GetEntryAssemblies(object scope)
    {
        return GetScope(scope).EntryAssemblies;
    }

    [CallableByEngine]
    public static void ReleaseLoadScope(object scope)
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
        private readonly Dictionary<string, string> AssemblyPaths =
            new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

        public ManagedAssemblyLoadContext(string name, string[] assemblyPaths) : base(name, isCollectible: false)
        {
            for (int i = 0; i < assemblyPaths.Length; i++) {
                string path = Path.GetFullPath(assemblyPaths[i]);
                string ? assemblyName;

                try {
                    // Read through a stream rather than AssemblyName.GetAssemblyName, which memory-maps the
                    // file: WebAssembly has no mmap, and only the simple name is needed here
                    using FileStream stream = File.OpenRead(path);
                    using PEReader peReader = new PEReader(stream, PEStreamOptions.PrefetchMetadata);

                    if (!peReader.HasMetadata) {
                        continue;
                    }

                    MetadataReader metadataReader = peReader.GetMetadataReader();
                    assemblyName = metadataReader.GetString(metadataReader.GetAssemblyDefinition().Name);
                }
                catch (BadImageFormatException) {
                    continue;
                }

                if (string.IsNullOrEmpty(assemblyName)) {
                    throw new InvalidOperationException("Managed assembly has no simple name: " + path);
                }
                if (!AssemblyPaths.TryAdd(assemblyName, path)) {
                    throw new InvalidOperationException("Duplicate managed assembly name: " + assemblyName);
                }
            }
        }

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
