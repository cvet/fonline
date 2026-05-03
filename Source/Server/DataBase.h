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

#pragma once

#include "Common.h"

#include "AnyData.h"
#include "Settings.h"

// ReSharper disable once CppInconsistentNaming
struct _bson_t;
using bson_t = _bson_t;

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(DataBaseException);

enum class DataBaseKeyType
{
    IntId,
    String,
};

enum class DataBaseStringKeyEscaping
{
    Raw,
    File,
    Hex,
};

using DataBasePanicCallback = function<void()>;
using DataBaseKey = variant<ident_t, string>;
using DataBaseCollection = unordered_map<DataBaseKey, AnyData::Document>;
using DataBaseCollections = unordered_map<hstring, DataBaseCollection>;
using DataBaseCollectionSchema = pair<hstring, DataBaseKeyType>;
using DataBaseCollectionSchemas = vector<DataBaseCollectionSchema>;

FO_DECLARE_TYPE_FORMATTER_EXT(FO_NAMESPACE DataBaseKey,
    std::visit(
        [](const auto& raw_value) -> FO_NAMESPACE string {
            using T = std::decay_t<decltype(raw_value)>;
            if constexpr (std::is_same_v<T, FO_NAMESPACE ident_t>) {
                return FO_NAMESPACE strex("{}", raw_value).str();
            }
            else {
                return raw_value;
            }
        },
        value));

FO_DECLARE_TYPE_HASHER_EXT2(FO_NAMESPACE DataBaseKey, std::visit([](const auto& value) -> uint64_t { return FO_NAMESPACE hashing::hash<std::decay_t<decltype(value)>> {}(value); }, v));

class DataBaseImpl;

class DataBase
{
    friend auto ConnectToDataBase(DataBaseSettings& db_settings, string_view connection_info, const DataBaseCollectionSchemas& collection_schemas, DataBasePanicCallback panic_callback) -> DataBase;

public:
    using Collection = DataBaseCollection;
    using Collections = DataBaseCollections;

    DataBase();
    DataBase(const DataBase&) = delete;
    DataBase(DataBase&&) noexcept;
    auto operator=(const DataBase&) = delete;
    auto operator=(DataBase&&) noexcept -> DataBase&;
    ~DataBase();

    [[nodiscard]] auto InValidState() const noexcept -> bool;
    [[nodiscard]] auto GetDbRequestsPerMinute() const -> size_t;
    [[nodiscard]] auto GetAllIds(hstring collection_name) const -> vector<DataBaseKey>;
    [[nodiscard]] auto GetAllIntIds(hstring collection_name) const -> vector<ident_t>;
    [[nodiscard]] auto GetAllStringIds(hstring collection_name) const -> vector<string>;
    [[nodiscard]] auto Get(hstring collection_name, const DataBaseKey& id) const -> AnyData::Document;
    [[nodiscard]] auto Valid(hstring collection_name, const DataBaseKey& id) const -> bool;

    void Insert(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc);
    void Update(hstring collection_name, const DataBaseKey& id, string_view key, const AnyData::Value& value);
    void Delete(hstring collection_name, const DataBaseKey& id);
    void StartCommitChanges();
    void WaitCommitChanges();
    void ClearChanges() noexcept;
    void DrawGui();

private:
    explicit DataBase(DataBaseImpl* impl);

    unique_ptr<DataBaseImpl> _impl;
};

extern auto ConnectToDataBase(DataBaseSettings& db_settings, string_view connection_info, const DataBaseCollectionSchemas& collection_schemas, DataBasePanicCallback panic_callback) -> DataBase;

class DataBaseImpl
{
public:
    class RecoveryLogHandle
    {
    public:
        using FdType = int;

        explicit RecoveryLogHandle(string path);
        RecoveryLogHandle(const RecoveryLogHandle&) = delete;
        RecoveryLogHandle(RecoveryLogHandle&&) noexcept = delete;
        auto operator=(const RecoveryLogHandle&) = delete;
        auto operator=(RecoveryLogHandle&&) noexcept = delete;
        ~RecoveryLogHandle() noexcept;

        [[nodiscard]] auto GetPath() const noexcept -> string_view { return _path; }
        [[nodiscard]] auto GetLinesCount() const noexcept -> size_t { return _content.size(); }
        [[nodiscard]] auto GetTextSize() const noexcept -> size_t { return _textSize; }
        [[nodiscard]] auto GetContent() const noexcept -> const vector<string>& { return _content; }

        auto Truncate() noexcept -> bool;
        auto Append(string_view text) noexcept -> bool;

    private:
        auto Read() noexcept -> optional<string>;

        string _path;
        FdType _fd {-1};
        size_t _textSize {};
        vector<string> _content {};
    };

    explicit DataBaseImpl(DataBaseSettings& db_settings, DataBasePanicCallback panic_callback);
    DataBaseImpl(const DataBaseImpl&) = delete;
    DataBaseImpl(DataBaseImpl&&) noexcept = delete;
    auto operator=(const DataBaseImpl&) = delete;
    auto operator=(DataBaseImpl&&) noexcept = delete;
    virtual ~DataBaseImpl() = default;

