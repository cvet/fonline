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

#include "NetCommand.h"
#include "GenericUtils.h"
#include "StringUtils.h"

struct CmdDef
{
    char Name[20];
    uchar Id;
};

static const CmdDef CMD_LIST[] = {
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
    {"ban", CMD_BAN},
    {"log", CMD_LOG},
};

auto PackNetCommand(string_view str, NetBuffer* pbuf, const LogCallback& logcb, string_view name, NameResolver& name_resolver) -> bool
{
    string args = _str(str).trim();
    auto cmd_str = args;
    auto space = cmd_str.find(' ');
    if (space != string::npos) {
        cmd_str = args.substr(0, space);
        args.erase(0, cmd_str.length());
    }
    istringstream args_str(args);

    uchar cmd = 0;
    for (const auto& cur_cmd : CMD_LIST) {
        if (_str(cmd_str).compareIgnoreCase(cur_cmd.Name)) {
            cmd = cur_cmd.Id;
        }
    }
    if (cmd == 0u) {
        return false;
    }

    auto msg = NETMSG_SEND_COMMAND;
    uint msg_len = sizeof(msg) + sizeof(msg_len) + sizeof(cmd);

    RUNTIME_ASSERT(pbuf);
    auto& buf = *pbuf;

    switch (cmd) {
    case CMD_EXIT: {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    } break;
    case CMD_MYINFO: {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    } break;
    case CMD_GAMEINFO: {
        auto type = 0;
        if (!(args_str >> type)) {
            logcb("Invalid arguments. Example: gameinfo type.");
            break;
        }
        msg_len += sizeof(type);

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << type;
    } break;
    case CMD_CRITID: {
        string cr_name;
        if (!(args_str >> cr_name)) {
            logcb("Invalid arguments. Example: id name.");
            break;
        }
        msg_len += NetBuffer::STRING_LEN_SIZE;

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << cr_name;
    } break;
    case CMD_MOVECRIT: {
        uint crid = 0;
        ushort hex_x = 0;
        ushort hex_y = 0;
        if (!(args_str >> crid >> hex_x >> hex_y)) {
            logcb("Invalid arguments. Example: move crid hx hy.");
            break;
        }
        msg_len += sizeof(crid) + sizeof(hex_x) + sizeof(hex_y);

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << crid;
        buf << hex_x;
        buf << hex_y;
    } break;
    case CMD_DISCONCRIT: {
        uint crid = 0;
        if (!(args_str >> crid)) {
            logcb("Invalid arguments. Example: disconnect crid.");
            break;
        }
        msg_len += sizeof(crid);

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << crid;
    } break;
    case CMD_TOGLOBAL: {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    } break;
    case CMD_PROPERTY: {
        uint crid = 0;
        string property_name;
        auto property_value = 0;
        if (!(args_str >> crid >> property_name >> property_value)) {
            logcb("Invalid arguments. Example: prop crid prop_name value.");
            break;
        }
        msg_len += sizeof(uint) + NetBuffer::STRING_LEN_SIZE + static_cast<uint>(property_name.length()) + sizeof(int);

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << crid;
        buf << property_name;
        buf << property_value;
    } break;
    case CMD_GETACCESS: {
        string name_access;
        string pasw_access;
        if (!(args_str >> name_access >> pasw_access)) {
            logcb("Invalid arguments. Example: getaccess name password.");
            break;
        }
        name_access = _str(name_access).replace('*', ' ');
        pasw_access = _str(pasw_access).replace('*', ' ');
        msg_len += NetBuffer::STRING_LEN_SIZE * 2 + static_cast<uint>(name_access.length() + pasw_access.length());
        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << name_access;
        buf << pasw_access;
    } break;
    case CMD_ADDITEM: {
        ushort hex_x = 0;
        ushort hex_y = 0;
        string proto_name;
        uint count = 0;
        if (!(args_str >> hex_x >> hex_y >> proto_name >> count)) {
            logcb("Invalid arguments. Example: additem hx hy name count.");
            break;
        }
        auto pid = name_resolver.ToHashedString(proto_name);
        msg_len += sizeof(hex_x) + sizeof(hex_y) + sizeof(pid) + sizeof(count);

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << hex_x;
        buf << hex_y;
        buf << pid;
        buf << count;
    } break;
    case CMD_ADDITEM_SELF: {
        string proto_name;
        uint count = 0;
        if (!(args_str >> proto_name >> count)) {
            logcb("Invalid arguments. Example: additemself name count.");
            break;
        }
        auto pid = name_resolver.ToHashedString(proto_name);
        msg_len += sizeof(pid) + sizeof(count);

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << pid;
        buf << count;
    } break;
    case CMD_ADDNPC: {
        ushort hex_x = 0;
        ushort hex_y = 0;
        uchar dir = 0;
        string proto_name;
        if (!(args_str >> hex_x >> hex_y >> dir >> proto_name)) {
            logcb("Invalid arguments. Example: addnpc hx hy dir name.");
            break;
        }
        auto pid = name_resolver.ToHashedString(proto_name);
        msg_len += sizeof(hex_x) + sizeof(hex_y) + sizeof(dir) + sizeof(pid);

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << hex_x;
        buf << hex_y;
        buf << dir;
        buf << pid;
    } break;
    case CMD_ADDLOCATION: {
        ushort wx = 0;
        ushort wy = 0;
        string proto_name;
        if (!(args_str >> wx >> wy >> proto_name)) {
            logcb("Invalid arguments. Example: addloc wx wy name.");
            break;
        }
        auto pid = name_resolver.ToHashedString(proto_name);
        msg_len += sizeof(wx) + sizeof(wy) + sizeof(pid);

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << wx;
        buf << wy;
        buf << pid;
    } break;
    case CMD_RUNSCRIPT: {
        string func_name;
        uint param0 = 0;
        uint param1 = 0;
        uint param2 = 0;
        if (!(args_str >> func_name >> param0 >> param1 >> param2)) {
            logcb("Invalid arguments. Example: runscript module::func param0 param1 param2.");
            break;
        }
        msg_len += NetBuffer::STRING_LEN_SIZE + static_cast<uint>(func_name.length()) + sizeof(uint) * 3;

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << func_name;
        buf << param0;
        buf << param1;
        buf << param2;
    } break;
    case CMD_REGENMAP: {
        buf << msg;
        buf << msg_len;
        buf << cmd;
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
            logcb("Invalid arguments. Example: settime tmul year month day hour minute second.");
            break;
        }
        msg_len += sizeof(multiplier) + sizeof(year) + sizeof(month) + sizeof(day) + sizeof(hour) + sizeof(minute) + sizeof(second);

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << multiplier;
        buf << year;
        buf << month;
        buf << day;
        buf << hour;
        buf << minute;
        buf << second;
    } break;
    case CMD_BAN: {
        string params;
        string cl_name;
        uint ban_hours = 0;
        string info;
        args_str >> params;
        if (!args_str.fail()) {
            args_str >> cl_name;
        }
        if (!args_str.fail()) {
            args_str >> ban_hours;
        }
        if (!args_str.fail()) {
            info = args_str.str();
        }
        if (!_str(params).compareIgnoreCase("add") && !_str(params).compareIgnoreCase("add+") && !_str(params).compareIgnoreCase("delete") && !_str(params).compareIgnoreCase("list")) {
            logcb("Invalid arguments. Example: ban [add,add+,delete,list] [user] [hours] [comment].");
            break;
        }
        cl_name = _str(cl_name).replace('*', ' ').trim();
        info = _str(info).replace('$', '*').trim();
        msg_len += NetBuffer::STRING_LEN_SIZE * 3 + static_cast<uint>(cl_name.length() + params.length() + info.length()) + sizeof(ban_hours);

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << cl_name;
        buf << params;
        buf << ban_hours;
        buf << info;
    } break;
    case CMD_LOG: {
        string flags;
        if (!(args_str >> flags) || flags.length() > 2) {
            logcb("Invalid arguments. Example: log flag. Valid flags: '+' attach, '-' detach, '--' detach all.");
            break;
        }
        msg_len += NetBuffer::STRING_LEN_SIZE + static_cast<uint>(flags.length());

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << flags;
    } break;
    default:
        return false;
    }

    return true;
}
