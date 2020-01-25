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

// Todo: fix pragma parsing as tokens not as strings

#pragma once

#include "Common.h"

#include "DataBase.h"
#include "Entity.h"

#include "angelscript.h"
#include "preprocessor.h"

#define PRAGMA_UNKNOWN (0)
#define PRAGMA_SERVER (1)
#define PRAGMA_CLIENT (2)
#define PRAGMA_MAPPER (3)

class PropertyRegistrator;
class IgnorePragma; // just delete
class GlobalVarPragma; // just delete
class EntityPragma; // class NewEntity { [Protected] uint Field1; }
class PropertyPragma; // extend class Critter { [Protected] uint LockerId; }
class ContentPragma; // improve dynamic scan
class EnumPragma; // extend enum MyEnum { NewA, NewB }
class EventPragma; // [Event] void MyEvent(...)
class RpcPragma; // [ServerRpc] void MyRpc(...)
// add [Export] to allow access from other modules
// ???

class EntityManager;
class ProtoManager;
using Pragmas = vector<Preprocessor::PragmaInstance>;

class ScriptPragmaCallback : public Preprocessor::PragmaCallback
{
private:
    int pragmaType;
    Pragmas processedPragmas;
    bool isError;
    IgnorePragma* ignorePragma;
    GlobalVarPragma* globalVarPragma;
    EntityPragma* entityPragma;
    PropertyPragma* propertyPragma;
    ContentPragma* contentPragma;
    EnumPragma* enumPragma;
    EventPragma* eventPragma;
    RpcPragma* rpcPragma;

public:
    ScriptPragmaCallback(int pragma_type, ProtoManager* proto_mngr, EntityManager* entity_mngr);
    ~ScriptPragmaCallback();
    virtual void CallPragma(const Preprocessor::PragmaInstance& pragma);
    const Pragmas& GetProcessedPragmas();
    void Finish();
    bool IsError();
    PropertyRegistrator** GetPropertyRegistrators();
    StrVec GetCustomEntityTypes();
    // Todo: rework FONLINE_
    /*#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
    bool RestoreCustomEntity(const string& class_name, uint id, const DataBase::Document& doc);
    #endif*/
    void* FindInternalEvent(const string& event_name);
    bool RaiseInternalEvent(void* event_ptr, va_list args);
    const IntVec& GetInternalEventArgInfos(void* event_ptr);
    void RemoveEventsEntity(Entity* entity);
    void HandleRpc(void* context);
};
