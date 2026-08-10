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

#pragma once

// Base infrastructure shared by the generated per-role NativeApi modules.
// User code imports exactly one `NativeApi.<Role>` module.

#include "Common.h"

#if FO_NATIVE_SCRIPTING

#include "EngineBase.h"
#include "ScriptSystem.h"

FO_BEGIN_NAMESPACE

class NativeScriptBackend;
class DialogAnswer;
class DialogAnswerReq;
class DialogPack;
class DialogSpeech;
class MovingContext;

FO_END_NAMESPACE

namespace NativeScripts
{
    // The wrapper-side types are plain C++ that lives in this namespace. Generated
    // headers extend it with one class per engine entity (Critter, Item, Player,
    // ...). Each wrapper is a thin object that owns one pointer to the underlying
    // engine entity; passing a wrapper by value is the same cost as passing a
    // pointer.
    //
    // Generated method bodies forward to the engine's existing C++ Export
    // functions (e.g. `Server_Critter_IsMoving`), so a Native script never touches
    // engine internals — only the `///@ ExportMethod` surface.

    // Target-neutral data passed from engine startup to a generated per-role
    // dispatcher. Each NativeApi.<Role> module derives its public
    // ModuleInitContext from this base and adds the role-specific GetGame()
    // result type without duplicating the shared context across named modules.
    //
    // The context is three raw pointers; capture it by value in deferred
    // lambdas (`OnPlayerLogin`, etc.) and the captured copy stays valid for
    // the engine's lifetime — same lifetime contract as `ctx.Engine` itself.
    struct ModuleInitContextBase
    {
        FO_NAMESPACE BaseEngine* Engine;
        FO_NAMESPACE ScriptSystem* ScriptSys;
        FO_NAMESPACE EngineMetadata* Meta;

        // True when this dispatch is firing inside the `BakerLib` context —
        // the `BakerServerEngine` derives from `EngineMetadata` /
        // `ScriptSystem` but NOT from `BaseEngine`, so `Engine` is nullptr
        // here and Game-entity-touching code (`ctx.GetGame()`,
        // `game.SetX(...)`, event subscriptions) crashes. Common-target
        // modules that want runtime behavior must guard with
        // `if (ctx.IsBaker) { return; }` at the top of their init.
        //
        // Baker-only modules live under `NativeScripts/Baker/`, compile into
        // the role-specific `NativeScripts_Baker` library, and run once per
        // MasterBaker session. The synth-emitted dispatcher isolates them from
        // Server/Client/Mapper roles.
        bool IsBaker {};

        // Context-bound shortcut for `NativeScripts::MakeScriptFunc<Ret,
        // Args...>(Engine, name, fn)`. Prefer this over the free function
        // when you already have a `ctx` in scope — the engine handle is
        // passed implicitly, matching the rest of the context API
        // (`ctx.GetGame()`, etc.).
        //
        // Two call shapes:
        //   - Explicit: `ctx.MakeScriptFunc<void, ptr<Entity>>(name, fn)` —
        //     required for non-void return or generic / templated
        //     callables whose `operator()` signature can't be deduced.
        //   - Deduced:  `ctx.MakeScriptFunc(name, fn)` — `Ret` and
        //     `Args...` are pulled from a concrete lambda's
        //     `operator()` signature via `Detail::ScriptFuncSig`.
        template<typename Ret, typename... Args, typename Fn>
        [[nodiscard]] auto MakeScriptFunc(FO_NAMESPACE string_view name, Fn fn) const -> FO_NAMESPACE ScriptFunc<Ret, Args...>;

        template<typename Fn>
        [[nodiscard]] auto MakeScriptFunc(FO_NAMESPACE string_view name, Fn fn) const;

        // Same as `MakeScriptFunc` but additionally registers the
        // produced `ScriptFuncDesc*` into `ScriptSystem::_globalFuncMap`
        // so `.fos` scripts can dispatch into the native callback by
        // name via `Invoke(name, args...)` (or anything else that
        // routes through `engine->FindFunc<...>`). Use for callbacks
        // meant as an AS→native call site; plain `MakeScriptFunc` is
        // enough for inward-only callbacks (TimeEvent targets, predicate
        // probes the native module fires itself).
        template<typename Ret, typename... Args, typename Fn>
        [[nodiscard]] auto MakeGlobalScriptFunc(FO_NAMESPACE string_view name, Fn fn) const -> FO_NAMESPACE ScriptFunc<Ret, Args...>;

        template<typename Fn>
        [[nodiscard]] auto MakeGlobalScriptFunc(FO_NAMESPACE string_view name, Fn fn) const;

        // Context-bound shortcut for `NativeScripts::SendRemoteCall(Engine,
        // name, caller, args...)`. Outbound RemoteCall from native code,
        // wire-format-compatible with AS — see the free function for the
        // full contract.
        template<typename... Args>
        void SendRemoteCall(FO_NAMESPACE string_view name, FO_NAMESPACE ptr<FO_NAMESPACE Entity> caller, const Args&... args) const;

        // Context-bound shortcut for `NativeScripts::BindRemoteCall<Args...>(Engine,
        // name, handler)`. For native-declared inbound calls the binding is
        // required. For AngelScript-declared calls it replaces the AS
        // implementation registered as a fallback. A second native binding
        // remains an invariant failure.
        template<typename... Args, typename Handler>
        void BindRemoteCall(FO_NAMESPACE string_view name, Handler handler) const;

        // Typed `<Name>(caller, args...)` and `Bind_<Name>(handler)` wrappers
        // for every native-declared `///@ RemoteCall <Target> <Name>(args)`
        // tag. LF_NativeScriptSynth emits this file with one member-function
        // pair per RemoteCall — bodies forward to the `SendRemoteCall<Args...>` /
        // `BindRemoteCall<Args...>` member templates above with the type
        // list fixed at synthesis time. The include MUST come after those
        // generic templates so the inline bodies can see them.
        //
        // The file is always written (even empty) so this `#include`
        // succeeds on a clean tree before the user declares any
        // RemoteCalls.
#include "NativeApi_ContextRpcMethods.h"
    };

    using ModuleInitFn = void (*)(const ModuleInitContextBase&);

    namespace Detail
    {
        // Per-role module dispatcher signature. LF_NativeScriptSynth emits one
        // such function per role into `NativeBindings-<Role>.cpp`. The body
        // holds an explicit, deterministic list of exported initializer calls
        // discovered in modules under FO_NATIVE_SCRIPTS_DIR.
        //
        // Engine startup glue declares and invokes the Common dispatcher plus
        // the dispatcher linked for the active target.
        using RegisterModulesFn = void (*)(const ModuleInitContextBase&);
    }

    // Module init contract. A user .cppm declares its init entry as a
    // free function with the signature
    //
    //   export void <Name>(const ::NativeScripts::ModuleInitContext& ctx);
    //
    // LF_NativeScriptSynth pattern-matches this signature in user `.cppm` files under
    // `FO_NATIVE_SCRIPTS_DIR/<Role>/` and wires it into the per-role
    // dispatcher (NativeBindings-<Role>.cpp) by `import`-ing the user
    // module and calling `::<Name>(ctx)` in deterministic module/function order.
    //
    // No macro, no attribute — just the signature. Function names must
    // be unique across the native source tree; synthesis errors on duplicates. The init
    // function is invoked once at engine startup, after metadata
    // registration and before AngelScript modules run.
    //
    // Base for entity wrappers emitted into NativeApi.<Role>. Holds one engine borrow
    // — copy/move is trivial. Generated subclasses add ExportMethod forwarders,
    // ExportProperty Get/Set, and ExportEvent proxies.
    template<typename EngineEntityT>
    class EntityWrapper
    {
    public:
        // Exposes the engine entity type to the event-proxy machinery below.
        // Treating `native_type` as the wrapper's identity lets the proxy detect
        // "this script type is a wrapper for engine type E" without enumerating
        // every concrete wrapper class.
        using native_type = EngineEntityT;

