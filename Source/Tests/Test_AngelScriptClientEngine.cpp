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

#if FO_ANGELSCRIPT_SCRIPTING

#include "Test_ClientEngineHelpers.h"

FO_BEGIN_NAMESPACE

using namespace ClientEngineTests;

#if (FO_ANGELSCRIPT_SCRIPTING)
TEST_CASE("AngelScriptScriptDebuggerEndpointServesItsTcpPort")
{
    // The debugger was assumed to need an attached debugger client, but the endpoint server is ordinary
    // engine code: it binds a loopback port and runs its worker threads without anyone connecting
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings);

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    auto backend = GetScriptBackend(client.as_ptr());

    DebuggerEndpointServer debugger {backend};

    auto stop_debugger = scope_exit([&debugger]() noexcept { safe_call([&debugger] { debugger.Stop(); }); });

    CHECK_FALSE(debugger.IsPaused());

    // Events are queued for whichever client attaches later, so emitting without one must be harmless
    REQUIRE_NOTHROW(debugger.EmitEvent("unitTestEvent"));
    REQUIRE_NOTHROW(debugger.EmitEvent("unitTestEventWithBody", R"({"value":1})"));

    // Attaching a plain socket drives the accept path, the handshake write and the request reader. The
    // listener picks its port from the process id inside a fixed span, so the same arithmetic finds it
    REQUIRE(net_sockets::startup());

    constexpr uint16_t DEBUGGER_BASE_PORT = 43000;
    constexpr uint16_t DEBUGGER_PORT_SPAN = 2000;
    int32_t pid_num = strvex(platform::get_current_process_id_str()).to_int32();
    uint16_t start_offset = pid_num > 0 ? numeric_cast<uint16_t>(pid_num % DEBUGGER_PORT_SPAN) : uint16_t {0};

    tcp_socket client_sock;
    bool attached = false;

    for (uint16_t i = 0; i < 8 && !attached; i++) {
        uint16_t candidate = numeric_cast<uint16_t>(DEBUGGER_BASE_PORT + (start_offset + i) % DEBUGGER_PORT_SPAN);
        attached = client_sock.connect("127.0.0.1", candidate);
    }

    CHECK(attached);

    if (attached) {
        // The server greets a fresh client, then answers requests line by line
        std::this_thread::sleep_for(std::chrono::milliseconds {120});

        // Walk the request surface: every command has its own handler and response shape
        vector<string> requests {
            R"({"seq":1,"type":"request","command":"capabilities"})",
            R"({"seq":2,"type":"request","command":"setBreakpoints","arguments":{"source":{"path":"ClientEngineTest.fos"},"breakpoints":[{"line":10},{"line":20}]}})",
            R"({"seq":3,"type":"request","command":"stackTrace"})",
            R"({"seq":4,"type":"request","command":"variables","arguments":{"variablesReference":1}})",
            R"({"seq":5,"type":"request","command":"pause"})",
            R"({"seq":6,"type":"request","command":"next"})",
            R"({"seq":7,"type":"request","command":"stepIn"})",
            R"({"seq":8,"type":"request","command":"stepOut"})",
            R"({"seq":9,"type":"request","command":"continue"})",
            R"({"seq":10,"type":"request","command":"setBreakpoints","arguments":{"source":{"path":"ClientEngineTest.fos"},"breakpoints":[]}})",
            R"({"seq":11,"type":"request","command":"noSuchCommand"})",
            R"({"seq":12,"type":"request"})",
        };

        for (const string& request_body : requests) {
            string request = request_body;
            request += "\n";
            ignore_unused(client_sock.send({reinterpret_cast<const uint8_t*>(request.data()), request.size()}));
            std::this_thread::sleep_for(std::chrono::milliseconds {40});
        }

        std::this_thread::sleep_for(std::chrono::milliseconds {120});

        REQUIRE_NOTHROW(debugger.EmitEvent("attachedEvent"));

        std::this_thread::sleep_for(std::chrono::milliseconds {120});

        for (string_view malformed : {"not json\n", "{\n", "{\"seq\":\"notanumber\"}\n", "\n"}) {
            ignore_unused(client_sock.send({reinterpret_cast<const uint8_t*>(malformed.data()), malformed.size()}));
            std::this_thread::sleep_for(std::chrono::milliseconds {40});
        }

        string disconnect_request = R"({"seq":99,"type":"request","command":"disconnect"})";
        disconnect_request += "\n";
        ignore_unused(client_sock.send({reinterpret_cast<const uint8_t*>(disconnect_request.data()), disconnect_request.size()}));

        std::this_thread::sleep_for(std::chrono::milliseconds {120});

        client_sock.close();
    }

    // Let the worker and discovery threads take at least one more pass over their loops
    std::this_thread::sleep_for(std::chrono::milliseconds {120});

    CHECK_FALSE(debugger.IsPaused());

    REQUIRE_NOTHROW(debugger.Stop());
    REQUIRE_NOTHROW(debugger.Stop());
    stop_debugger.release();
}
#endif

