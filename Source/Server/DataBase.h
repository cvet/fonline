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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "AnyData.h"

DECLARE_EXCEPTION(DataBaseException);

class DataBaseImpl;

class DataBase
{
    friend auto ConnectToDataBase(string_view connection_info) -> DataBase;

public:
    using Collection = map<uint, AnyData::Document>;
    using Collections = map<string, Collection>;
    using RecordsState = map<string, set<uint>>;

    DataBase();
    DataBase(const DataBase&) = delete;
    DataBase(DataBase&&) noexcept;
    auto operator=(const DataBase&) = delete;
    auto operator=(DataBase&&) noexcept -> DataBase&;
    explicit operator bool() const;
    ~DataBase();

    [[nodiscard]] auto GetAllIds(string_view collection_name) const -> vector<uint>;
    [[nodiscard]] auto Get(string_view collection_name, uint id) const -> AnyData::Document;
    [[nodiscard]] auto Valid(string_view collection_name, uint id) const -> bool;

    void StartChanges();
    void Insert(string_view collection_name, uint id, const AnyData::Document& doc);
    void Update(string_view collection_name, uint id, string_view key, const AnyData::Value& value);
    void Delete(string_view collection_name, uint id);
    void CommitChanges();

private:
    explicit DataBase(DataBaseImpl* impl);
    unique_ptr<DataBaseImpl> _impl {};
    bool _nonConstHelper {};
};

extern auto ConnectToDataBase(string_view connection_info) -> DataBase;
