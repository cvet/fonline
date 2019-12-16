#include "Log.h"
#include "Server.h"
#include "Settings.h"
#include "Testing.h"
#include "Timer.h"
#include "Version_Include.h"

void FOServer::ProcessCritter(Critter* cr)
{
    if (cr->CanBeRemoved || cr->IsDestroyed)
        return;
    if (Timer::IsGamePaused())
        return;

    // Moving
    ProcessMove(cr);

    // Idle functions
    Script::RaiseInternalEvent(ServerFunctions.CritterIdle, cr);
    if (!cr->GetMapId())
        Script::RaiseInternalEvent(ServerFunctions.CritterGlobalMapIdle, cr);

    // Internal misc/drugs time events
    // One event per cycle
    if (cr->IsNonEmptyTE_FuncNum())
    {
        CScriptArray* te_next_time = cr->GetTE_NextTime();
        uint next_time = *(uint*)te_next_time->At(0);
        if (!next_time || GameOpt.FullSecond >= next_time)
        {
            CScriptArray* te_func_num = cr->GetTE_FuncNum();
            CScriptArray* te_rate = cr->GetTE_Rate();
            CScriptArray* te_identifier = cr->GetTE_Identifier();
            RUNTIME_ASSERT(te_next_time->GetSize() == te_func_num->GetSize());
            RUNTIME_ASSERT(te_func_num->GetSize() == te_rate->GetSize());
            RUNTIME_ASSERT(te_rate->GetSize() == te_identifier->GetSize());
            hash func_num = *(hash*)te_func_num->At(0);
            uint rate = *(hash*)te_rate->At(0);
            int identifier = *(hash*)te_identifier->At(0);
            te_func_num->Release();
            te_rate->Release();
            te_identifier->Release();

            cr->EraseCrTimeEvent(0);

            uint time = Globals->GetTimeMultiplier() * 1800; // 30 minutes on error
            Script::PrepareScriptFuncContext(func_num, cr->GetName());
            Script::SetArgEntity(cr);
            Script::SetArgUInt(identifier);
            Script::SetArgAddress(&rate);
            if (Script::RunPrepared())
                time = Script::GetReturnedUInt();
            if (time)
                cr->AddCrTimeEvent(func_num, rate, time, identifier);
        }
        te_next_time->Release();
    }

    // Client
    if (cr->IsPlayer())
    {
        // Cast
        Client* cl = (Client*)cr;

        // Talk
        CrMngr.ProcessTalk(cl, false);

        // Ping client
        if (cl->IsToPing())
            cl->PingClient();

        // Kick from game
        if (cl->IsOffline() && cl->IsLife() && !IS_TIMEOUT(cl->GetTimeoutBattle()) && !cl->GetTimeoutRemoveFromGame() &&
            cl->GetOfflineTime() >= GameOpt.MinimumOfflineTime)
        {
            cl->RemoveFromGame();
        }

// Cache look distance
#pragma MESSAGE("Disable look distance caching")
        if (Timer::GameTick() >= cl->CacheValuesNextTick)
        {
            cl->LookCacheValue = cl->GetLookDistance();
            cl->CacheValuesNextTick = Timer::GameTick() + 3000;
        }
    }
}

bool FOServer::Act_Move(Critter* cr, ushort hx, ushort hy, uint move_params)
{
    uint map_id = cr->GetMapId();
    if (!map_id)
        return false;

    // Check run/walk
    bool is_run = FLAG(move_params, MOVE_PARAM_RUN);
    if (is_run && cr->GetIsNoRun())
    {
        // Switch to walk
        move_params ^= MOVE_PARAM_RUN;
        is_run = false;
    }
    if (!is_run && cr->GetIsNoWalk())
        return false;

    // Check
    Map* map = MapMngr.GetMap(map_id);
    if (!map || map_id != cr->GetMapId() || hx >= map->GetWidth() || hy >= map->GetHeight())
        return false;

    // Check passed
    ushort fx = cr->GetHexX();
    ushort fy = cr->GetHexY();
    uchar dir = GetNearDir(fx, fy, hx, hy);
    uint multihex = cr->GetMultihex();

    if (!map->IsMovePassed(hx, hy, dir, multihex))
    {
        if (cr->IsPlayer())
        {
            cr->Send_XY(cr);
            Critter* cr_hex = map->GetHexCritter(hx, hy, false);
            if (cr_hex)
                cr->Send_XY(cr_hex);
        }
        return false;
    }

    // Set last move type
    cr->IsRunning = is_run;

    // Process step
    bool is_dead = cr->IsDead();
    map->UnsetFlagCritter(fx, fy, multihex, is_dead);
    cr->SetHexX(hx);
    cr->SetHexY(hy);
    map->SetFlagCritter(hx, hy, multihex, is_dead);

    // Set dir
    cr->SetDir(dir);

    if (is_run)
    {
        if (cr->GetIsHide())
            cr->SetIsHide(false);
        cr->SetBreakTimeDelta(cr->GetRunTime());
    }
    else
    {
        cr->SetBreakTimeDelta(cr->GetWalkTime());
    }

    cr->SendA_Move(move_params);
    MapMngr.ProcessVisibleCritters(cr);
    MapMngr.ProcessVisibleItems(cr);

    // Triggers
    if (cr->GetMapId() == map->GetId())
        VerifyTrigger(map, cr, fx, fy, hx, hy, dir);

    return true;
}

