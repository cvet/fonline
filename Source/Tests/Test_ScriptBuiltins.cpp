//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
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

#include "AngelScriptArray.h"
#include "AngelScriptDict.h"
#include "AngelScriptHelpers.h"
#include "AngelScriptScripting.h"
#include "Baker.h"
#include "DataSerialization.h"
#include "Server.h"
#include "Test_BakerHelpers.h"

#include <angelscript.h>

FO_BEGIN_NAMESPACE

namespace
{
    struct ScriptMessages
    {
        vector<string> Entries {};

        static void Callback(const AngelScript::asSMessageInfo* msg, void* param)
        {
            auto self = cast_from_void<ScriptMessages*>(param);
            FO_VERIFY_AND_THROW(self, "Script message collector is null");

            self->Entries.emplace_back(strex("{}({},{}): {}", msg->section != nullptr ? msg->section : "<unknown>", msg->row, msg->col, msg->message != nullptr ? msg->message : "<no message>").str());
        }
    };

    struct ArrayDummyRef
    {
        int32_t RefCount {};
    };

    struct ArrayNoDefaultValue
    {
        explicit ArrayNoDefaultValue(int32_t value) noexcept :
            Value {value}
        {
        }

        ~ArrayNoDefaultValue() noexcept { Value = 0; }

        int32_t Value {};
    };

    struct ArrayComparableValue
    {
        int32_t Value {};
    };

    static void ArrayDummyRefAddRef(void* obj)
    {
        cast_from_void<ArrayDummyRef*>(obj)->RefCount++;
    }

    static void ArrayDummyRefRelease(void* obj)
    {
        cast_from_void<ArrayDummyRef*>(obj)->RefCount--;
    }

    static void ArrayNoDefaultValueConstruct(void* obj, int32_t value)
    {
        new (obj) ArrayNoDefaultValue(value);
    }

    static void ArrayNoDefaultValueDestruct(void* obj) noexcept
    {
        cast_from_void<ArrayNoDefaultValue*>(obj)->~ArrayNoDefaultValue();
    }

    static auto ArrayComparableValueEquals(const ArrayComparableValue& self, const ArrayComparableValue& other) -> bool
    {
        return self.Value == other.Value;
    }

    static auto ArrayComparableValueEqualsMutable(const ArrayComparableValue& self, ArrayComparableValue& other) -> bool
    {
        return self.Value == other.Value;
    }

    static auto ArrayComparableValueCmp(const ArrayComparableValue& self, const ArrayComparableValue& other) -> int32_t
    {
        if (self.Value < other.Value) {
            return -1;
        }
        if (self.Value > other.Value) {
            return 1;
        }
        return 0;
    }

    static auto ArrayComparableValueCmpMutable(const ArrayComparableValue& self, ArrayComparableValue& other) -> int32_t
    {
        if (self.Value < other.Value) {
            return -1;
        }
        if (self.Value > other.Value) {
            return 1;
        }
        return 0;
    }

    static auto ArrayComparableValueCmpByValue(const ArrayComparableValue& self, ArrayComparableValue other) -> int32_t
    {
        return ArrayComparableValueCmp(self, other);
    }

    static auto MakeAngelScriptEngine(ScriptMessages& messages) -> AngelScript::asIScriptEngine*
    {
        auto engine = make_nptr(AngelScript::asCreateScriptEngine(ANGELSCRIPT_VERSION));
        REQUIRE(engine != nullptr);

        REQUIRE(engine->SetEngineProperty(AngelScript::asEP_OPTIMIZE_BYTECODE, false) >= 0);
        REQUIRE(engine->SetMessageCallback(asFUNCTION(ScriptMessages::Callback), &messages, AngelScript::asCALL_CDECL) >= 0);
        return engine.get();
    }

    static void RegisterArrayDummyRef(AngelScript::asIScriptEngine* engine)
    {
        REQUIRE(engine->RegisterObjectType("ArrayDummyRef", 0, AngelScript::asOBJ_REF) >= 0);
        REQUIRE(engine->RegisterObjectBehaviour("ArrayDummyRef", AngelScript::asBEHAVE_ADDREF, "void f()", FO_SCRIPT_FUNC_THIS(ArrayDummyRefAddRef), FO_SCRIPT_FUNC_THIS_CONV) >= 0);
        REQUIRE(engine->RegisterObjectBehaviour("ArrayDummyRef", AngelScript::asBEHAVE_RELEASE, "void f()", FO_SCRIPT_FUNC_THIS(ArrayDummyRefRelease), FO_SCRIPT_FUNC_THIS_CONV) >= 0);
    }

    static void RegisterArrayNoDefaultValue(AngelScript::asIScriptEngine* engine)
    {
        REQUIRE(engine->RegisterObjectType("ArrayNoDefaultValue", sizeof(ArrayNoDefaultValue), AngelScript::asOBJ_VALUE | AngelScript::asOBJ_APP_CLASS_C | AngelScript::asGetTypeTraits<ArrayNoDefaultValue>()) >= 0);
        REQUIRE(engine->RegisterObjectBehaviour("ArrayNoDefaultValue", AngelScript::asBEHAVE_CONSTRUCT, "void f(int value)", FO_SCRIPT_FUNC_THIS(ArrayNoDefaultValueConstruct), FO_SCRIPT_FUNC_THIS_CONV) >= 0);
        REQUIRE(engine->RegisterObjectBehaviour("ArrayNoDefaultValue", AngelScript::asBEHAVE_DESTRUCT, "void f()", FO_SCRIPT_FUNC_THIS(ArrayNoDefaultValueDestruct), FO_SCRIPT_FUNC_THIS_CONV) >= 0);
    }

    static void RegisterArrayComparableValue(AngelScript::asIScriptEngine* engine, string_view type_name, bool with_multiple_equals, bool with_multiple_cmp)
    {
        string name {type_name};
        REQUIRE(engine->RegisterObjectType(name.c_str(), sizeof(ArrayComparableValue), AngelScript::asOBJ_VALUE | AngelScript::asOBJ_POD | AngelScript::asOBJ_APP_CLASS | AngelScript::asOBJ_APP_CLASS_ALLINTS) >= 0);

        if (with_multiple_equals) {
            REQUIRE(engine->RegisterObjectMethod(name.c_str(), strex("bool opEquals(const {} &in) const", name).c_str(), FO_SCRIPT_FUNC_THIS(ArrayComparableValueEquals), FO_SCRIPT_FUNC_THIS_CONV) >= 0);
            REQUIRE(engine->RegisterObjectMethod(name.c_str(), strex("bool opEquals({} &in) const", name).c_str(), FO_SCRIPT_FUNC_THIS(ArrayComparableValueEqualsMutable), FO_SCRIPT_FUNC_THIS_CONV) >= 0);
        }

        if (with_multiple_cmp) {
            REQUIRE(engine->RegisterObjectMethod(name.c_str(), strex("int opCmp(const {} &in) const", name).c_str(), FO_SCRIPT_FUNC_THIS(ArrayComparableValueCmp), FO_SCRIPT_FUNC_THIS_CONV) >= 0);
            REQUIRE(engine->RegisterObjectMethod(name.c_str(), strex("int opCmp({} &in) const", name).c_str(), FO_SCRIPT_FUNC_THIS(ArrayComparableValueCmpMutable), FO_SCRIPT_FUNC_THIS_CONV) >= 0);
        }
    }

    static void RegisterArrayCmpOnlyValue(AngelScript::asIScriptEngine* engine, string_view type_name)
    {
        string name {type_name};
        REQUIRE(engine->RegisterObjectType(name.c_str(), sizeof(ArrayComparableValue), AngelScript::asOBJ_VALUE | AngelScript::asOBJ_POD | AngelScript::asOBJ_APP_CLASS | AngelScript::asOBJ_APP_CLASS_ALLINTS) >= 0);
        REQUIRE(engine->RegisterObjectMethod(name.c_str(), strex("int opCmp(const {} &in) const", name).c_str(), FO_SCRIPT_FUNC_THIS(ArrayComparableValueCmp), FO_SCRIPT_FUNC_THIS_CONV) >= 0);
    }

    static void RegisterArrayComparatorFilterValue(AngelScript::asIScriptEngine* engine, string_view type_name, string_view method_decl, const AngelScript::asSFuncPtr& func_ptr)
    {
        string name {type_name};
        REQUIRE(engine->RegisterObjectType(name.c_str(), sizeof(ArrayComparableValue), AngelScript::asOBJ_VALUE | AngelScript::asOBJ_POD | AngelScript::asOBJ_APP_CLASS | AngelScript::asOBJ_APP_CLASS_ALLINTS) >= 0);
        REQUIRE(engine->RegisterObjectMethod(name.c_str(), string {method_decl}.c_str(), func_ptr, FO_SCRIPT_FUNC_THIS_CONV) >= 0);
    }

    static auto CheckArrayCmpOnlyValueOps() -> bool
    {
        FO_STACK_TRACE_ENTRY();

        auto ctx = make_nptr(AngelScript::asGetActiveContext());
        FO_VERIFY_AND_THROW(ctx != nullptr, "Missing active AngelScript context");

        nptr<AngelScript::asIScriptEngine> engine = ctx->GetEngine();
        FO_VERIFY_AND_THROW(engine != nullptr, "Missing AngelScript engine");

        nptr<AngelScript::asITypeInfo> array_type = engine->GetTypeInfoByDecl("array<ArrayCmpOnlyNativeValue>");
        FO_VERIFY_AND_THROW(array_type != nullptr, "Missing array<ArrayCmpOnlyNativeValue> type");

        auto values = ScriptArray::Create(array_type.get(), 2);

        auto same_values = ScriptArray::Create(array_type.get(), 2);

        ArrayComparableValue low_value {1};
        ArrayComparableValue same_low_value {1};
        ArrayComparableValue high_value {3};

        values->SetValue(0, &low_value);
        values->SetValue(1, &high_value);
        same_values->SetValue(0, &same_low_value);
        same_values->SetValue(1, &high_value);

        if (!(*values == *same_values)) {
            return false;
        }
        if (values->Find(&same_low_value) != 0) {
            return false;
        }

        same_values->SetValue(0, &high_value);

        if (*values == *same_values) {
            return false;
        }

        return true;
    }

    static auto BuildAngelScriptModule(AngelScript::asIScriptEngine* engine, string_view module_name, string_view source) -> int32_t
    {
        auto module = make_nptr(engine->GetModule(string(module_name).c_str(), AngelScript::asGM_ALWAYS_CREATE));
        REQUIRE(module != nullptr);
        REQUIRE(module->AddScriptSection("InlineArrayTemplateCheck", source.data(), source.size()) >= 0);
        return module->Build();
    }

    static auto HasScriptMessage(const ScriptMessages& messages, string_view text) -> bool
    {
        return std::ranges::any_of(messages.Entries, [text](const string& entry) { return entry.find(text) != string::npos; });
    }

    static void ReportScriptMessages(const ScriptMessages& messages)
    {
        FO_STACK_TRACE_ENTRY();

        for (const string& entry : messages.Entries) {
            UNSCOPED_INFO(entry);
        }
    }

