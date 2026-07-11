#nullable enable

using System;
using System.Collections;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;

namespace FOnline
{
    // Invoked by the engine when a virtual property with a managed setter is written; the setter may
    // mutate value, and the engine stores the result. Registered via Game.AddPropertySetter.
    public delegate void PropertySetter<TEntity, TValue>(TEntity entity, ref TValue value);
    public delegate void PropertySetterWithProperty<TEntity, TProperty, TValue>(TEntity entity, TProperty property, ref TValue value);

    internal static class Native
    {
        internal static T? WrapEntity<T>(IntPtr entityPtr) where T : Entity
        {
            if (entityPtr == IntPtr.Zero)
            {
                return null;
            }

            return (T)Activator.CreateInstance(
                typeof(T),
                BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic,
                null,
                new object[]
                {
                    entityPtr,
                },
                null)!;
        }

        internal static T? WrapRef<T>(IntPtr refPtr)
        {
            if (refPtr == IntPtr.Zero)
            {
                return default;
            }

            return (T)Activator.CreateInstance(
                typeof(T),
                BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic,
                null,
                new object[]
                {
                    refPtr,
                },
                null)!;
        }

        internal static EventResult InvokeEvent(
            Delegate handler,
            bool hasExplicitResult,
            object[] args)
        {
            try
            {
                object? result = handler.DynamicInvoke(AdaptInvokeArgs(handler, args));
                Task? task = result as Task;

                if (task != null)
                {
                    if (hasExplicitResult)
                    {
                        throw new InvalidOperationException("Task event handlers cannot return an explicit EventResult");
                    }

                    if (task.IsCompleted)
                    {
                        task.GetAwaiter().GetResult();
                    }
                    else
                    {
                        Game.ObserveInvokeTask(task);
                    }

                    return EventResult.ContinueChain;
                }

                if (hasExplicitResult)
                {
                    return (EventResult)result!;
                }

                return EventResult.ContinueChain;
            }
            catch (Exception ex)
            {
                Game.RecordManagedException(UnwrapInvocationException(ex), true);
                return EventResult.StopChain;
            }
        }

        internal static object? InvokeCallback(Delegate handler, object[] args)
        {
            try
            {
                object? result = handler.DynamicInvoke(AdaptInvokeArgs(handler, args));
                Task? task = result as Task;

                if (task == null)
                {
                    return result;
                }

                task.GetAwaiter().GetResult();

                Type taskType = result!.GetType();
                if (taskType.IsGenericType)
                {
                    return taskType.GetProperty("Result")!.GetValue(result);
                }

                return null;
            }
            catch (Exception ex)
            {
                Game.RecordManagedException(UnwrapInvocationException(ex), false);
                throw;
            }
        }

        private static object[] AdaptInvokeArgs(Delegate handler, object[] args)
        {
            ParameterInfo[] parameters = handler.Method.GetParameters();
            object[]? adaptedArgs = null;

            for (int i = 0; i < args.Length && i < parameters.Length; i++)
            {
                Type parameterType = parameters[i].ParameterType;

                if (parameterType == typeof(Dictionary<string, string>) &&
                    args[i] is IDictionary source &&
                    !(args[i] is Dictionary<string, string>))
                {
                    adaptedArgs = adaptedArgs ?? (object[])args.Clone();
                    adaptedArgs[i] = StringifyDictionary(source);
                }
                else if (parameterType == typeof(List<string>) &&
                    args[i] is IEnumerable sourceList &&
                    !(args[i] is List<string>) &&
                    !(args[i] is string))
                {
                    adaptedArgs = adaptedArgs ?? (object[])args.Clone();
                    adaptedArgs[i] = StringifyList(sourceList);
                }
            }

            return adaptedArgs ?? args;
        }

        private static Dictionary<string, string> StringifyDictionary(IDictionary source)
        {
            Dictionary<string, string> result = new Dictionary<string, string>();

            foreach (DictionaryEntry entry in source)
            {
                string? key = entry.Key as string;

                if (key == null)
                {
                    continue;
                }

                result[key] = entry.Value?.ToString() ?? string.Empty;
            }

            return result;
        }

