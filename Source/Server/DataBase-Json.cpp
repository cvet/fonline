#include "DataBase.h"

FO_DISABLE_WARNINGS_PUSH()
#include <bson/bson.h>
FO_DISABLE_WARNINGS_POP()

#include <json.hpp>

#include "WinApiUndef-Include.h"

FO_BEGIN_NAMESPACE

class DbJson final : public DataBaseImpl
{
public:
    DbJson(const DbJson&) = delete;
    DbJson(DbJson&&) noexcept = delete;
    auto operator=(const DbJson&) = delete;
    auto operator=(DbJson&&) noexcept = delete;

    explicit DbJson(DataBaseSettings& db_settings, string_view storage_dir, DataBasePanicCallback panic_callback) :
        DataBaseImpl(db_settings, std::move(panic_callback)),
        _storageDir {storage_dir},
        _jsonIndent {db_settings.DataBaseJsonIndent}
    {
        (void)fs_create_directories(storage_dir);
        StartCommitThread();
    }

    ~DbJson() override { StopCommitThread(); }

    [[nodiscard]] auto GetAllRecordIds(hstring collection_name) const -> vector<ident_t> override
    {
        FO_STACK_TRACE_ENTRY();

        std::scoped_lock locker {_storageLocker};

        vector<ident_t> ids;

        std::error_code ec;
        const auto dir_path = std::filesystem::path {fs_make_path(strex(_storageDir).combine_path(collection_name))};
        const auto dir_iterator = std::filesystem::directory_iterator(dir_path, ec);

        if (!ec) {
            for (const auto& dir_entry : dir_iterator) {
                if (dir_entry.is_directory()) {
                    continue;
                }

                const auto path_str = dir_entry.path().filename().u8string();
                const auto path = string(path_str.begin(), path_str.end());

                if (strex(path).get_file_extension() != "json") {
                    continue;
                }

                static_assert(sizeof(ident_t) == sizeof(int64));

                const auto id = strvex(path).extract_file_name().erase_file_extension().to_int64();

                if (id == 0) {
                    throw DataBaseException("DbJson Id is zero", path);
                }

                ids.emplace_back(id);
            }
        }

        return ids;
    }

protected:
    [[nodiscard]] auto GetRecord(hstring collection_name, ident_t id) const -> AnyData::Document override
    {
        FO_STACK_TRACE_ENTRY();

        std::scoped_lock locker {_storageLocker};

        const string path = strex("{}/{}/{}.json", _storageDir, collection_name, id);

        const auto json = fs_read_file(path);

        if (!json) {
            return {};
        }

        bson_t bson;
        bson_error_t error;

        if (!bson_init_from_json(&bson, json->c_str(), numeric_cast<ssize_t>(json->length()), &error)) {
            throw DataBaseException("DbJson bson_init_from_json", path);
        }

        AnyData::Document doc;
        BsonToDocument(&bson, doc);

        bson_destroy(&bson);
        return doc;
    }

    void InsertRecord(hstring collection_name, ident_t id, const AnyData::Document& doc) override
    {
        FO_STACK_TRACE_ENTRY();

        FO_RUNTIME_ASSERT(!doc.Empty());

        std::scoped_lock locker {_storageLocker};

        const string path = strex("{}/{}/{}.json", _storageDir, collection_name, id);

        if (fs_exists(path)) {
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
        const auto pretty_json_dump = pretty_json.dump(_jsonIndent > 0 ? _jsonIndent : -1);
        bson_free(json);

        const auto dir = strex(path).extract_dir().str();

        if (!dir.empty() && !fs_create_directories(dir)) {
            throw DataBaseException("DbJson Can't open file", path);
        }

        if (!fs_write_file(path, pretty_json_dump)) {
            throw DataBaseException("DbJson Can't write file", path);
        }
    }

    void UpdateRecord(hstring collection_name, ident_t id, const AnyData::Document& doc) override
    {
        FO_STACK_TRACE_ENTRY();

        FO_RUNTIME_ASSERT(!doc.Empty());

        std::scoped_lock locker {_storageLocker};

        const string path = strex("{}/{}/{}.json", _storageDir, collection_name, id);

        const auto json = fs_read_file(path);

        if (!json) {
            throw DataBaseException("DbJson Can't open file for reading", path);
        }

        bson_t bson;
        bson_error_t error;

        if (!bson_init_from_json(&bson, json->c_str(), numeric_cast<ssize_t>(json->length()), &error)) {
            throw DataBaseException("DbJson bson_init_from_json", path);
        }

        DocumentToBson(doc, &bson);

        size_t new_length = 0;
        auto* new_json = bson_as_canonical_extended_json(&bson, &new_length);

        if (new_json == nullptr) {
            throw DataBaseException("DbJson bson_as_canonical_extended_json", path);
        }

        bson_destroy(&bson);

        const auto pretty_json = nlohmann::json::parse(new_json);
        const auto pretty_json_dump = pretty_json.dump(_jsonIndent > 0 ? _jsonIndent : -1);
        bson_free(new_json);

        const auto dir = strex(path).extract_dir().str();

        if (!dir.empty() && !fs_create_directories(dir)) {
            throw DataBaseException("DbJson Can't open file for writing", path);
        }

        if (!fs_write_file(path, pretty_json_dump)) {
            throw DataBaseException("DbJson Can't write file", path);
        }
    }

    void DeleteRecord(hstring collection_name, ident_t id) override
    {
        FO_STACK_TRACE_ENTRY();

        std::scoped_lock locker {_storageLocker};

        const string path = strex("{}/{}/{}.json", _storageDir, collection_name, id);

        if (!fs_remove_file(path)) {
            throw DataBaseException("DbJson Can't delete file", path);
        }
    }

private:
    mutable std::mutex _storageLocker {};
    string _storageDir {};
    int32 _jsonIndent {};
};

auto CreateJsonDataBase(DataBaseSettings& db_settings, string_view storage_dir, DataBasePanicCallback panic_callback) -> DataBaseImpl*
{
    return SafeAlloc::MakeRaw<DbJson>(db_settings, storage_dir, std::move(panic_callback));
}

FO_END_NAMESPACE