    static auto RequireScriptModule(AngelScript::asIScriptEngine* engine, ScriptMessages& messages, string_view module_name, string_view source) -> ptr<AngelScript::asIScriptModule>
    {
        FO_STACK_TRACE_ENTRY();

        int32_t build_result = BuildAngelScriptModule(engine, module_name, source);
        ReportScriptMessages(messages);
        REQUIRE(build_result >= 0);

        nptr<AngelScript::asIScriptModule> module = engine->GetModule(string {module_name}.c_str(), AngelScript::asGM_ONLY_IF_EXISTS);
        REQUIRE(module != nullptr);
        return module;
    }

    static void RunScriptFunction(ptr<AngelScript::asIScriptEngine> engine, ptr<AngelScript::asIScriptModule> module, string_view declaration)
    {
        FO_STACK_TRACE_ENTRY();

        nptr<AngelScript::asIScriptFunction> func = module->GetFunctionByDecl(string {declaration}.c_str());
        REQUIRE(func != nullptr);

        nptr<AngelScript::asIScriptContext> ctx = engine->CreateContext();
        REQUIRE(ctx != nullptr);
        REQUIRE(ctx->Prepare(func.get()) >= 0);
        int32_t exec_result = ctx->Execute();
        UNSCOPED_INFO(strex("Script execution result: {}, exception: {}", exec_result, ctx->GetExceptionString()).str());
        REQUIRE(exec_result == AngelScript::asEXECUTION_FINISHED);
        REQUIRE(ctx->Unprepare() >= 0);
        ctx->Release();
    }

    static void RequireFullGarbageCollection(ptr<AngelScript::asIScriptEngine> engine)
    {
        FO_STACK_TRACE_ENTRY();

        REQUIRE(engine->GarbageCollect(AngelScript::asGC_FULL_CYCLE) >= 0);

        AngelScript::asUINT current_size = 0;
        engine->GetGCStatistics(&current_size, nullptr, nullptr, nullptr, nullptr);
        CHECK(current_size == 0);
    }

    static void ShutdownAndCheckGcDiagnostics(nptr<AngelScript::asIScriptEngine> engine, const ScriptMessages& messages)
    {
        FO_STACK_TRACE_ENTRY();

        REQUIRE(engine != nullptr);
        CHECK(engine->ShutDownAndRelease() >= 0);
        ReportScriptMessages(messages);
        CHECK_FALSE(HasScriptMessage(messages, "GC cannot destroy an object"));
        CHECK_FALSE(HasScriptMessage(messages, "There is an external reference to an object in module"));
    }

    struct GcShutdownTracker
    {
        int32_t DestructedObjects {};
    };

    static void NotifyGcShutdownDestruction()
    {
        FO_STACK_TRACE_ENTRY();

        nptr<AngelScript::asIScriptContext> context = AngelScript::asGetActiveContext();
        FO_VERIFY_AND_THROW(context, "Missing active AngelScript context");
        nptr<GcShutdownTracker> tracker = cast_from_void<GcShutdownTracker*>(context->GetEngine()->GetUserData());
        FO_VERIFY_AND_THROW(tracker, "Missing GC shutdown tracker");
        tracker->DestructedObjects++;
    }

    template<typename T>
    static void CheckPrimitiveScriptArrayDirectOps(AngelScript::asIScriptEngine* engine, string_view type_decl, T low, T high)
    {
        string array_type_decl = strex("array<{}>", type_decl).str();
        auto array_type = make_nptr(engine->GetTypeInfoByDecl(array_type_decl.c_str()));
        REQUIRE(array_type != nullptr);

        auto values = ScriptArray::Create(array_type.get(), 2);

        T low_value = low;
        T high_value = high;
        values->SetValue(0, &high_value);
        values->SetValue(1, &low_value);

        auto same_values = ScriptArray::Create(array_type.get(), 2);

        same_values->SetValue(0, &high_value);
        same_values->SetValue(1, &low_value);
        CHECK(*values == *same_values);

        same_values->SetValue(1, &high_value);
        CHECK_FALSE(*values == *same_values);

        values->SortAsc();
        CHECK(*values->AtAs<T>(0) == low_value);
        CHECK(*values->AtAs<T>(1) == high_value);

        values->SortDesc();
        CHECK(*values->AtAs<T>(0) == high_value);
        CHECK(*values->AtAs<T>(1) == low_value);

        CHECK(values->Find(&low_value) == 1);
        CHECK(values->Find(1, &high_value) == -1);
        CHECK(values->FindByRef(0, values->At(0)) == 0);
    }

    static auto MakeSettings() -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedServerSettings(settings);

