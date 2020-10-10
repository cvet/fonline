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
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

DECLARE_EXCEPTION(DataBaseException);

class DataBase
{
public:
    static constexpr auto INT_VALUE = 0;
    static constexpr auto INT64_VALUE = 1;
    static constexpr auto DOUBLE_VALUE = 2;
    static constexpr auto BOOL_VALUE = 3;
    static constexpr auto STRING_VALUE = 4;
    static constexpr auto ARRAY_VALUE = 5;
    static constexpr auto DICT_VALUE = 6;

    using Array = vector<std::variant<int, int64, double, bool, string>>;
    using Dict = map<string, std::variant<int, int64, double, bool, string, Array>>;
    using Value = std::variant<int, int64, double, bool, string, Array, Dict>;
    using Document = map<string, Value>;
    using Collection = map<uint, Document>;
    using Collections = map<string, Collection>;
    using RecordsState = map<string, set<uint>>;

    DataBase() = default;
    DataBase(const DataBase&) = delete;
    DataBase(DataBase&&) noexcept = default;
    auto operator=(const DataBase&) = delete;
    auto operator=(DataBase&&) noexcept = delete;
    virtual ~DataBase() = default;

    [[nodiscard]] virtual auto GetAllIds(const string& collection_name) -> UIntVec = 0;
    [[nodiscard]] auto Get(const string& collection_name, uint id) -> Document;

    void StartChanges();
    void Insert(const string& collection_name, uint id, const Document& doc);
    void Update(const string& collection_name, uint id, const string& key, const Value& value);
    void Delete(const string& collection_name, uint id);
    void CommitChanges();

protected:
    [[nodiscard]] virtual auto GetRecord(const string& collection_name, uint id) -> Document = 0;
    virtual void InsertRecord(const string& collection_name, uint id, const Document& doc) = 0;
    virtual void UpdateRecord(const string& collection_name, uint id, const Document& doc) = 0;
    virtual void DeleteRecord(const string& collection_name, uint id) = 0;
    virtual void CommitRecords() = 0;

private:
    bool _changesStarted {};
    Collections _recordChanges {};
    RecordsState _newRecords {};
    RecordsState _deletedRecords {};
};

extern auto GetDataBase(const string& connection_info) -> DataBase*;