        EntityWrapper() noexcept = default;
        // NOLINTNEXTLINE(google-explicit-constructor): wrappers behave like pointers
        EntityWrapper(FO_NAMESPACE nptr<EngineEntityT> self) noexcept :
            _self {self}
        {
        }

        [[nodiscard]] auto IsValid() const noexcept -> bool { return !!_self; }
        [[nodiscard]] explicit operator bool() const noexcept { return !!_self; }

        [[nodiscard]] auto Native() const noexcept -> FO_NAMESPACE nptr<EngineEntityT> { return _self; }

        friend auto operator==(EntityWrapper a, EntityWrapper b) noexcept -> bool { return a._self == b._self; }
        friend auto operator!=(EntityWrapper a, EntityWrapper b) noexcept -> bool { return a._self != b._self; }

    protected:
        FO_NAMESPACE nptr<EngineEntityT> _self {};
    };

    namespace Detail
    {
        // Forward-dispatch helper for native wrapper method calls. Mirrors
        // AngelScript's `Entity_MethodCall` + `NativeDataCaller::NativeCall`
        // pipeline so server / client / mapper code paths share one
        // wrapper class per entity (e.g. `Critter`, not `Critter` + `CritterView`):
        // engine builds the per-target `MethodDesc::Call` lambda which
        // adapts `FuncCallData` to the target's typed engine function,
        // and the wrapper just packs `(self, args...)` into a `ptr<void>`
        // array and invokes `method->Call(call_data)`.
        //
        // Dispatch a wrapper method through its `MethodDesc::Call` lambda.
        // The lambda — built by codegen via `NativeDataCaller::NativeCall<&Fn>`
        // when the engine registered the method — reads `(self, args...)`
        // out of `FuncCallData::ArgsData` (each slot a `ptr<void>` pointing
        // at the value) and stuffs the return through `RetData`. Wrapper
        // bodies emit a single call to `DispatchMethod` per method —
        // no per-target extern needed, server / client / mapper share
        // one wrapper class because the engine-side `MethodDesc::Call`
        // already knows which typed function to invoke.
        //
        // `Self` is the engine borrow the wrapper holds (`nptr<BaseEngine>`
        // for `Game`, `nptr<Player>` etc. for entity wrappers). `Args...` is
        // the unwrapped argument pack (`string_view`, `int32_t`, …).
        // We pass them by reference so the addresses we put into the
        // argument-slot array stay valid for the duration of the
        // `method->Call(call)` invocation.
        // Pack a wrapper arg into the `ptr<void>` slot the engine's
        // `NATIVE_DATA_ACCESSOR` expects. The engine-side `ConvertArg`
        // path consumes vectors via `ArrayDataProxy`, maps via
        // `DictDataProxy`, ScriptFunc via `GetCallback`, and entities
        // by reading the slot as a typed borrow handle. All other types pass
        // through by address. `NativeDataProvider::NormalizeArg` is
        // the engine-side helper that knows the proxy contract, but
        // it requires complete types for `is_base_of_v<Entity, T>` —
        // user `.cppm` TUs only see forward declarations for engine
        // ref types (`DialogAnswerReq`, etc.), so calling NormalizeArg
        // on every arg fails to compile. We route the special cases
        // explicitly here and fall back to a typed borrow of `arg`
        // for the rest (which covers all primitive types plus the
        // forward-only ref-type pointers).
        template<typename T>
        [[nodiscard]] auto NormalizeWrapperArg(T& arg, FO_NAMESPACE NativeDataProvider::StorageEntryType& storage) -> FO_NAMESPACE ptr<void>
        {
            using raw_t = std::remove_cvref_t<T>;
            if constexpr (FO_NAMESPACE vector_collection<raw_t>) {
                return FO_NAMESPACE make_ptr(&storage.template emplace<FO_NAMESPACE NativeDataProvider::ArrayDataProxy>(arg)).void_cast();
            }
            else if constexpr (FO_NAMESPACE map_collection<raw_t>) {
                return FO_NAMESPACE make_ptr(&storage.template emplace<FO_NAMESPACE NativeDataProvider::DictDataProxy>(arg)).void_cast();
            }
            else if constexpr (FO_NAMESPACE specialization_of<raw_t, FO_NAMESPACE ScriptFunc>) {
                return FO_NAMESPACE make_ptr(&storage.template emplace<FO_NAMESPACE unique_del_nptr<FO_NAMESPACE ScriptFuncDesc>>(arg.ReleaseDesc())).void_cast();
            }
            else if constexpr (std::is_same_v<raw_t, FO_NAMESPACE string_view> || std::is_same_v<raw_t, FO_NAMESPACE string>) {
                // Engine `ConvertArg<string_view>` reads the slot as
                // `string*` and constructs string_view from the
                // dereferenced string. Materialise a real `string` here
                // so the engine sees the expected memory layout —
                // string and string_view have different sizes / fields,
                // so passing `&string_view` directly would feed
                // `*cast_from_void<string*>(data)` UB.
                return FO_NAMESPACE make_ptr(&storage.template emplace<FO_NAMESPACE string>(arg)).void_cast();
            }
            else {
                return FO_NAMESPACE make_ptr(&arg).void_cast();
            }
        }

        // Pack the wrapper's `self` engine borrow into an `nptr<Entity>`
        // handle slot. Engine `Self` types (`ServerEngine`, `Critter`, etc.)
        // are always complete in the consuming TU because the wrapper
        // definitions are gated on `NATIVE_SCRIPTS_TARGET_*` macros
        // that imply the matching engine header was included. So
        // can therefore flow through `NativeDataProvider::NormalizeArg`,
        // unlike forward-declared ref-type args.
        template<typename Self>
        [[nodiscard]] auto NormalizeWrapperSelf(Self self, FO_NAMESPACE NativeDataProvider::StorageEntryType& storage) -> FO_NAMESPACE ptr<void>
        {
            return FO_NAMESPACE NativeDataProvider::NormalizeArg(FO_NAMESPACE nptr<FO_NAMESPACE Entity> {self}, storage);
        }

        template<typename Self, typename... Args>
        void DispatchMethodVoid(FO_NAMESPACE ptr<const FO_NAMESPACE MethodDesc> method, Self self, Args&... args)
        {
            FO_STRONG_ASSERT(method->Call, "Native entity method has no call handler", method->Name);
            constexpr ::size_t arg_count = sizeof...(Args) + 1;
            static_assert(arg_count <= FO_NAMESPACE MAX_CALL_ARGS, "Too many wrapper args");
            FO_NAMESPACE vector<FO_NAMESPACE ptr<void>> args_data;
            args_data.reserve(arg_count);
            FO_NAMESPACE array<FO_NAMESPACE NativeDataProvider::StorageEntryType, arg_count> slot_storage {};
            args_data.emplace_back(NormalizeWrapperSelf(self, slot_storage[0]));
            ::size_t idx = 1;
            (void)std::initializer_list<int> {(args_data.emplace_back(NormalizeWrapperArg(args, slot_storage[idx])), ++idx, 0)...};
            FO_NAMESPACE FuncCallData call {
                .Accessor = &FO_NAMESPACE NativeDataProvider::NATIVE_DATA_ACCESSOR,
                .ArgsData = args_data,
                .RetData = nullptr,
            };
            method->Call(call);
        }

