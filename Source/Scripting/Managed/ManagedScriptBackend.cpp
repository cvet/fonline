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
#include "ManagedInteropAbi.h"
#include "ManagedPInvokeTable.h"
#include "ManagedRuntime.h"
#include "Platform.h"
#include "Properties.h"
#include "RemoteCallWire.h"
#include "Settings.h"

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
#include <mono/metadata/mono-debug.h>
#include <mono/metadata/mono-gc.h>
#include <mono/metadata/object.h>
#include <mono/metadata/reflection.h>
#include <mono/metadata/threads.h>
#include <mono/utils/mono-publib.h>
FO_DISABLE_WARNINGS_POP()

// The published embedding headers omit this exported API. The unbalanced pair permits a worker to stay
// registered with Mono while it is parked outside managed code; see Docs/Scripting.md
extern "C" void* mono_threads_enter_gc_safe_region_unbalanced(void** stack_data);
extern "C" void mono_threads_exit_gc_safe_region_unbalanced(void* cookie, void** stack_data);

// eglib copies the vtable; the public setter still returns TRUE when ENABLE_OVERRIDABLE_ALLOCATORS is off
struct MonoEglibMemVTable
{
    void* (*malloc)(size_t);
    void* (*realloc)(void*, size_t);
    void (*free)(void*);
    void* (*calloc)(size_t, size_t);
};
extern "C" void monoeg_g_mem_get_vtable(MonoEglibMemVTable* vtable);

#include "WinApiUndef.inc"

// A Mono unmanaged thunk and an UnmanagedCallersOnly entry without CallConvs use the platform default calling
// convention, and only Windows x86 tells it apart from the C one
#if FO_WINDOWS
#define FO_MANAGED_ENTRY_CALLCONV __stdcall
#else
#define FO_MANAGED_ENTRY_CALLCONV
#endif

#if FO_WEB
// No public Mono header declares these: with the interpreter built as its own archive, mini carries only
// stubs and the embedder installs the real callbacks itself, exactly as dotnet's own browser host does
extern "C" void mono_ee_interp_init(const char* opts);
extern "C" void mono_icall_table_init();
extern "C" void* mono_wasm_interp_to_native_callback(char* cookie);
extern "C" void mono_wasm_install_interp_to_native_callback(void* (*cb)(char*));
extern "C" void mono_marshal_ilgen_init();
extern "C" void mono_method_builder_ilgen_init();
extern "C" void mono_sgen_mono_ilgen_init();
#endif

FO_BEGIN_NAMESPACE

#if FO_WINDOWS
constexpr char MANAGED_ASSEMBLY_PATH_SEPARATOR = ';';
#else
constexpr char MANAGED_ASSEMBLY_PATH_SEPARATOR = ':';
#endif
constexpr string_view MANAGED_HOST_ASSEMBLY_FILE_NAME = "FOnline.ManagedHost.dll";
constexpr string_view MANAGED_HOST_NAMESPACE = "FOnline.ManagedHost";
constexpr string_view MANAGED_HOST_CLASS_NAME = "ManagedLoadContextHost";

// The root domain becomes visible before Mono finishes initializing its core classes. Serialize the
// check-and-initialize sequence so another engine cannot attach to a partially initialized runtime
static mutex ManagedRuntimeInitLocker;

// Embedded Mono can overlap normal execution across load contexts, but concurrent context loading corrupts its
// loader state. Serialize only assembly loading; managed engines still run in parallel
static mutex ManagedAssemblyLoadLocker;

// Thread-affine active backend: threads are partitioned by engine ownership, so the thread-local slot never observes
// a foreign engine
static thread_local nptr<ManagedScriptBackend> ActiveBackend {};

// Diagnostic counts of the calling thread; the interop probe switches them on for its own thread only, so every
// other thread pays one thread-local flag test at the few sites that count. They describe a thread, not an engine
struct ManagedInteropThreadCounters
{
    bool Enabled {};
    uint64_t GcHandles {};
    uint64_t MetadataLookups {};
    uint64_t ManagedObjects {};
};

static thread_local ManagedInteropThreadCounters InteropThreadCounters {};

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

// Native workers normally detach after each managed entry. Recurring frame workers instead retain one attachment
// and park it GC-safe between pumps; see Docs/Scripting.md
enum class ManagedThreadAttachmentMode
{
    PreserveExisting,
    AdoptExisting,
    CacheForThread,
};

class ManagedThreadAttachmentCache final
{
public:
    ManagedThreadAttachmentCache() = default;
    ManagedThreadAttachmentCache(const ManagedThreadAttachmentCache&) = delete;
    ManagedThreadAttachmentCache(ManagedThreadAttachmentCache&&) noexcept = delete;
    auto operator=(const ManagedThreadAttachmentCache&) = delete;
    auto operator=(ManagedThreadAttachmentCache&&) noexcept = delete;

    ~ManagedThreadAttachmentCache()
    {
        FO_NO_STACK_TRACE_ENTRY();

        if (_thread) {
            FO_STRONG_ASSERT(_scopeDepth == 0, "Managed thread attachment cache destroyed inside an active scope");

            if (_thread == mono_thread_current()) {
                Unpark();
                mono_thread_detach(_thread.get());
            }
        }
    }

    [[nodiscard]] auto IsAttached() const noexcept -> bool { return !!_thread; }

    // Detaches now instead of in the thread-local destructor, which on the main thread runs inside process exit,
    // after Mono's own threads were killed possibly holding the locks the detach takes
    void Release() noexcept
    {
        FO_NO_STACK_TRACE_ENTRY();

        if (!_thread || _scopeDepth != 0 || _thread != mono_thread_current()) {
            return;
        }

        Unpark();
        mono_thread_detach(_thread.get());
        _thread = nullptr;
        _domain = nullptr;
    }

    void Enter(ptr<MonoDomain> domain)
    {
        FO_STACK_TRACE_ENTRY();

        if (!_thread) {
            _thread = mono_thread_attach(domain.get());
            FO_VERIFY_AND_THROW(_thread, "Failed to attach native thread to Managed runtime domain");
            _domain = domain;
        }
        else {
            FO_VERIFY_AND_THROW(_domain == domain, "Managed worker attachment changed runtime domain");
            FO_VERIFY_AND_THROW(_thread == mono_thread_current(), "Managed worker attachment belongs to another thread");
        }

        if (_scopeDepth == 0) {
            Unpark();
        }

        _scopeDepth++;
    }

    void Leave() noexcept
    {
        FO_NO_STACK_TRACE_ENTRY();

        FO_STRONG_ASSERT(_scopeDepth > 0, "Managed thread attachment cache scope is unbalanced");
        _scopeDepth--;

        if (_scopeDepth == 0) {
            void* stack_data = nullptr;
            _gcSafeCookie = mono_threads_enter_gc_safe_region_unbalanced(&stack_data);
            _parked = true;
        }
    }

private:
    void Unpark() noexcept
    {
        FO_NO_STACK_TRACE_ENTRY();

        if (_parked) {
            void* stack_data = nullptr;
            mono_threads_exit_gc_safe_region_unbalanced(_gcSafeCookie.get(), &stack_data);
            _gcSafeCookie = nullptr;
            _parked = false;
        }
    }

    nptr<MonoDomain> _domain {};
    nptr<MonoThread> _thread {};
    nptr<void> _gcSafeCookie {};
    size_t _scopeDepth {};
    bool _parked {};
};

// All managed backends share the Mono root domain, so this stores native-thread state without engine semantics
static thread_local ManagedThreadAttachmentCache ManagedFrameWorkerThreadAttachment;

class ManagedThreadAttachment final
{
public:
    explicit ManagedThreadAttachment(ptr<MonoDomain> domain, ManagedThreadAttachmentMode mode = ManagedThreadAttachmentMode::PreserveExisting)
    {
        FO_STACK_TRACE_ENTRY();

        if (ManagedFrameWorkerThreadAttachment.IsAttached()) {
            ManagedFrameWorkerThreadAttachment.Enter(domain);
            _usesWorkerCache = true;
            return;
        }

        nptr<MonoThread> current_thread = mono_thread_current();

        if (!current_thread) {
            if (mode == ManagedThreadAttachmentMode::CacheForThread) {
                ManagedFrameWorkerThreadAttachment.Enter(domain);
                _usesWorkerCache = true;
            }
            else {
                _attachedThread = mono_thread_attach(domain.get());
                FO_VERIFY_AND_THROW(_attachedThread, "Failed to attach native thread to Managed runtime domain");
            }
        }
        else if (mode == ManagedThreadAttachmentMode::AdoptExisting) {
            _attachedThread = current_thread;
        }
    }

    ManagedThreadAttachment(const ManagedThreadAttachment&) = delete;
    ManagedThreadAttachment(ManagedThreadAttachment&&) noexcept = delete;
    auto operator=(const ManagedThreadAttachment&) = delete;
    auto operator=(ManagedThreadAttachment&&) noexcept = delete;

    ~ManagedThreadAttachment()
    {
        FO_NO_STACK_TRACE_ENTRY();

        if (_usesWorkerCache) {
            ManagedFrameWorkerThreadAttachment.Leave();
        }
        else if (_attachedThread) {
            mono_thread_detach(_attachedThread.get());
        }
    }

private:
    nptr<MonoThread> _attachedThread {};
    bool _usesWorkerCache {};
};

// A native call into managed script code. Entries chain per thread, innermost first, so the stack-trace provider can
// give every run of managed frames the native stack it was entered from; see Docs/Debugging.md
class ManagedScriptEntryScope final
{
public:
    // Not inlined, so the birth capture can skip exactly the constructor frame
    FO_NO_INLINE explicit ManagedScriptEntryScope(nptr<MonoMethod> method) noexcept;
    ManagedScriptEntryScope(const ManagedScriptEntryScope&) = delete;
    ManagedScriptEntryScope(ManagedScriptEntryScope&&) noexcept = delete;
    auto operator=(const ManagedScriptEntryScope&) = delete;
    auto operator=(ManagedScriptEntryScope&&) noexcept = delete;
    ~ManagedScriptEntryScope();

    [[nodiscard]] static auto GetInnermostRunning() noexcept -> nptr<ManagedScriptEntryScope>;
    [[nodiscard]] auto GetMethod() const noexcept -> nptr<MonoMethod> { return _method; }
    [[nodiscard]] auto GetNextRunning() const noexcept -> nptr<ManagedScriptEntryScope>;

    void CopyBirthFrames(stack_trace::script_layer& layer) const noexcept;
    void AppendBirthRuntimeFrames(MonoDomain* domain, stack_trace::script_layer& layer) const;
    void Leave() noexcept { _running = false; }
    void SetCrossedNativeException(std::exception_ptr exception, MonoString* message);
    auto FindCrossedNativeException(MonoString* message) const noexcept -> std::exception_ptr;

private:
    nptr<MonoMethod> _method {};
    nptr<ManagedScriptEntryScope> _parent {};
    bool _running {true};
    // Left uninitialized: the capture fills the first _birthFrameCount slots and nothing reads past them, while
    // zeroing a kilobyte on every script entry is measurable
    std::array<stack_trace::native_frame_address, stack_trace::MAX_NATIVE_FRAMES> _birthFrames;
    uint32_t _birthFrameCount {};
    bool _birthTruncated {};
    vector<pair<uint32_t, std::exception_ptr>> _crossedNativeExceptions {};
};

// Per-thread chain of script entries; threads are partitioned by engine ownership, so the slot never observes a
// foreign engine
static thread_local nptr<ManagedScriptEntryScope> CurrentScriptEntry {};

// A managed exception reduced to what a native stack trace carries
struct ManagedExceptionDescription
{
    string Summary {};
    std::exception_ptr NativeException {};
    vector<pair<ptr<MonoMethod>, int32_t>> Frames {};
};

struct ManagedStackWalk
{
    nptr<ManagedScriptEntryScope> Entry {};
    nptr<MonoMethod> OutermostMethod {};
    stack_trace::script_layer Layer {};
    vector<stack_trace::script_layer> Layers {};
};

static void ReleaseManagedGcHandle(nptr<MonoDomain> domain, uint32_t& handle) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (handle == 0) {
        return;
    }

    safe_call([&] {
        FO_VERIFY_AND_THROW(domain, "Managed runtime domain is unavailable during GC handle release");
        ManagedThreadAttachment managed_thread {domain};
        mono_gchandle_free(handle);
        handle = 0;
    });
}

// Forward declarations of the internal marshaling/bridge helper structs (defined below).
struct ManagedScalarValue;
struct ManagedObjectRoot;
struct ManagedArrayBridgeData;
struct ManagedDictBridgeData;
struct ManagedCallbackBridgeData;
struct ManagedDataAccessor;
struct ManagedNativeValue;
struct ManagedEventSubscription;
struct ManagedAbiEventRuntime;
struct ManagedAssemblyResource;
struct ManagedWrapperClassEntry;
struct ManagedDynamicFieldAccessors;
struct ManagedCallbackPlan;

// Static free-function forward declarations, ordered high-level -> low-level

// Backend registry and active-backend lifecycle
static auto GetActiveBackendOrThrow() -> ptr<ManagedScriptBackend>;
static auto GetActiveEntityManagerOrThrow() -> ptr<EntityManagerApi>;
static auto GetTargetName(EngineSideKind side) -> string_view;
static auto GetBackendSettings(ptr<ManagedScriptBackend> backend) -> nptr<GlobalSettings>;

// Script entries, exceptions and stack traces
static auto InvokeManagedScript(MonoMethod* method, MonoObject* obj, void** args, string_view context) -> MonoObject*;
static void InvokeManagedScriptDelegate(MonoObject* delegate_obj, string_view context);
static void RunManagedScriptEntry(ptr<ManagedScriptBackend> backend, ptr<BaseEngine> engine, const function<MonoObject*()>& get_entry, const function<void()>& callback);
static void ReportManagedScriptOverrun(ptr<ManagedScriptBackend> backend, ptr<BaseEngine> engine, timespan total_duration, timespan lock_wait_duration, const function<MonoObject*()>& get_entry);
static auto DescribeManagedScriptEntry(ptr<ManagedScriptBackend> backend, const function<MonoObject*()>& get_entry) -> string;
static void NativeReportException(MonoString* summary, MonoString* native_error, MonoArray* frames);
static auto MakeManagedNativeError(const std::exception& ex) -> MonoString*;
static void CollectManagedScriptStackLayers(const stack_trace::data& st, std::vector<stack_trace::script_layer>& out_layers) noexcept;
static auto CollectManagedStackFrame(MonoMethod* method, int32_t native_offset, int32_t il_offset, mono_bool managed, void* data) -> mono_bool;
static auto DescribeManagedException(MonoObject* exception, nptr<ManagedScriptEntryScope> entry) -> ManagedExceptionDescription;
static auto ReadManagedExceptionFrames(MonoArray* frames) -> vector<pair<ptr<MonoMethod>, int32_t>>;
static auto MakeManagedExceptionLayer(const vector<pair<ptr<MonoMethod>, int32_t>>& frames) -> stack_trace::script_layer;
static auto MakeManagedStackFrame(ptr<MonoMethod> method, int32_t il_offset) -> optional<stack_trace::frame>;
static void AppendRuntimeNativeFrames(MonoDomain* domain, span<const stack_trace::native_frame_address> frames, stack_trace::script_layer& layer);
static auto IsManagedRuntimeInvokeWrapper(MonoMethod* method) -> bool;

// Native ABI: logging, hashing, backend and prototype queries
static void NativeLog(MonoString* text);
static auto NativeGetHash(MonoString* text) -> void*;
static auto NativeGetHashStr(void* value) -> MonoString*;
static auto NativeGetHashStrFromHash(uint64_t value) -> MonoString*;
static auto NativeResolveHash(uint64_t hash) -> void*;
static auto NativeGetBackendAliveFlag() -> MonoArray*;
static auto NativeGetBackend() -> void*;
static auto NativeRunScriptContinuation(MonoObject* continuation) -> MonoString*;
static auto NativeGetProtoEntity(MonoString* type_name, void* proto_id) -> void*;
static auto NativeCheckProtoEntity(MonoString* type_name, void* proto_id) -> mono_bool;
static auto NativeGetProtoEntityCount(MonoString* type_name) -> int32_t;
static auto NativeGetProtoEntityAt(MonoString* type_name, int32_t index) -> void*;

// Native ABI: entity lifetime and property access
static void NativeAddRefEntity(void* entity_ptr);
static void NativeReleaseEntity(void* entity_ptr);
static auto FindCoreScriptMethod(ptr<const ManagedScriptBackend> backend, const char* class_name, const char* method_name, int32_t param_count) -> MonoMethod*;
static auto InvokeEntityWrapperTrackerCollect(MonoMethod* method, int32_t pass_limit) -> int32_t;
static auto InvokeEntityWrapperTrackerDump(MonoMethod* method) -> string;
static auto NativeIsEntityDestroyed(void* entity_ptr) -> mono_bool;
static auto NativeIsEntityDestroying(void* entity_ptr) -> mono_bool;
static auto NativeGetEntityName(void* entity_ptr) -> MonoString*;
static auto NativeGetEntityId(void* entity_ptr) -> int64_t;
static auto NativeGetEntityProtoId(void* entity_ptr) -> void*;
static auto NativeGetEntityValueAsInt(void* entity_ptr, int32_t prop_index, MonoString** error) -> int32_t;
static auto NativeSetEntityValueAsInt(void* entity_ptr, int32_t prop_index, int32_t value) -> MonoString*;
static auto NativeGetEntityValueAsAny(void* entity_ptr, int32_t prop_index, MonoString** error) -> MonoString*;
static auto NativeSetEntityValueAsAny(void* entity_ptr, int32_t prop_index, MonoString* value) -> MonoString*;

// Native ABI: hex direction helpers
static auto NativeHdirToMdir(int8_t dir) -> int16_t;
static auto NativeMdirHex(int16_t angle) -> int8_t;
static auto NativeMdirRotateHex(int16_t angle, int32_t steps) -> int16_t;
static auto NativeMdirReverse(int16_t angle) -> int16_t;

// Native ABI: inner entities
static auto NativeCreateInnerEntity(void* holder_ptr, int32_t entry_id, void* proto_id) -> void*;
static auto NativeHasInnerEntities(void* holder_ptr, int32_t entry_id) -> mono_bool;
static auto NativeGetInnerEntity(void* holder_ptr, int32_t entry_id, int64_t id) -> void*;
static auto NativeFillInnerEntities(void* holder_ptr, int32_t entry_id, void** buffer, int32_t capacity, MonoString** error) -> int32_t;
static auto NativeGetAndResetInnerEntityVisits() -> int64_t;

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
static auto NativeGetSettingValue(int32_t setting_id, void* value, int32_t size) -> MonoString*;

// Native ABI: events, properties, methods and remote calls
static void NativeSubscribeEvent(int32_t event_id, void* entity_ptr, MonoObject* handler, mono_bool has_explicit_result, int32_t priority);
static void NativeUnsubscribeEvent(int32_t event_id, void* entity_ptr, MonoObject* handler);
static void NativeUnsubscribeAllEvents(int32_t event_id, void* entity_ptr);
static auto NativeFireEventBoxed(int32_t event_id, void* entity_ptr, MonoArray* args, MonoString** error) -> int32_t;
static auto NativeFireEventIndexed(int32_t event_id, void* entity_ptr, void* frame, int32_t frame_size, MonoString** error) -> int32_t;
static auto NativeGetProperty(void* entity_ptr, int32_t prop_index, MonoString** error) -> MonoObject*;
static auto NativeGetPropertyValue(void* entity_ptr, int32_t prop_index, void* value, int32_t size) -> MonoString*;
static auto NativeSetPropertyValue(void* entity_ptr, int32_t prop_index, void* value, int32_t size) -> MonoString*;
static auto NativeGetPropertyArray(void* entity_ptr, int32_t prop_index, void* buffer, int32_t capacity, int32_t element_size, int32_t* size) -> MonoString*;
static auto NativeSetPropertyArray(void* entity_ptr, int32_t prop_index, void* buffer, int32_t size, int32_t element_size) -> MonoString*;
static auto NativeSetProperty(void* entity_ptr, int32_t prop_index, MonoObject* value) -> MonoString*;
static void NativeSetPropertyGetter(MonoString* owner_type, MonoString* property_name, MonoObject* getter);
static void NativeAddPropertySetter(MonoString* owner_type, MonoString* property_name, MonoObject* setter);
static void NativeAddPropertySetterWithProperty(MonoString* owner_type, MonoString* property_name, MonoObject* setter);
static void NativeAddPropertyDeferredSetter(MonoString* owner_type, MonoString* property_name, MonoObject* setter);
static auto NativeCallMethodBoxed(int32_t method_id, void* entity_ptr, MonoArray* args, MonoString** error) -> MonoObject*;
static auto NativeCallMethodIndexed(int32_t method_id, void* entity_ptr, void* frame, int32_t frame_size) -> MonoString*;
static auto NativeBindAbi(uint64_t hash, int32_t method_count, int32_t event_count, int32_t setting_count, int32_t inner_count) -> MonoString*;
static auto NativeInvokeScriptFuncStatus(MonoString* func_name, MonoArray* args) -> int32_t;
static void NativeRegisterGlobalScriptFunc(MonoString* full_name, MonoString* attr_name, MonoArray* param_type_names, MonoString* ret_type_name, MonoObject* handler);
static void NativeRegisterRemoteCallHandler(MonoString* name_str, int32_t param_count, MonoObject* handler);
static void NativeSendRemoteCall(MonoObject* caller, MonoString* name_str, MonoArray* args_array);
static void NativeLoopbackRemoteCall(MonoObject* caller, MonoString* name_str, MonoArray* args_array);

// Internal-call registration
static void RegisterInternalCalls();

// Settings access helpers
static auto GetSettingValueAsString(MonoString* name) -> string;
static void SetSettingValueFromString(nptr<GlobalSettings> settings, string_view setting_name, string value);
static void SetSettingValueFromString(MonoString* name, string value);

// Property getter/setter callback bridge
static auto InvokeManagedCallbackHandler(ptr<ManagedScriptBackend> backend, MonoObject* handler, MonoArray* args_array) -> MonoObject*;
static auto ResolveVirtualPropertyForCallback(ptr<ManagedScriptBackend> backend, MonoString* owner_type, MonoString* property_name, bool require_virtual, bool require_marshalable_value) -> ptr<const Property>;
static auto MakeManagedCallbackPlan(ptr<ManagedScriptBackend> backend, const ComplexTypeDesc& ret, vector<ComplexTypeDesc> args) -> shared_ptr<ManagedCallbackPlan>;
static auto FindCallbackAdapter(ptr<ManagedScriptBackend> backend, string_view key) -> nptr<MonoMethod>;
static void DispatchManagedCallback(ptr<ManagedScriptBackend> backend, uint32_t handler_handle, const ManagedCallbackPlan& plan, FuncCallData& call);
static void DispatchManagedCallbackInContext(ptr<ManagedScriptBackend> backend, uint32_t handler_handle, const ManagedCallbackPlan& plan, FuncCallData& call);
static auto TryDispatchManagedCallbackTyped(ptr<ManagedScriptBackend> backend, uint32_t handler_handle, const ManagedCallbackPlan& plan, FuncCallData& call) -> bool;
static void DispatchManagedCallbackBoxed(ptr<ManagedScriptBackend> backend, uint32_t handler_handle, const ManagedCallbackPlan& plan, FuncCallData& call);
static auto NativeGetAndResetTypedCallbackDispatches() -> int64_t;
static auto NativeGetAndResetBoxedCallbackDispatches() -> int64_t;
static auto NativeProbeCallbackTransport(MonoObject* handler, int32_t mode, int32_t iterations, void* uco_entry, int32_t registration_id, MonoString** error) -> int64_t;
static auto NativeReadInteropCounters(mono_bool enable, int64_t* gc_handles, int64_t* metadata_lookups, int64_t* managed_objects, int64_t* native_allocations, int64_t* native_bytes) -> mono_bool;
static void NativeProbeTransportScenario(MonoObject* handler, int32_t transport, int32_t adapter_kind, int32_t iterations, mono_bool external_thread, void* uco_entry, int32_t registration_id, int32_t* faults, MonoString** error);
static void CopyManagedCallbackReturnValue(ptr<ManagedScriptBackend> backend, const ComplexTypeDesc& type, MonoObject* value, FuncCallData& call);
static void CopyManagedCallbackByRefArg(ptr<ManagedScriptBackend> backend, const ComplexTypeDesc& type, MonoObject* value, ptr<void> arg_data);
static auto CreateManagedCallbackDesc(ptr<const ManagedCallbackBridgeData> callback) -> unique_del_nptr<ScriptFuncDesc>;
static auto BoxNativeCallValue(ptr<const ManagedScriptBackend> backend, const ComplexTypeDesc& type, void* data, const DataAccessor* accessor) -> MonoObject*;

// Event dispatch bridge
static void WriteBackManagedEventArg(ptr<ManagedScriptBackend> backend, const ComplexTypeDesc& type, MonoObject* value, void* dst);
static auto ResolveEventEntity(ptr<ManagedScriptBackend> backend, const ManagedAbiEventRuntime& entry, void* entity_ptr) -> nptr<Entity>;
static auto FindManagedEventSubscription(ptr<ManagedScriptBackend> backend, ptr<const Entity> entity, string_view event_name, MonoObject* handler) -> optional<uintptr_t>;
static auto GetManagedEventSubscriptionOwner(ptr<const ManagedScriptBackend> backend) noexcept -> uintptr_t;
static auto DispatchManagedEvent(shared_ptr<ManagedEventSubscription> subscription, FuncCallData& call) -> Entity::EventResult;
static auto DispatchManagedEventInContext(shared_ptr<ManagedEventSubscription> subscription, FuncCallData& call) -> Entity::EventResult;

// Remote-call argument marshaling
static auto SerializeManagedRemoteCallArgs(ptr<ManagedScriptBackend> backend, const vector<ArgDesc>& call_args, MonoArray* args_array, string_view name) -> vector<uint8_t>;
static void AppendRawBytes(vector<uint8_t>& data, const_span<uint8_t> bytes);
static void AppendAlignedRawBytes(vector<uint8_t>& data, const_span<uint8_t> bytes, size_t alignment);
template<typename T>
static void AppendRawValue(vector<uint8_t>& data, const T& value);
template<typename T>
static void AppendAlignedRawValue(vector<uint8_t>& data, const T& value, size_t alignment);

// Managed object creation and native<->managed values
static auto CreateHashObject(ptr<const ManagedScriptBackend> backend, const hstring& value) -> MonoObject*;
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
static auto GetManagedStructClass(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type) -> MonoClass*;
static auto GetManagedPropertyValue(ptr<const ManagedScriptBackend> backend, MonoObject* obj, ptr<const Property> field_prop) -> MonoObject*;
static void SetManagedPropertyValue(ptr<const ManagedScriptBackend> backend, MonoObject* obj, ptr<const Property> field_prop, MonoObject* value);
static auto ResolveDynamicRefTypeField(ptr<const ManagedScriptBackend> backend, MonoObject* obj, ptr<const Property> field_prop) -> ManagedDynamicFieldAccessors;

// Managed collections (list/dictionary/delegate)
static auto CreateManagedList(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& element_type) -> MonoObject*;
static auto GetManagedListCount(ptr<const ManagedScriptBackend> backend, MonoObject* list) -> size_t;
static auto GetManagedListItem(ptr<const ManagedScriptBackend> backend, MonoObject* list, size_t index) -> MonoObject*;
static void AddManagedListItem(ptr<const ManagedScriptBackend> backend, MonoObject* list, MonoObject* item);
static auto CreateManagedDictionary(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& key_type, const BaseTypeDesc& value_type) -> MonoObject*;
static auto CreateManagedDictionaryOfList(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& key_type, const BaseTypeDesc& element_type) -> MonoObject*;
static auto GetManagedDictionaryCount(ptr<const ManagedScriptBackend> backend, MonoObject* dictionary) -> size_t;
static auto GetManagedDictionaryKey(ptr<const ManagedScriptBackend> backend, MonoObject* dictionary, size_t index) -> MonoObject*;
static auto GetManagedDictionaryValue(ptr<const ManagedScriptBackend> backend, MonoObject* dictionary, size_t index) -> MonoObject*;
static void AddManagedDictionaryItem(ptr<const ManagedScriptBackend> backend, MonoObject* dictionary, MonoObject* key, MonoObject* value);
static auto GetManagedDelegateKey(ptr<const ManagedScriptBackend> backend, MonoObject* handler) -> string;

// Managed<->native conversion and boxing
static auto GetManagedClass(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& type) -> MonoClass*;
static auto InvokeNativeHelper(ptr<const ManagedScriptBackend> backend, const char* method_name, uint32_t args_count, void** args) -> MonoObject*;
static auto InvokeNativeBoolHelper(ptr<const ManagedScriptBackend> backend, const char* method_name, MonoObject* value) -> bool;
static auto ConvertManagedSimpleObjectToNative(ptr<ManagedScriptBackend> backend, const BaseTypeDesc& base_type, MonoObject* value, ManagedScalarValue& storage) -> void*;
static auto ConvertManagedObjectToNative(ptr<ManagedScriptBackend> backend, const ComplexTypeDesc& type, MonoObject* value, ManagedNativeValue& storage) -> void*;
static void ReconcileMutableDynamicRefTypeOwner(const ComplexTypeDesc& type, ManagedNativeValue& storage) noexcept;
static auto ManagedObjectClassMatches(MonoObject* value, MonoClass* expected_class) -> bool;
static auto ManagedObjectClassMatchesOrDerives(MonoObject* value, MonoClass* expected_class) -> bool;
static auto CanConvertManagedSimpleObjectToNative(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, MonoObject* value) -> bool;
static auto CanConvertManagedObjectToNative(ptr<const ManagedScriptBackend> backend, const ComplexTypeDesc& type, MonoObject* value) -> bool;
static auto BoxNativeSimpleValue(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, void* data) -> MonoObject*;
static void ValidateManagedEntityKind(const BaseTypeDesc& base_type, nptr<Entity> entity);
static void PropertyDataToValue(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& type, uint8_t* data);
static void ValueToPropertyData(const BaseTypeDesc& type, uint8_t* data);
static void ValidateManagedFrameHandle(const ManagedAbiSlot& slot, const ArgDesc& arg, const uint8_t* slot_data, string_view owner, string_view name);

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
static auto IsManagedBridgeDictionaryArrayValueType(const BaseTypeDesc& type) -> bool;
static auto IsManagedBridgeDictionaryProperty(ptr<const Property> prop) -> bool;
static auto IsDynamicManagedRefType(const BaseTypeDesc& base_type) -> bool;
static auto MakeManagedDynamicRefTypePropertyName(ptr<const Property> prop) -> string;

// Type/class and metadata resolution
static auto FindFOnlineClass(ptr<const ManagedScriptBackend> backend, string_view class_name) -> MonoClass*;
static auto FindNativeMethod(ptr<const ManagedScriptBackend> backend, const char* method_name, int32_t args_count) -> MonoMethod*;
static auto ResolveWrapperClass(ptr<const ManagedScriptBackend> backend, string_view type_name) -> ManagedWrapperClassEntry;
static auto FindWrapperClass(ptr<const ManagedScriptBackend> backend, string_view type_name) -> ManagedWrapperClassEntry;
static void BuildWrapperClassCache(ptr<ManagedScriptBackend> backend);
static auto GetPrimitiveClass(const BaseTypeDesc& type) -> MonoClass*;
static auto GetValueClass(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& type) -> MonoClass*;
static auto FindFieldInHierarchy(MonoClass* klass, const char* field_name) -> MonoClassField*;
static auto FindEntityTypeDesc(ptr<EngineMetadata> meta, string_view owner_type_name) -> nptr<const EntityTypeDesc>;
static auto FindRefTypeDesc(ptr<EngineMetadata> meta, string_view owner_type_name) -> nptr<const RefTypeDesc>;
static auto MakeManagedGlobalSimpleType(ptr<EngineMetadata> meta, string_view type_name) -> ComplexTypeDesc;

// Entity resolution and inner-entry helpers
static auto ResolveEntity(ptr<ManagedScriptBackend> backend, void* entity_ptr) -> ptr<Entity>;
static auto ResolveProtoEntityFromRawData(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, span<const uint8_t> raw_data) -> nptr<Entity>;
static auto ExtractProtoHashFromManagedEntity(MonoObject* value) -> hstring::hash_t;
static void ValidateManagedInnerEntity(ptr<const Entity> entity);
static auto CollectManagedInnerEntities(ptr<ManagedScriptBackend> backend, ptr<Entity> holder, hstring entry) -> vector<ptr<Entity>>;

// Extraction and hash primitives
static auto ExtractEntityPtr(MonoObject* obj) -> Entity*;
static auto ExtractRefPtr(MonoObject* obj) -> void*;
static auto ExtractNativeHstring(MonoObject* obj) -> hstring;
static auto ResolveManagedHashValue(ptr<const ManagedScriptBackend> backend, hstring::hash_t value) -> hstring;

// Assembly loading, runtime configuration and resource cache
static auto IsManagedEntryAssemblyFileName(string_view file_name, string_view target_name) -> bool;
static auto IsManagedHostAssemblyFileName(string_view file_name) noexcept -> bool;
static auto CollectAssemblyResources(const FileSystem& resources, string_view target_name) -> vector<ManagedAssemblyResource>;
static void AppendExistingAssemblyPath(vector<string>& paths, const std::filesystem::path& dir);
static auto BuildAssemblySearchPath(const std::filesystem::path& lib_dir) -> string;
static void SetEnvironmentVariableDefault(const char* name, const char* value);
static void ConfigureManagedRuntime(const std::filesystem::path& runtime_dir);
static void AddManagedAssemblyCacheByte(uint64_t& hash, uint8_t byte) noexcept;
static auto MakeManagedAssemblyCacheKey(const vector<ManagedAssemblyResource>& assembly_resources) noexcept -> string;
static auto IsSameManagedAssemblyCacheFile(const std::filesystem::path& disk_path, const_span<uint8_t> assembly_data) -> bool;
static auto RestoreAssemblyResources(const vector<ManagedAssemblyResource>& assembly_resources, string_view cache_dir) -> unordered_map<string, std::filesystem::path>;
static auto CollectBakeOutputAssemblyPaths(string_view bake_output_dir, string_view target_name) -> vector<std::filesystem::path>;

// Low-level Mono/string primitives
static auto GetDomainOrThrow(void* domain) -> MonoDomain*;
static auto MakeManagedPathArray(MonoDomain* domain, const vector<std::filesystem::path>& paths) -> MonoArray*;
static auto ToStringAndFree(MonoString* text) -> string;
static auto NewManagedGcHandle(MonoObject* obj, mono_bool pinned) -> uint32_t;
static void CountMetadataLookup() noexcept;
static void CountManagedObject() noexcept;
static auto ManagedObjectToString(MonoObject* obj) -> string;
static void ThrowIfManagedException(MonoObject* exception, string_view context, nptr<ManagedScriptEntryScope> entry = nullptr);

// Helper struct definitions

struct ManagedScalarValue
{
    alignas(std::max_align_t) std::array<uint8_t, PropertyRawData::LOCAL_BUF_SIZE> Local {};
    vector<uint8_t> Dynamic {};
    string Text {};
    any_t Any {};
    hstring Hash {};
    nptr<Entity> EntityPtr {};
    refcount_nptr<DynamicRefTypeInstance> DynamicRefType {};
    nptr<void> RefTypePtr {};

    [[nodiscard]] auto Alloc(const BaseTypeDesc& type) -> void*
    {
        FO_NO_STACK_TRACE_ENTRY();

        size_t size = type.Size;

        if (size <= Local.size()) {
            return Local.data();
        }

        Dynamic.resize(size);
        return Dynamic.data();
    }
};

// A GC handle preserves managed objects across native allocation and invocation boundaries
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
            _objectHandle = NewManagedGcHandle(object, 0);
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
    nptr<MonoDomain> Domain {};
    ComplexTypeDesc Type {};
    uint32_t Handler {};
    hstring Name {};

    ~ManagedCallbackBridgeData()
    {
        FO_STACK_TRACE_ENTRY();

        ReleaseManagedGcHandle(Domain, Handler);
    }
};

struct ManagedDataAccessor final : DataAccessor
{
    [[nodiscard]] auto GetBackendIndex() const noexcept -> int32_t override { return ScriptSystemBackend::MANAGED_BACKEND_INDEX; }

    [[nodiscard]] auto GetArraySize(ptr<void> data) const -> size_t override
    {
        FO_STACK_TRACE_ENTRY();

        auto array = data.reinterpret_as<ManagedArrayBridgeData>();
        return GetManagedListCount(array->Backend, array->GetObject());
    }

