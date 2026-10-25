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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#include "catch_amalgamated.hpp"

#include "AngelScriptScripting.h"
#include "Application.h"
#include "Baker.h"
#include "Client.h"
#include "DataSerialization.h"
#include "ImGuiStuff.h"
#include "ResourcePack.h"
#include "Server.h"
#include "Test_BakerHelpers.h"
#include "Updater.h"

FO_BEGIN_NAMESPACE

namespace TestClientServerIntegration
{
    inline std::atomic_uint16_t IntegrationTestPort {46000};

    // The baked static item shared by the server map and the client map, addressed by both sides
    static constexpr ident_t STATIC_ITEM_ID {5003};
    static constexpr mpos STATIC_ITEM_HEX {20, 20};

#if FO_ANGELSCRIPT_SCRIPTING
    inline auto MakeServerScriptBinary(const FileSystem& metadata_resources) -> vector<uint8_t>
    {
        BakerServerEngine compiler_engine {metadata_resources};

        return BakerTests::CompileInlineScripts(&compiler_engine, "ClientServerServerScripts",
            {
                {"Scripts/ClientServerIntegrationServer.fos", R"(
#include "ClientServerIntegrationServerShared.fos"

namespace ClientServerIntegrationServer
{
    int LoginCalls = 0;

    void UnitTestEntry()
    {
        UnitTestNoop();
    }

    // Inbound remote call: the engine dispatches it as void <namespace>::<CallName>(Player player, args...)
    [[ServerRemoteCall]]
    void UnitTestLogin(Player player)
    {
        LoginCalls++;

        // The login insert refuses an empty document, so the fresh session carries at least one stored value
        player.UnitTestLoginMark = 7;

        Player loggedPlayer = Game.LoginPlayerToNewRecord(player);

        // Handing the session a controlled critter drives the client through the whole world-entry protocol:
        // load map, add critter, initial property sync
        Critter cr = Game.CreateCritter("UnitTestSharedCritter".hstr(), true);
        // Keep the ground-item probes visible after movement, including in projects with distance-based visibility hooks
        cr.LookDistance = 20;
        loggedPlayer.SwitchCritter(cr);
        SwitchedCritters++;

        // Entering a real map is what runs the load-map protocol, and each session gets its own location because
        // reusing one would need cover the login context does not carry
        hstring[] mapPids = {"UnitTestSharedMap".hstr()};
        Location loc = Game.CreateLocation("UnitTestSharedLocation".hstr(), mapPids);
        Map map = loc.GetMapByIndex(0);
        cr.TransferToMap(map, mpos(10, 10));

        // Inventory and map items each take their own send path to the owning client
        cr.AddItem("UnitTestSharedItem".hstr());
        Item groundItem = map.AddItem(mpos(10, 10), "UnitTestSharedItem".hstr());
        // A draw sub-layer of the item's own, which only its synced data can carry to the client
        groundItem.DrawOrderSubLayer = 3;

        // A second critter on the same map arrives at the client as a foreign critter, which is a different
        // send path from the controlled one, and moving it drives the position updates
        Critter npc = Game.CreateCritter("UnitTestSharedCritter".hstr(), false);
        npc.TransferToMap(map, mpos(10, 11));
        NpcCritters++;

        // Condition, action and attachment each push their own message down to the watching client
        npc.SetCondition(CritterCondition::Knockout, CritterActionAnim(1), null);
        npc.SetCondition(CritterCondition::Alive, CritterActionAnim(1), null);
        npc.Action(CritterAction::StandUp, 0, null);
        // Attach and detach both push a message; leaving the attachment live would trip a client-side
        // verification when the session tears down with attached critters still linked
        npc.AttachToCritter(cr);
        npc.DetachFromCritter();

        // Both remaining detached-item messages: the action context item and the slot-move item
        Item npcItem = npc.AddItem("UnitTestSharedItem".hstr());
        npc.Action(CritterAction::DropItem, 0, npcItem);
        npc.ChangeItemSlot(npcItem.Id, CritterItemSlot::Main);

        cr.SendItems(cr.GetItems(), true, false);
    }

    int NpcCritters = 0;

    int UnitTestGetNpcCritters()
    {
        return NpcCritters;
    }

    int WorldSteps = 0;

    // A second inbound call is how the test runs server-side world changes inside a proper sync context:
    // the handler is entered with the calling player covered, the same way the login handler is
    [[ServerRemoteCall]]
    [[Async]]
    void UnitTestWorldStep(Player player, int step)
    {
        WorldSteps++;

        Critter? controlled = player.GetControlledCritter();

        if (controlled is null) {
            return;
        }

        Critter cr = controlled;

        // The handler is entered with the calling player covered; everything else it reaches - the
        // controlled critter's map and its contents - has to be acquired explicitly
        Map? crMapCover = cr.GetMap();

        if (crMapCover !is null) {
            Game.Sync(cr, crMapCover);
        }
        else {
            Game.Sync(cr);
        }

        if (step == 0) {
            // Adding an item and pushing it out onto the map both send item messages to the owning client
            Item added = cr.AddItem("UnitTestSharedItem".hstr());

            if (crMapCover !is null) {
                Game.MoveItem(added, crMapCover, mpos(10, 10));
                // Written once the item is already drawn, so the client has to sort its sprite again
                added.DrawOrderSubLayer = 5;
            }
        }
        else if (step == 2) {
            // Moving the controlled critter sends a position update down to its own client
            cr.TransferToHex(mpos(11, 11));
            cr.TransferToHex(mpos(10, 10));
        }
        else if (step == 1) {
            // The reverse direction: the server calls into the logged-in client
            player.ClientCall.UnitTestClientPing(42);

            // Property writes on the controlled critter fan out as synchronized property messages
            cr.UnitTestClientMark = 11;
            cr.SetCondition(CritterCondition::Knockout, CritterActionAnim(1), null);
            cr.SetCondition(CritterCondition::Alive, CritterActionAnim(1), null);
        }
        else if (step == 3) {
            // A second critter appearing on the map is what makes the client build a view for someone
            // other than its own character, which is a different message family from everything above
            if (crMapCover !is null) {
                Map crMap = crMapCover;
                Critter npc = crMap.AddCritter("UnitTestSharedCritter".hstr(), mpos(12, 12), mdir(0));
                Game.Sync(npc);
                SpawnedNpcId = npc.Id;

                npc.SetDir(mdir(1));
                npc.Action(CritterAction::Refresh, 0, null);
                npc.UnitTestClientMark = 7;
            }
        }
        else if (step == 4) {
            // Property writes keep the synchronized-property path busy; moving the critter is unreachable here,
            // because transferring one the caller does not control exceeds an inbound call's cover
            if (crMapCover !is null && SpawnedNpcId.value != 0) {
                Map crMap = crMapCover;
                Critter? npcHandle = crMap.GetCritter(SpawnedNpcId);

                if (npcHandle !is null) {
                    Critter npc = npcHandle;
                    Game.Sync(npc);
                    npc.UnitTestClientMark = 8;
                }
            }
        }
        else if (step == 6) {
            // Dropping the map's one baked static item; the client watching the map must lose it too
            if (crMapCover !is null) {
                Map crMap = crMapCover;
                StaticItem[] statics = crMap.GetStaticItems();

                if (!statics.isEmpty()) {
                    crMap.RemoveStaticItem(statics[0]);
                }
            }
        }
        else if (step == 5) {
            // A property write on a critter the client only observes is a different fan-out from a write
            // on its own character, which step 1 already covers
            if (crMapCover !is null && SpawnedNpcId.value != 0) {
                Map crMap = crMapCover;
                Critter? npcHandle = crMap.GetCritter(SpawnedNpcId);

                if (npcHandle !is null) {
                    Critter npc = npcHandle;
                    Game.Sync(npc);
                    npc.UnitTestClientMark = 9;

                    for (uint8 dir = 0; dir < 6; dir++) {
                        npc.SetDir(mdir(dir));
                    }
                }
            }
        }
    }

    ident SpawnedNpcId;

    int UnitTestGetWorldSteps()
    {
        return WorldSteps;
    }


    int EveryArgCalls = 0;
    int EveryArgMismatch = 0;

    // Every wire-representable argument shape in one inbound call, so each type's marshalling branch runs
    [[ServerRemoteCall]]
    void UnitTestEveryArgToServer(Player player, int8 i8, int16 i16, int i32, int64 i64, uint8 u8, uint16 u16, uint32 u32, uint64 u64, float f32, double f64, bool flag, string text, hstring hash, ident id, timespan span, ucolor color, mpos hex, ipos offset, int[] ints, string[] texts, hstring[] hashes)
    {
        EveryArgCalls++;

        if (i8 != 1 || i16 != 2 || i32 != 3 || i64 != 4) EveryArgMismatch |= 1;
        if (u8 != 5 || u16 != 6 || u32 != 7 || u64 != 8) EveryArgMismatch |= 2;
        if (f32 != 1.5f || f64 != 2.5) EveryArgMismatch |= 4;
        if (!flag || text != "wire" || hash != "UnitTestSharedItem".hstr()) EveryArgMismatch |= 8;
        if (span != timespan(9, 3)) EveryArgMismatch |= 16;
        if (color != ucolor(1, 2, 3, 4)) EveryArgMismatch |= 32;
        if (hex != mpos(4, 5) || offset != ipos(-6, 7)) EveryArgMismatch |= 64;
        if (ints.length() != 2 || ints[1] != 20) EveryArgMismatch |= 128;
        if (texts.length() != 2 || texts[1] != "b") EveryArgMismatch |= 256;
        if (hashes.length() != 2 || hashes[1] != "UnitTestSharedCritter".hstr()) EveryArgMismatch |= 512;

        // Send the same shapes back the other way
        player.ClientCall.UnitTestEveryArgToClient(i8, i16, i32, i64, u8, u16, u32, u64, f32, f64, flag, text, hash, id, span, color, hex, offset, ints, texts, hashes);
    }

    int UnitTestGetEveryArgCalls()
    {
        return EveryArgCalls;
    }

    int UnitTestGetEveryArgMismatch()
    {
        return EveryArgMismatch;
    }


    int SwitchedCritters = 0;

    int UnitTestGetSwitchedCritters()
    {
        return SwitchedCritters;
    }

    int UnitTestGetLoginCalls()
    {
        return LoginCalls;
    }
}
)"},
                {"Scripts/ClientServerIntegrationServerShared.fos", R"(
namespace ClientServerIntegrationServer
{
    void UnitTestNoop() {}
}
)"},
            },
            [](string_view message) {
                string message_str = string(message);

                if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos || message_str.find("fatal") != string::npos || message_str.find("Fatal") != string::npos) {
                    throw ScriptSystemException(message_str);
                }
            });
    }
