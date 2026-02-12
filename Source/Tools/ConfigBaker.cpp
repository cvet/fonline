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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

FO_BEGIN_NAMESPACE

extern auto GetServerSettings() -> unordered_set<string>;
extern auto GetClientSettings() -> unordered_set<string>;

ConfigBaker::ConfigBaker(shared_ptr<BakingContext> ctx) :
    BaseBaker(std::move(ctx))
{
    FO_STACK_TRACE_ENTRY();
}

ConfigBaker::~ConfigBaker()
{
    FO_STACK_TRACE_ENTRY();
}

void ConfigBaker::BakeFiles(const FileCollection& files, string_view target_path) const
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(files);

    if (!target_path.empty() && !strex(target_path).get_file_extension().starts_with("fomain-")) {
        return;
    }

    unordered_set<string> configs_to_bake;

    const auto check_bake_config = [&](string_view sub_config) {
        const auto check_bake_config2 = [&](string_view cfg_name1, string_view cfg_name2) {
            const string cfg_name = strex("{}.fomain-{}", cfg_name1, cfg_name2);
            if (!_context->BakeChecker || _context->BakeChecker(cfg_name, std::numeric_limits<uint64>::max())) {
                configs_to_bake.emplace(sub_config);
            }
        };

        check_bake_config2(!sub_config.empty() ? sub_config : "(Root)", "server");
        check_bake_config2(!sub_config.empty() ? sub_config : "(Root)", "client");
    };

    check_bake_config("");
    std::ranges::for_each(_context->Settings->GetSubConfigs(), [&](auto&& sub_config) { check_bake_config(sub_config.Name); });

    if (!configs_to_bake.empty()) {
        const auto server_engine = BakerServerEngine(*_context->BakedFiles);
        const auto client_engine = BakerClientEngine(*_context->BakedFiles);

        const auto bake_config = [&](string_view sub_config) -> bool {
            FO_RUNTIME_ASSERT(_context->Settings->GetAppliedConfigs().size() == 1);
            const string config_path = _context->Settings->GetAppliedConfigs().front();
            const string config_name = strex(config_path).extract_file_name();
            const string config_dir = strex(config_path).extract_dir();

            auto maincfg = GlobalSettings(true);
            maincfg.ApplyConfigAtPath(config_name, config_dir);

            if (!sub_config.empty()) {
                maincfg.ApplySubConfigSection(sub_config);
            }

            const auto config_settings = maincfg.Save();

            auto server_settings = GetServerSettings();
            auto client_settings = GetClientSettings();

            for (const auto& name : server_engine.GetGameSettings() | std::views::keys) {
                server_settings.emplace(name);
            }
            for (const auto& name : client_engine.GetGameSettings() | std::views::keys) {
                client_settings.emplace(name);
            }

            string server_config_content;
            server_config_content.reserve(0x4000);
            string client_config_content;
            client_config_content.reserve(0x4000);

            int32 settings_errors = 0;

            for (auto&& [key, value] : config_settings) {
                const auto is_server_setting = server_settings.count(key) != 0;
                const auto is_client_setting = client_settings.count(key) != 0;
                const bool skip_write = value.empty() || value == "0" || strex(value).lower() == "false";
                const auto shortened_value = strex(value).is_explicit_bool() ? (strex(value).to_bool() ? "1" : "0") : value;

                if (!skip_write) {
                    server_config_content += strex("{}={}\n", key, shortened_value);
                }

                if (is_server_setting) {
                    server_settings.erase(key);
                }

                if (is_client_setting) {
                    if (!skip_write) {
                        client_config_content += strex("{}={}\n", key, shortened_value);
                    }

                    client_settings.erase(key);
                }

                if (!is_server_setting && !is_client_setting) {
                    WriteLog("Unknown setting {} = {}", key, value);
                }
            }

            for (const auto& key : server_settings) {
                WriteLog("Uninitialized server setting {}", key);
                settings_errors++;
            }
            for (const auto& key : client_settings) {
                WriteLog("Uninitialized client setting {}", key);
                settings_errors++;
            }

            if (settings_errors == 0) {
                const auto write_config = [&](string_view cfg_name1, string_view cfg_name2, string_view cfg_content) {
                    const string cfg_name = strex("{}.fomain-{}", cfg_name1, cfg_name2);
                    _context->WriteData(cfg_name, {reinterpret_cast<const uint8*>(cfg_content.data()), cfg_content.size()});
                };

                write_config(!sub_config.empty() ? sub_config : "(Root)", "server", server_config_content);
                write_config(!sub_config.empty() ? sub_config : "(Root)", "client", client_config_content);
                return true;
            }

            return false;
        };

        for (const auto& sub_config : configs_to_bake) {
            if (!bake_config(sub_config)) {
                throw ConfigBakerException("Main config baking error", sub_config);
            }
        }
    }
}

FO_END_NAMESPACE
