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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Settings.h"
#include "AnyData.h"
#include "CacheStorage.h"
#include "ConfigFile.h"
#include "DiskFileSystem.h"
#include "Log.h"
#include "StringUtils.h"
#include "WinApi-Include.h"

#include "imgui.h"

static void SetEntry(string& entry, string_view value, bool append)
{
    STACK_TRACE_ENTRY();

    if (!append) {
        entry.clear();
    }
    if (append && !entry.empty()) {
        entry += " ";
    }
    auto&& any_value = AnyData::ParseValue(string(value), false, false, AnyData::STRING_VALUE);
    entry += std::get<AnyData::STRING_VALUE>(any_value);
}
static void SetEntry(uint8& entry, string_view value, bool append)
{
    STACK_TRACE_ENTRY();

    auto&& any_value = AnyData::ParseValue(string(value), false, false, AnyData::INT_VALUE);
    entry += static_cast<uint8>(std::get<AnyData::INT_VALUE>(any_value));
}
static void SetEntry(int16& entry, string_view value, bool append)
{
    STACK_TRACE_ENTRY();

    if (!append) {
        entry = 0;
    }
    auto&& any_value = AnyData::ParseValue(string(value), false, false, AnyData::INT_VALUE);
    entry = static_cast<int16>(entry + std::get<AnyData::INT_VALUE>(any_value));
}
static void SetEntry(int& entry, string_view value, bool append)
{
    STACK_TRACE_ENTRY();

    if (!append) {
        entry = 0;
    }
    auto&& any_value = AnyData::ParseValue(string(value), false, false, AnyData::INT_VALUE);
    entry += std::get<AnyData::INT_VALUE>(any_value);
}
static void SetEntry(uint& entry, string_view value, bool append)
{
    STACK_TRACE_ENTRY();

    if (!append) {
        entry = 0u;
    }
    auto&& any_value = AnyData::ParseValue(string(value), false, false, AnyData::INT_VALUE);
    entry += std::get<AnyData::INT_VALUE>(any_value);
}
static void SetEntry(bool& entry, string_view value, bool append)
{
    STACK_TRACE_ENTRY();

    if (!append) {
        entry = false;
    }
    auto&& any_value = AnyData::ParseValue(string(value), false, false, AnyData::BOOL_VALUE);
    entry |= std::get<AnyData::BOOL_VALUE>(any_value);
}
static void SetEntry(float& entry, string_view value, bool append)
{
    STACK_TRACE_ENTRY();

    if (!append) {
        entry = 0.0f;
    }
    auto&& any_value = AnyData::ParseValue(string(value), false, false, AnyData::DOUBLE_VALUE);
    entry += static_cast<float>(std::get<AnyData::DOUBLE_VALUE>(any_value));
}
static void SetEntry(vector<string>& entry, string_view value, bool append)
{
    STACK_TRACE_ENTRY();

    if (!append) {
        entry.clear();
    }
    auto&& arr_value = AnyData::ParseValue(string(value), false, true, AnyData::STRING_VALUE);
    auto&& arr = std::get<AnyData::ARRAY_VALUE>(arr_value);
    for (const auto& str : arr) {
        entry.emplace_back(std::get<AnyData::STRING_VALUE>(str));
    }
}
static void SetEntry(vector<int>& entry, string_view value, bool append)
{
    STACK_TRACE_ENTRY();

    if (!append) {
        entry.clear();
    }
    auto&& arr_value = AnyData::ParseValue(string(value), false, true, AnyData::INT_VALUE);
    auto&& arr = std::get<AnyData::ARRAY_VALUE>(arr_value);
    for (const auto& str : arr) {
        entry.emplace_back(std::get<AnyData::INT_VALUE>(str));
    }
}
static void SetEntry(vector<uint>& entry, string_view value, bool append)
{
    STACK_TRACE_ENTRY();

    if (!append) {
        entry.clear();
    }
    auto&& arr_value = AnyData::ParseValue(string(value), false, true, AnyData::INT_VALUE);
    auto&& arr = std::get<AnyData::ARRAY_VALUE>(arr_value);
    for (const auto& str : arr) {
        entry.emplace_back(std::get<AnyData::INT_VALUE>(str));
    }
}
static void SetEntry(vector<float>& entry, string_view value, bool append)
{
    STACK_TRACE_ENTRY();

    if (!append) {
        entry.clear();
    }
    auto&& arr_value = AnyData::ParseValue(string(value), false, true, AnyData::DOUBLE_VALUE);
    auto&& arr = std::get<AnyData::ARRAY_VALUE>(arr_value);
    for (const auto& str : arr) {
        entry.emplace_back(static_cast<float>(std::get<AnyData::DOUBLE_VALUE>(str)));
    }
}
static void SetEntry(vector<bool>& entry, string_view value, bool append)
{
    STACK_TRACE_ENTRY();

    if (!append) {
        entry.clear();
    }
    auto&& arr_value = AnyData::ParseValue(string(value), false, true, AnyData::BOOL_VALUE);
    auto&& arr = std::get<AnyData::ARRAY_VALUE>(arr_value);
    for (const auto& str : arr) {
        entry.emplace_back(std::get<AnyData::BOOL_VALUE>(str));
    }
}

