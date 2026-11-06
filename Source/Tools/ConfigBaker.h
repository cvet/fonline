// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "Baker.h"
#include "FileSystem.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ConfigBakerException);

class ConfigBaker : public BaseBaker
{
public:
    static constexpr string_view_nt NAME {"Config"};

    explicit ConfigBaker(shared_ptr<BakingContext> ctx);
    ConfigBaker(const ConfigBaker&) = delete;
    ConfigBaker(ConfigBaker&&) noexcept = delete;
    auto operator=(const ConfigBaker&) = delete;
    auto operator=(ConfigBaker&&) noexcept = delete;
    ~ConfigBaker() override;

    [[nodiscard]] auto GetName() const -> string_view override { return NAME; }
    [[nodiscard]] auto GetOrder() const -> int32_t override { return 2; }

    void BakeFiles(const FileCollection& files, string_view target_path) const override;

protected:
    ConfigBaker(shared_ptr<BakingContext> ctx, string_view baker_name);

    virtual auto IsClientOnly() const -> bool { return false; }
};

class ClientConfigBaker final : public ConfigBaker
{
public:
    static constexpr string_view_nt NAME {"ClientConfig"};

    explicit ClientConfigBaker(shared_ptr<BakingContext> ctx);
    ClientConfigBaker(const ClientConfigBaker&) = delete;
    ClientConfigBaker(ClientConfigBaker&&) noexcept = delete;
    auto operator=(const ClientConfigBaker&) = delete;
    auto operator=(ClientConfigBaker&&) noexcept = delete;
    ~ClientConfigBaker() override;

    [[nodiscard]] auto GetName() const -> string_view override { return NAME; }

protected:
    auto IsClientOnly() const -> bool override { return true; }
};

FO_END_NAMESPACE
