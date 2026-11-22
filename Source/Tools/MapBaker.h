// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "Baker.h"
#include "FileSystem.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(MapBakerException);

class MapBaker final : public BaseBaker
{
public:
    static constexpr string_view_nt NAME {"Map"};

    explicit MapBaker(shared_ptr<BakingContext> ctx);
    MapBaker(const MapBaker&) = delete;
    MapBaker(MapBaker&&) noexcept = delete;
    auto operator=(const MapBaker&) = delete;
    auto operator=(MapBaker&&) noexcept = delete;
    ~MapBaker() override;

    [[nodiscard]] auto GetName() const -> string_view override { return NAME; }
    [[nodiscard]] auto GetOrder() const -> int32_t override { return 8; }

    void BakeFiles(const FileCollection& files, string_view target_path) const override;

private:
    static auto ResolveMapNames(const File& file) -> vector<string>;
};

FO_END_NAMESPACE
