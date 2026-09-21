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

#include "SettingsStorage.h"

FO_BEGIN_NAMESPACE

TEST_CASE("SettingsStorage")
{
    // Uses the real platform backend (registry on Windows, file store elsewhere) under a dedicated application
    // name so it never touches a tool's real settings. Every key is removed after use to leave no residue
    SettingsStorage store {"Test_SettingsStorage"};

    for (string_view key : {"str", "multiline", "int", "bool_true", "bool_false", "float"}) {
        store.Remove(key);
    }

    SECTION("StringRoundtrip")
    {
        store.SetString("str", "hello settings");

        CHECK(store.HasKey("str"));
        CHECK(store.GetString("str") == "hello settings");

        store.Remove("str");
    }

    SECTION("MultilineBlobRoundtrip")
    {
        // The ImGui layout blob is multi-line text and must survive the round-trip verbatim
        string blob = "[Window][Preview]\nPos=10,20\nSize=300,400\n\n[Window][List]\nPos=0,0\n";
        store.SetString("multiline", blob);

        CHECK(store.GetString("multiline") == blob);

        store.Remove("multiline");
    }

    SECTION("TypedRoundtrip")
    {
        store.SetInt("int", -12345);
        store.SetBool("bool_true", true);
        store.SetBool("bool_false", false);
        store.SetFloat("float", 2.5);

        CHECK(store.GetInt("int") == -12345);
        CHECK(store.GetBool("bool_true"));
        CHECK_FALSE(store.GetBool("bool_false"));
        CHECK(store.GetFloat("float") == 2.5);

        store.Remove("int");
        store.Remove("bool_true");
        store.Remove("bool_false");
        store.Remove("float");
    }

    SECTION("MissingKeysReturnDefaults")
    {
        CHECK_FALSE(store.HasKey("missing"));
        CHECK(store.GetString("missing", "fallback") == "fallback");
        CHECK(store.GetInt("missing", 77) == 77);
        CHECK(store.GetBool("missing", true));
        CHECK(store.GetFloat("missing", 1.5) == 1.5);
    }

    SECTION("RemoveClearsKey")
    {
        store.SetString("str", "temp");
        REQUIRE(store.HasKey("str"));

        store.Remove("str");

        CHECK_FALSE(store.HasKey("str"));
        CHECK(store.GetString("str", "def") == "def");
    }
}

FO_END_NAMESPACE
