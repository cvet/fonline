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

#include "Entity.h"
#include "ScriptSystem.h"

#define FO_API_ITEM_HEADER
#include "ScriptApi.h"

class Item;
using ItemVec = vector<Item*>;
using ItemMap = map<uint, Item*>;
class Critter;
using CritterMap = map<uint, Critter*>;
using CritterVec = vector<Critter*>;

class Item : public Entity
{
    friend class Entity;
    friend class ItemManager;

public:
    Item(uint id, ProtoItem* proto, ScriptSystem& script_sys);

    ProtoItem* GetProtoItem() { return (ProtoItem*)Proto; }
    void SetProto(ProtoItem* proto);
    bool SetScript(asIScriptFunction* func, bool first_time);
    void SetSortValue(ItemVec& items);
    void ChangeCount(int val);

    Item* ContGetItem(uint item_id, bool skip_hide);
    void ContGetAllItems(ItemVec& items, bool skip_hide);
    Item* ContGetItemByPid(hash pid, uint stack_id);
    void ContGetItems(ItemVec& items, uint stack_id);
    bool ContIsItems();

    bool IsStatic() { return GetIsStatic(); }
    bool IsAnyScenery() { return IsScenery() || IsWall(); }
    bool IsScenery() { return GetIsScenery(); }
    bool IsWall() { return GetIsWall(); }
    bool RadioIsSendActive() { return !FLAG(GetRadioFlags(), RADIO_DISABLE_SEND); }
    bool RadioIsRecvActive() { return !FLAG(GetRadioFlags(), RADIO_DISABLE_RECV); }

    bool ViewPlaceOnMap {};
    uint SceneryScriptBindId {};
    Critter* ViewByCritter {};

#define FO_API_ITEM_CLASS
#include "ScriptApi.h"

    PROPERTIES_HEADER();
#define FO_API_ITEM_PROPERTY CLASS_PROPERTY
#include "ScriptApi.h"

private:
    ScriptSystem& scriptSys;
    ItemVec* childItems {};
};
