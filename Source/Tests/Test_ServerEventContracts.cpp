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

#include "catch_amalgamated.hpp"

#include "Common.h"

FO_BEGIN_NAMESPACE

namespace
{
    struct EventCallContract
    {
        string_view Source {};
        string_view Text {};
        size_t Occurrence {};
        string_view Contract {};
    };

    struct EventCallSite
    {
        string Source {};
        string Text {};
        size_t Occurrence {};
    };

    static constexpr EventCallContract ExpectedServerEventCalls[] = {
        {"Source/Scripting/ServerCritterScriptMethods.cpp", "self->GetEngine()->OnCritterItemMoved.Fire(self, item, from_slot);", 1, "post-commit item move"},
        {"Source/Scripting/ServerCritterScriptMethods.cpp", "self->GetEngine()->OnCritterItemMoved.Fire(self, item_swap, slot);", 1, "post-commit item swap"},
        {"Source/Scripting/ServerCritterScriptMethods.cpp", "self->GetEngine()->OnCritterItemMoved.Fire(self, item, from_slot);", 2, "post-commit item move"},
        {"Source/Server/CritterManager.cpp", "_engine->OnCritterItemMoved.Fire(cr, item, CritterItemSlot::Outside);", 1, "post-commit inventory add"},
        {"Source/Server/CritterManager.cpp", "_engine->OnCritterItemMoved.Fire(cr, item, prev_slot);", 1, "post-commit inventory remove"},
        {"Source/Server/CritterManager.cpp", "_engine->OnMapCritterIn.Fire(map, cr);", 1, "post-attach critter create"},
        {"Source/Server/CritterManager.cpp", "_engine->OnCritterFinish.Fire(cr);", 1, "pre-teardown critter finish"},
        {"Source/Server/EntityManager.cpp", "_engine->OnLocationInit.Fire(loc, first_time);", 1, "init guarded"},
        {"Source/Server/EntityManager.cpp", "_engine->OnMapInit.Fire(map, first_time);", 1, "init guarded"},
        {"Source/Server/EntityManager.cpp", "if (_engine->OnCritterPreLoad.Fire(cr) == Entity::EventResult::StopChain) {", 1, "post-restore pre-attach migration gate"},
        {"Source/Server/EntityManager.cpp", "_engine->OnCritterInit.Fire(cr, first_time);", 1, "init guarded"},
        {"Source/Server/EntityManager.cpp", "_engine->OnItemInit.Fire(item, first_time);", 1, "init guarded"},
        {"Source/Server/ItemManager.cpp", "_engine->OnItemFinish.Fire(item);", 1, "pre-teardown item finish"},
        {"Source/Server/Map.cpp", "cr->OnItemOnMapAppeared.Fire(item, dropper);", 1, "visibility item appeared"},
        {"Source/Server/Map.cpp", "cr->OnItemOnMapDisappeared.Fire(item, nullptr);", 1, "visibility item disappeared"},
        {"Source/Server/Map.cpp", "cr->OnItemOnMapChanged.Fire(item);", 1, "visibility item changed"},
        {"Source/Server/Map.cpp", "cr->OnItemOnMapDisappeared.Fire(item, nullptr);", 2, "visibility item disappeared"},
        {"Source/Server/Map.cpp", "cr->OnItemOnMapAppeared.Fire(item, nullptr);", 1, "visibility item appeared"},
        {"Source/Server/Map.cpp", "_engine->OnStaticItemWalk.Fire(static_item, cr, false, dir);", 1, "walk trigger guarded"},
        {"Source/Server/Map.cpp", "_engine->OnStaticItemWalk.Fire(static_item, cr, true, dir);", 1, "walk trigger guarded"},
        {"Source/Server/Map.cpp", "item->OnCritterWalk.Fire(cr, false, dir);", 1, "walk trigger guarded"},
        {"Source/Server/Map.cpp", "item->OnCritterWalk.Fire(cr, true, dir);", 1, "walk trigger guarded"},
        {"Source/Server/MapManager.cpp", "loc->OnMapAdded.Fire(map);", 1, "post-create map add"},
        {"Source/Server/MapManager.cpp", "_engine->OnLocationFinish.Fire(loc);", 1, "pre-teardown location finish"},
        {"Source/Server/MapManager.cpp", "_engine->OnMapFinish.Fire(map);", 1, "pre-teardown map finish in location"},
        {"Source/Server/MapManager.cpp", "_engine->OnMapFinish.Fire(map);", 2, "pre-teardown map finish"},
        {"Source/Server/MapManager.cpp", "loc_ptr->OnMapRemoved.Fire(map);", 1, "pre-teardown map remove"},
        {"Source/Server/MapManager.cpp", "_engine->OnMapCritterIn.Fire(map, cr);", 1, "post-attach transfer in"},
        {"Source/Server/MapManager.cpp", "_engine->OnGlobalMapCritterIn.Fire(cr);", 1, "post-attach transfer in"},
        {"Source/Server/MapManager.cpp", "_engine->OnCritterSendInitialInfo.Fire(cr);", 1, "post-transfer initial info"},
        {"Source/Server/MapManager.cpp", "_engine->OnCritterTransfer.Fire(cr, prev_map_ref);", 1, "post-transfer final notification"},
        {"Source/Server/MapManager.cpp", "_engine->OnMapCritterOut.Fire(map, cr);", 1, "pre-detach transfer out"},
        {"Source/Server/MapManager.cpp", "other->OnCritterDisappeared.Fire(cr);", 1, "pre-detach observer notification"},
        {"Source/Server/MapManager.cpp", "_engine->OnGlobalMapCritterOut.Fire(cr);", 1, "pre-detach global out"},
        {"Source/Server/MapManager.cpp", "cr->OnCritterAppeared.Fire(target);", 1, "visibility critter appeared"},
        {"Source/Server/MapManager.cpp", "cr->OnCritterVisibilityModeChanged.Fire(target, vis_mode);", 1, "visibility mode changed"},
        {"Source/Server/MapManager.cpp", "cr->OnCritterDisappeared.Fire(target);", 1, "visibility critter disappeared"},
        {"Source/Server/MapManager.cpp", "cr->OnCritterAppearedDist1.Fire(target);", 1, "visibility distance group"},
        {"Source/Server/MapManager.cpp", "cr->OnCritterDisappearedDist1.Fire(target);", 1, "visibility distance group"},
        {"Source/Server/MapManager.cpp", "cr->OnCritterAppearedDist2.Fire(target);", 1, "visibility distance group"},
        {"Source/Server/MapManager.cpp", "cr->OnCritterDisappearedDist2.Fire(target);", 1, "visibility distance group"},
        {"Source/Server/MapManager.cpp", "cr->OnCritterAppearedDist3.Fire(target);", 1, "visibility distance group"},
        {"Source/Server/MapManager.cpp", "cr->OnCritterDisappearedDist3.Fire(target);", 1, "visibility distance group"},
        {"Source/Server/MapManager.cpp", "cr->OnItemOnMapAppeared.Fire(item, nullptr);", 1, "visibility item appeared"},
        {"Source/Server/MapManager.cpp", "cr->OnItemOnMapDisappeared.Fire(item, nullptr);", 1, "visibility item disappeared"},
        {"Source/Server/Server.cpp", "if (OnInit.Fire() == EventResult::StopChain) {", 1, "startup gate"},
        {"Source/Server/Server.cpp", "if (OnGenerateWorld.Fire() == EventResult::StopChain) {", 1, "startup gate"},
        {"Source/Server/Server.cpp", "if (OnStart.Fire() == EventResult::StopChain) {", 1, "startup gate"},
        {"Source/Server/Server.cpp", "OnFinish.Fire();", 1, "server finish"},
        {"Source/Server/Server.cpp", "OnPlayerLogout.Fire(player);", 1, "pre-teardown player logout"},
        {"Source/Server/Server.cpp", "OnGlobalMapCritterIn.Fire(cr);", 2, "post-attach critter create"},
        {"Source/Server/Server.cpp", "OnGlobalMapCritterIn.Fire(cr);", 1, "post-attach critter load"},
        {"Source/Server/Server.cpp", "OnCritterLoad.Fire(cr);", 1, "post-load guarded"},
        {"Source/Server/Server.cpp", "OnCritterUnload.Fire(cr);", 1, "pre-unload notification"},
        {"Source/Server/Server.cpp", "OnPlayerCritterSwitched.Fire(player, cr, prev_cr);", 1, "post-switch notification"},
        {"Source/Server/Server.cpp", "OnCritterSendInitialInfo.Fire(cr);", 1, "post-send initial info"},
        {"Source/Server/Server.cpp", "const EventResult login_result = OnPlayerLogin.Fire(player, nullptr);", 1, "player login gate"},
        {"Source/Server/Server.cpp", "const EventResult login_result = OnPlayerLogin.Fire(player, nullptr);", 2, "player login gate"},
        {"Source/Server/Server.cpp", "const EventResult login_result = OnPlayerLogin.Fire(player, unlogined_player);", 1, "player relogin gate"},
        {"Source/Server/Server.cpp", "const EventResult login_result = OnPlayerLogin.Fire(player, nullptr);", 3, "player login gate"},
        {"Source/Server/Server.cpp", "const EventResult move_result = OnPlayerMoveCritter.Fire(player, cr.as_ptr(), corrected_speed);", 1, "player movement gate"},
        {"Source/Server/Server.cpp", "const EventResult move_result = OnPlayerMoveCritter.Fire(player, cr.as_ptr(), zero_speed);", 1, "player movement gate"},
        {"Source/Server/Server.cpp", "const EventResult dir_result = OnPlayerDirCritter.Fire(player, cr, checked_dir);", 1, "player direction gate"},
        {"Source/Server/Server.cpp", "OnCritterMoved.Fire(cr, old_hex);", 1, "post-move guarded"},
        {"Source/Server/Server.cpp", "const EventResult dir_result = OnPlayerDirCritter.Fire(player, cr.as_ptr(), checked_dir);", 1, "player direction gate"},
        {"Source/Server/Server.cpp", "OnCritterMoved.Fire(cr, old_hex);", 2, "post-move guarded"},
        {"Source/Server/Server.cpp", "OnCritterStartMoving.Fire(cr, was_moving);", 1, "post-start-moving notification"},
        {"Source/Server/Server.cpp", "OnCritterStopMoving.Fire(cr);", 1, "post-stop-moving notification"},
        {"Source/Server/Server.cpp", "OnCritterStartMoving.Fire(cr, true);", 1, "post-speed-change notification"},
    };