TEST_CASE("AngelScriptClientEngineScriptModuleInitAndLoopAreCallable")
{
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings);

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    auto get_func_name = [&client](string_view name) { return client->Hashes.to_hashed_string(name); };

    int start_calls = 0;
    int loop_calls = 0;
    int manual_calls = 0;

    REQUIRE(client->CallFunc(get_func_name("ClientEngineTest::UnitTestGetStartCalls"), start_calls));
    REQUIRE(client->CallFunc(get_func_name("ClientEngineTest::UnitTestGetLoopCalls"), loop_calls));
    REQUIRE(client->CallFunc(get_func_name("ClientEngineTest::UnitTestGetManualCalls"), manual_calls));

    CHECK(start_calls == 1);
    CHECK(loop_calls == 0);
    CHECK(manual_calls == 0);

    REQUIRE(client->CallFunc(get_func_name("ClientEngineTest::UnitTestMarkManualCall")));
    REQUIRE(client->CallFunc(get_func_name("ClientEngineTest::UnitTestGetManualCalls"), manual_calls));
    CHECK(manual_calls == 1);

    client->MainLoop();
    client->MainLoop();

    REQUIRE(client->CallFunc(get_func_name("ClientEngineTest::UnitTestGetLoopCalls"), loop_calls));
    CHECK(loop_calls >= 2);
}

TEST_CASE("AngelScriptClientEngineMethodRefTypeOps")
{
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings);

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    auto get_func_name = [&client](string_view name) { return client->Hashes.to_hashed_string(name); };

    int32_t result = 0;
    REQUIRE(client->CallFunc(get_func_name("ClientEngineTest::UnitTestMapSpriteHolderRefType"), result));
    CHECK(result == 0);
}

TEST_CASE("AngelScriptClientEngineGlobalScriptBindings")
{
    auto settings = MakeClientTestSettings();
    auto client_resources = MakeUnitTestFontResources();
    client_resources.emplace_back("Quad.png", BakerTests::MakeMinimalBakedSprite(2, 2));
    auto client = MakeClientEngine(settings, MakeClientTestResources(std::move(client_resources)));

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    // The sweep below dumps the atlases, so the directories it writes are cleared once the case is done
    set<string> tex_dumps_before = TexDumpArtifacts::CollectDumpDirs();

    auto remove_tex_dumps = scope_exit([&tex_dumps_before]() noexcept { safe_call([&tex_dumps_before] { TexDumpArtifacts::RemoveNewDumpDirs(tex_dumps_before); }); });

    // The scripts below change the resolution, which writes through to the process-global app window and would
    // otherwise leave every later test computing ratios against the changed size
    isize32 saved_screen_size = GetApp()->MainWindow.GetScreenSize();
    auto restore_screen_size = scope_exit([saved_screen_size]() noexcept { safe_call([saved_screen_size] { GetApp()->MainWindow.SetScreenSize(saved_screen_size); }); });

    auto run_script = [&client](string_view name) {
        int32_t result = -1;
        INFO(name);
        REQUIRE(client->CallFunc(client->Hashes.to_hashed_string(name), result));
        CHECK(result == 0);
    };

    run_script("ClientEngineTest::UnitTestClientStateQueries");

    run_script("ClientEngineTest::UnitTestClientGeometry");
    run_script("ClientEngineTest::UnitTestClientTextAndCache");
    run_script("ClientEngineTest::UnitTestClientInputSimulation");
    run_script("ClientEngineTest::UnitTestClientSpriteApi");
    run_script("ClientEngineTest::UnitTestClientEffectsAndText");
    run_script("ClientEngineTest::UnitTestClientWindowAndConfig");
    run_script("ClientEngineTest::UnitTestClientRejectsBadArguments");

    int32_t rejection_count = 0;
    REQUIRE(client->CallFunc(client->Hashes.to_hashed_string("ClientEngineTest::UnitTestGetClientRejectionCount"), rejection_count));
    // Only five probes must reject; the rest legitimately answer instead of throwing, reporting a bool or a zero
    // sound handle, queueing nothing, or accepting a pack that resolves to no entries
    CHECK(rejection_count == 8 + 16 + 128 + 256 + 512);
}

