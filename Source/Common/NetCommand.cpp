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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "NetCommand.h"
#include "Entity.h"
#include "GenericUtils.h"
#include "StringUtils.h"

struct CmdDef
{
    char Name[20];
    uint8 Id;
};

static constexpr CmdDef CMD_LIST[] = {
    {"exit", CMD_EXIT},
    {"myinfo", CMD_MYINFO},
    {"gameinfo", CMD_GAMEINFO},
    {"id", CMD_CRITID},
    {"move", CMD_MOVECRIT},
    {"disconnect", CMD_DISCONCRIT},
    {"toglobal", CMD_TOGLOBAL},
    {"prop", CMD_PROPERTY},
    {"getaccess", CMD_GETACCESS},
    {"additem", CMD_ADDITEM},
    {"additemself", CMD_ADDITEM_SELF},
    {"ais", CMD_ADDITEM_SELF},
    {"addnpc", CMD_ADDNPC},
    {"addloc", CMD_ADDLOCATION},
    {"runscript", CMD_RUNSCRIPT},
    {"run", CMD_RUNSCRIPT},
    {"regenmap", CMD_REGENMAP},
    {"settime", CMD_SETTIME},
    {"log", CMD_LOG},
};

auto PackNetCommand(string_view str, NetOutBuffer* pbuf, const LogCallback& logcb, HashResolver& hash_resolver) -> bool
{
    STACK_TRACE_ENTRY();

    string args = strex(str).trim();
    auto cmd_str = args;
    auto space = cmd_str.find(' ');
    if (space != string::npos) {
        cmd_str = args.substr(0, space);
        args.erase(0, cmd_str.length());
    }
    istringstream args_str(args);

    uint8 cmd = 0;
    for (const auto& cur_cmd : CMD_LIST) {
        if (strex(cmd_str).compareIgnoreCase(cur_cmd.Name)) {
            cmd = cur_cmd.Id;
        }
    }
    if (cmd == 0u) {
        return false;
    }

    auto msg = NETMSG_SEND_COMMAND;

    RUNTIME_ASSERT(pbuf);
    auto& buf = *pbuf;

    switch (cmd) {
    case CMD_EXIT: {
        buf.StartMsg(msg);
        buf.Write(cmd);
        buf.EndMsg();
    } break;
    case CMD_MYINFO: {
        buf.StartMsg(msg);
        buf.Write(cmd);
        buf.EndMsg();
    } break;
    case CMD_GAMEINFO: {
        auto type = 0;
        if (!(args_str >> type)) {
            logcb("Invalid arguments. Example: gameinfo type");
            break;
        }

        buf.StartMsg(msg);
        buf.Write(cmd);
        buf.Write(type);
        buf.EndMsg();
    } break;
    case CMD_CRITID: {
        string cr_name;
        if (!(args_str >> cr_name)) {
            logcb("Invalid arguments. Example: id name");
            break;
        }

        buf.StartMsg(msg);
        buf.Write(cmd);
        buf.Write(cr_name);
        buf.EndMsg();
    } break;
    case CMD_MOVECRIT: {
        uint cr_id = 0;
        uint16 hex_x = 0;
        uint16 hex_y = 0;
        if (!(args_str >> cr_id >> hex_x >> hex_y)) {
            logcb("Invalid arguments. Example: move cr_id hx hy");
            break;
        }

        buf.StartMsg(msg);
        buf.Write(cmd);
        buf.Write(cr_id);
        buf.Write(mpos {hex_x, hex_y});
        buf.EndMsg();
    } break;
    case CMD_DISCONCRIT: {
        uint cr_id = 0;
        if (!(args_str >> cr_id)) {
            logcb("Invalid arguments. Example: disconnect cr_id");
            break;
        }

        buf.StartMsg(msg);
        buf.Write(cmd);
        buf.Write(cr_id);
        buf.EndMsg();
    } break;
    case CMD_TOGLOBAL: {
        buf.StartMsg(msg);
        buf.Write(cmd);
        buf.EndMsg();
    } break;
    case CMD_PROPERTY: {
        uint cr_id = 0;
        string property_name;
        auto property_value = 0;
        if (!(args_str >> cr_id >> property_name >> property_value)) {
            logcb("Invalid arguments. Example: prop cr_id prop_name value");
            break;
        }

        buf.StartMsg(msg);
        buf.Write(cmd);
        buf.Write(cr_id);
        buf.Write(property_name);
        buf.Write(property_value);
        buf.EndMsg();
    } break;
    case CMD_GETACCESS: {
        string name_access;
        string pasw_access;
        if (!(args_str >> name_access >> pasw_access)) {
            logcb("Invalid arguments. Example: getaccess name password");
            break;
        }
        name_access = strex(name_access).replace('*', ' ');
        pasw_access = strex(pasw_access).replace('*', ' ');

        buf.StartMsg(msg);
        buf.Write(cmd);
        buf.Write(name_access);
        buf.Write(pasw_access);
        buf.EndMsg();
    } break;
    case CMD_ADDITEM: {
        uint16 hex_x = 0;
        uint16 hex_y = 0;
        string proto_name;
        uint count = 0;
        if (!(args_str >> hex_x >> hex_y >> proto_name >> count)) {
            logcb("Invalid arguments. Example: additem hx hy name count");
            break;
        }
        auto pid = hash_resolver.ToHashedString(proto_name);

        buf.StartMsg(msg);
        buf.Write(cmd);
        buf.Write(mpos {hex_x, hex_y});
        buf.Write(pid);
        buf.Write(count);
        buf.EndMsg();
    } break;
    case CMD_ADDITEM_SELF: {
        string proto_name;
        uint count = 0;
        if (!(args_str >> proto_name >> count)) {
            logcb("Invalid arguments. Example: additemself name count");
            break;
        }

        const auto pid = hash_resolver.ToHashedString(proto_name);

        buf.StartMsg(msg);
        buf.Write(cmd);
        buf.Write(pid);
        buf.Write(count);
        buf.EndMsg();
    } break;
    case CMD_ADDNPC: {
        uint16 hex_x = 0;
        uint16 hex_y = 0;
        uint8 dir = 0;
        string proto_name;
        if (!(args_str >> hex_x >> hex_y >> dir >> proto_name)) {
            logcb("Invalid arguments. Example: addnpc hx hy dir name");
            break;
        }

        const auto pid = hash_resolver.ToHashedString(proto_name);

        buf.StartMsg(msg);
        buf.Write(cmd);
        buf.Write(mpos {hex_x, hex_y});
        buf.Write(dir);
        buf.Write(pid);
        buf.EndMsg();
    } break;
    case CMD_ADDLOCATION: {
        string proto_name;
        if (!(args_str >> proto_name)) {
            logcb("Invalid arguments. Example: addloc name");
            break;
        }

        const auto pid = hash_resolver.ToHashedString(proto_name);

        buf.StartMsg(msg);
        buf.Write(cmd);
        buf.Write(pid);
        buf.EndMsg();
    } break;
    case CMD_RUNSCRIPT: {
        string func_name;
        if (!(args_str >> func_name)) {
            logcb("No function name provided. Example: runscript module::func param0 param1 param2");
            break;
        }

        string param0;
        string param1;
        string param2;
        args_str >> param0;
        args_str >> param1;
        args_str >> param2;

        buf.StartMsg(msg);
        buf.Write(cmd);
        buf.Write(func_name);
        buf.Write(param0);
        buf.Write(param1);
        buf.Write(param2);
        buf.EndMsg();
    } break;
    case CMD_REGENMAP: {
        buf.StartMsg(msg);
        buf.Write(cmd);
        buf.EndMsg();
    } break;
    case CMD_SETTIME: {
        auto multiplier = 0;
        auto year = 0;
        auto month = 0;
        auto day = 0;
        auto hour = 0;
        auto minute = 0;
        auto second = 0;
        if (!(args_str >> multiplier >> year >> month >> day >> hour >> minute >> second)) {
            logcb("Invalid arguments. Example: settime tmul year month day hour minute second");
            break;
        }

        buf.StartMsg(msg);
        buf.Write(cmd);
        buf.Write(multiplier);
        buf.Write(year);
        buf.Write(month);
        buf.Write(day);
        buf.Write(hour);
        buf.Write(minute);
        buf.Write(second);
        buf.EndMsg();
    } break;
    case CMD_LOG: {
        string flags;
        if (!(args_str >> flags) || flags.length() > 2) {
            logcb("Invalid arguments. Example: log flag. Valid flags: '+' attach, '-' detach, '--' detach all");
            break;
        }

        buf.StartMsg(msg);
        buf.Write(cmd);
        buf.Write(flags);
        buf.EndMsg();
    } break;
    default:
        return false;
    }

    return true;
}
