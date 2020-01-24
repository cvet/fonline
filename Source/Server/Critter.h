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

#include "Dialogs.h"
#include "Entity.h"
#include "GeometryHelper.h"
#include "Networking.h"
#include "Settings.h"
#include "Timer.h"

#define CLIENT_OUTPUT_BEGIN(cl) \
    { \
        SCOPE_LOCK(cl->Connection->BoutLocker)
#define CLIENT_OUTPUT_END(cl) \
    } \
    cl->Connection->Dispatch()

// Client game states
#define STATE_NONE (0)
#define STATE_CONNECTED (1)
#define STATE_PLAYING (2)
#define STATE_TRANSFERRING (3)

class MapManager;
class Item;
using ItemVec = vector<Item*>;
using ItemMap = map<uint, Item*>;
class Critter;
using CritterMap = map<uint, Critter*>;
using CritterVec = vector<Critter*>;
class Npc;
using NpcMap = map<uint, Npc*>;
using NpcVec = vector<Npc*>;
class Client;
using ClientMap = map<uint, Client*>;
using ClientVec = vector<Client*>;
class Map;
using MapVec = vector<Map*>;
using MapMap = map<uint, Map*>;
class Location;
using LocationVec = vector<Location*>;
using LocationMap = map<uint, Location*>;

class Critter : public Entity
{
    friend class CritterManager;

public:
    static bool SlotEnabled[0x100];
    static bool SlotDataSendEnabled[0x100];

    bool CritterIsNpc;
    uint Flags;
    string Name;
    bool IsRunning;
    int LockMapTransfers;
    uint AllowedToDownloadMap;

    CritterVec VisCr;
    CritterVec VisCrSelf;
    CritterMap VisCrMap;
    CritterMap VisCrSelfMap;
    UIntSet VisCr1;
    UIntSet VisCr2;
    UIntSet VisCr3;
    UIntSet VisItem;
    uint ViewMapId;
    hash ViewMapPid;
    ushort ViewMapLook;
    ushort ViewMapHx;
    ushort ViewMapHy;
    uchar ViewMapDir;
    uint ViewMapLocId;
    uint ViewMapLocEnt;

    struct
    {
        int State;
        uint TargId;
        ushort HexX;
        ushort HexY;
        uint Cut;
        bool IsRun;
        uint PathNum;
        uint Iter;
        uint Trace;
        uint GagEntityId;
    } Moving;

    uint CacheValuesNextTick;
    uint LookCacheValue;

    int DisableSend;
    CritterVec* GlobalMapGroup;
    bool CanBeRemoved;

