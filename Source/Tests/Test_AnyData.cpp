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

#include "catch_amalgamated.hpp"

#include "AnyData.h"

FO_BEGIN_NAMESPACE

TEST_CASE("AnyData")
{
    SECTION("Int 1")
    {
        AnyData::Value val = numeric_cast<int64>(1234);
        CHECK(AnyData::ValueToString(val) == "1234");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::Int64));
    }

    SECTION("Float 1")
    {
        AnyData::Value val = 0.0f;
        CHECK(AnyData::ValueToString(val) == "0");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::Float64));
    }

    SECTION("Float 2")
    {
        AnyData::Value val = 0.999999;
        CHECK(AnyData::ValueToString(val) == "0.999999");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::Float64));
    }

    SECTION("Float 3")
    {
        AnyData::Value val = 100000000.0;
        CHECK(AnyData::ValueToString(val) == "100000000");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::Float64));
    }

    SECTION("Bool True")
    {
        AnyData::Value val = true;
        CHECK(AnyData::ValueToString(val) == "True");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::Bool));
    }

    SECTION("Bool False")
    {
        AnyData::Value val = false;
        CHECK(AnyData::ValueToString(val) == "False");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::Bool));
    }

    SECTION("String 1")
    {
        AnyData::Value val = string("ABCD");
        CHECK(AnyData::ValueToString(val) == "ABCD");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::String));
    }

    SECTION("String 2")
    {
        AnyData::Value val = string("AB   C D");
        CHECK(AnyData::ValueToString(val) == "AB   C D");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::String));
    }

    SECTION("String 3")
    {
        AnyData::Value val = string("   AB   C D");
        CHECK(AnyData::ValueToString(val) == "\"   AB   C D\"");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::String));
    }

    SECTION("String 4")
    {
        AnyData::Value val = string("ABCD ");
        CHECK(AnyData::ValueToString(val) == "\"ABCD \"");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::String));
    }

    SECTION("String 5")
    {
        AnyData::Value val = string("\tABCD");
        CHECK(AnyData::ValueToString(val) == "\"\tABCD\"");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::String));
    }

    SECTION("Array of ints")
    {
        AnyData::Array arr;
        arr.EmplaceBack(numeric_cast<int64>(1));
        arr.EmplaceBack(numeric_cast<int64>(2));
        arr.EmplaceBack(numeric_cast<int64>(3));
        arr.EmplaceBack(numeric_cast<int64>(4));
        CHECK(AnyData::ValueToString(arr.Copy()) == "1 2 3 4");
        CHECK(arr == AnyData::ParseValue(AnyData::ValueToString(arr.Copy()), false, true, AnyData::ValueType::Int64).AsArray());
    }

    SECTION("Array of strings")
    {
        AnyData::Array arr;
        arr.EmplaceBack(string("one"));
        arr.EmplaceBack(string("two"));
        arr.EmplaceBack(string("three"));
        CHECK(AnyData::ValueToString(arr.Copy()) == "one two three");
        CHECK(arr == AnyData::ParseValue(AnyData::ValueToString(arr.Copy()), false, true, AnyData::ValueType::String).AsArray());
    }

    SECTION("Dict with ints")
    {
        AnyData::Dict dict;
        dict.Emplace("key1", numeric_cast<int64>(1));
        dict.Emplace("key2", numeric_cast<int64>(2));
        dict.Emplace("key3", numeric_cast<int64>(3));
        CHECK(AnyData::ValueToString(dict.Copy()) == "key1 1 key2 2 key3 3");
        CHECK(dict == AnyData::ParseValue(AnyData::ValueToString(dict.Copy()), true, false, AnyData::ValueType::Int64).AsDict());
    }

    SECTION("Dict with strings")
    {
        AnyData::Dict dict;
        dict.Emplace("key1", string("value 1"));
        dict.Emplace("key2", string(" value 2 "));
        dict.Emplace("key3", string("value3"));
        CHECK(AnyData::ValueToString(dict.Copy()) == "key1 \"value 1\" key2 \" value 2 \" key3 value3");
        CHECK(dict == AnyData::ParseValue(AnyData::ValueToString(dict.Copy()), true, false, AnyData::ValueType::String).AsDict());
    }

    SECTION("Dict of array 1")
    {
        AnyData::Array arr1;
        arr1.EmplaceBack(numeric_cast<int64>(1));
        arr1.EmplaceBack(numeric_cast<int64>(2));
        arr1.EmplaceBack(numeric_cast<int64>(3));
        AnyData::Dict dict;
        dict.Emplace("key1", std::move(arr1));

        CHECK(AnyData::ValueToString(dict.Copy()) == "key1 \"1 2 3\"");
        CHECK(dict == AnyData::ParseValue(AnyData::ValueToString(dict.Copy()), true, true, AnyData::ValueType::Int64).AsDict());
    }

    SECTION("Dict of array 2")
    {
        AnyData::Array arr1;
        arr1.EmplaceBack(string("one 1"));
        arr1.EmplaceBack(string("two 2"));
        arr1.EmplaceBack(string(" three 3 "));
        AnyData::Array arr2;
        arr2.EmplaceBack(string("1"));
        arr2.EmplaceBack(string("2"));
        arr2.EmplaceBack(string("3"));
        AnyData::Dict dict;
        dict.Emplace("key1", std::move(arr1));
        dict.Emplace("key2", std::move(arr2));

        CHECK(AnyData::ValueToString(dict.Copy()) == "key1 \"\\\"one 1\\\" \\\"two 2\\\" \\\" three 3 \\\"\" key2 \"1 2 3\"");
        CHECK(dict == AnyData::ParseValue(AnyData::ValueToString(dict.Copy()), true, true, AnyData::ValueType::String).AsDict());
    }

    SECTION("ArrayCopyIsDeep")
    {
        AnyData::Dict nested;
        nested.Emplace("k", numeric_cast<int64>(42));

        AnyData::Array original;
        original.EmplaceBack(std::move(nested));

        auto copied = original.Copy();

        auto mutable_nested_copy = copied[0].AsDict().Copy();
        mutable_nested_copy.Assign("k", numeric_cast<int64>(100));

        CHECK(original[0].AsDict()["k"].AsInt64() == 42);
        CHECK(mutable_nested_copy["k"].AsInt64() == 100);
    }

    SECTION("EmptyContainersParsing")
    {
        const auto parsed_arr_value = AnyData::ParseValue("", false, true, AnyData::ValueType::String);
        const auto parsed_dict_value = AnyData::ParseValue("", true, false, AnyData::ValueType::Int64);
        const auto& parsed_arr = parsed_arr_value.AsArray();
        const auto& parsed_dict = parsed_dict_value.AsDict();

        CHECK(parsed_arr.Empty());
        CHECK(parsed_dict.Empty());
    }

    SECTION("InvalidBoolRejected")
    {
        CHECK_THROWS_AS((AnyData::ParseValue("Enabled", false, false, AnyData::ValueType::Bool)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue("foo", false, true, AnyData::ValueType::Bool)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue("key Enabled", true, false, AnyData::ValueType::Bool)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue("2", false, false, AnyData::ValueType::Bool)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue("-1", false, false, AnyData::ValueType::Bool)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue("0.5", false, false, AnyData::ValueType::Bool)), AnyDataException);
    }

    SECTION("InvalidNumbersRejected")
    {
        CHECK_THROWS_AS((AnyData::ParseValue("abc", false, false, AnyData::ValueType::Int64)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue("abc", false, false, AnyData::ValueType::Float64)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue("1 two 3", false, true, AnyData::ValueType::Int64)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue("key NaNish", true, false, AnyData::ValueType::Float64)), AnyDataException);
    }

    SECTION("BoolAcceptsNumbersAndExplicitLiterals")
    {
        CHECK(AnyData::ParseValue("True", false, false, AnyData::ValueType::Bool).AsBool());
        CHECK(!AnyData::ParseValue("False", false, false, AnyData::ValueType::Bool).AsBool());
        CHECK(AnyData::ParseValue("1", false, false, AnyData::ValueType::Bool).AsBool());
        CHECK(!AnyData::ParseValue("0", false, false, AnyData::ValueType::Bool).AsBool());
    }

    SECTION("MalformedDictRejected")
    {
        CHECK_THROWS_AS((AnyData::ParseValue("key_only", true, false, AnyData::ValueType::String)), AnyDataException);
    }

    SECTION("MalformedQuotedTokenRejected")
    {
        CHECK_THROWS_AS((AnyData::ParseValue("\"unterminated", false, false, AnyData::ValueType::String)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue("key \"unterminated", true, false, AnyData::ValueType::String)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue("\"1 2", false, true, AnyData::ValueType::Int64)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue("\"quoted\"tail", false, false, AnyData::ValueType::String)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue("\"key with space\"tail value", true, false, AnyData::ValueType::String)), AnyDataException);
    }

    SECTION("DanglingEscapeRejected")
    {
        CHECK_THROWS_AS((AnyData::ParseValue("abc\\", false, false, AnyData::ValueType::String)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue("one two\\", false, true, AnyData::ValueType::String)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue("key value\\", true, false, AnyData::ValueType::String)), AnyDataException);
    }

    SECTION("WhitespaceTrimmedForTypedScalars")
    {
        CHECK(AnyData::ParseValue("  -42\t", false, false, AnyData::ValueType::Int64).AsInt64() == -42);
        CHECK(AnyData::ParseValue("\t3.5 ", false, false, AnyData::ValueType::Float64).AsDouble() == 3.5);
        CHECK(AnyData::ParseValue("  true\t", false, false, AnyData::ValueType::Bool).AsBool());
    }

    SECTION("QuotedStringsRoundTripInContainers")
    {
        AnyData::Array arr;
        arr.EmplaceBack(string(" spaced value "));
        arr.EmplaceBack(string("quote \" value"));

        const auto encoded_arr = AnyData::ValueToString(arr.Copy());
        CHECK(arr == AnyData::ParseValue(encoded_arr, false, true, AnyData::ValueType::String).AsArray());

        AnyData::Dict dict;
        dict.Emplace("key space", string("value with spaces"));
        dict.Emplace("key_quote", string("value \"quote\""));

        const auto encoded_dict = AnyData::ValueToString(dict.Copy());
        CHECK(dict == AnyData::ParseValue(encoded_dict, true, false, AnyData::ValueType::String).AsDict());
    }

    SECTION("CarriageReturnIsStripped")
    {
        const string text {"line1\rline2\nline3"};
        const string normalized_text {"line1line2\nline3"};
        const auto encoded = AnyData::ValueToString(AnyData::Value {text});

        CHECK(StringEscaping::CodeString(text) == "\"line1line2\\nline3\"");
        CHECK(StringEscaping::DecodeString("\"line1\\rline2\\nline3\"") == normalized_text);
        CHECK(encoded == normalized_text);
        CHECK(AnyData::ParseValue(encoded, false, false, AnyData::ValueType::String).AsString() == normalized_text);

        AnyData::Dict dict;
        dict.Emplace("payload", string {"head\r\ntail"});

        AnyData::Dict normalized_dict;
        normalized_dict.Emplace("payload", string {"head\ntail"});

        const auto encoded_dict = AnyData::ValueToString(dict.Copy());
        CHECK(normalized_dict == AnyData::ParseValue(encoded_dict, true, false, AnyData::ValueType::String).AsDict());
    }

    SECTION("DictKeysAreDecodedAndDuplicateKeysRejected")
    {
        AnyData::Dict dict;
        dict.Emplace("key \"quoted\"", string("value1"));
        dict.Emplace("path\\segment", string("value2"));

        const auto encoded_dict = AnyData::ValueToString(dict.Copy());
        const auto parsed_value = AnyData::ParseValue(encoded_dict, true, false, AnyData::ValueType::String);
        const auto& parsed_dict = parsed_value.AsDict();

        CHECK(parsed_dict.Contains("key \"quoted\""));
        CHECK(parsed_dict.Contains("path\\segment"));
        CHECK(parsed_dict["key \"quoted\""].AsString() == "value1");
        CHECK(parsed_dict["path\\segment"].AsString() == "value2");

        CHECK_THROWS_AS((AnyData::ParseValue("dup 1 dup 2", true, false, AnyData::ValueType::Int64)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue("dup \"1 2\" dup \"3 4\"", true, true, AnyData::ValueType::Int64)), AnyDataException);
    }
}

FO_END_NAMESPACE
