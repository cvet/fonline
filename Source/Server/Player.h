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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "ClientConnection.h"
#include "EntityProperties.h"
#include "EntityProtos.h"
#include "ServerEntity.h"

class Item;
class Critter;
class Map;
class Location;
class MapManager;

class Player final : public ServerEntity, public PlayerProperties
{
public:
    Player() = delete;
    Player(FOServer* engine, ident_t id, ClientConnection* connection, const Properties* props = nullptr);
    Player(const Player&) = delete;
    Player(Player&&) noexcept = delete;
    auto operator=(const Player&) = delete;
    auto operator=(Player&&) noexcept = delete;
    ~Player() override;

    [[nodiscard]] auto GetIp() const -> uint;
    [[nodiscard]] auto GetHost() const -> string_view;
    [[nodiscard]] auto GetPort() const -> uint16;

    [[nodiscard]] auto GetControlledCritter() const -> const Critter* { return _controlledCr; }
    [[nodiscard]] auto GetControlledCritter() -> Critter* { return _controlledCr; }

    void SetName(string_view name);
    void SetControlledCritter(Critter* cr);

    void Send_LoginSuccess();
    void Send_Moving(const Critter* from_cr);
    void Send_Dir(const Critter* from_cr);
    void Send_AddCritter(const Critter* cr);
    void Send_RemoveCritter(const Critter* cr);
    void Send_LoadMap(const Map* map);
    void Send_Property(NetProperty type, const Property* prop, const Entity* entity);
    void Send_AddItemOnMap(const Item* item);
    void Send_RemoveItemFromMap(const Item* item);
    void Send_AnimateItem(const Item* item, hstring anim_name, bool looped, bool reversed);
    void Send_ChosenAddItem(const Item* item);
    void Send_ChosenRemoveItem(const Item* item);
    void Send_GlobalInfo(uint8 flags);
    void Send_GlobalLocation(const Location* loc, bool add);
    void Send_GlobalMapFog(uint16 zx, uint16 zy, uint8 fog);
    void Send_Teleport(const Critter* cr, uint16 to_hx, uint16 to_hy);
    void Send_Talk();
    void Send_TimeSync();
    void Send_Text(const Critter* from_cr, string_view text, uint8 how_say);
    void Send_TextEx(ident_t from_id, string_view text, uint8 how_say, bool unsafe_text);
    void Send_TextMsg(const Critter* from_cr, uint8 how_say, TextPackName text_pack, TextPackKey str_num);
    void Send_TextMsg(ident_t from_id, uint8 how_say, TextPackName text_pack, TextPackKey str_num);
    void Send_TextMsgLex(const Critter* from_cr, uint8 how_say, TextPackName text_pack, TextPackKey str_num, string_view lexems);
    void Send_TextMsgLex(ident_t from_id, uint8 how_say, TextPackName text_pack, TextPackKey str_num, string_view lexems);
    void Send_Action(const Critter* from_cr, CritterAction action, int action_data, const Item* context_item);
    void Send_MoveItem(const Critter* from_cr, const Item* moved_item, CritterAction action, CritterItemSlot prev_slot);
    void Send_Animate(const Critter* from_cr, CritterStateAnim state_anim, CritterActionAnim action_anim, const Item* context_item, bool clear_sequence, bool delay_play);
    void Send_SetAnims(const Critter* from_cr, CritterCondition cond, CritterStateAnim state_anim, CritterActionAnim action_anim);
    void Send_AutomapsInfo(const void* locs_vec, const Location* loc);
    void Send_Effect(hstring eff_pid, uint16 hx, uint16 hy, uint16 radius);
    void Send_FlyEffect(hstring eff_pid, ident_t from_cr_id, ident_t to_cr_id, uint16 from_hx, uint16 from_hy, uint16 to_hx, uint16 to_hy);
    void Send_PlaySound(ident_t cr_id_synchronize, string_view sound_name);
    void Send_MapText(uint16 hx, uint16 hy, ucolor color, string_view text, bool unsafe_text);
    void Send_MapTextMsg(uint16 hx, uint16 hy, ucolor color, TextPackName text_pack, TextPackKey str_num);
    void Send_MapTextMsgLex(uint16 hx, uint16 hy, ucolor color, TextPackName text_pack, TextPackKey str_num, string_view lexems);
    void Send_ViewMap();
    void Send_PlaceToGameComplete();
    void Send_AllAutomapsInfo();
    void Send_SomeItems(const vector<Item*>& items, bool owned, bool with_inner_entities, const any_t& context_param);
    void Send_Attachments(const Critter* from_cr);
    void Send_AddCustomEntity(CustomEntity* entity, bool owned);
    void Send_RemoveCustomEntity(ident_t id);

    ///@ ExportEvent
    ENTITY_EVENT(OnGetAccess, int /*arg1*/, string& /*arg2*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnAllowCommand, string /*arg1*/, uint8 /*arg2*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnLogout);

    ClientConnection* Connection {};
    uint8 Access {ACCESS_CLIENT};
    string LastSay {};
    uint LastSayEqualCount {};
    const Entity* SendIgnoreEntity {};
    const Property* SendIgnoreProperty {};

private:
    void SendItem(const Item* item, bool owned, bool with_slot, bool with_inner_entities);
    void SendInnerEntities(const Entity* holder, bool owned);

    Critter* _controlledCr {}; // Todo: allow attach many critters to sigle player
    time_point _talkNextTime {};
};
