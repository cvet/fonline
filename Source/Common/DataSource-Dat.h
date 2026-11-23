// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "DataSource.h"

#if FO_DATA_SOURCE_DAT

FO_BEGIN_NAMESPACE

namespace DataSourceDat
{
    auto Mount(string_view fname) -> unique_ptr<DataSource>;
}

FO_END_NAMESPACE

#endif
