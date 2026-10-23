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

#pragma once

#include "Common.h"

#include "catch_amalgamated.hpp"

#include "AngelScriptDebugger.h"
#include "AngelScriptHelpers.h"
#include "AngelScriptScripting.h"
#include "AnimationViewer.h"
#include "Application.h"
#include "Baker.h"
#include "Client.h"
#include "CritterView.h"
#include "DataSerialization.h"
#include "DefaultSprites.h"
#include "EffectBaker.h"
#include "ImGuiStuff.h"
#include "MapView.h"
#include "MetadataRegistration.h"
#include "ModelAnimationData.h"
#include "ModelInfoBaker.h"
#include "ModelManager.h"
#include "ModelMeshBaker.h"
#include "ModelMeshData.h"
#include "ModelSourceLoader.h"
#include "ModelSprites.h"
#include "PlayerView.h"
#include "ResourceIndex.h"
#include "SettingsStorage.h"
#include "Test_BakerHelpers.h"
#include "Test_DumpArtifacts.h"

FO_BEGIN_NAMESPACE

namespace ClientEngineTests
{
    struct RecordedQuadDraw
    {
        vector<Vertex2D> Vertices {};
        vector<vindex_t> Indices {};
        size_t StartIndex {};
        optional<size_t> IndicesToDraw {};
        nptr<const RenderTexture> CustomTexture {};
    };

    inline auto MakeRecordingQuadEffectLoader() -> RenderEffectLoader
    {
        return [](string_view name) -> string {
            if (name == "Effects/Test_Recording.fofx") {
                return "[Effect]\nPasses = 1\n";
            }

            if (name == "Effects/Test_Recording.fofx-1-info") {
                return "[EffectInfo]\nMainTex = 0\nSpriteBorderBuf = 1\n";
            }

            throw GenericException("Unexpected recording effect request", name);
        };
    }

    class RecordingQuadEffect final : public RenderEffect
    {
    public:
        RecordingQuadEffect() :
            RenderEffect(EffectUsage::QuadSprite, "Effects/Test_Recording.fofx", MakeRecordingQuadEffectLoader())
        {
        }

        void DrawBuffer(ptr<RenderDrawBuffer> dbuf, size_t start_index, optional<size_t> indices_to_draw, nptr<const RenderTexture> custom_tex) override
        {
            RecordedQuadDraw draw;
            draw.Vertices.assign(dbuf->Vertices.begin(), dbuf->Vertices.begin() + numeric_cast<ptrdiff_t>(dbuf->VertCount));
            draw.Indices.assign(dbuf->Indices.begin(), dbuf->Indices.begin() + numeric_cast<ptrdiff_t>(dbuf->IndCount));
            draw.StartIndex = start_index;
            draw.IndicesToDraw = indices_to_draw;
            draw.CustomTexture = custom_tex;
            Draws.emplace_back(std::move(draw));
        }

        vector<RecordedQuadDraw> Draws {};
    };

    inline auto MakeClientTestSettings() -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedClientSettings(settings);

