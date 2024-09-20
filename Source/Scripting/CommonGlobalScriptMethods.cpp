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

#include "Common.h"

#include "Application.h"
#include "EngineBase.h"
#include "GenericUtils.h"
#include "Log.h"
#include "ScriptSystem.h"
#include "StringUtils.h"
#include "Version-Include.h"
#include "WinApi-Include.h"

#include "sha1.h"
#include "sha2.h"

// ReSharper disable CppInconsistentNaming

///# ...
///@ ExportMethod
[[maybe_unused]] void Common_Game_BreakIntoDebugger([[maybe_unused]] FOEngineBase* engine)
{
    BreakIntoDebugger();
}

///# ...
///# param message ...
///@ ExportMethod
[[maybe_unused]] void Common_Game_BreakIntoDebugger([[maybe_unused]] FOEngineBase* engine, string_view message)
{
    BreakIntoDebugger(message);
}

///# ...
///# param text ...
///@ ExportMethod
[[maybe_unused]] void Common_Game_Log([[maybe_unused]] FOEngineBase* engine, string_view text)
{
    const auto* st_entry = GetStackTraceEntry(1);

    if (st_entry != nullptr) {
        const string module_name = _str(st_entry->file).extractFileName().eraseFileExtension();

        WriteLog("{}: {}", module_name, text);
    }
    else {
        WriteLog("{}", text);
    }
}

///# ...
///@ ExportMethod
[[maybe_unused]] void Common_Game_RequestQuit([[maybe_unused]] FOEngineBase* engine)
{
    App->RequestQuit();
}

///# ...
///# param resourcePath ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Common_Game_IsResourcePresent([[maybe_unused]] FOEngineBase* engine, string_view resourcePath)
{
    return !!engine->Resources.ReadFile(resourcePath);
}

///# ...
///# param resourcePath ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Common_Game_ReadResource([[maybe_unused]] FOEngineBase* engine, string_view resourcePath)
{
    return engine->Resources.ReadFileText(resourcePath);
}

static void PrintLog(string& log, bool last_call, const std::function<void(string_view)>& log_callback)
{
    // Normalize new lines to \n
    while (true) {
        const auto pos = log.find("\r\n");
        if (pos != string::npos) {
            log.replace(pos, 2, "\n");
        }
        else {
            break;
        }
    }

    log.erase(std::remove(log.begin(), log.end(), '\r'), log.end());

    // Write own log
    while (true) {
        auto pos = log.find('\n');
        if (pos == string::npos && last_call && !log.empty()) {
            pos = log.size();
        }

        if (pos != string::npos) {
            log_callback(log.substr(0, pos));
            log.erase(0, pos + 1);
        }
        else {
            break;
        }
    }
}

static auto SystemCall(string_view command, const std::function<void(string_view)>& log_callback) -> int
{
#if FO_WINDOWS && !FO_UWP
    HANDLE out_read = nullptr;
    HANDLE out_write = nullptr;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    if (::CreatePipe(&out_read, &out_write, &sa, 0) == 0) {
        return -1;
    }

    if (::SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0) == 0) {
        ::CloseHandle(out_read);
        ::CloseHandle(out_write);
        return -1;
    }

    STARTUPINFOW si = {};
    si.cb = sizeof(STARTUPINFO);
    si.hStdError = out_write;
    si.hStdOutput = out_write;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    auto* cmd_line = _wcsdup(_str(command).toWideChar().c_str());
    const auto result = ::CreateProcessW(nullptr, cmd_line, nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi);
    ::free(cmd_line);

    if (result == 0) {
        ::CloseHandle(out_read);
        ::CloseHandle(out_write);
        return -1;
    }

    string log;

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        DWORD bytes = 0;
        while (::PeekNamedPipe(out_read, nullptr, 0, nullptr, &bytes, nullptr) != 0 && bytes > 0) {
            char buf[4096];
            if (::ReadFile(out_read, buf, sizeof(buf), &bytes, nullptr) != 0) {
                log.append(buf, bytes);
                PrintLog(log, false, log_callback);
            }
        }

        if (::WaitForSingleObject(pi.hProcess, 0) != WAIT_TIMEOUT) {
            break;
        }
    }

    PrintLog(log, true, log_callback);

    DWORD retval = 0;
    ::GetExitCodeProcess(pi.hProcess, &retval);

    ::CloseHandle(out_read);
    ::CloseHandle(out_write);
    ::CloseHandle(pi.hProcess);
    ::CloseHandle(pi.hThread);

    return static_cast<int>(retval);

