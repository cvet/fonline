//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "ManagedScriptBackend.h"

#if FO_MANAGED_SCRIPTING

#include "Application.h"
#include "EngineBase.h"
#include "EntityProtos.h"
#include "FileSystem.h"
#include "Platform.h"
#include "Properties.h"
#include "RemoteCallWire.h"

#if FO_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

FO_DISABLE_WARNINGS_PUSH()
#include <mono/jit/jit.h>
#include <mono/metadata/appdomain.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/class.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/loader.h>
#include <mono/metadata/mono-config.h>
#include <mono/metadata/object.h>
#include <mono/metadata/reflection.h>
#include <mono/metadata/threads.h>
#include <mono/utils/mono-publib.h>
FO_DISABLE_WARNINGS_POP()

#include "WinApiUndef.inc"

FO_BEGIN_NAMESPACE

#if FO_WINDOWS
constexpr char MANAGED_ASSEMBLY_PATH_SEPARATOR = ';';
#else
constexpr char MANAGED_ASSEMBLY_PATH_SEPARATOR = ':';
#endif
constexpr string_view MANAGED_HOST_ASSEMBLY_FILE_NAME = "FOnline.ManagedHost.dll";
constexpr string_view MANAGED_HOST_NAMESPACE = "FOnline.ManagedHost";
constexpr string_view MANAGED_HOST_CLASS_NAME = "ManagedLoadContextHost";

// Embedded Mono can overlap normal execution across load contexts, but concurrent context loading corrupts its
// loader state. Serialize only assembly loading; managed engines still run in parallel.
static mutex ManagedAssemblyLoadLocker;

// Thread-affine active backend: threads are partitioned by engine ownership, so the thread-local slot never
// observes a foreign engine. Every native->managed invocation runs under ActiveBackendScope, making this the
// one and only resolution of "which engine is executing managed code right now".
static thread_local nptr<ManagedScriptBackend> ActiveBackend {};

class ActiveBackendScope final
{
public:
    explicit ActiveBackendScope(ptr<ManagedScriptBackend> backend) noexcept :
        _previous {ActiveBackend}
    {
        FO_NO_STACK_TRACE_ENTRY();

        ActiveBackend = backend;
    }

    ActiveBackendScope(const ActiveBackendScope&) = delete;
    ActiveBackendScope(ActiveBackendScope&&) noexcept = delete;
    auto operator=(const ActiveBackendScope&) = delete;
    auto operator=(ActiveBackendScope&&) noexcept = delete;

    ~ActiveBackendScope()
    {
        FO_NO_STACK_TRACE_ENTRY();

        ActiveBackend = _previous;
    }

private:
    nptr<ManagedScriptBackend> _previous {};
};

// Forward declarations of the internal marshaling/bridge helper structs (defined below).
struct ManagedScalarValue;
struct ManagedObjectRoot;
struct ManagedArrayBridgeData;
struct ManagedDictBridgeData;
struct ManagedCallbackBridgeData;
struct ManagedDataAccessor;
struct ManagedNativeValue;
struct ManagedEventSubscription;
struct ManagedAssemblyResource;

// Static free-function forward declarations, ordered high-level -> low-level.

// Backend registry and active-backend lifecycle
static auto GetActiveBackendOrThrow() -> ptr<ManagedScriptBackend>;
static auto GetActiveEntityManagerOrThrow() -> EntityManagerApi*;
static auto GetTargetName(EngineSideKind side) -> string_view;
static auto GetBackendSettings(ptr<ManagedScriptBackend> backend) -> GlobalSettings*;

// Native ABI: logging, hashing, backend and prototype queries
static void NativeLog(MonoString* text);
static auto NativeGetHash(MonoString* text) -> uint64_t;
static auto NativeGetHashStr(uint64_t value) -> MonoString*;
static auto NativeGetBackendAliveFlag() -> MonoArray*;
static auto NativeGetBackend() -> void*;
static auto NativeGetProtoEntity(MonoString* type_name, uint64_t proto_id_hash) -> void*;
static auto NativeCheckProtoEntity(MonoString* type_name, uint64_t proto_id_hash) -> mono_bool;
static auto NativeGetProtoEntityCount(MonoString* type_name) -> int32_t;
static auto NativeGetProtoEntityAt(MonoString* type_name, int32_t index) -> void*;

// Native ABI: entity lifetime and property access
static void NativeAddRefEntity(void* entity_ptr);
static void NativeReleaseEntity(void* entity_ptr);
static auto NativeIsEntityDestroyed(void* entity_ptr) -> mono_bool;
static auto NativeIsEntityDestroying(void* entity_ptr) -> mono_bool;
static auto NativeGetEntityName(void* entity_ptr) -> MonoString*;
static auto NativeGetEntityId(void* entity_ptr) -> int64_t;
static auto NativeGetEntityProtoId(void* entity_ptr) -> uint64_t;
static auto NativeGetEntityValueAsInt(void* entity_ptr, int32_t prop_index) -> int32_t;
static void NativeSetEntityValueAsInt(void* entity_ptr, int32_t prop_index, int32_t value);
static auto NativeGetEntityValueAsAny(void* entity_ptr, int32_t prop_index) -> MonoString*;
static void NativeSetEntityValueAsAny(void* entity_ptr, int32_t prop_index, MonoString* value);

// Native ABI: hex direction helpers
static auto NativeHdirToMdir(int8_t dir) -> int16_t;
static auto NativeMdirHex(int16_t angle) -> int8_t;
static auto NativeMdirRotateHex(int16_t angle, int32_t steps) -> int16_t;
static auto NativeMdirReverse(int16_t angle) -> int16_t;

// Native ABI: inner entities
static auto NativeCreateInnerEntity(void* holder_ptr, MonoString* entry_name, uint64_t proto_id_hash) -> void*;
static auto NativeHasInnerEntities(void* holder_ptr, MonoString* entry_name) -> mono_bool;
static auto NativeGetInnerEntity(void* holder_ptr, MonoString* entry_name, int64_t id) -> void*;
static auto NativeGetInnerEntityCount(void* holder_ptr, MonoString* entry_name) -> int32_t;
static auto NativeGetInnerEntityAt(void* holder_ptr, MonoString* entry_name, int32_t index) -> void*;

// Native ABI: settings
static auto NativeGetSettingBoolRaw(MonoString* name) -> int32_t;
static void NativeSetSettingBoolRaw(MonoString* name, int32_t value);
static auto NativeGetSettingInt(MonoString* name) -> int32_t;
static void NativeSetSettingInt(MonoString* name, int32_t value);
static auto NativeGetSettingUInt(MonoString* name) -> uint32_t;
static void NativeSetSettingUInt(MonoString* name, uint32_t value);
static auto NativeGetSettingLong(MonoString* name) -> int64_t;
static void NativeSetSettingLong(MonoString* name, int64_t value);
static auto NativeGetSettingULong(MonoString* name) -> uint64_t;
static void NativeSetSettingULong(MonoString* name, uint64_t value);
static auto NativeGetSettingFloat(MonoString* name) -> float32_t;
static void NativeSetSettingFloat(MonoString* name, float32_t value);
static auto NativeGetSettingDouble(MonoString* name) -> float64_t;
static void NativeSetSettingDouble(MonoString* name, float64_t value);
static auto NativeGetSettingString(MonoString* name) -> MonoString*;
static void NativeSetSettingString(MonoString* name, MonoString* value);

// Native ABI: events, properties, methods and remote calls
static auto NativeSubscribeEvent(MonoString* owner_type, MonoString* event_name, void* entity_ptr, MonoObject* handler, mono_bool has_explicit_result, int32_t priority) -> void*;
static void NativeUnsubscribeEvent(MonoString* event_name, void* entity_ptr, void* subscription);
static auto NativeFireEvent(MonoString* owner_type, MonoString* event_name, void* entity_ptr, MonoArray* args) -> int32_t;
static auto NativeGetProperty(MonoString* owner_type, MonoString* property_name, void* entity_ptr) -> MonoObject*;
static void NativeSetProperty(MonoString* owner_type, MonoString* property_name, void* entity_ptr, MonoObject* value);
static void NativeSetPropertyGetter(MonoString* owner_type, MonoString* property_name, MonoObject* getter);
static void NativeAddPropertySetter(MonoString* owner_type, MonoString* property_name, MonoObject* setter);
static void NativeAddPropertySetterWithProperty(MonoString* owner_type, MonoString* property_name, MonoObject* setter);
static void NativeAddPropertyDeferredSetter(MonoString* owner_type, MonoString* property_name, MonoObject* setter);
static auto NativeCallMethod(MonoString* owner_type, MonoString* method_name, int32_t method_index, void* entity_ptr, MonoArray* args) -> MonoObject*;
static auto NativeInvokeScriptFunc(MonoString* func_name, MonoArray* args) -> mono_bool;
static void NativeRegisterGlobalScriptFunc(MonoString* full_name, MonoString* attr_name, MonoArray* param_type_names, MonoString* ret_type_name, MonoObject* handler, mono_bool skip_existing_script_func);
static void NativeRegisterRemoteCallHandler(MonoString* name_str, int32_t param_count, MonoObject* handler);
static void NativeSendRemoteCall(MonoObject* caller, MonoString* name_str, MonoArray* args_array);
static void NativeLoopbackRemoteCall(MonoObject* caller, MonoString* name_str, MonoArray* args_array);

// Internal-call registration
static void RegisterInternalCalls();

// Settings access helpers
static auto GetSettingValueAsString(GlobalSettings* settings, const string& setting_name) -> string;
static auto GetSettingValueAsString(MonoString* name) -> string;
static void SetSettingValueFromString(GlobalSettings* settings, string_view setting_name, string value);
static void SetSettingValueFromString(MonoString* name, string value);

// Property getter/setter callback bridge
static auto InvokeManagedCallbackHandler(ptr<ManagedScriptBackend> backend, MonoObject* handler, MonoArray* args_array) -> MonoObject*;
static auto ResolveVirtualPropertyForCallback(ptr<ManagedScriptBackend> backend, MonoString* owner_type, MonoString* property_name, bool require_virtual, bool require_marshalable_value) -> ptr<const Property>;
static void DispatchManagedCallback(ptr<ManagedScriptBackend> backend, uint32_t handler_handle, const ComplexTypeDesc& ret, const vector<ComplexTypeDesc>& args, FuncCallData& call);
static void CopyManagedCallbackReturnValue(ptr<ManagedScriptBackend> backend, const ComplexTypeDesc& type, MonoObject* value, FuncCallData& call);
static auto CreateManagedCallbackDesc(const ManagedCallbackBridgeData* callback) -> unique_del_nptr<ScriptFuncDesc>;
static auto BoxNativeCallValue(ptr<const ManagedScriptBackend> backend, const ComplexTypeDesc& type, void* data, const DataAccessor* accessor) -> MonoObject*;

// Event dispatch bridge
static void WriteBackManagedEventArg(ptr<ManagedScriptBackend> backend, const ComplexTypeDesc& type, MonoObject* value, void* dst);
static auto DispatchManagedEvent(const std::shared_ptr<ManagedEventSubscription>& subscription, FuncCallData& call) -> Entity::EventResult;
static auto DispatchManagedEventInContext(const std::shared_ptr<ManagedEventSubscription>& subscription, FuncCallData& call) -> Entity::EventResult;

// Remote-call argument marshaling
static auto SerializeManagedRemoteCallArgs(ptr<ManagedScriptBackend> backend, const vector<ArgDesc>& call_args, MonoArray* args_array, string_view name) -> vector<uint8_t>;
static void AppendRawBytes(vector<uint8_t>& data, const void* value, size_t size);
static void AppendAlignedRawBytes(vector<uint8_t>& data, const void* value, size_t size, size_t alignment);

// Managed object creation and native<->managed values
static auto CreateHashObject(ptr<const ManagedScriptBackend> backend, hstring::hash_t value) -> MonoObject*;
static auto CreateEntityObject(ptr<const ManagedScriptBackend> backend, string_view type_name, nptr<Entity> entity) -> MonoObject*;
static auto CreatePropertyEnumObject(ptr<const ManagedScriptBackend> backend, string_view owner_type_name, ptr<const Property> prop) -> MonoObject*;
static void InvokeManagedConstructor(MonoClass* klass, MonoObject* obj, int32_t args_count, void** args, string_view context);
static auto CreateNativeRefTypeObject(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, void* ref_ptr) -> MonoObject*;
static auto CreateDynamicRefTypeObject(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, span<const uint8_t> raw_data) -> MonoObject*;
static auto CreateRefTypeObject(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, void* ref_ptr) -> MonoObject*;
static auto CreateDynamicRefTypeFromManaged(ptr<ManagedScriptBackend> backend, const BaseTypeDesc& base_type, MonoObject* value) -> refcount_nptr<DynamicRefTypeInstance>;
static void CopyManagedStructToNative(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, MonoObject* value, void* data);
static void CopyManagedStructToPropertyData(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, MonoObject* value, void* data);
static auto CreateStructObject(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, void* data) -> MonoObject*;
static auto CreatePropertyStructObject(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, span<const uint8_t> raw_data) -> MonoObject*;
static auto GetManagedPropertyValue(ptr<const ManagedScriptBackend> backend, MonoObject* obj, string_view property_name) -> MonoObject*;
static void SetManagedPropertyValue(ptr<const ManagedScriptBackend> backend, MonoObject* obj, string_view property_name, MonoObject* value);

// Managed collections (list/dictionary/delegate)
static auto CreateManagedList(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& element_type) -> MonoObject*;
static auto GetManagedListCount(ptr<const ManagedScriptBackend> backend, MonoObject* list) -> size_t;
static auto GetManagedListItem(ptr<const ManagedScriptBackend> backend, MonoObject* list, size_t index) -> MonoObject*;
static void AddManagedListItem(ptr<const ManagedScriptBackend> backend, MonoObject* list, MonoObject* item);
static auto CreateManagedDictionary(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& key_type, const BaseTypeDesc& value_type) -> MonoObject*;
static auto GetManagedDictionaryCount(ptr<const ManagedScriptBackend> backend, MonoObject* dictionary) -> size_t;
static auto GetManagedDictionaryKey(ptr<const ManagedScriptBackend> backend, MonoObject* dictionary, size_t index) -> MonoObject*;
static auto GetManagedDictionaryValue(ptr<const ManagedScriptBackend> backend, MonoObject* dictionary, size_t index) -> MonoObject*;
static void AddManagedDictionaryItem(ptr<const ManagedScriptBackend> backend, MonoObject* dictionary, MonoObject* key, MonoObject* value);
static auto GetManagedDelegateKey(ptr<const ManagedScriptBackend> backend, MonoObject* handler) -> string;

// Managed<->native conversion and boxing
static auto GetManagedClass(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& type) -> MonoClass*;
static auto InvokeNativeHelper(ptr<const ManagedScriptBackend> backend, const char* method_name, uint32_t args_count, void** args) -> MonoObject*;
static auto InvokeNativeBoolHelper(ptr<const ManagedScriptBackend> backend, const char* method_name, MonoObject* value) -> bool;
static auto IsManagedByteArray(MonoObject* value) -> bool;
static auto GetManagedByteArrayItem(ptr<const ManagedScriptBackend> backend, MonoObject* value, size_t index) -> uint8_t;
static void CopyManagedByteArrayToNative(ptr<const ManagedScriptBackend> backend, MonoObject* value, void* data, size_t size);
static auto ConvertManagedSimpleObjectToNative(ptr<ManagedScriptBackend> backend, const BaseTypeDesc& base_type, MonoObject* value, ManagedScalarValue& storage) -> void*;
static auto ConvertManagedObjectToNative(ptr<ManagedScriptBackend> backend, const ComplexTypeDesc& type, MonoObject* value, ManagedNativeValue& storage) -> void*;
static auto ManagedObjectClassMatches(MonoObject* value, MonoClass* expected_class) -> bool;
static auto ManagedObjectClassMatchesOrDerives(MonoObject* value, MonoClass* expected_class) -> bool;
static auto CanConvertManagedSimpleObjectToNative(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, MonoObject* value) -> bool;
static auto CanConvertManagedObjectToNative(ptr<const ManagedScriptBackend> backend, const ComplexTypeDesc& type, MonoObject* value) -> bool;
static auto BoxNativeSimpleValue(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, void* data) -> MonoObject*;

// Property value marshaling
static auto BoxSimplePropertyValue(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, span<const uint8_t> raw_data) -> MonoObject*;
static auto BoxPropertyValue(ptr<const ManagedScriptBackend> backend, ptr<const Property> prop, span<const uint8_t> raw_data) -> MonoObject*;
static auto ConvertManagedSimpleObjectToPropertyData(ptr<ManagedScriptBackend> backend, const BaseTypeDesc& base_type, MonoObject* value) -> PropertyRawData;
static auto ConvertManagedObjectToPropertyData(ptr<ManagedScriptBackend> backend, ptr<const Property> prop, MonoObject* value) -> PropertyRawData;
static auto GetPropertyRawData(ptr<Entity> entity, ptr<const Property> prop) -> PropertyRawData;

// Managed bridge type predicates
static auto IsManagedBridgeSimpleType(const BaseTypeDesc& type) -> bool;
static auto IsManagedBridgeType(const ComplexTypeDesc& type) -> bool;
static auto IsManagedBridgeFixedDictionaryValueType(const BaseTypeDesc& type) -> bool;
static auto IsManagedBridgeFixedDictionaryProperty(ptr<const Property> prop) -> bool;
static auto IsDynamicManagedRefType(const BaseTypeDesc& base_type) -> bool;

// Type/class and metadata resolution
static auto FindFOnlineClass(ptr<const ManagedScriptBackend> backend, string_view class_name) -> MonoClass*;
static auto GetPrimitiveClass(const BaseTypeDesc& type) -> MonoClass*;
static auto GetValueClass(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& type) -> MonoClass*;
static auto FindFieldInHierarchy(MonoClass* klass, const char* field_name) -> MonoClassField*;
static auto FindEntityTypeDesc(ptr<EngineMetadata> meta, string_view owner_type_name) -> nptr<const EntityTypeDesc>;
static auto FindRefTypeDesc(ptr<EngineMetadata> meta, string_view owner_type_name) -> nptr<const RefTypeDesc>;
static auto FindMethod(ptr<EngineMetadata> meta, string_view owner_type_name, string_view method_name, int32_t method_index, size_t args_count) -> nptr<const MethodDesc>;
static auto MakeManagedGlobalSimpleType(ptr<EngineMetadata> meta, string_view type_name) -> ComplexTypeDesc;

// Entity resolution and inner-entry helpers
static auto ResolveEntity(ptr<ManagedScriptBackend> backend, void* entity_ptr) -> ptr<Entity>;
static auto ResolveProtoEntityFromRawData(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, span<const uint8_t> raw_data) -> nptr<Entity>;
static auto ExtractProtoHashFromManagedEntity(MonoObject* value) -> hstring::hash_t;
static auto ResolveInnerEntry(ptr<ManagedScriptBackend> backend, MonoString* entry_name) -> hstring;
static void ValidateManagedInnerEntity(ptr<const Entity> entity);
static auto CollectManagedInnerEntities(ptr<Entity> holder, hstring entry) -> vector<ptr<Entity>>;

// Extraction and hash primitives
static auto ExtractEntityPtr(MonoObject* obj) -> Entity*;
static auto ExtractRefPtr(MonoObject* obj) -> void*;
static auto ExtractHashValue(MonoObject* obj) -> hstring::hash_t;
static auto MakeManagedHashValue(ptr<const ManagedScriptBackend> backend, const hstring& value) -> hstring::hash_t;
static auto ResolveManagedHashValue(ptr<const ManagedScriptBackend> backend, hstring::hash_t value) -> hstring;

// Assembly loading, runtime configuration and resource cache
static auto IsManagedEntryAssemblyFileName(string_view file_name, string_view target_name) -> bool;
static auto IsManagedHostAssemblyFileName(string_view file_name) noexcept -> bool;
static auto CollectAssemblyResources(const FileSystem& resources, string_view target_name) -> vector<ManagedAssemblyResource>;
static auto IsRuntimeLayoutPath(const std::filesystem::path& dir) -> bool;
static auto FindManagedRuntimeDir() -> optional<std::filesystem::path>;
static void AppendExistingAssemblyPath(vector<string>& paths, const std::filesystem::path& dir);
static auto BuildAssemblySearchPath(const std::filesystem::path& lib_dir) -> string;
static void SetEnvironmentVariableDefault(const char* name, const char* value);
static void ConfigureManagedRuntime();
static void AddManagedAssemblyCacheByte(uint64_t& hash, uint8_t byte) noexcept;
static auto MakeManagedAssemblyCacheKey(const vector<ManagedAssemblyResource>& assembly_resources) noexcept -> string;
static auto IsSameManagedAssemblyCacheFile(const std::filesystem::path& disk_path, const_span<uint8_t> assembly_data) -> bool;
static auto RestoreAssemblyResources(const vector<ManagedAssemblyResource>& assembly_resources) -> unordered_map<string, std::filesystem::path>;
static auto CollectBakeOutputAssemblyPaths(string_view bake_output_dir, string_view target_name) -> vector<std::filesystem::path>;

// Low-level Mono/string primitives
static auto GetDomainOrThrow(void* domain) -> MonoDomain*;
static auto MakeManagedPathArray(MonoDomain* domain, const vector<std::filesystem::path>& paths) -> MonoArray*;
static auto ToStringAndFree(MonoString* text) -> string;
static auto ManagedObjectToString(MonoObject* obj) -> string;
static void ThrowIfManagedException(MonoObject* exception, string_view context);

// ---------------------------------------------------------------------------------------------------
// Helper struct definitions
// ---------------------------------------------------------------------------------------------------

struct ManagedScalarValue
{
    alignas(std::max_align_t) std::array<uint8_t, PropertyRawData::LOCAL_BUF_SIZE> Local {};
    vector<uint8_t> Dynamic {};
    string Text {};
    any_t Any {};
    hstring Hash {};
    nptr<Entity> EntityPtr {};
    refcount_nptr<DynamicRefTypeInstance> DynamicRefType {};
    void* RefTypePtr {};

    [[nodiscard]] auto Alloc(size_t size) -> void*
    {
        FO_NO_STACK_TRACE_ENTRY();

        if (size <= Local.size()) {
            return Local.data();
        }

        Dynamic.resize(size);
        return Dynamic.data();
    }
};

// Roots the bridged managed List/Dictionary object with a GC handle for the lifetime of the bridge.
// The accessors below re-invoke managed helpers (which can trigger an SGen collection that moves or
// collects the object), so a raw MonoObject* would dangle between calls; SetObject/GetObject keep a
// live handle and always hand back the current address.
struct ManagedObjectRoot
{
    ManagedObjectRoot() = default;
    ManagedObjectRoot(const ManagedObjectRoot&) = delete;
    ManagedObjectRoot(ManagedObjectRoot&&) = delete;
    auto operator=(const ManagedObjectRoot&) -> ManagedObjectRoot& = delete;
    auto operator=(ManagedObjectRoot&&) -> ManagedObjectRoot& = delete;

    ~ManagedObjectRoot()
    {
        FO_STACK_TRACE_ENTRY();

        if (_objectHandle != 0) {
            mono_gchandle_free(_objectHandle);
        }
    }

    void SetObject(MonoObject* object)
    {
        FO_STACK_TRACE_ENTRY();

        if (_objectHandle != 0) {
            mono_gchandle_free(_objectHandle);
            _objectHandle = 0;
        }
        if (object != nullptr) {
            _objectHandle = mono_gchandle_new(object, 0);
        }
    }

    [[nodiscard]] auto GetObject() const -> MonoObject*
    {
        FO_STACK_TRACE_ENTRY();

        return _objectHandle != 0 ? mono_gchandle_get_target(_objectHandle) : nullptr;
    }

private:
    uint32_t _objectHandle {};
};

struct ManagedArrayBridgeData : ManagedObjectRoot
{
    nptr<ManagedScriptBackend> Backend {};
    ComplexTypeDesc Type {};
    vector<ManagedScalarValue> Elements {};
};

struct ManagedDictBridgeData : ManagedObjectRoot
{
    nptr<ManagedScriptBackend> Backend {};
    ComplexTypeDesc Type {};
    vector<ManagedScalarValue> Keys {};
    vector<ManagedScalarValue> Values {};
};

struct ManagedCallbackBridgeData
{
    nptr<ManagedScriptBackend> Backend {};
    ComplexTypeDesc Type {};
    uint32_t Handler {};
    hstring Name {};

    ~ManagedCallbackBridgeData()
    {
        FO_STACK_TRACE_ENTRY();

        if (Handler != 0) {
            mono_gchandle_free(Handler);
        }
    }
};

struct ManagedDataAccessor final : DataAccessor
{
    [[nodiscard]] auto GetBackendIndex() const noexcept -> int32_t override { return ScriptSystemBackend::MANAGED_BACKEND_INDEX; }

    [[nodiscard]] auto GetArraySize(ptr<void> data) const -> size_t override
    {
        FO_STACK_TRACE_ENTRY();

        auto* array = static_cast<ManagedArrayBridgeData*>(data.get());
        return GetManagedListCount(array->Backend.as_ptr(), array->GetObject());
    }

    [[nodiscard]] auto GetArrayElement(ptr<void> data, size_t index) const -> ptr<void> override
    {
        FO_STACK_TRACE_ENTRY();

        auto* array = static_cast<ManagedArrayBridgeData*>(data.get());

        if (array->Elements.size() <= index) {
            array->Elements.resize(index + 1);
        }

        auto* item = GetManagedListItem(array->Backend.as_ptr(), array->GetObject(), index);
        return ConvertManagedSimpleObjectToNative(array->Backend.as_ptr(), array->Type.BaseType, item, array->Elements[index]);
    }

    void ClearArray(ptr<void> data) const override
    {
        FO_STACK_TRACE_ENTRY();

        auto* array = static_cast<ManagedArrayBridgeData*>(data.get());
        array->SetObject(CreateManagedList(array->Backend.as_ptr(), array->Type.BaseType));
        array->Elements.clear();
    }

    void AddArrayElement(ptr<void> data, ptr<void> value) const override
    {
        FO_STACK_TRACE_ENTRY();

        auto* array = static_cast<ManagedArrayBridgeData*>(data.get());
        auto* item = BoxNativeSimpleValue(array->Backend.as_ptr(), array->Type.BaseType, value.get());
        AddManagedListItem(array->Backend.as_ptr(), array->GetObject(), item);
    }

