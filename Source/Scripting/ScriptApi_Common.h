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

#ifdef FO_API_COMMON_IMPL
#include "GenericUtils.h"
#include "StringUtils.h"
#include "Timer.h"
#include "Version_Include.h"
#include "WinApi_Include.h"

#include "sha1.h"
#include "sha2.h"

class MapSprite : public NonCopyable
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

/*******************************************************************************
 * ...
 *
 * @param condition ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(Assert, FO_API_RET(void), FO_API_ARG(bool, condition))
#ifdef FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(bool, condition))
{
    if (!condition)
        throw ScriptException("Assertion failed");
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param message ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(ThrowException, FO_API_RET(void), FO_API_ARG(string, message))
#ifdef FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, message))
{
    throw ScriptException(message);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param min ...
 * @param max ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(Random, FO_API_RET(int), FO_API_ARG(int, minValue), FO_API_ARG(int, maxValue))
#ifdef FO_API_GLOBAL_COMMON_FUNC_IMPL
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
#ifdef FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text))
{
    WriteLog("{}\n", text);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param text ...
 * @param result ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(StrToInt, FO_API_RET(bool), FO_API_ARG(string, text), FO_API_ARG_REF(int, result))
#ifdef FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text) FO_API_ARG_REF_MARSHAL(int, result))
{
    if (!_str(text).isNumber())
        FO_API_RETURN(false);
    result = _str(text).toInt();
    FO_API_RETURN(true);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param text ...
 * @param result ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(StrToFloat, FO_API_RET(bool), FO_API_ARG(string, text), FO_API_ARG_REF(float, result))
#ifdef FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text) FO_API_ARG_REF_MARSHAL(float, result))
{
    if (!_str(text).isNumber())
        FO_API_RETURN(false);
    result = _str(text).toFloat();
    FO_API_RETURN(true);
}
FO_API_EPILOG(0)
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
FO_API_GLOBAL_COMMON_FUNC(GetDistantion, FO_API_RET(uint), FO_API_ARG(ushort, hx1), FO_API_ARG(ushort, hy1),
    FO_API_ARG(ushort, hx2), FO_API_ARG(ushort, hy2))
#ifdef FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, hx1) FO_API_ARG_MARSHAL(ushort, hy1) FO_API_ARG_MARSHAL(ushort, hx2)
        FO_API_ARG_MARSHAL(ushort, hy2))
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
FO_API_GLOBAL_COMMON_FUNC(GetDirection, FO_API_RET(uchar), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy),
    FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy))
#ifdef FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_MARSHAL(ushort, toHx)
        FO_API_ARG_MARSHAL(ushort, toHy))
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
FO_API_GLOBAL_COMMON_FUNC(GetOffsetDir, FO_API_RET(uchar), FO_API_ARG(ushort, fromHx), FO_API_ARG(ushort, fromHy),
    FO_API_ARG(ushort, toHx), FO_API_ARG(ushort, toHy), FO_API_ARG(float, offset))
#ifdef FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(ushort, fromHx) FO_API_ARG_MARSHAL(ushort, fromHy) FO_API_ARG_MARSHAL(ushort, toHx)
        FO_API_ARG_MARSHAL(ushort, toHy) FO_API_ARG_MARSHAL(float, offset))
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
#ifdef FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN(Timer::FastTick());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param str ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(GetStrHash, FO_API_RET(hash), FO_API_ARG(string, text))
#ifdef FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text))
{
    if (text.empty())
        FO_API_RETURN(0);
    FO_API_RETURN(_str(text).toHash());
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param h ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(GetHashStr, FO_API_RET(string), FO_API_ARG(hash, h))
#ifdef FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, h))
{
    FO_API_RETURN(_str().parseHash(h).str());
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
#ifdef FO_API_GLOBAL_COMMON_FUNC_IMPL
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
#ifdef FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, ucs))
{
    char buf[4];
    uint len = utf8::Encode(ucs, buf);
    FO_API_RETURN(string(buf, len));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param time ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(Yield0, FO_API_RET(void), FO_API_ARG(uint, time))
#ifdef FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(uint, time))
{
    // Todo: fix script system
    // Script::SuspendCurrentContext(time);
}
FO_API_EPILOG()
#endif

/*******************************************************************************
 * ...
 *
 * @param text ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(SHA1, FO_API_RET(string), FO_API_ARG(string, text))
#ifdef FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text))
{
    SHA1_CTX ctx;
    _SHA1_Init(&ctx);
    _SHA1_Update(&ctx, (uchar*)text.c_str(), text.length());
    uchar digest[SHA1_DIGEST_SIZE];
    _SHA1_Final(&ctx, digest);

    static const char* nums = "0123456789abcdef";
    char hex_digest[SHA1_DIGEST_SIZE * 2];
    for (uint i = 0; i < sizeof(hex_digest); i++)
        hex_digest[i] = nums[i % 2 ? digest[i / 2] & 0xF : digest[i / 2] >> 4];
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
#ifdef FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, text))
{
    const uint digest_size = 32;
    uchar digest[digest_size];
    sha256((uchar*)text.c_str(), (uint)text.length(), digest);

    static const char* nums = "0123456789abcdef";
    char hex_digest[digest_size * 2];
    for (uint i = 0; i < sizeof(hex_digest); i++)
        hex_digest[i] = nums[i % 2 ? digest[i / 2] & 0xF : digest[i / 2] >> 4];
    FO_API_RETURN(string(hex_digest, sizeof(hex_digest)));
}
FO_API_EPILOG(0)
#endif

#ifdef FO_API_GLOBAL_COMMON_FUNC_IMPL
static void PrintLog(string& log, bool last_call, std::function<void(const string&)> log_callback)
{
    // Normalize new lines to \n
    while (true)
    {
        size_t pos = log.find("\r\n");
        if (pos != string::npos)
            log.replace(pos, 2, "\n");
        else
            break;
    }
    log.erase(std::remove(log.begin(), log.end(), '\r'), log.end());

    // Write own log
    while (true)
    {
        size_t pos = log.find('\n');
        if (pos == string::npos && last_call && !log.empty())
            pos = log.size();

        if (pos != string::npos)
        {
            log_callback(log.substr(0, pos));
            log.erase(0, pos + 1);
        }
        else
        {
            break;
        }
    }
}

static int SystemCall(string command, std::function<void(const string&)> log_callback)
{
#if defined(FO_WINDOWS) && !defined(WINRT)
    HANDLE out_read = nullptr;
    HANDLE out_write = nullptr;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;
    if (!CreatePipe(&out_read, &out_write, &sa, 0))
        return -1;
    if (!SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0))
    {
        CloseHandle(out_read);
        CloseHandle(out_write);
        return -1;
    }

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.hStdError = out_write;
    si.hStdOutput = out_write;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

    wchar_t* cmd_line = _wcsdup(_str(command).toWideChar().c_str());
    BOOL result = CreateProcessW(nullptr, cmd_line, nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi);
    SAFEDELA(cmd_line);
    if (!result)
    {
        CloseHandle(out_read);
        CloseHandle(out_write);
        return -1;
    }

    string log;
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        DWORD bytes;
        while (PeekNamedPipe(out_read, nullptr, 0, nullptr, &bytes, nullptr) && bytes > 0)
        {
            char buf[TEMP_BUF_SIZE];
            if (ReadFile(out_read, buf, sizeof(buf), &bytes, nullptr))
            {
                log.append(buf, bytes);
                PrintLog(log, false, log_callback);
            }
        }

        if (WaitForSingleObject(pi.hProcess, 0) != WAIT_TIMEOUT)
            break;
    }
    PrintLog(log, true, log_callback);

    DWORD retval;
    GetExitCodeProcess(pi.hProcess, &retval);
    CloseHandle(out_read);
    CloseHandle(out_write);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)retval;

#elif !defined(FO_WINDOWS) && !defined(FO_WEB)
    FILE* in = popen(command.c_str(), "r");
    if (!in)
        return -1;

    string log;
    char buf[TEMP_BUF_SIZE];
    while (fgets(buf, sizeof(buf), in))
    {
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
#ifdef FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, command))
{
    string prefix = command.substr(0, command.find(' '));
    FO_API_RETURN(SystemCall(command, [&prefix](const string& line) { WriteLog("{} : {}\n", prefix, line); }));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param command ...
 * @param output ...
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(SystemCallExt, FO_API_RET(int), FO_API_ARG(string, command), FO_API_ARG_REF(string, output))
#ifdef FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, command) FO_API_ARG_REF_MARSHAL(string, output))
{
    output = "";
    FO_API_RETURN(SystemCall(command, [&output](const string& line) {
        if (!output.empty())
            output += "\n";
        output += line;
    }));
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @param link ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(OpenLink, FO_API_RET(void), FO_API_ARG(string, link))
#ifdef FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(string, link))
{
#if defined(FO_WINDOWS)
#ifndef WINRT
    ::ShellExecuteW(nullptr, L"open", _str(link).toWideChar().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
#elif !defined(FO_IOS)
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
#ifdef FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG(FO_API_ARG_MARSHAL(hash, pid) FO_API_ARG_DICT_MARSHAL(int, int, props))
{
#if 0
    ProtoItem* proto = ProtoMngr.GetProtoItem( pid );
    if( !proto )
        FO_API_RETURN(nullptr);

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
    FO_API_RETURN((Entity*)nullptr);
}
FO_API_EPILOG(0)
#endif

/*******************************************************************************
 * ...
 *
 * @return ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(GetUnixTime, FO_API_RET(uint))
#ifdef FO_API_GLOBAL_COMMON_FUNC_IMPL
FO_API_PROLOG()
{
    FO_API_RETURN((uint)time(nullptr));
}
FO_API_EPILOG(0)
#endif
