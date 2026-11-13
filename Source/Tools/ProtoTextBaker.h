// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "Baker.h"
#include "FileSystem.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ProtoTextBakerException);

class EngineMetadata;
class ScriptSystem;
class ProtoEntity;

class ProtoTextBaker final : public BaseBaker
{
public:
    static constexpr string_view_nt NAME {"ProtoText"};

    explicit ProtoTextBaker(shared_ptr<BakingContext> ctx);
    ProtoTextBaker(const ProtoTextBaker&) = delete;
    ProtoTextBaker(ProtoTextBaker&&) noexcept = delete;
    auto operator=(const ProtoTextBaker&) = delete;
    auto operator=(ProtoTextBaker&&) noexcept = delete;
    ~ProtoTextBaker() override;

    [[nodiscard]] auto GetName() const -> string_view override { return NAME; }
    [[nodiscard]] auto GetOrder() const -> int32_t override { return 6; }

    void BakeFiles(const FileCollection& files, string_view target_path) const override;
};

FO_END_NAMESPACE
