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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

    static void ArrayNoDefaultValueDestruct(void* obj)
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
        return s.findLast(sub);
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
        // passed as the second dense native x86 argument to array<T>::insertAt
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

    class DictNoCompareKey
    {
        int Value = 0;
    }

    class DictComparableKey
    {
        int Value = 0;

        int opCmp(const DictComparableKey? other) const
        {
            if (other is null) return 1;
            if (Value < other.Value) return -1;
            if (Value > other.Value) return 1;
            return 0;
        }

        bool opEquals(const DictComparableKey? other) const
        {
            return other !is null && Value == other.Value;
        }
    }

    int DictAccessorOps()
    {
        dict<string, int> d = {};
        d.set("a", 1);

        if (d["a"] != 1) return -1;

        d["b"] = 5;
        if (d.length() != 2) return -2;
        if (d["b"] != 5) return -3;

        d.setIfNotExist("a", 100);
        d.setIfNotExist("c", 7);
        if (d["a"] != 1) return -4;
        if (d["c"] != 7) return -5;

        if (d.get("a", 42) != 1) return -6;
        if (d.get("missing", 42) != 42) return -7;

        if (d.remove("missing")) return -8;
        if (!d.remove("c")) return -9;
        if (d.length() != 2) return -10;

        return 1;
    }

    int DictKeysValuesOps()
    {
        dict<int, int> d = {};
        d.set(3, 30);
        d.set(1, 10);
        d.set(2, 20);

        int[] keys = d.getKeys();
        int[] values = d.getValues();

        if (keys.length() != 3) return -1;
        if (values.length() != 3) return -2;
        if (keys[0] != 1 || keys[1] != 2 || keys[2] != 3) return -3;
        if (values[0] != 10 || values[1] != 20 || values[2] != 30) return -4;

        dict<string, string> sd = {};
        sd.set("k", "v");

        string[] string_keys = sd.getKeys();
        string[] string_values = sd.getValues();
        if (string_keys.length() != 1 || string_keys[0] != "k") return -5;
        if (string_values.length() != 1 || string_values[0] != "v") return -6;

        dict<int, int> empty = {};
        if (empty.getKeys().length() != 0) return -7;
        if (empty.getValues().length() != 0) return -8;

        return 1;
    }

    int DictInitListOps()
    {
        dict<string, int> d = {{"a", 1}, {"b", 2}, {"c", 3}};
        if (d.length() != 3) return -1;
        if (d.get("a") != 1 || d.get("b") != 2 || d.get("c") != 3) return -2;

        dict<int, string> mixed = {{1, "one"}, {2, "two"}};
        if (mixed.length() != 2) return -3;
        if (mixed.get(2) != "two") return -4;

        dict<int, int64> wide = {{1, 5}, {2, 9}};
        if (wide.get(2) != 9) return -5;

        return 1;
    }

    int DictCloneEqualsOps()
    {
        dict<string, int> d = {};
        d.set("a", 1);
        d.set("b", 2);

        dict<string, int> clone = d.clone();
        if (clone.length() != 2) return -1;
        if (!(clone == d)) return -2;

        clone.set("b", 20);
        if (clone == d) return -3;
        if (d.get("b") != 2) return -4;

        dict<string, int> copy = dict<string, int>(d);
        if (!(copy == d)) return -5;

        copy.set("c", 3);
        if (copy == d) return -6;
        if (d.length() != 2) return -7;

        dict<string, int> other_keys = {};
        other_keys.set("a", 1);
        other_keys.set("z", 2);
        if (other_keys == d) return -8;

        dict<string, int> empty = {};
        if (empty == d) return -9;
        if (!(d == d)) return -10;

        return 1;
    }

    int DictRemoveValuesOps()
    {
        dict<int, int> d = {};
        d.set(1, 7);
        d.set(2, 8);
        d.set(3, 7);

        if (d.removeValues(7) != 2) return -1;
        if (d.length() != 1) return -2;
        if (!d.exists(2)) return -3;
        if (d.removeValues(99) != 0) return -4;

        dict<int, string> sd = {};
        sd.set(1, "keep");
        sd.set(2, "drop");
        sd.set(3, "drop");
        if (sd.removeValues("drop") != 2) return -5;
        if (sd.length() != 1) return -6;

        return 1;
    }

    int DictWideIntegerOps()
    {
        // Keys and values that share their low 32 bits must stay distinct
        int64 base = 4294967296;
        int64 first = base + 1;
        int64 second = base * 2 + 1;

        dict<int64, int64> d = {};
        d.set(first, first);
        d.set(second, second);

        if (d.length() != 2) return -1;
        if (d.get(first) != first) return -2;
        if (d.get(second) != second) return -3;

        dict<int64, int64> other = {};
        other.set(first, first);
        other.set(second, first);
        if (d == other) return -4;

        if (d.removeValues(second) != 1) return -5;
        if (d.length() != 1) return -6;
        if (!d.exists(first)) return -7;

        uint64 ubase = 4294967296;
        dict<uint64, uint64> ud = {};
        ud.set(ubase + 5, ubase + 5);
        ud.set(ubase * 2 + 5, ubase * 2 + 5);
        if (ud.removeValues(ubase + 5) != 1) return -8;
        if (ud.length() != 1) return -9;

        return 1;
    }

    int ArrayWideIntegerOps()
    {
        int64 base = 4294967296;
        int64 first = base + 1;
        int64 second = base * 2 + 1;

        int64[] values = {first, second};
        if (values.find(second) != 1) return -1;
        if (values.find(base * 3 + 1) != -1) return -2;

        int64[] same = {first, second};
        if (!(values == same)) return -3;

        int64[] different = {first, base * 3 + 1};
        if (values == different) return -4;

        values.sortDesc();
        if (values[0] != second || values[1] != first) return -5;

        uint64 ubase = 4294967296;
        uint64[] uvalues = {ubase * 2 + 5, ubase + 5};
        uvalues.sortAsc();
        if (uvalues[0] != ubase + 5 || uvalues[1] != ubase * 2 + 5) return -6;
        if (uvalues.find(ubase * 2 + 5) != 1) return -7;
        if (uvalues.find(ubase * 3 + 5) != -1) return -8;

        return 1;
    }

    int DictEnumKeyOps()
    {
        dict<GenderType, int> gd = {};
        gd.set(GenderType::Male, 1);
        gd.set(GenderType::Female, 2);
        if (gd.length() != 2) return -1;
        if (gd.get(GenderType::Female) != 2) return -2;

        dict<GenderType, int> gd_same = {};
        gd_same.set(GenderType::Male, 1);
        gd_same.set(GenderType::Female, 2);
        if (!(gd == gd_same)) return -3;

        gd_same.set(GenderType::Female, 3);
        if (gd == gd_same) return -4;

        dict<WideEnum, int> wd = {};
        wd.set(WideEnum::High, 20);
        wd.set(WideEnum::Low, 10);
        if (wd.getKey(0) != WideEnum::Low) return -5;
        if (wd.getValue(1) != 20) return -6;

        dict<int, WideEnum> wv = {};
        wv.set(1, WideEnum::Low);
        wv.set(2, WideEnum::High);
        if (wv.removeValues(WideEnum::High) != 1) return -7;
        if (wv.length() != 1) return -8;

        return 1;
    }

    int DictHandleValueOps()
    {
        DictComparableKey a = DictComparableKey();
        a.Value = 1;
        DictComparableKey b = DictComparableKey();
        b.Value = 2;

        dict<int, DictComparableKey> d = {};
        d.set(1, a);
        d.set(2, b);

        if (d.length() != 2) return -1;
        if (d.get(1).Value != 1) return -2;

        dict<int, DictComparableKey> same = {};
        same.set(1, a);
        same.set(2, b);
        if (!(d == same)) return -3;

        same.set(2, a);
        if (d == same) return -4;

        DictComparableKey[] values = d.getValues();
        if (values.length() != 2) return -8;
        if (values[0].Value != 1 || values[1].Value != 2) return -9;

        if (d.removeValues(b) != 1) return -5;
        if (d.length() != 1) return -6;

        d.clear();
        if (!d.isEmpty()) return -7;

        return 1;
    }

    int DictHandleKeyOps()
    {
        DictComparableKey low = DictComparableKey();
        low.Value = 1;
        DictComparableKey high = DictComparableKey();
        high.Value = 3;

        dict<DictComparableKey, int> d = {};
        d.set(low, 10);
        d.set(high, 30);

        if (d.length() != 2) return -1;
        if (d.get(low) != 10) return -2;
        if (!d.exists(high)) return -3;

        DictComparableKey[] keys = d.getKeys();
        if (keys.length() != 2) return -8;
        if (keys[0].Value != 1 || keys[1].Value != 3) return -9;

        DictComparableKey same_low = DictComparableKey();
        same_low.Value = 1;
        if (!d.exists(same_low)) return -4;
        if (d.get(same_low) != 10) return -5;

        DictComparableKey missing = DictComparableKey();
        missing.Value = 9;
        if (d.exists(missing)) return -6;

        if (!d.remove(same_low)) return -7;
        if (d.length() != 1) return -8;

        return 1;
    }

    void DictMissingKeyThrows()
    {
        dict<string, int> d = {};
)"
                    R"(        int value = d.get("missing");
    }

    void DictKeyIndexOutOfBoundsThrows()
    {
        dict<string, int> d = {};
        d.set("a", 1);
        string key = d.getKey(1);
    }

    void DictValueIndexOutOfBoundsThrows()
    {
        dict<string, int> d = {};
        d.set("a", 1);
        int value = d.getValue(-1);
    }

    void DictNoCompareKeyThrows()
    {
        DictNoCompareKey first = DictNoCompareKey();
        DictNoCompareKey second = DictNoCompareKey();
        dict<DictNoCompareKey, int> d = {};
        d.set(first, 1);
        d.set(second, 2);
    }

    // === Entity property conversion operations ===

    int PropertyScalarConversionOps()
    {
        Critter cr = Game.CreateCritter("UnitTestCr".hstr(), false);

        cr.TestString = "value";
        if (cr.TestString != "value") return -1;
        cr.TestString = "";
        if (!cr.TestString.isEmpty()) return -2;

        cr.TestHash = "hashed".hstr();
        if (cr.TestHash != "hashed".hstr()) return -3;

        cr.TestEnum = GenderType::Female;
        if (cr.TestEnum != GenderType::Female) return -4;
        cr.TestEnum = GenderType::Male;
        if (cr.TestEnum != GenderType::Male) return -5;

        cr.TestAny = any("boxed");
        if (string(cr.TestAny) != "boxed") return -6;

        cr.TestProto = Game.GetProtoCritter("UnitTestCr".hstr());
        if (cr.TestProto.ProtoId != "UnitTestCr".hstr()) return -7;

        Game.DestroyCritter(cr);
        return 1;
    }

    int PropertyArrayConversionOps()
    {
        Critter cr = Game.CreateCritter("UnitTestCr".hstr(), false);

        cr.TestIntArray = {1, 2, 3};
        array<int> ints = cr.TestIntArray;
        if (ints.length() != 3) return -1;
        if (ints[0] != 1 || ints[2] != 3) return -2;

        cr.TestIntArray = {};
        if (!cr.TestIntArray.isEmpty()) return -3;

        cr.TestFloatArray = {1.5f, 2.5f};
        array<float> floats = cr.TestFloatArray;
        if (floats.length() != 2) return -4;
        if (floats[1] != 2.5f) return -5;

        cr.TestStringArray = {"a", "", "ccc"};
        array<string> strings = cr.TestStringArray;
        if (strings.length() != 3) return -6;
        if (strings[0] != "a" || !strings[1].isEmpty() || strings[2] != "ccc") return -7;

        cr.TestHashArray = {"one".hstr(), "two".hstr()};
        array<hstring> hashes = cr.TestHashArray;
        if (hashes.length() != 2) return -8;
        if (hashes[1] != "two".hstr()) return -9;

        cr.TestEnumArray = {GenderType::Female, GenderType::Male};
        array<GenderType> enums = cr.TestEnumArray;
        if (enums.length() != 2) return -10;
        if (enums[0] != GenderType::Female) return -11;

        cr.TestAnyArray = {any("first"), any("second")};
        array<any> anys = cr.TestAnyArray;
        if (anys.length() != 2) return -12;
        if (string(anys[1]) != "second") return -13;

        // The remaining scalar widths and value types each take their own converter branch
        cr.TestInt8 = 7;
        if (cr.TestInt8 != 7) return -30;

        cr.TestInt16 = 300;
        if (cr.TestInt16 != 300) return -31;

        cr.TestInt64 = 5000000000;
        if (cr.TestInt64 != 5000000000) return -32;

        cr.TestUInt16 = 65000;
        if (cr.TestUInt16 != 65000) return -33;

        cr.TestUInt32 = 4000000000;
        if (cr.TestUInt32 != 4000000000) return -34;

        cr.TestUInt64 = 9000000000;
        if (cr.TestUInt64 != 9000000000) return -35;

        cr.TestBoolArray = {true, false, true};
        array<bool> bools = cr.TestBoolArray;
        if (bools.length() != 3 || !bools[0] || bools[1]) return -36;

        cr.TestInt64Array = {1, 5000000000};
        array<int64> int64s = cr.TestInt64Array;
        if (int64s.length() != 2 || int64s[1] != 5000000000) return -37;

        cr.TestIdent = cr.Id;
        if (cr.TestIdent != cr.Id) return -38;

        cr.TestIdentArray = {cr.Id};
        array<ident> idents = cr.TestIdentArray;
        if (idents.length() != 1 || idents[0] != cr.Id) return -39;

        cr.TestTimeSpan = timespan(5, 3);
        if (cr.TestTimeSpan != timespan(5, 3)) return -40;

        cr.TestColor = ucolor(1, 2, 3, 4);
        if (cr.TestColor != ucolor(1, 2, 3, 4)) return -41;

        cr.TestColorArray = {ucolor(1, 2, 3, 4), ucolor(5, 6, 7, 8)};
        array<ucolor> colors = cr.TestColorArray;
        if (colors.length() != 2 || colors[1] != ucolor(5, 6, 7, 8)) return -42;

        dict<string, string> stringStrings = dict<string, string>();
        stringStrings.set("k", "v");
        cr.TestStringStringDict = stringStrings;
        if (cr.TestStringStringDict.get("k") != "v") return -43;

        dict<hstring, string> hashStrings = dict<hstring, string>();
        hashStrings.set("hk".hstr(), "hv");
        cr.TestHashStringDict = hashStrings;
        if (cr.TestHashStringDict.get("hk".hstr()) != "hv") return -44;

        dict<int, hstring> hashValues = dict<int, hstring>();
        hashValues.set(1, "hv".hstr());
        cr.TestHashValueDict = hashValues;
        if (cr.TestHashValueDict.get(1) != "hv".hstr()) return -45;

        cr.TestProtoArray = {Game.GetProtoCritter("UnitTestCr".hstr())};
        array<ProtoCritter> protos = cr.TestProtoArray;
        if (protos.length() != 1) return -14;
        if (protos[0].ProtoId != "UnitTestCr".hstr()) return -15;

        // The geometry, time and float value types each take their own converter branch as well
        cr.TestNanoTime = Game.FrameTime;
        if (cr.TestNanoTime != Game.FrameTime) return -46;

        cr.TestSyncTime = Game.SynchronizedTime;
        if (cr.TestSyncTime != Game.SynchronizedTime) return -47;

        cr.TestMapPos = mpos(3, 4);
        if (cr.TestMapPos != mpos(3, 4)) return -48;

        // msize is the one geometry value type with no component constructor, so it is built field by field
        msize mapSize;
        mapSize.width = 20;
        mapSize.height = 30;
        cr.TestMapSize = mapSize;
        if (cr.TestMapSize.width != 20 || cr.TestMapSize.height != 30) return -49;

        cr.TestIntPos = ipos(-5, 6);
        if (cr.TestIntPos != ipos(-5, 6)) return -50;

        cr.TestIntSize = isize(7, 8);
        if (cr.TestIntSize != isize(7, 8)) return -51;

        cr.TestIntRect = irect(1, 2, 3, 4);
        if (cr.TestIntRect != irect(1, 2, 3, 4)) return -52;

        cr.TestFloatPos = fpos(1.5f, 2.5f);
        if (cr.TestFloatPos != fpos(1.5f, 2.5f)) return -53;

        cr.TestFloatSize = fsize(3.5f, 4.5f);
        if (cr.TestFloatSize != fsize(3.5f, 4.5f)) return -54;

        cr.TestFloat64 = 1.25;
        if (cr.TestFloat64 != 1.25) return -55;

        cr.TestMapPosArray = {mpos(1, 1), mpos(2, 2)};
        array<mpos> mapPositions = cr.TestMapPosArray;
        if (mapPositions.length() != 2 || mapPositions[1] != mpos(2, 2)) return -56;

        cr.TestTimeSpanArray = {timespan(1, 3), timespan(2, 3)};
        array<timespan> spans = cr.TestTimeSpanArray;
        if (spans.length() != 2 || spans[1] != timespan(2, 3)) return -57;

        cr.TestNanoTimeArray = {Game.FrameTime};
        if (cr.TestNanoTimeArray.length() != 1) return -58;

        cr.TestInt8Array = {1, -2};
        array<int8> int8s = cr.TestInt8Array;
        if (int8s.length() != 2 || int8s[1] != -2) return -59;

        cr.TestInt16Array = {300, -400};
        array<int16> int16s = cr.TestInt16Array;
        if (int16s.length() != 2 || int16s[1] != -400) return -60;

        cr.TestUInt16Array = {1, 65000};
        array<uint16> uint16s = cr.TestUInt16Array;
        if (uint16s.length() != 2 || uint16s[1] != 65000) return -61;

        cr.TestUInt32Array = {1, 4000000000};
        array<uint32> uint32s = cr.TestUInt32Array;
        if (uint32s.length() != 2 || uint32s[1] != 4000000000) return -62;

        cr.TestUInt64Array = {1, 9000000000};
        array<uint64> uint64s = cr.TestUInt64Array;
        if (uint64s.length() != 2 || uint64s[1] != 9000000000) return -63;

        cr.TestFloat64Array = {1.25, 2.5};
        array<double> float64s = cr.TestFloat64Array;
        if (float64s.length() != 2 || float64s[1] != 2.5) return -64;

        dict<ident, int> identKeys = dict<ident, int>();
        identKeys.set(cr.Id, 9);
        cr.TestIdentKeyDict = identKeys;
        if (cr.TestIdentKeyDict.get(cr.Id) != 9) return -65;

        dict<int, ident> identValues = dict<int, ident>();
        identValues.set(1, cr.Id);
        cr.TestIdentValueDict = identValues;
        if (cr.TestIdentValueDict.get(1) != cr.Id) return -66;

        dict<hstring, hstring> hashHashes = dict<hstring, hstring>();
        hashHashes.set("hk".hstr(), "hv".hstr());
        cr.TestHashHashDict = hashHashes;
        if (cr.TestHashHashDict.get("hk".hstr()) != "hv".hstr()) return -67;

        dict<string, array<hstring>> hashArrays = dict<string, array<hstring>>();
        hashArrays.set("k", {"a".hstr(), "b".hstr()});
        cr.TestDictOfHashArray = hashArrays;
        if (cr.TestDictOfHashArray.get("k").length() != 2) return -68;

        Game.DestroyCritter(cr);
        return 1;
    }

    int PropertyDictConversionOps()
    {
        Critter cr = Game.CreateCritter("UnitTestCr".hstr(), false);

        dict<string, int> stringKeys = {};
        stringKeys.set("a", 1);
        stringKeys.set("bb", 2);
        cr.TestStringKeyDict = stringKeys;

        dict<string, int> readStringKeys = cr.TestStringKeyDict;
        if (readStringKeys.length() != 2) return -1;
        if (readStringKeys.get("bb") != 2) return -2;

        dict<int, string> stringValues = {};
        stringValues.set(1, "one");
        stringValues.set(2, "");
        cr.TestStringValueDict = stringValues;

        dict<int, string> readStringValues = cr.TestStringValueDict;
        if (readStringValues.length() != 2) return -3;
        if (readStringValues.get(1) != "one") return -4;
        if (!readStringValues.get(2).isEmpty()) return -5;

        dict<hstring, int> hashKeys = {};
        hashKeys.set("k1".hstr(), 10);
        cr.TestHashKeyDict = hashKeys;

        dict<hstring, int> readHashKeys = cr.TestHashKeyDict;
        if (readHashKeys.length() != 1) return -6;
        if (readHashKeys.get("k1".hstr()) != 10) return -7;

        dict<GenderType, int> enumKeys = {};
        enumKeys.set(GenderType::Female, 5);
        cr.TestEnumKeyDict = enumKeys;

        dict<GenderType, int> readEnumKeys = cr.TestEnumKeyDict;
        if (readEnumKeys.length() != 1) return -8;
        if (readEnumKeys.get(GenderType::Female) != 5) return -9;

        dict<string, int> emptyDict = {};
        cr.TestStringKeyDict = emptyDict;
        if (!cr.TestStringKeyDict.isEmpty()) return -10;

        Game.DestroyCritter(cr);
        return 1;
    }

    int PropertyDictOfArrayConversionOps()
    {
        Critter cr = Game.CreateCritter("UnitTestCr".hstr(), false);

        dict<int, array<int>> intLists = {};
        array<int> first = {1, 2};
        array<int> second = {};
        intLists.set(1, first);
        intLists.set(2, second);
        cr.TestDictOfArray = intLists;

        dict<int, array<int>> readIntLists = cr.TestDictOfArray;
        if (readIntLists.length() != 2) return -1;
        if (readIntLists.get(1).length() != 2) return -2;
        if (readIntLists.get(1)[1] != 2) return -3;
        if (!readIntLists.get(2).isEmpty()) return -4;

        dict<string, array<string>> stringLists = {};
        array<string> words = {"a", "", "ccc"};
        stringLists.set("key", words);
        cr.TestDictOfStringArray = stringLists;

        dict<string, array<string>> readStringLists = cr.TestDictOfStringArray;
        if (readStringLists.length() != 1) return -5;

        array<string> readWords = readStringLists.get("key");
        if (readWords.length() != 3) return -6;
        if (readWords[0] != "a" || !readWords[1].isEmpty() || readWords[2] != "ccc") return -7;

        dict<int, array<int>> emptyLists = {};
        cr.TestDictOfArray = emptyLists;
        if (!cr.TestDictOfArray.isEmpty()) return -8;

        Game.DestroyCritter(cr);
        return 1;
    }

    // === Extended string operations ===

    int StringRawAndIndexOps()
    {
        // length() counts characters while rawLength() counts bytes
        string ascii = "abc";
        if (ascii.length() != 3) return -1;
        if (ascii.rawLength() != 3) return -2;

        string multibyte = "aЯb";
        if (multibyte.length() != 3) return -3;
        if (multibyte.rawLength() != 4) return -4;

        // opIndex works in characters, so a multi-byte character comes back whole
        if (multibyte[0] != "a") return -5;
        if (multibyte[1] != "Я") return -6;
        if (multibyte[2] != "b") return -7;

        multibyte[1] = "xy";
        if (multibyte != "axyb") return -8;

        multibyte[1] = "";
        if (multibyte != "ayb") return -9;

        // rawGet/rawSet address bytes and clamp silently instead of failing
        string raw = "abc";
        if (raw.rawGet(0) != 97) return -10;
        if (raw.rawGet(-1) != 0) return -11;
        if (raw.rawGet(99) != 0) return -12;

        raw.rawSet(0, 122);
        if (raw != "zbc") return -13;

        raw.rawSet(-1, 122);
        raw.rawSet(99, 122);
        if (raw != "zbc") return -14;

        raw.rawResize(2);
        if (raw != "zb") return -15;

        raw.rawResize(4);
        if (raw.rawLength() != 4) return -16;

        raw.clear();
        if (!raw.isEmpty()) return -17;

        return 1;
    }

    int StringSearchOps()
    {
        string text = "one,two,three,two";

        if (text.find("two") != 4) return -1;
        if (text.find("two", 5) != 14) return -2;
        if (text.find("missing") != -1) return -3;

        if (text.findLast("two") != 14) return -4;
        if (text.findLast("two", 10) != 4) return -5;

        if (text.findFirstOf(",") != 3) return -6;
        if (text.findFirstOf(",", 4) != 7) return -7;
        if (text.findFirstNotOf("one") != 3) return -8;

)"
                    R"(        if (text.findLastOf(",") != 13) return -9;
        if (text.findLastOf(",", 6) != 3) return -90;
        if (text.findLastNotOf("otw") != 13) return -10;
        if (text.findLastNotOf(",", 3) != 2) return -91;

        // A negative index counts back from the end, so the defaults search the whole string
        if (text.findLast("two", -1) != 14) return -92;
        if (text.findLastOf(",", -1) != 13) return -93;

        if (!text.startsWith("one")) return -11;
        if (text.startsWith("two")) return -12;
        if (!text.endsWith("two")) return -13;
        if (text.endsWith("one")) return -14;

        if (text.replace("two", "TWO") != "one,TWO,three,TWO") return -15;
        if (text.substr(4, 3) != "two") return -16;
        if (text.substr(14) != "two") return -17;

        return 1;
    }

    int StringTrimAndCaseOps()
    {
        string padded = "  value  ";
        if (padded.trim() != "value") return -1;
        if (padded.trimBegin() != "value  ") return -2;
        if (padded.trimEnd() != "  value") return -3;

        string custom = "xxvaluexx";
        if (custom.trim("x") != "value") return -4;
        if (custom.trimBegin("x") != "valuexx") return -5;
        if (custom.trimEnd("x") != "xxvalue") return -6;

        if ("MiXeD".lower() != "mixed") return -7;
        if ("MiXeD".upper() != "MIXED") return -8;

        return 1;
    }

    int StringSplitJoinOps()
    {
        string text = "a,b,,c";

        array<string> parts = text.split(",");
        if (parts.length() != 4) return -1;
        if (parts[0] != "a" || parts[1] != "b" || parts[2] != "" || parts[3] != "c") return -2;

        array<string> compact = text.split(",", true);
        if (compact.length() != 3) return -3;
        if (compact[2] != "c") return -4;

        array<string> kept = text.split(",", false);
        if (kept.length() != 4) return -5;

        if (",".join(compact) != "a,b,c") return -6;

        array<string> single = {"only"};
        if ("-".join(single) != "only") return -7;

        array<string> empty = {};
        if (!"-".join(empty).isEmpty()) return -8;

        return 1;
    }

    int StringConversionOps()
    {
        if ("42".toInt() != 42) return -1;
        if ("nope".toInt() != 0) return -2;
        if ("nope".toInt(7) != 7) return -3;

        if ("9000000000".toInt64() != 9000000000) return -4;
        if ("nope".toInt64(5) != 5) return -5;

        if ("1.5".toFloat() != 1.5f) return -6;
        if ("nope".toFloat(2.5f) != 2.5f) return -7;

        if ("2.25".toDouble() != 2.25) return -8;
        if ("nope".toDouble(3.5) != 3.5) return -9;

        int intResult = 0;
        if (!"42".tryToInt(intResult)) return -10;
        if (intResult != 42) return -11;
        if ("nope".tryToInt(intResult)) return -12;

        int64 int64Result = 0;
        if (!"9000000000".tryToInt64(int64Result)) return -13;
        if (int64Result != 9000000000) return -14;
        if ("nope".tryToInt64(int64Result)) return -15;

        float floatResult = 0.0f;
        if (!"1.5".tryToFloat(floatResult)) return -16;
        if (floatResult != 1.5f) return -17;
        if ("nope".tryToFloat(floatResult)) return -18;

        double doubleResult = 0.0;
        if (!"2.25".tryToDouble(doubleResult)) return -19;
        if (doubleResult != 2.25) return -20;
        if ("nope".tryToDouble(doubleResult)) return -21;

        return 1;
    }

    int StringNumericOperatorOps()
    {
        // Every numeric overload has assign, add-assign, add and reversed-add forms
        string fromInt64 = "";
        fromInt64 = int64(-9000000000);
        if (fromInt64 != "-9000000000") return -1;
        fromInt64 += int64(1);
        if (!fromInt64.endsWith("1")) return -2;
        if (("v" + int64(7)) != "v7") return -3;
        if ((int64(7) + "v") != "7v") return -4;

        string fromUint64 = "";
        fromUint64 = uint64(9000000000);
        if (fromUint64 != "9000000000") return -5;
        fromUint64 += uint64(1);
        if (!fromUint64.endsWith("1")) return -6;
        if (("v" + uint64(7)) != "v7") return -7;
        if ((uint64(7) + "v") != "7v") return -8;

        string fromFloat = "";
        fromFloat = 1.5f;
        if (fromFloat.isEmpty()) return -9;
        fromFloat += 2.5f;
        if (fromFloat.isEmpty()) return -10;
        if (("v" + 1.5f).isEmpty()) return -11;
        if ((1.5f + "v").isEmpty()) return -12;

        string fromDouble = "";
        fromDouble = 2.25;
        if (fromDouble.isEmpty()) return -13;
        fromDouble += 3.25;
        if (fromDouble.isEmpty()) return -14;
        if (("v" + 2.25).isEmpty()) return -15;
        if ((2.25 + "v").isEmpty()) return -16;

        string fromAny = "";
        fromAny = any("boxed");
        if (fromAny != "boxed") return -17;
        fromAny += any("-tail");
        if (fromAny != "boxed-tail") return -18;
        if (("v" + any("x")) != "vx") return -19;
        if ((any("x") + "v") != "xv") return -20;

        return 1;
    }

    void StringIndexOutOfBoundsThrows()
    {
        string text = "abc";
        string value = text[5];
    }

    void StringNegativeIndexThrows()
    {
        // A negative index counts back from the end, so only one past the start is out of range
        string text = "abc";
        string value = text[-5];
    }

    void StringSetIndexOutOfBoundsThrows()
    {
        string text = "abc";
        text[5] = "z";
    }

    void StringNegativeRawResizeThrows()
    {
        string text = "abc";
        text.rawResize(-1);
    }

    // === Common engine method operations ===

    int CommonResourceAndRandomOps()
    {
        Game.Log("ScriptBuiltins common method log message");

        if (!Game.IsResourcePresent("ScriptBuiltinsTest.focfg")) return -1;
        if (Game.IsResourcePresent("NoSuchResource.focfg")) return -2;

        string content = Game.ReadResource("ScriptBuiltinsTest.focfg");
        if (!content.startsWith("[TestSection]")) return -3;
        if (!Game.ReadResource("NoSuchResource.focfg").isEmpty()) return -4;

        dict<string, string> section = Game.ReadConfigSection("ScriptBuiltinsTest.focfg", "TestSection");
        if (section.length() != 2) return -5;
        if (section.get("FirstKey") != "FirstValue") return -6;
        if (section.get("SecondKey") != "SecondValue") return -7;

        dict<string, string> missing = Game.ReadConfigSection("ScriptBuiltinsTest.focfg", "NoSuchSection");
        if (!missing.isEmpty()) return -8;

        if (Game.Random(5, 5) != 5) return -9;

        for (int i = 0; i < 20; i++) {
            int value = Game.Random(1, 3);
            if (value < 1 || value > 3) return -10;
        }

        if (Game.GetUnixTime() == 0) return -11;
        if (Game.GetLanguage().str.isEmpty()) return -12;

        return 1;
    }

    int ContextNestedCallDepth(int depth)
    {
        if (depth <= 0) {
            return 0;
        }

        return 1 + ContextNestedCallDepth(depth - 1);
    }

    int ContextNestedCallOps()
    {
        if (ContextNestedCallDepth(8) != 8) return -1;
        if (ContextNestedCallDepth(0) != 0) return -2;

        return 1;
    }

    void ContextNestedThrowLeaf()
    {
        throw("Nested script failure for stack collection");
    }

    void ContextNestedThrowMiddle()
    {
        ContextNestedThrowLeaf();
    }

    void ContextNestedThrowThrows()
    {
        // A throw several frames deep makes the context manager walk every script stack level
        ContextNestedThrowMiddle();
    }

    int CommonProtoQueryOps()
    {
        if (!Game.CheckProtoCritter("UnitTestCr".hstr())) return -1;
        if (Game.CheckProtoCritter("NoSuchCritter".hstr())) return -2;

        ProtoCritter proto = Game.GetProtoCritter("UnitTestCr".hstr());
        if (proto.ProtoId != "UnitTestCr".hstr()) return -3;

        array<ProtoCritter> allCritters = Game.GetProtoCritters();
        if (allCritters.isEmpty()) return -4;

        // The property-filtered overload compares the authored value of the named property
        array<ProtoCritter> byProperty = Game.GetProtoCritters(CritterProperty::TestEnum, 0);
        if (byProperty.length() != allCritters.length()) return -5;

        array<ProtoCritter> byMissingProperty = Game.GetProtoCritters(CritterProperty::TestEnum, 12345);
        if (!byMissingProperty.isEmpty()) return -6;

        // The fixture declares no item, map or location protos, so these answer empty rather than failing
        if (!Game.GetProtoItems().isEmpty()) return -7;
        if (!Game.GetProtoItems(ItemProperty::Hidden, 0).isEmpty()) return -8;
        if (Game.CheckProtoItem("NoSuchItem".hstr())) return -9;

        if (!Game.GetProtoMaps().isEmpty()) return -10;
        if (Game.CheckProtoMap("NoSuchMap".hstr())) return -11;

        if (!Game.GetProtoLocations().isEmpty()) return -12;
        if (Game.CheckProtoLocation("NoSuchLocation".hstr())) return -13;

        return 1;
    }

    [[TimeEvent]]
    void OnGlobalTickTimer()
    {
    }

    [[TimeEvent]]
    void OnGlobalDataTimer(any data)
    {
    }

    [[TimeEvent]]
    void OnGlobalArrayTimer(array<any> data)
    {
    }

    int CommonGlobalTimeEventOps()
    {
        uint32 plainId = Game.StartTimeEvent(timespan(60, 3), OnGlobalTickTimer);
        if (plainId == 0) return -1;
        if (Game.CountTimeEvent(OnGlobalTickTimer) != 1) return -2;

        uint32 dataId = Game.StartTimeEvent(timespan(60, 3), OnGlobalDataTimer, any("payload"));
        if (dataId == 0) return -3;
        if (Game.CountTimeEvent(OnGlobalDataTimer) != 1) return -4;

        array<any> payload = {any("a"), any("b")};
        uint32 arrayId = Game.StartTimeEvent(timespan(60, 3), OnGlobalArrayTimer, payload);
        if (arrayId == 0) return -5;
        if (Game.CountTimeEvent(OnGlobalArrayTimer) != 1) return -6;

        uint32 repeatId = Game.StartTimeEvent(timespan(60, 3), timespan(5, 3), OnGlobalTickTimer);
        if (repeatId == 0) return -7;
        if (Game.CountTimeEvent(OnGlobalTickTimer) != 2) return -8;

        Game.StopTimeEvent(OnGlobalTickTimer);
        if (Game.CountTimeEvent(OnGlobalTickTimer) != 0) return -9;

        Game.StopTimeEvent(OnGlobalDataTimer);
        Game.StopTimeEvent(OnGlobalArrayTimer);
        if (Game.CountTimeEvent(OnGlobalDataTimer) != 0) return -10;
        if (Game.CountTimeEvent(OnGlobalArrayTimer) != 0) return -11;

        return 1;
    }

    int CommonUtf8Ops()
    {
        int length = 0;
        uint ascii = Game.DecodeUtf8("A", length);
        if (ascii != 65) return -1;
        if (length != 1) return -2;

        string encoded = Game.EncodeUtf8(65);
        if (encoded != "A") return -3;

        // Two-byte and three-byte sequences round-trip through the same pair, and the reported
        // length is the encoded byte count rather than the character count
        string cyrillic = Game.EncodeUtf8(1071);
        if (cyrillic.isEmpty()) return -4;

        int cyrillicLength = 0;
        if (Game.DecodeUtf8(cyrillic, cyrillicLength) != 1071) return -5;
        if (cyrillicLength != 2) return -6;

        string cjk = Game.EncodeUtf8(20320);
        if (cjk.isEmpty()) return -7;

        int cjkLength = 0;
        if (Game.DecodeUtf8(cjk, cjkLength) != 20320) return -8;
        if (cjkLength != 3) return -9;

        return 1;
    }

    int CommonGeometryOps()
    {
        mpos origin = mpos(100, 100);
        mpos nearHex = mpos(105, 100);

        if (Game.GetDistance(origin, origin) != 0) return -1;
        if (Game.GetDistance(origin, nearHex) <= 0) return -2;

        mdir dir = Game.GetDirection(origin, nearHex);
        mdir dirWithOffset = Game.GetDirection(origin, nearHex, 15.0f);
        if (Game.GetDirAngleDiff(dir, dir) != 0) return -3;
        if (Game.GetDirAngleDiff(dir, dirWithOffset) < 0) return -4;

        mdir opposite = Game.RotateDirAngle(dir, true, 180);
        if (Game.GetDirAngleDiff(dir, opposite) != 180) return -5;

        mdir back = Game.RotateDirAngle(opposite, false, 180);
        if (Game.GetDirAngleDiff(dir, back) != 0) return -6;

        mdir lineDir = Game.GetLineDirAngle(ZERO_IPOS, ipos(10, 0));
        if (Game.GetDirAngleDiff(lineDir, lineDir) != 0) return -7;

        ipos interval = ZERO_IPOS;
        Game.GetHexInterval(origin, nearHex, interval);
        if (interval.x == 0 && interval.y == 0) return -8;

        ipos sameInterval = ipos(7, 7);
        Game.GetHexInterval(origin, origin, sameInterval);
        if (sameInterval.x != 0 || sameInterval.y != 0) return -9;

        return 1;
    }

    int CommonTraceHexLineOps()
    {
        msize mapSize;
        mapSize.width = 200;
        mapSize.height = 200;

        mpos fromHex = mpos(100, 100);
        mpos toHex = mpos(110, 100);

        mpos[] line = Game.TraceHexLine(mapSize, fromHex, toHex, 20, 0.0f, ZERO_IPOS, ZERO_IPOS);
        if (line.isEmpty()) return -1;
        if (line[0] == fromHex) return -2;

        // The distance argument caps the walk, and a zero distance yields nothing at all
        mpos[] limited = Game.TraceHexLine(mapSize, fromHex, toHex, 3, 0.0f, ZERO_IPOS, ZERO_IPOS);
        if (limited.length() != 3) return -3;
        if (limited[0] != line[0]) return -4;

        mpos[] none = Game.TraceHexLine(mapSize, fromHex, toHex, 0, 0.0f, ZERO_IPOS, ZERO_IPOS);
        if (!none.isEmpty()) return -5;

        // Tracing onto the origin hex still walks, but never past the requested distance
        mpos[] sameHex = Game.TraceHexLine(mapSize, fromHex, fromHex, 20, 0.0f, ZERO_IPOS, ZERO_IPOS);
        if (sameHex.length() > 20) return -6;

        mpos[] angleOffset = Game.TraceHexLine(mapSize, fromHex, toHex, 20, 45.0f, ZERO_IPOS, ZERO_IPOS);
        if (angleOffset.isEmpty()) return -7;

        mpos targetHex = mpos(0, 0);
        mpos[] angled = Game.TraceHexLine(mapSize, fromHex, 0.0f, 5, ZERO_IPOS, ZERO_IPOS, targetHex);
        if (angled.isEmpty()) return -8;
)"
                    R"(        if (targetHex == fromHex) return -9;

        return 1;
    }

    int CommonTimePackingOps()
    {
        nanotime precision = Game.GetPrecisionTime();
        if (precision == ZERO_NANOTIME) return -1;

        // PackTime resolves a calendar date against the current clock, so the round trip is exact down to
        // the millisecond; the sub-millisecond fields absorb the drift between the two clock samples
        nanotime packed = Game.PackTime(2030, 8, 3, 12, 30, 45, 250, 0, 0);

        int year = 0;
        int month = 0;
        int day = 0;
        int hour = 0;
        int minute = 0;
        int second = 0;
        int millisecond = 0;
        int microsecond = 0;
        int nanosecond = 0;
        Game.UnpackTime(packed, year, month, day, hour, minute, second, millisecond, microsecond, nanosecond);

        if (year != 2030) return -2;
        if (month != 8) return -3;
        if (day != 3) return -4;
        if (hour != 12) return -5;
        if (minute != 30) return -6;
        if (second != 45) return -7;
        if (millisecond < 249 || millisecond > 251) return -8;
        if (microsecond < 0 || microsecond > 999) return -9;
        if (nanosecond < 0 || nanosecond > 999) return -10;

        nanotime laterPacked = Game.PackTime(2031, 8, 3, 12, 30, 45, 250, 0, 0);
        if (!(laterPacked > packed)) return -11;

        synctime packedSync = Game.PackSynchronizedTime(2030, 8, 3, 12, 30, 45, 250);

        int syncYear = 0;
        int syncMonth = 0;
        int syncDay = 0;
        int syncHour = 0;
        int syncMinute = 0;
        int syncSecond = 0;
        int syncMillisecond = 0;
        Game.UnpackSynchronizedTime(packedSync, syncYear, syncMonth, syncDay, syncHour, syncMinute, syncSecond, syncMillisecond);

        if (syncYear != 2030) return -12;
        if (syncMonth != 8) return -13;
        if (syncDay != 3) return -14;
        if (syncHour != 12) return -15;
        if (syncMinute != 30) return -16;
        if (syncSecond != 45) return -17;
        if (syncMillisecond < 249 || syncMillisecond > 251) return -18;

        return 1;
    }

    // === Global binding operations ===

    funcdef void GlobalNameOfCallback();

    void GlobalNameOfTarget()
    {
    }

    int GlobalNameOfOps()
    {
        GlobalNameOfCallback cb = GlobalNameOfCallback(GlobalNameOfTarget);
        string name = NameOf(cb);
        if (!name.endsWith("GlobalNameOfTarget")) return -1;
        if (!name.startsWith("ScriptBuiltins")) return -2;
        return 1;
    }

    int GlobalRuntimeHelperOps()
    {
        int globalBefore = GetGlobalExceptionCount();
        int contextBefore = GetContextExceptionCount();
        if (globalBefore < 0) return -1;
        if (contextBefore < 0) return -2;

        RunScriptGC();

        if (IsGameDestroying) return -3;

        return 1;
    }

    int GlobalEngineSettingOps()
    {
        // Group accessors are chained one level at a time
        if (Settings.Common.GameName.isEmpty()) return -1;
        if (Settings.Network.ServerPort <= 0) return -2;

        bool debugBuild = Settings.Common.DebugBuild;
        bool packaged = Settings.Common.Packaged;
        if (debugBuild && packaged) return -3;

        // Writable engine settings round-trip through the setter
        int oldVolume = Settings.Audio.SoundVolume;
        Settings.Audio.SoundVolume = 42;
        if (Settings.Audio.SoundVolume != 42) return -4;
        Settings.Audio.SoundVolume = oldVolume;
        if (Settings.Audio.SoundVolume != oldVolume) return -5;

        string oldProxy = Settings.ClientNetwork.ProxyHost;
        Settings.ClientNetwork.ProxyHost = "unit-test-proxy";
        if (Settings.ClientNetwork.ProxyHost != "unit-test-proxy") return -6;
        Settings.ClientNetwork.ProxyHost = oldProxy;

        bool oldUdp = Settings.ClientNetwork.UseUdp;
        Settings.ClientNetwork.UseUdp = !oldUdp;
        if (Settings.ClientNetwork.UseUdp == oldUdp) return -7;
        Settings.ClientNetwork.UseUdp = oldUdp;

        // Vector settings are exposed as arrays
        array<int> dayColorTime = Settings.View.GlobalDayColorTime;
        if (dayColorTime.length() != 4) return -8;

        array<string> secretTokens = Settings.Common.SecretSettingTokens;
        if (secretTokens.isEmpty()) return -9;

        return 1;
    }

    int GlobalGameSettingOps()
    {
        // Game settings are stored as text and converted per declared type on every read and write
        Settings.TestSettings.StringValue = "game-setting";
        if (Settings.TestSettings.StringValue != "game-setting") return -1;

        Settings.TestSettings.AnyValue = any("boxed");
        if (string(Settings.TestSettings.AnyValue) != "boxed") return -2;

        Settings.TestSettings.BoolValue = true;
        if (!Settings.TestSettings.BoolValue) return -3;
        Settings.TestSettings.BoolValue = false;
        if (Settings.TestSettings.BoolValue) return -4;

        Settings.TestSettings.Int8Value = -8;
        if (Settings.TestSettings.Int8Value != -8) return -5;

        Settings.TestSettings.Int16Value = -1600;
        if (Settings.TestSettings.Int16Value != -1600) return -6;

        Settings.TestSettings.Int32Value = -320000;
        if (Settings.TestSettings.Int32Value != -320000) return -7;

        Settings.TestSettings.Int64Value = -6400000000;
        if (Settings.TestSettings.Int64Value != -6400000000) return -8;

        Settings.TestSettings.UInt8Value = 200;
        if (Settings.TestSettings.UInt8Value != 200) return -9;

        Settings.TestSettings.UInt16Value = 60000;
        if (Settings.TestSettings.UInt16Value != 60000) return -10;

        Settings.TestSettings.UInt32Value = 4000000000;
        if (Settings.TestSettings.UInt32Value != 4000000000) return -11;

        Settings.TestSettings.UInt64Value = 12800000000;
        if (Settings.TestSettings.UInt64Value != 12800000000) return -12;

        Settings.TestSettings.Float32Value = 1.5f;
        if (Settings.TestSettings.Float32Value != 1.5f) return -13;

        Settings.TestSettings.Float64Value = 2.25;
        if (Settings.TestSettings.Float64Value != 2.25) return -14;

        Settings.TestSettings.EnumValue = GenderType::Female;
        if (Settings.TestSettings.EnumValue != GenderType::Female) return -15;

        // A setting without a group hangs directly off the root object
        Settings.UngroupedTestSetting = 77;
        if (Settings.UngroupedTestSetting != 77) return -16;

        return 1;
    }

    void GlobalThrowNoArgsThrows()
    {
        throw("Global throw with no context");
    }

    void GlobalThrowOneArgThrows()
    {
        throw("Global throw with one context", 42);
    }

    void GlobalThrowThreeArgsThrows()
    {
        throw("Global throw with three contexts", 1, "two", true);
    }

    void GlobalThrowTenArgsThrows()
    {
        throw("Global throw with ten contexts", 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    }

    void GlobalThrowEntityArgThrows()
    {
        Critter critter = Game.CreateCritter("UnitTestCr".hstr(), false);
        throw("Global throw with entity context", critter);
    }

    void GlobalNameOfNonFunctionThrows()
    {
        int value = 0;
        string name = NameOf(value);
    }

    void GlobalNameOfNullThrows()
    {
        GlobalNameOfCallback? cb = null;
        string name = NameOf(cb);
    }

    void GlobalInvokeMissingFuncThrows()
    {
        Invoke("ScriptBuiltins::NoSuchFunction");
    }

    // === Reflection operations ===

    interface ReflectionShape
    {
        int Area();
    }

    class ReflectionBase : ReflectionShape
    {
        int Width = 2;
        int Height = 3;

        int Area()
        {
            return Width * Height;
        }
    }

    class ReflectionDerived : ReflectionBase
    {
        int Depth = 4;

        int Volume()
        {
            return Area() * Depth;
        }
    }

    int ReflectionTypeInfoOps()
    {
        reflection::type derived = reflection::typeof<ReflectionDerived>();
        if (derived is null) return -1;

        if (derived.nameWithoutNamespace != "ReflectionDerived") return -2;
        if (!derived.name.endsWith("ReflectionDerived")) return -3;
        if (derived.module.isEmpty()) return -4;
        if (derived.isGlobal) return -5;
        if (!derived.isClass) return -6;
        if (derived.isInterface) return -7;
        if (derived.isEnum) return -8;
        if (derived.isFunction) return -9;
        if (derived.size <= 0) return -10;

        reflection::type base = derived.baseType;
        if (base is null) return -11;
        if (base.nameWithoutNamespace != "ReflectionBase") return -12;
        if (!derived.derivesFrom(base)) return -13;
        if (base.derivesFrom(derived)) return -14;
        if (derived == base) return -15;
        if (!(derived == reflection::typeof<ReflectionDerived>())) return -16;

        return 1;
    }

    int ReflectionInterfaceAndMemberOps()
    {
        reflection::type derived = reflection::typeof<ReflectionDerived>();
        reflection::type shape = reflection::typeof<ReflectionShape>();

        if (!shape.isInterface) return -1;
        if (shape.isClass) return -2;
        if (derived.interfaceCount != 1) return -3;

        reflection::type iface = derived.getInterface(0);
        if (iface is null) return -4;
        if (!(iface == shape)) return -5;
        if (!derived.implements(shape)) return -6;
        if (shape.implements(derived)) return -7;
        if (derived.getInterface(1) !is null) return -8;
        if (derived.getInterface(-1) !is null) return -9;

        if (derived.methodsCount <= 0) return -10;
        if (derived.getMethodDeclaration(0).isEmpty()) return -11;
        if (derived.getMethodDeclaration(0, true, true, true).isEmpty()) return -12;
        if (!derived.getMethodDeclaration(1000).isEmpty()) return -13;

        if (derived.propertiesCount != 3) return -14;
        if (derived.getPropertyDeclaration(0).isEmpty()) return -15;
        if (derived.getPropertyDeclaration(0, true).isEmpty()) return -16;
        if (!derived.getPropertyDeclaration(-1).isEmpty()) return -17;
        if (!derived.getPropertyDeclaration(1000).isEmpty()) return -18;

        // A class is not an enum, so the enum accessors stay empty instead of failing
        if (derived.enumLength != 0) return -19;
        if (!derived.enumNames.isEmpty()) return -20;
        if (!derived.enumValues.isEmpty()) return -21;

        return 1;
    }

    int ReflectionInstantiateOps()
    {
        reflection::type derived = reflection::typeof<ReflectionDerived>();

        ReflectionDerived? made = null;
        derived.instantiate(made);
        if (made is null) return -1;
        if (made.Area() != 6) return -2;
        if (made.Volume() != 24) return -3;

        made.Width = 5;

        ReflectionDerived? copied = null;
        derived.instantiate(made, copied);
        if (copied is null) return -4;
        if (copied.Width != 5) return -5;
        if (copied is made) return -6;

        copied.Width = 9;
        if (made.Width != 5) return -7;

        // A base handle is a legal target for a derived instance
        ReflectionBase? asBase = null;
        derived.instantiate(asBase);
        if (asBase is null) return -8;
        if (asBase.Area() != 6) return -9;

        return 1;
    }

    int ReflectionTypeOfInstanceOps()
    {
        ReflectionDerived instance = ReflectionDerived();
        ReflectionBase asBase = instance;

        // The two-argument factory resolves the runtime type, not the declared one
        reflection::type runtime = reflection::typeof<ReflectionBase>(asBase);
        if (runtime is null) return -1;
        if (runtime.nameWithoutNamespace != "ReflectionDerived") return -2;
        if (!(runtime == reflection::typeof<ReflectionDerived>())) return -3;

        return 1;
    }

    int ReflectionModuleAndEnumOps()
    {
        array<string> modules = reflection::getLoadedModules();
        if (modules.isEmpty()) return -1;

        string current = reflection::getCurrentModule();
        if (current.isEmpty()) return -2;
        if (modules.find(current) == -1) return -3;

        array<reflection::type> globalEnums = reflection::getGlobalEnums();
        if (globalEnums.isEmpty()) return -4;

        array<reflection::type> moduleEnums = reflection::getEnums();
        array<reflection::type> namedEnums = reflection::getEnums(current);
        if (moduleEnums.length() != namedEnums.length()) return -5;

        // An unknown module name yields an empty list instead of failing
        array<reflection::type> missingEnums = reflection::getEnums("NoSuchModule");
        if (!missingEnums.isEmpty()) return -6;

        reflection::type gender = reflection::typeof<GenderType>();
        if (gender is null) return -7;
        if (!gender.isEnum) return -8;
        // An enum is not an object type, so the object-shaped predicates must answer, not fail
        if (gender.isClass) return -9;
        if (gender.isInterface) return -90;
        if (gender.isFunction) return -91;
        // Application-registered enums are shared, plain script classes are not
        if (!gender.isShared) return -92;
        if (reflection::typeof<ReflectionDerived>().isShared) return -93;
        if (gender.isGlobal != true) return -10;
        if (!gender.module.startsWith("(global")) return -11;
        if (gender.enumLength != 2) return -12;

        array<string> names = gender.enumNames;
        array<int> values = gender.enumValues;
        if (names.length() != 2 || values.length() != 2) return -13;
        if (names.find("Male") == -1 || names.find("Female") == -1) return -14;
        if (values.find(0) == -1 || values.find(1) == -1) return -15;

        reflection::type wide = reflection::typeof<WideEnum>();
        if (wide.enumLength != 2) return -16;

        array<int> wideValues = wide.enumValues;
        if (wideValues.find(650) == -1) return -17;
        if (wideValues.find(12) == -1) return -18;

        return 1;
    }

    int ReflectionCallstackOps()
    {
        string[] modules = {};
        string[] names = {};
)"
                    R"(        int[] lines = {};
        int[] columns = {};

        int count = reflection::getCallstack(modules, names, lines, columns);
        if (count <= 0) return -1;
        if (modules.length() != count) return -2;
        if (names.length() != count) return -3;
        if (lines.length() != count) return -4;
        if (columns.length() != count) return -5;
        if (names[0].isEmpty()) return -6;
        if (lines[0] <= 0) return -7;

        string[] detailedModules = {};
        string[] detailedNames = {};
        int[] detailedLines = {};
        int[] detailedColumns = {};

        int detailed = reflection::getCallstack(detailedModules, detailedNames, detailedLines, detailedColumns, true, true, false);
        if (detailed != count) return -8;
        if (detailedNames[0] == names[0]) return -9;

        return 1;
    }

    void ReflectionInstantiateNonHandleThrows()
    {
        reflection::type derived = reflection::typeof<ReflectionDerived>();
        int notHandle = 0;
        derived.instantiate(notHandle);
    }

    void ReflectionInstantiateIncompatibleThrows()
    {
        reflection::type base = reflection::typeof<ReflectionBase>();
        ReflectionDerived? made = null;
        base.instantiate(made);
    }

    void ReflectionInstantiateCopyNonHandleSourceThrows()
    {
        reflection::type derived = reflection::typeof<ReflectionDerived>();
        int notHandle = 0;
        ReflectionDerived? made = null;
        derived.instantiate(notHandle, made);
    }

    void ReflectionInstantiateCopyNullSourceThrows()
    {
        reflection::type derived = reflection::typeof<ReflectionDerived>();
        ReflectionDerived? source = null;
        ReflectionDerived? made = null;
        derived.instantiate(source, made);
    }

    void ReflectionInstantiateCopyWrongRuntimeTypeThrows()
    {
        reflection::type base = reflection::typeof<ReflectionBase>();
        ReflectionDerived? source = ReflectionDerived();
        ReflectionBase? made = null;
        base.instantiate(source, made);
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
        if (s.findLast("l") != 10) return -5;
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

    // One game setting per script-visible value type, so the generic setting accessors are driven end to end
    static constexpr std::array GAME_SETTING_TYPES = {
        std::pair<string_view, string_view> {"TestSettings.StringValue", "string"},
        std::pair<string_view, string_view> {"TestSettings.AnyValue", "any"},
        std::pair<string_view, string_view> {"TestSettings.BoolValue", "bool"},
        std::pair<string_view, string_view> {"TestSettings.Int8Value", "int8"},
        std::pair<string_view, string_view> {"TestSettings.Int16Value", "int16"},
        std::pair<string_view, string_view> {"TestSettings.Int32Value", "int32"},
        std::pair<string_view, string_view> {"TestSettings.Int64Value", "int64"},
        std::pair<string_view, string_view> {"TestSettings.UInt8Value", "uint8"},
        std::pair<string_view, string_view> {"TestSettings.UInt16Value", "uint16"},
        std::pair<string_view, string_view> {"TestSettings.UInt32Value", "uint32"},
        std::pair<string_view, string_view> {"TestSettings.UInt64Value", "uint64"},
        std::pair<string_view, string_view> {"TestSettings.Float32Value", "float32"},
        std::pair<string_view, string_view> {"TestSettings.Float64Value", "float64"},
        std::pair<string_view, string_view> {"TestSettings.EnumValue", "GenderType"},
        std::pair<string_view, string_view> {"UngroupedTestSetting", "int32"},
    };

    // One critter property per shape the property/script object converters have to handle
    static constexpr std::array GAME_PROPERTY_TYPES = {
        std::pair<string_view, string_view> {"string", "TestString"},
        std::pair<string_view, string_view> {"hstring", "TestHash"},
        std::pair<string_view, string_view> {"GenderType", "TestEnum"},
        std::pair<string_view, string_view> {"int32 [ ]", "TestIntArray"},
        std::pair<string_view, string_view> {"float32 [ ]", "TestFloatArray"},
        std::pair<string_view, string_view> {"string [ ]", "TestStringArray"},
        std::pair<string_view, string_view> {"hstring [ ]", "TestHashArray"},
        std::pair<string_view, string_view> {"GenderType [ ]", "TestEnumArray"},
        std::pair<string_view, string_view> {"string = > int32", "TestStringKeyDict"},
        std::pair<string_view, string_view> {"int32 = > string", "TestStringValueDict"},
        std::pair<string_view, string_view> {"hstring = > int32", "TestHashKeyDict"},
        std::pair<string_view, string_view> {"GenderType = > int32", "TestEnumKeyDict"},
        std::pair<string_view, string_view> {"int32 = > int32 [ ]", "TestDictOfArray"},
        std::pair<string_view, string_view> {"string = > string [ ]", "TestDictOfStringArray"},
        std::pair<string_view, string_view> {"any", "TestAny"},
        std::pair<string_view, string_view> {"any [ ]", "TestAnyArray"},
        std::pair<string_view, string_view> {"ProtoCritter", "TestProto"},
        std::pair<string_view, string_view> {"ProtoCritter [ ]", "TestProtoArray"},
        // The remaining scalar widths and the value types, each of which the property/script converters
        // handle through its own branch
        std::pair<string_view, string_view> {"int8", "TestInt8"},
        std::pair<string_view, string_view> {"int16", "TestInt16"},
        std::pair<string_view, string_view> {"int64", "TestInt64"},
        std::pair<string_view, string_view> {"uint16", "TestUInt16"},
        std::pair<string_view, string_view> {"uint32", "TestUInt32"},
        std::pair<string_view, string_view> {"uint64", "TestUInt64"},
        std::pair<string_view, string_view> {"bool [ ]", "TestBoolArray"},
        std::pair<string_view, string_view> {"int64 [ ]", "TestInt64Array"},
        std::pair<string_view, string_view> {"ident", "TestIdent"},
        std::pair<string_view, string_view> {"ident [ ]", "TestIdentArray"},
        std::pair<string_view, string_view> {"timespan", "TestTimeSpan"},
        std::pair<string_view, string_view> {"ucolor", "TestColor"},
        std::pair<string_view, string_view> {"ucolor [ ]", "TestColorArray"},
        std::pair<string_view, string_view> {"string = > string", "TestStringStringDict"},
        std::pair<string_view, string_view> {"hstring = > string", "TestHashStringDict"},
        std::pair<string_view, string_view> {"int32 = > hstring", "TestHashValueDict"},
        // The remaining engine value types and the narrow-scalar arrays, each of which the property/script
        // converters reach through its own branch
        std::pair<string_view, string_view> {"nanotime", "TestNanoTime"},
        std::pair<string_view, string_view> {"synctime", "TestSyncTime"},
        std::pair<string_view, string_view> {"mpos", "TestMapPos"},
        std::pair<string_view, string_view> {"msize", "TestMapSize"},
        std::pair<string_view, string_view> {"ipos", "TestIntPos"},
        std::pair<string_view, string_view> {"isize", "TestIntSize"},
        std::pair<string_view, string_view> {"irect", "TestIntRect"},
        std::pair<string_view, string_view> {"fpos", "TestFloatPos"},
        std::pair<string_view, string_view> {"fsize", "TestFloatSize"},
        std::pair<string_view, string_view> {"mpos [ ]", "TestMapPosArray"},
        std::pair<string_view, string_view> {"timespan [ ]", "TestTimeSpanArray"},
        std::pair<string_view, string_view> {"nanotime [ ]", "TestNanoTimeArray"},
        std::pair<string_view, string_view> {"int8 [ ]", "TestInt8Array"},
        std::pair<string_view, string_view> {"int16 [ ]", "TestInt16Array"},
        std::pair<string_view, string_view> {"uint16 [ ]", "TestUInt16Array"},
        std::pair<string_view, string_view> {"uint32 [ ]", "TestUInt32Array"},
        std::pair<string_view, string_view> {"uint64 [ ]", "TestUInt64Array"},
        std::pair<string_view, string_view> {"float64", "TestFloat64"},
        std::pair<string_view, string_view> {"float64 [ ]", "TestFloat64Array"},
        std::pair<string_view, string_view> {"ident = > int32", "TestIdentKeyDict"},
        std::pair<string_view, string_view> {"int32 = > ident", "TestIdentValueDict"},
        std::pair<string_view, string_view> {"hstring = > hstring", "TestHashHashDict"},
        std::pair<string_view, string_view> {"string = > hstring [ ]", "TestDictOfHashArray"},
    };

    static auto MakeMetadataWithGenderEnum() -> vector<uint8_t>
    {
        vector<vector<string_view>> enums = {
            {"GenderType", "uint8", "Male", "0", "Female", "1"},
            {"WideEnum", "uint16", "Low", "12", "High", "650"},
        };

        vector<vector<string_view>> settings;
        settings.reserve(GAME_SETTING_TYPES.size());

        for (const auto& [setting_name, setting_type] : GAME_SETTING_TYPES) {
            settings.emplace_back(vector<string_view> {setting_name, setting_type, "0"});
        }

        vector<vector<string_view>> properties;
        properties.reserve(GAME_PROPERTY_TYPES.size());

        for (const auto& [property_type, property_name] : GAME_PROPERTY_TYPES) {
            // Sync tags are only legal on Common properties, so a server-only property carries none
            properties.emplace_back(vector<string_view> {"Critter", "Server", property_type, property_name, "Mutable"});
        }

        return BakerTests::MakeMetadataBlob({{"Enum", enums}, {"Setting", settings}, {"Property", properties}});
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

        string_view config_text = "[TestSection]\nFirstKey=FirstValue\nSecondKey=SecondValue\n";
        vector<uint8_t> config_blob(config_text.begin(), config_text.end());

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ScriptBuiltinsRuntimeResources");
        runtime_source->AddFile("ScriptBuiltinsTest.focfg", config_blob);
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

    auto run_success_func = [&server, &fn](string_view func_name) {
        auto func = server->FindFunc<int32_t>(fn(func_name));
        REQUIRE(func);
        REQUIRE(func.Call());
        INFO(func_name);
        CHECK(func.GetResult() == 1);
    };
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

    run_success_func("ScriptBuiltins::DictAccessorOps");
    run_success_func("ScriptBuiltins::DictKeysValuesOps");
    run_success_func("ScriptBuiltins::DictInitListOps");
    run_success_func("ScriptBuiltins::DictCloneEqualsOps");
    run_success_func("ScriptBuiltins::DictRemoveValuesOps");
    run_success_func("ScriptBuiltins::DictWideIntegerOps");
    run_success_func("ScriptBuiltins::ArrayWideIntegerOps");
    run_success_func("ScriptBuiltins::DictEnumKeyOps");
    run_success_func("ScriptBuiltins::DictHandleValueOps");
    run_success_func("ScriptBuiltins::DictHandleKeyOps");

    run_throwing_func("ScriptBuiltins::DictMissingKeyThrows", "Key not found");
    run_throwing_func("ScriptBuiltins::DictKeyIndexOutOfBoundsThrows", "Index out of bounds");
    run_throwing_func("ScriptBuiltins::DictValueIndexOutOfBoundsThrows", "Index out of bounds");
    run_throwing_func("ScriptBuiltins::DictNoCompareKeyThrows", "Type does not have a matching opCmp method");

    // Direct ScriptDict API coverage
    {
        ScriptMessages messages;
        nptr<AngelScript::asIScriptEngine> as_engine = MakeAngelScriptEngine(messages);
        auto release_engine = scope_exit([&as_engine]() noexcept { safe_call([&as_engine] { as_engine->ShutDownAndRelease(); }); });

        RegisterAngelScriptArray(as_engine.get());
        RegisterAngelScriptDict(as_engine.get());

        nptr<AngelScript::asITypeInfo> int_dict_type = as_engine->GetTypeInfoByDecl("dict<int,int>");
        REQUIRE(int_dict_type != nullptr);
        nptr<AngelScript::asITypeInfo> string_key_dict_type = as_engine->GetTypeInfoByDecl("dict<int64,int64>");
        REQUIRE(string_key_dict_type != nullptr);

        auto dict = ScriptDict::Create(int_dict_type.get());

        CHECK(dict->IsEmpty());
        CHECK(dict->GetSize() == 0);
        CHECK(dict->GetDictTypeId() == int_dict_type->GetTypeId());
        CHECK(dict->GetDictObjectType() == int_dict_type);
        CHECK(dict->GetRefCount() == 1);
        CHECK_FALSE(dict->GetFlag());

        dict->SetFlag();
        CHECK(dict->GetFlag());
        dict->AddRef();
        CHECK_FALSE(dict->GetFlag());
        CHECK(dict->GetRefCount() == 2);
        dict->Release();
        CHECK(dict->GetRefCount() == 1);

        int32_t first_key = 1;
        int32_t first_value = 10;
        int32_t second_key = 2;
        int32_t second_value = 20;
        int32_t missing_key = 7;
        int32_t replacement_value = 11;
        int32_t default_value = -1;

        dict->Set(&first_key, &first_value);
        dict->Set(&second_key, &second_value);
        CHECK(dict->GetSize() == 2);
        CHECK_FALSE(dict->IsEmpty());

        dict->Set(&first_key, &replacement_value);
        CHECK(dict->GetSize() == 2);
        CHECK(*dict->ValueAs<int32_t>(dict->Get(&first_key).get()) == replacement_value);

        dict->SetIfNotExist(&first_key, &first_value);
        CHECK(*dict->ValueAs<int32_t>(dict->Get(&first_key).get()) == replacement_value);
        dict->SetIfNotExist(&missing_key, &first_value);
        CHECK(dict->GetSize() == 3);

        CHECK(dict->Exists(&missing_key));
        CHECK(dict->Remove(&missing_key));
        CHECK_FALSE(dict->Remove(&missing_key));
        CHECK(dict->GetSize() == 2);

        CHECK_THROWS_AS(dict->Get(&missing_key), ScriptException);
        CHECK(*dict->ValueAs<int32_t>(dict->GetDefault(&missing_key, &default_value).get()) == default_value);
        CHECK(*dict->ValueAs<int32_t>(dict->GetDefault(&second_key, &default_value).get()) == second_value);

        CHECK(*dict->ValueAs<int32_t>(dict->GetOrCreate(&second_key).get()) == second_value);
        CHECK(*dict->ValueAs<int32_t>(dict->GetOrCreate(&missing_key).get()) == 0);
        CHECK(dict->GetSize() == 3);
        CHECK(dict->Remove(&missing_key));

        CHECK(*dict->KeyAtAs<int32_t>(0) == first_key);
        CHECK(*dict->ValueAtAs<int32_t>(0) == replacement_value);
        CHECK(*dict->KeyAtAs<int32_t>(1) == second_key);
        CHECK_THROWS_AS(dict->GetKey(-1), ScriptException);
        CHECK_THROWS_AS(dict->GetKey(2), ScriptException);
        CHECK_THROWS_AS(dict->GetValue(-1), ScriptException);
        CHECK_THROWS_AS(dict->GetValue(2), ScriptException);

        CHECK(dict->GetMap()->size() == 2);

        auto keys = dict->GetKeys();
        auto values = dict->GetValues();
        REQUIRE(keys->GetSize() == 2);
        REQUIRE(values->GetSize() == 2);
        CHECK(*keys->AtAs<int32_t>(0) == first_key);
        CHECK(*keys->AtAs<int32_t>(1) == second_key);
        CHECK(*values->AtAs<int32_t>(0) == replacement_value);
        CHECK(*values->AtAs<int32_t>(1) == second_value);

        auto copy = SafeAlloc::MakeRefCounted<ScriptDict>(*dict);
        CHECK(copy->GetSize() == 2);
        CHECK(*copy == *dict);
        CHECK(*dict == *dict);

        copy->Set(&second_key, &default_value);
        CHECK_FALSE(*copy == *dict);
        copy->Remove(&second_key);
        CHECK_FALSE(*copy == *dict);

        auto other_type_dict = ScriptDict::Create(string_key_dict_type.get());
        CHECK_FALSE(*dict == *other_type_dict);
        CHECK_THROWS_AS(*copy = *other_type_dict, ScriptException);

        *copy = *dict;
        CHECK(*copy == *dict);
        *copy = *copy;
        CHECK(*copy == *dict);

        dict->EnumReferences(as_engine.get());
        dict->ReleaseAllHandles();
        CHECK(dict->IsEmpty());

        // Wide integer keys and values must not collapse to their low 32 bits
        auto wide_dict = ScriptDict::Create(string_key_dict_type.get());
        auto wide_other = ScriptDict::Create(string_key_dict_type.get());

        int64_t wide_first = int64_t {1} << 32 | 1;
        int64_t wide_second = int64_t {2} << 32 | 1;

        wide_dict->Set(&wide_first, &wide_first);
        wide_dict->Set(&wide_second, &wide_second);
        CHECK(wide_dict->GetSize() == 2);

        wide_other->Set(&wide_first, &wide_first);
        wide_other->Set(&wide_second, &wide_first);
        CHECK_FALSE(*wide_dict == *wide_other);

        CHECK(wide_dict->RemoveValues(&wide_second) == 1);
        CHECK(wide_dict->GetSize() == 1);
        CHECK(wide_dict->Exists(&wide_first));
    }

    // Dict template callback diagnostics
    {
        ScriptMessages messages;
        nptr<AngelScript::asIScriptEngine> as_engine = MakeAngelScriptEngine(messages);
        auto release_engine = scope_exit([&as_engine]() noexcept { safe_call([&as_engine] { as_engine->ShutDownAndRelease(); }); });

        RegisterAngelScriptArray(as_engine.get());
        RegisterAngelScriptDict(as_engine.get());

        CHECK(BuildAngelScriptModule(as_engine.get(), "DictVoidKeyRejected", "void Test() { dict<void,int> values; }\n") < 0);
        CHECK(!messages.Entries.empty());
        messages.Entries.clear();

        CHECK(BuildAngelScriptModule(as_engine.get(), "DictVoidValueRejected", "void Test() { dict<int,void> values; }\n") < 0);
        CHECK(!messages.Entries.empty());
        messages.Entries.clear();

        RegisterArrayNoDefaultValue(as_engine.get());
        CHECK(BuildAngelScriptModule(as_engine.get(), "DictNoDefaultValueRejected", "void Test() { dict<int,ArrayNoDefaultValue> values; }\n") < 0);
        CHECK(HasScriptMessage(messages, "The subtype has no default constructor"));
        messages.Entries.clear();

        REQUIRE(as_engine->RegisterObjectType("DictNativeRef", 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOCOUNT) >= 0);
        CHECK(BuildAngelScriptModule(as_engine.get(), "DictRefValueRejected", "void Test() { dict<int,DictNativeRef> values; }\n") < 0);
        CHECK(HasScriptMessage(messages, "Can't store references in dict"));
    }

    // Dict value comparator diagnostics for native value sub-types
    {
        ScriptMessages messages;
        nptr<AngelScript::asIScriptEngine> as_engine = MakeAngelScriptEngine(messages);
        auto release_engine = scope_exit([&as_engine]() noexcept { safe_call([&as_engine] { as_engine->ShutDownAndRelease(); }); });

        RegisterAngelScriptArray(as_engine.get());
        RegisterAngelScriptDict(as_engine.get());
        RegisterArrayComparableValue(as_engine.get(), "DictNoCompareNativeValue", false, false);
        RegisterArrayComparableValue(as_engine.get(), "DictMultiEqualsNativeValue", true, false);
        RegisterArrayComparableValue(as_engine.get(), "DictMultiCmpNativeValue", false, true);

        auto check_value_comparator_throw = [&as_engine](string_view dict_decl, string_view expected_message) {
            nptr<AngelScript::asITypeInfo> dict_type = as_engine->GetTypeInfoByDecl(string {dict_decl}.c_str());
            REQUIRE(dict_type != nullptr);

            auto dict = ScriptDict::Create(dict_type.get());
            int32_t key = 1;
            ArrayComparableValue value {1};
            dict->Set(&key, &value);

            INFO(dict_decl);

            try {
                (void)dict->RemoveValues(&value);
                FAIL("Dict value comparison was expected to throw");
            }
            catch (const ScriptException& ex) {
                CHECK(string_view(ex.what()).find(expected_message) != string_view::npos);
            }
        };

        check_value_comparator_throw("dict<int,DictNoCompareNativeValue>", "Type does not have a matching opEquals or opCmp method");
        check_value_comparator_throw("dict<int,DictMultiEqualsNativeValue>", "Type has multiple matching opEquals or opCmp methods");

        nptr<AngelScript::asITypeInfo> multi_cmp_key_dict_type = as_engine->GetTypeInfoByDecl("dict<DictMultiCmpNativeValue,int>");
        REQUIRE(multi_cmp_key_dict_type != nullptr);

        auto multi_cmp_key_dict = ScriptDict::Create(multi_cmp_key_dict_type.get());
        ArrayComparableValue low_key {1};
        ArrayComparableValue high_key {3};
        int32_t key_value = 5;

        try {
            multi_cmp_key_dict->Set(&low_key, &key_value);
            multi_cmp_key_dict->Set(&high_key, &key_value);
            FAIL("Dict key comparison was expected to throw");
        }
        catch (const ScriptException& ex) {
            CHECK(string_view(ex.what()).find("Type has multiple matching opCmp methods") != string_view::npos);
        }
    }
}

TEST_CASE("ScriptBuiltinsGlobalBindings")
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
    auto run_success_func = [&server, &fn](string_view func_name) {
        auto func = server->FindFunc<int32_t>(fn(func_name));
        REQUIRE(func);
        REQUIRE(func.Call());
        INFO(func_name);
        CHECK(func.GetResult() == 1);
    };
    auto run_throwing_func = [&server, &fn](string_view func_name, string_view expected_message) -> string {
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
        return message;
    };

    run_success_func("ScriptBuiltins::PropertyScalarConversionOps");
    run_success_func("ScriptBuiltins::PropertyArrayConversionOps");
    run_success_func("ScriptBuiltins::PropertyDictConversionOps");
    run_success_func("ScriptBuiltins::PropertyDictOfArrayConversionOps");

    run_success_func("ScriptBuiltins::StringRawAndIndexOps");
    run_success_func("ScriptBuiltins::StringSearchOps");
    run_success_func("ScriptBuiltins::StringTrimAndCaseOps");
    run_success_func("ScriptBuiltins::StringSplitJoinOps");
    run_success_func("ScriptBuiltins::StringConversionOps");
    run_success_func("ScriptBuiltins::StringNumericOperatorOps");

    run_throwing_func("ScriptBuiltins::StringIndexOutOfBoundsThrows", "Out of range");
    run_throwing_func("ScriptBuiltins::StringNegativeIndexThrows", "Out of range");
    run_throwing_func("ScriptBuiltins::StringSetIndexOutOfBoundsThrows", "Out of range");
    run_throwing_func("ScriptBuiltins::StringNegativeRawResizeThrows", "String resize length must not be negative");

    run_success_func("ScriptBuiltins::CommonResourceAndRandomOps");
    run_success_func("ScriptBuiltins::ContextNestedCallOps");
    run_throwing_func("ScriptBuiltins::ContextNestedThrowThrows", "Nested script failure for stack collection");

    run_success_func("ScriptBuiltins::CommonProtoQueryOps");
    run_success_func("ScriptBuiltins::CommonGlobalTimeEventOps");
    run_success_func("ScriptBuiltins::CommonUtf8Ops");
    run_success_func("ScriptBuiltins::CommonGeometryOps");
    run_success_func("ScriptBuiltins::CommonTraceHexLineOps");
    run_success_func("ScriptBuiltins::CommonTimePackingOps");
    run_success_func("ScriptBuiltins::GlobalNameOfOps");
    run_success_func("ScriptBuiltins::GlobalRuntimeHelperOps");
    run_success_func("ScriptBuiltins::GlobalEngineSettingOps");
    run_success_func("ScriptBuiltins::GlobalGameSettingOps");

    run_throwing_func("ScriptBuiltins::GlobalThrowNoArgsThrows", "Global throw with no context");
    run_throwing_func("ScriptBuiltins::GlobalThrowOneArgThrows", "Global throw with one context");
    run_throwing_func("ScriptBuiltins::GlobalThrowThreeArgsThrows", "Global throw with three contexts");
    run_throwing_func("ScriptBuiltins::GlobalThrowTenArgsThrows", "Global throw with ten contexts");
    string entity_throw_message = run_throwing_func("ScriptBuiltins::GlobalThrowEntityArgThrows", "Critter: name UnitTestCr id ");
    CHECK(entity_throw_message.find(" proto UnitTestCr") != string::npos);
    run_throwing_func("ScriptBuiltins::GlobalNameOfNonFunctionThrows", "argument must be a function reference");
    run_throwing_func("ScriptBuiltins::GlobalNameOfNullThrows", "function reference is null");
    run_throwing_func("ScriptBuiltins::GlobalInvokeMissingFuncThrows", "Script function not found");
}