void FOServer::VerifyTrigger(
    Map* map, Critter* cr, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uchar dir)
{
    if (map->IsHexStaticTrigger(from_hx, from_hy))
    {
        ItemVec triggers;
        map->GetProtoMap()->GetStaticItemTriggers(from_hx, from_hy, triggers);
        for (Item* item : triggers)
        {
            if (item->SceneryScriptBindId)
            {
                Script::PrepareContext(item->SceneryScriptBindId, cr->GetName());
                Script::SetArgEntity(cr);
                Script::SetArgEntity(item);
                Script::SetArgBool(false);
                Script::SetArgUChar(dir);
                Script::RunPreparedSuspend();
            }

            Script::RaiseInternalEvent(ServerFunctions.StaticItemWalk, item, cr, false, dir);
        }
    }

    if (map->IsHexStaticTrigger(to_hx, to_hy))
    {
        ItemVec triggers;
        map->GetProtoMap()->GetStaticItemTriggers(to_hx, to_hy, triggers);
        for (Item* item : triggers)
        {
            if (item->SceneryScriptBindId)
            {
                Script::PrepareContext(item->SceneryScriptBindId, cr->GetName());
                Script::SetArgEntity(cr);
                Script::SetArgEntity(item);
                Script::SetArgBool(true);
                Script::SetArgUChar(dir);
                Script::RunPreparedSuspend();
            }

            Script::RaiseInternalEvent(ServerFunctions.StaticItemWalk, item, cr, true, dir);
        }
    }

    if (map->IsHexTrigger(from_hx, from_hy))
    {
        ItemVec triggers;
        map->GetItemsTrigger(from_hx, from_hy, triggers);
        for (Item* item : triggers)
            Script::RaiseInternalEvent(ServerFunctions.ItemWalk, item, cr, false, dir);
    }

    if (map->IsHexTrigger(to_hx, to_hy))
    {
        ItemVec triggers;
        map->GetItemsTrigger(to_hx, to_hy, triggers);
        for (Item* item : triggers)
            Script::RaiseInternalEvent(ServerFunctions.ItemWalk, item, cr, true, dir);
    }
}

void FOServer::Process_Update(Client* cl)
{
    // Net protocol
    ushort proto_ver = 0;
    cl->Connection->Bin >> proto_ver;
    bool outdated = (proto_ver != (ushort)FO_VERSION);

    // Begin data encrypting
    uint encrypt_key;
    cl->Connection->Bin >> encrypt_key;
    cl->Connection->Bin.SetEncryptKey(encrypt_key + 521);
    cl->Connection->Bout.SetEncryptKey(encrypt_key + 3491);

    // Send update files list
    uint msg_len =
        sizeof(NETMSG_UPDATE_FILES_LIST) + sizeof(msg_len) + sizeof(bool) + sizeof(uint) + (uint)UpdateFilesList.size();

    // With global properties
    PUCharVec* global_vars_data;
    UIntVec* global_vars_data_sizes;
    if (!outdated)
        msg_len += sizeof(ushort) + Globals->Props.StoreData(false, &global_vars_data, &global_vars_data_sizes);

    BOUT_BEGIN(cl);
    cl->Connection->Bout << NETMSG_UPDATE_FILES_LIST;
    cl->Connection->Bout << msg_len;
    cl->Connection->Bout << outdated;
    cl->Connection->Bout << (uint)UpdateFilesList.size();
    if (!UpdateFilesList.empty())
        cl->Connection->Bout.Push(&UpdateFilesList[0], (uint)UpdateFilesList.size());
    if (!outdated)
        NET_WRITE_PROPERTIES(cl->Connection->Bout, global_vars_data, global_vars_data_sizes);
    BOUT_END(cl);
}

void FOServer::Process_UpdateFile(Client* cl)
{
    uint file_index;
    cl->Connection->Bin >> file_index;

    if (file_index >= (uint)UpdateFiles.size())
    {
        WriteLog("Wrong file index {}, client ip '{}'.\n", file_index, cl->GetIpStr());
        cl->Disconnect();
        return;
    }

    cl->UpdateFileIndex = file_index;
    cl->UpdateFilePortion = 0;
    Process_UpdateFileData(cl);
}

void FOServer::Process_UpdateFileData(Client* cl)
{
    if (cl->UpdateFileIndex == -1)
    {
        WriteLog("Wrong update call, client ip '{}'.\n", cl->GetIpStr());
        cl->Disconnect();
        return;
    }

    UpdateFile& update_file = UpdateFiles[cl->UpdateFileIndex];
    uint offset = cl->UpdateFilePortion * FILE_UPDATE_PORTION;

    if (offset + FILE_UPDATE_PORTION < update_file.Size)
        cl->UpdateFilePortion++;
    else
        cl->UpdateFileIndex = -1;

    if (cl->IsSendDisabled() || cl->IsOffline())
        return;

    uchar data[FILE_UPDATE_PORTION];
    uint remaining_size = update_file.Size - offset;
    if (remaining_size >= sizeof(data))
    {
        memcpy(data, &update_file.Data[offset], sizeof(data));
    }
    else
    {
        memcpy(data, &update_file.Data[offset], remaining_size);
        memzero(&data[remaining_size], sizeof(data) - remaining_size);
    }

    BOUT_BEGIN(cl);
    cl->Connection->Bout << NETMSG_UPDATE_FILE_DATA;
    cl->Connection->Bout.Push(data, sizeof(data));
    BOUT_END(cl);
}