#endif

#if FO_ANGELSCRIPT_SCRIPTING
    inline auto MakeClientScriptBinary(const FileSystem& metadata_resources) -> vector<uint8_t>
    {
        BakerClientEngine compiler_engine {metadata_resources};

        return BakerTests::CompileInlineScripts(&compiler_engine, "ClientServerClientScripts",
            {
                {"Scripts/ClientServerIntegrationClient.fos", R"(
#include "ClientServerIntegrationClientShared.fos"

namespace ClientServerIntegrationClient
{
    int ConnectingCalls = 0;
    int ConnectedCalls = 0;
    int LoginSuccessCalls = 0;
    int DisconnectedCalls = 0;
    int ReceivedItemCount = 0;
    int ReceivedMapOwnedItemCount = 0;
    int ActionContextItemCount = 0;
    int ActionMapOwnedContextItemCount = 0;

    [[ModuleInit]]
    void InitClientServerIntegrationClient()
    {
        Game.OnConnecting.Subscribe(OnConnecting);
        Game.OnConnected.Subscribe(OnConnected);
        Game.OnLoginSuccess.Subscribe(OnLoginSuccess);
        Game.OnDisconnected.Subscribe(OnDisconnected);
        Game.OnReceiveItems.Subscribe(OnReceiveItems);
        Game.OnCritterAction.Subscribe(OnCritterAction);
    }

    // Items sent through Critter.SendItems arrive as detached views: ownership is not a synchronized
    // property, so without an explicit mode they would keep the zero default and read as map items
    [[Event]]
    void OnReceiveItems(Item[] items, any contextParam)
    {
        ReceivedItemCount += items.length();

        for (int i = 0; i < int(items.length()); i++) {
            if (items[i].Ownership == ItemOwnership::MapHex) {
                ReceivedMapOwnedItemCount++;
            }
        }
    }

    // A foreign critter's action carries its context item the same detached way, so the same
    // ownership stamp has to reach it: a weapon in another critter's hands is not a map item
    [[Event]]
    void OnCritterAction(bool localCall, Critter cr, CritterAction action, int actionData, AbstractItem? contextItem)
    {
        if (contextItem is null) {
            return;
        }

        Item? contextItemInstance = cast<Item>(contextItem);

        if (contextItemInstance is null) {
            return;
        }

        ActionContextItemCount++;

        if (contextItemInstance.Ownership == ItemOwnership::MapHex) {
            ActionMapOwnedContextItemCount++;
        }
    }

    [[Event]]
    void OnConnecting()
    {
        ConnectingCalls++;
    }

    [[Event]]
    void OnConnected()
    {
        ConnectedCalls++;
    }

    [[Event]]
    void OnLoginSuccess()
    {
        LoginSuccessCalls++;
    }

    [[Event]]
    void OnDisconnected()
    {
        DisconnectedCalls++;
    }

    void UnitTestEntry()
    {
        UnitTestNoop();
    }

    int UnitTestGetConnectingCalls()
    {
        return ConnectingCalls;
    }

    int UnitTestGetConnectedCalls()
    {
        return ConnectedCalls;
    }

    int UnitTestGetLoginSuccessCalls()
    {
        return LoginSuccessCalls;
    }

    int UnitTestGetDisconnectedCalls()
    {
        return DisconnectedCalls;
    }

    int UnitTestGetReceivedItemCount()
    {
        return ReceivedItemCount;
    }

    int UnitTestGetReceivedMapOwnedItemCount()
    {
        return ReceivedMapOwnedItemCount;
    }

    int UnitTestGetActionContextItemCount()
    {
        return ActionContextItemCount;
    }

    int UnitTestGetActionMapOwnedContextItemCount()
    {
        return ActionMapOwnedContextItemCount;
    }

    string UnitTestReadCritterModelName(Critter cr)
    {
        return cr.ModelName.str;
    }

    void UnitTestSendLogin()
    {
        CurPlayer.ServerCall.UnitTestLogin();
    }

    void UnitTestSendWorldStep(int step)
    {
        CurPlayer.ServerCall.UnitTestWorldStep(step);
    }

    int EveryArgEchoes = 0;
    int EveryArgEchoMismatch = 0;

    int UnitTestSendEveryArg()
    {
        int[] ints = {10, 20};
        string[] texts = {"a", "b"};
        hstring[] hashes = {"UnitTestSharedItem".hstr(), "UnitTestSharedCritter".hstr()};

        CurPlayer.ServerCall.UnitTestEveryArgToServer(1, 2, 3, 4, 5, 6, 7, 8, 1.5f, 2.5, true, "wire", "UnitTestSharedItem".hstr(), CurPlayer.Id, timespan(9, 3), ucolor(1, 2, 3, 4), mpos(4, 5), ipos(-6, 7), ints, texts, hashes);
        return 0;
    }

    [[ClientRemoteCall]]
    void UnitTestEveryArgToClient(int8 i8, int16 i16, int i32, int64 i64, uint8 u8, uint16 u16, uint32 u32, uint64 u64, float f32, double f64, bool flag, string text, hstring hash, ident id, timespan span, ucolor color, mpos hex, ipos offset, int[] ints, string[] texts, hstring[] hashes)
    {
        EveryArgEchoes++;

        if (i8 != 1 || i16 != 2 || i32 != 3 || i64 != 4) EveryArgEchoMismatch |= 1;
        if (u8 != 5 || u16 != 6 || u32 != 7 || u64 != 8) EveryArgEchoMismatch |= 2;
        if (f32 != 1.5f || f64 != 2.5) EveryArgEchoMismatch |= 4;
        if (!flag || text != "wire" || hash != "UnitTestSharedItem".hstr()) EveryArgEchoMismatch |= 8;
        if (span != timespan(9, 3)) EveryArgEchoMismatch |= 16;
        if (color != ucolor(1, 2, 3, 4)) EveryArgEchoMismatch |= 32;
        if (hex != mpos(4, 5) || offset != ipos(-6, 7)) EveryArgEchoMismatch |= 64;
        if (ints.length() != 2 || ints[1] != 20) EveryArgEchoMismatch |= 128;
        if (texts.length() != 2 || texts[1] != "b") EveryArgEchoMismatch |= 256;
        if (hashes.length() != 2 || hashes[1] != "UnitTestSharedCritter".hstr()) EveryArgEchoMismatch |= 512;
    }

    int UnitTestGetEveryArgEchoes()
    {
        return EveryArgEchoes;
    }

    int UnitTestGetEveryArgEchoMismatch()
    {
        return EveryArgEchoMismatch;
    }

    int ClientPings = 0;

    // Inbound on the client side: the engine dispatches it as void <namespace>::<CallName>(args...) with no
    // player argument, which is the only shape difference from the server-side handler
    [[ClientRemoteCall]]
    void UnitTestClientPing(int value)
    {
        ClientPings += value;
    }

    int UnitTestGetClientPings()
    {
        return ClientPings;
    }

    int UnitTestDriveChosen()
    {
        if (!HasChosen) return -1;
        if (!HasCurMap) return -2;

        // Both of these travel to the server as player commands and come back as authoritative state
        Chosen.MoveToHex(mpos(12, 12), ipos(0, 0), 10);
        Chosen.ChangeDir(mdir(2));

        return 0;
    }

    int UnitTestWriteChosenProperty()
    {
        if (!HasChosen) return -1;

        // A ModifiableByClient property is the one thing a client may write straight into the world state,
        // and each entity kind takes its own send-value path on the way out
        Chosen.UnitTestClientMark = 5;

        Item[] ownItems = Chosen.GetItems();

        if (!ownItems.isEmpty()) {
            ownItems[0].UnitTestItemMark = 6;
        }

        if (HasCurPlayer) {
            CurPlayer.UnitTestPlayerMark = 7;
        }

        // Map and location writes are not driven here: the server applies an inbound property write on a
        // worker that covers the player and its critter, not the map or location behind them
        return 0;
    }

    // Driving the chosen critter from the client is what makes it send movement messages, which is the
    // only way the server-side move handlers are entered
    int UnitTestDriveChosenMovement()
    {
        if (!HasChosen) return -1;
        if (!HasCurMap) return -2;

        Critter chosen = Chosen;
        mpos start = chosen.Hex;

        // A path request, a direction step and a stop each send their own message
        chosen.MoveToHex(mpos(start.x + 2, start.y + 1), ipos(0, 0), 1000);
        chosen.StopMove();

        for (uint8 dir = 0; dir < 6; dir++) {
            chosen.MoveToDir(mdir(dir), 1000);
        }

        chosen.StopMove();

        // A cut path and a zero-speed request are the remaining argument shapes
        chosen.MoveToHex(mpos(start.x + 1, start.y), 1, ipos(4, 4), 500);
        chosen.StopMove();

        // Writing a client-modifiable property is what drives the server property handler
        chosen.UnitTestClientMark = 21;

        // A client-local item lives only on this side and carries no server id; it is created here rather
        // than in the polled inspection so the map does not grow one per poll
        CurMap.CreateLocalItem("UnitTestSharedItem".hstr(), mpos(start.x + 2, start.y));

        // Each entity kind has its own client-writable property, and each one takes its own arm of the
        // send-value handler on the client and of the property handler on the server
        CurPlayer.UnitTestPlayerMark = 31;

        Item[] ownItems = chosen.GetItems();

        if (!ownItems.isEmpty()) {
            ownItems[0].UnitTestItemMark = 41;
        }

        // The minimap builds its own hex walk, which nothing else in the suite reaches
        Game.DrawMiniMap(1, 0, 0, 64, 64);

        return 0;
    }

    int UnitTestInspectChosenWorld()
    {
        if (!HasChosen) return -1;
        if (!HasCurMap) return -2;

        // The inventory and the map contents arrived over the wire, so the client can enumerate both
        if (Chosen.GetItems().isEmpty()) return -3;

        // Map contents are not asserted on: the world steps move items between the inventory and the map,
        // so the count here depends on which step last landed
        Item[] landedItems = CurMap.GetItems();

        // The login's ground item carries a draw sub-layer of its own, and the inspection waits until the client
        // holds that value as well
        bool layeredItemArrived = false;

        for (uint i = 0; i < landedItems.length(); i++) {
            if (landedItems[i].Id.value != 0 && landedItems[i].DrawOrderSubLayer == 3) {
                layeredItemArrived = true;
            }
        }

        if (!layeredItemArrived) return -20;

        if (CurMap.GetCritters(CritterFindType::Any).isEmpty()) return -5;
        if (CurMap.GetCritter(Chosen.Id) is null) return -6;

        CurMap.GetHexScreenPos(Chosen.Hex);
        CurMap.IsHexVisible(Chosen.Hex);
        CurMap.GetVisibleHexes();
        CurMap.RebuildFog();
        CurMap.RedrawMap();

        // Fog shapes build only outside mapper mode, so a real session is the only place they are reachable; the
        // three layers cover the follow, pinned and traced shape inputs
        FogLayer following = CurMap.AddFog(Chosen, DrawOrderType::Last);
        following.Radius = 5;
        following.Distance = 7;
        following.ExtraLength = 2;
        following.Traced = false;

        FogLayer pinned = CurMap.AddFog(mpos(Chosen.Hex.x + 2, Chosen.Hex.y + 2), DrawOrderType::Last);
        pinned.Radius = 3;
        pinned.OvalRoundness = 0.5f;
        pinned.EdgeNoise = 0.0f;

        FogLayer traced = CurMap.AddFog(Chosen, DrawOrderType::Last);
        traced.Traced = true;
        traced.Radius = 4;
        traced.Distance = 6;
        traced.CheckShootBlocks = false;
        traced.OverlayColor = ucolor(200, 40, 40, 128);
        traced.CenterColor = ucolor(255, 200, 200, 200);

        // The shapes morph over time, so several frames are drawn before the layers are taken down
        for (int i = 0; i < 4; i++) {
            CurMap.RebuildFog();
            CurMap.RedrawMap();
        }

        pinned.Enabled = false;
        CurMap.RebuildFog();

        following.Dispose();
        pinned.Dispose();
        traced.Dispose();
        CurMap.RebuildFog();

        // The map query surface only answers against a real session world, so it is walked here rather
        // than from the mapper, which has no chosen critter and no critter list to search
        mpos here = Chosen.Hex;
        mpos there = mpos(here.x + 3, here.y + 2);

        CurMap.GetPath(here, there, 0);
        CurMap.GetPath(Chosen, there, 1);
        if (CurMap.GetPathLength(here, there, 0) < 0) return -7;
        if (CurMap.GetPathLength(Chosen, there, 1) < 0) return -8;

        CurMap.GetCritterOnHex(here, CritterFindType::Any);
        CurMap.GetCritterInRadius(here, 5, CritterFindType::Any);
        CurMap.GetCrittersOnHex(here, CritterFindType::Any);
        CurMap.GetCrittersInRadius(here, 5, CritterFindType::NonDead);
        CurMap.GetCrittersInPath(here, there, 0.0f, 6, CritterFindType::Any);

        mpos preBlockHex;
        mpos blockHex;
        CurMap.GetCrittersWithBlockInPath(here, there, 0.0f, 6, CritterFindType::Any, preBlockHex, blockHex);

        mpos walked = here;
        CurMap.MoveHexByDir(walked, mdir(0));
        CurMap.MoveHexByDir(walked, mdir(3), 2);

        CurMap.MoveScreenToHex(there, ipos16(0, 0), 20, true);
        CurMap.SetTransparentEgg(TransparentEggSlot::Primary, Chosen);
        CurMap.SetTransparentEgg(TransparentEggSlot::Secondary, here, ipos(0, 0), isize(8, 8), TransparentEggTarget::Structure);

        // The global lookups resolve against the current map, so they only answer in a session
        if (Game.GetCritter(Chosen.Id) is null) return -9;
        // A zero id is rejected rather than answered - the two lookups disagree on which, so both are probed
        int zeroIdRejections = 0;
        try { if (Game.GetCritter(ident()) !is null) return -10; } catch { zeroIdRejections++; }
        if (Game.GetCritters("UnitTestSharedCritter".hstr(), CritterFindType::Any).isEmpty()) return -11;
        if (!Game.GetCritters("UnitTestSharedItem".hstr(), CritterFindType::Any).isEmpty()) return -12;

        Game.GetCritters(Game.GetProtoCritter("UnitTestSharedCritter".hstr()), CritterFindType::Any);
        Game.GetCritters(CritterFindType::NonDead);
        Game.SortCrittersByDeep(Game.GetCritters(CritterFindType::Any));

        // Distance is defined only for placed entities, so map items are used, and the lookup targets a
        // server-owned item because a client-local one carries no id
        Item[] mapItems = CurMap.GetItems();

        for (uint i = 0; i < mapItems.length(); i++) {
            Item mapItem = mapItems[i];

            if (mapItem.Id.value == 0) {
                continue;
            }

            if (Game.GetItem(mapItem.Id) is null) return -13;
            if (Game.GetDistance(Chosen, mapItem) < 0) return -14;
            if (Game.GetDistance(mapItem, Chosen) < 0) return -15;
            if (Game.GetDistance(mapItem, mapItem) != 0) return -16;
            Game.GetDistance(mapItem, Chosen.Hex);
            Game.GetDistance(Chosen.Hex, mapItem);
            break;
        }

        try { if (Game.GetItem(ident()) !is null) return -17; } catch { zeroIdRejections++; }
        if (zeroIdRejections == 0) return -19;
        if (Game.GetDistance(Chosen, Chosen) != 0) return -18;
        Game.GetDistance(Chosen, Chosen.Hex);
        Game.GetDistance(Chosen.Hex, Chosen);

        // The inventory side of the same surface
        Chosen.GetItem("UnitTestSharedItem".hstr());
        Chosen.GetItems();
        Chosen.GetBodyAngle();

        ipos bonePos;
        Chosen.GetBonePos("Root".hstr(), bonePos);

        return 0;
    }
}
)"},
                {"Scripts/ClientServerIntegrationClientShared.fos", R"(
namespace ClientServerIntegrationClient
{
    void UnitTestNoop() {}
}
)"},
            },
            [](string_view message) {
                string message_str = string(message);

                if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos || message_str.find("fatal") != string::npos || message_str.find("Fatal") != string::npos) {
                    throw ScriptSystemException(message_str);
                }
            });
    }
