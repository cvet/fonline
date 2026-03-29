#include "DataBase.h"

#include "ImGuiStuff.h"

FO_BEGIN_NAMESPACE

class DbMemory final : public DataBaseImpl
{
public:
    explicit DbMemory(DataBaseSettings& db_settings, DataBasePanicCallback panic_callback) :
        DataBaseImpl(db_settings, std::move(panic_callback))
    {
        StartCommitThread();
    }

    DbMemory(const DbMemory&) = delete;
    DbMemory(DbMemory&&) noexcept = delete;
    auto operator=(const DbMemory&) = delete;
    auto operator=(DbMemory&&) noexcept = delete;
    ~DbMemory() override { StopCommitThread(); }

    [[nodiscard]] auto GetAllRecordIds(hstring collection_name) const -> vector<ident_t> override
    {
        FO_STACK_TRACE_ENTRY();

        std::scoped_lock locker {_collectionsLocker};

        if (_collections.count(collection_name) == 0) {
            return {};
        }

        const auto& collection = _collections.at(collection_name);

        vector<ident_t> ids;
        ids.reserve(collection.size());

        for (const auto key : collection | std::views::keys) {
            ids.emplace_back(key);
        }

        return ids;
    }

protected:
    [[nodiscard]] auto GetRecord(hstring collection_name, ident_t id) const -> AnyData::Document override
    {
        FO_STACK_TRACE_ENTRY();

        std::scoped_lock locker {_collectionsLocker};

        if (_collections.count(collection_name) == 0) {
            return {};
        }

        const auto& collection = _collections.at(collection_name);

        const auto it = collection.find(id);
        return it != collection.end() ? it->second.Copy() : AnyData::Document();
    }

    void InsertRecord(hstring collection_name, ident_t id, const AnyData::Document& doc) override
    {
        FO_STACK_TRACE_ENTRY();

        FO_RUNTIME_ASSERT(!doc.Empty());

        std::scoped_lock locker {_collectionsLocker};

        auto& collection = _collections[collection_name];
        FO_RUNTIME_ASSERT(!collection.count(id));

        collection.emplace(id, doc.Copy());
    }

    void UpdateRecord(hstring collection_name, ident_t id, const AnyData::Document& doc) override
    {
        FO_STACK_TRACE_ENTRY();

        FO_RUNTIME_ASSERT(!doc.Empty());

        std::scoped_lock locker {_collectionsLocker};

        auto& collection = _collections[collection_name];

        const auto it_collection = collection.find(id);
        FO_RUNTIME_ASSERT(it_collection != collection.end());

        for (auto&& [doc_key, doc_value] : doc) {
            it_collection->second.Assign(doc_key, doc_value.Copy());
        }
    }

    void DeleteRecord(hstring collection_name, ident_t id) override
    {
        FO_STACK_TRACE_ENTRY();

        std::scoped_lock locker {_collectionsLocker};

        auto& collection = _collections[collection_name];

        const auto it = collection.find(id);
        FO_RUNTIME_ASSERT(it != collection.end());

        collection.erase(it);
    }

    void DrawGui() override
    {
        FO_STACK_TRACE_ENTRY();

        std::scoped_lock locker {_collectionsLocker};

        if (_collections.empty()) {
            ImGui::TextUnformatted("No memory collections");
            return;
        }

        for (auto&& [collection_name, collection] : _collections) {
            if (ImGui::TreeNode(strex("{} ({})", collection_name.as_str(), collection.size()).c_str())) {
                for (auto&& [id, doc] : collection) {
                    if (ImGui::TreeNode(strex("{} ({} keys)", id, doc.Size()).c_str())) {
                        for (auto&& [doc_key, doc_value] : doc) {
                            const auto value_str = AnyData::ValueToString(doc_value);
                            ImGui::BulletText("%s: %s", doc_key.c_str(), value_str.c_str());
                        }

                        ImGui::TreePop();
                    }
                }

                ImGui::TreePop();
            }
        }
    }

private:
    mutable std::mutex _collectionsLocker {};
    DataBase::Collections _collections {};
};

auto CreateMemoryDataBase(DataBaseSettings& db_settings, DataBasePanicCallback panic_callback) -> DataBaseImpl*
{
    return SafeAlloc::MakeRaw<DbMemory>(db_settings, std::move(panic_callback));
}

FO_END_NAMESPACE