void FOServer::Process_CreateClient(Client* cl)
{
    // Read message
    uint msg_len;
    cl->Connection->Bin >> msg_len;

    // Protocol version
    ushort proto_ver = 0;
    cl->Connection->Bin >> proto_ver;

    // Begin data encrypting
    cl->Connection->Bin.SetEncryptKey(1234567890);
    cl->Connection->Bout.SetEncryptKey(1234567890);

    // Check protocol
    if (proto_ver != (ushort)FO_VERSION)
    {
        cl->Send_CustomMessage(NETMSG_WRONG_NET_PROTO);
        cl->Disconnect();
        return;
    }

    // Check for ban by ip
    {
        SCOPE_LOCK(BannedLocker);

        uint ip = cl->GetIp();
        ClientBanned* ban = GetBanByIp(ip);
        if (ban)
        {
            cl->Send_TextMsg(cl, STR_NET_BANNED_IP, SAY_NETMSG, TEXTMSG_GAME);
            // cl->Send_TextMsgLex(cl,STR_NET_BAN_REASON,SAY_NETMSG,TEXTMSG_GAME,ban->GetBanLexems());
            cl->Send_TextMsgLex(
                cl, STR_NET_TIME_LEFT, SAY_NETMSG, TEXTMSG_GAME, _str("$time{}", GetBanTime(*ban)).c_str());
            cl->Disconnect();
            return;
        }
    }

    // Name
    char name[UTF8_BUF_SIZE(MAX_NAME)];
    cl->Connection->Bin.Pop(name, sizeof(name));
    name[sizeof(name) - 1] = 0;
    cl->Name = name;

    // Password hash
    char password[UTF8_BUF_SIZE(MAX_NAME)];
    cl->Connection->Bin.Pop(password, sizeof(password));
    password[sizeof(password) - 1] = 0;

    // Check net
    CHECK_IN_BUFF_ERROR_EXT(cl, cl->Send_TextMsg(cl, STR_NET_DATATRANS_ERR, SAY_NETMSG, TEXTMSG_GAME), return );

    // Check data
    if (!_str(cl->Name).isValidUtf8() || cl->Name.find('*') != string::npos)
    {
        cl->Send_TextMsg(cl, STR_NET_LOGINPASS_WRONG, SAY_NETMSG, TEXTMSG_GAME);
        cl->Disconnect();
        return;
    }

    uint name_len_utf8 = _str(cl->Name).lengthUtf8();
    if (name_len_utf8 < MIN_NAME || name_len_utf8 < GameOpt.MinNameLength || name_len_utf8 > MAX_NAME ||
        name_len_utf8 > GameOpt.MaxNameLength)
    {
        cl->Send_TextMsg(cl, STR_NET_LOGINPASS_WRONG, SAY_NETMSG, TEXTMSG_GAME);
        cl->Disconnect();
        return;
    }

    // Check for exist
    uint id = MAKE_CLIENT_ID(name);
    RUNTIME_ASSERT(IS_CLIENT_ID(id));
    if (!DbStorage->Get("Players", id).empty())
    {
        cl->Send_TextMsg(cl, STR_NET_ACCOUNT_ALREADY, SAY_NETMSG, TEXTMSG_GAME);
        cl->Disconnect();
        return;
    }

    // Check brute force registration
    if (GameOpt.RegistrationTimeout)
    {
        SCOPE_LOCK(RegIpLocker);

        uint ip = cl->GetIp();
        uint reg_tick = GameOpt.RegistrationTimeout * 1000;
        auto it = RegIp.find(ip);
        if (it != RegIp.end())
        {
            uint& last_reg = it->second;
            uint tick = Timer::FastTick();
            if (tick - last_reg < reg_tick)
            {
                cl->Send_TextMsg(cl, STR_NET_REGISTRATION_IP_WAIT, SAY_NETMSG, TEXTMSG_GAME);
                cl->Send_TextMsgLex(cl, STR_NET_TIME_LEFT, SAY_NETMSG, TEXTMSG_GAME,
                    _str("$time{}", (reg_tick - (tick - last_reg)) / 60000 + 1).c_str());
                cl->Disconnect();
                return;
            }
            last_reg = tick;
        }
        else
        {
            RegIp.insert(std::make_pair(ip, Timer::FastTick()));
        }
    }

    uint disallow_msg_num = 0, disallow_str_num = 0;
    string lexems;
    bool allow = Script::RaiseInternalEvent(
        ServerFunctions.PlayerRegistration, cl->GetIp(), &cl->Name, &disallow_msg_num, &disallow_str_num, &lexems);
    if (!allow)
    {
        if (disallow_msg_num < TEXTMSG_COUNT && disallow_str_num)
            cl->Send_TextMsgLex(cl, disallow_str_num, SAY_NETMSG, disallow_msg_num, lexems.c_str());
        else
            cl->Send_TextMsg(cl, STR_NET_LOGIN_SCRIPT_FAIL, SAY_NETMSG, TEXTMSG_GAME);
        cl->Disconnect();
        return;
    }

    // Register
    cl->SetId(id);
    DbStorage->Insert("Players", id, {{"_Proto", string("Player")}});
    cl->RefreshName();
    cl->SetPassword(password);
    cl->SetWorldX(GM_MAXX / 2);
    cl->SetWorldY(GM_MAXY / 2);
    cl->SetCond(COND_LIFE);

    CScriptArray* arr_reg_ip = Script::CreateArray("uint[]");
    uint reg_ip = cl->GetIp();
    arr_reg_ip->InsertLast(&reg_ip);
    cl->SetConnectionIp(arr_reg_ip);
    SAFEREL(arr_reg_ip);
    CScriptArray* arr_reg_port = Script::CreateArray("uint16[]");
    ushort reg_port = cl->GetPort();
    arr_reg_port->InsertLast(&reg_port);
    cl->SetConnectionPort(arr_reg_port);
    SAFEREL(arr_reg_port);

    // Assign base access
    cl->Access = ACCESS_DEFAULT;

    // Notify
    cl->Send_TextMsg(cl, STR_NET_REG_SUCCESS, SAY_NETMSG, TEXTMSG_GAME);

    BOUT_BEGIN(cl);
    cl->Connection->Bout << (uint)NETMSG_REGISTER_SUCCESS;
    BOUT_END(cl);

    cl->Disconnect();

    cl->AddRef();
    EntityMngr.RegisterEntity(cl);
    bool can = MapMngr.CanAddCrToMap(cl, nullptr, 0, 0, 0);
    RUNTIME_ASSERT(can);
    MapMngr.AddCrToMap(cl, nullptr, 0, 0, 0, 0);

    if (!Script::RaiseInternalEvent(ServerFunctions.CritterInit, cl, true))
    {
        // Todo: remove from game
    }
}