#endif

    inline auto MakeServerTestSettings(uint16_t port) -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedServerSettings(settings);
        BakerTests::OverrideSetting(settings.Network.ServerPort, port);

        return settings;
    }

    inline auto MakeClientTestSettings(uint16_t port) -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedClientSettings(settings);
        BakerTests::OverrideSetting(settings.Network.ServerPort, port);

        return settings;
    }

    inline auto MakeTempClientUpdaterBakeDir(string_view name) -> string
    {
        std::chrono::steady_clock::rep suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        string dir_name = strex("lf_client_updater_{}_{}", name, suffix).str();
        std::filesystem::path base = std::filesystem::temp_directory_path() / std::filesystem::path {fs::make_path(dir_name)};
        return fs::path_to_string(base);
    }

    inline auto PrepareClientUpdaterBakeOutput() -> string
    {
        string bake_dir = MakeTempClientUpdaterBakeDir("resources");
        string fonts_dir = strex(bake_dir).combine_path("Embedded/Fonts").str();

        REQUIRE(fs::create_directories(fonts_dir));

        constexpr string_view default_font = R"(Version 2
Image Default.png
YAdvance 1

Letter ' '
  PositionX 0
  PositionY 0
  Width 1
  Height 1
  XAdvance 1

End
)";

        REQUIRE(fs::write_file(strex(fonts_dir).combine_path("Default.fofnt").str(), default_font));

        vector<uint8_t> default_font_sprite = BakerTests::MakeMinimalBakedSprite();
        REQUIRE(fs::write_file(strex(fonts_dir).combine_path("Default.png").str(), default_font_sprite));

        string effects_dir = strex(bake_dir).combine_path("Embedded/Effects").str();
        REQUIRE(fs::create_directories(effects_dir));

        for (string_view name : {"ImGui_Default", "2D_Default", "Flush_RenderTarget"}) {
            REQUIRE(fs::write_file(strex(effects_dir).combine_path(strex("{}.fofx", name).str()).str(), "[Effect]\nPasses = 1\n"));
            REQUIRE(fs::write_file(strex(effects_dir).combine_path(strex("{}.fofx-1-info", name).str()).str(), "[EffectInfo]\n"));
        }

        return bake_dir;
    }

    // Properties of the one baked static item both map blobs carry, so a server-side removal of it is
    // observable in the client's map view
    template<typename TEngine>
    inline auto MakeStaticItemPropsBlob(TEngine& engine) -> vector<uint8_t>
    {
        auto registrar = engine.GetPropertyRegistrar(engine.Hashes.to_hashed_string("Item"));
        REQUIRE(static_cast<bool>(registrar));

        auto static_prop = registrar->FindProperty("Static");
        auto ownership_prop = registrar->FindProperty("Ownership");
        auto hex_prop = registrar->FindProperty("Hex");
        REQUIRE(static_cast<bool>(static_prop));
        REQUIRE(static_cast<bool>(ownership_prop));
        REQUIRE(static_cast<bool>(hex_prop));

        Properties props {registrar};
        props.SetValue<bool>(static_prop, true);
        props.SetValue<ItemOwnership>(ownership_prop, ItemOwnership::MapHex);
        props.SetValue<mpos>(hex_prop, STATIC_ITEM_HEX);

        vector<uint8_t> props_data;
        set<hstring> str_hashes;
        props.StoreAllData(props_data, str_hashes);
        return props_data;
    }

    // Authored content, so map creation runs the content generator instead of skipping it. Layout is the hash
    // table, then critter records, then item records; each record is an id, a proto hash and a props blob
    template<typename TEngine>
    inline auto MakeStaticServerMapBlob(TEngine& engine) -> vector<uint8_t>
    {
        hstring critter_pid = engine.Hashes.to_hashed_string("UnitTestSharedCritter");
        hstring item_pid = engine.Hashes.to_hashed_string("UnitTestSharedItem");

        // A default-constructed property set still serializes to a non-empty blob, so it is produced here
        // rather than writing a zero size the reader cannot restore from
        auto make_default_props_blob = [&engine](string_view type_name) {
            auto registrar = engine.GetPropertyRegistrar(engine.Hashes.to_hashed_string(type_name));
            REQUIRE(static_cast<bool>(registrar));

            Properties props {registrar};
            vector<uint8_t> props_data;
            set<hstring> str_hashes;
            props.StoreAllData(props_data, str_hashes);
            return props_data;
        };

        vector<uint8_t> critter_props = make_default_props_blob("Critter");
        vector<uint8_t> item_props = make_default_props_blob("Item");

        vector<uint8_t> map_data;
        auto writer = data_writer(map_data);

        writer.write<uint32_t>(BAKED_MAP_FILE_MAGIC);
        writer.write<uint32_t>(BAKED_MAP_FILE_VERSION);

        vector<string> hashed_strings {string {critter_pid.as_str()}, string {item_pid.as_str()}};
        writer.write<uint32_t>(numeric_cast<uint32_t>(hashed_strings.size()));

        for (const string& hashed_string : hashed_strings) {
            writer.write<uint32_t>(numeric_cast<uint32_t>(hashed_string.length()));
            writer.write_string_bytes(hashed_string);
        }

        writer.write<uint32_t>(uint32_t {1});
        writer.write<ident_t::underlying_type>(ident_t::underlying_type {5001});
        writer.write<hstring::hash_t>(critter_pid.as_hash());
        writer.write<uint32_t>(numeric_cast<uint32_t>(critter_props.size()));

        if (!critter_props.empty()) {
            writer.write_bytes({critter_props.data(), critter_props.size()});
        }

        vector<uint8_t> static_item_props = MakeStaticItemPropsBlob(engine);

        writer.write<uint32_t>(uint32_t {2});
        writer.write<ident_t::underlying_type>(ident_t::underlying_type {5002});
        writer.write<hstring::hash_t>(item_pid.as_hash());
        writer.write<uint32_t>(numeric_cast<uint32_t>(item_props.size()));

        if (!item_props.empty()) {
            writer.write_bytes({item_props.data(), item_props.size()});
        }

        writer.write<ident_t::underlying_type>(STATIC_ITEM_ID.underlying_value());
        writer.write<hstring::hash_t>(item_pid.as_hash());
        writer.write<uint32_t>(numeric_cast<uint32_t>(static_item_props.size()));
        writer.write_bytes({static_item_props.data(), static_item_props.size()});

        return map_data;
    }

    template<typename TEngine>
    inline auto MakeStaticClientMapBlob(TEngine& engine) -> vector<uint8_t>
    {
        hstring item_pid = engine.Hashes.to_hashed_string("UnitTestSharedItem");
        vector<uint8_t> static_item_props = MakeStaticItemPropsBlob(engine);

        vector<uint8_t> map_data;
        auto writer = data_writer(map_data);
        writer.write<uint32_t>(BAKED_MAP_FILE_MAGIC);
        writer.write<uint32_t>(BAKED_MAP_FILE_VERSION);

        writer.write<uint32_t>(uint32_t {1});
        writer.write<uint32_t>(numeric_cast<uint32_t>(item_pid.as_str().length()));
        writer.write_string_bytes(item_pid.as_str());

        writer.write<uint32_t>(uint32_t {1});
        writer.write<ident_t::underlying_type>(STATIC_ITEM_ID.underlying_value());
        writer.write<hstring::hash_t>(item_pid.as_hash());
        writer.write<uint32_t>(numeric_cast<uint32_t>(static_item_props.size()));
        writer.write_bytes({static_item_props.data(), static_item_props.size()});

        return map_data;
    }

    template<typename TEngine>
    inline auto MakeMapProtoBlob(TEngine& proto_engine, hstring type_name, string_view proto_name, msize map_size) -> vector<uint8_t>
    {
        vector<uint8_t> props_data;
        set<hstring> str_hashes;

        auto registrar = proto_engine.GetPropertyRegistrar(type_name);
        REQUIRE(static_cast<bool>(registrar));

        ProtoMap proto {proto_engine.Hashes.to_hashed_string(proto_name), registrar};
        proto.SetSize(map_size);
        proto.GetProperties()->StoreAllData(props_data, str_hashes);

        vector<uint8_t> protos_data;
        auto writer = data_writer(protos_data);

        writer.write<uint32_t>(uint32_t {0});
        ignore_unused(str_hashes);
        writer.write<uint32_t>(uint32_t {1});
        writer.write<uint32_t>(uint32_t {1});
        writer.write<uint16_t>(numeric_cast<uint16_t>(type_name.as_str().length()));
        writer.write_string_bytes(type_name.as_str());
        writer.write<uint16_t>(numeric_cast<uint16_t>(proto_name.length()));
        writer.write_string_bytes(proto_name);
        writer.write<uint32_t>(numeric_cast<uint32_t>(props_data.size()));

        if (!props_data.empty()) {
            writer.write_bytes({props_data.data(), props_data.size()});
        }

        return protos_data;
    }

    inline auto MakeServerTestResources() -> FileSystem
    {
        // The login handshake is a remote call, so both sides declare it: inbound on the server, outbound on
        // the client. The subsystem hint is the owning script file, whose stem becomes the handler namespace
        auto metadata_blob = BakerTests::MakeMetadataBlob({
            {"Property",
                {
                    {"Player", "Server", "int32", "UnitTestLoginMark", "Mutable", "Persistent"},
                    {"Critter", "Common", "int32", "UnitTestClientMark", "Mutable", "PublicSync", "ModifiableByClient"},
                    // One client-writable property per entity kind, so each send-value handler has a path
                    {"Item", "Common", "int32", "UnitTestItemMark", "Mutable", "PublicSync", "ModifiableByClient"},
                    {"Player", "Common", "int32", "UnitTestPlayerMark", "Mutable", "PublicSync", "ModifiableByClient"},
                    {"Map", "Common", "int32", "UnitTestMapMark", "Mutable", "PublicSync", "ModifiableByAnyClient"},
                    {"Location", "Common", "int32", "UnitTestLocationMark", "Mutable", "PublicSync", "ModifiableByAnyClient"},
                }},
            {"RemoteCall",
                {
                    {"UnitTestLogin", "ClientServerIntegrationServer.fos", "In", "Limits", "0", "0"},
                    {"UnitTestWorldStep", "ClientServerIntegrationServer.fos", "In", "int32", "", "step", "Limits", "0", "0"},
                    {"UnitTestClientPing", "ClientServerIntegrationServer.fos", "Out", "int32", "", "value", "Limits", "0", "0"},
                    // One call per direction carrying every wire-representable argument shape, so the
                    // marshalling walks each type's own branch on both sides
                    {"UnitTestEveryArgToServer", "ClientServerIntegrationServer.fos", "In", "int8", "", "i8", "int16", "", "i16", "int32", "", "i32", "int64", "", "i64", "uint8", "", "u8", "uint16", "", "u16", "uint32", "", "u32", "uint64", "", "u64", "float32", "", "f32", "float64", "", "f64", "bool", "", "flag", "string", "", "text", "hstring", "", "hash", "ident", "", "id", "timespan", "", "span", "ucolor", "", "color", "mpos", "", "hex", "ipos", "", "offset", "int32 [ ]", "", "ints", "string [ ]", "", "texts", "hstring [ ]", "", "hashes", "Limits", "0", "0"},
                    {"UnitTestEveryArgToClient", "ClientServerIntegrationServer.fos", "Out", "int8", "", "i8", "int16", "", "i16", "int32", "", "i32", "int64", "", "i64", "uint8", "", "u8", "uint16", "", "u16", "uint32", "", "u32", "uint64", "", "u64", "float32", "", "f32", "float64", "", "f64", "bool", "", "flag", "string", "", "text", "hstring", "", "hash", "ident", "", "id", "timespan", "", "span", "ucolor", "", "color", "mpos", "", "hex", "ipos", "", "offset", "int32 [ ]", "", "ints", "string [ ]", "", "texts", "hstring [ ]", "", "hashes", "Limits", "0", "0"},
                }},
        });

        auto compiler_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("ClientServerServerCompilerResources");
        compiler_source->AddFile("Metadata.fometa-server", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_source));

        BakerServerEngine proto_engine {compiler_resources};
        hstring critter_type = proto_engine.Hashes.to_hashed_string("Critter");
        auto proto_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "UnitTestSharedCritter");

        // A location plus its map lets the logged-in critter enter the world, which is what drives the
        // client through the load-map / add-critter / property-sync protocol
        hstring location_type = proto_engine.Hashes.to_hashed_string("Location");
        hstring map_type = proto_engine.Hashes.to_hashed_string("Map");
        hstring item_type = proto_engine.Hashes.to_hashed_string("Item");
        auto location_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoLocation>(proto_engine, location_type, "UnitTestSharedLocation");
        auto item_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(proto_engine, item_type, "UnitTestSharedItem");
        auto map_blob = MakeMapProtoBlob(proto_engine, map_type, "UnitTestSharedMap", msize {50, 50});
        auto fomap_blob = MakeStaticServerMapBlob(proto_engine);