        private static List<string> StringifyList(IEnumerable source)
        {
            List<string> result = new List<string>();

            foreach (object entry in source)
            {
                result.Add(entry?.ToString() ?? string.Empty);
            }

            return result;
        }

        private static Exception UnwrapInvocationException(Exception ex)
        {
            TargetInvocationException? invocation = ex as TargetInvocationException;
            return invocation != null && invocation.InnerException != null
                ? invocation.InnerException
                : ex;
        }

        internal static bool IsList(object value)
        {
            return value is IList;
        }

        internal static bool IsDictionary(object value)
        {
            return value is IDictionary;
        }

        internal static bool IsDelegate(object value)
        {
            return value is Delegate;
        }

        internal static void RequireEventAttribute(Delegate handler)
        {
            RequireMethodAttribute<EventAttribute>(handler);
        }

        private static void RequireMethodAttribute<TAttribute>(Delegate handler)
            where TAttribute : Attribute
        {
            Delegate[] invocationList = handler.GetInvocationList();

            for (int i = 0; i < invocationList.Length; i++)
            {
                Delegate item = invocationList[i];
                MethodInfo method = item.Method;

                if (Attribute.GetCustomAttribute(method, typeof(TAttribute)) != null)
                {
                    continue;
                }

                string message =
                    GetMethodName(method) + " must be marked " + GetAttributeName(typeof(TAttribute));
                throw new InvalidOperationException(message);
            }
        }

        private static string GetMethodName(MethodInfo method)
        {
            string? typeName = method.DeclaringType != null
                ? method.DeclaringType.FullName
                : string.Empty;

            if (string.IsNullOrEmpty(typeName))
            {
                return method.Name;
            }

            return typeName + "." + method.Name;
        }

        private static string GetAttributeName(Type attributeType)
        {
            const string suffix = "Attribute";
            string name = attributeType.Name;

            if (name.EndsWith(suffix, StringComparison.Ordinal))
            {
                name = name.Substring(0, name.Length - suffix.Length);
            }

            return "[" + name + "]";
        }

        internal static string GetDelegateKey(Delegate handler)
        {
            if (handler == null)
            {
                return string.Empty;
            }

            string key = string.Empty;
            Delegate[] invocationList = handler.GetInvocationList();

            for (int i = 0; i < invocationList.Length; i++)
            {
                Delegate item = invocationList[i];
                MethodInfo method = item.Method;
                object? target = item.Target;

                if (key.Length != 0)
                {
                    key += "|";
                }

                key += method.Module.ModuleVersionId.ToString();
                key += ":";
                key += method.MetadataToken.ToString();
                key += ":";
                key += method.DeclaringType != null ? method.DeclaringType.FullName : string.Empty;
                key += ":";
                key += method.Name;
                key += ":";
                key += target != null ? RuntimeHelpers.GetHashCode(target).ToString() : "static";
            }

            return key;
        }

        internal static int GetListCount(object value)
        {
            return ((IList)value).Count;
        }

        internal static object? GetListItem(object value, int index)
        {
            return ((IList)value)[index];
        }

        internal static int GetDictionaryCount(object value)
        {
            return ((IDictionary)value).Count;
        }

        internal static object GetDictionaryKey(object value, int index)
        {
            int i = 0;
            foreach (DictionaryEntry entry in (IDictionary)value)
            {
                if (i == index)
                {
                    return entry.Key;
                }

                i++;
            }

            throw new IndexOutOfRangeException();
        }

        internal static object? GetDictionaryValue(object value, int index)
        {
            int i = 0;
            foreach (DictionaryEntry entry in (IDictionary)value)
            {
                if (i == index)
                {
                    return entry.Value;
                }

                i++;
            }

            throw new IndexOutOfRangeException();
        }

        internal static object CreateList(Type elementType)
        {
            return Activator.CreateInstance(
                typeof(List<>).MakeGenericType(elementType))!;
        }

        internal static void AddListItem(object list, object value)
        {
            ((IList)list).Add(value);
        }

