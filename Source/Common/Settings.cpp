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
#include "FileSystem.h"
#include "StringUtils.h"
#include "WinApi-Include.h"

#include "imgui.h"

static void SetEntry(string& entry, string_view value, bool append)
{
    if (!append) {
        entry.clear();
    }
    if (append && !entry.empty()) {
        entry += " ";
    }
    auto&& any_value = AnyData::ParseValue(string(value), false, false, AnyData::STRING_VALUE);
    entry += std::get<AnyData::STRING_VALUE>(any_value);
}
static void SetEntry(uchar& entry, string_view value, bool append)
{
    auto&& any_value = AnyData::ParseValue(string(value), false, false, AnyData::INT_VALUE);
    entry += static_cast<uchar>(std::get<AnyData::INT_VALUE>(any_value));
}
static void SetEntry(short& entry, string_view value, bool append)
{
    if (!append) {
        entry = 0;
    }
    auto&& any_value = AnyData::ParseValue(string(value), false, false, AnyData::INT_VALUE);
    entry = static_cast<short>(entry + std::get<AnyData::INT_VALUE>(any_value));
}
static void SetEntry(int& entry, string_view value, bool append)
{
    if (!append) {
        entry = 0;
    }
    auto&& any_value = AnyData::ParseValue(string(value), false, false, AnyData::INT_VALUE);
    entry += std::get<AnyData::INT_VALUE>(any_value);
}
static void SetEntry(uint& entry, string_view value, bool append)
{
    if (!append) {
        entry = 0u;
    }
    auto&& any_value = AnyData::ParseValue(string(value), false, false, AnyData::INT_VALUE);
    entry += std::get<AnyData::INT_VALUE>(any_value);
}
static void SetEntry(bool& entry, string_view value, bool append)
{
    if (!append) {
        entry = false;
    }
    auto&& any_value = AnyData::ParseValue(string(value), false, false, AnyData::BOOL_VALUE);
    entry |= std::get<AnyData::BOOL_VALUE>(any_value);
}
static void SetEntry(float& entry, string_view value, bool append)
{
    if (!append) {
        entry = 0.0f;
    }
    auto&& any_value = AnyData::ParseValue(string(value), false, false, AnyData::DOUBLE_VALUE);
    entry += static_cast<float>(std::get<AnyData::DOUBLE_VALUE>(any_value));
}
static void SetEntry(vector<string>& entry, string_view value, bool append)
{
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
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEntry(string_view name, const uchar& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEntry(string_view name, const short& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEntry(string_view name, const int& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEntry(string_view name, const uint& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEntry(string_view name, const bool& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEntry(string_view name, const float& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEntry(string_view name, const vector<string>& entry)
{
    string value;
    for (const auto& e : entry) {
        value += e;
    }
    ImGui::TextUnformatted(_str("{}: {}", name, value).c_str());
}
static void DrawEntry(string_view name, const vector<int>& entry)
{
    string value;
    for (const auto& e : entry) {
        value += std::to_string(e);
    }
    ImGui::TextUnformatted(_str("{}: {}", name, value).c_str());
}
static void DrawEntry(string_view name, const vector<uint>& entry)
{
    string value;
    for (const auto& e : entry) {
        value += std::to_string(e);
    }
    ImGui::TextUnformatted(_str("{}: {}", name, value).c_str());
}
static void DrawEntry(string_view name, const vector<float>& entry)
{
    string value;
    for (const auto& e : entry) {
        value += std::to_string(e);
    }
    ImGui::TextUnformatted(_str("{}: {}", name, value).c_str());
}
static void DrawEntry(string_view name, const vector<bool>& entry)
{
    string value;
    for (const auto& e : entry) {
        value += e ? "True" : "False";
    }
    ImGui::TextUnformatted(_str("{}: {}", name, value).c_str());
}

static void DrawEditableEntry(string_view name, string& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEditableEntry(string_view name, uchar& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEditableEntry(string_view name, short& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEditableEntry(string_view name, int& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEditableEntry(string_view name, uint& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEditableEntry(string_view name, bool& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEditableEntry(string_view name, float& entry)
{
    ImGui::TextUnformatted(_str("{}: {}", name, entry).c_str());
}
static void DrawEditableEntry(string_view name, vector<string>& entry)
{
    // Todo: improve editable entry for arrays
    UNUSED_VARIABLE(entry);
    ImGui::TextUnformatted(_str("{}: {}", name, "n/a").c_str());
}
static void DrawEditableEntry(string_view name, vector<int>& entry)
{
    UNUSED_VARIABLE(entry);
    ImGui::TextUnformatted(_str("{}: {}", name, "n/a").c_str());
}
static void DrawEditableEntry(string_view name, vector<uint>& entry)
{
    UNUSED_VARIABLE(entry);
    ImGui::TextUnformatted(_str("{}: {}", name, "n/a").c_str());
}
static void DrawEditableEntry(string_view name, vector<float>& entry)
{
    UNUSED_VARIABLE(entry);
    ImGui::TextUnformatted(_str("{}: {}", name, "n/a").c_str());
}
static void DrawEditableEntry(string_view name, vector<bool>& entry)
{
    UNUSED_VARIABLE(entry);
    ImGui::TextUnformatted(_str("{}: {}", name, "n/a").c_str());
}

GlobalSettings::GlobalSettings(int argc, char** argv)
{
    // Default config
    static char default_config[] =
#include "DebugSettings-Include.h"
        ;

    if (const auto config = ConfigFile(default_config, nullptr)) {
        for (const auto& [key, value] : config.GetApp("")) {
            SetValue(key, value);
        }
    }

    // Injected config
    static char internal_config[5022] = {"###InternalConfig###\0"
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
                                         "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"};

    if (const auto config = ConfigFile(internal_config, nullptr)) {
        for (const auto& [key, value] : config.GetApp("")) {
            SetValue(key, value);
        }
    }

    for (auto i = 0; i < argc; i++) {
        // Skip path
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
            if (i < argc - 1 && argv[i + 1][0] != '-') {
                SetValue(key, _str("{}", argv[i + 1]).trim());
            }
            else {
                SetValue(key, "1");
            }
        }
    }

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

    const_cast<int&>(MapDirCount) = MapHexagonal ? 6 : 8;
}

void GlobalSettings::SetValue(string_view setting_name, const string& value)
{
#define SET_SETTING(sett) \
    if (!value.empty() && value[0] == '+') { \
        SetEntry(sett, value.substr(1), true); \
    } \
    else { \
        SetEntry(sett, value, false); \
    }
#define FIXED_SETTING(type, name, ...) \
    if (setting_name == #name) { \
        SET_SETTING(const_cast<type&>(name)); \
        return; \
    }
#define VARIABLE_SETTING(type, name, ...) \
    if (setting_name == #name) { \
        SET_SETTING(name); \
        return; \
    }
#define SETTING_GROUP(name, ...)
#define SETTING_GROUP_END()
#include "Settings-Include.h"
#undef SET_SETTING
}

void GlobalSettings::Draw(bool editable)
{
#define FIXED_SETTING(type, name, ...) \
    if (editable) \
        DrawEditableEntry(#name, const_cast<type&>(name)); \
    else \
        DrawEntry(#name, name)
#define VARIABLE_SETTING(type, name, ...) \
    if (editable) \
        DrawEditableEntry(#name, name); \
    else \
        DrawEntry(#name, name)
#define SETTING_GROUP(name, ...)
#define SETTING_GROUP_END()
#include "Settings-Include.h"
}