    [[nodiscard]] auto GetDictSize(ptr<void> data) const -> size_t override
    {
        FO_STACK_TRACE_ENTRY();

        auto* dict = static_cast<ManagedDictBridgeData*>(data.get());
        return GetManagedDictionaryCount(dict->Backend.as_ptr(), dict->GetObject());
    }

    [[nodiscard]] auto GetDictElement(ptr<void> data, size_t index) const -> pair<ptr<void>, ptr<void>> override
    {
        FO_STACK_TRACE_ENTRY();

        auto* dict = static_cast<ManagedDictBridgeData*>(data.get());
        FO_VERIFY_AND_THROW(dict->Type.KeyType, "Dictionary bridge has no key type");

        if (dict->Keys.size() <= index) {
            dict->Keys.resize(index + 1);
        }
        if (dict->Values.size() <= index) {
            dict->Values.resize(index + 1);
        }

        auto* key = GetManagedDictionaryKey(dict->Backend.as_ptr(), dict->GetObject(), index);
        auto* value = GetManagedDictionaryValue(dict->Backend.as_ptr(), dict->GetObject(), index);
        return pair<ptr<void>, ptr<void>>(ConvertManagedSimpleObjectToNative(dict->Backend.as_ptr(), *dict->Type.KeyType, key, dict->Keys[index]), ConvertManagedSimpleObjectToNative(dict->Backend.as_ptr(), dict->Type.BaseType, value, dict->Values[index]));
    }

    [[nodiscard]] auto GetCallback(ptr<void> data) const -> unique_del_nptr<ScriptFuncDesc> override
    {
        FO_STACK_TRACE_ENTRY();

        return CreateManagedCallbackDesc(static_cast<ManagedCallbackBridgeData*>(data.get()));
    }

    void ClearDict(ptr<void> data) const override
    {
        FO_STACK_TRACE_ENTRY();

        auto* dict = static_cast<ManagedDictBridgeData*>(data.get());
        FO_VERIFY_AND_THROW(dict->Type.KeyType, "Dictionary bridge has no key type");
        dict->SetObject(CreateManagedDictionary(dict->Backend.as_ptr(), *dict->Type.KeyType, dict->Type.BaseType));
        dict->Keys.clear();
        dict->Values.clear();
    }

    void AddDictElement(ptr<void> data, ptr<void> key, ptr<void> value) const override
    {
        FO_STACK_TRACE_ENTRY();

        auto* dict = static_cast<ManagedDictBridgeData*>(data.get());
        FO_VERIFY_AND_THROW(dict->Type.KeyType, "Dictionary bridge has no key type");
        auto* managed_key = BoxNativeSimpleValue(dict->Backend.as_ptr(), *dict->Type.KeyType, key.get());
        auto* managed_value = BoxNativeSimpleValue(dict->Backend.as_ptr(), dict->Type.BaseType, value.get());
        AddManagedDictionaryItem(dict->Backend.as_ptr(), dict->GetObject(), managed_key, managed_value);
    }
};

static const ManagedDataAccessor MANAGED_DATA_ACCESSOR {};

struct ManagedNativeValue : ManagedScalarValue
{
    unique_nptr<ManagedArrayBridgeData> Array {};
    unique_nptr<ManagedDictBridgeData> Dict {};
    unique_nptr<ManagedCallbackBridgeData> Callback {};
};

struct ManagedEventSubscription
{
    nptr<ManagedScriptBackend> Backend {};
    MonoImage* Image {};
    vector<ComplexTypeDesc> Args {};
    uint32_t Handler {};
    bool HasExplicitResult {};

    ~ManagedEventSubscription()
    {
        FO_STACK_TRACE_ENTRY();

        if (Handler != 0) {
            mono_gchandle_free(Handler);
        }
    }
};

struct ManagedAssemblyResource
{
    string ResourcePath {};
    string FileName {};
    vector<uint8_t> Data {};
};

// ---------------------------------------------------------------------------------------------------
// Static free-function definitions (same order as the declarations above)
// ---------------------------------------------------------------------------------------------------

// === Backend registry and active-backend lifecycle ===

auto ManagedScriptBackend::GetAliveFlagObject() const -> void*
{
    FO_STACK_TRACE_ENTRY();

    return _aliveFlagGcHandle != 0 ? mono_gchandle_get_target(_aliveFlagGcHandle) : nullptr;
}

// The alive flag is a pinned one-element managed bool array. Wrappers that outlive deterministic disposal
// (e.g. Sprite) capture it at construction, so their GC finalizers can ask "is MY engine still alive"
// without any process-global side registry — correct with any number of same-side engines in the process.
void ManagedScriptBackend::CreateAliveFlag()
{
    FO_STACK_TRACE_ENTRY();

    if (_aliveFlagGcHandle != 0) {
        return;
    }

    auto* domain = GetDomainOrThrow(_domain.get());
    auto* flag_array = mono_array_new(domain, mono_get_boolean_class(), 1);

    if (flag_array == nullptr) {
        throw ScriptSystemException("Can't create Managed backend alive flag");
    }

    mono_array_set(flag_array, uint8_t, 0, 1);
    _aliveFlagGcHandle = mono_gchandle_new(reinterpret_cast<MonoObject*>(flag_array), 0);
}

void ManagedScriptBackend::ReleaseAliveFlag()
{
    FO_STACK_TRACE_ENTRY();

    if (_aliveFlagGcHandle != 0) {
        auto* flag_obj = mono_gchandle_get_target(_aliveFlagGcHandle);

        if (flag_obj != nullptr) {
            mono_array_set(reinterpret_cast<MonoArray*>(flag_obj), uint8_t, 0, 0);
        }

        mono_gchandle_free(_aliveFlagGcHandle);
        _aliveFlagGcHandle = 0;
    }
}

static auto GetActiveBackendOrThrow() -> ptr<ManagedScriptBackend>
{
    FO_STACK_TRACE_ENTRY();

    if (ActiveBackend == nullptr || ActiveBackend->GetMetadata() == nullptr) {
        throw ScriptSystemException("Managed backend is not active");
    }

    return ActiveBackend.as_ptr();
}

static auto GetActiveEntityManagerOrThrow() -> EntityManagerApi*
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    auto* entity_mngr = dynamic_cast<EntityManagerApi*>(backend->GetMetadata());

    if (entity_mngr == nullptr) {
        throw ScriptSystemException("Managed entity manager is not available");
    }

    return entity_mngr;
}

static auto GetTargetName(EngineSideKind side) -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    switch (side) {
    case EngineSideKind::ServerSide:
        return "Server";
    case EngineSideKind::ClientSide:
        return "Client";
    case EngineSideKind::MapperSide:
        return "Mapper";
    default:
        return "Server";
    }
}

static auto GetBackendSettings(ptr<ManagedScriptBackend> backend) -> GlobalSettings*
{
    FO_STACK_TRACE_ENTRY();

    auto* engine = dynamic_cast<BaseEngine*>(backend->GetMetadata());

    if (engine != nullptr) {
        return engine->Settings.get();
    }

    return nullptr;
}

// === Native ABI: logging, hashing, backend and prototype queries ===

static void NativeLog(MonoString* text)
{
    FO_STACK_TRACE_ENTRY();

    if (text == nullptr) {
        WriteLog("{}", string_view {});
        return;
    }

    char* text_utf8 = mono_string_to_utf8(text);

    if (text_utf8 == nullptr) {
        WriteLog("{}", string_view {});
        return;
    }

    const string log_text = text_utf8;
    mono_free(text_utf8);
    WriteLog("{}", log_text);
}

static auto NativeGetHash(MonoString* text) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    const string value = ToStringAndFree(text);

    if (value.empty()) {
        return 0;
    }

    auto backend = GetActiveBackendOrThrow();
    return backend->GetMetadata()->Hashes.ToHashedString(value).as_hash();
}

static auto NativeGetHashStr(uint64_t value) -> MonoString*
{
    FO_STACK_TRACE_ENTRY();

    string text = strex("{}", value).str();

    if (ActiveBackend != nullptr && ActiveBackend->GetMetadata() != nullptr) {
        bool failed = false;
        const hstring resolved = ActiveBackend->GetMetadata()->Hashes.ResolveHash(value, &failed);

        if (!failed) {
            text = resolved.as_str();
        }
    }

    return mono_string_new(GetDomainOrThrow(mono_domain_get()), text.c_str());
}

// Returns the calling engine's alive flag (a one-element managed bool array). Managed wrappers capture it
// at construction time (under the active backend scope) and read it from GC finalizers, where no backend is
// active; the flag object stays valid as long as the wrapper references it, even after backend teardown.
static auto NativeGetBackendAliveFlag() -> MonoArray*
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    auto* flag = backend->GetAliveFlagObject();

    if (flag == nullptr) {
        throw ScriptSystemException("Managed backend alive flag is not created");
    }

    return static_cast<MonoArray*>(flag);
}

static auto NativeGetBackend() -> void*
{
    FO_STACK_TRACE_ENTRY();

    return GetActiveBackendOrThrow().get();
}

// Custom-entity proto getters (managed equivalent of AngelScript register_entity_protos / Game_GetProtoCustomEntity
// in AngelScriptEntity.cpp): the built-in entities (Item/Critter/Location/Map) have metadata-exported
// `Game.GetProto*` accessors, but custom HasProtos entities (Modifier/Faction/ItemBag/...) register theirs only at
// AngelScript runtime, so the managed baker emits `Game.GetProto<X>`/`CheckProto<X>` wrappers over these bridges.
// Returns the proto entity pointer (nullptr when the pid is unknown); the generated wrapper throws on null.
static auto NativeGetProtoEntity(MonoString* type_name, uint64_t proto_id_hash) -> void*
{
    FO_STACK_TRACE_ENTRY();

    if (ActiveBackend == nullptr || ActiveBackend->GetMetadata() == nullptr) {
        return nullptr;
    }

    const string type_name_str = ToStringAndFree(type_name);
    const auto* meta = ActiveBackend->GetMetadata();

    bool failed = false;
    const hstring proto_id = meta->Hashes.ResolveHash(static_cast<hstring::hash_t>(proto_id_hash), &failed);

    if (failed) {
        return nullptr;
    }

    const hstring type_hname = meta->Hashes.ToHashedString(type_name_str);
    auto proto = meta->GetProtoEntity(type_hname, proto_id);
    return static_cast<Entity*>(const_cast<ProtoEntity*>(proto.get()));
}

static auto NativeCheckProtoEntity(MonoString* type_name, uint64_t proto_id_hash) -> mono_bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (ActiveBackend == nullptr || ActiveBackend->GetMetadata() == nullptr) {
        return static_cast<mono_bool>(0);
    }

    const string type_name_str = ToStringAndFree(type_name);
    const auto* meta = ActiveBackend->GetMetadata();

    bool failed = false;
    const hstring proto_id = meta->Hashes.ResolveHash(static_cast<hstring::hash_t>(proto_id_hash), &failed);

    if (failed) {
        return static_cast<mono_bool>(0);
    }

    const hstring type_hname = meta->Hashes.ToHashedString(type_name_str);
    return static_cast<mono_bool>(meta->GetProtoEntity(type_hname, proto_id) != nullptr ? 1 : 0);
}

// Plural proto enumeration (managed equivalent of AngelScript Game_GetProtoCustomEntities): count + by-index,
// backing the generated Game.GetProto<X>s()/Get<X>s() loops. The proto map is built once and not mutated during
// gameplay, so its iteration order is stable across the count + per-index calls of a single enumeration.
static auto NativeGetProtoEntityCount(MonoString* type_name) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    if (ActiveBackend == nullptr || ActiveBackend->GetMetadata() == nullptr) {
        return 0;
    }

    const string type_name_str = ToStringAndFree(type_name);
    const auto* meta = ActiveBackend->GetMetadata();
    return numeric_cast<int32_t>(meta->GetProtoEntities(meta->Hashes.ToHashedString(type_name_str)).size());
}

static auto NativeGetProtoEntityAt(MonoString* type_name, int32_t index) -> void*
{
    FO_STACK_TRACE_ENTRY();

    if (ActiveBackend == nullptr || ActiveBackend->GetMetadata() == nullptr) {
        return nullptr;
    }

    const string type_name_str = ToStringAndFree(type_name);
    const auto* meta = ActiveBackend->GetMetadata();
    const auto& protos = meta->GetProtoEntities(meta->Hashes.ToHashedString(type_name_str));

    int32_t i = 0;

    for (const auto& [proto_id, proto] : protos) {
        if (i == index) {
            return static_cast<Entity*>(proto.get_no_const());
        }

        i++;
    }

    return nullptr;
}

// === Native ABI: entity lifetime and property access ===

// Managed entity wrappers hold a strong reference to the native entity (AddRef in the C# entity ctor, Release in
// the C# finalizer) so a wrapper retained past the entity's destroy keeps it alive-but-destroyed instead of
// dangling — mirroring AngelScript's refcounted handles, so the destroyed-entity guard in ResolveEntity stays
// safe. The refcount is atomic, so AddRef/Release are safe from the Mono finalizer thread; the last Release frees
// an already-destroyed (map-detached, unregistered) entity, whose destructor is memory-only.
static void NativeAddRefEntity(void* entity_ptr)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (entity_ptr != nullptr) {
        static_cast<const Entity*>(entity_ptr)->AddRef();
    }
}

static void NativeReleaseEntity(void* entity_ptr)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (entity_ptr != nullptr) {
        static_cast<const Entity*>(entity_ptr)->Release();
    }
}

// Backs the managed `Entity.IsDestroyed` property (parity with AngelScript). A null wrapper pointer counts as
// destroyed. The wrapper holds a strong ref (AddRef/Release), so reading a destroyed-but-alive entity is safe.
static auto NativeIsEntityDestroyed(void* entity_ptr) -> mono_bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const bool destroyed = entity_ptr == nullptr || static_cast<const Entity*>(entity_ptr)->IsDestroyed();
    return static_cast<mono_bool>(destroyed ? 1 : 0);
}

// Backs the managed `Entity.IsDestroying` property (parity with AngelScript get_IsDestroying). A null wrapper
// pointer is already gone rather than mid-destruction, so it reports false.
static auto NativeIsEntityDestroying(void* entity_ptr) -> mono_bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const bool destroying = entity_ptr != nullptr && static_cast<const Entity*>(entity_ptr)->IsDestroying();
    return static_cast<mono_bool>(destroying ? 1 : 0);
}

// Managed Entity.Name -> AngelScript base-entity `get_Name()` (Entity::GetName); a manual base binding.
static auto NativeGetEntityName(void* entity_ptr) -> MonoString*
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* domain = GetDomainOrThrow(mono_domain_get());

    if (entity_ptr == nullptr) {
        return mono_string_new(domain, "");
    }

    const string name = string(static_cast<const Entity*>(entity_ptr)->GetName());
    return mono_string_new(domain, name.c_str());
}

static auto NativeGetEntityId(void* entity_ptr) -> int64_t
{
    FO_STACK_TRACE_ENTRY();

    const auto* entity = static_cast<const Entity*>(entity_ptr);

    if (entity == nullptr) {
        return {};
    }

    return entity->GetId().underlying_value();
}

static auto NativeGetEntityProtoId(void* entity_ptr) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    const auto* entity = static_cast<const Entity*>(entity_ptr);

    if (entity == nullptr) {
        return {};
    }

    if (const auto* self_proto = dynamic_cast<const ProtoEntity*>(entity)) {
        return self_proto->GetProtoId().as_hash();
    }
    if (const auto* self_with_proto = dynamic_cast<const EntityWithProto*>(entity)) {
        return self_with_proto->GetProtoId().as_hash();
    }

    return {};
}

// Generic property accessors by index (managed equivalent of AngelScript Entity_GetValueAsInt / Entity_SetValueAsInt /
// Entity_GetValueAsAny / Entity_SetValueAsAny in register_entity_getset): the generated Entity.GetAs*/SetAs* wrappers
// pass the property enum value as the property index.
static auto NativeGetEntityValueAsInt(void* entity_ptr, int32_t prop_index) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    if (entity_ptr == nullptr) {
        return 0;
    }

    return static_cast<const Entity*>(entity_ptr)->GetValueAsInt(prop_index);
}

static void NativeSetEntityValueAsInt(void* entity_ptr, int32_t prop_index, int32_t value)
{
    FO_STACK_TRACE_ENTRY();

    if (entity_ptr == nullptr) {
        return;
    }

    static_cast<Entity*>(entity_ptr)->SetValueAsInt(prop_index, value);
}

static auto NativeGetEntityValueAsAny(void* entity_ptr, int32_t prop_index) -> MonoString*
{
    FO_STACK_TRACE_ENTRY();

    auto* domain = GetDomainOrThrow(mono_domain_get());

    if (entity_ptr == nullptr) {
        return mono_string_new(domain, "");
    }

    const any_t value = static_cast<const Entity*>(entity_ptr)->GetValueAsAny(prop_index);
    return mono_string_new_len(domain, value.data(), numeric_cast<uint32_t>(value.size()));
}

static void NativeSetEntityValueAsAny(void* entity_ptr, int32_t prop_index, MonoString* value)
{
    FO_STACK_TRACE_ENTRY();

    if (entity_ptr == nullptr) {
        return;
    }

    static_cast<Entity*>(entity_ptr)->SetValueAsAny(prop_index, any_t {ToStringAndFree(value)});
}

// === Native ABI: hex direction helpers ===

// Backs the managed `mdir.hex` property (parity with AngelScript mdir::get_hex). The conversion is
// geometry-dependent (hex vs square map dirs), so it routes through the engine rather than a managed mirror.
static auto NativeHdirToMdir(int8_t dir) -> int16_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return mdir(hdir(dir)).angle();
}

static auto NativeMdirHex(int16_t angle) -> int8_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return mdir(static_cast<int32_t>(angle)).hex().value();
}

static auto NativeMdirRotateHex(int16_t angle, int32_t steps) -> int16_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return mdir(static_cast<int32_t>(angle)).rotateHex(steps).angle();
}

static auto NativeMdirReverse(int16_t angle) -> int16_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return mdir(static_cast<int32_t>(angle)).reverse().angle();
}

// === Native ABI: inner entities ===

static auto NativeCreateInnerEntity(void* holder_ptr, MonoString* entry_name, uint64_t proto_id_hash) -> void*
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    auto* entity_mngr = GetActiveEntityManagerOrThrow();
    auto holder = ResolveEntity(backend, holder_ptr);
    ValidateEntityAccess(holder);

    const hstring entry = ResolveInnerEntry(backend, entry_name);
    hstring proto_id;

    if (proto_id_hash != 0) {
        bool failed = false;
        proto_id = backend->GetMetadata()->Hashes.ResolveHash(static_cast<hstring::hash_t>(proto_id_hash), &failed);

        if (failed) {
            throw ScriptSystemException("Unknown Managed inner entity proto id hash", proto_id_hash);
        }
    }

    return entity_mngr->CreateCustomInnerEntity(holder, entry, proto_id).get();
}

static auto NativeHasInnerEntities(void* holder_ptr, MonoString* entry_name) -> mono_bool
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    auto holder = ResolveEntity(backend, holder_ptr);
    ValidateEntityAccess(holder);

    const hstring entry = ResolveInnerEntry(backend, entry_name);
    auto entities = holder->GetInnerEntities(entry);
    return static_cast<mono_bool>(entities ? 1 : 0);
}

static auto NativeGetInnerEntity(void* holder_ptr, MonoString* entry_name, int64_t id) -> void*
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    auto holder = ResolveEntity(backend, holder_ptr);
    ValidateEntityAccess(holder);

    const hstring entry = ResolveInnerEntry(backend, entry_name);
    const ident_t entity_id {id};
    auto entities = holder->GetInnerEntities(entry);

    if (!entities || entities->empty()) {
        return nullptr;
    }

    for (auto& entity : *entities) {
        ValidateManagedInnerEntity(entity.get());

        if (entity->GetId() == entity_id) {
            return entity.get();
        }
    }

    return nullptr;
}

static auto NativeGetInnerEntityCount(void* holder_ptr, MonoString* entry_name) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    auto holder = ResolveEntity(backend, holder_ptr);
    ValidateEntityAccess(holder);

    const hstring entry = ResolveInnerEntry(backend, entry_name);
    const auto entities = CollectManagedInnerEntities(holder, entry);
    return numeric_cast<int32_t>(entities.size());
}

static auto NativeGetInnerEntityAt(void* holder_ptr, MonoString* entry_name, int32_t index) -> void*
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    auto holder = ResolveEntity(backend, holder_ptr);
    ValidateEntityAccess(holder);

    const hstring entry = ResolveInnerEntry(backend, entry_name);
    const auto entities = CollectManagedInnerEntities(holder, entry);

    if (index < 0 || index >= numeric_cast<int32_t>(entities.size())) {
        return nullptr;
    }

    return entities[numeric_cast<size_t>(index)].get_no_const();
}

// === Native ABI: settings ===

static auto NativeGetSettingBoolRaw(MonoString* name) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    return strvex(GetSettingValueAsString(name)).to_bool() ? 1 : 0;
}

static void NativeSetSettingBoolRaw(MonoString* name, int32_t value)
{
    FO_STACK_TRACE_ENTRY();

    SetSettingValueFromString(name, value != 0 ? "True" : "False");
}

static auto NativeGetSettingInt(MonoString* name) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    return strex(GetSettingValueAsString(name)).to_int32();
}

static void NativeSetSettingInt(MonoString* name, int32_t value)
{
    FO_STACK_TRACE_ENTRY();

    SetSettingValueFromString(name, strex("{}", value).str());
}

static auto NativeGetSettingUInt(MonoString* name) -> uint32_t
{
    FO_STACK_TRACE_ENTRY();

    return strex(GetSettingValueAsString(name)).to_uint32();
}

static void NativeSetSettingUInt(MonoString* name, uint32_t value)
{
    FO_STACK_TRACE_ENTRY();

    SetSettingValueFromString(name, strex("{}", value).str());
}

static auto NativeGetSettingLong(MonoString* name) -> int64_t
{
    FO_STACK_TRACE_ENTRY();

    return strex(GetSettingValueAsString(name)).to_int64();
}

static void NativeSetSettingLong(MonoString* name, int64_t value)
{
    FO_STACK_TRACE_ENTRY();

    SetSettingValueFromString(name, strex("{}", value).str());
}

static auto NativeGetSettingULong(MonoString* name) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    const string value = GetSettingValueAsString(name);
    return std::strtoull(value.c_str(), nullptr, 0);
}

static void NativeSetSettingULong(MonoString* name, uint64_t value)
{
    FO_STACK_TRACE_ENTRY();

    SetSettingValueFromString(name, strex("{}", value).str());
}

static auto NativeGetSettingFloat(MonoString* name) -> float32_t
{
    FO_STACK_TRACE_ENTRY();

    return strex(GetSettingValueAsString(name)).to_float32();
}

static void NativeSetSettingFloat(MonoString* name, float32_t value)
{
    FO_STACK_TRACE_ENTRY();

    SetSettingValueFromString(name, strex("{}", value).str());
}

static auto NativeGetSettingDouble(MonoString* name) -> float64_t
{
    FO_STACK_TRACE_ENTRY();

    return strex(GetSettingValueAsString(name)).to_float64();
}

static void NativeSetSettingDouble(MonoString* name, float64_t value)
{
    FO_STACK_TRACE_ENTRY();

    SetSettingValueFromString(name, strex("{}", value).str());
}

static auto NativeGetSettingString(MonoString* name) -> MonoString*
{
    FO_STACK_TRACE_ENTRY();

    const string value = GetSettingValueAsString(name);
    return mono_string_new(GetDomainOrThrow(mono_domain_get()), value.c_str());
}

static void NativeSetSettingString(MonoString* name, MonoString* value)
{
    FO_STACK_TRACE_ENTRY();

    SetSettingValueFromString(name, ToStringAndFree(value));
}

// === Native ABI: events, properties, methods and remote calls ===

static auto NativeSubscribeEvent(MonoString* owner_type, MonoString* event_name, void* entity_ptr, MonoObject* handler, mono_bool has_explicit_result, int32_t priority) -> void*
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    auto* meta = backend->GetMetadata();
    FO_VERIFY_AND_THROW(meta, "Backend metadata is not available");

    if (handler == nullptr) {
        throw ScriptSystemException("Null Managed event handler");
    }

    const string owner_type_name = ToStringAndFree(owner_type);
    const string event_name_str = ToStringAndFree(event_name);
    auto desc = FindEntityTypeDesc(meta, owner_type_name);

    if (desc == nullptr) {
        throw ScriptSystemException("Managed event owner type not found", owner_type_name);
    }

    const auto event_it = std::ranges::find_if(desc->Events, [&](const EntityEventDesc& event) { return event.Name == event_name_str; });

    if (event_it == desc->Events.end()) {
        throw ScriptSystemException("Managed event not found", owner_type_name, event_name_str);
    }

    auto entity = ResolveEntity(backend, entity_ptr);
    auto subscription = std::make_shared<ManagedEventSubscription>();
    subscription->Backend = backend;
    subscription->Image = mono_class_get_image(mono_object_get_class(handler));
    subscription->Handler = mono_gchandle_new(handler, false);
    subscription->HasExplicitResult = has_explicit_result != 0;

    if (!desc->IsGlobal) {
        subscription->Args.emplace_back(meta->ResolveComplexType(owner_type_name));
    }
    for (const ArgDesc& arg : event_it->Args) {
        if (!IsManagedBridgeType(arg.Type)) {
            throw ScriptSystemException("Managed event argument type is not supported yet", owner_type_name, event_name_str, arg.Name);
        }

        subscription->Args.emplace_back(arg.Type);
    }

    const uintptr_t token = subscription->Handler;
    Entity::EventCallbackData event_data;
    event_data.Callback = [subscription](FuncCallData& call) -> Entity::EventResult { return DispatchManagedEvent(subscription, call); };
    event_data.SubscriptionPtr = token;
    event_data.Priority = static_cast<Entity::EventPriority>(priority);
    event_data.HasExplicitResult = has_explicit_result != 0;

    entity->SubscribeEvent(event_name_str, std::move(event_data));
    return reinterpret_cast<void*>(token);
}

static void NativeUnsubscribeEvent(MonoString* event_name, void* entity_ptr, void* subscription)
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    auto entity = ResolveEntity(backend, entity_ptr);
    const string event_name_str = ToStringAndFree(event_name);
    entity->UnsubscribeEvent(event_name_str, reinterpret_cast<uintptr_t>(subscription));
}

