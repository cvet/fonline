// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_DATABASE_SQLITE
#include "DataBase.h"

FO_BEGIN_NAMESPACE

namespace DataBaseSQLite
{
    auto Create(ptr<DataBaseSettings> db_settings, string_view storage_dir, DataBasePanicCallback panic_callback) -> unique_ptr<DataBaseImpl>;
    void InitializeRuntime();
}

FO_END_NAMESPACE
#endif