TEST_CASE("AngelScriptClientEngineRunsMainLoopHeadlessly")
{
    // The ImGui sweep below writes under `Workspace/`, relative to whatever directory the binary was launched
    // from, and `ImGui::LogToFile` asserts on a file it cannot open — aborting the frame mid-sweep
    (void)fs::create_directories("Workspace");

    auto settings = MakeClientTestSettings();
    auto client_resources = MakeUnitTestFontResources();
    client_resources.emplace_back("Quad.png", BakerTests::MakeMinimalBakedSprite(2, 2));

    for (auto& [effect_path, effect_data] : MakeBakedEffectResources("Effects/UnitTestOffscreen.fofx")) {
        client_resources.emplace_back(effect_path, effect_data);
    }

#if FO_ENABLE_3D
    // The baked model makes the 3D critter draw bindings reachable from the render pass
    vector<uint8_t> model_mesh = MakeRuntimeModelTriangleMesh();
    client_resources.emplace_back("Models/RuntimeInstance.fbx", model_mesh);
    client_resources.emplace_back("Models/RuntimeInstance.fo3d", MakeRuntimeModelDescription("Models/RuntimeInstance.fo3d", "Models/RuntimeInstance.fbx", model_mesh));
    client_resources.emplace_back("ModelAnimationInfo.foinfo", MakeUnitTestModelAnimationInfo("Models/RuntimeInstance.fo3d"));
#endif
    auto client = MakeClientEngine(settings, MakeClientTestResources(std::move(client_resources)));

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    REQUIRE(ImGui::GetCurrentContext() == nullptr);
    ImGuiExt::Init();

    auto destroy_context = scope_exit([]() noexcept {
        safe_call([] {
            if (ImGui::GetCurrentContext() != nullptr) {
                ImGui::DestroyContext();
            }
        });
    });

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2 {1280.0f, 720.0f};
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    // A disconnected client still runs its whole frame: timers, scheduled callbacks, script loop
    // events and the interface render pass
    for (int32_t frame = 0; frame < 3; frame++) {
        ImGui::NewFrame();
        REQUIRE_NOTHROW(client->MainLoop());
        ImGui::Render();
    }

    int32_t loop_calls = 0;
    REQUIRE(client->CallFunc(client->Hashes.to_hashed_string("ClientEngineTest::UnitTestGetLoopCalls"), loop_calls));
    CHECK(loop_calls > 0);

    int32_t render_calls = 0;
    REQUIRE(client->CallFunc(client->Hashes.to_hashed_string("ClientEngineTest::UnitTestGetRenderCalls"), render_calls));
    CHECK(render_calls > 0);

    // Re-run with script drawing enabled so the render-pass-only draw bindings execute
    REQUIRE(client->CallFunc<void>(client->Hashes.to_hashed_string("ClientEngineTest::UnitTestEnableRenderDrawing")));

    for (int32_t frame = 0; frame < 2; frame++) {
        ImGui::NewFrame();
        REQUIRE_NOTHROW(client->MainLoop());
        ImGui::Render();
    }

    // An empty label must surface as a script exception rather than an unaddressable widget; the sweep runs inside
    // the render pass because the accessor exists only while a frame is open
    int32_t probe_count = 0;
    int32_t rejections = 0;
    REQUIRE(client->CallFunc(client->Hashes.to_hashed_string("ClientEngineTest::UnitTestImGuiEmptyIdProbeCount"), probe_count));
    REQUIRE(client->CallFunc(client->Hashes.to_hashed_string("ClientEngineTest::UnitTestGetImGuiEmptyIdRejections"), rejections));
    CHECK(probe_count > 0);
    CHECK(rejections > 0);
    CHECK(rejections % probe_count == 0);

    int32_t sprite_rejections = 0;
    REQUIRE(client->CallFunc(client->Hashes.to_hashed_string("ClientEngineTest::UnitTestGetImGuiSpriteRejections"), sprite_rejections));
    CHECK(sprite_rejections == 4);

    // The surface guard keeps ImGui balanced when a probe throws, but a throw still means a binding
    // misbehaved - the engine swallows it inside the event, so it is only visible through this counter
    int32_t surface_failures = 0;
    REQUIRE(client->CallFunc(client->Hashes.to_hashed_string("ClientEngineTest::UnitTestGetImGuiSurfaceFailures"), surface_failures));

    string failed_stage;
    REQUIRE(client->CallFunc(client->Hashes.to_hashed_string("ClientEngineTest::UnitTestGetImGuiFailedStage"), failed_stage));
    INFO("last ImGui stage entered before the throw: " << failed_stage);
    CHECK(surface_failures == 0);

    // Input events must be safe to feed outside of a session too
    InputEvent move;
    move.Type = InputEvent::EventType::MouseMoveEvent;
    move.MouseMove.MouseX = 40;
    move.MouseMove.MouseY = 40;
    REQUIRE_NOTHROW(client->ProcessInputEvent(move));

    InputEvent key_down;
    key_down.Type = InputEvent::EventType::KeyDownEvent;
    key_down.KeyDown.Code = KeyCode::A;
    key_down.KeyDown.Text = "a";
    REQUIRE_NOTHROW(client->ProcessInputEvent(key_down));

    InputEvent key_up;
    key_up.Type = InputEvent::EventType::KeyUpEvent;
    key_up.KeyUp.Code = KeyCode::A;
    REQUIRE_NOTHROW(client->ProcessInputEvent(key_up));
}

FO_END_NAMESPACE

#endif

