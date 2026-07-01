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

#include "catch_amalgamated.hpp"

#include "Common.h"

#include "angelscript.h"

#include "AngelScriptArray.h"
#include "AngelScriptDict.h"
#include "AngelScriptHelpers.h"

FO_BEGIN_NAMESPACE

namespace
{
    using namespace AngelScript;

    struct ResourceTrackerState
    {
        int NextId {};
        int FirstObservedId {};
        int LastObservedId {};
        int DoubleFreeCount {};
        int LeakedCount {};
        int GlobalCounter {};
        unordered_map<int, int> ActiveRefs {};
    };

    struct ScenarioMetrics
    {
        int FirstObservedId {};
        int LastObservedId {};
        int DoubleFreeCount {};
        int LeakedCount {};
    };

    class BytecodeStream final : public asIBinaryStream
    {
    public:
        explicit BytecodeStream(vector<asBYTE>& buf) :
            _buf {BufferPtr(buf)}
        {
            FO_NO_STACK_TRACE_ENTRY();
        }

        auto Read(void* raw_data, asUINT size) -> int override
        {
            if (size == 0) {
                return 0;
            }

            if (raw_data == nullptr) {
                return 0;
            }

            if (_readPos + size > _buf->size()) {
                return -1;
            }

            ptr<void> target = raw_data;
            ptr<const asBYTE> source = &_buf->at(_readPos);
            MemCopy(target.get(), source.get(), size);
            _readPos += size;
            return 0;
        }

        auto Write(const void* raw_data, asUINT size) -> int override
        {
            if (size == 0) {
                return 0;
            }

            if (raw_data == nullptr) {
                return 0;
            }

            _buf->resize(_writePos + size);
            ptr<asBYTE> target = &_buf->at(_writePos);
            ptr<const void> source = raw_data;
            MemCopy(target.get(), source.get(), size);
            _writePos += size;
            return 0;
        }

    private:
        static auto BufferPtr(vector<asBYTE>& buf) noexcept -> ptr<vector<asBYTE>>
        {
            FO_NO_STACK_TRACE_ENTRY();

            return &buf;
        }

        ptr<vector<asBYTE>> _buf;
        size_t _readPos {};
        size_t _writePos {};
    };

    static auto GetTrackerState(ptr<asIScriptEngine> engine) -> ptr<ResourceTrackerState>
    {
        nptr<ResourceTrackerState> nullable_tracker_state = cast_from_void<ResourceTrackerState*>(engine->GetUserData());
        FO_VERIFY_AND_THROW(nullable_tracker_state, "Script engine has no resource tracker state attached");
        return nullable_tracker_state.as_ptr();
    }

    static auto GetActiveEngine() -> ptr<asIScriptEngine>
    {
        nptr<asIScriptContext> ctx = asGetActiveContext();
        FO_VERIFY_AND_THROW(ctx, "Missing required context");
        nptr<asIScriptEngine> nullable_active_engine = ctx->GetEngine();
        FO_VERIFY_AND_THROW(nullable_active_engine, "Active script context has no engine");
        return nullable_active_engine.as_ptr();
    }

    static void ReleaseScriptEngine(ptr<asIScriptEngine> engine) noexcept
    {
        FO_NO_STACK_TRACE_ENTRY();

        engine->ShutDownAndRelease();
    }

    static auto MakeScriptEngine() -> unique_del_ptr<asIScriptEngine>
    {
        FO_STACK_TRACE_ENTRY();

        nptr<asIScriptEngine> nullable_engine = asCreateScriptEngine();
        REQUIRE(nullable_engine);
        return make_unique_del_ptr(nullable_engine.as_ptr(), ReleaseScriptEngine);
    }

    static auto AcquireResource() -> int
    {
        auto state = GetTrackerState(GetActiveEngine());
        const int id = ++state->NextId;
        state->ActiveRefs[id] = 1;
        return id;
    }

    static void ReleaseResource(int id)
    {
        auto state = GetTrackerState(GetActiveEngine());
        const auto it = state->ActiveRefs.find(id);

        if (it == state->ActiveRefs.end() || it->second == 0) {
            state->DoubleFreeCount++;
            return;
        }

        it->second--;
        if (it->second == 0) {
            state->ActiveRefs.erase(it);
        }
    }

    static void ObserveResource(int id)
    {
        auto state = GetTrackerState(GetActiveEngine());
        if (state->FirstObservedId == 0) {
            state->FirstObservedId = id;
        }
        state->LastObservedId = id;
    }

    // PtrSizedVal is a value type whose size equals sizeof(void*), simulating hstring.
    // On 64-bit it occupies 8 bytes (2 DWORDs), on 32-bit it occupies 4 bytes (1 DWORD).
    // This exercises the portable bytecode format's stack normalization for platform-dependent value types.
    struct PtrSizedVal
    {
        uintptr_t Value {};
    };

    static_assert(sizeof(PtrSizedVal) == sizeof(void*));

    static auto PtrSizedVal_CreateFromInt(int v) -> PtrSizedVal
    {
        return PtrSizedVal {static_cast<uintptr_t>(v)};
    }
    static auto PtrSizedVal_GetInt(const PtrSizedVal& v) -> int
    {
        return static_cast<int>(v.Value);
    }
    static auto PtrSizedVal_OpEquals(const PtrSizedVal& a, const PtrSizedVal& b) -> bool
    {
        return a.Value == b.Value;
    }

    static int GlobalInt = 0;

    static void SetGlobalInt(int v)
    {
        GlobalInt = v;
    }
    static auto GetGlobalInt() -> int
    {
        return GlobalInt;
    }

    static void IncrementGlobalCounter()
    {
        auto state = GetTrackerState(GetActiveEngine());
        state->GlobalCounter++;
    }

    static void MessageCallback(const asSMessageInfo* msg, void* param)
    {
        ignore_unused(param);

        ptr<const asSMessageInfo> message = msg;
        nptr<const char> nullable_section = message->section;

        if (!nullable_section) {
            return;
        }

        auto section = nullable_section.as_ptr();
        const string_view section_name {section.get()};

        if (section_name.empty()) {
            return;
        }

        // Treat only errors as test failures. Warnings (e.g. the new
        // "Redundant '?'" diagnostic that fires when `T? x = nonNullExpr;`
        // could be `T x = nonNullExpr;` instead) are emitted by intentional
        // test fixtures and should not abort the suite.
        if (message->type != asMSGTYPE_ERROR) {
            return;
        }

        const nptr<const char> text = message->message;
        FAIL(section_name << ':' << message->row << ' ' << (text ? text.get() : "<no message>"));
    }

    struct MsgCapture
    {
        vector<string> Errors {};
        vector<string> Warnings {};
    };

    static void CaptureMessageCallback(const asSMessageInfo* msg, void* param)
    {
        ptr<const asSMessageInfo> message = msg;
        nptr<MsgCapture> nullable_self = cast_from_void<MsgCapture*>(param);
        FO_VERIFY_AND_THROW(nullable_self, "Message callback param must carry capture state");
        auto self = nullable_self.as_ptr();
        const nptr<const char> text = message->message;
        const string message_text = text ? string {text.get()} : string {"<no message>"};

        if (message->type == asMSGTYPE_ERROR) {
            self->Errors.emplace_back(message_text);
        }
        else if (message->type == asMSGTYPE_WARNING) {
            self->Warnings.emplace_back(message_text);
        }
    }

    static void RegisterTestApi(ptr<asIScriptEngine> engine)
    {
        CHECK(engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true) >= 0);
        CHECK(engine->SetEngineProperty(asEP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE, true) >= 0);
        CHECK(engine->SetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_CONSTRUCT, true) >= 0);
        CHECK(engine->SetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_COPY, 2) >= 0);
        CHECK(engine->SetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_COPY_CONSTRUCT, 2) >= 0);
        CHECK(engine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE, false) >= 0);
        CHECK(engine->SetMessageCallback(asFUNCTION(MessageCallback), nullptr, asCALL_CDECL) >= 0);
        CHECK(engine->RegisterGlobalFunction("int AcquireResource()", FO_SCRIPT_FUNC(AcquireResource), FO_SCRIPT_FUNC_CONV) >= 0);
        CHECK(engine->RegisterGlobalFunction("void ReleaseResource(int)", FO_SCRIPT_FUNC(ReleaseResource), FO_SCRIPT_FUNC_CONV) >= 0);
        CHECK(engine->RegisterGlobalFunction("void ObserveResource(int)", FO_SCRIPT_FUNC(ObserveResource), FO_SCRIPT_FUNC_CONV) >= 0);

        // Register pointer-sized value type (like hstring)
        // POD with APP_CLASS + APP_CLASS_ALLINTS: engine handles construct/destruct/copy automatically
        CHECK(engine->RegisterObjectType("PtrSizedVal", sizeof(PtrSizedVal), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS | asOBJ_APP_CLASS_ALLINTS) >= 0);
        CHECK(engine->RegisterObjectMethod("PtrSizedVal", "bool opEquals(const PtrSizedVal &in) const", FO_SCRIPT_FUNC_THIS(PtrSizedVal_OpEquals), FO_SCRIPT_FUNC_THIS_CONV) >= 0);
        CHECK(engine->RegisterGlobalFunction("PtrSizedVal CreatePtrSizedVal(int)", FO_SCRIPT_FUNC(PtrSizedVal_CreateFromInt), FO_SCRIPT_FUNC_CONV) >= 0);
        CHECK(engine->RegisterGlobalFunction("int GetPtrSizedValInt(const PtrSizedVal &in)", FO_SCRIPT_FUNC(PtrSizedVal_GetInt), FO_SCRIPT_FUNC_CONV) >= 0);

        // Register global variable (exercises PGA/LDG/PshG4/CpyGtoV4/CpyVtoG4/SetG4 instructions)
        GlobalInt = 0;
        CHECK(engine->RegisterGlobalProperty("int GlobalInt", &GlobalInt) >= 0);
        CHECK(engine->RegisterGlobalFunction("void SetGlobalInt(int)", FO_SCRIPT_FUNC(SetGlobalInt), FO_SCRIPT_FUNC_CONV) >= 0);
        CHECK(engine->RegisterGlobalFunction("int GetGlobalInt()", FO_SCRIPT_FUNC(GetGlobalInt), FO_SCRIPT_FUNC_CONV) >= 0);
        CHECK(engine->RegisterGlobalFunction("void IncrementGlobalCounter()", FO_SCRIPT_FUNC(IncrementGlobalCounter), FO_SCRIPT_FUNC_CONV) >= 0);
    }

    static auto RunScenario(ptr<asIScriptModule> module, ResourceTrackerState& state) -> ScenarioMetrics
    {
        nptr<asIScriptEngine> engine = module->GetEngine();
        engine->SetUserData(&state);

        nptr<asIScriptFunction> nullable_func = module->GetFunctionByDecl("void RunScenario()");
        REQUIRE(nullable_func);

        nptr<asIScriptContext> ctx = engine->CreateContext();
        REQUIRE(ctx);

        auto func = nullable_func.as_ptr();
        CHECK(ctx->Prepare(func.get()) >= 0);
        const auto exec_result = ctx->Execute();
        if (exec_result != asEXECUTION_FINISHED) {
            nptr<const char> section;
            int column = 0;
            const auto line = ctx->GetExceptionLineNumber(&column, section.get_pp());
            nptr<const asIScriptFunction> exception_func = ctx->GetExceptionFunction();
            nptr<const char> exception_text = ctx->GetExceptionString();
            const string_view exception_decl = exception_func ? string_view {exception_func->GetDeclaration()} : string_view {"<unknown>"};

            FAIL("Execute failed: result=" << exec_result << ", section=" << (section ? section.get() : "<unknown>") << ", line=" << line << ", column=" << column << ", function=" << exception_decl << ", exception=" << (exception_text ? exception_text.get() : "<none>") << ", first_observed=" << state.FirstObservedId << ", last_observed=" << state.LastObservedId);
        }
        ctx->Release();

        CHECK(engine->GarbageCollect(asGC_FULL_CYCLE) >= 0);

        int leaked_count = 0;
        for (const auto& [id, ref_count] : state.ActiveRefs) {
            static_cast<void>(id);
            leaked_count += ref_count;
        }

        engine->SetUserData(nullptr);

        return ScenarioMetrics {
            .FirstObservedId = state.FirstObservedId,
            .LastObservedId = state.LastObservedId,
            .DoubleFreeCount = state.DoubleFreeCount,
            .LeakedCount = leaked_count,
        };
    }

    static constexpr string_view TestScript = R"(
