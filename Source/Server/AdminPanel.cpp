#include "AdminPanel.h"
#include "Log.h"
#include "NetBuffer.h"
#include "Server.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Testing.h"
#include "Timer.h"
#include "WinApi_Include.h"

#ifndef FO_WINDOWS
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

#define MAX_SESSIONS (10)

struct Session
{
    int RefCount {};
    SOCKET Sock {};
    sockaddr_in From {};
    DateTimeStamp StartWork {};
    bool Authorized {};
};

static void AdminManager(FOServer* server, ushort port);
static void AdminWork(FOServer* server, Session* session);

void InitAdminManager(FOServer* server, ushort port)
{
    std::thread(AdminManager, server, port).detach();
}

static void AdminManager(FOServer* server, ushort port)
{
#ifdef FO_WINDOWS
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa))
    {
        WriteLog("WSAStartup fail on creation listen socket for admin manager.\n");
        return;
    }
#endif

    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET)
    {
        WriteLog("Can't create listen socket for admin manager.\n");
        return;
    }

    const int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_sock, (sockaddr*)&sin, sizeof(sin)) == SOCKET_ERROR)
    {
        WriteLog("Can't bind listen socket for admin manager.\n");
        closesocket(listen_sock);
        return;
    }

    if (listen(listen_sock, SOMAXCONN) == SOCKET_ERROR)
    {
        WriteLog("Can't listen listen socket for admin manager.\n");
        closesocket(listen_sock);
        return;
    }

    // Accept clients
    vector<Session*> sessions {};
    while (true)
    {
        // Wait connection
        timeval tv = {1, 0};
        fd_set sock_set;
        FD_ZERO(&sock_set);
        FD_SET(listen_sock, &sock_set);
        if (select((int)listen_sock + 1, &sock_set, nullptr, nullptr, &tv) > 0)
        {
            sockaddr_in from;
#ifdef FO_WINDOWS
            int len = sizeof(from);
#else
            socklen_t len = sizeof(from);
#endif
            SOCKET sock = accept(listen_sock, (sockaddr*)&from, &len);
            if (sock != INVALID_SOCKET)
            {
                // Found already connected from this IP
                bool refuse = false;
                for (auto it = sessions.begin(); it != sessions.end(); ++it)
                {
                    Session* s = *it;
                    if (s->From.sin_addr.s_addr == from.sin_addr.s_addr)
                    {
                        refuse = true;
                        break;
                    }
                }
                if (refuse || sessions.size() > MAX_SESSIONS)
                {
                    shutdown(sock, SD_BOTH);
                    closesocket(sock);
                }

                // Add new session
                if (!refuse)
                {
                    Session* s = new Session();
                    s->RefCount = 2;
                    s->Sock = sock;
                    s->From = from;
                    Timer::GetCurrentDateTime(s->StartWork);
                    s->Authorized = false;
                    sessions.push_back(s);
                    std::thread(AdminWork, server, s).detach();
                }
            }
        }

        // Manage sessions
        if (!sessions.empty())
        {
            DateTimeStamp cur_dt;
            Timer::GetCurrentDateTime(cur_dt);
            for (auto it = sessions.begin(); it != sessions.end();)
            {
                Session* s = *it;
                bool erase = false;

                // Erase closed sessions
                if (s->Sock == INVALID_SOCKET)
                    erase = true;

                // Drop long not authorized connections
                if (!s->Authorized && Timer::GetTimeDifference(cur_dt, s->StartWork) > 60) // 1 minute
                    erase = true;

                // Erase
                if (erase)
                {
                    if (s->Sock != INVALID_SOCKET)
                        shutdown(s->Sock, SD_BOTH);
                    if (--s->RefCount == 0)
                        delete s;
                    it = sessions.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        // Sleep to prevent panel DDOS or keys brute force
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

#define ADMIN_PREFIX "Admin panel ({}): "
#define ADMIN_LOG(format, ...) \
    do \
    { \
        WriteLog(ADMIN_PREFIX format, admin_name, ##__VA_ARGS__); \
    } while (0)

/*
   string buf = _str(format, ## __VA_ARGS__);                       \
        uint   buf_len = (uint) buf.length() + 1;                                 \
        if( send( s->Sock, buf.c_str(), buf_len, 0 ) != (int) buf_len )           \
        {                                                                         \
            WriteLog( ADMIN_PREFIX "Send data fail, disconnect.\n", admin_name ); \
            goto label_Finish;                                                    \
        }                                                                         \
 */

static void AdminWork(FOServer* server, Session* session)
{
    // Data
    string admin_name = "Not authorized";

    // Welcome string
    string welcome = "Welcome to FOnline admin panel.\nEnter access key: ";
    int welcome_len = (int)welcome.length() + 1;
    if (send(session->Sock, welcome.c_str(), welcome_len, 0) != welcome_len)
    {
        WriteLog("Admin connection first send fail, disconnect.\n");
        goto label_Finish;
    }

    // Commands loop
    while (true)
    {
        // Get command
        char cmd_raw[MAX_FOTEXT];
        memzero(cmd_raw, sizeof(cmd_raw));
        int len = recv(session->Sock, cmd_raw, sizeof(cmd_raw), 0);
        if (len <= 0 || len == MAX_FOTEXT)
        {
            if (!len)
                WriteLog(ADMIN_PREFIX "Socket closed, disconnect.\n", admin_name);
            else
                WriteLog(ADMIN_PREFIX "Socket error, disconnect.\n", admin_name);
            goto label_Finish;
        }
        if (len > 200)
            len = 200;
        cmd_raw[len] = 0;
        string cmd = _str(cmd_raw).trim();

        // Authorization
        if (!session->Authorized)
        {
            StrVec client, tester, moder, admin, admin_names;
            server->GetAccesses(client, tester, moder, admin, admin_names);
            int pos = -1;
            for (size_t i = 0, j = admin.size(); i < j; i++)
            {
                if (admin[i] == cmd)
                {
                    pos = (int)i;
                    break;
                }
            }
            if (pos != -1)
            {
                if (pos < (int)admin_names.size())
                    admin_name = admin_names[pos];
                else
                    admin_name = _str("{}", pos);

                session->Authorized = true;
                ADMIN_LOG("Authorized for admin '{}', IP '{}'.\n", admin_name, inet_ntoa(session->From.sin_addr));
                continue;
            }
            else
            {
                WriteLog("Wrong access key entered in admin panel from IP '{}', disconnect.\n",
                    inet_ntoa(session->From.sin_addr));
                string failstr = "Wrong access key!\n";
                send(session->Sock, failstr.c_str(), (int)failstr.length() + 1, 0);
                goto label_Finish;
            }
        }

        // Process commands
        if (cmd == "exit")
        {
            ADMIN_LOG("Disconnect from admin panel.\n");
            goto label_Finish;
        }
        else if (cmd == "kill")
        {
            ADMIN_LOG("Kill whole process.\n");
            exit(0);
        }
        else if (_str(cmd).startsWith("log "))
        {
            if (cmd.substr(4) == "disable")
            {
                LogToFile(cmd.substr(4));
                ADMIN_LOG("Logging to file '{}'.\n", cmd.substr(4));
            }
            else
            {
                LogToFile("");
                ADMIN_LOG("Logging disabled.\n");
            }
        }
        else if (cmd == "stop")
        {
            if (server && server->Starting())
                ADMIN_LOG("Server starting, wait.\n");
            else if (server && server->Stopped())
                ADMIN_LOG("Server already stopped.\n");
            else if (server && server->Stopping())
                ADMIN_LOG("Server already stopping.\n");
            else
            {
                if (!GameOpt.Quit)
                {
                    ADMIN_LOG("Stopping server.\n");
                    GameOpt.Quit = true;
                }
            }
        }
        else if (cmd == "state")
        {
            if (server && server->Starting())
                ADMIN_LOG("Server starting.\n");
            else if (server && server->Started())
                ADMIN_LOG("Server started.\n");
            else if (server && server->Stopping())
                ADMIN_LOG("Server stopping.\n");
            else if (server && server->Stopped())
                ADMIN_LOG("Server stopped.\n");
            else
                ADMIN_LOG("Unknown state.\n");
        }
        else if (!cmd.empty() && cmd[0] == '~')
        {
            if (server && server->Started())
            {
                bool send_fail = false;
                LogFunc func = [&admin_name, &session, &send_fail](const string& str) {
                    string buf = str;
                    if (buf.empty() || buf.back() != '\n')
                        buf += "\n";

                    if (!send_fail &&
                        send(session->Sock, buf.c_str(), (int)buf.length() + 1, 0) != (int)buf.length() + 1)
                    {
                        WriteLog(ADMIN_PREFIX "Send data fail, disconnect.\n", admin_name);
                        send_fail = true;
                    }
                };

                NetBuffer buf;
                PackNetCommand(cmd.substr(1), &buf, func, "");
                if (!buf.IsEmpty())
                {
                    uint msg;
                    buf >> msg;
                    WriteLog(ADMIN_PREFIX "Execute command '{}'.\n", admin_name, cmd);
                    server->Process_Command(buf, func, nullptr, admin_name);
                }

                if (send_fail)
                    goto label_Finish;
            }
            else
            {
                ADMIN_LOG("Can't run command for not started server.\n");
            }
        }
        else if (!cmd.empty())
        {
            ADMIN_LOG("Unknown command '{}'.\n", cmd);
        }
    }

label_Finish:
    shutdown(session->Sock, SD_BOTH);
    closesocket(session->Sock);
    session->Sock = INVALID_SOCKET;
    if (--session->RefCount == 0)
        delete session;
}
