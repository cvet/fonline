//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#pragma once

#include "Common.h"

class ConfigFile : public NonCopyable
{
public:
    ConfigFile(const string& str);
    operator bool() { return !appKeyValues.empty(); }
    void CollectContent() { collectContent = true; }
    void AppendData(const string& str);
    string SerializeData();

    string GetStr(const string& app_name, const string& key_name, const string& def_val = "");
    int GetInt(const string& app_name, const string& key_name, int def_val = 0);

    void SetStr(const string& app_name, const string& key_name, const string& val);
    void SetInt(const string& app_name, const string& key_name, int val);

    StrMap& GetApp(const string& app_name);
    void GetApps(const string& app_name, PStrMapVec& key_values);
    StrMap& SetApp(const string& app_name);

    bool IsApp(const string& app_name);
    bool IsKey(const string& app_name, const string& key_name);

    void GetAppNames(StrSet& apps);
    void GotoNextApp(const string& app_name);
    const StrMap* GetAppKeyValues(const string& app_name);
    string GetAppContent(const string& app_name);

private:
    using ValuesMap = multimap<string, StrMap>;
    using ValuesMapItVec = vector<ValuesMap::const_iterator>;

    void ParseStr(const string& str);
    string* GetRawValue(const string& app_name, const string& key_name);

    bool collectContent {};
    ValuesMap appKeyValues {};
    ValuesMapItVec appKeyValuesOrder {};
};
