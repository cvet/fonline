//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "ConfigBaker.h"

#include "AnyData.h"
#include "EngineBase.h"

FO_BEGIN_NAMESPACE

auto GetServerSettings() -> unordered_set<string>;
auto GetClientSettings() -> unordered_set<string>;

template<typename T>
static void ApplyConfigBakerNativeType(const T& value, string_view name, map<string, string>& settings, unordered_set<string>& zero_settings);
static auto IsConfigBakerRuntimeDirective(string_view value) -> bool;
static auto ParseConfigBakerIntegerMagnitude(string_view value, bool& negative, uint64_t& magnitude) -> bool;
template<typename T>
static auto ParseConfigBakerIntegerLiteral(string_view value, T& result) -> bool;
static auto IsConfigBakerIntegerLiteralCandidate(string_view value) -> bool;
template<typename T>
static auto IsConfigBakerTypedIntegerZero(const BaseTypeDesc& type, string_view value) -> bool;
static auto IsConfigBakerIntegerZero(const BaseTypeDesc& type, string_view value) -> bool;
template<typename T>
static auto IsConfigBakerIntegerValueInRange(int64_t value) -> bool;
static auto IsConfigBakerIntegerValueInRange(const BaseTypeDesc& type, int64_t value) -> bool;
template<typename EngineType>
static auto IsConfigBakerGameSettingZero(const BaseTypeDesc& type, string_view value, const EngineType& enum_metadata) -> bool;

ConfigBaker::ConfigBaker(shared_ptr<BakingContext> ctx) :
    ConfigBaker(std::move(ctx), NAME)
{
}

ConfigBaker::ConfigBaker(shared_ptr<BakingContext> ctx, string_view baker_name) :
    BaseBaker(std::move(ctx), baker_name)
{
}

ConfigBaker::~ConfigBaker()
{
}