    [[nodiscard]] auto GetArrayElement(ptr<void> data, size_t index) const -> ptr<void> override
    {
        FO_STACK_TRACE_ENTRY();

        auto array = data.reinterpret_as<ManagedArrayBridgeData>();

        if (array->Elements.size() <= index) {
            array->Elements.resize(index + 1);
        }

        MonoObject* item = GetManagedListItem(array->Backend, array->GetObject(), index);
        return ConvertManagedSimpleObjectToNative(array->Backend, array->Type.BaseType, item, array->Elements[index]);
    }

    void ClearArray(ptr<void> data) const override
    {
        FO_STACK_TRACE_ENTRY();

        auto array = data.reinterpret_as<ManagedArrayBridgeData>();
        array->SetObject(CreateManagedList(array->Backend, array->Type.BaseType));
        array->Elements.clear();
    }

    void AddArrayElement(ptr<void> data, ptr<void> value) const override
    {
        FO_STACK_TRACE_ENTRY();

        auto array = data.reinterpret_as<ManagedArrayBridgeData>();
        MonoObject* item = BoxNativeSimpleValue(array->Backend, array->Type.BaseType, value.get());
        AddManagedListItem(array->Backend, array->GetObject(), item);
    }

    [[nodiscard]] auto GetDictSize(ptr<void> data) const -> size_t override
    {
        FO_STACK_TRACE_ENTRY();

        auto dict = data.reinterpret_as<ManagedDictBridgeData>();
        return GetManagedDictionaryCount(dict->Backend, dict->GetObject());
    }

    [[nodiscard]] auto GetDictElement(ptr<void> data, size_t index) const -> pair<ptr<void>, ptr<void>> override
    {
        FO_STACK_TRACE_ENTRY();

        auto dict = data.reinterpret_as<ManagedDictBridgeData>();
        FO_VERIFY_AND_THROW(dict->Type.KeyType, "Dictionary bridge has no key type");

        if (dict->Keys.size() <= index) {
            dict->Keys.resize(index + 1);
        }
        if (dict->Values.size() <= index) {
            dict->Values.resize(index + 1);
        }

        MonoObject* key = GetManagedDictionaryKey(dict->Backend, dict->GetObject(), index);
        ptr<void> native_key = ConvertManagedSimpleObjectToNative(dict->Backend, *dict->Type.KeyType, key, dict->Keys[index]);
        MonoObject* value = GetManagedDictionaryValue(dict->Backend, dict->GetObject(), index);
        return pair<ptr<void>, ptr<void>>(native_key, ConvertManagedSimpleObjectToNative(dict->Backend, dict->Type.BaseType, value, dict->Values[index]));
    }

    [[nodiscard]] auto GetCallback(ptr<void> data) const -> unique_del_nptr<ScriptFuncDesc> override
    {
        FO_STACK_TRACE_ENTRY();

        return CreateManagedCallbackDesc(data.reinterpret_as<ManagedCallbackBridgeData>());
    }

    void ClearDict(ptr<void> data) const override
    {
        FO_STACK_TRACE_ENTRY();

        auto dict = data.reinterpret_as<ManagedDictBridgeData>();
        FO_VERIFY_AND_THROW(dict->Type.KeyType, "Dictionary bridge has no key type");
        dict->SetObject(CreateManagedDictionary(dict->Backend, *dict->Type.KeyType, dict->Type.BaseType));
        dict->Keys.clear();
        dict->Values.clear();
    }

    void AddDictElement(ptr<void> data, ptr<void> key, ptr<void> value) const override
    {
        FO_STACK_TRACE_ENTRY();

        auto dict = data.reinterpret_as<ManagedDictBridgeData>();
        FO_VERIFY_AND_THROW(dict->Type.KeyType, "Dictionary bridge has no key type");
        ManagedObjectRoot managed_key;
        managed_key.SetObject(BoxNativeSimpleValue(dict->Backend, *dict->Type.KeyType, key.get()));
        MonoObject* managed_value = BoxNativeSimpleValue(dict->Backend, dict->Type.BaseType, value.get());
        AddManagedDictionaryItem(dict->Backend, dict->GetObject(), managed_key.GetObject(), managed_value);
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
    nptr<MonoDomain> Domain {};
    vector<ComplexTypeDesc> Args {};
    uint32_t Handler {};
    bool HasExplicitResult {};
    int32_t EventId {-1};
    bool UsesScalarFrame {};
    vector<ManagedAbiSlot> Slots {};
    uint16_t FrameSize {};
    nptr<MonoMethod> AdaptInvoke {};

    ~ManagedEventSubscription()
    {
        FO_STACK_TRACE_ENTRY();

        ReleaseManagedGcHandle(Domain, Handler);
    }
};

struct ManagedAbiMethodRuntime
{
    nptr<const MethodDesc> Method {};
    string Owner {};
    bool IsRefType {};
    bool UsesScalarFrame {};
    vector<ManagedAbiSlot> Args {};
    ManagedAbiSlot Ret {};
    uint16_t FrameSize {};
    uint16_t ResultOffset {};
};

struct ManagedAbiEventRuntime
{
    nptr<const EntityTypeDesc> Desc {};
    nptr<const EntityEventDesc> Event {};
    string Owner {};
    string Name {};
    bool IsGlobal {};
    bool UsesScalarFrame {};
    vector<ManagedAbiSlot> Args {};
    uint16_t FrameSize {};
};

struct ManagedAbiSettingRuntime
{
    string Name {};
    ManagedAbiValueKind Kind {};
    bool UsesTypedBridge {};
    nptr<const NumericSettingAccess> Builtin {};
    // Parsed copy of a project custom setting, valid while the custom map generation it was parsed at holds. Any
    // worker may fill it, so the value is stored before the generation that vouches for it
    std::atomic<uint64_t> CustomGeneration {std::numeric_limits<uint64_t>::max()};
    std::atomic<uint64_t> CustomValue {};
};

struct ManagedAbiInnerRuntime
{
    hstring Entry {};
    string Owner {};
    string TargetType {};
};

struct ManagedAbiRuntimeState
{
    uint64_t Hash {};
    bool Bound {};
    vector<ManagedAbiMethodRuntime> Methods {};
    vector<ManagedAbiEventRuntime> Events {};
    vector<ManagedAbiSettingRuntime> Settings {};
    vector<ManagedAbiInnerRuntime> Inners {};
    // Diagnostic, fed from any worker; off until a test first reads it
    std::atomic<bool> CountInnerEntityVisits {};
    std::atomic<uint64_t> InnerEntityVisits {};
};

struct ManagedAssemblyResource
{
    string ResourcePath {};
    string FileName {};
    vector<uint8_t> Data {};
};

struct ManagedWrapperClassEntry
{
    nptr<MonoClass> Class {};
    nptr<MonoMethod> PointerCtor {};
};

struct ManagedDynamicFieldAccessors
{
    nptr<MonoMethod> Getter {};
    nptr<MonoMethod> Setter {};
};

// Native helpers the bridge invokes by name, resolved once when the ABI binds
static constexpr array<pair<string_view, int32_t>, 19> MANAGED_NATIVE_HELPERS {{
    {"IsDelegate", 1},
    {"IsDictionary", 1},
    {"IsList", 1},
    {"AddDictionaryItem", 3},
    {"AddListItem", 2},
    {"CreateDictionary", 2},
    {"CreateDictionaryOfList", 2},
    {"CreateList", 1},
    {"DescribeScriptEntry", 1},
    {"DescribeException", 1},
    {"EventHandlersEqual", 2},
    {"GetDelegateKey", 1},
    {"GetDictionaryCount", 1},
    {"GetDictionaryKey", 2},
    {"GetDictionaryValue", 2},
    {"GetListCount", 1},
    {"GetListItem", 2},
    {"InvokeCallback", 2},
    {"InvokeEvent", 3},
}};

// Backend-scoped lookups shared by every worker of the engine. Wrapper classes are filled once when the ABI binds and
// only read afterwards; adapters are resolved at registration, which any worker may do, so they take the lock
struct ManagedBackendCaches
{
    unordered_map<string, ManagedWrapperClassEntry> WrapperClasses {};
    unordered_map<string, nptr<MonoMethod>> NativeMethods {};
    // Every FOnline class the bridge names at run time, and the property accessors of every dynamic ref type field
    unordered_map<string, nptr<MonoClass>> Classes {};
    unordered_map<const Property*, ManagedDynamicFieldAccessors> DynamicFields {};
    // By event id: the generated AdaptInvoke of an event that dispatches through a frame, null for a boxed one
    vector<nptr<MonoMethod>> EventAdapters {};
    mutex CallbackAdaptersLocker {};
    unordered_map<string, nptr<MonoMethod>> CallbackAdapters FO_TSA_GUARDED_BY(CallbackAdaptersLocker) {};
    // Diagnostic counters stay off until a test first reads them: a production dispatch pays one relaxed load
    std::atomic<bool> CountDispatches {};
    std::atomic<uint64_t> TypedCallbackDispatches {};
    std::atomic<uint64_t> BoxedCallbackDispatches {};
};

// Built once per callback registration: the native signature, the frame layout it maps to and the generated adapter
// that reads that frame; a null adapter means the signature stays on the boxed path
struct ManagedCallbackPlan
{
    ComplexTypeDesc Ret {};
    vector<ComplexTypeDesc> Args {};
    ManagedAbiCallbackLayout Layout {};
    nptr<MonoMethod> Adapter {};
    nptr<BaseEngine> Engine {};
};

// Static free-function definitions (same order as the declarations above)

// === Backend registry and active-backend lifecycle ===

auto ManagedScriptBackend::GetAliveFlagObject() const -> void*
{
    FO_STACK_TRACE_ENTRY();

    return _aliveFlagGcHandle != 0 ? mono_gchandle_get_target(_aliveFlagGcHandle) : nullptr;
}

// The alive flag is a rooted one-element managed bool array. Wrappers that outlive deterministic disposal (e.g
void ManagedScriptBackend::CreateAliveFlag()
{
    FO_STACK_TRACE_ENTRY();

    if (_aliveFlagGcHandle != 0) {
        return;
    }

    MonoDomain* domain = GetDomainOrThrow(_domain.get());
    MonoArray* flag_array = mono_array_new(domain, mono_get_boolean_class(), 1);

    if (flag_array == nullptr) {
        throw ScriptSystemException("Can't create Managed backend alive flag");
    }

    mono_array_set(flag_array, uint8_t, 0, 1);
    _aliveFlagGcHandle = NewManagedGcHandle(reinterpret_cast<MonoObject*>(flag_array), 0);
}

void ManagedScriptBackend::ReleaseAliveFlag()
{
    FO_STACK_TRACE_ENTRY();

    if (_aliveFlagGcHandle != 0) {
        MonoDomain* domain = GetDomainOrThrow(_domain.get());
        ManagedThreadAttachment managed_thread {domain};
        MonoObject* flag_obj = mono_gchandle_get_target(_aliveFlagGcHandle);

        if (flag_obj != nullptr) {
            mono_array_set(reinterpret_cast<MonoArray*>(flag_obj), uint8_t, 0, 0);
        }

        mono_gchandle_free(_aliveFlagGcHandle);
        _aliveFlagGcHandle = 0;
    }
}

void ManagedScriptBackend::EnableDeepEntityWrapperTracking()
{
    FO_STACK_TRACE_ENTRY();

    // The core scripts always keep the live wrapper count; this arms the weak table that names what it reports. A backend with no
    // engine behind its metadata (the baker's) has no settings to read and stays counting only, naming being a running-game diagnostic
    nptr<GlobalSettings> settings = GetBackendSettings(this);

    if (!settings || !settings->ManagedScript.DeepTrackEntityWrappers) {
        return;
    }

    ActiveBackendScope active_backend {this};
    MonoMethod* enable_method = FindCoreScriptMethod(this, "EntityWrapperTracker", "EnableDeepWrapperTracking", 0);

    if (enable_method == nullptr) {
        return;
    }

    MonoObject* exception = nullptr;
    (void)mono_runtime_invoke(enable_method, nullptr, nullptr, &exception);
    ThrowIfManagedException(exception, "Enabling managed entity wrapper tracking failed");
}

void ManagedScriptBackend::ClearScriptStatics() noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (!_domain) {
        return;
    }

    // Done in managed code: mono_field_static_set_value leaves the static area disagreeing with the root descriptor the collector
    // scans it by, which a bisection pinned to the very first field written
    safe_call([this] {
        // Reading a static through reflection runs the type's static constructor when it has not run yet, and
        // those reach back into native code, so the backend has to be the active one for this call
        ActiveBackendScope active_backend {this};
        MonoMethod* clear_method = FindCoreScriptMethod(this, "ScriptStaticCleanup", "ClearScriptStatics", 0);

        if (clear_method == nullptr) {
            return;
        }

        MonoObject* exception = nullptr;
        MonoObject* result = mono_runtime_invoke(clear_method, nullptr, nullptr, &exception);
        ThrowIfManagedException(exception, "Clearing script statics failed");

        if (result != nullptr) {
            string report = ToStringAndFree(reinterpret_cast<MonoString*>(result));

            if (!report.empty()) {
                logging::write("Script statics on shutdown: {}", report);
            }
        }
    });
}

void ManagedScriptBackend::FinalizeManagedObjects() noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (!_domain || _images.empty()) {
        return;
    }

    // GC.WaitForPendingFinalizers in managed code is the supported finalizer wait; mono_domain_finalize belongs to domain unloading and
    // timed out, then crashed, on a root-domain shutdown. Passes repeat because a finalized wrapper can drop the last reference to another
    constexpr int32_t PASS_LIMIT = 8;

    safe_call([this] {
        MonoMethod* collect_method = FindCoreScriptMethod(this, "EntityWrapperTracker", "CollectAndWaitForFinalizers", 1);
        time_meter collect_time;

        int32_t outstanding = -1;

        // A timed-out or failed collection must still leave a wrapper report for diagnosis
        safe_call([&] { outstanding = InvokeEntityWrapperTrackerCollect(collect_method, PASS_LIMIT); });

        timespan collect_duration = collect_time.get_duration();
        MonoMethod* dump_method = FindCoreScriptMethod(this, "EntityWrapperTracker", "DumpOutstandingWrappers", 0);
        string report = InvokeEntityWrapperTrackerDump(dump_method);

        // Empty on the ordinary path and not worth a line per destroyed engine; a real report carries the duration, because this phase
        // is paid per destroyed engine and a parallel run destroys dozens
        if (!report.empty()) {
            logging::write("Managed wrapper tracking: {}, collected in {}", report, collect_duration);
        }

        // Not a gate: what a wrapper still holds does not stop the rest of the shutdown, but it is a native entity
        // reference nobody gave back, and the report is the only place it is visible
        FO_VERIFY_AND_CONTINUE(outstanding <= 0, "Managed entity wrappers outlived the script backend", outstanding, report);
    });
}

static auto GetActiveBackendOrThrow() -> ptr<ManagedScriptBackend>
{
    FO_STACK_TRACE_ENTRY();

    if (!ActiveBackend || !ActiveBackend->GetMetadata()) {
        throw ScriptSystemException("Managed backend is not active");
    }

    return ActiveBackend;
}

static auto GetActiveEntityManagerOrThrow() -> ptr<EntityManagerApi>
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    nptr<EngineMetadata> meta = backend->GetMetadata();
    nptr<EntityManagerApi> entity_mngr = meta.dyn_cast<EntityManagerApi>();

    if (!entity_mngr) {
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

static auto GetBackendSettings(ptr<ManagedScriptBackend> backend) -> nptr<GlobalSettings>
{
    FO_STACK_TRACE_ENTRY();

    nptr<EngineMetadata> meta = backend->GetMetadata();
    nptr<BaseEngine> engine = meta.dyn_cast<BaseEngine>();

    if (engine) {
        return engine->Settings;
    }

    return nullptr;
}

// === Script entries, exceptions and stack traces ===

// Every native call into script code goes through an entry, which is what places its frames in native stack traces
static auto InvokeManagedScript(MonoMethod* method, MonoObject* obj, void** args, string_view context) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    ManagedScriptEntryScope entry {method};
    MonoObject* exception = nullptr;
    MonoObject* result = mono_runtime_invoke(method, obj, args, &exception);
    entry.Leave();
    ThrowIfManagedException(exception, context, &entry);
    return result;
}

static void InvokeManagedScriptDelegate(MonoObject* delegate_obj, string_view context)
{
    FO_STACK_TRACE_ENTRY();

    // The delegate target is not known natively, so the entry claims whichever frames run under its invoke
    ManagedScriptEntryScope entry {nullptr};
    MonoObject* exception = nullptr;
    mono_runtime_delegate_invoke(delegate_obj, nullptr, &exception);
    entry.Leave();
    ThrowIfManagedException(exception, context, &entry);
}

// Every native entry into script code runs its synchronization context here, so one place measures it against
// ManagedScript.OverrunReportTime; a run that throws is reported by its exception instead
static void RunManagedScriptEntry(ptr<ManagedScriptBackend> backend, ptr<BaseEngine> engine, const function<MonoObject*()>& get_entry, const function<void()>& callback)
{
    FO_STACK_TRACE_ENTRY();

    time_meter run_time;
    timespan lock_wait_duration = engine->RunScriptContext(callback);
    ReportManagedScriptOverrun(backend, engine, run_time.get_duration(), lock_wait_duration, get_entry);
}

// The same two measurements AngelScriptContextManager::RunContext reports, so both backends read alike in a log
static void ReportManagedScriptOverrun(ptr<ManagedScriptBackend> backend, ptr<BaseEngine> engine, timespan total_duration, timespan lock_wait_duration, const function<MonoObject*()>& get_entry)
{
    FO_STACK_TRACE_ENTRY();

    timespan overrun_time = std::chrono::milliseconds(engine->Settings->ManagedScript.OverrunReportTime);

    if (!overrun_time || is_run_in_debugger() || engine->IsStartingUp()) {
        return;
    }

    timespan execution_duration = total_duration >= lock_wait_duration ? total_duration - lock_wait_duration : timespan::zero;
    bool execution_overrun = execution_duration >= overrun_time;
    bool lock_wait_overrun = lock_wait_duration >= overrun_time;

    if (!execution_overrun && !lock_wait_overrun) {
        return;
    }

    if constexpr (!FO_DEBUG) {
        string entry_name = DescribeManagedScriptEntry(backend, get_entry);

        if (execution_overrun) {
            logging::write("Script execution overrun: {} (execution: {}, lock wait: {}, total: {})", entry_name, execution_duration, lock_wait_duration, total_duration);
        }
        if (lock_wait_overrun) {
            logging::write("Script lock wait overrun: {} (lock wait: {}, execution: {}, total: {})", entry_name, lock_wait_duration, execution_duration, total_duration);
        }
    }
}

static auto DescribeManagedScriptEntry(ptr<ManagedScriptBackend> backend, const function<MonoObject*()>& get_entry) -> string
{
    FO_STACK_TRACE_ENTRY();

    ManagedThreadAttachment managed_thread {GetDomainOrThrow(backend->GetDomain())};
    void* args[] = {get_entry()};
    return ToStringAndFree(reinterpret_cast<MonoString*>(InvokeNativeHelper(backend, "DescribeScriptEntry", 1, args)));
}

// Script code reports an exception it caught itself; see ScriptExceptions.Record
static void NativeReportException(MonoString* summary, MonoString* native_error, MonoArray* frames)
{
    FO_STACK_TRACE_ENTRY();

    try {
        string summary_str = ToStringAndFree(summary);
        nptr<ManagedScriptEntryScope> entry = ManagedScriptEntryScope::GetInnermostRunning();

        // A native failure handed to script as an error string is reported as the native exception it was
        if (entry && native_error != nullptr) {
            if (std::exception_ptr crossed = entry->FindCrossedNativeException(native_error)) {
                try {
                    std::rethrow_exception(crossed);
                }
                catch (const std::exception& ex) {
                    exceptions::report_and_continue(ex);
                }

                return;
            }
        }

        stack_trace::data st = stack_trace::get();
        stack_trace::splice_caught_script_frames(st, MakeManagedExceptionLayer(ReadManagedExceptionFrames(frames)));
        exceptions::report_and_continue(ScriptException(st, "Managed script exception", summary_str));
    }
    catch (const std::exception& ex) {
        exceptions::report_and_continue(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

// Called from the catch of an internal call. Script receives the message, and the running entry keeps the exception
// in case script lets the error propagate
static auto MakeManagedNativeError(const std::exception& ex) -> MonoString*
{
    FO_STACK_TRACE_ENTRY();

    MonoString* message = mono_string_new(mono_domain_get(), ex.what());

    if (nptr<ManagedScriptEntryScope> entry = ManagedScriptEntryScope::GetInnermostRunning()) {
        entry->SetCrossedNativeException(std::current_exception(), message);
    }

    return message;
}

static void CollectManagedScriptStackLayers(const stack_trace::data& st, std::vector<stack_trace::script_layer>& out_layers) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        // Script frames exist on a thread only under a running entry, which also proves the thread is attached
        nptr<ManagedScriptEntryScope> entry = ManagedScriptEntryScope::GetInnermostRunning();

        if (!entry) {
            return;
        }

        ManagedStackWalk walk;
        walk.Entry = entry;
        mono_stack_walk(&CollectManagedStackFrame, &walk);

        if (!walk.Layer.script_frames.empty()) {
            walk.Layers.emplace_back(std::move(walk.Layer));
        }

        if (walk.Layers.empty()) {
            return;
        }

        MonoDomain* domain = mono_get_root_domain();
        AppendRuntimeNativeFrames(domain, {st.native_frames.data(), st.native_frame_count}, walk.Layers.front());

        for (nptr<ManagedScriptEntryScope> running = entry; running; running = running->GetNextRunning()) {
            running->AppendBirthRuntimeFrames(domain, walk.Layers.front());
        }

        for (stack_trace::script_layer& layer : walk.Layers) {
            out_layers.emplace_back(std::move(layer));
        }
    }
    catch (...) {
        break_into_debugger();
    }
}

// Visits frames innermost first. A run of script frames ends at the runtime-invoke wrapper native code entered it through
static auto CollectManagedStackFrame(MonoMethod* method, int32_t native_offset, int32_t il_offset, mono_bool managed, void* data) -> mono_bool
{
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(native_offset);

    try {
        auto walk = cast_from_void<ManagedStackWalk*>(data).as_ptr();

        if (managed != 0) {
            FO_VERIFY_AND_THROW(method != nullptr, "Managed stack frame has no method");

            if (optional<stack_trace::frame> frame = MakeManagedStackFrame(method, il_offset)) {
                walk->Layer.script_frames.emplace_back(std::move(*frame));
                walk->OutermostMethod = method;
            }
        }
        else if (walk->Entry && !walk->Layer.script_frames.empty() && IsManagedRuntimeInvokeWrapper(method)) {
            nptr<MonoMethod> entry_method = walk->Entry->GetMethod();

            // An invoke that no entry recorded, such as a class constructor or an internal helper, stays in the enclosing run
            if (!entry_method || entry_method == walk->OutermostMethod) {
                walk->Entry->CopyBirthFrames(walk->Layer);
                walk->Layers.emplace_back(std::exchange(walk->Layer, stack_trace::script_layer {}));
                walk->Entry = walk->Entry->GetNextRunning();
            }
        }

        return 0;
    }
    catch (...) {
        return 1;
    }
}

static auto DescribeManagedException(MonoObject* exception, nptr<ManagedScriptEntryScope> entry) -> ManagedExceptionDescription
{
    FO_STACK_TRACE_ENTRY();

    ManagedExceptionDescription description;
    nptr<ManagedScriptBackend> backend = ActiveBackend;

    // The describing helper is part of the core scripts, which are not loaded while the load context itself is created
    if (!backend || backend->GetImages().empty()) {
        description.Summary = ManagedObjectToString(exception);
        return description;
    }

    MonoMethod* describe_method = FindNativeMethod(backend, "DescribeException", 1);

    void* args[] = {exception};
    MonoObject* describe_exception = nullptr;
    ManagedObjectRoot result;
    result.SetObject(mono_runtime_invoke(describe_method, nullptr, args, &describe_exception));
    FO_VERIFY_AND_THROW(describe_exception == nullptr && result.GetObject() != nullptr, "Managed exception description failed", ManagedObjectToString(describe_exception), ManagedObjectToString(exception));

    auto parts = reinterpret_cast<MonoArray*>(result.GetObject());
    FO_VERIFY_AND_THROW(mono_array_length(parts) == 3, "Managed exception description must hold summary, native error and frames", mono_array_length(parts));

    description.Summary = ToStringAndFree(reinterpret_cast<MonoString*>(mono_array_get(parts, MonoObject*, 0)));

    if (entry) {
        description.NativeException = entry->FindCrossedNativeException(reinterpret_cast<MonoString*>(mono_array_get(parts, MonoObject*, 1)));
    }

    description.Frames = ReadManagedExceptionFrames(reinterpret_cast<MonoArray*>(mono_array_get(parts, MonoObject*, 2)));
    return description;
}

static auto ReadManagedExceptionFrames(MonoArray* frames) -> vector<pair<ptr<MonoMethod>, int32_t>>
{
    FO_STACK_TRACE_ENTRY();

    size_t values_count = frames != nullptr ? mono_array_length(frames) : 0;
    FO_VERIFY_AND_THROW(values_count % 2 == 0, "Managed exception frames must pair a method handle with an IL offset", values_count);

    vector<pair<ptr<MonoMethod>, int32_t>> result;
    result.reserve(values_count / 2);

    for (size_t i = 0; i < values_count; i += 2) {
        int64_t method_handle = mono_array_get(frames, int64_t, i);
        int64_t il_offset = mono_array_get(frames, int64_t, i + 1);
        FO_VERIFY_AND_THROW(method_handle != 0, "Managed exception frame has no method handle", i);
        result.emplace_back(std::bit_cast<MonoMethod*>(numeric_cast<uintptr_t>(method_handle)), numeric_cast<int32_t>(il_offset));
    }

    return result;
}

static auto MakeManagedExceptionLayer(const vector<pair<ptr<MonoMethod>, int32_t>>& frames) -> stack_trace::script_layer
{
    FO_STACK_TRACE_ENTRY();

    stack_trace::script_layer layer;
    layer.script_frames.reserve(frames.size());

    for (const auto& [method, il_offset] : frames) {
        if (optional<stack_trace::frame> frame = MakeManagedStackFrame(method, il_offset)) {
            layer.script_frames.emplace_back(std::move(*frame));
        }
    }

    return layer;
}

// Generated marshalling stubs (reflection invoke stubs and the like) are runtime plumbing, not script frames
static auto MakeManagedStackFrame(ptr<MonoMethod> method, int32_t il_offset) -> optional<stack_trace::frame>
{
    FO_STACK_TRACE_ENTRY();

    stack_trace::frame frame;
    frame.type = stack_trace::frame::frame_type::script;

    if (char* full_name = mono_method_full_name(method.get(), 1); full_name != nullptr) {
        // Mono spells a method "Namespace.Outer/Inner:Method (args)", while script code reads "Namespace.Outer.Inner.Method(args)"
        string name {full_name};
        mono_free(full_name);

        if (name.starts_with("(wrapper ")) {
            return std::nullopt;
        }
        if (size_t separator = name.find(':'); separator != string::npos) {
            name[separator] = '.';
        }
        if (size_t args_space = name.find(" ("); args_space != string::npos) {
            name.erase(args_space, 1);
        }

        std::ranges::replace(name, '/', '.');
        frame.function.assign(name.data(), name.size());
    }

    if (il_offset >= 0 && mono_debug_enabled() != 0) {
        if (MonoDebugMethodInfo* method_info = mono_debug_lookup_method(method.get()); method_info != nullptr) {
            if (MonoDebugSourceLocation* location = mono_debug_method_lookup_location(method_info, il_offset); location != nullptr) {
                string file = location->source_file != nullptr ? string {location->source_file} : string {};
                uint32_t line = location->row;
                mono_debug_free_source_location(location);

                frame.file.assign(file.data(), file.size());
                frame.line = line;
            }
        }
    }

    return frame;
}

static void AppendRuntimeNativeFrames(MonoDomain* domain, span<const stack_trace::native_frame_address> frames, stack_trace::script_layer& layer)
{
    FO_STACK_TRACE_ENTRY();

    for (stack_trace::native_frame_address address : frames) {
        // A return address may sit just past the end of its method, so the lookup asks for the call instruction
        if (address > 1 && mono_jit_info_table_find(domain, std::bit_cast<void*>(address - 1)) != nullptr) {
            layer.runtime_native_frames.emplace_back(address);
        }
    }
}

// mono_runtime_invoke enters managed code through generated wrappers named after the signature they marshal
static auto IsManagedRuntimeInvokeWrapper(MonoMethod* method) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const char* name = method != nullptr ? mono_method_get_name(method) : nullptr;
    return name != nullptr && string_view {name}.starts_with("runtime_invoke");
}

ManagedScriptEntryScope::ManagedScriptEntryScope(nptr<MonoMethod> method) noexcept :
    _method {method},
    _parent {CurrentScriptEntry}
{
    FO_NO_STACK_TRACE_ENTRY();

    stack_trace::capture_native_frames(_birthFrames, _birthFrameCount, _birthTruncated, 1);
    CurrentScriptEntry = this;
}

ManagedScriptEntryScope::~ManagedScriptEntryScope()
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(CurrentScriptEntry == this, "Managed script entries must unwind in nesting order");
    CurrentScriptEntry = _parent;

    for (const auto& [handle, exception] : _crossedNativeExceptions) {
        mono_gchandle_free(handle);
    }
}

auto ManagedScriptEntryScope::GetInnermostRunning() noexcept -> nptr<ManagedScriptEntryScope>
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<ManagedScriptEntryScope> entry = CurrentScriptEntry;

    while (entry && !entry->_running) {
        entry = entry->_parent;
    }

    return entry;
}

auto ManagedScriptEntryScope::GetNextRunning() const noexcept -> nptr<ManagedScriptEntryScope>
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<ManagedScriptEntryScope> entry = _parent;

    while (entry && !entry->_running) {
        entry = entry->_parent;
    }

    return entry;
}

void ManagedScriptEntryScope::CopyBirthFrames(stack_trace::script_layer& layer) const noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    std::copy_n(_birthFrames.begin(), _birthFrameCount, layer.birth_native_frames.begin());
    layer.birth_native_frame_count = _birthFrameCount;
    layer.birth_native_truncated = _birthTruncated;
}

void ManagedScriptEntryScope::AppendBirthRuntimeFrames(MonoDomain* domain, stack_trace::script_layer& layer) const
{
    FO_STACK_TRACE_ENTRY();

    AppendRuntimeNativeFrames(domain, {_birthFrames.data(), _birthFrameCount}, layer);
}

void ManagedScriptEntryScope::SetCrossedNativeException(std::exception_ptr exception, MonoString* message)
{
    FO_STACK_TRACE_ENTRY();

    // Exception.Message preserves this string's identity; a strong handle keeps the key valid through moving collections
    uint32_t handle = NewManagedGcHandle(reinterpret_cast<MonoObject*>(message), 0);
    _crossedNativeExceptions.emplace_back(handle, std::move(exception));
}

auto ManagedScriptEntryScope::FindCrossedNativeException(MonoString* message) const noexcept -> std::exception_ptr
{
    FO_NO_STACK_TRACE_ENTRY();

    if (message == nullptr) {
        return {};
    }

    for (nptr<const ManagedScriptEntryScope> entry = this; entry; entry = entry->_parent) {
        for (const auto& [handle, exception] : entry->_crossedNativeExceptions) {
            if (mono_gchandle_get_target(handle) == reinterpret_cast<MonoObject*>(message)) {
                return exception;
            }
        }
    }

    return {};
}

// === Native ABI: logging, hashing, backend and prototype queries ===

static void NativeLog(MonoString* text)
{
    FO_STACK_TRACE_ENTRY();

    if (text == nullptr) {
        logging::write("{}", string_view {});
        return;
    }

    char* text_utf8 = mono_string_to_utf8(text);

    if (text_utf8 == nullptr) {
        logging::write("{}", string_view {});
        return;
    }

    string log_text = text_utf8;
    mono_free(text_utf8);
    logging::write("{}", log_text);
}

static auto NativeHstringHandle(const hstring& value) -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!value) {
        return nullptr;
    }

    return const_cast<void*>(static_cast<const void*>(value.get_entry().get()));
}

static auto NativeHstringFromHandle(void* value) -> hstring
{
    FO_NO_STACK_TRACE_ENTRY();

    if (value == nullptr) {
        return {};
    }

    return hstring(ptr<const hstring::entry>(static_cast<const hstring::entry*>(value)));
}

static auto NativeGetHash(MonoString* text) -> void*
{
    FO_STACK_TRACE_ENTRY();

    string value = ToStringAndFree(text);

    if (value.empty()) {
        return nullptr;
    }

    auto backend = GetActiveBackendOrThrow();
    return NativeHstringHandle(backend->GetMetadata()->Hashes.to_hashed_string(value));
}

static auto NativeGetHashStr(void* value) -> MonoString*
{
    FO_STACK_TRACE_ENTRY();

    if (value == nullptr) {
        return mono_string_new(GetDomainOrThrow(mono_domain_get()), "");
    }

    return mono_string_new(GetDomainOrThrow(mono_domain_get()), NativeHstringFromHandle(value).c_str());
}

static auto NativeGetHashStrFromHash(uint64_t value) -> MonoString*
{
    FO_STACK_TRACE_ENTRY();

    string text = strex("{}", value).str();

    if (ActiveBackend && ActiveBackend->GetMetadata()) {
        bool failed = false;
        hstring resolved = ActiveBackend->GetMetadata()->Hashes.resolve_hash(value, &failed);

        if (!failed) {
            text = resolved.as_str();
        }
    }

    return mono_string_new(GetDomainOrThrow(mono_domain_get()), text.c_str());
}

static auto NativeResolveHash(uint64_t hash) -> void*
{
    FO_STACK_TRACE_ENTRY();

    if (hash == 0) {
        return nullptr;
    }

    auto backend = GetActiveBackendOrThrow();
    bool failed = false;
    hstring resolved = backend->GetMetadata()->Hashes.resolve_hash(hash, &failed);

    if (failed) {
        throw ScriptSystemException("Managed hstring is not interned in the active backend", hash);
    }

    return NativeHstringHandle(resolved);
}

// Returns the calling engine's alive flag (a one-element managed bool array)
static auto NativeGetBackendAliveFlag() -> MonoArray*
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    void* flag = backend->GetAliveFlagObject();

    if (flag == nullptr) {
        throw ScriptSystemException("Managed backend alive flag is not created");
    }

    return static_cast<MonoArray*>(flag);
}

static auto NativeRunScriptContinuation(MonoObject* continuation) -> MonoString*
{
    FO_STACK_TRACE_ENTRY();

    try {
        auto backend = GetActiveBackendOrThrow();
        auto engine = backend->GetMetadata().dyn_cast<BaseEngine>();
        FO_VERIFY_AND_THROW(engine, "Managed continuation requires an engine context");
        FO_VERIFY_AND_THROW(continuation != nullptr, "Managed continuation is null");

        RunManagedScriptEntry(
            backend, engine, [continuation] { return continuation; },
            [&] {
                ActiveBackendScope active_backend {backend};
                InvokeManagedScriptDelegate(continuation, "Managed continuation failed");
            });
        return nullptr;
    }
    catch (const std::exception& ex) {
        return MakeManagedNativeError(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

static auto NativeGetBackend() -> void*
{
    FO_STACK_TRACE_ENTRY();

    return GetActiveBackendOrThrow().get();
}

// Custom-entity proto getters: the built-in entities carry metadata-exported Game.GetProto*, custom ones
// resolve through this path instead
static auto NativeGetProtoEntity(MonoString* type_name, void* proto_id) -> void*
{
    FO_STACK_TRACE_ENTRY();

    if (!ActiveBackend || !ActiveBackend->GetMetadata()) {
        return nullptr;
    }

    string type_name_str = ToStringAndFree(type_name);
    ptr<const EngineMetadata> meta = ActiveBackend->GetMetadata();
    hstring type_hname = meta->Hashes.to_hashed_string(type_name_str);
    auto proto = meta->GetProtoEntity(type_hname, NativeHstringFromHandle(proto_id));
    return proto.void_cast();
}

static auto NativeCheckProtoEntity(MonoString* type_name, void* proto_id) -> mono_bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!ActiveBackend || !ActiveBackend->GetMetadata()) {
        return static_cast<mono_bool>(0);
    }

    string type_name_str = ToStringAndFree(type_name);
    ptr<const EngineMetadata> meta = ActiveBackend->GetMetadata();
    hstring type_hname = meta->Hashes.to_hashed_string(type_name_str);
    return static_cast<mono_bool>(meta->GetProtoEntity(type_hname, NativeHstringFromHandle(proto_id)) ? 1 : 0);
}

// Plural proto enumeration (managed equivalent of AngelScript Game_GetProtoCustomEntities): count + by-index, backing
// the generated Game.GetProto<X>s()/Get<X>s() loops
static auto NativeGetProtoEntityCount(MonoString* type_name) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    if (!ActiveBackend || !ActiveBackend->GetMetadata()) {
        return 0;
    }

    string type_name_str = ToStringAndFree(type_name);
    ptr<const EngineMetadata> meta = ActiveBackend->GetMetadata();
    return numeric_cast<int32_t>(meta->GetProtoEntities(meta->Hashes.to_hashed_string(type_name_str)).size());
}

