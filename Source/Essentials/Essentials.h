//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

// Essential headers
// In order of dependency
//
// This is a strict layering — each Essentials header may only include (directly or transitively
// from another Essentials header) the headers listed above it in this block. The same rule applies
// to each Essentials .cpp: it must not pull in an Essentials header that sits below its own .h.
// Crossing the layer downward (e.g. MemorySystem.cpp including Platform.h) creates a cycle in the
// dependency DAG, breaks the bootstrap order (MemorySystem must be usable before Platform is
// initialised), and tangles features that need to stay independently buildable.
//
// If a low-layer module needs information that physically lives in a higher layer (e.g. an
// allocator query that would otherwise want OS process stats as a fallback), the lower layer must
// either expose only what it can produce on its own or let the caller supply the higher-layer
// data through a parameter — do not back-channel the include.

// clang-format off
#include "BasicCore.h"
#include "GlobalData.h"
#include "StackTrace.h"
#include "BaseLogging.h"
#include "SmartPointers.h"
#include "MemorySystem.h"
#include "Containers.h"
#include "StringUtils.h"
#include "Platform.h"
#include "ExceptionHandling.h"
#include "Threading.h"
#include "SafeArithmetics.h"
#include "DataSerialization.h"
#include "HashedString.h"
#include "StrongType.h"
#include "TimeRelated.h"
#include "ExtendedTypes.h"
#include "Compressor.h"
#include "WorkThread.h"
#include "Logging.h"
#include "DiskFileSystem.h"
#include "CommonHelpers.h"
#include "NetSockets.h"
// clang-format on
