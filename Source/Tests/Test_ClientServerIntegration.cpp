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

#include <charconv>
#include <chrono>
#include <filesystem>

#include "catch_amalgamated.hpp"

#include "AngelScriptScripting.h"
#include "Application.h"
#include "Baker.h"
#include "Client.h"
#include "DataSerialization.h"
#include "ImGuiStuff.h"
#include "Server.h"
#include "Test_BakerHelpers.h"
#include "Updater.h"

FO_BEGIN_NAMESPACE

namespace TestClientServerIntegration
{
    static std::atomic_uint16_t IntegrationTestPort {46000};

    static auto MakeServerScriptBinary(const FileSystem& metadata_resources) -> vector<uint8_t>
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
        loggedPlayer.SwitchCritter(cr);
        SwitchedCritters++;

        // Entering a real map is what runs the load-map protocol, and each session gets its own location because
        // reusing one would need cover the login context does not carry
        hstring[] mapPids = {"UnitTestSharedMap".hstr()};
        Location loc = Game.CreateLocation("UnitTestSharedLocation".hstr(), mapPids);
        Map map = loc.GetMapByIndex(0);
        cr.TransferToMap(map, mpos(10, 10));

        // Inventory and map items each take their own send path to the owning client
        cr.AddItem("UnitTestSharedItem".hstr(), 3);
        map.AddItem(mpos(10, 10), "UnitTestSharedItem".hstr(), 1);

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
        Item npcItem = npc.AddItem("UnitTestSharedItem".hstr(), 1);
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
            Item added = cr.AddItem("UnitTestSharedItem".hstr(), 2);

            if (crMapCover !is null) {
                Game.MoveItem(added, 1, crMapCover, mpos(10, 10));
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

    static auto MakeClientScriptBinary(const FileSystem& metadata_resources) -> vector<uint8_t>
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
        CurMap.GetItems();

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
        CurMap.SetTransparentEgg(TransparentEggSlot::Secondary, here, ipos(0, 0), isize(8, 8));

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
        Chosen.CountItem("UnitTestSharedItem".hstr());
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

    static auto MakeServerTestSettings(uint16_t port) -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedServerSettings(settings);
        BakerTests::OverrideSetting(settings.ServerPort, port);

        return settings;
    }

    static auto MakeClientTestSettings(uint16_t port) -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedClientSettings(settings);
        BakerTests::OverrideSetting(settings.ServerPort, port);

