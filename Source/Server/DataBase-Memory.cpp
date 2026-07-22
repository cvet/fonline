#include "DataBase.h"

#include "ImGuiStuff.h"

FO_BEGIN_NAMESPACE

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
            ImGuiTextUnformatted(key);
            ImGui::TableSetColumnIndex(1);
            ImGuiTextUnformatted(value);
        };

        scoped_lock locker {_storageLocker};

        // Exception safety: build a plain-data model first (Phase 1), then render it with pure ImGui calls (Phase 2).
        // Every recoverable throw (strex, AnyData::ValueToString on a non-finite Float64) happens in Phase 1, before any
        // ImGui push, so an exception leaves ImGui's ID/tree/table stacks untouched and balanced instead of corrupting the frame.
        struct FieldRow
        {
            string key;
            string value;
        };
        struct DocEntry
        {
            string label;
            const void* id;
            vector<FieldRow> fields;
        };
        struct CollEntry
        {
            string label;
            const void* id;
            vector<DocEntry> docs;
        };

        const string memory_docs_label = strex("Memory documents ({} collections)###MemoryDocs", _collections.size()).str();

        vector<CollEntry> model;
        model.reserve(_collections.size());

        for (auto&& [collection_name, collection] : _collections) {
            CollEntry coll_entry;
            coll_entry.label = strex("{} ({})", collection_name.as_str(), collection.size()).str();
            coll_entry.id = static_cast<const void*>(&collection);
            coll_entry.docs.reserve(collection.size());

            for (auto&& [id, doc] : collection) {
                DocEntry doc_entry;
                doc_entry.label = strex("{} ({} keys)", id, doc.Size()).str();
                doc_entry.id = static_cast<const void*>(&doc);
                doc_entry.fields.reserve(doc.Size());

                for (auto&& [doc_key, doc_value] : doc) {
                    doc_entry.fields.emplace_back(FieldRow {doc_key, AnyData::ValueToString(doc_value)});
                }

                coll_entry.docs.emplace_back(std::move(doc_entry));
            }

            model.emplace_back(std::move(coll_entry));
        }

        auto memory_docs_label_cstr = make_ptr(memory_docs_label.c_str());

        if (ImGui::TreeNode(memory_docs_label_cstr.get())) {
            if (model.empty()) {
                ImGui::TextUnformatted("No memory collections");
            }

            for (const auto& coll : model) {
                ImGui::PushID(coll.id);

                auto collection_label_cstr = make_ptr(coll.label.c_str());

                if (ImGui::TreeNode(collection_label_cstr.get())) {
                    for (const auto& doc_entry : coll.docs) {
                        ImGui::PushID(doc_entry.id);

                        auto doc_label_cstr = make_ptr(doc_entry.label.c_str());

                        if (ImGui::TreeNode(doc_label_cstr.get())) {
                            if (ImGui::BeginTable("##DocFields", 2, TABLE_FLAGS)) {
                                ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 220.0f);
                                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                                for (const auto& field : doc_entry.fields) {
                                    info_row(field.key, field.value);
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
