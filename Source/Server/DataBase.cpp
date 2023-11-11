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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "DataBase.h"
#include "DiskFileSystem.h"
#include "Log.h"
#include "StringUtils.h"
#include "Version-Include.h"

#if FO_HAVE_JSON
#include "json.hpp"
#endif
#if FO_HAVE_UNQLITE
#include "unqlite.h"
#endif

DISABLE_WARNINGS_PUSH()
#if FO_HAVE_MONGO && !FO_SINGLEPLAYER
#include "mongoc/mongoc.h"
#endif
#include "bson/bson.h"
DISABLE_WARNINGS_POP()

#include "WinApi-Include.h"

class DataBaseImpl
{
public:
    DataBaseImpl() = default;
    DataBaseImpl(const DataBaseImpl&) = delete;
    DataBaseImpl(DataBaseImpl&&) noexcept = delete;
    auto operator=(const DataBaseImpl&) = delete;
    auto operator=(DataBaseImpl&&) noexcept = delete;
    virtual ~DataBaseImpl() = default;

    [[nodiscard]] virtual auto GetAllIds(string_view collection_name) -> vector<ident_t> = 0;
    [[nodiscard]] auto Get(string_view collection_name, ident_t id) -> AnyData::Document;

    void StartChanges();
    void Insert(string_view collection_name, ident_t id, const AnyData::Document& doc);
    void Update(string_view collection_name, ident_t id, string_view key, const AnyData::Value& value);
    void Delete(string_view collection_name, ident_t id);
    void CommitChanges();
    void WaitCommitThread();

protected:
    [[nodiscard]] virtual auto GetRecord(string_view collection_name, ident_t id) -> AnyData::Document = 0;
    virtual void InsertRecord(string_view collection_name, ident_t id, const AnyData::Document& doc) = 0;
    virtual void UpdateRecord(string_view collection_name, ident_t id, const AnyData::Document& doc) = 0;
    virtual void DeleteRecord(string_view collection_name, ident_t id) = 0;
    virtual void CommitRecords() = 0;

private:
    bool _changesStarted {};
    DataBase::Collections _recordChanges {};
    DataBase::RecordsState _newRecords {};
    DataBase::RecordsState _deletedRecords {};
    WorkThread _commitThread {"DataBaseCommiter"};
};

DataBase::DataBase() = default;
DataBase::DataBase(DataBase&&) noexcept = default;
auto DataBase::operator=(DataBase&&) noexcept -> DataBase& = default;
DataBase::~DataBase() = default;

DataBase::DataBase(DataBaseImpl* impl) :
    _impl {impl}
{
    STACK_TRACE_ENTRY();
}

DataBase::operator bool() const
{
    STACK_TRACE_ENTRY();

    return _impl != nullptr;
}

auto DataBase::GetAllIds(string_view collection_name) const -> vector<ident_t>
{
    STACK_TRACE_ENTRY();

    return _impl->GetAllIds(collection_name);
}

auto DataBase::Get(string_view collection_name, ident_t id) const -> AnyData::Document
{
    STACK_TRACE_ENTRY();

    return _impl->Get(collection_name, id);
}

auto DataBase::Valid(string_view collection_name, ident_t id) const -> bool
{
    STACK_TRACE_ENTRY();

    return !_impl->Get(collection_name, id).empty();
}

void DataBase::StartChanges()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    _impl->StartChanges();
}

void DataBase::Insert(string_view collection_name, ident_t id, const AnyData::Document& doc)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    _impl->Insert(collection_name, id, doc);
}

void DataBase::Update(string_view collection_name, ident_t id, string_view key, const AnyData::Value& value)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    _impl->Update(collection_name, id, key, value);
}

void DataBase::Delete(string_view collection_name, ident_t id)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    _impl->Delete(collection_name, id);
}

void DataBase::CommitChanges(bool wait_commit_complete)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    _impl->CommitChanges();

    if (wait_commit_complete) {
        _impl->WaitCommitThread();
    }
}

