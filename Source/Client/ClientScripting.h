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

#include "Application.h"
#include "ScriptSystem.h"

class Entity;
class PlayerView;
class ItemView;
class CritterView;
class MapView;
class LocationView;

class ClientScriptSystem : public ScriptSystem
{
public:
    ClientScriptSystem(void* obj, GlobalSettings& settings, FileManager& file_mngr) : ScriptSystem(obj, settings, file_mngr)
    {
        InitNativeScripting();
        InitAngelScriptScripting();
        InitMonoScripting();
    }

    ///@ ExportEvent
    ScriptEvent<> StartEvent {};
    ///@ ExportEvent
    ScriptEvent<> FinishEvent {};
    ///@ ExportEvent
    ScriptEvent<string /*login*/, string /*password*/> AutoLoginEvent {};
    ///@ ExportEvent
    ScriptEvent<> LoopEvent {};
    ///@ ExportEvent
    ScriptEvent<vector<int>& /*screens*/> GetActiveScreensEvent {};
    ///@ ExportEvent
    ScriptEvent<bool /*show*/, int /*screen*/, map<string, int> /*data*/> ScreenChangeEvent {};
    ///@ ExportEvent
    ScriptEvent<int /*offsetX*/, int /*offsetY*/> ScreenScrollEvent {};
    ///@ ExportEvent
    ScriptEvent<> RenderIfaceEvent {};
    ///@ ExportEvent
    ScriptEvent<> RenderMapEvent {};
    ///@ ExportEvent
    ScriptEvent<MouseButton /*button*/> MouseDownEvent {};
    ///@ ExportEvent
    ScriptEvent<MouseButton /*button*/> MouseUpEvent {};
    ///@ ExportEvent
    ScriptEvent<int /*offsetX*/, int /*offsetY*/> MouseMoveEvent {};
    ///@ ExportEvent
    ScriptEvent<KeyCode /*key*/, string /*text*/> KeyDownEvent {};
    ///@ ExportEvent
    ScriptEvent<KeyCode /*key*/> KeyUpEvent {};
    ///@ ExportEvent
    ScriptEvent<> InputLostEvent {};
    ///@ ExportEvent
    ScriptEvent<CritterView* /*critter*/> CritterInEvent {};
    ///@ ExportEvent
    ScriptEvent<CritterView* /*critter*/> CritterOutEvent {};
    ///@ ExportEvent
    ScriptEvent<ItemView* /*item*/> ItemMapInEvent {};
    ///@ ExportEvent
    ScriptEvent<ItemView* /*item*/, ItemView* /*oldItem*/> ItemMapChangedEvent {};
    ///@ ExportEvent
    ScriptEvent<ItemView* /*item*/> ItemMapOutEvent {};
    ///@ ExportEvent
    ScriptEvent<> ItemInvAllInEvent {};
    ///@ ExportEvent
    ScriptEvent<ItemView* /*item*/> ItemInvInEvent {};
    ///@ ExportEvent
    ScriptEvent<ItemView* /*item*/, ItemView* /*oldItem*/> ItemInvChangedEvent {};
    ///@ ExportEvent
    ScriptEvent<ItemView* /*item*/> ItemInvOutEvent {};
    ///@ ExportEvent
    ScriptEvent<> MapLoadEvent {};
    ///@ ExportEvent
    ScriptEvent<> MapUnloadEvent {};
    ///@ ExportEvent
    ScriptEvent<vector<ItemView*> /*items*/, int /*param*/> ReceiveItemsEvent {};
    ///@ ExportEvent
    ScriptEvent<string& /*text*/, ushort& /*hexX*/, ushort& /*hexY*/, uint& /*color*/, uint& /*delay*/> MapMessageEvent {};
    ///@ ExportEvent
    ScriptEvent<string /*text*/, int& /*sayType*/, uint& /*critterId*/, uint& /*delay*/> InMessageEvent {};
    ///@ ExportEvent
    ScriptEvent<string& /*text*/, int& /*sayType*/> OutMessageEvent {};
    ///@ ExportEvent
    ScriptEvent<string /*text*/, uchar /*type*/, bool /*scriptCall*/> MessageBoxEvent {};
    ///@ ExportEvent
    ScriptEvent<vector<uint> /*result*/> CombatResultEvent {};
    ///@ ExportEvent
    ScriptEvent<ItemView* /*item*/, uint /*count*/, Entity* /*from*/, Entity* /*to*/> ItemCheckMoveEvent {};
    ///@ ExportEvent
    ScriptEvent<bool /*localCall*/, CritterView* /*critter*/, int /*action*/, int /*actionExt*/, ItemView* /*actionItem*/> CritterActionEvent {};
    ///@ ExportEvent
    ScriptEvent<bool /*arg1*/, CritterView* /*arg2*/, uint /*arg3*/, uint /*arg4*/, ItemView* /*arg5*/> Animation2dProcessEvent {};
    ///@ ExportEvent
    ScriptEvent<bool /*arg1*/, CritterView* /*arg2*/, uint /*arg3*/, uint /*arg4*/, ItemView* /*arg5*/> Animation3dProcessEvent {};
    ///@ ExportEvent
    ScriptEvent<hash /*arg1*/, uint /*arg2*/, uint /*arg3*/, uint& /*arg4*/, uint& /*arg5*/, int& /*arg6*/, int& /*arg7*/, string& /*arg8*/> CritterAnimationEvent {};
    ///@ ExportEvent
    ScriptEvent<hash /*arg1*/, uint /*arg2*/, uint /*arg3*/, hash& /*arg4*/, uint& /*arg5*/, uint& /*arg6*/> CritterAnimationSubstituteEvent {};
    ///@ ExportEvent
    ScriptEvent<hash /*arg1*/, uint& /*arg2*/, uint& /*arg3*/, uint& /*arg4*/, uint& /*arg5*/, uint& /*arg6*/> CritterAnimationFalloutEvent {};
    ///@ ExportEvent
    ScriptEvent<CritterView* /*critter*/, ItemView* /*item*/, char /*toSlot*/> CritterCheckMoveItemEvent {};
    ///@ ExportEvent
    ScriptEvent<CritterView* /*critter*/, ItemView* /*item*/, char /*itemMode*/, uint& /*dist*/> CritterGetAttackDistantionEvent {};

private:
    void InitNativeScripting();
    void InitAngelScriptScripting();
    void InitMonoScripting();
};