        return settings;
    }

    inline auto MakeTempClientResourceDir(string_view name) -> string
    {
        std::filesystem::path base = std::filesystem::temp_directory_path() / std::format("lf_client_resources_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count());
        return fs::path_to_string(base);
    }

    inline auto CanUseDirectoryBackedClientResourceFixtures() noexcept -> bool
    {
        return !IsPackaged();
    }

    inline void WriteClientTestPack(string_view directory, string_view name, const vector<pair<string, string>>& files)
    {
        REQUIRE(fs::create_directories(directory));
        ResourcePackWriter writer {strex(directory).combine_path(strex("{}.fores", name)).str()};

        for (const auto& [path, contents] : files) {
            writer.AddFile(path, {reinterpret_cast<const uint8_t*>(contents.data()), contents.size()});
        }

        writer.Finish();
    }

#if FO_ANGELSCRIPT_SCRIPTING
    inline auto MakeClientScriptBinary(const FileSystem& metadata_resources) -> vector<uint8_t>
    {
        BakerClientEngine compiler_engine {metadata_resources};

        return BakerTests::CompileInlineScripts(&compiler_engine, "ClientEngineScripts",
            {
                {"Scripts/ClientEngineTest.fos",
                    R"(
namespace ClientEngineTest
{
    int StartCalls = 0;
    int LoopCalls = 0;
    int ManualCalls = 0;

    int RenderCalls = 0;
    bool DrawDuringRender = false;

    [[ModuleInit]]
    void InitClientEngineTest()
    {
        Game.OnStart.Subscribe(OnStart);
        Game.OnLoop.Subscribe(OnLoop);
        Game.OnRenderIface.Subscribe(OnRenderIface);
        Game.OnCritterAnimationSprite.Subscribe(ResolveSprite);
    }

    [[Event]]
    void OnRenderIface()
    {
        RenderCalls++;

        if (!DrawDuringRender) {
            return;
        }

        // The 2D drawing surface is only legal inside the interface render pass
        // DrawPrimitive takes a flat int stream of x/y/colour triples
        int[] primitive = {0, 0, 0xFFFF0000, 10, 10, 0xFF00FF00};
        Game.DrawPrimitive(RenderPrimitiveType::LineList, primitive);

        Game.PushDrawScissor(ipos(0, 0), isize(100, 100));
        Game.PopDrawScissor();

        DrawSpritesAndText();
        DrawCritterPreviews();
        DrawImGuiSurface();
    }

    void DrawCritterPreviews()
    {
        // The 2d critter draw resolves its frames through the animation resolver and simply draws nothing
        // when the model has none, which is the path a client takes for an unknown critter
        Game.DrawCritter2d("Models/RuntimeInstance.fo3d".hstr(), CritterStateAnim(1), CritterActionAnim(1), mdir(0), 0, 0, 64, 64, false, true, ucolor(255, 255, 255, 255));

        // Game.DrawCritter3d needs a model sprite, and building one through the interface atlas fails to
        // converge without real atlas storage, so only the 2d path runs here
    }

    void DrawSpritesAndText()
    {
        uint spr = Game.LoadSprite("Quad.png");

        if (spr != 0) {
            Game.DrawSprite(spr, ipos(10, 10));
            Game.DrawSprite(spr, ipos(10, 10), ucolor(255, 255, 255, 255), true);
            Game.DrawSprite(spr, fpos(10.0f, 10.0f), ucolor(255, 255, 255, 255));
            Game.DrawSprite(spr, fpos(10.0f, 10.0f), fsize(20.0f, 20.0f), ucolor(255, 255, 255, 255));
            Game.DrawSprite(spr, ipos(10, 10), isize(20, 20));
            Game.DrawSprite(spr, ipos(10, 10), isize(20, 20), ucolor(255, 255, 255, 255), false, true);
            Game.DrawSpritePattern(spr, ipos(0, 0), isize(40, 40), isize(4, 4), ucolor(255, 255, 255, 255));
            Game.DrawSpriteRegion(spr, fpos(0.0f, 0.0f), fpos(1.0f, 1.0f), ipos(0, 0), isize(20, 20));
            Game.FreeSprite(spr);
        }

        Game.BindFont(FontType::Default, "UnitTestFont.fofnt");
        // Registering an offscreen effect is what makes the offscreen surface pair usable
        Game.SetEffect(EffectType::Offscreen, 0, "Effects/UnitTestOffscreen.fofx");

        Game.ActivateOffscreenSurface(true);
        Game.PresentOffscreenSurface(0);
        Game.ActivateOffscreenSurface(false);
        Game.PresentOffscreenSurface(0, ipos(0, 0), isize(50, 50));
        Game.ActivateOffscreenSurface(false);
        Game.PresentOffscreenSurface(0, ipos(0, 0), isize(50, 50), 0.0f, 0.0f, 0.0f, 0.0f);
        Game.ActivateOffscreenSurface(false);
        Game.PresentOffscreenSurface(0, 0, 0, 50, 50, 0, 0, 50, 50);

        Game.DrawText("drawn text", ipos(0, 0), isize(200, 40), ucolor(255, 255, 255, 255), TextFormat());

        TextFormat wrappedDraw;
        wrappedDraw.Flags = FontFlag(uint(FontFlag::CenterX) | uint(FontFlag::Bordered));
        Game.DrawText("a longer drawn sentence that wraps", ipos(0, 40), isize(60, 60), ucolor(200, 200, 200, 255), wrappedDraw);
    }

    void DrawImGuiSurface()
    {
        if (!Game.IsImGuiAvailable()) {
            return;
        }

        ImGui.SetNextWindowPos(ipos(20, 20), ImGui_Cond::Always);
        ImGui.SetNextWindowSize(isize(400, 300), ImGui_Cond::Always);
        ImGui.SetNextWindowBgAlpha(0.9f);
        ImGui.SetNextWindowCollapsed(false, ImGui_Cond::Always);
        ImGui.SetNextWindowFocus();

        if (ImGui.Begin("ScriptImGuiCoverage", ImGui_WindowFlags::None)) {
            // The engine swallows anything escaping OnRenderIface, so a throw would skip ImGui.End and abort the
            // next render instead of failing here; catching keeps the window balanced and records the failure
            try {
                ImGui.Text("plain text");
                ImGui.TextDisabled("disabled text");
                ImGui.TextWrapped("wrapped text that is long enough to actually wrap inside the window");
                ImGui.AlignTextToFramePadding();

                ImGui.PushID(1);
                ImGui.PopID();
                ImGui.PushID("named");
                ImGui.PopID();

                ImGui.PushStyleColor(ImGui_Col::Text, 1.0f, 1.0f, 1.0f, 1.0f);
                ImGui.PopStyleColor(1);
                ImGui.PushStyleVar(ImGui_StyleVar::Alpha, 1.0f);
                ImGui.PopStyleVar(1);

                ImGui.Separator();
                ImGui.SameLine();
                ImGui.Spacing();
                ImGui.NewLine();
                ImGui.Indent();
                ImGui.Unindent();

                if (ImGui.Button("button")) {
                }

                ImGui.SmallButton("small");
                ImGui.Bullet();
                ImGui.BulletText("bullet text");

                // Geometry and state queries answer inside a live window
                fsize textSize = ImGui.CalcTextSize("measure me");
                fpos windowPos = ImGui.GetWindowPos();
                fsize windowSize = ImGui.GetWindowSize();

                float scrollX = ImGui.GetScrollX();
                float scrollY = ImGui.GetScrollY();
                ImGui.SetScrollX(scrollX);
                ImGui.SetScrollY(scrollY);
                float maxX = ImGui.GetScrollMaxX();
                float maxY = ImGui.GetScrollMaxY();

                // Skipped by branching, never by returning: the matching ImGui.End must still run, or the next
                // render aborts the whole frame over the unbalanced window
                bool geometryIsSane = textSize.width >= 0.0f && windowPos.x >= -100000.0f && windowSize.width >= 0.0f && maxX >= 0.0f && maxY >= 0.0f;

                if (geometryIsSane) {
                    ImGui.GetTextLineHeight();
                    ImGui.GetTextLineHeightWithSpacing();
                    ImGui.GetFrameHeight();
                    ImGui.GetFrameHeightWithSpacing();
                    ImGui.GetWindowWidth();
                    ImGui.GetWindowHeight();
                    ImGui.GetTime();
                    ImGui.GetFrameCount();
                    ImGui.IsWindowAppearing();
                    ImGui.IsAnyItemHovered();
                    ImGui.IsAnyItemActive();

                    ImGuiStage = "layout";
                    DrawImGuiLayout();
                    ImGuiStage = "input widgets";
                    DrawImGuiInputWidgets();
                    ImGuiStage = "containers";
                    DrawImGuiContainers();
                    ImGuiStage = "item queries";
                    DrawImGuiItemQueries();
                    ImGuiStage = "trees and selectables";
                    DrawImGuiTreesAndSelectables();
                    ImGuiStage = "tooltips and popups";
                    DrawImGuiTooltipsAndPopups();
                    ImGuiStage = "tables and tabs";
                    DrawImGuiTablesAndTabs();
                    ImGuiStage = "text and color widgets";
                    DrawImGuiTextAndColorWidgets();
                    ImGuiStage = "menus and settings";
                    DrawImGuiMenusAndSettings();
                    ImGuiStage = "remaining bindings";
                    DrawImGuiRemainingBindings();
                    ImGuiStage = "sprites";
                    DrawImGuiSprites();
                    ImGuiStage = "empty id sweep";
                    SweepImGuiEmptyIds();
                }
            }
            catch {
                ImGuiSurfaceFailures++;
                ImGuiFailedStage = ImGuiStage;
            }
        }

        ImGui.End();
    }

    void DrawImGuiLayout()
    {
        ImGui.SeparatorText("layout");
        ImGui.GetTreeNodeToLabelSpacing();
        ImGui.GetContentRegionAvailX();
        ImGui.GetContentRegionAvailY();
        ImGui.GetCursorPosX();
        ImGui.GetCursorPosY();
        ImGui.GetCursorScreenPos();
        ImGui.SetCursorPos(10.0f, 10.0f);
        ImGui.SetCursorScreenPos(fpos(20.0f, 20.0f));
        ImGui.PushTextWrapPos(0.0f);
        ImGui.PopTextWrapPos();
        ImGui.SetNextItemWidth(120.0f);
        ImGui.PushItemWidth(120.0f);
        ImGui.CalcItemWidth();
        ImGui.PopItemWidth();
        ImGui.Dummy(isize(10, 10));
        ImGui.BeginGroup();
        ImGui.EndGroup();
        ImGui.SmallButton("small2");
    }

    void DrawImGuiInputWidgets()
    {
        bool flag = false;
        ImGui.Checkbox("checkbox", flag);

        int flags = 0;
        ImGui.CheckboxFlags("checkflags", flags, 1);

        int i1 = 1;
        int i2 = 2;
        int i3 = 3;
        int i4 = 4;
        ImGui.InputInt("int", i1);
        ImGui.InputInt2("int2", i1, i2);
        ImGui.InputInt3("int3", i1, i2, i3);
        ImGui.InputInt4("int4", i1, i2, i3, i4);

        float f1 = 1.0f;
        float f2 = 2.0f;
        float f3 = 3.0f;
        float f4 = 4.0f;
        ImGui.DragFloat("dragf", f1, 0.1f, 0.0f, 10.0f);
        ImGui.DragFloat2("dragf2", f1, f2, 0.1f, 0.0f, 10.0f);
        ImGui.DragFloat3("dragf3", f1, f2, f3, 0.1f, 0.0f, 10.0f);
        ImGui.DragFloat4("dragf4", f1, f2, f3, f4, 0.1f, 0.0f, 10.0f);
        ImGui.DragInt("dragi", i1, 1.0f, 0, 10);
        ImGui.DragInt2("dragi2", i1, i2, 1.0f, 0, 10);
        ImGui.DragInt3("dragi3", i1, i2, i3, 1.0f, 0, 10);
        ImGui.DragInt4("dragi4", i1, i2, i3, i4, 1.0f, 0, 10);

        ImGui.SliderFloat("slidef", f1, 0.0f, 10.0f);
        ImGui.SliderFloat2("slidef2", f1, f2, 0.0f, 10.0f);
        ImGui.SliderFloat3("slidef3", f1, f2, f3, 0.0f, 10.0f);
        ImGui.SliderFloat4("slidef4", f1, f2, f3, f4, 0.0f, 10.0f);
        ImGui.SliderInt("slidei", i1, 0, 10);
        ImGui.SliderInt2("slidei2", i1, i2, 0, 10);
        ImGui.SliderInt3("slidei3", i1, i2, i3, 0, 10);
        ImGui.SliderInt4("slidei4", i1, i2, i3, i4, 0, 10);
        ImGui.VSliderFloat("vslidef", fsize(20.0f, 60.0f), f1, 0.0f, 10.0f);
        ImGui.VSliderInt("vslidei", fsize(20.0f, 60.0f), i1, 0, 10);
    }

    void DrawImGuiContainers()
    {
        if (ImGui.BeginChild("child", isize(120, 80), true)) {
            ImGui.Text("inside child");
        }

        ImGui.EndChild();
    }

    void DrawImGuiSprites()
    {
        uint spr = Game.LoadSprite("Quad.png");

        if (spr != 0) {
            ImGui.Image(spr, fsize(16.0f, 16.0f));
            ImGui.Image(spr, fsize(16.0f, 16.0f), fpos(0.0f, 0.0f), fpos(0.5f, 0.5f));
            ImGui.ImageButton("image button", spr, fsize(16.0f, 16.0f));
            ImGui.ImageButton("tinted image button", spr, fsize(16.0f, 16.0f), fpos(0.0f, 0.0f), fpos(1.0f, 1.0f), ucolor(0, 0, 0, 0), ucolor(255, 255, 255, 255));
        }

        // An unknown sprite id draws nothing by design, but a degenerate size, an out-of-range UV pair and an
        // empty button id are all authoring mistakes and must be rejected
        ImGuiSpriteRejections = 0;
        ImGui.Image(0, fsize(16.0f, 16.0f));
        try { ImGui.Image(spr, fsize(0.0f, 16.0f)); } catch { ImGuiSpriteRejections++; }
        try { ImGui.Image(spr, fsize(16.0f, 16.0f), fpos(0.0f, 0.0f), fpos(2.0f, 2.0f)); } catch { ImGuiSpriteRejections++; }
        try { ImGui.ImageButton("", spr, fsize(16.0f, 16.0f)); } catch { ImGuiSpriteRejections++; }
        try { ImGui.ImageButton("bad size", spr, fsize(16.0f, -1.0f)); } catch { ImGuiSpriteRejections++; }

        Game.FreeSprite(spr);
    }

    void DrawImGuiRemainingBindings()
    {
        float f2a = 1.0f;
        float f2b = 2.0f;
        float f2c = 3.0f;
        float f2d = 4.0f;
        ImGui.InputFloat2("input float2", f2a, f2b);
        ImGui.InputFloat3("input float3", f2a, f2b, f2c);
        ImGui.InputFloat4("input float4", f2a, f2b, f2c, f2d);

        uint flagsValue = 0;
        ImGui.CheckboxFlags("checkbox flags", flagsValue, 1);

        ucolor pickerColor = ucolor(10, 20, 30, 255);
        ImGui.ColorPicker4("picker4", pickerColor);

        ImGui.Value("float", 1.5f);
        ImGui.IsRectVisible(fpos(0.0f, 0.0f), fpos(10.0f, 10.0f));
        ImGui.GetMousePosOnOpeningCurrentPopup();
        ImGui.TextLinkOpenURL("link label");
        ImGui.TextLinkOpenURL("link with url", "https://fonline.ru");
        if (ImGui.BeginItemTooltip()) {
            ImGui.EndTooltip();
        }

        ImGui.Button("context target");
        ImGui.OpenPopupOnItemClick("item popup");

        if (ImGui.BeginPopupContextItem("item popup")) {
            ImGui.EndPopup();
        }

        ImGui.OpenPopup("modal popup");

        if (ImGui.BeginPopupModal("modal popup")) {
            ImGui.Text("inside modal");
            ImGui.CloseCurrentPopup();
            ImGui.EndPopup();
        }

        // The disk-backed ini surface must round-trip through the workspace without disturbing the live layout
        ImGui.SaveIniSettingsToDisk("Workspace/imgui_coverage.ini");
        ImGui.LoadIniSettingsFromDisk("Workspace/imgui_coverage.ini");

        ImGui.LogToTTY(1);
        ImGui.LogFinish();
        ImGui.LogToFile(1, "Workspace/imgui_coverage.log");
        ImGui.LogFinish();
    }

    void DrawImGuiTablesAndTabs()
    {
        if (ImGui.BeginTable("table", 3, ImGui_TableFlags(uint(ImGui_TableFlags::Borders) | uint(ImGui_TableFlags::Hideable)))) {
            ImGui.TableSetupColumn("first");
            ImGui.TableSetupColumn("second", ImGui_TableColumnFlags::WidthFixed);
            ImGui.TableSetupColumn("third");
            ImGui.TableSetupScrollFreeze(1, 1);
            ImGui.TableHeadersRow();

            for (int row = 0; row < 2; row++) {
                ImGui.TableNextRow();

                for (int col = 0; col < 3; col++) {
                    ImGui.TableSetColumnIndex(col);
                    ImGui.Text("cell");
                }
            }

            ImGui.TableNextRow();
            ImGui.TableNextColumn();
            ImGui.TableSetBgColor(ImGui_TableBgTarget::CellBg, ucolor(200, 40, 40, 255), -1);
            ImGui.TableGetColumnCount();
            ImGui.TableGetColumnIndex();
            ImGui.TableGetRowIndex();
            ImGui.TableGetColumnName(0);
            ImGui.TableGetColumnFlags(0);
            ImGui.TableSetColumnEnabled(2, true);
            ImGui.TableGetHoveredColumn();

            ImGui.EndTable();
        }

        if (ImGui.BeginTabBar("tabs", ImGui_TabBarFlags::Reorderable)) {
            if (ImGui.BeginTabItem("first tab")) {
                ImGui.Text("first tab body");
                ImGui.EndTabItem();
            }

            bool opened = true;

            if (ImGui.BeginTabItem("closable tab", opened)) {
                ImGui.EndTabItem();
            }

            ImGui.TabItemButton("tab button");
            ImGui.SetTabItemClosed("closable tab");
            ImGui.EndTabBar();
        }

        if (ImGui.BeginMainMenuBar()) {
            if (ImGui.BeginMenu("File")) {
                ImGui.MenuItem("Open", false, true);
                ImGui.EndMenu();
            }

            ImGui.EndMainMenuBar();
        }
    }

    void DrawImGuiTextAndColorWidgets()
    {
        ImGui.TextColored("colored", 1.0f, 0.5f, 0.25f, 1.0f);
        ImGui.TextLink("link");
)"
                    R"(        ImGui.Value("bool", true);
        ImGui.Value("int", 42);
        ImGui.Value("uint", uint(7));
        ImGui.ProgressBar(0.5f);
        ImGui.ProgressBar(0.25f, isize(120, 16), "quarter");

        float[] plotValues = {0.0f, 0.5f, 1.0f, 0.25f};
        ImGui.PlotLines("lines", plotValues);
        ImGui.PlotHistogram("histogram", plotValues);

        float f1 = 1.0f;
        ImGui.InputFloat("input float", f1);

        ucolor editedColor = ucolor(50, 100, 150, 255);
        ImGui.ColorEdit3("color3", editedColor);
        ImGui.ColorPicker3("picker3", editedColor);
        ImGui.ColorEdit4("color4", editedColor);
        ImGui.ColorButton("color button", editedColor);
        ImGui.SetColorEditOptions(ImGui_ColorEditFlags::None);

        string textValue = "editable";
        ImGui.InputText("input text", textValue, 64);
        ImGui.InputTextWithHint("input hint", "hint", textValue, 64);
        ImGui.InputTextMultiline("input multiline", textValue, 64, isize(200, 60));

        if (ImGui.BeginCombo("combo", "preview")) {
            ImGui.Selectable("combo item", false);
            ImGui.EndCombo();
        }

        int comboItem = 0;
        ImGui.Combo("combo simple", comboItem, "one\0two\0three\0");

        if (ImGui.BeginListBox("listbox")) {
            ImGui.Selectable("list item", false);
            ImGui.EndListBox();
        }

        if (ImGui.CollapsingHeader("header")) {
            ImGui.Text("inside header");
        }

        if (ImGui.TreeNode("plain tree")) {
            ImGui.TreePop();
        }
    }

    void DrawImGuiMenusAndSettings()
    {
        ImGui.BeginDisabled(true);
        ImGui.Button("disabled button");
        ImGui.EndDisabled();

        ImGui.SetKeyboardFocusHere();
        ImGui.IsRectVisible(fsize(10.0f, 10.0f));
        ImGui.SetNextWindowContentSize(isize(200, 200));
        ImGui.SetNextWindowScroll(fpos(0.0f, 0.0f));
        ImGui.SetNextWindowSizeConstraints(isize(50, 50), isize(500, 500));
        ImGui.SetScrollHereX();
        ImGui.SetScrollHereY();
        ImGui.SetScrollFromPosX(1.0f);
        ImGui.SetScrollFromPosY(1.0f);
        ImGui.PushStyleVarVec2(ImGui_StyleVar::ItemSpacing, 4.0f, 4.0f);
        ImGui.PopStyleVar();

        ImGui.SetClipboardText("clipboard payload");
        ImGui.GetClipboardText();

        // The ini surface must round-trip an in-memory settings blob without touching disk
        string settings = ImGui.SaveIniSettingsToMemory();
        ImGui.LoadIniSettingsFromMemory(settings);

        ImGui.LogToClipboard();
        ImGui.LogText("logged text");
        ImGui.LogFinish();
        ImGui.LogButtons();
    }

    void DrawImGuiItemQueries()
    {
        ImGui.Button("query target");

        // Every per-item and per-window state query answers off the item just submitted
        ImGui.IsItemHovered();
        ImGui.IsItemClicked();
        ImGui.IsItemActivated();
        ImGui.IsItemDeactivated();
        ImGui.IsItemDeactivatedAfterEdit();
        ImGui.IsItemToggledOpen();
        ImGui.IsItemActive();
        ImGui.IsItemEdited();
        ImGui.IsItemVisible();
        ImGui.IsAnyItemFocused();
        ImGui.IsWindowFocused();
        ImGui.IsWindowHovered();
        ImGui.SetItemDefaultFocus();

        ImGui.GetItemRectMin();
        ImGui.GetItemRectMax();
        ImGui.GetItemRectSize();

        ImGui.GetMousePos();
        ImGui.IsMouseDown(ImGui_MouseButton::Left);
        ImGui.IsMouseClicked(ImGui_MouseButton::Left);
        ImGui.IsMouseReleased(ImGui_MouseButton::Left);
        ImGui.IsMouseDoubleClicked(ImGui_MouseButton::Left);
        ImGui.IsMouseHoveringRect(fpos(0.0f, 0.0f), fpos(50.0f, 50.0f));
        ImGui.IsMouseDragging(ImGui_MouseButton::Left);
        ImGui.GetMouseDragDelta();
        ImGui.ResetMouseDragDelta();

        ImGui.ArrowButton("arrow", ImGui_Dir::Right);
        ImGui.InvisibleButton("invisible", fsize(20.0f, 20.0f));
    }

    void DrawImGuiTreesAndSelectables()
    {
        ImGui.SetNextItemOpen(true, ImGui_Cond::Always);

        if (ImGui.TreeNodeEx("tree", ImGui_TreeNodeFlags::DefaultOpen)) {
            ImGui.Text("inside tree");
            ImGui.TreePop();
        }

        ImGui.Selectable("selectable", false);
        ImGui.Selectable("selectable selected", true);

        ImGui.RadioButton("radio", false);

        int radioValue = 0;
        ImGui.RadioButton("radio value", radioValue, 0);
    }

    void DrawImGuiTooltipsAndPopups()
    {
        ImGui.SetTooltip("tooltip text");
        ImGui.SetItemTooltip("item tooltip");

        if (ImGui.BeginTooltip()) {
            ImGui.Text("inside tooltip");
            ImGui.EndTooltip();
        }

        // A popup that was never opened must report closed and skip its body
        if (ImGui.IsPopupOpen("popup")) {
            return;
        }

        if (ImGui.BeginPopup("popup")) {
            ImGui.EndPopup();
        }

        ImGui.OpenPopup("popup");

        if (ImGui.BeginPopup("popup")) {
            ImGui.Text("inside popup");
            ImGui.CloseCurrentPopup();
            ImGui.EndPopup();
        }

        if (ImGui.BeginPopupContextWindow("ctxwindow")) {
            ImGui.EndPopup();
        }

        if (ImGui.BeginPopupContextVoid("ctxvoid")) {
            ImGui.EndPopup();
        }
    }

    int UnitTestGetRenderCalls()
    {
        return RenderCalls;
    }

    void UnitTestEnableRenderDrawing()
    {
        DrawDuringRender = true;
    }

    [[Event]]
    void OnStart()
    {
        StartCalls++;
    }

    [[Event]]
    void OnLoop()
    {
        LoopCalls++;
    }

    void UnitTestNoop() {}

    void UnitTestMarkManualCall()
    {
        ManualCalls++;
    }

    int UnitTestGetStartCalls()
    {
        return StartCalls;
    }

    int UnitTestGetLoopCalls()
    {
        return LoopCalls;
    }

    int UnitTestGetManualCalls()
    {
        return ManualCalls;
    }

    int ImGuiEmptyIdRejections = 0;
    int ImGuiSpriteRejections = 0;
    int ImGuiSurfaceFailures = 0;
    string ImGuiStage;
    string ImGuiFailedStage;

    int UnitTestGetImGuiSpriteRejections()
    {
        return ImGuiSpriteRejections;
    }

    void SweepImGuiEmptyIds()
    {
        for (int i = 0; i < UnitTestImGuiEmptyIdProbeCount(); i++) {
            try {
                UnitTestImGuiRejectsEmptyId(i);
            }
            catch {
                ImGuiEmptyIdRejections++;
            }
        }
    }

    int UnitTestGetImGuiEmptyIdRejections()
    {
        return ImGuiEmptyIdRejections;
    }

    int UnitTestGetImGuiSurfaceFailures()
    {
        return ImGuiSurfaceFailures;
    }

    string UnitTestGetImGuiFailedStage()
    {
        return ImGuiFailedStage;
    }

    void UnitTestImGuiRejectsEmptyId(int index)
    {
        bool boolValue = false;
        int intValue = 0;
        int intValue2 = 0;
        int intValue3 = 0;
        int intValue4 = 0;
        uint uintValue = 0;
        float floatValue = 0.0f;
        float floatValue2 = 0.0f;
        float floatValue3 = 0.0f;
        float floatValue4 = 0.0f;
        string stringValue = "";
        ucolor colorValue = ucolor(0, 0, 0, 255);
        float[] plotData = {0.0f, 1.0f};

        // Every entry passes an empty label or id, which each binding must reject before touching ImGui
        switch (index) {
            case 0:
                ImGui.Begin("");
                break;
            case 1:
                ImGui.Begin("", boolValue);
                break;
            case 2:
                ImGui.PushID("");
                break;
            case 3:
                ImGui.SeparatorText("");
                break;
            case 4:
                ImGui.Button("");
                break;
            case 5:
                ImGui.Checkbox("", boolValue);
                break;
            case 6:
                ImGui.CheckboxFlags("", intValue, 1);
                break;
            case 7:
                ImGui.CheckboxFlags("", uintValue, 1);
                break;
            case 8:
                ImGui.InputInt("", intValue);
                break;
            case 9:
                ImGui.InputInt2("", intValue, intValue2);
                break;
            case 10:
                ImGui.InputInt3("", intValue, intValue2, intValue3);
                break;
            case 11:
                ImGui.InputInt4("", intValue, intValue2, intValue3, intValue4);
                break;
            case 12:
                ImGui.DragFloat("", floatValue, 1.0f, 0.0f, 10.0f);
                break;
            case 13:
                ImGui.DragInt("", intValue, 1.0f, 0, 10);
                break;
            case 14:
                ImGui.DragFloat2("", floatValue, floatValue2, 1.0f, 0.0f, 10.0f);
                break;
            case 15:
                ImGui.DragFloat3("", floatValue, floatValue2, floatValue3, 1.0f, 0.0f, 10.0f);
                break;
            case 16:
                ImGui.DragFloat4("", floatValue, floatValue2, floatValue3, floatValue4, 1.0f, 0.0f, 10.0f);
                break;
            case 17:
                ImGui.DragInt2("", intValue, intValue2, 1.0f, 0, 10);
                break;
            case 18:
                ImGui.DragInt3("", intValue, intValue2, intValue3, 1.0f, 0, 10);
                break;
            case 19:
                ImGui.DragInt4("", intValue, intValue2, intValue3, intValue4, 1.0f, 0, 10);
                break;
            case 20:
                ImGui.SliderFloat("", floatValue, 0.0f, 1.0f);
                break;
            case 21:
                ImGui.SliderInt("", intValue, 0, 10);
                break;
            case 22:
                ImGui.SliderFloat2("", floatValue, floatValue2, 0.0f, 1.0f);
                break;
            case 23:
                ImGui.SliderFloat3("", floatValue, floatValue2, floatValue3, 0.0f, 1.0f);
                break;
            case 24:
                ImGui.SliderFloat4("", floatValue, floatValue2, floatValue3, floatValue4, 0.0f, 1.0f);
                break;
            case 25:
                ImGui.SliderInt2("", intValue, intValue2, 0, 10);
                break;
            case 26:
                ImGui.SliderInt3("", intValue, intValue2, intValue3, 0, 10);
                break;
            case 27:
                ImGui.SliderInt4("", intValue, intValue2, intValue3, intValue4, 0, 10);
                break;
            case 28:
                ImGui.VSliderFloat("", fsize(20.0f, 60.0f), floatValue, 0.0f, 1.0f);
                break;
            case 29:
                ImGui.VSliderInt("", fsize(20.0f, 60.0f), intValue, 0, 10);
                break;
            case 30:
                ImGui.BeginChild("", isize(10, 10), true);
                break;
            case 31:
                ImGui.BeginChild("", isize(10, 10), ImGui_ChildFlags::None, ImGui_WindowFlags::None);
                break;
            case 32:
                ImGui.CollapsingHeader("");
                break;
            case 33:
                ImGui.TreeNode("");
                break;
            case 34:
                ImGui.TreeNodeEx("", ImGui_TreeNodeFlags::None);
                break;
            case 35:
                ImGui.Selectable("", false);
                break;
            case 36:
                ImGui.RadioButton("", false);
                break;
            case 37:
                ImGui.RadioButton("", intValue, 0);
                break;
            case 38:
                ImGui.SmallButton("");
                break;
            case 39:
                ImGui.ArrowButton("", ImGui_Dir::Up);
                break;
            case 40:
                ImGui.InvisibleButton("", fsize(5.0f, 5.0f));
                break;
            case 41:
                ImGui.OpenPopup("");
                break;
            case 42:
                ImGui.BeginPopup("");
                break;
            case 43:
                ImGui.BeginPopupModal("");
                break;
            case 44:
                ImGui.BeginPopupModal("", boolValue, ImGui_WindowFlags::None);
                break;
            case 45:
                ImGui.IsPopupOpen("");
                break;
            case 46:
                ImGui.BeginMenu("");
                break;
            case 47:
                ImGui.MenuItem("");
                break;
            case 48:
                ImGui.BeginTable("", 1);
                break;
            case 49:
                ImGui.TableHeader("");
                break;
            case 50:
                ImGui.TableSetupColumn("");
                break;
            case 51:
                ImGui.BeginTabBar("");
                break;
            case 52:
                ImGui.BeginTabItem("");
                break;
            case 53:
                ImGui.BeginTabItem("", boolValue);
                break;
            case 54:
                ImGui.TabItemButton("");
                break;
            case 55:
                ImGui.SetTabItemClosed("");
                break;
            case 56:
                ImGui.BeginCombo("", "preview");
                break;
            case 57:
                ImGui.Combo("", intValue, "a\0b\0");
                break;
            case 58:
                ImGui.BeginListBox("");
                break;
            case 59:
                ImGui.TextLink("");
                break;
            case 60:
                ImGui.TextLinkOpenURL("");
                break;
            case 61:
                ImGui.TextLinkOpenURL("", "https://fonline.ru");
                break;
            case 62:
                ImGui.PlotLines("", plotData);
                break;
            case 63:
                ImGui.PlotHistogram("", plotData);
                break;
            case 64:
                ImGui.Value("", true);
                break;
            case 65:
                ImGui.Value("", 1);
                break;
            case 66:
                ImGui.Value("", uint(1));
                break;
            case 67:
                ImGui.Value("", 1.0f);
                break;
            case 68:
                ImGui.ColorEdit3("", colorValue);
                break;
            case 69:
)"
                    R"(                ImGui.ColorEdit4("", colorValue);
                break;
            case 70:
                ImGui.ColorPicker3("", colorValue);
                break;
            case 71:
                ImGui.ColorPicker4("", colorValue);
                break;
            case 72:
                ImGui.ColorButton("", colorValue);
                break;
            case 73:
                ImGui.InputFloat("", floatValue);
                break;
            case 74:
                ImGui.InputFloat2("", floatValue, floatValue2);
                break;
            case 75:
                ImGui.InputFloat3("", floatValue, floatValue2, floatValue3);
                break;
            case 76:
                ImGui.InputFloat4("", floatValue, floatValue2, floatValue3, floatValue4);
                break;
            case 77:
                ImGui.InputText("", stringValue, 8);
                break;
            case 78:
                ImGui.InputTextMultiline("", stringValue, 8, isize(50, 20));
                break;
            case 79:
                ImGui.InputTextWithHint("", "hint", stringValue, 8);
                break;
            case 80:
                ImGui.LoadIniSettingsFromDisk("");
                break;
            case 81:
                ImGui.SaveIniSettingsToDisk("");
                break;
        }
    }

    int UnitTestImGuiEmptyIdProbeCount()
    {
        return 82;
    }

    uint LastResolvedSprite = 0;

    [[Event]]
    void ResolveSprite(hstring model, CritterStateAnim state, CritterActionAnim action, uint& sprite)
    {
        if (model.str == "ScriptSequence" || model.str == "art/critters/ScriptSequence") {
            sprite = Game.LoadMapSprite("Sequence.fofrm");
            LastResolvedSprite = sprite;
        }
    }

    int GetLastResolvedSprite()
    {
        return LastResolvedSprite;
    }

    int UnitTestSpriteSequencePrimitives()
    {
        uint source = Game.LoadMapSprite("Sequence.fofrm");

        if (source == 0 || Game.GetSpriteFrameCount(source) != 3 || Game.GetSpriteDirectionCount(source) != 1 || Game.GetSpriteDuration(source).milliseconds != 120) return 1;

        uint[] sources = {};
        int[] frames = {};
        ipos[] offsets = {}, motions = {};

        for (int d = 0; d < MapDirCount; d++) {
            for (int frame = 0; frame < 2; frame++) {
                sources.insertLast(source);
                frames.insertLast(2 - frame);
                offsets.insertLast(ipos(d * 11 + frame, -d * 7 - frame));
                motions.insertLast(ipos(d + frame + 1, -d - frame - 2));
            }
        }

        uint sequence = Game.CreateSpriteSequence(sources, frames, offsets, motions, 2, timespan(300, 2));
        Game.FreeSprite(source);

        if (Game.GetSpriteFrameCount(sequence) != 2 || Game.GetSpriteDirectionCount(sequence) != MapDirCount || Game.GetSpriteDuration(sequence).milliseconds != 300) return 2;

        for (int d = 0; d < MapDirCount; d++) {
            for (int frame = 0; frame < 2; frame++) {
                if (Game.GetSpriteFrameOffset(sequence, frame, mdir(hdir(d))) != ipos(d * 11 + frame, -d * 7 - frame)) return 3;

                if (Game.GetSpriteFrameMotion(sequence, frame, mdir(hdir(d))) != ipos(d + frame + 1, -d - frame - 2)) return 4;
            }
        }

        uint again = Game.LoadMapSprite("Sequence.fofrm");

        if (Game.GetSpriteFrameCount(again) != 3 || Game.GetSpriteFrameOffset(again, 2, mdir(0)) != ipos() || Game.GetSpriteFrameMotion(again, 2, mdir(0)) != ipos()) return 5;

        bool rejected = false;

        try { Game.GetSpriteFrameOffset(again, 3, mdir(0)); } catch { rejected = true; }

        if (!rejected) return 6;

        rejected = false;

        try { Game.CreateSpriteSequence(sources, frames, offsets, motions, 2, timespan(-1, 0)); } catch { rejected = true; }

        if (!rejected) return 7;

        rejected = false;

        try { Game.CreateSpriteSequence({}, {}, {}, {}, 0, timespan()); } catch { rejected = true; }

        if (!rejected) return 8;

        rejected = false;

        try { Game.GetSpriteFrameCount(0); } catch { rejected = true; }

        if (!rejected) return 9;

        Game.FreeSprite(again);
        Game.FreeSprite(sequence);

        return 0;
    }

    int UnitTestClientSpriteApi()
    {
        // The fixture injects these baked sprites, so the loaders must resolve them to live ids
        uint quad = Game.LoadSprite("Quad.png");
        if (quad == 0) return -1;

        isize quadSize = Game.GetSpriteSize(quad);
        if (quadSize.width != 2 || quadSize.height != 2) return -2;

        uint mapSpr = Game.LoadMapSprite("Quad.png");
        if (mapSpr == 0) return -3;

        uint separate = Game.LoadSeparateSprite("Quad.png");
        if (separate == 0) return -4;

        uint hashed = Game.LoadSprite("Quad.png".hstr());
        if (hashed == 0) return -5;

        Game.IsSpriteHit(quad, ipos(0, 0));
        Game.IsSpriteHit(quad, ipos(100, 100));

        // Animation controls are legal on a still sprite and simply have nothing to advance
        Game.PlaySprite(quad, "".hstr(), true, false);
        Game.SetSpriteTime(quad, 0.5f);
        Game.StopSprite(quad);
        Game.SetParticleScale(quad, 2.0f);
        Game.PrewarmParticle(quad);

        // A missing sprite resolves to the zero id rather than throwing
        if (Game.LoadSprite("NoSuchSprite.png") != 0) return -6;
        if (Game.GetSpriteSize(0).width != 0) return -7;
        if (Game.IsSpriteHit(0, ipos(0, 0))) return -8;

        Game.FreeSprite(quad);
        Game.FreeSprite(mapSpr);
        Game.FreeSprite(separate);
        Game.FreeSprite(hashed);
        Game.FreeSprite(0);

        return 0;
    }

    int UnitTestClientEffectsAndText()
    {
        // The fixture binds no shader, so writing script values must be rejected rather than land nowhere
        float[] effectValues = {1.0f, 2.0f, 3.0f, 4.0f};
        int effectRejections = 0;
        try { Game.SetEffectScriptValue(EffectType::GenericSprite, 0, 0, 1.0f); } catch { effectRejections++; }
        try { Game.SetEffectScriptValues(EffectType::GenericSprite, 0, 0, effectValues); } catch { effectRejections++; }
        try { Game.SetEffectScriptValues(EffectType::GenericSprite, 0, 0, effectValues, 1, 2); } catch { effectRejections++; }
        try { Game.ClearEffectScriptValues(EffectType::GenericSprite, 0); } catch { effectRejections++; }
        if (effectRejections != 4) return -10;

        // Every effect slot resolves through its own arm of one switch, so the whole enum is walked. The
        // fixture binds no shader, so each call is expected to be rejected once the slot is resolved
        EffectType[] effectTypes = {
            EffectType::GenericSprite, EffectType::CritterSprite, EffectType::TileSprite,
            EffectType::RoofSprite, EffectType::RainSprite, EffectType::SkinnedMesh,
            EffectType::Interface, EffectType::Primitive, EffectType::Light, EffectType::Fog,
            EffectType::FlushRenderTarget, EffectType::FlushPrimitive, EffectType::FlushMap,
            EffectType::FlushLight, EffectType::FlushFog, EffectType::Offscreen};

        int slotRejections = 0;
        for (uint i = 0; i < effectTypes.length(); i++) {
            try { Game.ClearEffectScriptValues(effectTypes[i], 0); } catch { slotRejections++; }
        }
        if (slotRejections != int(effectTypes.length())) return -11;

        // Font is the one slot that takes -1 as its only accepted subtype
        int fontRejections = 0;
        try { Game.ClearEffectScriptValues(EffectType::Font, -1); } catch { fontRejections++; }
        try { Game.ClearEffectScriptValues(EffectType::Font, 0); } catch { fontRejections++; }
        if (fontRejections != 2) return -12;

        // A per-entity subtype needs a current map, and an out-of-range one is rejected before that
        int subtypeRejections = 0;
        try { Game.ClearEffectScriptValues(EffectType::GenericSprite, -5); } catch { subtypeRejections++; }
        try { Game.ClearEffectScriptValues(EffectType::CritterSprite, -5); } catch { subtypeRejections++; }
        try { Game.ClearEffectScriptValues(EffectType::GenericSprite, 1); } catch { subtypeRejections++; }
        try { Game.ClearEffectScriptValues(EffectType::CritterSprite, 1); } catch { subtypeRejections++; }
        if (subtypeRejections != 4) return -13;

        // The headless backend stubs any effect path, so this proves every slot is addressable rather than that
        // the path is validated
        int bindRejections = 0;
        for (uint i = 0; i < effectTypes.length(); i++) {
            try { Game.SetEffect(effectTypes[i], 0, "Effects/UnitTestMissing.fofx"); } catch { bindRejections++; }
        }
        if (bindRejections != 0) return -14;

        // The fixture declares no text packs, so presence queries answer empty instead of failing
        if (Game.IsTextPresent(TextPackKey(TextPackName("Game".hstr()), "UnitTestMissingKey".hstr(), hstring(), hstring()))) return -1;
        if (Game.GetTextCount(TextPackKey(TextPackName("Game".hstr()), "UnitTestMissingKey".hstr(), hstring(), hstring())) != 0) return -2;

        // Measurement needs a real font slot, so the fixture font is bound first
        Game.BindFont(FontType::Default, "UnitTestFont.fofnt");

        isize measured;
        int lines = 0;
        Game.GetTextInfo("measured text", isize(200, 50), TextFormat(), measured, lines);
        if (measured.width <= 0 || measured.height <= 0) return -3;
        if (lines <= 0) return -4;

        // Wrapping, truncation and alignment each take their own path through the formatter
        TextFormat wrapped;
        wrapped.Flags = FontFlag(uint(FontFlag::CenterX) | uint(FontFlag::CenterY));
        Game.GetTextInfo("a much longer sentence that has to wrap across several lines", isize(40, 200), wrapped, measured, lines);
        if (lines < 2) return -5;

        TextFormat noWrap;
        noWrap.Flags = FontFlag::NoWrap;
        Game.GetTextInfo("a much longer sentence that has to wrap across several lines", isize(40, 200), noWrap, measured, lines);
        if (lines < 1) return -6;

        TextFormat justified;
        justified.Flags = FontFlag(uint(FontFlag::Justify) | uint(FontFlag::Bordered));
        Game.GetTextInfo("justified text sample", isize(120, 200), justified, measured, lines);

        TextFormat skipped;
        skipped.Flags = FontFlag(uint(FontFlag::AlignRight) | uint(FontFlag::AlignBottom));
        skipped.SkipLines = 1;
        Game.GetTextInfo("first line\nsecond line\nthird line", isize(200, 200), skipped, measured, lines);

        // Inline colour tags take their own parse path, both when honoured and when stripped
        Game.GetTextInfo("plain @color:0xFFFF0000@red@color@ tail", isize(200, 50), TextFormat(), measured, lines);

        TextFormat stripped;
        stripped.Flags = FontFlag::NoColorize;
        Game.GetTextInfo("plain @color:0xFFFF0000@red@color@ tail", isize(200, 50), stripped, measured, lines);
        Game.GetTextInfo("malformed @color:notahexvalue@ tail", isize(200, 50), TextFormat(), measured, lines);

        if (Game.GetTextLines(isize(200, 50), FontType::Default) < 0) return -7;

        // Re-binding with a different scale re-bakes the glyph atlas
        Game.BindFont(FontType::Default, "UnitTestFont.fofnt", 0.5f);
        Game.GetTextInfo("rescaled text", isize(200, 50), TextFormat(), measured, lines);
        Game.BindFont(FontType::Default, "UnitTestFont.fofnt");

        string[] noModels;
)"
#if FO_ENABLE_3D
                    R"(        Game.Preload3dFiles(noModels);
)"
#else
                    R"(        // The binding stays exported without the 3D submodule and rejects the call
        int preloadRejections = 0;
        try { Game.Preload3dFiles(noModels); } catch { preloadRejections++; }
        if (preloadRejections != 1) return -15;
)"
#endif
                    R"(
        return 0;
    }

    int UnitTestClientWindowAndConfig()
    {
        bool wasFullscreen = Game.IsFullscreen();
        Game.ToggleFullscreen();
        Game.ToggleFullscreen();
        if (Game.IsFullscreen() != wasFullscreen) return -1;

        Game.MinimizeWindow();
        Game.SetAlwaysOnTop(Game.IsAlwaysOnTop());
        Game.FlashUnfocusedWindow();
        Game.SetScreenKeyboard(true);
        Game.SetScreenKeyboard(false);
        Game.DumpAtlases();

        ipos before = Game.MousePos;
        Game.SetMousePos(ipos(5, 7));
        Game.SetForcedMousePos(ipos(11, 13));
        Game.ClearForcedMousePos();
        Game.SetMousePos(before);

        string[] configPairs = {"UnitTestKey", "UnitTestValue"};
        Game.SetUserConfig(configPairs);

        dict<string, string> configMap = dict<string, string>();
        configMap.set("UnitTestKey", "UnitTestValue");
        Game.SetUserConfig(configMap);

        // The user config lands in the on-disk cache, so leaving a test key behind would follow the next run
        string[] emptyConfig;
        Game.SetUserConfig(emptyConfig);

        Game.SaveText("Workspace/client_script_save.txt", "saved by unit test");

        // Resolution changes reach the process-global app window, so the caller restores it afterwards
        Game.SetResolution(800, 600);

        return 0;
    }

    int ClientRejectionCount = 0;

    int UnitTestClientRejectsBadArguments()
    {
        ClientRejectionCount = 0;

        // Each probe passes media or a path the fixture cannot resolve; the bit records whether it was rejected
        try { Game.PlaySound("NoSuchSound.wav"); } catch { ClientRejectionCount |= 1; }
        try { Game.PlayMusic("NoSuchMusic.ogg", timespan()); } catch { ClientRejectionCount |= 2; }
        try { Game.PlayVideo("NoSuchVideo.ogv", true, false); } catch { ClientRejectionCount |= 4; }
        try { Game.CreateVideoPlayback("NoSuchVideo.ogv", false); } catch { ClientRejectionCount |= 8; }
        try { Game.BindFont(FontType::Default, "NoSuchFont"); } catch { ClientRejectionCount |= 16; }
        try { Game.SetEffect(EffectType::GenericSprite, 0, "NoSuchEffect.fofx"); } catch { ClientRejectionCount |= 32; }
        try { Game.ChangeLanguage("nolang"); } catch { ClientRejectionCount |= 64; }
        try { Game.SaveScreenshot(""); } catch { ClientRejectionCount |= 128; }
        try { Game.SaveText("", "text"); } catch { ClientRejectionCount |= 256; }
        try { Game.CaptureScreenshot(-1); } catch { ClientRejectionCount |= 512; }

        // Video playback is idle, so the query must answer false rather than fail
        if (Game.IsVideoPlaying()) return -1;

        return 0;
    }

    int UnitTestGetClientRejectionCount()
    {
        return ClientRejectionCount;
    }

    int UnitTestClientStateQueries()
    {
        // Without a session none of the current-context accessors have anything to hand out.
        // These are GlobalGetter exports, so they are bare globals rather than Game members
        if (HasChosen) return -1;
        if (HasCurPlayer) return -2;
        if (HasCurLocation) return -3;
        if (HasCurMap) return -4;
        if (Game.IsConnecting()) return -5;
        if (Game.IsConnected()) return -6;

        if (Game.BytesSend() < 0) return -7;
        if (Game.BytesReceive() < 0) return -8;

        // These read window/input state and must answer even with no real device attached
        ipos mouse = Game.MousePos;
        bool mouseAvailable = Game.IsMouseAvailable();
        bool fullscreen = Game.IsFullscreen();
        if (mouse.x != 0 || mouse.y != 0) return -9;
        if (mouseAvailable) return -10;
        if (fullscreen) return -11;

        Game.GetGamepadState();

        // With no current map the critter query answers empty instead of failing
        if (!Game.GetCritters(CritterFindType::Any).isEmpty()) return -14;

        return 0;
    }

    int UnitTestClientGeometry()
    {
        mpos a = mpos(10, 10);
        mpos b = mpos(14, 10);

        if (Game.GetDistance(a, a) != 0) return -1;
        if (Game.GetDistance(a, b) <= 0) return -2;

        return 0;
    }

    int UnitTestClientTextAndCache()
    {
        // The fixture declares no text packs, so only the pack-independent helpers are exercised here
        string replaced = Game.ReplaceText("hello $name", "$name", "world");
        if (replaced != "hello world") return -3;

        string replacedNum = Game.ReplaceText("count $n", "$n", 42);
        if (replacedNum != "count 42") return -4;

        // The client cache round-trips both binary and text payloads
        if (Game.IsCacheEntry("unit_test_entry")) return -5;

        Game.SetCacheText("unit_test_entry", "cached value");
        if (!Game.IsCacheEntry("unit_test_entry")) return -6;
        if (Game.GetCacheText("unit_test_entry") != "cached value") return -7;

        uint8[] payload = {1, 2, 3};
        Game.SetCacheData("unit_test_bin", payload);
        uint8[] readBack = Game.GetCacheData("unit_test_bin");
        if (readBack.length() != 3) return -8;
        if (readBack[2] != 3) return -9;

)"
                    R"(        Game.RemoveCacheEntry("unit_test_entry");
        Game.RemoveCacheEntry("unit_test_bin");
        if (Game.IsCacheEntry("unit_test_entry")) return -10;

        return 0;
    }

    int UnitTestClientInputSimulation()
    {
        // The simulated-input surface is what automated play uses, so it must run without a device
        Game.SimulateMouseMove(ipos(10, 10));
        Game.SimulateMouseDown(ipos(10, 10), MouseButton::Left);
        Game.SimulateMouseUp(ipos(10, 10), MouseButton::Left);
        Game.SimulateMouseClick(ipos(12, 12), MouseButton::Right);
        Game.SimulateTouchDown(0, ipos(5, 5));
        Game.SimulateTouchMove(0, ipos(6, 6), ipos(1, 1));
        Game.SimulateTouchUp(0, ipos(6, 6));
        Game.SimulateTouchTap(ipos(7, 7));

        return 0;
    }

    int UnitTestMapSpriteHolderRefType()
    {
        MapSpriteHolder holder = MapSpriteHolder();
        if (holder is null) return -1;
        if (holder.Valid) return -2;

        holder.SprId = 42;
        if (holder.SprId != 42) return -3;

        holder.NoLight = true;
        if (!holder.NoLight) return -4;

        holder.Angle = 90;
        if (holder.Angle != 90) return -5;

        holder.TweakAlpha = 123;
        if (holder.TweakAlpha != 123) return -6;

        holder.MapProjected = true;
        if (!holder.MapProjected) return -7;

        MapSpriteHolder same = holder;
        if (!(holder == same)) return -8;

        holder.StopDraw();
        return 0;
    }
}
)"},
            },
            [](string_view message) {
                string message_str = string(message);

                if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos || message_str.find("fatal") != string::npos || message_str.find("Fatal") != string::npos) {
                    throw ScriptSystemException(message_str);
                }
            });
    }
