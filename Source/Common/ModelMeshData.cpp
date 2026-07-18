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

#include "ModelMeshData.h"

#if FO_ENABLE_3D

FO_BEGIN_NAMESPACE

void WriteModelMeshHeader(DataWriter& writer)
{
    FO_STACK_TRACE_ENTRY();

    writer.WriteBytes({MODEL_MESH_MAGIC.data(), MODEL_MESH_MAGIC.size()});
    writer.Write<uint16_t>(MODEL_MESH_SCHEMA_VERSION);
    writer.Write<uint16_t>(MODEL_MESH_SUPPORTED_FLAGS);
}

void ReadModelMeshHeader(DataReader& reader, string_view context)
{
    FO_STACK_TRACE_ENTRY();

    DataReader source_reader = reader;
    const_span<uint8_t> header_data;

    try {
        header_data = source_reader.ReadBytes(MODEL_MESH_HEADER_SIZE);
    }
    catch (const std::exception& ex) {
        throw ModelMeshDataException("Truncated baked model mesh header", context, ex.what());
    }

    DataReader header_reader {header_data};
    const const_span<uint8_t> magic = header_reader.ReadBytes(MODEL_MESH_MAGIC.size());

    if (!std::equal(magic.begin(), magic.end(), MODEL_MESH_MAGIC.begin())) {
        throw ModelMeshDataException("Invalid baked model mesh magic; expected 'LFMODMSH'", context);
    }

    const uint16_t schema = header_reader.Read<uint16_t>();

    if (schema != MODEL_MESH_SCHEMA_VERSION) {
        throw ModelMeshDataException("Unsupported baked model mesh schema", context, MODEL_MESH_SCHEMA_VERSION, schema);
    }

    const uint16_t flags = header_reader.Read<uint16_t>();

    if ((flags & ~MODEL_MESH_SUPPORTED_FLAGS) != 0) {
        throw ModelMeshDataException("Baked model mesh contains unsupported flags", context, flags);
    }

    header_reader.VerifyEnd();
    (void)reader.ReadBytes(MODEL_MESH_HEADER_SIZE);
}

FO_END_NAMESPACE

#endif