    [[nodiscard]] auto InValidState() const noexcept -> bool;
    [[nodiscard]] auto GetDbRequestsPerMinute() const -> size_t;
    [[nodiscard]] auto GetCollectionKeyType(hstring collection_name) const -> DataBaseKeyType;
    [[nodiscard]] virtual auto GetStringKeyEscaping() const noexcept -> DataBaseStringKeyEscaping = 0;
    [[nodiscard]] virtual auto GetAllRecordIds(hstring collection_name) const -> vector<DataBaseKey> = 0;
    [[nodiscard]] auto GetDocument(hstring collection_name, const DataBaseKey& id) const -> AnyData::Document;

    void InitializeCollections(const DataBaseCollectionSchemas& collection_schemas);
    void InitializeOpLogs();
    void RestorePendingChanges();
    void Insert(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc);
    void Update(hstring collection_name, const DataBaseKey& id, string_view key, const AnyData::Value& value);
    void Delete(hstring collection_name, const DataBaseKey& id);
    void StartCommitChanges();
    void WaitCommitChanges();
    void ClearChanges() noexcept;
    virtual void DrawGui();

protected:
    void StartCommitThread();
    void StopCommitThread() noexcept;

    virtual void EnsureCollection(hstring collection_name, DataBaseKeyType key_type) = 0;
    virtual auto GetRecord(hstring collection_name, const DataBaseKey& id) const -> AnyData::Document = 0;
    virtual void InsertRecord(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc) = 0;
    virtual void UpdateRecord(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc) = 0;
    virtual void DeleteRecord(hstring collection_name, const DataBaseKey& id) = 0;
    virtual auto TryReconnect() -> bool { return true; }

    virtual void OnCommitOperationWrittenToOpLog() { } // Testing override point for a failed commit operation being durably written to oplog
    virtual void OnPendingChangesRestored() { } // Testing override point for successful pending oplog restore

private:
    struct DbRequestsPerMinuteBucket
    {
        int64_t Second {};
        size_t Count {};
    };

    enum class CommitOperationType
    {
        Insert,
        Update,
        Delete,
    };

    struct CommitOperationData
    {
        CommitOperationType Type {};
        hstring CollectionName {};
        DataBaseKey RecordId {};
        AnyData::Document Doc {};
    };

    void ScheduleCommit();
    void CommitNextChange() noexcept;
    void CommitThreadEntry() noexcept;
    void RegisterDbRequests(size_t request_count) const;
    auto ResolveCollectionName(string_view collection_name) const -> hstring;
    void ValidateCollectionKey(hstring collection_name, const DataBaseKey& id) const;
    void StartPanic(string_view message);

    raw_ptr<DataBaseSettings> _settings;
    mutable std::mutex _stateLocker {};
    std::thread _commitThread {};
    std::condition_variable _commitThreadSignal {};
    std::condition_variable _commitThreadDoneSignal {};
    bool _commitThreadStopRequested {};
    bool _commitThreadActive {};
    deque<shared_ptr<CommitOperationData>> _pendingCommitOperations {};
    mutable unordered_set<pair<hstring, DataBaseKey>> _docReadRetryMarkers {};
    mutable std::atomic_bool _backendFailed {};
    std::atomic_bool _panicStarted {};
    nanotime _panicRequestedTime {};
    bool _opLogEnabled {};
    unique_ptr<RecoveryLogHandle> _pendingChangesLog {};
    unique_ptr<RecoveryLogHandle> _committedChangesLog {};
    size_t _pendingChangesPanicThreshold {};
    timespan _panicShutdownTimeout {};
    timespan _reconnectRetryPeriod {};
    nanotime _reconnectRetryTime {};
    DataBasePanicCallback _panicCallback {};
    mutable std::mutex _dbRequestsMetricLocker {};
    mutable vector<DbRequestsPerMinuteBucket> _dbRequestsPerMinuteBuckets {vector<DbRequestsPerMinuteBucket>(60)};
    mutable std::atomic_size_t _dbRequestsPerMinute {};
    unordered_map<string, hstring> _collectionNames {};
    unordered_map<hstring, DataBaseKeyType> _collectionKeyTypes {};
};

auto CreateJsonDataBase(DataBaseSettings& db_settings, string_view storage_dir, DataBasePanicCallback panic_callback) -> DataBaseImpl*;
#if FO_HAVE_UNQLITE
auto CreateUnQLiteDataBase(DataBaseSettings& db_settings, string_view storage_dir, DataBasePanicCallback panic_callback) -> DataBaseImpl*;
#endif
#if FO_HAVE_MONGO
auto CreateMongoDataBase(DataBaseSettings& db_settings, string_view uri, string_view db_name, DataBasePanicCallback panic_callback) -> DataBaseImpl*;
#endif
auto CreateMemoryDataBase(DataBaseSettings& db_settings, DataBasePanicCallback panic_callback) -> DataBaseImpl*;

void DocumentToBson(const AnyData::Document& doc, bson_t* bson, char escape_dot = 0);
void BsonToDocument(const bson_t* bson, AnyData::Document& doc, char escape_dot = 0);
auto GetDbKeyType(const DataBaseKey& key) noexcept -> DataBaseKeyType;

FO_END_NAMESPACE