        return settings;
    }

    static auto MakeScriptBinary(const FileSystem& metadata_resources) -> vector<uint8_t>
    {
        BakerServerEngine compiler_engine {metadata_resources};

        return BakerTests::CompileInlineScripts(&compiler_engine, "ScriptBuiltinsScripts",
            {
                {"Scripts/ScriptBuiltins.fos",
                    R"(
namespace ScriptBuiltins
{
    // === String operations ===

    int StringLength(string s)
    {
        return s.length();
    }

    bool StringEmpty(string s)
    {
        return s.isEmpty();
    }

    string StringSubstr(string s, int start, int count)
    {
        return s.substr(start, count);
    }

    int StringFindFirst(string s, string sub)
    {
        return s.find(sub);
    }

    int StringFindLast(string s, string sub)
    {
        return s.findLast(sub, 0);
    }

    string StringToLower(string s)
    {
        return s.lower();
    }

    string StringToUpper(string s)
    {
        return s.upper();
    }

    int StringToInt(string s)
    {
        return s.toInt();
    }

    float StringToFloat(string s)
    {
        return s.toFloat();
    }

    string IntToString(int v)
    {
        return "" + v;
    }

    string BoolToString(bool v)
    {
        return "" + v;
    }

    string StringAddBool(string prefix, bool v)
    {
        return prefix + v;
    }

    string BoolAddString(bool v, string suffix)
    {
        return v + suffix;
    }

    string AssignBoolToString(bool v)
    {
        string result;
        result = v;
        return result;
    }

    string AddAssignBoolToString(bool v)
    {
        string result = "flag=";
        result += v;
        return result;
    }

    string StringConcat(string a, string b)
    {
        return a + b;
    }

    bool StringEquals(string a, string b)
    {
        return a == b;
    }

    bool StringNotEquals(string a, string b)
    {
        return a != b;
    }

    int StringCompare(string a, string b)
    {
        if (a < b) return -1;
        if (a > b) return 1;
        return 0;
    }

    int StringFindFirstOf(string s, string chars)
    {
        return s.findFirstOf(chars);
    }

    int StringFindFirstNotOf(string s, string chars)
    {
        return s.findFirstNotOf(chars);
    }

    string StringReplace(string s, string from, string to)
    {
        return s.replace(from, to);
    }

    bool StringStartsWith(string s, string prefix)
    {
        return s.startsWith(prefix);
    }

    bool StringEndsWith(string s, string suffix)
    {
        return s.endsWith(suffix);
    }

    string StringTrim(string s)
    {
        return s.trim();
    }

    string EnumToAnyString()
    {
        any value = GenderType::Male;
        string stored = value;
        return stored;
    }

    string EnumAssignToAnyString()
    {
        any value;
        value = GenderType::Female;
        string stored = value;
        return stored;
    }

    bool EnumRoundtripFromAny()
    {
        any value = GenderType::Male;
        GenderType parsed = value;
        return parsed == GenderType::Male;
    }

    bool EnumParseFullNameFromAny()
    {
        any value = "GenderType::Female";
        GenderType parsed = value;
        return parsed == GenderType::Female;
    }

    void InvalidIntConversionFromAny()
    {
        any value = "DefinitelyNotANumber";
        int parsed = value;
    }

    int EmptyAnyToInt()
    {
        any value;
        return int(value);
    }

    // === Array operations ===

    int ArrayLength(int[] arr)
    {
        return arr.length();
    }

    bool ArrayEmpty(int[] arr)
    {
        return arr.isEmpty();
    }

    int ArrayFind(int[] arr, int value)
    {
        return arr.find(value);
    }

    // Nested array operations
    int NestedArraySum()
    {
        int[][] nested = {};
        int[] inner1 = {1, 2, 3};
        int[] inner2 = {4, 5, 6};
        nested.insertLast(inner1);
        nested.insertLast(inner2);

        int total = 0;
        for (int i = 0; i < nested.length(); i++) {
            for (int j = 0; j < nested[i].length(); j++) {
                total += nested[i][j];
            }
        }
        return total;
    }

    int ArrayExtendedMutators()
    {
        int[] filled = {7, 7, 7};
        if (filled.length() != 3) return -1;
        if (filled[0] != 7 || filled[2] != 7) return -2;

        filled.reserve(2);
        filled.reserve(12);
        if (filled.length() != 3) return -3;

        filled.grow(2);
        if (filled.length() != 5) return -4;
        if (filled[3] != 0 || filled[4] != 0) return -5;

        filled.reduce(1);
        if (filled.length() != 4) return -6;

        filled.insertFirst(3);
        if (filled.first() != 3) return -7;

        filled.removeFirst();
        if (filled.first() != 7) return -8;

        filled.insertLast(9);
        if (filled.last() != 9) return -9;
        if (!filled.exists(9)) return -10;
        if (!filled.remove(9)) return -11;
        if (filled.exists(9)) return -12;

        filled.insertLast(7);
        filled.insertLast(7);
        if (filled.removeAll(7) != 5) return -13;
        if (filled.length() != 1 || filled[0] != 0) return -14;
        filled.clear();
        if (!filled.isEmpty()) return -29;

        int[] base = {1, 4};
        int[] middle = {2, 3};
        base.insertAt(1, middle);
        if (base.length() != 4) return -15;
        if (base[0] != 1 || base[1] != 2 || base[2] != 3 || base[3] != 4) return -16;

        int before_negative_insert = base.length();
        base.insertAt(-1, middle);
        if (base.length() != before_negative_insert) return -17;

        int[] prefix = {-1, 0};
        int[] suffix = {5, 6};
        base.insertFirst(prefix);
        base.insertLast(suffix);
        if (base.length() != 8) return -18;
        if (base[0] != -1 || base[1] != 0 || base[6] != 5 || base[7] != 6) return -19;

        base.removeRange(2, 2);
        if (base.length() != 6) return -20;
        if (base[2] != 3 || base[3] != 4) return -21;

        base.removeRange(2, 0);
        if (base.length() != 6) return -22;

        int[] clone = base.clone();
        if (!(base == clone)) return -24;

        int[] target = {};
        target.set(clone);
        if (!(target == base)) return -25;

        int[] self_insert = {1, 2, 3};
        self_insert.insertAt(1, self_insert);
        if (self_insert.length() != 6) return -26;
        if (self_insert[0] != 1 || self_insert[1] != 1 || self_insert[2] != 2 || self_insert[3] != 3 || self_insert[4] != 2 || self_insert[5] != 3) return -27;

        self_insert.clear();
        if (!self_insert.isEmpty()) return -28;

        return 1;
    }

    int ArrayStringObjectOps()
    {
        string[] names = {"beta", "alpha", "gamma"};
        names.sortAsc();
        if (names[0] != "alpha" || names[1] != "beta" || names[2] != "gamma") return -1;

        names.sortDesc(0, names.length());
        if (names[0] != "gamma" || names[1] != "beta" || names[2] != "alpha") return -2;

        if (names.find("beta") != 1) return -3;
        if (names.find(2, "beta") != -1) return -4;

        string[] cloned = names.clone();
        if (!(names == cloned)) return -6;

        cloned[1] = "delta";
        if (names == cloned) return -7;

        names.set(cloned);
        if (names[1] != "delta") return -8;

        string[] tail = {"omega"};
        names.insertLast(tail);
        if (names.last() != "omega") return -9;

        return 1;
    }

    int ArrayHandleOps()
    {
        Critter cr1 = Game.CreateCritter("UnitTestCr".hstr(), false);
        Critter cr2 = Game.CreateCritter("UnitTestCr".hstr(), false);
        if (cr1 is null || cr2 is null) return -1;

        Critter[] critters = {cr1, cr2, cr1};
        if (critters.length() != 3) return -2;
        if (critters.find(cr1) != 0) return -3;
        if (critters.find(1, cr1) != 2) return -4;
        if (critters.findByRef(cr2) != 1) return -5;
        if (critters.findByRef(2, cr2) != -1) return -6;

        Critter[] cloned = critters.clone();
        if (!(critters == cloned)) return -8;

        if (!cloned.remove(cr1)) return -9;
        if (cloned.length() != 2) return -10;
        if (cloned.removeAll(cr1) != 1) return -11;
        if (cloned.length() != 1) return -12;

        Critter[] target = {};
        target.set(cloned);
        if (target.length() != 1) return -13;
        if (target[0].Id != cr2.Id) return -14;

        target[0] = cr1;
        if (target[0].Id != cr1.Id) return -15;

        Critter[] replacement = {cr2};
        target.set(replacement);
        if (target[0].Id != cr2.Id) return -16;

        // The int index occupies a padded VM argument slot. The following handle must still be
        // passed as the second dense native x86 argument to array<T>::insertAt.
        target.insertAt(1, cr1);
        if (target.length() != 2) return -17;
        if (target[0].Id != cr2.Id || target[1].Id != cr1.Id) return -18;

        Game.DestroyCritter(cr1);
        Game.DestroyCritter(cr2);
        return 1;
    }

    int ArraySortSubrangeOps()
    {
        int[] values = {9, 4, 3, 2, 8};
        values.sortAsc(1, 3);
        if (values[0] != 9 || values[1] != 2 || values[2] != 3 || values[3] != 4 || values[4] != 8) return -1;

        values.sortDesc(1, 3);
        if (values[0] != 9 || values[1] != 4 || values[2] != 3 || values[3] != 2 || values[4] != 8) return -2;

        values.sortAsc(0, 1);
        if (values[0] != 9) return -3;

        return 1;
    }
)"
                    R"(
    int ArrayEdgeNoopOps()
    {
        int[] values = {1, 2};
        values.reserve(0);
        values.reserve(1);
        values.grow(0);
        values.grow(-3);
        values.reduce(0);
        values.reduce(-2);
        values.removeRange(1, 0);
        values.removeRange(1, -2);
        if (values.length() != 2) return -1;
        if (values[0] != 1 || values[1] != 2) return -2;

        int ref_value = 2;
        if (values.findByRef(ref_value) != -1) return -3;
        if (values.findByRef(1, ref_value) != -1) return -4;
        if (values.remove(99)) return -5;
        if (values.removeAll(99) != 0) return -6;

        int[] empty = {};
        empty.clear();
        empty.reverse();
        if (!empty.isEmpty()) return -7;

        int[] single = {9};
        single.reverse();
        if (single.length() != 1 || single[0] != 9) return -8;

        int[] trimmed = {1, 2, 3};
        trimmed.removeRange(1, 10);
        if (trimmed.length() != 1 || trimmed[0] != 1) return -9;

        GenderType[] genders = {GenderType::Female, GenderType::Male, GenderType::Female};
        if (genders.find(GenderType::Male) != 1) return -10;
        GenderType[] cloned = genders.clone();
        if (!(genders == cloned)) return -11;

        genders.sortAsc();
        if (genders[0] != GenderType::Male || genders[1] != GenderType::Female || genders[2] != GenderType::Female) return -12;

        return 1;
    }

    int ArrayPrimitiveTypeOps()
    {
        bool bool_true = true;
        bool bool_false = false;
        bool[] bools = {};
        bools.insertLast(bool_true);
        bools.insertLast(bool_false);
        bools.sortAsc();
        if (bools[0] != false || bools[1] != true) return -1;
        if (!bools.exists(true)) return -2;
        bool[] bools_clone = bools.clone();
        if (!(bools == bools_clone)) return -13;

        int8 int8_low = -2;
        int8 int8_high = 3;
        int8[] int8_values = {};
        int8_values.insertLast(int8_low);
        int8_values.insertLast(int8_high);
        int8_values.sortDesc();
        if (int8_values[0] != int8_high || int8_values[1] != int8_low) return -3;
        if (int8_values.find(int8_low) != 1) return -4;
        int8[] int8_clone = int8_values.clone();
        if (!(int8_values == int8_clone)) return -14;

        uint8 uint8_low = 1;
        uint8 uint8_high = 4;
        uint8[] uint8_values = {};
        uint8_values.insertLast(uint8_high);
        uint8_values.insertLast(uint8_low);
        uint8_values.sortAsc();
        if (uint8_values[0] != uint8_low || uint8_values[1] != uint8_high) return -5;
        uint8[] uint8_clone = uint8_values.clone();
        if (!(uint8_values == uint8_clone)) return -15;

        int16 int16_low = -200;
        int16 int16_high = 1200;
        int16[] int16_values = {};
        int16_values.insertLast(int16_high);
        int16_values.insertLast(int16_low);
        int16_values.sortAsc();
        if (int16_values[0] != int16_low || int16_values[1] != int16_high) return -6;
        int16[] int16_clone = int16_values.clone();
        if (!(int16_values == int16_clone)) return -16;

        uint16 uint16_low = 12;
        uint16 uint16_high = 650;
        uint16[] uint16_values = {};
        uint16_values.insertLast(uint16_low);
        uint16_values.insertLast(uint16_high);
        uint16_values.sortDesc();
        if (uint16_values[0] != uint16_high || uint16_values[1] != uint16_low) return -7;
        uint16[] uint16_clone = uint16_values.clone();
        if (!(uint16_values == uint16_clone)) return -17;

        uint uint_low = 8;
        uint uint_high = 90;
        uint[] uint_values = {};
        uint_values.insertLast(uint_high);
        uint_values.insertLast(uint_low);
        uint_values.sortAsc();
        if (uint_values[0] != uint_low || uint_values[1] != uint_high) return -8;
        uint[] uint_clone = uint_values.clone();
        if (!(uint_values == uint_clone)) return -18;

        int64 int64_low = 500;
        int64 int64_high = 700;
        int64[] int64_values = {};
        int64_values.insertLast(int64_high);
        int64_values.insertLast(int64_low);
        int64_values.sortAsc();
        if (int64_values[0] != int64_low || int64_values[1] != int64_high) return -9;
        int64[] int64_clone = int64_values.clone();
        if (!(int64_values == int64_clone)) return -19;

        uint64 uint64_low = 5;
        uint64 uint64_high = 900;
        uint64[] uint64_values = {};
        uint64_values.insertLast(uint64_high);
        uint64_values.insertLast(uint64_low);
        uint64_values.sortAsc();
        if (uint64_values[0] != uint64_low || uint64_values[1] != uint64_high) return -10;
        uint64[] uint64_clone = uint64_values.clone();
        if (!(uint64_values == uint64_clone)) return -20;
)"
                    R"(
        float float_low = -1.5f;
        float float_high = 2.25f;
        float[] float_values = {};
        float_values.insertLast(float_high);
        float_values.insertLast(float_low);
        float_values.sortAsc();
        if (float_values[0] != float_low || float_values[1] != float_high) return -11;
        float[] float_clone = float_values.clone();
        if (!(float_values == float_clone)) return -21;

        double double_low = -3.5;
        double double_high = 4.75;
        double[] double_values = {};
        double_values.insertLast(double_high);
        double_values.insertLast(double_low);
        double_values.sortAsc();
        if (double_values[0] != double_low || double_values[1] != double_high) return -12;
        double[] double_clone = double_values.clone();
        if (!(double_values == double_clone)) return -22;

        return 1;
    }

    int ArrayWideEnumOps()
    {
        WideEnum low = WideEnum::Low;
        WideEnum high = WideEnum::High;
        WideEnum[] values = {};
        values.insertLast(high);
        values.insertLast(low);
        values.sortAsc();
        if (values[0] != low || values[1] != high) return -1;
        if (values.find(high) != 1) return -2;

        WideEnum[] clone = values.clone();
        if (!(values == clone)) return -3;
        clone[1] = low;
        if (values == clone) return -4;

        return 1;
    }

    int ArrayConstructorOps()
    {
        array<int> filled = array<int>(3, 7);
        if (filled.length() != 3) return -1;
        if (filled[0] != 7 || filled[1] != 7 || filled[2] != 7) return -2;

        array<GenderType> genders = array<GenderType>(2, GenderType::Male);
        if (genders.length() != 2) return -3;
        if (genders[0] != GenderType::Male || genders[1] != GenderType::Male) return -4;

        array<string> names = array<string>(2, "seed");
        if (names.length() != 2) return -5;
        if (names[0] != "seed" || names[1] != "seed") return -6;

        array<int> source = {1, 2, 3};
)"
                    R"(
        array<int> copied = array<int>(source);
        if (copied.length() != 3) return -7;
        if (copied[0] != 1 || copied[1] != 2 || copied[2] != 3) return -8;

        copied[1] = 20;
        if (source[1] != 2) return -9;

        array<int> assigned = array<int>(2, 9);
        assigned = source;
        if (assigned.length() != 3) return -10;
        if (assigned[0] != 1 || assigned[1] != 2 || assigned[2] != 3) return -11;

        return 1;
    }

    class ArrayNoCompare
    {
        int Value = 0;
    }

    class ArrayComparableHandle
    {
        int Value = 0;

        int opCmp(const ArrayComparableHandle? other) const
        {
            if (other is null) return 1;
            if (Value < other.Value) return -1;
            if (Value > other.Value) return 1;
            return 0;
        }

        bool opEquals(const ArrayComparableHandle? other) const
        {
            return other !is null && Value == other.Value;
        }
    }

    class ArrayCmpOnlyHandle
    {
        int Value = 0;

        int opCmp(const ArrayCmpOnlyHandle? other) const
        {
            if (other is null) return 1;
            if (Value < other.Value) return -1;
            if (Value > other.Value) return 1;
            return 0;
        }
    }

    class ArrayHandleInRefComparator
    {
        int opCmp(ArrayHandleInRefComparator?&in other) const
        {
            return 0;
        }
    }

    class ArrayConstHandleComparator
    {
        int opCmp(ArrayConstHandleComparator? other) const
        {
            return 0;
        }
    }

    int ArrayInsertNegativeArrayNoop()
    {
        int[] values = {1};
        int[] other = {2, 3};
        values.insertAt(-1, other);
        if (values.length() != 1) return -1;
        if (values[0] != 1) return -2;
        return 1;
    }

    void ArrayNegativeSizeThrows()
    {
        int[] values = {};
        values.resize(-1);
    }

    void ArrayTooLargeThrows()
    {
        int[] values = {};
        values.resize(2147483647);
    }

    void ArrayIndexOutOfBoundsThrows()
    {
        int[] values = {1};
        int value = values[-1];
    }

    void ArrayInsertOutOfBoundsThrows()
    {
        int[] values = {1};
        values.insertAt(2, 2);
    }

    void ArrayRemoveLastEmptyThrows()
    {
        int[] values = {};
        values.removeLast();
    }

    void ArrayFirstEmptyThrows()
    {
        int[] values = {};
        int value = values.first();
    }

    void ArrayLastEmptyThrows()
    {
        int[] values = {};
        int value = values.last();
    }

    void ArrayRemoveRangeOutOfBoundsThrows()
    {
        int[] values = {1};
        values.removeRange(2, 1);
    }

    void ArraySortRangeOutOfBoundsThrows()
    {
        int[] values = {1, 2};
        values.sortAsc(1, 3);
    }
)"
                    R"(
    void ArrayReduceTooMuchThrows()
    {
        int[] values = {1};
        values.reduce(2);
    }

    void ArraySetNullThrows()
    {
        int[] values = {};
        int[]? other = null;
        values.set(other);
    }

    void ArrayInsertFirstNullThrows()
    {
        int[] values = {};
        int[]? other = null;
        values.insertFirst(other);
    }

    void ArrayInsertLastNullThrows()
    {
        int[] values = {};
        int[]? other = null;
        values.insertLast(other);
    }

    void ArrayInsertAtNullThrows()
    {
        int[] values = {};
        int[]? other = null;
        values.insertAt(0, other);
    }

    void ArrayEqualsNullThrows()
    {
        int[] values = {};
        int[]? other = null;
        bool same = values == other;
    }

    void ArrayFactoryNullThrows()
    {
        int[]? other = null;
        int[] values = int[](other);
    }

    void ArrayObjectNoCmpSortThrows()
    {
        ArrayNoCompare first;
        ArrayNoCompare second;
        ArrayNoCompare[] values = {};
        values.insertLast(first);
        values.insertLast(second);
        values.sortAsc();
    }

    void ArrayHandleInRefComparatorThrows()
    {
        ArrayHandleInRefComparator first;
        ArrayHandleInRefComparator second;
        ArrayHandleInRefComparator?[] values = {first, second};
        values.sortAsc();
    }

    void ArrayConstHandleComparatorThrows()
    {
        ArrayConstHandleComparator first;
        ArrayConstHandleComparator second;
        array<const ArrayConstHandleComparator?> values = {first, second};
        values.sortAsc();
    }

    int ArrayObjectNoCmpEqualsOps()
    {
        ArrayNoCompare first;
        ArrayNoCompare[] values = {};
        values.insertLast(first);
        bool same = values == values;
        if (!same) return -1;
        return 1;
    }

    int ArrayObjectNoCmpFindOps()
    {
        ArrayNoCompare first;
        ArrayNoCompare[] values = {};
        values.insertLast(first);
        int index = values.find(first);
        if (index < -1) return -1;
        return 1;
    }

    int ArrayHandleComparatorOps()
    {
        ArrayComparableHandle low = ArrayComparableHandle();
        low.Value = 1;
        ArrayComparableHandle same_low = ArrayComparableHandle();
        same_low.Value = 1;
        ArrayComparableHandle high = ArrayComparableHandle();
        high.Value = 3;

        ArrayComparableHandle?[] values = {high, low};
        values.sortAsc();
        if (values[0] !is low || values[1] !is high) return -1;
        if (values.find(same_low) != 0) return -2;
        if (values.find(high) != 1) return -3;

        ArrayComparableHandle?[] high_then_null = {high, null};
        high_then_null.sortAsc();
        if (high_then_null[0] !is null || high_then_null[1] !is high) return -4;

        ArrayComparableHandle?[] null_then_low = {null, low};
        null_then_low.sortAsc();
        if (null_then_low[0] !is null || null_then_low[1] !is low) return -5;

        return 1;
    }

    int ArrayHandleCmpOnlyOps()
    {
        ArrayCmpOnlyHandle low = ArrayCmpOnlyHandle();
        low.Value = 1;
        ArrayCmpOnlyHandle same_low = ArrayCmpOnlyHandle();
        same_low.Value = 1;
        ArrayCmpOnlyHandle high = ArrayCmpOnlyHandle();
        high.Value = 3;

        ArrayCmpOnlyHandle?[] values = {high, low};
        values.sortAsc();
        if (values[0] !is low || values[1] !is high) return -1;
        if (values.find(same_low) != 0) return -2;

        ArrayCmpOnlyHandle?[] same_values = {same_low, high};
        if (!(values == same_values)) return -3;

        same_values[0] = high;
        if (values == same_values) return -4;

        return 1;
    }

    // === Dict operations ===

    int DictLength()
    {
        dict<string, int> d = {};
        d.set("a", 1);
        d.set("b", 2);
        d.set("c", 3);
        return d.length();
    }

    bool DictEmpty()
    {
        dict<string, int> d = {};
        bool empty = d.isEmpty();
        d.set("key", 42);
        bool notEmpty = !d.isEmpty();
        return empty && notEmpty;
    }

    int DictGetSet()
    {
        dict<string, int> d = {};
        d.set("x", 10);
        d.set("y", 20);
        d.set("z", 30);

        int result = d.get("x") + d.get("y") + d.get("z");
        return result;
    }

    bool DictExistsRemove()
    {
        dict<string, int> d = {};
        d.set("key", 42);
        bool had = d.exists("key");
        d.remove("key");
        bool gone = !d.exists("key");
        return had && gone;
    }

    int DictClear()
    {
        dict<string, int> d = {};
        d.set("a", 1);
        d.set("b", 2);
        d.clear();
        return d.length();
    }

    int DictIntKeys()
    {
        dict<int, int> d = {};
        d.set(10, 100);
        d.set(20, 200);
        d.set(30, 300);

        int total = 0;
        for (int i = 0; i < d.length(); i++) {
            total += d.getKey(i);
            total += d.getValue(i);
        }
        return total;
    }

    string DictStringValues()
    {
        dict<int, string> d = {};
        d.set(1, "hello");
        d.set(2, " ");
        d.set(3, "world");

        string result = "";
        for (int i = 0; i < d.length(); i++) {
            result += d.getValue(i);
        }
        return result;
    }

    // === Math operations ===

    float MathAbs(float v)
    {
        return math::abs(v);
    }

    float MathSqrt(float v)
    {
        return math::sqrt(v);
    }

    float MathSin(float v)
    {
        return math::sin(v);
    }

    float MathCos(float v)
    {
        return math::cos(v);
    }

    float MathFloor(float v)
    {
        return math::floor(v);
    }

    float MathCeil(float v)
    {
        return math::ceil(v);
    }

    float MathPow(float base, float exp)
    {
        return math::pow(base, exp);
    }

    float MathLog(float v)
    {
        return math::log(v);
    }

    // === Type conversion operations ===

    bool HstringOps()
    {
        hstring h1 = "TestHash1".hstr();
        hstring h2 = "TestHash2".hstr();
        hstring h3 = "TestHash1".hstr();

        bool eq = (h1 == h3);
        bool neq = (h1 != h2);
        string s = string(h1);
        bool strCheck = (s == "TestHash1");
        return eq && neq && strCheck;
    }

    // === Global API operations ===

    bool GameLogWorks()
    {
        Game.Log("ScriptBuiltins test log message");
        return true;
    }

    // === Comprehensive exercise ===

    int ComprehensiveArrayTest()
    {
        int[] arr = {};

        if (!arr.isEmpty()) return -1;
        if (arr.length() != 0) return -2;

        arr.insertLast(5);
        arr.insertLast(3);
        arr.insertLast(8);
        arr.insertLast(1);
        arr.insertLast(9);

        if (arr.isEmpty()) return -3;
        if (arr.length() != 5) return -4;

        if (arr.find(3) != 1) return -5;
        if (arr.find(99) != -1) return -6;

        arr.sortAsc();
        if (arr[0] != 1 || arr[1] != 3 || arr[2] != 5 || arr[3] != 8 || arr[4] != 9) return -7;

        arr.sortDesc();
        if (arr[0] != 9 || arr[1] != 8 || arr[2] != 5 || arr[3] != 3 || arr[4] != 1) return -8;

        arr.reverse();
        if (arr[0] != 1 || arr[4] != 9) return -9;

        arr.insertAt(2, 42);
        if (arr[2] != 42 || arr.length() != 6) return -10;

        arr.removeAt(2);
        if (arr.length() != 5) return -11;

        arr.removeLast();
        if (arr.length() != 4) return -12;

        arr.resize(10);
        if (arr.length() != 10) return -13;
        if (arr[9] != 0) return -14;

        arr.resize(2);
        if (arr.length() != 2) return -15;

        return 1;
    }

    int ComprehensiveStringTest()
    {
        string s = "Hello, World!";

        if (s.length() != 13) return -1;
        if (s.isEmpty()) return -2;

        string sub = s.substr(0, 5);
        if (sub != "Hello") return -3;

        if (s.find("l") != 2) return -4;
        if (s.findLast("l", 0) != 10) return -5;
        if (s.find("xyz") != -1) return -6;

        if (s.lower() != "hello, world!") return -7;
        if (s.upper() != "HELLO, WORLD!") return -8;

        string cat = "abc" + "def";
        if (cat != "abcdef") return -9;

        if (!("aaa" < "bbb")) return -10;
        if (!("bbb" > "aaa")) return -11;
        if ("abc" != "abc") return -12;

        string empty = "";
        if (!empty.isEmpty()) return -13;
        if (empty.length() != 0) return -14;

        if ("42".toInt() != 42) return -15;
        if ("-10".toInt() != -10) return -16;

        if (s.findFirstOf("aeiou") != 1) return -17;
        if (s.findFirstNotOf("Helo") != 5) return -18;

        if (!s.startsWith("Hello")) return -19;
        if (!s.endsWith("World!")) return -20;

        string replaced = s.replace("World", "Test");
        if (replaced != "Hello, Test!") return -21;

        string trimmed = "\t  hello  \r\n".trim();
        if (trimmed != "hello") return -22;

        return 1;
    }

    int ComprehensiveDictTest()
    {
        dict<string, int> sd = {};

        if (!sd.isEmpty()) return -1;
        if (sd.length() != 0) return -2;

        sd.set("alpha", 1);
        sd.set("beta", 2);
        sd.set("gamma", 3);

        if (sd.isEmpty()) return -3;
        if (sd.length() != 3) return -4;

        if (sd.get("alpha") != 1) return -5;
        if (sd.get("beta") != 2) return -6;
        if (!sd.exists("gamma")) return -7;
        if (sd.exists("delta")) return -8;

        sd.remove("beta");
        if (sd.length() != 2) return -9;
        if (sd.exists("beta")) return -10;

        dict<int, string> id = {};
        id.set(100, "hundred");
        id.set(200, "two hundred");
        id.set(300, "three hundred");

        if (id.length() != 3) return -11;

        if (id.get(200) != "two hundred") return -12;

        id.clear();
        if (!id.isEmpty()) return -13;

        dict<int, int> nd = {};
        nd.set(1, 10);
        nd.set(2, 20);
        nd.set(3, 30);

        int keySum = 0;
        int valSum = 0;
        for (int i = 0; i < nd.length(); i++) {
            keySum += nd.getKey(i);
            valSum += nd.getValue(i);
        }

        if (keySum != 6) return -14;
        if (valSum != 60) return -15;

        nd.set(2, 200);
        if (nd.get(2) != 200) return -16;

        return 1;
    }

    int ComprehensiveMathTest()
    {
        if (math::abs(-5.0f) != 5.0f) return -1;
        if (math::abs(5.0f) != 5.0f) return -2;

        if (math::abs(math::sqrt(4.0f) - 2.0f) > 0.001f) return -3;
        if (math::abs(math::sqrt(9.0f) - 3.0f) > 0.001f) return -4;

        if (math::abs(math::sin(0.0f)) > 0.001f) return -5;
        if (math::abs(math::cos(0.0f) - 1.0f) > 0.001f) return -6;

        if (math::floor(3.7f) != 3.0f) return -7;
        if (math::ceil(3.2f) != 4.0f) return -8;

        if (math::abs(math::pow(2.0f, 3.0f) - 8.0f) > 0.001f) return -9;

        if (math::abs(math::log(1.0f)) > 0.001f) return -10;

        return 1;
    }
}
)"},
            },
            [](string_view message) {
                string message_str = string(message);

                if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos || message_str.find("fatal") != string::npos || message_str.find("Fatal") != string::npos) {
                    throw ScriptSystemException(message_str);
                }
            });
    }

    static auto MakeMetadataWithGenderEnum() -> vector<uint8_t>
    {
        vector<uint8_t> metadata;
        auto writer = DataWriter(metadata);

        writer.Write<uint16_t>(uint16_t {1}); // 1 section

        string_view section_name = "Enum";
        writer.Write<uint16_t>(numeric_cast<uint16_t>(section_name.size()));
        writer.WriteStringBytes(section_name);
        writer.Write<uint32_t>(uint32_t {2}); // 2 entries

        auto write_token = [&](string_view token) {
            writer.Write<uint16_t>(numeric_cast<uint16_t>(token.size()));
            writer.WriteStringBytes(token);
        };
        auto write_enum = [&](string_view name, string_view underlying_type, string_view first_name, string_view first_value, string_view second_name, string_view second_value) {
            writer.Write<uint32_t>(uint32_t {6}); // 6 tokens
            write_token(name);
            write_token(underlying_type);
            write_token(first_name);
            write_token(first_value);
            write_token(second_name);
            write_token(second_value);
        };

        write_enum("GenderType", "uint8", "Male", "0", "Female", "1");
        write_enum("WideEnum", "uint16", "Low", "12", "High", "650");

        return metadata;
    }

    static auto MakeResources() -> FileSystem
    {
        auto metadata_blob = MakeMetadataWithGenderEnum();

        auto compiler_resources_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ScriptBuiltinsCompilerResources");
        compiler_resources_source->AddFile("Metadata.fometa-server", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_resources_source));

        BakerServerEngine proto_engine {compiler_resources};
        hstring critter_type = proto_engine.Hashes.ToHashedString("Critter");
        auto critter_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "UnitTestCr");
        auto script_blob = MakeScriptBinary(compiler_resources);

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ScriptBuiltinsRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
        runtime_source->AddFile("ScriptBuiltins.fopro-bin-server", critter_blob);
        runtime_source->AddFile("ScriptBuiltins.fos-bin-server", script_blob);

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));

        return resources;
    }

    static auto WaitForStart(ptr<ServerEngine> server) -> string
    {
        for (int32_t i = 0; i < 6000; i++) {
            if (server->IsStarted()) {
                return {};
            }
            if (server->IsStartingError()) {
                return "ServerEngine startup failed";
            }

            std::this_thread::sleep_for(std::chrono::milliseconds {10});
        }

        return "ServerEngine startup timed out";
    }

    static auto MakeServerEngine(GlobalSettings& settings) -> refcount_ptr<ServerEngine>
    {
        return SafeAlloc::MakeRefCounted<ServerEngine>(&settings, MakeResources());
    }
}