static void ValueToBson(string_view key, const AnyData::Value& value, bson_t* bson)
{
    STACK_TRACE_ENTRY();

    const auto value_index = value.index();
    if (value_index == AnyData::INT_VALUE) {
        if (!bson_append_int32(bson, key.data(), static_cast<int>(key.length()), std::get<AnyData::INT_VALUE>(value))) {
            throw DataBaseException("ValueToBson bson_append_int32", key, std::get<AnyData::INT_VALUE>(value));
        }
    }
    else if (value_index == AnyData::INT64_VALUE) {
        if (!bson_append_int64(bson, key.data(), static_cast<int>(key.length()), std::get<AnyData::INT64_VALUE>(value))) {
            throw DataBaseException("ValueToBson bson_append_int64", key, std::get<AnyData::INT64_VALUE>(value));
        }
    }
    else if (value_index == AnyData::DOUBLE_VALUE) {
        if (!bson_append_double(bson, key.data(), static_cast<int>(key.length()), std::get<AnyData::DOUBLE_VALUE>(value))) {
            throw DataBaseException("ValueToBson bson_append_double", key, std::get<AnyData::DOUBLE_VALUE>(value));
        }
    }
    else if (value_index == AnyData::BOOL_VALUE) {
        if (!bson_append_bool(bson, key.data(), static_cast<int>(key.length()), std::get<AnyData::BOOL_VALUE>(value))) {
            throw DataBaseException("ValueToBson bson_append_double", key, std::get<AnyData::BOOL_VALUE>(value));
        }
    }
    else if (value_index == AnyData::STRING_VALUE) {
        if (!bson_append_utf8(bson, key.data(), static_cast<int>(key.length()), std::get<AnyData::STRING_VALUE>(value).c_str(), static_cast<int>(std::get<AnyData::STRING_VALUE>(value).length()))) {
            throw DataBaseException("ValueToBson bson_append_double", key, std::get<AnyData::STRING_VALUE>(value));
        }
    }
    else if (value_index == AnyData::ARRAY_VALUE) {
        bson_t bson_arr;
        if (!bson_append_array_begin(bson, key.data(), static_cast<int>(key.length()), &bson_arr)) {
            throw DataBaseException("ValueToBson bson_append_array_begin", key);
        }

        const auto& arr = std::get<AnyData::ARRAY_VALUE>(value);
        auto arr_key_index = 0;
        for (const auto& arr_value : arr) {
            string arr_key = _str("{}", arr_key_index);
            arr_key_index++;

            const auto arr_value_index = arr_value.index();
            if (arr_value_index == AnyData::INT_VALUE) {
                if (!bson_append_int32(&bson_arr, arr_key.c_str(), static_cast<int>(arr_key.length()), std::get<AnyData::INT_VALUE>(arr_value))) {
                    throw DataBaseException("ValueToBson bson_append_int32", arr_key, std::get<AnyData::INT_VALUE>(arr_value));
                }
            }
            else if (arr_value_index == AnyData::INT64_VALUE) {
                if (!bson_append_int64(&bson_arr, arr_key.c_str(), static_cast<int>(arr_key.length()), std::get<AnyData::INT64_VALUE>(arr_value))) {
                    throw DataBaseException("ValueToBson bson_append_int64", arr_key, std::get<AnyData::INT64_VALUE>(arr_value));
                }
            }
            else if (arr_value_index == AnyData::DOUBLE_VALUE) {
                if (!bson_append_double(&bson_arr, arr_key.c_str(), static_cast<int>(arr_key.length()), std::get<AnyData::DOUBLE_VALUE>(arr_value))) {
                    throw DataBaseException("ValueToBson bson_append_double", arr_key, std::get<AnyData::DOUBLE_VALUE>(arr_value));
                }
            }
            else if (arr_value_index == AnyData::BOOL_VALUE) {
                if (!bson_append_bool(&bson_arr, arr_key.c_str(), static_cast<int>(arr_key.length()), std::get<AnyData::BOOL_VALUE>(arr_value))) {
                    throw DataBaseException("ValueToBson bson_append_bool", arr_key, std::get<AnyData::BOOL_VALUE>(arr_value));
                }
            }
            else if (arr_value_index == AnyData::STRING_VALUE) {
                if (!bson_append_utf8(&bson_arr, arr_key.c_str(), static_cast<int>(arr_key.length()), std::get<AnyData::STRING_VALUE>(arr_value).c_str(), static_cast<int>(std::get<AnyData::STRING_VALUE>(arr_value).length()))) {
                    throw DataBaseException("ValueToBson bson_append_utf8", arr_key, std::get<AnyData::STRING_VALUE>(arr_value));
                }
            }
            else {
                throw DataBaseException("ValueToBson Invalid type");
            }
        }

        if (!bson_append_array_end(bson, &bson_arr)) {
            throw DataBaseException("ValueToBson bson_append_array_end");
        }
    }
    else if (value_index == AnyData::DICT_VALUE) {
        bson_t bson_doc;
        if (!bson_append_document_begin(bson, key.data(), static_cast<int>(key.length()), &bson_doc)) {
            throw DataBaseException("ValueToBson bson_append_bool", key);
        }

        const auto& dict = std::get<AnyData::DICT_VALUE>(value);
        for (auto&& [dict_key, dict_value] : dict) {
            const auto dict_value_index = dict_value.index();
            if (dict_value_index == AnyData::INT_VALUE) {
                if (!bson_append_int32(&bson_doc, dict_key.c_str(), static_cast<int>(dict_key.length()), std::get<AnyData::INT_VALUE>(dict_value))) {
                    throw DataBaseException("ValueToBson bson_append_int32", dict_key, std::get<AnyData::INT_VALUE>(dict_value));
                }
            }
            else if (dict_value_index == AnyData::INT64_VALUE) {
                if (!bson_append_int64(&bson_doc, dict_key.c_str(), static_cast<int>(dict_key.length()), std::get<AnyData::INT64_VALUE>(dict_value))) {
                    throw DataBaseException("ValueToBson bson_append_int64", dict_key, std::get<AnyData::INT64_VALUE>(dict_value));
                }
            }
            else if (dict_value_index == AnyData::DOUBLE_VALUE) {
                if (!bson_append_double(&bson_doc, dict_key.c_str(), static_cast<int>(dict_key.length()), std::get<AnyData::DOUBLE_VALUE>(dict_value))) {
                    throw DataBaseException("ValueToBson bson_append_double", dict_key, std::get<AnyData::DOUBLE_VALUE>(dict_value));
                }
            }
            else if (dict_value_index == AnyData::BOOL_VALUE) {
                if (!bson_append_bool(&bson_doc, dict_key.c_str(), static_cast<int>(dict_key.length()), std::get<AnyData::BOOL_VALUE>(dict_value))) {
                    throw DataBaseException("ValueToBson bson_append_bool", dict_key, std::get<AnyData::BOOL_VALUE>(dict_value));
                }
            }
            else if (dict_value_index == AnyData::STRING_VALUE) {
                if (!bson_append_utf8(&bson_doc, dict_key.c_str(), static_cast<int>(dict_key.length()), std::get<AnyData::STRING_VALUE>(dict_value).c_str(), static_cast<int>(std::get<AnyData::STRING_VALUE>(dict_value).length()))) {
                    throw DataBaseException("ValueToBson bson_append_utf8", dict_key, std::get<AnyData::STRING_VALUE>(dict_value));
                }
            }
            else if (dict_value_index == AnyData::ARRAY_VALUE) {
                bson_t bson_arr;
                if (!bson_append_array_begin(&bson_doc, dict_key.c_str(), static_cast<int>(dict_key.length()), &bson_arr)) {
                    throw DataBaseException("ValueToBson bson_append_array_begin", dict_key);
                }

                const auto& arr = std::get<AnyData::ARRAY_VALUE>(dict_value);
                auto arr_key_index = 0;
                for (const auto& arr_value : arr) {
                    string arr_key = _str("{}", arr_key_index);
                    arr_key_index++;

                    const auto arr_value_index = arr_value.index();
                    if (arr_value_index == AnyData::INT_VALUE) {
                        if (!bson_append_int32(&bson_arr, arr_key.c_str(), static_cast<int>(arr_key.length()), std::get<AnyData::INT_VALUE>(arr_value))) {
                            throw DataBaseException("ValueToBson bson_append_int32", arr_key, std::get<AnyData::INT_VALUE>(arr_value));
                        }
                    }
                    else if (arr_value_index == AnyData::INT64_VALUE) {
                        if (!bson_append_int64(&bson_arr, arr_key.c_str(), static_cast<int>(arr_key.length()), std::get<AnyData::INT64_VALUE>(arr_value))) {
                            throw DataBaseException("ValueToBson bson_append_int64", arr_key, std::get<AnyData::INT64_VALUE>(arr_value));
                        }
                    }
                    else if (arr_value_index == AnyData::DOUBLE_VALUE) {
                        if (!bson_append_double(&bson_arr, arr_key.c_str(), static_cast<int>(arr_key.length()), std::get<AnyData::DOUBLE_VALUE>(arr_value))) {
                            throw DataBaseException("ValueToBson bson_append_double", arr_key, std::get<AnyData::DOUBLE_VALUE>(arr_value));
                        }
                    }
                    else if (arr_value_index == AnyData::BOOL_VALUE) {
                        if (!bson_append_bool(&bson_arr, arr_key.c_str(), static_cast<int>(arr_key.length()), std::get<AnyData::BOOL_VALUE>(arr_value))) {
                            throw DataBaseException("ValueToBson bson_append_bool", arr_key, std::get<AnyData::BOOL_VALUE>(arr_value));
                        }
                    }
                    else if (arr_value_index == AnyData::STRING_VALUE) {
                        if (!bson_append_utf8(&bson_arr, arr_key.c_str(), static_cast<int>(arr_key.length()), std::get<AnyData::STRING_VALUE>(arr_value).c_str(), static_cast<int>(std::get<AnyData::STRING_VALUE>(arr_value).length()))) {
                            throw DataBaseException("ValueToBson bson_append_utf8", arr_key, std::get<AnyData::STRING_VALUE>(arr_value));
                        }
                    }
                    else {
                        throw DataBaseException("ValueToBson Invalid type");
                    }
                }

                if (!bson_append_array_end(&bson_doc, &bson_arr)) {
                    throw DataBaseException("ValueToBson bson_append_array_end");
                }
            }
            else {
                throw DataBaseException("ValueToBson Invalid type");
            }
        }

        if (!bson_append_document_end(bson, &bson_doc)) {
            throw DataBaseException("ValueToBson bson_append_document_end");
        }
    }
    else {
        throw DataBaseException("ValueToBson Invalid type");
    }
}