#endif

    // A minimal BMFont binary: the info/common/pages/chars blocks the loader walks, with one glyph per
    // ASCII letter so the measurement paths behave the same as with the text descriptor
    inline auto MakeUnitTestBmfFont(string_view image_name) -> vector<uint8_t>
    {
        vector<uint8_t> data;
        data_writer writer {data};

        writer.write<uint8_t>(uint8_t {'B'});
        writer.write<uint8_t>(uint8_t {'M'});
        writer.write<uint8_t>(uint8_t {'F'});
        writer.write<uint8_t>(uint8_t {3});

        // Info block: everything up to the padding quad is skipped, the padding itself must read as 1/1/1/1
        string font_name = "UnitTest";
        writer.write<uint8_t>(uint8_t {1});
        writer.write<uint32_t>(numeric_cast<uint32_t>(14 + font_name.size() + 1));
        writer.write<uint16_t>(uint16_t {8}); // Font size
        writer.write<uint8_t>(uint8_t {0}); // Bit field
        writer.write<uint8_t>(uint8_t {0}); // Char set
        writer.write<uint16_t>(uint16_t {100}); // Stretch height
        writer.write<uint8_t>(uint8_t {0}); // Anti-aliasing
        writer.write<uint8_t>(uint8_t {1}); // Padding up
        writer.write<uint8_t>(uint8_t {1}); // Padding right
        writer.write<uint8_t>(uint8_t {1}); // Padding down
        writer.write<uint8_t>(uint8_t {1}); // Padding left
        writer.write<uint8_t>(uint8_t {0}); // Spacing horizontal
        writer.write<uint8_t>(uint8_t {0}); // Spacing vertical
        writer.write<uint8_t>(uint8_t {0}); // Outline

        for (char ch : font_name) {
            writer.write<uint8_t>(numeric_cast<uint8_t>(ch));
        }

        writer.write<uint8_t>(uint8_t {0});

        // Common block
        writer.write<uint8_t>(uint8_t {2});
        writer.write<uint32_t>(uint32_t {15});
        writer.write<uint16_t>(uint16_t {10}); // Line height
        writer.write<uint16_t>(uint16_t {8}); // Base height
        writer.write<uint16_t>(uint16_t {16}); // Texture width
        writer.write<uint16_t>(uint16_t {16}); // Texture height
        writer.write<uint16_t>(uint16_t {1}); // Pages
        writer.write<uint8_t>(uint8_t {0}); // Bit field
        writer.write<uint8_t>(uint8_t {0}); // Alpha channel
        writer.write<uint8_t>(uint8_t {0}); // Red channel
        writer.write<uint8_t>(uint8_t {0}); // Green channel
        writer.write<uint8_t>(uint8_t {0}); // Blue channel

        // Pages block
        writer.write<uint8_t>(uint8_t {3});
        writer.write<uint32_t>(numeric_cast<uint32_t>(image_name.size() + 1));

        for (char ch : image_name) {
            writer.write<uint8_t>(numeric_cast<uint8_t>(ch));
        }

        writer.write<uint8_t>(uint8_t {0});

        // Chars block: 20 bytes per glyph
        string glyphs = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .,!?-";
        writer.write<uint8_t>(uint8_t {4});
        writer.write<uint32_t>(numeric_cast<uint32_t>(glyphs.size() * 20));

        for (char ch : glyphs) {
            writer.write<uint32_t>(numeric_cast<uint32_t>(numeric_cast<uint8_t>(ch)));
            writer.write<uint16_t>(uint16_t {0}); // X
            writer.write<uint16_t>(uint16_t {0}); // Y
            writer.write<uint16_t>(uint16_t {6}); // Width
            writer.write<uint16_t>(uint16_t {8}); // Height
            writer.write<uint16_t>(uint16_t {0}); // X offset
            writer.write<uint16_t>(uint16_t {0}); // Y offset
            writer.write<uint16_t>(uint16_t {4}); // X advance
            writer.write<uint16_t>(uint16_t {0}); // Page and channel
        }

        return data;
    }

    // A minimal .fofnt descriptor plus its atlas page. Every glyph shares one cell, which is enough for the
    // measurement, wrapping and draw paths to run end to end without a real bitmap font
    inline auto MakeUnitTestFontResources() -> vector<pair<string, vector<uint8_t>>>
    {
        string descriptor = "Version 2\n";
        descriptor += "Image UnitTestFont.png\n";
        descriptor += "LineHeight 8\n";
        descriptor += "YAdvance 1\n";

        for (char letter : string {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .,!?-"}) {
            descriptor += strex("Letter '{}'\nPositionX 0\nPositionY 0\nWidth 4\nHeight 6\nOffsetX 0\nOffsetY 0\nXAdvance 5\n", letter);
        }

        descriptor += "End\n";

        vector<uint8_t> descriptor_bytes(descriptor.begin(), descriptor.end());

        vector<pair<string, vector<uint8_t>>> resources;
        resources.emplace_back("UnitTestFont.fofnt", std::move(descriptor_bytes));
        resources.emplace_back("UnitTestFont.png", BakerTests::MakeMinimalBakedSprite(16, 16));
        resources.emplace_back("UnitTestFont.fnt", MakeUnitTestBmfFont("UnitTestFont.png"));
        return resources;
    }

    inline auto MakeClientTestResources(vector<pair<string, vector<uint8_t>>> extra_resources = {}) -> FileSystem
    {
        auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();

        auto compiler_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("ClientEngineCompilerResources");
        compiler_source->AddFile("Metadata.fometa-client", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_source));

        BakerClientEngine proto_engine {compiler_resources};
        hstring critter_type = proto_engine.Hashes.to_hashed_string("Critter");
        // The model-backed proto lets the animation viewer build a real preview instead of stopping at a
        // missing model
        vector<pair<string, function<void(ProtoCritter&)>>> critter_protos;
        critter_protos.emplace_back(string {"UnitTestClientCritter"}, [](ProtoCritter&) { });
        critter_protos.emplace_back(string {"UnitTestModelCritter"}, [&proto_engine](ProtoCritter& proto) { proto.SetModelName(proto_engine.Hashes.to_hashed_string("Models/RuntimeInstance.fo3d")); });
        auto proto_blob = BakerTests::MakeMultiProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, critter_protos);
