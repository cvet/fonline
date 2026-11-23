// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "Baker.h"

#if FO_SPARK_PARTICLES
namespace SPK
{
    class SPKContext;
}
#endif

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ParticleBakerException);

class ParticleBaker final : public BaseBaker
{
public:
    static constexpr string_view_nt NAME {"Particle"};

    explicit ParticleBaker(shared_ptr<BakingContext> ctx);
    ParticleBaker(const ParticleBaker&) = delete;
    ParticleBaker(ParticleBaker&&) noexcept = delete;
    auto operator=(const ParticleBaker&) = delete;
    auto operator=(ParticleBaker&&) noexcept = delete;
    ~ParticleBaker() override;

    [[nodiscard]] auto GetName() const -> string_view override { return NAME; }
    [[nodiscard]] auto GetOrder() const -> int32_t override { return 5; }

    void BakeFiles(const FileCollection& files, string_view target_path) const override;

private:
    auto CollectResourceDependencies(const FileCollection& files, string_view path, const ResourceProviderCallback& providers) const -> vector<string> override;

#if FO_SPARK_PARTICLES
    void BakeSparkFile(const File& file) const;
#endif
#if FO_EFFEKSEER_PARTICLES
    void BakeEffekseerFiles(const_span<File> files) const;
#endif

#if FO_SPARK_PARTICLES
    unique_ptr<SPK::SPKContext> _sparkContext;
#endif
};

FO_END_NAMESPACE