    [[nodiscard]] static auto Trim(string_view text) noexcept -> string_view
    {
        size_t begin = 0;

        while (begin < text.size() && (text[begin] == ' ' || text[begin] == '\t')) {
            begin++;
        }

        size_t end = text.size();

        while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\r')) {
            end--;
        }

        return text.substr(begin, end - begin);
    }

    [[nodiscard]] static auto FindEngineRoot() -> std::filesystem::path
    {
        std::filesystem::path file_path = std::filesystem::absolute(std::filesystem::path {__FILE__});

        for (std::filesystem::path path = file_path.parent_path(); !path.empty();) {
            if (std::filesystem::exists(path / "Source" / "Server" / "Server.cpp")) {
                return path;
            }

            const std::filesystem::path parent_path = path.parent_path();
            if (parent_path == path) {
                break;
            }

            path = parent_path;
        }

        std::filesystem::path current_path = std::filesystem::current_path();

        for (std::filesystem::path path = current_path; !path.empty();) {
            if (std::filesystem::exists(path / "Engine" / "Source" / "Server" / "Server.cpp")) {
                return path / "Engine";
            }
            if (std::filesystem::exists(path / "Source" / "Server" / "Server.cpp")) {
                return path;
            }

            const std::filesystem::path parent_path = path.parent_path();
            if (parent_path == path) {
                break;
            }

            path = parent_path;
        }

        FAIL("Unable to locate engine root");
        return {};
    }

    [[nodiscard]] static auto ReadSourceFile(const std::filesystem::path& path) -> string
    {
        std::ifstream file {path, std::ios::binary};
        REQUIRE(file.is_open());

        return {std::istreambuf_iterator<char> {file}, std::istreambuf_iterator<char> {}};
    }

    static void AddEventCallsFromSource(vector<EventCallSite>& calls, const std::filesystem::path& engine_root, const std::filesystem::path& source_path)
    {
        const std::string std_source = source_path.lexically_relative(engine_root).generic_string();
        const string source {std_source.begin(), std_source.end()};
        const string content = ReadSourceFile(source_path);

        size_t line_begin = 0;

        while (line_begin <= content.size()) {
            size_t line_end = content.find('\n', line_begin);

            if (line_end == string::npos) {
                line_end = content.size();
            }

            const string_view line = string_view {content}.substr(line_begin, line_end - line_begin);
            const string_view text = Trim(line);

            if (text.find(".Fire(") != string_view::npos) {
                size_t occurrence = 1;

                for (const EventCallSite& call : calls) {
                    if (call.Source == source && call.Text == text) {
                        occurrence++;
                    }
                }

                calls.emplace_back(EventCallSite {source, string {text}, occurrence});
            }

            if (line_end == content.size()) {
                break;
            }

            line_begin = line_end + 1;
        }
    }

    [[nodiscard]] static auto CollectServerEventCalls() -> vector<EventCallSite>
    {
        const std::filesystem::path engine_root = FindEngineRoot();
        vector<std::filesystem::path> sources;

        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(engine_root / "Source" / "Server")) {
            if (entry.is_regular_file() && entry.path().extension() == ".cpp") {
                sources.emplace_back(entry.path());
            }
        }

        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(engine_root / "Source" / "Scripting")) {
            if (entry.is_regular_file() && entry.path().extension() == ".cpp" && entry.path().filename().generic_string().starts_with("Server")) {
                sources.emplace_back(entry.path());
            }
        }

        std::sort(sources.begin(), sources.end());

        vector<EventCallSite> calls;

        for (const std::filesystem::path& source : sources) {
            AddEventCallsFromSource(calls, engine_root, source);
        }

        return calls;
    }

    [[nodiscard]] static auto MakeKey(string_view source, string_view text, size_t occurrence) -> string
    {
        string key;
        key.reserve(source.size() + text.size() + 32);
        key += source;
        key += "|";
        key += std::to_string(occurrence);
        key += "|";
        key += text;
        return key;
    }
}