#if FO_ANGELSCRIPT_SCRIPTING
        auto script_blob = MakeClientScriptBinary(compiler_resources);
#endif

        auto runtime_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("ClientEngineRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-client", metadata_blob);
        runtime_source->AddFile("ClientEngineTest.fopro-bin-client", proto_blob);
#if FO_ANGELSCRIPT_SCRIPTING
        runtime_source->AddFile("ClientEngineTest.fos-bin-client", script_blob);
#endif

        for (string_view name : {"ImGui_Default", "2D_Default", "2D_NoDepth", "Primitive_Default", "Primitive_Light", "Primitive_Fog", "Flush_RenderTarget", "Flush_Primitive", "Flush_Map", "Flush_Light", "Flush_Fog", "3D_Skinned", "Particles_ColorMulAtlas", "Particles_ColorAddAtlas", "Particles_ColorSubAtlas", "Particles_DistortionAtlas", "Particles_DistortionAddAtlas"}) {
            runtime_source->AddFile(strex("Effects/{}.fofx", name).str(), "[Effect]\nPasses = 1\n");
            runtime_source->AddFile(strex("Effects/{}.fofx-1-info", name).str(), "[EffectInfo]\n");
        }

        for (auto& [resource_path, resource_data] : extra_resources) {
            runtime_source->AddFile(resource_path, std::move(resource_data));
        }

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));
        return resources;
    }

    inline auto MakeClientEngine(GlobalSettings& settings, FileSystem resources) -> refcount_ptr<ClientEngine>
    {
        return safe_alloc::make_refcounted<ClientEngine>(&settings, std::move(resources), &GetApp()->MainWindow);
    }

    inline auto MakeClientEngine(GlobalSettings& settings) -> refcount_ptr<ClientEngine>
    {
        return MakeClientEngine(settings, MakeClientTestResources());
    }

    // A minimal effect, baked through the real EffectBaker so the runtime accepts it. Registering one as an
    // offscreen effect is what makes the offscreen surface bindings usable at all
    inline auto MakeBakedEffectResources(string_view effect_path) -> vector<pair<string, vector<uint8_t>>>
    {
        constexpr string_view EFFECT_SOURCE = R"EFFECT(
[Effect]

[VertexShader]
layout(binding = 0, std140) uniform ProjBuf { mat4 ProjMatrix; };

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec4 InColor;
layout(location = 2) in vec2 InTexCoord;

layout(location = 0) out vec2 TexCoord;

void main(void)
{
    gl_Position = ProjMatrix * vec4(InPosition.xy, 0.0, 1.0);
    TexCoord = InTexCoord;
}

[FragmentShader]
layout(binding = 0) uniform sampler2D MainTex;

layout(location = 0) in vec2 TexCoord;
layout(location = 0) out vec4 FragColor;

void main(void)
{
    FragColor = texture(MainTex, TexCoord);
}
)EFFECT";

        BakerTests::TestRig rig;
        rig.AddSourceFile(effect_path, EFFECT_SOURCE, 10);

        EffectBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        vector<pair<string, vector<uint8_t>>> resources;

        for (const auto& [output_path, output_data] : rig.Outputs) {
            resources.emplace_back(output_path, output_data);
        }

        return resources;
    }