        // RetData void-slot contract mirrors the engine-side
        // `NATIVE_DATA_ACCESSOR`: vectors are read/written through an
        // `ArrayDataProxy*`, maps through a `DictDataProxy*`, scalar /
        // entity / string returns through a raw value slot. Engine-side
        // `ConvertArg` / `ReturnArg` always go through the accessor,
        // so a vector return needs the proxy wrapping the local result
        // before `method->Call(call)` runs. `NormalizeRetSlot` materialises
        // both the storage variant (holding the proxy if needed) and the
        // `ptr<void>` the engine consumes.
        template<typename Ret>
        [[nodiscard]] auto NormalizeRetSlot(Ret& slot, FO_NAMESPACE NativeDataProvider::StorageEntryType& temp) -> FO_NAMESPACE ptr<void>
        {
            if constexpr (FO_NAMESPACE vector_collection<Ret>) {
                return FO_NAMESPACE make_ptr(&temp.template emplace<FO_NAMESPACE NativeDataProvider::ArrayDataProxy>(slot)).void_cast();
            }
            else if constexpr (FO_NAMESPACE map_collection<Ret>) {
                return FO_NAMESPACE make_ptr(&temp.template emplace<FO_NAMESPACE NativeDataProvider::DictDataProxy>(slot)).void_cast();
            }
            else {
                return FO_NAMESPACE make_ptr(&slot).void_cast();
            }
        }

        template<typename Ret, typename Self, typename... Args>
        [[nodiscard]] auto DispatchMethod(FO_NAMESPACE ptr<const FO_NAMESPACE MethodDesc> method, Self self, Args&... args) -> Ret
        {
            FO_STRONG_ASSERT(method->Call, "Native entity method has no call handler", method->Name);
            constexpr ::size_t arg_count = sizeof...(Args) + 1;
            static_assert(arg_count <= FO_NAMESPACE MAX_CALL_ARGS, "Too many wrapper args");
            FO_NAMESPACE vector<FO_NAMESPACE ptr<void>> args_data;
            args_data.reserve(arg_count);
            FO_NAMESPACE array<FO_NAMESPACE NativeDataProvider::StorageEntryType, arg_count> slot_storage {};
            args_data.emplace_back(NormalizeWrapperSelf(self, slot_storage[0]));
            ::size_t idx = 1;
            (void)std::initializer_list<int> {(args_data.emplace_back(NormalizeWrapperArg(args, slot_storage[idx])), ++idx, 0)...};
            Ret ret_slot {};
            FO_NAMESPACE NativeDataProvider::StorageEntryType ret_storage {};
            FO_NAMESPACE ptr<void> ret_data = NormalizeRetSlot(ret_slot, ret_storage);
            FO_NAMESPACE FuncCallData call {
                .Accessor = &FO_NAMESPACE NativeDataProvider::NATIVE_DATA_ACCESSOR,
                .ArgsData = args_data,
                .RetData = ret_data,
            };
            method->Call(call);
            return ret_slot;
        }

        // Method lookup helper. Walks `meta->GetEntityType(entity_name).Methods`
        // for the requested name. Stable pointer — `Methods` is
        // `vector<MethodDesc>` owned by `EntityTypeDesc`, lifetime tied
        // to `EngineMetadata` (which outlives any wrapper).
        [[nodiscard]] inline auto LookupEntityMethod(FO_NAMESPACE ptr<const FO_NAMESPACE EngineMetadata> meta, FO_NAMESPACE string_view entity_name, FO_NAMESPACE string_view method_name) -> FO_NAMESPACE ptr<const FO_NAMESPACE MethodDesc>
        {
            const auto& type_desc = meta->GetEntityType(meta->Hashes.ToHashedString(entity_name));
            for (const auto& m : type_desc.Methods) {
                if (m.Name == method_name) {
                    return &m;
                }
            }
            FO_STRONG_ASSERT(false, "Native entity method is not registered", entity_name, method_name);
            return &type_desc.Methods.front();
        }

        // Concept that detects a NativeScripts entity wrapper. Used by EventProxy
        // to decide which incoming engine borrows to wrap in a script
        // type before invoking the user's handler.
        template<typename T>
        concept IsWrapperClass = requires { typename T::native_type; };

        // Read one event argument out of a FuncCallData slot.
        // For wrapper types we go through the engine `nptr<Entity>` handle that
        // Fire produced via NormalizeArg, then downcast to the wrapper's native_type
        // and construct the wrapper. For everything else we read the value
        // directly from the slot.
        template<typename W>
        [[nodiscard]] auto NativeReadArg(FO_NAMESPACE ptr<void> data) noexcept -> W
        {
            if constexpr (IsWrapperClass<W>) {
                using EngineEnt = typename W::native_type;
                FO_NAMESPACE nptr<FO_NAMESPACE Entity> base = FO_NAMESPACE NativeDataProvider::ReadTypedHandleSlot<FO_NAMESPACE Entity>(data);
                // The engine downcasts via dynamic_cast; matching that keeps us
                // safe if the same callback is reused across mismatched types.
                FO_NAMESPACE nptr<EngineEnt> target = base.template dyn_cast<EngineEnt>();
                return W {target};
            }
            else {
                return *data.template reinterpret_as<W>();
            }
        }

        // Reverse of `NativeReadArg` for the Fire path: unwrap script-side
        // wrappers to their engine borrow before feeding to
        // `NormalizeArg`. The `nptr<Entity>` value from `arg.Native()` is
        // copied into the variant slot inside NormalizeArg, so its
        // lifetime is fine even though the prvalue itself dies at the
        // end of the full expression. Non-wrapper args pass through
        // (NormalizeArg returns `&arg` which is a pointer to the Fire
        // function's parameter — alive for the whole call).
        template<typename T>
        [[nodiscard]] auto NormalizeFireArg(T& arg, FO_NAMESPACE NativeDataProvider::StorageEntryType& storage) -> FO_NAMESPACE ptr<void>
        {
            using bare = std::remove_cvref_t<T>;
            if constexpr (IsWrapperClass<bare>) {
                return FO_NAMESPACE NativeDataProvider::NormalizeArg(arg.Native(), storage);
            }
            else {
                return FO_NAMESPACE NativeDataProvider::NormalizeArg(arg, storage);
            }
        }
    }

    // EventProxy is returned from a wrapper's `OnXxx()` accessor. It mirrors
    // the engine's `EntityEventWrapper` Subscribe shape but adapts arguments
    // so user handlers receive `NativeScripts::Xxx` wrappers instead of
    // engine borrow wrappers.
    //
    // Template parameters:
    //   IsGlobal    — true for events on `Game` (global entity). When true the
    //                 engine packs args starting at ArgsData[0]; when false
    //                 ArgsData[0] is the owning-entity self pointer and the
    //                 event's logical args start at ArgsData[1].
    //   WrapperArgs — the argument types as visible to user handlers. Wrapper
    //                 types (those with a `native_type` typedef) are translated
    //                 from the engine's borrow handle at call time.
    template<bool IsGlobal, typename... WrapperArgs>
    class EventProxy
    {
    public:
        explicit EventProxy(FO_NAMESPACE ptr<FO_NAMESPACE EntityEvent> event) noexcept :
            _event {event}
        {
        }

