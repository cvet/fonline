// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "DataSource.h"

FO_BEGIN_NAMESPACE

namespace DataSourceEmbedded
{
    auto Mount() -> unique_ptr<DataSource>;
}

FO_END_NAMESPACE
