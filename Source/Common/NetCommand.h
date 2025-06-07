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

#pragma once

#include "Common.h"

#include "NetBuffer.h"

FO_BEGIN_NAMESPACE();

// Commands
static constexpr auto CMD_EXIT = 1;
static constexpr auto CMD_MYINFO = 2;
static constexpr auto CMD_GAMEINFO = 3;
static constexpr auto CMD_CRITID = 4;
static constexpr auto CMD_MOVECRIT = 5;
static constexpr auto CMD_DISCONCRIT = 7;
static constexpr auto CMD_TOGLOBAL = 8;
static constexpr auto CMD_PROPERTY = 10;
static constexpr auto CMD_ADDITEM = 12;
static constexpr auto CMD_ADDITEM_SELF = 14;
static constexpr auto CMD_ADDNPC = 15;
static constexpr auto CMD_ADDLOCATION = 16;
static constexpr auto CMD_RUNSCRIPT = 20;
static constexpr auto CMD_REGENMAP = 25;
static constexpr auto CMD_LOG = 37;

using LogCallback = function<void(string_view)>;

extern auto PackNetCommand(string_view str, NetOutBuffer* pbuf, const LogCallback& logcb, HashResolver& hash_resolver) -> bool;

FO_END_NAMESPACE();