static auto NativeFireEvent(MonoString* owner_type, MonoString* event_name, void* entity_ptr, MonoArray* args) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    auto* meta = backend->GetMetadata();
    FO_VERIFY_AND_THROW(meta, "Backend metadata is not available");

    const string owner_type_name = ToStringAndFree(owner_type);
    const string event_name_str = ToStringAndFree(event_name);
    auto desc = FindEntityTypeDesc(meta, owner_type_name);

    if (desc == nullptr) {
        throw ScriptSystemException("Managed event owner type not found", owner_type_name);
    }

    const auto event_it = std::ranges::find_if(desc->Events, [&](const EntityEventDesc& event) { return event.Name == event_name_str; });

    if (event_it == desc->Events.end()) {
        throw ScriptSystemException("Managed event not found", owner_type_name, event_name_str);
    }

    auto entity = ResolveEntity(backend, entity_ptr);
    const size_t args_count = args != nullptr ? mono_array_length(args) : 0;

    if (args_count != event_it->Args.size()) {
        throw ScriptSystemException("Managed event argument count mismatch", owner_type_name, event_name_str, args_count, event_it->Args.size());
    }

    const size_t first_event_arg = desc->IsGlobal ? 0 : 1;
    const size_t call_args_count = args_count + first_event_arg;

    if (call_args_count > MAX_CALL_ARGS) {
        throw ScriptSystemException("Managed event argument count exceeds bridge limit", owner_type_name, event_name_str, call_args_count);
    }

    const uint32_t args_handle = args != nullptr ? mono_gchandle_new(reinterpret_cast<MonoObject*>(args), 0) : 0;
    auto free_args_handle = scope_exit([args_handle]() noexcept {
        if (args_handle != 0) {
            mono_gchandle_free(args_handle);
        }
    });
    const auto get_args = [args, args_handle]() -> MonoArray* { return args_handle != 0 ? reinterpret_cast<MonoArray*>(mono_gchandle_get_target(args_handle)) : args; };

    array<void*, MAX_CALL_ARGS> args_data {};
    array<ManagedNativeValue, MAX_CALL_ARGS> native_args {};
    Entity* self_entity = entity.get();

    if (!desc->IsGlobal) {
        args_data[0] = make_ptr(&self_entity).void_cast();
    }

    for (size_t i = 0; i < args_count; i++) {
        const ArgDesc& arg_desc = event_it->Args[i];

        if (!IsManagedBridgeType(arg_desc.Type)) {
            throw ScriptSystemException("Managed event argument type is not supported yet", owner_type_name, event_name_str, arg_desc.Name);
        }

        auto* arg = mono_array_get(get_args(), MonoObject*, i);
        args_data[i + first_event_arg] = ConvertManagedObjectToNative(backend, arg_desc.Type, arg, native_args[i]);
    }

    vector<ptr<void>> args_ptrs;
    args_ptrs.reserve(call_args_count);
    for (size_t arg_idx = 0; arg_idx < call_args_count; arg_idx++) {
        args_ptrs.emplace_back(args_data[arg_idx]);
    }

    FuncCallData call {.Accessor = &MANAGED_DATA_ACCESSOR};
    call.ArgsData = const_span<ptr<void>> {args_ptrs.data(), args_ptrs.size()};

    const auto result = entity->FireEvent(event_name_str, call);

    for (size_t i = 0; i < args_count; i++) {
        const ArgDesc& arg_desc = event_it->Args[i];

        if (!arg_desc.Type.IsMutable) {
            continue;
        }

        MonoObject* arg = BoxNativeCallValue(backend, arg_desc.Type, args_data[i + first_event_arg], call.Accessor.get());
        mono_array_setref(get_args(), i, arg);
    }

    return static_cast<int32_t>(result);
}

static auto NativeGetProperty(MonoString* owner_type, MonoString* property_name, void* entity_ptr) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    auto entity = ResolveEntity(backend, entity_ptr);
    const string property_name_str = ToStringAndFree(property_name);
    auto nullable_prop = entity->GetProperties()->GetRegistrator()->FindProperty(property_name_str);

    if (!nullable_prop) {
        throw ScriptSystemException("Managed property not found", ToStringAndFree(owner_type), property_name_str);
    }

    auto prop = nullable_prop.as_ptr();

    if (prop->IsDict()) {
        if (!IsManagedBridgeFixedDictionaryProperty(prop)) {
            throw ScriptSystemException("Managed dictionary property bridge supports only fixed-size key/value types", prop->GetName());
        }
    }
    else {
        if (prop->IsBaseTypeRefType() && !IsDynamicManagedRefType(prop->GetBaseType())) {
            throw ScriptSystemException("Managed property ref type is not supported yet", prop->GetName());
        }
        if (!IsManagedBridgeSimpleType(prop->GetBaseType())) {
            throw ScriptSystemException("Managed property type is not supported yet", prop->GetName());
        }
    }

    PropertyRawData prop_data = GetPropertyRawData(entity, prop);
    return BoxPropertyValue(backend, prop, {prop_data.GetPtrAs<uint8_t>().get(), prop_data.GetSize()});
}

static void NativeSetProperty(MonoString* owner_type, MonoString* property_name, void* entity_ptr, MonoObject* value)
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    auto entity = ResolveEntity(backend, entity_ptr);
    const string property_name_str = ToStringAndFree(property_name);
    auto nullable_prop = entity->GetProperties()->GetRegistrator()->FindProperty(property_name_str);

    if (!nullable_prop) {
        throw ScriptSystemException("Managed property not found", ToStringAndFree(owner_type), property_name_str);
    }

    auto prop = nullable_prop.as_ptr();
    if (prop->IsDict()) {
        if (!IsManagedBridgeFixedDictionaryProperty(prop)) {
            throw ScriptSystemException("Managed dictionary property bridge supports only fixed-size key/value types", prop->GetName());
        }
    }
    else {
        if (prop->IsBaseTypeRefType() && !IsDynamicManagedRefType(prop->GetBaseType())) {
            throw ScriptSystemException("Managed property ref type is not supported yet", prop->GetName());
        }
        if (!IsManagedBridgeSimpleType(prop->GetBaseType())) {
            throw ScriptSystemException("Managed property type is not supported yet", prop->GetName());
        }
    }

    PropertyRawData prop_data = ConvertManagedObjectToPropertyData(backend, prop, value);
    entity->SetValueFromData(prop, prop_data);
}

static void NativeSetPropertyGetter(MonoString* owner_type, MonoString* property_name, MonoObject* getter)
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();

    if (getter == nullptr) {
        throw ScriptSystemException("Null Managed property getter");
    }

    const string owner_type_name = ToStringAndFree(owner_type);
    auto prop = ResolveVirtualPropertyForCallback(backend, owner_type, property_name, true, true);
    const uint32_t getter_handle = mono_gchandle_new(getter, false);

    prop->SetGetter([backend, getter_handle, prop, owner_type_name](nptr<Entity> entity, ptr<const Property>) -> PropertyRawData FO_DEFERRED {
        const ActiveBackendScope active_backend {backend};

        auto* domain = GetDomainOrThrow(backend->GetDomain());

        if (mono_thread_attach(domain) == nullptr) {
            throw ScriptSystemException("Failed to attach native thread to Managed runtime domain");
        }

        auto* handler = mono_gchandle_get_target(getter_handle);

        if (handler == nullptr) {
            throw ScriptSystemException("Managed property getter delegate was collected", prop->GetName());
        }

        auto* entity_obj = CreateEntityObject(backend, owner_type_name, entity);
        auto* args_array = mono_array_new(domain, mono_get_object_class(), 1);
        mono_array_setref(args_array, 0, entity_obj);

        MonoObject* result = InvokeManagedCallbackHandler(backend, handler, args_array);
        return ConvertManagedObjectToPropertyData(backend, prop, result);
    });
}

static void NativeAddPropertySetter(MonoString* owner_type, MonoString* property_name, MonoObject* setter)
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();

    if (setter == nullptr) {
        throw ScriptSystemException("Null Managed property setter");
    }

    const string owner_type_name = ToStringAndFree(owner_type);
    auto prop = ResolveVirtualPropertyForCallback(backend, owner_type, property_name, false, true);
    const uint32_t setter_handle = mono_gchandle_new(setter, false);

    prop->AddSetter([backend, setter_handle, prop, owner_type_name](nptr<Entity> entity, ptr<const Property>, PropertyRawData& prop_data) FO_DEFERRED {
        const ActiveBackendScope active_backend {backend};

        auto* domain = GetDomainOrThrow(backend->GetDomain());

        if (mono_thread_attach(domain) == nullptr) {
            throw ScriptSystemException("Failed to attach native thread to Managed runtime domain");
        }

        auto* handler = mono_gchandle_get_target(setter_handle);

        if (handler == nullptr) {
            throw ScriptSystemException("Managed property setter delegate was collected", prop->GetName());
        }

        auto* entity_obj = CreateEntityObject(backend, owner_type_name, entity);
        auto* value_obj = BoxPropertyValue(backend, prop, {prop_data.GetPtrAs<uint8_t>().get(), prop_data.GetSize()});
        auto* args_array = mono_array_new(domain, mono_get_object_class(), 2);
        mono_array_setref(args_array, 0, entity_obj);
        mono_array_setref(args_array, 1, value_obj);

        (void)InvokeManagedCallbackHandler(backend, handler, args_array);

        auto* modified_value = mono_array_get(args_array, MonoObject*, 1);
        prop_data = ConvertManagedObjectToPropertyData(backend, prop, modified_value);
    });
}

static void NativeAddPropertySetterWithProperty(MonoString* owner_type, MonoString* property_name, MonoObject* setter)
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();

    if (setter == nullptr) {
        throw ScriptSystemException("Null Managed property setter");
    }

    const string owner_type_name = ToStringAndFree(owner_type);
    auto prop = ResolveVirtualPropertyForCallback(backend, owner_type, property_name, false, true);
    const uint32_t setter_handle = mono_gchandle_new(setter, false);

    prop->AddSetter([backend, setter_handle, prop, owner_type_name](nptr<Entity> entity, ptr<const Property>, PropertyRawData& prop_data) FO_DEFERRED {
        const ActiveBackendScope active_backend {backend};

        auto* domain = GetDomainOrThrow(backend->GetDomain());

        if (mono_thread_attach(domain) == nullptr) {
            throw ScriptSystemException("Failed to attach native thread to Managed runtime domain");
        }

        auto* handler = mono_gchandle_get_target(setter_handle);

        if (handler == nullptr) {
            throw ScriptSystemException("Managed property setter delegate was collected", prop->GetName());
        }

        auto* entity_obj = CreateEntityObject(backend, owner_type_name, entity);
        auto* property_obj = CreatePropertyEnumObject(backend, owner_type_name, prop);
        auto* value_obj = BoxPropertyValue(backend, prop, {prop_data.GetPtrAs<uint8_t>().get(), prop_data.GetSize()});
        auto* args_array = mono_array_new(domain, mono_get_object_class(), 3);
        mono_array_setref(args_array, 0, entity_obj);
        mono_array_setref(args_array, 1, property_obj);
        mono_array_setref(args_array, 2, value_obj);

        (void)InvokeManagedCallbackHandler(backend, handler, args_array);

        auto* modified_value = mono_array_get(args_array, MonoObject*, 2);
        prop_data = ConvertManagedObjectToPropertyData(backend, prop, modified_value);
    });
}

static void NativeAddPropertyDeferredSetter(MonoString* owner_type, MonoString* property_name, MonoObject* setter)
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();

    if (setter == nullptr) {
        throw ScriptSystemException("Null Managed deferred property setter");
    }

    const string owner_type_name = ToStringAndFree(owner_type);
    auto prop = ResolveVirtualPropertyForCallback(backend, owner_type, property_name, false, false);
    const uint32_t setter_handle = mono_gchandle_new(setter, false);

    // Reaction-only post-set callback: the managed delegate receives just the entity and runs after the
    // value is written. It does not reproduce AngelScript's deferred-until-safe-point batching/dedup, which
    // suits reaction handlers; true deferral for re-entrancy-sensitive handlers is a follow-up.
    prop->AddPostSetter([backend, setter_handle, prop, owner_type_name](nptr<Entity> entity, ptr<const Property>) FO_DEFERRED {
        const ActiveBackendScope active_backend {backend};

        auto* domain = GetDomainOrThrow(backend->GetDomain());

        if (mono_thread_attach(domain) == nullptr) {
            throw ScriptSystemException("Failed to attach native thread to Managed runtime domain");
        }

        auto* handler = mono_gchandle_get_target(setter_handle);

        if (handler == nullptr) {
            throw ScriptSystemException("Managed deferred property setter delegate was collected", prop->GetName());
        }

        auto* entity_obj = CreateEntityObject(backend, owner_type_name, entity);
        auto* args_array = mono_array_new(domain, mono_get_object_class(), 1);
        mono_array_setref(args_array, 0, entity_obj);

        (void)InvokeManagedCallbackHandler(backend, handler, args_array);
    });
}

static auto NativeCallMethod(MonoString* owner_type, MonoString* method_name, int32_t method_index, void* entity_ptr, MonoArray* args) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    auto* meta = backend->GetMetadata();
    const string owner_type_name = ToStringAndFree(owner_type);
    const string method_name_str = ToStringAndFree(method_name);
    auto ref_type_desc = FindRefTypeDesc(meta, owner_type_name);
    const bool is_ref_type_method = ref_type_desc != nullptr;
    auto entity = !is_ref_type_method ? nptr<Entity> {ResolveEntity(backend, entity_ptr)} : nptr<Entity> {};
    const size_t args_count = args != nullptr ? mono_array_length(args) : 0;
    const uint32_t args_handle = args != nullptr ? mono_gchandle_new(reinterpret_cast<MonoObject*>(args), 0) : 0;
    auto free_args_handle = scope_exit([args_handle]() noexcept {
        if (args_handle != 0) {
            mono_gchandle_free(args_handle);
        }
    });
    const auto get_args = [args, args_handle]() -> MonoArray* { return args_handle != 0 ? reinterpret_cast<MonoArray*>(mono_gchandle_get_target(args_handle)) : args; };
    auto method = FindMethod(meta, owner_type_name, method_name_str, method_index, args_count);

    if (method == nullptr) {
        throw ScriptSystemException("Managed method not found", owner_type_name, method_name_str, method_index, args_count);
    }

    const bool is_ref_type_factory = is_ref_type_method && method->Name == "__Factory";
    const size_t first_method_arg = is_ref_type_factory ? 0 : 1;
    array<void*, MAX_CALL_ARGS> args_data {};
    array<ManagedNativeValue, MAX_CALL_ARGS> native_args {};
    Entity* self_entity = entity.get();
    void* self_ref = entity_ptr;

    if (!is_ref_type_factory) {
        args_data[0] = is_ref_type_method ? &self_ref : make_ptr(&self_entity).void_cast();
    }

    if (is_ref_type_method && !is_ref_type_factory && self_ref == nullptr) {
        throw ScriptSystemException("Managed ref type target is null", owner_type_name, method_name_str);
    }

    for (size_t i = 0; i < args_count; i++) {
        auto* arg = mono_array_get(get_args(), MonoObject*, i);
        args_data[i + first_method_arg] = ConvertManagedObjectToNative(backend, method->Args[i].Type, arg, native_args[i]);
    }

    vector<ptr<void>> args_ptrs;
    args_ptrs.reserve(args_count + first_method_arg);
    for (size_t arg_idx = 0; arg_idx < args_count + first_method_arg; arg_idx++) {
        args_ptrs.emplace_back(args_data[arg_idx]);
    }

    FuncCallData call {.Accessor = &MANAGED_DATA_ACCESSOR};
    call.ArgsData = const_span<ptr<void>> {args_ptrs.data(), args_ptrs.size()};

    ManagedNativeValue ret_storage;
    void* ret_data = nullptr;

    if (method->Ret) {
        if (method->Ret.Kind == ComplexTypeKind::Simple) {
            const BaseTypeDesc& ret_base = method->Ret.BaseType;

            if (ret_base.IsString) {
                ret_data = &ret_storage.Text;
            }
            else if (ret_base.IsHashedString) {
                ret_data = &ret_storage.Hash;
            }
            else if (ret_base.IsEntity) {
                ret_data = &ret_storage.EntityPtr;
            }
            else if (ret_base.IsRefType) {
                ret_data = &ret_storage.RefTypePtr;
            }
            else if (ret_base.IsPrimitive || ret_base.IsEnum || ret_base.IsStruct) {
                ret_data = ret_storage.Alloc(ret_base.Size);
            }
            else {
                throw ScriptSystemException("Unsupported Managed method return type", ret_base.Name);
            }
        }
        else if (method->Ret.Kind == ComplexTypeKind::Array) {
            ret_storage.Array = SafeAlloc::MakeUnique<ManagedArrayBridgeData>();
            ret_storage.Array->Backend = backend;
            ret_storage.Array->Type = method->Ret;
            ret_data = ret_storage.Array.get();
        }
        else if (method->Ret.Kind == ComplexTypeKind::Dict) {
            ret_storage.Dict = SafeAlloc::MakeUnique<ManagedDictBridgeData>();
            ret_storage.Dict->Backend = backend;
            ret_storage.Dict->Type = method->Ret;
            ret_data = ret_storage.Dict.get();
        }
        else {
            throw ScriptSystemException("Unsupported Managed method return type", method->Ret.BaseType.Name);
        }

        call.RetData = ret_data;
    }

    method->Call(call);

    const size_t mutable_args_count = static_cast<size_t>(std::ranges::count_if(method->Args, [](const ArgDesc& arg) { return arg.Type.IsMutable; }));

    if (mutable_args_count != 0) {
        if (!method->Ret && mutable_args_count == 1) {
            for (size_t i = 0; i < args_count; i++) {
                if (method->Args[i].Type.IsMutable) {
                    return BoxNativeCallValue(backend, method->Args[i].Type, args_data[i + first_method_arg], call.Accessor.get());
                }
            }

            throw ScriptSystemException("Managed mutable argument not found", owner_type_name, method_name_str);
        }

        auto* domain = GetDomainOrThrow(backend->GetDomain());
        const size_t result_count = mutable_args_count + (method->Ret ? 1 : 0);
        auto* result = mono_array_new(domain, mono_get_object_class(), result_count);
        const uint32_t result_handle = mono_gchandle_new(reinterpret_cast<MonoObject*>(result), 0);
        auto free_result_handle = scope_exit([result_handle]() noexcept { mono_gchandle_free(result_handle); });
        size_t result_index = 0;
        const auto get_result = [result_handle]() -> MonoArray* { return reinterpret_cast<MonoArray*>(mono_gchandle_get_target(result_handle)); };

        if (method->Ret) {
            MonoObject* ret = BoxNativeCallValue(backend, method->Ret, ret_data, call.Accessor.get());
            mono_array_setref(get_result(), result_index, ret);
            result_index++;
        }

        for (size_t i = 0; i < args_count; i++) {
            if (!method->Args[i].Type.IsMutable) {
                continue;
            }

            MonoObject* arg = BoxNativeCallValue(backend, method->Args[i].Type, args_data[i + first_method_arg], call.Accessor.get());
            mono_array_setref(get_result(), result_index, arg);
            result_index++;
        }

        return reinterpret_cast<MonoObject*>(get_result());
    }

    if (!method->Ret) {
        // The generated C# wrapper discards the result of a void method, so return null instead of
        // allocating a throwaway managed string on every such call.
        return nullptr;
    }

    return BoxNativeCallValue(backend, method->Ret, ret_data, call.Accessor.get());
}

static auto NativeInvokeScriptFunc(MonoString* func_name, MonoArray* args) -> mono_bool
{
    FO_STACK_TRACE_ENTRY();

    auto backend = ActiveBackend;

    if (!backend || backend->GetMetadata() == nullptr) {
        return 0;
    }

    auto* script_sys = dynamic_cast<ScriptSystem*>(backend->GetMetadata());

    if (script_sys == nullptr) {
        return 0;
    }

    const string func_name_str = ToStringAndFree(func_name);
    const hstring hashed_func_name = backend->GetMetadata()->Hashes.ToHashedString(func_name_str);
    const size_t args_count = args != nullptr ? mono_array_length(args) : 0;

    if (args_count > MAX_CALL_ARGS) {
        throw ScriptSystemException("Managed Invoke supports too many arguments", func_name_str, args_count, MAX_CALL_ARGS);
    }

    const uint32_t args_handle = args != nullptr ? mono_gchandle_new(reinterpret_cast<MonoObject*>(args), 0) : 0;
    auto free_args_handle = scope_exit([args_handle]() noexcept {
        if (args_handle != 0) {
            mono_gchandle_free(args_handle);
        }
    });
    const auto get_args = [args, args_handle]() -> MonoArray* { return args_handle != 0 ? reinterpret_cast<MonoArray*>(mono_gchandle_get_target(args_handle)) : args; };

    const auto candidates = script_sys->FindFuncCandidates(hashed_func_name);

    for (ptr<ScriptFuncDesc> func_desc : candidates) {
        const bool is_void_call = !func_desc->Ret && func_desc->Args.size() == args_count;
        const bool is_result_call = func_desc->Ret && func_desc->Args.size() + 1 == args_count;

        if (!func_desc->Call || (!is_void_call && !is_result_call)) {
            continue;
        }

        const size_t call_args_count = func_desc->Args.size();
        array<void*, MAX_CALL_ARGS> args_data {};
        array<ManagedNativeValue, MAX_CALL_ARGS> native_args {};
        bool converted = true;

        try {
            for (size_t i = 0; i < call_args_count; i++) {
                MonoObject* arg = mono_array_get(get_args(), MonoObject*, i);

                if (!CanConvertManagedObjectToNative(backend.as_ptr(), func_desc->Args[i].Type, arg)) {
                    converted = false;
                    break;
                }

                args_data[i] = ConvertManagedObjectToNative(backend.as_ptr(), func_desc->Args[i].Type, arg, native_args[i]);
            }
        }
        catch (const std::exception&) {
            converted = false;
        }

        if (!converted) {
            continue;
        }

        vector<ptr<void>> args_ptrs;
        args_ptrs.reserve(call_args_count);
        for (size_t arg_idx = 0; arg_idx < call_args_count; arg_idx++) {
            args_ptrs.emplace_back(args_data[arg_idx]);
        }

        FuncCallData call {.Accessor = &MANAGED_DATA_ACCESSOR};
        call.ArgsData = const_span<ptr<void>> {args_ptrs.data(), args_ptrs.size()};
        ManagedNativeValue ret_storage;
        void* ret_data = nullptr;

        if (is_result_call) {
            if (func_desc->Ret.Kind == ComplexTypeKind::Simple) {
                const BaseTypeDesc& ret_base = func_desc->Ret.BaseType;

                if (ret_base.IsString) {
                    ret_data = &ret_storage.Text;
                }
                else if (ret_base.IsHashedString) {
                    ret_data = &ret_storage.Hash;
                }
                else if (ret_base.IsEntity) {
                    ret_data = &ret_storage.EntityPtr;
                }
                else if (ret_base.IsRefType) {
                    ret_data = &ret_storage.RefTypePtr;
                }
                else if (ret_base.IsPrimitive || ret_base.IsEnum || ret_base.IsStruct) {
                    ret_data = ret_storage.Alloc(ret_base.Size);
                }
                else {
                    throw ScriptSystemException("Unsupported Managed invoke return type", ret_base.Name);
                }
            }
            else if (func_desc->Ret.Kind == ComplexTypeKind::Array) {
                ret_storage.Array = SafeAlloc::MakeUnique<ManagedArrayBridgeData>();
                ret_storage.Array->Backend = backend;
                ret_storage.Array->Type = func_desc->Ret;
                ret_data = ret_storage.Array.get();
            }
            else if (func_desc->Ret.Kind == ComplexTypeKind::Dict) {
                ret_storage.Dict = SafeAlloc::MakeUnique<ManagedDictBridgeData>();
                ret_storage.Dict->Backend = backend;
                ret_storage.Dict->Type = func_desc->Ret;
                ret_data = ret_storage.Dict.get();
            }
            else {
                throw ScriptSystemException("Unsupported Managed invoke return type", func_desc->Ret.BaseType.Name);
            }

            call.RetData = ret_data;
        }

        try {
            func_desc->Call(call);
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
            return 0;
        }

        if (is_result_call) {
            MonoObject* ret = nullptr;

            if (func_desc->Ret.Kind == ComplexTypeKind::Array) {
                auto* ret_array = ret_storage.Array.get();
                FO_VERIFY_AND_THROW(ret_array != nullptr, "Managed invoke array return storage is null");

                ret = ret_array->GetObject();
                if (ret == nullptr) {
                    ret = CreateManagedList(backend.as_ptr(), func_desc->Ret.BaseType);
                    ret_array->SetObject(ret);
                }
            }
            else if (func_desc->Ret.Kind == ComplexTypeKind::Dict) {
                auto* ret_dict = ret_storage.Dict.get();
                FO_VERIFY_AND_THROW(ret_dict != nullptr, "Managed invoke dictionary return storage is null");
                FO_VERIFY_AND_THROW(func_desc->Ret.KeyType, "Managed invoke dictionary return type has no key type");

                ret = ret_dict->GetObject();
                if (ret == nullptr) {
                    ret = CreateManagedDictionary(backend.as_ptr(), *func_desc->Ret.KeyType, func_desc->Ret.BaseType);
                    ret_dict->SetObject(ret);
                }
            }
            else {
                ret = BoxNativeCallValue(backend.as_ptr(), func_desc->Ret, ret_data, call.Accessor.get());
            }

            mono_array_setref(get_args(), args_count - 1, ret);
        }

        for (size_t i = 0; i < call_args_count; i++) {
            if (!func_desc->Args[i].Type.IsMutable) {
                continue;
            }

            MonoObject* arg = BoxNativeCallValue(backend.as_ptr(), func_desc->Args[i].Type, args_data[i], call.Accessor.get());
            mono_array_setref(get_args(), i, arg);
        }

        return 1;
    }

    return 0;
}