#if FO_ANGELSCRIPT_SCRIPTING
        auto script_blob = MakeServerScriptBinary(compiler_resources);
#endif

        auto runtime_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("ClientServerServerRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
        runtime_source->AddFile("ClientServerIntegration.fopro-bin-server", proto_blob);
        runtime_source->AddFile("ClientServerIntegrationLocation.fopro-bin-server", location_blob);
        runtime_source->AddFile("ClientServerIntegrationItem.fopro-bin-server", item_blob);
        runtime_source->AddFile("UnitTestSharedMap.fopro-bin-server", map_blob);
        runtime_source->AddFile("UnitTestSharedMap.fomap-bin-server", fomap_blob);
#if FO_ANGELSCRIPT_SCRIPTING
        runtime_source->AddFile("ClientServerIntegration.fos-bin-server", script_blob);
#endif

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));
        return resources;
    }

    inline auto MakeClientTestResources() -> FileSystem
    {
        auto metadata_blob = BakerTests::MakeMetadataBlob({
            {"Property",
                {
                    {"Player", "Server", "int32", "UnitTestLoginMark", "Mutable", "Persistent"},
                    {"Critter", "Common", "int32", "UnitTestClientMark", "Mutable", "PublicSync", "ModifiableByClient"},
                    // One client-writable property per entity kind, so each send-value handler has a path
                    {"Item", "Common", "int32", "UnitTestItemMark", "Mutable", "PublicSync", "ModifiableByClient"},
                    {"Player", "Common", "int32", "UnitTestPlayerMark", "Mutable", "PublicSync", "ModifiableByClient"},
                    {"Map", "Common", "int32", "UnitTestMapMark", "Mutable", "PublicSync", "ModifiableByAnyClient"},
                    {"Location", "Common", "int32", "UnitTestLocationMark", "Mutable", "PublicSync", "ModifiableByAnyClient"},
                }},
            {"RemoteCall",
                {
                    {"UnitTestLogin", "ClientServerIntegrationClient.fos", "Out", "Limits", "0", "0"},
                    {"UnitTestWorldStep", "ClientServerIntegrationClient.fos", "Out", "int32", "", "step", "Limits", "0", "0"},
                    {"UnitTestClientPing", "ClientServerIntegrationClient.fos", "In", "int32", "", "value", "Limits", "0", "0"},
                    {"UnitTestEveryArgToServer", "ClientServerIntegrationClient.fos", "Out", "int8", "", "i8", "int16", "", "i16", "int32", "", "i32", "int64", "", "i64", "uint8", "", "u8", "uint16", "", "u16", "uint32", "", "u32", "uint64", "", "u64", "float32", "", "f32", "float64", "", "f64", "bool", "", "flag", "string", "", "text", "hstring", "", "hash", "ident", "", "id", "timespan", "", "span", "ucolor", "", "color", "mpos", "", "hex", "ipos", "", "offset", "int32 [ ]", "", "ints", "string [ ]", "", "texts", "hstring [ ]", "", "hashes", "Limits", "0", "0"},
                    {"UnitTestEveryArgToClient", "ClientServerIntegrationClient.fos", "In", "int8", "", "i8", "int16", "", "i16", "int32", "", "i32", "int64", "", "i64", "uint8", "", "u8", "uint16", "", "u16", "uint32", "", "u32", "uint64", "", "u64", "float32", "", "f32", "float64", "", "f64", "bool", "", "flag", "string", "", "text", "hstring", "", "hash", "ident", "", "id", "timespan", "", "span", "ucolor", "", "color", "mpos", "", "hex", "ipos", "", "offset", "int32 [ ]", "", "ints", "string [ ]", "", "texts", "hstring [ ]", "", "hashes", "Limits", "0", "0"},
                }},
        });

        auto compiler_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("ClientServerClientCompilerResources");
        compiler_source->AddFile("Metadata.fometa-client", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_source));

        BakerClientEngine proto_engine {compiler_resources};
        hstring critter_type = proto_engine.Hashes.to_hashed_string("Critter");
        auto proto_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "UnitTestSharedCritter");

        hstring location_type = proto_engine.Hashes.to_hashed_string("Location");
        hstring map_type = proto_engine.Hashes.to_hashed_string("Map");
        hstring item_type = proto_engine.Hashes.to_hashed_string("Item");
        auto location_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoLocation>(proto_engine, location_type, "UnitTestSharedLocation");
        auto item_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(proto_engine, item_type, "UnitTestSharedItem");
        auto map_blob = MakeMapProtoBlob(proto_engine, map_type, "UnitTestSharedMap", msize {50, 50});
        auto fomap_blob = MakeStaticClientMapBlob(proto_engine);