static auto NativeGetProtoEntityAt(MonoString* type_name, int32_t index) -> void*
{
    FO_STACK_TRACE_ENTRY();

    if (!ActiveBackend || !ActiveBackend->GetMetadata()) {
        return nullptr;
    }

    string type_name_str = ToStringAndFree(type_name);
    ptr<const EngineMetadata> meta = ActiveBackend->GetMetadata();
    const auto& protos = meta->GetProtoEntities(meta->Hashes.to_hashed_string(type_name_str));

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

static auto InvokeEntityWrapperTrackerCollect(MonoMethod* method, int32_t pass_limit) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    void* args[] = {&pass_limit};
    MonoObject* exception = nullptr;
    MonoObject* result = mono_runtime_invoke(method, nullptr, args, &exception);
    ThrowIfManagedException(exception, "Managed entity wrapper collection failed");
    FO_VERIFY_AND_THROW(result != nullptr, "Managed entity wrapper collection returned no count");

    return *static_cast<int32_t*>(mono_object_unbox(result));
}

static auto InvokeEntityWrapperTrackerDump(MonoMethod* method) -> string
{
    FO_STACK_TRACE_ENTRY();

    MonoObject* exception = nullptr;
    MonoObject* result = mono_runtime_invoke(method, nullptr, nullptr, &exception);
    ThrowIfManagedException(exception, "Managed entity wrapper report failed");
    FO_VERIFY_AND_THROW(result != nullptr, "Managed entity wrapper report returned no result");

    return ToStringAndFree(reinterpret_cast<MonoString*>(result));
}

// A wrapper AddRefs the native entity and releases it only in its finalizer, so one retained past destroy keeps it alive-but-destroyed;
// what teardown's collection cannot reach is reported by the live entity count rather than taken away behind the wrapper's back
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

// The tracker is part of the core scripts, so it is reached through whichever loaded image carries them
static auto FindCoreScriptMethod(ptr<const ManagedScriptBackend> backend, const char* class_name, const char* method_name, int32_t param_count) -> MonoMethod*
{
    FO_NO_STACK_TRACE_ENTRY();

    CountMetadataLookup();

    for (nptr<void> image_ptr : backend->GetImages()) {
        nptr<MonoImage> image = image_ptr.reinterpret_as<MonoImage>();
        MonoClass* core_class = mono_class_from_name(image.get(), "FOnline", class_name);

        if (core_class != nullptr) {
            MonoMethod* method = mono_class_get_method_from_name(core_class, method_name, param_count);
            FO_VERIFY_AND_THROW(method != nullptr, "Managed core method not found", class_name, method_name, param_count);
            return method;
        }
    }

    FO_VERIFY_AND_THROW(backend->GetImages().empty(), "Managed core class not found", class_name);
    return nullptr;
}

// Backs the managed `Entity.IsDestroyed` property (parity with AngelScript). A null wrapper pointer counts as
// destroyed. The wrapper holds a strong ref (AddRef/Release), so reading a destroyed-but-alive entity is safe
static auto NativeIsEntityDestroyed(void* entity_ptr) -> mono_bool
{
    FO_NO_STACK_TRACE_ENTRY();

    bool destroyed = entity_ptr == nullptr || static_cast<const Entity*>(entity_ptr)->IsDestroyed();
    return static_cast<mono_bool>(destroyed ? 1 : 0);
}

// Backs the managed `Entity.IsDestroying` property (parity with AngelScript get_IsDestroying). A null wrapper
// pointer is already gone rather than mid-destruction, so it reports false
static auto NativeIsEntityDestroying(void* entity_ptr) -> mono_bool
{
    FO_NO_STACK_TRACE_ENTRY();

    bool destroying = entity_ptr != nullptr && static_cast<const Entity*>(entity_ptr)->IsDestroying();
    return static_cast<mono_bool>(destroying ? 1 : 0);
}

// Managed Entity.Name -> AngelScript base-entity `get_Name()` (Entity::GetName); a manual base binding
static auto NativeGetEntityName(void* entity_ptr) -> MonoString*
{
    FO_NO_STACK_TRACE_ENTRY();

    MonoDomain* domain = GetDomainOrThrow(mono_domain_get());

    if (entity_ptr == nullptr) {
        return mono_string_new(domain, "");
    }

    string name = string(static_cast<const Entity*>(entity_ptr)->GetName());
    return mono_string_new(domain, name.c_str());
}

static auto NativeGetEntityId(void* entity_ptr) -> int64_t
{
    FO_STACK_TRACE_ENTRY();

    nptr<const Entity> entity = static_cast<const Entity*>(entity_ptr);

    if (!entity) {
        return {};
    }

    return entity->GetId().underlying_value();
}

static auto NativeGetEntityProtoId(void* entity_ptr) -> void*
{
    FO_STACK_TRACE_ENTRY();

    nptr<const Entity> entity = static_cast<const Entity*>(entity_ptr);

    if (!entity) {
        return nullptr;
    }

    if (nptr<const ProtoEntity> self_proto = entity.dyn_cast<ProtoEntity>(); self_proto) {
        return NativeHstringHandle(self_proto->GetProtoId());
    }
    if (nptr<const EntityWithProto> self_with_proto = entity.dyn_cast<EntityWithProto>(); self_with_proto) {
        return NativeHstringHandle(self_with_proto->GetProtoId());
    }

    return nullptr;
}

// Generic property accessors by index: the generated Entity.GetAs*/SetAs* wrappers route through here
static auto ResolveManagedGenericProperty(void* entity_ptr, int32_t prop_index, bool require_mutable) -> pair<ptr<Entity>, ptr<const Property>>
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    auto entity = ResolveEntity(backend, entity_ptr);

    auto nullable_prop = entity->GetProperties()->GetRegistrar()->GetPropertyByIndex(prop_index);

    if (!nullable_prop) {
        throw ScriptException("Property invalid enum", prop_index);
    }

    auto prop = nullable_prop.as_ptr();

    if (!prop->IsPlainData()) {
        throw ScriptException("Property in not plain data type");
    }
    if (prop->IsDisabled()) {
        throw ScriptException("Property is disabled");
    }
    if (require_mutable && !prop->IsMutable()) {
        throw ScriptException("Property is not mutable");
    }

    return {entity, prop};
}

static auto ResolveManagedScalarProperty(void* entity_ptr, int32_t prop_index, int32_t size, bool require_mutable) -> pair<ptr<Entity>, ptr<const Property>>
{
    FO_STACK_TRACE_ENTRY();

    auto [entity, prop] = ResolveManagedGenericProperty(entity_ptr, prop_index, require_mutable);
    const BaseTypeDesc& base_type = prop->GetBaseType();
    FO_VERIFY_AND_THROW(!prop->IsNullable() && IsManagedAbiFixedPropertyValue(base_type), "Managed property requires a non-nullable fixed value", prop->GetName());
    FO_VERIFY_AND_THROW(size > 0 && numeric_cast<size_t>(size) == prop->GetBaseSize(), "Managed scalar property size mismatch", prop->GetName(), size);
    return {entity, prop};
}

static auto NativeGetPropertyValue(void* entity_ptr, int32_t prop_index, void* value, int32_t size) -> MonoString*
{
    FO_STACK_TRACE_ENTRY();

    try {
        auto [entity, prop] = ResolveManagedScalarProperty(entity_ptr, prop_index, size, false);
        entity->LockForPropertyAccessShared();
        auto auto_unlock = scope_exit([entity]() mutable noexcept { entity->UnlockForPropertyAccessShared(); });

        entity->ValidateAccess();

        if (prop->IsVirtual()) {
            PropertyRawData prop_data = GetPropertyRawData(entity, prop);
            FO_VERIFY_AND_THROW(prop_data.GetSize() == prop->GetBaseSize(), "Managed scalar property data size mismatch", prop->GetName());
            memory::copy(ptr<void> {value}, prop_data.GetPtr(), prop_data.GetSize());
        }
        else {
            auto props = entity->GetProperties();
            props->ValidateForRawData(prop);
            auto raw_data = props->GetRawData(prop);
            FO_VERIFY_AND_THROW(raw_data.size() == prop->GetBaseSize(), "Managed scalar property data size mismatch", prop->GetName());
            memory::copy(ptr<void> {value}, raw_data.data(), raw_data.size());
        }

        const BaseTypeDesc& base_type = prop->GetBaseType();

        if (base_type.IsHashedString || base_type.IsStruct) {
            PropertyDataToValue(GetActiveBackendOrThrow(), base_type, static_cast<uint8_t*>(value));
        }

        return nullptr;
    }
    catch (const std::exception& ex) {
        return MakeManagedNativeError(ex);
    }
}

static auto NativeSetPropertyValue(void* entity_ptr, int32_t prop_index, void* value, int32_t size) -> MonoString*
{
    FO_STACK_TRACE_ENTRY();

    try {
        auto [entity, prop] = ResolveManagedScalarProperty(entity_ptr, prop_index, size, true);
        entity->LockForPropertyAccess();
        auto auto_unlock = scope_exit([entity]() mutable noexcept { entity->UnlockForPropertyAccess(); });

        entity->ValidateAccess();
        PropertyRawData prop_data;

        // Setters may re-enter managed code; own the bytes before invoking them
        prop_data.Set(ptr<const void> {value}, prop->GetBaseSize());
        ValueToPropertyData(prop->GetBaseType(), prop_data.GetPtrAs<uint8_t>().get());
        entity->SetValueFromData(prop, prop_data);
        return nullptr;
    }
    catch (const std::exception& ex) {
        return MakeManagedNativeError(ex);
    }
}

// An array property whose elements the scalar bridge carries: its storage is the elements back to back
static auto ResolveManagedArrayProperty(void* entity_ptr, int32_t prop_index, int32_t element_size, bool require_mutable) -> pair<ptr<Entity>, ptr<const Property>>
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    auto entity = ResolveEntity(backend, entity_ptr);
    auto nullable_prop = entity->GetProperties()->GetRegistrar()->GetPropertyByIndex(prop_index);

    if (!nullable_prop) {
        throw ScriptException("Property invalid enum", prop_index);
    }

    auto prop = nullable_prop.as_ptr();

    if (prop->IsDisabled()) {
        throw ScriptException("Property is disabled");
    }
    if (require_mutable && !prop->IsMutable()) {
        throw ScriptException("Property is not mutable");
    }

    FO_VERIFY_AND_THROW(prop->IsArray() && !prop->IsDict() && !prop->IsNullable() && IsManagedAbiFixedPropertyValue(prop->GetBaseType()), "Managed array property requires fixed-size elements", prop->GetName());
    FO_VERIFY_AND_THROW(element_size > 0 && numeric_cast<size_t>(element_size) == prop->GetBaseSize(), "Managed array property element size mismatch", prop->GetName(), element_size);
    return {entity, prop};
}

// Reports the array size in bytes and copies the elements when the caller's buffer holds them all; a smaller buffer
// is left untouched, so the caller sizes its list and asks again
static auto NativeGetPropertyArray(void* entity_ptr, int32_t prop_index, void* buffer, int32_t capacity, int32_t element_size, int32_t* size) -> MonoString*
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(size != nullptr, "Managed array property size output is null");
    *size = 0;

    try {
        auto backend = GetActiveBackendOrThrow();
        auto [entity, prop] = ResolveManagedArrayProperty(entity_ptr, prop_index, element_size, false);
        entity->LockForPropertyAccessShared();
        auto auto_unlock = scope_exit([entity]() mutable noexcept { entity->UnlockForPropertyAccessShared(); });

        entity->ValidateAccess();
        FO_VERIFY_AND_THROW(capacity >= 0, "Managed array property buffer capacity is negative", prop->GetName(), capacity);

        PropertyRawData prop_data = GetPropertyRawData(entity, prop);
        size_t data_size = prop_data.GetSize();
        size_t base_size = prop->GetBaseSize();
        FO_VERIFY_AND_THROW(data_size % base_size == 0, "Array property raw data size is not a multiple of the element size", prop->GetName());
        *size = numeric_cast<int32_t>(data_size);

        if (data_size == 0 || numeric_cast<size_t>(capacity) < data_size) {
            return nullptr;
        }

        FO_VERIFY_AND_THROW(buffer != nullptr, "Managed array property buffer is null", prop->GetName());
        memory::copy(ptr<void> {buffer}, prop_data.GetPtr(), data_size);

        const BaseTypeDesc& base_type = prop->GetBaseType();

        if (base_type.IsHashedString || base_type.IsStruct) {
            for (size_t offset = 0; offset < data_size; offset += base_size) {
                PropertyDataToValue(backend, base_type, static_cast<uint8_t*>(buffer) + offset);
            }
        }

        return nullptr;
    }
    catch (const std::exception& ex) {
        return MakeManagedNativeError(ex);
    }
}

static auto NativeSetPropertyArray(void* entity_ptr, int32_t prop_index, void* buffer, int32_t size, int32_t element_size) -> MonoString*
{
    FO_STACK_TRACE_ENTRY();

    try {
        auto [entity, prop] = ResolveManagedArrayProperty(entity_ptr, prop_index, element_size, true);
        entity->LockForPropertyAccess();
        auto auto_unlock = scope_exit([entity]() mutable noexcept { entity->UnlockForPropertyAccess(); });

        entity->ValidateAccess();
        size_t base_size = prop->GetBaseSize();
        FO_VERIFY_AND_THROW(size >= 0 && numeric_cast<size_t>(size) % base_size == 0, "Managed array property data size is not a multiple of the element size", prop->GetName(), size);
        FO_VERIFY_AND_THROW(size == 0 || buffer != nullptr, "Managed array property buffer is null", prop->GetName());

        // Setters may re-enter managed code; own the bytes before invoking them
        PropertyRawData prop_data;
        size_t data_size = numeric_cast<size_t>(size);

        if (data_size != 0) {
            ptr<uint8_t> dst = prop_data.Alloc(data_size);
            memory::copy(dst.get(), buffer, data_size);

            const BaseTypeDesc& base_type = prop->GetBaseType();

            if (base_type.IsHashedString || base_type.IsStruct) {
                for (size_t offset = 0; offset < data_size; offset += base_size) {
                    ValueToPropertyData(base_type, dst.get() + offset);
                }
            }
        }

        entity->SetValueFromData(prop, prop_data);
        return nullptr;
    }
    catch (const std::exception& ex) {
        return MakeManagedNativeError(ex);
    }
}

static auto NativeGetEntityValueAsIntImpl(void* entity_ptr, int32_t prop_index) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    auto [entity, prop] = ResolveManagedGenericProperty(entity_ptr, prop_index, false);
    entity->LockForPropertyAccessShared();
    auto auto_unlock = scope_exit([entity]() mutable noexcept { entity->UnlockForPropertyAccessShared(); });

    entity->ValidateAccess();
    return entity->GetValueAsInt(prop);
}

static auto NativeGetEntityValueAsInt(void* entity_ptr, int32_t prop_index, MonoString** error) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(error != nullptr, "Managed generic property getter error output is null");
    *error = nullptr;

    try {
        return NativeGetEntityValueAsIntImpl(entity_ptr, prop_index);
    }
    catch (const std::exception& ex) {
        *error = MakeManagedNativeError(ex);
        return 0;
    }
}

static void NativeSetEntityValueAsIntImpl(void* entity_ptr, int32_t prop_index, int32_t value)
{
    FO_STACK_TRACE_ENTRY();

    auto [entity, prop] = ResolveManagedGenericProperty(entity_ptr, prop_index, true);
    entity->LockForPropertyAccess();
    auto auto_unlock = scope_exit([entity]() mutable noexcept { entity->UnlockForPropertyAccess(); });

    entity->ValidateAccess();
    entity->SetValueAsInt(prop, value);
}

static auto NativeSetEntityValueAsInt(void* entity_ptr, int32_t prop_index, int32_t value) -> MonoString*
{
    FO_STACK_TRACE_ENTRY();

    try {
        NativeSetEntityValueAsIntImpl(entity_ptr, prop_index, value);
        return nullptr;
    }
    catch (const std::exception& ex) {
        return MakeManagedNativeError(ex);
    }
}

static auto NativeGetEntityValueAsAnyImpl(void* entity_ptr, int32_t prop_index) -> MonoString*
{
    FO_STACK_TRACE_ENTRY();

    MonoDomain* domain = GetDomainOrThrow(mono_domain_get());
    auto [entity, prop] = ResolveManagedGenericProperty(entity_ptr, prop_index, false);
    entity->LockForPropertyAccessShared();
    auto auto_unlock = scope_exit([entity]() mutable noexcept { entity->UnlockForPropertyAccessShared(); });

    entity->ValidateAccess();
    any_t value = entity->GetValueAsAny(prop);
    return mono_string_new_len(domain, value.data(), numeric_cast<uint32_t>(value.size()));
}

static auto NativeGetEntityValueAsAny(void* entity_ptr, int32_t prop_index, MonoString** error) -> MonoString*
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(error != nullptr, "Managed generic property getter error output is null");
    *error = nullptr;

    try {
        return NativeGetEntityValueAsAnyImpl(entity_ptr, prop_index);
    }
    catch (const std::exception& ex) {
        *error = MakeManagedNativeError(ex);
        return nullptr;
    }
}

static void NativeSetEntityValueAsAnyImpl(void* entity_ptr, int32_t prop_index, MonoString* value)
{
    FO_STACK_TRACE_ENTRY();

    auto [entity, prop] = ResolveManagedGenericProperty(entity_ptr, prop_index, true);
    entity->LockForPropertyAccess();
    auto auto_unlock = scope_exit([entity]() mutable noexcept { entity->UnlockForPropertyAccess(); });

    entity->ValidateAccess();
    entity->SetValueAsAny(prop, any_t {ToStringAndFree(value)});
}

static auto NativeSetEntityValueAsAny(void* entity_ptr, int32_t prop_index, MonoString* value) -> MonoString*
{
    FO_STACK_TRACE_ENTRY();

    try {
        NativeSetEntityValueAsAnyImpl(entity_ptr, prop_index, value);
        return nullptr;
    }
    catch (const std::exception& ex) {
        return MakeManagedNativeError(ex);
    }
}

// === Native ABI: hex direction helpers ===

// Backs the managed `mdir.hex` property (parity with AngelScript mdir::get_hex). The conversion is
// geometry-dependent (hex vs square map dirs), so it routes through the engine rather than a managed mirror
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

static auto ResolveAbiInner(ptr<ManagedScriptBackend> backend, int32_t entry_id) -> const ManagedAbiInnerRuntime&
{
    FO_STACK_TRACE_ENTRY();

    auto abi = backend->GetAbi();
    FO_VERIFY_AND_THROW(abi, "Managed ABI tables are not built");
    FO_VERIFY_AND_THROW(abi->Bound, "Managed ABI manifest is not bound");
    FO_VERIFY_AND_THROW(entry_id >= 0 && numeric_cast<size_t>(entry_id) < abi->Inners.size(), "Managed inner-entry id is out of range", entry_id, abi->Inners.size());
    return abi->Inners[numeric_cast<size_t>(entry_id)];
}

static auto NativeCreateInnerEntity(void* holder_ptr, int32_t entry_id, void* proto_id) -> void*
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    ptr<EntityManagerApi> entity_mngr = GetActiveEntityManagerOrThrow();
    auto holder = ResolveEntity(backend, holder_ptr);
    ValidateEntityAccess(holder);

    hstring entry = ResolveAbiInner(backend, entry_id).Entry;
    return entity_mngr->CreateCustomInnerEntity(holder, entry, NativeHstringFromHandle(proto_id)).get();
}

static auto NativeHasInnerEntities(void* holder_ptr, int32_t entry_id) -> mono_bool
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    auto holder = ResolveEntity(backend, holder_ptr);
    ValidateEntityAccess(holder);

    hstring entry = ResolveAbiInner(backend, entry_id).Entry;
    auto entities = holder->GetInnerEntities(entry);
    return static_cast<mono_bool>(entities ? 1 : 0);
}

static auto NativeGetInnerEntity(void* holder_ptr, int32_t entry_id, int64_t id) -> void*
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    auto holder = ResolveEntity(backend, holder_ptr);
    ValidateEntityAccess(holder);

    hstring entry = ResolveAbiInner(backend, entry_id).Entry;
    ident_t entity_id {id};
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

static auto NativeFillInnerEntities(void* holder_ptr, int32_t entry_id, void** buffer, int32_t capacity, MonoString** error) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(error != nullptr, "Managed inner-entity fill error output is null");
    *error = nullptr;

    try {
        auto backend = GetActiveBackendOrThrow();
        auto holder = ResolveEntity(backend, holder_ptr);
        ValidateEntityAccess(holder);

        hstring entry = ResolveAbiInner(backend, entry_id).Entry;
        auto entities = CollectManagedInnerEntities(backend, holder, entry);
        int32_t required = numeric_cast<int32_t>(entities.size());

        if (capacity < required) {
            return required;
        }

        FO_VERIFY_AND_THROW(required == 0 || buffer != nullptr, "Managed inner-entity fill buffer is null");

        for (int32_t i = 0; i < required; i++) {
            buffer[i] = entities[numeric_cast<size_t>(i)].get_no_const();
        }

        return required;
    }
    catch (const std::exception& ex) {
        *error = MakeManagedNativeError(ex);
        return 0;
    }
}

static auto NativeGetAndResetTypedCallbackDispatches() -> int64_t
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    auto caches = backend->GetCaches();
    FO_VERIFY_AND_THROW(caches, "Managed backend caches are not created");

    caches->CountDispatches.store(true, std::memory_order_relaxed);
    return numeric_cast<int64_t>(caches->TypedCallbackDispatches.exchange(0, std::memory_order_relaxed));
}

static auto NativeGetAndResetBoxedCallbackDispatches() -> int64_t
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    auto caches = backend->GetCaches();
    FO_VERIFY_AND_THROW(caches, "Managed backend caches are not created");

    caches->CountDispatches.store(true, std::memory_order_relaxed);
    return numeric_cast<int64_t>(caches->BoxedCallbackDispatches.exchange(0, std::memory_order_relaxed));
}

// Interop probe: drives InteropProbe.AdaptProbe from a native loop over one transport and returns the loop time in
// nanoseconds. Modes mirror InteropProbe.CallbackMode; the probe never touches a production registration
static auto NativeProbeCallbackTransport(MonoObject* handler, int32_t mode, int32_t iterations, void* uco_entry, int32_t registration_id, MonoString** error) -> int64_t
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(error != nullptr, "Managed probe error output is null");
    *error = nullptr;

    try {
        // The probe's own setup is not the bridge work it measures, so counting resumes when the timed loop starts
        bool counting = InteropThreadCounters.Enabled;
        InteropThreadCounters.Enabled = false;
        auto resume_counting = scope_exit([counting]() noexcept { InteropThreadCounters.Enabled = counting; });
        auto start_timing = [counting] {
            InteropThreadCounters.Enabled = counting;
            return nanotime::now();
        };

        auto backend = GetActiveBackendOrThrow();
        FO_VERIFY_AND_THROW(handler != nullptr, "Managed probe handler is null");
        FO_VERIFY_AND_THROW(iterations > 0, "Managed probe iteration count must be positive", iterations);

        using ThunkEntry = void(FO_MANAGED_ENTRY_CALLCONV*)(MonoObject*, uint8_t*, int32_t, MonoObject**);
        using UcoEntry = void(FO_MANAGED_ENTRY_CALLCONV*)(int32_t, uint8_t*, int32_t);

        MonoMethod* adapter = FindCoreScriptMethod(backend, "InteropProbe", "AdaptProbe", 3);
        array<int32_t, 4> values {1, 2, 3, 4};
        array<uint8_t, sizeof(values)> frame {};
        memory::copy(frame.data(), values.data(), sizeof(values));
        int32_t frame_size = numeric_cast<int32_t>(frame.size());
        void* adapter_args[] = {handler, frame.data(), &frame_size};
        nanotime start_time;

        if (mode == 0) {
            start_time = start_timing();

            for (int32_t i = 0; i < iterations; i++) {
                MonoObject* exception = nullptr;
                (void)mono_runtime_invoke(adapter, nullptr, adapter_args, &exception);
                ThrowIfManagedException(exception, "Managed probe adapter failed");
            }
        }
        else if (mode == 1) {
            // The thunk is compiled once outside the timed loop, as a registration would cache it
            ThunkEntry thunk = reinterpret_cast<ThunkEntry>(mono_method_get_unmanaged_thunk(adapter));
            FO_VERIFY_AND_THROW(thunk != nullptr, "Managed probe thunk was not created");
            start_time = start_timing();

            for (int32_t i = 0; i < iterations; i++) {
                MonoObject* exception = nullptr;
                thunk(handler, frame.data(), frame_size, &exception);
                ThrowIfManagedException(exception, "Managed probe adapter failed");
            }
        }
        else if (mode == 2) {
            UcoEntry entry = reinterpret_cast<UcoEntry>(uco_entry);
            FO_VERIFY_AND_THROW(entry != nullptr, "Managed probe unmanaged entry is null");
            start_time = start_timing();

            for (int32_t i = 0; i < iterations; i++) {
                entry(registration_id, frame.data(), frame_size);
            }
        }
        else if (mode == 3) {
            start_time = start_timing();

            for (int32_t i = 0; i < iterations; i++) {
                (void)InvokeManagedScript(adapter, nullptr, adapter_args, "Managed probe adapter failed");
            }
        }
        else if (mode == 4 || mode == 9) {
            nptr<EngineMetadata> meta = backend->GetMetadata();
            FO_VERIFY_AND_THROW(meta, "Backend metadata is not available");
            ComplexTypeDesc int_type = meta->ResolveComplexType("int32");

            // The production plan with the probe adapter in place of a generated one
            ManagedCallbackPlan plan;
            plan.Args.assign(values.size(), int_type);
            plan.Layout = BuildManagedAbiCallbackLayout(plan.Ret, plan.Args);
            FO_VERIFY_AND_THROW(plan.Layout.Supported && plan.Layout.FrameSize == frame.size(), "Managed probe layout does not match its frame");
            plan.Adapter = adapter;
            plan.Engine = meta.dyn_cast<BaseEngine>();

            uint32_t handler_handle = NewManagedGcHandle(handler, false);
            FO_VERIFY_AND_THROW(handler_handle != 0, "Can't root Managed probe handler");
            auto release_handler_handle = scope_exit([handler_handle]() noexcept { mono_gchandle_free(handler_handle); });

            array<ptr<void>, 4> args_ptrs {make_ptr(&values[0]).void_cast(), make_ptr(&values[1]).void_cast(), make_ptr(&values[2]).void_cast(), make_ptr(&values[3]).void_cast()};
            start_time = start_timing();

            for (int32_t i = 0; i < iterations; i++) {
                FuncCallData call {.Accessor = &MANAGED_DATA_ACCESSOR};
                call.ArgsData = const_span<ptr<void>> {args_ptrs.data(), args_ptrs.size()};

                // Mode 9 skips the script-context entry, which isolates what that entry costs
                if (mode == 4) {
                    DispatchManagedCallback(backend, handler_handle, plan, call);
                }
                else {
                    DispatchManagedCallbackInContext(backend, handler_handle, plan, call);
                }
            }
        }
        else if (mode >= 5 && mode <= 8) {
            // One piece of the dispatch scaffolding alone, with no managed call inside
            nptr<EngineMetadata> meta = backend->GetMetadata();
            nptr<BaseEngine> engine = meta.dyn_cast<BaseEngine>();
            FO_VERIFY_AND_THROW(engine, "Managed probe requires an engine context");
            MonoDomain* domain = GetDomainOrThrow(backend->GetDomain());
            function<MonoObject*()> get_entry = [handler] { return handler; };
            function<void()> empty_callback = [] { };
            start_time = start_timing();

            for (int32_t i = 0; i < iterations; i++) {
                if (mode == 5) {
                    (void)engine->RunScriptContext(empty_callback);
                }
                else if (mode == 6) {
                    ManagedScriptEntryScope entry {adapter};
                    entry.Leave();
                }
                else if (mode == 7) {
                    ActiveBackendScope active_backend {backend};
                    ManagedThreadAttachment managed_thread {domain};
                }
                else {
                    time_meter run_time;
                    ReportManagedScriptOverrun(backend, engine, run_time.get_duration(), timespan::zero, get_entry);
                }
            }
        }
        else {
            throw ScriptSystemException("Unknown managed probe mode", mode);
        }

        return (nanotime::now() - start_time).nanoseconds();
    }
    catch (const std::exception& ex) {
        *error = MakeManagedNativeError(ex);
        return 0;
    }
}

// Interop probe: calls one probe adapter over one transport `iterations` times, optionally from a thread of its own,
// and reports how many calls came back with a managed exception; the managed side checks what the handler received
static void NativeProbeTransportScenario(MonoObject* handler, int32_t transport, int32_t adapter_kind, int32_t iterations, mono_bool external_thread, void* uco_entry, int32_t registration_id, int32_t* faults, MonoString** error)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(faults != nullptr && error != nullptr, "Managed probe outputs are null");
    *faults = 0;
    *error = nullptr;

    try {
        auto backend = GetActiveBackendOrThrow();
        FO_VERIFY_AND_THROW(handler != nullptr, "Managed probe handler is null");
        FO_VERIFY_AND_THROW(iterations > 0, "Managed probe iteration count must be positive", iterations);
        FO_VERIFY_AND_THROW(transport >= 0 && transport <= 2, "Unknown managed probe transport", transport);
        FO_VERIFY_AND_THROW(transport != 2 || uco_entry != nullptr, "Managed probe unmanaged entry is null");
        nptr<EngineMetadata> meta = backend->GetMetadata();
        FO_VERIFY_AND_THROW(meta, "Backend metadata is not available");

        using ThunkEntry = void(FO_MANAGED_ENTRY_CALLCONV*)(MonoObject*, uint8_t*, int32_t, MonoObject**);
        using UcoEntry = void(FO_MANAGED_ENTRY_CALLCONV*)(int32_t, uint8_t*, int32_t);

        // The mixed frame mirrors a generated one: enum, bool, 64-bit value, value type and hashed string at their slots
        MonoMethod* adapter = adapter_kind == 0 ? FindCoreScriptMethod(backend, "InteropProbe", "AdaptProbe", 3) : FindCoreScriptMethod(backend, "InteropProbe", "AdaptProbeMixed", 3);
        array<uint8_t, 32> frame {};
        int32_t frame_size = 0;

        if (adapter_kind == 0) {
            array<int32_t, 4> values {1, 2, 3, 4};
            memory::copy(frame.data(), values.data(), sizeof(values));
            frame_size = numeric_cast<int32_t>(sizeof(values));
        }
        else {
            int32_t mode_value = 2;
            uint8_t flag = 1;
            int64_t wide = 0x1122334455667788;
            array<int16_t, 2> hex {7, -9};
            hstring tag = meta->Hashes.to_hashed_string("InteropProbeTag");
            memory::copy(frame.data(), &mode_value, sizeof(mode_value));
            memory::copy(frame.data() + 4, &flag, sizeof(flag));
            memory::copy(frame.data() + 8, &wide, sizeof(wide));
            memory::copy(frame.data() + 16, hex.data(), sizeof(hex));
            memory::copy(frame.data() + 24, &tag, sizeof(tag));
            frame_size = 32;
        }

        // A collection inside the handler may move it, so the loop re-reads it from a handle on every call
        uint32_t handler_handle = NewManagedGcHandle(handler, false);
        FO_VERIFY_AND_THROW(handler_handle != 0, "Can't root Managed probe handler");
        auto release_handler_handle = scope_exit([handler_handle]() noexcept { mono_gchandle_free(handler_handle); });

        ThunkEntry thunk = transport == 1 ? reinterpret_cast<ThunkEntry>(mono_method_get_unmanaged_thunk(adapter)) : nullptr;
        FO_VERIFY_AND_THROW(transport != 1 || thunk != nullptr, "Managed probe thunk was not created");
        MonoDomain* domain = GetDomainOrThrow(backend->GetDomain());
        int32_t fault_count = 0;

        auto run_calls = [&] {
            ActiveBackendScope active_backend {backend};
            ManagedThreadAttachment managed_thread {domain};

            for (int32_t i = 0; i < iterations; i++) {
                MonoObject* exception = nullptr;

                if (transport == 0) {
                    void* adapter_args[] = {mono_gchandle_get_target(handler_handle), frame.data(), &frame_size};
                    (void)mono_runtime_invoke(adapter, nullptr, adapter_args, &exception);
                }
                else if (transport == 1) {
                    thunk(mono_gchandle_get_target(handler_handle), frame.data(), frame_size, &exception);
                }
                else {
                    reinterpret_cast<UcoEntry>(uco_entry)(registration_id, frame.data(), frame_size);
                }

                if (exception != nullptr) {
                    fault_count++;
                }
            }
        };

        if (external_thread != 0) {
            std::exception_ptr failure;
            auto worker = run_thread("InteropProbe", [&]() {
                try {
                    run_calls();
                }
                catch (const std::exception&) {
                    failure = std::current_exception();
                }
            });

            // Waiting on another managed thread is a blocking interval, so this thread lets the collector run meanwhile
            void* stack_data = nullptr;
            void* gc_safe_cookie = mono_threads_enter_gc_safe_region_unbalanced(&stack_data);
            worker.join();
            mono_threads_exit_gc_safe_region_unbalanced(gc_safe_cookie, &stack_data);

            if (failure) {
                std::rethrow_exception(failure);
            }
        }
        else {
            run_calls();
        }

        *faults = fault_count;
    }
    catch (const std::exception& ex) {
        *error = MakeManagedNativeError(ex);
    }
}

// Interop probe: switches the calling thread's counters on or off and reads them; native allocation counts exist
// only in profiling builds, which the return value reports
static auto NativeReadInteropCounters(mono_bool enable, int64_t* gc_handles, int64_t* metadata_lookups, int64_t* managed_objects, int64_t* native_allocations, int64_t* native_bytes) -> mono_bool
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(gc_handles != nullptr && metadata_lookups != nullptr && managed_objects != nullptr && native_allocations != nullptr && native_bytes != nullptr, "Managed probe counter outputs are null");

    InteropThreadCounters.Enabled = enable != 0;
    *gc_handles = numeric_cast<int64_t>(InteropThreadCounters.GcHandles);
    *metadata_lookups = numeric_cast<int64_t>(InteropThreadCounters.MetadataLookups);
    *managed_objects = numeric_cast<int64_t>(InteropThreadCounters.ManagedObjects);

    uint64_t allocation_count = 0;
    uint64_t allocated_bytes = 0;
    bool native_available = memory::get_thread_allocations(allocation_count, allocated_bytes);
    *native_allocations = numeric_cast<int64_t>(allocation_count);
    *native_bytes = numeric_cast<int64_t>(allocated_bytes);
    return native_available ? 1 : 0;
}

static auto NativeGetAndResetInnerEntityVisits() -> int64_t
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    auto abi = backend->GetAbi();

    if (!abi) {
        return 0;
    }

    abi->CountInnerEntityVisits.store(true, std::memory_order_relaxed);
    return numeric_cast<int64_t>(abi->InnerEntityVisits.exchange(0, std::memory_order_relaxed));
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

    string value = GetSettingValueAsString(name);
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

    string value = GetSettingValueAsString(name);
    return mono_string_new(GetDomainOrThrow(mono_domain_get()), value.c_str());
}

static void NativeSetSettingString(MonoString* name, MonoString* value)
{
    FO_STACK_TRACE_ENTRY();

    SetSettingValueFromString(name, ToStringAndFree(value));
}

static auto SettingKindSize(ManagedAbiValueKind kind) -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    switch (kind) {
    case ManagedAbiValueKind::Bool:
    case ManagedAbiValueKind::Int8:
    case ManagedAbiValueKind::UInt8:
        return 1;
    case ManagedAbiValueKind::Int16:
    case ManagedAbiValueKind::UInt16:
        return 2;
    case ManagedAbiValueKind::Int32:
    case ManagedAbiValueKind::UInt32:
    case ManagedAbiValueKind::Float32:
    case ManagedAbiValueKind::Enum:
        return 4;
    case ManagedAbiValueKind::Int64:
    case ManagedAbiValueKind::UInt64:
    case ManagedAbiValueKind::Float64:
        return 8;
    default:
        return 0;
    }
}

static auto ResolveAbiSetting(ptr<ManagedScriptBackend> backend, int32_t setting_id) -> ManagedAbiSettingRuntime&
{
    FO_STACK_TRACE_ENTRY();

    auto abi = backend->GetAbi();
    FO_VERIFY_AND_THROW(abi, "Managed ABI tables are not built");
    FO_VERIFY_AND_THROW(abi->Bound, "Managed ABI manifest is not bound");
    FO_VERIFY_AND_THROW(setting_id >= 0 && numeric_cast<size_t>(setting_id) < abi->Settings.size(), "Managed setting id is out of range", setting_id, abi->Settings.size());
    return abi->Settings[numeric_cast<size_t>(setting_id)];
}