static void NativeRegisterGlobalScriptFunc(MonoString* full_name, MonoString* attr_name, MonoArray* param_type_names, MonoString* ret_type_name, MonoObject* handler, mono_bool skip_existing_script_func)
{
    FO_STACK_TRACE_ENTRY();

    // Register a managed global script function into the engine's cross-backend function map under a named marker
    // attribute, so a consumer that resolves funcs by attribute (via `ScriptSystem::FindFunc`) can invoke it. The
    // signature's type descriptors are built from the engine base-type names the C# side supplies, so the
    // FindFunc<Ret, Args...> validation matches; the call bridge reuses DispatchManagedCallback (the same path
    // that backs managed native callbacks), and AttributeChecker reports the attribute name the func was registered
    // under so the consumer's marker check passes. (Consumers that resolve funcs by marker live outside the
    // engine; the engine provides only this generic registry.) Transition modules can retain an exact function
    // already registered by another backend while still registering their managed-only functions.
    auto backend = ActiveBackend;
    FO_VERIFY_AND_THROW(backend, "No active managed script backend");
    auto* meta = backend->GetMetadata();
    FO_VERIFY_AND_THROW(meta, "Backend metadata is not available");

    const string full_name_str = ToStringAndFree(full_name);
    const string attr_name_str = ToStringAndFree(attr_name);
    const string ret_type_str = ToStringAndFree(ret_type_name);

    if (handler == nullptr) {
        throw ScriptSystemException("Null Managed global script func handler", full_name_str);
    }

    ComplexTypeDesc ret;
    vector<ComplexTypeDesc> args;
    bool supported = true;

    const size_t param_count = param_type_names != nullptr ? mono_array_length(param_type_names) : 0;

    for (size_t i = 0; i < param_count; i++) {
        auto* param_mono_str = mono_array_get(param_type_names, MonoString*, i);
        auto arg_type = MakeManagedGlobalSimpleType(meta, ToStringAndFree(param_mono_str));

        if (!arg_type) {
            supported = false;
            break;
        }

        args.emplace_back(std::move(arg_type));
    }

    if (supported && ret_type_str != "void") {
        ret = MakeManagedGlobalSimpleType(meta, ret_type_str);

        if (!ret) {
            supported = false;
        }
    }

    if (!supported) {
        WriteLog("Managed global script func '{}' has an unsupported signature; skipping registration", full_name_str);
        return;
    }

    auto* script_sys = dynamic_cast<ScriptSystem*>(meta);
    FO_VERIFY_AND_THROW(script_sys, "Backend metadata does not expose a script system");

    const hstring hashed_func_name = meta->Hashes.ToHashedString(full_name_str);

    if (skip_existing_script_func != 0) {
        const auto candidates = script_sys->FindFuncCandidates(hashed_func_name);

        for (ptr<ScriptFuncDesc> candidate : candidates) {
            if (!candidate->Call || candidate->Ret != ret || candidate->Args.size() != args.size() || !candidate->AttributeChecker(attr_name_str)) {
                continue;
            }

            bool args_match = true;

            for (size_t i = 0; i < args.size(); i++) {
                if (candidate->Args[i].Type != args[i]) {
                    args_match = false;
                    break;
                }
            }

            if (args_match) {
                return;
            }
        }
    }

    const uint32_t handler_handle = mono_gchandle_new(handler, false);

    auto func_desc = SafeAlloc::MakeUnique<ScriptFuncDesc>();
    func_desc->Name = hashed_func_name;
    func_desc->Ret = ret;
    func_desc->Args.reserve(args.size());

    for (const ComplexTypeDesc& arg_type : args) {
        func_desc->Args.emplace_back(ArgDesc {.Name = {}, .Type = arg_type});
    }

    func_desc->AttributeChecker = [attr_name_str](string_view attribute) -> bool { return attribute == attr_name_str; };
    func_desc->Call = [backend = backend.as_ptr(), handler_handle, ret, args](FuncCallData& call) { DispatchManagedCallback(backend, handler_handle, ret, args, call); };

    backend->AddManagedGlobalFunc(std::move(func_desc), handler_handle);
}

static void NativeRegisterRemoteCallHandler(MonoString* name_str, int32_t param_count, MonoObject* handler)
{
    FO_STACK_TRACE_ENTRY();

    // Wire a managed inbound remote-call handler ([ServerRemoteCall]/[ClientRemoteCall]/[AdminRemoteCall]).
    // Match the name to an inbound RemoteCallDesc for this side, then register engine->SetRemoteCallHandler with
    // a handler that deserializes the wire args via the shared RemoteCallWire and invokes the managed delegate
    // through the DispatchManagedCallback path (server side prepends the calling Player). Client-side managed
    // cutover may keep RPC metadata in an AngelScript facade while the implementation already lives in C#.
    auto backend = ActiveBackend;
    FO_VERIFY_AND_THROW(backend, "No active managed script backend");
    auto* meta = backend->GetMetadata();
    FO_VERIFY_AND_THROW(meta, "Backend metadata is not available");

    const string name = ToStringAndFree(name_str);

    if (handler == nullptr) {
        throw ScriptSystemException("Null Managed remote call handler", name);
    }

    auto* engine = dynamic_cast<BaseEngine*>(meta);

    if (engine == nullptr) {
        return;
    }

    const hstring name_hashed = meta->Hashes.ToHashedString(name);
    const auto inbound_calls = meta->GetInboundRemoteCalls();
    const auto it = inbound_calls->find(name_hashed);

    if (it == inbound_calls->end()) {
        return;
    }

    const RemoteCallDesc& inbound_call = it->second;

    const bool managed_declared_call = strvex(inbound_call.SubsystemHint).ends_with("cs");
    const bool client_facade_call = engine->GetSide() == EngineSideKind::ClientSide && strvex(inbound_call.SubsystemHint).ends_with("fos");

    if (!managed_declared_call && !client_facade_call) {
        return;
    }

    const bool server_side = engine->GetSide() == EngineSideKind::ServerSide;

    // Build the call's argument type list: the server side prepends the calling Player, then the wire args.
    vector<ComplexTypeDesc> args;
    args.reserve(inbound_call.Args.size() + (server_side ? 1 : 0));

    if (server_side) {
        auto player_type = MakeManagedGlobalSimpleType(meta, "Player");

        if (!player_type) {
            throw ScriptSystemException("Managed remote call cannot resolve Player type", name);
        }

        args.emplace_back(std::move(player_type));
    }

    for (const auto& arg : inbound_call.Args) {
        if (arg.Type.BaseType.IsRefType && !IsDynamicManagedRefType(arg.Type.BaseType)) {
            throw ScriptSystemException("Managed remote call supports only dynamic ref-type args", name, arg.Type.BaseType.Name);
        }
        if (arg.Type.Kind != ComplexTypeKind::Simple && arg.Type.Kind != ComplexTypeKind::Array) {
            throw ScriptSystemException("Managed remote call supports only scalar and array args for now", name);
        }

        args.emplace_back(arg.Type);
    }

    if (numeric_cast<size_t>(param_count) != args.size()) {
        throw ScriptSystemException("Managed remote call argument count mismatch", name);
    }

    const uint32_t handler_handle = mono_gchandle_new(handler, false);
    backend->AddRemoteCallHandlerGcHandle(handler_handle);

    WriteLog("Registered managed inbound remote call '{}' ({} wire arg(s), {}){}", name, inbound_call.Args.size(), server_side ? "server" : "client", client_facade_call ? ", replacing script handler" : "");

    engine->SetRemoteCallHandler(
        name_hashed,
        [backend = backend.as_ptr(), engine, args, handler_handle, server_side](hstring, nptr<Entity> entity, span<uint8_t> data) FO_DEFERRED {
            // Attach to the Managed domain up front: building managed List objects for array args (below) invokes mono,
            // and this handler may run on the engine's network-receive thread.
            auto* domain = GetDomainOrThrow(backend->GetDomain());

            if (mono_thread_attach(domain) == nullptr) {
                throw ScriptSystemException("Failed to attach native thread to Managed runtime domain");
            }

            DataReader reader(data);
            RemoteCallReadStorage storage;
            list<ManagedArrayBridgeData> array_bridges;
            list<refcount_ptr<DynamicRefTypeInstance>> ref_instances;
            const RemoteCallWireHooks hooks {
                .RawToRefType = [&ref_instances](const BaseTypeDesc& type, span<const uint8_t> raw_data) -> ptr<void> {
                    // Deserialize the ref type's fields into a DynamicRefTypeInstance (shared engine type); the
                    // MANAGED_DATA_ACCESSOR boxes it into a managed object (CreateRefTypeObject) when invoking. Kept
                    // alive in `ref_instances` for the call duration; the FuncCallData arg is a pointer to its pointer.
                    auto ref_instance = SafeAlloc::MakeRefCounted<DynamicRefTypeInstance>(type.RefType->FieldsRegistrator.get());
                    ref_instance->LoadFromRawData(type, raw_data);
                    auto&& stored = ref_instances.emplace_back(std::move(ref_instance));
                    return make_ptr(stored.get_pp()).reinterpret_as<void>();
                },
            };

            FuncCallData call {.Accessor = &MANAGED_DATA_ACCESSOR};
            array<void*, MAX_CALL_ARGS> data_storage;
            size_t arg_index = 0;
            Entity* raw_entity = entity.get();

            if (server_side) {
                data_storage[arg_index++] = make_ptr(&raw_entity).void_cast();
            }

            for (; arg_index < args.size(); arg_index++) {
                const ComplexTypeDesc& arg_type = args[arg_index];

                if (arg_type.Kind == ComplexTypeKind::Simple) {
                    data_storage[arg_index] = ReadRemoteCallSimple(reader, arg_type.BaseType, engine->Hashes, storage, hooks).get();
                }
                else {
                    // Array. Wire: int32 count, then each element (shared scalar format). Deserialize into a managed
                    // List rooted by a bridge so the MANAGED_DATA_ACCESSOR can read it back when boxing the argument.
                    const int32_t count = reader.Read<int32_t>();
                    FO_VERIFY_AND_THROW(count >= 0, "Remote call array element count is negative");

                    auto& bridge = array_bridges.emplace_back();
                    bridge.Backend = backend;
                    bridge.Type = arg_type;
                    bridge.SetObject(CreateManagedList(backend.as_ptr(), arg_type.BaseType));

                    for (int32_t j = 0; j < count; j++) {
                        auto element = ReadRemoteCallSimple(reader, arg_type.BaseType, engine->Hashes, storage, hooks);
                        AddManagedListItem(backend.as_ptr(), bridge.GetObject(), BoxNativeSimpleValue(backend.as_ptr(), arg_type.BaseType, element.get()));
                    }

                    data_storage[arg_index] = make_ptr(&bridge).void_cast();
                }
            }

            reader.VerifyEnd();

            vector<ptr<void>> args_ptrs;
            args_ptrs.reserve(args.size());
            for (size_t data_idx = 0; data_idx < args.size(); data_idx++) {
                args_ptrs.emplace_back(data_storage[data_idx]);
            }
            call.ArgsData = const_span<ptr<void>> {args_ptrs.data(), args_ptrs.size()};

            try {
                DispatchManagedCallback(backend, handler_handle, ComplexTypeDesc {}, args, call);
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
            }
        },
        client_facade_call);
}

static void NativeSendRemoteCall(MonoObject* caller, MonoString* name_str, MonoArray* args_array)
{
    FO_STACK_TRACE_ENTRY();

    // Managed outbound remote call: serialize the boxed args into the shared RemoteCallWire format and hand them to
    // engine->SendRemoteCall (which forwards to the remote peer). Mirrors AngelScript's OutboundRemoteCallFunc; the
    // call name must be an outbound "cs" RemoteCallDesc for this side. `caller` is the entity the call is bound to
    // (the Player for a client->server ServerCall, etc.).
    auto backend = ActiveBackend;
    FO_VERIFY_AND_THROW(backend, "No active managed script backend");
    auto* meta = backend->GetMetadata();
    FO_VERIFY_AND_THROW(meta, "Backend metadata is not available");

    const string name = ToStringAndFree(name_str);

    auto* engine = dynamic_cast<BaseEngine*>(meta);

    if (engine == nullptr) {
        throw ScriptSystemException("Managed remote call send without a game engine", name);
    }

    const hstring name_hashed = meta->Hashes.ToHashedString(name);
    const auto outbound_calls = meta->GetOutboundRemoteCalls();
    const auto it = outbound_calls->find(name_hashed);

    if (it == outbound_calls->end()) {
        throw ScriptSystemException("Unknown managed outbound remote call", name);
    }

    const RemoteCallDesc& outbound_call = it->second;

    // A managed caller may send BOTH managed ("cs") and AngelScript-defined ("fos") outbound RPCs: the args are
    // serialized into the shared RemoteCallWire byte format that the AngelScript inbound handler reads, so an
    // AngelScript-handled server RPC is invocable from managed code (required for the managed client cutover, where
    // the managed client replaces the AngelScript one). The baker emits the typed surface for all outbound RPCs in
    // lockstep with this. (The earlier cs-only guard was conservative, not a wire limitation.)
    nptr<Entity> caller_entity = caller != nullptr ? ExtractEntityPtr(caller) : nullptr;
    const auto data = SerializeManagedRemoteCallArgs(backend.as_ptr(), outbound_call.Args, args_array, name);

    engine->SendRemoteCall(name_hashed, caller_entity.as_ptr(), data);
}

static void NativeLoopbackRemoteCall(MonoObject* caller, MonoString* name_str, MonoArray* args_array)
{
    FO_STACK_TRACE_ENTRY();

    // Diagnostic/test helper: serialize the boxed args into the shared RemoteCallWire format and dispatch them
    // through the engine's real inbound path (HandleInboundRemoteCall) in-process, with no network peer. Exercises
    // the managed serialize -> deserialize -> dispatch glue end to end on a single side. The name must be an inbound
    // "cs" RemoteCallDesc for this side (so a handler is registered).
    auto backend = ActiveBackend;
    FO_VERIFY_AND_THROW(backend, "No active managed script backend");
    auto* meta = backend->GetMetadata();
    FO_VERIFY_AND_THROW(meta, "Backend metadata is not available");

    const string name = ToStringAndFree(name_str);

    auto* engine = dynamic_cast<BaseEngine*>(meta);

    if (engine == nullptr) {
        throw ScriptSystemException("Managed remote call loopback without a game engine", name);
    }

    const hstring name_hashed = meta->Hashes.ToHashedString(name);
    const auto inbound_calls = meta->GetInboundRemoteCalls();
    const auto it = inbound_calls->find(name_hashed);

    if (it == inbound_calls->end()) {
        throw ScriptSystemException("Unknown managed inbound remote call for loopback", name);
    }

    const RemoteCallDesc& inbound_call = it->second;

    if (!strvex(inbound_call.SubsystemHint).ends_with("cs")) {
        throw ScriptSystemException("Managed loopback remote call is not a managed (cs) call", name);
    }

    nptr<Entity> caller_entity = caller != nullptr ? ExtractEntityPtr(caller) : nullptr;
    auto data = SerializeManagedRemoteCallArgs(backend.as_ptr(), inbound_call.Args, args_array, name);

    engine->HandleInboundRemoteCall(name_hashed, caller_entity, data);
}

// === Internal-call registration ===

static void RegisterInternalCalls()
{
    FO_STACK_TRACE_ENTRY();

    mono_add_internal_call("FOnline.Native::Log", reinterpret_cast<const void*>(NativeLog));
    mono_add_internal_call("FOnline.Native::GetHashStr", reinterpret_cast<const void*>(NativeGetHashStr));
    mono_add_internal_call("FOnline.Native::GetHash", reinterpret_cast<const void*>(NativeGetHash));
    mono_add_internal_call("FOnline.Native::GetEntityId", reinterpret_cast<const void*>(NativeGetEntityId));
    mono_add_internal_call("FOnline.Native::GetEntityProtoId", reinterpret_cast<const void*>(NativeGetEntityProtoId));
    mono_add_internal_call("FOnline.Native::AddRefEntity", reinterpret_cast<const void*>(NativeAddRefEntity));
    mono_add_internal_call("FOnline.Native::ReleaseEntity", reinterpret_cast<const void*>(NativeReleaseEntity));
    mono_add_internal_call("FOnline.Native::IsEntityDestroyed", reinterpret_cast<const void*>(NativeIsEntityDestroyed));
    mono_add_internal_call("FOnline.Native::IsEntityDestroying", reinterpret_cast<const void*>(NativeIsEntityDestroying));
    mono_add_internal_call("FOnline.Native::HdirToMdir", reinterpret_cast<const void*>(NativeHdirToMdir));
    mono_add_internal_call("FOnline.Native::MdirHex", reinterpret_cast<const void*>(NativeMdirHex));
    mono_add_internal_call("FOnline.Native::MdirRotateHex", reinterpret_cast<const void*>(NativeMdirRotateHex));
    mono_add_internal_call("FOnline.Native::MdirReverse", reinterpret_cast<const void*>(NativeMdirReverse));
    mono_add_internal_call("FOnline.Native::GetEntityName", reinterpret_cast<const void*>(NativeGetEntityName));
    mono_add_internal_call("FOnline.Native::SubscribeEvent", reinterpret_cast<const void*>(NativeSubscribeEvent));
    mono_add_internal_call("FOnline.Native::UnsubscribeEvent", reinterpret_cast<const void*>(NativeUnsubscribeEvent));
    mono_add_internal_call("FOnline.Native::FireEvent", reinterpret_cast<const void*>(NativeFireEvent));
    mono_add_internal_call("FOnline.Native::GetProperty", reinterpret_cast<const void*>(NativeGetProperty));
    mono_add_internal_call("FOnline.Native::SetProperty", reinterpret_cast<const void*>(NativeSetProperty));
    mono_add_internal_call("FOnline.Native::SetPropertyGetter", reinterpret_cast<const void*>(NativeSetPropertyGetter));
    mono_add_internal_call("FOnline.Native::AddPropertySetter", reinterpret_cast<const void*>(NativeAddPropertySetter));
    mono_add_internal_call("FOnline.Native::AddPropertySetterWithProperty", reinterpret_cast<const void*>(NativeAddPropertySetterWithProperty));
    mono_add_internal_call("FOnline.Native::AddPropertyDeferredSetter", reinterpret_cast<const void*>(NativeAddPropertyDeferredSetter));
    mono_add_internal_call("FOnline.Native::CallMethod", reinterpret_cast<const void*>(NativeCallMethod));
    mono_add_internal_call("FOnline.Native::InvokeScriptFunc", reinterpret_cast<const void*>(NativeInvokeScriptFunc));
    mono_add_internal_call("FOnline.Native::GetBackendAliveFlag", reinterpret_cast<const void*>(NativeGetBackendAliveFlag));
    mono_add_internal_call("FOnline.Native::GetBackend", reinterpret_cast<const void*>(NativeGetBackend));
    mono_add_internal_call("FOnline.Native::GetProtoEntity", reinterpret_cast<const void*>(NativeGetProtoEntity));
    mono_add_internal_call("FOnline.Native::CheckProtoEntity", reinterpret_cast<const void*>(NativeCheckProtoEntity));
    mono_add_internal_call("FOnline.Native::GetProtoEntityCount", reinterpret_cast<const void*>(NativeGetProtoEntityCount));
    mono_add_internal_call("FOnline.Native::GetProtoEntityAt", reinterpret_cast<const void*>(NativeGetProtoEntityAt));
    mono_add_internal_call("FOnline.Native::CreateInnerEntity", reinterpret_cast<const void*>(NativeCreateInnerEntity));
    mono_add_internal_call("FOnline.Native::HasInnerEntities", reinterpret_cast<const void*>(NativeHasInnerEntities));
    mono_add_internal_call("FOnline.Native::GetInnerEntity", reinterpret_cast<const void*>(NativeGetInnerEntity));
    mono_add_internal_call("FOnline.Native::GetInnerEntityCount", reinterpret_cast<const void*>(NativeGetInnerEntityCount));
    mono_add_internal_call("FOnline.Native::GetInnerEntityAt", reinterpret_cast<const void*>(NativeGetInnerEntityAt));
    mono_add_internal_call("FOnline.Native::GetEntityValueAsInt", reinterpret_cast<const void*>(NativeGetEntityValueAsInt));
    mono_add_internal_call("FOnline.Native::SetEntityValueAsInt", reinterpret_cast<const void*>(NativeSetEntityValueAsInt));
    mono_add_internal_call("FOnline.Native::GetEntityValueAsAny", reinterpret_cast<const void*>(NativeGetEntityValueAsAny));
    mono_add_internal_call("FOnline.Native::SetEntityValueAsAny", reinterpret_cast<const void*>(NativeSetEntityValueAsAny));
    mono_add_internal_call("FOnline.Native::RegisterGlobalScriptFunc", reinterpret_cast<const void*>(NativeRegisterGlobalScriptFunc));
    mono_add_internal_call("FOnline.Native::RegisterRemoteCallHandler", reinterpret_cast<const void*>(NativeRegisterRemoteCallHandler));
    mono_add_internal_call("FOnline.Native::SendRemoteCall", reinterpret_cast<const void*>(NativeSendRemoteCall));
    mono_add_internal_call("FOnline.Native::LoopbackRemoteCall", reinterpret_cast<const void*>(NativeLoopbackRemoteCall));
    mono_add_internal_call("FOnline.Native::GetSettingBoolRaw", reinterpret_cast<const void*>(NativeGetSettingBoolRaw));
    mono_add_internal_call("FOnline.Native::SetSettingBoolRaw", reinterpret_cast<const void*>(NativeSetSettingBoolRaw));
    mono_add_internal_call("FOnline.Native::GetSettingInt", reinterpret_cast<const void*>(NativeGetSettingInt));
    mono_add_internal_call("FOnline.Native::SetSettingInt", reinterpret_cast<const void*>(NativeSetSettingInt));
    mono_add_internal_call("FOnline.Native::GetSettingUInt", reinterpret_cast<const void*>(NativeGetSettingUInt));
    mono_add_internal_call("FOnline.Native::SetSettingUInt", reinterpret_cast<const void*>(NativeSetSettingUInt));
    mono_add_internal_call("FOnline.Native::GetSettingLong", reinterpret_cast<const void*>(NativeGetSettingLong));
    mono_add_internal_call("FOnline.Native::SetSettingLong", reinterpret_cast<const void*>(NativeSetSettingLong));
    mono_add_internal_call("FOnline.Native::GetSettingULong", reinterpret_cast<const void*>(NativeGetSettingULong));
    mono_add_internal_call("FOnline.Native::SetSettingULong", reinterpret_cast<const void*>(NativeSetSettingULong));
    mono_add_internal_call("FOnline.Native::GetSettingFloat", reinterpret_cast<const void*>(NativeGetSettingFloat));
    mono_add_internal_call("FOnline.Native::SetSettingFloat", reinterpret_cast<const void*>(NativeSetSettingFloat));
    mono_add_internal_call("FOnline.Native::GetSettingDouble", reinterpret_cast<const void*>(NativeGetSettingDouble));
    mono_add_internal_call("FOnline.Native::SetSettingDouble", reinterpret_cast<const void*>(NativeSetSettingDouble));
    mono_add_internal_call("FOnline.Native::GetSettingString", reinterpret_cast<const void*>(NativeGetSettingString));
    mono_add_internal_call("FOnline.Native::SetSettingString", reinterpret_cast<const void*>(NativeSetSettingString));
}

// === Settings access helpers ===

static auto GetSettingValueAsString(GlobalSettings* settings, const string& setting_name) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (settings == nullptr) {
        return {};
    }