static void DocumentToBson(const AnyData::Document& doc, bson_t* bson)
{
    STACK_TRACE_ENTRY();

    for (auto&& [key, value] : doc) {
        ValueToBson(key, value, bson);
    }
}

static auto BsonToValue(bson_iter_t* iter) -> AnyData::Value
{
    STACK_TRACE_ENTRY();

    const auto* value = bson_iter_value(iter);

    if (value->value_type == BSON_TYPE_INT32) {
        return value->value.v_int32;
    }
    else if (value->value_type == BSON_TYPE_INT64) {
        return value->value.v_int64;
    }
    else if (value->value_type == BSON_TYPE_DOUBLE) {
        return value->value.v_double;
    }
    else if (value->value_type == BSON_TYPE_BOOL) {
        return value->value.v_bool;
    }
    else if (value->value_type == BSON_TYPE_UTF8) {
        return string(value->value.v_utf8.str, value->value.v_utf8.len);
    }
    else if (value->value_type == BSON_TYPE_ARRAY) {
        bson_iter_t arr_iter;
        if (!bson_iter_recurse(iter, &arr_iter)) {
            throw DataBaseException("BsonToValue bson_iter_recurse");
        }

        AnyData::Array arr;
        while (bson_iter_next(&arr_iter)) {
            const auto* arr_value = bson_iter_value(&arr_iter);
            if (arr_value->value_type == BSON_TYPE_INT32) {
                arr.push_back(arr_value->value.v_int32);
            }
            else if (arr_value->value_type == BSON_TYPE_INT64) {
                arr.push_back(arr_value->value.v_int64);
            }
            else if (arr_value->value_type == BSON_TYPE_DOUBLE) {
                arr.push_back(arr_value->value.v_double);
            }
            else if (arr_value->value_type == BSON_TYPE_BOOL) {
                arr.push_back(arr_value->value.v_bool);
            }
            else if (arr_value->value_type == BSON_TYPE_UTF8) {
                arr.push_back(string(arr_value->value.v_utf8.str, arr_value->value.v_utf8.len));
            }
            else {
                throw DataBaseException("BsonToValue Invalid type");
            }
        }

        return std::move(arr);
    }
    else if (value->value_type == BSON_TYPE_DOCUMENT) {
        bson_iter_t doc_iter;
        if (!bson_iter_recurse(iter, &doc_iter)) {
            throw DataBaseException("BsonToValue bson_iter_recurse");
        }

        AnyData::Dict dict;
        while (bson_iter_next(&doc_iter)) {
            const auto* key = bson_iter_key(&doc_iter);
            const auto* dict_value = bson_iter_value(&doc_iter);
            if (dict_value->value_type == BSON_TYPE_INT32) {
                dict.emplace(string(key), dict_value->value.v_int32);
            }
            else if (dict_value->value_type == BSON_TYPE_INT64) {
                dict.emplace(string(key), dict_value->value.v_int64);
            }
            else if (dict_value->value_type == BSON_TYPE_DOUBLE) {
                dict.emplace(string(key), dict_value->value.v_double);
            }
            else if (dict_value->value_type == BSON_TYPE_BOOL) {
                dict.emplace(string(key), dict_value->value.v_bool);
            }
            else if (dict_value->value_type == BSON_TYPE_UTF8) {
                dict.emplace(string(key), string(dict_value->value.v_utf8.str, dict_value->value.v_utf8.len));
            }
            else if (dict_value->value_type == BSON_TYPE_ARRAY) {
                bson_iter_t doc_arr_iter;
                if (!bson_iter_recurse(&doc_iter, &doc_arr_iter)) {
                    throw DataBaseException("BsonToValue bson_iter_recurse");
                }

                AnyData::Array dict_array;
                while (bson_iter_next(&doc_arr_iter)) {
                    const auto* doc_arr_value = bson_iter_value(&doc_arr_iter);
                    if (doc_arr_value->value_type == BSON_TYPE_INT32) {
                        dict_array.push_back(doc_arr_value->value.v_int32);
                    }
                    else if (doc_arr_value->value_type == BSON_TYPE_INT64) {
                        dict_array.push_back(doc_arr_value->value.v_int64);
                    }
                    else if (doc_arr_value->value_type == BSON_TYPE_DOUBLE) {
                        dict_array.push_back(doc_arr_value->value.v_double);
                    }
                    else if (doc_arr_value->value_type == BSON_TYPE_BOOL) {
                        dict_array.push_back(doc_arr_value->value.v_bool);
                    }
                    else if (doc_arr_value->value_type == BSON_TYPE_UTF8) {
                        dict_array.push_back(string(doc_arr_value->value.v_utf8.str, doc_arr_value->value.v_utf8.len));
                    }
                    else {
                        throw DataBaseException("BsonToValue Invalid type");
                    }
                }

                dict.emplace(string(key), dict_array);
            }
            else {
                throw DataBaseException("BsonToValue Invalid type");
            }
        }

        return std::move(dict);
    }
    else {
        throw DataBaseException("BsonToValue Invalid type");
    }
}

