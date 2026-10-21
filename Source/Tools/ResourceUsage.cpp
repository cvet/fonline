//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#include "ResourceUsage.h"
#include "Baker.h"
#include "ConfigFile.h"
#include "MapLoader.h"
#include "Properties.h"
#include "ResourceDefaults.h"

FO_DISABLE_WARNINGS_PUSH()
#include <json.hpp>
FO_DISABLE_WARNINGS_POP()

FO_BEGIN_NAMESPACE

static void ValidateUsagePath(string_view path)
{
    FO_VERIFY_AND_THROW(!path.empty() && path.find_first_of("\\:\0", 0, 3) == string_view::npos && !path.starts_with('/'), "Invalid resource usage path", path);

    for (string_view part : strvex(path).split('/')) {
        FO_VERIFY_AND_THROW(!part.empty() && part != "." && part != "..", "Noncanonical resource usage path", path);
    }
}

void ResourceUsage::RegisterPack(string_view pack)
{
    ValidateUsagePath(pack);
    scoped_lock lock {_locker};
    _packs.emplace(pack);
}

void ResourceUsage::RegisterOutput(string_view pack, string_view path, string_view baker, BakingOutputKind kind)
{
    ValidateUsagePath(pack);
    ValidateUsagePath(path);
    string key = strex(pack).combine_path(path);
    scoped_lock lock {_locker};
    _packs.emplace(pack);
    auto [it, inserted] = _outputs.try_emplace(key, ResourceOutput {string {pack}, string {path}, string {baker}, kind});

    if (!inserted) {
        FO_VERIFY_AND_THROW(it->second.Baker == baker, "Resource output has multiple baking owners", key, it->second.Baker, baker);

        if (kind == BakingOutputKind::Required) {
            it->second.Kind = kind;
        }
    }
}

void ResourceUsage::AddReference(string_view path, string_view reason)
{
    if (path.empty()) {
        return;
    }

    ValidateUsagePath(path);
    scoped_lock lock {_locker};
    _references[string {path}].emplace(reason);
}

void ResourceUsage::AddProperties(const Properties& properties, string_view owner)
{
    auto registrar = properties.GetRegistrar();

    for (size_t i = 1; i < registrar->GetPropertiesCount(); i++) {
        auto property = registrar->GetPropertyByIndexUnsafe(i);

        if (property->IsDisabled() || !property->IsBaseTypeResource()) {
            continue;
        }

        string reason = strex("{} property {}", owner, property->GetName());

        if (property->IsPlainData()) {
            AddReference(properties.GetValue<hstring>(property).as_str(), reason);
        }
        else if (property->IsArray()) {
            for (hstring path : properties.GetValue<vector<hstring>>(property)) {
                AddReference(path.as_str(), reason);
            }
        }
        else {
            throw ResourceBakingException("Resource property must be scalar or an array", owner, property->GetName());
        }
    }
}

static void CollectUsageMap(ResourceUsage& usage, EngineMetadata& metadata, const File& file, bool client)
{
    data_reader reader {file.GetDataSpan()};
    MapLoader::ReadBakedFileHeader(reader, file.GetPath());
    uint32_t hash_count = reader.read<uint32_t>();
    reader.verify_payload_count(hash_count, sizeof(uint32_t));

    for (uint32_t i = 0; i < hash_count; i++) {
        (void)metadata.Hashes.to_hashed_string(reader.read_string());
    }

    vector<hstring> types;

    if (!client) {
        types.emplace_back(metadata.Hashes.to_hashed_string("Critter"));
    }

    types.emplace_back(metadata.Hashes.to_hashed_string("Item"));

    for (hstring type : types) {
        uint32_t count = reader.read<uint32_t>();
        reader.verify_payload_count(count, sizeof(ident_t::underlying_type) + sizeof(hstring::hash_t) + sizeof(uint32_t));

        for (uint32_t i = 0; i < count; i++) {
            (void)reader.read<ident_t::underlying_type>();
            hstring pid = metadata.Hashes.resolve_hash(reader.read<hstring::hash_t>());
            auto proto = metadata.GetProtoEntity(type, pid);
            FO_VERIFY_AND_THROW(proto, "Resource usage map prototype is missing", file.GetPath(), type, pid);
            uint32_t size = reader.read<uint32_t>();
            const_span<uint8_t> data = reader.read_bytes(size);
            Properties properties {proto->GetProperties()->GetRegistrar()};
            properties.RestoreAllData(vector<uint8_t> {data.begin(), data.end()});
            usage.AddProperties(properties, strex("Map {} {}", file.GetPath(), type));
        }
    }

    FO_VERIFY_AND_THROW(reader.get_unread_size() == 0, "Resource usage map has trailing data", file.GetPath());
}