static void WriteSettingBytes(void* dst, int32_t size, const void* src, size_t src_size)
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(dst, "Managed setting value buffer is null");
    FO_VERIFY_AND_THROW(numeric_cast<size_t>(size) == src_size, "Managed setting value size mismatch", size, src_size);
    memory::copy(ptr<void> {dst}, src, src_size);
}

static auto NativeGetSettingValue(int32_t setting_id, void* value, int32_t size) -> MonoString*
{
    FO_STACK_TRACE_ENTRY();

    try {
        auto backend = GetActiveBackendOrThrow();
        ManagedAbiSettingRuntime& entry = ResolveAbiSetting(backend, setting_id);
        FO_VERIFY_AND_THROW(entry.UsesTypedBridge, "Managed setting does not use the typed bridge", entry.Name);
        FO_VERIFY_AND_THROW(size == SettingKindSize(entry.Kind), "Managed setting value size mismatch", entry.Name, size, SettingKindSize(entry.Kind));
        FO_VERIFY_AND_THROW(numeric_cast<size_t>(size) <= sizeof(uint64_t), "Managed setting value exceeds the typed cell", entry.Name, size);

        nptr<GlobalSettings> settings = GetBackendSettings(backend);
        FO_VERIFY_AND_THROW(settings, "Managed settings backend is not available");

        if (entry.Builtin) {
            switch (entry.Kind) {
            case ManagedAbiValueKind::Bool: {
                bool parsed = entry.Builtin->ReadBool(settings);
                WriteSettingBytes(value, size, &parsed, sizeof(parsed));
                break;
            }
            case ManagedAbiValueKind::Int8: {
                int8_t parsed = numeric_cast<int8_t>(entry.Builtin->ReadSigned(settings));
                WriteSettingBytes(value, size, &parsed, sizeof(parsed));
                break;
            }
            case ManagedAbiValueKind::Int16: {
                int16_t parsed = numeric_cast<int16_t>(entry.Builtin->ReadSigned(settings));
                WriteSettingBytes(value, size, &parsed, sizeof(parsed));
                break;
            }
            case ManagedAbiValueKind::Int32:
            case ManagedAbiValueKind::Enum: {
                int32_t parsed = numeric_cast<int32_t>(entry.Builtin->ReadSigned(settings));
                WriteSettingBytes(value, size, &parsed, sizeof(parsed));
                break;
            }
            case ManagedAbiValueKind::Int64: {
                int64_t parsed = entry.Builtin->ReadSigned(settings);
                WriteSettingBytes(value, size, &parsed, sizeof(parsed));
                break;
            }
            case ManagedAbiValueKind::UInt8: {
                uint8_t parsed = numeric_cast<uint8_t>(entry.Builtin->ReadUnsigned(settings));
                WriteSettingBytes(value, size, &parsed, sizeof(parsed));
                break;
            }
            case ManagedAbiValueKind::UInt16: {
                uint16_t parsed = numeric_cast<uint16_t>(entry.Builtin->ReadUnsigned(settings));
                WriteSettingBytes(value, size, &parsed, sizeof(parsed));
                break;
            }
            case ManagedAbiValueKind::UInt32: {
                uint32_t parsed = numeric_cast<uint32_t>(entry.Builtin->ReadUnsigned(settings));
                WriteSettingBytes(value, size, &parsed, sizeof(parsed));
                break;
            }
            case ManagedAbiValueKind::UInt64: {
                uint64_t parsed = entry.Builtin->ReadUnsigned(settings);
                WriteSettingBytes(value, size, &parsed, sizeof(parsed));
                break;
            }
            case ManagedAbiValueKind::Float32: {
                float32_t parsed = numeric_cast<float32_t>(entry.Builtin->ReadFloat(settings));
                WriteSettingBytes(value, size, &parsed, sizeof(parsed));
                break;
            }
            case ManagedAbiValueKind::Float64: {
                float64_t parsed = entry.Builtin->ReadFloat(settings);
                WriteSettingBytes(value, size, &parsed, sizeof(parsed));
                break;
            }
            default:
                throw ScriptSystemException("Unsupported Managed typed setting", entry.Name);
            }

            return nullptr;
        }

        // A project custom setting is text in the custom map; the parsed value is kept in the ABI entry and re-read
        // only after the map changed, so a warmed read is a generation compare and a copy
        uint64_t generation = settings->GetCustomSettingsGeneration();

        if (entry.CustomGeneration.load(std::memory_order_acquire) == generation) {
            uint64_t cached = entry.CustomValue.load(std::memory_order_relaxed);
            WriteSettingBytes(value, size, &cached, numeric_cast<size_t>(size));
            return nullptr;
        }

        const string& text = settings->GetCustomSetting(entry.Name);

        switch (entry.Kind) {
        case ManagedAbiValueKind::Bool: {
            bool parsed = strvex(text).to_bool();
            WriteSettingBytes(value, size, &parsed, sizeof(parsed));
            break;
        }
        case ManagedAbiValueKind::Int8: {
            int8_t parsed = numeric_cast<int8_t>(strex(text).to_int32());
            WriteSettingBytes(value, size, &parsed, sizeof(parsed));
            break;
        }
        case ManagedAbiValueKind::Int16: {
            int16_t parsed = numeric_cast<int16_t>(strex(text).to_int32());
            WriteSettingBytes(value, size, &parsed, sizeof(parsed));
            break;
        }
        case ManagedAbiValueKind::Int32:
        case ManagedAbiValueKind::Enum: {
            int32_t parsed = strex(text).to_int32();
            WriteSettingBytes(value, size, &parsed, sizeof(parsed));
            break;
        }
        case ManagedAbiValueKind::Int64: {
            int64_t parsed = strex(text).to_int64();
            WriteSettingBytes(value, size, &parsed, sizeof(parsed));
            break;
        }
        case ManagedAbiValueKind::UInt8: {
            uint8_t parsed = numeric_cast<uint8_t>(strex(text).to_uint32());
            WriteSettingBytes(value, size, &parsed, sizeof(parsed));
            break;
        }
        case ManagedAbiValueKind::UInt16: {
            uint16_t parsed = numeric_cast<uint16_t>(strex(text).to_uint32());
            WriteSettingBytes(value, size, &parsed, sizeof(parsed));
            break;
        }
        case ManagedAbiValueKind::UInt32: {
            uint32_t parsed = strex(text).to_uint32();
            WriteSettingBytes(value, size, &parsed, sizeof(parsed));
            break;
        }
        case ManagedAbiValueKind::UInt64: {
            uint64_t parsed = std::strtoull(text.c_str(), nullptr, 0);
            WriteSettingBytes(value, size, &parsed, sizeof(parsed));
            break;
        }
        case ManagedAbiValueKind::Float32: {
            float32_t parsed = strex(text).to_float32();
            WriteSettingBytes(value, size, &parsed, sizeof(parsed));
            break;
        }
        case ManagedAbiValueKind::Float64: {
            float64_t parsed = strex(text).to_float64();
            WriteSettingBytes(value, size, &parsed, sizeof(parsed));
            break;
        }
        default:
            throw ScriptSystemException("Unsupported Managed typed setting", entry.Name);
        }

        uint64_t parsed_bits = 0;
        memory::copy(&parsed_bits, value, numeric_cast<size_t>(size));
        entry.CustomValue.store(parsed_bits, std::memory_order_relaxed);
        entry.CustomGeneration.store(generation, std::memory_order_release);
        return nullptr;
    }
    catch (const std::exception& ex) {
        return MakeManagedNativeError(ex);
    }
}

// === Native ABI: events, properties, methods and remote calls ===

static auto ResolveAbiEvent(ptr<ManagedScriptBackend> backend, int32_t event_id) -> const ManagedAbiEventRuntime&
{
    FO_STACK_TRACE_ENTRY();

    auto abi = backend->GetAbi();
    FO_VERIFY_AND_THROW(abi, "Managed ABI tables are not built");
    FO_VERIFY_AND_THROW(abi->Bound, "Managed ABI manifest is not bound");
    FO_VERIFY_AND_THROW(event_id >= 0 && numeric_cast<size_t>(event_id) < abi->Events.size(), "Managed event id is out of range", event_id, abi->Events.size());
    return abi->Events[numeric_cast<size_t>(event_id)];
}

static void NativeSubscribeEvent(int32_t event_id, void* entity_ptr, MonoObject* handler, mono_bool has_explicit_result, int32_t priority)
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    nptr<EngineMetadata> meta = backend->GetMetadata();
    FO_VERIFY_AND_THROW(meta, "Backend metadata is not available");

    if (handler == nullptr) {
        throw ScriptSystemException("Null Managed event handler");
    }

    const ManagedAbiEventRuntime& entry = ResolveAbiEvent(backend, event_id);
    FO_VERIFY_AND_THROW(entry.Desc && entry.Event, "Managed ABI event descriptor is null", entry.Owner, entry.Name);

    nptr<Entity> entity = ResolveEventEntity(backend, entry, entity_ptr);
    FO_VERIFY_AND_THROW(entity, "Managed event target is destroyed", entry.Owner, entry.Name);

    // A handler holds one subscription per entity event, whichever wrapper of the entity it arrives through
    if (FindManagedEventSubscription(backend, entity, entry.Name, handler).has_value()) {
        return;
    }

    auto subscription = safe_alloc::make_shared<ManagedEventSubscription>();
    subscription->Backend = backend;
    subscription->Domain = GetDomainOrThrow(backend->GetDomain());
    subscription->Handler = NewManagedGcHandle(handler, false);
    subscription->HasExplicitResult = has_explicit_result != 0;
    subscription->EventId = event_id;
    subscription->UsesScalarFrame = entry.UsesScalarFrame;
    subscription->Slots = entry.Args;
    subscription->FrameSize = entry.FrameSize;

    if (entry.UsesScalarFrame) {
        auto caches = backend->GetCaches();
        FO_VERIFY_AND_THROW(caches && numeric_cast<size_t>(event_id) < caches->EventAdapters.size(), "Managed event adapters are not resolved", entry.Owner, entry.Name);
        subscription->AdaptInvoke = caches->EventAdapters[numeric_cast<size_t>(event_id)];
        FO_VERIFY_AND_THROW(subscription->AdaptInvoke, "Managed event AdaptInvoke adapter is missing", entry.Owner, entry.Name);
    }

    if (!entry.IsGlobal) {
        subscription->Args.emplace_back(meta->ResolveComplexType(entry.Owner));
    }
    for (const ArgDesc& arg : entry.Event->Args) {
        if (!IsManagedBridgeType(arg.Type)) {
            throw ScriptSystemException("Managed event argument type is not supported yet", entry.Owner, entry.Name, arg.Name);
        }

        subscription->Args.emplace_back(arg.Type);
    }

    Entity::EventCallbackData event_data;
    event_data.Callback = [subscription](FuncCallData& call) -> Entity::EventResult { return DispatchManagedEvent(subscription, call); };
    event_data.SubscriptionPtr = subscription->Handler;
    event_data.SubscriptionOwner = GetManagedEventSubscriptionOwner(backend);
    event_data.Priority = static_cast<Entity::EventPriority>(priority);
    event_data.HasExplicitResult = has_explicit_result != 0;

    entity->SubscribeEvent(entry.Name, std::move(event_data));
}

static void NativeUnsubscribeEvent(int32_t event_id, void* entity_ptr, MonoObject* handler)
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    const ManagedAbiEventRuntime& entry = ResolveAbiEvent(backend, event_id);
    nptr<Entity> entity = ResolveEventEntity(backend, entry, entity_ptr);

    if (!entity || handler == nullptr) {
        return;
    }

    if (optional<uintptr_t> subscription = FindManagedEventSubscription(backend, entity, entry.Name, handler); subscription.has_value()) {
        entity->UnsubscribeEvent(entry.Name, subscription.value());
    }
}

static void NativeUnsubscribeAllEvents(int32_t event_id, void* entity_ptr)
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    const ManagedAbiEventRuntime& entry = ResolveAbiEvent(backend, event_id);
    nptr<Entity> entity = ResolveEventEntity(backend, entry, entity_ptr);

    if (!entity) {
        return;
    }

    // Only this backend's subscriptions: native observers of the same event are not script state
    for (uintptr_t subscription : entity->GetEventSubscriptions(entry.Name, GetManagedEventSubscriptionOwner(backend))) {
        entity->UnsubscribeEvent(entry.Name, subscription);
    }
}

// Null for a destroyed entity, which has already dropped every subscription it held
static auto ResolveEventEntity(ptr<ManagedScriptBackend> backend, const ManagedAbiEventRuntime& entry, void* entity_ptr) -> nptr<Entity>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(entry.IsGlobal == (entity_ptr == nullptr), "Managed event target does not match the event owner", entry.Owner, entry.Name);

    nptr<Entity> entity = entity_ptr ? nptr<Entity>(static_cast<Entity*>(entity_ptr)) : backend->GetGlobalEntity();
    FO_VERIFY_AND_THROW(entity, "Managed event target is null", entry.Owner, entry.Name);

    if (entity->IsDestroyed()) {
        return nullptr;
    }

    return entity;
}

// Handlers match the way C# delegates do, so a method group converted again still names its subscription
static auto FindManagedEventSubscription(ptr<ManagedScriptBackend> backend, ptr<const Entity> entity, string_view event_name, MonoObject* handler) -> optional<uintptr_t>
{
    FO_STACK_TRACE_ENTRY();

    for (uintptr_t subscription : entity->GetEventSubscriptions(event_name, GetManagedEventSubscriptionOwner(backend))) {
        MonoObject* subscribed = mono_gchandle_get_target(numeric_cast<uint32_t>(subscription));

        if (subscribed == handler) {
            return subscription;
        }

        if (subscribed != nullptr) {
            void* args[] = {subscribed, handler};
            MonoObject* equal = InvokeNativeHelper(backend, "EventHandlersEqual", 2, args);
            FO_VERIFY_AND_THROW(equal != nullptr, "Managed event handler comparison returned null", event_name);

            if (*static_cast<mono_bool*>(mono_object_unbox(equal)) != 0) {
                return subscription;
            }
        }
    }

    return std::nullopt;
}

static auto GetManagedEventSubscriptionOwner(ptr<const ManagedScriptBackend> backend) noexcept -> uintptr_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return std::bit_cast<uintptr_t>(backend.get());
}

static auto NativeFireEventImpl(const ManagedAbiEventRuntime& entry, void* entity_ptr, MonoArray* args) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    FO_VERIFY_AND_THROW(entry.Desc && entry.Event, "Managed ABI event descriptor is null", entry.Owner, entry.Name);

    auto entity = ResolveEntity(backend, entity_ptr);
    size_t args_count = args != nullptr ? mono_array_length(args) : 0;

    if (args_count != entry.Event->Args.size()) {
        throw ScriptSystemException("Managed event argument count mismatch", entry.Owner, entry.Name, args_count, entry.Event->Args.size());
    }

    size_t first_event_arg = entry.IsGlobal ? 0 : 1;
    size_t call_args_count = args_count + first_event_arg;

    if (call_args_count > MAX_CALL_ARGS) {
        throw ScriptSystemException("Managed event argument count exceeds bridge limit", entry.Owner, entry.Name, call_args_count);
    }

    uint32_t args_handle = args != nullptr ? NewManagedGcHandle(reinterpret_cast<MonoObject*>(args), 0) : 0;
    auto free_args_handle = scope_exit([args_handle]() noexcept {
        if (args_handle != 0) {
            mono_gchandle_free(args_handle);
        }
    });
    auto get_args = [args, args_handle]() -> MonoArray* { return args_handle != 0 ? reinterpret_cast<MonoArray*>(mono_gchandle_get_target(args_handle)) : args; };

    array<void*, MAX_CALL_ARGS> args_data {};
    array<ManagedNativeValue, MAX_CALL_ARGS> native_args {};
    Entity* self_entity = entity.get_no_const();

    if (!entry.IsGlobal) {
        args_data[0] = make_ptr(&self_entity).void_cast();
    }

    for (size_t i = 0; i < args_count; i++) {
        const ArgDesc& arg_desc = entry.Event->Args[i];

        if (!IsManagedBridgeType(arg_desc.Type)) {
            throw ScriptSystemException("Managed event argument type is not supported yet", entry.Owner, entry.Name, arg_desc.Name);
        }

        MonoObject* arg = mono_array_get(get_args(), MonoObject*, i);
        args_data[i + first_event_arg] = ConvertManagedObjectToNative(backend, arg_desc.Type, arg, native_args[i]);
    }

    small_vector<ptr<void>, MAX_CALL_ARGS> args_ptrs;
    args_ptrs.reserve(call_args_count);
    for (size_t arg_idx = 0; arg_idx < call_args_count; arg_idx++) {
        args_ptrs.emplace_back(args_data[arg_idx]);
    }

    FuncCallData call {.Accessor = &MANAGED_DATA_ACCESSOR};
    call.ArgsData = const_span<ptr<void>> {args_ptrs.data(), args_ptrs.size()};

    bool ref_type_owners_reconciled = false;
    auto reconcile_ref_type_owners = [&]() noexcept {
        for (size_t i = 0; i < args_count; i++) {
            ReconcileMutableDynamicRefTypeOwner(entry.Event->Args[i].Type, native_args[i]);
        }
        ref_type_owners_reconciled = true;
    };
    auto reconcile_ref_type_owners_on_exit = scope_exit([&]() noexcept {
        if (!ref_type_owners_reconciled) {
            reconcile_ref_type_owners();
        }
    });

    auto result = entity->FireEvent(entry.Name, call);
    reconcile_ref_type_owners();

    for (size_t i = 0; i < args_count; i++) {
        const ArgDesc& arg_desc = entry.Event->Args[i];

        if (!arg_desc.Type.IsMutable) {
            continue;
        }

        MonoObject* arg = BoxNativeCallValue(backend, arg_desc.Type, args_data[i + first_event_arg], call.Accessor.get());
        mono_array_setref(get_args(), i, arg);
    }

    return static_cast<int32_t>(result);
}

static auto NativeFireEventBoxed(int32_t event_id, void* entity_ptr, MonoArray* args, MonoString** error) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(error != nullptr, "Managed event fire error output is null");
    *error = nullptr;

    try {
        auto backend = GetActiveBackendOrThrow();
        return NativeFireEventImpl(ResolveAbiEvent(backend, event_id), entity_ptr, args);
    }
    catch (const std::exception& ex) {
        *error = MakeManagedNativeError(ex);
        return 0;
    }
}

static auto NativeFireEventIndexed(int32_t event_id, void* entity_ptr, void* frame, int32_t frame_size, MonoString** error) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(error != nullptr, "Managed event fire error output is null");
    *error = nullptr;

    try {
        auto backend = GetActiveBackendOrThrow();
        const ManagedAbiEventRuntime& entry = ResolveAbiEvent(backend, event_id);
        FO_VERIFY_AND_THROW(entry.UsesScalarFrame, "Managed event does not use a scalar frame", entry.Owner, entry.Name);
        FO_VERIFY_AND_THROW(entry.Event, "Managed ABI event descriptor is null", entry.Owner, entry.Name);
        FO_VERIFY_AND_THROW(frame != nullptr, "Managed scalar event frame is null", entry.Owner, entry.Name);
        FO_VERIFY_AND_THROW(frame_size == numeric_cast<int32_t>(entry.FrameSize), "Managed scalar event frame size mismatch", entry.Owner, entry.Name, frame_size, entry.FrameSize);
        FO_VERIFY_AND_THROW(entry.Args.size() == entry.Event->Args.size(), "Managed event ABI slot count mismatch", entry.Owner, entry.Name);

        auto entity = ResolveEntity(backend, entity_ptr);
        Entity* self_entity = entity.get_no_const();
        size_t first_event_arg = entry.IsGlobal ? 0 : 1;
        size_t call_args_count = entry.Args.size() + first_event_arg;
        FO_VERIFY_AND_THROW(call_args_count <= MAX_CALL_ARGS, "Managed event argument count exceeds bridge limit", entry.Owner, entry.Name, call_args_count);

        array<void*, MAX_CALL_ARGS> args_data {};

        if (!entry.IsGlobal) {
            args_data[0] = make_ptr(&self_entity).void_cast();
        }

        ManagedAbiNativeFrame native_frame = BuildManagedAbiNativeFrame({static_cast<uint8_t*>(frame), entry.FrameSize}, entry.Args);

        for (size_t i = 0; i < entry.Args.size(); i++) {
            const ManagedAbiSlot& slot = entry.Args[i];
            ptr<void> arg_data = GetManagedAbiNativeFrameArg(native_frame, i);

            // A handle slot holds the pointer itself, so the slot is the Entity* / ref pointer storage the call reads
            ValidateManagedFrameHandle(slot, entry.Event->Args[i], arg_data.reinterpret_as<const uint8_t>().get(), entry.Owner, entry.Name);
            args_data[i + first_event_arg] = arg_data.get();
        }

        small_vector<ptr<void>, MAX_CALL_ARGS> args_ptrs;
        args_ptrs.reserve(call_args_count);

        for (size_t arg_idx = 0; arg_idx < call_args_count; arg_idx++) {
            args_ptrs.emplace_back(args_data[arg_idx]);
        }

        FuncCallData call {.Accessor = &MANAGED_DATA_ACCESSOR};
        call.ArgsData = const_span<ptr<void>> {args_ptrs.data(), args_ptrs.size()};
        int32_t result = static_cast<int32_t>(entity->FireEvent(entry.Name, call));
        CopyBackManagedAbiNativeFrame(native_frame);
        return result;
    }
    catch (const std::exception& ex) {
        *error = MakeManagedNativeError(ex);
        return 0;
    }
}

static auto NativeGetPropertyImpl(void* entity_ptr, int32_t prop_index) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    auto entity = ResolveEntity(backend, entity_ptr);
    entity->LockForPropertyAccessShared();
    auto auto_unlock = scope_exit([entity]() mutable noexcept { entity->UnlockForPropertyAccessShared(); });

    entity->ValidateAccess();
    auto nullable_prop = entity->GetProperties()->GetRegistrar()->GetPropertyByIndex(prop_index);

    if (!nullable_prop) {
        throw ScriptSystemException("Managed property not found", entity->GetTypeName(), prop_index);
    }

    auto prop = nullable_prop.as_ptr();

    if (prop->IsDict()) {
        if (!IsManagedBridgeDictionaryProperty(prop)) {
            throw ScriptSystemException("Managed dictionary property type is not supported", prop->GetName());
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

static auto NativeGetProperty(void* entity_ptr, int32_t prop_index, MonoString** error) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(error != nullptr, "Managed property getter error output is null");
    *error = nullptr;

    try {
        return NativeGetPropertyImpl(entity_ptr, prop_index);
    }
    catch (const std::exception& ex) {
        *error = MakeManagedNativeError(ex);
        return nullptr;
    }
}

static void NativeSetPropertyImpl(void* entity_ptr, int32_t prop_index, MonoObject* value)
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    auto entity = ResolveEntity(backend, entity_ptr);
    entity->LockForPropertyAccess();
    auto auto_unlock = scope_exit([entity]() mutable noexcept { entity->UnlockForPropertyAccess(); });

    entity->ValidateAccess();
    auto nullable_prop = entity->GetProperties()->GetRegistrar()->GetPropertyByIndex(prop_index);

    if (!nullable_prop) {
        throw ScriptSystemException("Managed property not found", entity->GetTypeName(), prop_index);
    }

    auto prop = nullable_prop.as_ptr();

    if (prop->IsDict()) {
        if (!IsManagedBridgeDictionaryProperty(prop)) {
            throw ScriptSystemException("Managed dictionary property type is not supported", prop->GetName());
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

static auto NativeSetProperty(void* entity_ptr, int32_t prop_index, MonoObject* value) -> MonoString*
{
    FO_STACK_TRACE_ENTRY();

    try {
        NativeSetPropertyImpl(entity_ptr, prop_index, value);
        return nullptr;
    }
    catch (const std::exception& ex) {
        return MakeManagedNativeError(ex);
    }
}

static auto MakeManagedCsTypeName(const BaseTypeDesc& type) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type.IsBool) {
        return "bool";
    }
    if (type.IsInt8) {
        return "sbyte";
    }
    if (type.IsUInt8) {
        return "byte";
    }
    if (type.IsInt16) {
        return "short";
    }
    if (type.IsUInt16) {
        return "ushort";
    }
    if (type.IsInt32) {
        return "int";
    }
    if (type.IsUInt32) {
        return "uint";
    }
    if (type.IsInt64) {
        return "long";
    }
    if (type.IsUInt64) {
        return "ulong";
    }
    if (type.IsSingleFloat) {
        return "float";
    }
    if (type.IsDoubleFloat) {
        return "double";
    }
    if (type.IsEnum) {
        return type.Name;
    }

    return {};
}

static auto FindPropertyCallbackAdapter(ptr<ManagedScriptBackend> backend, string_view method_name) -> nptr<MonoMethod>
{
    FO_STACK_TRACE_ENTRY();

    string method_name_str {method_name};

    for (nptr<void> image_ptr : backend->GetImages()) {
        nptr<MonoImage> image = image_ptr.reinterpret_as<MonoImage>();
        MonoClass* klass = mono_class_from_name(image.get(), "FOnline", "PropertyCallbackAdapters");

        if (klass == nullptr) {
            continue;
        }

        if (MonoMethod* method = mono_class_get_method_from_name(klass, method_name_str.c_str(), 3)) {
            return method;
        }
    }

    return nullptr;
}

static auto IsManagedScalarProperty(ptr<const Property> prop) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const BaseTypeDesc& base_type = prop->GetBaseType();
    return !prop->IsNullable() && !prop->IsArray() && !prop->IsDict() && IsManagedAbiFixedPropertyValue(base_type);
}

static void NativeSetPropertyGetter(MonoString* owner_type, MonoString* property_name, MonoObject* getter)
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();

    if (getter == nullptr) {
        throw ScriptSystemException("Null Managed property getter");
    }

    string owner_type_name = ToStringAndFree(owner_type);
    auto prop = ResolveVirtualPropertyForCallback(backend, owner_type, property_name, true, true);
    uint32_t getter_handle = NewManagedGcHandle(getter, false);
    FO_VERIFY_AND_THROW(getter_handle != 0, "Can't root Managed property getter");
    auto release_getter_handle_on_error = scope_fail([getter_handle]() noexcept { mono_gchandle_free(getter_handle); });

    backend->AdoptPersistentGcHandle(getter_handle);
    release_getter_handle_on_error.release();

    nptr<MonoMethod> adapt;

    if (IsManagedScalarProperty(prop)) {
        adapt = FindPropertyCallbackAdapter(backend, strex("AdaptGetter_{}_{}", owner_type_name, MakeManagedCsTypeName(prop->GetBaseType())));
    }

    prop->SetGetter([backend, getter_handle, prop, owner_type_name, adapt](nptr<Entity> entity, ptr<const Property>) -> PropertyRawData FO_DEFERRED {
        nptr<BaseEngine> engine = backend->GetMetadata().dyn_cast<BaseEngine>();
        FO_VERIFY_AND_THROW(engine, "Managed property getter requires an engine context");

        PropertyRawData prop_data;
        RunManagedScriptEntry(
            backend, engine, [getter_handle] { return mono_gchandle_get_target(getter_handle); },
            [&] {
                ActiveBackendScope active_backend {backend};

                MonoDomain* domain = GetDomainOrThrow(backend->GetDomain());

                ManagedThreadAttachment managed_thread {domain};

                if (mono_gchandle_get_target(getter_handle) == nullptr) {
                    throw ScriptSystemException("Managed property getter delegate was collected", prop->GetName());
                }

                if (adapt) {
                    array<uint8_t, MANAGED_ABI_PROPERTY_ADAPTER_STORAGE> storage {};
                    FO_VERIFY_AND_THROW(prop->GetBaseSize() <= storage.size(), "Managed scalar property getter exceeds adapter storage", prop->GetName());
                    void* entity_ptr = entity.get_no_const();
                    void* args[] = {mono_gchandle_get_target(getter_handle), &entity_ptr, storage.data()};
                    (void)InvokeManagedScript(adapt.get_no_const(), nullptr, args, "Managed property getter failed");
                    ValueToPropertyData(prop->GetBaseType(), storage.data());
                    prop_data.Set(ptr<const void> {storage.data()}, prop->GetBaseSize());
                    return;
                }

                uint32_t args_array_handle = NewManagedGcHandle(reinterpret_cast<MonoObject*>(mono_array_new(domain, mono_get_object_class(), 1)), 0);
                auto free_args_array_handle = scope_exit([args_array_handle]() noexcept { mono_gchandle_free(args_array_handle); });
                auto get_args_array = [args_array_handle]() -> MonoArray* { return reinterpret_cast<MonoArray*>(mono_gchandle_get_target(args_array_handle)); };

                MonoObject* entity_obj = CreateEntityObject(backend, owner_type_name, entity);
                mono_array_setref(get_args_array(), 0, entity_obj);

                ManagedObjectRoot result;
                result.SetObject(InvokeManagedCallbackHandler(backend, mono_gchandle_get_target(getter_handle), get_args_array()));
                prop_data = ConvertManagedObjectToPropertyData(backend, prop, result.GetObject());
            });
        return prop_data;
    });
}

static void NativeAddPropertySetter(MonoString* owner_type, MonoString* property_name, MonoObject* setter)
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();

    if (setter == nullptr) {
        throw ScriptSystemException("Null Managed property setter");
    }

    string owner_type_name = ToStringAndFree(owner_type);
    auto prop = ResolveVirtualPropertyForCallback(backend, owner_type, property_name, false, true);
    uint32_t setter_handle = NewManagedGcHandle(setter, false);
    FO_VERIFY_AND_THROW(setter_handle != 0, "Can't root Managed property setter");
    auto release_setter_handle_on_error = scope_fail([setter_handle]() noexcept { mono_gchandle_free(setter_handle); });

    backend->AdoptPersistentGcHandle(setter_handle);
    release_setter_handle_on_error.release();

    nptr<MonoMethod> adapt;

    if (IsManagedScalarProperty(prop)) {
        adapt = FindPropertyCallbackAdapter(backend, strex("AdaptSetter_{}_{}", owner_type_name, MakeManagedCsTypeName(prop->GetBaseType())));
    }

    prop->AddSetter([backend, setter_handle, prop, owner_type_name, adapt](nptr<Entity> entity, ptr<const Property>, PropertyRawData& prop_data) FO_DEFERRED {
        nptr<BaseEngine> engine = backend->GetMetadata().dyn_cast<BaseEngine>();
        FO_VERIFY_AND_THROW(engine, "Managed property setter requires an engine context");

        RunManagedScriptEntry(
            backend, engine, [setter_handle] { return mono_gchandle_get_target(setter_handle); },
            [&] {
                ActiveBackendScope active_backend {backend};

                MonoDomain* domain = GetDomainOrThrow(backend->GetDomain());

                ManagedThreadAttachment managed_thread {domain};

                if (mono_gchandle_get_target(setter_handle) == nullptr) {
                    throw ScriptSystemException("Managed property setter delegate was collected", prop->GetName());
                }

                if (adapt) {
                    array<uint8_t, MANAGED_ABI_PROPERTY_ADAPTER_STORAGE> storage {};
                    FO_VERIFY_AND_THROW(prop->GetBaseSize() <= storage.size() && prop_data.GetSize() == prop->GetBaseSize(), "Managed scalar property setter size mismatch", prop->GetName());
                    memory::copy(ptr<void> {storage.data()}, prop_data.GetPtr(), prop->GetBaseSize());
                    PropertyDataToValue(backend, prop->GetBaseType(), storage.data());
                    void* entity_ptr = entity.get_no_const();
                    void* args[] = {mono_gchandle_get_target(setter_handle), &entity_ptr, storage.data()};
                    (void)InvokeManagedScript(adapt.get_no_const(), nullptr, args, "Managed property setter failed");
                    ValueToPropertyData(prop->GetBaseType(), storage.data());
                    prop_data.Set(ptr<const void> {storage.data()}, prop->GetBaseSize());
                    return;
                }

                uint32_t args_array_handle = NewManagedGcHandle(reinterpret_cast<MonoObject*>(mono_array_new(domain, mono_get_object_class(), 2)), 0);
                auto free_args_array_handle = scope_exit([args_array_handle]() noexcept { mono_gchandle_free(args_array_handle); });
                auto get_args_array = [args_array_handle]() -> MonoArray* { return reinterpret_cast<MonoArray*>(mono_gchandle_get_target(args_array_handle)); };

                MonoObject* entity_obj = CreateEntityObject(backend, owner_type_name, entity);
                mono_array_setref(get_args_array(), 0, entity_obj);
                MonoObject* value_obj = BoxPropertyValue(backend, prop, {prop_data.GetPtrAs<uint8_t>().get(), prop_data.GetSize()});
                mono_array_setref(get_args_array(), 1, value_obj);

                (void)InvokeManagedCallbackHandler(backend, mono_gchandle_get_target(setter_handle), get_args_array());

                MonoObject* modified_value = mono_array_get(get_args_array(), MonoObject*, 1);
                prop_data = ConvertManagedObjectToPropertyData(backend, prop, modified_value);
            });
    });
}

static void NativeAddPropertySetterWithProperty(MonoString* owner_type, MonoString* property_name, MonoObject* setter)
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();

    if (setter == nullptr) {
        throw ScriptSystemException("Null Managed property setter");
    }

    string owner_type_name = ToStringAndFree(owner_type);
    auto prop = ResolveVirtualPropertyForCallback(backend, owner_type, property_name, false, true);
    uint32_t setter_handle = NewManagedGcHandle(setter, false);
    FO_VERIFY_AND_THROW(setter_handle != 0, "Can't root Managed property setter");
    auto release_setter_handle_on_error = scope_fail([setter_handle]() noexcept { mono_gchandle_free(setter_handle); });

    backend->AdoptPersistentGcHandle(setter_handle);
    release_setter_handle_on_error.release();

    prop->AddSetter([backend, setter_handle, prop, owner_type_name](nptr<Entity> entity, ptr<const Property>, PropertyRawData& prop_data) FO_DEFERRED {
        nptr<BaseEngine> engine = backend->GetMetadata().dyn_cast<BaseEngine>();
        FO_VERIFY_AND_THROW(engine, "Managed property setter requires an engine context");

        RunManagedScriptEntry(
            backend, engine, [setter_handle] { return mono_gchandle_get_target(setter_handle); },
            [&] {
                ActiveBackendScope active_backend {backend};

                MonoDomain* domain = GetDomainOrThrow(backend->GetDomain());

                ManagedThreadAttachment managed_thread {domain};

                if (mono_gchandle_get_target(setter_handle) == nullptr) {
                    throw ScriptSystemException("Managed property setter delegate was collected", prop->GetName());
                }

                uint32_t args_array_handle = NewManagedGcHandle(reinterpret_cast<MonoObject*>(mono_array_new(domain, mono_get_object_class(), 3)), 0);
                auto free_args_array_handle = scope_exit([args_array_handle]() noexcept { mono_gchandle_free(args_array_handle); });
                auto get_args_array = [args_array_handle]() -> MonoArray* { return reinterpret_cast<MonoArray*>(mono_gchandle_get_target(args_array_handle)); };

                MonoObject* entity_obj = CreateEntityObject(backend, owner_type_name, entity);
                mono_array_setref(get_args_array(), 0, entity_obj);
                MonoObject* property_obj = CreatePropertyEnumObject(backend, owner_type_name, prop);
                mono_array_setref(get_args_array(), 1, property_obj);
                MonoObject* value_obj = BoxPropertyValue(backend, prop, {prop_data.GetPtrAs<uint8_t>().get(), prop_data.GetSize()});
                mono_array_setref(get_args_array(), 2, value_obj);

                (void)InvokeManagedCallbackHandler(backend, mono_gchandle_get_target(setter_handle), get_args_array());

                MonoObject* modified_value = mono_array_get(get_args_array(), MonoObject*, 2);
                prop_data = ConvertManagedObjectToPropertyData(backend, prop, modified_value);
            });
    });
}

