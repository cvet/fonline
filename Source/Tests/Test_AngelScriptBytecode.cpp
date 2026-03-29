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
        _buf {buf}
    {
    }

    auto Read(void* ptr, asUINT size) -> int override
    {
        if (ptr == nullptr || size == 0) {
            return 0;
        }

        if (_readPos + size > _buf.size()) {
            return -1;
        }

        MemCopy(ptr, _buf.data() + _readPos, size);
        _readPos += size;
        return 0;
    }

    auto Write(const void* ptr, asUINT size) -> int override
    {
        if (ptr == nullptr || size == 0) {
            return 0;
        }

        _buf.resize(_writePos + size);
        MemCopy(_buf.data() + _writePos, ptr, size);
        _writePos += size;
        return 0;
    }

private:
    vector<asBYTE>& _buf;
    size_t _readPos {};
    size_t _writePos {};
};

static auto GetTrackerState(asIScriptEngine* engine) -> ResourceTrackerState&
{
    auto* state = static_cast<ResourceTrackerState*>(engine->GetUserData());
    FO_RUNTIME_ASSERT(state != nullptr);
    return *state;
}

static auto GetActiveEngine() -> asIScriptEngine*
{
    auto* ctx = asGetActiveContext();
    FO_RUNTIME_ASSERT(ctx != nullptr);
    auto* engine = ctx->GetEngine();
    FO_RUNTIME_ASSERT(engine != nullptr);
    return engine;
}

static auto AcquireResource() -> int
{
    auto& state = GetTrackerState(GetActiveEngine());
    const int id = ++state.NextId;
    state.ActiveRefs[id] = 1;
    return id;
}

static void ReleaseResource(int id)
{
    auto& state = GetTrackerState(GetActiveEngine());
    const auto it = state.ActiveRefs.find(id);

    if (it == state.ActiveRefs.end() || it->second == 0) {
        state.DoubleFreeCount++;
        return;
    }

    it->second--;
    if (it->second == 0) {
        state.ActiveRefs.erase(it);
    }
}

static void ObserveResource(int id)
{
    auto& state = GetTrackerState(GetActiveEngine());
    if (state.FirstObservedId == 0) {
        state.FirstObservedId = id;
    }
    state.LastObservedId = id;
}

// PtrSizedVal is a value type whose size equals sizeof(void*), simulating hstring.
// On 64-bit it occupies 8 bytes (2 DWORDs), on 32-bit it occupies 4 bytes (1 DWORD).
// This exercises the portable bytecode format's stack normalization for platform-dependent value types.
struct PtrSizedVal
{
    uintptr_t Value {};
};

static_assert(sizeof(PtrSizedVal) == sizeof(void*));

static auto PtrSizedVal_CreateFromInt(int v) -> PtrSizedVal { return PtrSizedVal {static_cast<uintptr_t>(v)}; }
static auto PtrSizedVal_GetInt(const PtrSizedVal& v) -> int { return static_cast<int>(v.Value); }
static auto PtrSizedVal_OpEquals(const PtrSizedVal& a, const PtrSizedVal& b) -> bool { return a.Value == b.Value; }

static int GlobalInt = 0;

static void SetGlobalInt(int v) { GlobalInt = v; }
static auto GetGlobalInt() -> int { return GlobalInt; }

static void IncrementGlobalCounter()
{
    auto& state = GetTrackerState(GetActiveEngine());
    state.GlobalCounter++;
}

static void MessageCallback(const asSMessageInfo* msg, void* param)
{
    static_cast<void>(param);

    if (msg->section == nullptr || msg->section[0] == '\0') {
        return;
    }

    const auto section = msg->section != nullptr ? msg->section : "<unknown>";
    const auto text = msg->message != nullptr ? msg->message : "<no message>";
    FAIL(section << ':' << msg->row << ' ' << text);
}