#define GET_SETTING_VALUE(sett) return strex("{}", settings->sett).str()
#define FIXED_SETTING(type, group, name, ...) \
    case const_hash(#name): \
    case const_hash(#group "." #name): \
        GET_SETTING_VALUE(name)
#define VARIABLE_SETTING(type, group, name, ...) \
    case const_hash(#name): \
    case const_hash(#group "." #name): \
        GET_SETTING_VALUE(name)
#define SETTING_GROUP(name, ...)
#define SETTING_GROUP_END()

    switch (const_hash(setting_name.c_str())) {
#include "Settings.inc"
    default:
        return settings->GetCustomSetting(setting_name);
    }

#undef GET_SETTING_VALUE
#undef FIXED_SETTING
#undef VARIABLE_SETTING
#undef SETTING_GROUP
#undef SETTING_GROUP_END
}

static auto GetSettingValueAsString(MonoString* name) -> string
{
    FO_STACK_TRACE_ENTRY();

    GlobalSettings* settings = GetBackendSettings(GetActiveBackendOrThrow());
    const string setting_name = ToStringAndFree(name);
    return GetSettingValueAsString(settings, setting_name);
}

static void SetSettingValueFromString(GlobalSettings* settings, string_view setting_name, string value)
{
    FO_STACK_TRACE_ENTRY();

    if (settings == nullptr) {
        return;
    }

    settings->SetCustomSetting(setting_name, any_t(std::move(value)));
}

static void SetSettingValueFromString(MonoString* name, string value)
{
    FO_STACK_TRACE_ENTRY();

    GlobalSettings* settings = GetBackendSettings(GetActiveBackendOrThrow());
    const string setting_name = ToStringAndFree(name);
    SetSettingValueFromString(settings, setting_name, std::move(value));
}

// === Property getter/setter callback bridge ===

static auto InvokeManagedCallbackHandler(ptr<ManagedScriptBackend> backend, MonoObject* handler, MonoArray* args_array) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    auto* native_class = FindFOnlineClass(backend, "Native");

    if (native_class == nullptr) {
        throw ScriptSystemException("Managed Native class not found");
    }

    auto* invoke_callback = mono_class_get_method_from_name(native_class, "InvokeCallback", 2);

    if (invoke_callback == nullptr) {
        throw ScriptSystemException("Managed Native.InvokeCallback method not found");
    }

    void* invoke_args[] = {handler, args_array};
    MonoObject* exception = nullptr;
    MonoObject* result = mono_runtime_invoke(invoke_callback, nullptr, invoke_args, &exception);
    ThrowIfManagedException(exception, "Managed property callback failed");
    return result;
}

static auto ResolveVirtualPropertyForCallback(ptr<ManagedScriptBackend> backend, MonoString* owner_type, MonoString* property_name, bool require_virtual, bool require_marshalable_value) -> ptr<const Property>
{
    FO_STACK_TRACE_ENTRY();

    auto* meta = backend->GetMetadata();
    FO_VERIFY_AND_THROW(meta, "Backend metadata is not available");

    const string owner_type_name = ToStringAndFree(owner_type);
    const string property_name_str = ToStringAndFree(property_name);
    auto nullable_registrator = meta->GetPropertyRegistrator(owner_type_name);

    if (!nullable_registrator) {
        throw ScriptSystemException("Managed property owner type not found", owner_type_name);
    }

    auto registrator = nullable_registrator.as_ptr();
    auto nullable_prop = registrator->FindProperty(property_name_str);

    if (!nullable_prop) {
        throw ScriptSystemException("Managed property not found", owner_type_name, property_name_str);
    }

    auto prop = nullable_prop.as_ptr();
    if (require_virtual && !prop->IsVirtual()) {
        throw ScriptSystemException("Managed property getter requires a virtual property", prop->GetName());
    }

    // Deferred (post-set) callbacks receive only the entity, so they place no constraint on the value
    // type; getter/setter callbacks that marshal the value require a bridgeable simple type.
    if (require_marshalable_value) {
        if (prop->IsBaseTypeRefType() && !IsDynamicManagedRefType(prop->GetBaseType())) {
            throw ScriptSystemException("Managed property callback ref type is not supported yet", prop->GetName());
        }
        if (!IsManagedBridgeSimpleType(prop->GetBaseType())) {
            throw ScriptSystemException("Managed property callback type is not supported yet", prop->GetName());
        }
    }

    return prop;
}

static void DispatchManagedCallback(ptr<ManagedScriptBackend> backend, uint32_t handler_handle, const ComplexTypeDesc& ret, const vector<ComplexTypeDesc>& args, FuncCallData& call)
{
    FO_STACK_TRACE_ENTRY();

    const ActiveBackendScope active_backend {backend};

    auto* domain = GetDomainOrThrow(backend->GetDomain());

    if (mono_thread_attach(domain) == nullptr) {
        throw ScriptSystemException("Failed to attach native thread to Managed runtime domain");
    }

    auto* handler = mono_gchandle_get_target(handler_handle);

    if (handler == nullptr) {
        throw ScriptSystemException("Managed callback delegate was collected");
    }
    if (call.ArgsData.size() != args.size()) {
        throw ScriptSystemException("Managed callback argument count mismatch");
    }

    auto* args_array = mono_array_new(domain, mono_get_object_class(), args.size());

    for (size_t i = 0; i < args.size(); i++) {
        MonoObject* arg = BoxNativeCallValue(backend, args[i], ptr<void>(call.ArgsData[i]).get(), call.Accessor.get());
        mono_array_setref(args_array, i, arg);
    }

    // Resolve FOnline.Native via the backend's loaded images, not the handler's class image: a handler may be a
    // System delegate (e.g. a reflection-built Func<>/Action<> registered as a global script func), whose image is
    // corelib and does not contain FOnline.Native. (Custom FOnline delegate handlers would resolve either way.)
    auto* native_class = FindFOnlineClass(backend, "Native");

    if (native_class == nullptr) {
        throw ScriptSystemException("Managed Native class not found");
    }

    auto* invoke_callback = mono_class_get_method_from_name(native_class, "InvokeCallback", 2);

    if (invoke_callback == nullptr) {
        throw ScriptSystemException("Managed Native.InvokeCallback method not found");
    }

    void* invoke_args[] = {handler, args_array};
    MonoObject* exception = nullptr;
    MonoObject* result = mono_runtime_invoke(invoke_callback, nullptr, invoke_args, &exception);
    ThrowIfManagedException(exception, "Managed callback failed");

    if (ret) {
        CopyManagedCallbackReturnValue(backend, ret, result, call);
    }
}

static void CopyManagedCallbackReturnValue(ptr<ManagedScriptBackend> backend, const ComplexTypeDesc& type, MonoObject* value, FuncCallData& call)
{
    FO_STACK_TRACE_ENTRY();

    if (!type) {
        return;
    }

    ptr<void> ret_data = call.RetData.as_ptr();
    ptr<const DataAccessor> accessor = call.Accessor;

    if (type.Kind == ComplexTypeKind::Array) {
        // Rebuild the caller's array from the managed List return (the reverse of BoxNativeCallValue's Array read).
        // Uses the accessor's ClearArray/AddArrayElement write API, so collection-returning [ScriptCallable] bridges
        // (e.g. Purchases::SelectUningestedWebOrdersManaged -> hstring[]) work the same as an AngelScript array return.
        accessor->ClearArray(ret_data);

        const size_t array_size = value != nullptr ? GetManagedListCount(backend, value) : 0;

        for (size_t i = 0; i < array_size; i++) {
            MonoObject* element = GetManagedListItem(backend, value, i);
            ManagedScalarValue element_storage;
            void* element_value = ConvertManagedSimpleObjectToNative(backend, type.BaseType, element, element_storage);
            accessor->AddArrayElement(ret_data, element_value);
        }

        return;
    }

    if (type.Kind != ComplexTypeKind::Simple) {
        throw ScriptSystemException("Managed callback dictionary returns are not supported yet");
    }

    const BaseTypeDesc& base_type = type.BaseType;
    ManagedScalarValue storage;
    void* native_value = ConvertManagedSimpleObjectToNative(backend, base_type, value, storage);

    if (base_type.IsString) {
        *ret_data.reinterpret_as<string>() = *static_cast<string*>(native_value);
    }
    else if (base_type.IsHashedString) {
        *ret_data.reinterpret_as<hstring>() = *static_cast<hstring*>(native_value);
    }
    else if (base_type.IsEntity || base_type.IsFixedType || base_type.IsEntityProto) {
        auto* entity = *static_cast<Entity**>(native_value);

        if (accessor->GetBackendIndex() == ScriptSystemBackend::ANGELSCRIPT_BACKEND_INDEX && entity != nullptr) {
            entity->AddRef();
        }

        *ret_data.reinterpret_as<Entity*>() = entity;
    }
    else if (base_type.IsRefType) {
        if (IsDynamicManagedRefType(base_type)) {
            nptr<DynamicRefTypeInstance> ref_instance = *static_cast<DynamicRefTypeInstance**>(native_value);
            NativeDataProvider::WriteTypedHandleSlot(ret_data, ref_instance);

            if (ref_instance) {
                if (accessor->GetBackendIndex() == ScriptSystemBackend::MANAGED_BACKEND_INDEX) {
                    refcount_nptr<DynamicRefTypeInstance> return_owner = std::move(storage.DynamicRefType);
                    call.RetValueOwner.Reset([return_owner = std::move(return_owner)]() mutable noexcept { return_owner = nullptr; });
                }
                else {
                    (void)storage.DynamicRefType.release_ownership();
                }
            }
        }
        else {
            *NativeDataProvider::GetHandleSlot(ret_data) = *static_cast<void**>(native_value);
        }
    }
    else if (base_type.IsPrimitive || base_type.IsEnum || base_type.IsStruct) {
        MemCopy(ret_data, native_value, base_type.Size);
    }
    else {
        throw ScriptSystemException("Unsupported Managed callback return type", base_type.Name);
    }
}

static auto CreateManagedCallbackDesc(const ManagedCallbackBridgeData* callback) -> unique_del_nptr<ScriptFuncDesc>
{
    FO_STACK_TRACE_ENTRY();

    if (callback == nullptr || callback->Handler == 0) {
        return nullptr;
    }

    auto* handler = mono_gchandle_get_target(callback->Handler);

    if (handler == nullptr) {
        return nullptr;
    }
    if (callback->Type.Kind != ComplexTypeKind::Callback || !callback->Type.CallbackArgs || callback->Type.CallbackArgs->empty()) {
        throw ScriptSystemException("Invalid Managed callback type");
    }

    const uint32_t handler_handle = mono_gchandle_new(handler, false);
    ComplexTypeDesc ret = callback->Type.CallbackArgs->front();
    vector<ComplexTypeDesc> args;

    for (const ComplexTypeDesc& arg_type : span(*callback->Type.CallbackArgs).subspan(1)) {
        args.emplace_back(arg_type);
    }

    auto func_desc = SafeAlloc::MakeUnique<ScriptFuncDesc>();
    func_desc->Name = callback->Name;
    func_desc->Ret = ret;
    func_desc->Args.reserve(args.size());

    for (const ComplexTypeDesc& arg_type : args) {
        func_desc->Args.emplace_back(ArgDesc {.Name = {}, .Type = arg_type});
    }

    auto* func_desc_ptr = func_desc.get();
    func_desc->Call = [backend = ptr<ManagedScriptBackend> {callback->Backend.get_no_const()}, handler_handle, func_desc_ptr](FuncCallData& call) {
        vector<ComplexTypeDesc> call_args;
        call_args.reserve(func_desc_ptr->Args.size());

        for (const ArgDesc& arg : func_desc_ptr->Args) {
            call_args.emplace_back(arg.Type);
        }

        DispatchManagedCallback(backend, handler_handle, func_desc_ptr->Ret, call_args, call);
    };
    func_desc->AttributeChecker = [](string_view /*attribute*/) -> bool { return true; };

    return make_unique_del_ptr(std::move(func_desc).release(), [handler_handle](ScriptFuncDesc* desc) {
        if (handler_handle != 0) {
            mono_gchandle_free(handler_handle);
        }

        delete desc;
    });
}

static auto BoxNativeCallValue(ptr<const ManagedScriptBackend> backend, const ComplexTypeDesc& type, void* data, const DataAccessor* accessor) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    if (!type) {
        return nullptr;
    }

    if (type.Kind == ComplexTypeKind::Simple) {
        return BoxNativeSimpleValue(backend, type.BaseType, data);
    }
    if (type.Kind == ComplexTypeKind::Array) {
        auto* list = CreateManagedList(backend, type.BaseType);
        const size_t size = accessor->GetArraySize(data);

        for (size_t i = 0; i < size; i++) {
            auto* item = BoxNativeSimpleValue(backend, type.BaseType, accessor->GetArrayElement(data, i).get());
            AddManagedListItem(backend, list, item);
        }

        return list;
    }
    if (type.Kind == ComplexTypeKind::Dict) {
        FO_VERIFY_AND_THROW(type.KeyType, "Dictionary type has no key type");
        auto* dictionary = CreateManagedDictionary(backend, *type.KeyType, type.BaseType);
        const size_t size = accessor->GetDictSize(data);

        for (size_t i = 0; i < size; i++) {
            auto [key, value] = accessor->GetDictElement(data, i);
            auto* managed_key = BoxNativeSimpleValue(backend, *type.KeyType, key.get());
            auto* managed_value = BoxNativeSimpleValue(backend, type.BaseType, value.get());
            AddManagedDictionaryItem(backend, dictionary, managed_key, managed_value);
        }

        return dictionary;
    }

    throw ScriptSystemException("Unsupported Managed return type", type.BaseType.Name);
}

// === Event dispatch bridge ===

// Writes a managed event argument that a [Event] handler mutated through a `ref` parameter back into
// the native inout slot the engine fired the event with. Only simple value/string/hstring/entity
// arguments roundtrip; ref-type, array, and dictionary inout arguments are rejected loudly rather
// than silently dropping the write.
static void WriteBackManagedEventArg(ptr<ManagedScriptBackend> backend, const ComplexTypeDesc& type, MonoObject* value, void* dst)
{
    FO_STACK_TRACE_ENTRY();

    if (type.Kind != ComplexTypeKind::Simple) {
        throw ScriptSystemException("Managed mutable event argument type is not supported", type.BaseType.Name);
    }

    const BaseTypeDesc& base_type = type.BaseType;
    ManagedScalarValue storage;
    void* converted = ConvertManagedSimpleObjectToNative(backend, base_type, value, storage);

    if (base_type.IsString) {
        *static_cast<string*>(dst) = *static_cast<string*>(converted);
    }
    else if (base_type.IsHashedString) {
        *static_cast<hstring*>(dst) = *static_cast<hstring*>(converted);
    }
    else if (base_type.IsEntity) {
        *static_cast<Entity**>(dst) = *static_cast<Entity**>(converted);
    }
    else if (base_type.IsPrimitive || base_type.IsEnum || base_type.IsStruct) {
        MemCopy(dst, converted, base_type.Size);
    }
    else {
        throw ScriptSystemException("Managed mutable event argument type is not supported", base_type.Name);
    }
}

static auto DispatchManagedEvent(const std::shared_ptr<ManagedEventSubscription>& subscription, FuncCallData& call) -> Entity::EventResult
{
    FO_STACK_TRACE_ENTRY();

    auto* engine = dynamic_cast<BaseEngine*>(subscription->Backend->GetMetadata());
    FO_VERIFY_AND_THROW(engine, "Managed event dispatch requires an engine context");

    Entity::EventResult result = Entity::EventResult::ContinueChain;
    engine->RunScriptContext([&] { result = DispatchManagedEventInContext(subscription, call); });
    return result;
}

static auto DispatchManagedEventInContext(const std::shared_ptr<ManagedEventSubscription>& subscription, FuncCallData& call) -> Entity::EventResult
{
    FO_STACK_TRACE_ENTRY();

    const ActiveBackendScope active_backend {subscription->Backend.as_ptr()};

    auto* domain = GetDomainOrThrow(subscription->Backend->GetDomain());

    if (mono_thread_attach(domain) == nullptr) {
        throw ScriptSystemException("Failed to attach native thread to Managed runtime domain");
    }

    if (mono_gchandle_get_target(subscription->Handler) == nullptr) {
        return Entity::EventResult::ContinueChain;
    }

    if (call.ArgsData.size() != subscription->Args.size()) {
        throw ScriptSystemException("Managed event argument count mismatch");
    }

    auto* native_class = mono_class_from_name(subscription->Image, "FOnline", "Native");

    if (native_class == nullptr) {
        throw ScriptSystemException("Managed Native class not found");
    }

    auto* invoke_event = mono_class_get_method_from_name(native_class, "InvokeEvent", 3);

    if (invoke_event == nullptr) {
        throw ScriptSystemException("Managed Native.InvokeEvent method not found");
    }

    // Root the boxed argument array so it survives the GC that BoxNativeCallValue and the managed
    // invoke can trigger, and re-fetch it on each use; DynamicInvoke writes mutated `ref` arguments
    // back into this array, which we then propagate to the native inout slots below.
    const uint32_t args_array_handle = mono_gchandle_new(reinterpret_cast<MonoObject*>(mono_array_new(domain, mono_get_object_class(), subscription->Args.size())), 0);
    auto free_args_array_handle = scope_exit([args_array_handle]() noexcept { mono_gchandle_free(args_array_handle); });
    const auto get_args_array = [args_array_handle]() -> MonoArray* { return reinterpret_cast<MonoArray*>(mono_gchandle_get_target(args_array_handle)); };

    for (size_t i = 0; i < subscription->Args.size(); i++) {
        MonoObject* arg = BoxNativeCallValue(subscription->Backend.as_ptr(), subscription->Args[i], ptr<void>(call.ArgsData[i]).get(), call.Accessor.get());
        mono_array_setref(get_args_array(), i, arg);
    }

    mono_bool has_result = subscription->HasExplicitResult ? 1 : 0;
    void* args[] = {mono_gchandle_get_target(subscription->Handler), &has_result, get_args_array()};
    MonoObject* exception = nullptr;
    MonoObject* ret = mono_runtime_invoke(invoke_event, nullptr, args, &exception);
    ThrowIfManagedException(exception, "Managed event handler failed");

    for (size_t i = 0; i < subscription->Args.size(); i++) {
        if (subscription->Args[i].IsMutable) {
            auto* mutated = mono_array_get(get_args_array(), MonoObject*, i);
            WriteBackManagedEventArg(subscription->Backend.as_ptr(), subscription->Args[i], mutated, ptr<void>(call.ArgsData[i]).get());
        }
    }

    if (ret == nullptr) {
        return Entity::EventResult::ContinueChain;
    }

    const auto result = *static_cast<int32_t*>(mono_object_unbox(ret));
    return static_cast<Entity::EventResult>(result);
}

// === Remote-call argument marshaling ===

static auto SerializeManagedRemoteCallArgs(ptr<ManagedScriptBackend> backend, const vector<ArgDesc>& call_args, MonoArray* args_array, string_view name) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    // Serialize boxed managed args into the shared RemoteCallWire byte format (the same format the AngelScript
    // backend reads/writes). Scalar args only for now (collections/ref types come later).
    const size_t arg_count = args_array != nullptr ? mono_array_length(args_array) : 0;

    if (arg_count != call_args.size()) {
        throw ScriptSystemException("Managed remote call argument count mismatch", name);
    }

    vector<uint8_t> data;
    DataWriter writer(data);
    const RemoteCallWireHooks hooks {
        .RefTypeToRaw = [](const BaseTypeDesc& type, ptr<void> arg) -> vector<uint8_t> {
            // Two levels of indirection: arg is `&storage.RefTypePtr` — the address of the slot, always valid,
            // hence ptr. The pointer *stored in* that slot (the DynamicRefTypeInstance for a managed dynamic ref
            // type) may be null, so it is read as an nptr. If non-null, serialize its fields via the shared method.
            nptr<void> ref = *arg.reinterpret_as<nptr<void>>();

            if (!ref) {
                return vector<uint8_t> {};
            }

            const span<const uint8_t> serialized = ref.reinterpret_as<DynamicRefTypeInstance>()->GetSerializedRawData(type);
            return vector<uint8_t>(serialized.begin(), serialized.end());
        },
    };

    for (size_t i = 0; i < call_args.size(); i++) {
        const auto& arg = call_args[i];
        auto* arg_obj = mono_array_get(args_array, MonoObject*, i);

        if (arg.Type.BaseType.IsRefType && !IsDynamicManagedRefType(arg.Type.BaseType)) {
            throw ScriptSystemException("Managed remote call supports only dynamic ref-type args", string(name), arg.Type.BaseType.Name);
        }

        if (arg.Type.Kind == ComplexTypeKind::Simple) {
            ManagedScalarValue storage;
            void* native = ConvertManagedSimpleObjectToNative(backend, arg.Type.BaseType, arg_obj, storage);
            WriteRemoteCallSimple(writer, native, arg.Type.BaseType, hooks);
        }
        else if (arg.Type.Kind == ComplexTypeKind::Array) {
            // Wire: int32 count, then each element (shared scalar format) — matches AngelScript's array framing.
            const size_t count = arg_obj != nullptr ? GetManagedListCount(backend, arg_obj) : 0;
            writer.Write<int32_t>(numeric_cast<int32_t>(count));

            for (size_t j = 0; j < count; j++) {
                MonoObject* item = GetManagedListItem(backend, arg_obj, j);
                ManagedScalarValue elem_storage;
                void* native = ConvertManagedSimpleObjectToNative(backend, arg.Type.BaseType, item, elem_storage);
                WriteRemoteCallSimple(writer, native, arg.Type.BaseType, hooks);
            }
        }
        else {
            throw ScriptSystemException("Managed remote call collection kind not supported yet", string(name));
        }
    }

    return data;
}

static void AppendRawBytes(vector<uint8_t>& data, const void* value, size_t size)
{
    FO_STACK_TRACE_ENTRY();

    if (size == 0) {
        return;
    }

    const size_t old_size = data.size();
    data.resize(old_size + size);
    MemCopy(data.data() + old_size, value, size);
}

static void AppendAlignedRawBytes(vector<uint8_t>& data, const void* value, size_t size, size_t alignment)
{
    FO_STACK_TRACE_ENTRY();

    if (size != 0) {
        data.resize(align_up(data.size(), alignment));
    }

    AppendRawBytes(data, value, size);
}

// === Managed object creation and native<->managed values ===

static auto CreateHashObject(ptr<const ManagedScriptBackend> backend, hstring::hash_t value) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    auto* domain = GetDomainOrThrow(backend->GetDomain());
    auto* hash_class = FindFOnlineClass(backend, "hstring");
    return mono_value_box(domain, hash_class, &value);
}

static auto CreateEntityObject(ptr<const ManagedScriptBackend> backend, string_view type_name, nptr<Entity> entity) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    if (entity == nullptr) {
        return nullptr;
    }

    auto* domain = GetDomainOrThrow(backend->GetDomain());
    auto* entity_class = FindFOnlineClass(backend, type_name);
    auto* obj = mono_object_new(domain, entity_class);

    if (obj == nullptr) {
        throw ScriptSystemException("Can't create Managed entity wrapper", type_name);
    }

    auto* ctor = mono_class_get_method_from_name(entity_class, ".ctor", 1);

    if (ctor == nullptr) {
        throw ScriptSystemException("Managed entity wrapper constructor not found", type_name);
    }

    void* entity_ptr = entity.get();
    void* args[] = {&entity_ptr};
    MonoObject* exception = nullptr;
    mono_runtime_invoke(ctor, obj, args, &exception);
    ThrowIfManagedException(exception, "Managed entity wrapper constructor failed");
    return obj;
}

static auto CreatePropertyEnumObject(ptr<const ManagedScriptBackend> backend, string_view owner_type_name, ptr<const Property> prop) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    auto* domain = GetDomainOrThrow(backend->GetDomain());
    const string property_enum_name = strex("{}Property", owner_type_name).str();
    auto* enum_class = FindFOnlineClass(backend, property_enum_name);
    int32_t value = prop->GetRegIndex();
    return mono_value_box(domain, enum_class, &value);
}

static void InvokeManagedConstructor(MonoClass* klass, MonoObject* obj, int32_t args_count, void** args, string_view context)
{
    FO_STACK_TRACE_ENTRY();

    auto* ctor = mono_class_get_method_from_name(klass, ".ctor", args_count);

    if (ctor == nullptr) {
        throw ScriptSystemException("Managed constructor not found", context, args_count);
    }

    MonoObject* exception = nullptr;
    mono_runtime_invoke(ctor, obj, args, &exception);
    ThrowIfManagedException(exception, strex("{} constructor failed", context).str());
}

static auto CreateNativeRefTypeObject(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, void* ref_ptr) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    if (ref_ptr == nullptr) {
        return nullptr;
    }

    auto* domain = GetDomainOrThrow(backend->GetDomain());
    auto* klass = FindFOnlineClass(backend, base_type.Name);
    auto* obj = mono_object_new(domain, klass);

    if (obj == nullptr) {
        throw ScriptSystemException("Can't create Managed ref type wrapper", base_type.Name);
    }

    void* args[] = {&ref_ptr};
    InvokeManagedConstructor(klass, obj, 1, args, base_type.Name);
    return obj;
}

static auto CreateDynamicRefTypeObject(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, span<const uint8_t> raw_data) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(IsDynamicManagedRefType(base_type), "Base type is not a dynamic managed ref type");

    auto* domain = GetDomainOrThrow(backend->GetDomain());
    auto* klass = FindFOnlineClass(backend, base_type.Name);
    auto* obj = mono_object_new(domain, klass);

    if (obj == nullptr) {
        throw ScriptSystemException("Can't create Managed dynamic ref type", base_type.Name);
    }

    InvokeManagedConstructor(klass, obj, 0, nullptr, base_type.Name);

    const auto* fields_registrator = base_type.RefType->FieldsRegistrator.get();
    size_t data_pos = 0;

    for (size_t i = 1; i < fields_registrator->GetPropertiesCount(); i++) {
        auto field_prop = fields_registrator->GetPropertyByIndexUnsafe(i);
        span<const uint8_t> field_raw_data {};

        if (data_pos < raw_data.size()) {
            data_pos = align_up(data_pos, sizeof(uint32_t));

            if (data_pos > raw_data.size() || raw_data.size() - data_pos < sizeof(uint32_t)) {
                throw ScriptSystemException("Corrupted Managed dynamic ref type data", base_type.Name, field_prop->GetName());
            }

            uint32_t field_size;
            MemCopy(&field_size, raw_data.data() + data_pos, sizeof(field_size));
            data_pos += sizeof(field_size);

            if (field_prop->IsPlainData() && field_size != 0 && field_size != field_prop->GetBaseSize()) {
                throw ScriptSystemException("Wrong Managed dynamic ref type field raw size", base_type.Name, field_prop->GetName());
            }
            if (field_size != 0) {
                data_pos = align_up(data_pos, field_prop->GetDataAlignment());
            }
            if (data_pos > raw_data.size() || raw_data.size() - data_pos < field_size) {
                throw ScriptSystemException("Corrupted Managed dynamic ref type field data", base_type.Name, field_prop->GetName());
            }

            field_raw_data = {raw_data.data() + data_pos, field_size};
            data_pos += field_size;
        }

        if (!field_raw_data.empty()) {
            MonoObject* field_value = BoxPropertyValue(backend, field_prop.get(), field_raw_data);
            SetManagedPropertyValue(backend, obj, field_prop->GetNameWithoutComponent(), field_value);
        }
    }

    if (data_pos != raw_data.size()) {
        throw ScriptSystemException("Corrupted Managed dynamic ref type data", base_type.Name);
    }

    return obj;
}

static auto CreateRefTypeObject(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, void* ref_ptr) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(base_type.IsRefType, "Base type is not a ref type");

    if (IsDynamicManagedRefType(base_type)) {
        if (ref_ptr == nullptr) {
            return nullptr;
        }

        auto* ref_instance = static_cast<DynamicRefTypeInstance*>(ref_ptr);
        const span<const uint8_t> raw_data = ref_instance->GetSerializedRawData(base_type);
        return CreateDynamicRefTypeObject(backend, base_type, raw_data);
    }

    return CreateNativeRefTypeObject(backend, base_type, ref_ptr);
}

static auto CreateDynamicRefTypeFromManaged(ptr<ManagedScriptBackend> backend, const BaseTypeDesc& base_type, MonoObject* value) -> refcount_nptr<DynamicRefTypeInstance>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(IsDynamicManagedRefType(base_type), "Base type is not a dynamic managed ref type");

    if (value == nullptr) {
        return {};
    }

    auto ref_instance = SafeAlloc::MakeRefCounted<DynamicRefTypeInstance>(base_type.RefType->FieldsRegistrator.get());
    const auto* fields_registrator = base_type.RefType->FieldsRegistrator.get();

    for (size_t i = 1; i < fields_registrator->GetPropertiesCount(); i++) {
        auto field_prop = fields_registrator->GetPropertyByIndexUnsafe(i);
        MonoObject* field_value = GetManagedPropertyValue(backend, value, field_prop->GetNameWithoutComponent());
        PropertyRawData field_data = ConvertManagedObjectToPropertyData(backend, field_prop.get(), field_value);
        ref_instance->SetValue(field_prop, field_data);
    }

    return ref_instance;
}