static void NativeAddPropertyDeferredSetter(MonoString* owner_type, MonoString* property_name, MonoObject* setter)
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();

    if (setter == nullptr) {
        throw ScriptSystemException("Null Managed deferred property setter");
    }

    string owner_type_name = ToStringAndFree(owner_type);
    auto prop = ResolveVirtualPropertyForCallback(backend, owner_type, property_name, false, false);
    uint32_t setter_handle = NewManagedGcHandle(setter, false);
    FO_VERIFY_AND_THROW(setter_handle != 0, "Can't root Managed deferred property setter");
    auto release_setter_handle_on_error = scope_fail([setter_handle]() noexcept { mono_gchandle_free(setter_handle); });

    backend->AdoptPersistentGcHandle(setter_handle);
    release_setter_handle_on_error.release();

    // Reaction-only post-set callback: the managed delegate receives just the entity and runs after the value is
    // written
    prop->AddPostSetter([backend, setter_handle, prop, owner_type_name](nptr<Entity> entity, ptr<const Property>) FO_DEFERRED {
        nptr<BaseEngine> engine = backend->GetMetadata().dyn_cast<BaseEngine>();
        FO_VERIFY_AND_THROW(engine, "Managed deferred property setter requires an engine context");

        RunManagedScriptEntry(
            backend, engine, [setter_handle] { return mono_gchandle_get_target(setter_handle); },
            [&] {
                ActiveBackendScope active_backend {backend};

                MonoDomain* domain = GetDomainOrThrow(backend->GetDomain());

                ManagedThreadAttachment managed_thread {domain};

                if (mono_gchandle_get_target(setter_handle) == nullptr) {
                    throw ScriptSystemException("Managed deferred property setter delegate was collected", prop->GetName());
                }

                uint32_t args_array_handle = NewManagedGcHandle(reinterpret_cast<MonoObject*>(mono_array_new(domain, mono_get_object_class(), 1)), 0);
                auto free_args_array_handle = scope_exit([args_array_handle]() noexcept { mono_gchandle_free(args_array_handle); });
                auto get_args_array = [args_array_handle]() -> MonoArray* { return reinterpret_cast<MonoArray*>(mono_gchandle_get_target(args_array_handle)); };

                MonoObject* entity_obj = CreateEntityObject(backend, owner_type_name, entity);
                mono_array_setref(get_args_array(), 0, entity_obj);

                (void)InvokeManagedCallbackHandler(backend, mono_gchandle_get_target(setter_handle), get_args_array());
            });
    });
}

static auto ResolveAbiMethod(ptr<ManagedScriptBackend> backend, int32_t method_id, bool require_bound) -> const ManagedAbiMethodRuntime&
{
    FO_STACK_TRACE_ENTRY();

    auto abi = backend->GetAbi();
    FO_VERIFY_AND_THROW(abi, "Managed ABI tables are not built");

    if (require_bound) {
        FO_VERIFY_AND_THROW(abi->Bound, "Managed ABI manifest is not bound");
    }

    FO_VERIFY_AND_THROW(method_id >= 0 && numeric_cast<size_t>(method_id) < abi->Methods.size(), "Managed method id is out of range", method_id, abi->Methods.size());
    return abi->Methods[numeric_cast<size_t>(method_id)];
}

static auto NativeCallMethodImpl(const ManagedAbiMethodRuntime& entry, void* entity_ptr, MonoArray* args) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    auto backend = GetActiveBackendOrThrow();
    string_view owner_type_name = entry.Owner;
    string_view method_name_str = entry.Method ? string_view {entry.Method->Name} : string_view {};
    bool is_ref_type_method = entry.IsRefType;
    auto entity = !is_ref_type_method ? nptr<Entity> {ResolveEntity(backend, entity_ptr)} : nptr<Entity> {};
    size_t args_count = args != nullptr ? mono_array_length(args) : 0;
    uint32_t args_handle = args != nullptr ? NewManagedGcHandle(reinterpret_cast<MonoObject*>(args), 0) : 0;
    auto free_args_handle = scope_exit([args_handle]() noexcept {
        if (args_handle != 0) {
            mono_gchandle_free(args_handle);
        }
    });
    auto get_args = [args, args_handle]() -> MonoArray* { return args_handle != 0 ? reinterpret_cast<MonoArray*>(mono_gchandle_get_target(args_handle)) : args; };
    nptr<const MethodDesc> method = entry.Method;

    if (!method) {
        throw ScriptSystemException("Managed method not found", owner_type_name, method_name_str, args_count);
    }

    if (method->Args.size() != args_count) {
        throw ScriptSystemException("Managed method argument count mismatch", owner_type_name, method_name_str, args_count, method->Args.size());
    }

    bool is_ref_type_factory = is_ref_type_method && method->Name == "__Factory";
    size_t first_method_arg = is_ref_type_factory ? 0 : 1;
    array<void*, MAX_CALL_ARGS> args_data {};
    array<ManagedNativeValue, MAX_CALL_ARGS> native_args {};
    Entity* self_entity = entity.get_no_const();
    void* self_ref = entity_ptr;

    if (!is_ref_type_factory) {
        args_data[0] = is_ref_type_method ? &self_ref : make_ptr(&self_entity).void_cast();
    }

    if (is_ref_type_method && !is_ref_type_factory && self_ref == nullptr) {
        throw ScriptSystemException("Managed ref type target is null", owner_type_name, method_name_str);
    }

    for (size_t i = 0; i < args_count; i++) {
        MonoObject* arg = mono_array_get(get_args(), MonoObject*, i);
        args_data[i + first_method_arg] = ConvertManagedObjectToNative(backend, method->Args[i].Type, arg, native_args[i]);
    }

    small_vector<ptr<void>, MAX_CALL_ARGS> args_ptrs;
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
                ret_data = ret_storage.Alloc(ret_base);
            }
            else {
                throw ScriptSystemException("Unsupported Managed method return type", ret_base.Name);
            }
        }
        else if (method->Ret.Kind == ComplexTypeKind::Array) {
            ret_storage.Array = safe_alloc::make_unique<ManagedArrayBridgeData>();
            ret_storage.Array->Backend = backend;
            ret_storage.Array->Type = method->Ret;
            ret_data = ret_storage.Array.get();
        }
        else if (method->Ret.Kind == ComplexTypeKind::Dict) {
            ret_storage.Dict = safe_alloc::make_unique<ManagedDictBridgeData>();
            ret_storage.Dict->Backend = backend;
            ret_storage.Dict->Type = method->Ret;
            ret_data = ret_storage.Dict.get();
        }
        else {
            throw ScriptSystemException("Unsupported Managed method return type", method->Ret.BaseType.Name);
        }

        call.RetData = ret_data;
    }

    bool ref_type_owners_reconciled = false;
    auto reconcile_ref_type_owners = [&]() noexcept {
        for (size_t i = 0; i < args_count; i++) {
            ReconcileMutableDynamicRefTypeOwner(method->Args[i].Type, native_args[i]);
        }
        ref_type_owners_reconciled = true;
    };
    auto reconcile_ref_type_owners_on_exit = scope_exit([&]() noexcept {
        if (!ref_type_owners_reconciled) {
            reconcile_ref_type_owners();
        }
    });

    method->Call(call);
    reconcile_ref_type_owners();

    // A PassOwnership export returns an entity carrying the caller's reference, while the managed wrapper takes a
    // reference of its own, so the handed one is adopted here and dropped once the result is boxed
    refcount_nptr<Entity> passed_entity_ownership;

    if (method->PassOwnership && method->Ret.Kind == ComplexTypeKind::Simple && method->Ret.BaseType.IsEntity && !method->Ret.BaseType.IsGlobalEntity) {
        passed_entity_ownership = refcount_nptr<Entity>::from_adopted_ref(ret_storage.EntityPtr.get());
    }

    size_t mutable_args_count = static_cast<size_t>(std::ranges::count_if(method->Args, [](const ArgDesc& arg) { return arg.Type.IsMutable; }));

    if (mutable_args_count != 0) {
        if (!method->Ret && mutable_args_count == 1) {
            for (size_t i = 0; i < args_count; i++) {
                if (method->Args[i].Type.IsMutable) {
                    return BoxNativeCallValue(backend, method->Args[i].Type, args_data[i + first_method_arg], call.Accessor.get());
                }
            }

            throw ScriptSystemException("Managed mutable argument not found", owner_type_name, method_name_str);
        }

        MonoDomain* domain = GetDomainOrThrow(backend->GetDomain());
        size_t result_count = mutable_args_count + (method->Ret ? 1 : 0);
        MonoArray* result = mono_array_new(domain, mono_get_object_class(), result_count);
        uint32_t result_handle = NewManagedGcHandle(reinterpret_cast<MonoObject*>(result), 0);
        auto free_result_handle = scope_exit([result_handle]() noexcept { mono_gchandle_free(result_handle); });
        size_t result_index = 0;
        auto get_result = [result_handle]() -> MonoArray* { return reinterpret_cast<MonoArray*>(mono_gchandle_get_target(result_handle)); };

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
        // allocating a throwaway managed string on every such call
        return nullptr;
    }

    return BoxNativeCallValue(backend, method->Ret, ret_data, call.Accessor.get());
}

static auto NativeCallMethodBoxed(int32_t method_id, void* entity_ptr, MonoArray* args, MonoString** error) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(error != nullptr, "Managed method call error output is null");
    *error = nullptr;

    try {
        auto backend = GetActiveBackendOrThrow();
        return NativeCallMethodImpl(ResolveAbiMethod(backend, method_id, true), entity_ptr, args);
    }
    catch (const std::exception& ex) {
        *error = MakeManagedNativeError(ex);
        return nullptr;
    }
}

static auto NativeCallMethodIndexed(int32_t method_id, void* entity_ptr, void* frame, int32_t frame_size) -> MonoString*
{
    FO_STACK_TRACE_ENTRY();

    try {
        auto backend = GetActiveBackendOrThrow();
        const ManagedAbiMethodRuntime& entry = ResolveAbiMethod(backend, method_id, true);
        FO_VERIFY_AND_THROW(entry.UsesScalarFrame, "Managed method does not use a scalar frame", entry.Owner, method_id);
        FO_VERIFY_AND_THROW(frame != nullptr, "Managed scalar method frame is null", entry.Owner, method_id);
        FO_VERIFY_AND_THROW(frame_size == numeric_cast<int32_t>(entry.FrameSize), "Managed scalar method frame size mismatch", entry.Owner, method_id, frame_size, entry.FrameSize);

        nptr<const MethodDesc> method = entry.Method;
        FO_VERIFY_AND_THROW(method, "Managed ABI method pointer is null", entry.Owner, method_id);

        bool is_ref_type_method = entry.IsRefType;
        bool is_ref_type_factory = is_ref_type_method && method->Name == "__Factory";
        auto entity = !is_ref_type_method ? nptr<Entity> {ResolveEntity(backend, entity_ptr)} : nptr<Entity> {};
        Entity* self_entity = entity.get_no_const();
        void* self_ref = entity_ptr;
        size_t first_method_arg = is_ref_type_factory ? 0 : 1;
        array<void*, MAX_CALL_ARGS> args_data {};

        if (!is_ref_type_factory) {
            args_data[0] = is_ref_type_method ? &self_ref : make_ptr(&self_entity).void_cast();
        }

        if (is_ref_type_method && !is_ref_type_factory && self_ref == nullptr) {
            throw ScriptSystemException("Managed ref type target is null", entry.Owner, method->Name);
        }

        FO_VERIFY_AND_THROW(method->Args.size() + first_method_arg <= MAX_CALL_ARGS, "Managed method argument count exceeds bridge limit", entry.Owner, method->Name);
        ManagedAbiNativeFrame native_frame = BuildManagedAbiNativeFrame({static_cast<uint8_t*>(frame), entry.FrameSize}, entry.Args, entry.Ret);

        for (size_t i = 0; i < method->Args.size(); i++) {
            ptr<void> arg_data = GetManagedAbiNativeFrameArg(native_frame, i);

            // A handle slot holds the pointer itself, so the slot is the Entity* / ref pointer storage the call reads
            ValidateManagedFrameHandle(entry.Args[i], method->Args[i], arg_data.reinterpret_as<const uint8_t>().get(), entry.Owner, method->Name);
            args_data[i + first_method_arg] = arg_data.get();
        }

        small_vector<ptr<void>, MAX_CALL_ARGS> args_ptrs;
        args_ptrs.reserve(method->Args.size() + first_method_arg);

        for (size_t arg_idx = 0; arg_idx < method->Args.size() + first_method_arg; arg_idx++) {
            args_ptrs.emplace_back(args_data[arg_idx]);
        }

        FuncCallData call {.Accessor = &MANAGED_DATA_ACCESSOR};
        call.ArgsData = const_span<ptr<void>> {args_ptrs.data(), args_ptrs.size()};

        if (method->Ret) {
            call.RetData = GetManagedAbiNativeFrameResult(native_frame);
        }

        method->Call(call);
        CopyBackManagedAbiNativeFrame(native_frame);
        return nullptr;
    }
    catch (const std::exception& ex) {
        return MakeManagedNativeError(ex);
    }
}

static auto NativeBindAbi(uint64_t hash, int32_t method_count, int32_t event_count, int32_t setting_count, int32_t inner_count) -> MonoString*
{
    FO_STACK_TRACE_ENTRY();

    try {
        auto backend = GetActiveBackendOrThrow();
        auto abi = backend->GetAbi();
        FO_VERIFY_AND_THROW(abi, "Managed ABI tables are not built");
        FO_VERIFY_AND_THROW(abi->Hash == hash, "Managed ABI manifest hash mismatch", hash, abi->Hash);
        FO_VERIFY_AND_THROW(numeric_cast<size_t>(method_count) == abi->Methods.size(), "Managed ABI method count mismatch", method_count, abi->Methods.size());
        FO_VERIFY_AND_THROW(numeric_cast<size_t>(event_count) == abi->Events.size(), "Managed ABI event count mismatch", event_count, abi->Events.size());
        FO_VERIFY_AND_THROW(numeric_cast<size_t>(setting_count) == abi->Settings.size(), "Managed ABI setting count mismatch", setting_count, abi->Settings.size());
        FO_VERIFY_AND_THROW(numeric_cast<size_t>(inner_count) == abi->Inners.size(), "Managed ABI inner-entry count mismatch", inner_count, abi->Inners.size());
        // The cache can fail on a broken assembly, so it is built before the manifest counts as bound
        BuildWrapperClassCache(backend);
        abi->Bound = true;
        return nullptr;
    }
    catch (const std::exception& ex) {
        return MakeManagedNativeError(ex);
    }
}

// Invocation status handed back to managed code
static constexpr int32_t INVOKE_STATUS_FAILED = -1;
static constexpr int32_t INVOKE_STATUS_NO_CANDIDATE = 0;
static constexpr int32_t INVOKE_STATUS_COMPLETED = 1;

static auto NativeInvokeScriptFuncStatus(MonoString* func_name, MonoArray* args) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    auto backend = ActiveBackend;

    if (!backend || !backend->GetMetadata()) {
        return INVOKE_STATUS_FAILED;
    }

    nptr<EngineMetadata> meta = backend->GetMetadata();
    nptr<ScriptSystem> script_sys = meta.dyn_cast<ScriptSystem>();

    if (!script_sys) {
        return INVOKE_STATUS_FAILED;
    }

    string func_name_str = ToStringAndFree(func_name);
    hstring hashed_func_name = backend->GetMetadata()->Hashes.to_hashed_string(func_name_str);
    size_t args_count = args != nullptr ? mono_array_length(args) : 0;

    if (args_count > MAX_CALL_ARGS) {
        throw ScriptSystemException("Managed Invoke supports too many arguments", func_name_str, args_count, MAX_CALL_ARGS);
    }

    uint32_t args_handle = args != nullptr ? NewManagedGcHandle(reinterpret_cast<MonoObject*>(args), 0) : 0;
    auto free_args_handle = scope_exit([args_handle]() noexcept {
        if (args_handle != 0) {
            mono_gchandle_free(args_handle);
        }
    });
    auto get_args = [args, args_handle]() -> MonoArray* { return args_handle != 0 ? reinterpret_cast<MonoArray*>(mono_gchandle_get_target(args_handle)) : args; };

    auto candidates = script_sys->FindFuncCandidates(hashed_func_name);

    for (ptr<ScriptFuncDesc> func_desc : candidates) {
        bool is_void_call = !func_desc->Ret && func_desc->Args.size() == args_count;
        bool is_result_call = func_desc->Ret && func_desc->Args.size() + 1 == args_count;

        if (!func_desc->Call || (!is_void_call && !is_result_call)) {
            continue;
        }

        size_t call_args_count = func_desc->Args.size();
        array<void*, MAX_CALL_ARGS> args_data {};
        array<ManagedNativeValue, MAX_CALL_ARGS> native_args {};
        bool converted = true;

        try {
            for (size_t i = 0; i < call_args_count; i++) {
                MonoObject* arg = mono_array_get(get_args(), MonoObject*, i);

                if (!CanConvertManagedObjectToNative(backend, func_desc->Args[i].Type, arg)) {
                    converted = false;
                    break;
                }

                args_data[i] = ConvertManagedObjectToNative(backend, func_desc->Args[i].Type, arg, native_args[i]);
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
        bool ref_type_owners_reconciled = false;
        auto reconcile_ref_type_owners = [&]() noexcept {
            for (size_t i = 0; i < call_args_count; i++) {
                ReconcileMutableDynamicRefTypeOwner(func_desc->Args[i].Type, native_args[i]);
            }
            ref_type_owners_reconciled = true;
        };
        auto reconcile_ref_type_owners_on_exit = scope_exit([&]() noexcept {
            if (!ref_type_owners_reconciled) {
                reconcile_ref_type_owners();
            }
        });

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
                    ret_data = ret_storage.Alloc(ret_base);
                }
                else {
                    throw ScriptSystemException("Unsupported Managed invoke return type", ret_base.Name);
                }
            }
            else if (func_desc->Ret.Kind == ComplexTypeKind::Array) {
                ret_storage.Array = safe_alloc::make_unique<ManagedArrayBridgeData>();
                ret_storage.Array->Backend = backend;
                ret_storage.Array->Type = func_desc->Ret;
                ret_data = ret_storage.Array.get();
            }
            else if (func_desc->Ret.Kind == ComplexTypeKind::Dict) {
                ret_storage.Dict = safe_alloc::make_unique<ManagedDictBridgeData>();
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
            reconcile_ref_type_owners();
            exceptions::report_and_continue(ex);
            return INVOKE_STATUS_FAILED;
        }

        reconcile_ref_type_owners();

        if (is_result_call) {
            MonoObject* ret = nullptr;

            if (func_desc->Ret.Kind == ComplexTypeKind::Array) {
                auto& ret_array = ret_storage.Array;
                FO_VERIFY_AND_THROW(ret_array, "Managed invoke array return storage is null");

                ret = ret_array->GetObject();
                if (ret == nullptr) {
                    ret = CreateManagedList(backend, func_desc->Ret.BaseType);
                    ret_array->SetObject(ret);
                }
            }
            else if (func_desc->Ret.Kind == ComplexTypeKind::Dict) {
                auto& ret_dict = ret_storage.Dict;
                FO_VERIFY_AND_THROW(ret_dict, "Managed invoke dictionary return storage is null");
                FO_VERIFY_AND_THROW(func_desc->Ret.KeyType, "Managed invoke dictionary return type has no key type");

                ret = ret_dict->GetObject();
                if (ret == nullptr) {
                    ret = CreateManagedDictionary(backend, *func_desc->Ret.KeyType, func_desc->Ret.BaseType);
                    ret_dict->SetObject(ret);
                }
            }
            else {
                ret = BoxNativeCallValue(backend, func_desc->Ret, ret_data, call.Accessor.get());
            }

            mono_array_setref(get_args(), args_count - 1, ret);
        }

        for (size_t i = 0; i < call_args_count; i++) {
            if (!func_desc->Args[i].Type.IsMutable) {
                continue;
            }

            MonoObject* arg = BoxNativeCallValue(backend, func_desc->Args[i].Type, args_data[i], call.Accessor.get());
            mono_array_setref(get_args(), i, arg);
        }

        return INVOKE_STATUS_COMPLETED;
    }

    return INVOKE_STATUS_NO_CANDIDATE;
}

static void NativeRegisterGlobalScriptFunc(MonoString* full_name, MonoString* attr_name, MonoArray* param_type_names, MonoString* ret_type_name, MonoObject* handler)
{
    FO_STACK_TRACE_ENTRY();

    // Register a managed global script function into the engine's cross-backend function map under a named marker
    // attribute, so a consumer that resolves funcs by attribute (via `ScriptSystem::FindFunc`) can invoke it
    auto backend = ActiveBackend;
    FO_VERIFY_AND_THROW(backend, "No active managed script backend");
    nptr<EngineMetadata> meta = backend->GetMetadata();
    FO_VERIFY_AND_THROW(meta, "Backend metadata is not available");

    string full_name_str = ToStringAndFree(full_name);
    string attr_name_str = ToStringAndFree(attr_name);
    string ret_type_str = ToStringAndFree(ret_type_name);

    if (handler == nullptr) {
        throw ScriptSystemException("Null Managed global script func handler", full_name_str);
    }

    ComplexTypeDesc ret;
    vector<ComplexTypeDesc> args;
    bool supported = true;

    size_t param_count = param_type_names != nullptr ? mono_array_length(param_type_names) : 0;

    for (size_t i = 0; i < param_count; i++) {
        MonoString* param_mono_str = mono_array_get(param_type_names, MonoString*, i);
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
        logging::write("Managed global script func '{}' has an unsupported signature; skipping registration", full_name_str);
        return;
    }

    nptr<ScriptSystem> script_sys = meta.dyn_cast<ScriptSystem>();
    FO_VERIFY_AND_THROW(script_sys, "Backend metadata does not expose a script system");

    hstring hashed_func_name = meta->Hashes.to_hashed_string(full_name_str);

    auto candidates = script_sys->FindFuncCandidates(hashed_func_name);

    for (ptr<ScriptFuncDesc> candidate : candidates) {
        if (!candidate->Call || candidate->Ret != ret || candidate->Args.size() != args.size() || !candidate->AttributeChecker || !candidate->AttributeChecker(attr_name_str)) {
            continue;
        }

        bool args_match = true;

        for (size_t i = 0; i < args.size(); i++) {
            if (candidate->Args[i].Type != args[i]) {
                args_match = false;
                break;
            }
        }

        FO_VERIFY_AND_THROW(!args_match, "Script function is already registered", full_name_str, attr_name_str);
    }

    FO_VERIFY_AND_THROW(hashed_func_name, "Managed script function has an empty name");

    uint32_t handler_handle = NewManagedGcHandle(handler, false);
    FO_VERIFY_AND_THROW(handler_handle != 0, "Can't root Managed global script function");
    auto release_handler_handle_on_error = scope_fail([handler_handle]() noexcept { mono_gchandle_free(handler_handle); });

    auto func_desc = safe_alloc::make_unique<ScriptFuncDesc>();
    func_desc->Name = hashed_func_name;
    func_desc->Ret = ret;
    func_desc->Args.reserve(args.size());

    for (const ComplexTypeDesc& arg_type : args) {
        func_desc->Args.emplace_back(ArgDesc {.Name = {}, .Type = arg_type});
    }

    shared_ptr<ManagedCallbackPlan> plan = MakeManagedCallbackPlan(backend, ret, args);
    func_desc->AttributeChecker = [attr_name_str](string_view attribute) -> bool { return attribute == attr_name_str; };
    func_desc->Call = [backend = backend.as_ptr(), handler_handle, plan](FuncCallData& call) { DispatchManagedCallback(backend, handler_handle, *plan, call); };

    backend->AdoptPersistentGcHandle(handler_handle);
    release_handler_handle_on_error.release();
    backend->AddManagedGlobalFunc(std::move(func_desc));
}

static void NativeRegisterRemoteCallHandler(MonoString* name_str, int32_t param_count, MonoObject* handler)
{
    FO_STACK_TRACE_ENTRY();

    // Wire a managed inbound remote-call handler ([ServerRemoteCall]/[ClientRemoteCall]/[AdminRemoteCall])
    auto backend = ActiveBackend;
    FO_VERIFY_AND_THROW(backend, "No active managed script backend");
    nptr<EngineMetadata> meta = backend->GetMetadata();
    FO_VERIFY_AND_THROW(meta, "Backend metadata is not available");

    string name = ToStringAndFree(name_str);

    if (handler == nullptr) {
        throw ScriptSystemException("Null Managed remote call handler", name);
    }

    nptr<BaseEngine> engine = meta.dyn_cast<BaseEngine>();

    if (!engine) {
        return;
    }

    hstring name_hashed = meta->Hashes.to_hashed_string(name);
    auto inbound_calls = meta->GetInboundRemoteCalls();
    auto it = inbound_calls->find(name_hashed);

    if (it == inbound_calls->end()) {
        return;
    }

    const RemoteCallDesc& inbound_call = it->second;

    bool managed_declared_call = strvex(inbound_call.SubsystemHint).ends_with("cs");
    bool client_facade_call = engine->GetSide() == EngineSideKind::ClientSide && strvex(inbound_call.SubsystemHint).ends_with("fos");

    if (!managed_declared_call && !client_facade_call) {
        return;
    }

    bool server_side = engine->GetSide() == EngineSideKind::ServerSide;

    // Build the call's argument type list: the server side prepends the calling Player, then the wire args
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

    // The handler outlives this registration, so the declaration's structural wire limits are copied out of
    // the RemoteCallDesc rather than captured by reference
    hstring call_name = inbound_call.Name;
    size_t max_payload_size = inbound_call.MaxPayloadSize;
    size_t max_collection_size = inbound_call.MaxCollectionSize;
    vector<string> wire_arg_names;
    wire_arg_names.reserve(inbound_call.Args.size());

    for (const auto& arg : inbound_call.Args) {
        wire_arg_names.emplace_back(arg.Name);
    }

    uint32_t handler_handle = NewManagedGcHandle(handler, false);
    FO_VERIFY_AND_THROW(handler_handle != 0, "Can't root Managed remote call handler");
    auto release_handler_handle_on_error = scope_fail([handler_handle]() noexcept { mono_gchandle_free(handler_handle); });

    backend->AdoptPersistentGcHandle(handler_handle);
    release_handler_handle_on_error.release();

    shared_ptr<ManagedCallbackPlan> plan = MakeManagedCallbackPlan(backend, ComplexTypeDesc {}, args);

    engine->SetRemoteCallHandler(
        name_hashed,
        [backend = backend.as_ptr(), engine, args, plan, handler_handle, server_side, call_name, max_payload_size, max_collection_size, wire_arg_names](hstring, nptr<Entity> entity, span<uint8_t> data) FO_DEFERRED {
            FO_VERIFY_AND_THROW(max_payload_size == 0 || data.size() <= max_payload_size, "Remote call payload exceeds structural limit", call_name, data.size(), max_payload_size);

            // Attach to the Managed domain up front: building managed List objects for array args (below) invokes mono,
            // and this handler may run on the engine's network-receive thread
            MonoDomain* domain = GetDomainOrThrow(backend->GetDomain());

            ManagedThreadAttachment managed_thread {domain};

            data_reader reader(data);
            RemoteCallReadStorage storage;
            list<ManagedArrayBridgeData> array_bridges;
            list<refcount_ptr<DynamicRefTypeInstance>> ref_instances;
            RemoteCallWireHooks hooks {
                .RawToRefType = [&ref_instances](const BaseTypeDesc& type, span<const uint8_t> raw_data) -> ptr<void> {
                    // Deserialize the ref type's fields into a DynamicRefTypeInstance (shared engine type); the
                    // MANAGED_DATA_ACCESSOR boxes it into a managed object (CreateRefTypeObject) when invoking
                    auto ref_instance = safe_alloc::make_refcounted<DynamicRefTypeInstance>(type.RefType->FieldsRegistrar.get());
                    ref_instance->LoadFromRawData(type, raw_data);
                    auto&& stored = ref_instances.emplace_back(std::move(ref_instance));
                    return make_ptr(stored.get_pp()).reinterpret_as<void>();
                },
            };

            FuncCallData call {.Accessor = &MANAGED_DATA_ACCESSOR};
            array<void*, MAX_CALL_ARGS> data_storage;
            size_t arg_index = 0;
            Entity* raw_entity = entity.get_no_const();

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
                    // List rooted by a bridge so the MANAGED_DATA_ACCESSOR can read it back when boxing the argument
                    const string& wire_arg_name = wire_arg_names[arg_index - (server_side ? 1 : 0)];
                    int32_t count = reader.read<int32_t>();
                    FO_VERIFY_AND_THROW(count >= 0, "Remote call array element count is negative");
                    FO_VERIFY_AND_THROW(max_collection_size == 0 || numeric_cast<size_t>(count) <= max_collection_size, "Arr size exceeds structural remote-call limit", call_name, wire_arg_name, count, max_collection_size);
                    reader.verify_payload_count(numeric_cast<size_t>(count), GetRemoteCallSimpleValueMinWireSize(arg_type.BaseType));

                    auto& bridge = array_bridges.emplace_back();
                    bridge.Backend = backend;
                    bridge.Type = arg_type;
                    bridge.SetObject(CreateManagedList(backend, arg_type.BaseType));

                    for (int32_t j = 0; j < count; j++) {
                        auto element = ReadRemoteCallSimple(reader, arg_type.BaseType, engine->Hashes, storage, hooks);
                        AddManagedListItem(backend, bridge.GetObject(), BoxNativeSimpleValue(backend, arg_type.BaseType, element.get()));
                    }

                    data_storage[arg_index] = make_ptr(&bridge).void_cast();
                }
            }

            reader.verify_end();

            vector<ptr<void>> args_ptrs;
            args_ptrs.reserve(args.size());
            for (size_t data_idx = 0; data_idx < args.size(); data_idx++) {
                args_ptrs.emplace_back(data_storage[data_idx]);
            }
            call.ArgsData = const_span<ptr<void>> {args_ptrs.data(), args_ptrs.size()};

            try {
                DispatchManagedCallback(backend, handler_handle, *plan, call);
            }
            catch (const std::exception& ex) {
                exceptions::report_and_continue(ex);
            }
        },
        client_facade_call);
}

static void NativeSendRemoteCall(MonoObject* caller, MonoString* name_str, MonoArray* args_array)
{
    FO_STACK_TRACE_ENTRY();

    // Managed outbound remote call: serialize the boxed args into the shared RemoteCallWire format and hand them to
    // engine->SendRemoteCall (which forwards to the remote peer)
    auto backend = ActiveBackend;
    FO_VERIFY_AND_THROW(backend, "No active managed script backend");
    nptr<EngineMetadata> meta = backend->GetMetadata();
    FO_VERIFY_AND_THROW(meta, "Backend metadata is not available");

    string name = ToStringAndFree(name_str);

    nptr<BaseEngine> engine = meta.dyn_cast<BaseEngine>();

    if (!engine) {
        throw ScriptSystemException("Managed remote call send without a game engine", name);
    }

    hstring name_hashed = meta->Hashes.to_hashed_string(name);
    auto outbound_calls = meta->GetOutboundRemoteCalls();
    auto it = outbound_calls->find(name_hashed);

    if (it == outbound_calls->end()) {
        throw ScriptSystemException("Unknown managed outbound remote call", name);
    }

    const RemoteCallDesc& outbound_call = it->second;

    // A managed caller may send both managed and AngelScript-defined outbound RPCs: arguments use the shared
    // RemoteCallWire byte format that either inbound handler reads
    nptr<Entity> caller_entity;
    if (caller != nullptr) {
        caller_entity = ExtractEntityPtr(caller);
    }
    FO_VERIFY_AND_THROW(caller_entity, "Managed remote call send requires a caller", name);
    auto data = SerializeManagedRemoteCallArgs(backend, outbound_call.Args, args_array, name);

    engine->SendRemoteCall(name_hashed, caller_entity, data);
}

static void NativeLoopbackRemoteCall(MonoObject* caller, MonoString* name_str, MonoArray* args_array)
{
    FO_STACK_TRACE_ENTRY();

    // Diagnostic/test helper: serialize the boxed args into the shared RemoteCallWire format and dispatch them
    // through the engine's real inbound path (HandleInboundRemoteCall) in-process, with no network peer
    auto backend = ActiveBackend;
    FO_VERIFY_AND_THROW(backend, "No active managed script backend");
    nptr<EngineMetadata> meta = backend->GetMetadata();
    FO_VERIFY_AND_THROW(meta, "Backend metadata is not available");

    string name = ToStringAndFree(name_str);

    nptr<BaseEngine> engine = meta.dyn_cast<BaseEngine>();

    if (!engine) {
        throw ScriptSystemException("Managed remote call loopback without a game engine", name);
    }

    hstring name_hashed = meta->Hashes.to_hashed_string(name);
    auto inbound_calls = meta->GetInboundRemoteCalls();
    auto it = inbound_calls->find(name_hashed);

    if (it == inbound_calls->end()) {
        throw ScriptSystemException("Unknown managed inbound remote call for loopback", name);
    }

    const RemoteCallDesc& inbound_call = it->second;

    if (!strvex(inbound_call.SubsystemHint).ends_with("cs")) {
        throw ScriptSystemException("Managed loopback remote call is not a managed (cs) call", name);
    }

    nptr<Entity> caller_entity;
    if (caller != nullptr) {
        caller_entity = ExtractEntityPtr(caller);
    }
    auto data = SerializeManagedRemoteCallArgs(backend, inbound_call.Args, args_array, name);

    engine->HandleInboundRemoteCall(name_hashed, caller_entity, data);
}

// === Internal-call registration ===

static void RegisterInternalCalls()
{
    FO_STACK_TRACE_ENTRY();

    mono_add_internal_call("FOnline.Native::RunScriptContinuationInternal", reinterpret_cast<const void*>(NativeRunScriptContinuation));
    mono_add_internal_call("FOnline.Native::Log", reinterpret_cast<const void*>(NativeLog));
    mono_add_internal_call("FOnline.Native::ReportExceptionInternal", reinterpret_cast<const void*>(NativeReportException));
    mono_add_internal_call("FOnline.Native::GetHashStr", reinterpret_cast<const void*>(NativeGetHashStr));
    mono_add_internal_call("FOnline.Native::GetHashStrFromHash", reinterpret_cast<const void*>(NativeGetHashStrFromHash));
    mono_add_internal_call("FOnline.Native::GetHash", reinterpret_cast<const void*>(NativeGetHash));
    mono_add_internal_call("FOnline.Native::ResolveHash", reinterpret_cast<const void*>(NativeResolveHash));
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
    mono_add_internal_call("FOnline.Native::UnsubscribeAllEvents", reinterpret_cast<const void*>(NativeUnsubscribeAllEvents));
    mono_add_internal_call("FOnline.Native::FireEventBoxedInternal", reinterpret_cast<const void*>(NativeFireEventBoxed));
    mono_add_internal_call("FOnline.Native::FireEventIndexedInternal", reinterpret_cast<const void*>(NativeFireEventIndexed));
    mono_add_internal_call("FOnline.Native::GetPropertyInternal", reinterpret_cast<const void*>(NativeGetProperty));
    mono_add_internal_call("FOnline.Native::GetPropertyValueInternal", reinterpret_cast<const void*>(NativeGetPropertyValue));
    mono_add_internal_call("FOnline.Native::SetPropertyValueInternal", reinterpret_cast<const void*>(NativeSetPropertyValue));
    mono_add_internal_call("FOnline.Native::GetPropertyArrayInternal", reinterpret_cast<const void*>(NativeGetPropertyArray));
    mono_add_internal_call("FOnline.Native::SetPropertyArrayInternal", reinterpret_cast<const void*>(NativeSetPropertyArray));
    mono_add_internal_call("FOnline.Native::SetPropertyInternal", reinterpret_cast<const void*>(NativeSetProperty));
    mono_add_internal_call("FOnline.Native::SetPropertyGetter", reinterpret_cast<const void*>(NativeSetPropertyGetter));
    mono_add_internal_call("FOnline.Native::AddPropertySetter", reinterpret_cast<const void*>(NativeAddPropertySetter));
    mono_add_internal_call("FOnline.Native::AddPropertySetterWithProperty", reinterpret_cast<const void*>(NativeAddPropertySetterWithProperty));
    mono_add_internal_call("FOnline.Native::AddPropertyDeferredSetter", reinterpret_cast<const void*>(NativeAddPropertyDeferredSetter));
    mono_add_internal_call("FOnline.Native::BindAbiInternal", reinterpret_cast<const void*>(NativeBindAbi));
    mono_add_internal_call("FOnline.Native::CallMethodBoxedInternal", reinterpret_cast<const void*>(NativeCallMethodBoxed));
    mono_add_internal_call("FOnline.Native::CallMethodIndexedInternal", reinterpret_cast<const void*>(NativeCallMethodIndexed));
    mono_add_internal_call("FOnline.Native::InvokeScriptFuncStatus", reinterpret_cast<const void*>(NativeInvokeScriptFuncStatus));
    mono_add_internal_call("FOnline.Native::GetBackendAliveFlag", reinterpret_cast<const void*>(NativeGetBackendAliveFlag));
    mono_add_internal_call("FOnline.Native::GetBackend", reinterpret_cast<const void*>(NativeGetBackend));
    mono_add_internal_call("FOnline.Native::GetProtoEntity", reinterpret_cast<const void*>(NativeGetProtoEntity));
    mono_add_internal_call("FOnline.Native::CheckProtoEntity", reinterpret_cast<const void*>(NativeCheckProtoEntity));
    mono_add_internal_call("FOnline.Native::GetProtoEntityCount", reinterpret_cast<const void*>(NativeGetProtoEntityCount));
    mono_add_internal_call("FOnline.Native::GetProtoEntityAt", reinterpret_cast<const void*>(NativeGetProtoEntityAt));
    mono_add_internal_call("FOnline.Native::CreateInnerEntity", reinterpret_cast<const void*>(NativeCreateInnerEntity));
    mono_add_internal_call("FOnline.Native::HasInnerEntities", reinterpret_cast<const void*>(NativeHasInnerEntities));
    mono_add_internal_call("FOnline.Native::GetInnerEntity", reinterpret_cast<const void*>(NativeGetInnerEntity));
    mono_add_internal_call("FOnline.Native::FillInnerEntitiesInternal", reinterpret_cast<const void*>(NativeFillInnerEntities));
    mono_add_internal_call("FOnline.Native::GetAndResetInnerEntityVisits", reinterpret_cast<const void*>(NativeGetAndResetInnerEntityVisits));
    mono_add_internal_call("FOnline.Native::GetAndResetTypedCallbackDispatches", reinterpret_cast<const void*>(NativeGetAndResetTypedCallbackDispatches));
    mono_add_internal_call("FOnline.Native::GetAndResetBoxedCallbackDispatches", reinterpret_cast<const void*>(NativeGetAndResetBoxedCallbackDispatches));
    mono_add_internal_call("FOnline.Native::ProbeCallbackTransportInternal", reinterpret_cast<const void*>(NativeProbeCallbackTransport));
    mono_add_internal_call("FOnline.Native::ProbeTransportScenarioInternal", reinterpret_cast<const void*>(NativeProbeTransportScenario));
    mono_add_internal_call("FOnline.Native::ReadInteropCountersInternal", reinterpret_cast<const void*>(NativeReadInteropCounters));
    mono_add_internal_call("FOnline.Native::GetEntityValueAsIntInternal", reinterpret_cast<const void*>(NativeGetEntityValueAsInt));
    mono_add_internal_call("FOnline.Native::SetEntityValueAsIntInternal", reinterpret_cast<const void*>(NativeSetEntityValueAsInt));
    mono_add_internal_call("FOnline.Native::GetEntityValueAsAnyInternal", reinterpret_cast<const void*>(NativeGetEntityValueAsAny));
    mono_add_internal_call("FOnline.Native::SetEntityValueAsAnyInternal", reinterpret_cast<const void*>(NativeSetEntityValueAsAny));
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
    mono_add_internal_call("FOnline.Native::GetSettingValueInternal", reinterpret_cast<const void*>(NativeGetSettingValue));
}