        template<typename Handler>
        void Subscribe(Handler&& handler, FO_NAMESPACE Entity::EventPriority priority = FO_NAMESPACE Entity::EventPriority::Normal) const
        {
            _event.get_no_const()->Subscribe(MakeCallbackData(std::forward<Handler>(handler), priority, std::index_sequence_for<WrapperArgs...> {}));
        }

        void UnsubscribeAll() const noexcept { _event.get_no_const()->UnsubscribeAll(); }

    private:
        template<typename Handler, size_t... I>
        static auto MakeCallbackData(Handler handler, FO_NAMESPACE Entity::EventPriority priority, std::index_sequence<I...> /*ix*/) -> FO_NAMESPACE Entity::EventCallbackData
        {
            constexpr size_t arg_offset = IsGlobal ? size_t {0} : size_t {1};
            return FO_NAMESPACE Entity::EventCallbackData {
                .Callback = [handler = std::move(handler)](FO_NAMESPACE FuncCallData& call) noexcept -> FO_NAMESPACE Entity::EventResult {
                    handler(Detail::NativeReadArg<WrapperArgs>(call.ArgsData[arg_offset + I])...);
                    return FO_NAMESPACE Entity::EventResult::ContinueChain;
                },
                .SubscriptionPtr = 0,
                .Priority = priority,
                .HasExplicitResult = false,
            };
        }

        FO_NAMESPACE ptr<FO_NAMESPACE EntityEvent> _event;
    };

    // DynamicEventProxy mirrors EventProxy but for events declared via the
    // unified-style `///@ Event <Target> <Entity> <Name>(args)` user tag — those
    // have no `EntityEventWrapper` C++ member on the engine class. Subscribe
    // goes through Entity::SubscribeEvent which addresses dynamic events by
    // string name. The IsGlobal flag still drives the implicit-self arg
    // offset so subscribers see the same layout as static EventProxy.
    //
    // `Fire(args...)` mirrors the engine's `EntityEventWrapper::Fire`
    // pattern (entity-side static counterpart): wrapper args are unwrapped
    // to engine borrows (`Player.Native()`), primitive args pass through
    // by reference, and the assembled `FuncCallData` is dispatched to
    // `Entity::FireEvent(name, call)`. Non-global events prepend `_entity`
    // as the self arg.
    template<bool IsGlobal, typename... WrapperArgs>
    class DynamicEventProxy
    {
    public:
        DynamicEventProxy(FO_NAMESPACE ptr<FO_NAMESPACE Entity> entity, FO_NAMESPACE string_view event_name) noexcept :
            _entity {entity},
            _name {event_name}
        {
        }

        template<typename Handler>
        void Subscribe(Handler&& handler, FO_NAMESPACE Entity::EventPriority priority = FO_NAMESPACE Entity::EventPriority::Normal) const
        {
            _entity.get_no_const()->SubscribeEvent(_name, MakeCallbackData(std::forward<Handler>(handler), priority, std::index_sequence_for<WrapperArgs...> {}));
        }

        void UnsubscribeAll() const noexcept { _entity.get_no_const()->UnsubscribeAllEvent(_name); }

        // Fire the dynamic event with `args`. Wrapper-typed args
        // (`NativeScripts::Player`, etc.) auto-unwrap via `.Native()` so
        // the engine's `NormalizeArg` sees an entity borrow and emplaces it
        // into the temp_storage variant; primitive args pass by
        // reference (NormalizeArg returns `&arg`, safe because `args`
        // are this function's parameters). For non-global events,
        // `_entity` is prepended as the implicit self arg to match the
        // FuncCallData layout subscribers expect.
        auto Fire(WrapperArgs... args) const -> FO_NAMESPACE Entity::EventResult
        {
            if constexpr (IsGlobal) {
                FO_NAMESPACE array<FO_NAMESPACE NativeDataProvider::StorageEntryType, sizeof...(WrapperArgs)> temp_storage {};
                size_t storage_index = 0;
                FO_NAMESPACE array<FO_NAMESPACE ptr<void>, sizeof...(WrapperArgs)> args_data {Detail::NormalizeFireArg(args, temp_storage[storage_index++])...};
                FO_NAMESPACE FuncCallData call {.Accessor = &FO_NAMESPACE NativeDataProvider::NATIVE_DATA_ACCESSOR};
                call.ArgsData = args_data;
                return _entity.get_no_const()->FireEvent(_name, call);
            }
            else {
                FO_NAMESPACE array<FO_NAMESPACE NativeDataProvider::StorageEntryType, sizeof...(WrapperArgs) + 1> temp_storage {};
                size_t storage_index = 1;
                FO_NAMESPACE array<FO_NAMESPACE ptr<void>, sizeof...(WrapperArgs) + 1> args_data {FO_NAMESPACE NativeDataProvider::NormalizeArg(_entity, temp_storage[0]), Detail::NormalizeFireArg(args, temp_storage[storage_index++])...};
                FO_NAMESPACE FuncCallData call {.Accessor = &FO_NAMESPACE NativeDataProvider::NATIVE_DATA_ACCESSOR};
                call.ArgsData = args_data;
                return _entity.get_no_const()->FireEvent(_name, call);
            }
        }

    private:
        template<typename Handler, size_t... I>
        static auto MakeCallbackData(Handler handler, FO_NAMESPACE Entity::EventPriority priority, std::index_sequence<I...> /*ix*/) -> FO_NAMESPACE Entity::EventCallbackData
        {
            constexpr size_t arg_offset = IsGlobal ? size_t {0} : size_t {1};
            return FO_NAMESPACE Entity::EventCallbackData {
                .Callback = [handler = std::move(handler)](FO_NAMESPACE FuncCallData& call) noexcept -> FO_NAMESPACE Entity::EventResult {
                    handler(Detail::NativeReadArg<WrapperArgs>(call.ArgsData[arg_offset + I])...);
                    return FO_NAMESPACE Entity::EventResult::ContinueChain;
                },
                .SubscriptionPtr = 0,
                .Priority = priority,
                .HasExplicitResult = false,
            };
        }

        FO_NAMESPACE ptr<FO_NAMESPACE Entity> _entity;
        FO_NAMESPACE string_view _name;
    };

    namespace Detail
    {
        // Extracts one positional argument from a `FuncCallData` slot for
        // the native-side ScriptFunc factory. `NormalizeArg` writes args
        // by address: for an entity borrow `args_data[i]` points to a
        // variant slot holding the handle; for primitives/structs it
        // points to caller storage. Either way the slot is a T-storage,
        // so a typed `reinterpret_as<T>()` recovers the value.
        //
        // For `vector<T>` args, `NormalizeArg` constructs an
        // `ArrayDataProxy` in the variant slot (not a raw `vector*`),
        // so we must iterate via `proxy->Size()` / `proxy->Get(i)` and
        // rebuild the vector element-by-element. Each element pointer
        // returned by `Get(i)` follows the same typed-slot
        // layout the scalar branch expects.
        template<typename T>
        [[nodiscard]] auto NativeReadFuncArg(FO_NAMESPACE ptr<void> data) -> T
        {
            using bare = std::remove_cvref_t<T>;
            if constexpr (FO_NAMESPACE vector_collection<bare>) {
                FO_NAMESPACE ptr<FO_NAMESPACE NativeDataProvider::ArrayDataProxy> proxy = data.template reinterpret_as<FO_NAMESPACE NativeDataProvider::ArrayDataProxy>();
                bare result;
                const size_t count = proxy->Size();
                result.reserve(count);
                for (size_t i = 0; i < count; ++i) {
                    result.push_back(NativeReadFuncArg<typename bare::value_type>(proxy->Get(i)));
                }
                return result;
            }
            else if constexpr (FO_NAMESPACE map_collection<bare>) {
                // `DictDataProxy::Get(i)` returns `pair<void*, void*>` —
                // the key and value pointers — so the iteration is one
                // step lighter than vector (no intermediate variant
                // unwrap). Each pointer follows the same per-T layout
                // the scalar typed-slot extraction expects, so
                // recursing into `NativeReadFuncArg<K>` /
                // `NativeReadFuncArg<V>` works.
                FO_NAMESPACE ptr<FO_NAMESPACE NativeDataProvider::DictDataProxy> proxy = data.template reinterpret_as<FO_NAMESPACE NativeDataProvider::DictDataProxy>();
                bare result;
                const size_t count = proxy->Size();
                for (size_t i = 0; i < count; ++i) {
                    auto [k_ptr, v_ptr] = proxy->Get(i);
                    auto key = NativeReadFuncArg<typename bare::key_type>(k_ptr);
                    auto val = NativeReadFuncArg<typename bare::mapped_type>(v_ptr);
                    result.emplace(std::move(key), std::move(val));
                }
                return result;
            }
            else if constexpr (FO_NAMESPACE is_borrow_pointer_wrapper_v<bare>) {
                using element_type = typename bare::element_type;
                return FO_NAMESPACE NativeDataProvider::ReadTypedHandleSlot<element_type>(data);
            }
            else {
                return *data.template reinterpret_as<bare>();
            }
        }