class Resource
{
    int Id;

    Resource()
    {
        Id = AcquireResource();
    }

    ~Resource()
    {
        if (Id != 0) {
            ReleaseResource(Id);
        }
    }
}

class Holder
{
    Resource Value;
}

void RunScenario()
{
    Holder holder;
    ObserveResource(holder == null ? 200 : -30);
    holder = Holder();
    ObserveResource(holder != null ? 300 : -40);
    holder.Value = Resource();
    ObserveResource(holder.Value != null ? 100 : -20);
}
)";

    // Extended script exercises:
    // - PtrSizedVal (platform-dependent value type on stack) Ã¢â€ â€™ COPY, stack adjustment
    // - Global property access Ã¢â€ â€™ PGA, LDG, PshG4, CpyGtoV4, CpyVtoG4, SetG4
    // - Mixed-type function parameters Ã¢â€ â€™ ALLOC, FREE, REFCPY, various GET instructions
    // - Function pointers Ã¢â€ â€™ FuncPtr, CallPtr
    // - Multiple value-type locals Ã¢â€ â€™ stack layout normalization
    static constexpr string_view PortableFormatTestScript = R"(
class Container
{
    PtrSizedVal StoredVal;
    int StoredInt;
}

PtrSizedVal PassThrough(PtrSizedVal v)
{
    return v;
}

int SumInts(int a, int b, int c)
{
    return a + b + c;
}

int MixedParams(int a, PtrSizedVal v, int b)
{
    return a + GetPtrSizedValInt(v) + b;
}

void RunScenario()
{
    // Exercise PtrSizedVal as local stack variable
    PtrSizedVal v1 = CreatePtrSizedVal(42);
    ObserveResource(GetPtrSizedValInt(v1));

    // Exercise copy and pass-through
    PtrSizedVal v2 = PassThrough(v1);
    ObserveResource(GetPtrSizedValInt(v2));

    // Exercise multiple locals of different sizes on the stack
    int i1 = 10;
    PtrSizedVal v3 = CreatePtrSizedVal(20);
    int i2 = 30;
    PtrSizedVal v4 = CreatePtrSizedVal(40);
    int mixed = MixedParams(i1, v3, i2);
    ObserveResource(mixed);

    // Exercise global variable access
    GlobalInt = 100;
    int fromGlobal = GlobalInt;
    GlobalInt = fromGlobal + 50;
    ObserveResource(GlobalInt);

    // Exercise Container with PtrSizedVal member
    Container c = Container();
    c.StoredVal = CreatePtrSizedVal(77);
    c.StoredInt = 33;
    ObserveResource(GetPtrSizedValInt(c.StoredVal) + c.StoredInt);

    // Exercise PtrSizedVal equality
    PtrSizedVal v5 = CreatePtrSizedVal(99);
    PtrSizedVal v6 = CreatePtrSizedVal(99);
    ObserveResource(v5 == v6 ? 1 : 0);

    // Exercise function call with multiple value-type args (stack adjustment)
    int sum = SumInts(GetPtrSizedValInt(v1), GetPtrSizedValInt(v3), GetPtrSizedValInt(v4));
    ObserveResource(sum);

    // Exercise global counter
    IncrementGlobalCounter();
    IncrementGlobalCounter();
    IncrementGlobalCounter();
}
)";

    // This script specifically exercises the GETREF offset adjustment bug that caused crashes
    // on 32-bit platforms loading 64-bit bytecode. The bug was in AdjustGetOffset: the REFCPY
    // early-return shortcut assumed exactly 1 pointer on the stack, but methods like
    // dict<K,V>::get(const K&in, const V&in) push multiple pointer-sized items (this + key ref).
    // The GETREF for the second reference parameter (defVal) must look through all of them.
    //
    // Pattern that triggers the bug:
    //   obj.Method(const PtrSizedVal&in arg1, const int&in arg2)
    // Stack layout at CALLSYS: [this_ptr] [&arg1] [&arg2]
    // GETREF for &arg2 has offset > 1 on 64-bit (AS_PTR_SIZE=2), and the old code
    // hit the REFCPY early-return instead of falling through to count all pointer slots.
    static constexpr string_view GetRefOffsetTestScript = R"(
class Lookup
{
    int Fallback;

    Lookup()
    {
        Fallback = -1;
    }

    // Method with ptr-sized ref + small value ref: exercises GETREF through multiple ptr-sized stack items
    int Get(const PtrSizedVal &in key, const int &in defVal) const
    {
        int k = GetPtrSizedValInt(key);
        if (k == Fallback)
            return defVal;
        return k + defVal;
    }

    // Method with two ptr-sized refs: both references go through REFCPY adjustment
    int GetTwo(const PtrSizedVal &in key1, const PtrSizedVal &in key2) const
    {
        return GetPtrSizedValInt(key1) + GetPtrSizedValInt(key2) + Fallback;
    }
}

// Free function with ptr-sized ref + small value ref (no this pointer, but still 2+ refs)
int FreeLookup(const PtrSizedVal &in key, const int &in defVal)
{
    int k = GetPtrSizedValInt(key);
    return k + defVal;
}

// Free function with 3 references including ptr-sized: maximally exercises offset counting
int TripleRef(const PtrSizedVal &in a, const int &in b, const PtrSizedVal &in c)
{
    return GetPtrSizedValInt(a) + b + GetPtrSizedValInt(c);
}

void RunScenario()
{
    Lookup lk = Lookup();
    lk.Fallback = 100;

    // Method call: this + PtrSizedVal ref + int ref Ã¢â€ â€™ GETREF offset bug scenario
    PtrSizedVal key1 = CreatePtrSizedVal(42);
    int r1 = lk.Get(key1, 10);
    ObserveResource(r1);  // 42 + 10 = 52

    // Method call hitting the fallback path (key == Fallback)
    PtrSizedVal key2 = CreatePtrSizedVal(100);
    int r2 = lk.Get(key2, 7);
    ObserveResource(r2);  // defVal = 7

    // Method with two ptr-sized refs
    PtrSizedVal key3 = CreatePtrSizedVal(20);
    PtrSizedVal key4 = CreatePtrSizedVal(30);
    int r3 = lk.GetTwo(key3, key4);
    ObserveResource(r3);  // 20 + 30 + 100 = 150

    // Free function: PtrSizedVal ref + int ref
    int r4 = FreeLookup(key1, 8);
    ObserveResource(r4);  // 42 + 8 = 50

    // Triple ref: PtrSizedVal + int + PtrSizedVal
    int r5 = TripleRef(key1, 5, key3);
    ObserveResource(r5);  // 42 + 5 + 20 = 67

    // Chain: use result of one lookup as default for the next
    PtrSizedVal key5 = CreatePtrSizedVal(100);
    int chained = lk.Get(key5, lk.Get(key1, 0));
    ObserveResource(chained);  // key5==Fallback Ã¢â€ â€™ defVal = lk.Get(key1, 0) = 42+0 = 42
}
)";

    static auto BuildModule(ptr<asIScriptEngine> engine, const string& module_name) -> ptr<asIScriptModule>
    {
        nptr<asIScriptModule> nullable_built_module = engine->GetModule(module_name.c_str(), asGM_ALWAYS_CREATE);
        REQUIRE(nullable_built_module);
        CHECK(nullable_built_module->AddScriptSection("bytecode_test", TestScript.data(), numeric_cast<unsigned>(TestScript.size())) >= 0);
        CHECK(nullable_built_module->Build() >= 0);
        return nullable_built_module.as_ptr();
    }

    static auto BuildModuleFromScript(ptr<asIScriptEngine> engine, const string& module_name, string_view script) -> ptr<asIScriptModule>
    {
        nptr<asIScriptModule> nullable_built_module = engine->GetModule(module_name.c_str(), asGM_ALWAYS_CREATE);
        REQUIRE(nullable_built_module);
        CHECK(nullable_built_module->AddScriptSection("bytecode_test", script.data(), numeric_cast<unsigned>(script.size())) >= 0);
        CHECK(nullable_built_module->Build() >= 0);
        return nullable_built_module.as_ptr();
    }
}

