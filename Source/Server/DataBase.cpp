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

#define FO_HAVE_JSON
#define FO_HAVE_UNQLITE
#define FO_HAVE_MONGO

#if defined(FO_SINGLEPLAYER)
#undef FO_HAVE_JSON
#undef FO_HAVE_MONGO
#endif
#if defined(FO_WEB)
#undef FO_HAVE_UNQLITE
#endif

#include "DataBase.h"
#include "DiskFileSystem.h"
#include "StringUtils.h"
#include "WinApi-Include.h"

#if defined(FO_HAVE_JSON)
#include "json.hpp"
#endif
#if defined(FO_HAVE_UNQLITE)
#include "unqlite.h"
#endif
#if defined(FO_HAVE_MONGO)
#include "mongoc.h"
#endif
#include "bson.h"

static void ValueToBson(const string& key, const DataBase::Value& value, bson_t* bson)
{
    const auto value_index = value.index();
    if (value_index == DataBase::INT_VALUE) {
        if (!bson_append_int32(bson, key.c_str(), static_cast<int>(key.length()), std::get<int>(value))) {
            throw DataBaseException("ValueToBson bson_append_int32", key, std::get<int>(value));
        }
    }
    else if (value_index == DataBase::INT64_VALUE) {
        if (!bson_append_int64(bson, key.c_str(), static_cast<int>(key.length()), std::get<int64>(value))) {
            throw DataBaseException("ValueToBson bson_append_int64", key, std::get<int64>(value));
        }
    }
    else if (value_index == DataBase::DOUBLE_VALUE) {
        if (!bson_append_double(bson, key.c_str(), static_cast<int>(key.length()), std::get<double>(value))) {
            throw DataBaseException("ValueToBson bson_append_double", key, std::get<double>(value));
        }
    }
    else if (value_index == DataBase::BOOL_VALUE) {
        if (!bson_append_bool(bson, key.c_str(), static_cast<int>(key.length()), std::get<bool>(value))) {
            throw DataBaseException("ValueToBson bson_append_double", key, std::get<bool>(value));
        }
    }
    else if (value_index == DataBase::STRING_VALUE) {
        if (!bson_append_utf8(bson, key.c_str(), static_cast<int>(key.length()), std::get<string>(value).c_str(), static_cast<int>(std::get<string>(value).length()))) {
            throw DataBaseException("ValueToBson bson_append_double", key, std::get<string>(value));
        }
    }
    else if (value_index == DataBase::ARRAY_VALUE) {
        bson_t bson_arr;
        if (!bson_append_array_begin(bson, key.c_str(), static_cast<int>(key.length()), &bson_arr)) {
            throw DataBaseException("ValueToBson bson_append_array_begin", key);
        }

        const auto& arr = std::get<DataBase::Array>(value);
        auto arr_key_index = 0;
        for (const auto& arr_value : arr) {
            string arr_key = _str("{}", arr_key_index);
            arr_key_index++;

            const auto arr_value_index = arr_value.index();
            if (arr_value_index == DataBase::INT_VALUE) {
                if (!bson_append_int32(&bson_arr, arr_key.c_str(), static_cast<int>(arr_key.length()), std::get<int>(arr_value))) {
                    throw DataBaseException("ValueToBson bson_append_int32", arr_key, std::get<int>(arr_value));
                }
            }
            else if (arr_value_index == DataBase::INT64_VALUE) {
                if (!bson_append_int64(&bson_arr, arr_key.c_str(), static_cast<int>(arr_key.length()), std::get<int64>(arr_value))) {
                    throw DataBaseException("ValueToBson bson_append_int64", arr_key, std::get<int64>(arr_value));
                }
            }
            else if (arr_value_index == DataBase::DOUBLE_VALUE) {
                if (!bson_append_double(&bson_arr, arr_key.c_str(), static_cast<int>(arr_key.length()), std::get<double>(arr_value))) {
                    throw DataBaseException("ValueToBson bson_append_double", arr_key, std::get<double>(arr_value));
                }
            }
            else if (arr_value_index == DataBase::BOOL_VALUE) {
                if (!bson_append_bool(&bson_arr, arr_key.c_str(), static_cast<int>(arr_key.length()), std::get<bool>(arr_value))) {
                    throw DataBaseException("ValueToBson bson_append_bool", arr_key, std::get<bool>(arr_value));
                }
            }
            else if (arr_value_index == DataBase::STRING_VALUE) {
                if (!bson_append_utf8(&bson_arr, arr_key.c_str(), static_cast<int>(arr_key.length()), std::get<string>(arr_value).c_str(), static_cast<int>(std::get<string>(arr_value).length()))) {
                    throw DataBaseException("ValueToBson bson_append_utf8", arr_key, std::get<string>(arr_value));
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
    else if (value_index == DataBase::DICT_VALUE) {
        bson_t bson_doc;
        if (!bson_append_document_begin(bson, key.c_str(), static_cast<int>(key.length()), &bson_doc)) {
            throw DataBaseException("ValueToBson bson_append_bool", key);
        }

        const auto& dict = std::get<DataBase::Dict>(value);
        for (const auto& [fst, snd] : dict) {
            const auto dict_value_index = snd.index();
            if (dict_value_index == DataBase::INT_VALUE) {
                if (!bson_append_int32(&bson_doc, fst.c_str(), static_cast<int>(fst.length()), std::get<int>(snd))) {
                    throw DataBaseException("ValueToBson bson_append_int32", fst, std::get<int>(snd));
                }
            }
            else if (dict_value_index == DataBase::INT64_VALUE) {
                if (!bson_append_int64(&bson_doc, fst.c_str(), static_cast<int>(fst.length()), std::get<int64>(snd))) {
                    throw DataBaseException("ValueToBson bson_append_int64", fst, std::get<int64>(snd));
                }
            }
            else if (dict_value_index == DataBase::DOUBLE_VALUE) {
                if (!bson_append_double(&bson_doc, fst.c_str(), static_cast<int>(fst.length()), std::get<double>(snd))) {
                    throw DataBaseException("ValueToBson bson_append_double", fst, std::get<double>(snd));
                }
            }
            else if (dict_value_index == DataBase::BOOL_VALUE) {
                if (!bson_append_bool(&bson_doc, fst.c_str(), static_cast<int>(fst.length()), std::get<bool>(snd))) {
                    throw DataBaseException("ValueToBson bson_append_bool", fst, std::get<bool>(snd));
                }
            }
            else if (dict_value_index == DataBase::STRING_VALUE) {
                if (!bson_append_utf8(&bson_doc, fst.c_str(), static_cast<int>(fst.length()), std::get<string>(snd).c_str(), static_cast<int>(std::get<string>(snd).length()))) {
                    throw DataBaseException("ValueToBson bson_append_utf8", fst, std::get<string>(snd));
                }
            }
            else if (dict_value_index == DataBase::ARRAY_VALUE) {
                bson_t bson_arr;
                if (!bson_append_array_begin(&bson_doc, fst.c_str(), static_cast<int>(fst.length()), &bson_arr)) {
                    throw DataBaseException("ValueToBson bson_append_array_begin", fst);
                }

                const auto& arr = std::get<DataBase::Array>(snd);
                auto arr_key_index = 0;
                for (const auto& arr_value : arr) {
                    string arr_key = _str("{}", arr_key_index);
                    arr_key_index++;

                    const auto arr_value_index = arr_value.index();
                    if (arr_value_index == DataBase::INT_VALUE) {
                        if (!bson_append_int32(&bson_arr, arr_key.c_str(), static_cast<int>(arr_key.length()), std::get<int>(arr_value))) {
                            throw DataBaseException("ValueToBson bson_append_int32", arr_key, std::get<int>(arr_value));
                        }
                    }
                    else if (arr_value_index == DataBase::INT64_VALUE) {
                        if (!bson_append_int64(&bson_arr, arr_key.c_str(), static_cast<int>(arr_key.length()), std::get<int64>(arr_value))) {
                            throw DataBaseException("ValueToBson bson_append_int64", arr_key, std::get<int64>(arr_value));
                        }
                    }
                    else if (arr_value_index == DataBase::DOUBLE_VALUE) {
                        if (!bson_append_double(&bson_arr, arr_key.c_str(), static_cast<int>(arr_key.length()), std::get<double>(arr_value))) {
                            throw DataBaseException("ValueToBson bson_append_double", arr_key, std::get<double>(arr_value));
                        }
                    }
                    else if (arr_value_index == DataBase::BOOL_VALUE) {
                        if (!bson_append_bool(&bson_arr, arr_key.c_str(), static_cast<int>(arr_key.length()), std::get<bool>(arr_value))) {
                            throw DataBaseException("ValueToBson bson_append_bool", arr_key, std::get<bool>(arr_value));
                        }
                    }
                    else if (arr_value_index == DataBase::STRING_VALUE) {
                        if (!bson_append_utf8(&bson_arr, arr_key.c_str(), static_cast<int>(arr_key.length()), std::get<string>(arr_value).c_str(), static_cast<int>(std::get<string>(arr_value).length()))) {
                            throw DataBaseException("ValueToBson bson_append_utf8", arr_key, std::get<string>(arr_value));
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

static void DocumentToBson(const DataBase::Document& doc, bson_t* bson)
{
    for (const auto& [fst, snd] : doc) {
        ValueToBson(fst, snd, bson);
    }
}

static auto BsonToValue(bson_iter_t* iter) -> DataBase::Value
{
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

        DataBase::Array arr;
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

        DataBase::Dict dict;
        while (bson_iter_next(&doc_iter)) {
            const auto* key = bson_iter_key(&doc_iter);
            const auto* dict_value = bson_iter_value(&doc_iter);
            if (dict_value->value_type == BSON_TYPE_INT32) {
                dict.insert(std::make_pair(string(key), dict_value->value.v_int32));
            }
            else if (dict_value->value_type == BSON_TYPE_INT64) {
                dict.insert(std::make_pair(string(key), dict_value->value.v_int64));
            }
            else if (dict_value->value_type == BSON_TYPE_DOUBLE) {
                dict.insert(std::make_pair(string(key), dict_value->value.v_double));
            }
            else if (dict_value->value_type == BSON_TYPE_BOOL) {
                dict.insert(std::make_pair(string(key), dict_value->value.v_bool));
            }
            else if (dict_value->value_type == BSON_TYPE_UTF8) {
                dict.insert(std::make_pair(string(key), string(dict_value->value.v_utf8.str, dict_value->value.v_utf8.len)));
            }
            else if (dict_value->value_type == BSON_TYPE_ARRAY) {
                bson_iter_t doc_arr_iter;
                if (!bson_iter_recurse(&doc_iter, &doc_arr_iter)) {
                    throw DataBaseException("BsonToValue bson_iter_recurse");
                }

                DataBase::Array dict_array;
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

                dict.insert(std::make_pair(string(key), dict_array));
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

static void BsonToDocument(const bson_t* bson, DataBase::Document& doc)
{
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
        doc.insert(std::make_pair(string(key), std::move(value)));
    }
}

auto DataBase::Get(const string& collection_name, uint id) -> Document
{
    if (_deletedRecords[collection_name].count(id) != 0u) {
        return Document();
    }

    if (_newRecords[collection_name].count(id) != 0u) {
        return _recordChanges[collection_name][id];
    }

    auto doc = GetRecord(collection_name, id);

    if (_recordChanges[collection_name].count(id) != 0u) {
        for (auto& [fst, snd] : _recordChanges[collection_name][id]) {
            doc[fst] = snd;
        }
    }

    return doc;
}

void DataBase::StartChanges()
{
    RUNTIME_ASSERT(!_changesStarted);
    RUNTIME_ASSERT(_recordChanges.empty());
    RUNTIME_ASSERT(_newRecords.empty());
    RUNTIME_ASSERT(_deletedRecords.empty());

    _changesStarted = true;
}

void DataBase::Insert(const string& collection_name, uint id, const Document& doc)
{
    RUNTIME_ASSERT(_changesStarted);
    RUNTIME_ASSERT(!_newRecords[collection_name].count(id));
    RUNTIME_ASSERT(!_deletedRecords[collection_name].count(id));

    _newRecords[collection_name].insert(id);
    for (const auto& [fst, snd] : doc) {
        _recordChanges[collection_name][id][fst] = snd;
    }
}

void DataBase::Update(const string& collection_name, uint id, const string& key, const Value& value)
{
    RUNTIME_ASSERT(_changesStarted);
    RUNTIME_ASSERT(!_deletedRecords[collection_name].count(id));

    _recordChanges[collection_name][id][key] = value;
}

void DataBase::Delete(const string& collection_name, uint id)
{
    RUNTIME_ASSERT(_changesStarted);
    RUNTIME_ASSERT(!_deletedRecords[collection_name].count(id));

    _deletedRecords[collection_name].insert(id);
    _newRecords[collection_name].erase(id);
    _recordChanges[collection_name].erase(id);
}

void DataBase::CommitChanges()
{
    RUNTIME_ASSERT(_changesStarted);

    _changesStarted = false;

    for (auto& [fst, snd] : _recordChanges) {
        for (auto& [fst2, snd2] : snd) {
            auto it = _newRecords.find(fst);
            if (it != _newRecords.end() && it->second.count(fst2) != 0u) {
                InsertRecord(fst, fst2, snd2);
            }
            else {
                UpdateRecord(fst, fst2, snd2);
            }
        }
    }

    for (auto& [fst, snd] : _deletedRecords) {
        for (const auto& id : snd) {
            DeleteRecord(fst, id);
        }
    }

    _recordChanges.clear();
    _newRecords.clear();
    _deletedRecords.clear();

    CommitRecords();
}

#ifdef FO_HAVE_JSON
class DbJson final : public DataBase
{
public:
    DbJson() = delete;
    DbJson(const DbJson&) = delete;
    DbJson(DbJson&&) noexcept = default;
    auto operator=(const DbJson&) = delete;
    auto operator=(DbJson&&) noexcept = delete;
    ~DbJson() override = default;

    explicit DbJson(const string& storage_dir)
    {
        DiskFileSystem::MakeDirTree(storage_dir + "/");

        _storageDir = storage_dir;
    }

    auto GetAllIds(const string& collection_name) -> UIntVec override
    {
        UIntVec ids;
        DiskFileSystem::IterateDir(_storageDir + "/" + collection_name + "/", "json", false, [&ids](const string& path, uint /*size*/, uint64 /*write_time*/) {
            const string id_str = _str(path).extractFileName().eraseFileExtension();
            const auto id = _str(id_str).toUInt();
            if (id == 0) {
                throw DataBaseException("DbJson Id is zero", path);
            }

            ids.push_back(id);
        });
        return ids;
    }

protected:
    auto GetRecord(const string& collection_name, uint id) -> Document override
    {
        const string path = _str("{}/{}/{}.json", _storageDir, collection_name, id);

        uint length;
        char* json;
        if (auto f = DiskFileSystem::OpenFile(path, false)) {
            length = f.GetSize();
            json = new char[length];
            if (!f.Read(json, length)) {
                throw DataBaseException("DbJson Can't read file", path);
            }
        }
        else {
            return Document();
        }

        bson_t bson;
        bson_error_t error;
        if (!bson_init_from_json(&bson, json, length, &error)) {
            throw DataBaseException("DbJson bson_init_from_json", path);
        }

        delete[] json;

        Document doc;
        BsonToDocument(&bson, doc);

        bson_destroy(&bson);
        return doc;
    }

    void InsertRecord(const string& collection_name, uint id, const Document& doc) override
    {
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
            if (!f.Write(pretty_json_dump.c_str(), static_cast<uint>(pretty_json_dump.length()))) {
                throw DataBaseException("DbJson Can't write file", path);
            }
        }
        else {
            throw DataBaseException("DbJson Can't open file", path);
        }
    }

    void UpdateRecord(const string& collection_name, uint id, const Document& doc) override
    {
        RUNTIME_ASSERT(!doc.empty());

        const string path = _str("{}/{}/{}.json", _storageDir, collection_name, id);

        uint length;
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
        if (!bson_init_from_json(&bson, json, length, &error)) {
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
            if (!f_write.Write(pretty_json_dump.c_str(), static_cast<uint>(pretty_json_dump.length()))) {
                throw DataBaseException("DbJson Can't write file", path);
            }
        }
        else {
            throw DataBaseException("DbJson Can't open file for writing", path);
        }
    }

    void DeleteRecord(const string& collection_name, uint id) override
    {
        const string path = _str("{}/{}/{}.json", _storageDir, collection_name, id);
        if (!DiskFileSystem::DeleteFile(path)) {
            throw DataBaseException("DbJson Can't delete file", path);
        }
    }

    void CommitRecords() override
    {
        // Nothing
    }

private:
    string _storageDir {};
};
#endif

#ifdef FO_HAVE_UNQLITE
class DbUnQLite final : public DataBase
{
public:
    DbUnQLite() = delete;
    DbUnQLite(const DbUnQLite&) = delete;
    DbUnQLite(DbUnQLite&&) noexcept = default;
    auto operator=(const DbUnQLite&) = delete;
    auto operator=(DbUnQLite&&) noexcept = delete;

    explicit DbUnQLite(const string& storage_dir)
    {
        DiskFileSystem::MakeDirTree(storage_dir + "/");

        unqlite* ping_db = nullptr;
        const auto ping_db_path = storage_dir + "/Ping.unqlite";
        if (unqlite_open(&ping_db, ping_db_path.c_str(), UNQLITE_OPEN_CREATE | UNQLITE_OPEN_OMIT_JOURNALING) != UNQLITE_OK) {
            throw DataBaseException("DbUnQLite Can't open db", ping_db_path);
        }

        const auto ping = 42u;
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
        for (auto& [fst, snd] : _collections) {
            unqlite_close(snd);
        }
    }

    auto GetAllIds(const string& collection_name) -> UIntVec override
    {
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

        UIntVec ids;
        while (unqlite_kv_cursor_valid_entry(cursor) != 0) {
            uint id = 0u;
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

            ids.push_back(id);

            const auto kv_cursor_next_entry = unqlite_kv_cursor_next_entry(cursor);
            if (kv_cursor_next_entry != UNQLITE_OK && kv_cursor_next_entry != UNQLITE_DONE) {
                throw DataBaseException("DbUnQLite kv_cursor_next_entry", kv_cursor_next_entry);
            }
        }

        return ids;
    }

protected:
    auto GetRecord(const string& collection_name, uint id) -> Document override
    {
        auto* db = GetCollection(collection_name);
        if (db == nullptr) {
            throw DataBaseException("DbUnQLite Can't open collection", collection_name);
        }

        Document doc;
        const auto kv_fetch_callback = unqlite_kv_fetch_callback(
            db, &id, sizeof(id),
            [](const void* output, unsigned int output_len, void* user_data) {
                bson_t bson;
                if (!bson_init_static(&bson, static_cast<const uint8_t*>(output), output_len)) {
                    throw DataBaseException("DbUnQLite bson_init_static");
                }

                auto& doc2 = *static_cast<Document*>(user_data);
                BsonToDocument(&bson, doc2);

                return UNQLITE_OK;
            },
            &doc);

        if (kv_fetch_callback != UNQLITE_OK && kv_fetch_callback != UNQLITE_NOTFOUND) {
            throw DataBaseException("DbUnQLite unqlite_kv_fetch_callback", kv_fetch_callback);
        }

        return doc;
    }

    void InsertRecord(const string& collection_name, uint id, const Document& doc) override
    {
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

    void UpdateRecord(const string& collection_name, uint id, const Document& doc) override
    {
        RUNTIME_ASSERT(!doc.empty());

        auto* db = GetCollection(collection_name);
        if (db == nullptr) {
            throw DataBaseException("DbUnQLite Can't open collection", collection_name);
        }

        auto actual_doc = Get(collection_name, id);
        if (actual_doc.empty()) {
            throw DataBaseException("DbUnQLite Document not found", collection_name, id);
        }

        for (const auto& [fst, snd] : doc) {
            actual_doc[fst] = snd;
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

    void DeleteRecord(const string& collection_name, uint id) override
    {
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
        for (const auto& [fst, snd] : _collections) {
            const auto commit = unqlite_commit(snd);
            if (commit != UNQLITE_OK) {
                throw DataBaseException("DbUnQLite unqlite_commit", commit);
            }
        }
    }

private:
    auto GetCollection(const string& collection_name) -> unqlite*
    {
        unqlite* db = nullptr;
        const auto it = _collections.find(collection_name);
        if (it == _collections.end()) {
            const string db_path = _str("{}/{}.unqlite", _storageDir, collection_name);
            const auto r = unqlite_open(&db, db_path.c_str(), UNQLITE_OPEN_CREATE | UNQLITE_OPEN_OMIT_JOURNALING);
            if (r != UNQLITE_OK) {
                throw DataBaseException("DbUnQLite Can't open db", collection_name, r);
            }

            _collections.insert(std::make_pair(collection_name, db));
        }
        else {
            db = it->second;
        }
        return db;
    }

    string _storageDir {};
    map<string, unqlite*> _collections {};
};
#endif

#ifdef FO_HAVE_MONGO
class DbMongo final : public DataBase
{
public:
    DbMongo() = delete;
    DbMongo(const DbMongo&) = delete;
    DbMongo(DbMongo&&) noexcept = default;
    auto operator=(const DbMongo&) = delete;
    auto operator=(DbMongo&&) noexcept = delete;

    DbMongo(const string& uri, const string& db_name)
    {
        mongoc_init();

        bson_error_t error;
        auto* mongo_uri = mongoc_uri_new_with_error(uri.c_str(), &error);
        if (mongo_uri == nullptr) {
            throw DataBaseException("DbMongo Failed to parse URI", uri, error.message);
        }

        auto* client = mongoc_client_new_from_uri(mongo_uri);
        if (client == nullptr) {
            throw DataBaseException("DbMongo Can't create client");
        }

        mongoc_uri_destroy(mongo_uri);
        mongoc_client_set_appname(client, "fonline");

        auto* database = mongoc_client_get_database(client, db_name.c_str());
        if (database == nullptr) {
            throw DataBaseException("DbMongo Can't get database", db_name);
        }

        // Ping
        auto* ping = BCON_NEW("ping", BCON_INT32(1));
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
        for (auto& [fst, snd] : _collections) {
            mongoc_collection_destroy(snd);
        }

        mongoc_database_destroy(_database);
        mongoc_client_destroy(_client);
        mongoc_cleanup();
    }

    auto GetAllIds(const string& collection_name) -> UIntVec override
    {
        auto* collection = GetCollection(collection_name);
        if (collection == nullptr) {
            throw DataBaseException("DbMongo Can't get collection", collection_name);
        }

        bson_t fields;
        bson_init(&fields);

        if (!bson_append_int32(&fields, "_id", 3, 1)) {
            throw DataBaseException("DbMongo bson_append_int32", collection_name);
        }

        bson_t query;
        bson_init(&query);

        auto* cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE, 0, 0, static_cast<uint>(-1), &query, &fields, nullptr);
        if (cursor == nullptr) {
            throw DataBaseException("DbMongo mongoc_collection_find", collection_name);
        }

        UIntVec ids;
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
            ids.push_back(id);
        }

        bson_error_t error;
        if (mongoc_cursor_error(cursor, &error)) {
            throw DataBaseException("DbMongo mongoc_cursor_error", collection_name, error.message);
        }

        mongoc_cursor_destroy(cursor);
        bson_destroy(&query);
        bson_destroy(&fields);
        return ids;
    }

protected:
    auto GetRecord(const string& collection_name, uint id) -> Document override
    {
        auto* collection = GetCollection(collection_name);
        if (collection == nullptr) {
            throw DataBaseException("DbMongo Can't get collection", collection_name);
        }

        bson_t query;
        bson_init(&query);

        if (!bson_append_int32(&query, "_id", 3, id)) {
            throw DataBaseException("DbMongo bson_append_int32", collection_name, id);
        }

        auto* cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE, 0, 1, 0, &query, nullptr, nullptr);
        if (cursor == nullptr) {
            throw DataBaseException("DbMongo mongoc_collection_find", collection_name, id);
        }

        const bson_t* bson = nullptr;
        if (!mongoc_cursor_next(cursor, &bson)) {
            mongoc_cursor_destroy(cursor);
            bson_destroy(&query);
            return Document();
        }

        Document doc;
        BsonToDocument(bson, doc);

        mongoc_cursor_destroy(cursor);
        bson_destroy(&query);
        return doc;
    }

    void InsertRecord(const string& collection_name, uint id, const Document& doc) override
    {
        RUNTIME_ASSERT(!doc.empty());

        auto* collection = GetCollection(collection_name);
        if (collection == nullptr) {
            throw DataBaseException("DbMongo Can't get collection", collection_name);
        }

        bson_t insert;
        bson_init(&insert);

        if (!bson_append_int32(&insert, "_id", 3, id)) {
            throw DataBaseException("DbMongo bson_append_int32", collection_name, id);
        }

        DocumentToBson(doc, &insert);

        bson_error_t error;
        if (!mongoc_collection_insert(collection, MONGOC_INSERT_NONE, &insert, nullptr, &error)) {
            throw DataBaseException("DbMongo mongoc_collection_insert", collection_name, id, error.message);
        }

        bson_destroy(&insert);
    }

    void UpdateRecord(const string& collection_name, uint id, const Document& doc) override
    {
        RUNTIME_ASSERT(!doc.empty());

        auto* collection = GetCollection(collection_name);
        if (collection == nullptr) {
            throw DataBaseException("DbMongo Can't get collection", collection_name);
        }

        bson_t selector;
        bson_init(&selector);

        if (!bson_append_int32(&selector, "_id", 3, id)) {
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

    void DeleteRecord(const string& collection_name, uint id) override
    {
        auto* collection = GetCollection(collection_name);
        if (collection == nullptr) {
            throw DataBaseException("DbMongo Can't get collection", collection_name);
        }

        bson_t selector;
        bson_init(&selector);

        if (!bson_append_int32(&selector, "_id", 3, id)) {
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
        // Nothing
    }

private:
    auto GetCollection(const string& collection_name) -> mongoc_collection_t*
    {
        mongoc_collection_t* collection = nullptr;
        const auto it = _collections.find(collection_name);
        if (it == _collections.end()) {
            collection = mongoc_client_get_collection(_client, _databaseName.c_str(), collection_name.c_str());
            if (collection == nullptr) {
                throw DataBaseException("DbMongo Can't get collection", collection_name);
            }

            _collections.insert(std::make_pair(collection_name, collection));
        }
        else {
            collection = it->second;
        }
        return collection;
    }

    mongoc_client_t* _client {};
    mongoc_database_t* _database {};
    string _databaseName {};
    map<string, mongoc_collection_t*> _collections {};
};
#endif

class DbMemory final : public DataBase
{
public:
    DbMemory() = default;
    DbMemory(const DbMemory&) = delete;
    DbMemory(DbMemory&&) noexcept = default;
    auto operator=(const DbMemory&) = delete;
    auto operator=(DbMemory&&) noexcept = delete;
    ~DbMemory() override = default;

    auto GetAllIds(const string& collection_name) -> UIntVec override
    {
        auto& collection = _collections[collection_name];

        UIntVec ids;
        ids.reserve(collection.size());
        for (auto& [fst, snd] : collection) {
            ids.push_back(fst);
        }

        return ids;
    }

protected:
    auto GetRecord(const string& collection_name, uint id) -> Document override
    {
        auto& collection = _collections[collection_name];

        const auto it = collection.find(id);
        return it != collection.end() ? it->second : Document();
    }

    void InsertRecord(const string& collection_name, uint id, const Document& doc) override
    {
        RUNTIME_ASSERT(!doc.empty());

        auto& collection = _collections[collection_name];
        RUNTIME_ASSERT(!collection.count(id));

        collection.insert(std::make_pair(id, doc));
    }

    void UpdateRecord(const string& collection_name, uint id, const Document& doc) override
    {
        RUNTIME_ASSERT(!doc.empty());

        auto& collection = _collections[collection_name];

        auto it = collection.find(id);
        RUNTIME_ASSERT(it != collection.end());

        for (const auto& [fst, snd] : doc) {
            it->second[fst] = snd;
        }
    }

    void DeleteRecord(const string& collection_name, uint id) override
    {
        auto& collection = _collections[collection_name];

        const auto it = collection.find(id);
        RUNTIME_ASSERT(it != collection.end());

        collection.erase(it);
    }

    void CommitRecords() override
    {
        // Nothing
    }

private:
    Collections _collections {};
};

auto GetDataBase(const string& connection_info) -> DataBase*
{
    auto options = _str(connection_info).split(' ');
    if (!options.empty()) {
#ifdef FO_HAVE_JSON
        if (options[0] == "JSON" && options.size() == 2) {
            return new DbJson(options[1]);
        }
#endif
#ifdef FO_HAVE_UNQLITE
        if (options[0] == "DbUnQLite" && options.size() == 2) {
            return new DbUnQLite(options[1]);
        }
#endif
#ifdef FO_HAVE_MONGO
        if (options[0] == "Mongo" && options.size() == 3) {
            return new DbMongo(options[1], options[2]);
        }
#endif
        if (options[0] == "Memory" && options.size() == 1) {
            return new DbMemory();
        }
    }

    throw DataBaseException("Wrong storage options", connection_info);
}