static void CopyManagedStructToNative(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, MonoObject* value, void* data)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(base_type.IsStruct, "Base type is not a struct");
    FO_VERIFY_AND_THROW(base_type.StructLayout != nullptr, "Struct layout is missing");

    auto* domain = GetDomainOrThrow(backend->GetDomain());
    auto* klass = FindFOnlineClass(backend, base_type.Name);
    auto* raw_data = static_cast<uint8_t*>(data);

    for (const FieldDesc& field_desc : base_type.StructLayout->Fields) {
        auto* field = mono_class_get_field_from_name(klass, field_desc.Name.c_str());

        if (field == nullptr) {
            throw ScriptSystemException("Managed struct field not found", base_type.Name, field_desc.Name);
        }

        if (field_desc.Type.IsHashedString) {
            hstring::hash_t hash {};
            mono_field_get_value(value, field, &hash);

            const hstring resolved_hash = ResolveManagedHashValue(backend, hash);
            MemCopy(raw_data + field_desc.Offset, &resolved_hash, sizeof(resolved_hash));
        }
        else if (field_desc.Type.IsStruct && field_desc.Type.StructLayout != nullptr) {
            MonoObject* field_value = mono_field_get_value_object(domain, field, value);

            if (field_value == nullptr) {
                throw ScriptSystemException("Managed struct field read failed", base_type.Name, field_desc.Name);
            }

            CopyManagedStructToNative(backend, field_desc.Type, field_value, raw_data + field_desc.Offset);
        }
        else {
            mono_field_get_value(value, field, raw_data + field_desc.Offset);
        }
    }
}

static void CopyManagedStructToPropertyData(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, MonoObject* value, void* data)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(base_type.IsStruct, "Base type is not a struct");
    FO_VERIFY_AND_THROW(base_type.StructLayout != nullptr, "Struct layout is missing");

    auto* domain = GetDomainOrThrow(backend->GetDomain());
    auto* klass = FindFOnlineClass(backend, base_type.Name);
    auto* raw_data = static_cast<uint8_t*>(data);

    for (const FieldDesc& field_desc : base_type.StructLayout->Fields) {
        auto* field = mono_class_get_field_from_name(klass, field_desc.Name.c_str());

        if (field == nullptr) {
            throw ScriptSystemException("Managed struct field not found", base_type.Name, field_desc.Name);
        }

        if (field_desc.Type.IsHashedString) {
            hstring::hash_t hash {};
            mono_field_get_value(value, field, &hash);

            const hstring resolved_hash = ResolveManagedHashValue(backend, hash);
            hash = resolved_hash.as_hash();
            MemCopy(raw_data + field_desc.Offset, &hash, sizeof(hash));
        }
        else if (field_desc.Type.IsStruct && field_desc.Type.StructLayout != nullptr) {
            MonoObject* field_value = mono_field_get_value_object(domain, field, value);

            if (field_value == nullptr) {
                throw ScriptSystemException("Managed struct field read failed", base_type.Name, field_desc.Name);
            }

            CopyManagedStructToPropertyData(backend, field_desc.Type, field_value, raw_data + field_desc.Offset);
        }
        else {
            mono_field_get_value(value, field, raw_data + field_desc.Offset);
        }
    }
}

static auto CreateStructObject(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, void* data) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(base_type.IsStruct, "Base type is not a struct");
    FO_VERIFY_AND_THROW(base_type.StructLayout != nullptr, "Struct layout is missing");

    auto* domain = GetDomainOrThrow(backend->GetDomain());
    auto* klass = FindFOnlineClass(backend, base_type.Name);
    auto* obj = mono_object_new(domain, klass);

    if (obj == nullptr) {
        throw ScriptSystemException("Can't create Managed struct", base_type.Name);
    }

    const auto* raw_data = static_cast<const uint8_t*>(data);

    for (const FieldDesc& field_desc : base_type.StructLayout->Fields) {
        auto* field = mono_class_get_field_from_name(klass, field_desc.Name.c_str());

        if (field == nullptr) {
            throw ScriptSystemException("Managed struct field not found", base_type.Name, field_desc.Name);
        }

        if (field_desc.Type.IsHashedString) {
            const hstring& hash = *reinterpret_cast<const hstring*>(raw_data + field_desc.Offset);
            hstring::hash_t managed_hash = MakeManagedHashValue(backend, hash);
            mono_field_set_value(obj, field, &managed_hash);
        }
        else if (field_desc.Type.IsStruct && field_desc.Type.StructLayout != nullptr) {
            MonoObject* field_value = CreateStructObject(backend, field_desc.Type, const_cast<uint8_t*>(raw_data + field_desc.Offset));

            if (field_value == nullptr) {
                throw ScriptSystemException("Managed struct field create failed", base_type.Name, field_desc.Name);
            }

            mono_field_set_value(obj, field, mono_object_unbox(field_value));
        }
        else {
            mono_field_set_value(obj, field, const_cast<uint8_t*>(raw_data + field_desc.Offset));
        }
    }

    return obj;
}

static auto CreatePropertyStructObject(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, span<const uint8_t> raw_data) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(base_type.IsStruct, "Base type is not a struct");
    FO_VERIFY_AND_THROW(base_type.StructLayout != nullptr, "Struct layout is missing");
    FO_VERIFY_AND_THROW(raw_data.size() == base_type.Size, "Raw property struct size does not match the value type size", base_type.Name, raw_data.size(), base_type.Size);

    auto* domain = GetDomainOrThrow(backend->GetDomain());
    auto* klass = FindFOnlineClass(backend, base_type.Name);
    auto* obj = mono_object_new(domain, klass);

    if (obj == nullptr) {
        throw ScriptSystemException("Can't create Managed property struct", base_type.Name);
    }

    const uint8_t* data = raw_data.data();

    for (const FieldDesc& field_desc : base_type.StructLayout->Fields) {
        auto* field = mono_class_get_field_from_name(klass, field_desc.Name.c_str());

        if (field == nullptr) {
            throw ScriptSystemException("Managed struct field not found", base_type.Name, field_desc.Name);
        }

        if (field_desc.Type.IsHashedString) {
            hstring::hash_t managed_hash {};
            MemCopy(&managed_hash, data + field_desc.Offset, sizeof(managed_hash));
            mono_field_set_value(obj, field, &managed_hash);
        }
        else if (field_desc.Type.IsStruct && field_desc.Type.StructLayout != nullptr) {
            MonoObject* field_value = CreatePropertyStructObject(backend, field_desc.Type, {data + field_desc.Offset, field_desc.Type.Size});

            if (field_value == nullptr) {
                throw ScriptSystemException("Managed property struct field create failed", base_type.Name, field_desc.Name);
            }

            mono_field_set_value(obj, field, mono_object_unbox(field_value));
        }
        else {
            mono_field_set_value(obj, field, const_cast<uint8_t*>(data + field_desc.Offset));
        }
    }

    return obj;
}

static auto GetManagedPropertyValue(ptr<const ManagedScriptBackend> backend, MonoObject* obj, string_view property_name) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(backend);

    if (obj == nullptr) {
        return nullptr;
    }

    const string property_name_str {property_name};
    auto* klass = mono_object_get_class(obj);
    auto* prop = mono_class_get_property_from_name(klass, property_name_str.c_str());

    if (prop == nullptr) {
        throw ScriptSystemException("Managed property not found", property_name);
    }

    auto* getter = mono_property_get_get_method(prop);

    if (getter == nullptr) {
        throw ScriptSystemException("Managed property getter not found", property_name);
    }

    MonoObject* exception = nullptr;
    MonoObject* result = mono_runtime_invoke(getter, obj, nullptr, &exception);
    ThrowIfManagedException(exception, strex("Managed property getter failed: {}", property_name).str());
    return result;
}

static void SetManagedPropertyValue(ptr<const ManagedScriptBackend> backend, MonoObject* obj, string_view property_name, MonoObject* value)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(backend);

    const string property_name_str {property_name};
    auto* klass = mono_object_get_class(obj);
    auto* prop = mono_class_get_property_from_name(klass, property_name_str.c_str());

    if (prop == nullptr) {
        throw ScriptSystemException("Managed property not found", property_name);
    }

    auto* setter = mono_property_get_set_method(prop);

    if (setter == nullptr) {
        throw ScriptSystemException("Managed property setter not found", property_name);
    }

    // mono_runtime_invoke wants the unboxed value pointer for a value-type parameter (int/enum/bool/struct) but the
    // object itself for a reference type (string/object/entity). Passing a boxed value type straight through makes
    // the setter read the object header as the value (garbage), so unbox value types first.
    void* arg = value != nullptr && (mono_class_is_valuetype(mono_object_get_class(value)) != 0) ? mono_object_unbox(value) : value;
    void* args[] = {arg};
    MonoObject* exception = nullptr;
    mono_runtime_invoke(setter, obj, args, &exception);
    ThrowIfManagedException(exception, strex("Managed property setter failed: {}", property_name).str());
}

// === Managed collections (list/dictionary/delegate) ===

static auto CreateManagedList(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& element_type) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    auto* domain = GetDomainOrThrow(backend->GetDomain());
    auto* element_class = GetManagedClass(backend, element_type);
    auto* reflection_type = mono_type_get_object(domain, mono_class_get_type(element_class));
    void* args[] = {reflection_type};
    auto* list = InvokeNativeHelper(backend, "CreateList", 1, args);

    if (list == nullptr) {
        throw ScriptSystemException("Managed list creation failed", element_type.Name);
    }

    return list;
}

static auto GetManagedListCount(ptr<const ManagedScriptBackend> backend, MonoObject* list) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    if (list == nullptr) {
        return 0;
    }

    void* args[] = {list};
    auto* result = InvokeNativeHelper(backend, "GetListCount", 1, args);

    if (result == nullptr) {
        throw ScriptSystemException("Managed list count failed");
    }

    const int32_t count = *static_cast<int32_t*>(mono_object_unbox(result));
    FO_VERIFY_AND_THROW(count >= 0, "Managed list count is negative");
    return numeric_cast<size_t>(count);
}

static auto GetManagedListItem(ptr<const ManagedScriptBackend> backend, MonoObject* list, size_t index) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    int32_t index_value = numeric_cast<int32_t>(index);
    void* args[] = {list, &index_value};
    return InvokeNativeHelper(backend, "GetListItem", 2, args);
}

static void AddManagedListItem(ptr<const ManagedScriptBackend> backend, MonoObject* list, MonoObject* item)
{
    FO_STACK_TRACE_ENTRY();

    void* args[] = {list, item};
    (void)InvokeNativeHelper(backend, "AddListItem", 2, args);
}

static auto CreateManagedDictionary(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& key_type, const BaseTypeDesc& value_type) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    auto* domain = GetDomainOrThrow(backend->GetDomain());
    auto* key_class = GetManagedClass(backend, key_type);
    auto* value_class = GetManagedClass(backend, value_type);
    auto* key_reflection_type = mono_type_get_object(domain, mono_class_get_type(key_class));
    auto* value_reflection_type = mono_type_get_object(domain, mono_class_get_type(value_class));
    void* args[] = {key_reflection_type, value_reflection_type};
    auto* dictionary = InvokeNativeHelper(backend, "CreateDictionary", 2, args);

    if (dictionary == nullptr) {
        throw ScriptSystemException("Managed dictionary creation failed", key_type.Name, value_type.Name);
    }

    return dictionary;
}

static auto GetManagedDictionaryCount(ptr<const ManagedScriptBackend> backend, MonoObject* dictionary) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    if (dictionary == nullptr) {
        return 0;
    }

    void* args[] = {dictionary};
    auto* result = InvokeNativeHelper(backend, "GetDictionaryCount", 1, args);

    if (result == nullptr) {
        throw ScriptSystemException("Managed dictionary count failed");
    }

    const int32_t count = *static_cast<int32_t*>(mono_object_unbox(result));
    FO_VERIFY_AND_THROW(count >= 0, "Managed dictionary count is negative");
    return numeric_cast<size_t>(count);
}

static auto GetManagedDictionaryKey(ptr<const ManagedScriptBackend> backend, MonoObject* dictionary, size_t index) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    int32_t index_value = numeric_cast<int32_t>(index);
    void* args[] = {dictionary, &index_value};
    return InvokeNativeHelper(backend, "GetDictionaryKey", 2, args);
}

static auto GetManagedDictionaryValue(ptr<const ManagedScriptBackend> backend, MonoObject* dictionary, size_t index) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    int32_t index_value = numeric_cast<int32_t>(index);
    void* args[] = {dictionary, &index_value};
    return InvokeNativeHelper(backend, "GetDictionaryValue", 2, args);
}

static void AddManagedDictionaryItem(ptr<const ManagedScriptBackend> backend, MonoObject* dictionary, MonoObject* key, MonoObject* value)
{
    FO_STACK_TRACE_ENTRY();

    void* args[] = {dictionary, key, value};
    (void)InvokeNativeHelper(backend, "AddDictionaryItem", 3, args);
}

static auto GetManagedDelegateKey(ptr<const ManagedScriptBackend> backend, MonoObject* handler) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (handler == nullptr) {
        return {};
    }

    void* args[] = {handler};
    auto* result = InvokeNativeHelper(backend, "GetDelegateKey", 1, args);
    return result != nullptr ? ToStringAndFree(reinterpret_cast<MonoString*>(result)) : string {};
}

// === Managed<->native conversion and boxing ===

static auto GetManagedClass(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& type) -> MonoClass*
{
    FO_STACK_TRACE_ENTRY();

    if (type.Name == "any") {
        return mono_get_string_class();
    }
    if (type.IsString) {
        return mono_get_string_class();
    }
    if (type.IsEntity) {
        return FindFOnlineClass(backend, type.Name);
    }
    if (type.IsRefType) {
        return FindFOnlineClass(backend, type.Name);
    }

    return GetValueClass(backend, type);
}

static auto InvokeNativeHelper(ptr<const ManagedScriptBackend> backend, const char* method_name, uint32_t args_count, void** args) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    auto* native_class = FindFOnlineClass(backend, "Native");
    auto* method = mono_class_get_method_from_name(native_class, method_name, numeric_cast<int>(args_count));

    if (method == nullptr) {
        throw ScriptSystemException("Managed Native helper method not found", method_name);
    }

    MonoObject* exception = nullptr;
    MonoObject* result = mono_runtime_invoke(method, nullptr, args, &exception);
    ThrowIfManagedException(exception, strex("Managed Native.{} failed", method_name).str());
    return result;
}

static auto InvokeNativeBoolHelper(ptr<const ManagedScriptBackend> backend, const char* method_name, MonoObject* value) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (value == nullptr) {
        return false;
    }

    void* args[] = {value};
    auto* result = InvokeNativeHelper(backend, method_name, 1, args);

    if (result == nullptr) {
        throw ScriptSystemException("Managed Native bool helper returned null", method_name);
    }

    return *static_cast<mono_bool*>(mono_object_unbox(result)) != 0;
}

static auto IsManagedByteArray(MonoObject* value) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (value == nullptr) {
        return false;
    }

    auto* klass = mono_object_get_class(value);
    return mono_class_get_rank(klass) == 1 && mono_class_get_element_class(klass) == mono_get_byte_class();
}

static auto GetManagedByteArrayItem(ptr<const ManagedScriptBackend> backend, MonoObject* value, size_t index) -> uint8_t
{
    FO_STACK_TRACE_ENTRY();

    int32_t index_value = numeric_cast<int32_t>(index);
    void* args[] = {value, &index_value};
    auto* result = InvokeNativeHelper(backend, "GetByteArrayItem", 2, args);

    if (result == nullptr) {
        throw ScriptSystemException("Managed byte array item read failed");
    }

    const int32_t item = *static_cast<int32_t*>(mono_object_unbox(result));
    FO_VERIFY_AND_THROW(item >= 0 && item <= UINT8_MAX, "Managed byte array item is out of byte range");
    return numeric_cast<uint8_t>(item);
}

static void CopyManagedByteArrayToNative(ptr<const ManagedScriptBackend> backend, MonoObject* value, void* data, size_t size)
{
    FO_STACK_TRACE_ENTRY();

    if (!IsManagedByteArray(value)) {
        throw ScriptSystemException("Managed byte array expected");
    }

    auto* bytes = reinterpret_cast<MonoArray*>(value);

    if (mono_array_length(bytes) != size) {
        throw ScriptSystemException("Managed byte array size mismatch", mono_array_length(bytes), size);
    }

    const uint32_t bytes_handle = mono_gchandle_new(value, 0);
    auto free_bytes_handle = scope_exit([bytes_handle]() noexcept { mono_gchandle_free(bytes_handle); });
    auto* raw_data = static_cast<uint8_t*>(data);

    for (size_t i = 0; i < size; i++) {
        auto* rooted_bytes = mono_gchandle_get_target(bytes_handle);
        raw_data[i] = GetManagedByteArrayItem(backend, rooted_bytes, i);
    }
}

static auto ConvertManagedSimpleObjectToNative(ptr<ManagedScriptBackend> backend, const BaseTypeDesc& base_type, MonoObject* value, ManagedScalarValue& storage) -> void*
{
    FO_STACK_TRACE_ENTRY();

    if (base_type.Name == "any") {
        storage.Any = any_t(ManagedObjectToString(value));
        return &storage.Any;
    }
    if (base_type.IsString) {
        storage.Text = ToStringAndFree(reinterpret_cast<MonoString*>(value));
        return &storage.Text;
    }
    if (base_type.IsHashedString) {
        storage.Hash = ResolveManagedHashValue(backend, ExtractHashValue(value));
        return &storage.Hash;
    }
    if (base_type.IsEntity || base_type.IsFixedType || base_type.IsEntityProto) {
        storage.EntityPtr = ExtractEntityPtr(value);
        return &storage.EntityPtr;
    }
    if (base_type.IsRefType) {
        if (IsDynamicManagedRefType(base_type)) {
            storage.DynamicRefType = CreateDynamicRefTypeFromManaged(backend, base_type, value);
            storage.RefTypePtr = storage.DynamicRefType.get();
        }
        else {
            storage.RefTypePtr = ExtractRefPtr(value);
        }

        return &storage.RefTypePtr;
    }
    if (base_type.IsPrimitive || base_type.IsEnum || base_type.IsStruct) {
        if (value == nullptr) {
            throw ScriptSystemException("Null passed to Managed value argument", base_type.Name);
        }

        void* data = storage.Alloc(base_type.Size);

        if (base_type.IsStruct && IsManagedByteArray(value)) {
            CopyManagedByteArrayToNative(backend, value, data, base_type.Size);
        }
        else if (base_type.IsStruct && base_type.StructLayout != nullptr) {
            CopyManagedStructToNative(backend, base_type, value, data);
        }
        else {
            MemCopy(data, mono_object_unbox(value), base_type.Size);
        }

        return data;
    }

    throw ScriptSystemException("Unsupported Managed argument type", base_type.Name);
}

static auto ConvertManagedObjectToNative(ptr<ManagedScriptBackend> backend, const ComplexTypeDesc& type, MonoObject* value, ManagedNativeValue& storage) -> void*
{
    FO_STACK_TRACE_ENTRY();

    if (type.Kind == ComplexTypeKind::Simple) {
        return ConvertManagedSimpleObjectToNative(backend, type.BaseType, value, storage);
    }
    if (type.Kind == ComplexTypeKind::Array) {
        storage.Array = SafeAlloc::MakeUnique<ManagedArrayBridgeData>();
        storage.Array->Backend = backend;
        storage.Array->Type = type;
        storage.Array->SetObject(value);
        return storage.Array.get();
    }
    if (type.Kind == ComplexTypeKind::Dict) {
        storage.Dict = SafeAlloc::MakeUnique<ManagedDictBridgeData>();
        storage.Dict->Backend = backend;
        storage.Dict->Type = type;
        storage.Dict->SetObject(value);
        return storage.Dict.get();
    }
    if (type.Kind == ComplexTypeKind::Callback) {
        if (value == nullptr) {
            storage.Callback = SafeAlloc::MakeUnique<ManagedCallbackBridgeData>();
            storage.Callback->Backend = backend;
            storage.Callback->Type = type;
            return storage.Callback.get();
        }

        const string delegate_key = GetManagedDelegateKey(backend, value);
        storage.Callback = SafeAlloc::MakeUnique<ManagedCallbackBridgeData>();
        storage.Callback->Backend = backend;
        storage.Callback->Type = type;
        storage.Callback->Handler = mono_gchandle_new(value, false);
        storage.Callback->Name = backend->GetMetadata()->Hashes.ToHashedString(strex("ManagedCallback:{}", delegate_key).str());
        return storage.Callback.get();
    }

    throw ScriptSystemException("Unsupported Managed argument type", type.BaseType.Name);
}

static auto ManagedObjectClassMatches(MonoObject* value, MonoClass* expected_class) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return value != nullptr && expected_class != nullptr && mono_object_get_class(value) == expected_class;
}

static auto ManagedObjectClassMatchesOrDerives(MonoObject* value, MonoClass* expected_class) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (value == nullptr || expected_class == nullptr) {
        return false;
    }

    for (auto* klass = mono_object_get_class(value); klass != nullptr; klass = mono_class_get_parent(klass)) {
        if (klass == expected_class) {
            return true;
        }
    }

    return false;
}

static auto CanConvertManagedSimpleObjectToNative(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, MonoObject* value) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (base_type.Name == "any") {
        return true;
    }
    if (base_type.IsString) {
        return value == nullptr || ManagedObjectClassMatches(value, mono_get_string_class());
    }
    if (base_type.IsHashedString) {
        return value == nullptr || ManagedObjectClassMatches(value, FindFOnlineClass(backend, "hstring"));
    }
    if (base_type.IsEntity || base_type.IsFixedType || base_type.IsEntityProto) {
        return value == nullptr || ManagedObjectClassMatchesOrDerives(value, FindFOnlineClass(backend, base_type.Name));
    }
    if (base_type.IsRefType) {
        if (IsDynamicManagedRefType(base_type)) {
            return value == nullptr || ManagedObjectClassMatchesOrDerives(value, FindFOnlineClass(backend, base_type.Name));
        }

        return value == nullptr || ManagedObjectClassMatchesOrDerives(value, FindFOnlineClass(backend, base_type.Name));
    }
    if (base_type.IsPrimitive || base_type.IsEnum || base_type.IsStruct) {
        return value != nullptr && ((base_type.IsStruct && IsManagedByteArray(value)) || ManagedObjectClassMatches(value, GetValueClass(backend, base_type)));
    }

    return false;
}

static auto CanConvertManagedObjectToNative(ptr<const ManagedScriptBackend> backend, const ComplexTypeDesc& type, MonoObject* value) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (type.Kind == ComplexTypeKind::Simple) {
        return CanConvertManagedSimpleObjectToNative(backend, type.BaseType, value);
    }
    if (type.Kind == ComplexTypeKind::Array) {
        return InvokeNativeBoolHelper(backend, "IsList", value);
    }
    if (type.Kind == ComplexTypeKind::Dict) {
        return InvokeNativeBoolHelper(backend, "IsDictionary", value);
    }
    if (type.Kind == ComplexTypeKind::Callback) {
        return value == nullptr || InvokeNativeBoolHelper(backend, "IsDelegate", value);
    }

    return false;
}

static auto BoxNativeSimpleValue(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, void* data) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    auto* domain = GetDomainOrThrow(backend->GetDomain());

    if (base_type.Name == "any") {
        const any_t& value = *static_cast<any_t*>(data);
        return reinterpret_cast<MonoObject*>(mono_string_new_len(domain, value.data(), numeric_cast<uint32_t>(value.size())));
    }
    if (base_type.IsString) {
        const string& text = *static_cast<string*>(data);
        return reinterpret_cast<MonoObject*>(mono_string_new_len(domain, text.data(), numeric_cast<uint32_t>(text.size())));
    }
    if (base_type.IsHashedString) {
        const hstring& value = *static_cast<hstring*>(data);
        return CreateHashObject(backend, MakeManagedHashValue(backend, value));
    }
    if (base_type.IsEntity || base_type.IsFixedType || base_type.IsEntityProto) {
        auto* entity = *static_cast<Entity**>(data);
        return CreateEntityObject(backend, base_type.Name, entity);
    }
    if (base_type.IsRefType) {
        auto* ref_ptr = *static_cast<void**>(data);
        return CreateRefTypeObject(backend, base_type, ref_ptr);
    }
    if (base_type.IsStruct && base_type.StructLayout != nullptr) {
        return CreateStructObject(backend, base_type, data);
    }
    if (base_type.IsPrimitive || base_type.IsEnum) {
        return mono_value_box(domain, GetValueClass(backend, base_type), data);
    }

    throw ScriptSystemException("Unsupported Managed return type", base_type.Name);
}

// === Property value marshaling ===

static auto BoxSimplePropertyValue(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, span<const uint8_t> raw_data) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    auto* domain = GetDomainOrThrow(backend->GetDomain());

    if (base_type.Name == "any") {
        return reinterpret_cast<MonoObject*>(mono_string_new_len(domain, reinterpret_cast<const char*>(raw_data.data()), numeric_cast<uint32_t>(raw_data.size())));
    }
    if (base_type.IsString) {
        return reinterpret_cast<MonoObject*>(mono_string_new_len(domain, reinterpret_cast<const char*>(raw_data.data()), numeric_cast<uint32_t>(raw_data.size())));
    }
    if (base_type.IsHashedString) {
        FO_VERIFY_AND_THROW(raw_data.size() == sizeof(hstring::hash_t), "Hashed string raw data size does not match a hash");
        const hstring::hash_t value = *reinterpret_cast<const hstring::hash_t*>(raw_data.data());
        return CreateHashObject(backend, value);
    }
    if (base_type.IsFixedType || base_type.IsEntityProto) {
        return CreateEntityObject(backend, base_type.Name, ResolveProtoEntityFromRawData(backend, base_type, raw_data));
    }
    if (base_type.IsRefType && IsDynamicManagedRefType(base_type)) {
        return CreateDynamicRefTypeObject(backend, base_type, raw_data);
    }
    if (base_type.IsPrimitive || base_type.IsEnum || base_type.IsStruct) {
        FO_VERIFY_AND_THROW(raw_data.size() == base_type.Size, "Raw data size does not match the value type size");

        if (base_type.IsStruct && base_type.StructLayout != nullptr) {
            return CreatePropertyStructObject(backend, base_type, raw_data);
        }

        return mono_value_box(domain, GetValueClass(backend, base_type), const_cast<uint8_t*>(raw_data.data()));
    }

    throw ScriptSystemException("Unsupported Managed property value type", base_type.Name);
}