TEST_CASE("AngelScriptBytecode", "[angelscript][bytecode]")
{
    SECTION("SaveLoadPreservesGeneratedScriptCopyBehavior")
    {
        auto build_engine = MakeScriptEngine();
        REQUIRE(nptr<asIScriptEngine> {build_engine});
        RegisterTestApi(build_engine.get());

        auto build_module = BuildModule(build_engine.get(), "BuildModule");

        ResourceTrackerState build_state {};
        const auto build_metrics = RunScenario(build_module, build_state);

        vector<asBYTE> bytecode {};
        BytecodeStream writer {bytecode};
        CHECK(build_module->SaveByteCode(&writer) >= 0);
        REQUIRE_FALSE(bytecode.empty());

        auto load_engine = MakeScriptEngine();
        REQUIRE(nptr<asIScriptEngine> {load_engine});
        RegisterTestApi(load_engine.get());

        nptr<asIScriptModule> load_module = load_engine->GetModule("LoadModule", asGM_ALWAYS_CREATE);
        REQUIRE(load_module);
        BytecodeStream reader {bytecode};
        CHECK(load_module->LoadByteCode(&reader) >= 0);

        ResourceTrackerState load_state {};
        const auto load_metrics = RunScenario(load_module.as_ptr(), load_state);

        CHECK(build_metrics.FirstObservedId == 200);
        CHECK(build_metrics.LastObservedId == 100);
        CHECK(build_metrics.DoubleFreeCount == 0);
        CHECK(build_metrics.LeakedCount == 0);

        CHECK(load_metrics.FirstObservedId == 200);
        CHECK(load_metrics.LastObservedId == 100);

        CHECK(load_metrics.DoubleFreeCount == build_metrics.DoubleFreeCount);
        CHECK(load_metrics.LeakedCount == build_metrics.LeakedCount);
    }

    SECTION("PortableFormatPreservesPlatformDependentValueTypes")
    {
        // This test exercises bytecode serialization with registered value types whose size
        // depends on pointer size (like hstring). On 64-bit, PtrSizedVal is 8 bytes (2 DWORDs),
        // on 32-bit it would be 4 bytes (1 DWORD). The portable bytecode format normalizes
        // stack layouts to handle this difference.

        auto build_engine = MakeScriptEngine();
        REQUIRE(nptr<asIScriptEngine> {build_engine});
        RegisterTestApi(build_engine.get());

        auto build_module = BuildModuleFromScript(build_engine.get(), "BuildModule", PortableFormatTestScript);

        ResourceTrackerState build_state {};
        const auto build_metrics = RunScenario(build_module, build_state);

        // Verify build-time execution produces expected values
        // Sequence: 42, 42, 60, 150, 110, 1, 102
        CHECK(build_metrics.FirstObservedId == 42);
        CHECK(build_metrics.LastObservedId == 102);
        CHECK(build_metrics.DoubleFreeCount == 0);
        CHECK(build_metrics.LeakedCount == 0);
        CHECK(build_state.GlobalCounter == 3);

        // Save bytecode
        vector<asBYTE> bytecode {};
        BytecodeStream writer {bytecode};
        CHECK(build_module->SaveByteCode(&writer) >= 0);
        REQUIRE_FALSE(bytecode.empty());

        // Load in fresh engine
        auto load_engine = MakeScriptEngine();
        REQUIRE(nptr<asIScriptEngine> {load_engine});
        RegisterTestApi(load_engine.get());

        nptr<asIScriptModule> load_module = load_engine->GetModule("LoadModule", asGM_ALWAYS_CREATE);
        REQUIRE(load_module);
        BytecodeStream reader {bytecode};
        CHECK(load_module->LoadByteCode(&reader) >= 0);

        // Execute loaded bytecode and verify identical results
        ResourceTrackerState load_state {};
        const auto load_metrics = RunScenario(load_module.as_ptr(), load_state);

        CHECK(load_metrics.FirstObservedId == build_metrics.FirstObservedId);
        CHECK(load_metrics.LastObservedId == build_metrics.LastObservedId);
        CHECK(load_metrics.DoubleFreeCount == build_metrics.DoubleFreeCount);
        CHECK(load_metrics.LeakedCount == build_metrics.LeakedCount);
        CHECK(load_state.GlobalCounter == build_state.GlobalCounter);
    }

    SECTION("NullableHandleRuntimeGuard")
    {
        // `T?` opts a handle into nullability; bare `T` rejects null at runtime.
        // The compiler emits asBC_RefCpyChk for non-nullable destinations and the VM throws
        // a null pointer access exception when the source handle is null.
        auto engine = MakeScriptEngine();
        REQUIRE(nptr<asIScriptEngine> {engine});
        RegisterTestApi(engine.get());

        static constexpr string_view NullableScript = R"(
class Resource
{
    int Id;
    Resource() { Id = AcquireResource(); }
    ~Resource() { if (Id != 0) ReleaseResource(Id); }
}

// Nullable assignment: must not throw.
void AssignNullToNullable()
{
    Resource? optional = Resource();
    optional = null;
    ObserveResource(optional == null ? 1 : 0);
}

// Non-nullable assignment to null: must throw at runtime.
void AssignNullToNonNullable()
{
    Resource required = Resource();
    required = null;
    ObserveResource(-999);
}
)";

        nptr<asIScriptModule> module = engine->GetModule("NullableModule", asGM_ALWAYS_CREATE);
        REQUIRE(module);
        CHECK(module->AddScriptSection("nullable_test", NullableScript.data(), numeric_cast<unsigned>(NullableScript.size())) >= 0);
        CHECK(module->Build() >= 0);

        ResourceTrackerState state {};
        engine->SetUserData(&state);

        // Nullable: assignment of null must succeed and the variable observes as null.
        {
            nptr<asIScriptFunction> func = module->GetFunctionByDecl("void AssignNullToNullable()");
            REQUIRE(func);
            nptr<asIScriptContext> ctx = engine->CreateContext();
            REQUIRE(ctx);
            CHECK(ctx->Prepare(func.get()) >= 0);
            const auto result = ctx->Execute();
            CHECK(result == asEXECUTION_FINISHED);
            ctx->Release();
            CHECK(state.LastObservedId == 1);
        }

        // Non-nullable: assignment of null must raise the dedicated
        // "Null assignment to non-nullable handle" exception before ObserveResource(-999) runs.
        {
            state.LastObservedId = 0;
            nptr<asIScriptFunction> func = module->GetFunctionByDecl("void AssignNullToNonNullable()");
            REQUIRE(func);
            nptr<asIScriptContext> ctx = engine->CreateContext();
            REQUIRE(ctx);
            CHECK(ctx->Prepare(func.get()) >= 0);
            const auto result = ctx->Execute();
            CHECK(result == asEXECUTION_EXCEPTION);
            nptr<const char> ex = ctx->GetExceptionString();
            REQUIRE(ex);
            CHECK(string_view {ex.get()} == "Null assignment to non-nullable handle");
            // The sentinel ObserveResource(-999) must NOT have been reached.
            CHECK(state.LastObservedId != -999);
            ctx->Release();
        }

        engine->SetUserData(nullptr);
        CHECK(engine->GarbageCollect(asGC_FULL_CYCLE) >= 0);
    }

    SECTION("CompileTimeRejectsNullableToNonNullableAssignment")
    {
        // Maximum compile-time guarantee: assigning a nullable
        // source to a non-nullable destination is rejected at build time. Two
        // shapes are tested:
        //   1. `T x = null;`         â€" explicit null literal.
        //   2. `T x = nullableExpr;` â€" nullable handle value.
        // The third shape is allowed: `T x = nullableExpr;` inside an
        // `if (nullableExpr != null)` guard, because smart-cast narrows the
        // source to non-nullable.
        const auto buildScript = [](const char* source, MsgCapture& capture) -> int {
            auto engine = MakeScriptEngine();
            REQUIRE(nptr<asIScriptEngine> {engine});
            engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true);
            engine->SetEngineProperty(asEP_DISALLOW_NULLABLE_TO_NON_NULLABLE, true);
            engine->RegisterObjectType("Resource", 0, asOBJ_REF | asOBJ_NOCOUNT);
            engine->SetMessageCallback(asFUNCTION(CaptureMessageCallback), &capture, asCALL_CDECL);
            nptr<asIScriptModule> module = engine->GetModule("RejectModule", asGM_ALWAYS_CREATE);
            REQUIRE(module);
            module->AddScriptSection("reject", source, std::strlen(source));
            return module->Build();
        };

        // 1) Bare `null` assigned to a non-nullable handle â€" reject.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void NullToNonNullable()
                {
                    Resource r = null;
                }
            )",
                cap);
            CHECK(r < 0);
            bool found = false;
            for (const auto& e : cap.Errors) {
                if (e.find("Cannot assign 'null' to a non-nullable handle") != string::npos) {
                    found = true;
                    break;
                }
            }
            CHECK(found);
        }

        // 2) Nullable handle assigned to non-nullable â€" reject.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                Resource? GetMaybe() { return null; }
                void NullableToNonNullable()
                {
                    Resource x = GetMaybe();
                }
            )",
                cap);
            CHECK(r < 0);
            bool found = false;
            for (const auto& e : cap.Errors) {
                if (e.find("Cannot assign nullable") != string::npos) {
                    found = true;
                    break;
                }
            }
            CHECK(found);
        }

        // 3) Same shape inside an `if (g != null)` guard â€" smart-cast lets it compile.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                Resource? GetMaybe() { return null; }
                void GuardedNullableToNonNullable()
                {
                    Resource? g = GetMaybe();
                    if (g != null) {
                        Resource x = g;
                    }
                }
            )",
                cap);
            if (!cap.Errors.empty()) {
                INFO(cap.Errors.front());
            }
            CHECK(r >= 0);
        }

        // 4) Conditional with a nullable branch assigned to non-nullable - reject.
        //    (FOnline) `cond ? a : b` is nullable when either branch is nullable.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                Resource? GetMaybe() { return null; }
                void TernaryNullableToNonNullable(bool c)
                {
                    Resource x = c ? GetMaybe() : GetMaybe();
                }
            )",
                cap);
            CHECK(r < 0);
            bool found = false;
            for (const auto& e : cap.Errors) {
                if (e.find("Cannot assign nullable") != string::npos) {
                    found = true;
                    break;
                }
            }
            CHECK(found);
        }

        // 5) Conditional with a `null` literal branch assigned to non-nullable - reject.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void TernaryNullLiteralToNonNullable(bool c, Resource res)
                {
                    Resource x = c ? res : null;
                }
            )",
                cap);
            CHECK(r < 0);
            bool found = false;
            for (const auto& e : cap.Errors) {
                if (e.find("Cannot assign nullable") != string::npos) {
                    found = true;
                    break;
                }
            }
            CHECK(found);
        }

        // 6) Conditional with a nullable branch assigned to a nullable destination - allowed.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                Resource? GetMaybe() { return null; }
                void TernaryNullableToNullable(bool c)
                {
                    Resource? x = c ? GetMaybe() : GetMaybe();
                }
            )",
                cap);
            if (!cap.Errors.empty()) {
                INFO(cap.Errors.front());
            }
            CHECK(r >= 0);
        }
    }

    SECTION("WarnsOnRedundantNullableOnLocalInit")
    {
        // Diagnostic for needless widening: declaring a
        // local `T? x = nonNullExpr;` is suspicious - the initializer cannot
        // produce null, so the `?` only enables a dead null-check downstream
        // and weakens the type information. Emit a warning (not an error)
        // so existing scripts keep building while authors clean up; the
        // warning carries the would-be replacement type in the message.
        const auto buildScript = [](const char* source, MsgCapture& capture) -> int {
            auto engine = MakeScriptEngine();
            REQUIRE(nptr<asIScriptEngine> {engine});
            engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true);
            engine->RegisterObjectType("Resource", 0, asOBJ_REF | asOBJ_NOCOUNT);
            engine->RegisterGlobalFunction("Resource MakeNonNull()", FO_SCRIPT_FUNC(+[]() -> void* {
                static int sentinel = 0;
                return &sentinel;
            }),
                FO_SCRIPT_FUNC_CONV);
            engine->RegisterGlobalFunction("Resource? MakeMaybe()", FO_SCRIPT_FUNC(+[]() -> void* { return nullptr; }), FO_SCRIPT_FUNC_CONV);
            engine->SetMessageCallback(asFUNCTION(CaptureMessageCallback), &capture, asCALL_CDECL);
            nptr<asIScriptModule> module = engine->GetModule("RedundantNullableMod", asGM_ALWAYS_CREATE);
            REQUIRE(module);
            module->AddScriptSection("redundant_nullable", source, std::strlen(source));
            return module->Build();
        };

        // 1) `T? x = nonNullExpr;` â€" must produce the "Redundant '?'" warning.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void RedundantQuestion()
                {
                    Resource? x = MakeNonNull();
                }
            )",
                cap);
            CHECK(r >= 0); // build succeeds (warning, not error)
            bool found = false;
            for (const auto& w : cap.Warnings) {
                if (w.find("Redundant '?'") != string::npos) {
                    found = true;
                    break;
                }
            }
            CHECK(found);
        }

        // 2) `T x = nonNullExpr;` â€" no warning (no `?` at all).
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void NoQuestion()
                {
                    Resource x = MakeNonNull();
                }
            )",
                cap);
            CHECK(r >= 0);
            for (const auto& w : cap.Warnings) {
                CHECK(w.find("Redundant '?'") == string::npos);
            }
        }

        // 3) `T? x = nullableExpr;` â€" no warning (RHS can really be null).
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void LegitNullable()
                {
                    Resource? x = MakeMaybe();
                }
            )",
                cap);
            CHECK(r >= 0);
            for (const auto& w : cap.Warnings) {
                CHECK(w.find("Redundant '?'") == string::npos);
            }
        }

        // 4) `T? x = null;` â€" no warning (the null-literal special case has
        //    its own diagnostic path; widening warning must not double-fire).
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void NullLiteral()
                {
                    Resource? x = null;
                }
            )",
                cap);
            CHECK(r >= 0);
            for (const auto& w : cap.Warnings) {
                CHECK(w.find("Redundant '?'") == string::npos);
            }
        }
    }

    SECTION("SuppressesRedundantNullableForRuntimeNullableShapes")
    {
        // The redundant-? warning must NOT fire for RHS shapes
        // whose static type is non-null but whose runtime value can still be
        // null. The exempt shapes are:
        //   * `cast<T>(...)`            - a failed downcast yields null.
        //   * `cond ? a : b`            - AS picks the non-null branch as the
        //                                 common type, dropping nullability.
        //   * `const T@&` ref           - `dict.get(key, default)` substitutes a
        //                                 (possibly null) default and yields null
        //                                 on a missing key, so the const ref it
        //                                 returns may hold null.
        // A NON-const handle reference is NOT exempt: `array[i]` of a
        // non-nullable element type (and a non-nullable field/local reached by
        // reference) aliases storage whose non-null invariant is enforced on
        // every write, so widening it to `T?` IS redundant and must warn - just
        // like a handle returned by value (a temporary).
        // Backing slot for the `GetByRef()` accessor. The function is never
        // executed (these are build-only fixtures), so the stored handle value
        // is irrelevant - only the AS signature matters.
        static void* g_resourceCell = nullptr;
        const auto buildScript = [](const char* source, MsgCapture& capture) -> int {
            auto engine = MakeScriptEngine();
            REQUIRE(nptr<asIScriptEngine> {engine});
            engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true);
            engine->RegisterObjectType("Resource", 0, asOBJ_REF | asOBJ_NOCOUNT);
            engine->RegisterGlobalFunction("Resource MakeNonNull()", FO_SCRIPT_FUNC(+[]() -> void* {
                static int sentinel = 0;
                return &sentinel;
            }),
                FO_SCRIPT_FUNC_CONV);
            engine->RegisterGlobalFunction("Resource? MakeMaybe()", FO_SCRIPT_FUNC(+[]() -> void* { return nullptr; }), FO_SCRIPT_FUNC_CONV);
            // Mimics `array[i]`: returns a (non-temporary) NON-const reference to
            // a handle cell of a non-nullable element type - enforced non-null,
            // so widening it to `T?` is redundant.
            engine->RegisterGlobalFunction("Resource@& GetByRef()", FO_SCRIPT_FUNC(+[]() -> void* { return &g_resourceCell; }), FO_SCRIPT_FUNC_CONV);
            // Real array+dict so the const-handle-reference shape can be tested
            // faithfully: `dict<K,V>` is registered `const T2& get(...)`, which
            // for a handle value type yields a const ref to a *mutable* handle -
            // a shape that cannot be spelled via a raw RegisterGlobalFunction
            // decl (there `const T@` means handle-to-const, a different type).
            RegisterAngelScriptArray(engine.get());
            RegisterAngelScriptDict(engine.get());
            engine->RegisterGlobalFunction("bool Cond()", FO_SCRIPT_FUNC(+[]() -> bool { return true; }), FO_SCRIPT_FUNC_CONV);
            engine->SetMessageCallback(asFUNCTION(CaptureMessageCallback), &capture, asCALL_CDECL);
            nptr<asIScriptModule> module = engine->GetModule("RuntimeNullableShapesMod", asGM_ALWAYS_CREATE);
            REQUIRE(module);
            module->AddScriptSection("runtime_nullable_shapes", source, std::strlen(source));
            return module->Build();
        };

        const auto hasRedundantWarning = [](const MsgCapture& cap) -> bool {
            for (const auto& w : cap.Warnings) {
                if (w.find("Redundant '?'") != string::npos) {
                    return true;
                }
            }
            return false;
        };
        const auto dumpErrors = [](const MsgCapture& cap) -> string {
            string joined;
            for (const auto& e : cap.Errors) {
                joined += e;
                joined += " | ";
            }
            return joined;
        };

        // 1) `cast<T>(...)` - failed downcast can return null at runtime.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                class CastBase {}
                class CastDerived : CastBase {}
                void CastCase()
                {
                    CastBase b = CastDerived();
                    CastDerived? d = cast<CastDerived>(b);
                }
            )",
                cap);
            INFO("cast build errors: " << dumpErrors(cap));
            CHECK(r >= 0);
            CHECK_FALSE(hasRedundantWarning(cap));
        }

        // 2) `cond ? a : null` - ternary with a null branch. AS infers the
        //    non-null common type, so `?` is needed to express the result.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void TernaryCase()
                {
                    Resource? x = Cond() ? MakeNonNull() : null;
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK_FALSE(hasRedundantWarning(cap));
        }

        // 3) `const T@&` - `dict.get` returns `const T2& get(...)`, so for a
        //    handle value type it yields a const ref to a *mutable* handle whose
        //    looked-up-or-default value can be null at runtime. Must NOT warn.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void ConstContainerRefCase()
                {
                    dict<int, Resource> d;
                    Resource? x = d.get(0, null);
                }
            )",
                cap);
            INFO("const-container-ref build errors: " << dumpErrors(cap));
            CHECK(r >= 0);
            CHECK_FALSE(hasRedundantWarning(cap));
        }

        // 3b) `T@&` - array[i]-style NON-const reference into a non-nullable
        //     element cell (enforced non-null). The `?` is redundant; must warn.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void NonConstContainerRefCase()
                {
                    Resource? x = GetByRef();
                }
            )",
                cap);
            INFO("non-const-container-ref build errors: " << dumpErrors(cap));
            CHECK(r >= 0);
            CHECK(hasRedundantWarning(cap));
        }

        // 4) Regression anchor: a handle returned BY VALUE (a temporary) is
        //    NOT a container reference, so the warning must still fire. This
        //    pins the `!isTemporary` half of the reference exemption.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void ByValueCase()
                {
                    Resource? x = MakeNonNull();
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK(hasRedundantWarning(cap));
        }
    }

    SECTION("RejectsNullableReturnFromNonNullableFunction")
    {
        // Returning a nullable value (the `null` literal or a
        // `T?` expression) from a function whose declared return type is a
        // non-nullable handle must be a compile error. Without this guard the
        // implicit conversion silently strips nullability and the caller
        // crashes at runtime with `asBC_RefCpyChk`.
        const auto buildScript = [](const char* source, MsgCapture& capture) -> int {
            auto engine = MakeScriptEngine();
            REQUIRE(nptr<asIScriptEngine> {engine});
            engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true);
            engine->RegisterObjectType("Resource", 0, asOBJ_REF | asOBJ_NOCOUNT);
            engine->RegisterGlobalFunction("Resource? MakeMaybe()", FO_SCRIPT_FUNC(+[]() -> void* { return nullptr; }), FO_SCRIPT_FUNC_CONV);
            engine->SetMessageCallback(asFUNCTION(CaptureMessageCallback), &capture, asCALL_CDECL);
            nptr<asIScriptModule> module = engine->GetModule("ReturnNullableMod", asGM_ALWAYS_CREATE);
            REQUIRE(module);
            module->AddScriptSection("return_nullable", source, std::strlen(source));
            return module->Build();
        };

        const auto hasReturnNullableError = [](const MsgCapture& cap) -> bool {
            for (const auto& e : cap.Errors) {
                if (e.find("Cannot return nullable value from a non-nullable return type") != string::npos) {
                    return true;
                }
            }
            return false;
        };

        // 1) `return null;` from a non-nullable return type - must error.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                Resource ReturnsNullLiteral() { return null; }
            )",
                cap);
            CHECK(r < 0);
            CHECK(hasReturnNullableError(cap));
        }

        // 2) `return nullableExpr;` from a non-nullable return type - must error.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                Resource ReturnsNullable() { return MakeMaybe(); }
            )",
                cap);
            CHECK(r < 0);
            CHECK(hasReturnNullableError(cap));
        }

        // 3) `T? Good() { return null; }` - nullable return type accepts null.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                Resource? ReturnsNullableOk() { return null; }
            )",
                cap);
            CHECK(r >= 0);
            CHECK_FALSE(hasReturnNullableError(cap));
        }
    }

    SECTION("WarnsOnDereferenceOfUnnarrowedNullable")
    {
        // Accessing a member (property / method / field) on a
        // nullable handle `T?` that has not been narrowed is suspicious: the
        // author should guard first. The compiler emits a warning (not an
        // error - the runtime still traps the null access) and smart-cast must
        // suppress it once an `if`-guard narrows the handle to `T`.
        const auto buildScript = [](const char* source, MsgCapture& capture) -> int {
            auto engine = MakeScriptEngine();
            REQUIRE(nptr<asIScriptEngine> {engine});
            engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true);
            engine->RegisterObjectType("Resource", 0, asOBJ_REF | asOBJ_NOCOUNT);
            engine->RegisterObjectMethod("Resource", "void Touch()", FO_SCRIPT_FUNC_THIS(+[](void*) { }), FO_SCRIPT_FUNC_THIS_CONV);
            engine->RegisterGlobalFunction("Resource? MakeMaybe()", FO_SCRIPT_FUNC(+[]() -> void* { return nullptr; }), FO_SCRIPT_FUNC_CONV);
            engine->SetMessageCallback(asFUNCTION(CaptureMessageCallback), &capture, asCALL_CDECL);
            nptr<asIScriptModule> module = engine->GetModule("DerefNullableMod", asGM_ALWAYS_CREATE);
            REQUIRE(module);
            module->AddScriptSection("deref_nullable", source, std::strlen(source));
            return module->Build();
        };

        const auto hasDerefWarning = [](const MsgCapture& cap) -> bool {
            for (const auto& w : cap.Warnings) {
                if (w.find("Dereference of nullable handle") != string::npos) {
                    return true;
                }
            }
            return false;
        };

        // 1) Member access on an un-narrowed nullable - must warn.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void Unguarded()
                {
                    Resource? r = MakeMaybe();
                    r.Touch();
                }
            )",
                cap);
            CHECK(r >= 0); // warning, not error
            CHECK(hasDerefWarning(cap));
        }

        // 2) `if (x == null) return;` narrows - no warning afterwards.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void GuardedByEarlyReturn()
                {
                    Resource? r = MakeMaybe();
                    if (r == null) {
                        return;
                    }
                    r.Touch();
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK_FALSE(hasDerefWarning(cap));
        }

        // 3) `if (x != null) { ... }` narrows inside the then-branch.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void GuardedByIf()
                {
                    Resource? r = MakeMaybe();
                    if (r != null) {
                        r.Touch();
                    }
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK_FALSE(hasDerefWarning(cap));
        }

        // 4) Member access on a non-nullable handle - never warns.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void NonNullable()
                {
                    Resource? maybe = MakeMaybe();
                    if (maybe == null) {
                        return;
                    }
                    Resource sure = maybe;
                    sure.Touch();
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK_FALSE(hasDerefWarning(cap));
        }
    }

    SECTION("SmartCastNarrowsThroughAndOrAndTernary")
    {
        // A `x != null` guard placed in a `&&` / `||`
        // short-circuit or a ternary condition narrows `x` for the guarded
        // operand/branch, exactly like an `if`-guard: `x != null && x.IsReady()`
        // dereferences `x` only when it was proven non-null, so no warning. The
        // narrowing is order-sensitive - dereferencing before the check still
        // warns.
        const auto buildScript = [](const char* source, MsgCapture& capture) -> int {
            auto engine = MakeScriptEngine();
            REQUIRE(nptr<asIScriptEngine> {engine});
            engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true);
            engine->RegisterObjectType("Resource", 0, asOBJ_REF | asOBJ_NOCOUNT);
            engine->RegisterObjectMethod("Resource", "bool IsReady()", FO_SCRIPT_FUNC_THIS(+[](void*) -> bool { return true; }), FO_SCRIPT_FUNC_THIS_CONV);
            engine->RegisterObjectMethod("Resource", "bool get_Ready() const property", FO_SCRIPT_FUNC_THIS(+[](void*) -> bool { return true; }), FO_SCRIPT_FUNC_THIS_CONV);
            engine->RegisterGlobalFunction("Resource? MakeMaybe()", FO_SCRIPT_FUNC(+[]() -> void* { return nullptr; }), FO_SCRIPT_FUNC_CONV);
            engine->RegisterGlobalFunction("bool Cond()", FO_SCRIPT_FUNC(+[]() -> bool { return true; }), FO_SCRIPT_FUNC_CONV);
            engine->SetMessageCallback(asFUNCTION(CaptureMessageCallback), &capture, asCALL_CDECL);
            nptr<asIScriptModule> module = engine->GetModule("AndOrNarrowMod", asGM_ALWAYS_CREATE);
            REQUIRE(module);
            module->AddScriptSection("andor_narrow", source, std::strlen(source));
            return module->Build();
        };

        const auto hasDerefWarning = [](const MsgCapture& cap) -> bool {
            for (const auto& w : cap.Warnings) {
                if (w.find("Dereference of nullable handle") != string::npos) {
                    return true;
                }
            }
            return false;
        };

        // 1) `x != null && x.IsReady()` - `&&` consumes the `!=` check, so the
        //    right operand sees `x` as non-null.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                bool AndNarrows()
                {
                    Resource? r = MakeMaybe();
                    return r != null && r.IsReady();
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK_FALSE(hasDerefWarning(cap));
        }

        // 2) `x == null || x.IsReady()` - `||` consumes the `==` check, so the
        //    right operand sees `x` as non-null.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                bool OrNarrows()
                {
                    Resource? r = MakeMaybe();
                    return r == null || r.IsReady();
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK_FALSE(hasDerefWarning(cap));
        }

        // 3) Ternary then-branch narrows when the condition is `x != null`.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                bool TernaryThenNarrows()
                {
                    Resource? r = MakeMaybe();
                    return r != null ? r.IsReady() : false;
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK_FALSE(hasDerefWarning(cap));
        }

        // 4) Ternary else-branch narrows when the condition is `x == null`.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                bool TernaryElseNarrows()
                {
                    Resource? r = MakeMaybe();
                    return r == null ? false : r.IsReady();
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK_FALSE(hasDerefWarning(cap));
        }

        // 5) Negative control: dereferencing before the null-check still warns -
        //    the left operand of `&&` is evaluated unconditionally.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                bool DerefBeforeCheckWarns()
                {
                    Resource? r = MakeMaybe();
                    return r.IsReady() && r != null;
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK(hasDerefWarning(cap));
        }

        // 6) The guarded operand may carry a `!` prefix.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                bool NotPrefixOnGuardedOperand()
                {
                    Resource? r = MakeMaybe();
                    return r != null && !r.IsReady();
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK_FALSE(hasDerefWarning(cap));
        }

        // 7) The tag propagates across a `&&` chain, so the trailing operand
        //    narrows too - not only the operand immediately after the check.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                bool ChainTailNarrows()
                {
                    Resource? r = MakeMaybe();
                    return r != null && Cond() && r.IsReady();
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK_FALSE(hasDerefWarning(cap));
        }

        // 8) A nullable PARAMETER narrows the same way. Parameter stack offsets
        //    are negative, which once aliased the "no tag" sentinel and silently
        //    disabled narrowing for every parameter; a dedicated validity flag
        //    now gates the tag. A nullable *second* parameter is used so its
        //    offset is unmistakably negative.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                bool NullableSecondParamNarrows(int first, Resource? r)
                {
                    return first > 0 && r != null && !r.IsReady() && Cond();
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK_FALSE(hasDerefWarning(cap));
        }

        // 9) Property access (get accessor) on the guarded operand narrows.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                bool ParamPropertyNarrows(Resource? r)
                {
                    return r != null && !r.Ready && Cond();
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK_FALSE(hasDerefWarning(cap));
        }

        // 10) The `if`-condition form (the real-world shape from
        //     AiThreatControl.fos): chain inside an `if` condition.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void IfConditionChain(Resource? r)
                {
                    if (r != null && !r.Ready && r.IsReady()) {
                    }
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK_FALSE(hasDerefWarning(cap));
        }

        // 11) Compound right operand: the guard narrows across the WHOLE right
        //     operand, not just an adjacent term. `r.Ready` is dereferenced
        //     inside `r.Ready == Cond()`, which is the right operand of `&&`.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                bool CompoundRightOperand(Resource? r)
                {
                    return r != null && r.Ready == Cond();
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK_FALSE(hasDerefWarning(cap));
        }

        // 12) Compound right operand inside an `if`, with a trailing term
        //     (mirrors `map != null && map.ProtoId == pid` patterns).
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void CompoundInIf(Resource? r)
                {
                    if (r != null && r.Ready == Cond() && Cond()) {
                    }
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK_FALSE(hasDerefWarning(cap));
        }
    }

    SECTION("WarnsOnRedundantNullComparison")
    {
        // Comparing a non-nullable handle to null (`h == null` /
        // `h != null`) has a constant result - the handle can never be null - so
        // the guarded branch is dead code. The compiler emits a warning (not an
        // error). A genuinely nullable handle compared to null must NOT warn, and
        // a handle already narrowed to non-null SHOULD warn on a second check.
        const auto buildScript = [](const char* source, MsgCapture& capture) -> int {
            auto engine = MakeScriptEngine();
            REQUIRE(nptr<asIScriptEngine> {engine});
            engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true);
            engine->RegisterObjectType("Resource", 0, asOBJ_REF | asOBJ_NOCOUNT);
            engine->RegisterGlobalFunction("Resource? MakeMaybe()", FO_SCRIPT_FUNC(+[]() -> void* { return nullptr; }), FO_SCRIPT_FUNC_CONV);
            engine->SetMessageCallback(asFUNCTION(CaptureMessageCallback), &capture, asCALL_CDECL);
            // Non-nullable return type: the *static* type is non-nullable, but a
            // call result is a temporary (compile-only here, never invoked).
            engine->RegisterGlobalFunction("Resource@ MakeSure()", FO_SCRIPT_FUNC(+[]() -> void* { return nullptr; }), FO_SCRIPT_FUNC_CONV);
            nptr<asIScriptModule> module = engine->GetModule("RedundantNullCmpMod", asGM_ALWAYS_CREATE);
            REQUIRE(module);
            module->AddScriptSection("redundant_null_cmp", source, std::strlen(source));
            return module->Build();
        };

        const auto hasRedundantCmpWarning = [](const MsgCapture& cap) -> bool {
            for (const auto& w : cap.Warnings) {
                if (w.find("Redundant null comparison") != string::npos) {
                    return true;
                }
            }
            return false;
        };

        // 1) Non-nullable parameter compared to null - must warn.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void F(Resource r)
                {
                    if (r == null) {
                    }
                }
            )",
                cap);
            CHECK(r >= 0); // warning, not error
            CHECK(hasRedundantCmpWarning(cap));
        }

        // 2) Nullable handle compared to null - legitimate, must NOT warn.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void F()
                {
                    Resource? r = MakeMaybe();
                    if (r == null) {
                    }
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK_FALSE(hasRedundantCmpWarning(cap));
        }

        // 3) `!=` form on a non-nullable parameter - must warn.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void F(Resource r)
                {
                    if (r != null) {
                    }
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK(hasRedundantCmpWarning(cap));
        }

        // 4) A nullable handle narrowed to non-null, then compared again - the
        //    second comparison is redundant and must warn.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void F()
                {
                    Resource? r = MakeMaybe();
                    if (r == null) {
                        return;
                    }
                    if (r != null) {
                    }
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK(hasRedundantCmpWarning(cap));
        }

        // 5) A non-nullable-typed temporary (a call result) compared to null.
        //    The static type is non-nullable, so the check is redundant and must
        //    warn for temporaries too - not just named locals/params. A source
        //    that can genuinely be null must spell itself nullable (`cast<T?>`,
        //    a `Resource@?` return, the `Nullable` property flag) instead.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void F()
                {
                    if (MakeSure() != null) {
                    }
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK(hasRedundantCmpWarning(cap));
        }
    }

    SECTION("NoReturnCallNarrowsLikeReturn")
    {
        // A function marked no-return (always throws/exits)
        // terminates control flow like `return`. A guard branch ending in such
        // a call - `if (x == null) { Fatal(); }` - narrows `x` for the rest of
        // the scope WITHOUT an explicit `return`, so the subsequent dereference
        // produces no warning. The control case (a normal void call in the
        // guard) does not narrow, so the dereference still warns.
        const auto buildScript = [](const char* source, MsgCapture& capture) -> int {
            auto engine = MakeScriptEngine();
            REQUIRE(nptr<asIScriptEngine> {engine});
            engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true);
            engine->RegisterObjectType("Resource", 0, asOBJ_REF | asOBJ_NOCOUNT);
            engine->RegisterObjectMethod("Resource", "void Touch()", FO_SCRIPT_FUNC_THIS(+[](void*) { }), FO_SCRIPT_FUNC_THIS_CONV);
            engine->RegisterGlobalFunction("Resource? MakeMaybe()", FO_SCRIPT_FUNC(+[]() -> void* { return nullptr; }), FO_SCRIPT_FUNC_CONV);
            engine->RegisterGlobalFunction("void Fatal()", FO_SCRIPT_FUNC(+[]() { }), FO_SCRIPT_FUNC_CONV);
            engine->RegisterGlobalFunction("void NormalVoid()", FO_SCRIPT_FUNC(+[]() { }), FO_SCRIPT_FUNC_CONV);
            // Mark Fatal() as no-return.
            for (asUINT i = 0, count = engine->GetGlobalFunctionCount(); i < count; i++) {
                nptr<asIScriptFunction> func = engine->GetGlobalFunctionByIndex(i);
                if (func && string_view(func->GetName()) == "Fatal") {
                    func->SetNoReturn();
                    CHECK(func->IsNoReturn());
                }
            }
            engine->SetMessageCallback(asFUNCTION(CaptureMessageCallback), &capture, asCALL_CDECL);
            nptr<asIScriptModule> module = engine->GetModule("NoReturnMod", asGM_ALWAYS_CREATE);
            REQUIRE(module);
            module->AddScriptSection("noreturn", source, std::strlen(source));
            return module->Build();
        };

        const auto hasDerefWarning = [](const MsgCapture& cap) -> bool {
            for (const auto& w : cap.Warnings) {
                if (w.find("Dereference of nullable handle") != string::npos) {
                    return true;
                }
            }
            return false;
        };

        // 1) Guard branch ends in a no-return call - narrows like `return`.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void GuardedByNoReturn()
                {
                    Resource? r = MakeMaybe();
                    if (r == null) {
                        Fatal();
                    }
                    r.Touch();
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK_FALSE(hasDerefWarning(cap));
        }

        // 2) No-return call without braces also narrows.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void GuardedByNoReturnNoBraces()
                {
                    Resource? r = MakeMaybe();
                    if (r == null)
                        Fatal();
                    r.Touch();
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK_FALSE(hasDerefWarning(cap));
        }

        // 3) Control: a normal (returning) void call does NOT narrow, so the
        //    later dereference still warns.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void GuardedByNormalCall()
                {
                    Resource? r = MakeMaybe();
                    if (r == null) {
                        NormalVoid();
                    }
                    r.Touch();
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK(hasDerefWarning(cap));
        }
    }

    SECTION("NegatedGuardNarrowsViaDeMorgan")
    {
        // A whole-condition negation `if (!(C)) <exit>` narrows
        // exactly like the inverted guard: `!(x != null)` behaves as
        // `x == null`, and `!(a != null && b != null)` behaves as
        // `a == null || b == null` (De Morgan). This is what lets the
        // `verify(cond, message)` macro (expanding to `if (!(cond)) throw(...)`)
        // keep narrowing.
        const auto buildScript = [](const char* source, MsgCapture& capture) -> int {
            auto engine = MakeScriptEngine();
            REQUIRE(nptr<asIScriptEngine> {engine});
            engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true);
            engine->RegisterObjectType("Resource", 0, asOBJ_REF | asOBJ_NOCOUNT);
            engine->RegisterObjectMethod("Resource", "void Touch()", FO_SCRIPT_FUNC_THIS(+[](void*) { }), FO_SCRIPT_FUNC_THIS_CONV);
            engine->RegisterGlobalFunction("Resource? MakeMaybe()", FO_SCRIPT_FUNC(+[]() -> void* { return nullptr; }), FO_SCRIPT_FUNC_CONV);
            engine->SetMessageCallback(asFUNCTION(CaptureMessageCallback), &capture, asCALL_CDECL);
            nptr<asIScriptModule> module = engine->GetModule("NegGuardMod", asGM_ALWAYS_CREATE);
            REQUIRE(module);
            module->AddScriptSection("neg_guard", source, std::strlen(source));
            return module->Build();
        };
        const auto hasDerefWarning = [](const MsgCapture& cap) -> bool {
            for (const auto& w : cap.Warnings) {
                if (w.find("Dereference of nullable handle") != string::npos) {
                    return true;
                }
            }
            return false;
        };

        // 1) `if (!(x != null)) return;` narrows like `if (x == null) return;`.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void NegSimple()
                {
                    Resource? r = MakeMaybe();
                    if (!(r != null)) {
                        return;
                    }
                    r.Touch();
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK_FALSE(hasDerefWarning(cap));
        }

        // 2) De Morgan: `if (!(a != null && b != null)) return;` narrows both.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void NegCompound()
                {
                    Resource? a = MakeMaybe();
                    Resource? b = MakeMaybe();
                    if (!(a != null && b != null)) {
                        return;
                    }
                    a.Touch();
                    b.Touch();
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK_FALSE(hasDerefWarning(cap));
        }

        // 3) Sanity: a partial `!(...)` that is NOT a whole-condition negation
        //    must not be mistaken for one - the unguarded deref still warns.
        {
            MsgCapture cap;
            const auto r = buildScript(R"(
                void NotWholeNegation()
                {
                    Resource? r = MakeMaybe();
                    bool flag = !(r != null) || true;
                    r.Touch();
                }
            )",
                cap);
            CHECK(r >= 0);
            CHECK(hasDerefWarning(cap));
        }
    }

    SECTION("SmartCastNarrowsNullableInsideGuard")
    {
        // Verify that `if (x != null) { ... }`, `if (x == null) return;`,
        // and `x != null && ...` narrow a `T?` local to `T` for the rest of the
        // guarded region. The body would otherwise need an explicit cast or Assert
        // to dereference the variable; smart-cast lets it be used as `T`.
        auto engine = MakeScriptEngine();
        REQUIRE(nptr<asIScriptEngine> {engine});
        RegisterTestApi(engine.get());

        static constexpr string_view SmartCastScript = R"(
class Resource
{
    int Id;
    Resource() { Id = AcquireResource(); }
    ~Resource() { if (Id != 0) ReleaseResource(Id); }
    int GetId() { return Id; }
}

Resource? GetMaybeResource(bool produce)
{
    if (produce) return Resource();
    return null;
}

// Guard 1: `if (x != null) { x.GetId(); }` narrows in then-branch.
void GuardedReadInThenBranch()
{
    Resource? optional = GetMaybeResource(true);
    if (optional != null) {
        // Inside this block `optional` is statically `Resource` (non-nullable).
        // Calling a method must compile without an Assert / explicit cast.
        Resource confirmed = optional;
        ObserveResource(confirmed.GetId());
    }
}

// Guard 2: `if (x == null) return;` narrows for the rest of the function body.
void GuardedReadAfterEarlyReturn()
{
    Resource? optional = GetMaybeResource(true);
    if (optional == null) {
        ObserveResource(-1);
        return;
    }
    // After the if-with-early-return, `optional` is non-null.
    Resource confirmed = optional;
    ObserveResource(confirmed.GetId());
}
)";

        nptr<asIScriptModule> module = engine->GetModule("SmartCastModule", asGM_ALWAYS_CREATE);
        REQUIRE(module);
        CHECK(module->AddScriptSection("smartcast", SmartCastScript.data(), numeric_cast<unsigned>(SmartCastScript.size())) >= 0);
        const auto build_result = module->Build();
        if (build_result < 0) {
            FAIL("Smart-cast script failed to build; the narrowed reads should compile without an Assert.");
        }
        REQUIRE(build_result >= 0);

        ResourceTrackerState state {};
        engine->SetUserData(&state);

        // Run guarded reads; neither should throw.
        for (const string_view decl : {string_view {"void GuardedReadInThenBranch()"}, string_view {"void GuardedReadAfterEarlyReturn()"}}) {
            nptr<asIScriptFunction> func = module->GetFunctionByDecl(decl.data());
            REQUIRE(func);
            nptr<asIScriptContext> ctx = engine->CreateContext();
            REQUIRE(ctx);
            CHECK(ctx->Prepare(func.get()) >= 0);
            const auto result = ctx->Execute();
            CHECK(result == asEXECUTION_FINISHED);
            ctx->Release();
        }

        engine->SetUserData(nullptr);
        CHECK(engine->GarbageCollect(asGC_FULL_CYCLE) >= 0);
    }

    SECTION("SmartCastNarrowsCompoundShapes")
    {
        // Two smart-cast shapes the compiler must narrow:
        //   1. `if (a != null && b != null) { ... }` â€" both narrow in then.
        //   2. `if (a == null || b == null) return;` â€" both narrow after.
        // The script body would otherwise need explicit Asserts or casts to
        // assign each nullable local to a non-nullable. Assert-based
        // narrowing was intentionally removed in favour of always-explicit
        // `if (x == null) { <recover>; return; }` shapes â€" production
        // callers benefit from a named recovery action over a generic
        // Assert.
        auto engine = MakeScriptEngine();
        REQUIRE(nptr<asIScriptEngine> {engine});
        RegisterTestApi(engine.get());

        static constexpr string_view CompoundSmartCastScript = R"(
class Resource
{
    int Id;
    Resource() { Id = AcquireResource(); }
    ~Resource() { if (Id != 0) ReleaseResource(Id); }
    int GetId() { return Id; }
}

Resource? Maybe(bool produce) { return produce ? Resource() : null; }

// 1) Compound && narrows every named atom in the then-branch.
void NarrowsCompoundAnd()
{
    Resource? a = Maybe(true);
    Resource? b = Maybe(true);
    if (a != null && b != null) {
        Resource ca = a;
        Resource cb = b;
        ObserveResource(ca.GetId() + cb.GetId());
    }
}

// 2) Compound || with early-return narrows every atom after the if.
void NarrowsCompoundOrAfterReturn()
{
    Resource? a = Maybe(true);
    Resource? b = Maybe(true);
    if (a == null || b == null) {
        ObserveResource(-7);
        return;
    }
    Resource ca = a;
    Resource cb = b;
    ObserveResource(ca.GetId() + cb.GetId());
}
)";

        nptr<asIScriptModule> module = engine->GetModule("CompoundSmartCast", asGM_ALWAYS_CREATE);
        REQUIRE(module);
        CHECK(module->AddScriptSection("compound_smartcast", CompoundSmartCastScript.data(), numeric_cast<unsigned>(CompoundSmartCastScript.size())) >= 0);
        const auto build_result = module->Build();
        if (build_result < 0) {
            FAIL("Compound smart-cast script failed to build; && / || narrowing should compile without an explicit cast.");
        }
        REQUIRE(build_result >= 0);

        ResourceTrackerState state {};
        engine->SetUserData(&state);

        for (const string_view decl : {string_view {"void NarrowsCompoundAnd()"}, string_view {"void NarrowsCompoundOrAfterReturn()"}}) {
            nptr<asIScriptFunction> func = module->GetFunctionByDecl(decl.data());
            REQUIRE(func);
            nptr<asIScriptContext> ctx = engine->CreateContext();
            REQUIRE(ctx);
            CHECK(ctx->Prepare(func.get()) >= 0);
            const auto result = ctx->Execute();
            CHECK(result == asEXECUTION_FINISHED);
            ctx->Release();
        }

        engine->SetUserData(nullptr);
        CHECK(engine->GarbageCollect(asGC_FULL_CYCLE) >= 0);
    }

    SECTION("SmartCastInvalidatedByAssignment")
    {
        // Smart-cast must drop the narrowing as soon as the
        // narrowed local is reassigned. The new value's static type is again
        // `T?`, so the next `T x = local;` must be rejected at compile time
        // (with `asEP_DISALLOW_NULLABLE_TO_NON_NULLABLE` on).
        const auto buildScript = [](const char* source, MsgCapture& capture) -> int {
            auto engine = MakeScriptEngine();
            REQUIRE(nptr<asIScriptEngine> {engine});
            engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true);
            engine->SetEngineProperty(asEP_DISALLOW_NULLABLE_TO_NON_NULLABLE, true);
            engine->RegisterObjectType("Resource", 0, asOBJ_REF | asOBJ_NOCOUNT);
            engine->SetMessageCallback(asFUNCTION(CaptureMessageCallback), &capture, asCALL_CDECL);
            nptr<asIScriptModule> module = engine->GetModule("InvalidateMod", asGM_ALWAYS_CREATE);
            REQUIRE(module);
            module->AddScriptSection("invalidate", source, std::strlen(source));
            return module->Build();
        };

        // After a reassignment to a nullable expression, the narrowed type
        // must reset so the next read produces a `T?`.
        MsgCapture cap;
        const auto r = buildScript(R"(
            Resource? GetMaybe() { return null; }
            void NarrowingResetsAfterAssign()
            {
                Resource? g = GetMaybe();
                if (g != null) {
                    Resource a = g;            // OK â€" narrowed.
                    g = GetMaybe();            // Reassign: drops narrowing.
                    Resource b = g;            // Must fail â€" g is `Resource?` again.
                }
            }
        )",
            cap);
        CHECK(r < 0);
        bool found = false;
        for (const auto& e : cap.Errors) {
            if (e.find("Cannot assign nullable") != string::npos) {
                found = true;
                break;
            }
        }
        CHECK(found);

        // A `null` literal reassignment drops the narrowing the same way - and the
        // write itself must compile: it goes through the declared nullable type
        // (read-time narrowing must not turn it into a non-nullable null write).
        {
            MsgCapture null_cap;
            const auto null_r = buildScript(R"(
                Resource? GetMaybe() { return null; }
                void NarrowingResetsAfterNullAssign()
                {
                    Resource? g = GetMaybe();
                    if (g != null) {
                        Resource a = g;        // OK - narrowed.
                        g = null;              // Un-narrowing write: legal.
                        Resource b = g;        // Must fail - g is `Resource?` again.
                    }
                }
            )",
                null_cap);
            CHECK(null_r < 0);
            bool found_bind_error = false;
            for (const auto& e : null_cap.Errors) {
                if (e.find("Cannot assign nullable") != string::npos) {
                    found_bind_error = true;
                }
                // The null write itself must not be rejected.
                CHECK(e.find("Cannot assign 'null'") == string::npos);
            }
            CHECK(found_bind_error);
        }
    }

    SECTION("NullAssignmentToNarrowedNullableUnNarrows")
    {
        // A write goes through the variable's *declared* type: an active smart-cast
        // narrowing is a read-time refinement only. `x = null;` on a declared-nullable
        // local (or `&` parameter) inside its own `x != null` guard is a legal
        // un-narrowing write compiled as a plain REFCPY. Regression: the copy
        // instruction used to be chosen from the narrowed (non-nullable) view, which
        // emitted asBC_RefCpyChk and threw "Null assignment to non-nullable handle"
        // on every execution of the branch.
        auto engine = MakeScriptEngine();
        REQUIRE(nptr<asIScriptEngine> {engine});
        RegisterTestApi(engine.get());
        // The `Resource? &` inout-parameter flavor needs unsafe references, same as the
        // game backend (AngelScriptBackend.cpp) which enables them for all scripts.
        CHECK(engine->SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES, true) >= 0);

        static constexpr string_view UnNarrowScript = R"(
