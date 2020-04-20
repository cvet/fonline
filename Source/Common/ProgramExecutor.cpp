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

#include "ProgramExecutor.h"
#include "StringUtils.h"
#include "WinApi_Include.h"

ProgramExecutor::ProgramExecutor(const string& path, const string& name, const StrVec& args) :
    programPath {path}, programName {name}, programArgs {args}, returnOutput {}
{
    returnOutput = std::async(std::launch::async, &ProgramExecutor::ExecuteAsync, this);
}

ProgramExecutor::~ProgramExecutor()
{
    returnOutput.wait();
}

string ProgramExecutor::GetResult()
{
    return returnOutput.get();
}

string ProgramExecutor::ExecuteAsync()
{
    string command = _str("{}/{}", programPath, programName).formatPath();
    for (const string& arg : programArgs)
        command += " " + arg;

#if defined(FO_WINDOWS) && !defined(WINRT)
    HANDLE out_read = nullptr;
    HANDLE out_write = nullptr;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;
    if (!CreatePipe(&out_read, &out_write, &sa, 0))
        throw GenericException("Can't create pipe", command);

    if (!SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0))
    {
        CloseHandle(out_read);
        CloseHandle(out_write);
        throw GenericException("Can't set handle information", command);
    }

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.hStdError = out_write;
    si.hStdOutput = out_write;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
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
        throw GenericException("Can't create process", command);
    }

    string log;
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        CharVec log_buf(2048);
        DWORD bytes;
        while (PeekNamedPipe(out_read, nullptr, 0, nullptr, &bytes, nullptr) && bytes > 0)
        {
            size_t log_buf_min_size = static_cast<size_t>(bytes) * 2;
            if (log_buf_min_size > log_buf.size())
                log_buf.resize(log_buf_min_size);

            if (ReadFile(out_read, &log_buf[0], (DWORD)log_buf.size(), &bytes, nullptr))
                log.append(&log_buf[0], bytes);
        }

        if (WaitForSingleObject(pi.hProcess, 0) != WAIT_TIMEOUT)
            break;
    }

    log = _str(log).replace('\r', '\n', '\n').trim();

    DWORD exit_code = 42;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(out_read);
    CloseHandle(out_write);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    if (exit_code)
        throw GenericException("Program execution failed", command, exit_code, log);

    return log;

#elif !defined(FO_WINDOWS) && !defined(FO_WEB)
    FILE* in = popen(command.c_str(), "r");
    if (!in)
        throw GenericException("Failed to call popen", command);

    string log;
    char buf[2048];
    while (fgets(buf, sizeof(buf), in))
        log += buf;

    log = _str(log).replace('\r', '\n', '\n').trim();

    int exit_code = pclose(in);
    if (exit_code)
        throw GenericException("Program execution failed", command, exit_code, log);

    return log;

#else
    throw GenericException("Unsupported OS", command);
#endif
}
