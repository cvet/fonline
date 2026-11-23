// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "Baker.h"
#include "FileSystem.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(AngelScriptBakerException);

class AngelScriptBaker final : public BaseBaker
{
public:
    static constexpr string_view_nt NAME {"AngelScript"};

    explicit AngelScriptBaker(shared_ptr<BakingContext> ctx);
    AngelScriptBaker(const AngelScriptBaker&) = delete;
    AngelScriptBaker(AngelScriptBaker&&) noexcept = delete;
    auto operator=(const AngelScriptBaker&) = delete;
    auto operator=(AngelScriptBaker&&) noexcept = delete;
    ~AngelScriptBaker() override;

    [[nodiscard]] auto GetName() const -> string_view override { return NAME; }
    [[nodiscard]] auto GetOrder() const -> int32_t override { return 4; }

    void BakeFiles(const FileCollection& files, string_view target_path) const override;
};

FO_END_NAMESPACE

#endif