#if FO_ENABLE_3D
    inline void WriteRuntimeModelBoneHeader(data_writer& writer, string_view name, bool attached_mesh)
    {
        writer.write_string(name);
        writer.write<mat44>(mat44 {1.0f});
        writer.write<mat44>(mat44 {1.0f});
        writer.write<uint8_t>(attached_mesh ? uint8_t {1} : uint8_t {0});
    }

    inline auto MakeRuntimeModelMesh(const function<void(data_writer&)>& write_root) -> vector<uint8_t>
    {
        vector<uint8_t> data;
        data_writer writer {data};
        WriteModelMeshHeader(writer);
        write_root(writer);
        return data;
    }

    inline auto MakeRuntimeModelMeshWithVertex(const Vertex3D& vertex, uint32_t skin_bones_count = 1) -> vector<uint8_t>
    {
        return MakeRuntimeModelMesh([&](data_writer& writer) {
            WriteRuntimeModelBoneHeader(writer, "Root", true);
            array<Vertex3D, 1> vertices {vertex};
            writer.write<uint32_t>(numeric_cast<uint32_t>(vertices.size()));
            writer.write_object_array(const_span<Vertex3D> {vertices});
            writer.write<uint32_t>(uint32_t {0});
            writer.write_string({});
            writer.write<uint32_t>(skin_bones_count);

            for (uint32_t i = 0; i < skin_bones_count; i++) {
                writer.write_string({});
            }

            writer.write<uint32_t>(skin_bones_count);

            for (uint32_t i = 0; i < skin_bones_count; i++) {
                writer.write<mat44>(mat44 {1.0f});
            }

            writer.write<uint32_t>(uint32_t {0});
        });
    }

    // A real triangle, so the model-info baker can compute static bounds from it. The origin moves the whole
    // triangle, which lets a test place it far outside the bounds and tell a swept mesh from a skipped one
    inline auto MakeRuntimeModelTriangleMesh(vec3 origin = vec3 {}) -> vector<uint8_t>
    {
        return MakeRuntimeModelMesh([origin](data_writer& writer) {
            WriteRuntimeModelBoneHeader(writer, "Root", true);

            array<Vertex3D, 3> vertices {};
            vertices[0].Position = origin;
            vertices[1].Position = origin + vec3 {1.0f, 0.0f, 0.0f};
            vertices[2].Position = origin + vec3 {0.0f, 1.0f, 0.0f};

            for (Vertex3D& vertex : vertices) {
                vertex.BlendWeights[0] = 1.0f;
                vertex.BlendIndices[0] = 0.0f;
            }

            writer.write<uint32_t>(numeric_cast<uint32_t>(vertices.size()));
            writer.write_object_array(const_span<Vertex3D> {vertices});

            array<ModelMeshIndexData, 3> indices {0, 1, 2};
            writer.write<uint32_t>(numeric_cast<uint32_t>(indices.size()));
            writer.write_object_array(const_span<ModelMeshIndexData> {indices});

            writer.write_string({});
            writer.write<uint32_t>(uint32_t {1});
            writer.write_string({});
            writer.write<uint32_t>(uint32_t {1});
            writer.write<mat44>(mat44 {1.0f});
            writer.write<uint32_t>(uint32_t {0});
        });
    }

    // The second bone carries its own offset, so half the corners move by a different matrix and the posed
    // silhouette is genuinely skeleton-driven
    inline auto MakeSkinnedRuntimeModelMesh() -> vector<uint8_t>
    {
        auto root_bone = safe_alloc::make_unique<ModelMeshBoneData>();
        root_bone->Name = "Root";
        root_bone->TransformationMatrix = mat44 {1.0f};
        root_bone->GlobalTransformationMatrix = mat44 {1.0f};

        auto limb_bone = safe_alloc::make_unique<ModelMeshBoneData>();
        limb_bone->Name = "Limb";
        limb_bone->TransformationMatrix = glm::translate(mat44 {1.0f}, vec3 {0.35f, 0.7f, 0.0f});
        limb_bone->GlobalTransformationMatrix = limb_bone->TransformationMatrix;

        ModelMeshGeometryData geometry;
        geometry.SkinBoneNames = {"Root", "Limb"};
        geometry.SkinBoneOffsets = {mat44 {1.0f}, mat44 {1.0f}};

        for (uint32_t corner = 0; corner < 8; corner++) {
            ModelMeshVertexData vertex {};

            vertex.Position = {
                (corner & 1U) != 0 ? 0.45f : -0.45f,
                (corner & 2U) != 0 ? 1.7f : 0.0f,
                (corner & 4U) != 0 ? 0.3f : -0.3f,
            };
            vertex.Normal = {0.0f, 1.0f, 0.0f};
            vertex.BlendWeights[0] = 1.0f;
            vertex.BlendIndices[0] = (corner & 2U) != 0 ? 1.0f : 0.0f;
            geometry.Vertices.emplace_back(vertex);
        }

        constexpr array<uint32_t, 36> box_indices {0, 1, 3, 0, 3, 2, 4, 6, 7, 4, 7, 5, 0, 2, 6, 0, 6, 4, 1, 5, 7, 1, 7, 3, 2, 3, 7, 2, 7, 6, 0, 4, 5, 0, 5, 1};

        for (uint32_t index : box_indices) {
            geometry.Indices.emplace_back(numeric_cast<ModelMeshIndexData>(index));
        }

        root_bone->AttachedMesh = std::move(geometry);
        root_bone->Children.emplace_back(std::move(limb_bone));

        ModelMeshData data;
        data.RootBone = std::move(root_bone);

        vector<uint8_t> blob;
        data_writer writer {blob};
        WriteModelMeshData(writer, data, "SkinnedSpriteBoundsModel");
        return blob;
    }

    inline void WriteRuntimeModelDescriptionPrefix(data_writer& writer, string_view base_model = "Models/UnusedBase.fbx")
    {
        writer.write_bytes({MODEL_DESCRIPTION_MAGIC.data(), MODEL_DESCRIPTION_MAGIC.size()});
        writer.write<uint16_t>(MODEL_DESCRIPTION_SCHEMA_VERSION);
        writer.write<uint16_t>(MODEL_DESCRIPTION_SUPPORTED_FLAGS);
        writer.write_string(base_model);
        writer.write<uint8_t>(uint8_t {0});
        writer.write<uint8_t>(uint8_t {0});
        writer.write<uint8_t>(uint8_t {0});
        writer.write<int32_t>(0);
        writer.write<int32_t>(0);
        writer.write<int32_t>(0);
        writer.write<int32_t>(0);
        writer.write_string({});
    }

    inline void WriteRuntimeModelDescriptionLinkPrefix(data_writer& writer)
    {
        writer.write<int32_t>(0);
        writer.write<int32_t>(0);
        writer.write_string({});
        writer.write_string({});
        writer.write<uint8_t>(uint8_t {0});

        for (size_t i = 0; i < 10; i++) {
            writer.write<float32_t>(0.0f);
        }

        writer.write<uint32_t>(uint32_t {0});
    }

    inline void WriteRuntimeModelDescriptionLink(data_writer& writer)
    {
        WriteRuntimeModelDescriptionLinkPrefix(writer);
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint8_t>(uint8_t {0});
    }

    // The runtime requires the baked animation-info document: a plain config keyed by the model resource
    // name, carrying the bounds version, the twelve model/view bounds keys and one duration record
    inline auto MakeUnitTestModelAnimationInfo(string_view model_path) -> vector<uint8_t>
    {
        string anim_info = strex(R"([{}]
BoundsVersion = 2
ModelBoundsMinX = -1
ModelBoundsMinY = -1
ModelBoundsMinZ = -1
ModelBoundsMaxX = 1
ModelBoundsMaxY = 1
ModelBoundsMaxZ = 1
ViewBoundsMinX = -1
ViewBoundsMinY = -1
ViewBoundsMinZ = -1
ViewBoundsMaxX = 1
ViewBoundsMaxY = 1
ViewBoundsMaxZ = 1
StateAnimations = 1 1 1 1
ActionAnimations = 1 3 5 17
DurationsMs = 1000 2000 1000 1000
BoundsStateAnimations = 1 1 1 1
BoundsActionAnimations = 1 3 5 17
BoundsMinX = -1 -1 -1 -1
BoundsMinY = -1 -1 -1 -1
BoundsMinZ = -1 -1 -1 -1
BoundsMaxX = 1 1 1 1
BoundsMaxY = 1 1 1 1
BoundsMaxZ = 1 1 1 1
)",
            model_path)
                               .str();

        return vector<uint8_t>(anim_info.begin(), anim_info.end());
    }

    // A valid baked model description is produced by the real ModelInfoBaker: the fixture supplies the
    // source asset directly through the loader callback, so no source-file format has to be reproduced
    inline auto MakeRuntimeModelDescription(string_view model_path, string_view mesh_path, const vector<uint8_t>& mesh_blob, string_view default_link_extra = {}, string_view attached_mesh_path = {}, nptr<const vector<uint8_t>> attached_mesh_blob = nullptr, bool use_two_bone_rig = false, float32_t end_translation = 0.0f, nptr<vector<uint8_t>> animation_info = nullptr) -> vector<uint8_t>
    {
        BakerTests::TestRig rig;
        // A one-line description leaves the layer machinery unreachable, so the caller's extra is inserted right
        // after the model line — where the description's own default link is authored
        string mesh_name = strex(mesh_path).extract_file_name().str();
        string description = strex("Model {}\n"
                                   "{}"
                                   "Anim 1 1 {} Base\n"
                                   "Anim 1 3 {} Base\n"
                                   "Anim 1 5 {} Base\n"
                                   "Anim 1 17 {} Base\n"
                                   "AnimSpeed 1 1 1.5\n"
                                   "AnimSpeed 1 3 0.5\n"
                                   "AnimLayerValue 1 1 1 1\n"
                                   "AnimLayerValue 1 3 1 2\n"
                                   "Layer 1\n"
                                   "Value 1\n"
                                   "Root\n"
                                   "Link Root\n"
                                   "Scale 1.5\n"
                                   "RotX+ 15.0\n"
                                   "MoveY* 2.0\n"
                                   "ScaleZ+ 0.25\n"
                                   "Speed* 1.25\n"
                                   "Value 2\n"
                                   "Root\n"
                                   "Link Root\n"
                                   "DisableLayer 2\n"
                                   "DisableMesh All\n"
                                   "Layer 2\n"
                                   "Value 1\n"
                                   "Root\n"
                                   "Link Root\n"
                                   "Scale* 0.5\n",
            mesh_name, default_link_extra, mesh_name, mesh_name, mesh_name, mesh_name)
                                 .str();

        rig.AddSourceFile(model_path, description, 1);

        // The info baker resolves the mesh through the source loader as well as the baked output
        rig.AddSourceFile(mesh_path, string {"model source fixture"}, 1);
        rig.AddBakedFile(mesh_path, mesh_blob, 1);

        if (!attached_mesh_path.empty()) {
            FO_VERIFY_AND_THROW(attached_mesh_blob, "Runtime model description attachment has no mesh blob", attached_mesh_path);
            rig.AddSourceFile(attached_mesh_path, string {"attached model source fixture"}, 1);
            rig.AddBakedFile(attached_mesh_path, *attached_mesh_blob, 1);
        }

        // The synthetic project supplies its numeric animation contract; native enums contain only None.
        rig.AddBakedFile("Metadata.fometa-client",
            BakerTests::MakeMetadataBlob({{"Enum",
                {
                    {"CritterStateAnim", "", "TestState", "1", "PolicyState", "401"},
                    {"CritterActionAnim", "", "TestAction", "1", "TestActionA", "3", "TestActionB", "5", "TestActionC", "17", "PolicyAction", "509"},
                }}}));

        ModelInfoBaker info_baker(rig.MakeContext(), [mesh_path, use_two_bone_rig, end_translation](string_view path, const File& file) -> ModelSourceAsset {
            ModelSourceAsset asset;
            asset.FileName = path;
            asset.WriteTime = file.GetWriteTime();
            asset.Skeleton.FileName = path;
            asset.Skeleton.Joints.emplace_back(ModelSkeletonJoint {.Name = "Root", .Hierarchy = {"Root"}, .RestLocalTransform = mat44 {1.0f}});

            if (use_two_bone_rig) {
                asset.Skeleton.Joints.emplace_back(ModelSkeletonJoint {.Name = "Limb", .Hierarchy = {"Root", "Limb"}, .RestLocalTransform = mat44 {1.0f}});
            }

            // One real clip, so the runtime rig carries a timeline the instance can actually play
            ModelAnimationSource animation;
            animation.FileName = path;
            animation.Name = "Base";
            animation.Duration = 1.0f;

            ModelAnimationJointSource joint;
            joint.OutputName = "Root";
            joint.Hierarchy = {"Root"};
            joint.Translation.Times = {0.0f, 0.5f, 1.0f};
            joint.Translation.Values = {vec3 {0.0f, 0.0f, 0.0f}, vec3 {0.0f, 1.0f, 0.0f}, vec3 {0.0f, end_translation, 0.0f}};
            joint.Rotation.Times = {0.0f, 1.0f};
            joint.Rotation.Values = {quaternion {1.0f, 0.0f, 0.0f, 0.0f}, quaternion {1.0f, 0.0f, 0.0f, 0.0f}};
            joint.Scale.Times = {0.0f, 1.0f};
            joint.Scale.Values = {vec3 {1.0f, 1.0f, 1.0f}, vec3 {1.0f, 1.0f, 1.0f}};
            animation.Joints.emplace_back(std::move(joint));

            if (use_two_bone_rig) {
                ModelAnimationJointSource limb_joint;
                limb_joint.OutputName = "Limb";
                limb_joint.Hierarchy = {"Root", "Limb"};
                limb_joint.Translation.Times = {0.0f, 1.0f};
                limb_joint.Translation.Values = {vec3 {}, vec3 {}};

                if (end_translation != 0.0f) {
                    limb_joint.Translation.Times = {0.0f, 0.5f, 1.0f};
                    limb_joint.Translation.Values = {vec3 {}, vec3 {0.0f, 1.0f, 0.0f}, vec3 {0.0f, end_translation, 0.0f}};
                }

                limb_joint.Rotation.Times = {0.0f, 1.0f};
                limb_joint.Rotation.Values = {quaternion {1.0f, 0.0f, 0.0f, 0.0f}, quaternion {1.0f, 0.0f, 0.0f, 0.0f}};
                limb_joint.Scale.Times = {0.0f, 1.0f};
                limb_joint.Scale.Values = {vec3 {1.0f}, vec3 {1.0f}};
                animation.Joints.emplace_back(std::move(limb_joint));
            }

            if (path == mesh_path) {
                asset.Animations.emplace_back(std::move(animation));
            }

            return asset;
        });

        info_baker.BakeFiles(rig.GetAllSourceFiles(), "");

        if (animation_info) {
            *animation_info = rig.Outputs.at("ModelAnimationInfo.foinfo");
        }

        REQUIRE(rig.Outputs.count(string {model_path}) == 1);

        return rig.Outputs.at(string {model_path});
    }

#endif
}

FO_END_NAMESPACE