    PROPERTIES_HEADER();
    CLASS_PROPERTY(hash, ModelName);
    CLASS_PROPERTY(uint, MapId);
    CLASS_PROPERTY(hash, RefMapId);
    CLASS_PROPERTY(hash, RefMapPid);
    CLASS_PROPERTY(uint, RefLocationId);
    CLASS_PROPERTY(hash, RefLocationPid);
    CLASS_PROPERTY(ushort, HexX);
    CLASS_PROPERTY(ushort, HexY);
    CLASS_PROPERTY(uchar, Dir);
    CLASS_PROPERTY(string, Password);
    CLASS_PROPERTY(int, Cond); // enum CritterCondition
    CLASS_PROPERTY(bool, ClientToDelete);
    CLASS_PROPERTY(uint, Multihex);
    CLASS_PROPERTY(ushort, WorldX);
    CLASS_PROPERTY(ushort, WorldY);
    CLASS_PROPERTY(uint, GlobalMapLeaderId);
    CLASS_PROPERTY(uint, GlobalMapTripId);
    CLASS_PROPERTY(uint, RefGlobalMapLeaderId);
    CLASS_PROPERTY(uint, RefGlobalMapTripId);
    CLASS_PROPERTY(ushort, LastMapHexX);
    CLASS_PROPERTY(ushort, LastMapHexY);
    CLASS_PROPERTY(uint, Anim1Life);
    CLASS_PROPERTY(uint, Anim1Knockout);
    CLASS_PROPERTY(uint, Anim1Dead);
    CLASS_PROPERTY(uint, Anim2Life);
    CLASS_PROPERTY(uint, Anim2Knockout);
    CLASS_PROPERTY(uint, Anim2Dead);
    CLASS_PROPERTY(CScriptArray*, GlobalMapFog);
    CLASS_PROPERTY(CScriptArray*, TE_FuncNum); // hash
    CLASS_PROPERTY(CScriptArray*, TE_Rate); // uint
    CLASS_PROPERTY(CScriptArray*, TE_NextTime); // uint
    CLASS_PROPERTY(CScriptArray*, TE_Identifier); // int
    CLASS_PROPERTY(uint, LookDistance);
    CLASS_PROPERTY(hash, DialogId);
    CLASS_PROPERTY(bool, IsNoTalk);
    CLASS_PROPERTY(int, MaxTalkers); // Callback on begin dialog?
    CLASS_PROPERTY(uint, TalkDistance);
    CLASS_PROPERTY(int, CurrentHp);
    CLASS_PROPERTY(bool, IsNoWalk);
    CLASS_PROPERTY(bool, IsNoRun);
    CLASS_PROPERTY(bool, IsNoRotate);
    CLASS_PROPERTY(uint, WalkTime);
    CLASS_PROPERTY(uint, RunTime);
    CLASS_PROPERTY(int, ScaleFactor);
    CLASS_PROPERTY(uint, TimeoutBattle);
    CLASS_PROPERTY(uint, TimeoutTransfer);
    CLASS_PROPERTY(uint, TimeoutRemoveFromGame);
    CLASS_PROPERTY(bool, IsGeck); // Rename
    CLASS_PROPERTY(bool, IsNoHome);
    CLASS_PROPERTY(uint, HomeMapId);
    CLASS_PROPERTY(ushort, HomeHexX);
    CLASS_PROPERTY(ushort, HomeHexY);
    CLASS_PROPERTY(uchar, HomeDir);
    CLASS_PROPERTY(bool, IsHide);
    CLASS_PROPERTY(CScriptArray*, KnownLocations);
    CLASS_PROPERTY(CScriptArray*, ConnectionIp);
    CLASS_PROPERTY(CScriptArray*, ConnectionPort);
    CLASS_PROPERTY(uint, ShowCritterDist1);
    CLASS_PROPERTY(uint, ShowCritterDist2);
    CLASS_PROPERTY(uint, ShowCritterDist3);
    CLASS_PROPERTY(hash, ScriptId);
    CLASS_PROPERTY(int, SneakCoefficient);
    // Exclude
    CLASS_PROPERTY(hash, NpcRole); // Find Npc criteria (maybe swap to some universal prop/value array as input)
    CLASS_PROPERTY(bool, IsNoUnarmed); // AI

    void ClearVisible();

    Critter* GetCritSelf(uint crid);
    void GetCrFromVisCr(CritterVec& critters, int find_type, bool vis_cr_self);
    Critter* GetGlobalMapCritter(uint cr_id);

    bool AddCrIntoVisVec(Critter* add_cr);
    bool DelCrFromVisVec(Critter* del_cr);

    bool AddCrIntoVisSet1(uint crid);
    bool AddCrIntoVisSet2(uint crid);
    bool AddCrIntoVisSet3(uint crid);
    bool DelCrFromVisSet1(uint crid);
    bool DelCrFromVisSet2(uint crid);
    bool DelCrFromVisSet3(uint crid);

    bool AddIdVisItem(uint item_id);
    bool DelIdVisItem(uint item_id);
    bool CountIdVisItem(uint item_id);

    void SetItem(Item* item);
    Item* GetItem(uint item_id, bool skip_hide);
    ItemVec& GetItemsNoLock() { return invItems; }
    Item* GetItemByPid(hash item_pid);
    Item* GetItemByPidSlot(hash item_pid, int slot);
    Item* GetItemSlot(int slot);
    void GetItemsSlot(int slot, ItemVec& items);
    uint CountItemPid(hash item_pid);
    uint RealCountItems() { return (uint)invItems.size(); }
    uint CountItems();
    ItemVec& GetInventory();
    bool IsHaveGeckItem();

    bool SetScript(asIScriptFunction* func, bool first_time);

    bool IsFree() { return Timer::GameTick() - startBreakTime >= breakTime; }
    bool IsBusy() { return !IsFree(); }
    void SetBreakTime(uint ms);
    void SetBreakTimeDelta(uint ms);
    void SetWait(uint ms) { waitEndTick = Timer::GameTick() + ms; }
    bool IsWait() { return Timer::GameTick() < waitEndTick; }

