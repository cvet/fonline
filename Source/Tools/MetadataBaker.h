// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "Baker.h"
#include "FileSystem.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(MetadataBakerException);

class MetadataBaker final : public BaseBaker
{
public:
    static constexpr string_view_nt NAME {"Metadata"};

    explicit MetadataBaker(shared_ptr<BakingContext> ctx);
    MetadataBaker(const MetadataBaker&) = delete;
    MetadataBaker(MetadataBaker&&) noexcept = delete;
    auto operator=(const MetadataBaker&) = delete;
    auto operator=(MetadataBaker&&) noexcept = delete;
    ~MetadataBaker() override;

    [[nodiscard]] auto GetName() const -> string_view override { return NAME; }
    [[nodiscard]] auto GetOrder() const -> int32_t override { return 1; }

    void BakeFiles(const FileCollection& files, string_view target_path) const override;

private:
    struct CodeGenTagDesc
    {
        string SourceFile {};
        size_t LineNumber {};
        vector<string_view> Tokens {};
    };

    struct RefTypeFieldState
    {
        string Name {};
        string Type {};
        vector<string> Flags {};
    };

    struct RefTypeState
    {
        string Target {};
        string SourceFile {};
        size_t LineNumber {};
        vector<RefTypeFieldState> Fields {};
    };

    struct TagsParsingContext
    {
        EngineMetadata Meta {[] { }};
        unordered_set<string> OtherEntityTypes {};
        unordered_map<string, unordered_map<string, string>> ComponentScopes {};
        unordered_map<string, RefTypeState> RefTypes {};
        vector<string> RefTypeRegistrationOrder {};
        vector<unique_ptr<string>> NormalizedLines {};
        // Keyed by the section-name constants and by tag names pointing into the parsed files, both of which
        // outlive this context
        unordered_map<string_view, vector<CodeGenTagDesc>> CodeGenTags {};
        map<string_view, vector<vector<string>>> ResultTags {};
        string_view Target {};
    };

    auto BakeMetadata(const vector<File>& files, string_view target) const -> vector<uint8_t>;
    auto MakeMetadataVersion(const TagsParsingContext& ctx) const -> string;
    void ParseEnum(TagsParsingContext& ctx) const;
    void ParseEntity(TagsParsingContext& ctx) const;
    void ParseFixedType(TagsParsingContext& ctx) const;
    void ParseValueType(TagsParsingContext& ctx) const;
    void ParseRefType(TagsParsingContext& ctx) const;
    void ParseEntityHolder(TagsParsingContext& ctx) const;
    void ParseProperty(TagsParsingContext& ctx) const;
    void ParseEvent(TagsParsingContext& ctx) const;
    void ParseRemoteCall(TagsParsingContext& ctx) const;
    void ParseSetting(TagsParsingContext& ctx) const;
    void ParseMigrationRule(TagsParsingContext& ctx) const;
};

FO_END_NAMESPACE