// === Settings access helpers ===

static auto GetSettingValueAsString(MonoString* name) -> string
{
    FO_STACK_TRACE_ENTRY();

    nptr<GlobalSettings> settings = GetBackendSettings(GetActiveBackendOrThrow());
    string setting_name = ToStringAndFree(name);

    if (!settings) {
        return {};
    }

    return settings->GetRuntimeSetting(setting_name);
}

static void SetSettingValueFromString(nptr<GlobalSettings> settings, string_view setting_name, string value)
{
    FO_STACK_TRACE_ENTRY();

    if (!settings) {
        return;
    }

    settings->SetRuntimeSetting(string(setting_name), value);
}

static void SetSettingValueFromString(MonoString* name, string value)
{
    FO_STACK_TRACE_ENTRY();

    nptr<GlobalSettings> settings = GetBackendSettings(GetActiveBackendOrThrow());
    string setting_name = ToStringAndFree(name);
    SetSettingValueFromString(settings, setting_name, std::move(value));
}

// === Property getter/setter callback bridge ===

static auto InvokeManagedCallbackHandler(ptr<ManagedScriptBackend> backend, MonoObject* handler, MonoArray* args_array) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    MonoMethod* invoke_callback = FindNativeMethod(backend, "InvokeCallback", 2);

    void* invoke_args[] = {handler, args_array};
    return InvokeManagedScript(invoke_callback, nullptr, invoke_args, "Managed property callback failed");
}

static auto ResolveVirtualPropertyForCallback(ptr<ManagedScriptBackend> backend, MonoString* owner_type, MonoString* property_name, bool require_virtual, bool require_marshalable_value) -> ptr<const Property>
{
    FO_STACK_TRACE_ENTRY();

    nptr<EngineMetadata> meta = backend->GetMetadata();
    FO_VERIFY_AND_THROW(meta, "Backend metadata is not available");

    string owner_type_name = ToStringAndFree(owner_type);
    string property_name_str = ToStringAndFree(property_name);
    auto nullable_registrar = meta->GetPropertyRegistrar(owner_type_name);

    if (!nullable_registrar) {
        throw ScriptSystemException("Managed property owner type not found", owner_type_name);
    }

    auto registrar = nullable_registrar.as_ptr();
    auto nullable_prop = registrar->FindProperty(property_name_str);

    if (!nullable_prop) {
        throw ScriptSystemException("Managed property not found", owner_type_name, property_name_str);
    }

    auto prop = nullable_prop.as_ptr();

    if (require_virtual && !prop->IsVirtual()) {
        throw ScriptSystemException("Managed property getter requires a virtual property", prop->GetName());
    }

    // Deferred (post-set) callbacks receive only the entity, so they place no constraint on the value
    // type; getter/setter callbacks that marshal the value require a bridgeable simple type
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

static auto MakeManagedCallbackPlan(ptr<ManagedScriptBackend> backend, const ComplexTypeDesc& ret, vector<ComplexTypeDesc> args) -> shared_ptr<ManagedCallbackPlan>
{
    FO_STACK_TRACE_ENTRY();

    shared_ptr<ManagedCallbackPlan> plan = safe_alloc::make_shared<ManagedCallbackPlan>();
    plan->Ret = ret;
    plan->Args = std::move(args);
    plan->Layout = BuildManagedAbiCallbackLayout(plan->Ret, plan->Args);
    plan->Engine = backend->GetMetadata().dyn_cast<BaseEngine>();

    if (plan->Layout.Supported) {
        plan->Adapter = FindCallbackAdapter(backend, MakeManagedAbiCallbackKey(plan->Ret, plan->Args));
    }

    return plan;
}

static auto FindCallbackAdapter(ptr<ManagedScriptBackend> backend, string_view key) -> nptr<MonoMethod>
{
    FO_STACK_TRACE_ENTRY();

    auto caches = backend->GetCaches();
    FO_VERIFY_AND_THROW(caches, "Managed backend caches are not created");

    scoped_lock adapters_locker {caches->CallbackAdaptersLocker};
    auto it = caches->CallbackAdapters.find(key);

    if (it != caches->CallbackAdapters.end()) {
        return it->second;
    }

    string method_name = strex("Adapt_{}", key).str();
    nptr<MonoMethod> adapter;

    for (nptr<void> image_ptr : backend->GetImages()) {
        nptr<MonoImage> image = image_ptr.reinterpret_as<MonoImage>();
        MonoClass* klass = mono_class_from_name(image.get(), "FOnline", "CallbackAdapters");

        if (klass == nullptr) {
            continue;
        }

        if (MonoMethod* method = mono_class_get_method_from_name(klass, method_name.c_str(), 3)) {
            adapter = method;
            break;
        }
    }

    caches->CallbackAdapters.emplace(string {key}, adapter);
    return adapter;
}

static void DispatchManagedCallback(ptr<ManagedScriptBackend> backend, uint32_t handler_handle, const ManagedCallbackPlan& plan, FuncCallData& call)
{
    FO_STACK_TRACE_ENTRY();

    // Resolved once with the plan: a cast across the engine hierarchy per dispatch is measurable
    FO_VERIFY_AND_THROW(plan.Engine, "Managed callback dispatch requires an engine context");
    ptr<BaseEngine> engine = plan.Engine.get_no_const();

    RunManagedScriptEntry(backend, engine, [handler_handle] { return mono_gchandle_get_target(handler_handle); }, [&] { DispatchManagedCallbackInContext(backend, handler_handle, plan, call); });
}

static void DispatchManagedCallbackInContext(ptr<ManagedScriptBackend> backend, uint32_t handler_handle, const ManagedCallbackPlan& plan, FuncCallData& call)
{
    FO_STACK_TRACE_ENTRY();

    ActiveBackendScope active_backend {backend};

    MonoDomain* domain = GetDomainOrThrow(backend->GetDomain());

    ManagedThreadAttachment managed_thread {domain};

    if (mono_gchandle_get_target(handler_handle) == nullptr) {
        throw ScriptSystemException("Managed callback delegate was collected");
    }
    if (call.ArgsData.size() != plan.Args.size()) {
        throw ScriptSystemException("Managed callback argument count mismatch");
    }

    if (!plan.Adapter || !TryDispatchManagedCallbackTyped(backend, handler_handle, plan, call)) {
        DispatchManagedCallbackBoxed(backend, handler_handle, plan, call);
    }
}

// The frame path: handles and fixed values are copied into a stack frame the generated adapter reads, and a
// fixed-value result comes back in it. The adapter wraps handles as non-null, so a null one declines to the boxed path
static auto TryDispatchManagedCallbackTyped(ptr<ManagedScriptBackend> backend, uint32_t handler_handle, const ManagedCallbackPlan& plan, FuncCallData& call) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto caches = backend->GetCaches();
    FO_VERIFY_AND_THROW(caches, "Managed backend caches are not created");
    FO_VERIFY_AND_THROW(plan.Layout.Args.size() == plan.Args.size(), "Managed callback layout does not match its signature");

    array<uint8_t, MANAGED_ABI_SCALAR_FRAME_CAPACITY> frame {};

    for (size_t i = 0; i < plan.Args.size(); i++) {
        const ManagedAbiSlot& slot = plan.Layout.Args[i];
        ptr<void> arg_data = call.ArgsData[i];

        if (slot.Kind == ManagedAbiValueKind::Handle) {
            // Entities travel as Entity*, native ref types as their object pointer; both are one pointer slot
            const void* handle = *arg_data.reinterpret_as<void*>();

            if (handle == nullptr) {
                return false;
            }

            uint64_t handle_bits = numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(handle));
            memory::copy(frame.data() + slot.Offset, &handle_bits, sizeof(handle_bits));
        }
        else {
            memory::copy(frame.data() + slot.Offset, arg_data.get(), slot.Size);
        }
    }

    int32_t frame_size = plan.Layout.FrameSize;
    void* adapter_args[] = {mono_gchandle_get_target(handler_handle), frame.data(), &frame_size};

    if (caches->CountDispatches.load(std::memory_order_relaxed)) {
        caches->TypedCallbackDispatches.fetch_add(1, std::memory_order_relaxed);
    }

    (void)InvokeManagedScript(plan.Adapter.get_no_const(), nullptr, adapter_args, "Managed callback failed");

    if (plan.Ret) {
        FO_VERIFY_AND_THROW(call.RetData, "Managed callback result storage is missing");
        memory::copy(call.RetData.as_ptr(), frame.data() + plan.Layout.ResultOffset, plan.Layout.Ret.Size);
    }

    return true;
}

// The boxed path: every argument becomes a managed object in a rooted array and Native.InvokeCallback drives the
// delegate through DynamicInvoke; signatures with strings, collections or by-ref arguments still take it
static void DispatchManagedCallbackBoxed(ptr<ManagedScriptBackend> backend, uint32_t handler_handle, const ManagedCallbackPlan& plan, FuncCallData& call)
{
    FO_STACK_TRACE_ENTRY();

    auto caches = backend->GetCaches();
    FO_VERIFY_AND_THROW(caches, "Managed backend caches are not created");

    if (caches->CountDispatches.load(std::memory_order_relaxed)) {
        caches->BoxedCallbackDispatches.fetch_add(1, std::memory_order_relaxed);
    }

    const ComplexTypeDesc& ret = plan.Ret;
    const vector<ComplexTypeDesc>& args = plan.Args;
    MonoDomain* domain = GetDomainOrThrow(backend->GetDomain());

    uint32_t args_array_handle = NewManagedGcHandle(reinterpret_cast<MonoObject*>(mono_array_new(domain, mono_get_object_class(), args.size())), 0);
    auto free_args_array_handle = scope_exit([args_array_handle]() noexcept { mono_gchandle_free(args_array_handle); });
    auto get_args_array = [args_array_handle]() -> MonoArray* { return reinterpret_cast<MonoArray*>(mono_gchandle_get_target(args_array_handle)); };

    for (size_t i = 0; i < args.size(); i++) {
        MonoObject* arg = BoxNativeCallValue(backend, args[i], ptr<void>(call.ArgsData[i]).get(), call.Accessor.get());
        mono_array_setref(get_args_array(), i, arg);
    }

    // Delegate types can belong to system assemblies; Native belongs to the backend's loaded scripts
    MonoMethod* invoke_callback = FindNativeMethod(backend, "InvokeCallback", 2);

    void* invoke_args[] = {mono_gchandle_get_target(handler_handle), get_args_array()};
    ManagedObjectRoot result;
    result.SetObject(InvokeManagedScript(invoke_callback, nullptr, invoke_args, "Managed callback failed"));

    for (size_t i = 0; i < args.size(); i++) {
        if (args[i].IsMutable) {
            CopyManagedCallbackByRefArg(backend, args[i], mono_array_get(get_args_array(), MonoObject*, i), ptr<void>(call.ArgsData[i]));
        }
    }

    if (ret) {
        CopyManagedCallbackReturnValue(backend, ret, result.GetObject(), call);
    }
}

// The managed registration admits one by-ref shape, the dialog start function's string, so anything else here is a
// signature that should never have been registered rather than a case to grow support for
static void CopyManagedCallbackByRefArg(ptr<ManagedScriptBackend> backend, const ComplexTypeDesc& type, MonoObject* value, ptr<void> arg_data)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(type.Kind == ComplexTypeKind::Simple && type.BaseType.IsString, "Only a string by-ref argument is supported", type.BaseType.Name);

    ManagedScalarValue storage;
    void* native_value = ConvertManagedSimpleObjectToNative(backend, type.BaseType, value, storage);
    *arg_data.reinterpret_as<string>() = *static_cast<string*>(native_value);
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
        // Rebuild the caller's array from the managed List return (the reverse of BoxNativeCallValue's Array read)
        ManagedObjectRoot list;
        list.SetObject(value);
        accessor->ClearArray(ret_data);

        size_t array_size = GetManagedListCount(backend, list.GetObject());

        for (size_t i = 0; i < array_size; i++) {
            MonoObject* element = GetManagedListItem(backend, list.GetObject(), i);
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
        nptr<Entity> entity = *static_cast<Entity**>(native_value);

        if (accessor->GetBackendIndex() == ScriptSystemBackend::ANGELSCRIPT_BACKEND_INDEX && entity) {
            entity->AddRef();
        }

        *ret_data.reinterpret_as<Entity*>() = entity.get_no_const();
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
        memory::copy(ret_data, native_value, base_type.Size);
    }
    else {
        throw ScriptSystemException("Unsupported Managed callback return type", base_type.Name);
    }
}

static auto CreateManagedCallbackDesc(ptr<const ManagedCallbackBridgeData> callback) -> unique_del_nptr<ScriptFuncDesc>
{
    FO_STACK_TRACE_ENTRY();

    if (callback->Handler == 0) {
        return nullptr;
    }

    MonoObject* handler = mono_gchandle_get_target(callback->Handler);

    if (handler == nullptr) {
        return nullptr;
    }
    if (callback->Type.Kind != ComplexTypeKind::Callback || !callback->Type.CallbackArgs || callback->Type.CallbackArgs->empty()) {
        throw ScriptSystemException("Invalid Managed callback type");
    }

    ComplexTypeDesc ret = callback->Type.CallbackArgs->front();
    vector<ComplexTypeDesc> args;

    for (const ComplexTypeDesc& arg_type : span(*callback->Type.CallbackArgs).subspan(1)) {
        args.emplace_back(arg_type);
    }

    auto func_desc = safe_alloc::make_unique<ScriptFuncDesc>();
    func_desc->Name = callback->Name;
    func_desc->Ret = ret;
    func_desc->Args.reserve(args.size());

    for (const ComplexTypeDesc& arg_type : args) {
        func_desc->Args.emplace_back(ArgDesc {.Name = {}, .Type = arg_type});
    }

    ptr<ManagedScriptBackend> backend {callback->Backend.get_no_const()};
    shared_ptr<ManagedCallbackPlan> plan = MakeManagedCallbackPlan(backend, ret, std::move(args));

    // The delegate stays rooted by the bridge handle while this runs, so its own root is taken last: nothing after
    // it can throw and leave the handle unreleased
    uint32_t handler_handle = NewManagedGcHandle(handler, false);
    func_desc->Call = [backend, handler_handle, plan](FuncCallData& call) { DispatchManagedCallback(backend, handler_handle, *plan, call); };
    func_desc->AttributeChecker = [](string_view /*attribute*/) -> bool { return true; };

    nptr<MonoDomain> domain = callback->Domain;
    return make_unique_del_ptr(std::move(func_desc).release(), [domain, handler_handle](ScriptFuncDesc* desc) mutable {
        ReleaseManagedGcHandle(domain, handler_handle);
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
        ManagedObjectRoot list;
        list.SetObject(CreateManagedList(backend, type.BaseType));
        size_t size = accessor->GetArraySize(data);

        for (size_t i = 0; i < size; i++) {
            MonoObject* item = BoxNativeSimpleValue(backend, type.BaseType, accessor->GetArrayElement(data, i).get());
            AddManagedListItem(backend, list.GetObject(), item);
        }

        return list.GetObject();
    }
    if (type.Kind == ComplexTypeKind::Dict) {
        FO_VERIFY_AND_THROW(type.KeyType, "Dictionary type has no key type");
        ManagedObjectRoot dictionary;
        dictionary.SetObject(CreateManagedDictionary(backend, *type.KeyType, type.BaseType));
        size_t size = accessor->GetDictSize(data);

        for (size_t i = 0; i < size; i++) {
            auto [key, value] = accessor->GetDictElement(data, i);
            ManagedObjectRoot managed_key;
            managed_key.SetObject(BoxNativeSimpleValue(backend, *type.KeyType, key.get()));
            MonoObject* managed_value = BoxNativeSimpleValue(backend, type.BaseType, value.get());
            AddManagedDictionaryItem(backend, dictionary.GetObject(), managed_key.GetObject(), managed_value);
        }

        return dictionary.GetObject();
    }

    throw ScriptSystemException("Unsupported Managed return type", type.BaseType.Name);
}

// === Event dispatch bridge ===

// Writes a managed event argument that a [Event] handler mutated through a `ref` parameter back into the native inout
// slot the engine fired the event with
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
        memory::copy(dst, converted, base_type.Size);
    }
    else {
        throw ScriptSystemException("Managed mutable event argument type is not supported", base_type.Name);
    }
}

static auto DispatchManagedEvent(shared_ptr<ManagedEventSubscription> subscription, FuncCallData& call) -> Entity::EventResult
{
    FO_STACK_TRACE_ENTRY();

    nptr<EngineMetadata> meta = subscription->Backend->GetMetadata();
    nptr<BaseEngine> engine = meta.dyn_cast<BaseEngine>();
    FO_VERIFY_AND_THROW(engine, "Managed event dispatch requires an engine context");

    Entity::EventResult result = Entity::EventResult::ContinueChain;
    RunManagedScriptEntry(subscription->Backend, engine, [handler = subscription->Handler] { return mono_gchandle_get_target(handler); }, [&] { result = DispatchManagedEventInContext(subscription, call); });
    return result;
}

static auto DispatchManagedEventInContext(shared_ptr<ManagedEventSubscription> subscription, FuncCallData& call) -> Entity::EventResult
{
    FO_STACK_TRACE_ENTRY();

    ActiveBackendScope active_backend {subscription->Backend};

    MonoDomain* domain = GetDomainOrThrow(subscription->Backend->GetDomain());

    ManagedThreadAttachment managed_thread {domain};

    if (mono_gchandle_get_target(subscription->Handler) == nullptr) {
        return Entity::EventResult::ContinueChain;
    }

    if (call.ArgsData.size() != subscription->Args.size()) {
        throw ScriptSystemException("Managed event argument count mismatch");
    }

    bool use_frame = subscription->UsesScalarFrame && subscription->AdaptInvoke;
    size_t first_event_arg = subscription->Args.size() == subscription->Slots.size() ? 0 : 1;
    array<uint8_t, MANAGED_ABI_SCALAR_FRAME_CAPACITY> frame {};

    if (use_frame) {
        FO_VERIFY_AND_THROW(subscription->FrameSize <= frame.size(), "Managed event frame exceeds adapter capacity", subscription->EventId, subscription->FrameSize);

        for (size_t i = 0; i < subscription->Slots.size(); i++) {
            const ManagedAbiSlot& slot = subscription->Slots[i];
            FO_VERIFY_AND_THROW(numeric_cast<size_t>(slot.Offset) + slot.Size <= subscription->FrameSize, "Managed event slot is outside the adapter frame", subscription->EventId);

            if (slot.Kind == ManagedAbiValueKind::Handle) {
                // The adapter wraps a non-nullable handle as non-null, so a null one takes the boxed path instead
                const void* handle = *ptr<void>(call.ArgsData[i + first_event_arg]).reinterpret_as<void*>();

                if (handle == nullptr && !slot.Nullable) {
                    use_frame = false;
                    break;
                }

                uint64_t handle_bits = numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(handle));
                memory::copy(frame.data() + slot.Offset, &handle_bits, sizeof(handle_bits));
            }
            else {
                memory::copy(ptr<void> {frame.data() + slot.Offset}, ptr<void>(call.ArgsData[i + first_event_arg]), slot.Size);
            }
        }
    }

    if (use_frame) {
        void* entity_ptr = nullptr;

        if (first_event_arg == 1) {
            entity_ptr = *static_cast<Entity**>(ptr<void>(call.ArgsData[0]).get());
        }

        MonoObject* handler = mono_gchandle_get_target(subscription->Handler);
        mono_bool has_result = subscription->HasExplicitResult ? 1 : 0;
        int32_t frame_size = numeric_cast<int32_t>(subscription->FrameSize);
        int32_t event_result = static_cast<int32_t>(Entity::EventResult::ContinueChain);
        void* args[] = {handler, &has_result, &entity_ptr, frame.data(), &frame_size, &event_result};
        (void)InvokeManagedScript(subscription->AdaptInvoke.get(), nullptr, args, "Managed event handler failed");

        for (size_t i = 0; i < subscription->Slots.size(); i++) {
            const ManagedAbiSlot& slot = subscription->Slots[i];

            if (!slot.Mutable) {
                continue;
            }

            memory::copy(ptr<void>(call.ArgsData[i + first_event_arg]), ptr<void> {frame.data() + slot.Offset}, slot.Size);
        }

        return static_cast<Entity::EventResult>(event_result);
    }

    MonoMethod* invoke_event = FindNativeMethod(subscription->Backend, "InvokeEvent", 3);

    // Root the boxed argument array across the managed invoke and re-fetch it on each use: DynamicInvoke writes
    // mutated ref arguments back into it
    uint32_t args_array_handle = NewManagedGcHandle(reinterpret_cast<MonoObject*>(mono_array_new(domain, mono_get_object_class(), subscription->Args.size())), 0);
    auto free_args_array_handle = scope_exit([args_array_handle]() noexcept { mono_gchandle_free(args_array_handle); });
    auto get_args_array = [args_array_handle]() -> MonoArray* { return reinterpret_cast<MonoArray*>(mono_gchandle_get_target(args_array_handle)); };

    for (size_t i = 0; i < subscription->Args.size(); i++) {
        MonoObject* arg = BoxNativeCallValue(subscription->Backend, subscription->Args[i], ptr<void>(call.ArgsData[i]).get(), call.Accessor.get());
        mono_array_setref(get_args_array(), i, arg);
    }

    mono_bool has_result = subscription->HasExplicitResult ? 1 : 0;
    void* args[] = {mono_gchandle_get_target(subscription->Handler), &has_result, get_args_array()};
    ManagedObjectRoot ret;
    ret.SetObject(InvokeManagedScript(invoke_event, nullptr, args, "Managed event handler failed"));

    for (size_t i = 0; i < subscription->Args.size(); i++) {
        if (subscription->Args[i].IsMutable) {
            MonoObject* mutated = mono_array_get(get_args_array(), MonoObject*, i);
            WriteBackManagedEventArg(subscription->Backend.as_ptr(), subscription->Args[i], mutated, ptr<void>(call.ArgsData[i]).get());
        }
    }

    if (ret.GetObject() == nullptr) {
        return Entity::EventResult::ContinueChain;
    }

    auto result = *static_cast<int32_t*>(mono_object_unbox(ret.GetObject()));
    return static_cast<Entity::EventResult>(result);
}

// === Remote-call argument marshaling ===

static auto SerializeManagedRemoteCallArgs(ptr<ManagedScriptBackend> backend, const vector<ArgDesc>& call_args, MonoArray* args_array, string_view name) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    // Serialize boxed managed args into the shared RemoteCallWire byte format (the same format the AngelScript
    // backend reads/writes). Scalar args only for now (collections/ref types come later).
    size_t arg_count = args_array != nullptr ? mono_array_length(args_array) : 0;

    if (arg_count != call_args.size()) {
        throw ScriptSystemException("Managed remote call argument count mismatch", name);
    }

    vector<uint8_t> data;
    data_writer writer(data);
    RemoteCallWireHooks hooks {
        .RefTypeToRaw = [](const BaseTypeDesc& type, ptr<void> arg) -> vector<uint8_t> {
            // Two levels of indirection: arg is `&storage.RefTypePtr` — the address of the slot, always valid, hence
            // ptr
            nptr<void> ref = *arg.reinterpret_as<nptr<void>>();

            if (!ref) {
                return vector<uint8_t> {};
            }

            span<const uint8_t> serialized = ref.reinterpret_as<DynamicRefTypeInstance>()->GetSerializedRawData(type);
            return vector<uint8_t>(serialized.begin(), serialized.end());
        },
    };

    for (size_t i = 0; i < call_args.size(); i++) {
        const auto& arg = call_args[i];
        MonoObject* arg_obj = mono_array_get(args_array, MonoObject*, i);

        if (arg.Type.BaseType.IsRefType && !IsDynamicManagedRefType(arg.Type.BaseType)) {
            throw ScriptSystemException("Managed remote call supports only dynamic ref-type args", string(name), arg.Type.BaseType.Name);
        }

        if (arg.Type.Kind == ComplexTypeKind::Simple) {
            ManagedScalarValue storage;
            void* native = ConvertManagedSimpleObjectToNative(backend, arg.Type.BaseType, arg_obj, storage);
            WriteRemoteCallSimple(writer, native, arg.Type.BaseType, hooks);
        }
        else if (arg.Type.Kind == ComplexTypeKind::Array) {
            // Wire: int32 count, then each element (shared scalar format) — matches AngelScript's array framing
            size_t count = arg_obj != nullptr ? GetManagedListCount(backend, arg_obj) : 0;
            writer.write<int32_t>(numeric_cast<int32_t>(count));

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

static void AppendRawBytes(vector<uint8_t>& data, const_span<uint8_t> bytes)
{
    FO_STACK_TRACE_ENTRY();

    if (bytes.empty()) {
        return;
    }

    size_t old_size = data.size();
    data.resize(old_size + bytes.size());
    memory::copy(data.data() + old_size, bytes.data(), bytes.size());
}

static void AppendAlignedRawBytes(vector<uint8_t>& data, const_span<uint8_t> bytes, size_t alignment)
{
    FO_STACK_TRACE_ENTRY();

    if (!bytes.empty()) {
        data.resize(align_up(data.size(), alignment));
    }

    AppendRawBytes(data, bytes);
}

template<typename T>
static void AppendRawValue(vector<uint8_t>& data, const T& value)
{
    FO_STACK_TRACE_ENTRY();

    AppendRawBytes(data, const_span<uint8_t> {reinterpret_cast<const uint8_t*>(std::addressof(value)), sizeof(value)});
}

template<typename T>
static void AppendAlignedRawValue(vector<uint8_t>& data, const T& value, size_t alignment)
{
    FO_STACK_TRACE_ENTRY();

    AppendAlignedRawBytes(data, const_span<uint8_t> {reinterpret_cast<const uint8_t*>(std::addressof(value)), sizeof(value)}, alignment);
}

// === Managed object creation and native<->managed values ===

static auto CreateHashObject(ptr<const ManagedScriptBackend> backend, const hstring& value) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    CountManagedObject();

    // The managed hstring is the native object representation, so boxing copies it as is
    MonoDomain* domain = GetDomainOrThrow(backend->GetDomain());
    MonoClass* hash_class = FindFOnlineClass(backend, "hstring");
    hstring copy = value;
    return mono_value_box(domain, hash_class, &copy);
}

static auto CreateEntityObject(ptr<const ManagedScriptBackend> backend, string_view type_name, nptr<Entity> entity) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    CountManagedObject();

    if (!entity) {
        return nullptr;
    }

    MonoDomain* domain = GetDomainOrThrow(backend->GetDomain());
    const ManagedWrapperClassEntry& wrapper = ResolveWrapperClass(backend, type_name);
    ManagedObjectRoot obj;
    obj.SetObject(mono_object_new(domain, wrapper.Class.get_no_const()));

    if (obj.GetObject() == nullptr) {
        throw ScriptSystemException("Can't create Managed entity wrapper", type_name);
    }

    void* entity_ptr = entity.void_cast();
    void* args[] = {&entity_ptr};
    MonoObject* exception = nullptr;
    mono_runtime_invoke(wrapper.PointerCtor.get_no_const(), obj.GetObject(), args, &exception);
    ThrowIfManagedException(exception, "Managed entity wrapper constructor failed");
    return obj.GetObject();
}

static auto CreatePropertyEnumObject(ptr<const ManagedScriptBackend> backend, string_view owner_type_name, ptr<const Property> prop) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    MonoDomain* domain = GetDomainOrThrow(backend->GetDomain());
    string property_enum_name = strex("{}Property", owner_type_name).str();
    MonoClass* enum_class = FindFOnlineClass(backend, property_enum_name);
    int32_t value = prop->GetRegIndex();
    return mono_value_box(domain, enum_class, &value);
}

static void InvokeManagedConstructor(MonoClass* klass, MonoObject* obj, int32_t args_count, void** args, string_view context)
{
    FO_STACK_TRACE_ENTRY();

    MonoMethod* ctor = mono_class_get_method_from_name(klass, ".ctor", args_count);

    if (ctor == nullptr) {
        throw ScriptSystemException("Managed constructor not found", context, args_count);
    }

    MonoObject* exception = nullptr;
    mono_runtime_invoke(ctor, obj, args, &exception);

    if (exception != nullptr) {
        ThrowIfManagedException(exception, strex("{} constructor failed", context).str());
    }
}

static auto CreateNativeRefTypeObject(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, void* ref_ptr) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    CountManagedObject();

    if (ref_ptr == nullptr) {
        return nullptr;
    }

    MonoDomain* domain = GetDomainOrThrow(backend->GetDomain());
    const ManagedWrapperClassEntry& wrapper = ResolveWrapperClass(backend, base_type.Name);
    ManagedObjectRoot obj;
    obj.SetObject(mono_object_new(domain, wrapper.Class.get_no_const()));

    if (obj.GetObject() == nullptr) {
        throw ScriptSystemException("Can't create Managed ref type wrapper", base_type.Name);
    }

    void* args[] = {&ref_ptr};
    MonoObject* exception = nullptr;
    mono_runtime_invoke(wrapper.PointerCtor.get_no_const(), obj.GetObject(), args, &exception);
    ThrowIfManagedException(exception, "Managed ref type wrapper constructor failed");
    return obj.GetObject();
}

static auto CreateDynamicRefTypeObject(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, span<const uint8_t> raw_data) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    CountManagedObject();

    FO_VERIFY_AND_THROW(IsDynamicManagedRefType(base_type), "Base type is not a dynamic managed ref type");

    MonoDomain* domain = GetDomainOrThrow(backend->GetDomain());
    MonoClass* klass = FindFOnlineClass(backend, base_type.Name);
    ManagedObjectRoot obj;
    obj.SetObject(mono_object_new(domain, klass));

    if (obj.GetObject() == nullptr) {
        throw ScriptSystemException("Can't create Managed dynamic ref type", base_type.Name);
    }

    InvokeManagedConstructor(klass, obj.GetObject(), 0, nullptr, base_type.Name);

    ptr<const PropertyRegistrar> fields_registrar = base_type.RefType->FieldsRegistrar;
    size_t data_pos = 0;

    for (size_t i = 1; i < fields_registrar->GetPropertiesCount(); i++) {
        auto field_prop = fields_registrar->GetPropertyByIndexUnsafe(i);
        span<const uint8_t> field_raw_data {};

        if (data_pos < raw_data.size()) {
            data_pos = align_up(data_pos, sizeof(uint32_t));

            if (data_pos > raw_data.size() || raw_data.size() - data_pos < sizeof(uint32_t)) {
                throw ScriptSystemException("Corrupted Managed dynamic ref type data", base_type.Name, field_prop->GetName());
            }

            uint32_t field_size;
            memory::copy(&field_size, raw_data.data() + data_pos, sizeof(field_size));
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
            SetManagedPropertyValue(backend, obj.GetObject(), field_prop, field_value);
        }
    }

    if (data_pos != raw_data.size()) {
        throw ScriptSystemException("Corrupted Managed dynamic ref type data", base_type.Name);
    }

    return obj.GetObject();
}

static auto CreateRefTypeObject(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, void* ref_ptr) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(base_type.IsRefType, "Base type is not a ref type");

    if (IsDynamicManagedRefType(base_type)) {
        if (ref_ptr == nullptr) {
            return nullptr;
        }

        ptr<DynamicRefTypeInstance> ref_instance = make_ptr(static_cast<DynamicRefTypeInstance*>(ref_ptr));
        span<const uint8_t> raw_data = ref_instance->GetSerializedRawData(base_type);
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

    ManagedObjectRoot value_root;
    value_root.SetObject(value);
    auto ref_instance = safe_alloc::make_refcounted<DynamicRefTypeInstance>(base_type.RefType->FieldsRegistrar.get());
    ptr<const PropertyRegistrar> fields_registrar = base_type.RefType->FieldsRegistrar;

    for (size_t i = 1; i < fields_registrar->GetPropertiesCount(); i++) {
        auto field_prop = fields_registrar->GetPropertyByIndexUnsafe(i);
        MonoObject* field_value = GetManagedPropertyValue(backend, value_root.GetObject(), field_prop);
        PropertyRawData field_data = ConvertManagedObjectToPropertyData(backend, field_prop.get(), field_value);
        ref_instance->SetValue(field_prop, field_data);
    }

    return ref_instance;
}

// A value type has the same layout on both sides (see StructLayoutDesc), so boxing and unboxing are single copies;
// the size check is what holds the managed declaration to that layout
static auto GetManagedStructClass(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type) -> MonoClass*
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(base_type.IsStruct && base_type.StructLayout, "Base type is not a value type", base_type.Name);
    MonoClass* klass = FindFOnlineClass(backend, base_type.Name);
    FO_VERIFY_AND_THROW(klass != nullptr, "Managed value type not found", base_type.Name);
    FO_VERIFY_AND_THROW(numeric_cast<size_t>(mono_class_value_size(klass, nullptr)) == base_type.Size, "Managed value type size does not match its layout", base_type.Name, base_type.Size);
    return klass;
}

static void CopyManagedStructToNative(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, MonoObject* value, void* data)
{
    FO_STACK_TRACE_ENTRY();

    (void)GetManagedStructClass(backend, base_type);
    FO_VERIFY_AND_THROW(value != nullptr, "Managed value type object is null", base_type.Name);
    memory::copy(data, mono_object_unbox(value), base_type.Size);
}

static void CopyManagedStructToPropertyData(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, MonoObject* value, void* data)
{
    FO_STACK_TRACE_ENTRY();

    CopyManagedStructToNative(backend, base_type, value, data);
    ValueToPropertyData(base_type, static_cast<uint8_t*>(data));
}

static auto CreateStructObject(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, void* data) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    CountManagedObject();

    MonoClass* klass = GetManagedStructClass(backend, base_type);
    MonoObject* obj = mono_value_box(GetDomainOrThrow(backend->GetDomain()), klass, data);
    FO_VERIFY_AND_THROW(obj != nullptr, "Can't create Managed struct", base_type.Name);
    return obj;
}

static auto CreatePropertyStructObject(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& base_type, span<const uint8_t> raw_data) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(raw_data.size() == base_type.Size, "Raw property struct size does not match the value type size", base_type.Name, raw_data.size(), base_type.Size);

    small_vector<uint8_t, 64> value(raw_data.begin(), raw_data.end());
    PropertyDataToValue(backend, base_type, value.data());
    return CreateStructObject(backend, base_type, value.data());
}