class Resource
{
    int Id;
    Resource() { Id = AcquireResource(); }
    ~Resource() { if (Id != 0) ReleaseResource(Id); }
}

// Local flavor: simple guard.
void NullAssignToNarrowedLocal()
{
    Resource? slot = Resource();
    if (slot != null) {
        slot = null;
    }
    ObserveResource(slot == null ? 1 : 0);
}

// Local flavor: compound `&&` guard with a member read - the deferred-attack
// "target died during hit delay" shape that hit this defect in production.
void NullAssignToCompoundNarrowedLocal()
{
    Resource? slot = Resource();
    if (slot != null && slot.Id > 0) {
        slot = null;
    }
    ObserveResource(slot == null ? 1 : 0);
}

// Ref-parameter flavor: the callee clears the caller's slot through `&`.
void ClearSlot(Resource? & slot)
{
    if (slot != null) {
        slot = null;
    }
}

void NullAssignToNarrowedRefParam()
{
    Resource? slot = Resource();
    ClearSlot(slot);
    ObserveResource(slot == null ? 1 : 0);
}
)";

        nptr<asIScriptModule> module = engine->GetModule("UnNarrowModule", asGM_ALWAYS_CREATE);
        REQUIRE(module);
        REQUIRE(module->AddScriptSection("un_narrow", UnNarrowScript.data(), numeric_cast<unsigned>(UnNarrowScript.size())) >= 0);
        REQUIRE(module->Build() >= 0);

        ResourceTrackerState state {};
        engine->SetUserData(&state);

        for (const string_view decl : {string_view {"void NullAssignToNarrowedLocal()"}, string_view {"void NullAssignToCompoundNarrowedLocal()"}, string_view {"void NullAssignToNarrowedRefParam()"}}) {
            state.LastObservedId = 0;
            nptr<asIScriptFunction> func = module->GetFunctionByDecl(decl.data());
            REQUIRE(func);
            nptr<asIScriptContext> ctx = engine->CreateContext();
            REQUIRE(ctx);
            CHECK(ctx->Prepare(func.get()) >= 0);
            const auto result = ctx->Execute();
            UNSCOPED_INFO(decl);
            if (result == asEXECUTION_EXCEPTION) {
                UNSCOPED_INFO(ctx->GetExceptionString());
            }
            CHECK(result == asEXECUTION_FINISHED);
            ctx->Release();
            // The branch body ran to completion and the slot observes as null.
            CHECK(state.LastObservedId == 1);
        }

        engine->SetUserData(nullptr);
        CHECK(engine->GarbageCollect(asGC_FULL_CYCLE) >= 0);
    }

    SECTION("SmartCastDoesNotNarrowMixedAndOr")
    {
        // Mixed `&&` and `||` at the same precedence level
        // is ambiguous without operator-precedence parsing. The smart-cast
        // detector keeps both narrow-lists empty for such conditions; the
        // user is expected to split the if. Verify the build rejects the
        // un-guarded read so the analyzer stays conservative.
        const auto buildScript = [](const char* source, MsgCapture& capture) -> int {
            auto engine = MakeScriptEngine();
            REQUIRE(nptr<asIScriptEngine> {engine});
            engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true);
            engine->SetEngineProperty(asEP_DISALLOW_NULLABLE_TO_NON_NULLABLE, true);
            engine->RegisterObjectType("Resource", 0, asOBJ_REF | asOBJ_NOCOUNT);
            engine->RegisterGlobalFunction("bool Flag()", FO_SCRIPT_FUNC(+[]() -> bool { return true; }), FO_SCRIPT_FUNC_CONV);
            engine->SetMessageCallback(asFUNCTION(CaptureMessageCallback), &capture, asCALL_CDECL);
            nptr<asIScriptModule> module = engine->GetModule("MixedMod", asGM_ALWAYS_CREATE);
            REQUIRE(module);
            module->AddScriptSection("mixed", source, std::strlen(source));
            return module->Build();
        };

        MsgCapture cap;
        const auto r = buildScript(R"(
            Resource? GetMaybe() { return null; }
            void MixedAndOrDoesNotNarrow()
            {
                Resource? g = GetMaybe();
                if (g != null && Flag() || g != null) {
                    Resource x = g; // Must fail â€" mixed shape is conservative.
                }
            }
        )",
            cap);
        CHECK(r < 0);
        bool found = false;
        for (const auto& e : cap.Errors) {
            if (e.find("Cannot assign nullable") != string::npos) {
                found = true;
                break;
            }
        }
        CHECK(found);
    }

    SECTION("HandleEqualityFallsBackToIdentity")
    {
        // For ref types without an `opEquals` method, `==`
        // and `!=` between two handles fall back to handle-identity
        // comparison (asBC_CmpPtr), identical in semantics to `is` / `!is`.
        // This lets the project drop `is` / `!is` from the style guide.
        auto engine = MakeScriptEngine();
        REQUIRE(nptr<asIScriptEngine> {engine});
        RegisterTestApi(engine.get());

        static constexpr string_view IdentityScript = R"(
class Node { int Tag; Node(int t) { Tag = t; } }

// Same handle assigned both sides â€" must compare equal.
void SameHandle()
{
    Node n = Node(7);
    Node m = n;
    ObserveResource(m == n ? 1 : 0);
    ObserveResource(m != n ? 0 : 2);
}

// Two freshly-constructed Node instances â€" must compare unequal even
// though their `Tag` fields are identical (no opEquals registered).
void DistinctHandles()
{
    Node a = Node(7);
    Node b = Node(7);
    ObserveResource(a == b ? 0 : 3);
    ObserveResource(a != b ? 4 : 0);
}
)";

        nptr<asIScriptModule> module = engine->GetModule("Identity", asGM_ALWAYS_CREATE);
        REQUIRE(module);
        CHECK(module->AddScriptSection("identity", IdentityScript.data(), numeric_cast<unsigned>(IdentityScript.size())) >= 0);
        const auto build_result = module->Build();
        if (build_result < 0) {
            FAIL("Handle-identity fallback script failed to build; `==` between two `Node` handles should fall through to asBC_CmpPtr.");
        }
        REQUIRE(build_result >= 0);

        ResourceTrackerState state {};
        engine->SetUserData(&state);

        // Sequence of ObserveResource calls produces 1,2 from SameHandle and 3,4 from DistinctHandles.
        for (const string_view decl : {string_view {"void SameHandle()"}, string_view {"void DistinctHandles()"}}) {
            nptr<asIScriptFunction> func = module->GetFunctionByDecl(decl.data());
            REQUIRE(func);
            nptr<asIScriptContext> ctx = engine->CreateContext();
            REQUIRE(ctx);
            CHECK(ctx->Prepare(func.get()) >= 0);
            const auto result = ctx->Execute();
            CHECK(result == asEXECUTION_FINISHED);
            ctx->Release();
        }
        // LastObservedId must be 4 (last ObserveResource in DistinctHandles).
        CHECK(state.LastObservedId == 4);

        engine->SetUserData(nullptr);
        CHECK(engine->GarbageCollect(asGC_FULL_CYCLE) >= 0);
    }

    SECTION("HandleEqualityUsesOpEqualsWhenRegistered")
    {
        // When the ref type DOES register `opEquals`, the
        // overloaded version still wins over the identity fallback. This
        // is what makes `Critter == Critter` an id-based comparison in
        // production code while `Gui::Screen == Gui::Screen` is identity.
        auto engine = MakeScriptEngine();
        REQUIRE(nptr<asIScriptEngine> {engine});
        RegisterTestApi(engine.get());

        static constexpr string_view OpEqualsScript = R"(
class Tagged
{
    int Tag;
    Tagged(int t) { Tag = t; }
    bool opEquals(const Tagged &in other) const
    {
        return Tag == other.Tag;
    }
}

void TagsEqual()
{
    Tagged a = Tagged(7);
    Tagged b = Tagged(7);   // Distinct handles, same tag.
    // opEquals matches â€" `==` should be true.
    ObserveResource(a == b ? 11 : 0);
    ObserveResource(a != b ? 0 : 12);
}

void TagsDiffer()
{
    Tagged a = Tagged(7);
    Tagged b = Tagged(8);
    ObserveResource(a == b ? 0 : 13);
    ObserveResource(a != b ? 14 : 0);
}
)";

        nptr<asIScriptModule> module = engine->GetModule("OpEquals", asGM_ALWAYS_CREATE);
        REQUIRE(module);
        CHECK(module->AddScriptSection("op_equals", OpEqualsScript.data(), numeric_cast<unsigned>(OpEqualsScript.size())) >= 0);
        const auto build_result = module->Build();
        if (build_result < 0) {
            FAIL("opEquals dispatch script failed to build.");
        }
        REQUIRE(build_result >= 0);

        ResourceTrackerState state {};
        engine->SetUserData(&state);

        for (const string_view decl : {string_view {"void TagsEqual()"}, string_view {"void TagsDiffer()"}}) {
            nptr<asIScriptFunction> func = module->GetFunctionByDecl(decl.data());
            REQUIRE(func);
            nptr<asIScriptContext> ctx = engine->CreateContext();
            REQUIRE(ctx);
            CHECK(ctx->Prepare(func.get()) >= 0);
            const auto result = ctx->Execute();
            CHECK(result == asEXECUTION_FINISHED);
            ctx->Release();
        }
        // Sequence: 11, 12, 13, 14 â€" all opEquals branches reached.
        CHECK(state.LastObservedId == 14);

        engine->SetUserData(nullptr);
        CHECK(engine->GarbageCollect(asGC_FULL_CYCLE) >= 0);
    }

    SECTION("NullableMarkerOnReturnAndParameter")
    {
        // The `?` suffix must parse on the return type and
        // every parameter type of a script function, producing a nullable
        // handle that the body and the caller can assign null to without a
        // runtime exception. Funcdef and array shapes are exercised by the
        // gameplay test suite where the array add-on is registered.
        auto engine = MakeScriptEngine();
        REQUIRE(nptr<asIScriptEngine> {engine});
        RegisterTestApi(engine.get());

        static constexpr string_view NullableShapesScript = R"(
class Resource
{
    int Id;
    Resource() { Id = AcquireResource(); }
    ~Resource() { if (Id != 0) ReleaseResource(Id); }
}

// `?` on the return type lets the body return null.
Resource? MakeNull() { return null; }

// `?` on a parameter type lets the caller pass null cleanly.
void RecordsNull(Resource? r) { ObserveResource(r == null ? 24 : 0); }

void NullableReturn()
{
    Resource? r = MakeNull();
    ObserveResource(r == null ? 23 : 0);
}

void NullableParameter()
{
    RecordsNull(null);
}
)";

        nptr<asIScriptModule> module = engine->GetModule("NullableShapes", asGM_ALWAYS_CREATE);
        REQUIRE(module);
        CHECK(module->AddScriptSection("nullable_shapes", NullableShapesScript.data(), numeric_cast<unsigned>(NullableShapesScript.size())) >= 0);
        const auto build_result = module->Build();
        if (build_result < 0) {
            FAIL("Nullable return / parameter parse test failed to build.");
        }
        REQUIRE(build_result >= 0);

        ResourceTrackerState state {};
        engine->SetUserData(&state);

        for (const string_view decl : {string_view {"void NullableReturn()"}, string_view {"void NullableParameter()"}}) {
            nptr<asIScriptFunction> func = module->GetFunctionByDecl(decl.data());
            REQUIRE(func);
            nptr<asIScriptContext> ctx = engine->CreateContext();
            REQUIRE(ctx);
            CHECK(ctx->Prepare(func.get()) >= 0);
            const auto result = ctx->Execute();
            CHECK(result == asEXECUTION_FINISHED);
            ctx->Release();
        }
        // Last call: NullableParameter records 24.
        CHECK(state.LastObservedId == 24);

        engine->SetUserData(nullptr);
        CHECK(engine->GarbageCollect(asGC_FULL_CYCLE) >= 0);
    }

    SECTION("NullableHandleFieldRejectsImplicitInit")
    {
        // A class field declared with a non-nullable handle
        // type AND an explicit `= null` initializer is the most common
        // pre-strict-mode bug. The compile-time check must reject this even
        // before runtime, because the field would otherwise initialize a
        // non-nullable slot to null at constructor time.
        const auto buildScript = [](const char* source, MsgCapture& capture) -> int {
            auto engine = MakeScriptEngine();
            REQUIRE(nptr<asIScriptEngine> {engine});
            engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true);
            engine->RegisterObjectType("Resource", 0, asOBJ_REF | asOBJ_NOCOUNT);
            engine->SetMessageCallback(asFUNCTION(CaptureMessageCallback), &capture, asCALL_CDECL);
            nptr<asIScriptModule> module = engine->GetModule("FieldNull", asGM_ALWAYS_CREATE);
            REQUIRE(module);
            module->AddScriptSection("field_null", source, std::strlen(source));
            return module->Build();
        };

        // Field with `= null` on a non-nullable handle â€" must reject.
        MsgCapture cap;
        const auto r = buildScript(R"(
            class Holder
            {
                Resource Slot = null;
            }
        )",
            cap);
        CHECK(r < 0);
        bool found = false;
        for (const auto& e : cap.Errors) {
            if (e.find("Cannot assign 'null' to a non-nullable handle") != string::npos) {
                found = true;
                break;
            }
        }
        CHECK(found);
    }

    SECTION("NullableHandleAcceptsNullField")
    {
        // The same field with `T?` accepts the `= null`
        // initializer and is observable as null at runtime. Holder itself
        // is a non-ref class (engine runs with implicit-handle on, so we
        // instantiate it via `Holder()` instead of bare `Holder h;` to
        // avoid value-vs-handle-default-construct ambiguity in the test
        // harness).
        auto engine = MakeScriptEngine();
        REQUIRE(nptr<asIScriptEngine> {engine});
        RegisterTestApi(engine.get());

        static constexpr string_view NullableFieldScript = R"(
class Resource
{
    int Id;
    Resource() { Id = AcquireResource(); }
    ~Resource() { if (Id != 0) ReleaseResource(Id); }
}

class Holder
{
    Resource? Slot = null;
}

void Run()
{
    Holder h = Holder();
    ObserveResource(h.Slot == null ? 31 : -1);
    // Assigning a fresh Resource flips the slot to non-null.
    h.Slot = Resource();
    ObserveResource(h.Slot != null ? 32 : -2);
    // Assigning null is allowed because the field is nullable.
    h.Slot = null;
    ObserveResource(h.Slot == null ? 33 : -3);
}
)";

        nptr<asIScriptModule> module = engine->GetModule("FieldNullable", asGM_ALWAYS_CREATE);
        REQUIRE(module);
        CHECK(module->AddScriptSection("field_nullable", NullableFieldScript.data(), numeric_cast<unsigned>(NullableFieldScript.size())) >= 0);
        CHECK(module->Build() >= 0);

        ResourceTrackerState state {};
        engine->SetUserData(&state);

        nptr<asIScriptFunction> func = module->GetFunctionByDecl("void Run()");
        REQUIRE(func);
        nptr<asIScriptContext> ctx = engine->CreateContext();
        REQUIRE(ctx);
        CHECK(ctx->Prepare(func.get()) >= 0);
        CHECK(ctx->Execute() == asEXECUTION_FINISHED);
        ctx->Release();
        CHECK(state.LastObservedId == 33);

        engine->SetUserData(nullptr);
        CHECK(engine->GarbageCollect(asGC_FULL_CYCLE) >= 0);
    }

    SECTION("CalleeLocalAssignCatchesPassedNull")
    {
        // The runtime null check fires on
        // *handle assignment / initialization*, not on script-to-script
        // argument passing (the latter uses copy-on-call patterns that
        // bypass the `asBC_RefCpyChk` slot). The agreed contract:
        //
        //   * Script callee with `T param` and `T local = param;` â€"
        //     the local-assignment slot raises "Null assignment to
        //     non-nullable handle" if the caller passed null.
        //   * Script callee with `T? param` â€" no exception, param holds
        //     null, the callee branches on it normally.
        //
        // Document the contract by exercising both shapes.
        auto engine = MakeScriptEngine();
        REQUIRE(nptr<asIScriptEngine> {engine});
        RegisterTestApi(engine.get());

        static constexpr string_view ParamNullScript = R"(
class Resource
{
    int Id;
    Resource() { Id = AcquireResource(); }
    ~Resource() { if (Id != 0) ReleaseResource(Id); }
}

// Callee declares non-nullable param then immediately copies it to
// another non-nullable local: the local-assign slot is what
// asBC_RefCpyChk protects.
void CalleeCopiesParamToLocal(Resource r)
{
    Resource local = r;
    ObserveResource(-998);
}

// Callee declares nullable param: no runtime check expected.
void CalleeNullableParam(Resource? r)
{
    ObserveResource(r == null ? 41 : -1);
}

void CallCalleeWithNullThroughLocal()
{
    Resource? maybe = null;
    CalleeCopiesParamToLocal(maybe);
    ObserveResource(-997);
}

void CallCalleeNullableWithNull()
{
    Resource? maybe = null;
    CalleeNullableParam(maybe);
}
)";

        nptr<asIScriptModule> module = engine->GetModule("ParamNull", asGM_ALWAYS_CREATE);
        REQUIRE(module);
        CHECK(module->AddScriptSection("param_null", ParamNullScript.data(), numeric_cast<unsigned>(ParamNullScript.size())) >= 0);
        CHECK(module->Build() >= 0);

        ResourceTrackerState state {};
        engine->SetUserData(&state);

        // Callee's `Resource local = r;` slot must throw when r is null.
        {
            nptr<asIScriptFunction> func = module->GetFunctionByDecl("void CallCalleeWithNullThroughLocal()");
            REQUIRE(func);
            nptr<asIScriptContext> ctx = engine->CreateContext();
            REQUIRE(ctx);
            CHECK(ctx->Prepare(func.get()) >= 0);
            CHECK(ctx->Execute() == asEXECUTION_EXCEPTION);
            nptr<const char> ex = ctx->GetExceptionString();
            REQUIRE(ex);
            CHECK(string_view {ex.get()} == "Null assignment to non-nullable handle");
            CHECK(state.LastObservedId != -998);
            CHECK(state.LastObservedId != -997);
            ctx->Release();
        }

        // Nullable param accepts null cleanly.
        {
            state.LastObservedId = 0;
            nptr<asIScriptFunction> func = module->GetFunctionByDecl("void CallCalleeNullableWithNull()");
            REQUIRE(func);
            nptr<asIScriptContext> ctx = engine->CreateContext();
            REQUIRE(ctx);
            CHECK(ctx->Prepare(func.get()) >= 0);
            CHECK(ctx->Execute() == asEXECUTION_FINISHED);
            CHECK(state.LastObservedId == 41);
            ctx->Release();
        }

        engine->SetUserData(nullptr);
        CHECK(engine->GarbageCollect(asGC_FULL_CYCLE) >= 0);
    }

    SECTION("GetRefOffsetAdjustmentWithMultiplePtrSizedArgs")
    {
        // Regression test for AdjustGetOffset REFCPY early-return bug.
        // On 64-bit build Ã¢â€ â€™ 32-bit load, GETREF offsets for reference parameters
        // past multiple pointer-sized stack items were miscalculated, causing crashes.
        // The fix guards the REFCPY shortcut with offset == 1 (reader) / offset == AS_PTR_SIZE (writer).

        auto build_engine = MakeScriptEngine();
        REQUIRE(nptr<asIScriptEngine> {build_engine});
        RegisterTestApi(build_engine.get());

        auto build_module = BuildModuleFromScript(build_engine.get(), "BuildModule", GetRefOffsetTestScript);

        ResourceTrackerState build_state {};
        const auto build_metrics = RunScenario(build_module, build_state);

        // Verify build-time execution produces expected values
        // Sequence: 52, 7, 150, 50, 67, 42
        CHECK(build_metrics.FirstObservedId == 52);
        CHECK(build_metrics.LastObservedId == 42);
        CHECK(build_metrics.DoubleFreeCount == 0);
        CHECK(build_metrics.LeakedCount == 0);

        // Save bytecode in portable format
        vector<asBYTE> bytecode {};
        BytecodeStream writer {bytecode};
        CHECK(build_module->SaveByteCode(&writer) >= 0);
        REQUIRE_FALSE(bytecode.empty());

        // Load in fresh engine (same platform, but exercises the full save/load path
        // including AdjustGetOffset normalization and denormalization)
        auto load_engine = MakeScriptEngine();
        REQUIRE(nptr<asIScriptEngine> {load_engine});
        RegisterTestApi(load_engine.get());

        nptr<asIScriptModule> load_module = load_engine->GetModule("LoadModule", asGM_ALWAYS_CREATE);
        REQUIRE(load_module);
        BytecodeStream reader {bytecode};
        CHECK(load_module->LoadByteCode(&reader) >= 0);

        // Execute loaded bytecode and verify identical results
        ResourceTrackerState load_state {};
        const auto load_metrics = RunScenario(load_module.as_ptr(), load_state);

        CHECK(load_metrics.FirstObservedId == build_metrics.FirstObservedId);
        CHECK(load_metrics.LastObservedId == build_metrics.LastObservedId);
        CHECK(load_metrics.DoubleFreeCount == build_metrics.DoubleFreeCount);
        CHECK(load_metrics.LeakedCount == build_metrics.LeakedCount);
    }
}

