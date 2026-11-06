// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "Baker.h"
#include "FileSystem.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ProtoBakerException);

class EngineMetadata;
class ScriptSystem;
class ProtoEntity;

class ProtoBaker final : public BaseBaker
{
public:
    static constexpr string_view_nt NAME {"Proto"};

    explicit ProtoBaker(shared_ptr<BakingContext> ctx);
    ProtoBaker(const ProtoBaker&) = delete;
    ProtoBaker(ProtoBaker&&) noexcept = delete;
    auto operator=(const ProtoBaker&) = delete;
    auto operator=(ProtoBaker&&) noexcept = delete;
    ~ProtoBaker() override;

    [[nodiscard]] auto GetName() const -> string_view override { return NAME; }
    [[nodiscard]] auto GetOrder() const -> int32_t override { return 7; }

    void BakeFiles(const FileCollection& files, string_view target_path) const override;

private:
    auto BakeProtoFiles(ptr<EngineMetadata> meta, nptr<const ScriptSystem> script_sys, const vector<File>& files, const set<string>& extra_hash_strings, nptr<set<string>> out_hash_strings) const -> vector<uint8_t>;
};

FO_END_NAMESPACE