static void BsonToDocument(const bson_t* bson, AnyData::Document& doc)
{
    STACK_TRACE_ENTRY();

    bson_iter_t iter;
    if (!bson_iter_init(&iter, bson)) {
        throw DataBaseException("BsonToDocument bson_iter_init");
    }

    while (bson_iter_next(&iter)) {
        const auto* key = bson_iter_key(&iter);

        if (key[0] == '_' && key[1] == 'i' && key[2] == 'd' && key[3] == 0) {
            continue;
        }

        auto value = BsonToValue(&iter);
        doc.emplace(string(key), std::move(value));
    }
}

auto DataBaseImpl::Get(string_view collection_name, ident_t id) -> AnyData::Document
{
    STACK_TRACE_ENTRY();

    const auto collection_name_str = string(collection_name);

    if (_deletedRecords[collection_name_str].count(id) != 0) {
        return {};
    }

    if (_newRecords[collection_name_str].count(id) != 0) {
        return _recordChanges[collection_name_str].at(id);
    }

    _commitThread.Wait();

    auto doc = GetRecord(collection_name_str, id);

    if (_recordChanges[collection_name_str].count(id) != 0) {
        for (auto&& [key, value] : _recordChanges[collection_name_str].at(id)) {
            doc[key] = value;
        }
    }

    return doc;
}

void DataBaseImpl::StartChanges()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_changesStarted);
    RUNTIME_ASSERT(_recordChanges.empty());
    RUNTIME_ASSERT(_newRecords.empty());
    RUNTIME_ASSERT(_deletedRecords.empty());

    _changesStarted = true;
}

void DataBaseImpl::Insert(string_view collection_name, ident_t id, const AnyData::Document& doc)
{
    STACK_TRACE_ENTRY();

    const auto collection_name_str = string(collection_name);

    RUNTIME_ASSERT(_changesStarted);
    RUNTIME_ASSERT(!_newRecords[collection_name_str].count(id));
    RUNTIME_ASSERT(!_deletedRecords[collection_name_str].count(id));

    _newRecords[collection_name_str].insert(id);
    for (auto&& [key, value] : doc) {
        _recordChanges[collection_name_str][id][key] = value;
    }
}

void DataBaseImpl::Update(string_view collection_name, ident_t id, string_view key, const AnyData::Value& value)
{
    STACK_TRACE_ENTRY();

    const auto collection_name_str = string(collection_name);

    RUNTIME_ASSERT(_changesStarted);
    RUNTIME_ASSERT(!_deletedRecords[collection_name_str].count(id));

    _recordChanges[collection_name_str][id][string(key)] = value;
}

void DataBaseImpl::Delete(string_view collection_name, ident_t id)
{
    STACK_TRACE_ENTRY();

    const auto collection_name_str = string(collection_name);

    RUNTIME_ASSERT(_changesStarted);
    RUNTIME_ASSERT(!_deletedRecords[collection_name_str].count(id));

    if (_newRecords[collection_name_str].count(id) != 0) {
        _newRecords[collection_name_str].erase(id);
    }
    else {
        _deletedRecords[collection_name_str].insert(id);
    }

    _recordChanges[collection_name_str].erase(id);
}

void DataBaseImpl::CommitChanges()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_changesStarted);

    _changesStarted = false;

    _commitThread.AddJob([this, recordChanges = std::move(_recordChanges), newRecords = std::move(_newRecords), deletedRecords = std::move(_deletedRecords)] {
        for (auto&& [key, value] : recordChanges) {
            for (auto&& [key2, value2] : value) {
                auto it = newRecords.find(key);
                if (it != newRecords.end() && it->second.count(key2) != 0) {
                    InsertRecord(key, key2, value2);
                }
                else {
                    UpdateRecord(key, key2, value2);
                }
            }
        }

        for (auto&& [key, value] : deletedRecords) {
            for (const auto& id : value) {
                DeleteRecord(key, id);
            }
        }

        CommitRecords();
    });

    _recordChanges.clear();
    _newRecords.clear();
    _deletedRecords.clear();
}

void DataBaseImpl::WaitCommitThread()
{
    STACK_TRACE_ENTRY();

    _commitThread.Wait();
}

#if FO_HAVE_JSON
class DbJson final : public DataBaseImpl
{
public:
    DbJson() = delete;
    DbJson(const DbJson&) = delete;
    DbJson(DbJson&&) noexcept = delete;
    auto operator=(const DbJson&) = delete;
    auto operator=(DbJson&&) noexcept = delete;
    ~DbJson() override = default;

    explicit DbJson(string_view storage_dir) :
        _storageDir {storage_dir}
    {
        DiskFileSystem::MakeDirTree(storage_dir);
    }

    [[nodiscard]] auto GetAllIds(string_view collection_name) -> vector<ident_t> override
    {
        STACK_TRACE_ENTRY();

        vector<ident_t> ids;

        DiskFileSystem::IterateDir(_str(_storageDir).combinePath(collection_name), "json", false, [&ids](string_view path, size_t size, uint64 write_time) {
            UNUSED_VARIABLE(size);
            UNUSED_VARIABLE(write_time);

            const string id_str = _str(path).extractFileName().eraseFileExtension();
            const auto id = _str(id_str).toUInt();
            if (id == 0) {
                throw DataBaseException("DbJson Id is zero", path);
            }

            ids.emplace_back(id);
        });

        return ids;
    }

protected:
    [[nodiscard]] auto GetRecord(string_view collection_name, ident_t id) -> AnyData::Document override
    {
        STACK_TRACE_ENTRY();

        const string path = _str("{}/{}/{}.json", _storageDir, collection_name, id);

        size_t length;
        char* json;
        if (auto f = DiskFileSystem::OpenFile(path, false)) {
            length = f.GetSize();
            json = new char[length];
            if (!f.Read(json, length)) {
                throw DataBaseException("DbJson Can't read file", path);
            }
        }
        else {
            return {};
        }

        bson_t bson;
        bson_error_t error;
        if (!bson_init_from_json(&bson, json, static_cast<ssize_t>(length), &error)) {
            throw DataBaseException("DbJson bson_init_from_json", path);
        }

        delete[] json;

        AnyData::Document doc;
        BsonToDocument(&bson, doc);

        bson_destroy(&bson);
        return doc;
    }

