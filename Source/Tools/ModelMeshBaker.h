// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_ENABLE_3D

#include "Baker.h"
#include "FileSystem.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ModelMeshBakerException);

class ModelMeshBaker final : public BaseBaker
{
public:
    static constexpr string_view_nt NAME {"ModelMesh"};

    explicit ModelMeshBaker(shared_ptr<BakingContext> ctx);
    ModelMeshBaker(const ModelMeshBaker&) = delete;
    ModelMeshBaker(ModelMeshBaker&&) noexcept = delete;
    auto operator=(const ModelMeshBaker&) = delete;
    auto operator=(ModelMeshBaker&&) noexcept = delete;
    ~ModelMeshBaker() override;

    [[nodiscard]] auto GetName() const -> string_view override { return NAME; }
    [[nodiscard]] auto GetOrder() const -> int32_t override { return 4; }

    void BakeFiles(const FileCollection& files, string_view target_path) const override;

private:
    auto BakeFbxFile(string_view fname, const File& file) const -> vector<uint8_t>;
};

FO_END_NAMESPACE

#endif
