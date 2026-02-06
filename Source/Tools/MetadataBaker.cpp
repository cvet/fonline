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

#include "MetadataBaker.h"
#include "MetadataRegistration.h"

FO_BEGIN_NAMESPACE

MetadataBaker::MetadataBaker(shared_ptr<BakingContext> ctx) :
    BaseBaker(std::move(ctx))
{
    FO_STACK_TRACE_ENTRY();
}

MetadataBaker::~MetadataBaker()
{
    FO_STACK_TRACE_ENTRY();
}

void MetadataBaker::BakeFiles(const FileCollection& files, string_view target_path) const
{
    FO_STACK_TRACE_ENTRY();

    if (!target_path.empty() && !strex(target_path).get_file_extension().starts_with("fometa-")) {
        return;
    }

    // Collect files
    vector<File> filtered_files;
    uint64 max_write_time = 0;

    for (const auto& file_header : files) {
        const string ext = strex(file_header.GetPath()).get_file_extension();

        if (ext != "fos") {
            continue;
        }

        max_write_time = std::max(max_write_time, file_header.GetWriteTime());
        filtered_files.emplace_back(File::Load(file_header));
    }

    if (filtered_files.empty()) {
        return;
    }

    const bool bake_server = !_context->BakeChecker || _context->BakeChecker(_context->PackName + ".fometa-server", max_write_time);
    const bool bake_client = !_context->BakeChecker || _context->BakeChecker(_context->PackName + ".fometa-client", max_write_time);
    const bool bake_mapper = !_context->BakeChecker || _context->BakeChecker(_context->PackName + ".fometa-mapper", max_write_time);

    if (!bake_server && !bake_client && !bake_mapper) {
        return;
    }

    // Process files
    vector<std::future<void>> file_bakings;

    std::ranges::stable_sort(filtered_files, [](auto&& a, auto&& b) { return a.GetPath() < b.GetPath(); });

    if (bake_server) {
        file_bakings.emplace_back(async(GetAsyncMode(), [&]() FO_DEFERRED {
            auto data = BakeMetadata(filtered_files, "Server");
            _context->WriteData(_context->PackName + ".fometa-server", data);
        }));
    }
    if (bake_client) {
        file_bakings.emplace_back(async(GetAsyncMode(), [&]() FO_DEFERRED {
            auto data = BakeMetadata(filtered_files, "Client");
            _context->WriteData(_context->PackName + ".fometa-client", data);
        }));
    }
    if (bake_mapper) {
        file_bakings.emplace_back(async(GetAsyncMode(), [&]() FO_DEFERRED {
            auto data = BakeMetadata(filtered_files, "Mapper");
            _context->WriteData(_context->PackName + ".fometa-mapper", data);
        }));
    }

    size_t errors = 0;

    for (auto& file_baking : file_bakings) {
        try {
            file_baking.get();
        }
        catch (const std::exception& ex) {
            if (_context->ForceSyncMode.value_or(false)) {
                throw;
            }

            WriteLog("Metadata error: {}", ex.what());
            errors++;
        }
    }

    if (errors != 0) {
        throw MetadataBakerException("Errors during preparing of metadata");
    }
}