        template<typename Fn, typename... Args, size_t... I>
        auto NativeInvokeFn(const Fn& fn, FO_NAMESPACE FuncCallData& call, std::index_sequence<I...> /*ix*/)
        {
            return fn(NativeReadFuncArg<Args>(call.ArgsData[I])...);
        }

        // Resolves the per-engine `NativeScriptBackend` and emplaces a fresh
        // `ScriptFuncDesc`. Implemented in NativeScriptCore.cpp so the
        // user-facing template body in `MakeScriptFunc` doesn't need
        // `NativeScriptBackend` to be a complete type (NativeScriptCore.h only
        // forward-declares it to avoid a `NativeScriptBackend.h <-> NativeScriptCore.h`
        // include cycle).
        [[nodiscard]] auto AllocateNativeScriptFunc(FO_NAMESPACE ptr<FO_NAMESPACE BaseEngine> engine) -> FO_NAMESPACE ScriptFuncDesc&;
    }

    // Build a `fo::ScriptFunc<Ret, Args...>` whose `Call` invokes a native
    // C++ callable. Use this when an engine wrapper expects a `ScriptFunc`
    // parameter — `Game.StartTimeEvent(delay, func)` is the canonical case.
    //
    // Template arguments are explicit so codegen wrappers (which take
    // `ScriptFunc<...>` by value, move-only) get the exact instantiation
    // they need. The callable's signature must match `Ret(Args...)`; a
    // mismatch is a static_assert through the wrapping `fo::function`.
    //
    // Lifetime: the underlying `ScriptFuncDesc` lives in `NativeScriptBackend`'s
    // per-engine deque for the lifetime of the engine. `ScriptFunc<...>`
    // holds a non-owning pointer with a no-op deleter — moving the result
    // into a `StartTimeEvent` slot is safe across the time event's full
    // wait + fire window, and across reuse if the user copies the
    // `ScriptFuncName` and constructs another lookup.
    //
    // Limitations (v1):
    //   - Arg types must be those `NormalizeArg` knows: primitives,
    //     strings, `hstring`, `ptr<Entity>` / `nptr<Entity>` (and derived
    //     script-entity borrows), `any_t`. Vectors / dicts (`ArrayDataProxy`,
    //     `DictDataProxy`) round-trip on the arg side via
    //     `NativeReadFuncArg`'s vector/map branches.
    //   - `Ret` may be void, a primitive / string / hstring / entity borrow,
    //     or `vector<T>` / `map<K, V>` of those — for container returns
    //     the writeback goes through `call.Accessor->ClearArray` /
    //     `AddArrayElement` (or the dict equivalents) so the caller's
    //     `ArrayDataProxy` / `DictDataProxy` reflects the result. Other
    //     return shapes (ref-types, structs) aren't bridged.
    //   - TimeEvents are in-memory only in this engine. Persistent
    //     ScriptFuncType properties store a function name instead of a
    //     callback object; use `MakeGlobalScriptFunc` with a stable name so
    //     the regular `FindFunc` path can resolve it after startup.
    //
    // Name: becomes `ScriptFuncDesc::Name` (hashed). Engine APIs that
    // identify the function by name (`StopTimeEvent(func)`,
    // `CountTimeEvent(func)`) match by this string — pick a unique one
    // per native module if you intend to look it up later.
    // Internal helper — implements both `MakeScriptFunc` and
    // `MakeGlobalScriptFunc`. The `register_globally` flag adds the
    // freshly-allocated desc to `ScriptSystem::_globalFuncMap` so the
    // engine's `FindFunc<...>(name)` (and AS-callable
    // `Invoke(name, ...)`) can reach it by name. Sibling
    // public templates select the flag at the call site so the user
    // gets clear opt-in semantics rather than a "did you remember to
    // register?" question.
    namespace Detail
    {
        template<typename Ret, typename... Args, typename Fn>
        auto MakeScriptFuncImpl(FO_NAMESPACE ptr<FO_NAMESPACE BaseEngine> engine, FO_NAMESPACE string_view name, Fn fn, bool register_globally) -> FO_NAMESPACE ScriptFunc<Ret, Args...>;
    }

    template<typename Ret, typename... Args, typename Fn>
    [[nodiscard]] auto MakeScriptFunc(FO_NAMESPACE ptr<FO_NAMESPACE BaseEngine> engine, FO_NAMESPACE string_view name, Fn fn) -> FO_NAMESPACE ScriptFunc<Ret, Args...>
    {
        return Detail::MakeScriptFuncImpl<Ret, Args...>(engine, name, std::move(fn),
            /*register_globally=*/false);
    }

    // Like `MakeScriptFunc` but additionally registers the underlying
    // `ScriptFuncDesc` into `ScriptSystem::_globalFuncMap` so the
    // engine and AS scripts can resolve it by name. Pair with the
    // engine `Invoke(funcName, ...)` global function (which calls
    // `engine->FindFunc<void, any_t...>(funcName)`) to let `.fos`
    // scripts reach native callbacks by string lookup. Use this when
    // a native module wants AS to be able to dispatch into it by name;
    // for callbacks that only the native module itself fires (e.g.
    // local TimeEvents), plain `MakeScriptFunc` is enough and skips
    // the map insertion.
    //
    // The desc lives in `NativeScriptBackend::_scriptFuncs` (per-engine deque
    // with stable addresses) for the engine lifetime — same lifetime
    // contract as the multimap entry's `ScriptFuncDesc*`. The desc is
    // never removed from the map on engine shutdown; the entire
    // `ScriptSystem` is destroyed together so the pointer never
    // outlives its slot.
    //
    // Naming: each call inserts one multimap entry. If you call
    // `MakeGlobalScriptFunc` with the same name twice, both entries
    // coexist and `FindFunc` returns the first matching signature.
    // Prefer stable, unique names per native module to avoid surprise
    // dispatch.
    template<typename Ret, typename... Args, typename Fn>
    [[nodiscard]] auto MakeGlobalScriptFunc(FO_NAMESPACE ptr<FO_NAMESPACE BaseEngine> engine, FO_NAMESPACE string_view name, Fn fn) -> FO_NAMESPACE ScriptFunc<Ret, Args...>
    {
        return Detail::MakeScriptFuncImpl<Ret, Args...>(engine, name, std::move(fn),
            /*register_globally=*/true);
    }

