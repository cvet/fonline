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

#include "MapBaker.h"
#include "AngelScriptScripting.h"
#include "ConfigFile.h"
#include "ManagedScripting.h"
#include "MapLoader.h"
#include "ProtoManager.h"
#include "ScriptSystem.h"

FO_BEGIN_NAMESPACE

MapBaker::MapBaker(shared_ptr<BakingContext> ctx) :
    BaseBaker(std::move(ctx), NAME)
{
    FO_STACK_TRACE_ENTRY();
}

MapBaker::~MapBaker()
{
    FO_STACK_TRACE_ENTRY();
}

void MapBaker::BakeFiles(const FileCollection& files, string_view target_path) const
{
    FO_STACK_TRACE_ENTRY();

    struct MapBakeEntry
    {
        File SourceFile {};
        string MapName {};
    };

    // Collect map files
    vector<MapBakeEntry> filtered_files;

    const auto check_file = [&](const FileHeader& file_header, string_view map_name) -> bool {
        const bool server_side = _context->BakeChecker(strex("{}.fomap-bin-server", map_name), file_header.GetWriteTime());
        const bool client_side = _context->BakeChecker(strex("{}.fomap-bin-client", map_name), file_header.GetWriteTime());

        if (server_side || client_side) {
            if (!server_side) {
                (void)_context->BakeChecker(strex("{}.fomap-bin-server", map_name), file_header.GetWriteTime());
            }
            if (!client_side) {
                (void)_context->BakeChecker(strex("{}.fomap-bin-client", map_name), file_header.GetWriteTime());
            }
        }

        return server_side || client_side;
    };

    if (target_path.empty()) {
        for (const auto& file_header : files) {
            const string ext = strex(file_header.GetPath()).get_file_extension();

            if (ext != "fomap") {
                continue;
            }

            File file = File::Load(file_header);
            string map_name = ResolveMapName(file);

            if (_context->BakeChecker && !check_file(file_header, map_name)) {
                continue;
            }

            filtered_files.emplace_back(MapBakeEntry {std::move(file), std::move(map_name)});
        }
    }
    else {
        if (!strex(target_path).get_file_extension().starts_with("fomap-")) {
            return;
        }

        File file;
        bool file_found = false;
        const string target_map_name = strex(target_path).extract_file_name().erase_file_extension().str();

        if (auto exact_file = files.FindFileByPath(strex(target_path).change_file_extension("fomap"))) {
            file = std::move(exact_file);
            file_found = true;
        }
        else {
            for (const auto& file_header : files) {
                if (strex(file_header.GetPath()).get_file_extension() != "fomap") {
                    continue;
                }

                File candidate_file = File::Load(file_header);
                string candidate_map_name = ResolveMapName(candidate_file);

                if (candidate_map_name == target_map_name) {
                    file = std::move(candidate_file);
                    file_found = true;
                    break;
                }
            }
        }

        if (!file_found) {
            return;
        }

        string map_name = ResolveMapName(file);

        if (_context->BakeChecker && !check_file(file, map_name)) {
            return;
        }

        filtered_files.emplace_back(MapBakeEntry {std::move(file), std::move(map_name)});
    }

    if (filtered_files.empty()) {
        return;
    }

    // Load protos
    auto server_engine = BakerServerEngine(*_context->BakedFiles);
    auto client_engine = BakerClientEngine(*_context->BakedFiles);

    vector<std::future<void>> proto_loadings;
    proto_loadings.emplace_back(run_async(GetAsyncMode(), "BakeMap-RegisterServerProtos", [&]() FO_DEFERRED { server_engine.RegisterProtos(*_context->BakedFiles); }));
    proto_loadings.emplace_back(run_async(GetAsyncMode(), "BakeMap-RegisterClientProtos", [&]() FO_DEFERRED { client_engine.RegisterProtos(*_context->BakedFiles); }));

    for (auto& proto_loading : proto_loadings) {
        proto_loading.get();
    }

    server_engine.FinalizeRegistration();
    client_engine.FinalizeRegistration();

    server_engine.MapScriptTypes(&server_engine);
#if FO_ANGELSCRIPT_SCRIPTING
    InitAngelScriptScripting(&server_engine, *_context->Settings, *_context->BakedFiles);
#endif
#if FO_MANAGED_SCRIPTING
    InitManagedScripting(&server_engine, *_context->BakedFiles, _context->Settings->BakeOutput);
#endif

    // Bake maps
    const auto bake_map = [&](const MapBakeEntry& entry) {
        const File& file = entry.SourceFile;
        const string file_content = file.GetStr();
        const string& map_name = entry.MapName;

        vector<uint8_t> props_data;
        uint32_t map_cr_count = 0;
        uint32_t map_item_count = 0;
        uint32_t map_client_item_count = 0;
        vector<uint8_t> map_cr_data;
        vector<uint8_t> map_item_data;
        vector<uint8_t> map_client_item_data;
        auto map_cr_data_writer = DataWriter(map_cr_data);
        auto map_item_data_writer = DataWriter(map_item_data);
        auto map_client_item_data_writer = DataWriter(map_client_item_data);
        set<hstring> str_hashes;
        set<hstring> client_str_hashes;

        size_t errors = 0;

        MapLoader::Load(
            map_name, file_content, server_engine, server_engine.Hashes,
            [&](ident_t id, ptr<const ProtoCritter> proto, ptr<const map<string_view, string_view>> kv) {
                auto props = proto->GetProperties()->Copy();
                props.ApplyFromText(*kv);

                errors += ValidateProperties(props, strex("map {} critter {} with id {}", map_name, proto->GetName(), id), &server_engine);

                map_cr_count++;
                map_cr_data_writer.Write<ident_t::underlying_type>(id.underlying_value());
                map_cr_data_writer.Write<hstring::hash_t>(proto->GetProtoId().as_hash());
                props.StoreAllData(props_data, str_hashes);
                map_cr_data_writer.Write<uint32_t>(numeric_cast<uint32_t>(props_data.size()));
                auto map_cr_writer = make_ptr(&map_cr_data_writer);
                map_cr_writer->WriteByteVector(props_data);
            },
            [&](ident_t id, ptr<const ProtoItem> proto, ptr<const map<string_view, string_view>> kv) {
                auto props = proto->GetProperties()->Copy();
                props.ApplyFromText(*kv);

                errors += ValidateProperties(props, strex("map {} item {} with id {}", map_name, proto->GetName(), id), &server_engine);

                map_item_count++;
                map_item_data_writer.Write<ident_t::underlying_type>(id.underlying_value());
                map_item_data_writer.Write<hstring::hash_t>(proto->GetProtoId().as_hash());
                props.StoreAllData(props_data, str_hashes);
                map_item_data_writer.Write<uint32_t>(numeric_cast<uint32_t>(props_data.size()));
                auto map_item_writer = make_ptr(&map_item_data_writer);
                map_item_writer->WriteByteVector(props_data);

                const auto is_static = proto->GetStatic();
                const auto is_hidden = proto->GetHidden();

                if (is_static) {
                    auto client_proto = client_engine.GetProtoItem(proto->GetProtoId());
                    FO_VERIFY_AND_THROW(client_proto, "Missing required client prototype");

                    auto client_props = client_proto->GetProperties()->Copy();
                    client_props.ApplyFromText(*kv);

                    // For hidden items keep only string hashes for client
                    client_props.StoreAllData(props_data, client_str_hashes);

                    if (!is_hidden) {
                        map_client_item_count++;
                        map_client_item_data_writer.Write<ident_t::underlying_type>(id.underlying_value());
                        map_client_item_data_writer.Write<hstring::hash_t>(client_proto->GetProtoId().as_hash());
                        map_client_item_data_writer.Write<uint32_t>(numeric_cast<uint32_t>(props_data.size()));
                        auto map_client_item_writer = make_ptr(&map_client_item_data_writer);
                        map_client_item_writer->WriteByteVector(props_data);
                    }
                }
            });

        if (errors != 0) {
            throw MapBakerException("Map validation error(s)");
        }

        // Server side
        {
            vector<uint8_t> map_data;
            auto final_writer = DataWriter(map_data);

            final_writer.Write<uint32_t>(numeric_cast<uint32_t>(str_hashes.size()));

            for (const auto& hstr : str_hashes) {
                const string_view str = hstr.as_str();
                final_writer.Write<uint32_t>(numeric_cast<uint32_t>(str.length()));
                final_writer.WriteStringBytes(str);
            }

            final_writer.Write<uint32_t>(map_cr_count);
            auto final_writer_ptr = make_ptr(&final_writer);
            final_writer_ptr->WriteByteVector(map_cr_data);
            final_writer.Write<uint32_t>(map_item_count);
            final_writer_ptr->WriteByteVector(map_item_data);

            _context->WriteData(strex("{}.fomap-bin-server", map_name), map_data);
        }

        // Client side
        {
            vector<uint8_t> map_data;
            auto final_writer = DataWriter(map_data);

            final_writer.Write<uint32_t>(numeric_cast<uint32_t>(client_str_hashes.size()));

            for (const auto& hstr : client_str_hashes) {
                const string_view str = hstr.as_str();
                final_writer.Write<uint32_t>(numeric_cast<uint32_t>(str.length()));
                final_writer.WriteStringBytes(str);
            }

            final_writer.Write<uint32_t>(map_client_item_count);
            auto final_writer_ptr = make_ptr(&final_writer);
            final_writer_ptr->WriteByteVector(map_client_item_data);

            _context->WriteData(strex("{}.fomap-bin-client", map_name), map_data);
        }
    };

    // Async baking
    vector<std::future<void>> file_bakings;

    for (const auto& entry : filtered_files) {
        auto entry_ptr = make_ptr(&entry);
        file_bakings.emplace_back(run_async(GetAsyncMode(), strex("BakeMap-{}", entry_ptr->MapName), [&, entry_ptr]() FO_DEFERRED { bake_map(*entry_ptr); }));
    }

    size_t errors = 0;

    for (auto& file_baking : file_bakings) {
        try {
            file_baking.get();
        }
        catch (const std::exception& ex) {
            WriteLog("Map baking error: {}", ex.what());
            errors++;
        }
    }

    if (errors != 0) {
        throw MapBakerException("Errors during map baking");
    }
}

auto MapBaker::ResolveMapName(const File& file) -> string
{
    FO_STACK_TRACE_ENTRY();

    const auto fomap = ConfigFile(file.GetPath(), file.GetStr(), ConfigFileOption::ReadFirstSection);
    return string(fomap.GetAsStr("Header", "$Name", file.GetNameNoExt()));
}

FO_END_NAMESPACE
