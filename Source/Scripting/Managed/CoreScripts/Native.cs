namespace FOnline;

using System;
using System.Collections;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;

// Invoked by the engine when a virtual property with a managed setter is written; the setter may
// mutate value, and the engine stores the result. Registered via Game.AddPropertySetter.
public delegate void PropertySetter<TEntity, TValue>(TEntity entity, ref TValue value);
public delegate void PropertySetterWithProperty<TEntity, TProperty, TValue>(TEntity entity, TProperty property,
                                                                            ref TValue value);

// An engine failure an internal call handed to script as a message; the engine keeps the native exception behind it
internal sealed class NativeCallException : InvalidOperationException
{
    public NativeCallException(string message) : base(message)
    {
    }
}

[System.Runtime.CompilerServices.InlineArray(256)]
internal struct ScalarCallFrame
{
    private byte Element0;
}

[System.Runtime.CompilerServices.InlineArray(256)]
internal struct InnerEntityFillFrame
{
    private IntPtr Element0;
}

// A generated wrapper class registers its factory at InitializeEarly, so wrapping a native pointer is one delegate
// call instead of a reflection-driven Activator.CreateInstance. A class no generator registered keeps the reflection
// path. The slot holds a delegate to a static lambda and never an entity, so backend teardown has nothing to clear
internal static class WrapperFactory<T>
    where T : class
{
    internal static Func<IntPtr, T>? Create;
}

internal static class Native
{
    // Every backend loads its own copy of the core scripts, so this names the engine behind this assembly on any
    // thread; internal calls that reach an engine pass it, and zero means the engine is gone
    private static volatile IntPtr BoundBackend;

    // A finalizer may run after the engine is destroyed, and whatever outlives its engine must not reach it
    internal static bool IsBackendAlive => BoundBackend != IntPtr.Zero;

    // Called by the engine before any other code of this assembly runs, so type initializers already see it
    [CallableByEngine]
    internal static void BindBackend(IntPtr backend)
    {
        Invariant.Verify(backend != IntPtr.Zero, "Managed backend binding requires an engine");
        Invariant.Verify(BoundBackend == IntPtr.Zero, "Managed entry assembly is bound to one backend only");
        BoundBackend = backend;
    }

    [CallableByEngine]
    internal static void UnbindBackend()
    {
        BoundBackend = IntPtr.Zero;
    }

    // An orderly teardown clears the script statics and collects while the backend is still bound, so a finalizer
    // running in that window cannot tell a dropped resource from one its owner held until the engine stopped
    private static volatile bool BackendTearingDown;

    internal static bool IsBackendTearingDown => BackendTearingDown;

    // Called by the engine as the first step of backend teardown, before the statics are cleared
    [CallableByEngine]
    internal static void BeginBackendTeardown()
    {
        BackendTearingDown = true;
    }

    // The generated non-nullable members prove the pointer before they wrap it -- a property that reads a
    // component the entity has, an element of a list built from live pointers. A null here would mean the
    // native side broke that contract, so it is an invariant failure rather than a value to hand back
    internal static T WrapEntityNotNull<T>(IntPtr entityPtr)
        where T : Entity
    {
        T? entity = WrapEntity<T>(entityPtr);
        Invariant.Verify(entity != null, "Entity pointer must not be null");
        return entity;
    }

    // A mutable argument is read back out of the array the native call just wrote into, so the slot holds the
    // value the callee produced; an empty one would mean the call did not run to the end
    internal static T UnboxArg<T>(object? value)
    {
        Invariant.Verify(value != null, "Mutable argument must be written by the call");
        return (T)value;
    }

    // Wrapper constructions, counted only while the interop probe measures; a measurement sees every thread of the
    // backend, so it is taken on a quiet scene
    private static bool CountWrappers;
    private static long WrappersCreated;

    // Switches wrapper counting on or off and returns the constructions counted so far
    internal static long ReadWrapperCount(bool enable)
    {
        CountWrappers = enable;
        return Interlocked.Read(ref WrappersCreated);
    }

    internal static T? WrapEntity<T>(IntPtr entityPtr)
        where T : Entity
    {
        if (entityPtr == IntPtr.Zero) {
            return null;
        }

        if (CountWrappers) {
            Interlocked.Increment(ref WrappersCreated);
        }

        Func<IntPtr, T>? create = WrapperFactory<T>.Create;

        if (create != null) {
            return create(entityPtr);
        }

        return (T)Activator.CreateInstance(typeof(T),
                                           BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic,
                                           null,
                                           new object[] {
                                               entityPtr,
                                           },
                                           null)!;
    }

    internal static T? WrapRef<T>(IntPtr refPtr)
        where T : class
    {
        if (refPtr == IntPtr.Zero) {
            return null;
        }

        if (CountWrappers) {
            Interlocked.Increment(ref WrappersCreated);
        }

        Func<IntPtr, T>? create = WrapperFactory<T>.Create;

        if (create != null) {
            return create(refPtr);
        }

        return (T)Activator.CreateInstance(typeof(T),
                                           BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic,
                                           null,
                                           new object[] {
                                               refPtr,
                                           },
                                           null)!;
    }

    // A callback frame carries a ref-type handle the native side proved before dispatching, so a null here is the
    // bridge breaking its contract, like a null entity pointer in WrapEntityNotNull
    internal static T WrapRefNotNull<T>(IntPtr refPtr)
        where T : class
    {
        T? value = WrapRef<T>(refPtr);
        Invariant.Verify(value != null, "Ref pointer must not be null");
        return value;
    }

    internal static void RegisterWrapperFactory<T>(Func<IntPtr, T> create)
        where T : class
    {
        WrapperFactory<T>.Create = create;
    }

    internal static bool HasWrapperFactory<T>()
        where T : class
    {
        return WrapperFactory<T>.Create != null;
    }

    // A Task-returning callback is registered as a native void: it continues asynchronously instead of blocking the
    // script pump, and a deferred fault stays accounted the way InvokeCallback accounts it
    internal static void CompleteCallbackTask(Task task)
    {
        if (task.IsCompleted) {
            task.GetAwaiter().GetResult();
        }
        else {
            ScriptExceptions.ObserveTask(task);
        }
    }

    [CallableByEngine]
    internal static EventResult InvokeEvent(Delegate handler, bool hasExplicitResult, object?[] args)
    {
        using ScriptSynchronizationContext context = ScriptSynchronizationContext.Enter(hasExplicitResult);

        try {
            object? result = handler.DynamicInvoke(AdaptInvokeArgs(handler, args));
            Task? task = result as Task;

            if (task != null) {
                if (hasExplicitResult) {
                    // Native event dispatch cannot advance the subscriber chain until it knows whether to stop.
                    context.Wait(task);
                    object? taskResult = task.GetType().GetProperty("Result")?.GetValue(task);

                    if (taskResult is EventResult eventResult) {
                        return eventResult;
                    }

                    throw new InvalidOperationException("Async result event handlers must return Task<EventResult>");
                }

                if (task.IsCompleted) {
                    task.GetAwaiter().GetResult();
                }
                else {
                    ScriptExceptions.ObserveTask(task);
                }

                return EventResult.ContinueChain;
            }

            if (hasExplicitResult) {
                return (EventResult)result!;
            }

            return EventResult.ContinueChain;
        }
        catch (Exception ex) {
            ScriptExceptions.Record(ex, true);
            return EventResult.StopChain;
        }
    }