void FOServer::Process_LogIn(Client*& cl)
{
    // Net protocol
    ushort proto_ver = 0;
    cl->Connection->Bin >> proto_ver;

    // Begin data encrypting
    cl->Connection->Bin.SetEncryptKey(12345);
    cl->Connection->Bout.SetEncryptKey(12345);

    // Check protocol
    if (proto_ver != (ushort)FO_VERSION)
    {
        cl->Send_CustomMessage(NETMSG_WRONG_NET_PROTO);
        cl->Disconnect();
        return;
    }

    // Login, password hash
    char name[UTF8_BUF_SIZE(MAX_NAME)];
    cl->Connection->Bin.Pop(name, sizeof(name));
    name[sizeof(name) - 1] = 0;
    cl->Name = name;
    char password[UTF8_BUF_SIZE(MAX_NAME)];
    cl->Connection->Bin.Pop(password, sizeof(password));

    // Bin hashes
    uint msg_language;
    cl->Connection->Bin >> msg_language;

    CHECK_IN_BUFF_ERROR_EXT(cl, cl->Send_TextMsg(cl, STR_NET_DATATRANS_ERR, SAY_NETMSG, TEXTMSG_GAME), return );

    // Language
    auto it_l = std::find(LangPacks.begin(), LangPacks.end(), msg_language);
    if (it_l != LangPacks.end())
        cl->LanguageMsg = msg_language;
    else
        cl->LanguageMsg = (*LangPacks.begin()).Name;

    // If only cache checking than disconnect
    if (!name[0])
    {
        cl->Disconnect();
        return;
    }

    // Check for ban by ip
    {
        SCOPE_LOCK(BannedLocker);

        uint ip = cl->GetIp();
        ClientBanned* ban = GetBanByIp(ip);
        if (ban)
        {
            cl->Send_TextMsg(cl, STR_NET_BANNED_IP, SAY_NETMSG, TEXTMSG_GAME);
            if (_str(ban->ClientName).compareIgnoreCaseUtf8(cl->Name))
                cl->Send_TextMsgLex(cl, STR_NET_BAN_REASON, SAY_NETMSG, TEXTMSG_GAME, ban->GetBanLexems().c_str());
            cl->Send_TextMsgLex(
                cl, STR_NET_TIME_LEFT, SAY_NETMSG, TEXTMSG_GAME, _str("$time{}", GetBanTime(*ban)).c_str());
            cl->Disconnect();
            return;
        }
    }

    // Check login/password
    uint name_len_utf8 = _str(cl->Name).lengthUtf8();
    if (name_len_utf8 < MIN_NAME || name_len_utf8 < GameOpt.MinNameLength || name_len_utf8 > MAX_NAME ||
        name_len_utf8 > GameOpt.MaxNameLength)
    {
        cl->Send_TextMsg(cl, STR_NET_WRONG_LOGIN, SAY_NETMSG, TEXTMSG_GAME);
        cl->Disconnect();
        return;
    }

    if (!_str(cl->Name).isValidUtf8() || cl->Name.find('*') != string::npos)
    {
        cl->Send_TextMsg(cl, STR_NET_WRONG_DATA, SAY_NETMSG, TEXTMSG_GAME);
        cl->Disconnect();
        return;
    }

    // Check password
    uint id = MAKE_CLIENT_ID(cl->Name);
    DataBase::Document doc = DbStorage->Get("Players", id);
    if (!doc.count("Password") || doc["Password"].which() != DataBase::StringValue ||
        !Str::Compare(password, doc["Password"].get<string>().c_str()))
    {
        cl->Send_TextMsg(cl, STR_NET_LOGINPASS_WRONG, SAY_NETMSG, TEXTMSG_GAME);
        cl->Disconnect();
        return;
    }

    // Check for ban by name
    {
        SCOPE_LOCK(BannedLocker);

        ClientBanned* ban = GetBanByName(cl->Name.c_str());
        if (ban)
        {
            cl->Send_TextMsg(cl, STR_NET_BANNED, SAY_NETMSG, TEXTMSG_GAME);
            cl->Send_TextMsgLex(cl, STR_NET_BAN_REASON, SAY_NETMSG, TEXTMSG_GAME, ban->GetBanLexems().c_str());
            cl->Send_TextMsgLex(
                cl, STR_NET_TIME_LEFT, SAY_NETMSG, TEXTMSG_GAME, _str("$time{}", GetBanTime(*ban)).c_str());
            cl->Disconnect();
            return;
        }
    }

    // Request script
    uint disallow_msg_num = 0, disallow_str_num = 0;
    string lexems;
    bool allow = Script::RaiseInternalEvent(
        ServerFunctions.PlayerLogin, cl->GetIp(), &cl->Name, id, &disallow_msg_num, &disallow_str_num, &lexems);
    if (!allow)
    {
        if (disallow_msg_num < TEXTMSG_COUNT && disallow_str_num)
            cl->Send_TextMsgLex(cl, disallow_str_num, SAY_NETMSG, disallow_msg_num, lexems.c_str());
        else
            cl->Send_TextMsg(cl, STR_NET_LOGIN_SCRIPT_FAIL, SAY_NETMSG, TEXTMSG_GAME);
        cl->Disconnect();
        return;
    }

    // Copy data
    cl->RefreshName();

    // Find in players in game
    Client* cl_old = CrMngr.GetPlayer(id);
    RUNTIME_ASSERT(cl != cl_old);

    // Avatar in game
    if (cl_old)
    {
        ConnectedClientsLocker.Lock();

        // Disconnect current connection
        if (!cl_old->Connection->IsDisconnected)
            cl_old->Disconnect();

        // Swap data
        std::swap(cl_old->Connection, cl->Connection);
        cl_old->GameState = STATE_CONNECTED;
        cl->GameState = STATE_NONE;
        cl_old->LastActivityTime = 0;
        UNSETFLAG(cl_old->Flags, FCRIT_DISCONNECT);
        SETFLAG(cl->Flags, FCRIT_DISCONNECT);
        cl->IsDestroyed = true;
        cl_old->IsDestroyed = false;
        Script::RemoveEventsEntity(cl);

        // Change in list
        cl_old->AddRef();
        auto it = std::find(ConnectedClients.begin(), ConnectedClients.end(), cl);
        RUNTIME_ASSERT(it != ConnectedClients.end());
        *it = cl_old;
        cl->Release();
        cl = cl_old;

        ConnectedClientsLocker.Unlock();

        cl->SendA_Action(ACTION_CONNECT, 0, nullptr);
    }
    // Avatar not in game
    else
    {
        cl->SetId(id);

        // Data
        if (!cl->Props.LoadFromDbDocument(doc))
        {
            WriteLog("Player '{}' data truncated.\n", cl->Name);
            cl->Send_TextMsg(cl, STR_NET_BD_ERROR, SAY_NETMSG, TEXTMSG_GAME);
            cl->Disconnect();
            return;
        }

        // Add to collection
        cl->AddRef();
        EntityMngr.RegisterEntity(cl);

        // Disable network data sending, because we resend all data later
        cl->DisableSend++;

        // Find items
        ItemMngr.SetCritterItems(cl);

        // Initially add on global map
        cl->SetMapId(0);
        uint ref_gm_leader_id = cl->GetRefGlobalMapLeaderId();
        uint ref_gm_trip_id = cl->GetRefGlobalMapTripId();
        bool can = MapMngr.CanAddCrToMap(cl, nullptr, 0, 0, 0);
        RUNTIME_ASSERT(can);
        MapMngr.AddCrToMap(cl, nullptr, 0, 0, 0, 0);
        cl->SetRefGlobalMapLeaderId(ref_gm_leader_id);
        cl->SetRefGlobalMapLeaderId(ref_gm_trip_id);

        // Initial scripts
        if (!Script::RaiseInternalEvent(ServerFunctions.CritterInit, cl, false))
        {
            // Todo: remove from game
        }
        cl->SetScript(nullptr, false);

        // Restore data sending
        cl->DisableSend--;
    }

    // Connection info
    uint ip = cl->GetIp();
    ushort port = cl->GetPort();
    CScriptArray* conn_ip = cl->GetConnectionIp();
    CScriptArray* conn_port = cl->GetConnectionPort();
    RUNTIME_ASSERT(conn_ip->GetSize() == conn_port->GetSize());
    bool ip_found = false;
    for (uint i = 0, j = conn_ip->GetSize(); i < j; i++)
    {
        if (*(uint*)conn_ip->At(i) == ip)
        {
            if (i < j - 1)
            {
                conn_ip->RemoveAt(i);
                conn_ip->InsertLast(&ip);
                cl->SetConnectionIp(conn_ip);
                conn_port->RemoveAt(i);
                conn_port->InsertLast(&port);
                cl->SetConnectionPort(conn_port);
            }
            else if (*(ushort*)conn_port->At(j - 1) != port)
            {
                conn_port->SetValue(i, &port);
                cl->SetConnectionPort(conn_port);
            }
            ip_found = true;
            break;
        }
    }
    if (!ip_found)
    {
        conn_ip->InsertLast(&ip);
        cl->SetConnectionIp(conn_ip);
        conn_port->InsertLast(&port);
        cl->SetConnectionPort(conn_port);
    }
    SAFEREL(conn_ip);
    SAFEREL(conn_port);

    // Login ok
    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(uint) * 2;
    uint bin_seed = Random(100000, 2000000000);
    uint bout_seed = Random(100000, 2000000000);
    PUCharVec* global_vars_data;
    UIntVec* global_vars_data_sizes;
    uint whole_data_size = Globals->Props.StoreData(false, &global_vars_data, &global_vars_data_sizes);
    msg_len += sizeof(ushort) + whole_data_size;
    BOUT_BEGIN(cl);
    cl->Connection->Bout << NETMSG_LOGIN_SUCCESS;
    cl->Connection->Bout << msg_len;
    cl->Connection->Bout << bin_seed;
    cl->Connection->Bout << bout_seed;
    NET_WRITE_PROPERTIES(cl->Connection->Bout, global_vars_data, global_vars_data_sizes);
    BOUT_END(cl);

    cl->Connection->Bin.SetEncryptKey(bin_seed);
    cl->Connection->Bout.SetEncryptKey(bout_seed);

    cl->Send_LoadMap(nullptr, MapMngr);
}