        return settings;
    }

    static auto MakeTempClientUpdaterBakeDir(string_view name) -> string
    {
        FO_STACK_TRACE_ENTRY();

        std::chrono::steady_clock::rep suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        string dir_name = strex("lf_client_updater_{}_{}", name, suffix).str();
        std::filesystem::path base = std::filesystem::temp_directory_path() / std::filesystem::path {fs_make_path(dir_name)};
        return fs_path_to_string(base);
    }

    static auto PrepareClientUpdaterBakeOutput() -> string
    {
        FO_STACK_TRACE_ENTRY();

        string bake_dir = MakeTempClientUpdaterBakeDir("resources");
        string fonts_dir = strex(bake_dir).combine_path("Embedded/Fonts").str();

        REQUIRE(fs_create_directories(fonts_dir));

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

        REQUIRE(fs_write_file(strex(fonts_dir).combine_path("Default.fofnt").str(), default_font));

        vector<uint8_t> default_font_sprite = BakerTests::MakeMinimalBakedSprite();
        REQUIRE(fs_write_file(strex(fonts_dir).combine_path("Default.png").str(), default_font_sprite));

        return bake_dir;
    }

    // Authored content, so map creation runs the content generator instead of skipping it. Layout is the hash
    // table, then critter records, then item records; each record is an id, a proto hash and an empty props blob
    template<typename TEngine>
    static auto MakeStaticServerMapBlob(TEngine& engine) -> vector<uint8_t>
    {
        hstring critter_pid = engine.Hashes.ToHashedString("UnitTestSharedCritter");
        hstring item_pid = engine.Hashes.ToHashedString("UnitTestSharedItem");

        // A default-constructed property set still serializes to a non-empty blob, so it is produced here
        // rather than writing a zero size the reader cannot restore from
        auto make_default_props_blob = [&engine](string_view type_name) {
            auto registrar = engine.GetPropertyRegistrar(engine.Hashes.ToHashedString(type_name));
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
        auto writer = DataWriter(map_data);

        writer.Write<uint32_t>(BAKED_MAP_FILE_MAGIC);
        writer.Write<uint32_t>(BAKED_MAP_FILE_VERSION);

        vector<string> hashed_strings {string {critter_pid.as_str()}, string {item_pid.as_str()}};
        writer.Write<uint32_t>(numeric_cast<uint32_t>(hashed_strings.size()));

        for (const string& hashed_string : hashed_strings) {
            writer.Write<uint32_t>(numeric_cast<uint32_t>(hashed_string.length()));
            writer.WriteStringBytes(hashed_string);
        }

        writer.Write<uint32_t>(uint32_t {1});
        writer.Write<ident_t::underlying_type>(ident_t::underlying_type {5001});
        writer.Write<hstring::hash_t>(critter_pid.as_hash());
        writer.Write<uint32_t>(numeric_cast<uint32_t>(critter_props.size()));

        if (!critter_props.empty()) {
            writer.WriteBytes({critter_props.data(), critter_props.size()});
        }

        writer.Write<uint32_t>(uint32_t {1});
        writer.Write<ident_t::underlying_type>(ident_t::underlying_type {5002});
        writer.Write<hstring::hash_t>(item_pid.as_hash());
        writer.Write<uint32_t>(numeric_cast<uint32_t>(item_props.size()));

        if (!item_props.empty()) {
            writer.WriteBytes({item_props.data(), item_props.size()});
        }

        return map_data;
    }

    static auto MakeEmptyClientMapBlob() -> vector<uint8_t>
    {
        vector<uint8_t> map_data;
        auto writer = DataWriter(map_data);
        writer.Write<uint32_t>(BAKED_MAP_FILE_MAGIC);
        writer.Write<uint32_t>(BAKED_MAP_FILE_VERSION);
        writer.Write<uint32_t>(uint32_t {0});
        writer.Write<uint32_t>(uint32_t {0});
        return map_data;
    }

    template<typename TEngine>
    static auto MakeMapProtoBlob(TEngine& proto_engine, hstring type_name, string_view proto_name, msize map_size) -> vector<uint8_t>
    {
        vector<uint8_t> props_data;
        set<hstring> str_hashes;

        auto registrar = proto_engine.GetPropertyRegistrar(type_name);
        REQUIRE(static_cast<bool>(registrar));

        ProtoMap proto {proto_engine.Hashes.ToHashedString(proto_name), registrar};
        proto.SetSize(map_size);
        proto.GetProperties()->StoreAllData(props_data, str_hashes);

        vector<uint8_t> protos_data;
        auto writer = DataWriter(protos_data);

        writer.Write<uint32_t>(uint32_t {0});
        ignore_unused(str_hashes);
        writer.Write<uint32_t>(uint32_t {1});
        writer.Write<uint32_t>(uint32_t {1});
        writer.Write<uint16_t>(numeric_cast<uint16_t>(type_name.as_str().length()));
        writer.WriteStringBytes(type_name.as_str());
        writer.Write<uint16_t>(numeric_cast<uint16_t>(proto_name.length()));
        writer.WriteStringBytes(proto_name);
        writer.Write<uint32_t>(numeric_cast<uint32_t>(props_data.size()));

        if (!props_data.empty()) {
            writer.WriteBytes({props_data.data(), props_data.size()});
        }

        return protos_data;
    }

    static auto MakeServerTestResources() -> FileSystem
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
                    {"UnitTestLogin", "ClientServerIntegrationServer.fos", "In"},
                    {"UnitTestWorldStep", "ClientServerIntegrationServer.fos", "In", "int32", "", "step"},
                    {"UnitTestClientPing", "ClientServerIntegrationServer.fos", "Out", "int32", "", "value"},
                    // One call per direction carrying every wire-representable argument shape, so the
                    // marshalling walks each type's own branch on both sides
                    {"UnitTestEveryArgToServer", "ClientServerIntegrationServer.fos", "In", "int8", "", "i8", "int16", "", "i16", "int32", "", "i32", "int64", "", "i64", "uint8", "", "u8", "uint16", "", "u16", "uint32", "", "u32", "uint64", "", "u64", "float32", "", "f32", "float64", "", "f64", "bool", "", "flag", "string", "", "text", "hstring", "", "hash", "ident", "", "id", "timespan", "", "span", "ucolor", "", "color", "mpos", "", "hex", "ipos", "", "offset", "int32 [ ]", "", "ints", "string [ ]", "", "texts", "hstring [ ]", "", "hashes"},
                    {"UnitTestEveryArgToClient", "ClientServerIntegrationServer.fos", "Out", "int8", "", "i8", "int16", "", "i16", "int32", "", "i32", "int64", "", "i64", "uint8", "", "u8", "uint16", "", "u16", "uint32", "", "u32", "uint64", "", "u64", "float32", "", "f32", "float64", "", "f64", "bool", "", "flag", "string", "", "text", "hstring", "", "hash", "ident", "", "id", "timespan", "", "span", "ucolor", "", "color", "mpos", "", "hex", "ipos", "", "offset", "int32 [ ]", "", "ints", "string [ ]", "", "texts", "hstring [ ]", "", "hashes"},
                }},
        });

        auto compiler_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ClientServerServerCompilerResources");
        compiler_source->AddFile("Metadata.fometa-server", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_source));

        BakerServerEngine proto_engine {compiler_resources};
        hstring critter_type = proto_engine.Hashes.ToHashedString("Critter");
        auto proto_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "UnitTestSharedCritter");

        // A location plus its map lets the logged-in critter enter the world, which is what drives the
        // client through the load-map / add-critter / property-sync protocol
        hstring location_type = proto_engine.Hashes.ToHashedString("Location");
        hstring map_type = proto_engine.Hashes.ToHashedString("Map");
        hstring item_type = proto_engine.Hashes.ToHashedString("Item");
        auto location_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoLocation>(proto_engine, location_type, "UnitTestSharedLocation");
        auto item_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(proto_engine, item_type, "UnitTestSharedItem");
        auto map_blob = MakeMapProtoBlob(proto_engine, map_type, "UnitTestSharedMap", msize {50, 50});
        auto fomap_blob = MakeStaticServerMapBlob(proto_engine);

        auto script_blob = MakeServerScriptBinary(compiler_resources);

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ClientServerServerRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
        runtime_source->AddFile("ClientServerIntegration.fopro-bin-server", proto_blob);
        runtime_source->AddFile("ClientServerIntegrationLocation.fopro-bin-server", location_blob);
        runtime_source->AddFile("ClientServerIntegrationItem.fopro-bin-server", item_blob);
        runtime_source->AddFile("UnitTestSharedMap.fopro-bin-server", map_blob);
        runtime_source->AddFile("UnitTestSharedMap.fomap-bin-server", fomap_blob);
        runtime_source->AddFile("ClientServerIntegration.fos-bin-server", script_blob);

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));
        return resources;
    }

    static auto MakeClientTestResources() -> FileSystem
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
                    {"UnitTestLogin", "ClientServerIntegrationClient.fos", "Out"},
                    {"UnitTestWorldStep", "ClientServerIntegrationClient.fos", "Out", "int32", "", "step"},
                    {"UnitTestClientPing", "ClientServerIntegrationClient.fos", "In", "int32", "", "value"},
                    {"UnitTestEveryArgToServer", "ClientServerIntegrationClient.fos", "Out", "int8", "", "i8", "int16", "", "i16", "int32", "", "i32", "int64", "", "i64", "uint8", "", "u8", "uint16", "", "u16", "uint32", "", "u32", "uint64", "", "u64", "float32", "", "f32", "float64", "", "f64", "bool", "", "flag", "string", "", "text", "hstring", "", "hash", "ident", "", "id", "timespan", "", "span", "ucolor", "", "color", "mpos", "", "hex", "ipos", "", "offset", "int32 [ ]", "", "ints", "string [ ]", "", "texts", "hstring [ ]", "", "hashes"},
                    {"UnitTestEveryArgToClient", "ClientServerIntegrationClient.fos", "In", "int8", "", "i8", "int16", "", "i16", "int32", "", "i32", "int64", "", "i64", "uint8", "", "u8", "uint16", "", "u16", "uint32", "", "u32", "uint64", "", "u64", "float32", "", "f32", "float64", "", "f64", "bool", "", "flag", "string", "", "text", "hstring", "", "hash", "ident", "", "id", "timespan", "", "span", "ucolor", "", "color", "mpos", "", "hex", "ipos", "", "offset", "int32 [ ]", "", "ints", "string [ ]", "", "texts", "hstring [ ]", "", "hashes"},
                }},
        });

        auto compiler_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ClientServerClientCompilerResources");
        compiler_source->AddFile("Metadata.fometa-client", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_source));

        BakerClientEngine proto_engine {compiler_resources};
        hstring critter_type = proto_engine.Hashes.ToHashedString("Critter");
        auto proto_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "UnitTestSharedCritter");

        hstring location_type = proto_engine.Hashes.ToHashedString("Location");
        hstring map_type = proto_engine.Hashes.ToHashedString("Map");
        hstring item_type = proto_engine.Hashes.ToHashedString("Item");
        auto location_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoLocation>(proto_engine, location_type, "UnitTestSharedLocation");
        auto item_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(proto_engine, item_type, "UnitTestSharedItem");
        auto map_blob = MakeMapProtoBlob(proto_engine, map_type, "UnitTestSharedMap", msize {50, 50});
        auto fomap_blob = MakeEmptyClientMapBlob();

        auto script_blob = MakeClientScriptBinary(compiler_resources);

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ClientServerClientRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-client", metadata_blob);
        runtime_source->AddFile("ClientServerIntegration.fopro-bin-client", proto_blob);
        runtime_source->AddFile("ClientServerIntegrationLocation.fopro-bin-client", location_blob);
        runtime_source->AddFile("ClientServerIntegrationItem.fopro-bin-client", item_blob);
        runtime_source->AddFile("UnitTestSharedMap.fopro-bin-client", map_blob);
        runtime_source->AddFile("UnitTestSharedMap.fomap-bin-client", fomap_blob);
        runtime_source->AddFile("ClientServerIntegration.fos-bin-client", script_blob);

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));
        return resources;
    }

    static auto MakeServerEngine(GlobalSettings& settings) -> refcount_ptr<ServerEngine>
    {
        return SafeAlloc::MakeRefCounted<ServerEngine>(&settings, MakeServerTestResources());
    }

    static auto MakeClientEngine(GlobalSettings& settings) -> refcount_ptr<ClientEngine>
    {
        return SafeAlloc::MakeRefCounted<ClientEngine>(&settings, MakeClientTestResources(), &GetApp()->MainWindow);
    }

    static auto WaitForServerStart(ptr<ServerEngine> server) -> string
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

    static auto GetServerConnectionCount(ptr<ServerEngine> server) -> size_t
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

    static auto WaitForServerConnectionCount(ptr<ServerEngine> server, size_t expected_connections) -> bool
    {
        for (int32_t i = 0; i < 2000; i++) {
            if (GetServerConnectionCount(server) == expected_connections) {
                return true;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }

        return false;
    }

    static auto WaitForConnected(ptr<ClientEngine> client, ptr<ServerEngine> server, size_t expected_connections = 1) -> bool
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

    static auto WaitForDisconnected(ptr<ClientEngine> client, ptr<ServerEngine> server) -> bool
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

    static auto WaitForLearnedHash(ptr<ClientEngine> client, hstring::hash_t hash, string_view expected_string) -> bool
    {
        constexpr int32_t max_attempts = 15000;

        for (int32_t i = 0; i < max_attempts; i++) {
            client->MainLoop();

            bool failed = false;
            hstring resolved = client->Hashes.ResolveHash(hash, &failed);

            if (!failed && string_view {resolved.as_str()} == expected_string) {
                return true;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }

        return false;
    }

    static auto WaitForUpdaterResult(Updater& updater) -> bool
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

TEST_CASE("ClientAndServerHandshakeOverInterthreadTransport")
{
    using namespace TestClientServerIntegration;

    auto port = IntegrationTestPort.fetch_add(1);

    auto server_settings = MakeServerTestSettings(port);
    auto client_settings = MakeClientTestSettings(port);

    auto server = MakeServerEngine(server_settings);
    auto client = MakeClientEngine(client_settings);

    auto shutdown = scope_exit([&server, &client]() noexcept {
        safe_call([&client] { client->Shutdown(); });

        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    CHECK(GetServerConnectionCount(server) == 0);
    CHECK_FALSE(client->IsConnecting());
    CHECK_FALSE(client->IsConnected());
    CHECK_FALSE(static_cast<bool>(client->GetCurPlayer()));

    auto get_client_func_name = [&client](string_view name) { return client->Hashes.ToHashedString(name); };

    int connecting_calls = 0;
    int connected_calls = 0;
    int login_success_calls = 0;
    int disconnected_calls = 0;

    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetConnectingCalls"), connecting_calls));
    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetConnectedCalls"), connected_calls));
    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetLoginSuccessCalls"), login_success_calls));
    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetDisconnectedCalls"), disconnected_calls));

    CHECK(connecting_calls == 0);
    CHECK(connected_calls == 0);
    CHECK(login_success_calls == 0);
    CHECK(disconnected_calls == 0);

    client->Connect();

    REQUIRE(WaitForConnected(client, server));

    CHECK(client->IsConnected());
    CHECK_FALSE(client->IsConnecting());
    REQUIRE(static_cast<bool>(client->GetCurPlayer()));
    CHECK(client->GetConnection()->GetBytesSend() > 0);
    CHECK(client->GetConnection()->GetBytesReceived() > 0);
    CHECK(GetServerConnectionCount(server) == 1);

    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetConnectingCalls"), connecting_calls));
    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetConnectedCalls"), connected_calls));
    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetLoginSuccessCalls"), login_success_calls));
    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetDisconnectedCalls"), disconnected_calls));

    CHECK(connecting_calls >= 1);
    CHECK(connected_calls >= 1);
    CHECK(login_success_calls == 0);
    CHECK(disconnected_calls == 0);

    client->Disconnect();

    REQUIRE(WaitForDisconnected(client, server));

    CHECK_FALSE(client->IsConnecting());
    CHECK_FALSE(client->IsConnected());
    CHECK_FALSE(static_cast<bool>(client->GetCurPlayer()));
    CHECK(GetServerConnectionCount(server) == 0);

    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetDisconnectedCalls"), disconnected_calls));
    CHECK(disconnected_calls >= 1);
}

TEST_CASE("ClientLogsInThroughARemoteCall")
{
    using namespace TestClientServerIntegration;

    auto port = IntegrationTestPort.fetch_add(1);

    auto server_settings = MakeServerTestSettings(port);
    auto client_settings = MakeClientTestSettings(port);

    auto server = MakeServerEngine(server_settings);
    auto client = MakeClientEngine(client_settings);

    auto shutdown = scope_exit([&server, &client]() noexcept {
        safe_call([&client] { client->Shutdown(); });

        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    client->Connect();
    REQUIRE(WaitForConnected(client, server));

    // The connected-but-not-logged-in session only accepts a remote call, which is how a real client logs in
    REQUIRE(client->CallFunc<void>(client->Hashes.ToHashedString("ClientServerIntegrationClient::UnitTestSendLogin")));

    int32_t login_success_calls = 0;
    bool logged_in = false;

    for (int32_t i = 0; i < 2000 && !logged_in; i++) {
        client->MainLoop();

        REQUIRE(client->CallFunc(client->Hashes.ToHashedString("ClientServerIntegrationClient::UnitTestGetLoginSuccessCalls"), login_success_calls));
        logged_in = login_success_calls >= 1;

        if (!logged_in) {
            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }
    }

    CHECK(logged_in);
    CHECK(login_success_calls >= 1);

    int32_t server_login_calls = 0;
    REQUIRE(server->CallFunc(server->Hashes.ToHashedString("ClientServerIntegrationServer::UnitTestGetLoginCalls"), server_login_calls));
    CHECK(server_login_calls == 1);

    CHECK(client->IsConnected());
    REQUIRE(static_cast<bool>(client->GetCurPlayer()));

    int32_t switched_critters = 0;
    REQUIRE(server->CallFunc(server->Hashes.ToHashedString("ClientServerIntegrationServer::UnitTestGetSwitchedCritters"), switched_critters));
    CHECK(switched_critters == 1);

    // The controlled critter arrives over the wire, so the client ends up with a chosen critter of its own
    bool has_chosen = false;

    for (int32_t i = 0; i < 2000 && !has_chosen; i++) {
        client->MainLoop();
        has_chosen = static_cast<bool>(client->GetChosen());

        if (!has_chosen) {
            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }
    }

    CHECK(has_chosen);

    // Player commands round-trip: the client sends move and direction, the server validates and applies them
    int32_t drive_result = -1;
    REQUIRE(client->CallFunc(client->Hashes.ToHashedString("ClientServerIntegrationClient::UnitTestDriveChosen"), drive_result));
    CHECK(drive_result == 0);

    for (int32_t i = 0; i < 200; i++) {
        client->MainLoop();
        std::this_thread::sleep_for(std::chrono::milliseconds {2});
    }

    int32_t npc_critters = 0;
    REQUIRE(server->CallFunc(server->Hashes.ToHashedString("ClientServerIntegrationServer::UnitTestGetNpcCritters"), npc_critters));
    CHECK(npc_critters == 1);

    int32_t write_result = -1;
    REQUIRE(client->CallFunc(client->Hashes.ToHashedString("ClientServerIntegrationClient::UnitTestWriteChosenProperty"), write_result));
    CHECK(write_result == 0);

    for (int32_t i = 0; i < 100; i++) {
        client->MainLoop();
        std::this_thread::sleep_for(std::chrono::milliseconds {2});
    }

    // Drive the server-side world steps through the second remote call, so each one runs with proper cover
    for (int32_t step = 0; step < 6; step++) {
        REQUIRE(client->CallFunc<void, int32_t>(client->Hashes.ToHashedString("ClientServerIntegrationClient::UnitTestSendWorldStep"), step));

        for (int32_t i = 0; i < 100; i++) {
            client->MainLoop();
            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }
    }

    int32_t client_pings = 0;
    REQUIRE(client->CallFunc(client->Hashes.ToHashedString("ClientServerIntegrationClient::UnitTestGetClientPings"), client_pings));
    CHECK(client_pings == 42);

    // The login handler pushes the controlled critter's inventory down as a separate item payload, and
    // those views must not present themselves as items lying on a map hex
    int32_t received_items = 0;
    int32_t received_map_owned_items = -1;
    REQUIRE(client->CallFunc(client->Hashes.ToHashedString("ClientServerIntegrationClient::UnitTestGetReceivedItemCount"), received_items));
    REQUIRE(client->CallFunc(client->Hashes.ToHashedString("ClientServerIntegrationClient::UnitTestGetReceivedMapOwnedItemCount"), received_map_owned_items));
    CHECK(received_items > 0);
    CHECK(received_map_owned_items == 0);

    int32_t action_context_items = 0;
    int32_t action_map_owned_context_items = -1;
    REQUIRE(client->CallFunc(client->Hashes.ToHashedString("ClientServerIntegrationClient::UnitTestGetActionContextItemCount"), action_context_items));
    REQUIRE(client->CallFunc(client->Hashes.ToHashedString("ClientServerIntegrationClient::UnitTestGetActionMapOwnedContextItemCount"), action_map_owned_context_items));
    CHECK(action_context_items > 0);
    CHECK(action_map_owned_context_items == 0);

    // Movement and property writes from the client side enter the server through their own message
    // handlers, which nothing else in the suite reaches
    int32_t movement_result = -1;
    REQUIRE(client->CallFunc(client->Hashes.ToHashedString("ClientServerIntegrationClient::UnitTestDriveChosenMovement"), movement_result));
    CHECK(movement_result == 0);

    for (int32_t i = 0; i < 200; i++) {
        client->MainLoop();
        std::this_thread::sleep_for(std::chrono::milliseconds {2});
    }

    // Every wire-representable argument shape, sent to the server and echoed back
    int32_t every_arg_result = -1;
    REQUIRE(client->CallFunc(client->Hashes.ToHashedString("ClientServerIntegrationClient::UnitTestSendEveryArg"), every_arg_result));
    CHECK(every_arg_result == 0);

    int32_t every_arg_echoes = 0;

    for (int32_t i = 0; i < 500 && every_arg_echoes == 0; i++) {
        client->MainLoop();
        REQUIRE(client->CallFunc(client->Hashes.ToHashedString("ClientServerIntegrationClient::UnitTestGetEveryArgEchoes"), every_arg_echoes));

        if (every_arg_echoes == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }
    }

    CHECK(every_arg_echoes == 1);

    int32_t every_arg_echo_mismatch = -1;
    REQUIRE(client->CallFunc(client->Hashes.ToHashedString("ClientServerIntegrationClient::UnitTestGetEveryArgEchoMismatch"), every_arg_echo_mismatch));
    CHECK(every_arg_echo_mismatch == 0);

    int32_t every_arg_calls = 0;
    REQUIRE(server->CallFunc(server->Hashes.ToHashedString("ClientServerIntegrationServer::UnitTestGetEveryArgCalls"), every_arg_calls));
    CHECK(every_arg_calls == 1);

    int32_t every_arg_mismatch = -1;
    REQUIRE(server->CallFunc(server->Hashes.ToHashedString("ClientServerIntegrationServer::UnitTestGetEveryArgMismatch"), every_arg_mismatch));
    CHECK(every_arg_mismatch == 0);

    int32_t world_steps = 0;
    REQUIRE(server->CallFunc(server->Hashes.ToHashedString("ClientServerIntegrationServer::UnitTestGetWorldSteps"), world_steps));
    CHECK(world_steps == 6);

    // The world arrives asynchronously and the instrumented build is much slower, so the inspection polls
    // instead of asserting on the first attempt
    int32_t inspect_result = -1;

    for (int32_t i = 0; i < 1000 && inspect_result != 0; i++) {
        client->MainLoop();
        REQUIRE(client->CallFunc(client->Hashes.ToHashedString("ClientServerIntegrationClient::UnitTestInspectChosenWorld"), inspect_result));

        if (inspect_result != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }
    }

    CHECK(inspect_result == 0);

    // Fog of war is a client-only path: the mapper's map view returns from PrepareFogToDraw before it
    // starts, so the shapes and their draw slots are only reachable from a session map
    {
        auto session_map = client->GetCurMap();
        REQUIRE(session_map);
        ptr<MapView> map_view = session_map.as_ptr();

        REQUIRE(ImGui::GetCurrentContext() == nullptr);
        ImGuiExt::Init();

        auto destroy_context = scope_exit([]() noexcept {
            safe_call([] {
                if (ImGui::GetCurrentContext() != nullptr) {
                    ImGui::DestroyContext();
                }
            });
        });

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2 {1280.0f, 720.0f};
        io.DeltaTime = 1.0f / 60.0f;
        io.IniFilename = nullptr;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

        // A hex-anchored shape and one that follows the chosen critter are the two ways a fog layer is made
        REQUIRE_NOTHROW(ignore_unused(map_view->AddFog(mpos {8, 8}, DrawOrderType::Light, nullptr).get()));
        REQUIRE_NOTHROW(ignore_unused(map_view->AddFog(client->GetChosen(), DrawOrderType::Light, nullptr).get()));
        REQUIRE_NOTHROW(map_view->RebuildFog());

        for (int32_t frame = 0; frame < 4; frame++) {
            ImGui::NewFrame();
            REQUIRE_NOTHROW(map_view->Process());
            REQUIRE_NOTHROW(map_view->DrawMap());
            ImGui::Render();
        }

        // Scrolling walks hexes in and out of the view field, which is what runs the per-hex show/hide
        // bookkeeping over real content instead of an empty viewport
        for (const mpos& center : {mpos {4, 4}, mpos {12, 12}, mpos {8, 4}, mpos {10, 10}}) {
            REQUIRE_NOTHROW(map_view->InstantScrollTo(center));

            ImGui::NewFrame();
            REQUIRE_NOTHROW(map_view->Process());
            REQUIRE_NOTHROW(map_view->DrawMap());
            ImGui::Render();
        }

        // A smooth scroll advances over frames rather than snapping, and zoom rebuilds the view field
        REQUIRE_NOTHROW(map_view->ScrollToHex(mpos {6, 6}, ipos16 {0, 0}, 20, true));

        for (float32_t zoom : {1.5f, 0.75f, 1.0f}) {
            REQUIRE_NOTHROW(map_view->ChangeZoom(zoom, fpos32 {0.5f, 0.5f}));

            ImGui::NewFrame();
            REQUIRE_NOTHROW(map_view->Process());
            REQUIRE_NOTHROW(map_view->DrawMap());
            ImGui::Render();
        }
    }

    // The server diagnostic panels only have real rows to render once a world exists, which is exactly the
    // state this session leaves behind: a logged-in player, a location with a map, critters and items
    {
        REQUIRE(ImGui::GetCurrentContext() == nullptr);
        ImGuiExt::Init();

        auto destroy_context = scope_exit([]() noexcept {
            safe_call([] {
                if (ImGui::GetCurrentContext() != nullptr) {
                    ImGui::DestroyContext();
                }
            });
        });

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2 {1280.0f, 720.0f};
        io.DeltaTime = 1.0f / 60.0f;
        io.IniFilename = nullptr;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

        // Collapsing headers opt out of the log auto-expansion, so the root panels are seeded open by hand;
        // otherwise the per-map, per-critter and per-item rows never render even with a populated world
        constexpr std::array ROOT_PANEL_IDS = {"Info", "Performance details", "###Players", "###NotLoggedInPlayers", "###Locations", "Data base"};

        for (int32_t frame = 0; frame < 3; frame++) {
            ImGui::NewFrame();

            if (ImGui::Begin("ServerDiagnostics")) {
                ptr<ImGuiWindow> window = ImGui::GetCurrentWindow();

                for (string_view panel_id : ROOT_PANEL_IDS) {
                    window->StateStorage.SetInt(ImGui::GetID(panel_id.data(), panel_id.data() + panel_id.size()), 1);
                }

                ImGui::LogToBuffer(12);
                REQUIRE_NOTHROW(server->DrawGui());
                ImGui::LogFinish();
            }

            ImGui::End();
            ImGui::Render();
        }
    }

    client->Disconnect();
    REQUIRE(WaitForDisconnected(client, server));
}

TEST_CASE("TwoClientsShareOneMapSession")
{
    using namespace TestClientServerIntegration;

    auto port = IntegrationTestPort.fetch_add(1);

    auto server_settings = MakeServerTestSettings(port);
    auto first_settings = MakeClientTestSettings(port);
    auto second_settings = MakeClientTestSettings(port);

    auto server = MakeServerEngine(server_settings);
    auto first = MakeClientEngine(first_settings);
    auto second = MakeClientEngine(second_settings);

    auto shutdown = scope_exit([&server, &first, &second]() noexcept {
        safe_call([&second] { second->Shutdown(); });
        safe_call([&first] { first->Shutdown(); });

        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    auto login = [&server](ptr<ClientEngine> client, size_t expected_connections) {
        client->Connect();
        REQUIRE(WaitForConnected(client, server, expected_connections));
        REQUIRE(client->CallFunc<void>(client->Hashes.ToHashedString("ClientServerIntegrationClient::UnitTestSendLogin")));

        for (int32_t i = 0; i < 2000; i++) {
            client->MainLoop();

            if (client->GetChosen()) {
                return true;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }

        return false;
    };

    REQUIRE(login(first.as_ptr(), 1));
    REQUIRE(login(second.as_ptr(), 2));

    // Both sessions land on their own map instance, so each still sees exactly one player critter of its
    // own; what this drives is the server fanning world state out to two independent player views at once
    for (int32_t i = 0; i < 300; i++) {
        first->MainLoop();
        second->MainLoop();
        std::this_thread::sleep_for(std::chrono::milliseconds {2});
    }

    int32_t first_result = -1;
    int32_t second_result = -1;

    for (int32_t i = 0; i < 1000 && (first_result != 0 || second_result != 0); i++) {
        first->MainLoop();
        second->MainLoop();
        REQUIRE(first->CallFunc(first->Hashes.ToHashedString("ClientServerIntegrationClient::UnitTestInspectChosenWorld"), first_result));
        REQUIRE(second->CallFunc(second->Hashes.ToHashedString("ClientServerIntegrationClient::UnitTestInspectChosenWorld"), second_result));

        if (first_result != 0 || second_result != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }
    }

    CHECK(first_result == 0);
    CHECK(second_result == 0);

    // Dropping one session must not disturb the other
    first->Disconnect();
    REQUIRE(WaitForServerConnectionCount(server, 1));

    for (int32_t i = 0; i < 100; i++) {
        second->MainLoop();
        std::this_thread::sleep_for(std::chrono::milliseconds {2});
    }

    CHECK(second->IsConnected());
    CHECK(static_cast<bool>(second->GetChosen()));

    second->Disconnect();
    REQUIRE(WaitForServerConnectionCount(server, 0));
}

TEST_CASE("ServerRejectsMalformedPreHandshakePayloadWithoutExceptionReport")
{
    using namespace TestClientServerIntegration;

    auto port = IntegrationTestPort.fetch_add(1);
    auto server_settings = MakeServerTestSettings(port);
    auto server = MakeServerEngine(server_settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(InterthreadListeners.count(port) == 1);

    auto previous_exception_callback = GetExceptionCallback();
    std::atomic_int exception_reports {};
    SetExceptionCallback([&exception_reports](string_view, const CatchedStackTraceData&, bool) { exception_reports.fetch_add(1); });
    auto restore_exception_callback = scope_exit([previous = std::move(previous_exception_callback)]() mutable noexcept { SetExceptionCallback(std::move(previous)); });

    std::atomic_bool disconnected {};
    auto send_to_server = InterthreadListeners[port]([&disconnected](const_span<uint8_t> data) {
        if (data.empty()) {
            disconnected.store(true);
        }
    });
    REQUIRE(send_to_server);
    REQUIRE(WaitForServerConnectionCount(server, 1));

    auto malformed_handshake = NetOutBuffer(64);
    malformed_handshake.StartMsg(NetMessage::Handshake);
    malformed_handshake.Write<uint32_t>(std::numeric_limits<uint32_t>::max());
    malformed_handshake.Write<uint16_t>(uint16_t {0});
    malformed_handshake.EndMsg();
    send_to_server(malformed_handshake.GetData());

    REQUIRE(WaitForServerConnectionCount(server, 0));
    CHECK(disconnected.load());
    CHECK(exception_reports.load() == 0);
}

TEST_CASE("ServerDisconnectsPreLoginConnectionAfterLoginTimeout")
{
    using namespace TestClientServerIntegration;

    uint16_t port = IntegrationTestPort.fetch_add(1);
    auto server_settings = MakeServerTestSettings(port);
    BakerTests::OverrideSetting(server_settings.InactivityDisconnectTime, 0);
    BakerTests::OverrideSetting(server_settings.LoginTimeout, 25);
    auto server = MakeServerEngine(server_settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(InterthreadListeners.count(port) == 1);

    std::atomic_bool disconnected {};
    auto send_to_server = InterthreadListeners[port]([&disconnected](const_span<uint8_t> data) {
        if (data.empty()) {
            disconnected.store(true);
        }
    });
    REQUIRE(send_to_server);
    REQUIRE(WaitForServerConnectionCount(server, 1));

    for (int32_t i = 0; i < 2000 && !disconnected.load(); i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds {2});
    }

    CHECK(disconnected.load());
    CHECK(WaitForServerConnectionCount(server, 0));
}

TEST_CASE("ServerReportsMetadataMismatchInHandshake")
{
    using namespace TestClientServerIntegration;

    uint16_t port = IntegrationTestPort.fetch_add(1);
    auto server_settings = MakeServerTestSettings(port);
    BakerTests::OverrideSetting(server_settings.DisableZlibCompression, true);
    auto server = MakeServerEngine(server_settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(InterthreadListeners.count(port) == 1);

    mutex received_data_lock;
    vector<uint8_t> received_data;
    auto send_to_server = InterthreadListeners[port]([&received_data_lock, &received_data](const_span<uint8_t> data) {
        if (!data.empty()) {
            scoped_lock locker {received_data_lock};
            received_data.insert(received_data.end(), data.begin(), data.end());
        }
    });
    REQUIRE(send_to_server);
    REQUIRE(WaitForServerConnectionCount(server, 1));

    // A binary-compatible client whose resources come from another bake: the layout verdict is what keeps its
    // property payloads from reaching deserialization
    REQUIRE_FALSE(server->GetMetadataVersion().empty());
    auto handshake = NetOutBuffer(128);
    handshake.StartMsg(NetMessage::Handshake);
    handshake.Write(server_settings.CompatibilityVersion);
    handshake.Write<string_view>("0123456789abcdef");
    handshake.Write<uint32_t>(FO_UPDATER_VERSION);
    handshake.Write<string_view>("Linux-x64");
    handshake.Write<uint32_t>(0x12345678);
    handshake.EndMsg();
    send_to_server(handshake.GetData());

    bool received_answer = false;

    for (int32_t i = 0; i < 2000 && !received_answer; i++) {
        vector<uint8_t> response_data;
        {
            scoped_lock locker {received_data_lock};
            response_data = received_data;
        }

        if (!response_data.empty()) {
            NetInBuffer response {response_data.size()};
            response.AddData(response_data);

            if (response.NeedProcess()) {
                REQUIRE(response.ReadMsg() == NetMessage::HandshakeAnswer);
                CHECK_FALSE(response.Read<bool>());
                CHECK_FALSE(response.Read<bool>());
                CHECK(response.Read<bool>());
                CHECK(response.Read<string>() == server->GetMetadataVersion());
                uint32_t response_encrypt_key = response.Read<uint32_t>();
                CHECK(response_encrypt_key != 0);
                received_answer = true;
            }
        }

        if (!received_answer) {
            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }
    }

    REQUIRE(received_answer);
    send_to_server({});
    CHECK(WaitForServerConnectionCount(server, 0));
}

TEST_CASE("ServerRejectsUnsafeUpdaterGenerationBeforeInitData")
{
    using namespace TestClientServerIntegration;

    uint16_t port = IntegrationTestPort.fetch_add(1);
    auto server_settings = MakeServerTestSettings(port);
    BakerTests::OverrideSetting(server_settings.DisableZlibCompression, true);
    auto server = MakeServerEngine(server_settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(InterthreadListeners.count(port) == 1);

    mutex received_data_lock;
    vector<uint8_t> received_data;
    auto send_to_server = InterthreadListeners[port]([&received_data_lock, &received_data](const_span<uint8_t> data) {
        if (!data.empty()) {
            scoped_lock locker {received_data_lock};
            received_data.insert(received_data.end(), data.begin(), data.end());
        }
    });
    REQUIRE(send_to_server);
    REQUIRE(WaitForServerConnectionCount(server, 1));

    static_assert(FO_UPDATER_VERSION > 1);
    auto handshake = NetOutBuffer(128);
    handshake.StartMsg(NetMessage::Handshake);
    handshake.Write(server_settings.CompatibilityVersion);
    handshake.Write(server->GetMetadataVersion());
    handshake.Write<uint32_t>(FO_UPDATER_VERSION - 1);
    handshake.Write<string_view>("Linux-x64");
    handshake.Write<uint32_t>(0x12345678);
    handshake.EndMsg();
    send_to_server(handshake.GetData());

    bool received_rejection = false;
    for (int32_t i = 0; i < 2000 && !received_rejection; i++) {
        vector<uint8_t> response_data;
        {
            scoped_lock locker {received_data_lock};
            response_data = received_data;
        }

        if (!response_data.empty()) {
            NetInBuffer response {response_data.size()};
            response.AddData(response_data);

            if (response.NeedProcess()) {
                REQUIRE(response.ReadMsg() == NetMessage::HandshakeAnswer);
                CHECK_FALSE(response.Read<bool>());
                CHECK(response.Read<bool>());
                CHECK_FALSE(response.Read<bool>());
                CHECK(response.Read<string>() == server->GetMetadataVersion());
                uint32_t response_encrypt_key = response.Read<uint32_t>();
                CHECK(response_encrypt_key != 0);
                response.SetEncryptKey(response_encrypt_key);

                if (response.NeedProcess()) {
                    REQUIRE(response.ReadMsg() == NetMessage::Disconnect);
                    response.ShrinkReadBuf();
                    CHECK(response.GetDataSize() == 0);
                    received_rejection = true;
                }
            }
        }

        if (!received_rejection) {
            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }
    }

    REQUIRE(received_rejection);
    send_to_server({});
    REQUIRE(WaitForServerConnectionCount(server, 0));
}

TEST_CASE("ClientShutdownDisconnectsActiveConnection")
{
    using namespace TestClientServerIntegration;

    auto port = IntegrationTestPort.fetch_add(1);

    auto server_settings = MakeServerTestSettings(port);
    auto client_settings = MakeClientTestSettings(port);

    auto server = MakeServerEngine(server_settings);
    auto client = MakeClientEngine(client_settings);
    bool client_shutdown = false;
    int32_t shutdown_disconnected_calls = 0;

    auto shutdown = scope_exit([&server, &client, &client_shutdown]() noexcept {
        safe_call([&client, &client_shutdown] {
            if (!client_shutdown) {
                client->Shutdown();
            }
        });

        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    client->Connect();
    REQUIRE(WaitForConnected(client, server));

    Entity::EventCallbackData shutdown_disconnect_observer;
    shutdown_disconnect_observer.Callback = [&shutdown_disconnected_calls](FuncCallData&) {
        shutdown_disconnected_calls++;
        return Entity::EventResult::ContinueChain;
    };
    shutdown_disconnect_observer.SubscriptionPtr = reinterpret_cast<uintptr_t>(&shutdown_disconnected_calls);
    client->OnDisconnected.Subscribe(std::move(shutdown_disconnect_observer));

    client->Shutdown();
    client_shutdown = true;

    CHECK_FALSE(client->IsConnecting());
    CHECK_FALSE(client->IsConnected());
    CHECK_FALSE(static_cast<bool>(client->GetCurPlayer()));
    CHECK(shutdown_disconnected_calls == 1);
    REQUIRE(WaitForServerConnectionCount(server, 0));
}

TEST_CASE("ClientAndServerInterthreadConnectionKeepsProcessingAfterHandshake")
{
    using namespace TestClientServerIntegration;

    auto port = IntegrationTestPort.fetch_add(1);

    auto server_settings = MakeServerTestSettings(port);
    auto client_settings = MakeClientTestSettings(port);

    auto server = MakeServerEngine(server_settings);
    auto client = MakeClientEngine(client_settings);

    auto shutdown = scope_exit([&server, &client]() noexcept {
        safe_call([&client] {
            client->Disconnect();
            client->Shutdown();
        });

        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    client->Connect();
    REQUIRE(WaitForConnected(client, server));

    size_t bytes_received_after_handshake = client->GetConnection()->GetBytesReceived();

    bool received_post_handshake_packet = false;

    for (int32_t i = 0; i < 1000; i++) {
        client->MainLoop();

        if (client->GetConnection()->GetBytesReceived() > bytes_received_after_handshake) {
            received_post_handshake_packet = true;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds {2});
    }

    CHECK(received_post_handshake_packet);
}

TEST_CASE("ClientReportsUnresolvedHashAndLearnsWithoutDisconnect")
{
    using namespace TestClientServerIntegration;

    auto port = IntegrationTestPort.fetch_add(1);

    auto server_settings = MakeServerTestSettings(port);
    auto client_settings = MakeClientTestSettings(port);

    auto server = MakeServerEngine(server_settings);
    auto client = MakeClientEngine(client_settings);

    auto shutdown = scope_exit([&server, &client]() noexcept {
        safe_call([&client] { client->Shutdown(); });

        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    client->Connect();
    REQUIRE(WaitForConnected(client, server));

    // A string the server knows but the client doesn't РІР‚вЂќ mimics a runtime hstring the client can't resolve
    hstring reported = server->Hashes.ToHashedString("integration_test_only_hash");

    // Send the exact wire message ClientEngine emits when it hits an unresolved hash
    client->GetConnection()->OutBuf->StartMsg(NetMessage::UnresolvedHash);
    client->GetConnection()->OutBuf->Write<hstring::hash_t>(reported.as_hash());
    client->GetConnection()->OutBuf->EndMsg();

    REQUIRE(WaitForLearnedHash(client, reported.as_hash(), "integration_test_only_hash"));
    CHECK(client->IsConnected());
    CHECK(GetServerConnectionCount(server) == 1);

    auto second_client = MakeClientEngine(client_settings);
    auto shutdown_second_client = scope_exit([&second_client]() noexcept { safe_call([&second_client] { second_client->Shutdown(); }); });

    second_client->Connect();
    REQUIRE(WaitForConnected(second_client, server, 2));
    REQUIRE(WaitForLearnedHash(second_client, reported.as_hash(), "integration_test_only_hash"));
    CHECK(GetServerConnectionCount(server) == 2);
}

TEST_CASE("ClientUpdaterConsumesReportedHashListDuringHandshake")
{
    using namespace TestClientServerIntegration;

    auto port = IntegrationTestPort.fetch_add(1);

    auto server_settings = MakeServerTestSettings(port);
    auto client_settings = MakeClientTestSettings(port);
    string updater_bake_output = PrepareClientUpdaterBakeOutput();
    auto cleanup_updater_bake_output = scope_exit([&updater_bake_output]() noexcept { fs_remove_dir_tree(updater_bake_output); });
    BakerTests::OverrideSetting(client_settings.BakeOutput, updater_bake_output);

    // The rig has no resource packs to read a version back from, and this case is about the hash list rather
    // than about pack reading, so the client reports the version the test metadata carries
    BakerTests::OverrideSetting(client_settings.ForceMetadataVersion, string(BakerTests::TEST_METADATA_VERSION));

    auto server = MakeServerEngine(server_settings);
    auto client = MakeClientEngine(client_settings);

    auto shutdown = scope_exit([&server, &client]() noexcept {
        safe_call([&client] { client->Shutdown(); });

        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    client->Connect();
    REQUIRE(WaitForConnected(client, server));

    hstring reported = server->Hashes.ToHashedString("integration_test_updater_hash");

    client->GetConnection()->OutBuf->StartMsg(NetMessage::UnresolvedHash);
    client->GetConnection()->OutBuf->Write<hstring::hash_t>(reported.as_hash());
    client->GetConnection()->OutBuf->EndMsg();

    REQUIRE(WaitForLearnedHash(client, reported.as_hash(), "integration_test_updater_hash"));
    client->Disconnect();

    Updater updater {&client_settings, &GetApp()->MainWindow};
    REQUIRE(WaitForUpdaterResult(updater));
    CHECK(updater.GetResult() == UpdaterResult::ResourcesReady);
    CHECK_FALSE(updater.IsAborted());
}

TEST_CASE("ClientReportsLazyUnresolvedHashAndLearnsWithoutDisconnect")
{
    using namespace TestClientServerIntegration;

    auto port = IntegrationTestPort.fetch_add(1);

    auto server_settings = MakeServerTestSettings(port);
    // Linux debug stack traces for the expected script exception below can outlive the default ping window
    BakerTests::OverrideSetting(server_settings.ClientPingTime, 120000);
    auto client_settings = MakeClientTestSettings(port);

    auto server = MakeServerEngine(server_settings);
    auto client = MakeClientEngine(client_settings);

    auto shutdown = scope_exit([&server, &client]() noexcept {
        safe_call([&client] { client->Shutdown(); });

        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    client->Connect();
    REQUIRE(WaitForConnected(client, server));

    // A server-only runtime hstring that is not read through NetInBuffer, matching lazy property/script resolves
    hstring reported = server->Hashes.ToHashedString("integration_test_lazy_hash");

    auto critter_registrar = client->GetPropertyRegistrar(CritterView::ENTITY_TYPE_NAME);
    REQUIRE(static_cast<bool>(critter_registrar));

    auto critter_props = Properties(critter_registrar);
    auto model_name_prop = critter_registrar->GetPropertyByIndex(CritterView::ModelName_RegIndex);
    REQUIRE(static_cast<bool>(model_name_prop));

    hstring::hash_t unresolved_hash = reported.as_hash();
    critter_props.SetRawData(model_name_prop, {reinterpret_cast<const uint8_t*>(&unresolved_hash), sizeof(unresolved_hash)});

    auto proto = client->GetProtoCritter(client->Hashes.ToHashedString("UnitTestSharedCritter"));
    REQUIRE(static_cast<bool>(proto));

    auto critter_props_ptr = make_nptr(&critter_props);
    auto critter = SafeAlloc::MakeRefCounted<CritterView>(client, ident_t {}, proto, critter_props_ptr);
    auto get_client_func_name = [&client](string_view name) { return client->Hashes.ToHashedString(name); };

    // Trigger the same client unresolved-hash reporter without forcing a slow script exception
    bool failed = false;
    hstring unresolved = client->Hashes.ResolveHash(reported.as_hash(), &failed);
    CHECK(failed);
    CHECK_FALSE(static_cast<bool>(unresolved));

    REQUIRE(WaitForLearnedHash(client, reported.as_hash(), "integration_test_lazy_hash"));
    CHECK(client->IsConnected());
    CHECK(GetServerConnectionCount(server) == 1);

    string model_name;
    REQUIRE(client->CallFunc<string, ptr<CritterView>>(get_client_func_name("ClientServerIntegrationClient::UnitTestReadCritterModelName"), critter, model_name));
    CHECK(model_name == "integration_test_lazy_hash");
}

FO_END_NAMESPACE
