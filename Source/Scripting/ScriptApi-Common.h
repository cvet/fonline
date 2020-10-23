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

#if FO_API_COMMON_IMPL
#include "GenericUtils.h"
#include "StringUtils.h"
#include "Timer.h"
#include "WinApi-Include.h"

#include "sha1.h"
#include "sha2.h"

class MapSprite final
{
public:
    void AddRef() const { ++RefCount; }
    void Release() const
    {
        if (--RefCount == 0)
            delete this;
    }
    static MapSprite* Factory() { return new MapSprite(); }

    mutable int RefCount {1};
    bool Valid {};
    uint SprId {};
    ushort HexX {};
    ushort HexY {};
    hash ProtoId {};
    int FrameIndex {};
    int OffsX {};
    int OffsY {};
    bool IsFlat {};
    bool NoLight {};
    int DrawOrder {};
    int DrawOrderHyOffset {};
    int Corner {};
    bool DisableEgg {};
    uint Color {};
    uint ContourColor {};
    bool IsTweakOffs {};
    short TweakOffsX {};
    short TweakOffsY {};
    bool IsTweakAlpha {};
    uchar TweakAlpha {};
};
#endif

// Settings
#define SETTING(type, name, ...) FO_API_SETTING(type, name)
#define VAR_SETTING(type, name, ...) FO_API_SETTING(type, name)
#define SETTING_GROUP(name, ...)
#define SETTING_GROUP_END()
#include "Settings-Include.h"

