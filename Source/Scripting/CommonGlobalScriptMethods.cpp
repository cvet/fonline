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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "EngineBase.h"
#include "GenericUtils.h"
#include "Log.h"
#include "ScriptSystem.h"
#include "StringUtils.h"
#include "WinApi-Include.h"

#include "sha1.h"
#include "sha2.h"

// ReSharper disable CppInconsistentNaming

///# ...
///# param text ...
///@ ExportMethod
[[maybe_unused]] void Common_Game_Log([[maybe_unused]] FOEngineBase* engine, string_view text)
{
    WriteLog("{}", text);
}

///# ...
///# param resourcePath ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Common_Game_IsResourcePresent([[maybe_unused]] FOEngineBase* engine, string_view resourcePath)
{
    return !!engine->FileSys.ReadFile(resourcePath);
}

///# ...
///# param resourcePath ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Common_Game_ReadResource([[maybe_unused]] FOEngineBase* engine, string_view resourcePath)
{
    return engine->FileSys.ReadFileText(resourcePath);
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
    const auto str = string(text);
    return utf8::Decode(str.c_str(), &length);
}

///# ...
///# param ucs ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Common_Game_EncodeUTF8([[maybe_unused]] FOEngineBase* engine, uint ucs)
{
    char buf[4];
    const auto len = utf8::Encode(ucs, buf);
    return string(buf, len);
}

///# ...
///# param text ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Common_Game_SHA1([[maybe_unused]] FOEngineBase* engine, string_view text)
{
    SHA1_CTX ctx;
    _SHA1_Init(&ctx);
    _SHA1_Update(&ctx, reinterpret_cast<const uchar*>(text.data()), text.length());
    uchar digest[SHA1_DIGEST_SIZE];
    _SHA1_Final(&ctx, digest);

    const auto* nums = "0123456789abcdef";
    char hex_digest[SHA1_DIGEST_SIZE * 2];
    for (uint i = 0; i < sizeof(hex_digest); i++) {
        hex_digest[i] = nums[(i % 2) != 0u ? digest[i / 2] & 0xF : digest[i / 2] >> 4];
    }

    return string(hex_digest, sizeof(hex_digest));
}

///# ...
///# param text ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Common_Game_SHA2([[maybe_unused]] FOEngineBase* engine, string_view text)
{
    constexpr uint digest_size = 32;
    uchar digest[digest_size];
    sha256(reinterpret_cast<const uchar*>(text.data()), static_cast<uint>(text.length()), digest);

    const auto* nums = "0123456789abcdef";
    char hex_digest[digest_size * 2];
    for (uint i = 0; i < sizeof(hex_digest); i++) {
        hex_digest[i] = nums[(i % 2) != 0u ? digest[i / 2] & 0xF : digest[i / 2] >> 4];
    }
    return {hex_digest, sizeof(hex_digest)};
}

///# ...
///# param link ...
///@ ExportMethod
[[maybe_unused]] void Common_Game_OpenLink([[maybe_unused]] FOEngineBase* engine, string_view link)
{
#if FO_WINDOWS
#if !FO_UWP
    ::ShellExecuteW(nullptr, L"open", _str(link).toWideChar().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
#elif !FO_IOS
    int r = system((string("xdg-open ") + string(link)).c_str());
    UNUSED_VARIABLE(r); // Supress compiler warning
#endif
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
[[maybe_unused]] uint Common_Game_GetDistance([[maybe_unused]] FOEngineBase* engine, ushort hx1, ushort hy1, ushort hx2, ushort hy2)
{
    return engine->Geometry.DistGame(hx1, hy1, hx2, hy2);
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uchar Common_Game_GetDirection([[maybe_unused]] FOEngineBase* engine, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy)
{
    return engine->Geometry.GetFarDir(fromHx, fromHy, toHx, toHy);
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param offset ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uchar Common_Game_GetDirection([[maybe_unused]] FOEngineBase* engine, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy, float offset)
{
    return engine->Geometry.GetFarDir(fromHx, fromHy, toHx, toHy, offset);
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] short Common_Game_GetDirAngle([[maybe_unused]] FOEngineBase* engine, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy)
{
    return static_cast<short>(engine->Geometry.GetDirAngle(fromHx, fromHy, toHx, toHy));
}

///# ...
///# param fromX ...
///# param fromY ...
///# param toX ...
///# param toY ...
///# return ...
///@ ExportMethod
[[maybe_unused]] short Common_Game_GetLineDirAngle([[maybe_unused]] FOEngineBase* engine, int fromX, int fromY, int toX, int toY)
{
    return static_cast<short>(engine->Geometry.GetLineDirAngle(fromX, fromY, toX, toY));
}

///# ...
///# param dirAngle ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uchar Common_Game_AngleToDir([[maybe_unused]] FOEngineBase* engine, short dirAngle)
{
    // Todo: quad tiles fix
    return static_cast<uchar>(dirAngle / 60);
}

///# ...
///# param dir ...
///# return ...
///@ ExportMethod
[[maybe_unused]] short Common_Game_DirToAngle([[maybe_unused]] FOEngineBase* engine, uchar dir)
{
    // Todo: quad tiles fix
    return static_cast<short>(static_cast<int>(dir) * 60 + 30);
}

///# ...
///# param dirAngle ...
///# param clockwise ...
///# param step ...
///# return ...
///@ ExportMethod
[[maybe_unused]] short Common_Game_RotateDirAngle([[maybe_unused]] FOEngineBase* engine, short dirAngle, bool clockwise, short step)
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

    return static_cast<short>(rotated);
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param ox ...
///# param oy ...
///@ ExportMethod
[[maybe_unused]] void Common_Game_GetHexInterval([[maybe_unused]] FOEngineBase* engine, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy, int& ox, int& oy)
{
    std::tie(ox, oy) = engine->Geometry.GetHexInterval(fromHx, fromHy, toHx, toHy);
}