static auto BoxPropertyValue(ptr<const ManagedScriptBackend> backend, ptr<const Property> prop, span<const uint8_t> raw_data) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    const BaseTypeDesc& base_type = prop->GetBaseType();

    if (prop->IsArray()) {
        auto* list = CreateManagedList(backend, base_type);
        const uint8_t* data = raw_data.data();
        const uint8_t* data_end = raw_data.data() + raw_data.size();

        if (raw_data.empty()) {
            return list;
        }

        if (prop->IsArrayOfString()) {
            const auto data_span = const_span<uint8_t> {raw_data.data(), raw_data.size()};
            size_t data_pos = 0;
            const uint32_t arr_size = span_read_aligned_object<uint32_t>(data_span, data_pos);

            for (uint32_t i = 0; i < arr_size; i++) {
                const uint32_t str_size = span_read_aligned_object<uint32_t>(data_span, data_pos);
                const string text = span_read_string(data_span, data_pos, str_size);
                auto* item = reinterpret_cast<MonoObject*>(mono_string_new_len(GetDomainOrThrow(backend->GetDomain()), text.data(), numeric_cast<uint32_t>(text.size())));
                AddManagedListItem(backend, list, item);
            }

            data = raw_data.data() + data_pos;
        }
        else if (prop->IsBaseTypeRefType()) {
            if (!IsDynamicManagedRefType(base_type)) {
                throw ScriptSystemException("Managed property ref type array is not supported", prop->GetName());
            }

            const auto data_span = const_span<uint8_t> {raw_data.data(), raw_data.size()};
            size_t data_pos = 0;
            const uint32_t arr_size = span_read_aligned_object<uint32_t>(data_span, data_pos);

            for (uint32_t i = 0; i < arr_size; i++) {
                const uint32_t ref_size = span_read_aligned_object<uint32_t>(data_span, data_pos);
                const auto ref_data = span_read_aligned_bytes(data_span, data_pos, ref_size, MAX_SERIALIZED_ALIGNMENT);
                MonoObject* item = CreateDynamicRefTypeObject(backend, base_type, ref_data);
                AddManagedListItem(backend, list, item);
            }

            data = raw_data.data() + data_pos;
        }
        else {
            FO_VERIFY_AND_THROW(raw_data.size() % base_type.Size == 0, "Array property raw data size is not a multiple of the element size");
            const size_t arr_size = raw_data.size() / base_type.Size;

            for (size_t i = 0; i < arr_size; i++) {
                MonoObject* item = BoxSimplePropertyValue(backend, base_type, {data, base_type.Size});
                AddManagedListItem(backend, list, item);
                data += base_type.Size;
            }
        }

        if (data != data_end) {
            throw ScriptSystemException("Corrupted Managed array property tail", prop->GetName());
        }

        return list;
    }
    if (prop->IsDict()) {
        if (!IsManagedBridgeFixedDictionaryProperty(prop)) {
            throw ScriptSystemException("Managed dictionary property bridge supports only fixed-size key/value types", prop->GetName());
        }

        const BaseTypeDesc& key_type = prop->GetDictKeyType();
        auto* dictionary = CreateManagedDictionary(backend, key_type, base_type);
        if (raw_data.empty()) {
            return dictionary;
        }
        if (key_type.Size == 0 || base_type.Size == 0) {
            throw ScriptSystemException("Corrupted Managed dictionary property", prop->GetName());
        }

        const auto data_span = const_span<uint8_t> {raw_data.data(), raw_data.size()};
        size_t data_pos = 0;

        while (data_pos < raw_data.size()) {
            const auto key_data = span_read_aligned_bytes(data_span, data_pos, key_type.Size, alignment_for_size(key_type.Size));
            MonoObject* key = BoxSimplePropertyValue(backend, key_type, key_data);

            const auto value_data = span_read_aligned_bytes(data_span, data_pos, base_type.Size, alignment_for_size(base_type.Size));
            MonoObject* value = BoxSimplePropertyValue(backend, base_type, value_data);

            AddManagedDictionaryItem(backend, dictionary, key, value);
        }
        if (data_pos != raw_data.size()) {
            throw ScriptSystemException("Corrupted Managed dictionary property tail", prop->GetName());
        }

        return dictionary;
    }

    return BoxSimplePropertyValue(backend, base_type, raw_data);
}

static auto ConvertManagedSimpleObjectToPropertyData(ptr<ManagedScriptBackend> backend, const BaseTypeDesc& base_type, MonoObject* value) -> PropertyRawData
{
    FO_STACK_TRACE_ENTRY();

    PropertyRawData prop_data;

    if (base_type.Name == "any") {
        const string text = ManagedObjectToString(value);
        prop_data.Set(text.data(), text.size());
    }
    else if (base_type.IsString) {
        const string text = ToStringAndFree(reinterpret_cast<MonoString*>(value));
        prop_data.Set(text.data(), text.size());
    }
    else if (base_type.IsHashedString) {
        const hstring hash = ResolveManagedHashValue(backend, ExtractHashValue(value));
        prop_data.SetAs(hash.as_hash());
    }
    else if (base_type.IsFixedType || base_type.IsEntityProto) {
        const hstring::hash_t proto_hash = ExtractProtoHashFromManagedEntity(value);
        prop_data.SetAs(proto_hash);
    }
    else if (base_type.IsRefType && IsDynamicManagedRefType(base_type)) {
        refcount_nptr<DynamicRefTypeInstance> ref_instance = CreateDynamicRefTypeFromManaged(backend, base_type, value);

        if (ref_instance) {
            const span<const uint8_t> raw_data = ref_instance->GetSerializedRawData(base_type);

            if (!raw_data.empty()) {
                prop_data.Set(raw_data.data(), raw_data.size());
            }
        }
    }
    else if (base_type.IsPrimitive || base_type.IsEnum || base_type.IsStruct) {
        if (value == nullptr) {
            throw ScriptSystemException("Null passed to Managed property value", base_type.Name);
        }

        void* data = prop_data.Alloc(base_type.Size).get();

        if (base_type.IsStruct && base_type.StructLayout != nullptr) {
            CopyManagedStructToPropertyData(backend, base_type, value, data);
        }
        else {
            MemCopy(data, mono_object_unbox(value), base_type.Size);
        }
    }
    else {
        throw ScriptSystemException("Unsupported Managed property type", base_type.Name);
    }

    return prop_data;
}

static auto ConvertManagedObjectToPropertyData(ptr<ManagedScriptBackend> backend, ptr<const Property> prop, MonoObject* value) -> PropertyRawData
{
    FO_STACK_TRACE_ENTRY();

    const BaseTypeDesc& base_type = prop->GetBaseType();

    if (!prop->IsArray() && !prop->IsDict()) {
        return ConvertManagedSimpleObjectToPropertyData(backend, base_type, value);
    }
    if (prop->IsDict()) {
        if (!IsManagedBridgeFixedDictionaryProperty(prop)) {
            throw ScriptSystemException("Managed dictionary property bridge supports only fixed-size key/value types", prop->GetName());
        }

        PropertyRawData prop_data;
        const BaseTypeDesc& key_type = prop->GetDictKeyType();
        const size_t dict_size = GetManagedDictionaryCount(backend, value);

        if (dict_size == 0) {
            return prop_data;
        }

        vector<uint8_t> data;
        data.reserve(dict_size * (key_type.Size + base_type.Size));

        for (size_t i = 0; i < dict_size; i++) {
            MonoObject* key = GetManagedDictionaryKey(backend, value, i);
            PropertyRawData key_data = ConvertManagedSimpleObjectToPropertyData(backend, key_type, key);

            if (key_data.GetSize() != key_type.Size) {
                throw ScriptSystemException("Managed property dictionary key size mismatch", prop->GetName());
            }

            AppendAlignedRawBytes(data, key_data.GetPtr().get(), key_data.GetSize(), alignment_for_size(key_data.GetSize()));

            MonoObject* item = GetManagedDictionaryValue(backend, value, i);
            PropertyRawData item_data = ConvertManagedSimpleObjectToPropertyData(backend, base_type, item);

            if (item_data.GetSize() != base_type.Size) {
                throw ScriptSystemException("Managed property dictionary value size mismatch", prop->GetName());
            }

            AppendAlignedRawBytes(data, item_data.GetPtr().get(), item_data.GetSize(), alignment_for_size(item_data.GetSize()));
        }

        prop_data.Set(data.data(), data.size());
        return prop_data;
    }

    PropertyRawData prop_data;
    const size_t arr_size = GetManagedListCount(backend, value);

    if (arr_size == 0) {
        return prop_data;
    }

    vector<uint8_t> data;

    if (prop->IsArrayOfString()) {
        const uint32_t arr_size_value = numeric_cast<uint32_t>(arr_size);
        AppendAlignedRawBytes(data, &arr_size_value, sizeof(arr_size_value), sizeof(uint32_t));

        for (size_t i = 0; i < arr_size; i++) {
            MonoObject* item = GetManagedListItem(backend, value, i);
            const string text = ToStringAndFree(reinterpret_cast<MonoString*>(item));
            const uint32_t text_size = numeric_cast<uint32_t>(text.size());
            AppendAlignedRawBytes(data, &text_size, sizeof(text_size), sizeof(uint32_t));
            AppendRawBytes(data, text.data(), text.size());
        }
    }
    else if (prop->IsBaseTypeRefType()) {
        if (!IsDynamicManagedRefType(base_type)) {
            throw ScriptSystemException("Managed property ref type array is not supported", prop->GetName());
        }

        const uint32_t arr_size_value = numeric_cast<uint32_t>(arr_size);
        AppendRawBytes(data, &arr_size_value, sizeof(arr_size_value));

        for (size_t i = 0; i < arr_size; i++) {
            MonoObject* item = GetManagedListItem(backend, value, i);
            refcount_nptr<DynamicRefTypeInstance> ref_instance = CreateDynamicRefTypeFromManaged(backend, base_type, item);
            span<const uint8_t> raw_data;

            if (ref_instance) {
                raw_data = ref_instance->GetSerializedRawData(base_type);
            }

            const uint32_t ref_size = numeric_cast<uint32_t>(raw_data.size());
            AppendAlignedRawBytes(data, &ref_size, sizeof(ref_size), sizeof(uint32_t));
            AppendAlignedRawBytes(data, raw_data.data(), raw_data.size(), MAX_SERIALIZED_ALIGNMENT);
        }
    }
    else {
        data.reserve(arr_size * base_type.Size);

        for (size_t i = 0; i < arr_size; i++) {
            MonoObject* item = GetManagedListItem(backend, value, i);
            PropertyRawData item_data = ConvertManagedSimpleObjectToPropertyData(backend, base_type, item);

            if (item_data.GetSize() != base_type.Size) {
                throw ScriptSystemException("Managed property array item size mismatch", prop->GetName());
            }

            AppendRawBytes(data, item_data.GetPtr().get(), item_data.GetSize());
        }
    }

    if (!data.empty()) {
        prop_data.Set(data.data(), data.size());
    }

    return prop_data;
}

static auto GetPropertyRawData(ptr<Entity> entity, ptr<const Property> prop) -> PropertyRawData
{
    FO_STACK_TRACE_ENTRY();

    PropertyRawData prop_data;

    if (prop->IsVirtual()) {
        auto getter = prop->GetGetter();

        if (!*getter) {
            throw ScriptSystemException("Property getter not set", prop->GetName());
        }

        prop_data = (*getter)(entity, prop);
    }
    else {
        auto props = entity->GetProperties();
        props->ValidateForRawData(prop);
        prop_data.Pass(props->GetRawData(prop));
    }

    return prop_data;
}

// === Managed bridge type predicates ===

static auto IsManagedBridgeSimpleType(const BaseTypeDesc& type) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    // A fixed type is a proto-reference value (stored as a proto-id hash, resolved to its proto entity on
    // both sides), so it crosses the bridge like an entity proto even though it is not flagged IsEntity.
    return type.Name == "any" || type.IsPrimitive || type.IsString || type.IsHashedString || type.IsEnum || type.IsStruct || type.IsEntity || type.IsFixedType || type.IsEntityProto || type.IsRefType;
}

static auto IsManagedBridgeType(const ComplexTypeDesc& type) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!type) {
        return true;
    }
    if (type.Kind == ComplexTypeKind::Simple || type.Kind == ComplexTypeKind::Array) {
        return IsManagedBridgeSimpleType(type.BaseType);
    }
    if (type.Kind == ComplexTypeKind::Dict) {
        FO_VERIFY_AND_THROW(type.KeyType, "Dictionary type has no key type");
        return IsManagedBridgeSimpleType(*type.KeyType) && IsManagedBridgeSimpleType(type.BaseType);
    }
    if (type.Kind == ComplexTypeKind::Callback) {
        if (!type.CallbackArgs) {
            return false;
        }

        return std::ranges::all_of(*type.CallbackArgs, [](const ComplexTypeDesc& callback_arg) { return IsManagedBridgeType(callback_arg); });
    }

    return false;
}

static auto IsManagedBridgeFixedDictionaryValueType(const BaseTypeDesc& type) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.IsPrimitive || type.IsEnum || type.IsStruct || type.IsHashedString || type.IsFixedType || type.IsEntityProto;
}

static auto IsManagedBridgeFixedDictionaryProperty(ptr<const Property> prop) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!prop->IsDict() || prop->IsDictOfArray() || prop->IsDictKeyString() || prop->IsDictOfString()) {
        return false;
    }

    return IsManagedBridgeFixedDictionaryValueType(prop->GetDictKeyType()) && IsManagedBridgeFixedDictionaryValueType(prop->GetBaseType());
}

static auto IsDynamicManagedRefType(const BaseTypeDesc& base_type) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return base_type.IsRefType && base_type.RefType != nullptr && base_type.RefType->FieldsRegistrator != nullptr;
}

// === Type/class and metadata resolution ===

static auto FindFOnlineClass(ptr<const ManagedScriptBackend> backend, string_view class_name) -> MonoClass*
{
    FO_STACK_TRACE_ENTRY();

    const string class_name_str {class_name};

    for (nptr<void> image_ptr : backend->GetImages()) {
        auto* klass = mono_class_from_name(static_cast<MonoImage*>(image_ptr.get()), "FOnline", class_name_str.c_str());

        if (klass != nullptr) {
            return klass;
        }
    }

    throw ScriptSystemException("Managed class not found", strex("FOnline.{}", class_name).str());
}

static auto GetPrimitiveClass(const BaseTypeDesc& type) -> MonoClass*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type.IsBool) {
        return mono_get_boolean_class();
    }
    if (type.IsInt8) {
        return mono_get_sbyte_class();
    }
    if (type.IsUInt8) {
        return mono_get_byte_class();
    }
    if (type.IsInt16) {
        return mono_get_int16_class();
    }
    if (type.IsUInt16) {
        return mono_get_uint16_class();
    }
    if (type.IsInt32) {
        return mono_get_int32_class();
    }
    if (type.IsUInt32) {
        return mono_get_uint32_class();
    }
    if (type.IsInt64) {
        return mono_get_int64_class();
    }
    if (type.IsUInt64) {
        return mono_get_uint64_class();
    }
    if (type.IsSingleFloat) {
        return mono_get_single_class();
    }
    if (type.IsDoubleFloat) {
        return mono_get_double_class();
    }

    return nullptr;
}

static auto GetValueClass(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& type) -> MonoClass*
{
    FO_STACK_TRACE_ENTRY();

    if (MonoClass* primitive_class = GetPrimitiveClass(type); primitive_class != nullptr) {
        return primitive_class;
    }
    if (type.IsHashedString) {
        return FindFOnlineClass(backend, "hstring");
    }
    if (type.IsEnum || type.IsStruct || type.IsEntity || type.IsFixedType || type.IsEntityProto) {
        // The managed representation of an enum/struct/entity/entity-proto/fixed type is a class named after
        // the type (e.g. FOnline.ItemBag), so an array element of such a type resolves to that class.
        return FindFOnlineClass(backend, type.Name);
    }

    throw ScriptSystemException("Unsupported Managed value type", type.Name);
}

static auto FindFieldInHierarchy(MonoClass* klass, const char* field_name) -> MonoClassField*
{
    FO_NO_STACK_TRACE_ENTRY();

    for (auto* cur_class = klass; cur_class != nullptr; cur_class = mono_class_get_parent(cur_class)) {
        if (auto* field = mono_class_get_field_from_name(cur_class, field_name); field != nullptr) {
            return field;
        }
    }

    return nullptr;
}

static auto FindEntityTypeDesc(ptr<EngineMetadata> meta, string_view owner_type_name) -> nptr<const EntityTypeDesc>
{
    FO_STACK_TRACE_ENTRY();

    if (meta->IsValidEntityType(owner_type_name)) {
        return &meta->GetEntityType(meta->Hashes.ToHashedString(owner_type_name));
    }
    if (meta->IsFixedType(owner_type_name)) {
        return &meta->GetFixedType(meta->Hashes.ToHashedString(owner_type_name));
    }

    return nullptr;
}

static auto FindRefTypeDesc(ptr<EngineMetadata> meta, string_view owner_type_name) -> nptr<const RefTypeDesc>
{
    FO_STACK_TRACE_ENTRY();

    if (!meta->IsValidBaseType(owner_type_name)) {
        return nullptr;
    }

    const BaseTypeDesc& base_type = meta->GetBaseType(owner_type_name);

    if (!base_type.IsRefType || base_type.RefType == nullptr) {
        return nullptr;
    }

    return base_type.RefType.get();
}

static auto FindMethod(ptr<EngineMetadata> meta, string_view owner_type_name, string_view method_name, int32_t method_index, size_t args_count) -> nptr<const MethodDesc>
{
    FO_STACK_TRACE_ENTRY();

    auto entity_desc = FindEntityTypeDesc(meta, owner_type_name);
    auto ref_type_desc = entity_desc == nullptr ? FindRefTypeDesc(meta, owner_type_name) : nullptr;
    const vector<MethodDesc>* methods = entity_desc != nullptr ? &entity_desc->Methods : (ref_type_desc != nullptr ? &ref_type_desc->Methods : nullptr);

    if (methods == nullptr || method_index < 0 || numeric_cast<size_t>(method_index) >= methods->size()) {
        return nullptr;
    }

    const MethodDesc& method = (*methods)[numeric_cast<size_t>(method_index)];

    // Property getters/setters are invoked through this same path: the generated property bodies route
    // through Native.CallMethod by the accessor's method index, and a getter/setter is a plain func
    // (0 args -> value / 1 arg -> void) that the generic invocation below handles like any method.
    if (method.Name != method_name || method.Args.size() != args_count) {
        return nullptr;
    }
    if (!IsManagedBridgeType(method.Ret)) {
        return nullptr;
    }
    if (std::ranges::any_of(method.Args, [](const ArgDesc& arg) { return !IsManagedBridgeType(arg.Type); })) {
        return nullptr;
    }

    return &method;
}

static auto MakeManagedGlobalSimpleType(ptr<EngineMetadata> meta, string_view type_name) -> ComplexTypeDesc
{
    FO_STACK_TRACE_ENTRY();

    // Build a ComplexTypeDesc from an engine type name (mirrors the simple-type and array branches of
    // AngelScript's resolve_type). Returns an empty (false) desc when the name is not a valid engine base type,
    // signalling the caller to skip the function (it could not match any FindFunc<...> signature anyway).
    ComplexTypeDesc type;

    // Array element: "T[]" -> Array desc wrapping the element base type. The managed registration emits this for
    // List<T> / T[] params and returns (see ScriptFuncRegistration.EngineTypeName), so collection-typed bridges
    // (e.g. Barter::MakeBarterManaged, Purchases::SelectUningestedWebOrdersManaged) match the same FindFunc<...>
    // signature an AngelScript array arg produces.
    if (type_name.ends_with("[]")) {
        const string_view elem_name = type_name.substr(0, type_name.size() - 2);

        if (!meta->IsValidBaseType(elem_name)) {
            return {};
        }

        type.Kind = ComplexTypeKind::Array;
        type.BaseType = meta->GetBaseType(elem_name);
        return type;
    }

    if (!meta->IsValidBaseType(type_name)) {
        return {};
    }

    type.Kind = ComplexTypeKind::Simple;
    type.BaseType = meta->GetBaseType(type_name);
    return type;
}

// === Entity resolution and inner-entry helpers ===

static auto ResolveEntity(ptr<ManagedScriptBackend> backend, void* entity_ptr) -> ptr<Entity>
{
    FO_STACK_TRACE_ENTRY();

    auto* entity = entity_ptr != nullptr ? static_cast<Entity*>(entity_ptr) : backend->GetGlobalEntity();

    if (entity == nullptr) {
        throw ScriptSystemException("Managed entity target is null");
    }
    if (entity->IsDestroyed()) {
        throw ScriptSystemException("Managed entity target is destroyed", entity->GetName());
    }

    return entity;
}

static auto ResolveProtoEntityFromRawData(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, span<const uint8_t> raw_data) -> nptr<Entity>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(base_type.IsFixedType || base_type.IsEntityProto, "Base type is not a fixed type or entity proto");
    FO_VERIFY_AND_THROW(raw_data.size() == sizeof(hstring::hash_t), "Proto reference raw data size does not match a hash");

    hstring::hash_t proto_hash {};
    MemCopy(&proto_hash, raw_data.data(), sizeof(proto_hash));

    const hstring proto_id = backend->GetMetadata()->Hashes.ResolveHash(proto_hash);
    auto proto = backend->GetMetadata()->GetProtoEntity(base_type.HashedName, proto_id);
    return const_cast<ProtoEntity*>(proto.get());
}

static auto ExtractProtoHashFromManagedEntity(MonoObject* value) -> hstring::hash_t
{
    FO_STACK_TRACE_ENTRY();

    auto* entity = ExtractEntityPtr(value);

    if (entity == nullptr) {
        return {};
    }

    auto* proto = dynamic_cast<ProtoEntity*>(entity);

    if (proto == nullptr) {
        throw ScriptSystemException("Managed proto entity wrapper expected", entity->GetName());
    }

    return proto->GetProtoId().as_hash();
}

static auto ResolveInnerEntry(ptr<ManagedScriptBackend> backend, MonoString* entry_name) -> hstring
{
    FO_STACK_TRACE_ENTRY();

    const string entry_name_str = ToStringAndFree(entry_name);
    return backend->GetMetadata()->Hashes.ToHashedString(entry_name_str);
}

static void ValidateManagedInnerEntity(ptr<const Entity> entity)
{
    FO_NO_STACK_TRACE_ENTRY();

    entity->ValidateAccess();

    if (entity->IsDestroyed()) {
        throw ScriptSystemException("Access to destroyed inner entity");
    }
}

static auto CollectManagedInnerEntities(ptr<Entity> holder, hstring entry) -> vector<ptr<Entity>>
{
    FO_STACK_TRACE_ENTRY();

    auto entities = holder->GetInnerEntities(entry);
    vector<ptr<Entity>> result;

    if (!entities || entities->empty()) {
        return result;
    }

    result.reserve(entities->size());

    for (auto& entity : *entities) {
        ValidateManagedInnerEntity(entity.get());
        result.emplace_back(entity.get());
    }

    return result;
}

// === Extraction and hash primitives ===

static auto ExtractEntityPtr(MonoObject* obj) -> Entity*
{
    FO_STACK_TRACE_ENTRY();

    if (obj == nullptr) {
        return nullptr;
    }

    auto* object_class = mono_object_get_class(obj);
    auto* field = FindFieldInHierarchy(object_class, "_entityPtrValue");

    if (field != nullptr) {
        auto* backend_field = FindFieldInHierarchy(object_class, "_backend");
        auto* alive_field = FindFieldInHierarchy(object_class, "_backendAlive");

        if (backend_field == nullptr || alive_field == nullptr) {
            throw ScriptSystemException("Managed entity wrapper has incomplete backend identity fields");
        }

        void* wrapper_backend = nullptr;
        mono_field_get_value(obj, backend_field, &wrapper_backend);

        if (wrapper_backend != GetActiveBackendOrThrow().get()) {
            throw ScriptSystemException("Managed entity wrapper belongs to a different backend");
        }

        MonoArray* alive_flag = nullptr;
        mono_field_get_value(obj, alive_field, &alive_flag);

        if (alive_flag == nullptr || mono_array_length(alive_flag) == 0 || mono_array_get(alive_flag, uint8_t, 0) == 0) {
            throw ScriptSystemException("Managed entity wrapper backend is no longer alive");
        }
    }
    else {
        field = FindFieldInHierarchy(object_class, "_entityPtr");
    }

    if (field == nullptr) {
        throw ScriptSystemException("Managed entity wrapper has no native pointer field");
    }

    void* entity_ptr = nullptr;
    mono_field_get_value(obj, field, &entity_ptr);
    return static_cast<Entity*>(entity_ptr);
}

static auto ExtractRefPtr(MonoObject* obj) -> void*
{
    FO_STACK_TRACE_ENTRY();

    if (obj == nullptr) {
        return nullptr;
    }

    auto* field = FindFieldInHierarchy(mono_object_get_class(obj), "_refPtr");

    if (field == nullptr) {
        throw ScriptSystemException("Managed ref type wrapper has no native pointer field");
    }

    void* ref_ptr = nullptr;
    mono_field_get_value(obj, field, &ref_ptr);
    return ref_ptr;
}

static auto ExtractHashValue(MonoObject* obj) -> hstring::hash_t
{
    FO_STACK_TRACE_ENTRY();

    if (obj == nullptr) {
        return {};
    }

    auto* field = FindFieldInHierarchy(mono_object_get_class(obj), "Value");

    if (field != nullptr) {
        hstring::hash_t value {};
        mono_field_get_value(obj, field, &value);
        return value;
    }

    return *static_cast<hstring::hash_t*>(mono_object_unbox(obj));
}

static auto MakeManagedHashValue(ptr<const ManagedScriptBackend> backend, const hstring& value) -> hstring::hash_t
{
    FO_STACK_TRACE_ENTRY();

    if (!value) {
        return {};
    }

    return backend->GetMetadata()->Hashes.ToHashedString(value.as_str()).as_hash();
}

static auto ResolveManagedHashValue(ptr<const ManagedScriptBackend> backend, hstring::hash_t value) -> hstring
{
    FO_STACK_TRACE_ENTRY();

    if (value == 0) {
        return {};
    }

    bool failed = false;
    const hstring backend_value = backend->GetMetadata()->Hashes.ResolveHash(value, &failed);

    if (!failed) {
        return backend_value;
    }

    throw ScriptSystemException("Managed hstring is not interned in the active backend", value);
}