    void InsertRecord(string_view collection_name, ident_t id, const AnyData::Document& doc) override
    {
        STACK_TRACE_ENTRY();

        RUNTIME_ASSERT(!doc.empty());

        const string path = _str("{}/{}/{}.json", _storageDir, collection_name, id);

        if (const auto f_check = DiskFileSystem::OpenFile(path, false)) {
            throw DataBaseException("DbJson File exists for inserting", path);
        }

        bson_t bson;
        bson_init(&bson);
        DocumentToBson(doc, &bson);

        size_t length = 0;
        auto* json = bson_as_canonical_extended_json(&bson, &length);
        if (json == nullptr) {
            throw DataBaseException("DbJson bson_as_canonical_extended_json", path);
        }

        bson_destroy(&bson);

        const auto pretty_json = nlohmann::json::parse(json);
        const auto pretty_json_dump = pretty_json.dump(4);
        bson_free(json);

        if (auto f = DiskFileSystem::OpenFile(path, true)) {
            if (!f.Write(pretty_json_dump)) {
                throw DataBaseException("DbJson Can't write file", path);
            }
        }
        else {
            throw DataBaseException("DbJson Can't open file", path);
        }
    }

    void UpdateRecord(string_view collection_name, ident_t id, const AnyData::Document& doc) override
    {
        STACK_TRACE_ENTRY();

        RUNTIME_ASSERT(!doc.empty());

        const string path = _str("{}/{}/{}.json", _storageDir, collection_name, id);

        size_t length;
        char* json;
        if (auto f_read = DiskFileSystem::OpenFile(path, false)) {
            length = f_read.GetSize();
            json = new char[length];
            if (!f_read.Read(json, length)) {
                throw DataBaseException("DbJson Can't read file", path);
            }
        }
        else {
            throw DataBaseException("DbJson Can't open file for reading", path);
        }

        bson_t bson;
        bson_error_t error;
        if (!bson_init_from_json(&bson, json, static_cast<ssize_t>(length), &error)) {
            throw DataBaseException("DbJson bson_init_from_json", path);
        }

        delete[] json;

        DocumentToBson(doc, &bson);

        size_t new_length = 0;
        auto* new_json = bson_as_canonical_extended_json(&bson, &new_length);
        if (new_json == nullptr) {
            throw DataBaseException("DbJson bson_as_canonical_extended_json", path);
        }

        bson_destroy(&bson);

        const auto pretty_json = nlohmann::json::parse(new_json);
        const auto pretty_json_dump = pretty_json.dump(4);
        bson_free(new_json);

        if (auto f_write = DiskFileSystem::OpenFile(path, true)) {
            if (!f_write.Write(pretty_json_dump)) {
                throw DataBaseException("DbJson Can't write file", path);
            }
        }
        else {
            throw DataBaseException("DbJson Can't open file for writing", path);
        }
    }

    void DeleteRecord(string_view collection_name, ident_t id) override
    {
        STACK_TRACE_ENTRY();

        const string path = _str("{}/{}/{}.json", _storageDir, collection_name, id);
        if (!DiskFileSystem::DeleteFile(path)) {
            throw DataBaseException("DbJson Can't delete file", path);
        }
    }

    void CommitRecords() override
    {
        STACK_TRACE_ENTRY();

        // Nothing
    }

private:
    string _storageDir {};
};
#endif

#if FO_HAVE_UNQLITE
class DbUnQLite final : public DataBaseImpl
{
public:
    DbUnQLite() = delete;
    DbUnQLite(const DbUnQLite&) = delete;
    DbUnQLite(DbUnQLite&&) noexcept = delete;
    auto operator=(const DbUnQLite&) = delete;
    auto operator=(DbUnQLite&&) noexcept = delete;

    explicit DbUnQLite(string_view storage_dir)
    {
        STACK_TRACE_ENTRY();

        DiskFileSystem::MakeDirTree(storage_dir);

        unqlite* ping_db = nullptr;
        const auto ping_db_path = _str("{}/Ping.unqlite", storage_dir);
        if (unqlite_open(&ping_db, ping_db_path.c_str(), UNQLITE_OPEN_CREATE | UNQLITE_OPEN_OMIT_JOURNALING) != UNQLITE_OK) {
            throw DataBaseException("DbUnQLite Can't open db", ping_db_path);
        }

        constexpr auto ping = 42u;
        if (unqlite_kv_store(ping_db, &ping, sizeof(ping), &ping, sizeof(ping)) != UNQLITE_OK) {
            unqlite_close(ping_db);
            throw DataBaseException("DbUnQLite Can't write to db", ping_db_path);
        }

        unqlite_close(ping_db);
        DiskFileSystem::DeleteFile(ping_db_path);

        _storageDir = storage_dir;
    }

    ~DbUnQLite() override
    {
        STACK_TRACE_ENTRY();

        for (auto&& [key, value] : _collections) {
            unqlite_close(value);
        }
    }