static auto GetManagedPropertyValue(ptr<const ManagedScriptBackend> backend, MonoObject* obj, ptr<const Property> field_prop) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    if (obj == nullptr) {
        return nullptr;
    }

    ManagedDynamicFieldAccessors accessors = ResolveDynamicRefTypeField(backend, obj, field_prop);
    FO_VERIFY_AND_THROW(accessors.Getter, "Managed property getter not found", field_prop->GetName());
    MonoObject* exception = nullptr;
    MonoObject* result = mono_runtime_invoke(accessors.Getter.get_no_const(), obj, nullptr, &exception);

    if (exception != nullptr) {
        ThrowIfManagedException(exception, strex("Managed property getter failed: {}", field_prop->GetName()).str());
    }

    return result;
}

static void SetManagedPropertyValue(ptr<const ManagedScriptBackend> backend, MonoObject* obj, ptr<const Property> field_prop, MonoObject* value)
{
    FO_STACK_TRACE_ENTRY();

    ManagedDynamicFieldAccessors accessors = ResolveDynamicRefTypeField(backend, obj, field_prop);
    FO_VERIFY_AND_THROW(accessors.Setter, "Managed property setter not found", field_prop->GetName());

    // mono_runtime_invoke wants the unboxed value pointer for a value-type parameter (int/enum/bool/struct) but the
    // object itself for a reference type (string/object/entity)
    void* arg = value != nullptr && (mono_class_is_valuetype(mono_object_get_class(value)) != 0) ? mono_object_unbox(value) : value;
    void* args[] = {arg};
    MonoObject* exception = nullptr;
    mono_runtime_invoke(accessors.Setter.get_no_const(), obj, args, &exception);

    if (exception != nullptr) {
        ThrowIfManagedException(exception, strex("Managed property setter failed: {}", field_prop->GetName()).str());
    }
}

// Read-only after the ABI binds; a field met before that is looked up by its C# name on the spot
static auto ResolveDynamicRefTypeField(ptr<const ManagedScriptBackend> backend, MonoObject* obj, ptr<const Property> field_prop) -> ManagedDynamicFieldAccessors
{
    FO_STACK_TRACE_ENTRY();

    auto caches = backend->GetCaches();
    FO_VERIFY_AND_THROW(caches, "Managed backend caches are not created");

    if (auto it = caches->DynamicFields.find(field_prop.get()); it != caches->DynamicFields.end()) {
        return it->second;
    }

    string field_name = MakeManagedDynamicRefTypePropertyName(field_prop);
    MonoProperty* prop = mono_class_get_property_from_name(mono_object_get_class(obj), field_name.c_str());
    FO_VERIFY_AND_THROW(prop != nullptr, "Managed property not found", field_name);

    ManagedDynamicFieldAccessors accessors;
    accessors.Getter = mono_property_get_get_method(prop);
    accessors.Setter = mono_property_get_set_method(prop);
    return accessors;
}

// === Managed collections (list/dictionary/delegate) ===

static auto CreateManagedList(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& element_type) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    CountManagedObject();

    MonoDomain* domain = GetDomainOrThrow(backend->GetDomain());
    MonoClass* element_class = GetManagedClass(backend, element_type);
    MonoReflectionType* reflection_type = mono_type_get_object(domain, mono_class_get_type(element_class));
    void* args[] = {reflection_type};
    MonoObject* list = InvokeNativeHelper(backend, "CreateList", 1, args);

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
    MonoObject* result = InvokeNativeHelper(backend, "GetListCount", 1, args);

    if (result == nullptr) {
        throw ScriptSystemException("Managed list count failed");
    }

    int32_t count = *static_cast<int32_t*>(mono_object_unbox(result));
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

    CountManagedObject();

    MonoDomain* domain = GetDomainOrThrow(backend->GetDomain());
    MonoClass* key_class = GetManagedClass(backend, key_type);
    MonoClass* value_class = GetManagedClass(backend, value_type);
    ManagedObjectRoot key_reflection_type;
    key_reflection_type.SetObject(reinterpret_cast<MonoObject*>(mono_type_get_object(domain, mono_class_get_type(key_class))));
    MonoReflectionType* value_reflection_type = mono_type_get_object(domain, mono_class_get_type(value_class));
    void* args[] = {key_reflection_type.GetObject(), value_reflection_type};
    MonoObject* dictionary = InvokeNativeHelper(backend, "CreateDictionary", 2, args);

    if (dictionary == nullptr) {
        throw ScriptSystemException("Managed dictionary creation failed", key_type.Name, value_type.Name);
    }

    return dictionary;
}

static auto CreateManagedDictionaryOfList(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& key_type, const BaseTypeDesc& element_type) -> MonoObject*
{
    FO_STACK_TRACE_ENTRY();

    CountManagedObject();

    MonoDomain* domain = GetDomainOrThrow(backend->GetDomain());
    MonoClass* key_class = GetManagedClass(backend, key_type);
    MonoClass* element_class = GetManagedClass(backend, element_type);
    ManagedObjectRoot key_reflection_type;
    key_reflection_type.SetObject(reinterpret_cast<MonoObject*>(mono_type_get_object(domain, mono_class_get_type(key_class))));
    MonoReflectionType* element_reflection_type = mono_type_get_object(domain, mono_class_get_type(element_class));
    void* args[] = {key_reflection_type.GetObject(), element_reflection_type};
    MonoObject* dictionary = InvokeNativeHelper(backend, "CreateDictionaryOfList", 2, args);

    if (dictionary == nullptr) {
        throw ScriptSystemException("Managed dictionary-of-list creation failed", key_type.Name, element_type.Name);
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
    MonoObject* result = InvokeNativeHelper(backend, "GetDictionaryCount", 1, args);

    if (result == nullptr) {
        throw ScriptSystemException("Managed dictionary count failed");
    }

    int32_t count = *static_cast<int32_t*>(mono_object_unbox(result));
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
    MonoObject* result = InvokeNativeHelper(backend, "GetDelegateKey", 1, args);
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

    MonoMethod* method = FindNativeMethod(backend, method_name, numeric_cast<int32_t>(args_count));
    MonoObject* exception = nullptr;
    MonoObject* result = mono_runtime_invoke(method, nullptr, args, &exception);

    // The context string is built only for a failure, not on every helper call
    if (exception != nullptr) {
        ThrowIfManagedException(exception, strex("Managed Native.{} failed", method_name).str());
    }

    return result;
}

static auto InvokeNativeBoolHelper(ptr<const ManagedScriptBackend> backend, const char* method_name, MonoObject* value) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (value == nullptr) {
        return false;
    }

    void* args[] = {value};
    MonoObject* result = InvokeNativeHelper(backend, method_name, 1, args);

    if (result == nullptr) {
        throw ScriptSystemException("Managed Native bool helper returned null", method_name);
    }

    return *static_cast<mono_bool*>(mono_object_unbox(result)) != 0;
}

// The managed entity hierarchy is not the native one: managed ProtoCritter derives from Critter, while
// natively it derives from ProtoEntity and shares no base with it - see Docs/Scripts.md
static void ValidateManagedEntityKind(const BaseTypeDesc& base_type, nptr<Entity> entity)
{
    FO_STACK_TRACE_ENTRY();

    if (!entity || base_type.IsEntityProto || base_type.IsFixedType || base_type.IsAbstractEntity) {
        return;
    }

    FO_VERIFY_AND_THROW(!entity.dyn_cast<const ProtoEntity>(), "A prototype was passed where a live entity is expected -- the two are unrelated native types", base_type.Name, entity->GetName());
}

// A hashed string is one intern handle in managed and native code alike and moves by memcpy. Property storage is
// the one place that keeps its hash instead, so these two swap hash and handle in place, field by field
static void PropertyDataToValue(ptr<const ManagedScriptBackend> backend, const BaseTypeDesc& type, uint8_t* data)
{
    FO_STACK_TRACE_ENTRY();

    static_assert(sizeof(hstring) == sizeof(hstring::hash_t));

    if (type.IsHashedString) {
        hstring::hash_t hash = 0;
        memory::copy(&hash, data, sizeof(hash));
        hstring value = ResolveManagedHashValue(backend, hash);
        memory::copy(data, &value, sizeof(value));
    }
    else if (type.IsStruct && type.StructLayout) {
        for (const FieldDesc& field : type.StructLayout->Fields) {
            if (field.Type.IsHashedString || field.Type.IsStruct) {
                PropertyDataToValue(backend, field.Type, data + field.Offset);
            }
        }
    }
}

static void ValueToPropertyData(const BaseTypeDesc& type, uint8_t* data)
{
    FO_STACK_TRACE_ENTRY();

    if (type.IsHashedString) {
        hstring value;
        memory::copy(&value, data, sizeof(value));
        hstring::hash_t hash = value.as_hash();
        memory::copy(data, &hash, sizeof(hash));
    }
    else if (type.IsStruct && type.StructLayout) {
        for (const FieldDesc& field : type.StructLayout->Fields) {
            if (field.Type.IsHashedString || field.Type.IsStruct) {
                ValueToPropertyData(field.Type, data + field.Offset);
            }
        }
    }
}

// A handle slot arrives from managed code as a bare pointer, so it is held to what a boxed argument is held to
static void ValidateManagedFrameHandle(const ManagedAbiSlot& slot, const ArgDesc& arg, const uint8_t* slot_data, string_view owner, string_view name)
{
    FO_STACK_TRACE_ENTRY();

    if (slot.Kind != ManagedAbiValueKind::Handle) {
        return;
    }

    void* handle = nullptr;
    memory::copy(&handle, slot_data, sizeof(handle));
    FO_VERIFY_AND_THROW(handle != nullptr || slot.Nullable, "Managed handle argument is null", owner, name, arg.Name);

    if (!arg.Type.BaseType.IsRefType) {
        ValidateManagedEntityKind(arg.Type.BaseType, static_cast<Entity*>(handle));
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
        storage.Hash = ExtractNativeHstring(value);
        return &storage.Hash;
    }
    if (base_type.IsEntity || base_type.IsFixedType || base_type.IsEntityProto) {
        storage.EntityPtr = ExtractEntityPtr(value);
        ValidateManagedEntityKind(base_type, storage.EntityPtr);
        return &storage.EntityPtr;
    }
    if (base_type.IsRefType) {
        if (IsDynamicManagedRefType(base_type)) {
            storage.DynamicRefType = CreateDynamicRefTypeFromManaged(backend, base_type, value);
            storage.RefTypePtr = storage.DynamicRefType;
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

        void* data = storage.Alloc(base_type);

        if (base_type.IsStruct) {
            CopyManagedStructToNative(backend, base_type, value, data);
        }
        else {
            memory::copy(data, mono_object_unbox(value), base_type.Size);
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
        storage.Array = safe_alloc::make_unique<ManagedArrayBridgeData>();
        storage.Array->Backend = backend;
        storage.Array->Type = type;
        storage.Array->SetObject(value);
        return storage.Array.get();
    }
    if (type.Kind == ComplexTypeKind::Dict) {
        storage.Dict = safe_alloc::make_unique<ManagedDictBridgeData>();
        storage.Dict->Backend = backend;
        storage.Dict->Type = type;
        storage.Dict->SetObject(value);
        return storage.Dict.get();
    }
    if (type.Kind == ComplexTypeKind::Callback) {
        if (value == nullptr) {
            storage.Callback = safe_alloc::make_unique<ManagedCallbackBridgeData>();
            storage.Callback->Backend = backend;
            storage.Callback->Domain = GetDomainOrThrow(backend->GetDomain());
            storage.Callback->Type = type;
            return storage.Callback.get();
        }

        string delegate_key = GetManagedDelegateKey(backend, value);
        storage.Callback = safe_alloc::make_unique<ManagedCallbackBridgeData>();
        storage.Callback->Backend = backend;
        storage.Callback->Domain = GetDomainOrThrow(backend->GetDomain());
        storage.Callback->Type = type;
        storage.Callback->Handler = NewManagedGcHandle(value, false);
        storage.Callback->Name = backend->GetMetadata()->Hashes.to_hashed_string(strex("ManagedCallback:{}", delegate_key).str());
        return storage.Callback.get();
    }

    throw ScriptSystemException("Unsupported Managed argument type", type.BaseType.Name);
}

static void ReconcileMutableDynamicRefTypeOwner(const ComplexTypeDesc& type, ManagedNativeValue& storage) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!type.IsMutable || type.Kind != ComplexTypeKind::Simple || !IsDynamicManagedRefType(type.BaseType)) {
        return;
    }

    nptr<DynamicRefTypeInstance> current = storage.RefTypePtr.reinterpret_as<DynamicRefTypeInstance>();

    if (storage.DynamicRefType.as_nptr() == current) {
        return;
    }

    // AngelScript receives a mutable ref type as an owning handle slot
    (void)storage.DynamicRefType.release_ownership();

    if (current) {
        storage.DynamicRefType = refcount_nptr<DynamicRefTypeInstance>::from_adopted_ref(current.get_no_const());
    }
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

    for (MonoClass* klass = mono_object_get_class(value); klass != nullptr; klass = mono_class_get_parent(klass)) {
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
        return value != nullptr && ManagedObjectClassMatches(value, GetValueClass(backend, base_type));
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

    CountManagedObject();

    MonoDomain* domain = GetDomainOrThrow(backend->GetDomain());

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
        return CreateHashObject(backend, value);
    }
    if (base_type.IsEntity || base_type.IsFixedType || base_type.IsEntityProto) {
        nptr<Entity> entity = *static_cast<Entity**>(data);
        string managed_type_name = base_type.Name;

        // Preserve the concrete runtime type when boxing an abstract entity argument or the Entity base
        // itself: a script receiving an Entity parameter cannot downcast unless the wrapper carries it
        if (entity && (base_type.IsAbstractEntity || base_type.Name == "Entity")) {
            string entity_type_name = string(entity->GetTypeName());
            managed_type_name = entity.dyn_cast<const ProtoEntity>() ? strex("Proto{}", entity_type_name).str() : entity_type_name;
        }

        return CreateEntityObject(backend, managed_type_name, entity);
    }
    if (base_type.IsRefType) {
        void* ref_ptr = *static_cast<void**>(data);
        return CreateRefTypeObject(backend, base_type, ref_ptr);
    }
    if (base_type.IsStruct && base_type.StructLayout) {
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

    MonoDomain* domain = GetDomainOrThrow(backend->GetDomain());

    if (base_type.Name == "any") {
        return reinterpret_cast<MonoObject*>(mono_string_new_len(domain, reinterpret_cast<const char*>(raw_data.data()), numeric_cast<uint32_t>(raw_data.size())));
    }
    if (base_type.IsString) {
        return reinterpret_cast<MonoObject*>(mono_string_new_len(domain, reinterpret_cast<const char*>(raw_data.data()), numeric_cast<uint32_t>(raw_data.size())));
    }
    if (base_type.IsHashedString) {
        FO_VERIFY_AND_THROW(raw_data.size() == sizeof(hstring::hash_t), "Hashed string raw data size does not match a hash");
        hstring::hash_t value = *reinterpret_cast<const hstring::hash_t*>(raw_data.data());
        return CreateHashObject(backend, ResolveManagedHashValue(backend, value));
    }
    if (base_type.IsFixedType || base_type.IsEntityProto) {
        return CreateEntityObject(backend, base_type.Name, ResolveProtoEntityFromRawData(backend, base_type, raw_data));
    }
    if (base_type.IsRefType && IsDynamicManagedRefType(base_type)) {
        return CreateDynamicRefTypeObject(backend, base_type, raw_data);
    }
    if (base_type.IsPrimitive || base_type.IsEnum || base_type.IsStruct) {
        FO_VERIFY_AND_THROW(raw_data.size() == base_type.Size, "Raw data size does not match the value type size");

        if (base_type.IsStruct && base_type.StructLayout) {
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
        ManagedObjectRoot list;
        list.SetObject(CreateManagedList(backend, base_type));
        const uint8_t* data = raw_data.data();
        const uint8_t* data_end = raw_data.data() + raw_data.size();

        if (raw_data.empty()) {
            return list.GetObject();
        }

        if (prop->IsArrayOfString()) {
            auto data_span = const_span<uint8_t> {raw_data.data(), raw_data.size()};
            size_t data_pos = 0;
            uint32_t arr_size = span_read_aligned_object<uint32_t>(data_span, data_pos);

            for (uint32_t i = 0; i < arr_size; i++) {
                uint32_t str_size = span_read_aligned_object<uint32_t>(data_span, data_pos);
                string text = span_read_string(data_span, data_pos, str_size);
                MonoObject* item = reinterpret_cast<MonoObject*>(mono_string_new_len(GetDomainOrThrow(backend->GetDomain()), text.data(), numeric_cast<uint32_t>(text.size())));
                AddManagedListItem(backend, list.GetObject(), item);
            }

            data = raw_data.data() + data_pos;
        }
        else if (prop->IsBaseTypeRefType()) {
            if (!IsDynamicManagedRefType(base_type)) {
                throw ScriptSystemException("Managed property ref type array is not supported", prop->GetName());
            }

            auto data_span = const_span<uint8_t> {raw_data.data(), raw_data.size()};
            size_t data_pos = 0;
            uint32_t arr_size = span_read_aligned_object<uint32_t>(data_span, data_pos);

            for (uint32_t i = 0; i < arr_size; i++) {
                uint32_t ref_size = span_read_aligned_object<uint32_t>(data_span, data_pos);
                auto ref_data = span_read_aligned_bytes(data_span, data_pos, ref_size, MAX_SERIALIZED_ALIGNMENT);
                MonoObject* item = CreateDynamicRefTypeObject(backend, base_type, ref_data);
                AddManagedListItem(backend, list.GetObject(), item);
            }

            data = raw_data.data() + data_pos;
        }
        else {
            FO_VERIFY_AND_THROW(raw_data.size() % base_type.Size == 0, "Array property raw data size is not a multiple of the element size");
            size_t arr_size = raw_data.size() / base_type.Size;

            for (size_t i = 0; i < arr_size; i++) {
                MonoObject* item = BoxSimplePropertyValue(backend, base_type, {data, base_type.Size});
                AddManagedListItem(backend, list.GetObject(), item);
                data += base_type.Size;
            }
        }

        if (data != data_end) {
            throw ScriptSystemException("Corrupted Managed array property tail", prop->GetName());
        }

        return list.GetObject();
    }
    if (prop->IsDict()) {
        if (!IsManagedBridgeDictionaryProperty(prop)) {
            throw ScriptSystemException("Managed dictionary property type is not supported", prop->GetName());
        }

        const BaseTypeDesc& key_type = prop->GetDictKeyType();
        ManagedObjectRoot dictionary;
        dictionary.SetObject(prop->IsDictOfArray() ? CreateManagedDictionaryOfList(backend, key_type, base_type) : CreateManagedDictionary(backend, key_type, base_type));

        if (raw_data.empty()) {
            return dictionary.GetObject();
        }
        if (key_type.Size == 0 || (!prop->IsDictOfArray() && base_type.Size == 0)) {
            throw ScriptSystemException("Corrupted Managed dictionary property", prop->GetName());
        }

        auto data_span = const_span<uint8_t> {raw_data.data(), raw_data.size()};
        size_t data_pos = 0;

        while (data_pos < raw_data.size()) {
            auto key_data = span_read_aligned_bytes(data_span, data_pos, key_type.Size, alignment_for_size(key_type.Size));
            ManagedObjectRoot key;
            key.SetObject(BoxSimplePropertyValue(backend, key_type, key_data));

            if (prop->IsDictOfArray()) {
                ManagedObjectRoot list;
                list.SetObject(CreateManagedList(backend, base_type));
                uint32_t arr_size = span_read_aligned_object<uint32_t>(data_span, data_pos);

                for (uint32_t i = 0; i < arr_size; i++) {
                    MonoObject* item = nullptr;

                    if (prop->IsDictOfArrayOfString()) {
                        uint32_t text_size = span_read_aligned_object<uint32_t>(data_span, data_pos);
                        auto text_data = span_read_bytes(data_span, data_pos, text_size);
                        item = BoxSimplePropertyValue(backend, base_type, text_data);
                    }
                    else {
                        auto item_data = span_read_aligned_bytes(data_span, data_pos, base_type.Size, alignment_for_size(base_type.Size));
                        item = BoxSimplePropertyValue(backend, base_type, item_data);
                    }

                    AddManagedListItem(backend, list.GetObject(), item);
                }

                AddManagedDictionaryItem(backend, dictionary.GetObject(), key.GetObject(), list.GetObject());
            }
            else {
                auto value_data = span_read_aligned_bytes(data_span, data_pos, base_type.Size, alignment_for_size(base_type.Size));
                MonoObject* item = BoxSimplePropertyValue(backend, base_type, value_data);
                AddManagedDictionaryItem(backend, dictionary.GetObject(), key.GetObject(), item);
            }
        }

        if (data_pos != raw_data.size()) {
            throw ScriptSystemException("Corrupted Managed dictionary property tail", prop->GetName());
        }

        return dictionary.GetObject();
    }

    return BoxSimplePropertyValue(backend, base_type, raw_data);
}

static auto ConvertManagedSimpleObjectToPropertyData(ptr<ManagedScriptBackend> backend, const BaseTypeDesc& base_type, MonoObject* value) -> PropertyRawData
{
    FO_STACK_TRACE_ENTRY();

    PropertyRawData prop_data;

    if (base_type.Name == "any") {
        string text = ManagedObjectToString(value);
        prop_data.Set(text.data(), text.size());
    }
    else if (base_type.IsString) {
        string text = ToStringAndFree(reinterpret_cast<MonoString*>(value));
        prop_data.Set(text.data(), text.size());
    }
    else if (base_type.IsHashedString) {
        prop_data.SetAs(ExtractNativeHstring(value).as_hash());
    }
    else if (base_type.IsFixedType || base_type.IsEntityProto) {
        hstring::hash_t proto_hash = ExtractProtoHashFromManagedEntity(value);
        prop_data.SetAs(proto_hash);
    }
    else if (base_type.IsRefType && IsDynamicManagedRefType(base_type)) {
        refcount_nptr<DynamicRefTypeInstance> ref_instance = CreateDynamicRefTypeFromManaged(backend, base_type, value);

        if (ref_instance) {
            span<const uint8_t> raw_data = ref_instance->GetSerializedRawData(base_type);

            if (!raw_data.empty()) {
                prop_data.Set(raw_data.data(), raw_data.size());
            }
        }
    }
    else if (base_type.IsPrimitive || base_type.IsEnum || base_type.IsStruct) {
        if (value == nullptr) {
            throw ScriptSystemException("Null passed to Managed property value", base_type.Name);
        }

        ptr<void> data = prop_data.Alloc(base_type.Size);

        if (base_type.IsStruct && base_type.StructLayout) {
            CopyManagedStructToPropertyData(backend, base_type, value, data.get());
        }
        else {
            memory::copy(data, mono_object_unbox(value), base_type.Size);
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

    ManagedObjectRoot collection;
    collection.SetObject(value);

    if (prop->IsDict()) {
        if (!IsManagedBridgeDictionaryProperty(prop)) {
            throw ScriptSystemException("Managed dictionary property type is not supported", prop->GetName());
        }

        PropertyRawData prop_data;
        const BaseTypeDesc& key_type = prop->GetDictKeyType();
        size_t dict_size = GetManagedDictionaryCount(backend, collection.GetObject());

        if (dict_size == 0) {
            return prop_data;
        }

        vector<uint8_t> data;
        data.reserve(dict_size * (key_type.Size + base_type.Size));

        for (size_t i = 0; i < dict_size; i++) {
            MonoObject* key = GetManagedDictionaryKey(backend, collection.GetObject(), i);
            PropertyRawData key_data = ConvertManagedSimpleObjectToPropertyData(backend, key_type, key);

            if (key_data.GetSize() != key_type.Size) {
                throw ScriptSystemException("Managed property dictionary key size mismatch", prop->GetName());
            }

            AppendAlignedRawBytes(data, const_span<uint8_t> {reinterpret_cast<const uint8_t*>(key_data.GetPtr().get()), key_data.GetSize()}, alignment_for_size(key_data.GetSize()));

            MonoObject* item = GetManagedDictionaryValue(backend, collection.GetObject(), i);

            if (prop->IsDictOfArray()) {
                ManagedObjectRoot list;
                list.SetObject(item);
                size_t arr_size = GetManagedListCount(backend, list.GetObject());
                uint32_t arr_size_value = numeric_cast<uint32_t>(arr_size);
                AppendAlignedRawValue(data, arr_size_value, sizeof(uint32_t));

                for (size_t j = 0; j < arr_size; j++) {
                    MonoObject* list_item = GetManagedListItem(backend, list.GetObject(), j);
                    PropertyRawData item_data = ConvertManagedSimpleObjectToPropertyData(backend, base_type, list_item);

                    if (prop->IsDictOfArrayOfString()) {
                        uint32_t item_size = numeric_cast<uint32_t>(item_data.GetSize());
                        AppendAlignedRawValue(data, item_size, sizeof(uint32_t));
                        AppendRawBytes(data, const_span<uint8_t> {reinterpret_cast<const uint8_t*>(item_data.GetPtr().get()), item_data.GetSize()});
                    }
                    else {
                        if (item_data.GetSize() != base_type.Size) {
                            throw ScriptSystemException("Managed property dictionary array item size mismatch", prop->GetName());
                        }

                        AppendAlignedRawBytes(data, const_span<uint8_t> {reinterpret_cast<const uint8_t*>(item_data.GetPtr().get()), item_data.GetSize()}, alignment_for_size(item_data.GetSize()));
                    }
                }
            }
            else {
                PropertyRawData item_data = ConvertManagedSimpleObjectToPropertyData(backend, base_type, item);

                if (item_data.GetSize() != base_type.Size) {
                    throw ScriptSystemException("Managed property dictionary value size mismatch", prop->GetName());
                }

                AppendAlignedRawBytes(data, const_span<uint8_t> {reinterpret_cast<const uint8_t*>(item_data.GetPtr().get()), item_data.GetSize()}, alignment_for_size(item_data.GetSize()));
            }
        }

        prop_data.Set(data.data(), data.size());
        return prop_data;
    }

    PropertyRawData prop_data;
    size_t arr_size = GetManagedListCount(backend, collection.GetObject());

    if (arr_size == 0) {
        return prop_data;
    }

    vector<uint8_t> data;

    if (prop->IsArrayOfString()) {
        uint32_t arr_size_value = numeric_cast<uint32_t>(arr_size);
        AppendAlignedRawValue(data, arr_size_value, sizeof(uint32_t));

        for (size_t i = 0; i < arr_size; i++) {
            MonoObject* item = GetManagedListItem(backend, collection.GetObject(), i);
            string text = ToStringAndFree(reinterpret_cast<MonoString*>(item));
            uint32_t text_size = numeric_cast<uint32_t>(text.size());
            AppendAlignedRawValue(data, text_size, sizeof(uint32_t));
            AppendRawBytes(data, const_span<uint8_t> {reinterpret_cast<const uint8_t*>(text.data()), text.size()});
        }
    }
    else if (prop->IsBaseTypeRefType()) {
        if (!IsDynamicManagedRefType(base_type)) {
            throw ScriptSystemException("Managed property ref type array is not supported", prop->GetName());
        }

        uint32_t arr_size_value = numeric_cast<uint32_t>(arr_size);
        AppendRawValue(data, arr_size_value);

        for (size_t i = 0; i < arr_size; i++) {
            MonoObject* item = GetManagedListItem(backend, collection.GetObject(), i);
            refcount_nptr<DynamicRefTypeInstance> ref_instance = CreateDynamicRefTypeFromManaged(backend, base_type, item);
            span<const uint8_t> raw_data;

            if (ref_instance) {
                raw_data = ref_instance->GetSerializedRawData(base_type);
            }

            uint32_t ref_size = numeric_cast<uint32_t>(raw_data.size());
            AppendAlignedRawValue(data, ref_size, sizeof(uint32_t));
            AppendAlignedRawBytes(data, raw_data, MAX_SERIALIZED_ALIGNMENT);
        }
    }
    else {
        data.reserve(arr_size * base_type.Size);

        for (size_t i = 0; i < arr_size; i++) {
            MonoObject* item = GetManagedListItem(backend, collection.GetObject(), i);
            PropertyRawData item_data = ConvertManagedSimpleObjectToPropertyData(backend, base_type, item);

            if (item_data.GetSize() != base_type.Size) {
                throw ScriptSystemException("Managed property array item size mismatch", prop->GetName());
            }

            AppendRawBytes(data, const_span<uint8_t> {reinterpret_cast<const uint8_t*>(item_data.GetPtr().get()), item_data.GetSize()});
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
    // both sides), so it crosses the bridge like an entity proto even though it is not flagged IsEntity
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

static auto IsManagedBridgeDictionaryArrayValueType(const BaseTypeDesc& type) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Name == "any" || type.IsString || IsManagedBridgeFixedDictionaryValueType(type);
}

static auto IsManagedBridgeDictionaryProperty(ptr<const Property> prop) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!prop->IsDict() || prop->IsDictKeyString() || !IsManagedBridgeFixedDictionaryValueType(prop->GetDictKeyType())) {
        return false;
    }

    if (prop->IsDictOfArray()) {
        return IsManagedBridgeDictionaryArrayValueType(prop->GetBaseType());
    }

    return !prop->IsDictOfString() && IsManagedBridgeFixedDictionaryValueType(prop->GetBaseType());
}

static auto IsDynamicManagedRefType(const BaseTypeDesc& base_type) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return base_type.IsRefType && base_type.RefType && base_type.RefType->FieldsRegistrar;
}

static auto MakeManagedDynamicRefTypePropertyName(ptr<const Property> prop) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (prop->IsInComponent()) {
        return strex("{}{}", prop->GetComponentName(), prop->GetNameWithoutComponent()).str();
    }

    return string {prop->GetNameWithoutComponent()};
}

// === Type/class and metadata resolution ===

// The cache is complete once the ABI is bound and nothing writes it afterwards, so workers read it without a lock; a
// name it does not hold is resolved on the spot and never added
static auto ResolveWrapperClass(ptr<const ManagedScriptBackend> backend, string_view type_name) -> ManagedWrapperClassEntry
{
    FO_STACK_TRACE_ENTRY();

    auto caches = backend->GetCaches();
    FO_VERIFY_AND_THROW(caches, "Managed backend caches are not created");

    auto it = caches->WrapperClasses.find(type_name);

    if (it != caches->WrapperClasses.end()) {
        return it->second;
    }

    ManagedWrapperClassEntry entry = FindWrapperClass(backend, type_name);
    FO_VERIFY_AND_THROW(entry.Class, "Managed wrapper class not found", type_name);
    FO_VERIFY_AND_THROW(entry.PointerCtor, "Managed wrapper pointer constructor not found", type_name);
    return entry;
}

static auto FindWrapperClass(ptr<const ManagedScriptBackend> backend, string_view type_name) -> ManagedWrapperClassEntry
{
    FO_STACK_TRACE_ENTRY();

    ManagedWrapperClassEntry entry;
    entry.Class = FindFOnlineClass(backend, type_name);

    if (entry.Class) {
        entry.PointerCtor = mono_class_get_method_from_name(entry.Class.get(), ".ctor", 1);
    }

    return entry;
}

// Runs while the assembly binds its ABI, before any worker can wrap a pointer; a class this target does not generate
// is simply left out
static void BuildWrapperClassCache(ptr<ManagedScriptBackend> backend)
{
    FO_STACK_TRACE_ENTRY();

    auto caches = backend->GetCaches();
    FO_VERIFY_AND_THROW(caches, "Managed backend caches are not created");
    nptr<EngineMetadata> meta = backend->GetMetadata();
    FO_VERIFY_AND_THROW(meta, "Backend metadata is not available");

    for (const string& type_name : CollectManagedAbiWrapperClasses(*meta)) {
        if (caches->WrapperClasses.contains(type_name)) {
            continue;
        }

        ManagedWrapperClassEntry entry = FindWrapperClass(backend, type_name);

        if (entry.Class && entry.PointerCtor) {
            caches->WrapperClasses.emplace(type_name, entry);
        }
    }

    // Every Native helper the bridge calls by name, so no conversion looks one up per element
    MonoClass* native_class = FindFOnlineClass(backend, "Native");
    FO_VERIFY_AND_THROW(native_class != nullptr, "Managed Native class not found");

    for (const auto& [method_name, args_count] : MANAGED_NATIVE_HELPERS) {
        MonoMethod* method = mono_class_get_method_from_name(native_class, method_name.data(), args_count);
        FO_VERIFY_AND_THROW(method != nullptr, "Managed Native helper method not found", method_name, args_count);
        caches->NativeMethods.emplace(string {method_name}, method);
    }

    // Every class the bridge names by metadata type, so a conversion does not search the images; a type this target
    // does not generate is simply left out
    unordered_map<string, nptr<MonoClass>> classes;

    auto add_class = [&](string_view class_name) {
        if (classes.contains(class_name)) {
            return;
        }

        for (nptr<void> image_ptr : backend->GetImages()) {
            nptr<MonoImage> image = image_ptr.reinterpret_as<MonoImage>();

            if (MonoClass* klass = mono_class_from_name(image.get(), "FOnline", string {class_name}.c_str()); klass != nullptr) {
                classes.emplace(string {class_name}, klass);
                return;
            }
        }
    };

    add_class("Native");
    add_class("hstring");

    for (const string& type_name : CollectManagedAbiWrapperClasses(*meta)) {
        add_class(type_name);
    }
    for (const auto& [type_name, desc] : meta->GetEntityTypes()) {
        add_class(type_name.as_str());
        add_class(strex("{}Property", type_name).str());
    }
    for (const BaseTypeDesc& type : meta->GetBaseTypes() | std::views::values) {
        if (type.IsEnum || type.IsStruct || type.IsRefType || type.IsEntity || type.IsFixedType || type.IsEntityProto) {
            add_class(type.Name);
        }
    }

    // Dynamic ref type fields are C# properties the bridge reads and writes one by one
    for (const BaseTypeDesc& type : meta->GetBaseTypes() | std::views::values) {
        if (!IsDynamicManagedRefType(type)) {
            continue;
        }

        auto class_it = classes.find(type.Name);

        if (class_it == classes.end()) {
            continue;
        }

        ptr<const PropertyRegistrar> fields_registrar = type.RefType->FieldsRegistrar;

        for (size_t i = 1; i < fields_registrar->GetPropertiesCount(); i++) {
            auto field_prop = fields_registrar->GetPropertyByIndexUnsafe(i);
            string field_name = MakeManagedDynamicRefTypePropertyName(field_prop);
            MonoProperty* prop = mono_class_get_property_from_name(class_it->second.get_no_const(), field_name.c_str());

            if (prop != nullptr) {
                ManagedDynamicFieldAccessors accessors;
                accessors.Getter = mono_property_get_get_method(prop);
                accessors.Setter = mono_property_get_set_method(prop);
                caches->DynamicFields.emplace(field_prop.get(), accessors);
            }
        }
    }

    caches->Classes = std::move(classes);

    // Event adapters by id, so a subscription looks nothing up
    auto abi = backend->GetAbi();
    FO_VERIFY_AND_THROW(abi, "Managed ABI tables are not built");
    caches->EventAdapters.assign(abi->Events.size(), nullptr);

    for (size_t i = 0; i < abi->Events.size(); i++) {
        const ManagedAbiEventRuntime& event = abi->Events[i];

        if (event.UsesScalarFrame) {
            MonoClass* event_class = FindFOnlineClass(backend, strex("{}{}Event", event.Owner, event.Name).str());
            FO_VERIFY_AND_THROW(event_class != nullptr, "Managed event class not found", event.Owner, event.Name);
            caches->EventAdapters[i] = mono_class_get_method_from_name(event_class, "AdaptInvoke", 6);
            FO_VERIFY_AND_THROW(caches->EventAdapters[i], "Managed event AdaptInvoke adapter is missing", event.Owner, event.Name);
        }
    }
}

// Read-only after the ABI binds; a helper looked up before that, or one outside the table, is resolved on the spot
static auto FindNativeMethod(ptr<const ManagedScriptBackend> backend, const char* method_name, int32_t args_count) -> MonoMethod*
{
    FO_STACK_TRACE_ENTRY();

    auto caches = backend->GetCaches();
    FO_VERIFY_AND_THROW(caches, "Managed backend caches are not created");

    if (auto it = caches->NativeMethods.find(string_view {method_name}); it != caches->NativeMethods.end()) {
        return it->second.get_no_const();
    }

    MonoClass* native_class = FindFOnlineClass(backend, "Native");
    FO_VERIFY_AND_THROW(native_class != nullptr, "Managed Native class not found");
    MonoMethod* method = mono_class_get_method_from_name(native_class, method_name, args_count);
    FO_VERIFY_AND_THROW(method != nullptr, "Managed Native helper method not found", method_name, args_count);
    return method;
}

static auto FindFOnlineClass(ptr<const ManagedScriptBackend> backend, string_view class_name) -> MonoClass*
{
    FO_STACK_TRACE_ENTRY();

    // Read-only once the ABI binds; a class looked up before that, or one the bridge does not name, is found on the spot
    if (auto caches = backend->GetCaches(); caches) {
        if (auto it = caches->Classes.find(class_name); it != caches->Classes.end()) {
            return it->second.get_no_const();
        }
    }

    CountMetadataLookup();

    string class_name_str {class_name};

    for (nptr<void> image_ptr : backend->GetImages()) {
        nptr<MonoImage> image = image_ptr.reinterpret_as<MonoImage>();
        MonoClass* klass = mono_class_from_name(image.get(), "FOnline", class_name_str.c_str());

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
        // the type (e.g. FOnline.ItemBag), so an array element of such a type resolves to that class
        return FindFOnlineClass(backend, type.Name);
    }

    throw ScriptSystemException("Unsupported Managed value type", type.Name);
}

static auto FindFieldInHierarchy(MonoClass* klass, const char* field_name) -> MonoClassField*
{
    FO_NO_STACK_TRACE_ENTRY();

    for (MonoClass* cur_class = klass; cur_class != nullptr; cur_class = mono_class_get_parent(cur_class)) {
        if (MonoClassField* field = mono_class_get_field_from_name(cur_class, field_name); field != nullptr) {
            return field;
        }
    }

    return nullptr;
}

static auto FindEntityTypeDesc(ptr<EngineMetadata> meta, string_view owner_type_name) -> nptr<const EntityTypeDesc>
{
    FO_STACK_TRACE_ENTRY();

    if (meta->IsValidEntityType(owner_type_name)) {
        return &meta->GetEntityType(meta->Hashes.to_hashed_string(owner_type_name));
    }
    if (meta->IsFixedType(owner_type_name)) {
        return &meta->GetFixedType(meta->Hashes.to_hashed_string(owner_type_name));
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

    if (!base_type.IsRefType || !base_type.RefType) {
        return nullptr;
    }

    return base_type.RefType.get();
}

static auto MakeManagedGlobalSimpleType(ptr<EngineMetadata> meta, string_view type_name) -> ComplexTypeDesc
{
    FO_STACK_TRACE_ENTRY();

    // Build a ComplexTypeDesc from an engine type name (mirrors the simple-type and array branches of AngelScript's
    // resolve_type)
    ComplexTypeDesc type;

    // A by-ref argument arrives as "T&". The descriptor already models it: IsMutable is what the metadata uses for
    // an argument the callee writes and the caller reads back
    if (type_name.ends_with("&")) {
        ComplexTypeDesc ref_type = MakeManagedGlobalSimpleType(meta, type_name.substr(0, type_name.size() - 1));

        if (ref_type) {
            ref_type.IsMutable = true;
        }

        return ref_type;
    }

    // Array element: "T[]" -> Array desc wrapping the element base type. The managed registration emits this for
    // List<T> / T[] params and returns (see ScriptFuncRegistration.EngineTypeName)
    if (type_name.ends_with("[]")) {
        string_view elem_name = type_name.substr(0, type_name.size() - 2);

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

    nptr<Entity> entity = entity_ptr ? nptr<Entity>(static_cast<Entity*>(entity_ptr)) : backend->GetGlobalEntity();

    if (!entity) {
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
    memory::copy(&proto_hash, raw_data.data(), sizeof(proto_hash));

    hstring proto_id = backend->GetMetadata()->Hashes.resolve_hash(proto_hash);
    auto proto = backend->GetMetadata()->GetProtoEntity(base_type.HashedName, proto_id);
    return cast_from_void<ProtoEntity*>(proto.void_cast());
}

static auto ExtractProtoHashFromManagedEntity(MonoObject* value) -> hstring::hash_t
{
    FO_STACK_TRACE_ENTRY();

    nptr<Entity> entity = ExtractEntityPtr(value);

    if (!entity) {
        return {};
    }

    nptr<ProtoEntity> proto = entity.dyn_cast<ProtoEntity>();

    if (!proto) {
        throw ScriptSystemException("Managed proto entity wrapper expected", entity->GetName());
    }

    return proto->GetProtoId().as_hash();
}

static void ValidateManagedInnerEntity(ptr<const Entity> entity)
{
    FO_NO_STACK_TRACE_ENTRY();

    entity->ValidateAccess();

    if (entity->IsDestroyed()) {
        throw ScriptSystemException("Access to destroyed inner entity");
    }
}

static auto CollectManagedInnerEntities(ptr<ManagedScriptBackend> backend, ptr<Entity> holder, hstring entry) -> vector<ptr<Entity>>
{
    FO_STACK_TRACE_ENTRY();

    auto entities = holder->GetInnerEntities(entry);
    vector<ptr<Entity>> result;

    if (!entities || entities->empty()) {
        backend->AddInnerEntityVisits(0);
        return result;
    }

    result.reserve(entities->size());

    for (auto& entity : *entities) {
        ValidateManagedInnerEntity(entity.get());
        result.emplace_back(entity.get());
    }

    backend->AddInnerEntityVisits(result.size());
    return result;
}

// === Extraction and hash primitives ===

static auto ExtractEntityPtr(MonoObject* obj) -> Entity*
{
    FO_STACK_TRACE_ENTRY();

    if (obj == nullptr) {
        return nullptr;
    }

    MonoClass* object_class = mono_object_get_class(obj);
    MonoClassField* field = FindFieldInHierarchy(object_class, "_entityPtrValue");

    if (field != nullptr) {
        MonoClassField* backend_field = FindFieldInHierarchy(object_class, "_backend");
        MonoClassField* alive_field = FindFieldInHierarchy(object_class, "_backendAlive");

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

    MonoClassField* field = FindFieldInHierarchy(mono_object_get_class(obj), "_refPtr");

    if (field == nullptr) {
        throw ScriptSystemException("Managed ref type wrapper has no native pointer field");
    }

    void* ref_ptr = nullptr;
    mono_field_get_value(obj, field, &ref_ptr);
    return ref_ptr;
}

static auto ExtractNativeHstring(MonoObject* obj) -> hstring
{
    FO_STACK_TRACE_ENTRY();

    if (obj == nullptr) {
        return {};
    }

    FO_VERIFY_AND_THROW(mono_class_value_size(mono_object_get_class(obj), nullptr) == sizeof(hstring), "Managed hstring size does not match the native one");
    hstring value;
    memory::copy(&value, mono_object_unbox(obj), sizeof(value));
    return value;
}

static auto ResolveManagedHashValue(ptr<const ManagedScriptBackend> backend, hstring::hash_t value) -> hstring
{
    FO_STACK_TRACE_ENTRY();

    if (value == 0) {
        return {};
    }

    bool failed = false;
    hstring backend_value = backend->GetMetadata()->Hashes.resolve_hash(value, &failed);

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

    string assembly_dir = MakeManagedAssemblyResourceDir(target_name);
    vector<ManagedAssemblyResource> result;

    for (const FileHeader& file : resources.FilterFiles("dll", assembly_dir, false)) {
        string resource_path {file.GetPath()};
        string file_name = strex(resource_path).extract_file_name().str();
        auto assembly_file = resources.ReadFile(resource_path);

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

static auto ManagedMemMalloc(size_t size) noexcept -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

    return safe_alloc::malloc_raw(size).get();
}

static auto ManagedMemRealloc(void* mem, size_t size) noexcept -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

    return safe_alloc::realloc_raw(mem, size).get();
}

static void ManagedMemFree(void* mem) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    safe_alloc::free_raw(mem);
}

static auto ManagedMemCalloc(size_t num, size_t size) noexcept -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

    return safe_alloc::calloc_raw(num, size).get();
}

static void ConfigureManagedRuntime(const std::filesystem::path& runtime_dir)
{
    FO_STACK_TRACE_ENTRY();

    // eglib g_malloc (metadata, runtime internals). Must precede every other Mono call, including
    // debug init. SGen and code pages stay on mono_valloc
    MonoAllocatorVTable allocator_vtable {};
    allocator_vtable.version = MONO_ALLOCATOR_VTABLE_VERSION;
    allocator_vtable.malloc = &ManagedMemMalloc;
    allocator_vtable.realloc = &ManagedMemRealloc;
    allocator_vtable.free = &ManagedMemFree;
    allocator_vtable.calloc = &ManagedMemCalloc;
    FO_VERIFY_AND_THROW(mono_set_allocator_vtable(&allocator_vtable) != 0, "Failed to install Managed runtime allocator vtable");

    MonoEglibMemVTable installed_vtable {};
    monoeg_g_mem_get_vtable(&installed_vtable);
    FO_VERIFY_AND_THROW(installed_vtable.malloc == allocator_vtable.malloc, "Managed runtime allocator vtable is not installed", "malloc");
    FO_VERIFY_AND_THROW(installed_vtable.realloc == allocator_vtable.realloc, "Managed runtime allocator vtable is not installed", "realloc");
    FO_VERIFY_AND_THROW(installed_vtable.free == allocator_vtable.free, "Managed runtime allocator vtable is not installed", "free");
    FO_VERIFY_AND_THROW(installed_vtable.calloc == allocator_vtable.calloc, "Managed runtime allocator vtable is not installed", "calloc");

    // The engine host owns process-level crash reporting. Ask embedded Mono to chain the handlers
    // that were already installed by the host (or by a test runner) instead of replacing them
    mono_set_signal_chaining(true);
    mono_set_crash_chaining(true);

    // CoreLib reaches the OS through the interop shims, and the very first managed call already needs
    // one (GlobalizationMode reads its env var through Interop.Sys), so this precedes every other step
    RegisterManagedInteropShims();

    // The shims are linked in, but no ICU package is shipped with the game, so globalization stays
    // invariant by default; callers can still opt into a system ICU through the env var
    SetEnvironmentVariableDefault("DOTNET_SYSTEM_GLOBALIZATION_INVARIANT", "1");

    // Force preemptive GC thread suspension. Under the multithreaded game-logic model the process hosts several
    // in-process engines
#if !FO_WEB
    SetEnvironmentVariableDefault("MONO_THREADS_SUSPEND", "preemptive");
#endif

    // Script frames in stack traces carry file and line only with the portable PDBs embedded in the assemblies loaded,
    // which has to be requested before the domain exists
    mono_debug_init(MONO_DEBUG_FORMAT_MONO);

    auto lib_dir = runtime_dir / "lib";
    auto etc_dir = runtime_dir / "etc";
    auto config_file = etc_dir / "mono" / "config";
    string lib_dir_str = fs::path_to_string(lib_dir);
    string etc_dir_str = fs::path_to_string(etc_dir);
    string config_file_str = fs::path_to_string(config_file);
    string assembly_search_path = BuildAssemblySearchPath(lib_dir);

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

#if FO_WEB
    // WebAssembly has no JIT, so IL runs through the interpreter, and both halves are needed before the
    // domain exists - see Docs/WebDebugging.md, "Managed Runtime On Wasm"
    mono_jit_set_aot_mode(MONO_AOT_MODE_INTERP_ONLY);

    // The runtime is built with DISABLE_ICALL_TABLES, so mono_icall_init skips the table and every icall
    // resolves to a niladic no_icall_table stub - which a four-argument call site cannot invoke on wasm
    mono_icall_table_init();
    mono_wasm_install_interp_to_native_callback(mono_wasm_interp_to_native_callback);
    mono_ee_interp_init("");

    // The interpreter reaches native code through generated marshalling wrappers, so the IL generators
    // have to be installed before the domain exists - mini_init does not do it for this build
    mono_marshal_ilgen_init();
    mono_method_builder_ilgen_init();
    mono_sgen_mono_ilgen_init();
#endif
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
        for (char c : resource.ResourcePath) {
            AddManagedAssemblyCacheByte(hash, static_cast<uint8_t>(c));
        }

        AddManagedAssemblyCacheByte(hash, 0);

        for (uint8_t byte : resource.Data) {
            AddManagedAssemblyCacheByte(hash, byte);
        }

        AddManagedAssemblyCacheByte(hash, 0);
    }

    return strex("{}", hash).str();
}

static auto IsSameManagedAssemblyCacheFile(const std::filesystem::path& disk_path, const_span<uint8_t> assembly_data) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto existing_data = fs::read_file(disk_path.string());

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

static auto RestoreAssemblyResources(const vector<ManagedAssemblyResource>& assembly_resources, string_view cache_dir) -> unordered_map<string, std::filesystem::path>
{
    FO_STACK_TRACE_ENTRY();

    unordered_map<string, std::filesystem::path> restored_paths;

    if (assembly_resources.empty()) {
        return restored_paths;
    }

    // The cache directory is handed in rather than assembled from the working directory: an installed
    // client cannot write into the directory it runs from, which is where the runtime would never load
    auto cache_root = std::filesystem::path {fs::make_path(cache_dir)} / "ManagedAssemblies" / fs::make_path(MakeManagedAssemblyCacheKey(assembly_resources));
    restored_paths.reserve(assembly_resources.size());

    for (const ManagedAssemblyResource& resource : assembly_resources) {
        auto disk_path = cache_root / fs::make_path(resource.ResourcePath);
        string disk_dir = fs::path_to_string(disk_path.parent_path());

        if (!fs::create_directories(disk_dir)) {
            throw ScriptSystemException("Can't create Managed assembly cache directory", disk_dir);
        }
        if (!IsSameManagedAssemblyCacheFile(disk_path, resource.Data)) {
            if (!fs::write_file(disk_path.string(), resource.Data)) {
                throw ScriptSystemException("Can't restore Managed assembly from resources", resource.ResourcePath);
            }
        }

        restored_paths.emplace(resource.ResourcePath, disk_path);
    }

    return restored_paths;
}

// Bake-time only: scan the bake output tree for the managed entry assembly a validation engine needs
static auto CollectBakeOutputAssemblyPaths(string_view bake_output_dir, string_view target_name) -> vector<std::filesystem::path>
{
    FO_STACK_TRACE_ENTRY();

    std::filesystem::path bake_root {bake_output_dir};

    if (!std::filesystem::exists(bake_root)) {
        return {};
    }

    string target_subdir = MakeManagedAssemblyResourceDir(target_name);

    for (auto pack_it = std::filesystem::directory_iterator(bake_root); pack_it != std::filesystem::directory_iterator(); ++pack_it) {
        if (!pack_it->is_directory()) {
            continue;
        }

        auto target_dir = pack_it->path() / fs::make_path(target_subdir);

        if (!std::filesystem::exists(target_dir)) {
            continue;
        }

        vector<std::filesystem::path> result;
        bool has_entry = false;

        for (auto it = std::filesystem::directory_iterator(target_dir); it != std::filesystem::directory_iterator(); ++it) {
            if (!it->is_regular_file() || it->path().extension() != ".dll") {
                continue;
            }

            string file_name = strex("{}", it->path().filename().string()).str();
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

    MonoDomain* mdomain = static_cast<MonoDomain*>(domain);

    if (mdomain == nullptr) {
        throw ScriptSystemException("Managed runtime domain is not initialized");
    }

    return mdomain;
}

static auto MakeManagedPathArray(MonoDomain* domain, const vector<std::filesystem::path>& paths) -> MonoArray*
{
    FO_STACK_TRACE_ENTRY();

    MonoArray* result = mono_array_new(domain, mono_get_string_class(), paths.size());

    if (result == nullptr) {
        throw ScriptSystemException("Can't create Managed assembly path array");
    }

    for (size_t i = 0; i < paths.size(); i++) {
        std::error_code ec;
        auto absolute_path = std::filesystem::absolute(paths[i], ec).lexically_normal();
        string path = fs::path_to_string(ec ? paths[i].lexically_normal() : absolute_path);
        MonoString* managed_path = mono_string_new(domain, path.c_str());

        if (managed_path == nullptr) {
            throw ScriptSystemException("Can't create Managed assembly path string", path);
        }

        mono_array_setref(result, i, managed_path);
    }

    return result;
}

static auto NewManagedGcHandle(MonoObject* obj, mono_bool pinned) -> uint32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    if (InteropThreadCounters.Enabled) {
        InteropThreadCounters.GcHandles++;
    }

    return mono_gchandle_new(obj, pinned);
}

static void CountMetadataLookup() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (InteropThreadCounters.Enabled) {
        InteropThreadCounters.MetadataLookups++;
    }
}

static void CountManagedObject() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (InteropThreadCounters.Enabled) {
        InteropThreadCounters.ManagedObjects++;
    }
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

    MonoString* str = mono_object_to_string(obj, nullptr);

    if (str == nullptr) {
        return {};
    }

    return ToStringAndFree(str);
}

static void ThrowIfManagedException(MonoObject* exception, string_view context, nptr<ManagedScriptEntryScope> entry)
{
    FO_STACK_TRACE_ENTRY();

    if (exception == nullptr) {
        return;
    }

    ManagedExceptionDescription description = DescribeManagedException(exception, entry);

    // A native failure handed to script as an error string, which script did not handle, continues as the native exception
    if (description.NativeException) {
        std::rethrow_exception(description.NativeException);
    }

    stack_trace::data st = stack_trace::get();
    stack_trace::add_unwound_script_frames(st, MakeManagedExceptionLayer(description.Frames));
    throw ScriptException(st, "Managed script exception", description.Summary, context);
}

// ManagedScriptBackend member functions

auto ManagedScriptBackend::CreateLoadScope(const std::filesystem::path& host_assembly_path, const vector<std::filesystem::path>& assembly_paths, const vector<std::filesystem::path>& entry_assembly_paths) -> vector<nptr<void>>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_loadScopeGcHandle == 0, "Managed load scope is already created");
    scoped_lock load_locker {ManagedAssemblyLoadLocker};

    MonoDomain* domain = GetDomainOrThrow(_domain.get());
    string host_path = fs::path_to_string(host_assembly_path);
    MonoAssembly* host_assembly = mono_domain_assembly_open(domain, host_path.c_str());

    if (host_assembly == nullptr) {
        throw ScriptSystemException("Failed to load Managed load-context host assembly", host_path);
    }

    MonoImage* host_image = mono_assembly_get_image(host_assembly);

    if (host_image == nullptr) {
        throw ScriptSystemException("Managed load-context host image is null", host_path);
    }

    MonoClass* host_class = mono_class_from_name(host_image, MANAGED_HOST_NAMESPACE.data(), MANAGED_HOST_CLASS_NAME.data());

    if (host_class == nullptr) {
        throw ScriptSystemException("Managed load-context host class not found");
    }

    MonoMethod* create_method = mono_class_get_method_from_name(host_class, "CreateLoadScope", 3);
    MonoMethod* get_entries_method = mono_class_get_method_from_name(host_class, "GetEntryAssemblies", 1);

    if (create_method == nullptr || get_entries_method == nullptr) {
        throw ScriptSystemException("Managed load-context host methods not found");
    }

    string context_name = strex("FOnline.{}.{}", GetTargetName(_meta->GetSide()), reinterpret_cast<uintptr_t>(this)).str();
    MonoString* managed_context_name = mono_string_new(domain, context_name.c_str());
    MonoArray* managed_assembly_paths = MakeManagedPathArray(domain, assembly_paths);
    MonoArray* managed_entry_paths = MakeManagedPathArray(domain, entry_assembly_paths);

    if (managed_context_name == nullptr) {
        throw ScriptSystemException("Can't create Managed load-context name");
    }

    ActiveBackendScope active_backend {this};
    void* create_args[] = {managed_context_name, managed_assembly_paths, managed_entry_paths};
    MonoObject* exception = nullptr;
    MonoObject* load_scope = mono_runtime_invoke(create_method, nullptr, create_args, &exception);
    ThrowIfManagedException(exception, "Managed load-context creation failed");

    if (load_scope == nullptr) {
        throw ScriptSystemException("Managed load-context host returned a null scope");
    }

    _managedHostImage = host_image;
    _loadScopeGcHandle = NewManagedGcHandle(load_scope, false);

    if (_loadScopeGcHandle == 0) {
        throw ScriptSystemException("Can't root Managed load-context scope");
    }

    void* get_entries_args[] = {load_scope};
    exception = nullptr;
    MonoArray* entry_assembly_objects = reinterpret_cast<MonoArray*>(mono_runtime_invoke(get_entries_method, nullptr, get_entries_args, &exception));
    ThrowIfManagedException(exception, "Managed entry assembly query failed");

    if (entry_assembly_objects == nullptr || mono_array_length(entry_assembly_objects) != entry_assembly_paths.size()) {
        throw ScriptSystemException("Managed load-context returned an invalid entry assembly list");
    }

    vector<nptr<void>> entry_assemblies;
    entry_assemblies.reserve(entry_assembly_paths.size());

    for (size_t i = 0; i < entry_assembly_paths.size(); i++) {
        MonoReflectionAssembly* reflection_assembly = mono_array_get(entry_assembly_objects, MonoReflectionAssembly*, i);
        MonoAssembly* assembly = reflection_assembly != nullptr ? mono_reflection_assembly_get_assembly(reflection_assembly) : nullptr;

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

    uint32_t load_scope_handle = _loadScopeGcHandle;
    _loadScopeGcHandle = 0;

    if (load_scope_handle == 0) {
        _managedHostImage = nullptr;
        return;
    }

    nptr<MonoDomain> domain = _domain.reinterpret_as<MonoDomain>();

    try {
        nptr<MonoImage> host_image = _managedHostImage.reinterpret_as<MonoImage>();

        if (domain && host_image) {
            ManagedThreadAttachment managed_thread {domain};
            MonoObject* load_scope = mono_gchandle_get_target(load_scope_handle);

            if (load_scope == nullptr) {
                throw ScriptSystemException("Managed load-context scope was collected before release");
            }

            MonoClass* host_class = mono_class_from_name(host_image.get(), MANAGED_HOST_NAMESPACE.data(), MANAGED_HOST_CLASS_NAME.data());
            MonoMethod* release_method = host_class != nullptr ? mono_class_get_method_from_name(host_class, "ReleaseLoadScope", 1) : nullptr;

            if (release_method == nullptr) {
                logging::write("Managed load-context release method not found");
            }
            else {
                ActiveBackendScope active_backend {this};
                void* release_args[] = {load_scope};
                MonoObject* exception = nullptr;
                mono_runtime_invoke(release_method, nullptr, release_args, &exception);

                if (exception != nullptr) {
                    logging::write("Managed load-context release failed: {}", ManagedObjectToString(exception));
                }
            }
        }
    }
    catch (const std::exception& ex) {
        logging::write("Managed load-context release failed: {}", ex.what());
    }
    catch (...) {
        logging::write("Managed load-context release failed with an unknown exception");
    }

    ReleaseManagedGcHandle(domain, load_scope_handle);
    _managedHostImage = nullptr;
}

ManagedScriptBackend::ManagedScriptBackend()
{
    FO_STACK_TRACE_ENTRY();

    _caches = safe_alloc::make_unique<ManagedBackendCaches>();
}

auto ManagedScriptBackend::GetCaches() const -> nptr<ManagedBackendCaches>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _caches.get_no_const();
}

