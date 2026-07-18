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

extern auto GetServerSettings() -> unordered_set<string>;
extern auto GetClientSettings() -> unordered_set<string>;

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
    uint64_t max_write_time = 0;

    for (const auto& file_header : files) {
        string ext = strex(file_header.GetPath()).get_file_extension();

        if (ext != "fos") {
            continue;
        }

        max_write_time = std::max(max_write_time, file_header.GetWriteTime());
        filtered_files.emplace_back(File::Load(file_header));
    }

    if (filtered_files.empty()) {
        return;
    }

    bool bake_server = !_context->BakeChecker || _context->BakeChecker(_context->PackName + ".fometa-server", max_write_time);
    bool bake_client = !_context->BakeChecker || _context->BakeChecker(_context->PackName + ".fometa-client", max_write_time);
    bool bake_mapper = !_context->BakeChecker || _context->BakeChecker(_context->PackName + ".fometa-mapper", max_write_time);

    if (!bake_server && !bake_client && !bake_mapper) {
        return;
    }

    // Process files
    vector<std::future<void>> file_bakings;

    std::ranges::stable_sort(filtered_files, [](auto&& a, auto&& b) { return a.GetPath() < b.GetPath(); });

    if (bake_server) {
        file_bakings.emplace_back(run_async(GetAsyncMode(), "BakeMetadata-Server", [&]() FO_DEFERRED {
            auto data = BakeMetadata(filtered_files, "Server");
            _context->WriteData(_context->PackName + ".fometa-server", data);
        }));
    }
    if (bake_client) {
        file_bakings.emplace_back(run_async(GetAsyncMode(), "BakeMetadata-Client", [&]() FO_DEFERRED {
            auto data = BakeMetadata(filtered_files, "Client");
            _context->WriteData(_context->PackName + ".fometa-client", data);
        }));
    }
    if (bake_mapper) {
        file_bakings.emplace_back(run_async(GetAsyncMode(), "BakeMetadata-Mapper", [&]() FO_DEFERRED {
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

auto MetadataBaker::BakeMetadata(const vector<File>& files, string_view target) const -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    // Read codegen tags
    unordered_set<string_view> valid_codegen_tags = {"Entity", "EntityHolder", "FixedType", "ValueType", "RefType", "Enum", "Property", "Event", "RemoteCall", "Setting", "MigrationRule"};

    vector<string> readed_files;
    readed_files.reserve(files.size());

    for (const auto& file : files) {
        readed_files.emplace_back(file.GetStr());
    }

    TagsParsingContext ctx {.Target = target};
    ctx.ResultTags["Target"].emplace_back(vector<string> {string(target)});

    for (size_t i = 0; i < files.size(); i++) {
        const auto& file_str = readed_files[i];
        size_t pos = 0;
        size_t line_number = 1;

        while (true) {
            size_t prev_pos = pos;
            pos = file_str.find("///@", pos);

            if (pos == string::npos) {
                break;
            }

            auto it_begin = file_str.begin() + numeric_cast<ptrdiff_t>(prev_pos);
            auto it_end = file_str.begin() + numeric_cast<ptrdiff_t>(pos);
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

            string normalized_line;

            if (line.find('\\') != string_view::npos) {
                normalized_line.reserve(line.size());

                for (size_t line_pos = 0; line_pos < line.size(); line_pos++) {
                    char ch = line[line_pos];

                    if (ch == '\\' && line_pos + 1 < line.size() && (line[line_pos + 1] == '\n' || line[line_pos + 1] == '\r')) {
                        normalized_line += ' ';

                        while (line_pos + 1 < line.size() && (line[line_pos + 1] == '\n' || line[line_pos + 1] == '\r')) {
                            line_pos++;
                        }

                        continue;
                    }

                    normalized_line += ch;
                }

                ctx.NormalizedLines.emplace_back(SafeAlloc::MakeUnique<string>(std::move(normalized_line)));
                line = *ctx.NormalizedLines.back();
            }

            size_t comment_pos = line.find("//");

            if (comment_pos != string::npos) {
                line = line.substr(0, comment_pos);
            }

            auto tokens = strvex(line).tokenize();

            if (tokens.empty()) {
                throw MetadataBakerException("Invalid codegen tag: empty tag", files[i].GetPath(), line_number);
            }

            string_view tag_name = tokens.front();
            tokens.erase(tokens.begin());

            if (!valid_codegen_tags.contains(tag_name)) {
                throw MetadataBakerException("Invalid codegen tag: unknown tag name", files[i].GetPath(), line_number, tag_name);
            }

            CodeGenTagDesc tag_desc;
            tag_desc.SourceFile = string(files[i].GetPath());
            tag_desc.LineNumber = line_number;
            tag_desc.Tokens = std::move(tokens);
            ctx.CodeGenTags[string(tag_name)].emplace_back(std::move(tag_desc));
        }
    }

    nptr<const FileSystem> resources = nullptr;

    if (target == "Server") {
        RegisterServerStubMetadata(&ctx.Meta, resources);
    }
    else if (target == "Client") {
        RegisterClientStubMetadata(&ctx.Meta, resources);
    }
    else if (target == "Mapper") {
        RegisterMapperStubMetadata(&ctx.Meta, resources);
    }
    else {
        FO_UNREACHABLE_PLACE();
    }

    // Parse tags
    ParseEnum(ctx);
    ParseEntity(ctx);
    ParseEntityHolder(ctx);
    ParseFixedType(ctx);
    ParseValueType(ctx);
    ParseRefType(ctx);
    ParseProperty(ctx);
    ParseEvent(ctx);
    ParseRemoteCall(ctx);
    ParseSetting(ctx);
    ParseMigrationRule(ctx);
    ctx.Meta.FinalizeRegistration();

    // Serialize data
    vector<uint8_t> data;
    DataWriter writer(data);

    writer.Write<uint16_t>(numeric_cast<uint16_t>(ctx.ResultTags.size()));

    for (const auto& [tag_name, tag_values] : ctx.ResultTags) {
        writer.Write<uint16_t>(numeric_cast<uint16_t>(tag_name.size()));
        writer.WriteStringBytes(tag_name);
        writer.Write<uint32_t>(numeric_cast<uint32_t>(tag_values.size()));

        for (const auto& tag_value : tag_values) {
            writer.Write<uint32_t>(numeric_cast<uint32_t>(tag_value.size()));

            for (const auto& tag_value_part : tag_value) {
                writer.Write<uint16_t>(numeric_cast<uint16_t>(tag_value_part.size()));
                writer.WriteStringBytes(tag_value_part);
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
        vector<pair<string, optional<int64_t>>> Entries {};
        unordered_map<string, int32_t> EngineEntries {};
    };

    map<string, EnumDesc> enums;

    // Fill engine enums
    const auto& engine_enums = ctx.Meta.GetAllEnums();

    for (const auto& engine_enum : engine_enums) {
        const auto& engine_enum_type = ctx.Meta.GetBaseType(engine_enum.first);

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

        auto enum_name = tag_desc.Tokens[0];
        auto& enum_desc = enums[string(enum_name)];
        auto enum_entry = tag_desc.Tokens[1];

        if (std::ranges::any_of(enum_desc.Entries, [&](auto&& e) { return e.first == enum_entry; })) {
            throw MetadataBakerException("Invalid Enum codegen tag: duplicate enum entry", tag_desc.SourceFile, tag_desc.LineNumber, enum_entry);
        }
        if (enum_desc.IsEngine && enum_desc.EngineEntries.contains(enum_entry)) {
            throw MetadataBakerException("Invalid Enum codegen tag: cannot override engine enum value", tag_desc.SourceFile, tag_desc.LineNumber, enum_name, enum_entry);
        }

        optional<int32_t> enum_value;

        if (tag_desc.Tokens.size() >= 3 && tag_desc.Tokens[2] == "=") {
            if (tag_desc.Tokens.size() < 4) {
                throw MetadataBakerException("Invalid Enum codegen tag: expected value after '='", tag_desc.SourceFile, tag_desc.LineNumber);
            }
            if (!strvex(tag_desc.Tokens[3]).is_number() && tag_desc.Tokens[3] != "-") {
                throw MetadataBakerException("Invalid Enum codegen tag: expected number after '='", tag_desc.SourceFile, tag_desc.LineNumber, tag_desc.Tokens[3]);
            }

            int64_t value;

            if (tag_desc.Tokens[3] == "-") {
                if (tag_desc.Tokens.size() < 5 || !strvex(tag_desc.Tokens[4]).is_number()) {
                    throw MetadataBakerException("Invalid Enum codegen tag: expected number after '-'", tag_desc.SourceFile, tag_desc.LineNumber, tag_desc.Tokens[4]);
                }

                value = -strvex(tag_desc.Tokens[4]).to_int64();
            }
            else {
                value = strvex(tag_desc.Tokens[3]).to_int64();
            }

            if (value < std::numeric_limits<int32_t>::min() || value > std::numeric_limits<int32_t>::max()) {
                throw MetadataBakerException("Invalid Enum codegen tag: enum value out of int32 range", tag_desc.SourceFile, tag_desc.LineNumber, value);
            }

            enum_value = numeric_cast<int32_t>(value);

            if (std::ranges::any_of(enum_desc.Entries, [&](auto&& kv) { return kv.second == enum_value.value(); })) {
                throw MetadataBakerException("Invalid Enum codegen tag: duplicate enum value", tag_desc.SourceFile, tag_desc.LineNumber, enum_value.value());
            }
            if (enum_desc.IsEngine && std::ranges::any_of(enum_desc.EngineEntries, [&](auto&& kv) { return numeric_cast<int64_t>(kv.second) == enum_value.value(); })) {
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

        int64_t last_value = -1;

        for (const auto& value : enum_desc.Entries | std::views::values) {
            if (value.has_value()) {
                last_value = std::max(value.value(), last_value);
            }
        }

        if (enum_desc.IsEngine) {
            for (const auto& value : enum_desc.EngineEntries | std::views::values) {
                last_value = std::max(numeric_cast<int64_t>(value), last_value);
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
            int64_t min_value = std::numeric_limits<int32_t>::max();
            int64_t max_value = std::numeric_limits<int32_t>::min();

            for (const auto& value : enum_desc.Entries | std::views::values) {
                if (value.has_value()) {
                    min_value = std::min(value.value(), min_value);
                    max_value = std::max(value.value(), max_value);
                }
            }

            if (enum_desc.IsEngine) {
                for (const auto& value : enum_desc.EngineEntries | std::views::values) {
                    min_value = std::min(numeric_cast<int64_t>(value), min_value);
                    max_value = std::max(numeric_cast<int64_t>(value), max_value);
                }
            }

            FO_VERIFY_AND_THROW(max_value >= min_value, "Enum metadata did not collect any value before choosing an underlying type", enum_name, min_value, max_value);

            if (min_value < 0) {
                if (max_value > std::numeric_limits<int32_t>::max()) {
                    throw MetadataBakerException("Enum underlying type overflow: max value exceed int32 range", "", enum_name);
                }
                if (min_value < std::numeric_limits<int32_t>::min()) {
                    throw MetadataBakerException("Enum underlying type overflow: min value less than int32 range", "", enum_name);
                }

                utype = "int32";
            }
            else {
                if (max_value <= numeric_cast<int64_t>(std::numeric_limits<uint8_t>::max())) {
                    utype = "uint8";
                }
                else if (max_value <= numeric_cast<int64_t>(std::numeric_limits<uint16_t>::max())) {
                    utype = "uint16";
                }
                else if (max_value <= numeric_cast<int64_t>(std::numeric_limits<int32_t>::max())) {
                    utype = "int32";
                }
                else if (max_value <= numeric_cast<int64_t>(std::numeric_limits<uint32_t>::max())) {
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
            unordered_map<string, int32_t> key_values;

            for (const auto& entry : enum_desc.Entries) {
                key_values.emplace(string(entry.first), numeric_cast<int32_t>(entry.second.value()));
            }

            ctx.Meta.RegisterEnumGroup(enum_name, enum_desc.UnderlyingType, std::move(key_values));
        }
        else {
            for (const auto& entry : enum_desc.Entries) {
                ctx.Meta.RegisterEnumEntry(enum_name, entry.first, numeric_cast<int32_t>(entry.second.value()));
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

        auto target = tag_desc.Tokens[0];
        auto name = tag_desc.Tokens[1];

        if (!(ctx.Target == "Common" || target == "Common" || target == ctx.Target)) {
            ctx.OtherEntityTypes.emplace(name);
            continue;
        }

        hstring hname = ctx.Meta.Hashes.ToHashedString(name);
        auto flags = span(tag_desc.Tokens).subspan(2);
        bool is_global = std::ranges::any_of(flags, [](auto&& f) { return f == "Global"; });
        bool has_protos = std::ranges::any_of(flags, [](auto&& f) { return f == "HasProtos"; });
        bool has_statics = std::ranges::any_of(flags, [](auto&& f) { return f == "HasStatics"; });
        bool has_abstract = std::ranges::any_of(flags, [](auto&& f) { return f == "HasAbstract"; });

        if (name.starts_with("Proto")) {
            throw MetadataBakerException("Invalid Entity codegen tag: entity name cannot start with 'Proto'", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (name.starts_with("Abstract")) {
            throw MetadataBakerException("Invalid Entity codegen tag: entity name cannot start with 'Abstract'", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (name.starts_with("Static")) {
            throw MetadataBakerException("Invalid Entity codegen tag: entity name cannot start with 'Static'", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (ctx.Meta.IsValidEntityType(hname) || ctx.Meta.IsFixedType(hname)) {
            throw MetadataBakerException("Invalid Entity codegen tag: duplicate entity type", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (ctx.Meta.IsValidBaseType(name)) {
            throw MetadataBakerException("Invalid Entity codegen tag: entity name conflict with another type", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (ctx.Meta.IsValidBaseType(strex("{}Property", name))) {
            throw MetadataBakerException("Invalid Entity codegen tag: entity property enum conflict with another type", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }

        ctx.Meta.RegisterEntityType(name, false, is_global, has_protos, has_statics, has_abstract);

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

        auto target = tag_desc.Tokens[0];

        auto holder_entity_name = tag_desc.Tokens[1];
        hstring holder_entity_hname = ctx.Meta.Hashes.ToHashedString(holder_entity_name);
        auto target_entity_name = tag_desc.Tokens[2];
        hstring target_entity_hname = ctx.Meta.Hashes.ToHashedString(target_entity_name);
        auto entry_name = tag_desc.Tokens[3];
        auto flags = span(tag_desc.Tokens).subspan(4);
        bool has_no_sync = std::ranges::any_of(flags, [](auto&& f) { return f == "NoSync"; });
        bool has_owner_sync = std::ranges::any_of(flags, [](auto&& f) { return f == "OwnerSync"; });
        bool has_public_sync = std::ranges::any_of(flags, [](auto&& f) { return f == "PublicSync"; });
        bool has_persistent = std::ranges::any_of(flags, [](auto&& f) { return f == "Persistent"; });
        auto sync = has_public_sync ? EntityHolderEntrySync::PublicSync : (has_owner_sync ? EntityHolderEntrySync::OwnerSync : EntityHolderEntrySync::NoSync);

        if (!ctx.Meta.IsValidEntityType(holder_entity_hname)) {
            throw MetadataBakerException("Invalid EntityHolder codegen tag: unknown holder entity type", tag_desc.SourceFile, tag_desc.LineNumber, holder_entity_name);
        }
        if (target == "Server" && (has_no_sync || has_owner_sync || has_public_sync)) {
            throw MetadataBakerException("Invalid EntityHolder codegen tag: sync flags cannot be used with Server target", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        if (!(ctx.Target == "Common" || target == "Common" || target == ctx.Target)) {
            // Stub for entry property
            if (ctx.Meta.IsValidEntityType(holder_entity_name)) {
                vector<string> stub;
                stub.emplace_back("Stub");
                stub.emplace_back(holder_entity_name);
                stub.emplace_back("");
                stub.emplace_back(entry_name);

                for (const auto& flag : flags) {
                    stub.emplace_back(string(flag));
                }

                result_tag_entity_holder.emplace_back(stub);
            }

            continue;
        }

        if (!ctx.Meta.IsValidEntityType(target_entity_hname)) {
            throw MetadataBakerException("Invalid EntityHolder codegen tag: unknown target entity type", tag_desc.SourceFile, tag_desc.LineNumber, target_entity_name);
        }
        if (static_cast<int32_t>(has_no_sync) + static_cast<int32_t>(has_owner_sync) + static_cast<int32_t>(has_public_sync) > 1) {
            throw MetadataBakerException("Invalid EntityHolder codegen tag: sync flags invalid mixture", tag_desc.SourceFile, tag_desc.LineNumber);
        }
        if (target == "Common" && !has_owner_sync && !has_public_sync && !has_no_sync) {
            throw MetadataBakerException("Invalid EntityHolder codegen tag: Common target requires sync flag", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        const auto& target_entity_type = ctx.Meta.GetEntityType(target_entity_hname);

        if (target_entity_type.Exported) {
            throw MetadataBakerException("Invalid EntityHolder codegen tag: cannot hold exported entity type", tag_desc.SourceFile, tag_desc.LineNumber, target_entity_name);
        }

        ctx.Meta.RegsiterEntityHolderEntry(holder_entity_name, target_entity_name, entry_name, sync, has_persistent);

        auto tokens = vec_transform(tag_desc.Tokens, [](auto&& e) -> string { return string(e); });
        result_tag_entity_holder.emplace_back(tokens);
    }

    ctx.ResultTags["EntityHolder"] = std::move(result_tag_entity_holder);
}

void MetadataBaker::ParseFixedType(TagsParsingContext& ctx) const
{
    FO_STACK_TRACE_ENTRY();

    vector<vector<string>> result_tag_fixed_type;

    for (const auto& tag_desc : ctx.CodeGenTags["FixedType"]) {
        if (tag_desc.Tokens.size() < 2) {
            throw MetadataBakerException("Invalid FixedType codegen tag: insufficient parameters", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        auto target = tag_desc.Tokens[0];
        auto name = tag_desc.Tokens[1];

        if (!(ctx.Target == "Common" || target == "Common" || target == ctx.Target)) {
            ctx.OtherEntityTypes.emplace(name);
            continue;
        }
        if (tag_desc.Tokens.size() != 2) {
            throw MetadataBakerException("Invalid FixedType codegen tag: flags are not supported", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }

        hstring hname = ctx.Meta.Hashes.ToHashedString(name);

        if (ctx.Meta.IsValidEntityType(hname) || ctx.Meta.IsFixedType(hname)) {
            throw MetadataBakerException("Invalid FixedType codegen tag: duplicate fixed type", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (ctx.Meta.IsValidBaseType(name)) {
            throw MetadataBakerException("Invalid FixedType codegen tag: type name conflict with another type", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (ctx.Meta.IsValidBaseType(strex("{}Property", name))) {
            throw MetadataBakerException("Invalid FixedType codegen tag: property enum conflict with another type", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }

        ctx.Meta.RegisterFixedType(name, false);

        auto tokens = vec_transform(tag_desc.Tokens, [](auto&& e) -> string { return string(e); });
        tokens.erase(tokens.begin()); // Remove target
        result_tag_fixed_type.emplace_back(tokens);
    }

    ctx.ResultTags["FixedType"] = std::move(result_tag_fixed_type);
}

void MetadataBaker::ParseValueType(TagsParsingContext& ctx) const
{
    FO_STACK_TRACE_ENTRY();

    vector<vector<string>> result_tag_value_type;

    for (const auto& tag_desc : ctx.CodeGenTags["ValueType"]) {
        if (tag_desc.Tokens.size() < 7) {
            throw MetadataBakerException("Invalid ValueType codegen tag: insufficient parameters", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        auto target = tag_desc.Tokens[0];

        if (target != "Common" && target != "Server" && target != "Client") {
            throw MetadataBakerException("Invalid ValueType codegen tag: invalid target", tag_desc.SourceFile, tag_desc.LineNumber, target);
        }

        if (target != "Common" && target != ctx.Target) {
            continue;
        }

        auto type_name = tag_desc.Tokens[1];

        if (tag_desc.Tokens[2] != "Layout" || tag_desc.Tokens[3] != "=") {
            throw MetadataBakerException("Invalid ValueType codegen tag: expected 'Layout ='", tag_desc.SourceFile, tag_desc.LineNumber, type_name);
        }
        if (ctx.Meta.IsValidBaseType(type_name)) {
            throw MetadataBakerException("Invalid ValueType codegen tag: duplicate type name", tag_desc.SourceFile, tag_desc.LineNumber, type_name);
        }

        vector<pair<string, string>> layout_entries;
        vector<string> result_entry;
        unordered_set<string> field_names;
        result_entry.emplace_back(type_name);

        size_t tok_index = 4;
        while (tok_index < tag_desc.Tokens.size()) {
            size_t type_begin = tok_index;

            while (tok_index < tag_desc.Tokens.size() && tag_desc.Tokens[tok_index] != "-") {
                tok_index++;
            }
            if (tok_index == type_begin || tok_index >= tag_desc.Tokens.size()) {
                throw MetadataBakerException("Invalid ValueType codegen tag: expected 'type-field' layout entry", tag_desc.SourceFile, tag_desc.LineNumber, type_name);
            }

            ComplexTypeDesc field_type;
            size_t type_tokens = 0;

            try {
                std::tie(field_type, type_tokens) = ctx.Meta.ResolveComplexType(span(tag_desc.Tokens).subspan(type_begin, tok_index - type_begin));
            }
            catch (TypeResolveException& ex) {
                throw MetadataBakerException("Invalid ValueType codegen tag: cannot resolve field type", tag_desc.SourceFile, tag_desc.LineNumber, ex.what());
            }
            if (type_tokens != tok_index - type_begin) {
                throw MetadataBakerException("Invalid ValueType codegen tag: invalid field type", tag_desc.SourceFile, tag_desc.LineNumber, type_name);
            }

            if (field_type.Kind != ComplexTypeKind::Simple || field_type.IsMutable) {
                throw MetadataBakerException("Invalid ValueType codegen tag: field type must be a plain value", tag_desc.SourceFile, tag_desc.LineNumber, type_name);
            }
            if (!field_type.BaseType.IsPrimitive && !field_type.BaseType.IsEnum && !field_type.BaseType.IsSimpleStruct && !field_type.BaseType.IsHashedString) {
                throw MetadataBakerException("Invalid ValueType codegen tag: unsupported field type", tag_desc.SourceFile, tag_desc.LineNumber, type_name);
            }

            string merged_type = string(field_type.BaseType.Name);
            tok_index++;

            if (tok_index >= tag_desc.Tokens.size()) {
                throw MetadataBakerException("Invalid ValueType codegen tag: missing field name", tag_desc.SourceFile, tag_desc.LineNumber, type_name);
            }

            auto field_name = tag_desc.Tokens[tok_index++];

            if (field_name.empty() || field_name == ",") {
                throw MetadataBakerException("Invalid ValueType codegen tag: invalid field name", tag_desc.SourceFile, tag_desc.LineNumber, type_name);
            }
            if (!field_names.emplace(field_name).second) {
                throw MetadataBakerException("Invalid ValueType codegen tag: duplicate field name", tag_desc.SourceFile, tag_desc.LineNumber, field_name);
            }

            layout_entries.emplace_back(string(field_name), merged_type);
            result_entry.emplace_back(field_name);
            result_entry.emplace_back(merged_type);

            if (tok_index < tag_desc.Tokens.size()) {
                if (tag_desc.Tokens[tok_index] != "+") {
                    throw MetadataBakerException("Invalid ValueType codegen tag: expected '+' between layout entries", tag_desc.SourceFile, tag_desc.LineNumber, type_name);
                }

                tok_index++;
            }
        }

        if (layout_entries.empty()) {
            throw MetadataBakerException("Invalid ValueType codegen tag: no fields in layout", tag_desc.SourceFile, tag_desc.LineNumber, type_name);
        }

        vector<pair<string_view, string_view>> layout;
        layout.reserve(layout_entries.size());

        for (const auto& [field_name, field_type] : layout_entries) {
            layout.emplace_back(field_name, field_type);
        }

        ctx.Meta.RegisterValueType(type_name);
        ctx.Meta.RegisterValueTypeLayout(type_name, layout);
        result_tag_value_type.emplace_back(std::move(result_entry));
    }

    ctx.ResultTags["ValueType"] = std::move(result_tag_value_type);
}

void MetadataBaker::ParseRefType(TagsParsingContext& ctx) const
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& tag_desc : ctx.CodeGenTags["RefType"]) {
        if (tag_desc.Tokens.size() < 2) {
            throw MetadataBakerException("Invalid RefType codegen tag: insufficient parameters", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        auto target = tag_desc.Tokens[0];

        if (target != "Common" && target != "Server" && target != "Client") {
            throw MetadataBakerException("Invalid RefType codegen tag: invalid target", tag_desc.SourceFile, tag_desc.LineNumber, target);
        }

        auto type_name = tag_desc.Tokens[1];

        if (tag_desc.Tokens.size() != 2) {
            if (tag_desc.Tokens.size() >= 4 && tag_desc.Tokens[2] == "Layout" && tag_desc.Tokens[3] == "=") {
                throw MetadataBakerException("Invalid RefType codegen tag: Layout is no longer supported, declare fields via Property tags", tag_desc.SourceFile, tag_desc.LineNumber, type_name);
            }

            throw MetadataBakerException("Invalid RefType codegen tag: unexpected parameters", tag_desc.SourceFile, tag_desc.LineNumber, type_name);
        }
        if (ctx.Meta.IsValidBaseType(type_name) || ctx.RefTypes.contains(string(type_name))) {
            throw MetadataBakerException("Invalid RefType codegen tag: duplicate type name", tag_desc.SourceFile, tag_desc.LineNumber, type_name);
        }

        auto& ref_type = ctx.RefTypes[string(type_name)];
        ref_type.Target = string(target);
        ref_type.SourceFile = tag_desc.SourceFile;
        ref_type.LineNumber = tag_desc.LineNumber;
        ctx.RefTypeRegistrationOrder.emplace_back(type_name);

        ctx.Meta.RegisterRefType(type_name);
    }
}

void MetadataBaker::ParseProperty(TagsParsingContext& ctx) const
{
    FO_STACK_TRACE_ENTRY();

    vector<vector<string>> result_tag_property;
    vector<vector<string>> result_tag_ref_type;

    // Pass 1: detect Component declarations for both entity and RefType properties so the order of
    // tags inside a script doesn't matter (a `Foo.Bar` field can appear before its `Foo Component`
    // marker in source code).
    for (const auto& tag_desc : ctx.CodeGenTags["Property"]) {
        if (tag_desc.Tokens.size() < 4) {
            throw MetadataBakerException("Invalid Property codegen tag: insufficient parameters", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        auto entity_name = tag_desc.Tokens[0];
        auto target = tag_desc.Tokens[1];

        if (ctx.OtherEntityTypes.contains(entity_name)) {
            continue;
        }
        if (target != "Common" && target != "Server" && target != "Client") {
            continue; // Re-validated in pass 2
        }

        ComplexTypeDesc type;
        size_t type_tokens = 0;

        try {
            std::tie(type, type_tokens) = ctx.Meta.ResolveComplexType(span(tag_desc.Tokens).subspan(2));
        }
        catch (TypeResolveException&) {
            continue; // Re-validated in pass 2
        }

        size_t name_index = 2 + type_tokens;

        if (name_index >= tag_desc.Tokens.size()) {
            continue;
        }

        string name = string(tag_desc.Tokens[name_index]);
        size_t tok_index = name_index;

        if (tok_index < tag_desc.Tokens.size() - 2 && tag_desc.Tokens[tok_index + 1] == ".") {
            name += ".";
            name += tag_desc.Tokens[tok_index + 2];
            tok_index += 2;
        }

        auto flags = span(tag_desc.Tokens).subspan(tok_index + 1);
        bool is_component = std::ranges::any_of(flags, [](auto&& f) { return f == "Component"; });

        if (name.find('.') == string::npos && is_component) {
            if (!type.BaseType.IsBool || type.Kind != ComplexTypeKind::Simple) {
                throw MetadataBakerException("Invalid Property codegen tag: component property must be plain bool", tag_desc.SourceFile, tag_desc.LineNumber, name);
            }

            auto& entity_components = ctx.ComponentScopes[string(entity_name)];

            if (entity_components.contains(name)) {
                throw MetadataBakerException("Invalid Property codegen tag: duplicate component", tag_desc.SourceFile, tag_desc.LineNumber, name);
            }

            entity_components.emplace(name, string(target));
        }
    }

    // Pass 2: process RefType field properties.
    for (const auto& tag_desc : ctx.CodeGenTags["Property"]) {
        if (tag_desc.Tokens.size() < 4) {
            throw MetadataBakerException("Invalid Property codegen tag: insufficient parameters", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        auto entity_name = tag_desc.Tokens[0];
        auto target = tag_desc.Tokens[1];

        if (ctx.OtherEntityTypes.contains(entity_name)) {
            continue;
        }

        if (target != "Common" && target != "Server" && target != "Client") {
            throw MetadataBakerException("Invalid Property codegen tag: invalid target", tag_desc.SourceFile, tag_desc.LineNumber, target);
        }

        auto ref_type_it = ctx.RefTypes.find(string(entity_name));

        if (ref_type_it == ctx.RefTypes.end() && !ctx.Meta.IsValidEntityType(ctx.Meta.Hashes.ToHashedString(entity_name)) && !ctx.Meta.IsFixedType(entity_name)) {
            if (ctx.Meta.IsValidBaseType(entity_name)) {
                throw MetadataBakerException("Invalid Property codegen tag: only RefType supports script metadata properties", tag_desc.SourceFile, tag_desc.LineNumber, entity_name);
            }

            throw MetadataBakerException("Invalid Property codegen tag: unknown entity type", tag_desc.SourceFile, tag_desc.LineNumber, entity_name);
        }

        if (ref_type_it == ctx.RefTypes.end()) {
            continue;
        }

        auto& ref_type = ref_type_it->second;

        if (target != ref_type.Target) {
            throw MetadataBakerException("Invalid Property codegen tag: RefType field target must match RefType target", tag_desc.SourceFile, tag_desc.LineNumber, entity_name);
        }

        // Fields are collected for every target so off-target metadata also has the layout — its
        // properties get registered as IsServerOnly/IsClientOnly and disabled at runtime, but they
        // still need a known layout for serialization.

        ComplexTypeDesc type;
        size_t type_tokens = 0;

        try {
            std::tie(type, type_tokens) = ctx.Meta.ResolveComplexType(span(tag_desc.Tokens).subspan(2));
        }
        catch (TypeResolveException& ex) {
            throw MetadataBakerException("Invalid Property codegen tag: cannot resolve property type", tag_desc.SourceFile, tag_desc.LineNumber, ex.what());
        }

        size_t tok_index = 2 + type_tokens;
        string name = string(tag_desc.Tokens[tok_index]);

        if (tok_index < tag_desc.Tokens.size() - 2 && tag_desc.Tokens[tok_index + 1] == ".") {
            name += ".";
            name += tag_desc.Tokens[tok_index + 2];
            tok_index += 2;
        }

        auto flags = span(tag_desc.Tokens).subspan(tok_index + 1);
        bool is_component = std::ranges::any_of(flags, [](auto&& f) { return f == "Component"; });

        if (type.IsMutable) {
            throw MetadataBakerException("Invalid Property codegen tag: RefType field type can't be ref", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (type.Kind == ComplexTypeKind::Callback) {
            throw MetadataBakerException("Invalid Property codegen tag: RefType callback fields are unsupported", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (type.BaseType.IsEntity && !type.BaseType.IsFixedType && !type.BaseType.IsEntityProto) {
            throw MetadataBakerException("Invalid Property codegen tag: RefType entity fields must be fixed or proto references", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (type.KeyType.has_value() && type.KeyType->IsEntity) {
            throw MetadataBakerException("Invalid Property codegen tag: RefType entity dict keys are unsupported", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }

        if (auto dot_pos = name.find('.'); dot_pos != string::npos) {
            if (is_component) {
                throw MetadataBakerException("Invalid Property codegen tag: component property cannot be nested", tag_desc.SourceFile, tag_desc.LineNumber, name);
            }

            auto component_name = name.substr(0, dot_pos);
            auto entity_components_it = ctx.ComponentScopes.find(string(entity_name));

            if (entity_components_it == ctx.ComponentScopes.end() || !entity_components_it->second.contains(component_name)) {
                throw MetadataBakerException("Invalid Property codegen tag: RefType component is not declared", tag_desc.SourceFile, tag_desc.LineNumber, name);
            }
        }

        for (const auto& flag : flags) {
            if (flag != "Component") {
                throw MetadataBakerException("Invalid Property codegen tag: RefType fields only allow the 'Component' flag", tag_desc.SourceFile, tag_desc.LineNumber, name);
            }
        }

        if (std::ranges::any_of(ref_type.Fields, [&](const auto& entry) { return entry.Name == name; })) {
            throw MetadataBakerException("Invalid Property codegen tag: duplicate RefType field name", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }

        string merged_type = strex("").join(span(tag_desc.Tokens).subspan(2, type_tokens));
        ref_type.Fields.emplace_back(RefTypeFieldState {.Name = name, .Type = merged_type, .Flags = vector<string>(flags.begin(), flags.end())});
    }

    FO_VERIFY_AND_THROW(ctx.RefTypeRegistrationOrder.size() == ctx.RefTypes.size(), "RefType registration order does not cover every parsed RefType", ctx.RefTypeRegistrationOrder.size(), ctx.RefTypes.size());

    for (const auto& ref_type_name : ctx.RefTypeRegistrationOrder) {
        auto& ref_type = ctx.RefTypes.at(ref_type_name);

        if (ref_type.Fields.empty()) {
            throw MetadataBakerException("Invalid RefType codegen tag: no fields declared via Property", ref_type.SourceFile, ref_type.LineNumber, ref_type_name);
        }

        vector<vector<string_view>> layout;
        vector<string> result_entry;
        result_entry.emplace_back(ref_type_name);
        layout.reserve(ref_type.Fields.size());

        for (const auto& field : ref_type.Fields) {
            vector<string_view> field_tokens;
            field_tokens.reserve(2 + field.Flags.size());
            field_tokens.emplace_back(field.Name);
            field_tokens.emplace_back(field.Type);

            for (const auto& flag : field.Flags) {
                field_tokens.emplace_back(flag);
            }

            layout.emplace_back(std::move(field_tokens));

            result_entry.emplace_back(field.Name);
            result_entry.emplace_back(field.Type);
            result_entry.emplace_back(strex("{}", field.Flags.size()).str());

            for (const auto& flag : field.Flags) {
                result_entry.emplace_back(flag);
            }
        }

        ctx.Meta.RegisterRefTypeLayout(ref_type_name, layout);
        result_tag_ref_type.emplace_back(std::move(result_entry));
    }

    if (!result_tag_ref_type.empty()) {
        ctx.ResultTags["RefType"] = std::move(result_tag_ref_type);
    }

    for (const auto& tag_desc : ctx.CodeGenTags["Property"]) {
        if (tag_desc.Tokens.size() < 4) {
            throw MetadataBakerException("Invalid Property codegen tag: insufficient parameters", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        auto entity_name = tag_desc.Tokens[0];
        auto target = tag_desc.Tokens[1];

        if (ctx.OtherEntityTypes.contains(entity_name)) {
            continue;
        }
        if (ctx.RefTypes.contains(string(entity_name))) {
            continue;
        }

        if (!ctx.Meta.IsValidEntityType(ctx.Meta.Hashes.ToHashedString(entity_name)) && !ctx.Meta.IsFixedType(entity_name)) {
            throw MetadataBakerException("Invalid Property codegen tag: unknown entity type", tag_desc.SourceFile, tag_desc.LineNumber, entity_name);
        }

        if (target != "Common" && target != "Server" && target != "Client") {
            throw MetadataBakerException("Invalid Property codegen tag: invalid target", tag_desc.SourceFile, tag_desc.LineNumber, target);
        }

        ComplexTypeDesc type;
        size_t type_tokens = 0;

        try {
            std::tie(type, type_tokens) = ctx.Meta.ResolveComplexType(span(tag_desc.Tokens).subspan(2));
        }
        catch (TypeResolveException& ex) {
            throw MetadataBakerException("Invalid Property codegen tag: cannot resolve property type", tag_desc.SourceFile, tag_desc.LineNumber, ex.what());
        }

        if (type.IsMutable) {
            throw MetadataBakerException("Invalid Property codegen tag: property type can't be ref", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        size_t tok_index = 2 + type_tokens;
        string name = string(tag_desc.Tokens[tok_index]);

        if (tok_index < tag_desc.Tokens.size() - 2 && tag_desc.Tokens[tok_index + 1] == ".") {
            name += ".";
            name += tag_desc.Tokens[tok_index + 2];
            tok_index += 2;
        }

        auto registrator = ctx.Meta.GetPropertyRegistratorForEdit(entity_name);
        auto flags = span(tag_desc.Tokens).subspan(tok_index + 1);
        bool is_component = std::ranges::any_of(flags, [](auto&& f) { return f == "Component"; });

        if (auto dot_pos = name.find('.'); dot_pos != string::npos) {
            auto component_name = name.substr(0, dot_pos);
            auto entity_it = ctx.ComponentScopes.find(string(entity_name));

            if (is_component) {
                throw MetadataBakerException("Invalid Property codegen tag: component property cannot be nested", tag_desc.SourceFile, tag_desc.LineNumber, name);
            }
            if (entity_it == ctx.ComponentScopes.end()) {
                throw MetadataBakerException("Invalid Property codegen tag: unknown component for property", tag_desc.SourceFile, tag_desc.LineNumber, name);
            }

            auto component_it = entity_it->second.find(component_name);

            if (component_it == entity_it->second.end()) {
                throw MetadataBakerException("Invalid Property codegen tag: unknown component for property", tag_desc.SourceFile, tag_desc.LineNumber, name);
            }
            if ((component_it->second == "Server" && target != "Server") || (component_it->second == "Client" && target != "Client")) {
                throw MetadataBakerException("Invalid Property codegen tag: property target is incompatible with component target", tag_desc.SourceFile, tag_desc.LineNumber, name, target, component_it->second);
            }
        }
        if (registrator->FindProperty(name)) {
            throw MetadataBakerException("Invalid Property codegen tag: duplicate property", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }

        bool is_common = target == "Common";
        bool is_client_only = target == "Client";
        bool is_owner_sync = std::ranges::any_of(flags, [](auto&& f) { return f == "OwnerSync"; });
        bool is_public_sync = std::ranges::any_of(flags, [](auto&& f) { return f == "PublicSync"; });
        bool is_no_sync = std::ranges::any_of(flags, [](auto&& f) { return f == "NoSync"; });
        bool is_mutable = std::ranges::any_of(flags, [](auto&& f) { return f == "Mutable"; });
        bool is_persistent = std::ranges::any_of(flags, [](auto&& f) { return f == "Persistent"; });
        bool is_virtual = std::ranges::any_of(flags, [](auto&& f) { return f == "Virtual"; });
        bool is_modifiable_by_client = std::ranges::any_of(flags, [](auto&& f) { return f == "ModifiableByClient"; });
        bool is_modifiable_by_any_client = std::ranges::any_of(flags, [](auto&& f) { return f == "ModifiableByAnyClient"; });
        bool is_null_getter_for_proto = std::ranges::any_of(flags, [](auto&& f) { return f == "NullGetterForProto"; });
        bool is_nullable = std::ranges::any_of(flags, [](auto&& f) { return f == "Nullable"; });
        bool is_synced = is_common && is_mutable && (is_owner_sync || is_public_sync);

        if (is_mutable && is_common && !is_virtual && !is_owner_sync && !is_public_sync && !is_no_sync) {
            throw MetadataBakerException("Invalid Property codegen tag: common mutable property must have sync type", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (is_virtual && (is_owner_sync || is_public_sync || is_no_sync)) {
            throw MetadataBakerException("Invalid Property codegen tag: synced property can't be virtual", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (static_cast<int32_t>(is_owner_sync) + static_cast<int32_t>(is_public_sync) + static_cast<int32_t>(is_no_sync) > 1) {
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
        if (is_nullable && !type.BaseType.IsFixedType && !type.BaseType.IsEntityProto) {
            throw MetadataBakerException("Invalid Property codegen tag: Nullable can only be used on FixedType or Proto entity properties", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }
        if (is_persistent && is_client_only) {
            throw MetadataBakerException("Invalid Property codegen tag: persistent property can't be client only", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }

        vector<string_view> tokens;
        tokens.emplace_back(entity_name);
        tokens.emplace_back(target);
        string merged_type = strex("").join(span(tag_desc.Tokens).subspan(2, type_tokens));
        tokens.emplace_back(merged_type);
        tokens.emplace_back(name);
        tokens.insert(tokens.end(), flags.begin(), flags.end());

        auto prop = registrator->RegisterProperty(span(tokens).subspan(1));
        string prop_enum_name = prop->IsInComponent() ? strex("{}_{}", prop->GetComponentName(), prop->GetNameWithoutComponent()).str() : string {prop->GetName()};
        ctx.Meta.RegisterEnumEntry(strex("{}Property", entity_name), prop_enum_name, numeric_cast<int32_t>(prop->GetRegIndex()));

        result_tag_property.emplace_back(vec_transform(tokens, [](auto&& e) -> string { return string(e); }));
    }

    ctx.ResultTags["Property"] = std::move(result_tag_property);
}

// Split any tag token whose last character is '?' into two tokens:
// the original token without the trailing '?' followed by a literal "?".
// This lets `///@ Event` / `///@ RemoteCall` declarations carry per-arg
// nullable markers (`Type? name`) without requiring `?` to be a token
// separator globally in strvex::tokenize.
static auto SplitTrailingQuestionMarks(span<const string_view> tokens) -> vector<string_view>
{
    vector<string_view> result;
    result.reserve(tokens.size());

    for (string_view tok : tokens) {
        if (tok.size() > 1 && tok.back() == '?') {
            result.emplace_back(tok.substr(0, tok.size() - 1));
            result.emplace_back(tok.substr(tok.size() - 1));
        }
        else {
            result.emplace_back(tok);
        }
    }

    return result;
}

void MetadataBaker::ParseEvent(TagsParsingContext& ctx) const
{
    FO_STACK_TRACE_ENTRY();

    vector<vector<string>> result_tag_event;

    for (const auto& tag_desc : ctx.CodeGenTags["Event"]) {
        auto tokens = SplitTrailingQuestionMarks(tag_desc.Tokens);

        if (tokens.size() < 5) {
            throw MetadataBakerException("Invalid Event codegen tag: insufficient parameters", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        auto target = tokens[0];

        if (!(ctx.Target == "Common" || target == "Common" || target == ctx.Target)) {
            continue;
        }

        auto entity_name = tokens[1];
        hstring entity_hname = ctx.Meta.Hashes.ToHashedString(entity_name);

        if (!ctx.Meta.IsValidEntityType(entity_hname)) {
            throw MetadataBakerException("Invalid Event codegen tag: invalid entity type", tag_desc.SourceFile, tag_desc.LineNumber, entity_hname);
        }

        EntityEventDesc event_desc;
        auto event_name = tokens[2];
        event_desc.Name = ctx.Meta.Hashes.ToHashedString(event_name);

        vector<string> tag_tokens;
        tag_tokens.emplace_back(entity_name);
        tag_tokens.emplace_back(event_name);

        if (tokens[3] != "(") {
            throw MetadataBakerException("Invalid Event codegen tag: expected '(' after event name", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        size_t cur_token = 4;

        while (true) {
            if (cur_token == 4 && tokens[cur_token] == ")") {
                cur_token++;
                break;
            }

            ComplexTypeDesc type;

            try {
                size_t type_tokens = 0;
                std::tie(type, type_tokens) = ctx.Meta.ResolveComplexType(span(tokens).subspan(cur_token));
                tag_tokens.emplace_back(strex(" ").join(span(tokens).subspan(cur_token, type_tokens)));
                cur_token += type_tokens;
            }
            catch (TypeResolveException& ex) {
                throw MetadataBakerException("Invalid Event codegen tag: cannot resolve arg type", tag_desc.SourceFile, tag_desc.LineNumber, ex.what());
            }

            bool nullable = false;

            if (cur_token < tokens.size() && tokens[cur_token] == "?") {
                nullable = true;
                cur_token++;
            }

            tag_tokens.emplace_back(nullable ? "?" : "");

            if (cur_token == tokens.size()) {
                throw MetadataBakerException("Invalid Event codegen tag: expected arg name after it's type", tag_desc.SourceFile, tag_desc.LineNumber);
            }

            auto name = tokens[cur_token];
            tag_tokens.emplace_back(name);
            event_desc.Args.emplace_back(string(name), std::move(type), nullable);
            cur_token++;

            if (cur_token == tokens.size() || (tokens[cur_token] != "," && tokens[cur_token] != ")")) {
                throw MetadataBakerException("Invalid Event codegen tag: expected ')' or ',' after arg", tag_desc.SourceFile, tag_desc.LineNumber);
            }

            if (tokens[cur_token++] == ")") {
                break;
            }
        }

        if (cur_token < tokens.size()) {
            throw MetadataBakerException("Invalid Event codegen tag: unexpected tokens after ')'", tag_desc.SourceFile, tag_desc.LineNumber);
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
        auto tokens = SplitTrailingQuestionMarks(tag_desc.Tokens);

        if (tokens.size() < 4) {
            throw MetadataBakerException("Invalid RemoteCall codegen tag: insufficient parameters", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        auto target = tokens[0];

        if (target != "Server" && target != "Client") {
            throw MetadataBakerException("Invalid RemoteCall codegen tag: expected 'Server' or 'Client' as target", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        bool inbound = target == ctx.Target;
        auto remote_call_name = tokens[1];

        if (tokens[2] != "(") {
            throw MetadataBakerException("Invalid RemoteCall codegen tag: expected '(' after remote call name", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        RemoteCallDesc recote_call_desc;
        recote_call_desc.Name = ctx.Meta.Hashes.ToHashedString(remote_call_name);

        vector<string> tag_tokens;
        tag_tokens.emplace_back(remote_call_name);
        tag_tokens.emplace_back(strvex(tag_desc.SourceFile).extract_file_name());
        tag_tokens.emplace_back(inbound ? "In" : "Out");

        size_t cur_token = 3;

        while (true) {
            if (cur_token == 3 && tokens[cur_token] == ")") {
                break;
            }

            ComplexTypeDesc type;

            try {
                size_t type_tokens = 0;
                std::tie(type, type_tokens) = ctx.Meta.ResolveComplexType(span(tokens).subspan(cur_token));
                tag_tokens.emplace_back(strex(" ").join(span(tokens).subspan(cur_token, type_tokens)));
                cur_token += type_tokens;
            }
            catch (TypeResolveException& ex) {
                throw MetadataBakerException("Invalid RemoteCall codegen tag: cannot resolve arg type", tag_desc.SourceFile, tag_desc.LineNumber, ex.what());
            }

            bool nullable = false;

            if (cur_token < tokens.size() && tokens[cur_token] == "?") {
                nullable = true;
                cur_token++;
            }

            tag_tokens.emplace_back(nullable ? "?" : "");

            if (cur_token == tokens.size()) {
                throw MetadataBakerException("Invalid RemoteCall codegen tag: expected arg name after it's type", tag_desc.SourceFile, tag_desc.LineNumber);
            }

            auto name = tokens[cur_token];
            tag_tokens.emplace_back(name);
            recote_call_desc.Args.emplace_back(string(name), std::move(type), nullable);
            cur_token++;

            if (cur_token == tokens.size() || (tokens[cur_token] != "," && tokens[cur_token] != ")")) {
                throw MetadataBakerException("Invalid RemoteCall codegen tag: expected ')' or ',' after arg", tag_desc.SourceFile, tag_desc.LineNumber);
            }

            if (tokens[cur_token++] == ")") {
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
    auto known_settings = ctx.Target == "Server" ? GetServerSettings() : GetClientSettings();

    auto resolve_setting_name = [&](const CodeGenTagDesc& tag_desc, string_view name) -> string {
        if (name.find('.') != string_view::npos) {
            return string(name);
        }

        vector<string> matches;

        for (const auto& setting_name : known_settings) {
            if (setting_name == name || setting_name.ends_with(strex(".{}", name))) {
                matches.emplace_back(setting_name);
            }
        }

        if (matches.empty()) {
            return string(name);
        }

        if (matches.size() != 1) {
            throw MetadataBakerException("Invalid Setting codegen tag: ambiguous setting name", tag_desc.SourceFile, tag_desc.LineNumber, name);
        }

        return std::move(matches.front());
    };

    for (const auto& tag_desc : ctx.CodeGenTags["Setting"]) {
        if (tag_desc.Tokens.size() < 3) {
            throw MetadataBakerException("Invalid Setting codegen tag: insufficient parameters", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        auto target = tag_desc.Tokens[0];

        if (!(target == "Common" || target == "Server" || target == "Client")) {
            throw MetadataBakerException("Invalid Setting codegen tag: expected 'Common', 'Server' or 'Client' as target", tag_desc.SourceFile, tag_desc.LineNumber, target);
        }

        if (!(target == "Common" || target == ctx.Target)) {
            continue;
        }

        auto type_str = tag_desc.Tokens[1];
        string raw_name;

        for (size_t i = 2; i < tag_desc.Tokens.size(); i++) {
            auto token = tag_desc.Tokens[i];

            if (token == ".") {
                if (!raw_name.empty() && raw_name.back() != '.') {
                    raw_name += '.';
                }

                continue;
            }

            if (!raw_name.empty() && raw_name.back() != '.') {
                raw_name += '.';
            }

            raw_name += token;
        }

        string name = resolve_setting_name(tag_desc, raw_name);

        if (!ctx.Meta.IsValidBaseType(type_str)) {
            throw MetadataBakerException("Invalid Setting codegen tag: invalid type", tag_desc.SourceFile, tag_desc.LineNumber, type_str);
        }

        auto type = ctx.Meta.GetBaseType(type_str);

        if (!type.IsPrimitive && !type.IsEnum && !type.IsString) {
            throw MetadataBakerException("Invalid Setting codegen tag: type must be primitive or enum or string or any", tag_desc.SourceFile, tag_desc.LineNumber, type.Name);
        }

        vector<string> tag_tokens;
        tag_tokens.emplace_back(std::move(name));
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

        auto merge_dotted_tokens = [&](const auto tokens) -> string {
            if (tokens.empty()) {
                throw MetadataBakerException("Invalid MigrationRule codegen tag: empty rule argument", tag_desc.SourceFile, tag_desc.LineNumber);
            }

            string value;
            bool expect_token = true;

            for (auto token : tokens) {
                if (token == ".") {
                    if (expect_token) {
                        throw MetadataBakerException("Invalid MigrationRule codegen tag: malformed dotted name", tag_desc.SourceFile, tag_desc.LineNumber);
                    }

                    value += '.';
                    expect_token = true;
                    continue;
                }

                if (!expect_token) {
                    throw MetadataBakerException("Invalid MigrationRule codegen tag: too many rule arguments", tag_desc.SourceFile, tag_desc.LineNumber);
                }

                value += token;
                expect_token = false;
            }

            if (expect_token) {
                throw MetadataBakerException("Invalid MigrationRule codegen tag: malformed dotted name", tag_desc.SourceFile, tag_desc.LineNumber);
            }

            return value;
        };

        auto last_arg_begin = tag_desc.Tokens.size() - 1;

        while (last_arg_begin > 2 && tag_desc.Tokens[last_arg_begin - 1] == ".") {
            last_arg_begin -= 2;
        }

        if (last_arg_begin <= 2) {
            throw MetadataBakerException("Invalid MigrationRule codegen tag: insufficient parameters", tag_desc.SourceFile, tag_desc.LineNumber);
        }

        string rule_name = string(tag_desc.Tokens[0]);
        string extra_info = string(tag_desc.Tokens[1]);
        string target = merge_dotted_tokens(span(tag_desc.Tokens).subspan(2, last_arg_begin - 2));
        string replacement = merge_dotted_tokens(span(tag_desc.Tokens).subspan(last_arg_begin));

        ctx.Meta.RegisterMigrationRule(rule_name, extra_info, target, replacement);

        vector<string> tag_tokens;
        tag_tokens.emplace_back(rule_name);
        tag_tokens.emplace_back(extra_info);
        tag_tokens.emplace_back(target);
        tag_tokens.emplace_back(replacement);
        result_tag_migration_rule.emplace_back(std::move(tag_tokens));
    }

    ctx.ResultTags["MigrationRule"] = std::move(result_tag_migration_rule);
}

FO_END_NAMESPACE
