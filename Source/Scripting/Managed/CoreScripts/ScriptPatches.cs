namespace FOnline;

using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;

// A woven prologue tests Active and only then reads Slots, the native table of replacement functions by slot. No static
// initializer, so neither read needs a class-init check
internal static class PatchPointSlots
{
    internal static bool Active;
    internal static nint Slots;
}

// One replacement a patch assembly supplies: the function is taken with ldftn inside that assembly, since the
// interpreter expects its own method handle where the JIT expects code
public sealed class ScriptPatchFunction
{
    public ScriptPatchFunction(MethodInfo replacement, nint function)
    {
        ArgumentNullException.ThrowIfNull(replacement);

        if (function == 0) {
            throw new ArgumentException("Replacement function pointer is null: " + replacement.Name, nameof(function));
        }

        Replacement = replacement;
        Function = function;
    }

    public MethodInfo Replacement { get; }

    public nint Function { get; }
}

public sealed class ScriptPatchSet
{
    internal ScriptPatchSet(string name, List<(MethodInfo Target, int Slot, nint Function)> entries)
    {
        Name = name;
        Entries = entries;
        List<MethodInfo> targets = new List<MethodInfo>();

        foreach ((MethodInfo target, int _, nint _) in entries) {
            targets.Add(target);
        }

        Targets = targets;
    }

    public string Name { get; }

    public IReadOnlyList<MethodInfo> Targets { get; }

    public bool IsApplied { get; internal set; }

    internal List<(MethodInfo Target, int Slot, nint Function)> Entries { get; }
}

// Replaces the body of script methods in the running backend through the patch points the baker weaves when
// ManagedScript.PatchPointWeaver is set. A patch assembly names its targets with [ReplacesMethod] and lists its
// replacement functions in a manifest type; a later patch of the same method wins, and reverting one restores the
// method to whatever it was before that patch
public static class ScriptPatches
{
    public const string TableResourceName = "FOnline.PatchPoints.Table";
    public const string ManifestTypeName = "DynamicPatchManifest";
    public const string ManifestMethodName = "GetFunctions";

    private const BindingFlags DeclaredMembers = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static |
                                                 BindingFlags.Instance | BindingFlags.DeclaredOnly;

    private static readonly object Lock = new object();
    private static readonly List<ScriptPatchSet> AppliedSets = new List<ScriptPatchSet>();
    // Every table ever published, kept for the life of the process: see Publish
    private static readonly List<nint> PublishedTables = new List<nint>();
    private static Dictionary<int, int>? SlotsByToken;

    // Each side reads its own setting, so a client can refuse what its server sends; the mapper receives no patches
#if SERVER
    public static bool IsEnabled => Settings.ManagedScript.ServerPatchesEnabled;
#elif CLIENT
    public static bool IsEnabled => Settings.ManagedScript.ClientPatchesEnabled;
#else
    public static bool IsEnabled => false;
#endif

    // Whether the running scripts were built with patch points at all
    public static bool IsAvailable => GetSlotsByToken().Count != 0;

    public static int PatchPointCount => GetSlotsByToken().Count;

    public static IReadOnlyList<ScriptPatchSet> Applied
    {
        get {
            lock (Lock)
            {
                return AppliedSets.ToArray();
            }
        }
    }

    public static bool HasPatchPoint(MethodBase method)
    {
        ArgumentNullException.ThrowIfNull(method);

        return method.Module == typeof(ScriptPatches).Module && GetSlotsByToken().ContainsKey(method.MetadataToken);
    }

    // Every replacement is checked before the first one takes effect, and all of them switch with one publication
    public static ScriptPatchSet Apply(Assembly patch)
    {
        ArgumentNullException.ThrowIfNull(patch);

        if (!IsEnabled) {
            throw new InvalidOperationException(
                "Script patches are disabled on this side (ManagedScript.ServerPatchesEnabled / ManagedScript.ClientPatchesEnabled)");
        }

        MethodInfo manifest =
            patch.GetType(ManifestTypeName)
                ?.GetMethod(ManifestMethodName, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static) ??
            throw new ArgumentException("Patch assembly has no manifest: " + patch.GetName().Name, nameof(patch));
        ScriptPatchFunction[] functions =
            manifest.Invoke(null, null) as ScriptPatchFunction[] ??
            throw new ArgumentException("Patch manifest returned no functions", nameof(patch));

        if (functions.Length == 0) {
            throw new ArgumentException("Patch replaces no method: " + patch.GetName().Name, nameof(patch));
        }

        List<(MethodInfo Target, int Slot, nint Function)> entries =
            new List<(MethodInfo Target, int Slot, nint Function)>();
        HashSet<int> slots = new HashSet<int>();

        foreach (ScriptPatchFunction function in functions) {
            MethodInfo target = ResolveTarget(function.Replacement);

            if (target.Module != typeof(ScriptPatches).Module ||
                !GetSlotsByToken().TryGetValue(target.MetadataToken, out int slot)) {
                throw new ArgumentException("Method has no patch point: " + Describe(target), nameof(patch));
            }
            if (!slots.Add(slot)) {
                throw new ArgumentException("Patch replaces a method twice: " + Describe(target), nameof(patch));
            }

            entries.Add((target, slot, function.Function));
        }

        ScriptPatchSet set = new ScriptPatchSet(patch.GetName().Name ?? "", entries);

        lock (Lock)
        {
            AppliedSets.Add(set);
            set.IsApplied = true;
            Publish();
        }

        return set;
    }

