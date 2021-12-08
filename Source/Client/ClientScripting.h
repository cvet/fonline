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

    ///@ ExportEvent Client
    ScriptEvent<> StartEvent {};
    ///@ ExportEvent Client
    ScriptEvent<> FinishEvent {};
    ///@ ExportEvent Client
    ScriptEvent<string /*login*/, string /*password*/> AutoLoginEvent {};
    ///@ ExportEvent Client
    ScriptEvent<> LoopEvent {};
    ///@ ExportEvent Client
    ScriptEvent<vector<int>& /*screens*/> GetActiveScreensEvent {};
    ///@ ExportEvent Client
    ScriptEvent<bool /*show*/, int /*screen*/, map<string, int> /*data*/> ScreenChangeEvent {};
    ///@ ExportEvent Client
    ScriptEvent<int /*offsetX*/, int /*offsetY*/> ScreenScrollEvent {};
    ///@ ExportEvent Client
    ScriptEvent<> RenderIfaceEvent {};
    ///@ ExportEvent Client
    ScriptEvent<> RenderMapEvent {};
    ///@ ExportEvent Client
    ScriptEvent<MouseButton /*button*/> MouseDownEvent {};
    ///@ ExportEvent Client
    ScriptEvent<MouseButton /*button*/> MouseUpEvent {};
    ///@ ExportEvent Client
    ScriptEvent<int /*offsetX*/, int /*offsetY*/> MouseMoveEvent {};
    ///@ ExportEvent Client
    ScriptEvent<KeyCode /*key*/, string /*text*/> KeyDownEvent {};
    ///@ ExportEvent Client
    ScriptEvent<KeyCode /*key*/> KeyUpEvent {};
    ///@ ExportEvent Client
    ScriptEvent<> InputLostEvent {};
    ///@ ExportEvent Client
    ScriptEvent<CritterView* /*critter*/> CritterInEvent {};
    ///@ ExportEvent Client
    ScriptEvent<CritterView* /*critter*/> CritterOutEvent {};
    ///@ ExportEvent Client
    ScriptEvent<ItemView* /*item*/> ItemMapInEvent {};
    ///@ ExportEvent Client
    ScriptEvent<ItemView* /*item*/, ItemView* /*oldItem*/> ItemMapChangedEvent {};
    ///@ ExportEvent Client
    ScriptEvent<ItemView* /*item*/> ItemMapOutEvent {};
    ///@ ExportEvent Client
    ScriptEvent<> ItemInvAllInEvent {};
    ///@ ExportEvent Client
    ScriptEvent<ItemView* /*item*/> ItemInvInEvent {};
    ///@ ExportEvent Client
    ScriptEvent<ItemView* /*item*/, ItemView* /*oldItem*/> ItemInvChangedEvent {};
    ///@ ExportEvent Client
    ScriptEvent<ItemView* /*item*/> ItemInvOutEvent {};
    ///@ ExportEvent Client
    ScriptEvent<> MapLoadEvent {};
    ///@ ExportEvent Client
    ScriptEvent<> MapUnloadEvent {};
    ///@ ExportEvent Client
    ScriptEvent<vector<ItemView*> /*items*/, int /*param*/> ReceiveItemsEvent {};
    ///@ ExportEvent Client
    ScriptEvent<string& /*text*/, ushort& /*hexX*/, ushort& /*hexY*/, uint& /*color*/, uint& /*delay*/> MapMessageEvent {};
    ///@ ExportEvent Client
    ScriptEvent<string /*text*/, int& /*sayType*/, uint& /*critterId*/, uint& /*delay*/> InMessageEvent {};
    ///@ ExportEvent Client
    ScriptEvent<string& /*text*/, int& /*sayType*/> OutMessageEvent {};
    ///@ ExportEvent Client
    ScriptEvent<string /*text*/, uchar /*type*/, bool /*scriptCall*/> MessageBoxEvent {};
    ///@ ExportEvent Client
    ScriptEvent<vector<uint> /*result*/> CombatResultEvent {};
    ///@ ExportEvent Client
    ScriptEvent<ItemView* /*item*/, uint /*count*/, Entity* /*from*/, Entity* /*to*/> ItemCheckMoveEvent {};
    ///@ ExportEvent Client
    ScriptEvent<bool /*localCall*/, CritterView* /*critter*/, int /*action*/, int /*actionExt*/, ItemView* /*actionItem*/> CritterActionEvent {};
    ///@ ExportEvent Client
    ScriptEvent<bool /*arg1*/, CritterView* /*arg2*/, uint /*arg3*/, uint /*arg4*/, ItemView* /*arg5*/> Animation2dProcessEvent {};
    ///@ ExportEvent Client
    ScriptEvent<bool /*arg1*/, CritterView* /*arg2*/, uint /*arg3*/, uint /*arg4*/, ItemView* /*arg5*/> Animation3dProcessEvent {};
    ///@ ExportEvent Client
    ScriptEvent<hash /*arg1*/, uint /*arg2*/, uint /*arg3*/, uint& /*arg4*/, uint& /*arg5*/, int& /*arg6*/, int& /*arg7*/, string& /*arg8*/> CritterAnimationEvent {};
    ///@ ExportEvent Client
    ScriptEvent<hash /*arg1*/, uint /*arg2*/, uint /*arg3*/, hash& /*arg4*/, uint& /*arg5*/, uint& /*arg6*/> CritterAnimationSubstituteEvent {};
    ///@ ExportEvent Client
    ScriptEvent<hash /*arg1*/, uint& /*arg2*/, uint& /*arg3*/, uint& /*arg4*/, uint& /*arg5*/, uint& /*arg6*/> CritterAnimationFalloutEvent {};
    ///@ ExportEvent Client
    ScriptEvent<CritterView* /*critter*/, ItemView* /*item*/, char /*toSlot*/> CritterCheckMoveItemEvent {};
    ///@ ExportEvent Client
    ScriptEvent<CritterView* /*critter*/, ItemView* /*item*/, char /*itemMode*/, uint& /*dist*/> CritterGetAttackDistantionEvent {};

private:
    void InitNativeScripting();
    void InitAngelScriptScripting();
    void InitMonoScripting();
};