void ConfigBaker::BakeFiles(const FileCollection& files, string_view target_path) const
{
    FO_TRACE_ZONE(Baking);

    ignore_unused(files);

    if (!target_path.empty() && !strex(target_path).get_file_extension().starts_with("fomain-")) {
        return;
    }

    bool client_only {IsClientOnly()};
    string pack_declarations {_context->GetSettings()->GetResourcePackDeclarations()};
    bool bake_client {true};

    if (!client_only && !pack_declarations.empty()) {
        auto packs {_context->GetSettings()->GetResourcePacks()};
        auto pack {std::ranges::find_if(packs, [&](const auto& candidate) { return candidate.Name == _context->GetPackName(); })};
        bake_client = pack == packs.end() || !vec_exists(pack->Bakers, ClientConfigBaker::NAME);
    }

    unordered_set<string> configs_to_bake {};

    auto check_bake_config {[&](string_view sub_config) {
        auto check_bake_config2 {[&](string_view cfg_name1, string_view cfg_name2) {
            string cfg_name {static_cast<string&&>(strex("{}.fomain-{}", cfg_name1, cfg_name2))};
            string embedded_cfg_name {static_cast<string&&>(strex("{}.fomain-embedded-{}", cfg_name1, cfg_name2))};
            bool needs_bake {true};

            if (_context->HasBakeChecker()) {
                bool output_outdated {_context->CheckOutput(cfg_name, std::numeric_limits<uint64_t>::max())};
                bool embedded_output_outdated {_context->CheckOutput(embedded_cfg_name, std::numeric_limits<uint64_t>::max())};

                needs_bake = output_outdated || embedded_output_outdated;
            }

            if (needs_bake) {
                configs_to_bake.emplace(sub_config);
            }
        }};

        if (!client_only) {
            check_bake_config2(!sub_config.empty() ? sub_config : "(Root)", "server");
        }
        if (bake_client) {
            check_bake_config2(!sub_config.empty() ? sub_config : "(Root)", "client");
        }
    }};

    check_bake_config("");
    std::ranges::for_each(_context->GetSettings()->GetSubConfigs(), [&](auto&& sub_config) { check_bake_config(sub_config.Name); });

    if (!configs_to_bake.empty()) {
        auto server_engine {BakerServerEngine(*_context->GetBakedFiles())};
        auto client_engine {BakerClientEngine(*_context->GetBakedFiles())};
        const auto& server_game_settings {server_engine.GetGameSettings()};
        const auto& client_game_settings {client_engine.GetGameSettings()};

        auto resolve_config_settings {[&](string_view sub_config) -> GlobalSettings {
            FO_VERIFY_AND_THROW(_context->GetSettings()->GetAppliedConfigs().size() == 1, "Config baker expected a single root config before applying bake subconfig", sub_config, _context->GetSettings()->GetAppliedConfigs().size());
            string config_path {_context->GetSettings()->GetAppliedConfigs().front()};
            string_view config_name {static_cast<string_view>(strvex(config_path).extract_file_name())};
            string config_dir {static_cast<string&&>(strex(config_path).extract_dir())};

            auto maincfg {GlobalSettings(true)};
            maincfg.SetupConfigAtPath(config_name, config_dir);

            if (!sub_config.empty()) {
                maincfg.ApplySubConfigSection(sub_config);
            }

            return maincfg;
        }};

        // Bootstrap consumers run before the resource profile is available, so embedded profiles carry these values
        unordered_set<string> bootstrap_game_settings {};

        for (const auto& name : _context->GetSettings()->Baking.BootstrapGameSettings) {
            FO_VERIFY_AND_THROW(server_game_settings.contains(name) || client_game_settings.contains(name), "Bootstrap game setting is not a declared game setting", name);
            bootstrap_game_settings.emplace(name);
        }

        auto bake_config {[&](string_view sub_config) -> bool {
            auto resolved_settings {resolve_config_settings(sub_config)};
            auto config_settings {resolved_settings.Save()};
            unordered_set<string> zero_native_settings {};

#define SETTING(type, group, name, ...) ApplyConfigBakerNativeType(resolved_settings.group.name, SettingNames::group##_##name, config_settings, zero_native_settings)
#define SETTING_GROUP(group, ...)
#define SETTING_GROUP_END(group)
#include "Settings.inc"

            auto server_settings {GetServerSettings()};
            auto client_settings {GetClientSettings()};

            string server_config_content {"#" FO_COMPATIBILITY_VERSION "\n"};
            server_config_content.reserve(0x4000);
            string client_config_content {"#" FO_COMPATIBILITY_VERSION "\n"};
            client_config_content.reserve(0x4000);
            string embedded_server_config_content {"#" FO_COMPATIBILITY_VERSION "\n"};
            embedded_server_config_content.reserve(0x4000);
            string embedded_client_config_content {"#" FO_COMPATIBILITY_VERSION "\n"};
            embedded_client_config_content.reserve(0x4000);

            int32_t settings_errors {0};

            // Full profiles keep every nonzero declared value; embedded siblings keep native settings and bootstrap values
            for (auto&& [key, value] : config_settings) {
                bool is_server_setting {server_settings.count(key) != 0};
                bool is_client_setting {client_settings.count(key) != 0};
                bool is_server_game_setting {server_game_settings.contains(key)};
                bool is_client_game_setting {client_game_settings.contains(key)};
                bool is_game_setting {is_server_game_setting || is_client_game_setting};
                bool is_native_setting {is_server_setting || is_client_setting};
                bool is_bootstrap_setting {bootstrap_game_settings.contains(key)};
                bool write_to_client {is_client_setting || is_client_game_setting};
                bool is_zero {false};
                string output_value {value};

                if (is_native_setting) {
                    is_zero = zero_native_settings.contains(key);
                }
                else if (is_game_setting && (!client_only || write_to_client) && !IsConfigBakerRuntimeDirective(value)) {
                    const auto& game_setting_type {is_client_game_setting ? client_engine.GetGameSetting(key) : server_engine.GetGameSetting(key)};

                    if (is_client_game_setting) {
                        is_zero = IsConfigBakerGameSettingZero(game_setting_type, value, client_engine);
                    }
                    else {
                        is_zero = IsConfigBakerGameSettingZero(game_setting_type, value, server_engine);
                    }

                    if (!is_zero && game_setting_type.IsBool) {
                        output_value = "1";
                    }
                }

                if (!is_zero) {
                    if (!client_only) {
                        server_config_content += strex("{}={}\n", key, output_value);

                        // Embedded server keeps the client half of the native aggregate for embedded-client clones
                        if (is_native_setting || is_bootstrap_setting) {
                            embedded_server_config_content += strex("{}={}\n", key, output_value);
                        }
                    }

                    if (write_to_client) {
                        client_config_content += strex("{}={}\n", key, output_value);

                        if (is_client_setting || is_bootstrap_setting) {
                            embedded_client_config_content += strex("{}={}\n", key, output_value);
                        }
                    }
                }

                if (is_server_setting) {
                    server_settings.erase(key);
                }

                if (is_client_setting) {
                    client_settings.erase(key);
                }

                if (!is_server_setting && !is_client_setting && !is_game_setting) {
                    logging::write("Unknown setting {} = {}", key, value);
                }
            }

            for (const auto& key : server_settings) {
                logging::write("Uninitialized server setting {}", key);
                settings_errors++;
            }
            for (const auto& key : client_settings) {
                logging::write("Uninitialized client setting {}", key);
                settings_errors++;
            }

            if (settings_errors == 0) {
                // A packaged application reads nothing but this config, so the pack list travels in it; sections go last,
                // since every line after a section header belongs to that section
                server_config_content += pack_declarations;
                client_config_content += pack_declarations;
                embedded_server_config_content += pack_declarations;
                embedded_client_config_content += pack_declarations;

                auto write_config {[&](string_view cfg_name1, string_view cfg_name2, string_view cfg_content) {
                    string cfg_name {static_cast<string&&>(strex("{}.fomain-{}", cfg_name1, cfg_name2))};
                    _context->WriteData(cfg_name, make_const_span(cfg_content));
                }};

                string profile_name {!sub_config.empty() ? sub_config : string_view {"(Root)"}};

                if (!client_only) {
                    write_config(profile_name, "server", server_config_content);
                    write_config(profile_name, "embedded-server", embedded_server_config_content);
                }

                if (bake_client) {
                    write_config(profile_name, "client", client_config_content);
                    write_config(profile_name, "embedded-client", embedded_client_config_content);
                }
                return true;
            }

            return false;
        }};

        for (const auto& sub_config : configs_to_bake) {
            if (!bake_config(sub_config)) {
                throw ConfigBakerException("Main config baking error", sub_config);
            }
        }
    }
}