    [CallableByEngine]
    internal static object? InvokeCallback(Delegate handler, object?[] args)
    {
        MethodInfo delegateInvoke = handler.GetType().GetMethod("Invoke") ??
                                    throw new InvalidOperationException("Delegate type is missing its Invoke method");
        Type declaredReturnType = delegateInvoke.ReturnType;
        bool hasResult =
            declaredReturnType.IsGenericType && declaredReturnType.GetGenericTypeDefinition() == typeof(Task<>);
        using ScriptSynchronizationContext context = ScriptSynchronizationContext.Enter(hasResult);

        try {
            // A by-ref parameter is written by the callee, and the caller reads it back out of the very array it
            // handed over. AdaptInvokeArgs may hand DynamicInvoke a copy, so the written values are carried back
            object?[] invokeArgs = AdaptInvokeArgs(handler, args);
            object? result = handler.DynamicInvoke(invokeArgs);

            if (!ReferenceEquals(invokeArgs, args)) {
                CopyBackByRefArgs(handler, invokeArgs, args);
            }

            Task? task = result as Task;

            if (task == null) {
                return result;
            }

            if (hasResult) {
                context.Wait(task);
                return declaredReturnType.GetProperty("Result")!.GetValue(result);
            }

            // Task-returning script functions are registered as native void callbacks. Waiting here would
            // block the script pump that must fire ScriptTask.Delay's completion event, so let the callback
            // continue asynchronously and retain deferred exception accounting.
            if (task.IsCompleted) {
                task.GetAwaiter().GetResult();
            }
            else {
                ScriptExceptions.ObserveTask(task);
            }

            return null;
        }
        catch (Exception ex) {
            ScriptExceptions.Record(ex, false);
            throw;
        }
    }

    [CallableByEngine]
    internal static void PumpContinuations()
    {
        ScriptSynchronizationContext.Pump();
    }

    [CallableByEngine]
    internal static void ShutdownContinuations()
    {
        ScriptSynchronizationContext.Shutdown();
    }

