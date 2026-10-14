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

#if FO_MANAGED_SCRIPTING

#include "ManagedScriptBaker.h"
#include "Test_ClientEngineHelpers.h"
#include "Test_MapperHelpers.h"
#include "Test_ServerEngineHelpers.h"

FO_BEGIN_NAMESPACE

class ManagedRuntimeTestResources final
{
public:
    ManagedRuntimeTestResources() :
        Root {std::filesystem::temp_directory_path() / fs::make_path(strex("FOnlineManagedRuntimeTest_{}_{}", platform::get_current_process_id_str(), nanotime::now().milliseconds()).str())}
    {
        bool created = fs::create_directories(fs::path_to_string(Root));
        REQUIRE(created);

        auto cleanup_failed_construction = scope_fail([this]() noexcept { (void)fs::remove_dir_tree(fs::path_to_string(Root)); });

        std::filesystem::path engine_root = std::filesystem::path {fs::make_path(__FILE__)}.parent_path().parent_path().parent_path();
        std::filesystem::path config_path = Root / "Runtime.fomain";
        bool config_written = fs::write_file(fs::path_to_string(config_path), string_view {});
        REQUIRE(config_written);

        Rig.Settings.ApplyConfigAtPath("Runtime.fomain", fs::path_to_string(Root));
        BakerTests::OverrideSetting(Rig.Settings.Baking.BakeOutput, fs::path_to_string(Root / "Baking"));
        BakerTests::OverrideSetting(Rig.Settings.ManagedScript.GeneratedDir, fs::path_to_string(Root / "Generated"));
        BakerTests::OverrideSetting(Rig.Settings.ManagedScript.ProjectName, string {"RuntimeProbe"});
        BakerTests::OverrideSetting(Rig.Settings.ManagedScript.Dirs,
            vector<string> {
                fs::path_to_string(engine_root / "Source/Scripting/Managed/CoreScripts"),
                fs::path_to_string(engine_root / "Source/Tests/Fixtures"),
            });

        for (string_view target : {"server", "client", "mapper"}) {
            Rig.AddBakedFile(strex("Metadata.fometa-{}", target).str(), BakerTests::MakeEmptyMetadataBlob());
        }

        ManagedScriptBaker baker {Rig.MakeContext("RuntimeProbe")};
        baker.BakeFiles(BakerTests::TestRig::MakeEmptyFiles(), "");
        REQUIRE(Rig.Outputs.contains("Assemblies/Assemblies-server/RuntimeProbe.Server.dll"));
        REQUIRE(Rig.Outputs.contains("Assemblies/Assemblies-client/RuntimeProbe.Client.dll"));
        REQUIRE(Rig.Outputs.contains("Assemblies/Assemblies-mapper/RuntimeProbe.Mapper.dll"));
    }
    ManagedRuntimeTestResources(const ManagedRuntimeTestResources&) = delete;
    ManagedRuntimeTestResources(ManagedRuntimeTestResources&&) noexcept = delete;
    auto operator=(const ManagedRuntimeTestResources&) = delete;
    auto operator=(ManagedRuntimeTestResources&&) noexcept = delete;
    ~ManagedRuntimeTestResources() { (void)fs::remove_dir_tree(fs::path_to_string(Root)); }

    void AddTo(FileSystem& resources) const
    {
        auto source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("ManagedRuntimeAssemblies");

        for (const auto& [path, bytes] : Rig.Outputs) {
            source->AddFile(path, bytes);
        }

        resources.AddCustomSource(std::move(source));
    }

private:
    std::filesystem::path Root;
    BakerTests::TestRig Rig {};
};