    namespace Detail
    {
        template<typename Ret, typename... Args, typename Fn>
        auto MakeScriptFuncImpl(FO_NAMESPACE ptr<FO_NAMESPACE BaseEngine> engine, FO_NAMESPACE string_view name, Fn fn, bool register_globally) -> FO_NAMESPACE ScriptFunc<Ret, Args...>
        {
            FO_NAMESPACE function<Ret(Args...)> wrapped {std::move(fn)};
            auto& desc = Detail::AllocateNativeScriptFunc(engine);
            desc.Name = engine->Hashes.ToHashedString(name);

            // Populate `Args` / `Ret` from the engine's type registry when
            // we're registering the desc into the global func map — without
            // matching type info, `ScriptSystem::ValidateArgs` (used by
            // `FindFunc<TRet, Args...>` and therefore by AS's
            // `Invoke(name, ...)`) would reject the desc on signature
            // mismatch. Non-global `MakeScriptFunc` calls keep the desc's
            // `Args` / `Ret` empty — those are used only via the typed
            // `ScriptFunc<>` handle returned here (TimeEvent target,
            // predicate gag-callback, etc.) where signature matching isn't
            // mediated by the global map.
            //
            // The fold pushes `desc.Args` left-to-right preserving the
            // template parameter order. If any `Args` C++ type isn't a
            // mapped script type (e.g. a user struct not registered via
            // `MapEngineType`), `GetEngineType<Args>()` returns nullptr —
            // assert so the failure surfaces immediately rather than
            // turning into a silent `FindFunc` miss later.
            if (register_globally) {
                if constexpr (sizeof...(Args) > 0) {
                    (([&] {
                        FO_NAMESPACE nptr<const FO_NAMESPACE ComplexTypeDesc> type = engine->template GetEngineType<Args>();
                        FO_STRONG_ASSERT(type, "Native script argument type is not registered");
                        desc.Args.emplace_back(FO_NAMESPACE ArgDesc {.Name = {}, .Type = *type});
                    }()),
                        ...);
                }
                if constexpr (!std::is_same_v<Ret, void>) {
                    FO_NAMESPACE nptr<const FO_NAMESPACE ComplexTypeDesc> ret_type = engine->template GetEngineType<Ret>();
                    FO_STRONG_ASSERT(ret_type, "Native script return type is not registered");
                    desc.Ret = *ret_type;
                }
            }
            desc.AttributeChecker = [](FO_NAMESPACE string_view) noexcept { return false; };
            desc.Call = [callable = std::move(wrapped)](FO_NAMESPACE FuncCallData& call) {
                if constexpr (std::is_same_v<Ret, void>) {
                    Detail::NativeInvokeFn<decltype(callable), Args...>(callable, call, std::index_sequence_for<Args...> {});
                }
                else {
                    // For non-void return, `ScriptFunc<Ret, Args...>::Call`
                    // sets `call.RetData` to a pointer-to-Ret slot owned by
                    // the caller's `NativeDataProvider::StorageEntryType`
                    // variant. Three shapes:
                    //   - vector return → RetData points to an
                    //     `ArrayDataProxy` wrapping the caller's `_ret`
                    //     vector. Use the accessor's `ClearArray` +
                    //     `AddArrayElement` so the proxy's _addCallback
                    //     mutates the underlying container.
                    //   - map return → same with `DictDataProxy`.
                    //   - everything else (primitives, strings, hstring,
                    //     entity borrows, `any_t`) → RetData points directly at
                    //     the `_ret` storage, so `*static_cast<Ret*>` =
                    //     assignment works.
                    auto result = Detail::NativeInvokeFn<decltype(callable), Args...>(callable, call, std::index_sequence_for<Args...> {});
                    FO_STRONG_ASSERT(call.RetData, "Native script call has no return value storage");
                    if constexpr (FO_NAMESPACE vector_collection<Ret>) {
                        call.Accessor->ClearArray(call.RetData);
                        for (auto& element : result) {
                            // Accessor's Add stores a copy via the proxy's
                            // _addCallback (T::value_type emplace_back); the
                            // `&element` here just feeds the address into the
                            // copy site. Mutable ref is fine — `result` is
                            // about to be destroyed anyway.
                            call.Accessor->AddArrayElement(call.RetData, &element);
                        }
                    }
                    else if constexpr (FO_NAMESPACE map_collection<Ret>) {
                        call.Accessor->ClearDict(call.RetData);
                        for (auto& kv : result) {
                            // map iteration yields `pair<const K, V>` — the
                            // const on the key matches what the proxy's
                            // _addCallback reads (`const key_type*`).
                            call.Accessor->AddDictElement(call.RetData, const_cast<typename Ret::key_type*>(&kv.first), &kv.second);
                        }
                    }
                    else {
                        *call.RetData.template reinterpret_as<Ret>() = std::move(result);
                    }
                }
            };

            if (register_globally) {
                // `engine` IS the `ScriptSystem` (BaseEngine inherits from it
                // — see the static_cast in NativeScriptCore.cpp's
                // `AllocateNativeScriptFunc`). Inserts a stable
                // `ScriptFuncDesc*` into `_globalFuncMap` keyed by
                // `desc.Name`. The desc lives in the NativeScriptBackend's deque
                // (stable address) for the engine's lifetime.
                engine->AddGlobalScriptFunc(&desc);
            }

            return FO_NAMESPACE ScriptFunc<Ret, Args...> {&desc};
        }
    } // namespace Detail

    template<typename Ret, typename... Args, typename Fn>
    auto ModuleInitContextBase::MakeScriptFunc(FO_NAMESPACE string_view name, Fn fn) const -> FO_NAMESPACE ScriptFunc<Ret, Args...>
    {
        return NativeScripts::MakeScriptFunc<Ret, Args...>(Engine, name, std::move(fn));
    }

    namespace Detail
    {
        // Tag-dispatch helper for `ctx.MakeScriptFunc(name, fn)` signature
        // deduction. Indexed by the lambda's `operator()` member-function-
        // pointer type; specializations cover the four cv / exception
        // qualifier shapes non-generic lambdas can produce. Generic
        // lambdas (templated `operator()`) and overloaded callables don't
        // match — those require the explicit `MakeScriptFunc<Ret, Args...>`
        // form.
        template<typename T>
        struct ScriptFuncSig;

        template<typename C, typename Ret, typename... Args>
        struct ScriptFuncSig<Ret (C::*)(Args...) const>
        {
            template<typename Ctx, typename Fn>
            static auto Make(const Ctx& ctx, FO_NAMESPACE string_view name, Fn fn) -> FO_NAMESPACE ScriptFunc<Ret, Args...>
            {
                return ctx.template MakeScriptFunc<Ret, Args...>(name, std::move(fn));
            }
        };

        template<typename C, typename Ret, typename... Args>
        struct ScriptFuncSig<Ret (C::*)(Args...)>
        {
            template<typename Ctx, typename Fn>
            static auto Make(const Ctx& ctx, FO_NAMESPACE string_view name, Fn fn) -> FO_NAMESPACE ScriptFunc<Ret, Args...>
            {
                return ctx.template MakeScriptFunc<Ret, Args...>(name, std::move(fn));
            }
        };

        template<typename C, typename Ret, typename... Args>
        struct ScriptFuncSig<Ret (C::*)(Args...) const noexcept>
        {
            template<typename Ctx, typename Fn>
            static auto Make(const Ctx& ctx, FO_NAMESPACE string_view name, Fn fn) -> FO_NAMESPACE ScriptFunc<Ret, Args...>
            {
                return ctx.template MakeScriptFunc<Ret, Args...>(name, std::move(fn));
            }
        };