ManagedScriptBackend::~ManagedScriptBackend()
{
    FO_STACK_TRACE_ENTRY();

    if (_domain) {
        bool managed_teardown_complete = false;

        safe_call([this, &managed_teardown_complete] {
            MonoDomain* domain = GetDomainOrThrow(_domain.get());
            ManagedThreadAttachment managed_thread {domain};

            safe_call([this] {
                ActiveBackendScope active_backend {this};

                for (nptr<void> shutdown : _continuationShutdowns) {
                    (void)InvokeManagedScript(shutdown.reinterpret_as<MonoMethod>().get(), nullptr, nullptr, "Managed continuation shutdown failed");
                }
            });

            // The engine clears script statics because the script load context is not collectible and they root for the process lifetime;
            // the ones this cannot reach (generic types, thread statics, references inside value-type statics) static analysis forbids
            ClearScriptStatics();

            for (uint32_t gc_handle : _persistentGcHandles) {
                if (gc_handle != 0) {
                    mono_gchandle_free(gc_handle);
                }
            }

            _persistentGcHandles.clear();
            _globalFuncs.clear();

            // Callback handles can root wrappers too; collect after releasing them and before losing the images
            FinalizeManagedObjects();
            ReleaseAliveFlag();

            _images.clear();
            ReleaseLoadScope();
            managed_teardown_complete = true;
        });

        FO_STRONG_ASSERT(managed_teardown_complete, "Managed backend teardown did not complete");

        // Another backend on this thread attaches it again on its next pump
        ManagedFrameWorkerThreadAttachment.Release();
    }

    _continuationPumps.clear();
    _continuationShutdowns.clear();
    _persistentGcHandles.clear();
    _globalFuncs.clear();
    _images.clear();

    // Mono VM state is process-wide. Server/client/mapper backends may coexist
    // in one process, so shutdown is left to process teardown
    _domain = nullptr;
}

void ManagedScriptBackend::Process()
{
    FO_STACK_TRACE_ENTRY();

    if (_continuationPumps.empty()) {
        return;
    }

    ActiveBackendScope active_backend {this};
    MonoDomain* domain = GetDomainOrThrow(_domain.get());
    ManagedThreadAttachment managed_thread {domain, ManagedThreadAttachmentMode::CacheForThread};

    for (nptr<void> pump : _continuationPumps) {
        (void)InvokeManagedScript(pump.reinterpret_as<MonoMethod>().get(), nullptr, nullptr, "Managed continuation pump failed");
    }
}

void ManagedScriptBackend::AdoptPersistentGcHandle(uint32_t gc_handle)
{
    FO_STACK_TRACE_ENTRY();

    _persistentGcHandles.emplace_back(gc_handle);
}

void ManagedScriptBackend::AddManagedGlobalFunc(unique_ptr<ScriptFuncDesc> desc)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(desc->Name, "Script function descriptor has no name");

    _globalFuncs.emplace_back(std::move(desc));

    if (_scriptSys) {
        _scriptSys->AddGlobalScriptFunc(_globalFuncs.back().get());
    }
}

void ManagedScriptBackend::InvokeInitializator(void* assembly, const char* method_name)
{
    FO_STACK_TRACE_ENTRY();

    ActiveBackendScope active_backend {this};

    MonoDomain* domain = GetDomainOrThrow(_domain.get());

    ManagedThreadAttachment managed_thread {domain};

    MonoAssembly* massembly = static_cast<MonoAssembly*>(assembly);
    FO_VERIFY_AND_THROW(massembly, "Managed assembly is null");

    MonoImage* image = mono_assembly_get_image(massembly);

    if (image == nullptr) {
        throw ScriptSystemException("Managed image is null for loaded assembly");
    }

    MonoClass* init_class = mono_class_from_name(image, "FOnline", "Initializator");

    if (init_class == nullptr) {
        return;
    }

    MonoMethod* init_method = mono_class_get_method_from_name(init_class, method_name, 0);

    if (init_method == nullptr) {
        return;
    }

    (void)InvokeManagedScript(init_method, nullptr, nullptr, "Managed initializator failed");
}

void ManagedScriptBackend::RegisterMetadata(ptr<EngineMetadata> meta)
{
    FO_STACK_TRACE_ENTRY();

    _meta = meta;
    // The embedding engine is both metadata and script system, so take the script system here to register managed
    // global funcs into the cross-backend func map
    _scriptSys = meta.dyn_cast<ScriptSystem>();
    BuildAbiTables();
}

void ManagedScriptBackend::BuildAbiTables()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_meta, "Engine metadata is not registered");

    auto state = safe_alloc::make_unique<ManagedAbiRuntimeState>();
    ManagedAbiManifest manifest = BuildManagedAbiManifest(*_meta, GetTargetName(_meta->GetSide()));
    state->Hash = manifest.Hash;
    state->Methods.reserve(manifest.Methods.size());
    state->Events.reserve(manifest.Events.size());
    // The typed cells are atomics, which a growing vector cannot move: size the table once and fill it in place
    state->Settings = vector<ManagedAbiSettingRuntime>(manifest.Settings.size());
    state->Inners.reserve(manifest.InnerEntries.size());

    for (const ManagedAbiMethodEntry& entry : manifest.Methods) {
        ManagedAbiMethodRuntime runtime;
        runtime.Owner = entry.Owner;
        runtime.IsRefType = entry.IsRefType;
        runtime.UsesScalarFrame = entry.UsesScalarFrame;
        runtime.Args = entry.Args;
        runtime.Ret = entry.Ret;
        runtime.FrameSize = entry.FrameSize;
        runtime.ResultOffset = entry.ResultOffset;

        if (entry.IsRefType) {
            auto ref_type = FindRefTypeDesc(_meta, entry.Owner);
            FO_VERIFY_AND_THROW(ref_type && entry.OwnerIndex < ref_type->Methods.size(), "Managed ABI ref-type method is missing", entry.Owner, entry.Name, entry.OwnerIndex);
            runtime.Method = &ref_type->Methods[entry.OwnerIndex];
        }
        else {
            auto entity_desc = FindEntityTypeDesc(_meta, entry.Owner);
            FO_VERIFY_AND_THROW(entity_desc && entry.OwnerIndex < entity_desc->Methods.size(), "Managed ABI method is missing", entry.Owner, entry.Name, entry.OwnerIndex);
            runtime.Method = &entity_desc->Methods[entry.OwnerIndex];
        }

        FO_VERIFY_AND_THROW(runtime.Method->Name == entry.Name, "Managed ABI method name mismatch", entry.Owner, entry.Name, runtime.Method->Name);
        state->Methods.emplace_back(std::move(runtime));
    }

    for (const ManagedAbiEventEntry& entry : manifest.Events) {
        ManagedAbiEventRuntime runtime;
        runtime.Owner = entry.Owner;
        runtime.Name = entry.Name;
        runtime.IsGlobal = entry.IsGlobal;
        runtime.UsesScalarFrame = entry.UsesScalarFrame;
        runtime.Args = entry.Args;
        runtime.FrameSize = entry.FrameSize;
        runtime.Desc = FindEntityTypeDesc(_meta, entry.Owner);
        FO_VERIFY_AND_THROW(runtime.Desc, "Managed ABI event owner is missing", entry.Owner);

        auto event_it = std::ranges::find_if(runtime.Desc->Events, [&](const EntityEventDesc& event) { return event.Name == entry.Name; });
        FO_VERIFY_AND_THROW(event_it != runtime.Desc->Events.end(), "Managed ABI event is missing", entry.Owner, entry.Name);
        runtime.Event = &*event_it;
        state->Events.emplace_back(std::move(runtime));
    }

    for (size_t i = 0; i < manifest.Settings.size(); i++) {
        const ManagedAbiSettingEntry& entry = manifest.Settings[i];
        ManagedAbiSettingRuntime& runtime = state->Settings[i];
        runtime.Name = entry.Name;
        runtime.Kind = entry.Kind;
        runtime.UsesTypedBridge = entry.UsesTypedBridge;
        runtime.Builtin = FindNumericSettingAccess(entry.Name);
    }

    for (const ManagedAbiInnerEntry& entry : manifest.InnerEntries) {
        ManagedAbiInnerRuntime runtime;
        runtime.Owner = entry.Owner;
        runtime.TargetType = entry.TargetType;
        runtime.Entry = _meta->Hashes.to_hashed_string(entry.EntryName);
        state->Inners.emplace_back(std::move(runtime));
    }

    _abi = std::move(state);
}

auto ManagedScriptBackend::GetAbi() const -> nptr<const ManagedAbiRuntimeState>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _abi.get();
}

auto ManagedScriptBackend::GetAbi() -> nptr<ManagedAbiRuntimeState>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _abi.get();
}

void ManagedScriptBackend::AddInnerEntityVisits(uint64_t count)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_abi && _abi->CountInnerEntityVisits.load(std::memory_order_relaxed)) {
        _abi->InnerEntityVisits.fetch_add(count, std::memory_order_relaxed);
    }
}

auto ManagedScriptBackend::GetGlobalEntity() const noexcept -> nptr<Entity>
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetMetadata().dyn_cast<Entity>();
}

void ManagedScriptBackend::LoadAssemblies(const FileSystem& resources, string_view assembly_cache_dir, string_view bake_output_dir)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_meta, "Engine metadata is not registered");
    FO_VERIFY_AND_THROW(_scriptSys, "Script system is not available");

    ManagedThreadAttachmentMode attachment_mode = ManagedThreadAttachmentMode::PreserveExisting;
    auto resource_runtime_dir = RestoreManagedRuntimeResources(resources, assembly_cache_dir);

    if (!_domain) {
        MonoDomain* domain = nullptr;

        {
            scoped_lock runtime_init_locker {ManagedRuntimeInitLocker};
            domain = mono_get_root_domain();

            if (domain == nullptr) {
                auto runtime_dir = resource_runtime_dir.has_value() ? resource_runtime_dir : FindManagedRuntimeDirectory();

                // Fail before Mono turns missing CoreLib into an opaque `corlib' assertion; unpackaged
                // applications retain the side-by-side fallback
                FO_VERIFY_AND_THROW(runtime_dir.has_value(), "Managed runtime directory not found", std::filesystem::current_path().string(), platform::get_exe_path().value_or(""));

                ConfigureManagedRuntime(*runtime_dir);

#if FO_WINDOWS
                // Catch2 owns the top-level SEH filter while a unit-test session is active
                bool preserve_test_exception_filter = IsTestingInProgress;
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

                stack_trace::set_script_provider("Managed", &CollectManagedScriptStackLayers);

#if !FO_WEB
                // mono_jit_init_version attaches its caller; adopt that attachment into this scope so the
                // long-lived engine initialization worker is detached after the first backend is loaded
                attachment_mode = ManagedThreadAttachmentMode::AdoptExisting;
#endif
            }
        }

        _domain = domain;
    }

    MonoDomain* domain = GetDomainOrThrow(_domain.get());

    ManagedThreadAttachment managed_thread {domain, attachment_mode};

    RegisterInternalCalls();

    string_view target_name = GetTargetName(_meta->GetSide());
    CreateAliveFlag();

    auto resource_assemblies = CollectAssemblyResources(resources, target_name);
    auto restored_assembly_paths = RestoreAssemblyResources(resource_assemblies, assembly_cache_dir);

    vector<std::filesystem::path> assembly_paths;
    vector<std::filesystem::path> entry_assembly_paths;
    optional<std::filesystem::path> host_assembly_path;

    auto append_assembly_path = [&](const std::filesystem::path& assembly_path) {
        string file_name = strex("{}", assembly_path.filename().string()).str();

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
        if (auto it = restored_assembly_paths.find(resource.ResourcePath); it != restored_assembly_paths.end()) {
            append_assembly_path(it->second);
        }
    }

    // Bake-time fallback: the assemblies the managed baker just compiled live under the bake output and are not
    // mounted into the data sources yet
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

        vector<nptr<void>> entry_assemblies = CreateLoadScope(*host_assembly_path, assembly_paths, entry_assembly_paths);

        for (const nptr<void>& entry_assembly : entry_assemblies) {
            nptr<MonoAssembly> assembly = entry_assembly.reinterpret_as<MonoAssembly>();

            MonoImage* image = mono_assembly_get_image(assembly.get());

            if (image == nullptr) {
                throw ScriptSystemException("Managed image is null for loaded entry assembly");
            }

            MonoClass* native_class = mono_class_from_name(image, "FOnline", "Native");
            FO_VERIFY_AND_THROW(native_class != nullptr, "Managed Native class not found for continuation pump");
            MonoMethod* pump = mono_class_get_method_from_name(native_class, "PumpContinuations", 0);
            MonoMethod* shutdown = mono_class_get_method_from_name(native_class, "ShutdownContinuations", 0);
            FO_VERIFY_AND_THROW(pump != nullptr && shutdown != nullptr, "Managed continuation scheduler methods not found");

            _continuationPumps.emplace_back(pump);
            _continuationShutdowns.emplace_back(shutdown);
            _images.emplace_back(image);
            InvokeInitializator(assembly.get(), "InitializeEarly");

            auto init_func = safe_alloc::make_unique<ScriptFuncDesc>();
            init_func->Call = [this, assembly](FuncCallData& call) {
                FO_STACK_TRACE_ENTRY();

                ignore_unused(call);
                InvokeInitializator(assembly.get_no_const(), "Initialize");
            };
            unique_del_nptr<ScriptFuncDesc> init_func_desc = make_unique_del_ptr(std::move(init_func).release(), [](ScriptFuncDesc* desc) { delete desc; });
            _scriptSys->AddInitFunc(ScriptFunc<void>(std::move(init_func_desc)), 0);

            loaded_count++;
        }
    }

    if (loaded_count == 0) {
        logging::write("No Managed assemblies found for target '{}', skip", target_name);
    }

    // Armed here rather than asked for from the tracker's static constructor: those run while the initializator
    // walks every type, long before there is an active backend to ask
    EnableDeepEntityWrapperTracking();
}

void ManagedScriptBackend::BindRequiredStuff()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_meta, "Engine metadata is not registered");
}

FO_END_NAMESPACE

#endif
