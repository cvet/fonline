// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_DATABASE_JSON
#include "DataBase.h"

FO_BEGIN_NAMESPACE

namespace DataBaseJson
{
    auto Create(ptr<DataBaseSettings> db_settings, string_view storage_dir, DataBasePanicCallback panic_callback) -> unique_ptr<DataBaseImpl>;
}

FO_END_NAMESPACE
#endif