static void RegisterTestApi(asIScriptEngine* engine)
{
    CHECK(engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true) >= 0);
    CHECK(engine->SetEngineProperty(asEP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE, true) >= 0);
    CHECK(engine->SetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_CONSTRUCT, true) >= 0);
    CHECK(engine->SetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_COPY, 2) >= 0);
    CHECK(engine->SetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_COPY_CONSTRUCT, 2) >= 0);
    CHECK(engine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE, false) >= 0);
    CHECK(engine->SetMessageCallback(asFUNCTION(MessageCallback), nullptr, asCALL_CDECL) >= 0);
    CHECK(engine->RegisterGlobalFunction("int AcquireResource()", asFUNCTION(AcquireResource), asCALL_CDECL) >= 0);
    CHECK(engine->RegisterGlobalFunction("void ReleaseResource(int)", asFUNCTION(ReleaseResource), asCALL_CDECL) >= 0);
    CHECK(engine->RegisterGlobalFunction("void ObserveResource(int)", asFUNCTION(ObserveResource), asCALL_CDECL) >= 0);

    // Register pointer-sized value type (like hstring)
    // POD with APP_CLASS + APP_CLASS_ALLINTS: engine handles construct/destruct/copy automatically
    CHECK(engine->RegisterObjectType("PtrSizedVal", sizeof(PtrSizedVal), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS | asOBJ_APP_CLASS_ALLINTS) >= 0);
    CHECK(engine->RegisterObjectMethod("PtrSizedVal", "bool opEquals(const PtrSizedVal &in) const", asFUNCTION(PtrSizedVal_OpEquals), asCALL_CDECL_OBJFIRST) >= 0);
    CHECK(engine->RegisterGlobalFunction("PtrSizedVal CreatePtrSizedVal(int)", asFUNCTION(PtrSizedVal_CreateFromInt), asCALL_CDECL) >= 0);
    CHECK(engine->RegisterGlobalFunction("int GetPtrSizedValInt(const PtrSizedVal &in)", asFUNCTION(PtrSizedVal_GetInt), asCALL_CDECL) >= 0);

    // Register global variable (exercises PGA/LDG/PshG4/CpyGtoV4/CpyVtoG4/SetG4 instructions)
    GlobalInt = 0;
    CHECK(engine->RegisterGlobalProperty("int GlobalInt", &GlobalInt) >= 0);
    CHECK(engine->RegisterGlobalFunction("void SetGlobalInt(int)", asFUNCTION(SetGlobalInt), asCALL_CDECL) >= 0);
    CHECK(engine->RegisterGlobalFunction("int GetGlobalInt()", asFUNCTION(GetGlobalInt), asCALL_CDECL) >= 0);
    CHECK(engine->RegisterGlobalFunction("void IncrementGlobalCounter()", asFUNCTION(IncrementGlobalCounter), asCALL_CDECL) >= 0);
}

static auto RunScenario(asIScriptModule* module, ResourceTrackerState& state) -> ScenarioMetrics
{
    auto* engine = module->GetEngine();
    engine->SetUserData(&state);

    auto* func = module->GetFunctionByDecl("void RunScenario()");
    REQUIRE(func != nullptr);

    auto* ctx = engine->CreateContext();
    REQUIRE(ctx != nullptr);

    CHECK(ctx->Prepare(func) >= 0);
    const auto exec_result = ctx->Execute();
    if (exec_result != asEXECUTION_FINISHED) {
        const char* section = nullptr;
        int column = 0;
        const auto line = ctx->GetExceptionLineNumber(&column, &section);
        const auto* exception_func = ctx->GetExceptionFunction();
        const auto* exception_decl = exception_func != nullptr ? exception_func->GetDeclaration() : "<unknown>";
        const auto* exception_text = ctx->GetExceptionString();

        FAIL("Execute failed: result=" << exec_result << ", section=" << (section != nullptr ? section : "<unknown>") << ", line=" << line << ", column=" << column << ", function=" << exception_decl << ", exception=" << (exception_text != nullptr ? exception_text : "<none>") << ", first_observed=" << state.FirstObservedId << ", last_observed=" << state.LastObservedId);
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
// - PtrSizedVal (platform-dependent value type on stack) → COPY, stack adjustment
// - Global property access → PGA, LDG, PshG4, CpyGtoV4, CpyVtoG4, SetG4
// - Mixed-type function parameters → ALLOC, FREE, REFCPY, various GET instructions
// - Function pointers → FuncPtr, CallPtr
// - Multiple value-type locals → stack layout normalization
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

    // Method call: this + PtrSizedVal ref + int ref → GETREF offset bug scenario
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
    ObserveResource(chained);  // key5==Fallback → defVal = lk.Get(key1, 0) = 42+0 = 42
}
)";

static auto BuildModule(asIScriptEngine* engine, const string& module_name) -> asIScriptModule*
{
    auto* module = engine->GetModule(module_name.c_str(), asGM_ALWAYS_CREATE);
    REQUIRE(module != nullptr);
    CHECK(module->AddScriptSection("bytecode_test", TestScript.data(), numeric_cast<unsigned>(TestScript.size())) >= 0);
    CHECK(module->Build() >= 0);
    return module;
}

static auto BuildModuleFromScript(asIScriptEngine* engine, const string& module_name, string_view script) -> asIScriptModule*
{
    auto* module = engine->GetModule(module_name.c_str(), asGM_ALWAYS_CREATE);
    REQUIRE(module != nullptr);
    CHECK(module->AddScriptSection("bytecode_test", script.data(), numeric_cast<unsigned>(script.size())) >= 0);
    CHECK(module->Build() >= 0);
    return module;
}
}