#if FO_API_ANGELSCRIPT_ONLY
/*******************************************************************************
 * ...
 *
 * @param condition ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(Assert, FO_API_RET(void), FO_API_ARG(bool, condition))
#if FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(bool, condition))
{
    if (!condition) {
        throw ScriptException("Assertion failed");
    }
}
FO_API_EPILOG()
#endif
#endif

#if FO_API_ANGELSCRIPT_ONLY
/*******************************************************************************
 * ...
 *
 * @param message ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(ThrowException, FO_API_RET(void), FO_API_ARG(string, message))
#if FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, message))
{
    throw ScriptException(message);
}
FO_API_EPILOG()
#endif
#endif

/*******************************************************************************
 * ...
 *
 * @param minValue ...
 * @param maxValue ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(Random, FO_API_RET(int), FO_API_ARG(int, minValue), FO_API_ARG(int, maxValue))
#if FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(int, minValue) FO_API_ARG_MARSHAL(int, maxValue))
{
    FO_API_RETURN(GenericUtils::Random(minValue, maxValue));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param text ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(Log, FO_API_RET(void), FO_API_ARG(string, text))
#if FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text))
{
    WriteLog("{}\n", text);
}
FO_API_EPILOG()
#endif

#if FO_API_ANGELSCRIPT_ONLY
/*******************************************************************************
 * ...
 *
 * @param text ...
 * @param result ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(StrToInt, FO_API_RET(bool), FO_API_ARG(string, text), FO_API_ARG_REF(int, result))
#if FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text) FO_API_ARG_REF_MARSHAL(int, result))
{
    if (!_str(text).isNumber()) {
        FO_API_RETURN(false);
    }
    result = _str(text).toInt();
    FO_API_RETURN(true);
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_ANGELSCRIPT_ONLY
/*******************************************************************************
 * ...
 *
 * @param text ...
 * @param result ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(StrToFloat, FO_API_RET(bool), FO_API_ARG(string, text), FO_API_ARG_REF(float, result))
#if FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text) FO_API_ARG_REF_MARSHAL(float, result))
{
    if (!_str(text).isNumber()) {
        FO_API_RETURN(false);
    }
    result = _str(text).toFloat();
    FO_API_RETURN(true);
}
FO_API_EPILOG(0)
#endif
#endif

/*******************************************************************************
 * ...
 *
 * @param hx1 ...
 * @param hy1 ...
 * @param hx2 ...
 * @param hy2 ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(GetHexDistance, FO_API_RET(int), FO_API_ARG(ushort, hx1), FO_API_ARG(ushort, hy1), FO_API_ARG(ushort, hx2), FO_API_ARG(ushort, hy2))
#if FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx1) FO_API_ARG_MARSHAL(ushort, hy1) FO_API_ARG_MARSHAL(ushort, hx2) FO_API_ARG_MARSHAL(ushort, hy2))
{
    FO_API_RETURN(_common->GeomHelper.DistGame(hx1, hy1, hx2, hy2));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param fromHx ...
 * @param fromHy ...
 * @param toHx ...
 * @param toHy ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(GetHexDir, FO_API_RET(uchar), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy), FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy))
#if FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_MARSHAL(ushort, toHx) FO_API_ARG_MARSHAL(ushort, toHy))
{
    FO_API_RETURN(_common->GeomHelper.GetFarDir(fromHx, fromHy, toHx, toHy));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param fromHx ...
 * @param fromHy ...
 * @param toHx ...
 * @param toHy ...
 * @param offset ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(GetHexDirWithOffset, FO_API_RET(uchar), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy), FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(float, offset))
#if FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_MARSHAL(ushort, toHx) FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(float, offset))
{
    FO_API_RETURN(_common->GeomHelper.GetFarDir(fromHx, fromHy, toHx, toHy, offset));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(GetTick, FO_API_RET(uint))
#if FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(_common->GameTime.FrameTick());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param text ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(GetStrHash, FO_API_RET(hash), FO_API_ARG(string, text))
#if FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text))
{
    if (text.empty()) {
        FO_API_RETURN(0);
    }
    FO_API_RETURN(_str(text).toHash());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param value ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(GetHashStr, FO_API_RET(string), FO_API_ARG(hash, value))
#if FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, value))
{
    FO_API_RETURN(_str().parseHash(value).str());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param text ...
 * @param length ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(DecodeUTF8, FO_API_RET(uint), FO_API_ARG(string, text), FO_API_ARG_REF(uint, length))
#if FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text) FO_API_ARG_REF_MARSHAL(uint, length))
{
    FO_API_RETURN(utf8::Decode(text.c_str(), &length));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param ucs ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(EncodeUTF8, FO_API_RET(string), FO_API_ARG(uint, ucs))
#if FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, ucs))
{
    char buf[4];
    const auto len = utf8::Encode(ucs, buf);
    FO_API_RETURN(string(buf, len));
}
FO_API_EPILOG(0)
#endif

#if FO_API_ANGELSCRIPT_ONLY
/*******************************************************************************
 * ...
 *
 * @param time ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(Yield, FO_API_RET(void), FO_API_ARG(uint, time))
#if FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, time))
{
    // Todo: fix script system
    // Script::SuspendCurrentContext(time);
}
FO_API_EPILOG()
#endif
#endif

/*******************************************************************************
 * ...
 *
 * @param text ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(SHA1, FO_API_RET(string), FO_API_ARG(string, text))
#if FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text))
{
    SHA1_CTX ctx;
    _SHA1_Init(&ctx);
    _SHA1_Update(&ctx, reinterpret_cast<const uchar*>(text.c_str()), text.length());
    uchar digest[SHA1_DIGEST_SIZE];
    _SHA1_Final(&ctx, digest);

    const auto* nums = "0123456789abcdef";
    char hex_digest[SHA1_DIGEST_SIZE * 2];
    for (uint i = 0; i < sizeof(hex_digest); i++) {
        hex_digest[i] = nums[(i % 2) != 0u ? digest[i / 2] & 0xF : digest[i / 2] >> 4];
    }

    FO_API_RETURN(string(hex_digest, sizeof(hex_digest)));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param text ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(SHA2, FO_API_RET(string), FO_API_ARG(string, text))
#if FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text))
{
    const uint digest_size = 32;
    uchar digest[digest_size];
    sha256(reinterpret_cast<const uchar*>(text.c_str()), static_cast<uint>(text.length()), digest);

    const auto* nums = "0123456789abcdef";
    char hex_digest[digest_size * 2];
    for (uint i = 0; i < sizeof(hex_digest); i++) {
        hex_digest[i] = nums[(i % 2) != 0u ? digest[i / 2] & 0xF : digest[i / 2] >> 4];
    }
    FO_API_RETURN(string(hex_digest, sizeof(hex_digest)));
}
FO_API_EPILOG(0)
#endif

#if FO_API_ANGELSCRIPT_ONLY
#if FO_API_GLOBAL_COMMON_FUNC_IMPL
static void PrintLog(string& log, bool last_call, const std::function<void(const string&)>& log_callback)
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

static auto SystemCall(string command, const std::function<void(const string&)>& log_callback) -> int
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
            char buf[TEMP_BUF_SIZE];
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
    FILE* in = popen(command.c_str(), "r");
    if (!in) {
        return -1;
    }

    string log;
    char buf[TEMP_BUF_SIZE];
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
#endif
/*******************************************************************************
 * ...
 *
 * @param command ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(SystemCall, FO_API_RET(int), FO_API_ARG(string, command))
#if FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, command))
{
    auto prefix = command.substr(0, command.find(' '));
    const auto ret_code = SystemCall(command, [&prefix](const string& line) { WriteLog("{} : {}\n", prefix, line); });
    FO_API_RETURN(ret_code);
}
FO_API_EPILOG(0)
#endif
#endif

#if FO_API_ANGELSCRIPT_ONLY
/*******************************************************************************
 * ...
 *
 * @param command ...
 * @param output ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(SystemCallExt, FO_API_RET(int), FO_API_ARG(string, command), FO_API_ARG_REF(string, output))
#if FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, command) FO_API_ARG_REF_MARSHAL(string, output))
{
    output = "";

    FO_API_RETURN(SystemCall(command, [&output](const string& line) {
        if (!output.empty()) {
            output += "\n";
        }
        output += line;
    }));
}
FO_API_EPILOG(0)
#endif
#endif

/*******************************************************************************
 * ...
 *
 * @param link ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(OpenLink, FO_API_RET(void), FO_API_ARG(string, link))
#if FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, link))
{
#if FO_WINDOWS
#if !FO_UWP
    ::ShellExecuteW(nullptr, L"open", _str(link).toWideChar().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
#elif !FO_IOS
    int r = system((string("xdg-open ") + link).c_str());
    UNUSED_VARIABLE(r); // Supress compiler warning
#endif
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param pid ...
 * @param props ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(GetProtoItem, FO_API_RET_OBJ(Entity), FO_API_ARG(hash, pid), FO_API_ARG_DICT(int, int, props))
#if FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid) FO_API_ARG_DICT_MARSHAL(int, int, props))
{
#if 0
    const auto* proto = ProtoMngr.GetProtoItem( pid );
    if( !proto ) {
        FO_API_RETURN(nullptr);
    }

    Item* item = new Item( 0, proto );
    if( props )
    {
        for( asUINT i = 0; i < props->GetSize(); i++ )
        {
            if( !Properties::SetValueAsIntProps( (Properties*) &item->Props, *(int*) props->GetKey( i ), *(int*) props->GetValue( i ) ) )
            {
                item->Release();
                FO_API_RETURN(nullptr);
            }
        }
    }
    FO_API_RETURN(item);
#endif
    FO_API_RETURN(static_cast<Entity*>(nullptr));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(GetUnixTime, FO_API_RET(uint))
#if FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(static_cast<uint>(time(nullptr)));
}
FO_API_EPILOG(0)
#endif