#if FO_ANGELSCRIPT_SCRIPTING
        auto script_blob = MakeClientScriptBinary(compiler_resources);
#endif

        auto runtime_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("ClientServerClientRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-client", metadata_blob);
        runtime_source->AddFile("ClientServerIntegration.fopro-bin-client", proto_blob);
        runtime_source->AddFile("ClientServerIntegrationLocation.fopro-bin-client", location_blob);
        runtime_source->AddFile("ClientServerIntegrationItem.fopro-bin-client", item_blob);
        runtime_source->AddFile("UnitTestSharedMap.fopro-bin-client", map_blob);
        runtime_source->AddFile("UnitTestSharedMap.fomap-bin-client", fomap_blob);
#if FO_ANGELSCRIPT_SCRIPTING
        runtime_source->AddFile("ClientServerIntegration.fos-bin-client", script_blob);
#endif

        for (string_view name : {"ImGui_Default", "2D_Default", "2D_NoDepth", "Primitive_Default", "Primitive_Light", "Primitive_Fog", "Flush_RenderTarget", "Flush_Primitive", "Flush_Map", "Flush_Light", "Flush_Fog", "3D_Skinned", "Particles_ColorMulAtlas", "Particles_ColorAddAtlas", "Particles_ColorSubAtlas", "Particles_DistortionAtlas", "Particles_DistortionAddAtlas"}) {
            runtime_source->AddFile(strex("Effects/{}.fofx", name).str(), "[Effect]\nPasses = 1\n");
            runtime_source->AddFile(strex("Effects/{}.fofx-1-info", name).str(), "[EffectInfo]\n");
        }

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));
        return resources;
    }

    inline auto MakeServerEngine(GlobalSettings& settings) -> refcount_ptr<ServerEngine>
    {
        return safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerTestResources());
    }

    inline auto MakeClientEngine(GlobalSettings& settings) -> refcount_ptr<ClientEngine>
    {
        return safe_alloc::make_refcounted<ClientEngine>(&settings, MakeClientTestResources(), &GetApp()->MainWindow);
    }

    inline auto WaitForServerStart(ptr<ServerEngine> server) -> string
    {
        for (int32_t i = 0; i < 6000; i++) {
            if (server->IsStarted()) {
                return {};
            }
            if (server->IsStartingError()) {
                return "ServerEngine startup failed";
            }

            std::this_thread::sleep_for(std::chrono::milliseconds {10});
        }

        return "ServerEngine startup timed out";
    }

    inline auto GetServerConnectionCount(ptr<ServerEngine> server) -> size_t
    {
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        auto unlock = scope_exit([server]() noexcept { safe_call([server] { server.get_no_const()->Unlock(); }); });

        string health_info = server->GetHealthInfo();
        constexpr string_view prefix {"Connections: "};
        auto pos = health_info.find(prefix);
        REQUIRE(pos != string::npos);

        auto begin = pos + prefix.length();
        auto end = health_info.find('\n', begin);
        auto value_sv = string_view {health_info}.substr(begin, end == string::npos ? string::npos : end - begin);

        size_t value = 0;
        const auto [ptr, ec] = std::from_chars(value_sv.data(), value_sv.data() + value_sv.size(), value);
        REQUIRE(ec == std::errc {});
        REQUIRE(ptr == value_sv.data() + value_sv.size());

        return value;
    }

    inline auto WaitForServerConnectionCount(ptr<ServerEngine> server, size_t expected_connections) -> bool
    {
        for (int32_t i = 0; i < 2000; i++) {
            if (GetServerConnectionCount(server) == expected_connections) {
                return true;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }

        return false;
    }

    // Talks to the server's interthread listener by hand, so a test can write raw messages; the secure channel runs
    // underneath as on every connection. The listener callback shares the state, so it may outlive this object
    class RawChannelClient final
    {
    public:
        explicit RawChannelClient(uint16_t port) :
            _state {safe_alloc::make_shared<State>()}
        {
            vector<crypto::key_bytes> server_keys {ParseSecureChannelKey(BakerTests::TEST_CHANNEL_PUBLIC_KEY, "Test")};
            _channel.emplace(server_keys);

            _send = FindInterthreadListener(port).value()([state = _state](const_span<uint8_t> data) mutable {
                scoped_lock locker {state->Locker};

                if (data.empty()) {
                    state->Disconnected = true;
                }
                else {
                    state->Received.insert(state->Received.end(), data.begin(), data.end());
                }
            });

            vector<uint8_t> offer;
            _channel->TakeHandshakeOutput(offer);
            _send(offer);
        }

        [[nodiscard]] auto IsDisconnected() -> bool
        {
            scoped_lock locker {_state->Locker};

            return _state->Disconnected;
        }

        // Everything the server has sent so far, decrypted
        [[nodiscard]] auto GetPlaintext() -> vector<uint8_t>
        {
            vector<uint8_t> received;

            {
                scoped_lock locker {_state->Locker};

                received = std::move(_state->Received);
                _state->Received.clear();
            }

            _channel->Receive(received, _plaintext);
            return _plaintext;
        }

        void Send(const_span<uint8_t> plaintext)
        {
            (void)GetPlaintext();
            REQUIRE(_channel->IsEstablished());

            vector<uint8_t> sealed;
            _channel->Seal(plaintext, sealed);
            _send(sealed);
        }

        void Close() { _send({}); }

    private:
        struct State
        {
            mutex Locker {};
            vector<uint8_t> Received {};
            bool Disconnected {};
        };

        shared_ptr<State> _state;
        optional<SecureChannel> _channel {};
        vector<uint8_t> _plaintext {};
        InterthreadDataCallback _send {};
    };

    inline auto WaitForConnected(ptr<ClientEngine> client, ptr<ServerEngine> server, size_t expected_connections = 1) -> bool
    {
        for (int32_t i = 0; i < 2000; i++) {
            client->MainLoop();

            if (client->IsConnected() && client->GetCurPlayer() && GetServerConnectionCount(server) == expected_connections) {
                return true;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }

        return false;
    }

    inline auto WaitForDisconnected(ptr<ClientEngine> client, ptr<ServerEngine> server) -> bool
    {
        for (int32_t i = 0; i < 2000; i++) {
            client->MainLoop();

            if (!client->IsConnecting() && !client->IsConnected() && !client->GetCurPlayer() && GetServerConnectionCount(server) == 0) {
                return true;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }

        return false;
    }

    inline auto WaitForLearnedHash(ptr<ClientEngine> client, hstring::hash_t hash, string_view expected_string) -> bool
    {
        constexpr int32_t max_attempts = 15000;

        for (int32_t i = 0; i < max_attempts; i++) {
            client->MainLoop();

            bool failed = false;
            hstring resolved = client->Hashes.resolve_hash(hash, &failed);

            if (!failed && string_view {resolved.as_str()} == expected_string) {
                return true;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }

        return false;
    }

    inline auto WaitForUpdaterResult(Updater& updater) -> bool
    {
        for (int32_t i = 0; i < 2000; i++) {
            if (updater.Process()) {
                return true;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }

        return false;
    }
}

FO_END_NAMESPACE