TEST_CASE("AngelScriptInitListSubDwordValueType", "[angelscript][bytecode]")
{
    // Regression for the init-list buffer overflow with a value type smaller than a
    // dword (FOnline Patch in as_compiler.cpp's CompileListConstructor). The compiler
    // sized the list buffer as count(4) + sum of EXACT element sizes, but each
    // value-type element is written with asBC_COPY using the dword-rounded size; for a
    // sub-dword element the final copy spilled past the buffer end — a heap-buffer-
    // overflow caught by AddressSanitizer. The fix rounds the buffer up to a dword.
    //
    // A 2-byte value type with 3 elements yields a 4 + 3*2 = 10-byte buffer; the last
    // element's dword-rounded (4-byte) copy lands at offset 8..11 and previously
    // overflowed at offset 10..11.
    using namespace AngelScript;

    auto engine = MakeScriptEngine();
    RegisterTestApi(engine.get());
    RegisterAngelScriptArray(engine.get());

    // 2-byte POD value type (sub-dword) — the trigger condition for the overflow.
    CHECK(engine->RegisterObjectType("Word2", 2, asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS | asOBJ_APP_CLASS_ALLINTS) >= 0);

    static constexpr string_view InitListScript = R"(
int RunInitList()
{
    array<Word2> a = {Word2(), Word2(), Word2()};
    array<Word2> b = {Word2(), Word2(), Word2(), Word2(), Word2()};
    return int(a.length()) + int(b.length());
}
)";

    auto* module = engine->GetModule("InitListModule", asGM_ALWAYS_CREATE);
    REQUIRE(module != nullptr);
    CHECK(module->AddScriptSection("initlist_test", InitListScript.data(), numeric_cast<unsigned>(InitListScript.size())) >= 0);
    REQUIRE(module->Build() >= 0);

    auto* func = module->GetFunctionByDecl("int RunInitList()");
    REQUIRE(func != nullptr);

    auto* ctx = engine->CreateContext();
    REQUIRE(ctx != nullptr);
    CHECK(ctx->Prepare(func) >= 0);
    REQUIRE(ctx->Execute() == asEXECUTION_FINISHED);
    CHECK(ctx->GetReturnDWord() == 8U);
    ctx->Release();
}

FO_END_NAMESPACE
