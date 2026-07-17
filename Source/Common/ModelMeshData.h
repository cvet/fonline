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

#pragma once

#include "Common.h"

#if FO_ENABLE_3D

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ModelMeshDataException);

inline constexpr array<uint8_t, 8> MODEL_MESH_MAGIC {'L', 'F', 'M', 'O', 'D', 'M', 'S', 'H'};
inline constexpr uint16_t MODEL_MESH_SCHEMA_VERSION = 1;
inline constexpr uint16_t MODEL_MESH_SUPPORTED_FLAGS = 0;
inline constexpr size_t MODEL_MESH_HEADER_SIZE = MODEL_MESH_MAGIC.size() + sizeof(uint16_t) + sizeof(uint16_t);
inline constexpr uint32_t MODEL_MESH_MAX_HIERARCHY_DEPTH = 128;
inline constexpr float32_t MODEL_MESH_SKIN_WEIGHT_SUM_TOLERANCE = 1.0e-4f;

// Wire order: magic[8], schema:u16, flags:u16. The mesh payload immediately follows the header.
void WriteModelMeshHeader(DataWriter& writer);
void ReadModelMeshHeader(DataReader& reader, string_view context);

FO_END_NAMESPACE

#endif
