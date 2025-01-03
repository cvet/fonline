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

#include "AdminPanel.h"

#if !FO_SINGLEPLAYER

#include "Application.h"
#include "Log.h"
#include "NetBuffer.h"
#include "NetCommand.h"
#include "Server.h"
#include "StringUtils.h"
#include "Timer.h"
#include "WinApi-Include.h"

#if !FO_WINDOWS
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#define SD_RECEIVE SHUT_RD
#define SD_SEND SHUT_WR
#define SD_BOTH SHUT_RDWR
#endif

static constexpr size_t MAX_SESSIONS = 10;

struct Session
{
    int RefCount {};
    SOCKET Sock {};
    sockaddr_in From {};
    DateTimeStamp StartWork {};
    bool Authorized {};
};

static void AdminManager(FOServer* server, uint16 port);
static void AdminWork(FOServer* server, Session* session);

void InitAdminManager(FOServer* server, uint16 port)
{
    STACK_TRACE_ENTRY();

    std::thread(AdminManager, server, port).detach();
}

static void AdminManager(FOServer* server, uint16 port)
{
    STACK_TRACE_ENTRY();

#if FO_WINDOWS
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        WriteLog("WSAStartup fail on creation listen socket for admin manager");
        return;
    }
#endif

#if FO_LINUX
    constexpr int sock_type = SOCK_STREAM | SOCK_CLOEXEC;
#else
    constexpr int sock_type = SOCK_STREAM;
