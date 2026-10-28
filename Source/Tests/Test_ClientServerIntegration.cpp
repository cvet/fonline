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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#include "Test_ClientServerIntegrationHelpers.h"

FO_BEGIN_NAMESPACE

using namespace TestClientServerIntegration;

TEST_CASE("ServerRejectsMalformedPreHandshakePayloadWithoutExceptionReport")
{
    using namespace TestClientServerIntegration;

    auto port = IntegrationTestPort.fetch_add(1);
    auto server_settings = MakeServerTestSettings(port);
    auto server = TestClientServerIntegration::MakeServerEngine(server_settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(HasInterthreadListener(port));

    auto previous_exception_callback = exceptions::get_callback();
    std::atomic_int exception_reports {};
    exceptions::set_callback([&exception_reports](string_view, const stack_trace::catched_data&, bool) { exception_reports.fetch_add(1); });
    auto restore_exception_callback = scope_exit([previous = std::move(previous_exception_callback)]() mutable noexcept { exceptions::set_callback(std::move(previous)); });

    RawChannelClient client {port};
    REQUIRE(WaitForServerConnectionCount(server, 1));

    auto malformed_handshake = NetOutBuffer(64);
    malformed_handshake.StartMsg(NetMessage::Handshake);
    malformed_handshake.Write<uint32_t>(std::numeric_limits<uint32_t>::max());
    malformed_handshake.Write<uint16_t>(uint16_t {0});
    malformed_handshake.EndMsg();
    client.Send(malformed_handshake.GetData());

    REQUIRE(WaitForServerConnectionCount(server, 0));
    CHECK(client.IsDisconnected());
    CHECK(exception_reports.load() == 0);
}

TEST_CASE("ServerDisconnectsPreLoginConnectionAfterLoginTimeout")
{
    using namespace TestClientServerIntegration;

    bool simulation_paused = GENERATE(false, true);
    bool inactivity_timeout = GENERATE(false, true);
    CAPTURE(simulation_paused, inactivity_timeout);
    uint16_t port = IntegrationTestPort.fetch_add(1);
    auto server_settings = MakeServerTestSettings(port);
    BakerTests::OverrideSetting(server_settings.ServerNetwork.InactivityDisconnectTime, inactivity_timeout ? 100 : 0);
    BakerTests::OverrideSetting(server_settings.ServerNetwork.LoginTimeout, inactivity_timeout ? 0 : 100);
    auto server = TestClientServerIntegration::MakeServerEngine(server_settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(HasInterthreadListener(port));

    if (simulation_paused) {
        server->PauseSimulation();
    }
    synctime simulation_time = server->GameTime.GetSynchronizedTime();
    RawChannelClient client {port};
    REQUIRE(WaitForServerConnectionCount(server, 1));

    for (int32_t i = 0; i < 2000 && !client.IsDisconnected(); i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds {2});
    }

    CHECK(client.IsDisconnected());
    CHECK(WaitForServerConnectionCount(server, 0));
    if (simulation_paused) {
        CHECK(server->GameTime.GetSynchronizedTime() == simulation_time);
    }
}

TEST_CASE("ServerReportsMetadataMismatchInHandshake")
{
    using namespace TestClientServerIntegration;

    uint16_t port = IntegrationTestPort.fetch_add(1);
    auto server_settings = MakeServerTestSettings(port);
    BakerTests::OverrideSetting(server_settings.Network.DisableZlibCompression, true);
    auto server = TestClientServerIntegration::MakeServerEngine(server_settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(HasInterthreadListener(port));

    RawChannelClient client {port};
    REQUIRE(WaitForServerConnectionCount(server, 1));

    // A binary-compatible client whose resources come from another bake: the layout verdict is what keeps its
    // property payloads from reaching deserialization
    REQUIRE_FALSE(server->GetMetadataVersion().empty());
    auto handshake = NetOutBuffer(128);
    handshake.StartMsg(NetMessage::Handshake);
    handshake.Write(server_settings.Network.CompatibilityVersion);
    handshake.Write<string_view>("0123456789abcdef");
    handshake.Write<uint32_t>(FO_UPDATER_VERSION);
    handshake.Write<string_view>("Linux-x64");
    handshake.EndMsg();
    client.Send(handshake.GetData());

    bool received_answer = false;

    for (int32_t i = 0; i < 2000 && !received_answer; i++) {
        vector<uint8_t> response_data = client.GetPlaintext();

        if (!response_data.empty()) {
            NetInBuffer response {response_data.size()};
            response.AddData(response_data);

            if (response.NeedProcess()) {
                REQUIRE(response.ReadMsg() == NetMessage::HandshakeAnswer);
                CHECK_FALSE(response.Read<bool>());
                CHECK_FALSE(response.Read<bool>());
                CHECK(response.Read<bool>());
                CHECK(response.Read<string>() == server->GetMetadataVersion());
                received_answer = true;
            }
        }

        if (!received_answer) {
            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }
    }

    REQUIRE(received_answer);
    client.Close();
    CHECK(WaitForServerConnectionCount(server, 0));
}

TEST_CASE("ServerRejectsUnsafeUpdaterGenerationBeforeInitData")
{
    using namespace TestClientServerIntegration;

    uint16_t port = IntegrationTestPort.fetch_add(1);
    auto server_settings = MakeServerTestSettings(port);
    BakerTests::OverrideSetting(server_settings.Network.DisableZlibCompression, true);
    auto server = TestClientServerIntegration::MakeServerEngine(server_settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(HasInterthreadListener(port));

    RawChannelClient client {port};
    REQUIRE(WaitForServerConnectionCount(server, 1));

    static_assert(FO_UPDATER_VERSION > 1);
    auto handshake = NetOutBuffer(128);
    handshake.StartMsg(NetMessage::Handshake);
    handshake.Write(server_settings.Network.CompatibilityVersion);
    handshake.Write(server->GetMetadataVersion());
    handshake.Write<uint32_t>(FO_UPDATER_VERSION - 1);
    handshake.Write<string_view>("Linux-x64");
    handshake.EndMsg();
    client.Send(handshake.GetData());

    bool received_rejection = false;
    for (int32_t i = 0; i < 2000 && !received_rejection; i++) {
        vector<uint8_t> response_data = client.GetPlaintext();

        if (!response_data.empty()) {
            NetInBuffer response {response_data.size()};
            response.AddData(response_data);

            if (response.NeedProcess()) {
                REQUIRE(response.ReadMsg() == NetMessage::HandshakeAnswer);
                CHECK_FALSE(response.Read<bool>());
                CHECK(response.Read<bool>());
                CHECK_FALSE(response.Read<bool>());
                CHECK(response.Read<string>() == server->GetMetadataVersion());

                if (response.NeedProcess()) {
                    REQUIRE(response.ReadMsg() == NetMessage::Disconnect);
                    response.ShrinkReadBuf();
                    CHECK(response.GetDataSize() == 0);
                    received_rejection = true;
                }
            }
        }

        if (!received_rejection) {
            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }
    }

    REQUIRE(received_rejection);
    client.Close();
    REQUIRE(WaitForServerConnectionCount(server, 0));
}

TEST_CASE("ClientShutdownDisconnectsActiveConnection")
{
    using namespace TestClientServerIntegration;

    auto port = IntegrationTestPort.fetch_add(1);

    auto server_settings = MakeServerTestSettings(port);
    auto client_settings = MakeClientTestSettings(port);

    auto server = TestClientServerIntegration::MakeServerEngine(server_settings);
    auto client = TestClientServerIntegration::MakeClientEngine(client_settings);
    bool client_shutdown = false;
    int32_t shutdown_disconnected_calls = 0;

    auto shutdown = scope_exit([&server, &client, &client_shutdown]() noexcept {
        safe_call([&client, &client_shutdown] {
            if (!client_shutdown) {
                client->Shutdown();
            }
        });

        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    client->Connect();
    REQUIRE(WaitForConnected(client, server));

    Entity::EventCallbackData shutdown_disconnect_observer;
    shutdown_disconnect_observer.Callback = [&shutdown_disconnected_calls](FuncCallData&) {
        shutdown_disconnected_calls++;
        return Entity::EventResult::ContinueChain;
    };
    shutdown_disconnect_observer.SubscriptionPtr = reinterpret_cast<uintptr_t>(&shutdown_disconnected_calls);
    client->OnDisconnected.Subscribe(std::move(shutdown_disconnect_observer));

    client->Shutdown();
    client_shutdown = true;

    CHECK_FALSE(client->IsConnecting());
    CHECK_FALSE(client->IsConnected());
    CHECK_FALSE(static_cast<bool>(client->GetCurPlayer()));
    CHECK(shutdown_disconnected_calls == 1);
    REQUIRE(WaitForServerConnectionCount(server, 0));
}

TEST_CASE("ClientAndServerInterthreadConnectionKeepsProcessingAfterHandshake")
{
    using namespace TestClientServerIntegration;

    auto port = IntegrationTestPort.fetch_add(1);

    auto server_settings = MakeServerTestSettings(port);
    auto client_settings = MakeClientTestSettings(port);

    auto server = TestClientServerIntegration::MakeServerEngine(server_settings);
    auto client = TestClientServerIntegration::MakeClientEngine(client_settings);

    auto shutdown = scope_exit([&server, &client]() noexcept {
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

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    client->Connect();
    REQUIRE(WaitForConnected(client, server));

    size_t bytes_received_after_handshake = client->GetConnection()->GetBytesReceived();

    bool received_post_handshake_packet = false;

    for (int32_t i = 0; i < 1000; i++) {
        client->MainLoop();

        if (client->GetConnection()->GetBytesReceived() > bytes_received_after_handshake) {
            received_post_handshake_packet = true;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds {2});
    }

    CHECK(received_post_handshake_packet);
}

TEST_CASE("ClientReportsUnresolvedHashAndLearnsWithoutDisconnect")
{
    using namespace TestClientServerIntegration;

    auto port = IntegrationTestPort.fetch_add(1);

    auto server_settings = MakeServerTestSettings(port);
    auto client_settings = MakeClientTestSettings(port);

    auto server = TestClientServerIntegration::MakeServerEngine(server_settings);
    auto client = TestClientServerIntegration::MakeClientEngine(client_settings);

    auto shutdown = scope_exit([&server, &client]() noexcept {
        safe_call([&client] { client->Shutdown(); });

        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    client->Connect();
    REQUIRE(WaitForConnected(client, server));

    // A string the server knows but the client doesn't РІР‚вЂќ mimics a runtime hstring the client can't resolve
    hstring reported = server->Hashes.to_hashed_string("integration_test_only_hash");

    // Send the exact wire message ClientEngine emits when it hits an unresolved hash
    client->GetConnection()->OutBuf->StartMsg(NetMessage::UnresolvedHash);
    client->GetConnection()->OutBuf->Write<hstring::hash_t>(reported.as_hash());
    client->GetConnection()->OutBuf->EndMsg();

    REQUIRE(WaitForLearnedHash(client, reported.as_hash(), "integration_test_only_hash"));
    CHECK(client->IsConnected());
    CHECK(GetServerConnectionCount(server) == 1);

    auto second_client = TestClientServerIntegration::MakeClientEngine(client_settings);
    auto shutdown_second_client = scope_exit([&second_client]() noexcept { safe_call([&second_client] { second_client->Shutdown(); }); });

    second_client->Connect();
    REQUIRE(WaitForConnected(second_client, server, 2));
    REQUIRE(WaitForLearnedHash(second_client, reported.as_hash(), "integration_test_only_hash"));
    CHECK(GetServerConnectionCount(server) == 2);
}

TEST_CASE("ClientUpdaterResourcePatchLifecycle")
{
    using namespace TestClientServerIntegration;

    string install = PrepareClientUpdaterBakeOutput();
    string published = MakeTempClientUpdaterBakeDir("published");
    string writable = MakeTempClientUpdaterBakeDir("writable");
    auto cleanup = scope_exit([&]() noexcept {
        (void)fs::remove_dir_tree(install);
        (void)fs::remove_dir_tree(published);
        (void)fs::remove_dir_tree(writable);
    });
    REQUIRE(fs::create_directories(published));
    REQUIRE(fs::create_directories(writable));
    vector<uint8_t> metadata = BakerTests::MakeMetadataBlob({});

    for (const string& directory : {install, published}) {
        ResourcePackWriter writer {strex(directory).combine_path("Metadata.fores").str()};
        writer.AddFile("Metadata.fometa-client", metadata);
        writer.Finish();
    }

    auto write_art = [](string_view directory, const vector<pair<string, string>>& files) {
        ResourcePackWriter writer {strex(directory).combine_path("Art.fores").str(), {0, 100}};

        for (const auto& [name, content] : files) {
            writer.AddFile(name, {reinterpret_cast<const uint8_t*>(content.data()), content.size()});
        }

        writer.Finish();
    };
    string unchanged(65536, 'b');
    write_art(install, {{"Keep.bin", unchanged}, {"Change.txt", "old"}, {"Removed.txt", "removed"}});
    write_art(published, {{"Keep.bin", unchanged}, {"Change.txt", "first update"}});
    string installed_base = strex(install).combine_path("Art.fores").str();
    auto original = fs::read_file(installed_base);
    REQUIRE(original);
    string writable_resources = strex(writable).combine_path("Resources").str();
    string patch = strex(writable_resources).combine_path("Art.patch.fores").str();
    string replacement = strex(writable_resources).combine_path("Art.fores").str();

    auto synchronize = [&](bool in_memory, bool replace_published = false) {
        uint16_t port = IntegrationTestPort.fetch_add(1);
        GlobalSettings server_settings = MakeServerTestSettings(port);
        BakerTests::OverrideSetting(server_settings.Common.Packaged, true);
        BakerTests::OverrideSetting(server_settings.Baking.ClientResources, published);
        auto server_pack_config = ConfigFile("[ResourcePack]\nName = Metadata\nClientOnly = True\n[ResourcePack]\nName = Art\nClientOnly = True\n");
        server_settings.ApplyConfigFile(server_pack_config, "");
        BakerTests::OverrideSetting(server_settings.Baking.PlatformBinaries, strex(published).combine_path("NoBinaries").str());
        BakerTests::OverrideSetting(server_settings.ServerNetwork.UpdateFilesInMemory, in_memory);
        auto server = TestClientServerIntegration::MakeServerEngine(server_settings);
        auto shutdown = scope_exit([&]() noexcept { safe_call([&] { server->Shutdown(); }); });
        string error = WaitForServerStart(server);
        INFO(error);
        REQUIRE(error.empty());

#if !FO_WINDOWS
        if (replace_published) {
            REQUIRE(fs::rename_durable(strex(published).combine_path("Art.fores").str(), strex(published).combine_path("Pinned.fores").str()));
            write_art(published, {{"Keep.bin", unchanged}, {"Change.txt", "replaced after server start"}});
        }
#else
        ignore_unused(replace_published);
#endif

        GlobalSettings client_settings = MakeClientTestSettings(port);
        BakerTests::OverrideSetting(client_settings.Common.Packaged, true);
        BakerTests::OverrideSetting(client_settings.Baking.ClientResources, install);
        auto client_pack_config = ConfigFile("[ResourcePack]\nName = Embedded\nClientOnly = True\n[ResourcePack]\nName = Metadata\nClientOnly = True\n[ResourcePack]\nName = Art\nClientOnly = True\n");
        client_settings.ApplyConfigFile(client_pack_config, "");
        client_settings.ApplyWritableRoot(writable);
        Updater updater {&client_settings, &GetApp()->MainWindow};
        REQUIRE(WaitForUpdaterResult(updater));
        REQUIRE(updater.GetResult() == UpdaterResult::ResourcesReady);
        REQUIRE_FALSE(updater.IsAborted());
        FileSystem resources = GetClientResources(client_settings);
        CHECK(resources.ReadFileText("Keep.bin") == unchanged);
        CHECK_FALSE(resources.IsFileExists("Removed.txt"));
    };

    synchronize(false);
    REQUIRE(fs::exists(patch));
    CHECK_FALSE(fs::exists(replacement));
    CHECK(fs::read_file(installed_base) == original);
    auto first = fs::read_file(patch);
    REQUIRE(first);
    CHECK(first->size() < 1024);

    SECTION("SecondUpdateAppendsAndRetainsTheCommittedPrefix")
    {
        write_art(published, {{"Keep.bin", unchanged}, {"Renamed.txt", "first update"}, {"Added.txt", "second update"}});
        synchronize(true);
        auto second = fs::read_file(patch);
        REQUIRE(second);
        CHECK(second->size() > first->size());
        CHECK(second->starts_with(*first));
        ResourcePackSource pair {installed_base, patch};
        CHECK(pair.IsFileExists("Renamed.txt"));
        CHECK_FALSE(pair.IsFileExists("Change.txt"));
        CHECK(pair.GetContentHash() == ResourcePackSource(strex(published).combine_path("Art.fores").str()).GetContentHash());
    }

    SECTION("DamagedBaseTheCatalogStillNamesIsDownloadedAgain")
    {
        // One flipped payload byte leaves the header and the catalog intact, so the pair still reports the advertised
        // content; only reading its bytes back tells the damage apart, and the whole pack is the repair
        string damaged = *original;
        damaged[RESOURCE_PACK_HEADER_SIZE] = static_cast<char>(damaged[RESOURCE_PACK_HEADER_SIZE] ^ 0x01);
        REQUIRE(fs::write_file(installed_base, damaged));

        synchronize(false);
        CHECK(fs::read_file(replacement) == fs::read_file(strex(published).combine_path("Art.fores").str()));
        CHECK_FALSE(fs::exists(patch));
        CHECK(fs::read_file(installed_base) == optional<string> {damaged});
    }

    SECTION("DamagedPatchPayloadIsFetchedAgain")
    {
        string damaged = *first;
        damaged[RESOURCE_PATCH_HEADER_SIZE] = static_cast<char>(damaged[RESOURCE_PATCH_HEADER_SIZE] ^ 0x01);
        REQUIRE(fs::write_file(patch, damaged));

        synchronize(false);
        CHECK_FALSE(fs::exists(replacement));
        auto repaired = fs::read_file(patch);
        REQUIRE(repaired);
        CHECK(repaired->starts_with(damaged));
        ResourcePackSource pair {installed_base, patch};
        size_t size = 0;
        uint64_t write_time = 0;
        CHECK(static_cast<bool>(pair.OpenFile("Change.txt", size, write_time)));
        CHECK(size == string_view {"first update"}.size());
    }

    SECTION("PatchThatWouldOutgrowItsPackIsReplacedByThePack")
    {
        constexpr size_t large_size = 65 * 1024 * 1024;

        {
            vector<uint8_t> content(large_size, 'n');
            string intermediate = strex(writable).combine_path("Intermediate.fores").str();

            {
                ResourcePackWriter writer {intermediate, {0, 100}};
                writer.AddFile("Keep.bin", {reinterpret_cast<const uint8_t*>(unchanged.data()), unchanged.size()});
                writer.AddFile("Change.txt", content);
                writer.Finish();
            }

            ResourcePackSource target {intermediate};
            ResourcePatchWriter writer {installed_base, patch, target.GetEntryRefs(), target.GetContentHash()};
            REQUIRE(writer.GetDownloads().size() == 1);
            REQUIRE(writer.GetDownloads().front().Path == "Change.txt");
            fs::disk_directory_lock patch_lock {strex(patch).extract_dir().str()};
            writer.Begin(patch_lock);
            writer.AddEncodedFile(content);
            writer.Finish();
        }

        // The published pack dropped the large payload again, so another append would leave a patch far larger than
        // the pack it patches: the pack replaces the pair instead
        REQUIRE(fs::file_size(patch).value() > large_size);
        synchronize(false);
        CHECK(fs::read_file(replacement) == fs::read_file(strex(published).combine_path("Art.fores").str()));
        CHECK_FALSE(fs::exists(patch));
        synchronize(true);
        CHECK_FALSE(fs::exists(patch));
        CHECK(fs::read_file(installed_base) == original);
    }

#if !FO_WINDOWS
    SECTION("DiskBackendPinsTheAdvertisedArtifactAcrossReplacement")
    {
        write_art(published, {{"Keep.bin", unchanged}, {"Pinned.txt", "advertised before replacement"}});
        synchronize(false, true);
        ResourcePackSource pair {installed_base, patch};
        CHECK(pair.IsFileExists("Pinned.txt"));
        CHECK_FALSE(pair.IsFileExists("Change.txt"));
        CHECK(pair.GetContentHash() == ResourcePackSource(strex(published).combine_path("Pinned.fores").str()).GetContentHash());
        CHECK(pair.GetContentHash() != ResourcePackSource(strex(published).combine_path("Art.fores").str()).GetContentHash());
    }
#endif
}

TEST_CASE("ClientUpdaterConsumesReportedHashListDuringHandshake")
{
    using namespace TestClientServerIntegration;

    auto port = IntegrationTestPort.fetch_add(1);

    auto server_settings = MakeServerTestSettings(port);
    auto client_settings = MakeClientTestSettings(port);
    string updater_bake_output = PrepareClientUpdaterBakeOutput();
    auto cleanup_updater_bake_output = scope_exit([&updater_bake_output]() noexcept { fs::remove_dir_tree(updater_bake_output); });
    BakerTests::OverrideSetting(client_settings.Baking.BakeOutput, updater_bake_output);

    // The rig has no resource packs to read a version back from, and this case is about the hash list rather
    // than about pack reading, so the client reports the version the test metadata carries
    BakerTests::OverrideSetting(client_settings.Network.ForceMetadataVersion, string(BakerTests::TEST_METADATA_VERSION));

    auto server = TestClientServerIntegration::MakeServerEngine(server_settings);
    auto client = TestClientServerIntegration::MakeClientEngine(client_settings);

    auto shutdown = scope_exit([&server, &client]() noexcept {
        safe_call([&client] { client->Shutdown(); });

        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    client->Connect();
    REQUIRE(WaitForConnected(client, server));

    hstring reported = server->Hashes.to_hashed_string("integration_test_updater_hash");

    client->GetConnection()->OutBuf->StartMsg(NetMessage::UnresolvedHash);
    client->GetConnection()->OutBuf->Write<hstring::hash_t>(reported.as_hash());
    client->GetConnection()->OutBuf->EndMsg();

    REQUIRE(WaitForLearnedHash(client, reported.as_hash(), "integration_test_updater_hash"));
    client->Disconnect();

    Updater updater {&client_settings, &GetApp()->MainWindow};
    REQUIRE(WaitForUpdaterResult(updater));
    CHECK(updater.GetResult() == UpdaterResult::ResourcesReady);
    CHECK_FALSE(updater.IsAborted());
}

TEST_CASE("ClientUpdaterDoesNotSurfaceOutdatedMetadataLayoutBeforeRepair")
{
    using namespace TestClientServerIntegration;

    uint16_t port = IntegrationTestPort.fetch_add(1);
    GlobalSettings client_settings = MakeClientTestSettings(port);
    string updater_bake_output = PrepareClientUpdaterBakeOutput();
    auto cleanup_updater_bake_output = scope_exit([&updater_bake_output]() noexcept { fs::remove_dir_tree(updater_bake_output); });
    string pack_name = "OutdatedClientPack";
    string metadata_path = strex(updater_bake_output).combine_path(pack_name).combine_path("Metadata.fometa-client").str();

    STATIC_REQUIRE(METADATA_FILE_VERSION > 1);
    constexpr uint16_t outdated_file_version = METADATA_FILE_VERSION - 1;
    vector<uint8_t> outdated_metadata;
    data_writer outdated_writer {outdated_metadata};
    outdated_writer.write<uint32_t>(METADATA_FILE_MAGIC);
    outdated_writer.write<uint16_t>(outdated_file_version);
    outdated_writer.write<uint16_t>(numeric_cast<uint16_t>(BakerTests::TEST_METADATA_VERSION.length()));
    outdated_writer.write_string_bytes(BakerTests::TEST_METADATA_VERSION);
    outdated_writer.write<uint16_t>(uint16_t {0});

    REQUIRE(fs::write_file(metadata_path, outdated_metadata));
    BakerTests::OverrideSetting(client_settings.Baking.BakeOutput, updater_bake_output);
    auto client_pack_config = ConfigFile(strex("[ResourcePack]\nName = {}\nClientOnly = True\n", pack_name).str());
    client_settings.ApplyConfigFile(client_pack_config, "");

    CHECK_NOTHROW([&client_settings] {
        Updater updater {&client_settings, &GetApp()->MainWindow};
        ignore_unused(updater);
    }());
}

TEST_CASE("SingleGameplayWorkerProcessesRequestsFromTwoClientsWithoutLostUpdates", "[network][player]")
{
    bool dedicated_remote_only = GENERATE(false, true);
    bool simulation_paused = GENERATE(false, true);
    CAPTURE(dedicated_remote_only, simulation_paused);

    int32_t login_calls = 0;
    int32_t world_steps = 0;
    int32_t world_sum = 0;
    map<ident_t, int32_t> steps_by_player;
    uint16_t port = IntegrationTestPort.fetch_add(1);
    auto server_settings = MakeServerTestSettings(port);
    auto first_settings = MakeClientTestSettings(port);
    auto second_settings = MakeClientTestSettings(port);
    BakerTests::OverrideSetting(server_settings.Server.SingleThreadedLogic, true);
    BakerTests::OverrideSetting(server_settings.ServerNetwork.DisableNetworking, false);
    BakerTests::OverrideSetting(server_settings.Network.DisableInterthreadCommunication, dedicated_remote_only);
    BakerTests::OverrideSetting(server_settings.Network.WebSocketPort, 0);
    BakerTests::OverrideSetting(first_settings.ClientNetwork.RequireInterthreadTransport, !dedicated_remote_only);
    BakerTests::OverrideSetting(second_settings.ClientNetwork.RequireInterthreadTransport, !dedicated_remote_only);

    auto server = TestClientServerIntegration::MakeServerEngine(server_settings);
    auto first = TestClientServerIntegration::MakeClientEngine(first_settings);
    auto second = TestClientServerIntegration::MakeClientEngine(second_settings);
    auto shutdown = scope_exit([&]() noexcept {
        safe_call([&] { second->Shutdown(); });
        safe_call([&] { first->Shutdown(); });
        safe_call([&] { server->Shutdown(); });
    });
    REQUIRE(WaitForServerStart(server).empty());
    REQUIRE(server->GetWorkerThreadCount() == 1);
    CHECK(HasInterthreadListener(port) == !dedicated_remote_only);
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    {
        auto unlock = scope_exit([&]() noexcept { safe_call([&] { server->Unlock(); }); });

        // Both scripting backends enter the same validated remote-call boundary. Native handlers keep this
        // worker/transport regression independent of the backend's fixture compiler and call syntax
        server->SetRemoteCallHandler(
            server->Hashes.to_hashed_string("UnitTestLogin"),
            [&](hstring, nptr<Entity> caller, span<uint8_t>, shared_ptr<ScriptCallCompletion>) {
                ptr<Player> player = caller.dyn_cast<Player>();
                login_calls++;
                auto logged = server->LoginPlayerToTempSession(player);
                auto cr = server->CreateCritter(server->Hashes.to_hashed_string("UnitTestSharedCritter"), true);
                auto loc = server->MapMngr.CreateLocation(server->Hashes.to_hashed_string("UnitTestSharedLocation"), vector<hstring> {server->Hashes.to_hashed_string("UnitTestSharedMap")});
                auto map = loc->GetMapByIndex(0);
                server->MapMngr.TransferToMap(cr, map, mpos {20, 20}, mdir {}, std::nullopt);
                server->SwitchPlayerCritter(logged, cr);
            },
            true);
        server->SetRemoteCallHandler(
            server->Hashes.to_hashed_string("UnitTestWorldStep"),
            [&](hstring, nptr<Entity> caller, span<uint8_t> data, shared_ptr<ScriptCallCompletion>) {
                ptr<Player> player = caller.dyn_cast<Player>();
                data_reader reader {data};
                int32_t step = reader.read<int32_t>();
                world_steps++;
                world_sum += step;
                steps_by_player[player->GetId()]++;
            },
            true);
    }

    auto login = [&](ptr<ClientEngine> client, size_t expected_connections) {
        client->Connect();
        REQUIRE(WaitForConnected(client, server, expected_connections));
        client->SendRemoteCall(client->Hashes.to_hashed_string("UnitTestLogin"), client->GetCurPlayer(), {});
        nanotime deadline = nanotime::now() + std::chrono::seconds {10};

        while (nanotime::now() < deadline) {
            first->MainLoop();
            second->MainLoop();

            if (client->GetChosen()) {
                return true;
            }

            coarse_sleep(std::chrono::milliseconds {2});
        }

        return false;
    };

    REQUIRE(login(first, 1));
    REQUIRE(login(second, 2));
    CHECK(first->GetChosen()->GetId() != second->GetChosen()->GetId());
    if (simulation_paused) {
        server->PauseSimulation();
    }
    synctime simulation_time = server->GameTime.GetSynchronizedTime();
    vector<uint8_t> payload;
    data_writer writer {payload};
    writer.write<int32_t>(7);

    for (int32_t i = 0; i < 32; i++) {
        first->SendRemoteCall(first->Hashes.to_hashed_string("UnitTestWorldStep"), first->GetCurPlayer(), payload);
        second->SendRemoteCall(second->Hashes.to_hashed_string("UnitTestWorldStep"), second->GetCurPlayer(), payload);
        first->MainLoop();
        second->MainLoop();
    }

    bool complete = false;
    nanotime deadline = nanotime::now() + std::chrono::seconds {10};

    while (nanotime::now() < deadline) {
        first->MainLoop();
        second->MainLoop();
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        {
            auto unlock = scope_exit([&]() noexcept { safe_call([&] { server->Unlock(); }); });
            complete = world_steps >= 64;

            if (complete) {
                CHECK(login_calls == 2);
                CHECK(world_steps == 64);
                CHECK(world_sum == 448);
                REQUIRE(steps_by_player.size() == 2);

                for (const auto& [id, steps] : steps_by_player) {
                    CAPTURE(id);
                    CHECK(steps == 32);
                }
            }
        }

        if (complete) {
            break;
        }

        coarse_sleep(std::chrono::milliseconds {2});
    }

    REQUIRE(complete);
    CHECK(GetServerConnectionCount(server) == 2);
    if (simulation_paused) {
        CHECK(server->IsSimulationPaused());
        CHECK(server->GameTime.GetSynchronizedTime() == simulation_time);
        server->ResumeSimulation();
    }
    first->Disconnect();
    second->Disconnect();
    REQUIRE(WaitForServerConnectionCount(server, 0));
}

struct RpcTransportProbe
{
    mutex Locker {};
    shared_ptr<ScriptCallCompletion> Retained {};
    shared_ptr<ServerConnection::RuntimeState> Runtime {};
    std::atomic<bool> Entered {};
    std::atomic<bool> Finished {};
    std::atomic<bool> Release {};
    std::atomic<bool> HasCompletion {};
    std::atomic<uint32_t> Warnings {};
};

template<typename Predicate>
static auto WaitForRpcTransportProbe(Predicate&& predicate) -> bool
{
    nanotime deadline = nanotime::now() + std::chrono::seconds {15};

    while (!predicate()) {
        if (nanotime::now() >= deadline) {
            return false;
        }

        coarse_sleep(std::chrono::milliseconds {1});
    }

    return true;
}

static void CheckRpcRuntimeTransport(bool disconnect, bool deferred, bool disabled)
{
    auto probe = safe_alloc::make_shared<RpcTransportProbe>();
    logging::set_callback("rpc-runtime-transport", [probe](logging::type, string_view message, nptr<const stack_trace::catched_data>) mutable {
        if (message.find("RPC runtime warning:") != string_view::npos && message.find("cause=pending-call") != string_view::npos) {
            probe->Warnings.fetch_add(1);
        }
    });
    auto restore_logger = scope_exit([]() noexcept { logging::set_callback("rpc-runtime-transport", {}); });
    uint16_t port = IntegrationTestPort.fetch_add(1);
    auto settings = MakeServerTestSettings(port);
    BakerTests::OverrideSetting(settings.Server.SingleThreadedLogic, true);
    BakerTests::OverrideSetting(settings.Network.DisableZlibCompression, true);
    BakerTests::OverrideSetting(settings.ServerNetwork.RemoteCallWarningTimeMs, !disconnect && !disabled ? 1000 : 0);
    BakerTests::OverrideSetting(settings.ServerNetwork.RemoteCallDisconnectTimeMs, disconnect ? 1000 : 0);
    BakerTests::OverrideSetting(settings.ServerNetwork.MaxPendingRemoteCalls, disabled ? 0 : 8);
    auto server = TestClientServerIntegration::MakeServerEngine(settings);
    auto shutdown = scope_exit([probe, &server]() mutable noexcept {
        probe->Release.store(true);

        {
            scoped_lock locker {probe->Locker};
            probe->Retained.reset();
        }

        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });
    string error = WaitForServerStart(server);
    INFO(error);
    REQUIRE(error.empty());
    hstring call_name;

    {
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
        call_name = server->Hashes.to_hashed_string("UnitTestLogin");
        server->SetRemoteCallHandler(
            call_name,
            [probe, deferred](hstring, nptr<Entity> caller, span<uint8_t>, shared_ptr<ScriptCallCompletion> completion) mutable {
                auto player = caller.dyn_cast<Player>();
                FO_VERIFY_AND_THROW(player, "RPC transport probe requires a player");

                {
                    scoped_lock locker {probe->Locker};
                    probe->Runtime = player->GetConnection()->GetRuntimeState();

                    if (deferred) {
                        probe->Retained = completion;
                    }
                }

                probe->HasCompletion.store(!!completion);
                probe->Entered.store(true);

                if (!deferred) {
                    while (!probe->Release.load()) {
                        coarse_sleep(std::chrono::milliseconds {1});
                    }
                }

                probe->Finished.store(true);
            },
            true);
    }

    RawChannelClient client {port};
    NetOutBuffer handshake {128};
    handshake.StartMsg(NetMessage::Handshake);
    handshake.Write(settings.Network.CompatibilityVersion);
    handshake.Write<string_view>(server->GetMetadataVersion());
    handshake.Write<uint32_t>(FO_UPDATER_VERSION);
    handshake.Write<string_view>("Windows-win64");
    handshake.EndMsg();
    client.Send(handshake.GetData());
    REQUIRE(WaitForRpcTransportProbe([&] {
        auto data = client.GetPlaintext();
        NetInBuffer response {data.size()};
        response.AddData(data);

        if (!response.NeedProcess()) {
            return false;
        }

        REQUIRE(response.ReadMsg() == NetMessage::HandshakeAnswer);
        CHECK_FALSE(response.Read<bool>());
        CHECK_FALSE(response.Read<bool>());
        CHECK_FALSE(response.Read<bool>());
        return true;
    }));
    NetOutBuffer request {32};
    request.StartMsg(NetMessage::RemoteCall);
    request.Write<hstring::hash_t>(call_name.as_hash());
    request.Write<int32_t>(0);
    request.EndMsg();
    client.Send(request.GetData());
    REQUIRE(WaitForRpcTransportProbe([&] { return probe->Entered.load(); }));
    shared_ptr<ServerConnection::RuntimeState> runtime;

    {
        scoped_lock locker {probe->Locker};
        runtime = probe->Runtime;
    }

    REQUIRE(runtime);

    if (disabled) {
        CHECK_FALSE(probe->HasCompletion.load());
        CHECK_FALSE(ServerConnection::IsRuntimeStateActive(runtime));
        CHECK_FALSE(client.IsDisconnected());
    }
    else if (disconnect) {
        REQUIRE(WaitForRpcTransportProbe([&] { return client.IsDisconnected(); }));
        CHECK(ServerConnection::GetRuntimeDisconnectReason(runtime) == DisconnectReason::RpcRuntimeLimit);
        CHECK(probe->Finished.load() == deferred);
        REQUIRE(WaitForRpcTransportProbe([&] { return !ServerConnection::IsRuntimeStateActive(runtime); }));
        auto diagnostics = ServerConnection::GetRuntimeDiagnostics(runtime);
        CHECK(diagnostics.PendingRemoteCalls == 0);
        CHECK(diagnostics.AbandonedRemoteCalls == 1);
        CHECK(diagnostics.RuntimeDisconnects == 1);
    }
    else {
        REQUIRE(WaitForRpcTransportProbe([&] { return probe->Warnings.load() != 0; }));
        CHECK_FALSE(client.IsDisconnected());
        CHECK(ServerConnection::GetRuntimeDiagnostics(runtime).PendingRemoteCalls == 1);
        CHECK(ServerConnection::GetRuntimeDisconnectReason(runtime) == DisconnectReason::None);
    }

    probe->Release.store(true);

    {
        scoped_lock locker {probe->Locker};
        probe->Retained.reset();
    }

    REQUIRE(WaitForRpcTransportProbe([&] { return probe->Finished.load(); }));
    REQUIRE(WaitForRpcTransportProbe([&] { return ServerConnection::GetRuntimeDiagnostics(runtime).PendingRemoteCalls == 0; }));
    client.Close();
    CHECK(WaitForServerConnectionCount(server, 0));
}

TEST_CASE("ServerRpcRuntimeWatchdogTransport", "[rpc-runtime-transport]")
{
    SECTION("A warning leaves the client connected")
    {
        CheckRpcRuntimeTransport(false, false, false);
    }

    SECTION("An independent watchdog disconnects while the only script worker is blocked")
    {
        CheckRpcRuntimeTransport(true, false, false);
    }

    SECTION("A deferred completion retains its deadline after the handler returns")
    {
        CheckRpcRuntimeTransport(true, true, false);
    }

    SECTION("Disabled limits do not create completion owners or disconnect a client")
    {
        CheckRpcRuntimeTransport(false, true, true);
    }
}

FO_END_NAMESPACE
