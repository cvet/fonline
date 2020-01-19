#include "ConfigFile.h"
#include "StringUtils.h"
#include "Testing.h"

#include "ryml.hpp"

static bool IsYaml(const string& str)
{
    return str.length() >= 3 && str[0] == '-' && str[1] == '-' && str[2] == '-';
}

ConfigFile::ConfigFile(const string& str)
{
    ParseStr(str);
}

void ConfigFile::AppendData(const string& str)
{
    ParseStr(str);
}

void ConfigFile::ParseStr(const string& str)
{
    if (IsYaml(str))
    {
        ryml::Tree tree = ryml::parse(c4::to_csubstr(str.c_str()));
        // RUNTIME_ASSERT
    }
    else
    {
        StrMap* cur_app;
        auto it_app = appKeyValues.find("");
        if (it_app == appKeyValues.end())
        {
            auto it = appKeyValues.insert(std::make_pair("", StrMap()));
            appKeyValuesOrder.push_back(it);
            cur_app = &it->second;
        }
        else
        {
            cur_app = &it_app->second;
        }

        string app_content;
        if (collectContent)
            app_content.reserve(0xFFFF);

        istringstream istr(str);
        string line;
        string accum_line;
        while (std::getline(istr, line, '\n'))
        {
            if (!line.empty())
                line = _str(line).trim();

            // Accumulate line
            if (!accum_line.empty())
            {
                line.insert(0, accum_line);
                accum_line.clear();
            }

            if (line.empty())
                continue;

            if (line.length() >= 2 && line.back() == '\\' &&
                (line[line.length() - 2] == ' ' || line[line.length() - 2] == '\t'))
            {
                line.pop_back();
                accum_line = _str(line).trim() + " ";
                continue;
            }

            // New section
            if (line[0] == '[')
            {
                // Parse name
                size_t end = line.find(']');
                if (end == string::npos)
                    continue;

                string buf = _str(line.substr(1, end - 1)).trim();
                if (buf.empty())
                    continue;

                // Store current section content
                if (collectContent)
                {
                    (*cur_app)[""] = app_content;
                    app_content.clear();
                }

                // Add new section
                auto it = appKeyValues.insert(std::make_pair(buf, StrMap()));
                appKeyValuesOrder.push_back(it);
                cur_app = &it->second;
            }
            // Section content
            else
            {
                // Store raw content
                if (collectContent)
                    app_content.append(line).append("\n");

                // Text format {}{}{}
                if (line[0] == '{')
                {
                    uint num = 0;
                    size_t offset = 0;
                    for (int i = 0; i < 3; i++)
                    {
                        size_t first = line.find('{', offset);
                        size_t last = line.find('}', first);
                        if (first == string::npos || last == string::npos)
                            break;

                        string str = line.substr(first + 1, last - first - 1);
                        offset = last + 1;
                        if (i == 0 && !num)
                            num = (_str(str).isNumber() ? _str(str).toInt() : _str(str).toHash());
                        else if (i == 1 && num)
                            num += (!str.empty() ? (_str(str).isNumber() ? _str(str).toInt() : _str(str).toHash()) : 0);
                        else if (i == 2 && num)
                            (*cur_app)[_str("{}", num)] = str;
                    }
                }
                else
                {
                    // Cut comments
                    size_t comment = line.find('#');
                    if (comment != string::npos)
                        line.erase(comment);
                    if (line.empty())
                        continue;

                    // Key value format
                    size_t separator = line.find('=');
                    if (separator != string::npos)
                    {
                        string key = _str(line.substr(0, separator)).trim();
                        string value = _str(line.substr(separator + 1)).trim();
                        if (!key.empty())
                            (*cur_app)[key] = value;
                    }
                }
            }
        }
        RUNTIME_ASSERT(istr.eof());

        // Store current section content
        if (collectContent)
            (*cur_app)[""] = app_content;
    }
}

