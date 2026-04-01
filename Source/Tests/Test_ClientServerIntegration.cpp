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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include <charconv>

#include "catch_amalgamated.hpp"

#include "AngelScriptScripting.h"
#include "Application.h"
#include "Baker.h"
#include "Client.h"
#include "DataSerialization.h"
#include "Server.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

namespace TestClientServerIntegration
{
    static std::atomic_uint16_t IntegrationTestPort {46000};

    static auto MakeServerScriptBinary(const FileSystem& metadata_resources) -> vector<uint8>
    {
        BakerServerEngine compiler_engine {metadata_resources};

        return BakerTests::CompileInlineScripts(&compiler_engine, "ClientServerServerScripts",
            {
                {"Scripts/ClientServerIntegrationServer.fos", R"(
#include "ClientServerIntegrationServerShared.fos"

namespace ClientServerIntegrationServer
{
    void UnitTestEntry()
    {
        UnitTestNoop();
    }
}
)"},
                {"Scripts/ClientServerIntegrationServerShared.fos", R"(
namespace ClientServerIntegrationServer
{
    void UnitTestNoop() {}
}
)"},
            },
            [](string_view message) {
                const auto message_str = string(message);

                if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos || message_str.find("fatal") != string::npos || message_str.find("Fatal") != string::npos) {
                    throw ScriptSystemException(message_str);
                }
            });
    }

    static auto MakeClientScriptBinary(const FileSystem& metadata_resources) -> vector<uint8>
    {
        BakerClientEngine compiler_engine {metadata_resources};

        return BakerTests::CompileInlineScripts(&compiler_engine, "ClientServerClientScripts",
            {
                {"Scripts/ClientServerIntegrationClient.fos", R"(
#include "ClientServerIntegrationClientShared.fos"

namespace ClientServerIntegrationClient
{
    int ConnectingCalls = 0;
    int ConnectedCalls = 0;
    int LoginSuccessCalls = 0;
    int DisconnectedCalls = 0;

    void ModuleInit()
    {
        Game.OnConnecting.Subscribe(OnConnecting);
        Game.OnConnected.Subscribe(OnConnected);
        Game.OnLoginSuccess.Subscribe(OnLoginSuccess);
        Game.OnDisconnected.Subscribe(OnDisconnected);
    }

    void OnConnecting()
    {
        ConnectingCalls++;
    }

    void OnConnected()
    {
        ConnectedCalls++;
    }

    void OnLoginSuccess()
    {
        LoginSuccessCalls++;
    }

    void OnDisconnected()
    {
        DisconnectedCalls++;
    }

    void UnitTestEntry()
    {
        UnitTestNoop();
    }

    int UnitTestGetConnectingCalls()
    {
        return ConnectingCalls;
    }

    int UnitTestGetConnectedCalls()
    {
        return ConnectedCalls;
    }

    int UnitTestGetLoginSuccessCalls()
    {
        return LoginSuccessCalls;
    }

    int UnitTestGetDisconnectedCalls()
    {
        return DisconnectedCalls;
    }
}
)"},
                {"Scripts/ClientServerIntegrationClientShared.fos", R"(
namespace ClientServerIntegrationClient
{
    void UnitTestNoop() {}
}
)"},
            },
            [](string_view message) {
                const auto message_str = string(message);

                if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos || message_str.find("fatal") != string::npos || message_str.find("Fatal") != string::npos) {
                    throw ScriptSystemException(message_str);
                }
            });
    }

    static auto MakeServerTestSettings(uint16 port) -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedServerSettings(settings);
        BakerTests::OverrideSetting(settings.ServerPort, port);

        return settings;
    }

    static auto MakeClientTestSettings(uint16 port) -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedClientSettings(settings);
        BakerTests::OverrideSetting(settings.ServerPort, port);

        return settings;
    }

    static auto MakeServerTestResources() -> FileSystem
    {
        const auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();

        auto compiler_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ClientServerServerCompilerResources");
        compiler_source->AddFile("Metadata.fometa-server", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_source));

        BakerServerEngine proto_engine {compiler_resources};
        const auto critter_type = proto_engine.Hashes.ToHashedString("Critter");
        const auto proto_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "UnitTestSharedCritter");
        const auto script_blob = MakeServerScriptBinary(compiler_resources);

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ClientServerServerRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
        runtime_source->AddFile("ClientServerIntegration.fopro-bin-server", proto_blob);
        runtime_source->AddFile("ClientServerIntegration.fos-bin-server", script_blob);

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));
        return resources;
    }

    static auto MakeClientTestResources() -> FileSystem
    {
        const auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();

        auto compiler_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ClientServerClientCompilerResources");
        compiler_source->AddFile("Metadata.fometa-client", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_source));

        BakerClientEngine proto_engine {compiler_resources};
        const auto critter_type = proto_engine.Hashes.ToHashedString("Critter");
        const auto proto_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "UnitTestSharedCritter");
        const auto script_blob = MakeClientScriptBinary(compiler_resources);

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ClientServerClientRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-client", metadata_blob);
        runtime_source->AddFile("ClientServerIntegration.fopro-bin-client", proto_blob);
        runtime_source->AddFile("ClientServerIntegration.fos-bin-client", script_blob);

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));
        return resources;
    }

    static auto WaitForServerStart(ServerEngine* server) -> string
    {
        FO_RUNTIME_ASSERT(server);

        for (int32 i = 0; i < 6000; i++) {
            if (server->IsStarted()) {
                return {};
            }
            if (server->IsStartingError()) {
                return "ServerEngine startup failed";
            }

            std::this_thread::sleep_for(std::chrono::milliseconds {10});
        }

        return "ServerEngine startup timed out";
    }

    static auto GetServerConnectionCount(ServerEngine* server) -> size_t
    {
        FO_RUNTIME_ASSERT(server);

        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        const auto unlock = scope_exit([server]() noexcept { safe_call([server] { server->Unlock(); }); });

        const auto health_info = server->GetHealthInfo();
        constexpr string_view prefix {"Connections: "};
        const auto pos = health_info.find(prefix);
        REQUIRE(pos != string::npos);

        const auto begin = pos + prefix.length();
        const auto end = health_info.find('\n', begin);
        const auto value_sv = string_view {health_info}.substr(begin, end == string::npos ? string::npos : end - begin);

        size_t value = 0;
        const auto [ptr, ec] = std::from_chars(value_sv.data(), value_sv.data() + value_sv.size(), value);
        REQUIRE(ec == std::errc {});
        REQUIRE(ptr == value_sv.data() + value_sv.size());

        return value;
    }

    static auto WaitForConnected(ClientEngine* client, ServerEngine* server) -> bool
    {
        FO_RUNTIME_ASSERT(client);
        FO_RUNTIME_ASSERT(server);

        for (int32 i = 0; i < 2000; i++) {
            client->MainLoop();

            if (client->IsConnected() && client->GetCurPlayer() != nullptr && GetServerConnectionCount(server) == 1) {
                return true;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }

        return false;
    }

    static auto WaitForDisconnected(ClientEngine* client, ServerEngine* server) -> bool
    {
        FO_RUNTIME_ASSERT(client);
        FO_RUNTIME_ASSERT(server);

        for (int32 i = 0; i < 2000; i++) {
            client->MainLoop();

            if (!client->IsConnecting() && !client->IsConnected() && client->GetCurPlayer() == nullptr && GetServerConnectionCount(server) == 0) {
                return true;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }

        return false;
    }
}

TEST_CASE("ClientAndServerHandshakeOverInterthreadTransport")
{
    using namespace TestClientServerIntegration;

    const auto port = IntegrationTestPort.fetch_add(1);

    auto server_settings = MakeServerTestSettings(port);
    auto client_settings = MakeClientTestSettings(port);

    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(server_settings, MakeServerTestResources());
    auto client = SafeAlloc::MakeRefCounted<ClientEngine>(client_settings, MakeClientTestResources(), &App->MainWindow);

    const auto shutdown = scope_exit([&server, &client]() noexcept {
        safe_call([&client] {
            client->Disconnect();
            client->Shutdown();
        });

        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForServerStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    CHECK(GetServerConnectionCount(server.get()) == 0);
    CHECK_FALSE(client->IsConnecting());
    CHECK_FALSE(client->IsConnected());
    CHECK(client->GetCurPlayer() == nullptr);

    const auto get_client_func_name = [&client](string_view name) { return client->Hashes.ToHashedString(name); };

    int connecting_calls = 0;
    int connected_calls = 0;
    int login_success_calls = 0;
    int disconnected_calls = 0;

    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetConnectingCalls"), connecting_calls));
    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetConnectedCalls"), connected_calls));
    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetLoginSuccessCalls"), login_success_calls));
    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetDisconnectedCalls"), disconnected_calls));

    CHECK(connecting_calls == 0);
    CHECK(connected_calls == 0);
    CHECK(login_success_calls == 0);
    CHECK(disconnected_calls == 0);

    client->Connect();

    REQUIRE(WaitForConnected(client.get(), server.get()));

    CHECK(client->IsConnected());
    CHECK_FALSE(client->IsConnecting());
    REQUIRE(client->GetCurPlayer() != nullptr);
    CHECK(client->GetConnection().GetBytesSend() > 0);
    CHECK(client->GetConnection().GetBytesReceived() > 0);
    CHECK(GetServerConnectionCount(server.get()) == 1);

    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetConnectingCalls"), connecting_calls));
    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetConnectedCalls"), connected_calls));
    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetLoginSuccessCalls"), login_success_calls));
    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetDisconnectedCalls"), disconnected_calls));

    CHECK(connecting_calls >= 1);
    CHECK(connected_calls >= 1);
    CHECK(login_success_calls == 0);
    CHECK(disconnected_calls == 0);

    client->Disconnect();

    REQUIRE(WaitForDisconnected(client.get(), server.get()));

    CHECK_FALSE(client->IsConnecting());
    CHECK_FALSE(client->IsConnected());
    CHECK(client->GetCurPlayer() == nullptr);
    CHECK(GetServerConnectionCount(server.get()) == 0);

    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetDisconnectedCalls"), disconnected_calls));
    CHECK(disconnected_calls >= 1);
}

FO_END_NAMESPACE
