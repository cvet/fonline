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
FO_SCRIPT_API void Common_Game_BreakIntoDebugger(BaseEngine* engine)
{
    UNUSED_VARIABLE(engine);

    BreakIntoDebugger();
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_BreakIntoDebugger(BaseEngine* engine, string_view message)
{
    UNUSED_VARIABLE(engine);

    BreakIntoDebugger(message);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_Log(BaseEngine* engine, string_view text)
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
FO_SCRIPT_API void Common_Game_RequestQuit(BaseEngine* engine)
{
    UNUSED_VARIABLE(engine);

    App->RequestQuit();
}

///@ ExportMethod
FO_SCRIPT_API bool Common_Game_IsResourcePresent(BaseEngine* engine, string_view resourcePath)
{
    UNUSED_VARIABLE(engine);

    return !!engine->Resources.ReadFile(resourcePath);
}

///@ ExportMethod
FO_SCRIPT_API string Common_Game_ReadResource(BaseEngine* engine, string_view resourcePath)
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
FO_SCRIPT_API int Common_Game_SystemCall(BaseEngine* engine, string_view command)
{
    UNUSED_VARIABLE(engine);

    auto prefix = command.substr(0, command.find(' '));
    return SystemCall(command, [&prefix](string_view line) { WriteLog("{} : {}\n", prefix, line); });
}

///@ ExportMethod
FO_SCRIPT_API int Common_Game_SystemCall(BaseEngine* engine, string_view command, string& output)
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
FO_SCRIPT_API int Common_Game_Random(BaseEngine* engine, int minValue, int maxValue)
{
    UNUSED_VARIABLE(engine);

    return GenericUtils::Random(minValue, maxValue);
}

///@ ExportMethod
FO_SCRIPT_API uint Common_Game_DecodeUtf8(BaseEngine* engine, string_view text, uint& length)
{
    UNUSED_VARIABLE(engine);

    size_t decode_length = text.length();
    const auto ch = utf8::Decode(text.data(), decode_length); // NOLINT(bugprone-suspicious-stringview-data-usage)

    length = static_cast<uint>(decode_length);
    return ch;
}

///@ ExportMethod
FO_SCRIPT_API string Common_Game_EncodeUtf8(BaseEngine* engine, uint ucs)
{
    UNUSED_VARIABLE(engine);

    char buf[4];
    const auto len = utf8::Encode(ucs, buf);
    return {buf, len};
}

///@ ExportMethod
FO_SCRIPT_API string Common_Game_Sha1(BaseEngine* engine, string_view text)
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
FO_SCRIPT_API string Common_Game_Sha2(BaseEngine* engine, string_view text)
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
FO_SCRIPT_API void Common_Game_OpenLink(BaseEngine* engine, string_view link)
{
    UNUSED_VARIABLE(engine);

    App->OpenLink(link);
}

///@ ExportMethod
FO_SCRIPT_API uint64 Common_Game_GetUnixTime(BaseEngine* engine)
{
    UNUSED_VARIABLE(engine);

    return static_cast<uint64>(::time(nullptr));
}

///@ ExportMethod
FO_SCRIPT_API uint Common_Game_GetDistance(BaseEngine* engine, mpos hex1, mpos hex2)
{
    UNUSED_VARIABLE(engine);

    return GeometryHelper::DistGame(hex1, hex2);
}

///@ ExportMethod
FO_SCRIPT_API uint8 Common_Game_GetDirection(BaseEngine* engine, mpos fromHex, mpos toHex)
{
    UNUSED_VARIABLE(engine);

    return GeometryHelper::GetFarDir(fromHex, toHex);
}

///@ ExportMethod
FO_SCRIPT_API uint8 Common_Game_GetDirection(BaseEngine* engine, mpos fromHex, mpos toHex, float offset)
{
    UNUSED_VARIABLE(engine);

    return GeometryHelper::GetFarDir(fromHex, toHex, offset);
}

///@ ExportMethod
FO_SCRIPT_API int16 Common_Game_GetDirAngle(BaseEngine* engine, mpos fromHex, mpos toHex)
{
    UNUSED_VARIABLE(engine);

    return static_cast<int16>(iround(GeometryHelper::GetDirAngle(fromHex, toHex)));
}

///@ ExportMethod
FO_SCRIPT_API int16 Common_Game_GetLineDirAngle(BaseEngine* engine, ipos fromPos, ipos toPos)
{
    UNUSED_VARIABLE(engine);

    return static_cast<int16>(iround(engine->Geometry.GetLineDirAngle(fromPos.x, fromPos.y, toPos.x, toPos.y)));
}

///@ ExportMethod
FO_SCRIPT_API uint8 Common_Game_AngleToDir(BaseEngine* engine, int16 dirAngle)
{
    UNUSED_VARIABLE(engine);

    return GeometryHelper::AngleToDir(dirAngle);
}

///@ ExportMethod
FO_SCRIPT_API int16 Common_Game_DirToAngle(BaseEngine* engine, uint8 dir)
{
    UNUSED_VARIABLE(engine);

    return GeometryHelper::DirToAngle(dir);
}

///@ ExportMethod
FO_SCRIPT_API int16 Common_Game_RotateDirAngle(BaseEngine* engine, int16 dirAngle, bool clockwise, int16 step)
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
FO_SCRIPT_API int16 Common_Game_GetDirAngleDiff(BaseEngine* engine, int16 dirAngle1, int16 dirAngle2)
{
    UNUSED_VARIABLE(engine);

    return static_cast<int16>(iround(GeometryHelper::GetDirAngleDiff(dirAngle1, dirAngle2)));
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_GetHexInterval(BaseEngine* engine, mpos fromHex, mpos toHex, ipos& hexOffset)
{
    UNUSED_VARIABLE(engine);

    hexOffset = engine->Geometry.GetHexInterval(fromHex, toHex);
}

///@ ExportMethod
FO_SCRIPT_API string Common_Game_GetClipboardText(BaseEngine* engine)
{
    UNUSED_VARIABLE(engine);

    return App->Input.GetClipboardText();
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetClipboardText(BaseEngine* engine, string_view text)
{
    UNUSED_VARIABLE(engine);

    return App->Input.SetClipboardText(text);
}

///@ ExportMethod
FO_SCRIPT_API ProtoItem* Common_Game_GetProtoItem(BaseEngine* engine, hstring pid)
{
    return const_cast<ProtoItem*>(engine->ProtoMngr.GetProtoItemSafe(pid));
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoItem*> Common_Game_GetProtoItems(BaseEngine* engine)
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
FO_SCRIPT_API vector<ProtoItem*> Common_Game_GetProtoItems(BaseEngine* engine, ItemComponent component)
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
FO_SCRIPT_API vector<ProtoItem*> Common_Game_GetProtoItems(BaseEngine* engine, ItemProperty property, int propertyValue)
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
FO_SCRIPT_API ProtoCritter* Common_Game_GetProtoCritter(BaseEngine* engine, hstring pid)
{
    return const_cast<ProtoCritter*>(engine->ProtoMngr.GetProtoCritterSafe(pid));
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoCritter*> Common_Game_GetProtoCritters(BaseEngine* engine)
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
FO_SCRIPT_API vector<ProtoCritter*> Common_Game_GetProtoCritters(BaseEngine* engine, CritterComponent component)
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
FO_SCRIPT_API vector<ProtoCritter*> Common_Game_GetProtoCritters(BaseEngine* engine, CritterProperty property, int propertyValue)
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
FO_SCRIPT_API ProtoMap* Common_Game_GetProtoMap(BaseEngine* engine, hstring pid)
{
    return const_cast<ProtoMap*>(engine->ProtoMngr.GetProtoMapSafe(pid));
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoMap*> Common_Game_GetProtoMaps(BaseEngine* engine)
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
FO_SCRIPT_API vector<ProtoMap*> Common_Game_GetProtoMaps(BaseEngine* engine, MapComponent component)
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
FO_SCRIPT_API vector<ProtoMap*> Common_Game_GetProtoMaps(BaseEngine* engine, MapProperty property, int propertyValue)
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
FO_SCRIPT_API ProtoLocation* Common_Game_GetProtoLocation(BaseEngine* engine, hstring pid)
{
    return const_cast<ProtoLocation*>(engine->ProtoMngr.GetProtoLocationSafe(pid));
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoLocation*> Common_Game_GetProtoLocations(BaseEngine* engine)
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
FO_SCRIPT_API vector<ProtoLocation*> Common_Game_GetProtoLocations(BaseEngine* engine, LocationComponent component)
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
FO_SCRIPT_API vector<ProtoLocation*> Common_Game_GetProtoLocations(BaseEngine* engine, LocationProperty property, int propertyValue)
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
FO_SCRIPT_API nanotime Common_Game_GetPrecisionTime(BaseEngine* engine)
{
    UNUSED_VARIABLE(engine);

    return nanotime::now();
}

///@ ExportMethod
FO_SCRIPT_API nanotime Common_Game_PackTime(BaseEngine* engine, int year, int month, int day, int hour, int minute, int second, int millisecond, int microsecond, int nanosecond)
{
    UNUSED_VARIABLE(engine);

    return nanotime::now() + make_time_offset(year, month, day, hour, minute, second, millisecond, microsecond, nanosecond, true);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_UnpackTime(BaseEngine* engine, nanotime time, int& year, int& month, int& day, int& hour, int& minute, int& second, int& millisecond, int& microsecond, int& nanosecond)
{
    UNUSED_VARIABLE(engine);

    const auto time_desc = time.desc(true);
    year = time_desc.year;
    month = time_desc.month;
    day = time_desc.day;
    hour = time_desc.hour;
    minute = time_desc.minute;
    second = time_desc.second;
    millisecond = time_desc.millisecond;
    microsecond = time_desc.microsecond;
    nanosecond = time_desc.nanosecond;
}

///@ ExportMethod
FO_SCRIPT_API synctime Common_Game_PackSynchronizedTime(BaseEngine* engine, int year, int month, int day, int hour, int minute, int second, int millisecond)
{
    UNUSED_VARIABLE(engine);

    return engine->GameTime.GetSynchronizedTime() + make_time_offset(year, month, day, hour, minute, second, millisecond, 0, 0, true);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_UnpackSynchronizedTime(BaseEngine* engine, synctime time, int& year, int& month, int& day, int& hour, int& minute, int& second, int& millisecond)
{
    UNUSED_VARIABLE(engine);

    const auto time_desc = make_time_desc(time - engine->GameTime.GetSynchronizedTime(), true);
    year = time_desc.year;
    month = time_desc.month;
    day = time_desc.day;
    hour = time_desc.hour;
    minute = time_desc.minute;
    second = time_desc.second;
    millisecond = time_desc.millisecond;
}

///@ ExportMethod
FO_SCRIPT_API uint Common_Game_StartTimeEvent(BaseEngine* engine, timespan delay, ScriptFuncName<void> func)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, false, func, delay, {}, {});
}

///@ ExportMethod
FO_SCRIPT_API uint Common_Game_StartTimeEvent(BaseEngine* engine, timespan delay, ScriptFuncName<void, any_t> func, any_t data)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, false, func, delay, {}, vector<any_t> {std::move(data)});
}

///@ ExportMethod
FO_SCRIPT_API uint Common_Game_StartTimeEvent(BaseEngine* engine, timespan delay, ScriptFuncName<void, vector<any_t>> func, const vector<any_t>& data)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, false, func, delay, {}, data);
}

///@ ExportMethod
FO_SCRIPT_API uint Common_Game_StartTimeEvent(BaseEngine* engine, timespan delay, timespan repeat, ScriptFuncName<void> func)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, false, func, delay, repeat, {});
}

///@ ExportMethod
FO_SCRIPT_API uint Common_Game_StartTimeEvent(BaseEngine* engine, timespan delay, timespan repeat, ScriptFuncName<void, any_t> func, any_t data)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, false, func, delay, repeat, vector<any_t> {std::move(data)});
}

