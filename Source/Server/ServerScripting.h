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
class Player;
class Item;
class Critter;
class Map;
class Location;

class ServerScriptSystem : public ScriptSystem
{
public:
    ServerScriptSystem(void* obj, GlobalSettings& settings, FileManager& file_mngr) : ScriptSystem(obj, settings, file_mngr)
    {
        InitNativeScripting();
        InitAngelScriptScripting();
        InitMonoScripting();
    }

      ///@ ExportEvent Server
    ScriptEvent<> InitEvent {};
    ///@ ExportEvent Server
    ScriptEvent<> GenerateWorldEvent {};
    ///@ ExportEvent Server
    ScriptEvent<> StartEvent {};
    ///@ ExportEvent Server
    ScriptEvent<> FinishEvent {};
    ///@ ExportEvent Server
    ScriptEvent<> LoopEvent {};
    ///@ ExportEvent Server
    ScriptEvent<uint /*ip*/, string /*name*/, uint& /*disallowMsgNum*/, uint& /*disallowStrNum*/, string& /*disallowLex*/> PlayerRegistrationEvent {};
    ///@ ExportEvent Server
    ScriptEvent<uint /*ip*/, string /*name*/, uint /*id*/, uint& /*disallowMsgNum*/, uint& /*disallowStrNum*/, string& /*disallowLex*/> PlayerLoginEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Player* /*player*/, int /*arg1*/, string& /*arg2*/> PlayerGetAccessEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Player* /*player*/, string /*arg1*/, uchar /*arg2*/> PlayerAllowCommandEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Player* /*player*/> PlayerLogoutEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Critter* /*critter*/> GlobalMapCritterInEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Critter* /*critter*/> GlobalMapCritterOutEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Location* /*location*/, bool /*firstTime*/> LocationInitEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Location* /*location*/> LocationFinishEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Map* /*map*/, bool /*firstTime*/> MapInitEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Map* /*map*/> MapFinishEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Map* /*map*/, uint /*loopIndex*/> MapLoopEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Map* /*map*/, Critter* /*critter*/> MapCritterInEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Map* /*map*/, Critter* /*critter*/> MapCritterOutEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Map* /*map*/, Critter* /*critter*/, Critter* /*target*/> MapCheckLookEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Map* /*map*/, Critter* /*critter*/, Item* /*item*/> MapCheckTrapLookEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Critter* /*critter*/, bool /*firstTime*/> CritterInitEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Critter* /*critter*/> CritterFinishEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Critter* /*critter*/> CritterIdleEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Critter* /*critter*/> CritterGlobalMapIdleEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Critter* /*critter*/, Item* /*item*/, uchar /*toSlot*/> CritterCheckMoveItemEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Critter* /*critter*/, Item* /*item*/, uchar /*fromSlot*/> CritterMoveItemEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Critter* /*critter*/, Critter* /*showCritter*/> CritterShowEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Critter* /*critter*/, Critter* /*showCritter*/> CritterShowDist1Event {};
    ///@ ExportEvent Server
    ScriptEvent<Critter* /*critter*/, Critter* /*showCritter*/> CritterShowDist2Event {};
    ///@ ExportEvent Server
    ScriptEvent<Critter* /*critter*/, Critter* /*showCritter*/> CritterShowDist3Event {};
    ///@ ExportEvent Server
    ScriptEvent<Critter* /*critter*/, Critter* /*hideCritter*/> CritterHideEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Critter* /*critter*/, Critter* /*hideCritter*/> CritterHideDist1Event {};
    ///@ ExportEvent Server
    ScriptEvent<Critter* /*critter*/, Critter* /*hideCritter*/> CritterHideDist2Event {};
    ///@ ExportEvent Server
    ScriptEvent<Critter* /*critter*/, Critter* /*hideCritter*/> CritterHideDist3Event {};
    ///@ ExportEvent Server
    ScriptEvent<Critter* /*critter*/, Item* /*item*/, bool /*added*/, Critter* /*fromCritter*/> CritterShowItemOnMapEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Critter* /*critter*/, Item* /*item*/, bool /*removed*/, Critter* /*toCritter*/> CritterHideItemOnMapEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Critter* /*critter*/, Item* /*item*/> CritterChangeItemOnMapEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Critter* /*critter*/, Critter* /*receiver*/, int /*num*/, int /*value*/> CritterMessageEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Critter* /*critter*/, Critter* /*who*/, bool /*begin*/, uint /*talkers*/> CritterTalkEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Critter* /*cr*/, Critter* /*trader*/, bool /*begin*/, uint /*barterCount*/> CritterBarterEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Critter* /*critter*/, Item* /*item*/, uchar /*itemMode*/, uint& /*dist*/> CritterGetAttackDistantionEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Item* /*item*/, bool /*firstTime*/> ItemInitEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Item* /*item*/> ItemFinishEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Item* /*item*/, Critter* /*critter*/, bool /*isIn*/, uchar /*dir*/> ItemWalkEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Item* /*item*/, uint /*count*/, Entity* /*from*/, Entity* /*to*/> ItemCheckMoveEvent {};
    ///@ ExportEvent Server
    ScriptEvent<Item* /*item*/, Critter* /*critter*/, bool /*isIn*/, uchar /*dir*/> StaticItemWalkEvent {};

private:
    void InitNativeScripting();
    void InitAngelScriptScripting();
    void InitMonoScripting();
};