    [[nodiscard]] auto GetAllIds(string_view collection_name) -> vector<ident_t> override
    {
        STACK_TRACE_ENTRY();

        auto* db = GetCollection(collection_name);
        if (db == nullptr) {
            throw DataBaseException("DbUnQLite Can't open collection", collection_name);
        }

        unqlite_kv_cursor* cursor = nullptr;
        const auto kv_cursor_init = unqlite_kv_cursor_init(db, &cursor);
        if (kv_cursor_init != UNQLITE_OK) {
            throw DataBaseException("DbUnQLite unqlite_kv_cursor_init", kv_cursor_init);
        }

        const auto kv_cursor_first_entry = unqlite_kv_cursor_first_entry(cursor);
        if (kv_cursor_first_entry != UNQLITE_OK && kv_cursor_first_entry != UNQLITE_DONE) {
            throw DataBaseException("DbUnQLite unqlite_kv_cursor_first_entry", kv_cursor_first_entry);
        }

        vector<ident_t> ids;

        while (unqlite_kv_cursor_valid_entry(cursor) != 0) {
            auto id = 0u;
            const auto kv_cursor_key_callback = unqlite_kv_cursor_key_callback(
                cursor,
                [](const void* output, unsigned int output_len, void* user_data) {
                    RUNTIME_ASSERT(output_len == sizeof(uint));
                    *static_cast<uint*>(user_data) = *static_cast<const uint*>(output);
                    return UNQLITE_OK;
                },
                &id);

            if (kv_cursor_key_callback != UNQLITE_OK) {
                throw DataBaseException("DbUnQLite unqlite_kv_cursor_init", kv_cursor_key_callback);
            }
            if (id == 0) {
                throw DataBaseException("DbUnQLite Id is zero");
            }

            ids.emplace_back(id);

            const auto kv_cursor_next_entry = unqlite_kv_cursor_next_entry(cursor);
            if (kv_cursor_next_entry != UNQLITE_OK && kv_cursor_next_entry != UNQLITE_DONE) {
                throw DataBaseException("DbUnQLite kv_cursor_next_entry", kv_cursor_next_entry);
            }
        }

        return ids;
    }

protected:
    [[nodiscard]] auto GetRecord(string_view collection_name, ident_t id) -> AnyData::Document override
    {
        STACK_TRACE_ENTRY();

        auto* db = GetCollection(collection_name);
        if (db == nullptr) {
            throw DataBaseException("DbUnQLite Can't open collection", collection_name);
        }

        AnyData::Document doc;
        const auto kv_fetch_callback = unqlite_kv_fetch_callback(
            db, &id, sizeof(id),
            [](const void* output, unsigned int output_len, void* user_data) {
                bson_t bson;
                if (!bson_init_static(&bson, static_cast<const uint8*>(output), output_len)) {
                    throw DataBaseException("DbUnQLite bson_init_static");
                }

                auto& doc2 = *static_cast<AnyData::Document*>(user_data);
                BsonToDocument(&bson, doc2);

                return UNQLITE_OK;
            },
            &doc);

        if (kv_fetch_callback != UNQLITE_OK && kv_fetch_callback != UNQLITE_NOTFOUND) {
            throw DataBaseException("DbUnQLite unqlite_kv_fetch_callback", kv_fetch_callback);
        }

        return doc;
    }

    void InsertRecord(string_view collection_name, ident_t id, const AnyData::Document& doc) override
    {
        STACK_TRACE_ENTRY();

        RUNTIME_ASSERT(!doc.empty());

        auto* db = GetCollection(collection_name);
        if (db == nullptr) {
            throw DataBaseException("DbUnQLite Can't open collection", collection_name);
        }

        const auto kv_fetch_callback = unqlite_kv_fetch_callback(
            db, &id, sizeof(id), [](const void*, unsigned int, void*) { return UNQLITE_OK; }, nullptr);
        if (kv_fetch_callback != UNQLITE_NOTFOUND) {
            throw DataBaseException("DbUnQLite unqlite_kv_fetch_callback", kv_fetch_callback);
        }

        bson_t bson;
        bson_init(&bson);
        DocumentToBson(doc, &bson);

        const auto* bson_data = bson_get_data(&bson);
        if (bson_data == nullptr) {
            throw DataBaseException("DbUnQLite bson_get_data");
        }

        const auto kv_store = unqlite_kv_store(db, &id, sizeof(id), bson_data, bson.len);
        if (kv_store != UNQLITE_OK) {
            bson_destroy(&bson);
            throw DataBaseException("DbUnQLite unqlite_kv_store", kv_store);
        }

        bson_destroy(&bson);
    }

    void UpdateRecord(string_view collection_name, ident_t id, const AnyData::Document& doc) override
    {
        STACK_TRACE_ENTRY();

        RUNTIME_ASSERT(!doc.empty());

        auto* db = GetCollection(collection_name);
        if (db == nullptr) {
            throw DataBaseException("DbUnQLite Can't open collection", collection_name);
        }

        auto actual_doc = Get(collection_name, id);
        if (actual_doc.empty()) {
            throw DataBaseException("DbUnQLite Document not found", collection_name, id);
        }

        for (auto&& [key, value] : doc) {
            actual_doc[key] = value;
        }

        bson_t bson;
        bson_init(&bson);
        DocumentToBson(actual_doc, &bson);

        const auto* bson_data = bson_get_data(&bson);
        if (bson_data == nullptr) {
            throw DataBaseException("DbUnQLite bson_get_data");
        }

        const auto kv_store = unqlite_kv_store(db, &id, sizeof(id), bson_data, bson.len);
        if (kv_store != UNQLITE_OK) {
            bson_destroy(&bson);
            throw DataBaseException("DbUnQLite unqlite_kv_store", kv_store);
        }

        bson_destroy(&bson);
    }

    void DeleteRecord(string_view collection_name, ident_t id) override
    {
        STACK_TRACE_ENTRY();

        auto* db = GetCollection(collection_name);
        if (db == nullptr) {
            throw DataBaseException("DbUnQLite Can't open collection", collection_name);
        }

        const auto kv_delete = unqlite_kv_delete(db, &id, sizeof(id));
        if (kv_delete != UNQLITE_OK) {
            throw DataBaseException("DbUnQLite unqlite_kv_delete", kv_delete);
        }
    }

    void CommitRecords() override
    {
        STACK_TRACE_ENTRY();

        for (auto&& [key, value] : _collections) {
            const auto commit = unqlite_commit(value);
            if (commit != UNQLITE_OK) {
                throw DataBaseException("DbUnQLite unqlite_commit", commit);
            }
        }
    }

private:
    [[nodiscard]] auto GetCollection(string_view collection_name) const -> unqlite*
    {
        STACK_TRACE_ENTRY();

        unqlite* db;

        const auto it = _collections.find(string(collection_name));
        if (it == _collections.end()) {
            const string db_path = _str("{}/{}.unqlite", _storageDir, collection_name);
            const auto r = unqlite_open(&db, db_path.c_str(), UNQLITE_OPEN_CREATE | UNQLITE_OPEN_OMIT_JOURNALING);
            if (r != UNQLITE_OK) {
                throw DataBaseException("DbUnQLite Can't open db", collection_name, r);
            }

            _collections.emplace(collection_name, db);
        }
        else {
            db = it->second;
        }

        return db;
    }

