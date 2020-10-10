//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Common.h"

GlobalDataCallback CreateGlobalDataCallbacks[MAX_GLOBAL_DATA_CALLBACKS];
GlobalDataCallback DeleteGlobalDataCallbacks[MAX_GLOBAL_DATA_CALLBACKS];
int GlobalDataCallbacksCount;

void CreateGlobalData()
{
    for (auto i = 0; i < GlobalDataCallbacksCount; i++) {
        CreateGlobalDataCallbacks[i]();
    }
}

void DeleteGlobalData()
{
    for (auto i = 0; i < GlobalDataCallbacksCount; i++) {
        DeleteGlobalDataCallbacks[i]();
    }
}

// Dummy symbols for web build to avoid linker errors
#ifdef FO_WEB
void* SDL_LoadObject(const char* sofile)
{
    throw UnreachablePlaceException(LINE_STR);
}

void* SDL_LoadFunction(void* handle, const char* name)
{
    throw UnreachablePlaceException(LINE_STR);
}

void SDL_UnloadObject(void* handle)
{
    throw UnreachablePlaceException(LINE_STR);
}
#endif

struct FormatsAppData
{
    unordered_set<string> SupportedFormats {"fo3d", "fbx", "x", "3ds", "obj", "dae", "blend", "ase", "ply", "dxf", "lwo", "lxo", "stl", "ms3d", "scn", "smd", "vta", "mdl", "md2", "md3", "pk3", "mdc", "md5", "bvh", "csm", "b3d", "q3d", "cob", "q3s", "mesh", "xml", "irrmesh", "irr", "nff", "nff", "off", "raw", "ter", "mdl", "hmp", "ndo", "ac"};
};
GLOBAL_DATA(FormatsAppData, FormatsData);

auto Is3dExtensionSupported(const string& ext) -> bool
{
    return FormatsData->SupportedFormats.count(ext) != 0u;
}