// === Assembly loading, runtime configuration and resource cache ===

static auto IsManagedEntryAssemblyFileName(string_view file_name, string_view target_name) -> bool
{
    FO_STACK_TRACE_ENTRY();

    return file_name.ends_with(strex(".{}.dll", target_name).str());
}

static auto IsManagedHostAssemblyFileName(string_view file_name) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return file_name == MANAGED_HOST_ASSEMBLY_FILE_NAME;
}

static auto CollectAssemblyResources(const FileSystem& resources, string_view target_name) -> vector<ManagedAssemblyResource>
{
    FO_STACK_TRACE_ENTRY();

    const string assembly_dir = strex("Assemblies/{}Assemblies", target_name).str();
    vector<ManagedAssemblyResource> result;

    for (const FileHeader& file : resources.FilterFiles("dll", assembly_dir, false)) {
        const string resource_path {file.GetPath()};
        const string file_name = strex(resource_path).extract_file_name().str();
        const auto assembly_file = resources.ReadFile(resource_path);

        if (!assembly_file) {
            throw ScriptSystemException("Can't read Managed assembly from resources", resource_path);
        }

        result.emplace_back(ManagedAssemblyResource {
            .ResourcePath = resource_path,
            .FileName = file_name,
            .Data = assembly_file.GetData(),
        });
    }

    std::ranges::sort(result, {}, &ManagedAssemblyResource::ResourcePath);
    return result;
}

static auto IsRuntimeLayoutPath(const std::filesystem::path& dir) -> bool
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    return std::filesystem::exists(dir / "lib", ec) || std::filesystem::exists(dir / "etc", ec) || std::filesystem::exists(dir / "bin", ec);
}

static auto FindManagedRuntimeDir() -> optional<std::filesystem::path>
{
    FO_STACK_TRACE_ENTRY();

    vector<std::filesystem::path> candidates;

    candidates.emplace_back(std::filesystem::current_path() / "ManagedRuntime");

    if (const auto exe_path = Platform::GetExePath()) {
        const auto exe_dir = std::filesystem::path(*exe_path).parent_path();
        candidates.emplace_back(exe_dir / "ManagedRuntime");
    }

    for (const std::filesystem::path& candidate : candidates) {
        std::error_code ec;
        const auto normalized = std::filesystem::weakly_canonical(candidate, ec);
        const std::filesystem::path& runtime_dir = !ec ? normalized : candidate;

        if (IsRuntimeLayoutPath(runtime_dir)) {
            return runtime_dir;
        }
    }

    return std::nullopt;
}

static void AppendExistingAssemblyPath(vector<string>& paths, const std::filesystem::path& dir)
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;

    if (std::filesystem::is_directory(dir, ec)) {
        paths.emplace_back(dir.string());
    }
}

static auto BuildAssemblySearchPath(const std::filesystem::path& lib_dir) -> string
{
    FO_STACK_TRACE_ENTRY();

    vector<string> paths;
    AppendExistingAssemblyPath(paths, lib_dir / "netcoreapp");
    AppendExistingAssemblyPath(paths, lib_dir);

    string result;

    for (const string& path : paths) {
        if (!result.empty()) {
            result.push_back(MANAGED_ASSEMBLY_PATH_SEPARATOR);
        }

        result += path;
    }

    return result;
}

static void SetEnvironmentVariableDefault(const char* name, const char* value)
{
    FO_STACK_TRACE_ENTRY();

    if (std::getenv(name) != nullptr) {
        return;
    }

#if FO_WINDOWS
    (void)_putenv_s(name, value);
#else
    (void)setenv(name, value, 1);
#endif
}

static void ConfigureManagedRuntime()
{
    FO_STACK_TRACE_ENTRY();

    // The engine host owns process-level crash reporting. Ask embedded Mono to chain the handlers
    // that were already installed by the host (or by a test runner) instead of replacing them.
    mono_set_signal_chaining(true);
    mono_set_crash_chaining(true);

    // The embedded Mono runtime is published without System.Globalization.Native.
    // Use invariant globalization by default; callers can override the env var.
    SetEnvironmentVariableDefault("DOTNET_SYSTEM_GLOBALIZATION_INVARIANT", "1");

    // Force preemptive GC thread suspension. Under the multithreaded game-logic model the process
    // hosts several in-process engines (e.g. the parallel test runner spawns a ServerEngine plus a
    // ClientEngine per suite worker), each with native worker/loop threads attached to the shared
    // Mono domain. The default hybrid/cooperative suspend strategy deadlocks when a GC has to
    // suspend those native threads while they sit in engine loops that never reach a managed
    // safepoint; preemptive (OS-level) suspension stops them directly. Callers can override the env var.
    SetEnvironmentVariableDefault("MONO_THREADS_SUSPEND", "preemptive");

    const auto runtime_dir = FindManagedRuntimeDir();

    if (!runtime_dir.has_value()) {
        mono_config_parse(nullptr);
        return;
    }

    const auto lib_dir = *runtime_dir / "lib";
    const auto etc_dir = *runtime_dir / "etc";
    const auto config_file = etc_dir / "mono" / "config";
    const std::string lib_dir_str = lib_dir.string();
    const std::string etc_dir_str = etc_dir.string();
    const std::string config_file_str = config_file.string();
    const string assembly_search_path = BuildAssemblySearchPath(lib_dir);

    mono_set_dirs(lib_dir_str.c_str(), etc_dir_str.c_str());

    if (!assembly_search_path.empty()) {
        mono_set_assemblies_path(assembly_search_path.c_str());
    }

    if (std::filesystem::exists(config_file)) {
        mono_config_parse(config_file_str.c_str());
    }
    else {
        mono_config_parse(nullptr);
    }

    WriteLog("Managed runtime directory: {}", runtime_dir->string());
}

static void AddManagedAssemblyCacheByte(uint64_t& hash, uint8_t byte) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    hash ^= byte;
    hash *= 1099511628211ull;
}

static auto MakeManagedAssemblyCacheKey(const vector<ManagedAssemblyResource>& assembly_resources) noexcept -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    uint64_t hash = 1469598103934665603ull;

    for (const ManagedAssemblyResource& resource : assembly_resources) {
        for (const char c : resource.ResourcePath) {
            AddManagedAssemblyCacheByte(hash, static_cast<uint8_t>(c));
        }

        AddManagedAssemblyCacheByte(hash, 0);

        for (const uint8_t byte : resource.Data) {
            AddManagedAssemblyCacheByte(hash, byte);
        }

        AddManagedAssemblyCacheByte(hash, 0);
    }

    return strex("{}", hash).str();
}

static auto IsSameManagedAssemblyCacheFile(const std::filesystem::path& disk_path, const_span<uint8_t> assembly_data) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto existing_data = fs_read_file(disk_path.string());

    if (!existing_data.has_value()) {
        return false;
    }
    if (existing_data->size() != assembly_data.size()) {
        return false;
    }

    for (size_t i = 0; i != assembly_data.size(); i++) {
        if (static_cast<uint8_t>((*existing_data)[i]) != assembly_data[i]) {
            return false;
        }
    }

    return true;
}

static auto RestoreAssemblyResources(const vector<ManagedAssemblyResource>& assembly_resources) -> unordered_map<string, std::filesystem::path>
{
    FO_STACK_TRACE_ENTRY();

    unordered_map<string, std::filesystem::path> restored_paths;

    if (assembly_resources.empty()) {
        return restored_paths;
    }

    const auto cache_root = std::filesystem::current_path() / "Cache" / "ManagedAssemblies" / MakeManagedAssemblyCacheKey(assembly_resources);
    restored_paths.reserve(assembly_resources.size());

    for (const ManagedAssemblyResource& resource : assembly_resources) {
        const auto disk_path = cache_root / resource.ResourcePath;
        const std::string disk_dir = disk_path.parent_path().string();

        if (!fs_create_directories(disk_dir)) {
            throw ScriptSystemException("Can't create Managed assembly cache directory", disk_dir);
        }
        if (!IsSameManagedAssemblyCacheFile(disk_path, resource.Data)) {
            if (!fs_write_file(disk_path.string(), resource.Data)) {
                throw ScriptSystemException("Can't restore Managed assembly from resources", resource.ResourcePath);
            }
        }

        restored_paths.emplace(resource.ResourcePath, disk_path);
    }

    return restored_paths;
}

// Bake-time only: scan the bake output tree for the managed entry assembly a validation engine needs. The managed
// baker writes <bake_output_dir>/<managed pack>/Assemblies/<Target>Assemblies/<Pack>.<Target>.dll during its own
// (earlier) bake order; the validators run in later packs whose resource FileSystem does not yet expose that
// output, so read it straight from disk. Returns every dll in the first matching <Target>Assemblies directory so
// helper assemblies load next to the entry.
static auto CollectBakeOutputAssemblyPaths(string_view bake_output_dir, string_view target_name) -> vector<std::filesystem::path>
{
    FO_STACK_TRACE_ENTRY();

    const std::filesystem::path bake_root {bake_output_dir};
    std::error_code ec;

    if (!std::filesystem::is_directory(bake_root, ec)) {
        return {};
    }

    const string target_subdir = strex("{}Assemblies", target_name).str();

    for (std::filesystem::directory_iterator pack_it(bake_root, ec); !ec && pack_it != std::filesystem::directory_iterator(); pack_it.increment(ec)) {
        if (!pack_it->is_directory()) {
            continue;
        }

        const auto target_dir = pack_it->path() / "Assemblies" / target_subdir;
        std::error_code dir_ec;

        if (!std::filesystem::is_directory(target_dir, dir_ec)) {
            continue;
        }

        vector<std::filesystem::path> result;
        bool has_entry = false;

        for (std::filesystem::directory_iterator it(target_dir, dir_ec); !dir_ec && it != std::filesystem::directory_iterator(); it.increment(dir_ec)) {
            if (!it->is_regular_file() || it->path().extension() != ".dll") {
                continue;
            }

            const string file_name = strex("{}", it->path().filename().string()).str();
            result.emplace_back(it->path().lexically_normal());
            has_entry = has_entry || IsManagedEntryAssemblyFileName(file_name, target_name);
        }

        if (has_entry) {
            std::ranges::sort(result, {}, [](const std::filesystem::path& path) { return path.string(); });
            return result;
        }
    }

    return {};
}

// === Low-level Mono/string primitives ===

static auto GetDomainOrThrow(void* domain) -> MonoDomain*
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* mdomain = static_cast<MonoDomain*>(domain);

    if (mdomain == nullptr) {
        throw ScriptSystemException("Managed runtime domain is not initialized");
    }

    return mdomain;
}

static auto MakeManagedPathArray(MonoDomain* domain, const vector<std::filesystem::path>& paths) -> MonoArray*
{
    FO_STACK_TRACE_ENTRY();

    auto* result = mono_array_new(domain, mono_get_string_class(), paths.size());

    if (result == nullptr) {
        throw ScriptSystemException("Can't create Managed assembly path array");
    }

    for (size_t i = 0; i < paths.size(); i++) {
        std::error_code ec;
        const auto absolute_path = std::filesystem::absolute(paths[i], ec).lexically_normal();
        const string path = fs_path_to_string(ec ? paths[i].lexically_normal() : absolute_path);
        auto* managed_path = mono_string_new(domain, path.c_str());

        if (managed_path == nullptr) {
            throw ScriptSystemException("Can't create Managed assembly path string", path);
        }

        mono_array_setref(result, i, managed_path);
    }

    return result;
}

static auto ToStringAndFree(MonoString* text) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (text == nullptr) {
        return {};
    }

    char* text_utf8 = mono_string_to_utf8(text);

    if (text_utf8 == nullptr) {
        return {};
    }

    string result = text_utf8;
    mono_free(text_utf8);
    return result;
}

static auto ManagedObjectToString(MonoObject* obj) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (obj == nullptr) {
        return {};
    }

    auto* str = mono_object_to_string(obj, nullptr);

    if (str == nullptr) {
        return {};
    }

    return ToStringAndFree(str);
}

static void ThrowIfManagedException(MonoObject* exception, string_view context)
{
    FO_STACK_TRACE_ENTRY();

    if (exception != nullptr) {
        throw ScriptSystemException(strex("{}: {}", context, ManagedObjectToString(exception)).str());
    }
}

// ---------------------------------------------------------------------------------------------------
// ManagedScriptBackend member functions
// ---------------------------------------------------------------------------------------------------

auto ManagedScriptBackend::CreateLoadScope(const std::filesystem::path& host_assembly_path, const vector<std::filesystem::path>& assembly_paths, const vector<std::filesystem::path>& entry_assembly_paths) -> vector<nptr<void>>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_loadScopeGcHandle == 0, "Managed load scope is already created");
    scoped_lock load_locker {ManagedAssemblyLoadLocker};

    auto* domain = GetDomainOrThrow(_domain.get());
    const string host_path = fs_path_to_string(host_assembly_path);
    auto* host_assembly = mono_domain_assembly_open(domain, host_path.c_str());

    if (host_assembly == nullptr) {
        throw ScriptSystemException("Failed to load Managed load-context host assembly", host_path);
    }

    auto* host_image = mono_assembly_get_image(host_assembly);

    if (host_image == nullptr) {
        throw ScriptSystemException("Managed load-context host image is null", host_path);
    }

    auto* host_class = mono_class_from_name(host_image, MANAGED_HOST_NAMESPACE.data(), MANAGED_HOST_CLASS_NAME.data());

    if (host_class == nullptr) {
        throw ScriptSystemException("Managed load-context host class not found");
    }

    auto* create_method = mono_class_get_method_from_name(host_class, "CreateLoadScope", 3);
    auto* get_entries_method = mono_class_get_method_from_name(host_class, "GetEntryAssemblies", 1);

    if (create_method == nullptr || get_entries_method == nullptr) {
        throw ScriptSystemException("Managed load-context host methods not found");
    }

    const string context_name = strex("FOnline.{}.{}", GetTargetName(_meta->GetSide()), reinterpret_cast<uintptr_t>(this)).str();
    auto* managed_context_name = mono_string_new(domain, context_name.c_str());
    auto* managed_assembly_paths = MakeManagedPathArray(domain, assembly_paths);
    auto* managed_entry_paths = MakeManagedPathArray(domain, entry_assembly_paths);

    if (managed_context_name == nullptr) {
        throw ScriptSystemException("Can't create Managed load-context name");
    }

    const ActiveBackendScope active_backend {this};
    void* create_args[] = {managed_context_name, managed_assembly_paths, managed_entry_paths};
    MonoObject* exception = nullptr;
    MonoObject* load_scope = mono_runtime_invoke(create_method, nullptr, create_args, &exception);
    ThrowIfManagedException(exception, "Managed load-context creation failed");

    if (load_scope == nullptr) {
        throw ScriptSystemException("Managed load-context host returned a null scope");
    }

    _managedHostImage = host_image;
    _loadScopeGcHandle = mono_gchandle_new(load_scope, false);

    if (_loadScopeGcHandle == 0) {
        throw ScriptSystemException("Can't root Managed load-context scope");
    }

    void* get_entries_args[] = {load_scope};
    exception = nullptr;
    auto* entry_assembly_objects = reinterpret_cast<MonoArray*>(mono_runtime_invoke(get_entries_method, nullptr, get_entries_args, &exception));
    ThrowIfManagedException(exception, "Managed entry assembly query failed");

    if (entry_assembly_objects == nullptr || mono_array_length(entry_assembly_objects) != entry_assembly_paths.size()) {
        throw ScriptSystemException("Managed load-context returned an invalid entry assembly list");
    }

    vector<nptr<void>> entry_assemblies;
    entry_assemblies.reserve(entry_assembly_paths.size());

    for (size_t i = 0; i < entry_assembly_paths.size(); i++) {
        auto* reflection_assembly = mono_array_get(entry_assembly_objects, MonoReflectionAssembly*, i);
        auto* assembly = reflection_assembly != nullptr ? mono_reflection_assembly_get_assembly(reflection_assembly) : nullptr;

        if (assembly == nullptr) {
            throw ScriptSystemException("Managed load-context returned a null entry assembly", entry_assembly_paths[i].string());
        }

        entry_assemblies.emplace_back(assembly);
    }

    return entry_assemblies;
}

void ManagedScriptBackend::ReleaseLoadScope() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    const uint32_t load_scope_handle = _loadScopeGcHandle;
    _loadScopeGcHandle = 0;

    if (load_scope_handle == 0) {
        _managedHostImage = nullptr;
        return;
    }

    try {
        auto* domain = static_cast<MonoDomain*>(_domain.get());
        auto* host_image = static_cast<MonoImage*>(_managedHostImage.get());
        auto* load_scope = mono_gchandle_get_target(load_scope_handle);

        if (domain != nullptr && host_image != nullptr && load_scope != nullptr && mono_thread_attach(domain) != nullptr) {
            auto* host_class = mono_class_from_name(host_image, MANAGED_HOST_NAMESPACE.data(), MANAGED_HOST_CLASS_NAME.data());
            auto* release_method = host_class != nullptr ? mono_class_get_method_from_name(host_class, "ReleaseLoadScope", 1) : nullptr;

            if (release_method == nullptr) {
                WriteLog("Managed load-context release method not found");
            }
            else {
                const ActiveBackendScope active_backend {this};
                void* release_args[] = {load_scope};
                MonoObject* exception = nullptr;
                mono_runtime_invoke(release_method, nullptr, release_args, &exception);

                if (exception != nullptr) {
                    WriteLog("Managed load-context release failed: {}", ManagedObjectToString(exception));
                }
            }
        }
    }
    catch (const std::exception& ex) {
        WriteLog("Managed load-context release failed: {}", ex.what());
    }
    catch (...) {
        WriteLog("Managed load-context release failed with an unknown exception");
    }

    mono_gchandle_free(load_scope_handle);
    _managedHostImage = nullptr;
}

ManagedScriptBackend::~ManagedScriptBackend()
{
    FO_STACK_TRACE_ENTRY();

    ReleaseAliveFlag();

    for (const uint32_t gc_handle : _globalFuncGcHandles) {
        if (gc_handle != 0) {
            mono_gchandle_free(gc_handle);
        }
    }

    _globalFuncGcHandles.clear();
    _globalFuncs.clear();
    _images.clear();
    ReleaseLoadScope();

    // Mono VM state is process-wide. Server/client/mapper backends may coexist
    // in one process, so shutdown is left to process teardown.
    _domain = nullptr;
}

void ManagedScriptBackend::AddManagedGlobalFunc(unique_ptr<ScriptFuncDesc> desc, uint32_t gc_handle)
{
    FO_STACK_TRACE_ENTRY();

    if (_scriptSys) {
        _scriptSys->AddGlobalScriptFunc(desc.get());
    }

    _globalFuncs.emplace_back(std::move(desc));
    _globalFuncGcHandles.emplace_back(gc_handle);
}

void ManagedScriptBackend::InvokeInitializator(void* assembly, const char* method_name)
{
    FO_STACK_TRACE_ENTRY();

    const ActiveBackendScope active_backend {this};

    auto* domain = GetDomainOrThrow(_domain.get());

    if (mono_thread_attach(domain) == nullptr) {
        throw ScriptSystemException("Failed to attach native thread to Managed runtime domain");
    }

    auto* massembly = static_cast<MonoAssembly*>(assembly);
    FO_VERIFY_AND_THROW(massembly, "Managed assembly is null");

    auto* image = mono_assembly_get_image(massembly);

    if (image == nullptr) {
        throw ScriptSystemException("Managed image is null for loaded assembly");
    }

    auto* init_class = mono_class_from_name(image, "FOnline", "Initializator");

    if (init_class == nullptr) {
        return;
    }

    auto* init_method = mono_class_get_method_from_name(init_class, method_name, 0);

    if (init_method == nullptr) {
        return;
    }

    MonoObject* exception = nullptr;
    mono_runtime_invoke(init_method, nullptr, nullptr, &exception);

    if (exception != nullptr) {
        string exception_text = "Managed initialization exception";

        if (auto* exception_str = mono_object_to_string(exception, nullptr); exception_str != nullptr) {
            if (auto* exception_utf8 = mono_string_to_utf8(exception_str); exception_utf8 != nullptr) {
                exception_text = exception_utf8;
                mono_free(exception_utf8);
            }
        }

        throw ScriptSystemException("Managed initializator failed", exception_text);
    }
}

void ManagedScriptBackend::RegisterMetadata(ptr<EngineMetadata> meta)
{
    FO_STACK_TRACE_ENTRY();

    _meta = meta;
    // The embedding engine is both the metadata and the script system (multiple inheritance); grab the script
    // system so managed global funcs can be registered into the cross-backend func map
    // (mirrors AngelScriptBackend acquiring its `_scriptSys` from the metadata).
    _scriptSys = dynamic_cast<ScriptSystem*>(meta.get());
}

auto ManagedScriptBackend::GetGlobalEntity() const noexcept -> Entity*
{
    FO_NO_STACK_TRACE_ENTRY();

    return dynamic_cast<Entity*>(_meta.get_no_const());
}

void ManagedScriptBackend::LoadAssemblies(const FileSystem& resources, string_view bake_output_dir)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_meta, "Engine metadata is not registered");
    FO_VERIFY_AND_THROW(_scriptSys, "Script system is not available");

    if (!_domain) {
        auto* domain = mono_get_root_domain();

        if (domain == nullptr) {
            ConfigureManagedRuntime();

#if FO_WINDOWS
            // Catch2 owns the top-level SEH filter while a unit-test session is active. Mono keeps its own
            // filter installed even with crash chaining enabled, so preserve the test host's filter across
            // the one-time runtime initialization. Production keeps Mono's chained crash filter.
            const bool preserve_test_exception_filter = IsTestingInProgress;
            LPTOP_LEVEL_EXCEPTION_FILTER test_exception_filter = nullptr;

            if (preserve_test_exception_filter) {
                test_exception_filter = SetUnhandledExceptionFilter(nullptr);
                SetUnhandledExceptionFilter(test_exception_filter);
            }

            auto restore_test_exception_filter = scope_exit([preserve_test_exception_filter, test_exception_filter]() noexcept {
                if (preserve_test_exception_filter) {
                    SetUnhandledExceptionFilter(test_exception_filter);
                }
            });
#endif

            domain = mono_jit_init_version("FOnlineManaged", "v4.0.30319");

            if (domain == nullptr) {
                throw ScriptSystemException("Failed to initialize Managed runtime domain");
            }
        }

        _domain = domain;
    }

    auto* domain = GetDomainOrThrow(_domain.get());

    if (mono_thread_attach(domain) == nullptr) {
        throw ScriptSystemException("Failed to attach native thread to Managed runtime domain");
    }

    RegisterInternalCalls();

    const auto target_name = GetTargetName(_meta->GetSide());
    CreateAliveFlag();

    const auto resource_assemblies = CollectAssemblyResources(resources, target_name);
    const auto restored_assembly_paths = RestoreAssemblyResources(resource_assemblies);

    vector<std::filesystem::path> assembly_paths;
    vector<std::filesystem::path> entry_assembly_paths;
    optional<std::filesystem::path> host_assembly_path;

    const auto append_assembly_path = [&](const std::filesystem::path& assembly_path) {
        const string file_name = strex("{}", assembly_path.filename().string()).str();

        if (IsManagedHostAssemblyFileName(file_name)) {
            if (host_assembly_path.has_value() && *host_assembly_path != assembly_path) {
                throw ScriptSystemException("Multiple Managed load-context host assemblies found");
            }

            host_assembly_path = assembly_path;
            return;
        }

        assembly_paths.emplace_back(assembly_path);

        if (IsManagedEntryAssemblyFileName(file_name, target_name)) {
            entry_assembly_paths.emplace_back(assembly_path);
        }
    };

    for (const ManagedAssemblyResource& resource : resource_assemblies) {
        if (const auto it = restored_assembly_paths.find(resource.ResourcePath); it != restored_assembly_paths.end()) {
            append_assembly_path(it->second);
        }
    }

    // Bake-time fallback: a validation engine (proto/map/dialog baker) restores managed to reflect over script
    // funcs, but the assemblies the managed baker just compiled live on disk under the bake output and are not
    // yet mounted into the resource FileSystem it sees. Scan the bake output tree for the entry assembly.
    if (entry_assembly_paths.empty() && !bake_output_dir.empty()) {
        assembly_paths.clear();
        entry_assembly_paths.clear();
        host_assembly_path.reset();

        for (const std::filesystem::path& assembly_path : CollectBakeOutputAssemblyPaths(bake_output_dir, target_name)) {
            append_assembly_path(assembly_path);
        }
    }

    size_t loaded_count = 0;

    if (!entry_assembly_paths.empty()) {
        if (!host_assembly_path.has_value()) {
            throw ScriptSystemException("Managed load-context host assembly not found", string(MANAGED_HOST_ASSEMBLY_FILE_NAME));
        }

        const vector<nptr<void>> entry_assemblies = CreateLoadScope(*host_assembly_path, assembly_paths, entry_assembly_paths);

        for (const nptr<void>& entry_assembly : entry_assemblies) {
            auto* assembly = static_cast<MonoAssembly*>(entry_assembly.get_no_const());

            auto* image = mono_assembly_get_image(assembly);

            if (image == nullptr) {
                throw ScriptSystemException("Managed image is null for loaded entry assembly");
            }

            _images.emplace_back(image);
            InvokeInitializator(assembly, "InitializeEarly");

            auto init_func = SafeAlloc::MakeUnique<ScriptFuncDesc>();
            init_func->Call = [this, assembly](FuncCallData& call) {
                FO_STACK_TRACE_ENTRY();

                ignore_unused(call);
                InvokeInitializator(assembly, "Initialize");
            };
            unique_del_nptr<ScriptFuncDesc> init_func_desc = make_unique_del_ptr(std::move(init_func).release(), [](ScriptFuncDesc* desc) { delete desc; });
            _scriptSys->AddInitFunc(ScriptFunc<void>(std::move(init_func_desc)), 0);

            loaded_count++;
        }
    }

    if (loaded_count == 0u) {
        WriteLog("No Managed assemblies found for target '{}', skip", target_name);
    }

    WriteLog("Managed backend initialized, target '{}', loaded assemblies {}", target_name, loaded_count);
}

void ManagedScriptBackend::BindRequiredStuff()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_meta, "Engine metadata is not registered");
}

FO_END_NAMESPACE

#endif