TEST_CASE("AngelScriptBytecode", "[.][angelscript][bytecode]")
{
    SECTION("SaveLoadPreservesGeneratedScriptCopyBehavior")
    {
        unique_del_ptr<asIScriptEngine> build_engine {asCreateScriptEngine(), [](asIScriptEngine* engine) {
            if (engine != nullptr) {
                engine->ShutDownAndRelease();
            }
        }};
        REQUIRE(build_engine);
        RegisterTestApi(build_engine.get());

        auto* build_module = BuildModule(build_engine.get(), "BuildModule");

        ResourceTrackerState build_state {};
        const auto build_metrics = RunScenario(build_module, build_state);

        vector<asBYTE> bytecode {};
        BytecodeStream writer {bytecode};
        CHECK(build_module->SaveByteCode(&writer) >= 0);
        REQUIRE_FALSE(bytecode.empty());

        unique_del_ptr<asIScriptEngine> load_engine {asCreateScriptEngine(), [](asIScriptEngine* engine) {
            if (engine != nullptr) {
                engine->ShutDownAndRelease();
            }
        }};
        REQUIRE(load_engine);
        RegisterTestApi(load_engine.get());

        auto* load_module = load_engine->GetModule("LoadModule", asGM_ALWAYS_CREATE);
        REQUIRE(load_module != nullptr);
        BytecodeStream reader {bytecode};
        CHECK(load_module->LoadByteCode(&reader) >= 0);

        ResourceTrackerState load_state {};
        const auto load_metrics = RunScenario(load_module, load_state);

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

        unique_del_ptr<asIScriptEngine> build_engine {asCreateScriptEngine(), [](asIScriptEngine* engine) {
            if (engine != nullptr) {
                engine->ShutDownAndRelease();
            }
        }};
        REQUIRE(build_engine);
        RegisterTestApi(build_engine.get());

        auto* build_module = BuildModuleFromScript(build_engine.get(), "BuildModule", PortableFormatTestScript);

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
        unique_del_ptr<asIScriptEngine> load_engine {asCreateScriptEngine(), [](asIScriptEngine* engine) {
            if (engine != nullptr) {
                engine->ShutDownAndRelease();
            }
        }};
        REQUIRE(load_engine);
        RegisterTestApi(load_engine.get());

        auto* load_module = load_engine->GetModule("LoadModule", asGM_ALWAYS_CREATE);
        REQUIRE(load_module != nullptr);
        BytecodeStream reader {bytecode};
        CHECK(load_module->LoadByteCode(&reader) >= 0);

        // Execute loaded bytecode and verify identical results
        ResourceTrackerState load_state {};
        const auto load_metrics = RunScenario(load_module, load_state);

        CHECK(load_metrics.FirstObservedId == build_metrics.FirstObservedId);
        CHECK(load_metrics.LastObservedId == build_metrics.LastObservedId);
        CHECK(load_metrics.DoubleFreeCount == build_metrics.DoubleFreeCount);
        CHECK(load_metrics.LeakedCount == build_metrics.LeakedCount);
        CHECK(load_state.GlobalCounter == build_state.GlobalCounter);
    }

    SECTION("GetRefOffsetAdjustmentWithMultiplePtrSizedArgs")
    {
        // Regression test for AdjustGetOffset REFCPY early-return bug.
        // On 64-bit build → 32-bit load, GETREF offsets for reference parameters
        // past multiple pointer-sized stack items were miscalculated, causing crashes.
        // The fix guards the REFCPY shortcut with offset == 1 (reader) / offset == AS_PTR_SIZE (writer).

        unique_del_ptr<asIScriptEngine> build_engine {asCreateScriptEngine(), [](asIScriptEngine* engine) {
            if (engine != nullptr) {
                engine->ShutDownAndRelease();
            }
        }};
        REQUIRE(build_engine);
        RegisterTestApi(build_engine.get());

        auto* build_module = BuildModuleFromScript(build_engine.get(), "BuildModule", GetRefOffsetTestScript);

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
        unique_del_ptr<asIScriptEngine> load_engine {asCreateScriptEngine(), [](asIScriptEngine* engine) {
            if (engine != nullptr) {
                engine->ShutDownAndRelease();
            }
        }};
        REQUIRE(load_engine);
        RegisterTestApi(load_engine.get());

        auto* load_module = load_engine->GetModule("LoadModule", asGM_ALWAYS_CREATE);
        REQUIRE(load_module != nullptr);
        BytecodeStream reader {bytecode};
        CHECK(load_module->LoadByteCode(&reader) >= 0);

        // Execute loaded bytecode and verify identical results
        ResourceTrackerState load_state {};
        const auto load_metrics = RunScenario(load_module, load_state);

        CHECK(load_metrics.FirstObservedId == build_metrics.FirstObservedId);
        CHECK(load_metrics.LastObservedId == build_metrics.LastObservedId);
        CHECK(load_metrics.DoubleFreeCount == build_metrics.DoubleFreeCount);
        CHECK(load_metrics.LeakedCount == build_metrics.LeakedCount);
    }
}

FO_END_NAMESPACE