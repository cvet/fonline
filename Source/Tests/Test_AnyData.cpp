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
        AnyData::Value val = numeric_cast<int64_t>(1234);
        CHECK(AnyData::ValueToString(val) == u8"1234");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::Int64));
    }

    SECTION("Float 1")
    {
        AnyData::Value val = 0.0f;
        CHECK(AnyData::ValueToString(val) == u8"0");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::Float64));
    }

    SECTION("Float 2")
    {
        AnyData::Value val = 0.999999;
        CHECK(AnyData::ValueToString(val) == u8"0.999999");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::Float64));
    }

    SECTION("Float 3")
    {
        AnyData::Value val = 100000000.0;
        CHECK(AnyData::ValueToString(val) == u8"100000000");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::Float64));
    }

    SECTION("Bool True")
    {
        AnyData::Value val = true;
        CHECK(AnyData::ValueToString(val) == u8"True");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::Bool));
    }

    SECTION("Bool False")
    {
        AnyData::Value val = false;
        CHECK(AnyData::ValueToString(val) == u8"False");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::Bool));
    }

    SECTION("String 1")
    {
        AnyData::Value val = u8string(u8"ABCD");
        CHECK(AnyData::ValueToString(val) == u8"ABCD");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::String));
    }

    SECTION("String 2")
    {
        AnyData::Value val = u8string(u8"AB   C D");
        CHECK(AnyData::ValueToString(val) == u8"AB   C D");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::String));
    }

    SECTION("String 3")
    {
        AnyData::Value val = u8string(u8"   AB   C D");
        CHECK(AnyData::ValueToString(val) == u8"\"   AB   C D\"");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::String));
    }

    SECTION("String 4")
    {
        AnyData::Value val = u8string(u8"ABCD ");
        CHECK(AnyData::ValueToString(val) == u8"\"ABCD \"");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::String));
    }

    SECTION("String 5")
    {
        AnyData::Value val = u8string(u8"\tABCD");
        CHECK(AnyData::ValueToString(val) == u8"\"\tABCD\"");
        CHECK(val == AnyData::ParseValue(AnyData::ValueToString(val), false, false, AnyData::ValueType::String));
    }

    SECTION("Utf8StringAndDictKeyRoundTrip")
    {
        u8string text {u8"Привет, 🌍"};
        auto encoded_text = AnyData::ValueToString(AnyData::Value {text});
        CHECK(AnyData::ParseValue(encoded_text, false, false, AnyData::ValueType::String).AsString() == text.view());

        AnyData::Dict dict;
        u8string key {u8"ключ-🔑"};
        u8string value {u8"значение-📌"};
        dict.Emplace(key, value);

        auto encoded_dict = AnyData::ValueToString(dict.Copy());
        auto parsed_dict = AnyData::ParseValue(encoded_dict, true, false, AnyData::ValueType::String);
        CHECK(parsed_dict.AsDict()[key].AsString() == value.view());
    }

    SECTION("Array of ints")
    {
        AnyData::Array arr;
        arr.EmplaceBack(numeric_cast<int64_t>(1));
        arr.EmplaceBack(numeric_cast<int64_t>(2));
        arr.EmplaceBack(numeric_cast<int64_t>(3));
        arr.EmplaceBack(numeric_cast<int64_t>(4));
        CHECK(AnyData::ValueToString(arr.Copy()) == u8"1 2 3 4");
        CHECK(arr == AnyData::ParseValue(AnyData::ValueToString(arr.Copy()), false, true, AnyData::ValueType::Int64).AsArray());
    }

    SECTION("Array of strings")
    {
        AnyData::Array arr;
        arr.EmplaceBack(u8string(u8"one"));
        arr.EmplaceBack(u8string(u8"two"));
        arr.EmplaceBack(u8string(u8"three"));
        CHECK(AnyData::ValueToString(arr.Copy()) == u8"one two three");
        CHECK(arr == AnyData::ParseValue(AnyData::ValueToString(arr.Copy()), false, true, AnyData::ValueType::String).AsArray());
    }

    SECTION("Dict with ints")
    {
        AnyData::Dict dict;
        dict.Emplace("key1", numeric_cast<int64_t>(1));
        dict.Emplace("key2", numeric_cast<int64_t>(2));
        dict.Emplace("key3", numeric_cast<int64_t>(3));
        CHECK(AnyData::ValueToString(dict.Copy()) == u8"key1 1 key2 2 key3 3");
        CHECK(dict == AnyData::ParseValue(AnyData::ValueToString(dict.Copy()), true, false, AnyData::ValueType::Int64).AsDict());
    }

    SECTION("Dict with strings")
    {
        AnyData::Dict dict;
        dict.Emplace("key1", u8string(u8"value 1"));
        dict.Emplace("key2", u8string(u8" value 2 "));
        dict.Emplace("key3", u8string(u8"value3"));
        CHECK(AnyData::ValueToString(dict.Copy()) == u8"key1 \"value 1\" key2 \" value 2 \" key3 value3");
        CHECK(dict == AnyData::ParseValue(AnyData::ValueToString(dict.Copy()), true, false, AnyData::ValueType::String).AsDict());
    }

    SECTION("Dict of array 1")
    {
        AnyData::Array arr1;
        arr1.EmplaceBack(numeric_cast<int64_t>(1));
        arr1.EmplaceBack(numeric_cast<int64_t>(2));
        arr1.EmplaceBack(numeric_cast<int64_t>(3));
        AnyData::Dict dict;
        dict.Emplace("key1", std::move(arr1));

        CHECK(AnyData::ValueToString(dict.Copy()) == u8"key1 \"1 2 3\"");
        CHECK(dict == AnyData::ParseValue(AnyData::ValueToString(dict.Copy()), true, true, AnyData::ValueType::Int64).AsDict());
    }

    SECTION("Dict of array 2")
    {
        AnyData::Array arr1;
        arr1.EmplaceBack(u8string(u8"one 1"));
        arr1.EmplaceBack(u8string(u8"two 2"));
        arr1.EmplaceBack(u8string(u8" three 3 "));
        AnyData::Array arr2;
        arr2.EmplaceBack(u8string(u8"1"));
        arr2.EmplaceBack(u8string(u8"2"));
        arr2.EmplaceBack(u8string(u8"3"));
        AnyData::Dict dict;
        dict.Emplace("key1", std::move(arr1));
        dict.Emplace("key2", std::move(arr2));

        CHECK(AnyData::ValueToString(dict.Copy()) == u8"key1 \"\\\"one 1\\\" \\\"two 2\\\" \\\" three 3 \\\"\" key2 \"1 2 3\"");
        CHECK(dict == AnyData::ParseValue(AnyData::ValueToString(dict.Copy()), true, true, AnyData::ValueType::String).AsDict());
    }

    SECTION("ArrayCopyIsDeep")
    {
        AnyData::Dict nested;
        nested.Emplace("k", numeric_cast<int64_t>(42));

        AnyData::Array original;
        original.EmplaceBack(std::move(nested));

        auto copied = original.Copy();

        auto mutable_nested_copy = copied[0].AsDict().Copy();
        mutable_nested_copy.Assign("k", numeric_cast<int64_t>(100));

        CHECK(original[0].AsDict()["k"].AsInt64() == 42);
        CHECK(mutable_nested_copy["k"].AsInt64() == 100);
    }

    SECTION("EmptyContainersParsing")
    {
        auto parsed_arr_value = AnyData::ParseValue(u8"", false, true, AnyData::ValueType::String);
        auto parsed_dict_value = AnyData::ParseValue(u8"", true, false, AnyData::ValueType::Int64);
        const auto& parsed_arr = parsed_arr_value.AsArray();
        const auto& parsed_dict = parsed_dict_value.AsDict();

        CHECK(parsed_arr.Empty());
        CHECK(parsed_dict.Empty());
    }

    SECTION("InvalidBoolRejected")
    {
        CHECK_THROWS_AS((AnyData::ParseValue(u8"Enabled", false, false, AnyData::ValueType::Bool)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue(u8"foo", false, true, AnyData::ValueType::Bool)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue(u8"key Enabled", true, false, AnyData::ValueType::Bool)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue(u8"2", false, false, AnyData::ValueType::Bool)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue(u8"-1", false, false, AnyData::ValueType::Bool)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue(u8"0.5", false, false, AnyData::ValueType::Bool)), AnyDataException);
    }

    SECTION("InvalidNumbersRejected")
    {
        CHECK_THROWS_AS((AnyData::ParseValue(u8"abc", false, false, AnyData::ValueType::Int64)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue(u8"abc", false, false, AnyData::ValueType::Float64)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue(u8"1 two 3", false, true, AnyData::ValueType::Int64)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue(u8"key NaNish", true, false, AnyData::ValueType::Float64)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue(u8"nan", false, false, AnyData::ValueType::Float64)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue(u8"1 inf 3", false, true, AnyData::ValueType::Float64)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue(u8"key -inf", true, false, AnyData::ValueType::Float64)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ValueToString(AnyData::Value {std::numeric_limits<float64_t>::quiet_NaN()})), AnyDataException);

        AnyData::Array non_finite_values;
        non_finite_values.EmplaceBack(numeric_cast<int64_t>(1));
        non_finite_values.EmplaceBack(std::numeric_limits<float64_t>::infinity());
        CHECK_THROWS_AS((AnyData::ValueToString(AnyData::Value {std::move(non_finite_values)})), AnyDataException);
    }

    SECTION("BoolAcceptsNumbersAndExplicitLiterals")
    {
        CHECK(AnyData::ParseValue(u8"True", false, false, AnyData::ValueType::Bool).AsBool());
        CHECK(!AnyData::ParseValue(u8"False", false, false, AnyData::ValueType::Bool).AsBool());
        CHECK(AnyData::ParseValue(u8"1", false, false, AnyData::ValueType::Bool).AsBool());
        CHECK(!AnyData::ParseValue(u8"0", false, false, AnyData::ValueType::Bool).AsBool());
    }

    SECTION("MalformedDictRejected")
    {
        CHECK_THROWS_AS((AnyData::ParseValue(u8"key_only", true, false, AnyData::ValueType::String)), AnyDataException);
    }

    SECTION("MalformedQuotedTokenRejected")
    {
        CHECK_THROWS_AS((AnyData::ParseValue(u8"\"unterminated", false, false, AnyData::ValueType::String)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue(u8"key \"unterminated", true, false, AnyData::ValueType::String)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue(u8"\"1 2", false, true, AnyData::ValueType::Int64)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue(u8"\"quoted\"tail", false, false, AnyData::ValueType::String)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue(u8"\"key with space\"tail value", true, false, AnyData::ValueType::String)), AnyDataException);
    }

    SECTION("DanglingEscapeRejected")
    {
        CHECK_THROWS_AS((AnyData::ParseValue(u8"abc\\", false, false, AnyData::ValueType::String)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue(u8"one two\\", false, true, AnyData::ValueType::String)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue(u8"key value\\", true, false, AnyData::ValueType::String)), AnyDataException);
    }

    SECTION("WhitespaceTrimmedForTypedScalars")
    {
        CHECK(AnyData::ParseValue(u8"  -42\t", false, false, AnyData::ValueType::Int64).AsInt64() == -42);
        CHECK(AnyData::ParseValue(u8"\t3.5 ", false, false, AnyData::ValueType::Float64).AsDouble() == 3.5);
        CHECK(AnyData::ParseValue(u8"  true\t", false, false, AnyData::ValueType::Bool).AsBool());
    }

    SECTION("QuotedStringsRoundTripInContainers")
    {
        AnyData::Array arr;
        arr.EmplaceBack(u8string(u8" spaced value "));
        arr.EmplaceBack(u8string(u8"quote \" value"));

        u8string encoded_arr = AnyData::ValueToString(arr.Copy());
        CHECK(arr == AnyData::ParseValue(encoded_arr, false, true, AnyData::ValueType::String).AsArray());

        AnyData::Dict dict;
        dict.Emplace("key space", u8string(u8"value with spaces"));
        dict.Emplace("key_quote", u8string(u8"value \"quote\""));

        u8string encoded_dict = AnyData::ValueToString(dict.Copy());
        CHECK(dict == AnyData::ParseValue(encoded_dict, true, false, AnyData::ValueType::String).AsDict());
    }

    SECTION("CarriageReturnIsStripped")
    {
        u8string text {u8"line1\rline2\nline3"};
        u8string normalized_text {u8"line1line2\nline3"};
        auto encoded = AnyData::ValueToString(AnyData::Value {text});

        CHECK(StringEscaping::CodeString(text.view()) == u8"\"line1line2\\nline3\"");
        CHECK(StringEscaping::DecodeString(u8"\"line1\\rline2\\nline3\"") == normalized_text);
        CHECK(encoded == normalized_text);
        CHECK(AnyData::ParseValue(encoded, false, false, AnyData::ValueType::String).AsString() == normalized_text.view());

        AnyData::Dict dict;
        dict.Emplace("payload", u8string {u8"head\r\ntail"});

        AnyData::Dict normalized_dict;
        normalized_dict.Emplace("payload", u8string {u8"head\ntail"});

        u8string encoded_dict = AnyData::ValueToString(dict.Copy());
        CHECK(normalized_dict == AnyData::ParseValue(encoded_dict, true, false, AnyData::ValueType::String).AsDict());
    }

    SECTION("DictKeysAreDecodedAndDuplicateKeysRejected")
    {
        AnyData::Dict dict;
        dict.Emplace("key \"quoted\"", u8string(u8"value1"));
        dict.Emplace("path\\segment", u8string(u8"value2"));

        u8string encoded_dict = AnyData::ValueToString(dict.Copy());
        auto parsed_value = AnyData::ParseValue(encoded_dict, true, false, AnyData::ValueType::String);
        const auto& parsed_dict = parsed_value.AsDict();

        CHECK(parsed_dict.Contains("key \"quoted\""));
        CHECK(parsed_dict.Contains("path\\segment"));
        CHECK(parsed_dict["key \"quoted\""].AsString() == u8"value1");
        CHECK(parsed_dict["path\\segment"].AsString() == u8"value2");

        CHECK_THROWS_AS((AnyData::ParseValue(u8"dup 1 dup 2", true, false, AnyData::ValueType::Int64)), AnyDataException);
        CHECK_THROWS_AS((AnyData::ParseValue(u8"dup \"1 2\" dup \"3 4\"", true, true, AnyData::ValueType::Int64)), AnyDataException);
    }
}

FO_END_NAMESPACE