///@ ExportMethod
FO_SCRIPT_API uint Common_Game_StartTimeEvent(BaseEngine* engine, timespan delay, timespan repeat, ScriptFuncName<void, vector<any_t>> func, const vector<any_t>& data)
{
    return engine->TimeEventMngr.StartTimeEvent(engine, false, func, delay, repeat, data);
}

///@ ExportMethod
FO_SCRIPT_API uint Common_Game_CountTimeEvent(BaseEngine* engine, ScriptFuncName<void> func)
{
    return numeric_cast<uint>(engine->TimeEventMngr.CountTimeEvent(engine, func, {}));
}

///@ ExportMethod
FO_SCRIPT_API uint Common_Game_CountTimeEvent(BaseEngine* engine, ScriptFuncName<void, any_t> func)
{
    return numeric_cast<uint>(engine->TimeEventMngr.CountTimeEvent(engine, func, {}));
}

///@ ExportMethod
FO_SCRIPT_API uint Common_Game_CountTimeEvent(BaseEngine* engine, ScriptFuncName<void, vector<any_t>> func)
{
    return numeric_cast<uint>(engine->TimeEventMngr.CountTimeEvent(engine, func, {}));
}

///@ ExportMethod
FO_SCRIPT_API uint Common_Game_CountTimeEvent(BaseEngine* engine, uint id)
{
    return numeric_cast<uint>(engine->TimeEventMngr.CountTimeEvent(engine, {}, id));
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_StopTimeEvent(BaseEngine* engine, ScriptFuncName<void> func)
{
    engine->TimeEventMngr.StopTimeEvent(engine, func, {});
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_StopTimeEvent(BaseEngine* engine, ScriptFuncName<void, any_t> func)
{
    engine->TimeEventMngr.StopTimeEvent(engine, func, {});
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_StopTimeEvent(BaseEngine* engine, ScriptFuncName<void, vector<any_t>> func)
{
    engine->TimeEventMngr.StopTimeEvent(engine, func, {});
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_StopTimeEvent(BaseEngine* engine, uint id)
{
    engine->TimeEventMngr.StopTimeEvent(engine, {}, id);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_RepeatTimeEvent(BaseEngine* engine, ScriptFuncName<void> func, timespan repeat)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func, {}, repeat, std::nullopt);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_RepeatTimeEvent(BaseEngine* engine, ScriptFuncName<void, any_t> func, timespan repeat)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func, {}, repeat, std::nullopt);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_RepeatTimeEvent(BaseEngine* engine, ScriptFuncName<void, vector<any_t>> func, timespan repeat)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func, {}, repeat, std::nullopt);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_RepeatTimeEvent(BaseEngine* engine, uint id, timespan repeat)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, {}, id, repeat, std::nullopt);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetTimeEventData(BaseEngine* engine, ScriptFuncName<void> func, any_t data)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func, {}, {}, vector<any_t> {std::move(data)});
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetTimeEventData(BaseEngine* engine, ScriptFuncName<void, vector<any_t>> func, const vector<any_t>& data)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, func, {}, {}, data);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetTimeEventData(BaseEngine* engine, uint id, any_t data)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, {}, id, {}, vector<any_t> {std::move(data)});
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetTimeEventData(BaseEngine* engine, uint id, const vector<any_t>& data)
{
    engine->TimeEventMngr.ModifyTimeEvent(engine, {}, id, {}, data);
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_StopCurrentTimeEvent(BaseEngine* engine)
{
    if (auto&& [entity, te] = engine->TimeEventMngr.GetCurTimeEvent(); entity != nullptr) {
        engine->TimeEventMngr.StopTimeEvent(entity, {}, te->Id);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_RepeatCurrentTimeEvent(BaseEngine* engine, timespan repeat)
{
    if (auto&& [entity, te] = engine->TimeEventMngr.GetCurTimeEvent(); entity != nullptr) {
        engine->TimeEventMngr.ModifyTimeEvent(engine, {}, te->Id, repeat, std::nullopt);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetCurrentTimeEventData(BaseEngine* engine, any_t data)
{
    if (auto&& [entity, te] = engine->TimeEventMngr.GetCurTimeEvent(); entity != nullptr) {
        engine->TimeEventMngr.ModifyTimeEvent(engine, {}, te->Id, std::nullopt, vector<any_t> {std::move(data)});
    }
}

///@ ExportMethod
FO_SCRIPT_API void Common_Game_SetCurrentTimeEventData(BaseEngine* engine, const vector<any_t>& data)
{
    if (auto&& [entity, te] = engine->TimeEventMngr.GetCurTimeEvent(); entity != nullptr) {
        engine->TimeEventMngr.ModifyTimeEvent(engine, {}, te->Id, std::nullopt, data);
    }
}