string ConfigFile::SerializeData()
{
    string str;
    bool to_yaml = true;
    if (to_yaml)
    {
    }
    else
    {
        str.reserve(100000);
        for (auto& app_it : appKeyValuesOrder)
        {
            auto& app = *app_it;
            if (!app.first.empty())
                str += _str("[{}]\n", app.first);
            for (const auto& kv : app.second)
                if (!kv.first.empty())
                    str += _str("{} = {}\n", kv.first, kv.second);
            str += "\n";
        }
    }
    return str;
}

string* ConfigFile::GetRawValue(const string& app_name, const string& key_name)
{
    auto it_app = appKeyValues.find(app_name);
    if (it_app == appKeyValues.end())
        return nullptr;
    auto it_key = it_app->second.find(key_name);
    if (it_key == it_app->second.end())
        return nullptr;
    return &it_key->second;
}

string ConfigFile::GetStr(const string& app_name, const string& key_name, const string& def_val)
{
    string* str = GetRawValue(app_name, key_name);
    return str ? *str : def_val;
}

int ConfigFile::GetInt(const string& app_name, const string& key_name, int def_val)
{
    string* str = GetRawValue(app_name, key_name);
    if (str && str->length() == 4 && _str(*str).compareIgnoreCase("true"))
        return 1;
    if (str && str->length() == 5 && _str(*str).compareIgnoreCase("false"))
        return 0;
    return str ? _str(*str).toInt() : def_val;
}

void ConfigFile::SetStr(const string& app_name, const string& key_name, const string& val)
{
    auto it_app = appKeyValues.find(app_name);
    if (it_app == appKeyValues.end())
    {
        StrMap key_values;
        key_values[key_name] = val;
        auto it = appKeyValues.insert(std::make_pair(app_name, key_values));
        appKeyValuesOrder.push_back(it);
    }
    else
    {
        it_app->second[key_name] = val;
    }
}

void ConfigFile::SetInt(const string& app_name, const string& key_name, int val)
{
    SetStr(app_name, key_name, _str("{}", val));
}

StrMap& ConfigFile::GetApp(const string& app_name)
{
    auto it = appKeyValues.find(app_name);
    RUNTIME_ASSERT(it != appKeyValues.end());
    return it->second;
}

void ConfigFile::GetApps(const string& app_name, PStrMapVec& key_values)
{
    size_t count = appKeyValues.count(app_name);
    auto it = appKeyValues.find(app_name);
    key_values.reserve(key_values.size() + count);
    for (size_t i = 0; i < count; i++, it++)
        key_values.push_back(&it->second);
}

StrMap& ConfigFile::SetApp(const string& app_name)
{
    auto it = appKeyValues.insert(std::make_pair(app_name, StrMap()));
    appKeyValuesOrder.push_back(it);
    return it->second;
}

bool ConfigFile::IsApp(const string& app_name)
{
    auto it_app = appKeyValues.find(app_name);
    return it_app != appKeyValues.end();
}

bool ConfigFile::IsKey(const string& app_name, const string& key_name)
{
    auto it_app = appKeyValues.find(app_name);
    if (it_app == appKeyValues.end())
        return false;

    return it_app->second.find(key_name) != it_app->second.end();
}

void ConfigFile::GetAppNames(StrSet& apps)
{
    for (const auto& kv : appKeyValues)
        apps.insert(kv.first);
}

void ConfigFile::GotoNextApp(const string& app_name)
{
    auto it_app = appKeyValues.find(app_name);
    if (it_app == appKeyValues.end())
        return;

    auto it = std::find(appKeyValuesOrder.begin(), appKeyValuesOrder.end(), it_app);
    RUNTIME_ASSERT(it != appKeyValuesOrder.end());
    appKeyValuesOrder.erase(it);
    appKeyValues.erase(it_app);
}

const StrMap* ConfigFile::GetAppKeyValues(const string& app_name)
{
    auto it_app = appKeyValues.find(app_name);
    return it_app != appKeyValues.end() ? &it_app->second : nullptr;
}

string ConfigFile::GetAppContent(const string& app_name)
{
    RUNTIME_ASSERT(collectContent);

    auto it_app = appKeyValues.find(app_name);
    if (it_app == appKeyValues.end())
        return nullptr;

    auto it_key = it_app->second.find("");
    return it_key != it_app->second.end() ? it_key->second : nullptr;
}
