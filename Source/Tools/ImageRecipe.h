// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "ConfigFile.h"

FO_BEGIN_NAMESPACE

struct ImageRecipe
{
    ConfigFile Values;
    map<string, vector<string>> References {};
    string Source {};
};

auto ParseImageRecipe(string_view descriptor_path, string_view text) -> ImageRecipe;

FO_END_NAMESPACE
