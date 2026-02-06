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

#pragma once

#include "Common.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "Baker.h"
#include "FileSystem.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(MetadataBakerException);

class MetadataBaker final : public BaseBaker
{
public:
    static constexpr string_view_nt NAME = "Metadata";

    explicit MetadataBaker(shared_ptr<BakingContext> ctx);
    MetadataBaker(const MetadataBaker&) = delete;
    MetadataBaker(MetadataBaker&&) noexcept = delete;
    auto operator=(const MetadataBaker&) = delete;
    auto operator=(MetadataBaker&&) noexcept = delete;
    ~MetadataBaker() override;

    [[nodiscard]] auto GetName() const -> string_view override { return NAME; }
    [[nodiscard]] auto GetOrder() const -> int32 override { return 1; }

    void BakeFiles(const FileCollection& files, string_view target_path) const override;

private:
    struct CodeGenTagDesc
    {
        string SourceFile {};
        size_t LineNumber {};
        vector<string_view> Tokens {};
    };

    struct TagsParsingContext
    {
        unique_ptr<EngineMetadata> Meta {};
        unordered_set<string> OtherEntityTypes {};
        unordered_map<string, vector<CodeGenTagDesc>> CodeGenTags {};
        map<string, vector<vector<string>>> ResultTags {};
        string_view Target {};
    };

    auto BakeMetadata(const vector<File>& files, string_view target) const -> vector<uint8>;
    void ParseEnum(TagsParsingContext& ctx) const;
    void ParseEntity(TagsParsingContext& ctx) const;
    void ParseEntityHolder(TagsParsingContext& ctx) const;
    void ParsePropertyComponent(TagsParsingContext& ctx) const;
    void ParseProperty(TagsParsingContext& ctx) const;
    void ParseEvent(TagsParsingContext& ctx) const;
    void ParseRemoteCall(TagsParsingContext& ctx) const;
    void ParseSetting(TagsParsingContext& ctx) const;
    void ParseMigrationRule(TagsParsingContext& ctx) const;
};

FO_END_NAMESPACE

#endif