        internal static object CreateDictionary(Type keyType, Type valueType)
        {
            return Activator.CreateInstance(
                typeof(Dictionary<,>).MakeGenericType(keyType, valueType))!;
        }

        internal static void AddDictionaryItem(object dictionary, object key, object value)
        {
            ((IDictionary)dictionary).Add(key, value);
        }

        internal static int GetByteArrayItem(object bytes, int index)
        {
            return ((byte[])bytes)[index];
        }

        internal static void SetByteArrayItem(object bytes, int index, int value)
        {
            ((byte[])bytes)[index] = (byte)value;
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Log(string text);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string GetHashStr(ulong value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern ulong GetHash(string text);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern long GetEntityId(IntPtr entityPtr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern ulong GetEntityProtoId(IntPtr entityPtr);

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

        // The calling engine's alive flag: a one-element bool array that turns false when the backend is torn
        // down. Capture it at construction time and read it from finalizers to ask "is my engine still alive"
        // (a static probe cannot answer that: several same-side engines may coexist in one process).
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool[] GetBackendAliveFlag();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr GetBackend();

        // Custom-entity proto lookup (mirrors AngelScript Game_GetProtoCustomEntity / Game_CheckProtoCustomEntity):
        // returns the proto entity pointer for `typeName`/`protoIdHash` (IntPtr.Zero if unknown), or whether it
        // exists. Backs the baker-generated Game.GetProto<X> / CheckProto<X> wrappers for custom HasProtos entities.
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr GetProtoEntity(string typeName, ulong protoIdHash);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool CheckProtoEntity(string typeName, ulong protoIdHash);

        // Plural proto enumeration (count + by-index) backing the generated Game.GetProto<X>s()/Get<X>s().
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int GetProtoEntityCount(string typeName);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr GetProtoEntityAt(string typeName, int index);

        // Entity-holder accessors (managed equivalent of AngelScript CustomEntity_Add/HasAny/GetOne/GetAll),
        // backing generated Add<X>/Has<X>s/Get<X>/Get<X>s methods for metadata EntityHolder entries.
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr CreateInnerEntity(IntPtr holderPtr, string entryName, ulong protoIdHash);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool HasInnerEntities(IntPtr holderPtr, string entryName);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr GetInnerEntity(IntPtr holderPtr, string entryName, long id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int GetInnerEntityCount(IntPtr holderPtr, string entryName);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr GetInnerEntityAt(IntPtr holderPtr, string entryName, int index);

        // Generic property accessors by index (mirror AngelScript Entity_GetValueAsInt/SetValueAsInt and
        // Entity_GetValueAsAny/SetValueAsAny); back the generated Entity.GetAs*/SetAs* wrappers.
        // propIndex is the property enum's member value.
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int GetEntityValueAsInt(IntPtr entityPtr, int propIndex);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetEntityValueAsInt(IntPtr entityPtr, int propIndex, int value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string GetEntityValueAsAny(IntPtr entityPtr, int propIndex);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetEntityValueAsAny(IntPtr entityPtr, int propIndex, string value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr SubscribeEvent(
            string ownerType,
            string eventName,
            IntPtr entityPtr,
            Delegate handler,
            bool hasExplicitResult,
            int priority);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void UnsubscribeEvent(
            string eventName,
            IntPtr entityPtr,
            IntPtr subscription);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int FireEvent(
            string ownerType,
            string eventName,
            IntPtr entityPtr,
            object[] args);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern object GetProperty(
            string ownerType,
            string propertyName,
            IntPtr entityPtr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetProperty(
            string ownerType,
            string propertyName,
            IntPtr entityPtr,
            object value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetPropertyGetter(
            string ownerType,
            string propertyName,
            Delegate getter);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void AddPropertySetter(
            string ownerType,
            string propertyName,
            Delegate setter);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void AddPropertySetterWithProperty(
            string ownerType,
            string propertyName,
            Delegate setter);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void AddPropertyDeferredSetter(
            string ownerType,
            string propertyName,
            Delegate setter);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern object CallMethod(
            string ownerType,
            string methodName,
            int methodIndex,
            IntPtr entityPtr,
            object[] args);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool InvokeScriptFunc(string funcName, object?[] args);

        // Registers a managed global script function into the engine's cross-backend function map under a named
        // marker attribute, so a consumer that resolves funcs by attribute (ScriptSystem::FindFunc) can invoke it.
        // paramTypeNames/returnTypeName are engine base-type names; the engine builds the matching signature.
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void RegisterGlobalScriptFunc(
            string fullName,
            string attributeName,
            string[] paramTypeNames,
            string returnTypeName,
            Delegate handler,
            bool skipExistingScriptFunc);

        // Registers a managed inbound remote-call handler (a [ServerRemoteCall]/[ClientRemoteCall]/[AdminRemoteCall]
        // method) with the engine. The engine matches `name` to the inbound remote-call metadata (subsystem "cs")
        // for this side; if it is inbound here, it wires engine->SetRemoteCallHandler to deserialize the wire args
        // (shared RemoteCallWire format) and invoke the handler. `paramCount` is the C# method's parameter count
        // (including the leading Player on the server side) for an arity sanity-check. No-op when the name is not
        // inbound on this side (e.g. the opposite side's outbound caller).
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void RegisterRemoteCallHandler(
            string name,
            int paramCount,
            Delegate handler);

        // Serializes the boxed args (shared RemoteCallWire format) and sends the named outbound "cs" remote call to
        // the remote peer via the engine. `caller` is the entity the call is bound to (e.g. the Player).
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SendRemoteCall(object? caller, string name, object[] args);

        // Diagnostic/test: serializes the boxed args and dispatches them through the engine's real inbound
        // remote-call path in-process (no network peer), invoking the registered handler for the named inbound
        // "cs" remote call. Used to exercise the managed serialize -> deserialize -> dispatch glue on one side.
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void LoopbackRemoteCall(object? caller, string name, object[] args);

        internal static bool GetSettingBool(string name)
        {
            return GetSettingBoolRaw(name) != 0;
        }

        internal static void SetSettingBool(string name, bool value)
        {
            SetSettingBoolRaw(name, value ? 1 : 0);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int GetSettingBoolRaw(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetSettingBoolRaw(string name, int value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int GetSettingInt(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetSettingInt(string name, int value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern uint GetSettingUInt(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetSettingUInt(string name, uint value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern long GetSettingLong(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetSettingLong(string name, long value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern ulong GetSettingULong(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetSettingULong(string name, ulong value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetSettingFloat(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetSettingFloat(string name, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern double GetSettingDouble(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetSettingDouble(string name, double value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string GetSettingString(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetSettingString(string name, string value);

        private static readonly char[] SettingListSeparators = new char[] { ' ', '\t', '\r', '\n' };

        private static List<T> ParseSettingList<T>(string value, Func<string, T> parse)
        {
            List<T> result = new List<T>();

            if (string.IsNullOrWhiteSpace(value))
            {
                return result;
            }

            foreach (string part in value.Split(SettingListSeparators, StringSplitOptions.RemoveEmptyEntries))
            {
                result.Add(parse(part));
            }

            return result;
        }

        private static string JoinSettingList<T>(IEnumerable<T> values, Func<T, string> format)
        {
            if (values == null)
            {
                return string.Empty;
            }

            List<string> parts = new List<string>();

            foreach (T value in values)
            {
                parts.Add(format(value));
            }

            return string.Join(" ", parts);
        }

        private static bool ParseSettingBool(string value)
        {
            if (int.TryParse(value, out int intValue))
            {
                return intValue != 0;
            }

            return bool.Parse(value);
        }

        internal static List<bool> GetSettingBoolList(string name) => ParseSettingList(GetSettingString(name), ParseSettingBool);

        internal static void SetSettingBoolList(string name, List<bool> value) => SetSettingString(name, JoinSettingList(value, item => item ? "True" : "False"));

        internal static List<sbyte> GetSettingSByteList(string name) => ParseSettingList(GetSettingString(name), item => sbyte.Parse(item, System.Globalization.NumberStyles.Integer, System.Globalization.CultureInfo.InvariantCulture));

        internal static void SetSettingSByteList(string name, List<sbyte> value) => SetSettingString(name, JoinSettingList(value, item => item.ToString(System.Globalization.CultureInfo.InvariantCulture)));

        internal static List<byte> GetSettingByteList(string name) => ParseSettingList(GetSettingString(name), item => byte.Parse(item, System.Globalization.NumberStyles.Integer, System.Globalization.CultureInfo.InvariantCulture));

        internal static void SetSettingByteList(string name, List<byte> value) => SetSettingString(name, JoinSettingList(value, item => item.ToString(System.Globalization.CultureInfo.InvariantCulture)));

        internal static List<short> GetSettingShortList(string name) => ParseSettingList(GetSettingString(name), item => short.Parse(item, System.Globalization.NumberStyles.Integer, System.Globalization.CultureInfo.InvariantCulture));

        internal static void SetSettingShortList(string name, List<short> value) => SetSettingString(name, JoinSettingList(value, item => item.ToString(System.Globalization.CultureInfo.InvariantCulture)));

        internal static List<ushort> GetSettingUShortList(string name) => ParseSettingList(GetSettingString(name), item => ushort.Parse(item, System.Globalization.NumberStyles.Integer, System.Globalization.CultureInfo.InvariantCulture));

        internal static void SetSettingUShortList(string name, List<ushort> value) => SetSettingString(name, JoinSettingList(value, item => item.ToString(System.Globalization.CultureInfo.InvariantCulture)));

        internal static List<int> GetSettingIntList(string name) => ParseSettingList(GetSettingString(name), item => int.Parse(item, System.Globalization.NumberStyles.Integer, System.Globalization.CultureInfo.InvariantCulture));

        internal static void SetSettingIntList(string name, List<int> value) => SetSettingString(name, JoinSettingList(value, item => item.ToString(System.Globalization.CultureInfo.InvariantCulture)));

        internal static List<uint> GetSettingUIntList(string name) => ParseSettingList(GetSettingString(name), item => uint.Parse(item, System.Globalization.NumberStyles.Integer, System.Globalization.CultureInfo.InvariantCulture));

        internal static void SetSettingUIntList(string name, List<uint> value) => SetSettingString(name, JoinSettingList(value, item => item.ToString(System.Globalization.CultureInfo.InvariantCulture)));

        internal static List<long> GetSettingLongList(string name) => ParseSettingList(GetSettingString(name), item => long.Parse(item, System.Globalization.NumberStyles.Integer, System.Globalization.CultureInfo.InvariantCulture));

        internal static void SetSettingLongList(string name, List<long> value) => SetSettingString(name, JoinSettingList(value, item => item.ToString(System.Globalization.CultureInfo.InvariantCulture)));

        internal static List<ulong> GetSettingULongList(string name) => ParseSettingList(GetSettingString(name), item => ulong.Parse(item, System.Globalization.NumberStyles.Integer, System.Globalization.CultureInfo.InvariantCulture));

        internal static void SetSettingULongList(string name, List<ulong> value) => SetSettingString(name, JoinSettingList(value, item => item.ToString(System.Globalization.CultureInfo.InvariantCulture)));

        internal static List<float> GetSettingFloatList(string name) => ParseSettingList(GetSettingString(name), item => float.Parse(item, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture));

        internal static void SetSettingFloatList(string name, List<float> value) => SetSettingString(name, JoinSettingList(value, item => item.ToString(System.Globalization.CultureInfo.InvariantCulture)));

        internal static List<double> GetSettingDoubleList(string name) => ParseSettingList(GetSettingString(name), item => double.Parse(item, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture));

        internal static void SetSettingDoubleList(string name, List<double> value) => SetSettingString(name, JoinSettingList(value, item => item.ToString(System.Globalization.CultureInfo.InvariantCulture)));

        internal static List<string> GetSettingStringList(string name) => ParseSettingList(GetSettingString(name), item => item);

        internal static void SetSettingStringList(string name, List<string> value) => SetSettingString(name, JoinSettingList(value, item => item ?? string.Empty));
    }
}
