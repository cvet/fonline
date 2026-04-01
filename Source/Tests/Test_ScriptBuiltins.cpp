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

#include "AngelScriptScripting.h"
#include "Baker.h"
#include "DataSerialization.h"
#include "Server.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

namespace
{
    static auto MakeSettings() -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedServerSettings(settings);

        return settings;
    }

    static auto MakeScriptBinary(const FileSystem& metadata_resources) -> vector<uint8>
    {
        BakerServerEngine compiler_engine {metadata_resources};

        return BakerTests::CompileInlineScripts(&compiler_engine, "ScriptBuiltinsScripts",
            {
                {"Scripts/ScriptBuiltins.fos", R"(
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

        string trimmed = "  hello  ".trim();
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
                const auto message_str = string(message);

                if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos || message_str.find("fatal") != string::npos || message_str.find("Fatal") != string::npos) {
                    throw ScriptSystemException(message_str);
                }
            });
    }

    static auto MakeResources() -> FileSystem
    {
        const auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();

        auto compiler_resources_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ScriptBuiltinsCompilerResources");
        compiler_resources_source->AddFile("Metadata.fometa-server", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_resources_source));

        BakerServerEngine proto_engine {compiler_resources};
        const auto critter_type = proto_engine.Hashes.ToHashedString("Critter");
        const auto critter_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "UnitTestCr");
        const auto script_blob = MakeScriptBinary(compiler_resources);

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ScriptBuiltinsRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
        runtime_source->AddFile("ScriptBuiltins.fopro-bin-server", critter_blob);
        runtime_source->AddFile("ScriptBuiltins.fos-bin-server", script_blob);

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));

        return resources;
    }

    static auto WaitForStart(ServerEngine* server) -> string
    {
        FO_RUNTIME_ASSERT(server);

        for (int32 i = 0; i < 6000; i++) {
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
}

TEST_CASE("ScriptBuiltinsStringOperations")
{
    auto settings = MakeSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, MakeResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto fn = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    // StringLength
    {
        auto func = server->FindFunc<int32, string>(fn("ScriptBuiltins::StringLength"));
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
        auto func = server->FindFunc<string, string, int32, int32>(fn("ScriptBuiltins::StringSubstr"));
        REQUIRE(func);
        REQUIRE(func.Call(string {"Hello, World!"}, 7, 5));
        CHECK(func.GetResult() == "World");
    }

    // StringFindFirst / StringFindLast
    {
        auto func_first = server->FindFunc<int32, string, string>(fn("ScriptBuiltins::StringFindFirst"));
        REQUIRE(func_first);
        REQUIRE(func_first.Call(string {"abcabc"}, string {"bc"}));
        CHECK(func_first.GetResult() == 1);

        auto func_last = server->FindFunc<int32, string, string>(fn("ScriptBuiltins::StringFindLast"));
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
        auto func_int = server->FindFunc<int32, string>(fn("ScriptBuiltins::StringToInt"));
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
        auto func_int = server->FindFunc<string, int32>(fn("ScriptBuiltins::IntToString"));
        REQUIRE(func_int);
        REQUIRE(func_int.Call(42));
        CHECK(func_int.GetResult() == "42");
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
        auto func = server->FindFunc<int32, string, string>(fn("ScriptBuiltins::StringCompare"));
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
        auto func = server->FindFunc<int32>(fn("ScriptBuiltins::ComprehensiveStringTest"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 1);
    }
}

TEST_CASE("ScriptBuiltinsArrayOperations")
{
    auto settings = MakeSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, MakeResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto fn = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    // ArrayLength with FindFunc<int, vector<int>>
    {
        auto func = server->FindFunc<int32, vector<int32>>(fn("ScriptBuiltins::ArrayLength"));
        REQUIRE(func);

        vector<int32> arr {1, 2, 3, 4, 5};
        REQUIRE(func.Call(arr));
        CHECK(func.GetResult() == 5);

        vector<int32> empty {};
        REQUIRE(func.Call(empty));
        CHECK(func.GetResult() == 0);
    }

    // ArrayEmpty
    {
        auto func = server->FindFunc<bool, vector<int32>>(fn("ScriptBuiltins::ArrayEmpty"));
        REQUIRE(func);

        vector<int32> arr {1};
        REQUIRE(func.Call(arr));
        CHECK(func.GetResult() == false);

        vector<int32> empty {};
        REQUIRE(func.Call(empty));
        CHECK(func.GetResult() == true);
    }

    // ArrayFind
    {
        auto func = server->FindFunc<int32, vector<int32>, int32>(fn("ScriptBuiltins::ArrayFind"));
        REQUIRE(func);

        vector<int32> arr {10, 20, 30, 40, 50};
        REQUIRE(func.Call(arr, 30));
        CHECK(func.GetResult() == 2);
        REQUIRE(func.Call(arr, 99));
        CHECK(func.GetResult() == -1);
    }

    // ComprehensiveArrayTest
    {
        auto func = server->FindFunc<int32>(fn("ScriptBuiltins::ComprehensiveArrayTest"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 1);
    }

    // NestedArraySum
    {
        auto func = server->FindFunc<int32>(fn("ScriptBuiltins::NestedArraySum"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 21);
    }
}

TEST_CASE("ScriptBuiltinsDictOperations")
{
    auto settings = MakeSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, MakeResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto fn = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    // DictLength
    {
        auto func = server->FindFunc<int32>(fn("ScriptBuiltins::DictLength"));
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
        auto func = server->FindFunc<int32>(fn("ScriptBuiltins::DictGetSet"));
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
        auto func = server->FindFunc<int32>(fn("ScriptBuiltins::DictClear"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 0);
    }

    // DictIntKeys
    {
        auto func = server->FindFunc<int32>(fn("ScriptBuiltins::DictIntKeys"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 660);
    }

    // ComprehensiveDictTest
    {
        auto func = server->FindFunc<int32>(fn("ScriptBuiltins::ComprehensiveDictTest"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == 1);
    }
}

TEST_CASE("ScriptBuiltinsMathAndTypeOperations")
{
    auto settings = MakeSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, MakeResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto fn = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    // ComprehensiveMathTest
    {
        auto func = server->FindFunc<int32>(fn("ScriptBuiltins::ComprehensiveMathTest"));
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