static void CollectUsageMetadata(ResourceUsage& usage, EngineMetadata& metadata, const FileSystem& resources, bool client)
{
    metadata.RegisterProtos(resources);
    metadata.FinalizeRegistration();

    for (const auto& [type, protos] : metadata.GetAllProtos()) {
        for (const auto& [pid, proto] : protos) {
            usage.AddProperties(*proto->GetProperties(), strex("Proto {} {}", type, pid));
        }
    }

    for (const FileHeader& header : resources.FilterFiles(client ? "fomap-bin-client" : "fomap-bin-server")) {
        CollectUsageMap(usage, metadata, File::Load(header), client);
    }
}

void ResourceUsage::CollectReferences(const FileSystem& resources)
{
    if (resources.IsFileExists(FontDefaults::Updater)) {
        AddReference(FontDefaults::Updater, "Native updater font");
    }

    for (const FileHeader& header : resources.FilterFiles("fomain-client")) {
        File file = File::Load(header);
        ConfigFile config {file.GetStr()};

        for (string_view setting : {"Render.ImGuiDefaultEffect", "Render.CritterStubSpriteName", "Render.ItemStubSpriteName"}) {
            string value {config.GetAsStr("", setting)};

            if (!value.empty()) {
                AddReference(value, strex("Config {} setting {}", header.GetPath(), setting));
            }
        }
    }

    if (resources.FilterFiles("fometa-server").GetFilesCount() != 0) {
        BakerServerEngine metadata {resources};
        CollectUsageMetadata(*this, metadata, resources, false);
    }

    if (resources.FilterFiles("fometa-client").GetFilesCount() != 0) {
        BakerClientEngine metadata {resources};
        CollectUsageMetadata(*this, metadata, resources, true);
    }
}

auto ResourceUsage::GetOutputs() const -> vector<ResourceOutput>
{
    scoped_lock lock {_locker};
    vector<ResourceOutput> result;
    result.reserve(_outputs.size());

    for (const auto& output : _outputs | std::views::values) {
        result.emplace_back(output);
    }

    return result;
}

auto ResourceUsage::CreateManifest(string_view build_hash, string_view config_hash, const OutputAvailable& available, const Dependencies& dependencies) const -> string
{
    map<string, ResourceOutput> outputs;
    map<string, set<string>> references;
    set<string> packs;

    {
        scoped_lock lock {_locker};
        outputs = _outputs;
        references = _references;
        packs = _packs;
    }

    map<string, vector<string>> providers;

    for (const auto& [key, output] : outputs) {
        if (available(output)) {
            providers[output.Path].emplace_back(key);
        }
    }

    set<string> selected;
    map<string, set<string>> reasons;
    deque<string> pending;
    auto select = [&](const string& key, string_view reason) {
        reasons[key].emplace(reason);

        if (selected.emplace(key).second) {
            pending.emplace_back(key);
        }
    };
    auto reference = [&](const string& path, string_view reason) {
        auto it = providers.find(path);
        FO_VERIFY_AND_THROW(it != providers.end(), "Referenced resource has no baked output", path, reason);

        for (const string& key : it->second) {
            select(key, reason);
        }
    };

    for (const auto& [key, output] : outputs) {
        if (output.Kind == BakingOutputKind::Required) {
            FO_VERIFY_AND_THROW(available(output), "Required baked output is missing", key, output.Baker);
            select(key, strex("Required output of {}", output.Baker));
        }
    }

    for (const auto& [path, owners] : references) {
        for (const string& owner : owners) {
            reference(path, owner);
        }
    }

    while (!pending.empty()) {
        string key = std::move(pending.front());
        pending.pop_front();

        for (const string& path : dependencies(outputs.at(key))) {
            ValidateUsagePath(path);
            reference(path, strex("Dependency of {}", key));
        }
    }

    nlohmann::ordered_json manifest;
    manifest["Version"] = 1;
    manifest["BuildHash"] = std::string {build_hash};
    manifest["BakingConfigHash"] = std::string {config_hash};
    manifest["Packs"] = nlohmann::ordered_json::object();
    manifest["Reasons"] = nlohmann::ordered_json::object();

    for (const string& pack : packs) {
        manifest["Packs"][std::string {pack}] = nlohmann::ordered_json::array();
    }

    for (const string& key : selected) {
        const ResourceOutput& output = outputs.at(key);
        manifest["Packs"][std::string {output.Pack}].push_back(std::string {output.Path});
        auto& entry = manifest["Reasons"][std::string {key}];
        entry = nlohmann::ordered_json::array();

        for (const string& reason : reasons.at(key)) {
            entry.push_back(std::string {reason});
        }
    }

    return string {manifest.dump(2)} + "\n";
}

FO_END_NAMESPACE