#elif !FO_WINDOWS && !FO_WEB
    FILE* in = popen(string(command).c_str(), "r");
    if (!in) {
        return -1;
    }

    string log;
    char buf[4096];
    while (fgets(buf, sizeof(buf), in)) {
        log += buf;
        PrintLog(log, false, log_callback);
    }
    PrintLog(log, true, log_callback);

    return pclose(in);
#else
    return 1;
#endif
}

///# ...
///# param command ...
///# return ...
///@ ExportMethod
[[maybe_unused]] int Common_Game_SystemCall([[maybe_unused]] FOEngineBase* engine, string_view command)
{
    auto prefix = command.substr(0, command.find(' '));
    return SystemCall(command, [&prefix](string_view line) { WriteLog("{} : {}\n", prefix, line); });
}

///# ...
///# param command ...
///# param output ...
///# return ...
///@ ExportMethod
[[maybe_unused]] int Common_Game_SystemCall([[maybe_unused]] FOEngineBase* engine, string_view command, string& output)
{
    output = "";

    return SystemCall(command, [&output](string_view line) {
        if (!output.empty()) {
            output += "\n";
        }
        output += line;
    });
}

///# ...
///# param minValue ...
///# param maxValue ...
///# return ...
///@ ExportMethod
[[maybe_unused]] int Common_Game_Random([[maybe_unused]] FOEngineBase* engine, int minValue, int maxValue)
{
    return GenericUtils::Random(minValue, maxValue);
}

///# ...
///# param text ...
///# param length ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Common_Game_DecodeUTF8([[maybe_unused]] FOEngineBase* engine, string_view text, uint& length)
{
    size_t decode_length = text.length();
    const auto ch = utf8::Decode(text.data(), decode_length);

    length = static_cast<uint>(decode_length);
    return ch;
}

///# ...
///# param ucs ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Common_Game_EncodeUTF8([[maybe_unused]] FOEngineBase* engine, uint ucs)
{
    char buf[4];
    const auto len = utf8::Encode(ucs, buf);
    return {buf, len};
}

///# ...
///# param text ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Common_Game_SHA1([[maybe_unused]] FOEngineBase* engine, string_view text)
{
    SHA1_CTX ctx;
    _SHA1_Init(&ctx);
    _SHA1_Update(&ctx, reinterpret_cast<const uint8*>(text.data()), text.length());
    uint8 digest[SHA1_DIGEST_SIZE];
    _SHA1_Final(&ctx, digest);

    const auto* nums = "0123456789abcdef";
    char hex_digest[SHA1_DIGEST_SIZE * 2];
    for (uint i = 0; i < sizeof(hex_digest); i++) {
        hex_digest[i] = nums[(i % 2) != 0 ? digest[i / 2] & 0xF : digest[i / 2] >> 4];
    }

    return {hex_digest, sizeof(hex_digest)};
}

///# ...
///# param text ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Common_Game_SHA2([[maybe_unused]] FOEngineBase* engine, string_view text)
{
    constexpr uint digest_size = 32;
    uint8 digest[digest_size];
    sha256(reinterpret_cast<const uint8*>(text.data()), static_cast<uint>(text.length()), digest);

    const auto* nums = "0123456789abcdef";
    char hex_digest[digest_size * 2];
    for (uint i = 0; i < sizeof(hex_digest); i++) {
        hex_digest[i] = nums[(i % 2) != 0 ? digest[i / 2] & 0xF : digest[i / 2] >> 4];
    }
    return {hex_digest, sizeof(hex_digest)};
}