ClientConfigBaker::ClientConfigBaker(shared_ptr<BakingContext> ctx) :
    ConfigBaker(std::move(ctx), NAME)
{
}

ClientConfigBaker::~ClientConfigBaker()
{
}

template<typename T>
static void ApplyConfigBakerNativeType(const T& value, string_view name, map<string, string>& settings, unordered_set<string>& zero_settings)
{
    if constexpr (std::same_as<T, bool>) {
        auto it {settings.find(string(name))};

        if (it != settings.end()) {
            it->second = value ? "1" : "0";
        }
    }
    else {
        ignore_unused(settings);
    }

    bool is_zero {false};

    if constexpr (std::same_as<T, string>) {
        is_zero = value.empty();
    }
    else if constexpr (std::same_as<T, bool>) {
        is_zero = !value;
    }
    else if constexpr (std::is_floating_point_v<T>) {
        is_zero = value == T {};
    }
    else if constexpr (std::is_enum_v<T> || std::is_arithmetic_v<T> || some_strong_type<T>) {
        is_zero = value == T {};
    }
    else if constexpr (some_property_plain_type<T>) {
        is_zero = strex("{}", value) == strex("{}", T {});
    }
    else {
        is_zero = value.empty();
    }

    if (is_zero) {
        zero_settings.emplace(string(name));
    }
}