// Lives here because this rig is the one that declares game settings in its metadata blob. Pins
// BaseEngine's own precedence rule, so dropping the guard in its constructor fails a test
TEST_CASE("EngineAppliesGameSettingMetadataOnlyToUnconfiguredValues")
{
    auto settings = MakeSettings();

    ConfigFile runtime_config {"TestSettings.Int32Value = 7\nUngroupedTestSetting = 9\n"};
    settings.ApplyConfigFile(runtime_config, "");

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

    // The configuration named these before the engine existed, so the metadata baseline of "0" must not
    // have replaced them; the ungrouped one proves the bare spelling matches the same way
    CHECK(settings.GetCustomSetting("TestSettings.Int32Value") == "7");
    CHECK(settings.GetCustomSetting("UngroupedTestSetting") == "9");

    // The configuration never named these, so they arrive from the metadata baseline
    CHECK(settings.GetCustomSetting("TestSettings.Int64Value") == "0");
    CHECK(settings.GetCustomSetting("TestSettings.StringValue") == "0");
    CHECK(settings.GetCustomSetting("TestSettings.BoolValue") == "0");
}

TEST_CASE("ScriptBuiltinsReflectionOperations")
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
    auto run_success_func = [&server, &fn](string_view func_name) {
        auto func = server->FindFunc<int32_t>(fn(func_name));
        REQUIRE(func);
        REQUIRE(func.Call());
        INFO(func_name);
        CHECK(func.GetResult() == 1);
    };
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

    run_success_func("ScriptBuiltins::ReflectionTypeInfoOps");
    run_success_func("ScriptBuiltins::ReflectionInterfaceAndMemberOps");
    run_success_func("ScriptBuiltins::ReflectionInstantiateOps");
    run_success_func("ScriptBuiltins::ReflectionTypeOfInstanceOps");
    run_success_func("ScriptBuiltins::ReflectionModuleAndEnumOps");
    run_success_func("ScriptBuiltins::ReflectionCallstackOps");

    // The "handle must be null" guard is deliberately not probed: AngelScript clears every `?&out`
    // slot before the call, so a script can never hand a non-null instance handle to instantiate()
    run_throwing_func("ScriptBuiltins::ReflectionInstantiateNonHandleThrows", "not an handle");
    run_throwing_func("ScriptBuiltins::ReflectionInstantiateIncompatibleThrows", "incompatible types");
    run_throwing_func("ScriptBuiltins::ReflectionInstantiateCopyNonHandleSourceThrows", "not an handle");
    run_throwing_func("ScriptBuiltins::ReflectionInstantiateCopyNullSourceThrows", "handle must be not null");
    run_throwing_func("ScriptBuiltins::ReflectionInstantiateCopyWrongRuntimeTypeThrows", "incompatible runtime type");
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