///# ...
///# param link ...
///@ ExportMethod
[[maybe_unused]] void Common_Game_OpenLink([[maybe_unused]] FOEngineBase* engine, string_view link)
{
    App->OpenLink(link);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint64 Common_Game_GetUnixTime([[maybe_unused]] FOEngineBase* engine)
{
    return static_cast<uint64>(::time(nullptr));
}

///# ...
///# param hx1 ...
///# param hy1 ...
///# param hx2 ...
///# param hy2 ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Common_Game_GetDistance([[maybe_unused]] FOEngineBase* engine, uint16 hx1, uint16 hy1, uint16 hx2, uint16 hy2)
{
    return GeometryHelper::DistGame(hx1, hy1, hx2, hy2);
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint8 Common_Game_GetDirection([[maybe_unused]] FOEngineBase* engine, uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy)
{
    return GeometryHelper::GetFarDir(fromHx, fromHy, toHx, toHy);
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param offset ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint8 Common_Game_GetDirection([[maybe_unused]] FOEngineBase* engine, uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float offset)
{
    return GeometryHelper::GetFarDir(fromHx, fromHy, toHx, toHy, offset);
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] int16 Common_Game_GetDirAngle([[maybe_unused]] FOEngineBase* engine, uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy)
{
    return static_cast<int16>(GeometryHelper::GetDirAngle(fromHx, fromHy, toHx, toHy));
}

///# ...
///# param fromX ...
///# param fromY ...
///# param toX ...
///# param toY ...
///# return ...
///@ ExportMethod
[[maybe_unused]] int16 Common_Game_GetLineDirAngle([[maybe_unused]] FOEngineBase* engine, int fromX, int fromY, int toX, int toY)
{
    return static_cast<int16>(engine->Geometry.GetLineDirAngle(fromX, fromY, toX, toY));
}

///# ...
///# param dirAngle ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint8 Common_Game_AngleToDir([[maybe_unused]] FOEngineBase* engine, int16 dirAngle)
{
    return GeometryHelper::AngleToDir(dirAngle);
}

///# ...
///# param dir ...
///# return ...
///@ ExportMethod
[[maybe_unused]] int16 Common_Game_DirToAngle([[maybe_unused]] FOEngineBase* engine, uint8 dir)
{
    return GeometryHelper::DirToAngle(dir);
}

///# ...
///# param dirAngle ...
///# param clockwise ...
///# param step ...
///# return ...
///@ ExportMethod
[[maybe_unused]] int16 Common_Game_RotateDirAngle([[maybe_unused]] FOEngineBase* engine, int16 dirAngle, bool clockwise, int16 step)
{
    auto rotated = static_cast<int>(dirAngle);

    if (clockwise) {
        rotated += step;
    }
    else {
        rotated -= step;
    }

    while (rotated < 0) {
        rotated += 360;
    }
    while (rotated >= 360) {
        rotated -= 360;
    }

    return static_cast<int16>(rotated);
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param ox ...
///# param oy ...
///@ ExportMethod
[[maybe_unused]] void Common_Game_GetHexInterval([[maybe_unused]] FOEngineBase* engine, uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, int& ox, int& oy)
{
    std::tie(ox, oy) = engine->Geometry.GetHexInterval(fromHx, fromHy, toHx, toHy);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Common_Game_GetClipboardText([[maybe_unused]] FOEngineBase* engine)
{
    return App->Input.GetClipboardText();
}

///# ...
///# param text ...
///@ ExportMethod
[[maybe_unused]] void Common_Game_SetClipboardText([[maybe_unused]] FOEngineBase* engine, string_view text)
{
    return App->Input.SetClipboardText(text);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Common_Game_GetGameVersion([[maybe_unused]] FOEngineBase* engine)
{
    return FO_GAME_VERSION;
}

///# ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ProtoItem* Common_Game_GetProtoItem(FOEngineBase* engine, hstring pid)
{
    return const_cast<ProtoItem*>(engine->ProtoMngr.GetProtoItemSafe(pid));
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<ProtoItem*> Common_Game_GetProtoItems(FOEngineBase* engine)
{
    const auto& protos = engine->ProtoMngr.GetProtoItems();

    vector<ProtoItem*> result;
    result.reserve(protos.size());

    for (auto&& [pid, proto] : protos) {
        result.emplace_back(const_cast<ProtoItem*>(proto));
    }

    return result;
}

///# ...
///# param component ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<ProtoItem*> Common_Game_GetProtoItems(FOEngineBase* engine, ItemComponent component)
{
    const auto& protos = engine->ProtoMngr.GetProtoItems();

    vector<ProtoItem*> result;
    result.reserve(protos.size());

    for (auto&& [pid, proto] : protos) {
        if (proto->HasComponent(static_cast<hstring::hash_t>(component))) {
            result.emplace_back(const_cast<ProtoItem*>(proto));
        }
    }

    return result;
}

///# ...
///# param property ...
///# param propertyValue ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<ProtoItem*> Common_Game_GetProtoItems(FOEngineBase* engine, ItemProperty property, int propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<ItemProperties>(engine, property);
    const auto& protos = engine->ProtoMngr.GetProtoItems();

    vector<ProtoItem*> result;
    result.reserve(protos.size());

    for (auto&& [pid, proto] : protos) {
        if (proto->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(const_cast<ProtoItem*>(proto));
        }
    }

    return result;
}

///# ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ProtoCritter* Common_Game_GetProtoCritter(FOEngineBase* engine, hstring pid)
{
    return const_cast<ProtoCritter*>(engine->ProtoMngr.GetProtoCritterSafe(pid));
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<ProtoCritter*> Common_Game_GetProtoCritters(FOEngineBase* engine)
{
    const auto& protos = engine->ProtoMngr.GetProtoCritters();

    vector<ProtoCritter*> result;
    result.reserve(protos.size());

    for (auto&& [pid, proto] : protos) {
        result.emplace_back(const_cast<ProtoCritter*>(proto));
    }

    return result;
}

///# ...
///# param component ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<ProtoCritter*> Common_Game_GetProtoCritters(FOEngineBase* engine, CritterComponent component)
{
    const auto& protos = engine->ProtoMngr.GetProtoCritters();

    vector<ProtoCritter*> result;
    result.reserve(protos.size());

    for (auto&& [pid, proto] : protos) {
        if (proto->HasComponent(static_cast<hstring::hash_t>(component))) {
            result.emplace_back(const_cast<ProtoCritter*>(proto));
        }
    }

    return result;
}

///# ...
///# param property ...
///# param propertyValue ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<ProtoCritter*> Common_Game_GetProtoCritters(FOEngineBase* engine, CritterProperty property, int propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<CritterProperties>(engine, property);
    const auto& protos = engine->ProtoMngr.GetProtoCritters();

    vector<ProtoCritter*> result;
    result.reserve(protos.size());

    for (auto&& [pid, proto] : protos) {
        if (proto->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(const_cast<ProtoCritter*>(proto));
        }
    }

    return result;
}

///# ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ProtoMap* Common_Game_GetProtoMap(FOEngineBase* engine, hstring pid)
{
    return const_cast<ProtoMap*>(engine->ProtoMngr.GetProtoMapSafe(pid));
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<ProtoMap*> Common_Game_GetProtoMaps(FOEngineBase* engine)
{
    const auto& protos = engine->ProtoMngr.GetProtoMaps();

    vector<ProtoMap*> result;
    result.reserve(protos.size());

    for (auto&& [pid, proto] : protos) {
        result.emplace_back(const_cast<ProtoMap*>(proto));
    }

    return result;
}

///# ...
///# param component ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<ProtoMap*> Common_Game_GetProtoMaps(FOEngineBase* engine, MapComponent component)
{
    const auto& protos = engine->ProtoMngr.GetProtoMaps();

    vector<ProtoMap*> result;
    result.reserve(protos.size());

    for (auto&& [pid, proto] : protos) {
        if (proto->HasComponent(static_cast<hstring::hash_t>(component))) {
            result.emplace_back(const_cast<ProtoMap*>(proto));
        }
    }

    return result;
}

///# ...
///# param property ...
///# param propertyValue ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<ProtoMap*> Common_Game_GetProtoMaps(FOEngineBase* engine, MapProperty property, int propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<MapProperties>(engine, property);
    const auto& protos = engine->ProtoMngr.GetProtoMaps();

    vector<ProtoMap*> result;
    result.reserve(protos.size());

    for (auto&& [pid, proto] : protos) {
        if (proto->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(const_cast<ProtoMap*>(proto));
        }
    }

    return result;
}

///# ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ProtoLocation* Common_Game_GetProtoLocation(FOEngineBase* engine, hstring pid)
{
    return const_cast<ProtoLocation*>(engine->ProtoMngr.GetProtoLocationSafe(pid));
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<ProtoLocation*> Common_Game_GetProtoLocations(FOEngineBase* engine)
{
    const auto& protos = engine->ProtoMngr.GetProtoLocations();

    vector<ProtoLocation*> result;
    result.reserve(protos.size());

    for (auto&& [pid, proto] : protos) {
        result.emplace_back(const_cast<ProtoLocation*>(proto));
    }

    return result;
}

///# ...
///# param component ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<ProtoLocation*> Common_Game_GetProtoLocations(FOEngineBase* engine, LocationComponent component)
{
    const auto& protos = engine->ProtoMngr.GetProtoLocations();

    vector<ProtoLocation*> result;
    result.reserve(protos.size());

    for (auto&& [pid, proto] : protos) {
        if (proto->HasComponent(static_cast<hstring::hash_t>(component))) {
            result.emplace_back(const_cast<ProtoLocation*>(proto));
        }
    }

    return result;
}

///# ...
///# param property ...
///# param propertyValue ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<ProtoLocation*> Common_Game_GetProtoLocations(FOEngineBase* engine, LocationProperty property, int propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<LocationProperties>(engine, property);
    const auto& protos = engine->ProtoMngr.GetProtoLocations();

    vector<ProtoLocation*> result;
    result.reserve(protos.size());

    for (auto&& [pid, proto] : protos) {
        if (proto->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(const_cast<ProtoLocation*>(proto));
        }
    }

    return result;
}
