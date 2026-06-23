#include "DataBase.h"

#include "ImGuiStuff.h"

FO_BEGIN_NAMESPACE

static void DataBaseMemoryImGuiTextUnformatted(string_view text)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (text.empty()) {
        ImGui::TextUnformatted("");
        return;
    }

    ptr<const char> text_data = text.data();
    ptr<const char> text_end = text_data.get() + text.size();
    ImGui::TextUnformatted(text_data.get(), text_end.get());
}

class DbMemory final : public DataBaseImpl
{
public:
    explicit DbMemory(ptr<DataBaseSettings> db_settings, DataBasePanicCallback panic_callback) :
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

        scoped_lock locker {_storageLocker};

        _collections.try_emplace(collection_name);
    }

    [[nodiscard]] auto GetAllRecordIds(hstring collection_name) const -> vector<DataBaseKey> override
    {
        FO_STACK_TRACE_ENTRY();

        scoped_lock locker {_storageLocker};

        const auto& collection = _collections.at(collection_name);

        vector<DataBaseKey> ids;
        ids.reserve(collection.size());

        for (const auto& key : collection | std::views::keys) {
            ids.emplace_back(key);
        }

        return ids;
    }

protected:
    [[nodiscard]] auto GetRecord(hstring collection_name, const DataBaseKey& id) const -> AnyData::Document override
    {
        FO_STACK_TRACE_ENTRY();

        scoped_lock locker {_storageLocker};

        const auto& collection = _collections.at(collection_name);

        const auto it = collection.find(id);
        return it != collection.end() ? it->second.Copy() : AnyData::Document();
    }

    void InsertRecord(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc) override
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(!doc.Empty(), "Memory database insert received an empty document", collection_name, id);

        scoped_lock locker {_storageLocker};

        auto& collection = _collections.at(collection_name);
        FO_VERIFY_AND_THROW(!collection.count(id), "Memory database collection already contains the inserted record id", collection_name, id);

        collection.emplace(id, doc.Copy());
    }

    void UpdateRecord(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc) override
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(!doc.Empty(), "Memory database update received an empty document", collection_name, id);

        scoped_lock locker {_storageLocker};

        auto& collection = _collections.at(collection_name);

        const auto it_collection = collection.find(id);

        if (it_collection == collection.end()) {
            throw DataBaseException("DbMemory Document not found for update", collection_name, id);
        }

        for (auto&& [doc_key, doc_value] : doc) {
            it_collection->second.Assign(doc_key, doc_value.Copy());
        }
    }

    void DeleteRecord(hstring collection_name, const DataBaseKey& id) override
    {
        FO_STACK_TRACE_ENTRY();

        scoped_lock locker {_storageLocker};

        auto& collection = _collections.at(collection_name);

        const auto it = collection.find(id);

        if (it == collection.end()) {
            throw DataBaseException("DbMemory Document not found for delete", collection_name, id);
        }

        collection.erase(it);
    }

    void DrawGui() override
    {
        FO_STACK_TRACE_ENTRY();

        DataBaseImpl::DrawGui();

        constexpr ImGuiTableFlags TABLE_FLAGS = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchProp;

        const auto info_row = [](string_view key, string_view value) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            DataBaseMemoryImGuiTextUnformatted(key);
            ImGui::TableSetColumnIndex(1);
            DataBaseMemoryImGuiTextUnformatted(value);
        };

        scoped_lock locker {_storageLocker};

        const string memory_docs_label = strex("Memory documents ({} collections)###MemoryDocs", _collections.size()).str();
        ptr<const char> memory_docs_label_cstr = memory_docs_label.c_str();

        if (ImGui::TreeNode(memory_docs_label_cstr.get())) {
            if (_collections.empty()) {
                ImGui::TextUnformatted("No memory collections");
            }

            for (auto&& [collection_name, collection] : _collections) {
                ImGui::PushID(static_cast<const void*>(&collection));

                const string collection_label = strex("{} ({})", collection_name.as_str(), collection.size()).str();
                ptr<const char> collection_label_cstr = collection_label.c_str();

                if (ImGui::TreeNode(collection_label_cstr.get())) {
                    for (auto&& [id, doc] : collection) {
                        ImGui::PushID(static_cast<const void*>(&doc));

                        const string doc_label = strex("{} ({} keys)", id, doc.Size()).str();
                        ptr<const char> doc_label_cstr = doc_label.c_str();

                        if (ImGui::TreeNode(doc_label_cstr.get())) {
                            if (ImGui::BeginTable("##DocFields", 2, TABLE_FLAGS)) {
                                ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 220.0f);
                                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                                for (auto&& [doc_key, doc_value] : doc) {
                                    info_row(doc_key, AnyData::ValueToString(doc_value));
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
    mutable mutex _storageLocker {};
    DataBase::Collections _collections FO_TSA_GUARDED_BY(_storageLocker) {};
};

auto CreateMemoryDataBase(ptr<DataBaseSettings> db_settings, DataBasePanicCallback panic_callback) -> unique_ptr<DataBaseImpl>
{
    return SafeAlloc::MakeUnique<DbMemory>(db_settings, std::move(panic_callback));
}

FO_END_NAMESPACE