void FOServer::Process_ParseToGame(Client* cl)
{
    if (!cl->GetId() || !CrMngr.GetPlayer(cl->GetId()))
        return;
    cl->SetBreakTime(GameOpt.Breaktime);

    cl->GameState = STATE_PLAYING;
    cl->PingOk(PING_CLIENT_LIFE_TIME * 5);

    if (cl->ViewMapId)
    {
        Map* map = MapMngr.GetMap(cl->ViewMapId);
        cl->ViewMapId = 0;
        if (map)
        {
            MapMngr.ViewMap(cl, map, cl->ViewMapLook, cl->ViewMapHx, cl->ViewMapHy, cl->ViewMapDir);
            cl->Send_ViewMap();
            return;
        }
    }

    // Parse
    Map* map = MapMngr.GetMap(cl->GetMapId());
    cl->Send_GameInfo(map);

    // Parse to global
    if (!cl->GetMapId())
    {
        RUNTIME_ASSERT(cl->GlobalMapGroup);

        cl->Send_GlobalInfo(GM_INFO_ALL, MapMngr);
        for (auto it = cl->GlobalMapGroup->begin(), end = cl->GlobalMapGroup->end(); it != end; ++it)
        {
            Critter* cr = *it;
            if (cr != cl)
                cr->Send_CustomCommand(cl, OTHER_FLAGS, cl->Flags);
        }
        cl->Send_AllAutomapsInfo(MapMngr);
        if (cl->Talk.TalkType != TALK_NONE)
        {
            CrMngr.ProcessTalk(cl, true);
            cl->Send_Talk();
        }
    }
    else
    {
        if (!map)
        {
            WriteLog("Map not found, client '{}'.\n", cl->GetName());
            cl->Disconnect();
            return;
        }

        // Parse to local
        SETFLAG(cl->Flags, FCRIT_CHOSEN);
        cl->Send_AddCritter(cl);
        UNSETFLAG(cl->Flags, FCRIT_CHOSEN);

        // Send all data
        cl->Send_AddAllItems();
        cl->Send_AllAutomapsInfo(MapMngr);

        // Send current critters
        CritterVec critters = cl->VisCrSelf;
        for (auto it = critters.begin(), end = critters.end(); it != end; ++it)
            cl->Send_AddCritter(*it);

        // Send current items on map
        cl->VisItemLocker.Lock();
        UIntSet items = cl->VisItem;
        cl->VisItemLocker.Unlock();
        for (auto it = items.begin(), end = items.end(); it != end; ++it)
        {
            Item* item = ItemMngr.GetItem(*it);
            if (item)
                cl->Send_AddItemOnMap(item);
        }

        // Check active talk
        if (cl->Talk.TalkType != TALK_NONE)
        {
            CrMngr.ProcessTalk(cl, true);
            cl->Send_Talk();
        }
    }

    // Notify about end of parsing
    cl->Send_CustomMessage(NETMSG_END_PARSE_TO_GAME);
}

