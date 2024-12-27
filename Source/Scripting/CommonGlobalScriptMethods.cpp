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
// Copyright (c) 2006 - 2024, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

///@ ExportMethod
FO_SCRIPT_API void Common_Game_BreakIntoDebugger(FOEngineBase* engine)
{
    UNUSED_VARIABLE(engine);

    BreakIntoDebugger();
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_BreakIntoDebugger(FOEngineBase* engine, string_view message)
{
    UNUSED_VARIABLE(engine);

    BreakIntoDebugger(message);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_Log(FOEngineBase* engine, string_view text)
{
    UNUSED_VARIABLE(engine);

    const auto* st_entry = GetStackTraceEntry(1);

    if (st_entry != nullptr) {
        const string module_name = strex(st_entry->file).extractFileName().eraseFileExtension();

        WriteLog("{}: {}", module_name, text);
    }
    else {
        WriteLog("{}", text);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_RequestQuit(FOEngineBase* engine)
{
    UNUSED_VARIABLE(engine);

    App->RequestQuit();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_Game_IsResourcePresent(FOEngineBase* engine, string_view resourcePath)
{
    UNUSED_VARIABLE(engine);

    return !!engine->Resources.ReadFile(resourcePath);
}

///@ ExportMethod
FO_SCRIPT_API string Common_Game_ReadResource(FOEngineBase* engine, string_view resourcePath)
{
    UNUSED_VARIABLE(engine);

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
    auto* cmd_line = _wcsdup(strex(command).toWideChar().c_str());
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

///@ ExportMethod
FO_SCRIPT_API int Common_Game_SystemCall(FOEngineBase* engine, string_view command)
{
    UNUSED_VARIABLE(engine);

    auto prefix = command.substr(0, command.find(' '));
    return SystemCall(command, [&prefix](string_view line) { WriteLog("{} : {}\n", prefix, line); });
}

///@ ExportMethod
FO_SCRIPT_API int Common_Game_SystemCall(FOEngineBase* engine, string_view command, string& output)
{
    UNUSED_VARIABLE(engine);

    output = "";

    return SystemCall(command, [&output](string_view line) {
        if (!output.empty()) {
            output += "\n";
        }
        output += line;
    });
}

///@ ExportMethod
FO_SCRIPT_API int Common_Game_Random(FOEngineBase* engine, int minValue, int maxValue)
{
    UNUSED_VARIABLE(engine);

    return GenericUtils::Random(minValue, maxValue);
}

///@ ExportMethod
FO_SCRIPT_API uint Common_Game_DecodeUtf8(FOEngineBase* engine, string_view text, uint& length)
{
    UNUSED_VARIABLE(engine);

    size_t decode_length = text.length();
    const auto ch = utf8::Decode(text.data(), decode_length); // NOLINT(bugprone-suspicious-stringview-data-usage)

    length = static_cast<uint>(decode_length);
    return ch;
}

///@ ExportMethod
FO_SCRIPT_API string Common_Game_EncodeUtf8(FOEngineBase* engine, uint ucs)
{
    UNUSED_VARIABLE(engine);

    char buf[4];
    const auto len = utf8::Encode(ucs, buf);
    return {buf, len};
}

///@ ExportMethod
FO_SCRIPT_API string Common_Game_Sha1(FOEngineBase* engine, string_view text)
{
    UNUSED_VARIABLE(engine);

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

///@ ExportMethod
FO_SCRIPT_API string Common_Game_Sha2(FOEngineBase* engine, string_view text)
{
    UNUSED_VARIABLE(engine);

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

///@ ExportMethod
FO_SCRIPT_API void Common_Game_OpenLink(FOEngineBase* engine, string_view link)
{
    UNUSED_VARIABLE(engine);

    App->OpenLink(link);
}

///@ ExportMethod
FO_SCRIPT_API uint64 Common_Game_GetUnixTime(FOEngineBase* engine)
{
    UNUSED_VARIABLE(engine);

    return static_cast<uint64>(::time(nullptr));
}

///@ ExportMethod
FO_SCRIPT_API uint Common_Game_GetDistance(FOEngineBase* engine, mpos hex1, mpos hex2)
{
    UNUSED_VARIABLE(engine);

    return GeometryHelper::DistGame(hex1, hex2);
}

///@ ExportMethod
FO_SCRIPT_API uint8 Common_Game_GetDirection(FOEngineBase* engine, mpos fromHex, mpos toHex)
{
    UNUSED_VARIABLE(engine);

    return GeometryHelper::GetFarDir(fromHex, toHex);
}

///@ ExportMethod
FO_SCRIPT_API uint8 Common_Game_GetDirection(FOEngineBase* engine, mpos fromHex, mpos toHex, float offset)
{
    UNUSED_VARIABLE(engine);

    return GeometryHelper::GetFarDir(fromHex, toHex, offset);
}

///@ ExportMethod
FO_SCRIPT_API int16 Common_Game_GetDirAngle(FOEngineBase* engine, mpos fromHex, mpos toHex)
{
    UNUSED_VARIABLE(engine);

    return static_cast<int16>(iround(GeometryHelper::GetDirAngle(fromHex, toHex)));
}

///@ ExportMethod
FO_SCRIPT_API int16 Common_Game_GetLineDirAngle(FOEngineBase* engine, ipos fromPos, ipos toPos)
{
    UNUSED_VARIABLE(engine);

    return static_cast<int16>(iround(engine->Geometry.GetLineDirAngle(fromPos.x, fromPos.y, toPos.x, toPos.y)));
}

///@ ExportMethod
FO_SCRIPT_API uint8 Common_Game_AngleToDir(FOEngineBase* engine, int16 dirAngle)
{
    UNUSED_VARIABLE(engine);

    return GeometryHelper::AngleToDir(dirAngle);
}

///@ ExportMethod
FO_SCRIPT_API int16 Common_Game_DirToAngle(FOEngineBase* engine, uint8 dir)
{
    UNUSED_VARIABLE(engine);

    return GeometryHelper::DirToAngle(dir);
}

///@ ExportMethod
FO_SCRIPT_API int16 Common_Game_RotateDirAngle(FOEngineBase* engine, int16 dirAngle, bool clockwise, int16 step)
{
    UNUSED_VARIABLE(engine);

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

///@ ExportMethod
FO_SCRIPT_API int16 Common_Game_GetDirAngleDiff(FOEngineBase* engine, int16 dirAngle1, int16 dirAngle2)
{
    UNUSED_VARIABLE(engine);

    return static_cast<int16>(iround(GeometryHelper::GetDirAngleDiff(dirAngle1, dirAngle2)));
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_GetHexInterval(FOEngineBase* engine, mpos fromHex, mpos toHex, ipos& hexOffset)
{
    UNUSED_VARIABLE(engine);

    hexOffset = engine->Geometry.GetHexInterval(fromHex, toHex);
}

///@ ExportMethod
FO_SCRIPT_API string Common_Game_GetClipboardText(FOEngineBase* engine)
{
    UNUSED_VARIABLE(engine);

    return App->Input.GetClipboardText();
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetClipboardText(FOEngineBase* engine, string_view text)
{
    UNUSED_VARIABLE(engine);

    return App->Input.SetClipboardText(text);
}

///@ ExportMethod
FO_SCRIPT_API string Common_Game_GetGameVersion(FOEngineBase* engine)
{
    UNUSED_VARIABLE(engine);

    return FO_GAME_VERSION;
}

///@ ExportMethod
FO_SCRIPT_API ProtoItem* Common_Game_GetProtoItem(FOEngineBase* engine, hstring pid)
{
    return const_cast<ProtoItem*>(engine->ProtoMngr.GetProtoItemSafe(pid));
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoItem*> Common_Game_GetProtoItems(FOEngineBase* engine)
{
    const auto& protos = engine->ProtoMngr.GetProtoItems();

    vector<ProtoItem*> result;
    result.reserve(protos.size());

    for (auto&& [pid, proto] : protos) {
        result.emplace_back(const_cast<ProtoItem*>(proto));
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoItem*> Common_Game_GetProtoItems(FOEngineBase* engine, ItemComponent component)
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

///@ ExportMethod
FO_SCRIPT_API vector<ProtoItem*> Common_Game_GetProtoItems(FOEngineBase* engine, ItemProperty property, int propertyValue)
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

///@ ExportMethod
FO_SCRIPT_API ProtoCritter* Common_Game_GetProtoCritter(FOEngineBase* engine, hstring pid)
{
    return const_cast<ProtoCritter*>(engine->ProtoMngr.GetProtoCritterSafe(pid));
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoCritter*> Common_Game_GetProtoCritters(FOEngineBase* engine)
{
    const auto& protos = engine->ProtoMngr.GetProtoCritters();

    vector<ProtoCritter*> result;
    result.reserve(protos.size());

    for (auto&& [pid, proto] : protos) {
        result.emplace_back(const_cast<ProtoCritter*>(proto));
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoCritter*> Common_Game_GetProtoCritters(FOEngineBase* engine, CritterComponent component)
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

///@ ExportMethod
FO_SCRIPT_API vector<ProtoCritter*> Common_Game_GetProtoCritters(FOEngineBase* engine, CritterProperty property, int propertyValue)
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

///@ ExportMethod
FO_SCRIPT_API ProtoMap* Common_Game_GetProtoMap(FOEngineBase* engine, hstring pid)
{
    return const_cast<ProtoMap*>(engine->ProtoMngr.GetProtoMapSafe(pid));
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoMap*> Common_Game_GetProtoMaps(FOEngineBase* engine)
{
    const auto& protos = engine->ProtoMngr.GetProtoMaps();

    vector<ProtoMap*> result;
    result.reserve(protos.size());

    for (auto&& [pid, proto] : protos) {
        result.emplace_back(const_cast<ProtoMap*>(proto));
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoMap*> Common_Game_GetProtoMaps(FOEngineBase* engine, MapComponent component)
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

///@ ExportMethod
FO_SCRIPT_API vector<ProtoMap*> Common_Game_GetProtoMaps(FOEngineBase* engine, MapProperty property, int propertyValue)
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

///@ ExportMethod
FO_SCRIPT_API ProtoLocation* Common_Game_GetProtoLocation(FOEngineBase* engine, hstring pid)
{
    return const_cast<ProtoLocation*>(engine->ProtoMngr.GetProtoLocationSafe(pid));
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoLocation*> Common_Game_GetProtoLocations(FOEngineBase* engine)
{
    const auto& protos = engine->ProtoMngr.GetProtoLocations();

    vector<ProtoLocation*> result;
    result.reserve(protos.size());

    for (auto&& [pid, proto] : protos) {
        result.emplace_back(const_cast<ProtoLocation*>(proto));
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoLocation*> Common_Game_GetProtoLocations(FOEngineBase* engine, LocationComponent component)
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

///@ ExportMethod
FO_SCRIPT_API vector<ProtoLocation*> Common_Game_GetProtoLocations(FOEngineBase* engine, LocationProperty property, int propertyValue)
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

///@ ExportMethod
FO_SCRIPT_API uint Common_Game_GetTick(FOEngineBase* engine)
{
    return time_duration_to_ms<uint>(engine->GameTime.FrameTime().time_since_epoch());
}

///@ ExportMethod
FO_SCRIPT_API tick_t Common_Game_GetServerTime(FOEngineBase* engine)
{
    return engine->GameTime.GetServerTime();
}

///@ ExportMethod
FO_SCRIPT_API tick_t Common_Game_DateToServerTime(FOEngineBase* engine, uint16 year, uint16 month, uint16 day, uint16 hour, uint16 minute, uint16 second)
{
    if (year != 0 && year < engine->Settings.StartYear) {
        throw ScriptException("Invalid year", year);
    }
    if (year != 0 && year > engine->Settings.StartYear + 100) {
        throw ScriptException("Invalid year", year);
    }
    if (month != 0 && month < 1) {
        throw ScriptException("Invalid month", month);
    }
    if (month != 0 && month > 12) {
        throw ScriptException("Invalid month", month);
    }

    auto year_ = year;
    auto month_ = month;
    auto day_ = day;

    if (year_ == 0) {
        year_ = engine->GetYear();
    }
    if (month_ == 0) {
        month_ = engine->GetMonth();
    }

    if (day_ != 0) {
        const auto month_day = Timer::GameTimeMonthDays(year, month_);

        if (day_ < month_day || day_ > month_day) {
            throw ScriptException("Invalid day", day_, month_day);
        }
    }

    if (day_ == 0) {
        day_ = engine->GetDay();
    }

    if (hour > 23) {
        throw ScriptException("Invalid hour", hour);
    }
    if (minute > 59) {
        throw ScriptException("Invalid minute", minute);
    }
    if (second > 59) {
        throw ScriptException("Invalid second", second);
    }

    return engine->GameTime.DateToServerTime(year_, month_, day_, hour, minute, second);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_ServerToDateTime(FOEngineBase* engine, tick_t serverTime, uint16& year, uint16& month, uint16& day, uint16& dayOfWeek, uint16& hour, uint16& minute, uint16& second)
{
    const auto dt = engine->GameTime.ServerToDateTime(serverTime);
    year = dt.Year;
    month = dt.Month;
    dayOfWeek = dt.DayOfWeek;
    day = dt.Day;
    hour = dt.Hour;
    minute = dt.Minute;
    second = dt.Second;
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_GetCurDateTime(FOEngineBase* engine, uint16& year, uint16& month, uint16& day, uint16& dayOfWeek, uint16& hour, uint16& minute, uint16& second, uint16& milliseconds)
{
    UNUSED_VARIABLE(engine);

    const auto cur_time = Timer::GetCurrentDateTime();
    year = cur_time.Year;
    month = cur_time.Month;
    dayOfWeek = cur_time.DayOfWeek;
    day = cur_time.Day;
    hour = cur_time.Hour;
    minute = cur_time.Minute;
    second = cur_time.Second;
    milliseconds = cur_time.Milliseconds;
}