auto MetadataBaker::BakeMetadata(const vector<File>& files, string_view target) const -> vector<uint8>
{
    FO_STACK_TRACE_ENTRY();

    // Read codegen tags
    const unordered_set<string_view> valid_codegen_tags = {"Entity", "EntityHolder", "Enum", "PropertyComponent", "Property", "Event", "RemoteCall", "Setting", "MigrationRule"};

    vector<string> readed_files;
    readed_files.reserve(files.size());

    for (const auto& file : files) {
        readed_files.emplace_back(file.GetStr());
    }

    TagsParsingContext ctx;
    ctx.Target = target;
    ctx.ResultTags["Target"].emplace_back(vector<string> {string(target)});

    for (size_t i = 0; i < files.size(); i++) {
        const auto& file_str = readed_files[i];
        size_t pos = 0;
        size_t line_number = 1;

        while (true) {
            const size_t prev_pos = pos;
            pos = file_str.find("///@", pos);

            if (pos == string::npos) {
                break;
            }

            const auto it_begin = file_str.begin() + numeric_cast<ptrdiff_t>(prev_pos);
            const auto it_end = file_str.begin() + numeric_cast<ptrdiff_t>(pos);
            line_number += std::count(it_begin, it_end, '\n');

            string_view line;
            size_t end_pos = file_str.find('\n', pos);

            while (end_pos != string::npos && (file_str[end_pos - 1] == '\\' || (file_str[end_pos - 1] == '\r' && file_str[end_pos - 2] == '\\'))) {
                end_pos = file_str.find('\n', end_pos + 1);
            }

            if (end_pos != string::npos) {
                line = string_view(file_str).substr(pos + 4, end_pos - pos - 4);
                pos = end_pos;
            }
            else {
                line = string_view(file_str).substr(pos + 5);
                pos = file_str.size();
            }

            const size_t comment_pos = line.find("//");

            if (comment_pos != string::npos) {
                line = line.substr(0, comment_pos);
            }

            auto tokens = strvex(line).tokenize();

            if (tokens.empty()) {
                throw MetadataBakerException("Invalid codegen tag: empty tag", files[i].GetDiskPath(), line_number);
            }

            string_view tag_name = tokens.front();
            tokens.erase(tokens.begin());

            if (!valid_codegen_tags.contains(tag_name)) {
                throw MetadataBakerException("Invalid codegen tag: unknown tag name", files[i].GetDiskPath(), line_number, tag_name);
            }

            CodeGenTagDesc tag_desc;
            tag_desc.SourceFile = files[i].GetDiskPath();
            tag_desc.LineNumber = line_number;
            tag_desc.Tokens = std::move(tokens);
            ctx.CodeGenTags[string(tag_name)].emplace_back(std::move(tag_desc));
        }
    }

    ctx.Meta = SafeAlloc::MakeUnique<EngineMetadata>([] { });

    if (target == "Server") {
        RegisterServerStubMetadata(ctx.Meta.get(), nullptr, true);
    }
    else if (target == "Client") {
        RegisterClientStubMetadata(ctx.Meta.get(), nullptr, true);
    }
    else if (target == "Mapper") {
        RegisterMapperStubMetadata(ctx.Meta.get(), nullptr, true);
    }
    else {
        FO_UNREACHABLE_PLACE();
    }

    // Parse tags
    ParseEnum(ctx);
    ParseEntity(ctx);
    ParseEntityHolder(ctx);
    ParsePropertyComponent(ctx);
    ParseProperty(ctx);
    ParseEvent(ctx);
    ParseRemoteCall(ctx);
    ParseSetting(ctx);

    // Serialize data
    vector<uint8> data;
    DataWriter writer(data);

    writer.Write<uint16>(numeric_cast<uint16>(ctx.ResultTags.size()));

    for (const auto& [tag_name, tag_values] : ctx.ResultTags) {
        writer.Write<uint16>(numeric_cast<uint16>(tag_name.size()));
        writer.WritePtr(tag_name.data(), tag_name.size());
        writer.Write<uint32>(numeric_cast<uint32>(tag_values.size()));

        for (const auto& tag_value : tag_values) {
            writer.Write<uint32>(numeric_cast<uint32>(tag_value.size()));

            for (const auto& tag_value_part : tag_value) {
                writer.Write<uint16>(numeric_cast<uint16>(tag_value_part.size()));
                writer.WritePtr(tag_value_part.data(), tag_value_part.size());
            }
        }
    }

    return data;
}