void FOServer::Process_GiveMap(Client* cl)
{
    bool automap;
    hash map_pid;
    uint loc_id;
    hash hash_tiles;
    hash hash_scen;

    cl->Connection->Bin >> automap;
    cl->Connection->Bin >> map_pid;
    cl->Connection->Bin >> loc_id;
    cl->Connection->Bin >> hash_tiles;
    cl->Connection->Bin >> hash_scen;
    CHECK_IN_BUFF_ERROR(cl);

    uint tick = Timer::FastTick();
    if (tick - cl->LastSendedMapTick < GameOpt.Breaktime * 3)
        cl->SetBreakTime(GameOpt.Breaktime * 3);
    else
        cl->SetBreakTime(GameOpt.Breaktime);
    cl->LastSendedMapTick = tick;

    ProtoMap* pmap = ProtoMngr.GetProtoMap(map_pid);
    if (!pmap)
    {
        WriteLog("Map prototype not found, client '{}'.\n", cl->GetName());
        cl->Disconnect();
        return;
    }

    if (automap)
    {
        if (!MapMngr.CheckKnownLocById(cl, loc_id))
        {
            WriteLog("Request for loading unknown automap, client '{}'.\n", cl->GetName());
            return;
        }

        Location* loc = MapMngr.GetLocation(loc_id);
        if (!loc)
        {
            WriteLog("Request for loading incorrect automap, client '{}'.\n", cl->GetName());
            return;
        }

        bool found = false;
        CScriptArray* automaps = (loc->IsNonEmptyAutomaps() ? loc->GetAutomaps() : nullptr);
        if (automaps)
        {
            for (uint i = 0, j = automaps->GetSize(); i < j && !found; i++)
                if (*(hash*)automaps->At(i) == map_pid)
                    found = true;
        }
        if (!found)
        {
            WriteLog("Request for loading incorrect automap, client '{}'.\n", cl->GetName());
            return;
        }
    }
    else
    {
        Map* map = MapMngr.GetMap(cl->GetMapId());
        if ((!map || map_pid != map->GetProtoId()) && map_pid != cl->ViewMapPid)
        {
            WriteLog("Request for loading incorrect map, client '{}'.\n", cl->GetName());
            return;
        }
    }

    Send_MapData(cl, pmap, pmap->HashTiles != hash_tiles, pmap->HashScen != hash_scen);

    if (!automap)
    {
        Map* map = nullptr;
        if (cl->ViewMapId)
            map = MapMngr.GetMap(cl->ViewMapId);
        cl->Send_LoadMap(map, MapMngr);
    }
}