static auto IsConfigBakerRuntimeDirective(string_view value) -> bool
{
    return value.find("$TARGET_ENV{") != string_view::npos || value.find("$TARGET_FILE{") != string_view::npos;
}

static auto ParseConfigBakerIntegerMagnitude(string_view value, bool& negative, uint64_t& magnitude) -> bool
{
    strvex text {value};
    text.trim();
    string_view body {text.strv()};

    if (body.empty()) {
        return false;
    }

    negative = false;

    if (body.front() == '-' || body.front() == '+') {
        negative = body.front() == '-';
        body.remove_prefix(1);

        if (body.empty()) {
            return false;
        }
    }

    int32_t base {10};

    // from_chars takes the base explicitly and does not understand the 0x prefix
    if (body.length() >= 2 && body.front() == '0' && (body[1] == 'x' || body[1] == 'X')) {
        body.remove_prefix(2);
        base = 16;

        if (body.empty()) {
            return false;
        }
    }

    magnitude = 0;

    auto parse_begin {make_nptr(body.data())};
    ptr<const char> parse_end {parse_begin.offset(body.length())};
    auto parse_result {std::from_chars(parse_begin.get(), parse_end.get(), magnitude, base)};

    return parse_result.ec == std::errc() && parse_result.ptr == parse_end.get();
}

template<typename T>
static auto ParseConfigBakerIntegerLiteral(string_view value, T& result) -> bool
{
    static_assert(std::is_integral_v<T> && !std::same_as<T, bool>);

    bool negative {};
    uint64_t magnitude {};
    bool parsed {ParseConfigBakerIntegerMagnitude(value, negative, magnitude)};

    if (!parsed) {
        return false;
    }

    if constexpr (std::is_signed_v<T>) {
        uint64_t max_magnitude {numeric_cast<uint64_t>(std::numeric_limits<T>::max())};

        if (negative) {
            if (magnitude > max_magnitude + 1) {
                return false;
            }
            if (magnitude == max_magnitude + 1) {
                result = std::numeric_limits<T>::min();
                return true;
            }

            int64_t signed_magnitude {numeric_cast<int64_t>(magnitude)};
            result = numeric_cast<T>(-signed_magnitude);
        }
        else {
            if (magnitude > max_magnitude) {
                return false;
            }

            result = numeric_cast<T>(magnitude);
        }
    }
    else {
        if ((negative && magnitude != 0) || magnitude > numeric_cast<uint64_t>(std::numeric_limits<T>::max())) {
            return false;
        }

        result = numeric_cast<T>(magnitude);
    }

    return true;
}

static auto IsConfigBakerIntegerLiteralCandidate(string_view value) -> bool
{
    strvex text {value};
    text.trim();
    string_view body {text.strv()};

    if (!body.empty() && (body.front() == '-' || body.front() == '+')) {
        body.remove_prefix(1);
    }

    return !body.empty() && body.front() >= '0' && body.front() <= '9';
}

template<typename T>
static auto IsConfigBakerTypedIntegerZero(const BaseTypeDesc& type, string_view value) -> bool
{
    T parsed_value {};

    if (!ParseConfigBakerIntegerLiteral(value, parsed_value)) {
        throw ConfigBakerException("Invalid game setting integer literal", type.Name, value);
    }

    return parsed_value == T {};
}

static auto IsConfigBakerIntegerZero(const BaseTypeDesc& type, string_view value) -> bool
{
    if (type.IsInt8) {
        return IsConfigBakerTypedIntegerZero<int8_t>(type, value);
    }
    if (type.IsInt16) {
        return IsConfigBakerTypedIntegerZero<int16_t>(type, value);
    }
    if (type.IsInt32) {
        return IsConfigBakerTypedIntegerZero<int32_t>(type, value);
    }
    if (type.IsInt64) {
        return IsConfigBakerTypedIntegerZero<int64_t>(type, value);
    }
    if (type.IsUInt8) {
        return IsConfigBakerTypedIntegerZero<uint8_t>(type, value);
    }
    if (type.IsUInt16) {
        return IsConfigBakerTypedIntegerZero<uint16_t>(type, value);
    }
    if (type.IsUInt32) {
        return IsConfigBakerTypedIntegerZero<uint32_t>(type, value);
    }
    if (type.IsUInt64) {
        return IsConfigBakerTypedIntegerZero<uint64_t>(type, value);
    }

    throw ConfigBakerException("Unsupported game setting integer type", type.Name, value);
}