    bool IsSendDisabled() { return DisableSend > 0; }
    void Send_Property(NetProperty::Type type, Property* prop, Entity* entity);
    void Send_Move(Critter* from_cr, uint move_params);
    void Send_Dir(Critter* from_cr);
    void Send_AddCritter(Critter* cr);
    void Send_RemoveCritter(Critter* cr);
    void Send_LoadMap(Map* map, MapManager& map_mngr);
    void Send_XY(Critter* cr);
    void Send_AddItemOnMap(Item* item);
    void Send_EraseItemFromMap(Item* item);
    void Send_AnimateItem(Item* item, uchar from_frm, uchar to_frm);
    void Send_AddItem(Item* item);
    void Send_EraseItem(Item* item);
    void Send_GlobalInfo(uchar flags, MapManager& map_mngr);
    void Send_GlobalLocation(Location* loc, bool add);
    void Send_GlobalMapFog(ushort zx, ushort zy, uchar fog);
    void Send_CustomCommand(Critter* cr, ushort cmd, int val);
    void Send_AllProperties();
    void Send_Talk();
    void Send_GameInfo(Map* map);
    void Send_Text(Critter* from_cr, const string& text, uchar how_say);
    void Send_TextEx(uint from_id, const string& text, uchar how_say, bool unsafe_text);
    void Send_TextMsg(Critter* from_cr, uint str_num, uchar how_say, ushort num_msg);
    void Send_TextMsg(uint from_id, uint str_num, uchar how_say, ushort num_msg);
    void Send_TextMsgLex(Critter* from_cr, uint num_str, uchar how_say, ushort num_msg, const char* lexems);
    void Send_TextMsgLex(uint from_id, uint num_str, uchar how_say, ushort num_msg, const char* lexems);
    void Send_Action(Critter* from_cr, int action, int action_ext, Item* item);
    void Send_MoveItem(Critter* from_cr, Item* item, uchar action, uchar prev_slot);
    void Send_Animate(Critter* from_cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play);
    void Send_SetAnims(Critter* from_cr, int cond, uint anim1, uint anim2);
    void Send_CombatResult(uint* combat_res, uint len);
    void Send_AutomapsInfo(void* locs_vec, Location* loc);
    void Send_Effect(hash eff_pid, ushort hx, ushort hy, ushort radius);
    void Send_FlyEffect(
        hash eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy);
    void Send_PlaySound(uint crid_synchronize, const string& sound_name);

    // Send all
    void SendA_Property(NetProperty::Type type, Property* prop, Entity* entity);
    void SendA_Move(uint move_params);
    void SendA_XY();
    void SendA_Action(int action, int action_ext, Item* item);
    void SendAA_Action(int action, int action_ext, Item* item);
    void SendAA_MoveItem(Item* item, uchar action, uchar prev_slot);
    void SendAA_Animate(uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play);
    void SendAA_SetAnims(int cond, uint anim1, uint anim2);
    void SendAA_Text(const CritterVec& to_cr, const string& text, uchar how_say, bool unsafe_text);
    void SendAA_Msg(const CritterVec& to_cr, uint num_str, uchar how_say, ushort num_msg);
    void SendAA_MsgLex(const CritterVec& to_cr, uint num_str, uchar how_say, ushort num_msg, const char* lexems);
    void SendA_Dir();
    void SendA_CustomCommand(ushort num_param, int val);

    // Chosen data
    void Send_AddAllItems();
    void Send_AllAutomapsInfo(MapManager& map_mngr);

    bool IsPlayer() { return !CritterIsNpc; }
    bool IsNpc() { return CritterIsNpc; }
    void SendMessage(int num, int val, int to, MapManager& map_mngr);
    uint GetAttackDist(Item* weap, int use);
    bool IsLife();
    bool IsDead();
    bool IsKnockout();
    bool CheckFind(int find_type);

    // Timeouts
    bool IsTransferTimeouts(bool send);

    // Time events
    void AddCrTimeEvent(hash func_num, uint rate, uint duration, int identifier);
    void EraseCrTimeEvent(int index);
    void ContinueTimeEvents(int offs_time);

protected:
    Critter(uint id, EntityType type, ProtoCritter* proto, CritterSettings& sett);
    ~Critter();

    CritterSettings& settings;
    GeometryHelper geomHelper;
    ItemVec invItems {};

private:
    uint startBreakTime {};
    uint breakTime {};
    uint waitEndTick {};
};