#endif
    const auto listen_sock = socket(AF_INET, sock_type, 0);
    if (listen_sock == INVALID_SOCKET) {
        WriteLog("Can't create listen socket for admin manager");
        return;
    }

    constexpr auto opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
    sockaddr_in sin {};
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_sock, reinterpret_cast<sockaddr*>(&sin), sizeof(sin)) == SOCKET_ERROR) {
        WriteLog("Can't bind listen socket for admin manager");
        closesocket(listen_sock);
        return;
    }

    if (listen(listen_sock, SOMAXCONN) == SOCKET_ERROR) {
        WriteLog("Can't listen listen socket for admin manager");
        closesocket(listen_sock);
        return;
    }

    // Accept clients
    vector<Session*> sessions {};
    while (true) {
        // Wait connection
        timeval tv = {1, 0};
        fd_set sock_set;
        FD_ZERO(&sock_set);
        FD_SET(listen_sock, &sock_set);
        if (select(static_cast<int>(listen_sock) + 1, &sock_set, nullptr, nullptr, &tv) > 0) {
            sockaddr_in from {};
#if FO_WINDOWS
            int len = sizeof(from);
#else
            socklen_t len = sizeof(from);
#endif
            const auto sock = accept(listen_sock, reinterpret_cast<sockaddr*>(&from), &len);
            if (sock != INVALID_SOCKET) {
                // Find already connected from this IP
                auto refuse = false;
                for (const auto* s : sessions) {
                    if (s->From.sin_addr.s_addr == from.sin_addr.s_addr) {
                        refuse = true;
                        break;
                    }
                }
                if (refuse || sessions.size() > MAX_SESSIONS) {
                    shutdown(sock, SD_BOTH);
                    closesocket(sock);
                }

                // Add new session
                if (!refuse) {
                    auto* s = SafeAlloc::MakeRaw<Session>();
                    s->RefCount = 2;
                    s->Sock = sock;
                    s->From = from;
                    s->StartWork = Timer::GetCurrentDateTime();
                    s->Authorized = false;
                    sessions.push_back(s);
                    std::thread(AdminWork, server, s).detach();
                }
            }
        }

        // Manage sessions
        if (!sessions.empty()) {
            const auto cur_dt = Timer::GetCurrentDateTime();
            for (auto it = sessions.begin(); it != sessions.end();) {
                auto* s = *it;
                auto erase = false;

                // Erase closed sessions
                if (s->Sock == INVALID_SOCKET) {
                    erase = true;
                }

                // Drop long not authorized connections
                if (!s->Authorized && Timer::GetTimeDifference(cur_dt, s->StartWork) > 60) {
                    // 1 minute
                    erase = true;
                }

                // Erase
                if (erase) {
                    if (s->Sock != INVALID_SOCKET) {
                        shutdown(s->Sock, SD_BOTH);
                    }
                    if (--s->RefCount == 0) {
                        delete s;
                    }
                    it = sessions.erase(it);
                }
                else {
                    ++it;
                }
            }
        }

        // Sleep to prevent panel DDOS or keys brute force
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

static void AdminWork(FOServer* server, Session* session)
{
    STACK_TRACE_ENTRY();

    auto* event_unsubscriber = SafeAlloc::MakeRaw<EventUnsubscriber>();
    *event_unsubscriber += server->OnWillFinish += [&server, &event_unsubscriber]() {
        server = nullptr;
        event_unsubscriber->Unsubscribe();
    };

    // Data
    string admin_name = "Not authorized";

    // Welcome string
    const string welcome = "Welcome to FOnline admin panel.\nEnter access key: ";
    const auto welcome_len = static_cast<int>(welcome.length()) + 1;
    if (::send(session->Sock, welcome.c_str(), welcome_len, 0) != welcome_len) {
        WriteLog("Admin connection first send fail, disconnect");
        goto label_Finish;
    }

    // Commands loop
    while (server != nullptr) {
        // Get command
        char cmd_raw[1024] = {};
        auto len = ::recv(session->Sock, cmd_raw, sizeof(cmd_raw), 0);

        if (len <= 0 || len == 1024) {
            if (len == 0) {
                WriteLog("Admin panel ({}): Socket closed, disconnect", admin_name);
            }
            else {
                WriteLog("Admin panel ({}): Socket error, disconnect", admin_name);
            }

            goto label_Finish;
        }

        if (len > 200) {
            len = 200;
        }

        cmd_raw[len] = 0;
        string cmd = strex(cmd_raw).trim();

        // Authorization
        if (!session->Authorized) {
            auto pos = -1;
            for (size_t i = 0, j = server->Settings.AccessAdmin.size(); i < j; i++) {
                if (server->Settings.AccessAdmin[i] == cmd) {
                    pos = static_cast<int>(i);
                    break;
                }
            }

            if (pos != -1) {
                admin_name = strex("{}", pos);

                session->Authorized = true;
                WriteLog("Admin panel ({}): Authorized for admin '{}', IP '{}'", admin_name, admin_name, inet_ntoa(session->From.sin_addr));
                continue;
            }

            WriteLog("Wrong access key entered in admin panel from IP '{}', disconnect", inet_ntoa(session->From.sin_addr));
            const string failstr = "Wrong access key!\n";
            ::send(session->Sock, failstr.c_str(), static_cast<int>(failstr.length()) + 1, 0);
            goto label_Finish;
        }

        // Process commands
        if (cmd == "exit") {
            WriteLog("Admin panel ({}): Disconnect from admin panel", admin_name);
            goto label_Finish;
        }
        else if (cmd == "kill") {
            WriteLog("Admin panel ({}): Kill whole process", admin_name);
            ExitApp(true);
        }
        else if (cmd == "stop") {
            if (!server->IsStarted()) {
                WriteLog("Admin panel ({}): Server starting, wait", admin_name);
            }
            else {
                WriteLog("Admin panel ({}): Stopping server", admin_name);
                delete server;
            }
        }
        else if (cmd == "state") {
            if (!server->IsStarted()) {
                WriteLog("Admin panel ({}): Server starting", admin_name);
            }
            else {
                WriteLog("Admin panel ({}): Server working", admin_name);
            }
        }
        else if (!cmd.empty() && cmd[0] == '~') {
            if (server->IsStarted()) {
                auto send_fail = false;
                LogFunc func = [&admin_name, &session, &send_fail](string_view str) {
                    auto buf = string(str);
                    if (buf.empty() || buf.back() != '\n') {
                        buf += "\n";
                    }

                    if (!send_fail && send(session->Sock, buf.c_str(), static_cast<int>(buf.length()) + 1, 0) != static_cast<int>(buf.length()) + 1) {
                        WriteLog("Admin panel ({}): Send data fail, disconnect", admin_name);
                        send_fail = true;
                    }
                };

                auto buf = NetOutBuffer(server->Settings.NetBufferSize);
                PackNetCommand(cmd.substr(1), &buf, func, *server);
                if (!buf.IsEmpty()) {
                    // uint msg = 0;
                    // buf >> msg;
                    // WriteLog("Admin panel ({}): Execute command '{}'", admin_name, cmd);
                    // server->Process_Command(buf, func, nullptr, admin_name);
                }

                if (send_fail) {
                    goto label_Finish;
                }
            }
            else {
                WriteLog("Admin panel ({}): Can't run command for not started server", admin_name);
            }
        }
        else if (!cmd.empty()) {
            WriteLog("Admin panel ({}): Unknown command '{}'", admin_name, cmd);
        }
    }

label_Finish:
    shutdown(session->Sock, SD_BOTH);
    closesocket(session->Sock);
    session->Sock = INVALID_SOCKET;
    if (--session->RefCount == 0) {
        delete session;
    }
}

#else

void InitAdminManager(FOServer* server, uint16 port)
{
    STACK_TRACE_ENTRY();
}

#endif
