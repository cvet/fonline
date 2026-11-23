// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

// Section names are stored verbatim: a '/' marks nesting syntactically, but what a prefix means is the
// consuming format's rule, which is why GetOrderedSections() exposes file order

enum class ConfigFileOption : uint8_t
{
    None = 0,
    CollectContent = 0x1,
    SkipNestedSections = 0x2,
};

class ConfigFile final
{
public:
    explicit ConfigFile(string str, ConfigFileOption options = ConfigFileOption::None);
    ConfigFile(const ConfigFile&) = delete;
    ConfigFile(ConfigFile&&) noexcept = default;
    auto operator=(const ConfigFile&) = delete;
    auto operator=(ConfigFile&&) noexcept -> ConfigFile& = default;
    ~ConfigFile() = default;

    [[nodiscard]] auto HasSection(string_view section_name) const noexcept -> bool;
    [[nodiscard]] auto HasKey(string_view section_name, string_view key_name) const noexcept -> bool;
    [[nodiscard]] auto GetAsStr(string_view section_name, string_view key_name) const noexcept -> string_view;
    [[nodiscard]] auto GetAsStr(string_view section_name, string_view key_name, string_view def_val) const noexcept -> string_view;
    [[nodiscard]] auto GetAsInt(string_view section_name, string_view key_name) const noexcept -> int32_t;
    [[nodiscard]] auto GetAsInt(string_view section_name, string_view key_name, int32_t def_val) const noexcept -> int32_t;
    [[nodiscard]] auto GetSection(string_view section_name) const -> const map<string_view, string_view>&;
    [[nodiscard]] auto GetSections(string_view section_name) -> vector<ptr<map<string_view, string_view>>>;
    [[nodiscard]] auto GetSections() noexcept -> ptr<multimap<string_view, map<string_view, string_view>>>;
    [[nodiscard]] auto GetOrderedSections() const noexcept -> const vector<pair<string_view, ptr<map<string_view, string_view>>>>& { return _orderedSections; }
    [[nodiscard]] auto GetSectionKeyValues(string_view section_name) noexcept -> nptr<const map<string_view, string_view>>;
    [[nodiscard]] auto GetSectionContent(string_view section_name) const -> string_view;

private:
    auto ParseConfigKeyValueLine(string_view line, string_view& key, string_view& value, bool& append_value) -> bool;
    void TrimConfigRange(string_view line, size_t& begin, size_t& end);
    auto IsConfigSpace(char ch) -> bool;
    auto FindFirstSection(string_view section_name) const noexcept -> multimap<string_view, map<string_view, string_view>>::const_iterator;
    auto GetRawValue(string_view section_name, string_view key_name) const noexcept -> nptr<const string_view>;
    auto StoreOwnedString(string_view value) -> string_view;
    auto StoreOwnedString(string&& value) -> string_view;

    ConfigFileOption _options;
    list<string> _ownedStrings {}; // List nodes keep their address across a move, an SSO string member would not
    multimap<string_view, map<string_view, string_view>> _sectionKeyValues {};
    vector<pair<string_view, ptr<map<string_view, string_view>>>> _orderedSections {};
};

FO_END_NAMESPACE
