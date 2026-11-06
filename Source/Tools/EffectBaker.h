// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "Baker.h"
#include "FileSystem.h"

namespace glslang
{
    class TIntermediate;
}

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(EffectBakerException);

class EffectBaker final : public BaseBaker
{
public:
    static constexpr string_view_nt NAME {"Effect"};

    struct SdlStageSlots;

    explicit EffectBaker(shared_ptr<BakingContext> ctx);
    EffectBaker(const EffectBaker&) = delete;
    EffectBaker(EffectBaker&&) noexcept = delete;
    auto operator=(const EffectBaker&) = delete;
    auto operator=(EffectBaker&&) noexcept = delete;
    ~EffectBaker() override;

    [[nodiscard]] auto GetName() const -> string_view override { return NAME; }
    [[nodiscard]] auto GetOrder() const -> int32_t override { return 4; }

    void BakeFiles(const FileCollection& files, string_view target_path) const override;

private:
    auto CollectResourceDependencies(const FileCollection& files, string_view path, const ResourceProviderCallback& providers) const -> vector<string> override;

    void BakeShaderProgram(string_view fname, string_view content) const;
    void BakeShaderStage(string_view fname_wo_ext, const glslang::TIntermediate& intermediate, const SdlStageSlots& sdl_slots, bool is_vertex) const;
};

FO_END_NAMESPACE