    internal static void RunScriptContinuation(Action continuation)
    {
        ThrowNativeError(RunScriptContinuationInternal(BoundBackend, continuation));
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern string? RunScriptContinuationInternal(IntPtr backend, Action continuation);

    private static void CopyBackByRefArgs(Delegate handler, object?[] invokeArgs, object?[] args)
    {
        ParameterInfo[] parameters = handler.Method.GetParameters();

        for (int i = 0; i < args.Length && i < parameters.Length && i < invokeArgs.Length; i++) {
            if (parameters[i].ParameterType.IsByRef) {
                args[i] = invokeArgs[i];
            }
        }
    }

    private static object?[] AdaptInvokeArgs(Delegate handler, object?[] args)
    {
        ParameterInfo[] parameters = handler.Method.GetParameters();
        object?[]? adaptedArgs = null;

        for (int i = 0; i < args.Length && i < parameters.Length; i++) {
            Type parameterType = parameters[i].ParameterType;

            if (parameterType == typeof(Dictionary<string, string>) && args[i] is IDictionary source &&
                !(args[i] is Dictionary<string, string>)) {
                adaptedArgs = adaptedArgs ?? (object?[])args.Clone();
                adaptedArgs[i] = StringifyDictionary(source);
            }
            else if (parameterType == typeof(List<string>) && args[i] is IEnumerable sourceList &&
                     !(args[i] is List<string>) && !(args[i] is string)) {
                adaptedArgs = adaptedArgs ?? (object?[])args.Clone();
                adaptedArgs[i] = StringifyList(sourceList);
            }
            else if (parameterType == typeof(List<object>) && args[i] is IEnumerable sourceObjectList &&
                     !(args[i] is List<object>) && !(args[i] is string)) {
                adaptedArgs = adaptedArgs ?? (object?[])args.Clone();
                adaptedArgs[i] = ObjectList(sourceObjectList);
            }
        }

        return adaptedArgs ?? args;
    }

    private static Dictionary<string, string> StringifyDictionary(IDictionary source)
    {
        Dictionary<string, string> result = new Dictionary<string, string>();

        foreach (DictionaryEntry entry in source) {
            string? key = entry.Key as string;

            if (key == null) {
                continue;
            }

            result[key] = entry.Value?.ToString() ?? string.Empty;
        }

        return result;
    }

    private static List<string> StringifyList(IEnumerable source)
    {
        List<string> result = new List<string>();

        foreach (object entry in source) {
            result.Add(entry?.ToString() ?? string.Empty);
        }

        return result;
    }

    private static List<object> ObjectList(IEnumerable source)
    {
        List<object> result = new List<object>();

        foreach (object entry in source) {
            result.Add(entry);
        }

        return result;
    }

    // Called by the engine when a script exception reaches native code
    [CallableByEngine]
    internal static object?[] DescribeException(Exception exception)
    {
        DescribeException(exception, out string summary, out string? nativeError, out long[] frames);
        return new object?[] { summary, nativeError, frames };
    }

    // Frames run from the innermost cause outwards as runtime method handle and IL offset pairs, which the engine
    // resolves the same way as live frames
    private static void DescribeException(Exception exception, out string summary, out string? nativeError,
                                          out long[] frames)
    {
        List<Exception> chain = new List<Exception>();

        CollectExceptionChain(exception, chain);

        System.Text.StringBuilder text = new System.Text.StringBuilder();

        foreach (Exception cause in chain) {
            if (IsExceptionWrapper(cause)) {
                continue;
            }

            if (text.Length != 0) {
                text.Append(" ---> ");
            }

            text.Append(cause.GetType().FullName).Append(": ").Append(cause.Message);
        }

        List<long> frameValues = new List<long>();
        long previousHandle = 0;

        // StackTrace is implemented in CoreLib, but naming it references System.Diagnostics.StackTrace, whose
        // implementation brings System.Reflection.Metadata and its dependencies into every runtime payload
        Assembly coreLib = typeof(object).Assembly;
        Type stackTraceType = coreLib.GetType("System.Diagnostics.StackTrace", true)!;
        Type stackFrameType = coreLib.GetType("System.Diagnostics.StackFrame", true)!;
        MethodInfo getFrames = stackTraceType.GetMethod("GetFrames", Type.EmptyTypes)!;
        MethodInfo getMethod = stackFrameType.GetMethod("GetMethod", Type.EmptyTypes)!;
        MethodInfo getILOffset = stackFrameType.GetMethod("GetILOffset", Type.EmptyTypes)!;

        for (int i = chain.Count - 1; i >= 0; i--) {
            object stackTrace = Activator.CreateInstance(stackTraceType, chain[i], false)!;
            Array stackFrames = (Array)getFrames.Invoke(stackTrace, null)!;

            for (int j = 0; j < stackFrames.Length; j++) {
                object stackFrame = stackFrames.GetValue(j)!;
                long handle = GetMethodHandle((MethodBase?)getMethod.Invoke(stackFrame, null));

                // A wrapping exception is thrown from the frame that caught its cause, which the cause already lists
                if (handle == 0 || (j == 0 && handle == previousHandle)) {
                    continue;
                }

                frameValues.Add(handle);
                frameValues.Add((int)getILOffset.Invoke(stackFrame, null)!);
                previousHandle = handle;
            }
        }

        summary = text.ToString();
        Exception unwrapped = exception;

        while (IsExceptionWrapper(unwrapped) && unwrapped.InnerException is Exception inner) {
            unwrapped = inner;
        }

        nativeError = unwrapped is NativeCallException nativeCause ? nativeCause.Message : null;
        frames = frameValues.ToArray();
    }

    private static void CollectExceptionChain(Exception exception, List<Exception> chain)
    {
        chain.Add(exception);

        if (exception is AggregateException aggregate) {
            foreach (Exception inner in aggregate.InnerExceptions) {
                CollectExceptionChain(inner, chain);
            }
        }
        else if (exception.InnerException is Exception inner) {
            CollectExceptionChain(inner, chain);
        }
    }

    // Dynamic methods have no runtime handle, so their frames cannot be resolved by the engine
    private static long GetMethodHandle(MethodBase? method)
    {
        if (method == null) {
            return 0;
        }

        try {
            return method.MethodHandle.Value.ToInt64();
        }
        catch (InvalidOperationException) {
            return 0;
        }
    }

    private static bool IsExceptionWrapper(Exception exception)
    {
        if (exception.InnerException == null) {
            return false;
        }

        return exception is TargetInvocationException ||
               (exception is AggregateException aggregate && aggregate.InnerExceptions.Count == 1);
    }

    [CallableByEngine]
    internal static bool IsList(object value)
    {
        return value is IList;
    }

    [CallableByEngine]
    internal static bool IsDictionary(object value)
    {
        return value is IDictionary;
    }

    [CallableByEngine]
    internal static bool IsDelegate(object value)
    {
        return value is Delegate;
    }

    // Handler methods already proved to carry [Event]: scripts subscribe the same handlers on every entity init, and
    // the proof reads attributes through reflection
    private static readonly ConditionalWeakTable<MethodInfo, object> EventHandlerMethods =
        new ConditionalWeakTable<MethodInfo, object>();

    internal static void RequireEventAttribute(Delegate handler)
    {
        if (handler.HasSingleTarget && EventHandlerMethods.TryGetValue(handler.Method, out _)) {
            return;
        }

        RequireMethodAttribute<EventAttribute>(handler);

        if (handler.HasSingleTarget) {
            EventHandlerMethods.TryAdd(handler.Method, handler.Method);
        }
    }

    private static void RequireMethodAttribute<TAttribute>(Delegate handler)
        where TAttribute : Attribute
    {
        Delegate[] invocationList = handler.GetInvocationList();

        for (int i = 0; i < invocationList.Length; i++) {
            Delegate item = invocationList[i];
            MethodInfo method = item.Method;

            if (Attribute.GetCustomAttribute(method, typeof(TAttribute)) != null) {
                continue;
            }

            string message = GetMethodName(method) + " must be marked " + GetAttributeName(typeof(TAttribute));
            throw new InvalidOperationException(message);
        }
    }

    private static string GetMethodName(MethodInfo method)
    {
        string? typeName = method.DeclaringType != null ? method.DeclaringType.FullName : string.Empty;

        if (string.IsNullOrEmpty(typeName)) {
            return method.Name;
        }

        return typeName + "." + method.Name;
    }

    private static string GetAttributeName(Type attributeType)
    {
        const string suffix = "Attribute";
        string name = attributeType.Name;

        if (name.EndsWith(suffix, StringComparison.Ordinal)) {
            name = name.Substring(0, name.Length - suffix.Length);
        }

        return "[" + name + "]";
    }

    [CallableByEngine]
    internal static string DescribeScriptEntry(object entry)
    {
        return ScriptEntryNames.Describe(entry);
    }

    [CallableByEngine]
    internal static bool EventHandlersEqual(Delegate subscribed, Delegate handler)
    {
        return subscribed.Equals(handler);
    }

    [CallableByEngine]
    internal static string GetDelegateKey(Delegate handler)
    {
        if (handler == null) {
            return string.Empty;
        }

        string key = string.Empty;
        Delegate[] invocationList = handler.GetInvocationList();

        for (int i = 0; i < invocationList.Length; i++) {
            Delegate item = invocationList[i];
            MethodInfo method = item.Method;
            object? target = item.Target;

            if (key.Length != 0) {
                key += "|";
            }

            key += method.Module.ModuleVersionId.ToString();
            key += ":";
            key += method.MetadataToken.ToString(System.Globalization.CultureInfo.InvariantCulture);
            key += ":";
            key += method.DeclaringType != null ? method.DeclaringType.FullName : string.Empty;
            key += ":";
            key += method.Name;
            key += ":";
            key += target != null
                     ? RuntimeHelpers.GetHashCode(target).ToString(System.Globalization.CultureInfo.InvariantCulture)
                     : "static";
        }

        return key;
    }

    [CallableByEngine]
    internal static int GetListCount(object value)
    {
        return ((IList)value).Count;
    }

    [CallableByEngine]
    internal static object? GetListItem(object value, int index)
    {
        return ((IList)value)[index];
    }

    [CallableByEngine]
    internal static int GetDictionaryCount(object value)
    {
        return ((IDictionary)value).Count;
    }

    [CallableByEngine]
    internal static object GetDictionaryKey(object value, int index)
    {
        int i = 0;
        foreach (DictionaryEntry entry in (IDictionary)value) {
            if (i == index) {
                return entry.Key;
            }

            i++;
        }

        throw new ArgumentOutOfRangeException(nameof(index));
    }

    [CallableByEngine]
    internal static object? GetDictionaryValue(object value, int index)
    {
        int i = 0;
        foreach (DictionaryEntry entry in (IDictionary)value) {
            if (i == index) {
                return entry.Value;
            }

            i++;
        }

        throw new ArgumentOutOfRangeException(nameof(index));
    }

    // A list per element type is built through a delegate made once per type: the engine creates one for nearly every
    // collection it hands to script, and a generic type construction plus Activator each time was most of that cost
    private static readonly ConcurrentDictionary<Type, Func<object>> ListFactories =
        new ConcurrentDictionary<Type, Func<object>>();

    [CallableByEngine]
    internal static object CreateList(Type elementType)
    {
        return ListFactories.GetOrAdd(elementType, MakeListFactory)();
    }

    private static Func<object> MakeListFactory(Type elementType)
    {
        MethodInfo? create = typeof(Native).GetMethod(nameof(NewList), BindingFlags.Static | BindingFlags.NonPublic);
        Invariant.Verify(create != null, "The list factory method must exist");
        return create.MakeGenericMethod(elementType).CreateDelegate<Func<object>>();
    }

    private static object NewList<T>()
    {
        return new List<T>();
    }

    [CallableByEngine]
    internal static void AddListItem(object list, object value)
    {
        ((IList)list).Add(value);
    }

    [CallableByEngine]
    internal static object CreateDictionary(Type keyType, Type valueType)
    {
        return Activator.CreateInstance(typeof(Dictionary<, >).MakeGenericType(keyType, valueType))!;
    }

    [CallableByEngine]
    internal static object CreateDictionaryOfList(Type keyType, Type elementType)
    {
        Type listType = typeof(List<>).MakeGenericType(elementType);
        return Activator.CreateInstance(typeof(Dictionary<, >).MakeGenericType(keyType, listType))!;
    }

    [CallableByEngine]
    internal static void AddDictionaryItem(object dictionary, object key, object value)
    {
        ((IDictionary)dictionary).Add(key, value);
    }

    internal static void SetByteArrayItem(object bytes, int index, int value)
    {
        ((byte[])bytes)[index] = (byte)value;
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    internal static extern void Log(string text);

    // Reports a handled script exception through the engine exception reporter, with its frames in the native stack trace
    internal static void ReportException(Exception exception)
    {
        DescribeException(exception, out string summary, out string? nativeError, out long[] frames);
        ReportExceptionInternal(summary, nativeError, frames);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void ReportExceptionInternal(string summary, string? nativeError, long[] frames);

    [MethodImpl(MethodImplOptions.InternalCall)]
    internal static extern string GetHashStr(System.IntPtr value);

    internal static string GetHashStrFromHash(ulong value)
    {
        return GetHashStrFromHashInternal(BoundBackend, value);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern string GetHashStrFromHashInternal(IntPtr backend, ulong value);

    internal static System.IntPtr GetHash(string text)
    {
        return GetHashInternal(BoundBackend, text);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern System.IntPtr GetHashInternal(IntPtr backend, string text);

    internal static System.IntPtr ResolveHash(ulong hash)
    {
        return ResolveHashInternal(BoundBackend, hash);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern System.IntPtr ResolveHashInternal(IntPtr backend, ulong hash);

    [MethodImpl(MethodImplOptions.InternalCall)]
    internal static extern long GetEntityId(IntPtr entityPtr);

    [MethodImpl(MethodImplOptions.InternalCall)]
    internal static extern IntPtr GetEntityProtoId(IntPtr entityPtr);

    [MethodImpl(MethodImplOptions.InternalCall)]
    internal static extern void AddRefEntity(IntPtr entityPtr);

    [MethodImpl(MethodImplOptions.InternalCall)]
    internal static extern void ReleaseEntity(IntPtr entityPtr);

    // Backs the managed Entity.IsDestroyed property (parity with AngelScript Entity.IsDestroyed).
    [MethodImpl(MethodImplOptions.InternalCall)]
    internal static extern bool IsEntityDestroyed(IntPtr entityPtr);

    // Backs the managed Entity.IsDestroying property (parity with AngelScript Entity.IsDestroying).
    [MethodImpl(MethodImplOptions.InternalCall)]
    internal static extern bool IsEntityDestroying(IntPtr entityPtr);

    // Backs the managed mdir(hdir) constructor (parity with AngelScript mdir(hdir); geometry-dependent).
    [MethodImpl(MethodImplOptions.InternalCall)]
    internal static extern short HdirToMdir(sbyte dir);

    // Backs the managed mdir.hex property (parity with AngelScript mdir::get_hex; geometry-dependent).
    [MethodImpl(MethodImplOptions.InternalCall)]
    internal static extern sbyte MdirHex(short angle);

    // Backs managed mdir.rotateHex/incHex/decHex (parity with AngelScript mdir::rotateHex).
    [MethodImpl(MethodImplOptions.InternalCall)]
    internal static extern short MdirRotateHex(short angle, int steps);

    // Backs managed mdir.reverse (parity with AngelScript mdir::reverse).
    [MethodImpl(MethodImplOptions.InternalCall)]
    internal static extern short MdirReverse(short angle);

    // Backs the managed Entity.Name property (parity with AngelScript base-entity get_Name -> Entity::GetName).
    [MethodImpl(MethodImplOptions.InternalCall)]
    internal static extern string GetEntityName(IntPtr entityPtr);

    // Custom-entity proto lookup (mirrors AngelScript Game_GetProtoCustomEntity / Game_CheckProtoCustomEntity):
    // returns the proto entity pointer for `typeName`/`protoIdHash` (IntPtr.Zero if unknown), or whether it
    // exists. Backs the baker-generated Game.GetProto<X> / CheckProto<X> wrappers for custom HasProtos entities.
    internal static IntPtr GetProtoEntity(string typeName, IntPtr protoId)
    {
        return GetProtoEntityInternal(BoundBackend, typeName, protoId);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern IntPtr GetProtoEntityInternal(IntPtr backend, string typeName, IntPtr protoId);

    internal static bool CheckProtoEntity(string typeName, IntPtr protoId)
    {
        return CheckProtoEntityInternal(BoundBackend, typeName, protoId);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern bool CheckProtoEntityInternal(IntPtr backend, string typeName, IntPtr protoId);

    // Plural proto enumeration (count + by-index) backing the generated Game.GetProto<X>s()/Get<X>s().
    internal static int GetProtoEntityCount(string typeName)
    {
        return GetProtoEntityCountInternal(BoundBackend, typeName);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern int GetProtoEntityCountInternal(IntPtr backend, string typeName);

    internal static IntPtr GetProtoEntityAt(string typeName, int index)
    {
        return GetProtoEntityAtInternal(BoundBackend, typeName, index);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern IntPtr GetProtoEntityAtInternal(IntPtr backend, string typeName, int index);

    // Entity-holder accessors (managed equivalent of AngelScript CustomEntity_Add/HasAny/GetOne/GetAll),
    // backing generated Add<X>/Has<X>s/Get<X>/Get<X>s methods for metadata EntityHolder entries.
    internal static IntPtr CreateInnerEntity(IntPtr holderPtr, int entryId, IntPtr protoId)
    {
        return CreateInnerEntityInternal(BoundBackend, holderPtr, entryId, protoId);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern IntPtr CreateInnerEntityInternal(IntPtr backend, IntPtr holderPtr, int entryId,
                                                           IntPtr protoId);

    internal static bool HasInnerEntities(IntPtr holderPtr, int entryId)
    {
        return HasInnerEntitiesInternal(BoundBackend, holderPtr, entryId);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern bool HasInnerEntitiesInternal(IntPtr backend, IntPtr holderPtr, int entryId);

    internal static IntPtr GetInnerEntity(IntPtr holderPtr, int entryId, long id)
    {
        return GetInnerEntityInternal(BoundBackend, holderPtr, entryId, id);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern IntPtr GetInnerEntityInternal(IntPtr backend, IntPtr holderPtr, int entryId, long id);

    internal static int FillInnerEntities(IntPtr holderPtr, int entryId, ref IntPtr buffer, int capacity)
    {
        string ? error;
        int count = FillInnerEntitiesInternal(BoundBackend, holderPtr, entryId, ref buffer, capacity, out error);
        ThrowNativeError(error);
        return count;
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern int FillInnerEntitiesInternal(IntPtr backend, IntPtr holderPtr, int entryId,
                                                        ref IntPtr buffer, int capacity, out string? error);

    internal static long GetAndResetInnerEntityVisits()
    {
        return GetAndResetInnerEntityVisitsInternal(BoundBackend);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern long GetAndResetInnerEntityVisitsInternal(IntPtr backend);

    // Diagnostic counters of native-to-managed callback dispatches: through a generated typed adapter, or through the
    // boxed DynamicInvoke path. The interop tests read them; they cost one increment per dispatch
    internal static long GetAndResetTypedCallbackDispatches()
    {
        return GetAndResetTypedCallbackDispatchesInternal(BoundBackend);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern long GetAndResetTypedCallbackDispatchesInternal(IntPtr backend);

    internal static long GetAndResetBoxedCallbackDispatches()
    {
        return GetAndResetBoxedCallbackDispatchesInternal(BoundBackend);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern long GetAndResetBoxedCallbackDispatchesInternal(IntPtr backend);

    // Drives the InteropProbe adapter from a native loop over one transport and returns the loop time in nanoseconds
    internal static long ProbeCallbackTransport(Delegate handler, int mode, int iterations, IntPtr ucoEntry,
                                                int registrationId)
    {
        string ? error;
        long elapsedNs = ProbeCallbackTransportInternal(BoundBackend,
                                                        handler,
                                                        mode,
                                                        iterations,
                                                        ucoEntry,
                                                        registrationId,
                                                        out error);
        ThrowNativeError(error);
        return elapsedNs;
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern long ProbeCallbackTransportInternal(IntPtr backend, Delegate handler, int mode,
                                                              int iterations, IntPtr ucoEntry, int registrationId,
                                                              out string? error);

    // Calls one probe adapter over one transport, optionally from a native thread of its own, and returns how many
    // calls came back with a managed exception
    internal static int ProbeTransportScenario(Delegate handler, int transport, int adapterKind, int iterations,
                                               bool externalThread, IntPtr ucoEntry, int registrationId)
    {
        int faults;
        string ? error;
        ProbeTransportScenarioInternal(BoundBackend,
                                       handler,
                                       transport,
                                       adapterKind,
                                       iterations,
                                       externalThread,
                                       ucoEntry,
                                       registrationId,
                                       out faults,
                                       out error);
        ThrowNativeError(error);
        return faults;
    }

    // Switches the calling thread's native interop counters on or off and reads them. Native allocation counts exist
    // only in profiling builds; the result says whether they were available
    internal static bool ReadInteropCounters(bool enable, out long gcHandles, out long metadataLookups,
                                             out long managedObjects, out long nativeAllocations, out long nativeBytes)
    {
        return ReadInteropCountersInternal(enable,
                                           out gcHandles,
                                           out metadataLookups,
                                           out managedObjects,
                                           out nativeAllocations,
                                           out nativeBytes);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern bool ReadInteropCountersInternal(bool enable, out long gcHandles, out long metadataLookups,
                                                           out long managedObjects, out long nativeAllocations,
                                                           out long nativeBytes);

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void ProbeTransportScenarioInternal(IntPtr backend, Delegate handler, int transport,
                                                              int adapterKind, int iterations, bool externalThread,
                                                              IntPtr ucoEntry, int registrationId, out int faults,
                                                              out string? error);

    // Generic property accessors by index (mirror AngelScript Entity_GetValueAsInt/SetValueAsInt and
    // Entity_GetValueAsAny/SetValueAsAny); back the generated Entity.GetAs*/SetAs* wrappers.
    // propIndex is the property enum's member value.
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static int GetEntityValueAsInt(IntPtr entityPtr, int propIndex)
    {
        string ? error;
        int value = GetEntityValueAsIntInternal(BoundBackend, entityPtr, propIndex, out error);
        ThrowNativeError(error);
        return value;
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern int GetEntityValueAsIntInternal(IntPtr backend, IntPtr entityPtr, int propIndex,
                                                          out string? error);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void SetEntityValueAsInt(IntPtr entityPtr, int propIndex, int value)
    {
        ThrowNativeError(SetEntityValueAsIntInternal(BoundBackend, entityPtr, propIndex, value));
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern string? SetEntityValueAsIntInternal(IntPtr backend, IntPtr entityPtr, int propIndex,
                                                              int value);

    internal static string GetEntityValueAsAny(IntPtr entityPtr, int propIndex)
    {
        string ? error;
        string? value = GetEntityValueAsAnyInternal(BoundBackend, entityPtr, propIndex, out error);
        ThrowNativeError(error);
        return value!;
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern string? GetEntityValueAsAnyInternal(IntPtr backend, IntPtr entityPtr, int propIndex,
                                                              out string? error);

    internal static void SetEntityValueAsAny(IntPtr entityPtr, int propIndex, string value)
    {
        ThrowNativeError(SetEntityValueAsAnyInternal(BoundBackend, entityPtr, propIndex, value));
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern string? SetEntityValueAsAnyInternal(IntPtr backend, IntPtr entityPtr, int propIndex,
                                                              string value);

    // The engine keeps entity event subscriptions on the entity and matches handlers as C# delegates do, so any
    // wrapper of the entity reaches the same subscriptions
    internal static void SubscribeEvent(int eventId, IntPtr entityPtr, Delegate handler, bool hasExplicitResult,
                                        int priority)
    {
        SubscribeEventInternal(BoundBackend, eventId, entityPtr, handler, hasExplicitResult, priority);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void SubscribeEventInternal(IntPtr backend, int eventId, IntPtr entityPtr, Delegate handler,
                                                      bool hasExplicitResult, int priority);

    internal static void UnsubscribeEvent(int eventId, IntPtr entityPtr, Delegate handler)
    {
        UnsubscribeEventInternal(BoundBackend, eventId, entityPtr, handler);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void UnsubscribeEventInternal(IntPtr backend, int eventId, IntPtr entityPtr,
                                                        Delegate handler);

    internal static void UnsubscribeAllEvents(int eventId, IntPtr entityPtr)
    {
        UnsubscribeAllEventsInternal(BoundBackend, eventId, entityPtr);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void UnsubscribeAllEventsInternal(IntPtr backend, int eventId, IntPtr entityPtr);

    internal static int FireEventBoxed(int eventId, IntPtr entityPtr, object?[] args)
    {
        string ? error;
        int result = FireEventBoxedInternal(BoundBackend, eventId, entityPtr, args, out error);
        ThrowNativeError(error);
        return result;
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern int FireEventBoxedInternal(IntPtr backend, int eventId, IntPtr entityPtr, object?[] args,
                                                     out string? error);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static int FireEventIndexed(int eventId, IntPtr entityPtr, ref byte frame, int frameSize)
    {
        string ? error;
        int result = FireEventIndexedInternal(BoundBackend, eventId, entityPtr, ref frame, frameSize, out error);
        ThrowNativeError(error);
        return result;
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern int FireEventIndexedInternal(IntPtr backend, int eventId, IntPtr entityPtr, ref byte frame,
                                                       int frameSize, out string? error);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static int EnumToInt32<TProp>(TProp prop)
        where TProp : unmanaged, Enum
    {
        if (Unsafe.SizeOf<TProp>() == 4) {
            return Unsafe.As<TProp, int>(ref prop);
        }

        if (Unsafe.SizeOf<TProp>() == 8) {
            return checked((int)Unsafe.As<TProp, long>(ref prop));
        }

        if (Unsafe.SizeOf<TProp>() == 2) {
            return Unsafe.As<TProp, short>(ref prop);
        }

        return Unsafe.As<TProp, byte>(ref prop);
    }

    internal static void BindAbi(ulong hash, int methodCount, int eventCount, int settingCount, int innerCount)
    {
        ThrowNativeError(BindAbiInternal(BoundBackend, hash, methodCount, eventCount, settingCount, innerCount));
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern string? BindAbiInternal(IntPtr backend, ulong hash, int methodCount, int eventCount,
                                                  int settingCount, int innerCount);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static T GetPropertyValue<T>(IntPtr entityPtr, int propIndex)
        where T : unmanaged
    {
        T value = default;
        ThrowNativeError(GetPropertyValueInternal(BoundBackend,
                                                  entityPtr,
                                                  propIndex,
                                                  ref Unsafe.As<T, byte>(ref value),
                                                  Unsafe.SizeOf<T>()));
        return value;
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern string? GetPropertyValueInternal(IntPtr backend, IntPtr entityPtr, int propIndex,
                                                           ref byte value, int size);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void SetPropertyValue<T>(IntPtr entityPtr, int propIndex, T value)
        where T : unmanaged
    {
        ThrowNativeError(SetPropertyValueInternal(BoundBackend,
                                                  entityPtr,
                                                  propIndex,
                                                  ref Unsafe.As<T, byte>(ref value),
                                                  Unsafe.SizeOf<T>()));
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern string? SetPropertyValueInternal(IntPtr backend, IntPtr entityPtr, int propIndex,
                                                           ref byte value, int size);

    private const int PropertyListStackBytes = 512;

    // An array property of fixed-size values crosses as raw bytes: the native side copies the elements straight
    // into the list's storage, with no per-element boxing or managed call. A list that fits the stack buffer
    // costs one crossing; a longer one is read again into storage sized from the first answer
    internal static List<T> GetPropertyList<T>(IntPtr entityPtr, int propIndex)
        where T : unmanaged
    {
        int elementSize = Unsafe.SizeOf<T>();
        Span<byte> stack = stackalloc byte[PropertyListStackBytes];
        int size;
        ThrowNativeError(GetPropertyArrayInternal(BoundBackend,
                                                  entityPtr,
                                                  propIndex,
                                                  ref MemoryMarshal.GetReference(stack),
                                                  stack.Length,
                                                  elementSize,
                                                  out size));

        List<T> list = new List<T>(size / elementSize);
        CollectionsMarshal.SetCount(list, size / elementSize);
        Span<byte> items = MemoryMarshal.AsBytes(CollectionsMarshal.AsSpan(list));

        if (size <= stack.Length) {
            stack.Slice(0, size).CopyTo(items);
            return list;
        }

        int secondSize;
        ThrowNativeError(GetPropertyArrayInternal(BoundBackend,
                                                  entityPtr,
                                                  propIndex,
                                                  ref MemoryMarshal.GetReference(items),
                                                  items.Length,
                                                  elementSize,
                                                  out secondSize));
        Invariant.Verify(secondSize == size,
                         "Array property must keep its size between two reads under one cover",
                         propIndex,
                         size,
                         secondSize);
        return list;
    }

    internal static void SetPropertyList<T>(IntPtr entityPtr, int propIndex, List<T> value)
        where T : unmanaged
    {
        ArgumentNullException.ThrowIfNull(value);
        Span<byte> items = MemoryMarshal.AsBytes(CollectionsMarshal.AsSpan(value));
        ThrowNativeError(SetPropertyArrayInternal(BoundBackend,
                                                  entityPtr,
                                                  propIndex,
                                                  ref MemoryMarshal.GetReference(items),
                                                  items.Length,
                                                  Unsafe.SizeOf<T>()));
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern string? GetPropertyArrayInternal(IntPtr backend, IntPtr entityPtr, int propIndex,
                                                           ref byte buffer, int capacity, int elementSize,
                                                           out int size);

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern string? SetPropertyArrayInternal(IntPtr backend, IntPtr entityPtr, int propIndex,
                                                           ref byte buffer, int size, int elementSize);

    // Boxed bridge for the values no fixed layout carries: strings, dictionaries, arrays of strings and ref types.
    // The property travels as its registrar index, never as a pair of names
    internal static object GetProperty(IntPtr entityPtr, int propIndex)
    {
        string ? error;
        object? value = GetPropertyInternal(BoundBackend, entityPtr, propIndex, out error);
        ThrowNativeError(error);
        return value!;
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern object? GetPropertyInternal(IntPtr backend, IntPtr entityPtr, int propIndex,
                                                      out string? error);

    internal static void SetProperty(IntPtr entityPtr, int propIndex, object? value)
    {
        ThrowNativeError(SetPropertyInternal(BoundBackend, entityPtr, propIndex, value));
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern string? SetPropertyInternal(IntPtr backend, IntPtr entityPtr, int propIndex, object? value);

    internal static void SetPropertyGetter(string ownerType, string propertyName, Delegate getter)
    {
        SetPropertyGetterInternal(BoundBackend, ownerType, propertyName, getter);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void SetPropertyGetterInternal(IntPtr backend, string ownerType, string propertyName,
                                                         Delegate getter);

    internal static void AddPropertySetter(string ownerType, string propertyName, Delegate setter)
    {
        AddPropertySetterInternal(BoundBackend, ownerType, propertyName, setter);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void AddPropertySetterInternal(IntPtr backend, string ownerType, string propertyName,
                                                         Delegate setter);

    internal static void AddPropertySetterWithProperty(string ownerType, string propertyName, Delegate setter)
    {
        AddPropertySetterWithPropertyInternal(BoundBackend, ownerType, propertyName, setter);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void AddPropertySetterWithPropertyInternal(IntPtr backend, string ownerType,
                                                                     string propertyName, Delegate setter);

    internal static void AddPropertyDeferredSetter(string ownerType, string propertyName, Delegate setter)
    {
        AddPropertyDeferredSetterInternal(BoundBackend, ownerType, propertyName, setter);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void AddPropertyDeferredSetterInternal(IntPtr backend, string ownerType, string propertyName,
                                                                 Delegate setter);

    internal static object CallMethodBoxed(int methodId, IntPtr entityPtr, object?[] args)
    {
        string ? error;
        object? value = CallMethodBoxedInternal(BoundBackend, methodId, entityPtr, args, out error);
        ThrowNativeError(error);
        return value!;
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern object? CallMethodBoxedInternal(IntPtr backend, int methodId, IntPtr entityPtr,
                                                          object?[] args, out string? error);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void CallMethodIndexed(int methodId, IntPtr entityPtr, ref byte frame, int frameSize)
    {
        ThrowNativeError(CallMethodIndexedInternal(BoundBackend, methodId, entityPtr, ref frame, frameSize));
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern string? CallMethodIndexedInternal(IntPtr backend, int methodId, IntPtr entityPtr,
                                                            ref byte frame, int frameSize);

    // Mono inlines no method that makes a call unless told to, so the check is inlined and the throw is kept out
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static void ThrowNativeError(string? error)
    {
        if (error != null) {
            ThrowNativeCallException(error);
        }
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static void ThrowNativeCallException(string error)
    {
        throw new NativeCallException(error);
    }

    // Outcome of a managed -> script invocation. Mirrors INVOKE_STATUS_* in ManagedScriptBackend.cpp.
    // Kept distinct because a single bool made "no such function" and "the call failed" the same answer,
    // and callers legitimately assert the former as a missing bridge.
    internal const int ScriptInvokeStatusFailed = -1;
    internal const int ScriptInvokeStatusNoCandidate = 0;
    internal const int ScriptInvokeStatusCompleted = 1;

    internal static int InvokeScriptFuncStatus(string funcName, object?[] args)
    {
        return InvokeScriptFuncStatusInternal(BoundBackend, funcName, args);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern int InvokeScriptFuncStatusInternal(IntPtr backend, string funcName, object?[] args);

    // A failed invocation is an error at the callee, not a missing entry: surface it instead of letting the
    // caller mistake it for one. The engine has already logged the underlying exception with its stack.
    internal static bool InvokeScriptFunc(string funcName, object?[] args)
    {
        int status = InvokeScriptFuncStatus(funcName, args);

        if (status == ScriptInvokeStatusFailed) {
            throw new InvalidOperationException("Script function invocation failed: " + funcName);
        }

        return status == ScriptInvokeStatusCompleted;
    }

    // Registers a managed global script function into the engine's cross-backend function map under a named
    // marker attribute, so a consumer that resolves funcs by attribute (ScriptSystem::FindFunc) can invoke it.
    // paramTypeNames/returnTypeName are engine base-type names; the engine builds the matching signature.
    internal static void RegisterGlobalScriptFunc(string fullName, string attributeName, string[] paramTypeNames,
                                                  string returnTypeName, Delegate handler)
    {
        RegisterGlobalScriptFuncInternal(BoundBackend,
                                         fullName,
                                         attributeName,
                                         paramTypeNames,
                                         returnTypeName,
                                         handler);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void RegisterGlobalScriptFuncInternal(IntPtr backend, string fullName, string attributeName,
                                                                string[] paramTypeNames, string returnTypeName,
                                                                Delegate handler);

    // Registers a managed inbound remote-call handler (a [ServerRemoteCall]/[ClientRemoteCall]/[AdminRemoteCall]
    // method) with the engine. The engine matches `name` to the inbound remote-call metadata (subsystem "cs")
    // for this side; if it is inbound here, it wires engine->SetRemoteCallHandler to deserialize the wire args
    // (shared RemoteCallWire format) and invoke the handler. `paramCount` is the C# method's parameter count
    // (including the leading Player on the server side) for an arity sanity-check. No-op when the name is not
    // inbound on this side (e.g. the opposite side's outbound caller).
    internal static void RegisterRemoteCallHandler(string name, int paramCount, Delegate handler)
    {
        RegisterRemoteCallHandlerInternal(BoundBackend, name, paramCount, handler);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void RegisterRemoteCallHandlerInternal(IntPtr backend, string name, int paramCount,
                                                                 Delegate handler);

    // Serializes the boxed args (shared RemoteCallWire format) and sends the named outbound "cs" remote call to
    // the remote peer via the engine. `caller` is the entity the call is bound to (e.g. the Player).
    internal static void SendRemoteCall(object? caller, string name, object?[] args)
    {
        SendRemoteCallInternal(BoundBackend, caller, name, args);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void SendRemoteCallInternal(IntPtr backend, object? caller, string name, object?[] args);

    // Diagnostic/test: serializes the boxed args and dispatches them through the engine's real inbound
    // remote-call path in-process (no network peer), invoking the registered handler for the named inbound
    // "cs" remote call. Used to exercise the managed serialize -> deserialize -> dispatch glue on one side.
    internal static void LoopbackRemoteCall(object? caller, string name, object?[] args)
    {
        LoopbackRemoteCallInternal(BoundBackend, caller, name, args);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void LoopbackRemoteCallInternal(IntPtr backend, object? caller, string name, object?[] args);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static T GetSettingValue<T>(int settingId)
        where T : unmanaged
    {
        T value = default;
        ThrowNativeError(
            GetSettingValueInternal(BoundBackend, settingId, ref Unsafe.As<T, byte>(ref value), Unsafe.SizeOf<T>()));
        return value;
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern string? GetSettingValueInternal(IntPtr backend, int settingId, ref byte value, int size);

    internal static bool GetSettingBool(string name)
    {
        return GetSettingBoolRaw(BoundBackend, name) != 0;
    }

    internal static void SetSettingBool(string name, bool value)
    {
        SetSettingBoolRaw(BoundBackend, name, value ? 1 : 0);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern int GetSettingBoolRaw(IntPtr backend, string name);

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void SetSettingBoolRaw(IntPtr backend, string name, int value);

    internal static int GetSettingInt(string name)
    {
        return GetSettingIntInternal(BoundBackend, name);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern int GetSettingIntInternal(IntPtr backend, string name);

    internal static void SetSettingInt(string name, int value)
    {
        SetSettingIntInternal(BoundBackend, name, value);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void SetSettingIntInternal(IntPtr backend, string name, int value);

    internal static uint GetSettingUInt(string name)
    {
        return GetSettingUIntInternal(BoundBackend, name);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern uint GetSettingUIntInternal(IntPtr backend, string name);

    internal static void SetSettingUInt(string name, uint value)
    {
        SetSettingUIntInternal(BoundBackend, name, value);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void SetSettingUIntInternal(IntPtr backend, string name, uint value);

    internal static long GetSettingLong(string name)
    {
        return GetSettingLongInternal(BoundBackend, name);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern long GetSettingLongInternal(IntPtr backend, string name);

    internal static void SetSettingLong(string name, long value)
    {
        SetSettingLongInternal(BoundBackend, name, value);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void SetSettingLongInternal(IntPtr backend, string name, long value);

    internal static ulong GetSettingULong(string name)
    {
        return GetSettingULongInternal(BoundBackend, name);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern ulong GetSettingULongInternal(IntPtr backend, string name);

    internal static void SetSettingULong(string name, ulong value)
    {
        SetSettingULongInternal(BoundBackend, name, value);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void SetSettingULongInternal(IntPtr backend, string name, ulong value);

    internal static float GetSettingFloat(string name)
    {
        return GetSettingFloatInternal(BoundBackend, name);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern float GetSettingFloatInternal(IntPtr backend, string name);

    internal static void SetSettingFloat(string name, float value)
    {
        SetSettingFloatInternal(BoundBackend, name, value);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void SetSettingFloatInternal(IntPtr backend, string name, float value);

    internal static double GetSettingDouble(string name)
    {
        return GetSettingDoubleInternal(BoundBackend, name);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern double GetSettingDoubleInternal(IntPtr backend, string name);

    internal static void SetSettingDouble(string name, double value)
    {
        SetSettingDoubleInternal(BoundBackend, name, value);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void SetSettingDoubleInternal(IntPtr backend, string name, double value);

    internal static string GetSettingString(string name)
    {
        return GetSettingStringInternal(BoundBackend, name);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern string GetSettingStringInternal(IntPtr backend, string name);

    internal static void SetSettingString(string name, string value)
    {
        SetSettingStringInternal(BoundBackend, name, value);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern void SetSettingStringInternal(IntPtr backend, string name, string value);

    private static readonly char[] SettingListSeparators = new char[] { ' ', '\t', '\r', '\n' };

    private static List<T> ParseSettingList<T>(string value, Func<string, T> parse)
    {
        List<T> result = new List<T>();

        if (string.IsNullOrWhiteSpace(value)) {
            return result;
        }

        foreach (string part in value.Split(SettingListSeparators, StringSplitOptions.RemoveEmptyEntries)) {
            result.Add(parse(part));
        }

        return result;
    }

    private static string JoinSettingList<T>(IEnumerable<T> values, Func<T, string> format)
    {
        if (values == null) {
            return string.Empty;
        }

        List<string> parts = new List<string>();

        foreach (T value in values) {
            parts.Add(format(value));
        }

        return string.Join(" ", parts);
    }

    private static bool ParseSettingBool(string value)
    {
        if (int.TryParse(value, out int intValue)) {
            return intValue != 0;
        }

        return bool.Parse(value);
    }

    internal static List<bool> GetSettingBoolList(string name) => ParseSettingList(GetSettingString(name),
                                                                                   ParseSettingBool);

    internal static void SetSettingBoolList(string name, List<bool> value) =>
        SetSettingString(name, JoinSettingList(value, item => item ? "True" : "False"));

    internal static List<sbyte> GetSettingSByteList(string name) => ParseSettingList(
        GetSettingString(name), item => sbyte.Parse(item, System.Globalization.NumberStyles.Integer,
                                                    System.Globalization.CultureInfo.InvariantCulture));

    internal static void SetSettingSByteList(string name, List<sbyte> value) => SetSettingString(
        name, JoinSettingList(value, item => item.ToString(System.Globalization.CultureInfo.InvariantCulture)));

    internal static List<byte> GetSettingByteList(string name) =>
        ParseSettingList(GetSettingString(name), item => byte.Parse(item, System.Globalization.NumberStyles.Integer,
                                                                    System.Globalization.CultureInfo.InvariantCulture));

    internal static void SetSettingByteList(string name, List<byte> value) => SetSettingString(
        name, JoinSettingList(value, item => item.ToString(System.Globalization.CultureInfo.InvariantCulture)));

    internal static List<short> GetSettingShortList(string name) => ParseSettingList(
        GetSettingString(name), item => short.Parse(item, System.Globalization.NumberStyles.Integer,
                                                    System.Globalization.CultureInfo.InvariantCulture));

    internal static void SetSettingShortList(string name, List<short> value) => SetSettingString(
        name, JoinSettingList(value, item => item.ToString(System.Globalization.CultureInfo.InvariantCulture)));

    internal static List<ushort> GetSettingUShortList(string name) => ParseSettingList(
        GetSettingString(name), item => ushort.Parse(item, System.Globalization.NumberStyles.Integer,
                                                     System.Globalization.CultureInfo.InvariantCulture));

    internal static void SetSettingUShortList(string name, List<ushort> value) => SetSettingString(
        name, JoinSettingList(value, item => item.ToString(System.Globalization.CultureInfo.InvariantCulture)));

    internal static List<int> GetSettingIntList(string name) =>
        ParseSettingList(GetSettingString(name), item => int.Parse(item, System.Globalization.NumberStyles.Integer,
                                                                   System.Globalization.CultureInfo.InvariantCulture));

    internal static void SetSettingIntList(string name, List<int> value) => SetSettingString(
        name, JoinSettingList(value, item => item.ToString(System.Globalization.CultureInfo.InvariantCulture)));

    internal static List<uint> GetSettingUIntList(string name) =>
        ParseSettingList(GetSettingString(name), item => uint.Parse(item, System.Globalization.NumberStyles.Integer,
                                                                    System.Globalization.CultureInfo.InvariantCulture));

    internal static void SetSettingUIntList(string name, List<uint> value) => SetSettingString(
        name, JoinSettingList(value, item => item.ToString(System.Globalization.CultureInfo.InvariantCulture)));

    internal static List<long> GetSettingLongList(string name) =>
        ParseSettingList(GetSettingString(name), item => long.Parse(item, System.Globalization.NumberStyles.Integer,
                                                                    System.Globalization.CultureInfo.InvariantCulture));

    internal static void SetSettingLongList(string name, List<long> value) => SetSettingString(
        name, JoinSettingList(value, item => item.ToString(System.Globalization.CultureInfo.InvariantCulture)));

    internal static List<ulong> GetSettingULongList(string name) => ParseSettingList(
        GetSettingString(name), item => ulong.Parse(item, System.Globalization.NumberStyles.Integer,
                                                    System.Globalization.CultureInfo.InvariantCulture));

    internal static void SetSettingULongList(string name, List<ulong> value) => SetSettingString(
        name, JoinSettingList(value, item => item.ToString(System.Globalization.CultureInfo.InvariantCulture)));

    internal static List<float> GetSettingFloatList(string name) => ParseSettingList(
        GetSettingString(name), item => float.Parse(item, System.Globalization.NumberStyles.Float,
                                                    System.Globalization.CultureInfo.InvariantCulture));

    internal static void SetSettingFloatList(string name, List<float> value) => SetSettingString(
        name, JoinSettingList(value, item => item.ToString(System.Globalization.CultureInfo.InvariantCulture)));

    internal static List<double> GetSettingDoubleList(string name) => ParseSettingList(
        GetSettingString(name), item => double.Parse(item, System.Globalization.NumberStyles.Float,
                                                     System.Globalization.CultureInfo.InvariantCulture));

    internal static void SetSettingDoubleList(string name, List<double> value) => SetSettingString(
        name, JoinSettingList(value, item => item.ToString(System.Globalization.CultureInfo.InvariantCulture)));

    internal static List<string> GetSettingStringList(string name) => ParseSettingList(GetSettingString(name),
                                                                                       item => item);

    internal static void SetSettingStringList(string name, List<string> value) =>
        SetSettingString(name, JoinSettingList(value, item => item ?? string.Empty));
}