template<typename T>
static auto IsConfigBakerIntegerValueInRange(int64_t value) -> bool
{
    if constexpr (std::is_signed_v<T>) {
        return value >= numeric_cast<int64_t>(std::numeric_limits<T>::min()) && value <= numeric_cast<int64_t>(std::numeric_limits<T>::max());
    }
    else {
        return value >= 0 && numeric_cast<uint64_t>(value) <= numeric_cast<uint64_t>(std::numeric_limits<T>::max());
    }
}

static auto IsConfigBakerIntegerValueInRange(const BaseTypeDesc& type, int64_t value) -> bool
{
    if (type.IsInt8) {
        return IsConfigBakerIntegerValueInRange<int8_t>(value);
    }
    if (type.IsInt16) {
        return IsConfigBakerIntegerValueInRange<int16_t>(value);
    }
    if (type.IsInt32) {
        return IsConfigBakerIntegerValueInRange<int32_t>(value);
    }
    if (type.IsInt64) {
        return IsConfigBakerIntegerValueInRange<int64_t>(value);
    }
    if (type.IsUInt8) {
        return IsConfigBakerIntegerValueInRange<uint8_t>(value);
    }
    if (type.IsUInt16) {
        return IsConfigBakerIntegerValueInRange<uint16_t>(value);
    }
    if (type.IsUInt32) {
        return IsConfigBakerIntegerValueInRange<uint32_t>(value);
    }
    if (type.IsUInt64) {
        return IsConfigBakerIntegerValueInRange<uint64_t>(value);
    }

    throw ConfigBakerException("Unsupported game setting enum underlying type", type.Name);
}

template<typename EngineType>
static auto IsConfigBakerGameSettingZero(const BaseTypeDesc& type, string_view value, const EngineType& enum_metadata) -> bool
{
    if (value.empty()) {
        return true;
    }
    if (type.IsString) {
        return false;
    }
    if (type.IsBool) {
        auto parsed_value {AnyData::ParseValue(string(value), false, false, AnyData::ValueType::Bool)};

        return !parsed_value.AsBool();
    }
    if (type.IsEnum) {
        FO_VERIFY_AND_THROW(type.EnumUnderlyingType, "Game setting enum has no underlying type", type.Name);
        const auto& underlying_type {*type.EnumUnderlyingType};

        // Numeric-looking enum text must be validated as a literal, not resolved as a member name
        if (IsConfigBakerIntegerLiteralCandidate(value)) {
            return IsConfigBakerIntegerZero(underlying_type, value);
        }

        bool failed {false};
        int32_t enum_value {value.find("::") != string_view::npos ? enum_metadata.ResolveEnumValue(value, &failed) : enum_metadata.ResolveEnumValue(type.Name, value, &failed)};

        if (failed) {
            throw ConfigBakerException("Invalid game setting enum value", type.Name, value);
        }
        if (!IsConfigBakerIntegerValueInRange(underlying_type, numeric_cast<int64_t>(enum_value))) {
            throw ConfigBakerException("Game setting enum value is outside its underlying type range", type.Name, value, enum_value);
        }

        return enum_value == 0;
    }
    if (type.IsFloat) {
        float64_t float_value {AnyData::ParseValue(string(value), false, false, AnyData::ValueType::Float64).AsDouble()};

        if (type.IsSingleFloat) {
            return numeric_cast<float32_t>(float_value) == 0.0f;
        }

        return float_value == 0.0;
    }
    if (type.IsInt) {
        return IsConfigBakerIntegerZero(type, value);
    }

    return false;
}

FO_END_NAMESPACE