        template<typename C, typename Ret, typename... Args>
        struct ScriptFuncSig<Ret (C::*)(Args...) noexcept>
        {
            template<typename Ctx, typename Fn>
            static auto Make(const Ctx& ctx, FO_NAMESPACE string_view name, Fn fn) -> FO_NAMESPACE ScriptFunc<Ret, Args...>
            {
                return ctx.template MakeScriptFunc<Ret, Args...>(name, std::move(fn));
            }
        };
    }

    template<typename Fn>
    auto ModuleInitContextBase::MakeScriptFunc(FO_NAMESPACE string_view name, Fn fn) const
    {
        return Detail::ScriptFuncSig<decltype(&Fn::operator())>::Make(*this, name, std::move(fn));
    }

    template<typename Ret, typename... Args, typename Fn>
    auto ModuleInitContextBase::MakeGlobalScriptFunc(FO_NAMESPACE string_view name, Fn fn) const -> FO_NAMESPACE ScriptFunc<Ret, Args...>
    {
        return NativeScripts::MakeGlobalScriptFunc<Ret, Args...>(Engine, name, std::move(fn));
    }

    namespace Detail
    {
        // Deduces `Ret`/`Args...` for `ctx.MakeGlobalScriptFunc(name, fn)`
        // exactly like `ScriptFuncSig` does for `MakeScriptFunc` — the
        // signature is read off the lambda's `operator()` and forwarded to
        // the explicit-template form. Same four cv / exception qualifier
        // shapes a non-generic lambda can have.
        template<typename T>
        struct GlobalScriptFuncSig;

        template<typename C, typename Ret, typename... Args>
        struct GlobalScriptFuncSig<Ret (C::*)(Args...) const>
        {
            template<typename Ctx, typename Fn>
            static auto Make(const Ctx& ctx, FO_NAMESPACE string_view name, Fn fn) -> FO_NAMESPACE ScriptFunc<Ret, Args...>
            {
                return ctx.template MakeGlobalScriptFunc<Ret, Args...>(name, std::move(fn));
            }
        };

        template<typename C, typename Ret, typename... Args>
        struct GlobalScriptFuncSig<Ret (C::*)(Args...)>
        {
            template<typename Ctx, typename Fn>
            static auto Make(const Ctx& ctx, FO_NAMESPACE string_view name, Fn fn) -> FO_NAMESPACE ScriptFunc<Ret, Args...>
            {
                return ctx.template MakeGlobalScriptFunc<Ret, Args...>(name, std::move(fn));
            }
        };

        template<typename C, typename Ret, typename... Args>
        struct GlobalScriptFuncSig<Ret (C::*)(Args...) const noexcept>
        {
            template<typename Ctx, typename Fn>
            static auto Make(const Ctx& ctx, FO_NAMESPACE string_view name, Fn fn) -> FO_NAMESPACE ScriptFunc<Ret, Args...>
            {
                return ctx.template MakeGlobalScriptFunc<Ret, Args...>(name, std::move(fn));
            }
        };

        template<typename C, typename Ret, typename... Args>
        struct GlobalScriptFuncSig<Ret (C::*)(Args...) noexcept>
        {
            template<typename Ctx, typename Fn>
            static auto Make(const Ctx& ctx, FO_NAMESPACE string_view name, Fn fn) -> FO_NAMESPACE ScriptFunc<Ret, Args...>
            {
                return ctx.template MakeGlobalScriptFunc<Ret, Args...>(name, std::move(fn));
            }
        };
    }

    template<typename Fn>
    auto ModuleInitContextBase::MakeGlobalScriptFunc(FO_NAMESPACE string_view name, Fn fn) const
    {
        return Detail::GlobalScriptFuncSig<decltype(&Fn::operator())>::Make(*this, name, std::move(fn));
    }

    namespace Detail
    {
        // Serializes one `SendRemoteCall` argument using the exact wire
        // format AS uses in `AngelScriptRemoteCalls.cpp::OutboundRemoteCallFunc`
        // so AS-bound inbound handlers can decode native-originated calls.
        //
        // - arithmetic types (bool, int*, uint*, float*) → raw bytes via
        //   `writer.Write<T>(value)`.
        // - `string` / `string_view` → `int32_t` length + raw bytes (no
        //   null terminator).
        // - `hstring` → `hstring::hash_t` (uint64) as raw bytes.
        //
        // Refused at compile time for anything else — vectors, dicts, ref
        // types, structs go through more elaborate encodings on the AS
        // side and would need matching native support.
        template<typename T>
        void WriteRemoteCallArg(FO_NAMESPACE DataWriter& writer, const T& arg)
        {
            using bare = std::remove_cvref_t<T>;
            if constexpr (std::is_arithmetic_v<bare>) {
                writer.template Write<bare>(arg);
            }
            else if constexpr (std::is_same_v<bare, FO_NAMESPACE hstring>) {
                writer.template Write<FO_NAMESPACE hstring::hash_t>(arg.as_hash());
            }
            else if constexpr (FO_NAMESPACE vector_collection<bare>) {
                // AS wire format for array args: int32_t count followed by
                // count * element-encoded-bytes. Recurse element-wise so
                // nested encodings (string elements, hstring elements) use
                // the same per-type rules.
                writer.template Write<int32_t>(FO_NAMESPACE numeric_cast<int32_t>(arg.size()));
                for (const auto& elem : arg) {
                    WriteRemoteCallArg(writer, elem);
                }
            }
            else if constexpr (FO_NAMESPACE map_collection<bare>) {
                // AS wire format for dict args: int32_t count followed by
                // count * (key + value) encoded pairs. Same recursion as
                // vector; key + value types follow the regular per-type
                // encoding rules.
                writer.template Write<int32_t>(FO_NAMESPACE numeric_cast<int32_t>(arg.size()));
                for (const auto& kv : arg) {
                    WriteRemoteCallArg(writer, kv.first);
                    WriteRemoteCallArg(writer, kv.second);
                }
            }
            else if constexpr (std::is_constructible_v<FO_NAMESPACE string_view, const bare&>) {
                // Catches `string`, `string_view`, `const char*`, and
                // string literals (`char[N]`). All encode as `int32_t
                // length + bytes`, matching AS's wire format for the
                // `string` arg type.
                FO_NAMESPACE string_view sv {arg};
                writer.template Write<int32_t>(FO_NAMESPACE numeric_cast<int32_t>(sv.length()));
                writer.WriteStringBytes(sv);
            }
            else {
                static_assert(!std::is_same_v<bare, bare>,
                    "SendRemoteCall: unsupported argument type "
                    "(primitives, vector<T>, string-like, and hstring are bridged)");
            }
        }

