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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Common.h"

FO_BEGIN_NAMESPACE();

class AnyData final
{
public:
    AnyData() = delete;

    enum class ValueType : uint8
    {
        Int64 = 0,
        Float64,
        Bool,
        String,
        Array,
        Dict
    };

    class Dict;
    class Array;

    class Value
    {
    public:
        // ReSharper disable CppNonExplicitConvertingConstructor
        Value(int64 value) :
            _value(value)
        {
        }
        Value(float64 value) :
            _value(value)
        {
        }
        Value(bool value) :
            _value(value)
        {
        }
        Value(string value) :
            _value(std::move(value))
        {
        }
        Value(Array&& value) :
            _value(SafeAlloc::MakeUnique<Array>(std::move(value)))
        {
        }
        Value(Dict&& value) :
            _value(SafeAlloc::MakeUnique<Dict>(std::move(value)))
        {
        }
        // ReSharper restore CppNonExplicitConvertingConstructor

        Value() = delete;
        Value(const Value&) = delete;
        Value(Value&&) noexcept = default;
        auto operator=(const Value&) = delete;
        auto operator=(Value&&) noexcept -> Value& = default;
        ~Value() = default;

        [[nodiscard]] auto operator==(const Value& other) const -> bool;
        [[nodiscard]] auto Type() const noexcept -> ValueType { return static_cast<ValueType>(_value.index()); }
        [[nodiscard]] auto AsInt64() const -> int64 { return std::get<int64>(_value); }
        [[nodiscard]] auto AsDouble() const -> float64 { return std::get<float64>(_value); }
        [[nodiscard]] auto AsBool() const -> bool { return std::get<bool>(_value); }
        [[nodiscard]] auto AsString() const -> const string& { return std::get<string>(_value); }
        [[nodiscard]] auto AsArray() const -> const Array& { return *std::get<unique_ptr<Array>>(_value); }
        [[nodiscard]] auto AsDict() const -> const Dict& { return *std::get<unique_ptr<Dict>>(_value); }
        [[nodiscard]] auto Copy() const -> Value;

    private:
        std::variant<int64, float64, bool, string, unique_ptr<Array>, unique_ptr<Dict>> _value {};
    };

    class Array
    {
    public:
        Array() noexcept = default;
        Array(const Array&) = delete;
        Array(Array&&) noexcept = default;
        auto operator=(const Array&) = delete;
        auto operator=(Array&&) noexcept -> Array& = default;
        ~Array() = default;

        [[nodiscard]] auto operator==(const Array& other) const -> bool { return _value == other._value; }
        [[nodiscard]] auto operator[](size_t index) const -> const Value& { return _value.at(index); }
        [[nodiscard]] auto Size() const noexcept -> size_t { return _value.size(); }
        [[nodiscard]] auto Empty() const noexcept -> bool { return _value.empty(); }
        [[nodiscard]] auto Copy() const -> Array;

        [[nodiscard]] auto begin() const noexcept { return _value.cbegin(); }
        [[nodiscard]] auto end() const noexcept { return _value.cend(); }

        void Reserve(size_t size) noexcept { _value.reserve(size); }
        void EmplaceBack(Value value) noexcept { _value.emplace_back(std::move(value)); }

    private:
        vector<Value> _value {};
    };

    class Dict
    {
    public:
        Dict() noexcept = default;
        Dict(const Dict&) = delete;
        Dict(Dict&&) noexcept = default;
        auto operator=(const Dict&) = delete;
        auto operator=(Dict&&) noexcept -> Dict& = default;
        ~Dict() = default;

        [[nodiscard]] auto operator==(const Dict& other) const -> bool { return _value == other._value; }
        [[nodiscard]] auto operator[](const string& key) const -> const Value& { return _value.at(key); }
        [[nodiscard]] auto Size() const noexcept -> size_t { return _value.size(); }
        [[nodiscard]] auto Empty() const noexcept -> bool { return _value.empty(); }
        [[nodiscard]] auto Contains(string_view key) const noexcept -> bool { return _value.count(key) != 0; }
        [[nodiscard]] auto Copy() const -> Dict;

        [[nodiscard]] auto begin() const noexcept { return _value.cbegin(); }
        [[nodiscard]] auto end() const noexcept { return _value.cend(); }

        void Emplace(string key, Value value) noexcept { _value.emplace(std::move(key), std::move(value)); }
        void Assign(const string& key, Value value) noexcept { _value.insert_or_assign(key, std::move(value)); }

    private:
        map<string, Value> _value {};
    };

    class Document : public Dict
    {
    public:
        Document() noexcept = default;
        Document(const Document&) = delete;
        Document(Document&&) noexcept = default;
        auto operator=(const Document&) = delete;
        auto operator=(Document&&) noexcept -> Document& = default;
        ~Document() = default;

        [[nodiscard]] auto Copy() const -> Document;
    };

    [[nodiscard]] static auto ValueToString(const Value& value) -> string;
    [[nodiscard]] static auto ParseValue(const string& str, bool as_dict, bool as_array, ValueType value_type) -> Value;

private:
    [[nodiscard]] static auto ValueToCodedString(const Value& value) -> string;
    [[nodiscard]] static auto ReadToken(const char* str, string& result) -> const char*;
};

class StringEscaping final
{
public:
    StringEscaping() = delete;

    [[nodiscard]] static auto CodeString(string_view str) -> string;
    [[nodiscard]] static auto DecodeString(string_view str) -> string;
};

FO_END_NAMESPACE();