    string _storageDir {};
    mutable map<string, unqlite*> _collections {};
};
#endif

#if FO_HAVE_MONGO && !FO_SINGLEPLAYER
class DbMongo final : public DataBaseImpl
{
public:
    DbMongo() = delete;
    DbMongo(const DbMongo&) = delete;
    DbMongo(DbMongo&&) noexcept = delete;
    auto operator=(const DbMongo&) = delete;
    auto operator=(DbMongo&&) noexcept = delete;

    DbMongo(string_view uri, string_view db_name)
    {
        STACK_TRACE_ENTRY();

        mongoc_init();

        bson_error_t error;
        auto* mongo_uri = mongoc_uri_new_with_error(string(uri).c_str(), &error);
        if (mongo_uri == nullptr) {
            throw DataBaseException("DbMongo Failed to parse URI", uri, error.message);
        }

        auto* client = mongoc_client_new_from_uri(mongo_uri);
        if (client == nullptr) {
            throw DataBaseException("DbMongo Can't create client");
        }

        mongoc_uri_destroy(mongo_uri);
        mongoc_client_set_appname(client, FO_DEV_NAME);

        auto* database = mongoc_client_get_database(client, string(db_name).c_str());
        if (database == nullptr) {
            throw DataBaseException("DbMongo Can't get database", db_name);
        }

        // Ping
        const auto* ping = BCON_NEW("ping", BCON_INT32(1));
        bson_t reply;
        if (!mongoc_client_command_simple(client, "admin", ping, nullptr, &reply, &error)) {
            throw DataBaseException("DbMongo Can't ping database", error.message);
        }

        _client = client;
        _database = database;
        _databaseName = db_name;
    }

    ~DbMongo() override
    {
        STACK_TRACE_ENTRY();

        for (auto&& [key, value] : _collections) {
            mongoc_collection_destroy(value);
        }

        mongoc_database_destroy(_database);
        mongoc_client_destroy(_client);
        mongoc_cleanup();
    }

    [[nodiscard]] auto GetAllIds(string_view collection_name) -> vector<ident_t> override
    {
        STACK_TRACE_ENTRY();

        auto* collection = GetCollection(collection_name);
        if (collection == nullptr) {
            throw DataBaseException("DbMongo Can't get collection", collection_name);
        }

        bson_t filter;
        bson_init(&filter);

        auto* opts = BCON_NEW("projection", "{", "_id", BCON_BOOL(true), "}");

        auto* cursor = mongoc_collection_find_with_opts(collection, &filter, opts, nullptr);
        if (cursor == nullptr) {
            throw DataBaseException("DbMongo mongoc_collection_find", collection_name);
        }

        vector<ident_t> ids;

        const bson_t* document = nullptr;
        while (mongoc_cursor_next(cursor, &document)) {
            bson_iter_t iter;
            if (!bson_iter_init(&iter, document)) {
                throw DataBaseException("DbMongo bson_iter_init", collection_name);
            }
            if (!bson_iter_next(&iter)) {
                throw DataBaseException("DbMongo bson_iter_next", collection_name);
            }
            if (bson_iter_type(&iter) != BSON_TYPE_INT32) {
                throw DataBaseException("DbMongo bson_iter_type", collection_name, bson_iter_type(&iter));
            }

            const auto id = static_cast<uint>(bson_iter_int32(&iter));
            ids.emplace_back(id);
        }

        bson_error_t error;
        if (mongoc_cursor_error(cursor, &error)) {
            throw DataBaseException("DbMongo mongoc_cursor_error", collection_name, error.message);
        }

        mongoc_cursor_destroy(cursor);
        bson_destroy(&filter);
        bson_destroy(opts);

        return ids;
    }

protected:
    [[nodiscard]] auto GetRecord(string_view collection_name, ident_t id) -> AnyData::Document override
    {
        STACK_TRACE_ENTRY();

        auto* collection = GetCollection(collection_name);
        if (collection == nullptr) {
            throw DataBaseException("DbMongo Can't get collection", collection_name);
        }

        bson_t filter;
        bson_init(&filter);

        static_assert(sizeof(ident_t) == sizeof(uint));
        if (!bson_append_int32(&filter, "_id", 3, static_cast<int32_t>(id.underlying_value()))) {
            throw DataBaseException("DbMongo bson_append_int32", collection_name, id);
        }

        bson_t opts;
        bson_init(&opts);

        if (!bson_append_int32(&opts, "limit", 5, 1)) {
            throw DataBaseException("DbMongo bson_append_int32", collection_name, id);
        }

        auto* cursor = mongoc_collection_find_with_opts(collection, &filter, &opts, nullptr);
        if (cursor == nullptr) {
            throw DataBaseException("DbMongo mongoc_collection_find", collection_name, id);
        }

        const bson_t* bson = nullptr;
        if (!mongoc_cursor_next(cursor, &bson)) {
            mongoc_cursor_destroy(cursor);
            bson_destroy(&filter);
            bson_destroy(&opts);
            return AnyData::Document();
        }

        AnyData::Document doc;
        BsonToDocument(bson, doc);

        mongoc_cursor_destroy(cursor);
        bson_destroy(&filter);
        bson_destroy(&opts);
        return doc;
    }

    void InsertRecord(string_view collection_name, ident_t id, const AnyData::Document& doc) override
    {
        STACK_TRACE_ENTRY();

        RUNTIME_ASSERT(!doc.empty());

        auto* collection = GetCollection(collection_name);
        if (collection == nullptr) {
            throw DataBaseException("DbMongo Can't get collection", collection_name);
        }

        bson_t insert;
        bson_init(&insert);

        static_assert(sizeof(ident_t) == sizeof(uint));
        if (!bson_append_int32(&insert, "_id", 3, static_cast<int32_t>(id.underlying_value()))) {
            throw DataBaseException("DbMongo bson_append_int32", collection_name, id);
        }

        DocumentToBson(doc, &insert);

        bson_error_t error;
        if (!mongoc_collection_insert(collection, MONGOC_INSERT_NONE, &insert, nullptr, &error)) {
            throw DataBaseException("DbMongo mongoc_collection_insert", collection_name, id, error.message);
        }

        bson_destroy(&insert);
    }

