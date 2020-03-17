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

#pragma once

#include "Common.h"

#include "ScriptSystem.h"

class Entity;
class Item;
class Critter;
class Map;
class Location;

class ServerScriptSystem : public ScriptSystem
{
public:
    ServerScriptSystem(void* obj, GlobalSettings& sett, FileManager& file_mngr) : ScriptSystem(obj, sett, file_mngr) {}

#define FO_API_SERVER_EVENT(name, ...) ScriptEvent<__VA_ARGS__> name##Event {};
#define FO_API_ARG(type, name) type
#define FO_API_ARG_ARR(type, name) vector<type>
#define FO_API_ARG_OBJ(type, name) type*
#define FO_API_ARG_OBJ_ARR(type, name) vector<type*>
#define FO_API_ARG_REF(type, name) type&
#define FO_API_ARG_ARR_REF(type, name) vector<type>&
#define FO_API_ARG_ENUM(type, name) int
#include "ScriptApi.h"

protected:
    virtual void InitNativeScripting() override;
    virtual void InitAngelScriptScripting() override;
    virtual void InitMonoScripting() override;
};
