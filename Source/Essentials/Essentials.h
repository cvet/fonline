// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

// Strict dependency order: each Essentials header and implementation may depend only on entries above it.
// Pass higher-layer data downward explicitly instead of creating an include cycle

// clang-format off
#include "BasicCore.h"
#include "GlobalData.h"
#include "StackTrace.h"
#include "BaseLogging.h"
#include "FatalError.h"
#include "FunctionObjects.h"
#include "SmartPointers.h"
#include "MemorySystem.h"
#include "StringObject.h"
#include "DequeObject.h"
#include "Containers.h"
#include "StringUtils.h"
#include "WinApi.h"
#include "Posix.h"
#include "Platform.h"
#include "ExceptionHandling.h"
#include "RandomGenerator.h"
#include "Threading.h"
#include "SafeArithmetics.h"
#include "DataSerialization.h"
#include "HashedString.h"
#include "StrongType.h"
#include "TimeRelated.h"
#include "ExtendedTypes.h"
#include "Compressor.h"
#include "Cryptography.h"
#include "WorkThread.h"
#include "Logging.h"
#include "DiskFileSystem.h"
#include "CommonHelpers.h"
#include "NetSockets.h"
// clang-format on