        // Reverse of `WriteRemoteCallArg`. Used by the synth-emitted
        // `ModuleInitContext::Bind_<Name>` wrapper to deserialize incoming
        // packet bytes back into the user handler's arg types. Mirrors
        // AS's `AngelScriptRemoteCalls.cpp::InboundRemoteCallHandler` so
        // packets fired by AS-side `Player.<Side>Call.<Name>(args...)`
        // decode identically — same byte layout, same hstring hash
        // resolution path.
        template<typename T>
        [[nodiscard]] auto ReadRemoteCallArg(FO_NAMESPACE DataReader& reader, FO_NAMESPACE ptr<FO_NAMESPACE BaseEngine> engine) -> T
        {
            using bare = std::remove_cvref_t<T>;
            if constexpr (std::is_arithmetic_v<bare>) {
                return reader.template Read<bare>();
            }
            else if constexpr (std::is_same_v<bare, FO_NAMESPACE hstring>) {
                const auto hash = reader.template Read<FO_NAMESPACE hstring::hash_t>();
                return engine->Hashes.ResolveHash(hash);
            }
            else if constexpr (std::is_same_v<bare, FO_NAMESPACE string>) {
                const auto len = reader.template Read<int32_t>();
                FO_STRONG_ASSERT(len >= 0, "Remote call string length is negative", len);
                const size_t size = FO_NAMESPACE numeric_cast<size_t>(len);
                if (size == 0) {
                    return {};
                }
                const FO_NAMESPACE nptr<const char> bytes = reader.template ReadPtr<char>(size);
                return FO_NAMESPACE string {bytes.get(), size};
            }
            else if constexpr (FO_NAMESPACE vector_collection<bare>) {
                // AS wire format for array args: int32_t count followed by
                // count * element-encoded-bytes. Mirror `WriteRemoteCallArg`'s
                // element-wise recursion to decode.
                const auto count = reader.template Read<int32_t>();
                FO_STRONG_ASSERT(count >= 0, "Remote call array length is negative", count);
                bare result;
                result.reserve(FO_NAMESPACE numeric_cast<size_t>(count));
                for (int32_t i = 0; i < count; ++i) {
                    result.push_back(ReadRemoteCallArg<typename bare::value_type>(reader, engine));
                }
                return result;
            }
            else if constexpr (FO_NAMESPACE map_collection<bare>) {
                // AS wire format for dict args: int32_t count followed by
                // count * (key + value) encoded pairs. Mirror
                // `WriteRemoteCallArg`'s element-wise recursion.
                const auto count = reader.template Read<int32_t>();
                FO_STRONG_ASSERT(count >= 0, "Remote call dictionary length is negative", count);
                bare result;
                for (int32_t i = 0; i < count; ++i) {
                    auto key = ReadRemoteCallArg<typename bare::key_type>(reader, engine);
                    auto val = ReadRemoteCallArg<typename bare::mapped_type>(reader, engine);
                    result.emplace(std::move(key), std::move(val));
                }
                return result;
            }
            else {
                static_assert(!std::is_same_v<bare, bare>,
                    "ReadRemoteCallArg: unsupported argument type "
                    "(primitives, vector<T>, map<K,V>, string, and hstring are bridged)");
            }
        }

        template<typename... Args>
        [[nodiscard]] auto ReadRemoteCallArgs(FO_NAMESPACE DataReader& reader, FO_NAMESPACE ptr<FO_NAMESPACE BaseEngine> engine) -> std::tuple<Args...>
        {
            std::tuple<Args...> args;
            [&]<size_t... Indexes>(std::index_sequence<Indexes...>) {
                // A comma fold guarantees packet-order reads. Passing the
                // reads directly to std::tuple's constructor does not because
                // function argument evaluation order is unspecified.
                ((std::get<Indexes>(args) = ReadRemoteCallArg<std::tuple_element_t<Indexes, std::tuple<Args...>>>(reader, engine)), ...);
            }(std::index_sequence_for<Args...> {});
            return args;
        }
    }

    // Outbound RemoteCall from native code, using the AS-compatible wire
    // format. Calls `engine->SendRemoteCall(name, caller, data)` with the
    // args serialized exactly the way AS's
    // `AngelScriptRemoteCalls.cpp::OutboundRemoteCallFunc` writes them — so
    // an inbound handler on the receiving side, declared in `.fos` via
    // `///@ RemoteCall <Target> <Name>(args)` + `[[<Target>RemoteCall]]
    // void Namespace::Name(...)`, decodes the bytes identically to a call
    // originating from AS.
    //
    // Metadata may come from AngelScript or from a native module. For an
    // AS-declared call the script handler remains the inbound fallback until
    // native initialization claims it through `BindRemoteCall`; native-only
    // calls use the generated typed binder below.
    //
    // Caller entity: matches AS's contract. On the server, `caller` must
    // be a `ptr<Player>` or `ptr<Critter>` whose player owns the connection
    // (engine routes the packet to that player's socket); on the client,
    // it must be the `ptr<Player>` representing the local client.
    template<typename... Args>
    void SendRemoteCall(FO_NAMESPACE ptr<FO_NAMESPACE BaseEngine> engine, FO_NAMESPACE string_view name, FO_NAMESPACE ptr<FO_NAMESPACE Entity> caller, const Args&... args)
    {
        FO_NAMESPACE vector<uint8_t> data;
        FO_NAMESPACE DataWriter writer {data};
        (Detail::WriteRemoteCallArg(writer, args), ...);
        engine->SendRemoteCall(engine->Hashes.ToHashedString(name), caller, data);
    }

    template<typename... Args>
    void ModuleInitContextBase::SendRemoteCall(FO_NAMESPACE string_view name, FO_NAMESPACE ptr<FO_NAMESPACE Entity> caller, const Args&... args) const
    {
        NativeScripts::SendRemoteCall<Args...>(Engine, name, caller, args...);
    }

    // Bind a native handler for a `///@ RemoteCall <Target> <Name>(args)`
    // declared in a native `.cppm` / `.ixx` file (under
    // `FO_NATIVE_SCRIPTS_DIR`).
    // Codegen emits the metadata entry with `SubsystemHint = "native"`
    // so AS skips it; the user MUST call this once at module init for
    // every native-declared inbound RemoteCall, otherwise the engine's
    // `VerifyBindedRemoteCalls()` asserts at startup (handler count !=
    // declared count).
    //
    // The wrapper deserializes args from the packet using the
    // AS-compatible wire format (`Detail::ReadRemoteCallArg`), so this
    // works regardless of whether the outbound caller was AS-side
    // (`Player.<Side>Call.<Name>(args)`) or native-side
    // (`ctx.SendRemoteCall<args...>`).
    //
    // The handler receives the nullable `nptr<Entity> caller` (the player whose
    // packet this is) as the first argument, followed by the decoded
    // args. The caller is `nptr<Player>` on the server, the local `nptr<Player>`
    // on the client — same contract as AS-side `[[ServerRemoteCall]]`
    // handler functions.
    template<typename... Args, typename Handler>
    void BindRemoteCall(FO_NAMESPACE ptr<FO_NAMESPACE BaseEngine> engine, FO_NAMESPACE string_view name, Handler handler)
    {
        FO_NAMESPACE hstring hashed = engine->Hashes.ToHashedString(name);
        engine->SetRemoteCallHandler(
            hashed,
            [engine, handler = std::move(handler)](FO_NAMESPACE hstring, FO_NAMESPACE nptr<FO_NAMESPACE Entity> caller, FO_NAMESPACE span<uint8_t> data) FO_DEFERRED {
                FO_NAMESPACE DataReader reader {data};
                std::tuple<Args...> args = Detail::ReadRemoteCallArgs<Args...>(reader, engine);
                reader.VerifyEnd();
                std::apply([&](auto&&... a) { handler(caller, std::forward<decltype(a)>(a)...); }, std::move(args));
            },
            FO_NAMESPACE BaseEngine::RemoteCallHandlerMode::OverrideFallback);
    }

    template<typename... Args, typename Handler>
    void ModuleInitContextBase::BindRemoteCall(FO_NAMESPACE string_view name, Handler handler) const
    {
        NativeScripts::BindRemoteCall<Args...>(Engine, name, std::move(handler));
    }
}

#endif