class Client : public Critter
{
    friend class CritterManager;

    uint pingNextTick;
    bool pingOk;
    uint talkNextTick;

public:
    NetConnection* Connection;
    uchar Access;
    uint LanguageMsg;
    int GameState;
    uint LastActivityTime;
    uint LastSendedMapTick;
    char LastSay[UTF8_BUF_SIZE(MAX_CHAT_MESSAGE)];
    uint LastSayEqualCount;
    uint RadioMessageSended;
    int UpdateFileIndex;
    uint UpdateFilePortion;
    Talking Talk;

    Client(NetConnection* conn, ProtoCritter* proto, CritterSettings& sett);
    ~Client();

    uint GetIp();
    const char* GetIpStr();
    ushort GetPort();
    bool IsOnline();
    bool IsOffline();
    void Disconnect();
    void RemoveFromGame();
    uint GetOfflineTime();

    bool IsToPing();
    void PingClient();
    void PingOk(uint next_ping);

    void Send_Property(NetProperty::Type type, Property* prop, Entity* entity);
    void Send_Move(Critter* from_cr, uint move_params);
    void Send_Dir(Critter* from_cr);
    void Send_AddCritter(Critter* cr);
    void Send_RemoveCritter(Critter* cr);
    void Send_LoadMap(Map* map, MapManager& map_mngr);
    void Send_XY(Critter* cr);
    void Send_AddItemOnMap(Item* item);
    void Send_EraseItemFromMap(Item* item);
    void Send_AnimateItem(Item* item, uchar from_frm, uchar to_frm);
    void Send_AddItem(Item* item);
    void Send_EraseItem(Item* item);
    void Send_SomeItems(CScriptArray* items_arr, int param);
    void Send_GlobalInfo(uchar flags, MapManager& map_mngr);
    void Send_GlobalLocation(Location* loc, bool add);
    void Send_GlobalMapFog(ushort zx, ushort zy, uchar fog);
    void Send_CustomCommand(Critter* cr, ushort cmd, int val);
    void Send_AllProperties();
    void Send_Talk();
    void Send_GameInfo(Map* map);
    void Send_Text(Critter* from_cr, const string& text, uchar how_say);
    void Send_TextEx(uint from_id, const string& text, uchar how_say, bool unsafe_text);
    void Send_TextMsg(Critter* from_cr, uint str_num, uchar how_say, ushort num_msg);
    void Send_TextMsg(uint from_id, uint str_num, uchar how_say, ushort num_msg);
    void Send_TextMsgLex(Critter* from_cr, uint num_str, uchar how_say, ushort num_msg, const char* lexems);
    void Send_TextMsgLex(uint from_id, uint num_str, uchar how_say, ushort num_msg, const char* lexems);
    void Send_Action(Critter* from_cr, int action, int action_ext, Item* item);
    void Send_MoveItem(Critter* from_cr, Item* item, uchar action, uchar prev_slot);
    void Send_Animate(Critter* from_cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play);
    void Send_SetAnims(Critter* from_cr, int cond, uint anim1, uint anim2);
    void Send_CombatResult(uint* combat_res, uint len);
    void Send_AutomapsInfo(void* locs_vec, Location* loc);
    void Send_Effect(hash eff_pid, ushort hx, ushort hy, ushort radius);
    void Send_FlyEffect(
        hash eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy);
    void Send_PlaySound(uint crid_synchronize, const string& sound_name);
    void Send_MapText(ushort hx, ushort hy, uint color, const string& text, bool unsafe_text);
    void Send_MapTextMsg(ushort hx, ushort hy, uint color, ushort num_msg, uint num_str);
    void Send_MapTextMsgLex(
        ushort hx, ushort hy, uint color, ushort num_msg, uint num_str, const char* lexems, ushort lexems_len);
    void Send_ViewMap();
    void Send_SomeItem(Item* item); // Without checks
    void Send_CustomMessage(uint msg);
    void Send_CustomMessage(uint msg, uchar* data, uint data_size);

    bool IsTalking();
};

class Npc : public Critter
{
    friend class CritterManager;

public:
    Npc(uint id, ProtoCritter* proto, CritterSettings& sett);
    ~Npc();

    uint GetTalkedPlayers();
    bool IsTalkedPlayers();
    uint GetBarterPlayers();
    bool IsFreeToTalk();
};
