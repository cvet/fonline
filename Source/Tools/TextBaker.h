// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "Baker.h"
#include "FileSystem.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(TextBakerException);

class TextBaker final : public BaseBaker
{
public:
    static constexpr string_view_nt NAME {"Text"};

    explicit TextBaker(shared_ptr<BakingContext> ctx);
    TextBaker(const TextBaker&) = delete;
    TextBaker(TextBaker&&) noexcept = delete;
    auto operator=(const TextBaker&) = delete;
    auto operator=(TextBaker&&) noexcept = delete;
    ~TextBaker() override;

    [[nodiscard]] auto GetName() const -> string_view override { return NAME; }
    [[nodiscard]] auto GetOrder() const -> int32_t override { return 4; }

    void BakeFiles(const FileCollection& files, string_view target_path) const override;
};

FO_END_NAMESPACE
