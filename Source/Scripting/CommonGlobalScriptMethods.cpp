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

#include "Common.h"

#include "GenericUtils.h"
#include "Log.h"
#include "ScriptSystem.h"
#include "StringUtils.h"
#include "WinApi-Include.h"

#include "sha1.h"
#include "sha2.h"

// ReSharper disable CppInconsistentNaming

///# ...
///# param condition ...
///@ ExportMethod AngelScriptOnly
[[maybe_unused]] void Common_Global_Assert([[maybe_unused]] void* dummy, bool condition)
{
    if (!condition) {
        throw ScriptException("Assertion failed");
    }
}

///# ...
///# param message ...
///@ ExportMethod AngelScriptOnly
[[maybe_unused]] void Common_Global_ThrowException([[maybe_unused]] void* dummy, string_view message)
{
    throw ScriptException(message);
}

///# ...
///# param time ...
///@ ExportMethod AngelScriptOnly
[[maybe_unused]] void Common_Global_Yield([[maybe_unused]] void* dummy, uint time)
{
    // Todo: fix script system
    // Script::SuspendCurrentContext(time);
}

///# ...
///# param text ...
///# param result ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Common_Global_StrToInt([[maybe_unused]] void* dummy, string_view text, int& result)
{
    if (!_str(text).isNumber()) {
        return false;
    }
    result = _str(text).toInt();
    return true;
}

///# ...
///# param text ...
///# param result ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Common_Global_StrToFloat([[maybe_unused]] void* dummy, string_view text, float& result)
{
    if (!_str(text).isNumber()) {
        return false;
    }
    result = _str(text).toFloat();
    return true;
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

    STARTUPINFOW si;
    std::memset(&si, 0, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.hStdError = out_write;
    si.hStdOutput = out_write;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi;
    std::memset(&pi, 0, sizeof(PROCESS_INFORMATION));

    auto* cmd_line = _wcsdup(_str(std::move(command)).toWideChar().c_str());
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
[[maybe_unused]] int Common_Global_SystemCall([[maybe_unused]] void* dummy, string_view command)
{
    auto prefix = command.substr(0, command.find(' '));
    return SystemCall(command, [&prefix](string_view line) { WriteLog("{} : {}\n", prefix, line); });
}

///# ...
///# param command ...
///# param output ...
///# return ...
///@ ExportMethod
[[maybe_unused]] int Common_Global_SystemCallExt([[maybe_unused]] void* dummy, string_view command, string& output)
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
[[maybe_unused]] int Common_Global_Random([[maybe_unused]] void* dummy, int minValue, int maxValue)
{
    return GenericUtils::Random(minValue, maxValue);
}

///# ...
///# param text ...
///@ ExportMethod
[[maybe_unused]] void Common_Global_Log([[maybe_unused]] void* dummy, string_view text)
{
    WriteLog("{}\n", text);
}

///# ...
///# param text ...
///# return ...
///@ ExportMethod
[[maybe_unused]] hash Common_Global_GetStrHash([[maybe_unused]] void* dummy, string_view text)
{
    if (text.empty()) {
        return 0;
    }
    return _str(text).toHash();
}

///# ...
///# param value ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Common_Global_GetHashStr([[maybe_unused]] void* dummy, hash value)
{
    return _str().parseHash(value).str();
}

///# ...
///# param text ...
///# param length ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Common_Global_DecodeUTF8([[maybe_unused]] void* dummy, string_view text, uint& length)
{
    const auto str = string(text);
    return utf8::Decode(str.c_str(), &length);
}

///# ...
///# param ucs ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Common_Global_EncodeUTF8([[maybe_unused]] void* dummy, uint ucs)
{
    char buf[4];
    const auto len = utf8::Encode(ucs, buf);
    return string(buf, len);
}

///# ...
///# param text ...
///# return ...
///@ ExportMethod
[[maybe_unused]] string Common_Global_SHA1([[maybe_unused]] void* dummy, string_view text)
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
[[maybe_unused]] string Common_Global_SHA2([[maybe_unused]] void* dummy, string_view text)
{
    const uint digest_size = 32;
    uchar digest[digest_size];
    sha256(reinterpret_cast<const uchar*>(text.data()), static_cast<uint>(text.length()), digest);

    const auto* nums = "0123456789abcdef";
    char hex_digest[digest_size * 2];
    for (uint i = 0; i < sizeof(hex_digest); i++) {
        hex_digest[i] = nums[(i % 2) != 0u ? digest[i / 2] & 0xF : digest[i / 2] >> 4];
    }
    return string(hex_digest, sizeof(hex_digest));
}

///# ...
///# param link ...
///@ ExportMethod
[[maybe_unused]] void Common_Global_OpenLink([[maybe_unused]] void* dummy, string_view link)
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
///# param pid ...
///# param props ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Entity* Common_Global_GetProtoItem([[maybe_unused]] void* dummy, hash pid, const map<int, int>& props)
{
#if 0
    const auto* proto = ProtoMngr.GetProtoItem( pid );
    if( !proto ) {
        return nullptr;
    }

    Item* item = new Item( 0, proto );
    if( props )
    {
        for( asUINT i = 0; i < props->GetSize(); i++ )
        {
            if( !Properties::SetValueAsIntProps( (Properties*) &item->Props, *(int*) props->GetKey( i ), *(int*) props->GetValue( i ) ) )
            {
                item->Release();
                return nullptr;
            }
        }
    }
    return item;
#endif
    return nullptr;
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Common_Global_GetUnixTime([[maybe_unused]] void* dummy)
{
    return static_cast<uint>(time(nullptr));
}