    public static void Revert(ScriptPatchSet set)
    {
        ArgumentNullException.ThrowIfNull(set);

        lock (Lock)
        {
            if (!AppliedSets.Remove(set)) {
                return;
            }

            set.IsApplied = false;
            Publish();
        }
    }

    public static void RevertAll()
    {
        lock (Lock)
        {
            foreach (ScriptPatchSet set in AppliedSets) {
                set.IsApplied = false;
            }

            AppliedSets.Clear();
            Publish();
        }
    }

    // A new table per publication switches a patch whole; a prologue reads Slots twice, so no table is ever freed and
    // Slots never returns to zero, while Active falls back to false after the last revert
    private static void Publish()
    {
        int count = GetSlotsByToken().Count;

        if (count == 0 || (AppliedSets.Count == 0 && PatchPointSlots.Slots == 0)) {
            return;
        }

        bool first = PatchPointSlots.Slots == 0;

        nint table = Marshal.AllocHGlobal(checked(count * IntPtr.Size));

        for (int slot = 0; slot < count; slot++) {
            Marshal.WriteIntPtr(table, slot * IntPtr.Size, 0);
        }

        foreach (ScriptPatchSet set in AppliedSets) {
            foreach ((MethodInfo _, int slot, nint function) in set.Entries) {
                Marshal.WriteIntPtr(table, slot * IntPtr.Size, function);
            }
        }

        PublishedTables.Add(table);
        Volatile.Write(ref PatchPointSlots.Slots, table);

        // A prologue loads Active and then Slots with plain loads, which ARM64 may reorder; every core observes the first
        // table before any of them can observe Active set
        if (first) {
            Interlocked.MemoryBarrierProcessWide();
        }

        Volatile.Write(ref PatchPointSlots.Active, AppliedSets.Count != 0);
    }

    // The replacement is static and takes the target's parameters, preceded by the target object for an instance
    // method; the manifest's function is called through the target's own signature, so any mismatch is refused here
    private static MethodInfo ResolveTarget(MethodInfo replacement)
    {
        ReplacesMethodAttribute attribute =
            replacement.GetCustomAttribute<ReplacesMethodAttribute>() ??
            throw new ArgumentException("Replacement is not marked [ReplacesMethod]: " + Describe(replacement));

        if (!replacement.IsStatic || replacement.ContainsGenericParameters) {
            throw new ArgumentException("Replacement must be a static non-generic method: " + Describe(replacement));
        }

        ParameterInfo[] parameters = replacement.GetParameters();
        MethodInfo? found = null;

        foreach (MethodInfo candidate in attribute.Type.GetMethods(DeclaredMembers)) {
            if (candidate.Name != attribute.Name || !MatchesSignature(candidate, parameters, replacement.ReturnType)) {
                continue;
            }
            if (found != null) {
                throw new ArgumentException("Replacement matches more than one method: " + Describe(replacement));
            }

            found = candidate;
        }

        if (found == null) {
            throw new ArgumentException("No method with the replacement's signature: " + attribute.Type.FullName +
                                        "::" + attribute.Name);
        }

        return found;
    }

    private static bool MatchesSignature(MethodInfo target, ParameterInfo[] replacementParameters,
                                         Type replacementReturn)
    {
        if (target.ContainsGenericParameters || target.ReturnType != replacementReturn) {
            return false;
        }

        ParameterInfo[] targetParameters = target.GetParameters();
        int offset = target.IsStatic ? 0 : 1;

        if (replacementParameters.Length != targetParameters.Length + offset) {
            return false;
        }
        if (!target.IsStatic) {
            Type self =
                target.DeclaringType ?? throw new ArgumentException("Method has no declaring type: " + target.Name);
            Type expected = self.IsValueType ? self.MakeByRefType() : self;

            if (replacementParameters[0].ParameterType != expected) {
                return false;
            }
        }

        for (int i = 0; i < targetParameters.Length; i++) {
            if (replacementParameters[i + offset].ParameterType != targetParameters[i].ParameterType) {
                return false;
            }
        }

        return true;
    }

    // The weaver stores the metadata token of every patch point by slot index
    private static Dictionary<int, int> GetSlotsByToken()
    {
        lock (Lock)
        {
            if (SlotsByToken == null) {
                Dictionary<int, int> table = new Dictionary<int, int>();
                using Stream? stream = typeof(ScriptPatches).Assembly.GetManifestResourceStream(TableResourceName);

                if (stream != null) {
                    using BinaryReader reader = new BinaryReader(stream);
                    int count = checked((int)(stream.Length / sizeof(int)));

                    for (int slot = 0; slot < count; slot++) {
                        table.Add(reader.ReadInt32(), slot);
                    }
                }

                SlotsByToken = table;
            }

            return SlotsByToken;
        }
    }

    private static string Describe(MethodInfo method)
    {
        return (method.DeclaringType?.FullName ?? "?") + "::" + method.Name;
    }
}