void FOServer::Send_MapData(Client* cl, ProtoMap* pmap, bool send_tiles, bool send_scenery)
{
    uint msg = NETMSG_MAP;
    hash map_pid = pmap->ProtoId;
    ushort maxhx = pmap->GetWidth();
    ushort maxhy = pmap->GetHeight();
    uint msg_len = sizeof(msg) + sizeof(msg_len) + sizeof(map_pid) + sizeof(maxhx) + sizeof(maxhy) + sizeof(bool) * 2;

    if (send_tiles)
        msg_len += sizeof(uint) + (uint)pmap->Tiles.size() * sizeof(ProtoMap::Tile);
    if (send_scenery)
        msg_len += sizeof(uint) + (uint)pmap->SceneryData.size();

    // Header
    BOUT_BEGIN(cl);
    cl->Connection->Bout << msg;
    cl->Connection->Bout << msg_len;
    cl->Connection->Bout << map_pid;
    cl->Connection->Bout << maxhx;
    cl->Connection->Bout << maxhy;
    cl->Connection->Bout << send_tiles;
    cl->Connection->Bout << send_scenery;
    if (send_tiles)
    {
        cl->Connection->Bout << (uint)(pmap->Tiles.size() * sizeof(ProtoMap::Tile));
        if (pmap->Tiles.size())
            cl->Connection->Bout.Push(&pmap->Tiles[0], (uint)pmap->Tiles.size() * sizeof(ProtoMap::Tile));
    }
    if (send_scenery)
    {
        cl->Connection->Bout << (uint)pmap->SceneryData.size();
        if (pmap->SceneryData.size())
            cl->Connection->Bout.Push(&pmap->SceneryData[0], (uint)pmap->SceneryData.size());
    }
    BOUT_END(cl);
}

void FOServer::Process_Move(Client* cl)
{
    uint move_params;
    ushort hx;
    ushort hy;

    cl->Connection->Bin >> move_params;
    cl->Connection->Bin >> hx;
    cl->Connection->Bin >> hy;
    CHECK_IN_BUFF_ERROR(cl);

    if (!cl->GetMapId())
        return;

    // The player informs that has stopped
    if (FLAG(move_params, MOVE_PARAM_STEP_DISALLOW))
    {
        // cl->Send_XY(cl);
        cl->SendA_XY();
        return;
    }

    // Check running availability
    bool is_run = FLAG(move_params, MOVE_PARAM_RUN);
    if (is_run)
    {
        if (cl->GetIsNoRun() || (GameOpt.RunOnCombat ? false : IS_TIMEOUT(cl->GetTimeoutBattle())) ||
            (GameOpt.RunOnTransfer ? false : IS_TIMEOUT(cl->GetTimeoutTransfer())))
        {
            // Switch to walk
            move_params ^= MOVE_PARAM_RUN;
            is_run = false;
        }
    }

    // Check walking availability
    if (!is_run)
    {
        if (cl->GetIsNoWalk())
        {
            cl->Send_XY(cl);
            return;
        }
    }

    // Check dist
    if (!CheckDist(cl->GetHexX(), cl->GetHexY(), hx, hy, 1))
    {
        cl->Send_XY(cl);
        return;
    }

    // Try move
    Act_Move(cl, hx, hy, move_params);
}

void FOServer::Process_Dir(Client* cl)
{
    uchar dir;
    cl->Connection->Bin >> dir;
    CHECK_IN_BUFF_ERROR(cl);

    if (!cl->GetMapId() || dir >= DIRS_COUNT || cl->GetDir() == dir || cl->IsTalking())
    {
        if (cl->GetDir() != dir)
            cl->Send_Dir(cl);
        return;
    }

    cl->SetDir(dir);
    cl->SendA_Dir();
}

void FOServer::Process_Ping(Client* cl)
{
    uchar ping;

    cl->Connection->Bin >> ping;
    CHECK_IN_BUFF_ERROR(cl);

    if (ping == PING_CLIENT)
    {
        cl->PingOk(PING_CLIENT_LIFE_TIME);
        return;
    }
    else if (ping == PING_PING || ping == PING_WAIT)
    {
        // Valid pings
    }
    else
    {
        WriteLog("Unknown ping {}, client '{}'.\n", ping, cl->GetName());
        return;
    }

    BOUT_BEGIN(cl);
    cl->Connection->Bout << NETMSG_PING;
    cl->Connection->Bout << ping;
    BOUT_END(cl);
}

