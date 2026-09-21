namespace FOnline;

using System;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.ExceptionServices;
using System.Threading.Tasks;

// Code compiled after the bake joins the load context of the backend that runs it, so it sees the same script types
// and statics. The context is not collectible: every loaded assembly stays for the life of the process
public static class DynamicAssemblies
{
    // Identifies the scripts build the backend runs; code compiled against another build does not belong in it
    public static Guid ScriptsVersionId => typeof(DynamicAssemblies).Assembly.ManifestModule.ModuleVersionId;

    // The assembly name must start with FOnline.Dynamic. and be new to the backend: a stream load never replaces an
    // assembly of the same name, so a reused one would sit beside the old and references could bind to either
    public static Assembly Load(byte[] image, byte[]? symbols = null)
    {
        ArgumentNullException.ThrowIfNull(image);

        Assembly assembly = Native.LoadDynamicAssembly(image, symbols);
        ScriptStaticCleanup.TrackDynamicAssembly(assembly);
        return assembly;
    }

    // The client entry assembly this server hands out, from the distributed packs or, unpackaged, the bake output; a
    // client fragment compiles against it and runs only on clients whose scripts carry the same version id
    public static byte[] ReadClientScriptsImage()
    {
        return Native.ReadClientScriptsImage();
    }

    // Runs a static method without parameters as a script entry of its own: on a server the cover it takes or releases
    // stays inside it and the caller's is left as it was. A Task result is awaited, and a Task<T> answers with its value
    public static async Task<object?> RunEntryAsync(MethodInfo entry)
    {
        ArgumentNullException.ThrowIfNull(entry);

        if (!entry.IsStatic || entry.ContainsGenericParameters || entry.GetParameters().Length != 0) {
            throw new ArgumentException("Script entry must be a static method without parameters: " + entry.Name,
                                        nameof(entry));
        }
        if (entry.ReturnType == typeof(void) && entry.IsDefined(typeof(AsyncStateMachineAttribute), false)) {
            throw new ArgumentException("Async void script methods are not supported; return Task: " + entry.Name,
                                        nameof(entry));
        }

        EntryRun run = new EntryRun(entry);
        Native.RunScriptContinuation(run.Run);
        run.Failure?.Throw();

        if (run.Result is not Task task) {
            return run.Result;
        }

        await task;

        Type returnType = entry.ReturnType;

        if (!returnType.IsGenericType || returnType.GetGenericTypeDefinition() != typeof(Task<>)) {
            return null;
        }

        // The runtime type of an async method's task is a state machine box, so the result is read as declared
        return returnType.GetProperty(nameof(Task<object>.Result))?.GetValue(task);
    }

    internal sealed class EntryRun : INamedScriptEntry
    {
        public EntryRun(MethodInfo entry)
        {
            Entry = entry;
        }

        public MethodInfo Entry { get; }

        public object? Result { get; private set; }

        public ExceptionDispatchInfo? Failure { get; private set; }

        // Reflection wraps the entry's own exception, and the caller is owed the one the entry threw
        public void Run()
        {
            try {
                Result = Entry.Invoke(null, null);
            }
            catch (TargetInvocationException ex) when (ex.InnerException != null) {
                Failure = ExceptionDispatchInfo.Capture(ex.InnerException);
            }
        }
    }
}