static void DrawEntry(string_view name, string_view entry)
{
    STACK_TRACE_ENTRY();

    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEntry(string_view name, const uint8& entry)
{
    STACK_TRACE_ENTRY();

    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEntry(string_view name, const int16& entry)
{
    STACK_TRACE_ENTRY();

    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEntry(string_view name, const int& entry)
{
    STACK_TRACE_ENTRY();

    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEntry(string_view name, const uint& entry)
{
    STACK_TRACE_ENTRY();

    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEntry(string_view name, const bool& entry)
{
    STACK_TRACE_ENTRY();

    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEntry(string_view name, const float& entry)
{
    STACK_TRACE_ENTRY();

    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEntry(string_view name, const vector<string>& entry)
{
    STACK_TRACE_ENTRY();

    string value;
    for (const auto& e : entry) {
        value += e;
    }
    ImGui::TextUnformatted(_str("{}: {}", name, value).c_str());
}
static void DrawEntry(string_view name, const vector<int>& entry)
{
    STACK_TRACE_ENTRY();

    string value;
    for (const auto& e : entry) {
        value += std::to_string(e);
    }
    ImGui::TextUnformatted(_str("{}: {}", name, value).c_str());
}
static void DrawEntry(string_view name, const vector<uint>& entry)
{
    STACK_TRACE_ENTRY();

    string value;
    for (const auto& e : entry) {
        value += std::to_string(e);
    }
    ImGui::TextUnformatted(_str("{}: {}", name, value).c_str());
}
static void DrawEntry(string_view name, const vector<float>& entry)
{
    STACK_TRACE_ENTRY();

    string value;
    for (const auto& e : entry) {
        value += std::to_string(e);
    }
    ImGui::TextUnformatted(_str("{}: {}", name, value).c_str());
}
static void DrawEntry(string_view name, const vector<bool>& entry)
{
    STACK_TRACE_ENTRY();

    string value;
    for (const auto& e : entry) {
        value += e ? "True" : "False";
    }
    ImGui::TextUnformatted(_str("{}: {}", name, value).c_str());
}

static void DrawEditableEntry(string_view name, string& entry)
{
    STACK_TRACE_ENTRY();

    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEditableEntry(string_view name, uint8& entry)
{
    STACK_TRACE_ENTRY();

    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEditableEntry(string_view name, int16& entry)
{
    STACK_TRACE_ENTRY();

    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEditableEntry(string_view name, int& entry)
{
    STACK_TRACE_ENTRY();

    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEditableEntry(string_view name, uint& entry)
{
    STACK_TRACE_ENTRY();

    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEditableEntry(string_view name, bool& entry)
{
    STACK_TRACE_ENTRY();

    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEditableEntry(string_view name, float& entry)
{
    STACK_TRACE_ENTRY();

    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEditableEntry(string_view name, vector<string>& entry)
{
    STACK_TRACE_ENTRY();

    // Todo: improve editable entry for arrays
    UNUSED_VARIABLE(entry);
    ImGui::TextUnformatted(_str("{}: {}", name, "n/a").c_str());
}
static void DrawEditableEntry(string_view name, vector<int>& entry)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(entry);
    ImGui::TextUnformatted(_str("{}: {}", name, "n/a").c_str());
}
static void DrawEditableEntry(string_view name, vector<uint>& entry)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(entry);
    ImGui::TextUnformatted(_str("{}: {}", name, "n/a").c_str());
}
static void DrawEditableEntry(string_view name, vector<float>& entry)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(entry);
    ImGui::TextUnformatted(_str("{}: {}", name, "n/a").c_str());
}
static void DrawEditableEntry(string_view name, vector<bool>& entry)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(entry);
    ImGui::TextUnformatted(_str("{}: {}", name, "n/a").c_str());
}

GlobalSettings::GlobalSettings(int argc, char** argv, bool client_mode)
{
    STACK_TRACE_ENTRY();

    const_cast<bool&>(ClientMode) = client_mode;

    const auto volatile_char_to_string = [](const volatile char* str, size_t len) -> string {
        string result;
        result.resize(len);
        for (size_t i = 0; i < len; i++) {
            result[i] = str[i];
        }
        return result;
    };

    // Debugging config
    if (IsRunInDebugger()) {
        static const volatile char DEBUG_CONFIG[] =
#include "DebugSettings-Include.h"
            ;

        const auto config = ConfigFile("DebugConfig.focfg", volatile_char_to_string(DEBUG_CONFIG, sizeof(DEBUG_CONFIG)));
        for (auto&& [key, value] : config.GetSection("")) {
            SetValue(key, value);
        }
    }

    // Injected config
    {
        static const volatile char INTERNAL_CONFIG[5022] = {"###InternalConfig###\0"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                            "12345678901234567890123456789012345678901234567890123456789012345678901234567###InternalConfigEnd###"};

        const auto config = ConfigFile("InternalConfig.focfg", volatile_char_to_string(INTERNAL_CONFIG, sizeof(INTERNAL_CONFIG)));
        for (auto&& [key, value] : config.GetSection("")) {
            SetValue(key, value);
        }
    }

    // External config
    const auto apply_external_config = [&](string_view config_path) {
        WriteLog("Load external config {}", config_path);

        if (auto settings_file = DiskFileSystem::OpenFile(config_path, false)) {
            string settings_content;
            settings_content.resize(settings_file.GetSize());
            settings_file.Read(settings_content.data(), settings_content.size());

            const auto config = ConfigFile("ExternalConfig.focfg", settings_content);
            for (auto&& [key, value] : config.GetSection("")) {
                SetValue(key, value);
            }
        }
        else {
            if (!BreakIntoDebugger("External config not found")) {
                throw GenericException("External config not found", config_path);
            }
        }
    };

    if (!ExternalConfig.empty()) {
        apply_external_config(ExternalConfig);
    }

    // Command line settings before local config and other command line settings
    for (auto i = 0; i < argc; i++) {
        if (i == 0 && argv[0][0] != '-') {
            continue;
        }

        if (argv[i][0] == '-') {
            auto key = _str("{}", argv[i]).trim().str().substr(1);

            if (!key.empty() && key.front() == '-') {
                key = key.substr(1);
            }

            if (key == "ExternalConfig" || key == "ResourcesDir") {
                const auto value = i < argc - 1 && argv[i + 1][0] != '-' ? _str("{}", argv[i + 1]).trim().str() : "1";

                WriteLog("Command line set {} = {}", key, value);

                SetValue(key, value);

                if (key == "ExternalConfig" && !ExternalConfig.empty()) {
                    apply_external_config(ExternalConfig);
                }
            }
        }
    }

    // Local config
    if (ClientMode) {
        auto&& cache = CacheStorage(_str(ResourcesDir).combinePath("Cache.fobin"));

        if (cache.HasEntry(LOCAL_CONFIG_NAME)) {
            WriteLog("Load local config {}", LOCAL_CONFIG_NAME);

            const auto config = ConfigFile(LOCAL_CONFIG_NAME, cache.GetString(LOCAL_CONFIG_NAME));
            for (auto&& [key, value] : config.GetSection("")) {
                SetValue(key, value);
            }
        }
    }

    // Command line config
    for (auto i = 0; i < argc; i++) {
        if (i == 0 && argv[0][0] != '-') {
            continue;
        }

        const_cast<vector<string>&>(CommandLineArgs).emplace_back(argv[i]);
        const_cast<string&>(CommandLine) += argv[i];
        if (i < argc - 1) {
            const_cast<string&>(CommandLine) += " ";
        }

        if (argv[i][0] == '-') {
            auto key = _str("{}", argv[i]).trim().str().substr(1);

            if (!key.empty() && key.front() == '-') {
                key = key.substr(1);
            }

            if (key != "ExternalConfig" && key != "ResourcesDir") {
                const auto value = i < argc - 1 && argv[i + 1][0] != '-' ? _str("{}", argv[i + 1]).trim().str() : "1";

                WriteLog("Command line set {} = {}", key, value);

                SetValue(key, value);
            }
        }
    }

    // Auto settings
#if FO_WEB
    const_cast<bool&>(WebBuild) = true;
#else
    const_cast<bool&>(WebBuild) = false;
#endif
#if FO_WINDOWS
    const_cast<bool&>(WindowsBuild) = true;
#else
    const_cast<bool&>(WindowsBuild) = false;
#endif
#if FO_LINUX
    const_cast<bool&>(LinuxBuild) = true;
#else
    const_cast<bool&>(LinuxBuild) = false;
#endif
#if FO_MAC
    const_cast<bool&>(MacOsBuild) = true;
#else
    const_cast<bool&>(MacOsBuild) = false;
#endif
#if FO_ANDROID
    const_cast<bool&>(AndroidBuild) = true;
#else
    const_cast<bool&>(AndroidBuild) = false;
#endif
#if FO_IOS
    const_cast<bool&>(IOsBuild) = true;
#else
    const_cast<bool&>(IOsBuild) = false;
#endif
    const_cast<bool&>(DesktopBuild) = WindowsBuild || LinuxBuild || MacOsBuild;
    const_cast<bool&>(TabletBuild) = AndroidBuild || IOsBuild;

#if FO_WINDOWS && !FO_UWP
    if (GetSystemMetrics(SM_TABLETPC) != 0) {
        const_cast<bool&>(DesktopBuild) = false;
        const_cast<bool&>(TabletBuild) = true;
    }
#endif

    const_cast<bool&>(MapHexagonal) = GameSettings::HEXAGONAL_GEOMETRY;
    const_cast<bool&>(MapSquare) = GameSettings::SQUARE_GEOMETRY;
    const_cast<int&>(MapDirCount) = GameSettings::MAP_DIR_COUNT;

#if FO_DEBUG
    const_cast<bool&>(DebugBuild) = true;
    const_cast<bool&>(RenderDebug) = true;
#endif
}

void GlobalSettings::SetValue(const string& setting_name, const string& setting_value)
{
    STACK_TRACE_ENTRY();

    const bool append = !setting_value.empty() && setting_value[0] == '+';
    string_view value = append ? string_view(setting_value).substr(1) : setting_value;

    // Resolve environment variables and files
    string resolved_value;
    size_t prev_pos = 0;
    size_t pos = setting_value.find('$');

    if (pos != string::npos) {
        while (pos != string::npos) {
            const bool is_env = setting_value.compare(pos + 1, "ENV{"_len, "ENV{") == 0;
            const bool is_file = setting_value.compare(pos + 1, "FILE{"_len, "FILE{") == 0;

            if (is_env || is_file) {
                pos += is_env ? "$ENV{"_len : "$FILE{"_len;
                size_t end_pos = setting_value.find('}', pos);

                if (end_pos != string::npos) {
                    const string name = setting_value.substr(pos, end_pos - pos);

                    if (is_env) {
                        const char* env = !name.empty() ? std::getenv(name.c_str()) : nullptr;
                        if (env != nullptr) {
                            resolved_value += setting_value.substr(prev_pos, pos - prev_pos - "$ENV{"_len) + string(env);
                            end_pos++;
                        }
                        else {
                            WriteLog(LogType::Warning, "Environment variable $ENV{{{}}} for setting {} is not found", name, setting_name);
                            resolved_value += setting_value.substr(prev_pos, pos - prev_pos) + name;
                        }
                    }
                    else {
                        auto file = DiskFileSystem::OpenFile(name, false);
                        if (file) {
                            string file_content;
                            file_content.resize(file.GetSize());
                            file.Read(file_content.data(), file_content.size());
                            file_content = _str(file_content).trim();

                            resolved_value += setting_value.substr(prev_pos, pos - prev_pos - "$FILE{"_len) + string(file_content);
                            end_pos++;
                        }
                        else {
                            WriteLog(LogType::Warning, "File $FILE{{{}}} for setting {} is not found", name, setting_name);
                            resolved_value += setting_value.substr(prev_pos, pos - prev_pos) + name;
                        }
                    }

                    prev_pos = end_pos;
                    pos = setting_value.find('$', end_pos);
                }
                else {
                    WriteLog(LogType::Warning, "Not closed $ tag in setting {}: {}", setting_name, setting_value);
                }
            }
            else {
                pos = setting_value.find('$', pos + 1);
            }
        }

        if (prev_pos != string::npos) {
            resolved_value += setting_value.substr(prev_pos);
        }

        value = resolved_value;
    }

#define SET_SETTING(sett) \
    SetEntry(sett, value, append); \
    return
#define FIXED_SETTING(type, name, ...) \
    case const_hash(#name): \
        SET_SETTING(const_cast<type&>(name))
#define VARIABLE_SETTING(type, name, ...) \
    case const_hash(#name): \
        SET_SETTING(name)
#define SETTING_GROUP(name, ...)
#define SETTING_GROUP_END()

    switch (const_hash(setting_name.c_str())) {
#include "Settings-Include.h"
    default:
        Custom[setting_name] = setting_value;
    }

#undef SET_SETTING
}

void GlobalSettings::Draw(bool editable)
{
    STACK_TRACE_ENTRY();

#define FIXED_SETTING(type, name, ...) \
    if (editable) { \
        DrawEditableEntry(#name, const_cast<type&>(name)); \
    } \
    else { \
        DrawEntry(#name, name); \
    }
#define VARIABLE_SETTING(type, name, ...) \
    if (editable) { \
        DrawEditableEntry(#name, name); \
    } \
    else { \
        DrawEntry(#name, name); \
    }
#define SETTING_GROUP(name, ...)
#define SETTING_GROUP_END()
#include "Settings-Include.h"
}
