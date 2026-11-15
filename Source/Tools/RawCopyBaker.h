// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "Baker.h"
#include "FileSystem.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(RawCopyBakerException);

class RawCopyBaker final : public BaseBaker
{
public:
    static constexpr string_view_nt NAME {"RawCopy"};

    explicit RawCopyBaker(shared_ptr<BakingContext> ctx);
    RawCopyBaker(const RawCopyBaker&) = delete;
    RawCopyBaker(RawCopyBaker&&) noexcept = delete;
    auto operator=(const RawCopyBaker&) = delete;
    auto operator=(RawCopyBaker&&) noexcept = delete;
    ~RawCopyBaker() override;

    [[nodiscard]] auto GetName() const -> string_view override { return NAME; }
    [[nodiscard]] auto GetOrder() const -> int32_t override { return 4; }

    void BakeFiles(const FileCollection& files, string_view target_path) const override;

private:
    auto CollectResourceDependencies(const FileCollection& files, string_view path, const ResourceProviderCallback& providers) const -> vector<string> override;
};

FO_END_NAMESPACE
