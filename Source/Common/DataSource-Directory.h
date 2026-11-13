// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "DataSource.h"

FO_BEGIN_NAMESPACE

namespace DataSourceDirectory
{
    auto Mount(string_view dir, bool recursive, bool non_cached, bool maybe_not_available) -> unique_ptr<DataSource>;
    auto CreateEmpty() -> unique_ptr<DataSource>;
}

FO_END_NAMESPACE