    void UpdateRecord(string_view collection_name, ident_t id, const AnyData::Document& doc) override
    {
        STACK_TRACE_ENTRY();

        RUNTIME_ASSERT(!doc.empty());

        auto* collection = GetCollection(collection_name);
        if (collection == nullptr) {
            throw DataBaseException("DbMongo Can't get collection", collection_name);
        }

        bson_t selector;
        bson_init(&selector);

        static_assert(sizeof(ident_t) == sizeof(uint));
        if (!bson_append_int32(&selector, "_id", 3, static_cast<int32_t>(id.underlying_value()))) {
            throw DataBaseException("DbMongo bson_append_int32", collection_name, id);
        }

        bson_t update;
        bson_init(&update);

        bson_t update_set;
        if (!bson_append_document_begin(&update, "$set", 4, &update_set)) {
            throw DataBaseException("DbMongo bson_append_document_begin", collection_name, id);
        }

        DocumentToBson(doc, &update_set);

        if (!bson_append_document_end(&update, &update_set)) {
            throw DataBaseException("DbMongo bson_append_document_end", collection_name, id);
        }

        bson_error_t error;
        if (!mongoc_collection_update(collection, MONGOC_UPDATE_NONE, &selector, &update, nullptr, &error)) {
            throw DataBaseException("DbMongo mongoc_collection_update", collection_name, id, error.message);
        }

        bson_destroy(&selector);
        bson_destroy(&update);
    }

    void DeleteRecord(string_view collection_name, ident_t id) override
    {
        STACK_TRACE_ENTRY();

        auto* collection = GetCollection(collection_name);
        if (collection == nullptr) {
            throw DataBaseException("DbMongo Can't get collection", collection_name);
        }

        bson_t selector;
        bson_init(&selector);

        static_assert(sizeof(ident_t) == sizeof(uint));
        if (!bson_append_int32(&selector, "_id", 3, static_cast<int32_t>(id.underlying_value()))) {
            throw DataBaseException("DbMongo bson_append_int32", collection_name, id);
        }

        bson_error_t error;
        if (!mongoc_collection_remove(collection, MONGOC_REMOVE_SINGLE_REMOVE, &selector, nullptr, &error)) {
            throw DataBaseException("DbMongo mongoc_collection_remove", collection_name, id, error.message);
        }

        bson_destroy(&selector);
    }

    void CommitRecords() override
    {
        STACK_TRACE_ENTRY();

        // Nothing
    }

private:
    [[nodiscard]] auto GetCollection(string_view collection_name) const -> mongoc_collection_t*
    {
        STACK_TRACE_ENTRY();

        mongoc_collection_t* collection;

        const auto it = _collections.find(string(collection_name));
        if (it == _collections.end()) {
            collection = mongoc_client_get_collection(_client, _databaseName.c_str(), string(collection_name).c_str());
            if (collection == nullptr) {
                throw DataBaseException("DbMongo Can't get collection", collection_name);
            }

            _collections.emplace(collection_name, collection);
        }
        else {
            collection = it->second;
        }

        return collection;
    }

    mongoc_client_t* _client {};
    mongoc_database_t* _database {};
    string _databaseName {};
    mutable map<string, mongoc_collection_t*> _collections {};
};
#endif

class DbMemory final : public DataBaseImpl
{
public:
    DbMemory() = default;
    DbMemory(const DbMemory&) = delete;
    DbMemory(DbMemory&&) noexcept = delete;
    auto operator=(const DbMemory&) = delete;
    auto operator=(DbMemory&&) noexcept = delete;
    ~DbMemory() override = default;

    [[nodiscard]] auto GetAllIds(string_view collection_name) -> vector<ident_t> override
    {
        STACK_TRACE_ENTRY();

        const auto& collection = _collections[string(collection_name)];

        vector<ident_t> ids;
        ids.reserve(collection.size());

        for (auto&& [key, value] : collection) {
            ids.emplace_back(key);
        }

        return ids;
    }

protected:
    [[nodiscard]] auto GetRecord(string_view collection_name, ident_t id) -> AnyData::Document override
    {
        STACK_TRACE_ENTRY();

        const auto& collection = _collections[string(collection_name)];

        const auto it = collection.find(id);
        return it != collection.end() ? it->second : AnyData::Document();
    }

    void InsertRecord(string_view collection_name, ident_t id, const AnyData::Document& doc) override
    {
        STACK_TRACE_ENTRY();

        RUNTIME_ASSERT(!doc.empty());

        auto& collection = _collections[string(collection_name)];
        RUNTIME_ASSERT(!collection.count(id));

        collection.emplace(id, doc);
    }

    void UpdateRecord(string_view collection_name, ident_t id, const AnyData::Document& doc) override
    {
        STACK_TRACE_ENTRY();

        RUNTIME_ASSERT(!doc.empty());

        auto& collection = _collections[string(collection_name)];

        const auto it = collection.find(id);
        RUNTIME_ASSERT(it != collection.end());

        for (auto&& [key, value] : doc) {
            it->second[key] = value;
        }
    }

    void DeleteRecord(string_view collection_name, ident_t id) override
    {
        STACK_TRACE_ENTRY();

        auto& collection = _collections[string(collection_name)];

        const auto it = collection.find(id);
        RUNTIME_ASSERT(it != collection.end());

        collection.erase(it);
    }

    void CommitRecords() override
    {
        STACK_TRACE_ENTRY();

        // Nothing
    }

private:
    DataBase::Collections _collections {};
};

auto ConnectToDataBase(string_view connection_info) -> DataBase
{
    STACK_TRACE_ENTRY();

    if (const auto options = _str(connection_info).split(' '); !options.empty()) {
        WriteLog("Connect to {} data base", options.front());

#if FO_HAVE_JSON
        if (options.front() == "JSON" && options.size() == 2) {
            return DataBase(new DbJson(options[1]));
        }
#endif
#if FO_HAVE_UNQLITE
        if (options.front() == "DbUnQLite" && options.size() == 2) {
            return DataBase(new DbUnQLite(options[1]));
        }
#endif
#if FO_HAVE_MONGO && !FO_SINGLEPLAYER
        if (options.front() == "Mongo" && options.size() == 3) {
            return DataBase(new DbMongo(options[1], options[2]));
        }
#endif
        if (options.front() == "Memory" && options.size() == 1) {
            return DataBase(new DbMemory());
        }
    }

    throw DataBaseException("Wrong storage options", connection_info);
}
