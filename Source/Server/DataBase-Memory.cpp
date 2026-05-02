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

protected:
    [[nodiscard]] auto GetStringKeyEscaping() const noexcept -> DataBaseStringKeyEscaping override { return DataBaseStringKeyEscaping::Raw; }

    void EnsureCollection(hstring collection_name, DataBaseKeyType key_type) override
    {
        ignore_unused(key_type);

        std::scoped_lock locker {_storageLocker};

        _collections.try_emplace(collection_name);
    }

    [[nodiscard]] auto GetAllRecordIds(hstring collection_name) const -> vector<DataBaseKey> override
    {
        FO_STACK_TRACE_ENTRY();

        std::scoped_lock locker {_storageLocker};

        const auto& collection = _collections.at(collection_name);

        vector<DataBaseKey> ids;
        ids.reserve(collection.size());

        for (const auto key : collection | std::views::keys) {
            ids.emplace_back(key);
        }

        return ids;
    }

protected:
    [[nodiscard]] auto GetRecord(hstring collection_name, const DataBaseKey& id) const -> AnyData::Document override
    {
        FO_STACK_TRACE_ENTRY();

        std::scoped_lock locker {_storageLocker};

        const auto& collection = _collections.at(collection_name);

        const auto it = collection.find(id);
        return it != collection.end() ? it->second.Copy() : AnyData::Document();
    }

    void InsertRecord(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc) override
    {
        FO_STACK_TRACE_ENTRY();

        FO_RUNTIME_ASSERT(!doc.Empty());

        std::scoped_lock locker {_storageLocker};

        auto& collection = _collections.at(collection_name);
        FO_RUNTIME_ASSERT(!collection.count(id));

        collection.emplace(id, doc.Copy());
    }

    void UpdateRecord(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc) override
    {
        FO_STACK_TRACE_ENTRY();

        FO_RUNTIME_ASSERT(!doc.Empty());

        std::scoped_lock locker {_storageLocker};

        auto& collection = _collections.at(collection_name);

        const auto it_collection = collection.find(id);
        FO_RUNTIME_ASSERT(it_collection != collection.end());

        for (auto&& [doc_key, doc_value] : doc) {
            it_collection->second.Assign(doc_key, doc_value.Copy());
        }
    }

    void DeleteRecord(hstring collection_name, const DataBaseKey& id) override
    {
        FO_STACK_TRACE_ENTRY();

        std::scoped_lock locker {_storageLocker};

        auto& collection = _collections.at(collection_name);

        const auto it = collection.find(id);
        FO_RUNTIME_ASSERT(it != collection.end());

        collection.erase(it);
    }

    void DrawGui() override
    {
        FO_STACK_TRACE_ENTRY();

        DataBaseImpl::DrawGui();

        constexpr ImGuiTableFlags TABLE_FLAGS = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchProp;

        const auto info_row = [](const char* key, string_view value) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(key);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(value.data(), value.data() + value.size());
        };

        std::scoped_lock locker {_storageLocker};

        if (ImGui::TreeNode(strex("Memory documents ({} collections)###MemoryDocs", _collections.size()).c_str())) {
            if (_collections.empty()) {
                ImGui::TextUnformatted("No memory collections");
            }

            for (auto&& [collection_name, collection] : _collections) {
                ImGui::PushID(static_cast<const void*>(&collection));

                if (ImGui::TreeNode(strex("{} ({})", collection_name.as_str(), collection.size()).c_str())) {
                    for (auto&& [id, doc] : collection) {
                        ImGui::PushID(static_cast<const void*>(&doc));

                        if (ImGui::TreeNode(strex("{} ({} keys)", id, doc.Size()).c_str())) {
                            if (ImGui::BeginTable("##DocFields", 2, TABLE_FLAGS)) {
                                ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 220.0f);
                                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                                for (auto&& [doc_key, doc_value] : doc) {
                                    info_row(doc_key.c_str(), AnyData::ValueToString(doc_value));
                                }

                                ImGui::EndTable();
                            }

                            ImGui::TreePop();
                        }

                        ImGui::PopID();
                    }

                    ImGui::TreePop();
                }

                ImGui::PopID();
            }

            ImGui::TreePop();
        }
    }

private:
    mutable std::mutex _storageLocker {};
    DataBase::Collections _collections {};
};

auto CreateMemoryDataBase(DataBaseSettings& db_settings, DataBasePanicCallback panic_callback) -> DataBaseImpl*
{
    return SafeAlloc::MakeRaw<DbMemory>(db_settings, std::move(panic_callback));
}

FO_END_NAMESPACE
