// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_ENABLE_3D

#include "Baker.h"
#include "FileSystem.h"
#include "ModelSourceLoader.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ModelInfoBakerException);

class ModelInfoBaker final : public BaseBaker
{
public:
    static constexpr string_view_nt NAME {"ModelInfo"};

    explicit ModelInfoBaker(shared_ptr<BakingContext> ctx, ModelSourceAssetCache::LoadCallback model_source_loader = {});
    ModelInfoBaker(const ModelInfoBaker&) = delete;
    ModelInfoBaker(ModelInfoBaker&&) noexcept = delete;
    auto operator=(const ModelInfoBaker&) = delete;
    auto operator=(ModelInfoBaker&&) noexcept = delete;
    ~ModelInfoBaker() override;

    [[nodiscard]] auto GetName() const -> string_view override { return NAME; }
    [[nodiscard]] auto GetOrder() const -> int32_t override { return 6; }

    void BakeFiles(const FileCollection& files, string_view target_path) const override;

private:
    auto CollectResourceDependencies(const FileCollection& files, string_view path, const ResourceProviderCallback& providers) const -> vector<string> override;

    ModelSourceAssetCache::LoadCallback _modelSourceLoader {};
};

FO_END_NAMESPACE

#endif