void FOServer::Process_Property(Client* cl, uint data_size)
{
    uint msg_len = 0;
    NetProperty::Type type = NetProperty::None;
    uint cr_id = 0;
    uint item_id = 0;
    ushort property_index = 0;

    if (data_size == 0)
        cl->Connection->Bin >> msg_len;

    char type_ = 0;
    cl->Connection->Bin >> type_;
    type = (NetProperty::Type)type_;

    uint additional_args = 0;
    switch (type)
    {
    case NetProperty::CritterItem:
        additional_args = 2;
        cl->Connection->Bin >> cr_id;
        cl->Connection->Bin >> item_id;
        break;
    case NetProperty::Critter:
        additional_args = 1;
        cl->Connection->Bin >> cr_id;
        break;
    case NetProperty::MapItem:
        additional_args = 1;
        cl->Connection->Bin >> item_id;
        break;
    case NetProperty::ChosenItem:
        additional_args = 1;
        cl->Connection->Bin >> item_id;
        break;
    default:
        break;
    }

    cl->Connection->Bin >> property_index;

    CHECK_IN_BUFF_ERROR(cl);

    UCharVec data;
    if (data_size != 0)
    {
        data.resize(data_size);
        cl->Connection->Bin.Pop(&data[0], data_size);
    }
    else
    {
        uint len =
            msg_len - sizeof(uint) - sizeof(msg_len) - sizeof(char) - additional_args * sizeof(uint) - sizeof(ushort);
#pragma MESSAGE("Control max size explicitly, add option to property registration")
        if (len > 0xFFFF) // For now 64Kb for all
            return;
        data.resize(len);
        cl->Connection->Bin.Pop(&data[0], len);
    }

    CHECK_IN_BUFF_ERROR(cl);

    bool is_public = false;
    Property* prop = nullptr;
    Entity* entity = nullptr;
    switch (type)
    {
    case NetProperty::Global:
        is_public = true;
        prop = GlobalVars::PropertiesRegistrator->Get(property_index);
        if (prop)
            entity = Globals;
        break;
    case NetProperty::Critter:
        is_public = true;
        prop = Critter::PropertiesRegistrator->Get(property_index);
        if (prop)
            entity = CrMngr.GetCritter(cr_id);
        break;
    case NetProperty::Chosen:
        prop = Critter::PropertiesRegistrator->Get(property_index);
        if (prop)
            entity = cl;
        break;
    case NetProperty::MapItem:
        is_public = true;
        prop = Item::PropertiesRegistrator->Get(property_index);
        if (prop)
            entity = ItemMngr.GetItem(item_id);
        break;
    case NetProperty::CritterItem:
        is_public = true;
        prop = Item::PropertiesRegistrator->Get(property_index);
        if (prop)
        {
            Critter* cr = CrMngr.GetCritter(cr_id);
            if (cr)
                entity = cr->GetItem(item_id, true);
        }
        break;
    case NetProperty::ChosenItem:
        prop = Item::PropertiesRegistrator->Get(property_index);
        if (prop)
            entity = cl->GetItem(item_id, true);
        break;
    case NetProperty::Map:
        is_public = true;
        prop = Map::PropertiesRegistrator->Get(property_index);
        if (prop)
            entity = MapMngr.GetMap(cl->GetMapId());
        break;
    case NetProperty::Location:
        is_public = true;
        prop = Location::PropertiesRegistrator->Get(property_index);
        if (prop)
        {
            Map* map = MapMngr.GetMap(cl->GetMapId());
            if (map)
                entity = map->GetLocation();
        }
        break;
    default:
        break;
    }
    if (!prop || !entity)
        return;

    Property::AccessType access = prop->GetAccess();
    if (is_public && !(access & Property::PublicMask))
        return;
    if (!is_public && !(access & (Property::ProtectedMask | Property::PublicMask)))
        return;
    if (!(access & Property::ModifiableMask))
        return;
    if (is_public && access != Property::PublicFullModifiable)
        return;
    if (prop->IsPOD() && data_size != prop->GetBaseSize())
        return;
    if (!prop->IsPOD() && data_size != 0)
        return;

#pragma MESSAGE("Disable send changing field by client to this client")
    prop->SetData(entity, !data.empty() ? &data[0] : nullptr, (uint)data.size());
}

void FOServer::OnSendGlobalValue(Entity* entity, Property* prop)
{
    if ((prop->GetAccess() & Property::PublicMask) != 0)
    {
        ClientVec players;
        Self->CrMngr.GetClients(players);
        for (auto it = players.begin(); it != players.end(); ++it)
        {
            Client* cl = *it;
            cl->Send_Property(NetProperty::Global, prop, Globals);
        }
    }
}

void FOServer::OnSendCritterValue(Entity* entity, Property* prop)
{
    Critter* cr = (Critter*)entity;

    bool is_public = (prop->GetAccess() & Property::PublicMask) != 0;
    bool is_protected = (prop->GetAccess() & Property::ProtectedMask) != 0;
    if (is_public || is_protected)
        cr->Send_Property(NetProperty::Chosen, prop, cr);
    if (is_public)
        cr->SendA_Property(NetProperty::Critter, prop, cr);
}

void FOServer::OnSendMapValue(Entity* entity, Property* prop)
{
    if ((prop->GetAccess() & Property::PublicMask) != 0)
    {
        Map* map = (Map*)entity;
        map->SendProperty(NetProperty::Map, prop, map);
    }
}

void FOServer::OnSendLocationValue(Entity* entity, Property* prop)
{
    if ((prop->GetAccess() & Property::PublicMask) != 0)
    {
        Location* loc = (Location*)entity;
        for (Map* map : loc->GetMaps())
            map->SendProperty(NetProperty::Location, prop, loc);
    }
}
