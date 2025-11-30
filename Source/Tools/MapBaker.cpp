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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "ConfigFile.h"
#include "MapLoader.h"
#include "ProtoManager.h"
#include "ScriptSystem.h"

FO_BEGIN_NAMESPACE();

MapBaker::MapBaker(BakerData& data) :
    BaseBaker(data)
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

    // Collect map files
    vector<File> filtered_files;

    const auto check_file = [&](const FileHeader& file_header) -> bool {
        const bool server_side = _bakeChecker(strex(file_header.GetPath()).change_file_extension("fomap-bin-server"), file_header.GetWriteTime());
        const bool client_side = _bakeChecker(strex(file_header.GetPath()).change_file_extension("fomap-bin-client"), file_header.GetWriteTime());

        if (server_side || client_side) {
            if (!server_side) {
                (void)_bakeChecker(strex(file_header.GetPath()).change_file_extension("fomap-bin-server"), file_header.GetWriteTime());
            }
            if (!client_side) {
                (void)_bakeChecker(strex(file_header.GetPath()).change_file_extension("fomap-bin-client"), file_header.GetWriteTime());
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
            if (_bakeChecker && !check_file(file_header)) {
                continue;
            }

            filtered_files.emplace_back(File::Load(file_header));
        }
    }
    else {
        if (!strex(target_path).get_file_extension().starts_with("fomap-")) {
            return;
        }

        auto file = files.FindFileByPath(strex(target_path).change_file_extension("fomap"));

        if (!file) {
            return;
        }
        if (_bakeChecker && !check_file(file)) {
            return;
        }

        filtered_files.emplace_back(std::move(file));
    }

    if (filtered_files.empty()) {
        return;
    }

    // Load protos
    auto server_engine = BakerEngine(PropertiesRelationType::ServerRelative);
    auto client_engine = BakerEngine(PropertiesRelationType::ClientRelative);
    auto server_proto_mngr = ProtoManager(server_engine);
    auto client_proto_mngr = ProtoManager(client_engine);
    const auto server_script_sys = BakerScriptSystem(server_engine, *_bakedFiles);

    vector<std::future<void>> proto_loadings;
    proto_loadings.emplace_back(std::async(GetAsyncMode(), [&] { server_proto_mngr.LoadFromResources(*_bakedFiles); }));
    proto_loadings.emplace_back(std::async(GetAsyncMode(), [&] { client_proto_mngr.LoadFromResources(*_bakedFiles); }));

    for (auto& proto_loading : proto_loadings) {
        proto_loading.get();
    }

    // Bake maps
    const auto bake_map = [&](const File& file) {
        const string& file_content = file.GetStr();

        string map_name = [&]() -> string {
            const auto fomap = ConfigFile(file.GetPath(), file_content, nullptr, ConfigFileOption::ReadFirstSection);
            return string(fomap.GetAsStr("Header", "$Name", file.GetNameNoExt()));
        }();

        vector<uint8> props_data;
        uint32 map_cr_count = 0;
        uint32 map_item_count = 0;
        uint32 map_client_item_count = 0;
        vector<uint8> map_cr_data;
        vector<uint8> map_item_data;
        vector<uint8> map_client_item_data;
        auto map_cr_data_writer = DataWriter(map_cr_data);
        auto map_item_data_writer = DataWriter(map_item_data);
        auto map_client_item_data_writer = DataWriter(map_client_item_data);
        set<hstring> str_hashes;
        set<hstring> client_str_hashes;

        size_t errors = 0;

        MapLoader::Load(
            map_name, file_content, server_proto_mngr, server_engine.Hashes,
            [&](ident_t id, const ProtoCritter* proto, const map<string, string>& kv) {
                auto props = proto->GetProperties().Copy();
                props.ApplyFromText(kv);

                errors += ValidateProperties(props, strex("map {} critter {} with id {}", map_name, proto->GetName(), id), &server_script_sys);

                map_cr_count++;
                map_cr_data_writer.Write<ident_t::underlying_type>(id.underlying_value());
                map_cr_data_writer.Write<hstring::hash_t>(proto->GetProtoId().as_hash());
                props.StoreAllData(props_data, str_hashes);
                map_cr_data_writer.Write<uint32>(numeric_cast<uint32>(props_data.size()));
                map_cr_data_writer.WritePtr(props_data.data(), props_data.size());
            },
            [&](ident_t id, const ProtoItem* proto, const map<string, string>& kv) {
                auto props = proto->GetProperties().Copy();
                props.ApplyFromText(kv);

                errors += ValidateProperties(props, strex("map {} item {} with id {}", map_name, proto->GetName(), id), &server_script_sys);

                map_item_count++;
                map_item_data_writer.Write<ident_t::underlying_type>(id.underlying_value());
                map_item_data_writer.Write<hstring::hash_t>(proto->GetProtoId().as_hash());
                props.StoreAllData(props_data, str_hashes);
                map_item_data_writer.Write<uint32>(numeric_cast<uint32>(props_data.size()));
                map_item_data_writer.WritePtr(props_data.data(), props_data.size());

                const auto is_static = proto->GetStatic();
                const auto is_hidden = proto->GetHidden();

                if (is_static && !is_hidden) {
                    const auto* client_proto = client_proto_mngr.GetProtoItem(proto->GetProtoId());
                    auto client_props = client_proto->GetProperties().Copy();
                    client_props.ApplyFromText(kv);

                    map_client_item_count++;
                    map_client_item_data_writer.Write<ident_t::underlying_type>(id.underlying_value());
                    map_client_item_data_writer.Write<hstring::hash_t>(client_proto->GetProtoId().as_hash());
                    client_props.StoreAllData(props_data, client_str_hashes);
                    map_client_item_data_writer.Write<uint32>(numeric_cast<uint32>(props_data.size()));
                    map_client_item_data_writer.WritePtr(props_data.data(), props_data.size());
                }
            });

        if (errors != 0) {
            throw MapBakerException("Map validation error(s)");
        }

        // Server side
        {
            vector<uint8> map_data;
            auto final_writer = DataWriter(map_data);

            final_writer.Write<uint32>(numeric_cast<uint32>(str_hashes.size()));

            for (const auto& hstr : str_hashes) {
                const auto& str = hstr.as_str();
                final_writer.Write<uint32>(numeric_cast<uint32>(str.length()));
                final_writer.WritePtr(str.c_str(), str.length());
            }

            final_writer.Write<uint32>(map_cr_count);
            final_writer.WritePtr(map_cr_data.data(), map_cr_data.size());
            final_writer.Write<uint32>(map_item_count);
            final_writer.WritePtr(map_item_data.data(), map_item_data.size());

            _writeData(strex("{}.fomap-bin-server", map_name), map_data);
        }

        // Client side
        {
            vector<uint8> map_data;
            auto final_writer = DataWriter(map_data);

            final_writer.Write<uint32>(numeric_cast<uint32>(client_str_hashes.size()));

            for (const auto& hstr : client_str_hashes) {
                const auto& str = hstr.as_str();
                final_writer.Write<uint32>(numeric_cast<uint32>(str.length()));
                final_writer.WritePtr(str.c_str(), str.length());
            }

            final_writer.Write<uint32>(map_client_item_count);
            final_writer.WritePtr(map_client_item_data.data(), map_client_item_data.size());

            _writeData(strex("{}.fomap-bin-client", map_name), map_data);
        }
    };

    // Async baking
    vector<std::future<void>> file_bakings;

    for (const auto& file : filtered_files) {
        file_bakings.emplace_back(std::async(GetAsyncMode(), [&] { bake_map(file); }));
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
        catch (...) {
            FO_UNKNOWN_EXCEPTION();
        }
    }

    if (errors != 0) {
        throw MapBakerException("Errors during map baking");
    }
}

FO_END_NAMESPACE();
