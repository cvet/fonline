// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_DATABASE_MONGO
#include "DataBase.h"

FO_BEGIN_NAMESPACE

namespace DataBaseMongo
{
    auto Create(ptr<DataBaseSettings> db_settings, string_view uri, string_view db_name, DataBasePanicCallback panic_callback) -> unique_ptr<DataBaseImpl>;
}

FO_END_NAMESPACE
#endif