TEST_CASE("ManagedScriptRuntime", "[managed][scripting]")
{
    ManagedRuntimeTestResources assemblies;
    auto settings = ServerEngineTests::MakeServerTestSettings();
    BakerTests::OverrideSetting(settings.Network.DisableInterthreadCommunication, true);
    auto resources = ServerEngineTests::MakeServerTestResources();
    assemblies.AddTo(resources);
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, std::move(resources));
    auto shutdown = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });
    string startup_error = ServerEngineTests::WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
    auto name = [&server](string_view method) { return server->Hashes.to_hashed_string(strex("ManagedRuntimeProbe::{}", method).str()); };

    int32_t init_calls = 0;
    REQUIRE(server->CallFunc(name("GetInitCalls"), init_calls));
    CHECK(init_calls == 1);
    CHECK_FALSE(server->FindFunc<void>(name("MissingFunction")));

    auto sum_array = server->FindFunc<int32_t, vector<int32_t>>(name("SumArray"));
    REQUIRE(sum_array);
    vector<int32_t> values {1, 2, 3};
    REQUIRE(sum_array.Call(values));
    CHECK(sum_array.GetResult() == 6);

    auto transform_array = server->FindFunc<vector<int32_t>, vector<int32_t>>(name("TransformArray"));
    REQUIRE(transform_array);
    REQUIRE(transform_array.Call(values));
    CHECK(transform_array.GetResult() == vector<int32_t> {11, 2, 3, 77});

    auto sum_dict = server->FindFunc<int32_t, map<int32_t, int32_t>>(name("SumDictionary"));
    REQUIRE(sum_dict);
    REQUIRE(sum_dict.Call(map<int32_t, int32_t> {{1, 10}, {2, 20}}));
    CHECK(sum_dict.GetResult() == 33);

    auto transform_dict = server->FindFunc<map<int32_t, int32_t>, map<int32_t, int32_t>>(name("TransformDictionary"));
    REQUIRE(transform_dict);
    REQUIRE(transform_dict.Call(map<int32_t, int32_t> {{1, 10}, {2, 20}}));
    CHECK(transform_dict.GetResult() == map<int32_t, int32_t> {{1, 15}, {2, 20}, {3, 30}});

    auto echo_string = server->FindFunc<string, string>(name("EchoString"));
    REQUIRE(echo_string);
    string text = "UTF-8 \xD0\x9C\xD0\xB8\xD1\x80";
    REQUIRE(echo_string.Call(text));
    CHECK(echo_string.GetResult() == text);

    auto echo_hash = server->FindFunc<hstring, hstring>(name("EchoHash"));
    REQUIRE(echo_hash);
    hstring hash = server->Hashes.to_hashed_string("RuntimeHash");
    REQUIRE(echo_hash.Call(hash));
    CHECK(echo_hash.GetResult() == hash);

    auto critter = server->CreateCritter(server->Hashes.to_hashed_string("UnitTestRat"), false);
    int32_t critter_init_calls = 0;
    int64_t last_critter_id = 0;
    REQUIRE(server->CallFunc(name("GetCritterInitCalls"), critter_init_calls));
    REQUIRE(server->CallFunc(name("GetLastCritterId"), last_critter_id));
    CHECK(critter_init_calls == 1);
    CHECK(last_critter_id == critter->GetId().underlying_value());

    auto critter_id = server->FindFunc<int64_t, ptr<Critter>>(name("GetCritterId"));
    REQUIRE(critter_id);
    REQUIRE(critter_id.Call(critter));
    CHECK(critter_id.GetResult() == critter->GetId().underlying_value());

    auto update_string = server->FindFunc<void, ptr<Critter>, ptr<Critter>, string&>(name("UpdateString"));
    REQUIRE(update_string);
    string updated_text = "value";
    REQUIRE(update_string.Call(critter, critter, updated_text));
    CHECK(updated_text == "same:value");

    auto item_null = server->FindFunc<bool, nptr<Item>>(name("IsItemNull"));
    REQUIRE(item_null);
    REQUIRE(item_null.Call(nptr<Item> {}));
    CHECK(item_null.GetResult());
    auto item = server->ItemMngr.CreateItem(server->Hashes.to_hashed_string("TestItem"), nullptr);
    REQUIRE(item_null.Call(item));
    CHECK_FALSE(item_null.GetResult());

    string exception_message;
    auto previous_exception_callback = exceptions::get_callback();
    exceptions::set_callback([&exception_message](string_view message, const stack_trace::catched_data&, bool) { exception_message = message; });
    auto restore_exception_callback = scope_exit([&previous_exception_callback]() noexcept { exceptions::set_callback(std::move(previous_exception_callback)); });
    auto throwing = server->FindFunc<void>(name("ThrowFromScript"));
    REQUIRE(throwing);
    CHECK_FALSE(throwing.Call());
    CHECK(exception_message.find("Managed runtime probe failure") != string::npos);
    exceptions::set_callback(std::move(previous_exception_callback));
    restore_exception_callback.release();
    server->Unlock();
    unlock.release();

    // A second live engine must initialize its own assembly statics and event subscriptions
    auto second_settings = ServerEngineTests::MakeServerTestSettings();
    BakerTests::OverrideSetting(second_settings.Network.DisableInterthreadCommunication, true);
    auto second_resources = ServerEngineTests::MakeServerTestResources();
    assemblies.AddTo(second_resources);
    auto second = safe_alloc::make_refcounted<ServerEngine>(&second_settings, std::move(second_resources));
    auto shutdown_second = scope_exit([&second]() noexcept { safe_call([&second] { second->Shutdown(); }); });
    startup_error = ServerEngineTests::WaitForServerStart(second);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(second->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock_second = scope_exit([&second]() noexcept { safe_call([&second] { second->Unlock(); }); });
    REQUIRE(second->CallFunc(second->Hashes.to_hashed_string("ManagedRuntimeProbe::GetInitCalls"), init_calls));
    CHECK(init_calls == 1);
    REQUIRE(second->CallFunc(second->Hashes.to_hashed_string("ManagedRuntimeProbe::GetCritterInitCalls"), critter_init_calls));
    CHECK(critter_init_calls == 0);
    second->Unlock();
    unlock_second.release();

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock_first_again = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
    REQUIRE(server->CallFunc(name("GetInitCalls"), init_calls));
    CHECK(init_calls == 1);
    REQUIRE(server->CallFunc(name("GetCritterInitCalls"), critter_init_calls));
    CHECK(critter_init_calls == 1);
    server->Unlock();
    unlock_first_again.release();

    auto client_settings = ClientEngineTests::MakeClientTestSettings();
    auto client_resources = ClientEngineTests::MakeClientTestResources();
    assemblies.AddTo(client_resources);
    auto client = ClientEngineTests::MakeClientEngine(client_settings, std::move(client_resources));
    auto shutdown_client = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    REQUIRE(client->CallFunc(client->Hashes.to_hashed_string("ManagedRuntimeProbe::GetInitCalls"), init_calls));
    CHECK(init_calls == 1);
    client->MainLoop();

    auto mapper_settings = MapperTests::MakeMapperTestSettings();
    auto mapper_resources = MapperTests::MakeMapperTestResources();
    assemblies.AddTo(mapper_resources);
    auto mapper = safe_alloc::make_refcounted<MapperEngine>(&mapper_settings, std::move(mapper_resources), &GetApp()->MainWindow);
    auto shutdown_mapper = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });
    REQUIRE(mapper->CallFunc(mapper->Hashes.to_hashed_string("ManagedRuntimeProbe::GetInitCalls"), init_calls));
    CHECK(init_calls == 1);
}

FO_END_NAMESPACE

#endif