TEST_CASE("ScriptBuiltinsStringOperations")
{
    auto settings = MakeSettings();
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    auto fn = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    // StringLength
    {
        auto func = server->FindFunc<int32_t, string>(fn("ScriptBuiltins::StringLength"));
        REQUIRE(func);
        REQUIRE(func.Call(string {"hello"}));
        CHECK(func.GetResult() == 5);
        REQUIRE(func.Call(string {""}));
        CHECK(func.GetResult() == 0);
    }

    // StringEmpty
    {
        auto func = server->FindFunc<bool, string>(fn("ScriptBuiltins::StringEmpty"));
        REQUIRE(func);
        REQUIRE(func.Call(string {""}));
        CHECK(func.GetResult() == true);
        REQUIRE(func.Call(string {"x"}));
        CHECK(func.GetResult() == false);
    }

    // StringSubstr
    {
        auto func = server->FindFunc<string, string, int32_t, int32_t>(fn("ScriptBuiltins::StringSubstr"));
        REQUIRE(func);
        REQUIRE(func.Call(string {"Hello, World!"}, 7, 5));
        CHECK(func.GetResult() == "World");
    }

    // StringFindFirst / StringFindLast
    {
        auto func_first = server->FindFunc<int32_t, string, string>(fn("ScriptBuiltins::StringFindFirst"));
        REQUIRE(func_first);
        REQUIRE(func_first.Call(string {"abcabc"}, string {"bc"}));
        CHECK(func_first.GetResult() == 1);

        auto func_last = server->FindFunc<int32_t, string, string>(fn("ScriptBuiltins::StringFindLast"));
        REQUIRE(func_last);
        REQUIRE(func_last.Call(string {"abcabc"}, string {"bc"}));
        CHECK(func_last.GetResult() == 4);
    }

    // StringToLower / StringToUpper
    {
        auto func_lower = server->FindFunc<string, string>(fn("ScriptBuiltins::StringToLower"));
        REQUIRE(func_lower);
        REQUIRE(func_lower.Call(string {"HELLO"}));
        CHECK(func_lower.GetResult() == "hello");

        auto func_upper = server->FindFunc<string, string>(fn("ScriptBuiltins::StringToUpper"));
        REQUIRE(func_upper);
        REQUIRE(func_upper.Call(string {"hello"}));
        CHECK(func_upper.GetResult() == "HELLO");
    }

    // StringToInt / StringToFloat
    {
        auto func_int = server->FindFunc<int32_t, string>(fn("ScriptBuiltins::StringToInt"));
        REQUIRE(func_int);
        REQUIRE(func_int.Call(string {"42"}));
        CHECK(func_int.GetResult() == 42);

        auto func_float = server->FindFunc<float, string>(fn("ScriptBuiltins::StringToFloat"));
        REQUIRE(func_float);
        REQUIRE(func_float.Call(string {"3.14"}));
        CHECK(func_float.GetResult() == Catch::Approx(3.14f).epsilon(0.01f));
    }

    // IntToString / FloatToString
    {
        auto func_int = server->FindFunc<string, int32_t>(fn("ScriptBuiltins::IntToString"));
        REQUIRE(func_int);
        REQUIRE(func_int.Call(42));
        CHECK(func_int.GetResult() == "42");
    }

    // BoolToString
    {
        auto func = server->FindFunc<string, bool>(fn("ScriptBuiltins::BoolToString"));
        REQUIRE(func);
        REQUIRE(func.Call(true));
        CHECK(func.GetResult() == "true");
        REQUIRE(func.Call(false));
        CHECK(func.GetResult() == "false");
    }

    // StringAddBool / BoolAddString
    {
        auto func_add = server->FindFunc<string, string, bool>(fn("ScriptBuiltins::StringAddBool"));
        REQUIRE(func_add);
        REQUIRE(func_add.Call(string {"enabled="}, true));
        CHECK(func_add.GetResult() == "enabled=true");
        REQUIRE(func_add.Call(string {"enabled="}, false));
        CHECK(func_add.GetResult() == "enabled=false");

        auto func_add_r = server->FindFunc<string, bool, string>(fn("ScriptBuiltins::BoolAddString"));
        REQUIRE(func_add_r);
        REQUIRE(func_add_r.Call(true, string {" flag"}));
        CHECK(func_add_r.GetResult() == "true flag");
        REQUIRE(func_add_r.Call(false, string {" flag"}));
        CHECK(func_add_r.GetResult() == "false flag");
    }

    // AssignBoolToString / AddAssignBoolToString
    {
        auto func_assign = server->FindFunc<string, bool>(fn("ScriptBuiltins::AssignBoolToString"));
        REQUIRE(func_assign);
        REQUIRE(func_assign.Call(true));
        CHECK(func_assign.GetResult() == "true");
        REQUIRE(func_assign.Call(false));
        CHECK(func_assign.GetResult() == "false");

        auto func_add_assign = server->FindFunc<string, bool>(fn("ScriptBuiltins::AddAssignBoolToString"));
        REQUIRE(func_add_assign);
        REQUIRE(func_add_assign.Call(true));
        CHECK(func_add_assign.GetResult() == "flag=true");
        REQUIRE(func_add_assign.Call(false));
        CHECK(func_add_assign.GetResult() == "flag=false");
    }

    // StringConcat
    {
        auto func = server->FindFunc<string, string, string>(fn("ScriptBuiltins::StringConcat"));
        REQUIRE(func);
        REQUIRE(func.Call(string {"hello"}, string {" world"}));
        CHECK(func.GetResult() == "hello world");
    }

    // StringEquals / StringNotEquals
    {
        auto func_eq = server->FindFunc<bool, string, string>(fn("ScriptBuiltins::StringEquals"));
        REQUIRE(func_eq);
        REQUIRE(func_eq.Call(string {"abc"}, string {"abc"}));
        CHECK(func_eq.GetResult() == true);
        REQUIRE(func_eq.Call(string {"abc"}, string {"def"}));
        CHECK(func_eq.GetResult() == false);
    }

    // StringCompare
    {
        auto func = server->FindFunc<int32_t, string, string>(fn("ScriptBuiltins::StringCompare"));
        REQUIRE(func);
        REQUIRE(func.Call(string {"aaa"}, string {"bbb"}));
        CHECK(func.GetResult() == -1);
        REQUIRE(func.Call(string {"bbb"}, string {"aaa"}));
        CHECK(func.GetResult() == 1);
        REQUIRE(func.Call(string {"abc"}, string {"abc"}));
        CHECK(func.GetResult() == 0);
    }

    // ComprehensiveStringTest
    {
        auto func = server->FindFunc<int32_t>(fn("ScriptBuiltins::ComprehensiveStringTest"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 1);
    }

    // Enum <-> any conversions store full enum names
    {
        auto func = server->FindFunc<string>(fn("ScriptBuiltins::EnumToAnyString"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == "GenderType::Male");
    }

    {
        auto func = server->FindFunc<string>(fn("ScriptBuiltins::EnumAssignToAnyString"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == "GenderType::Female");
    }

    {
        auto func = server->FindFunc<bool>(fn("ScriptBuiltins::EnumRoundtripFromAny"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == true);
    }

    {
        auto func = server->FindFunc<bool>(fn("ScriptBuiltins::EnumParseFullNameFromAny"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == true);
    }

    {
        auto func = server->FindFunc<void>(fn("ScriptBuiltins::InvalidIntConversionFromAny"));
        REQUIRE(func);

        auto prev_callback = GetExceptionCallback();
        string message;
        string traceback;
        bool fatal = true;
        SetExceptionCallback([&](string_view msg, const CatchedStackTraceData& st, bool is_fatal) {
            message = string(msg);
            traceback = FormatStackTrace(st);
            fatal = is_fatal;
        });
        auto restore_callback = scope_exit([prev = std::move(prev_callback)]() mutable noexcept { SetExceptionCallback(std::move(prev)); });

        CHECK_FALSE(func.Call());
        CHECK(message.find("Invalid int value for any conversion") != string::npos);
        CHECK(message.find("DefinitelyNotANumber") != string::npos);
        CHECK_FALSE(traceback.empty());
        CHECK_FALSE(fatal);
    }

    // Empty any в†’ int must return 0
    {
        auto func = server->FindFunc<int32_t>(fn("ScriptBuiltins::EmptyAnyToInt"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }
}

TEST_CASE("ScriptBuiltinsContainerReferencesAreCollectable")
{
    ScriptMessages messages;
    nptr<AngelScript::asIScriptEngine> engine = MakeAngelScriptEngine(messages);
    auto release_engine = scope_exit([&engine]() noexcept {
        safe_call([&engine] {
            if (engine) {
                engine->ShutDownAndRelease();
            }
        });
    });

    REQUIRE(engine->SetEngineProperty(AngelScript::asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true) >= 0);
    REQUIRE(engine->SetEngineProperty(AngelScript::asEP_ALLOW_UNSAFE_REFERENCES, true) >= 0);
    REQUIRE(engine->SetEngineProperty(AngelScript::asEP_DISALLOW_NULLABLE_TO_NON_NULLABLE, true) >= 0);
    RegisterAngelScriptArray(engine.get());
    RegisterAngelScriptDict(engine.get());

    auto module = RequireScriptModule(engine.get(), messages, "ContainerReferenceModule",
        R"(
funcdef void Callback();

class Node
{
    Node? Peer;
    Node[] Children = {};
    dict<int, Node> Links = {};
    dict<int, Callback> DictCallbacks = {};
    Callback[] ArrayCallbacks = {};

    void OnCallback()
    {
    }

    void Link(Node other)
    {
        Peer = other;
        Children.insertLast(other);
        Links[1] = other;
        Callback callback = Callback(this.OnCallback);
        DictCallbacks[1] = callback;
        ArrayCallbacks.insertLast(callback);
    }
}

class DerivedNode : Node
{
}

class LeafNode : DerivedNode
{
}

void BuildCycle()
{
    Node first = LeafNode();
    Node second = LeafNode();
    first.Link(second);
    second.Link(first);
    LeafNode firstLeaf = cast<LeafNode>(first);
    LeafNode secondLeaf = cast<LeafNode>(second);
}
)");

    RunScriptFunction(engine, module, "void BuildCycle()");
    RequireFullGarbageCollection(engine);
    ShutdownAndCheckGcDiagnostics(engine, messages);
    engine = nullptr;
}

TEST_CASE("ScriptBuiltinsDeferredReceiverTemporaryIsReleased")
{
    ScriptMessages messages;
    nptr<AngelScript::asIScriptEngine> engine = MakeAngelScriptEngine(messages);
    auto release_engine = scope_exit([&engine]() noexcept {
        safe_call([&engine] {
            if (engine) {
                engine->ShutDownAndRelease();
            }
        });
    });

    GcShutdownTracker tracker;
    engine->SetUserData(&tracker);
    REQUIRE(engine->SetEngineProperty(AngelScript::asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true) >= 0);
    REQUIRE(engine->SetEngineProperty(AngelScript::asEP_ALLOW_UNSAFE_REFERENCES, true) >= 0);
    REQUIRE(engine->SetEngineProperty(AngelScript::asEP_DISALLOW_NULLABLE_TO_NON_NULLABLE, true) >= 0);
    REQUIRE(engine->SetEngineProperty(AngelScript::asEP_PROPERTY_ACCESSOR_MODE, 2) >= 0);
    REQUIRE(engine->SetEngineProperty(AngelScript::asEP_BUILD_WITHOUT_LINE_CUES, true) >= 0);
    REQUIRE(engine->SetEngineProperty(AngelScript::asEP_OPTIMIZE_BYTECODE, true) >= 0);
    REQUIRE(engine->RegisterGlobalFunction("void NotifyGcShutdownDestruction()", FO_SCRIPT_FUNC(NotifyGcShutdownDestruction), FO_SCRIPT_FUNC_CONV) >= 0);
    RegisterAngelScriptArray(engine.get());

    auto module = RequireScriptModule(engine.get(), messages, "DeferredReceiverLifetimeModule",
        R"(
class GuiScreen
{
    ~GuiScreen()
    {
        NotifyGcShutdownDestruction();
    }

    bool get_Active() final
    {
        return true;
    }

    GuiScreen? FindHit()
    {
        return this;
    }
}

GuiScreen[] Screens = {};

bool CheckHit()
{
    for (int i = 0; i < Screens.length(); i++) {
        if (Screens[i].Active && Screens[i].FindHit() != null) {
            return true;
        }
    }
    return false;
}

void ExerciseDeferredReceiverLifetime()
{
    GuiScreen screen = GuiScreen();
    Screens.insertLast(screen);

    CheckHit();

    Screens.clear();
}
)");

    RunScriptFunction(engine, module, "void ExerciseDeferredReceiverLifetime()");
    CHECK(tracker.DestructedObjects == 1);
    ShutdownAndCheckGcDiagnostics(engine, messages);
    engine = nullptr;
}

TEST_CASE("ScriptBuiltinsShutdownCollectsDestructorCascade")
{
    ScriptMessages messages;
    nptr<AngelScript::asIScriptEngine> engine = MakeAngelScriptEngine(messages);
    auto release_engine = scope_exit([&engine]() noexcept {
        safe_call([&engine] {
            if (engine) {
                engine->ShutDownAndRelease();
            }
        });
    });

    GcShutdownTracker tracker;
    engine->SetUserData(&tracker);
    REQUIRE(engine->SetEngineProperty(AngelScript::asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true) >= 0);
    REQUIRE(engine->SetEngineProperty(AngelScript::asEP_DISALLOW_NULLABLE_TO_NON_NULLABLE, true) >= 0);
    REQUIRE(engine->RegisterGlobalFunction("void NotifyGcShutdownDestruction()", FO_SCRIPT_FUNC(NotifyGcShutdownDestruction), FO_SCRIPT_FUNC_CONV) >= 0);

    auto module = RequireScriptModule(engine.get(), messages, "GcShutdownCascadeModule",
        R"(
class CascadeNode
{
    CascadeNode? Self;
    int Remaining;

    CascadeNode(int remaining)
    {
        Remaining = remaining;
        Self = this;
    }

    ~CascadeNode()
    {
        NotifyGcShutdownDestruction();

        if (Remaining > 0) {
            CascadeNode next = CascadeNode(Remaining - 1);
        }
    }
}

CascadeNode? Root;

void BuildCascade()
{
    Root = CascadeNode(96);
}
)");

    RunScriptFunction(engine, module, "void BuildCascade()");
    ShutdownAndCheckGcDiagnostics(engine, messages);
    engine = nullptr;
    CHECK(tracker.DestructedObjects == 97);
}

TEST_CASE("ScriptBuiltinsArrayOperations")
{
    auto settings = MakeSettings();
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    auto fn = [&server](string_view name) { return server->Hashes.ToHashedString(name); };
    auto run_throwing_func = [&server, &fn](string_view func_name, string_view expected_message) {
        auto func = server->FindFunc<void>(fn(func_name));
        REQUIRE(func);

        auto prev_callback = GetExceptionCallback();
        string message;
        SetExceptionCallback([&](string_view msg, const CatchedStackTraceData&, bool) { message = string(msg); });
        auto restore_callback = scope_exit([prev = std::move(prev_callback)]() mutable noexcept { SetExceptionCallback(std::move(prev)); });

        CHECK_FALSE(func.Call());
        INFO(func_name);
        INFO(message);
        CHECK(message.find(expected_message) != string::npos);
    };

    // ArrayLength with FindFunc<int, vector<int>>
    {
        auto func = server->FindFunc<int32_t, vector<int32_t>>(fn("ScriptBuiltins::ArrayLength"));
        REQUIRE(func);

        vector<int32_t> arr {1, 2, 3, 4, 5};
        REQUIRE(func.Call(arr));
        CHECK(func.GetResult() == 5);

        vector<int32_t> empty {};
        REQUIRE(func.Call(empty));
        CHECK(func.GetResult() == 0);
    }

    // ArrayEmpty
    {
        auto func = server->FindFunc<bool, vector<int32_t>>(fn("ScriptBuiltins::ArrayEmpty"));
        REQUIRE(func);

        vector<int32_t> arr {1};
        REQUIRE(func.Call(arr));
        CHECK(func.GetResult() == false);

        vector<int32_t> empty {};
        REQUIRE(func.Call(empty));
        CHECK(func.GetResult() == true);
    }

    // ArrayFind
    {
        auto func = server->FindFunc<int32_t, vector<int32_t>, int32_t>(fn("ScriptBuiltins::ArrayFind"));
        REQUIRE(func);

        vector<int32_t> arr {10, 20, 30, 40, 50};
        REQUIRE(func.Call(arr, 30));
        CHECK(func.GetResult() == 2);
        REQUIRE(func.Call(arr, 99));
        CHECK(func.GetResult() == -1);
    }

    // ComprehensiveArrayTest
    {
        auto func = server->FindFunc<int32_t>(fn("ScriptBuiltins::ComprehensiveArrayTest"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 1);
    }

    // NestedArraySum
    {
        auto func = server->FindFunc<int32_t>(fn("ScriptBuiltins::NestedArraySum"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 21);
    }

    // ArrayExtendedMutators
    {
        auto func = server->FindFunc<int32_t>(fn("ScriptBuiltins::ArrayExtendedMutators"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 1);
    }

    // ArrayStringObjectOps
    {
        auto func = server->FindFunc<int32_t>(fn("ScriptBuiltins::ArrayStringObjectOps"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 1);
    }

    // ArrayHandleOps
    {
        auto func = server->FindFunc<int32_t>(fn("ScriptBuiltins::ArrayHandleOps"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 1);
    }

    // ArraySortSubrangeOps
    {
        auto func = server->FindFunc<int32_t>(fn("ScriptBuiltins::ArraySortSubrangeOps"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 1);
    }

    // ArrayEdgeNoopOps
    {
        auto func = server->FindFunc<int32_t>(fn("ScriptBuiltins::ArrayEdgeNoopOps"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 1);
    }

    // ArrayPrimitiveTypeOps
    {
        auto func = server->FindFunc<int32_t>(fn("ScriptBuiltins::ArrayPrimitiveTypeOps"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 1);
    }

    // ArrayConstructorOps
    {
        auto func = server->FindFunc<int32_t>(fn("ScriptBuiltins::ArrayConstructorOps"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 1);
    }

    // ArrayWideEnumOps
    {
        auto func = server->FindFunc<int32_t>(fn("ScriptBuiltins::ArrayWideEnumOps"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 1);
    }

    // ArrayInsertNegativeArrayNoop
    {
        auto func = server->FindFunc<int32_t>(fn("ScriptBuiltins::ArrayInsertNegativeArrayNoop"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 1);
    }

    // ArrayObjectNoCmpEqualsOps
    {
        auto func = server->FindFunc<int32_t>(fn("ScriptBuiltins::ArrayObjectNoCmpEqualsOps"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 1);
    }

    // ArrayObjectNoCmpFindOps
    {
        auto func = server->FindFunc<int32_t>(fn("ScriptBuiltins::ArrayObjectNoCmpFindOps"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 1);
    }

    // ArrayHandleComparatorOps
    {
        auto func = server->FindFunc<int32_t>(fn("ScriptBuiltins::ArrayHandleComparatorOps"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 1);
    }

    // ArrayHandleCmpOnlyOps
    {
        auto func = server->FindFunc<int32_t>(fn("ScriptBuiltins::ArrayHandleCmpOnlyOps"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 1);
    }

    // Direct ScriptArray API coverage
    {
        ScriptMessages messages;
        nptr<AngelScript::asIScriptEngine> as_engine = MakeAngelScriptEngine(messages);
        auto release_engine = scope_exit([&as_engine]() noexcept { safe_call([&as_engine] { as_engine->ShutDownAndRelease(); }); });

        RegisterAngelScriptArray(as_engine.get());

        nptr<AngelScript::asITypeInfo> int_type = as_engine->GetTypeInfoByDecl("array<int>");
        REQUIRE(int_type != nullptr);
        nptr<AngelScript::asITypeInfo> uint_type = as_engine->GetTypeInfoByDecl("array<uint>");
        REQUIRE(uint_type != nullptr);

        RegisterArrayDummyRef(as_engine.get());
        nptr<AngelScript::asITypeInfo> dummy_ref_handle_type = as_engine->GetTypeInfoByDecl("array<ArrayDummyRef@>");
        REQUIRE(dummy_ref_handle_type != nullptr);

        REQUIRE(BuildAngelScriptModule(as_engine.get(), "ArrayGcNodeModule", "class ArrayGcNode { ArrayGcNode@ Next; }\n") >= 0);
        nptr<AngelScript::asIScriptModule> gc_module = as_engine->GetModule("ArrayGcNodeModule", AngelScript::asGM_ONLY_IF_EXISTS);
        REQUIRE(gc_module != nullptr);
        nptr<AngelScript::asITypeInfo> gc_node_type = gc_module->GetTypeInfoByDecl("ArrayGcNode");
        REQUIRE(gc_node_type != nullptr);
        nptr<AngelScript::asITypeInfo> gc_node_handle_type = gc_module->GetTypeInfoByDecl("array<ArrayGcNode@>");
        REQUIRE(gc_node_handle_type != nullptr);
        CHECK((gc_node_handle_type->GetFlags() & AngelScript::asOBJ_GC) != 0);

        REQUIRE(BuildAngelScriptModule(as_engine.get(), "ArrayFinalNodeModule", "final class ArrayFinalNode { int Value; }\n") >= 0);
        nptr<AngelScript::asIScriptModule> final_module = as_engine->GetModule("ArrayFinalNodeModule", AngelScript::asGM_ONLY_IF_EXISTS);
        REQUIRE(final_module != nullptr);
        nptr<AngelScript::asITypeInfo> final_node_type = final_module->GetTypeInfoByDecl("ArrayFinalNode");
        REQUIRE(final_node_type != nullptr);
        CHECK((final_node_type->GetFlags() & AngelScript::asOBJ_NOINHERIT) != 0);
        nptr<AngelScript::asITypeInfo> final_node_handle_type = final_module->GetTypeInfoByDecl("array<ArrayFinalNode@>");
        REQUIRE(final_node_handle_type != nullptr);
        CHECK((final_node_handle_type->GetFlags() & AngelScript::asOBJ_GC) == 0);

        RegisterArrayComparableValue(as_engine.get(), "ArrayNoCompareValue", false, false);
        nptr<AngelScript::asITypeInfo> no_compare_value_type = as_engine->GetTypeInfoByDecl("array<ArrayNoCompareValue>");
        REQUIRE(no_compare_value_type != nullptr);

        RegisterArrayComparableValue(as_engine.get(), "ArrayMultiEqualsValue", true, false);
        nptr<AngelScript::asITypeInfo> multi_equals_value_type = as_engine->GetTypeInfoByDecl("array<ArrayMultiEqualsValue>");
        REQUIRE(multi_equals_value_type != nullptr);

        RegisterArrayComparableValue(as_engine.get(), "ArrayMultiCmpValue", false, true);
        nptr<AngelScript::asITypeInfo> multi_cmp_value_type = as_engine->GetTypeInfoByDecl("array<ArrayMultiCmpValue>");
        REQUIRE(multi_cmp_value_type != nullptr);

        RegisterArrayCmpOnlyValue(as_engine.get(), "ArrayCmpOnlyNativeValue");
        REQUIRE(as_engine->RegisterGlobalFunction("bool CheckArrayCmpOnlyValueOps()", FO_SCRIPT_FUNC(CheckArrayCmpOnlyValueOps), FO_SCRIPT_FUNC_CONV) >= 0);
        REQUIRE(BuildAngelScriptModule(as_engine.get(), "ArrayCmpOnlyValueOpsModule", "bool RunArrayCmpOnlyValueOps() { return CheckArrayCmpOnlyValueOps(); }\n") >= 0);
        nptr<AngelScript::asIScriptModule> cmp_only_module = as_engine->GetModule("ArrayCmpOnlyValueOpsModule", AngelScript::asGM_ONLY_IF_EXISTS);
        REQUIRE(cmp_only_module != nullptr);
        nptr<AngelScript::asIScriptFunction> cmp_only_func = cmp_only_module->GetFunctionByDecl("bool RunArrayCmpOnlyValueOps()");
        REQUIRE(cmp_only_func != nullptr);

        RegisterArrayComparatorFilterValue(as_engine.get(), "ArrayCmpParamMismatchValue", "int opCmp(const ArrayCmpOnlyNativeValue &in) const", FO_SCRIPT_FUNC_THIS(ArrayComparableValueCmp));
        nptr<AngelScript::asITypeInfo> param_mismatch_value_type = as_engine->GetTypeInfoByDecl("array<ArrayCmpParamMismatchValue>");
        REQUIRE(param_mismatch_value_type != nullptr);

        RegisterArrayComparatorFilterValue(as_engine.get(), "ArrayCmpByValueParamValue", "int opCmp(ArrayCmpByValueParamValue) const", FO_SCRIPT_FUNC_THIS(ArrayComparableValueCmpByValue));
        nptr<AngelScript::asITypeInfo> by_value_param_type = as_engine->GetTypeInfoByDecl("array<ArrayCmpByValueParamValue>");
        REQUIRE(by_value_param_type != nullptr);

        RegisterArrayComparatorFilterValue(as_engine.get(), "ArrayCmpOutRefParamValue", "int opCmp(ArrayCmpOutRefParamValue &out) const", FO_SCRIPT_FUNC_THIS(ArrayComparableValueCmpMutable));
        nptr<AngelScript::asITypeInfo> out_ref_param_type = as_engine->GetTypeInfoByDecl("array<ArrayCmpOutRefParamValue>");
        REQUIRE(out_ref_param_type != nullptr);

        auto int_arr = ScriptArray::Create(int_type.get(), 2);

        auto uint_arr = ScriptArray::Create(uint_type.get(), 1);

        auto dummy_ref_arr = ScriptArray::Create(dummy_ref_handle_type.get(), 1);

        CHECK(int_arr->GetArrayObjectType() == int_type);
        CHECK(static_cast<const ScriptArray&>(*int_arr).GetArrayObjectType() == int_type.get());
        CHECK(int_arr->GetArrayTypeId() == int_type->GetTypeId());
        CHECK(int_arr->GetElementTypeId() == AngelScript::asTYPEID_INT32);
        CHECK(int_arr->GetRefCount() == 1);

        int_arr->SetFlag();
        CHECK(int_arr->GetFlag());
        int_arr->AddRef();
        CHECK(int_arr->GetRefCount() == 2);
        int_arr->Release();
        CHECK(int_arr->GetRefCount() == 1);

        int32_t first_int_value = 17;
        int32_t second_int_value = 29;
        int_arr->SetValue(0, &first_int_value);
        int_arr->SetValue(1, &second_int_value);
        {
            auto copied_int_arr = SafeAlloc::MakeRefCounted<ScriptArray>(*int_arr);
            CHECK(copied_int_arr->GetSize() == int_arr->GetSize());
            CHECK(*copied_int_arr->AtAs<int32_t>(0) == first_int_value);
            CHECK(*copied_int_arr->AtAs<int32_t>(1) == second_int_value);
            CHECK(*copied_int_arr == *int_arr);
        }

        CHECK_THROWS_AS((*int_arr = *uint_arr), ScriptException);
        CHECK_FALSE(*int_arr == *uint_arr);
        {
            auto smaller_int_arr = ScriptArray::Create(int_type.get(), 1);
            CHECK_FALSE(*int_arr == *smaller_int_arr);
        }
        CHECK_THROWS_AS(int_arr->InsertAt(-1, &first_int_value), ScriptException);
        CHECK_THROWS_AS(int_arr->InsertAt(0, *uint_arr), ScriptException);
        CHECK_THROWS_AS(int_arr->InsertAt(-1, *int_arr), ScriptException);
        CHECK_THROWS_AS(int_arr->InsertAt(3, *int_arr), ScriptException);
        CHECK_THROWS_AS(int_arr->RemoveAt(int_arr->GetSize()), ScriptException);
        CHECK_THROWS_AS(int_arr->SortAsc(-1, 2), ScriptException);
        CHECK_THROWS_AS(int_arr->SortDesc(int_arr->GetSize(), 2), ScriptException);

        ArrayDummyRef first_dummy_ref;
        void* first_dummy_ref_handle = &first_dummy_ref;
        dummy_ref_arr->SetValue(0, &first_dummy_ref_handle);
        CHECK(first_dummy_ref.RefCount == 1);
        {
            auto copied_dummy_ref_arr = SafeAlloc::MakeRefCounted<ScriptArray>(*dummy_ref_arr);
            CHECK(copied_dummy_ref_arr->GetSize() == dummy_ref_arr->GetSize());
            CHECK(first_dummy_ref.RefCount == 2);
            CHECK(copied_dummy_ref_arr->FindByRef(&first_dummy_ref_handle) == 0);
        }
        CHECK(first_dummy_ref.RefCount == 1);

        void* gc_node = as_engine->CreateScriptObject(gc_node_type.get());
        REQUIRE(gc_node != nullptr);
        auto release_gc_node = scope_exit([&]() noexcept { safe_call([&] { as_engine->ReleaseScriptObject(gc_node, gc_node_type.get()); }); });

        auto gc_handle_arr = ScriptArray::Create(gc_node_handle_type.get(), 1);

        auto final_handle_arr = ScriptArray::Create(final_node_handle_type.get(), 0);
        CHECK(final_handle_arr->IsEmpty());

        void* gc_node_handle = gc_node;
        gc_handle_arr->SetValue(0, &gc_node_handle);
        CHECK(as_engine->GarbageCollect(AngelScript::asGC_FULL_CYCLE) >= 0);

        {
            auto defaulted_gc_handle_arr = ScriptArray::Create(gc_node_handle_type.get(), 1, &gc_node_handle);

            auto copied_gc_handle_arr = SafeAlloc::MakeRefCounted<ScriptArray>(*defaulted_gc_handle_arr);
            CHECK(copied_gc_handle_arr->GetSize() == defaulted_gc_handle_arr->GetSize());
            CHECK(copied_gc_handle_arr->FindByRef(&gc_node_handle) == 0);
        }

        gc_handle_arr->ReleaseAllHandles();
        CHECK(gc_handle_arr->IsEmpty());

        ArrayDummyRef second_dummy_ref;
        void* second_dummy_ref_handle = &second_dummy_ref;
        dummy_ref_arr->SetValue(0, &second_dummy_ref_handle);
        CHECK(first_dummy_ref.RefCount == 0);
        CHECK(second_dummy_ref.RefCount == 1);

        dummy_ref_arr->Resize(0);
        CHECK(second_dummy_ref.RefCount == 0);

        CheckPrimitiveScriptArrayDirectOps(as_engine.get(), "bool", false, true);
        CheckPrimitiveScriptArrayDirectOps<int8_t>(as_engine.get(), "int8", -2, 3);
        CheckPrimitiveScriptArrayDirectOps<uint8_t>(as_engine.get(), "uint8", 1, 4);
        CheckPrimitiveScriptArrayDirectOps<int16_t>(as_engine.get(), "int16", -200, 1200);
        CheckPrimitiveScriptArrayDirectOps<uint16_t>(as_engine.get(), "uint16", 12, 650);
        CheckPrimitiveScriptArrayDirectOps<uint32_t>(as_engine.get(), "uint", 8, 90);
        CheckPrimitiveScriptArrayDirectOps<int64_t>(as_engine.get(), "int64", 500, 700);
        CheckPrimitiveScriptArrayDirectOps<uint64_t>(as_engine.get(), "uint64", 5, 900);
        CheckPrimitiveScriptArrayDirectOps<float32_t>(as_engine.get(), "float", -1.5f, 2.25f);
        CheckPrimitiveScriptArrayDirectOps<float64_t>(as_engine.get(), "double", -3.5, 4.75);

        {
            auto no_compare_values = ScriptArray::Create(no_compare_value_type.get(), 1);
            auto no_compare_other = ScriptArray::Create(no_compare_value_type.get(), 1);

            ArrayComparableValue low_value {1};
            ArrayComparableValue high_value {3};
            no_compare_values->SetValue(0, &low_value);
            no_compare_other->SetValue(0, &high_value);

            CHECK_THROWS_WITH((*no_compare_values == *no_compare_other), Catch::Matchers::ContainsSubstring("Type does not have a matching opEquals or opCmp method"));
            CHECK_THROWS_WITH(no_compare_values->Find(&high_value), Catch::Matchers::ContainsSubstring("Type does not have a matching opEquals or opCmp method"));
        }

        {
            auto multi_equals_values = ScriptArray::Create(multi_equals_value_type.get(), 1);
            auto multi_equals_other = ScriptArray::Create(multi_equals_value_type.get(), 1);

            ArrayComparableValue first_value {1};
            ArrayComparableValue same_value {1};
            multi_equals_values->SetValue(0, &first_value);
            multi_equals_other->SetValue(0, &same_value);

            CHECK_THROWS_WITH((*multi_equals_values == *multi_equals_other), Catch::Matchers::ContainsSubstring("Type has multiple matching opEquals or opCmp methods"));
            CHECK_THROWS_WITH(multi_equals_values->Find(&same_value), Catch::Matchers::ContainsSubstring("Type has multiple matching opEquals or opCmp methods"));
        }

        {
            auto multi_cmp_values = ScriptArray::Create(multi_cmp_value_type.get(), 2);

            ArrayComparableValue low_value {1};
            ArrayComparableValue high_value {3};
            multi_cmp_values->SetValue(0, &high_value);
            multi_cmp_values->SetValue(1, &low_value);

            CHECK_THROWS_WITH(multi_cmp_values->SortAsc(), Catch::Matchers::ContainsSubstring("Type has multiple matching opCmp methods"));
        }

        {
            nptr<AngelScript::asIScriptContext> ctx = as_engine->CreateContext();
            REQUIRE(ctx != nullptr);
            auto release_ctx = scope_exit([&ctx]() noexcept { safe_call([&ctx] { ctx->Release(); }); });

            REQUIRE(ctx->Prepare(cmp_only_func.get()) >= 0);
            REQUIRE(ctx->Execute() == AngelScript::asEXECUTION_FINISHED);
            CHECK(ctx->GetReturnByte() != 0);
        }

        for (nptr<AngelScript::asITypeInfo> filtered_type : {param_mismatch_value_type, by_value_param_type, out_ref_param_type}) {
            auto filtered_values = ScriptArray::Create(filtered_type.get(), 1);
            auto filtered_other = ScriptArray::Create(filtered_type.get(), 1);

            ArrayComparableValue low_value {1};
            ArrayComparableValue high_value {3};
            filtered_values->SetValue(0, &low_value);
            filtered_other->SetValue(0, &high_value);

            CHECK_THROWS_WITH((*filtered_values == *filtered_other), Catch::Matchers::ContainsSubstring("Type does not have a matching opEquals or opCmp method"));
        }

        int_arr->EnumReferences(as_engine);
        int_arr->ReleaseAllHandles();
        CHECK(int_arr->IsEmpty());
    }

    // Array template callback diagnostics
    {
        ScriptMessages messages;
        nptr<AngelScript::asIScriptEngine> as_engine = MakeAngelScriptEngine(messages);
        auto release_engine = scope_exit([&as_engine]() noexcept { safe_call([&as_engine] { as_engine->ShutDownAndRelease(); }); });

        RegisterAngelScriptArray(as_engine.get());

        CHECK(BuildAngelScriptModule(as_engine.get(), "ArrayVoidRejected", "void Test() { array<void> values; }\n") < 0);
        CHECK(!messages.Entries.empty());
        messages.Entries.clear();
        RegisterArrayNoDefaultValue(as_engine.get());
        CHECK(BuildAngelScriptModule(as_engine.get(), "ArrayNativeNoDefaultValueRejected", "void Test() { array<ArrayNoDefaultValue> values; }\n") < 0);
        CHECK(HasScriptMessage(messages, "The subtype has no default constructor"));
        messages.Entries.clear();
        CHECK(BuildAngelScriptModule(as_engine.get(), "ArrayNoDefaultCtorRejected", "class NoDefault { NoDefault(int) {} }\nvoid Test() { array<NoDefault> values; }\n") < 0);
        CHECK(!messages.Entries.empty());
    }

    {
        ScriptMessages messages;
        nptr<AngelScript::asIScriptEngine> as_engine = MakeAngelScriptEngine(messages);
        auto release_engine = scope_exit([&as_engine]() noexcept { safe_call([&as_engine] { as_engine->ShutDownAndRelease(); }); });

        RegisterAngelScriptArray(as_engine.get());
        REQUIRE(as_engine->RegisterObjectType("NativeRef", 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOCOUNT) >= 0);

        CHECK(BuildAngelScriptModule(as_engine.get(), "ArrayRefValueRejected", "void Test() { array<NativeRef> values; }\n") < 0);
        CHECK(HasScriptMessage(messages, "Can't store references in array"));
    }

    run_throwing_func("ScriptBuiltins::ArrayNegativeSizeThrows", "Negative array size");
    run_throwing_func("ScriptBuiltins::ArrayTooLargeThrows", "Array size is too large");
    run_throwing_func("ScriptBuiltins::ArrayIndexOutOfBoundsThrows", "Index out of bounds");
    run_throwing_func("ScriptBuiltins::ArrayInsertOutOfBoundsThrows", "Index out of bounds");
    run_throwing_func("ScriptBuiltins::ArrayRemoveLastEmptyThrows", "Index out of bounds");
    run_throwing_func("ScriptBuiltins::ArrayFirstEmptyThrows", "Index out of bounds");
    run_throwing_func("ScriptBuiltins::ArrayLastEmptyThrows", "Index out of bounds");
    run_throwing_func("ScriptBuiltins::ArrayRemoveRangeOutOfBoundsThrows", "Index out of bounds");
    run_throwing_func("ScriptBuiltins::ArraySortRangeOutOfBoundsThrows", "Index out of bounds");
    run_throwing_func("ScriptBuiltins::ArrayReduceTooMuchThrows", "Array size is less than reduce count");
    run_throwing_func("ScriptBuiltins::ArraySetNullThrows", "Array arg is null");
    run_throwing_func("ScriptBuiltins::ArrayInsertFirstNullThrows", "Array arg is null");
    run_throwing_func("ScriptBuiltins::ArrayInsertLastNullThrows", "Array arg is null");
    run_throwing_func("ScriptBuiltins::ArrayInsertAtNullThrows", "Array arg is null");
    run_throwing_func("ScriptBuiltins::ArrayEqualsNullThrows", "Array arg is null");
    run_throwing_func("ScriptBuiltins::ArrayFactoryNullThrows", "Array arg is null");
    run_throwing_func("ScriptBuiltins::ArrayObjectNoCmpSortThrows", "Type does not have a matching opCmp method");
    run_throwing_func("ScriptBuiltins::ArrayHandleInRefComparatorThrows", "Type does not have a matching opCmp method");
    run_throwing_func("ScriptBuiltins::ArrayConstHandleComparatorThrows", "Type does not have a matching opCmp method");
}

TEST_CASE("ScriptBuiltinsDictOperations")
{
    auto settings = MakeSettings();
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    auto fn = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    // DictLength
    {
        auto func = server->FindFunc<int32_t>(fn("ScriptBuiltins::DictLength"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 3);
    }

    // DictEmpty
    {
        auto func = server->FindFunc<bool>(fn("ScriptBuiltins::DictEmpty"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == true);
    }

    // DictGetSet
    {
        auto func = server->FindFunc<int32_t>(fn("ScriptBuiltins::DictGetSet"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 60);
    }

    // DictExistsRemove
    {
        auto func = server->FindFunc<bool>(fn("ScriptBuiltins::DictExistsRemove"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == true);
    }

    // DictClear
    {
        auto func = server->FindFunc<int32_t>(fn("ScriptBuiltins::DictClear"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    // DictIntKeys
    {
        auto func = server->FindFunc<int32_t>(fn("ScriptBuiltins::DictIntKeys"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 660);
    }

    // ComprehensiveDictTest
    {
        auto func = server->FindFunc<int32_t>(fn("ScriptBuiltins::ComprehensiveDictTest"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 1);
    }
}

TEST_CASE("ScriptBuiltinsMathAndTypeOperations")
{
    auto settings = MakeSettings();
    auto server = MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    auto fn = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    // ComprehensiveMathTest
    {
        auto func = server->FindFunc<int32_t>(fn("ScriptBuiltins::ComprehensiveMathTest"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 1);
    }

    // HstringOps
    {
        auto func = server->FindFunc<bool>(fn("ScriptBuiltins::HstringOps"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == true);
    }

    // GameLogWorks
    {
        auto func = server->FindFunc<bool>(fn("ScriptBuiltins::GameLogWorks"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == true);
    }
}

FO_END_NAMESPACE