TEST_CASE("ServerEventCallContracts")
{
    const vector<EventCallSite> actual_calls = CollectServerEventCalls();

    set<string> actual_keys;

    for (const EventCallSite& call : actual_calls) {
        const string key = MakeKey(call.Source, call.Text, call.Occurrence);
        const bool inserted = actual_keys.emplace(key).second;
        CHECK(inserted);
    }

    set<string> expected_keys;

    for (const EventCallContract& expected : ExpectedServerEventCalls) {
        CHECK_FALSE(expected.Source.empty());
        CHECK_FALSE(expected.Text.empty());
        CHECK_FALSE(expected.Contract.empty());

        const string key = MakeKey(expected.Source, expected.Text, expected.Occurrence);
        const bool inserted = expected_keys.emplace(key).second;
        CHECK(inserted);
    }

    vector<string> unexpected_calls;

    for (const string& key : actual_keys) {
        if (!expected_keys.count(key)) {
            unexpected_calls.emplace_back(key);
        }
    }

    vector<string> missing_calls;

    for (const string& key : expected_keys) {
        if (!actual_keys.count(key)) {
            missing_calls.emplace_back(key);
        }
    }

    string unexpected_report = "Unexpected server event calls:";
    for (const string& key : unexpected_calls) {
        unexpected_report += "\n  ";
        unexpected_report += key;
    }
    INFO(unexpected_report);
    CHECK(unexpected_calls.empty());

    string missing_report = "Missing server event calls:";
    for (const string& key : missing_calls) {
        missing_report += "\n  ";
        missing_report += key;
    }
    INFO(missing_report);
    CHECK(missing_calls.empty());
}

FO_END_NAMESPACE
