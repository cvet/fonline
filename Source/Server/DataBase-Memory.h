// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "DataBase.h"

FO_BEGIN_NAMESPACE

namespace DataBaseMemory
{
    auto Create(ptr<DataBaseSettings> db_settings, DataBasePanicCallback panic_callback) -> unique_ptr<DataBaseImpl>;
}

FO_END_NAMESPACE