void MetadataBaker::ParseEnum(TagsParsingContext& ctx) const
{
    FO_STACK_TRACE_ENTRY();

    struct EnumDesc
    {
        bool IsEngine {};
        string UnderlyingType {};
        vector<pair<string, optional<int64>>> Entries {};
        unordered_map<string, int32> EngineEntries {};
    };

    map<string, EnumDesc> enums;

    // Fill engine enums
    const auto& engine_enums = ctx.Meta->GetAllEnums();

    for (const auto& engine_enum : engine_enums) {
        const auto& engine_enum_type = ctx.Meta->GetBaseType(engine_enum.first);

        EnumDesc enum_desc;
        enum_desc.IsEngine = true;
        enum_desc.UnderlyingType = engine_enum_type.EnumUnderlyingType->Name;
        enum_desc.EngineEntries = engine_enum.second;

        enums.emplace(engine_enum.first, std::move(enum_desc));
    }

    // Parse tokens
    for (const auto& tag_desc : ctx.CodeGenTags["Enum"]) {
        if (tag_desc.Tokens.size() < 2) {
            throw MetadataBakerException("Invalid Enum codegen tag: insufficient parameters", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        const auto enum_name = tag_desc.Tokens[0];
        auto& enum_desc = enums[string(enum_name)];
        const auto enum_entry = tag_desc.Tokens[1];

        if (std::ranges::any_of(enum_desc.Entries, [&](auto&& e) { return e.first == enum_entry; })) {
            throw MetadataBakerException("Invalid Enum codegen tag: duplicate enum entry", tag_desc.SourceFile, tag_desc.LineNumber, enum_entry);
        }
        if (enum_desc.IsEngine && enum_desc.EngineEntries.contains(enum_entry)) {
            throw MetadataBakerException("Invalid Enum codegen tag: cannot override engine enum value", tag_desc.SourceFile, tag_desc.LineNumber, enum_name, enum_entry);
        }

        optional<int32> enum_value;

        if (tag_desc.Tokens.size() >= 3 && tag_desc.Tokens[2] == "=") {
            if (tag_desc.Tokens.size() < 4) {
                throw MetadataBakerException("Invalid Enum codegen tag: expected value after '='", tag_desc.SourceFile, tag_desc.LineNumber);
            }
            if (!strvex(tag_desc.Tokens[3]).is_number()) {
                throw MetadataBakerException("Invalid Enum codegen tag: expected number after '='", tag_desc.SourceFile, tag_desc.LineNumber, tag_desc.Tokens[3]);
            }

            enum_value = numeric_cast<int32>(strvex(tag_desc.Tokens[3]).to_int64());

            if (std::ranges::any_of(enum_desc.Entries, [&](auto&& kv) { return kv.second == enum_value.value(); })) {
                throw MetadataBakerException("Invalid Enum codegen tag: duplicate enum value", tag_desc.SourceFile, tag_desc.LineNumber, enum_value.value());
            }
            if (enum_desc.IsEngine && std::ranges::any_of(enum_desc.EngineEntries, [&](auto&& kv) { return numeric_cast<int64>(kv.second) == enum_value.value(); })) {
                throw MetadataBakerException("Invalid Enum codegen tag: duplicate enum value in engine enum", tag_desc.SourceFile, tag_desc.LineNumber, enum_value.value());
            }
        }

        enum_desc.Entries.emplace_back(enum_entry, enum_value);
    }

    // Calculate missed values
    for (auto& enum_desc : enums | std::views::values) {
        if (enum_desc.IsEngine && enum_desc.Entries.empty()) {
            continue;
        }

        int64 last_value = -1;

        for (const auto& value : enum_desc.Entries | std::views::values) {
            if (value.has_value()) {
                last_value = std::max(value.value(), last_value);
            }
        }

        if (enum_desc.IsEngine) {
            for (const auto& value : enum_desc.EngineEntries | std::views::values) {
                last_value = std::max(numeric_cast<int64>(value), last_value);
            }
        }

        for (auto& entry : enum_desc.Entries | std::views::values) {
            if (!entry.has_value()) {
                entry = ++last_value;
            }
        }
    }

    // Calculate underlying type
    for (auto&& [enum_name, enum_desc] : enums) {
        if (enum_desc.IsEngine) {
            continue;
        }

        string utype;

        if (enum_desc.Entries.empty()) {
            utype = "uint8";
        }
        else {
            int64 min_value = std::numeric_limits<int32>::max();
            int64 max_value = std::numeric_limits<int32>::min();

            for (const auto& value : enum_desc.Entries | std::views::values) {
                if (value.has_value()) {
                    min_value = std::min(value.value(), min_value);
                    max_value = std::max(value.value(), max_value);
                }
            }

            if (enum_desc.IsEngine) {
                for (const auto& value : enum_desc.EngineEntries | std::views::values) {
                    min_value = std::min(numeric_cast<int64>(value), min_value);
                    max_value = std::max(numeric_cast<int64>(value), max_value);
                }
            }

            FO_RUNTIME_ASSERT(max_value >= min_value);

            if (min_value < 0) {
                if (max_value > std::numeric_limits<int32>::max()) {
                    throw MetadataBakerException("Enum underlying type overflow: max value exceed int32 range", "", enum_name);
                }
                if (max_value < std::numeric_limits<int32>::max()) {
                    throw MetadataBakerException("Enum underlying type overflow: min value less than int32 range", "", enum_name);
                }

                utype = "int32";
            }
            else {
                if (max_value <= numeric_cast<int64>(std::numeric_limits<uint8>::max())) {
                    utype = "uint8";
                }
                else if (max_value <= numeric_cast<int64>(std::numeric_limits<uint16>::max())) {
                    utype = "uint16";
                }
                else if (max_value <= numeric_cast<int64>(std::numeric_limits<int32>::max())) {
                    utype = "int32";
                }
                else if (max_value <= numeric_cast<int64>(std::numeric_limits<uint32>::max())) {
                    utype = "uint32";
                }
                else {
                    throw MetadataBakerException("Enum underlying type overflow: max value exceed uint32 range", "", enum_name);
                }
            }
        }

        enum_desc.UnderlyingType = std::move(utype);
    }

    // Store final result
    vector<vector<string>> result_tag_enum;

    for (const auto& [enum_name, enum_desc] : enums) {
        if (enum_desc.IsEngine && enum_desc.Entries.empty()) {
            continue;
        }

        vector<string> enum_info;
        enum_info.emplace_back(enum_name);

        if (!enum_desc.IsEngine) {
            enum_info.emplace_back(enum_desc.UnderlyingType);
        }
        else {
            enum_info.emplace_back("");
        }

        for (const auto& entry : enum_desc.Entries) {
            enum_info.emplace_back(entry.first);
            enum_info.emplace_back(strex("{}", entry.second.value()));
        }

        if (!enum_desc.IsEngine) {
            unordered_map<string, int32> key_values;

            for (const auto& entry : enum_desc.Entries) {
                key_values.emplace(string(entry.first), numeric_cast<int32>(entry.second.value()));
            }

            ctx.Meta->RegisterEnumGroup(enum_name, enum_desc.UnderlyingType, std::move(key_values));
        }
        else {
            for (const auto& entry : enum_desc.Entries) {
                ctx.Meta->RegisterEnumEntry(enum_name, entry.first, numeric_cast<int32>(entry.second.value()));
            }
        }

        result_tag_enum.emplace_back(std::move(enum_info));
    }

    ctx.ResultTags["Enum"] = std::move(result_tag_enum);
}

void MetadataBaker::ParseEntity(TagsParsingContext& ctx) const
{
    FO_STACK_TRACE_ENTRY();

    vector<vector<string>> result_tag_entity;

    for (const auto& tag_desc : ctx.CodeGenTags["Entity"]) {
        if (tag_desc.Tokens.size() < 2) {
            throw MetadataBakerException("Invalid Entity codegen tag: insufficient parameters", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        const auto target = tag_desc.Tokens[0];
        const auto name = tag_desc.Tokens[1];

        if (!(ctx.Target == "Common" || target == "Common" || target == ctx.Target)) {
            ctx.OtherEntityTypes.emplace(name);
            continue;
        }

        const auto hname = ctx.Meta->Hashes.ToHashedString(name);
        const auto flags = span(tag_desc.Tokens).subspan(2);
        const auto is_global = std::ranges::any_of(flags, [](auto&& f) { return f == "Global"; });
        const auto has_protos = std::ranges::any_of(flags, [](auto&& f) { return f == "HasProtos"; });
        const auto has_statics = std::ranges::any_of(flags, [](auto&& f) { return f == "HasStatics"; });
        const auto has_abstract = std::ranges::any_of(flags, [](auto&& f) { return f == "HasAbstract"; });

        if (name.starts_with("Proto")) {
            throw MetadataBakerException("Invalid Entity codegen tag: entity name cannot start with 'Proto'", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (name.starts_with("Abstract")) {
            throw MetadataBakerException("Invalid Entity codegen tag: entity name cannot start with 'Abstract'", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (name.starts_with("Static")) {
            throw MetadataBakerException("Invalid Entity codegen tag: entity name cannot start with 'Static'", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (ctx.Meta->IsValidEntityType(hname)) {
            throw MetadataBakerException("Invalid Entity codegen tag: duplicate entity type", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (ctx.Meta->IsValidBaseType(name)) {
            throw MetadataBakerException("Invalid Entity codegen tag: entity name conflict with another type", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (ctx.Meta->IsValidBaseType(strex("{}Component", name))) {
            throw MetadataBakerException("Invalid Entity codegen tag: entity component enum conflict with another type", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (ctx.Meta->IsValidBaseType(strex("{}Property", name))) {
            throw MetadataBakerException("Invalid Entity codegen tag: entity property enum conflict with another type", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }

        ctx.Meta->RegisterEntityType(name, false, is_global, has_protos, has_statics, has_abstract);

        auto tokens = vec_transform(tag_desc.Tokens, [](auto&& e) -> string { return string(e); });
        tokens.erase(tokens.begin()); // Remove target
        result_tag_entity.emplace_back(tokens);
    }

    ctx.ResultTags["Entity"] = std::move(result_tag_entity);
}

void MetadataBaker::ParseEntityHolder(TagsParsingContext& ctx) const
{
    FO_STACK_TRACE_ENTRY();

    vector<vector<string>> result_tag_entity_holder;

    for (const auto& tag_desc : ctx.CodeGenTags["EntityHolder"]) {
        if (tag_desc.Tokens.size() < 4) {
            throw MetadataBakerException("Invalid EntityHolder codegen tag: insufficient parameters", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        const auto target = tag_desc.Tokens[0];

        const auto holder_entity_name = tag_desc.Tokens[1];
        const auto holder_entity_hname = ctx.Meta->Hashes.ToHashedString(holder_entity_name);
        const auto target_entity_name = tag_desc.Tokens[2];
        const auto target_entity_hname = ctx.Meta->Hashes.ToHashedString(target_entity_name);
        const auto entry_name = tag_desc.Tokens[3];
        const auto flags = span(tag_desc.Tokens).subspan(4);
        const auto has_no_sync = std::ranges::any_of(flags, [](auto&& f) { return f == "NoSync"; });
        const auto has_owner_sync = std::ranges::any_of(flags, [](auto&& f) { return f == "OwnerSync"; });
        const auto has_public_sync = std::ranges::any_of(flags, [](auto&& f) { return f == "PublicSync"; });
        const auto sync = has_public_sync ? EntityHolderEntrySync::PublicSync : (has_owner_sync ? EntityHolderEntrySync::OwnerSync : EntityHolderEntrySync::NoSync);

        if (!ctx.Meta->IsValidEntityType(holder_entity_hname)) {
            throw MetadataBakerException("Invalid EntityHolder codegen tag: unknown holder entity type", tag_desc.SourceFile, tag_desc.LineNumber, holder_entity_name);
        }

        if (!(ctx.Target == "Common" || target == "Common" || target == ctx.Target)) {
            // Stub for entry property
            if (ctx.Meta->IsValidEntityType(holder_entity_name)) {
                vector<string> stub;
                stub.emplace_back("Stub");
                stub.emplace_back(holder_entity_name);
                stub.emplace_back("");
                stub.emplace_back(entry_name);
                result_tag_entity_holder.emplace_back(stub);
            }

            continue;
        }

        if (!ctx.Meta->IsValidEntityType(target_entity_hname)) {
            throw MetadataBakerException("Invalid EntityHolder codegen tag: unknown target entity type", tag_desc.SourceFile, tag_desc.LineNumber, target_entity_name);
        }
        if (static_cast<int32>(has_no_sync) + static_cast<int32>(has_owner_sync) + static_cast<int32>(has_public_sync) > 1) {
            throw MetadataBakerException("Invalid EntityHolder codegen tag: sync flags invalid mixture", tag_desc.SourceFile, tag_desc.LineNumber);
        }
        if (target == "Common" && !has_owner_sync && !has_public_sync && !has_no_sync) {
            throw MetadataBakerException("Invalid EntityHolder codegen tag: Common target requires sync flag", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        const auto& target_entity_type = ctx.Meta->GetEntityType(target_entity_hname);

        if (target_entity_type.Exported) {
            throw MetadataBakerException("Invalid EntityHolder codegen tag: cannot hold exported entity type", tag_desc.SourceFile, tag_desc.LineNumber, target_entity_name);
        }

        ctx.Meta->RegsiterEntityHolderEntry(holder_entity_name, target_entity_name, entry_name, sync);

        auto tokens = vec_transform(tag_desc.Tokens, [](auto&& e) -> string { return string(e); });
        result_tag_entity_holder.emplace_back(tokens);
    }

    ctx.ResultTags["EntityHolder"] = std::move(result_tag_entity_holder);
}

void MetadataBaker::ParsePropertyComponent(TagsParsingContext& ctx) const
{
    FO_STACK_TRACE_ENTRY();

    vector<vector<string>> result_tag_property_component;

    for (const auto& tag_desc : ctx.CodeGenTags["PropertyComponent"]) {
        if (tag_desc.Tokens.size() < 2) {
            throw MetadataBakerException("Invalid PropertyComponent codegen tag: insufficient parameters", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        const auto entity_name = tag_desc.Tokens[0];
        const auto entity_hname = ctx.Meta->Hashes.ToHashedString(entity_name);
        const auto component_name = tag_desc.Tokens[1];
        const auto component_hname = ctx.Meta->Hashes.ToHashedString(component_name);

        if (!ctx.Meta->IsValidEntityType(entity_hname)) {
            throw MetadataBakerException("Invalid PropertyComponent codegen tag: unknown entity type", tag_desc.SourceFile, tag_desc.LineNumber, entity_name);
        }

        auto* prop_registrator = ctx.Meta->GetPropertyRegistratorForEdit(entity_name);

        if (prop_registrator->IsComponentRegistered(component_hname)) {
            throw MetadataBakerException("Invalid PropertyComponent codegen tag: duplicate property component", tag_desc.SourceFile, tag_desc.LineNumber, component_name);
        }

        prop_registrator->RegisterComponent(component_name);
        ctx.Meta->RegisterEnumEntry(strex("{}Component", entity_name), component_name, component_hname.as_int32());

        auto tokens = vec_transform(tag_desc.Tokens, [](auto&& e) -> string { return string(e); });
        result_tag_property_component.emplace_back(std::move(tokens));
    }

    ctx.ResultTags["PropertyComponent"] = std::move(result_tag_property_component);
}

void MetadataBaker::ParseProperty(TagsParsingContext& ctx) const
{
    FO_STACK_TRACE_ENTRY();

    vector<vector<string>> result_tag_property;

    for (const auto& tag_desc : ctx.CodeGenTags["Property"]) {
        if (tag_desc.Tokens.size() < 4) {
            throw MetadataBakerException("Invalid Property codegen tag: insufficient parameters", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        const auto entity_name = tag_desc.Tokens[0];

        if (ctx.OtherEntityTypes.contains(entity_name)) {
            continue;
        }

        if (!ctx.Meta->IsValidEntityType(ctx.Meta->Hashes.ToHashedString(entity_name))) {
            throw MetadataBakerException("Invalid Property codegen tag: unknown entity type", tag_desc.SourceFile, tag_desc.LineNumber, entity_name);
        }

        const auto target = tag_desc.Tokens[1];

        if (target != "Common" && target != "Server" && target != "Client") {
            throw MetadataBakerException("Invalid Property codegen tag: invalid target", tag_desc.SourceFile, tag_desc.LineNumber, target);
        }

        ComplexTypeDesc type;
        size_t type_tokens = 0;

        try {
            std::tie(type, type_tokens) = ctx.Meta->ResolveComplexType(span(tag_desc.Tokens).subspan(2));
        }
        catch (TypeResolveException& ex) {
            throw MetadataBakerException("Invalid Property codegen tag: cannot resolve property type", tag_desc.SourceFile, tag_desc.LineNumber, ex.what());
        }

        if (type.IsMutable) {
            throw MetadataBakerException("Invalid Property codegen tag: property type can't be ref", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        size_t tok_index = 2 + type_tokens;
        auto name = string(tag_desc.Tokens[tok_index]);

        if (tok_index < tag_desc.Tokens.size() - 2 && tag_desc.Tokens[tok_index + 1] == ".") {
            name += ".";
            name += tag_desc.Tokens[tok_index + 2];
            tok_index += 2;
        }

        auto* registrator = ctx.Meta->GetPropertyRegistratorForEdit(entity_name);

        if (registrator->FindProperty(name) != nullptr) {
            throw MetadataBakerException("Invalid Property codegen tag: duplicate property", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }

        const auto flags = span(tag_desc.Tokens).subspan(tok_index + 1);
        const bool is_common = target == "Common";
        const bool is_owner_sync = std::ranges::any_of(flags, [](auto&& f) { return f == "OwnerSync"; });
        const bool is_public_sync = std::ranges::any_of(flags, [](auto&& f) { return f == "PublicSync"; });
        const bool is_no_sync = std::ranges::any_of(flags, [](auto&& f) { return f == "NoSync"; });
        const bool is_mutable = std::ranges::any_of(flags, [](auto&& f) { return f == "Mutable"; });
        const bool is_persistent = std::ranges::any_of(flags, [](auto&& f) { return f == "Persistent"; });
        const bool is_virtual = std::ranges::any_of(flags, [](auto&& f) { return f == "Virtual"; });
        const bool is_modifiable_by_client = std::ranges::any_of(flags, [](auto&& f) { return f == "ModifiableByClient"; });
        const bool is_modifiable_by_any_client = std::ranges::any_of(flags, [](auto&& f) { return f == "ModifiableByAnyClient"; });
        const bool is_null_getter_for_proto = std::ranges::any_of(flags, [](auto&& f) { return f == "NullGetterForProto"; });
        const bool is_synced = is_common && is_mutable && (is_owner_sync || is_public_sync);

        if (is_mutable && is_common && !is_virtual && !is_owner_sync && !is_public_sync && !is_no_sync) {
            throw MetadataBakerException("Invalid Property codegen tag: common mutable property must have sync type", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (is_virtual && (is_owner_sync || is_public_sync || is_no_sync)) {
            throw MetadataBakerException("Invalid Property codegen tag: synced property can't be virtual", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (static_cast<int32>(is_owner_sync) + static_cast<int32>(is_public_sync) + static_cast<int32>(is_no_sync) > 1) {
            throw MetadataBakerException("Invalid Property codegen tag: sync flags invalid mixture", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (is_modifiable_by_client && !is_synced) {
            throw MetadataBakerException("Invalid Property codegen tag: modifiable property must be synced", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (is_modifiable_by_any_client && !is_public_sync) {
            throw MetadataBakerException("Invalid Property codegen tag: modifiable by any property must be public synced", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (is_persistent && !is_mutable) {
            throw MetadataBakerException("Invalid Property codegen tag: persistent property must be mutable", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (is_synced && is_virtual) {
            throw MetadataBakerException("Invalid Property codegen tag: virtual property can't be synced", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (is_persistent && is_virtual) {
            throw MetadataBakerException("Invalid Property codegen tag: virtual property can't be persistent", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (is_null_getter_for_proto && !is_virtual) {
            throw MetadataBakerException("Invalid Property codegen tag: null getter can only be on virtual property", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }

        vector<string_view> tokens;
        tokens.emplace_back(entity_name);
        tokens.emplace_back(target);
        const string merged_type = strex("").join(span(tag_desc.Tokens).subspan(2, type_tokens));
        tokens.emplace_back(merged_type);
        tokens.emplace_back(name);
        tokens.insert(tokens.end(), flags.begin(), flags.end());

        const auto* prop = registrator->RegisterProperty(span(tokens).subspan(1));
        const auto prop_enum_name = prop->GetComponent() ? strex("{}_{}", prop->GetComponent(), prop->GetNameWithoutComponent()) : prop->GetName();
        ctx.Meta->RegisterEnumEntry(strex("{}Property", entity_name), prop_enum_name, numeric_cast<int32>(prop->GetRegIndex()));

        result_tag_property.emplace_back(vec_transform(tokens, [](auto&& e) -> string { return string(e); }));
    }

    ctx.ResultTags["Property"] = std::move(result_tag_property);
}

void MetadataBaker::ParseEvent(TagsParsingContext& ctx) const
{
    FO_STACK_TRACE_ENTRY();

    vector<vector<string>> result_tag_event;

    for (const auto& tag_desc : ctx.CodeGenTags["Event"]) {
        if (tag_desc.Tokens.size() < 5) {
            throw MetadataBakerException("Invalid Event codegen tag: insufficient parameters", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        const auto target = tag_desc.Tokens[0];

        if (!(ctx.Target == "Common" || target == "Common" || target == ctx.Target)) {
            continue;
        }

        const auto entity_name = tag_desc.Tokens[1];
        const auto entity_hname = ctx.Meta->Hashes.ToHashedString(entity_name);

        if (!ctx.Meta->IsValidEntityType(entity_hname)) {
            throw MetadataBakerException("Invalid Event codegen tag: invalid entity type", tag_desc.SourceFile, tag_desc.LineNumber, entity_hname);
        }

        EntityEventDesc event_desc;
        const auto event_name = tag_desc.Tokens[2];
        event_desc.Name = ctx.Meta->Hashes.ToHashedString(event_name);

        vector<string> tag_tokens;
        tag_tokens.emplace_back(entity_name);
        tag_tokens.emplace_back(event_name);
        tag_tokens.emplace_back(""); // Deferred set below

        if (tag_desc.Tokens[3] != "(") {
            throw MetadataBakerException("Invalid Event codegen tag: expected '(' after event name", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        size_t cur_token = 4;

        while (true) {
            if (cur_token == 4 && tag_desc.Tokens[cur_token] == ")") {
                cur_token++;
                break;
            }

            ComplexTypeDesc type;

            try {
                size_t type_tokens = 0;
                std::tie(type, type_tokens) = ctx.Meta->ResolveComplexType(span(tag_desc.Tokens).subspan(cur_token));
                tag_tokens.emplace_back(strex(" ").join(span(tag_desc.Tokens).subspan(cur_token, type_tokens)));
                cur_token += type_tokens;
            }
            catch (TypeResolveException& ex) {
                throw MetadataBakerException("Invalid Event codegen tag: cannot resolve arg type", tag_desc.SourceFile, tag_desc.LineNumber, ex.what());
            }

            if (cur_token == tag_desc.Tokens.size()) {
                throw MetadataBakerException("Invalid Event codegen tag: expected arg name after it's type", tag_desc.SourceFile, tag_desc.LineNumber);
            }

            const auto name = tag_desc.Tokens[cur_token];
            tag_tokens.emplace_back(name);
            event_desc.Args.emplace_back(string(name), std::move(type));
            cur_token++;

            if (cur_token == tag_desc.Tokens.size() || (tag_desc.Tokens[cur_token] != "," && tag_desc.Tokens[cur_token] != ")")) {
                throw MetadataBakerException("Invalid Event codegen tag: expected ')' or ',' after arg", tag_desc.SourceFile, tag_desc.LineNumber);
            }

            if (tag_desc.Tokens[cur_token++] == ")") {
                break;
            }
        }

        if (cur_token < tag_desc.Tokens.size()) {
            if (tag_desc.Tokens[cur_token] != "Deferred") {
                throw MetadataBakerException("Invalid Event codegen tag: expected 'Deferred'", tag_desc.SourceFile, tag_desc.LineNumber);
            }

            tag_tokens[2] = "Deferred";
        }

        result_tag_event.emplace_back(std::move(tag_tokens));
    }

    ctx.ResultTags["Event"] = std::move(result_tag_event);
}

void MetadataBaker::ParseRemoteCall(TagsParsingContext& ctx) const
{
    FO_STACK_TRACE_ENTRY();

    vector<vector<string>> result_tag_remote_call;

    for (const auto& tag_desc : ctx.CodeGenTags["RemoteCall"]) {
        if (tag_desc.Tokens.size() < 4) {
            throw MetadataBakerException("Invalid RemoteCall codegen tag: insufficient parameters", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        const auto target = tag_desc.Tokens[0];

        if (target != "Server" && target != "Client") {
            throw MetadataBakerException("Invalid RemoteCall codegen tag: expected 'Server' or 'Client' as target", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        const bool inbound = target == ctx.Target;
        const auto remote_call_name = tag_desc.Tokens[1];

        if (tag_desc.Tokens[2] != "(") {
            throw MetadataBakerException("Invalid RemoteCall codegen tag: expected '(' after remote call name", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        RemoteCallDesc recote_call_desc;
        recote_call_desc.Name = ctx.Meta->Hashes.ToHashedString(remote_call_name);

        vector<string> tag_tokens;
        tag_tokens.emplace_back(remote_call_name);
        tag_tokens.emplace_back(strex(tag_desc.SourceFile).extract_file_name());
        tag_tokens.emplace_back(inbound ? "In" : "Out");

        size_t cur_token = 3;

        while (true) {
            if (cur_token == 3 && tag_desc.Tokens[cur_token] == ")") {
                break;
            }

            ComplexTypeDesc type;

            try {
                size_t type_tokens = 0;
                std::tie(type, type_tokens) = ctx.Meta->ResolveComplexType(span(tag_desc.Tokens).subspan(cur_token));
                tag_tokens.emplace_back(strex(" ").join(span(tag_desc.Tokens).subspan(cur_token, type_tokens)));
                cur_token += type_tokens;
            }
            catch (TypeResolveException& ex) {
                throw MetadataBakerException("Invalid RemoteCall codegen tag: cannot resolve arg type", tag_desc.SourceFile, tag_desc.LineNumber, ex.what());
            }

            if (cur_token == tag_desc.Tokens.size()) {
                throw MetadataBakerException("Invalid RemoteCall codegen tag: expected arg name after it's type", tag_desc.SourceFile, tag_desc.LineNumber);
            }

            const auto name = tag_desc.Tokens[cur_token];
            tag_tokens.emplace_back(name);
            recote_call_desc.Args.emplace_back(string(name), std::move(type));
            cur_token++;

            if (cur_token == tag_desc.Tokens.size() || (tag_desc.Tokens[cur_token] != "," && tag_desc.Tokens[cur_token] != ")")) {
                throw MetadataBakerException("Invalid RemoteCall codegen tag: expected ')' or ',' after arg", tag_desc.SourceFile, tag_desc.LineNumber);
            }

            if (tag_desc.Tokens[cur_token++] == ")") {
                break;
            }
        }

        result_tag_remote_call.emplace_back(std::move(tag_tokens));
    }

    ctx.ResultTags["RemoteCall"] = std::move(result_tag_remote_call);
}

void MetadataBaker::ParseSetting(TagsParsingContext& ctx) const
{
    FO_STACK_TRACE_ENTRY();

    vector<vector<string>> result_tag_setting;

    for (const auto& tag_desc : ctx.CodeGenTags["Setting"]) {
        if (tag_desc.Tokens.size() < 3) {
            throw MetadataBakerException("Invalid Setting codegen tag: insufficient parameters", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        const auto target = tag_desc.Tokens[0];

        if (!(target == "Common" || target == ctx.Target)) {
            continue;
        }

        const auto type_str = tag_desc.Tokens[1];
        const auto name = tag_desc.Tokens[2];

        if (!ctx.Meta->IsValidBaseType(type_str)) {
            throw MetadataBakerException("Invalid Setting codegen tag: invalid type", tag_desc.SourceFile, tag_desc.LineNumber, type_str);
        }

        const auto type = ctx.Meta->GetBaseType(type_str);

        if (!type.IsPrimitive && !type.IsEnum && !type.IsString) {
            throw MetadataBakerException("Invalid Setting codegen tag: type must be primitive or enum or string or any", tag_desc.SourceFile, tag_desc.LineNumber, type.Name);
        }

        vector<string> tag_tokens;
        tag_tokens.emplace_back(name);
        tag_tokens.emplace_back(type_str);
        result_tag_setting.emplace_back(std::move(tag_tokens));
    }

    ctx.ResultTags["Setting"] = std::move(result_tag_setting);
}

void MetadataBaker::ParseMigrationRule(TagsParsingContext& ctx) const
{
    FO_STACK_TRACE_ENTRY();

    vector<vector<string>> result_tag_migration_rule;

    for (const auto& tag_desc : ctx.CodeGenTags["MigrationRule"]) {
        if (tag_desc.Tokens.size() < 4) {
            throw MetadataBakerException("Invalid MigrationRule codegen tag: insufficient parameters", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        vector<string> tag_tokens;
        tag_tokens.emplace_back(tag_desc.Tokens[0]);
        tag_tokens.emplace_back(tag_desc.Tokens[1]);
        tag_tokens.emplace_back(tag_desc.Tokens[2]);
        tag_tokens.emplace_back(tag_desc.Tokens[3]);
        result_tag_migration_rule.emplace_back(std::move(tag_tokens));
    }

    ctx.ResultTags["Setting"] = std::move(result_tag_migration_rule);
}

FO_END_NAMESPACE
