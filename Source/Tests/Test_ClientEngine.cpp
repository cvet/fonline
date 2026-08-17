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

#include <chrono>
#include <thread>

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
#include "ModelAnimationData.h"
#include "ModelInfoBaker.h"
#include "ModelManager.h"
#include "ModelMeshBaker.h"
#include "ModelMeshData.h"
#include "ModelSourceLoader.h"
#include "ModelSprites.h"
#include "PlayerView.h"
#include "SettingsStorage.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

namespace
{
    struct RecordedQuadDraw
    {
        vector<Vertex2D> Vertices {};
        vector<vindex_t> Indices {};
        size_t StartIndex {};
        optional<size_t> IndicesToDraw {};
        nptr<const RenderTexture> CustomTexture {};
    };

    static auto MakeRecordingQuadEffectLoader() -> RenderEffectLoader
    {
        FO_STACK_TRACE_ENTRY();

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
            FO_STACK_TRACE_ENTRY();
        }

        void DrawBuffer(ptr<RenderDrawBuffer> dbuf, size_t start_index, optional<size_t> indices_to_draw, nptr<const RenderTexture> custom_tex) override
        {
            FO_STACK_TRACE_ENTRY();

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

    static auto MakeClientTestSettings() -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedClientSettings(settings);

        return settings;
    }

    static auto MakeClientScriptBinary(const FileSystem& metadata_resources) -> vector<uint8_t>
    {
        BakerClientEngine compiler_engine {metadata_resources};

        return BakerTests::CompileInlineScripts(&compiler_engine, "ClientEngineScripts",
            {
                {"Scripts/ClientEngineTest.fos", R"(
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
        Game.Preload3dFiles(noModels);

        return 0;
    }

    int UnitTestClientWindowAndConfig()
    {
        bool wasFullscreen = Game.IsFullscreen();
        Game.ToggleFullscreen();
        Game.ToggleFullscreen();
        if (Game.IsFullscreen() != wasFullscreen) return -1;

        Game.MinimizeWindow();
        Game.RefreshAlwaysOnTop();
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

    // A minimal BMFont binary: the info/common/pages/chars blocks the loader walks, with one glyph per
    // ASCII letter so the measurement paths behave the same as with the text descriptor
    static auto MakeUnitTestBmfFont(string_view image_name) -> vector<uint8_t>
    {
        vector<uint8_t> data;
        DataWriter writer {data};

        writer.Write<uint8_t>(uint8_t {'B'});
        writer.Write<uint8_t>(uint8_t {'M'});
        writer.Write<uint8_t>(uint8_t {'F'});
        writer.Write<uint8_t>(uint8_t {3});

        // Info block: everything up to the padding quad is skipped, the padding itself must read as 1/1/1/1
        string font_name = "UnitTest";
        writer.Write<uint8_t>(uint8_t {1});
        writer.Write<uint32_t>(numeric_cast<uint32_t>(14 + font_name.size() + 1));
        writer.Write<uint16_t>(uint16_t {8}); // Font size
        writer.Write<uint8_t>(uint8_t {0}); // Bit field
        writer.Write<uint8_t>(uint8_t {0}); // Char set
        writer.Write<uint16_t>(uint16_t {100}); // Stretch height
        writer.Write<uint8_t>(uint8_t {0}); // Anti-aliasing
        writer.Write<uint8_t>(uint8_t {1}); // Padding up
        writer.Write<uint8_t>(uint8_t {1}); // Padding right
        writer.Write<uint8_t>(uint8_t {1}); // Padding down
        writer.Write<uint8_t>(uint8_t {1}); // Padding left
        writer.Write<uint8_t>(uint8_t {0}); // Spacing horizontal
        writer.Write<uint8_t>(uint8_t {0}); // Spacing vertical
        writer.Write<uint8_t>(uint8_t {0}); // Outline

        for (char ch : font_name) {
            writer.Write<uint8_t>(numeric_cast<uint8_t>(ch));
        }

        writer.Write<uint8_t>(uint8_t {0});

        // Common block
        writer.Write<uint8_t>(uint8_t {2});
        writer.Write<uint32_t>(uint32_t {15});
        writer.Write<uint16_t>(uint16_t {10}); // Line height
        writer.Write<uint16_t>(uint16_t {8}); // Base height
        writer.Write<uint16_t>(uint16_t {16}); // Texture width
        writer.Write<uint16_t>(uint16_t {16}); // Texture height
        writer.Write<uint16_t>(uint16_t {1}); // Pages
        writer.Write<uint8_t>(uint8_t {0}); // Bit field
        writer.Write<uint8_t>(uint8_t {0}); // Alpha channel
        writer.Write<uint8_t>(uint8_t {0}); // Red channel
        writer.Write<uint8_t>(uint8_t {0}); // Green channel
        writer.Write<uint8_t>(uint8_t {0}); // Blue channel

        // Pages block
        writer.Write<uint8_t>(uint8_t {3});
        writer.Write<uint32_t>(numeric_cast<uint32_t>(image_name.size() + 1));

        for (char ch : image_name) {
            writer.Write<uint8_t>(numeric_cast<uint8_t>(ch));
        }

        writer.Write<uint8_t>(uint8_t {0});

        // Chars block: 20 bytes per glyph
        string glyphs = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .,!?-";
        writer.Write<uint8_t>(uint8_t {4});
        writer.Write<uint32_t>(numeric_cast<uint32_t>(glyphs.size() * 20));

        for (char ch : glyphs) {
            writer.Write<uint32_t>(numeric_cast<uint32_t>(numeric_cast<uint8_t>(ch)));
            writer.Write<uint16_t>(uint16_t {0}); // X
            writer.Write<uint16_t>(uint16_t {0}); // Y
            writer.Write<uint16_t>(uint16_t {6}); // Width
            writer.Write<uint16_t>(uint16_t {8}); // Height
            writer.Write<uint16_t>(uint16_t {0}); // X offset
            writer.Write<uint16_t>(uint16_t {0}); // Y offset
            writer.Write<uint16_t>(uint16_t {4}); // X advance
            writer.Write<uint16_t>(uint16_t {0}); // Page and channel
        }

        return data;
    }

    // A minimal .fofnt descriptor plus its atlas page. Every glyph shares one cell, which is enough for the
    // measurement, wrapping and draw paths to run end to end without a real bitmap font
    static auto MakeUnitTestFontResources() -> vector<pair<string, vector<uint8_t>>>
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

    static auto MakeClientTestResources(vector<pair<string, vector<uint8_t>>> extra_resources = {}) -> FileSystem
    {
        auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();

        auto compiler_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ClientEngineCompilerResources");
        compiler_source->AddFile("Metadata.fometa-client", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_source));

        BakerClientEngine proto_engine {compiler_resources};
        hstring critter_type = proto_engine.Hashes.ToHashedString("Critter");
        // The model-backed proto lets the animation viewer build a real preview instead of stopping at a
        // missing model
        vector<pair<string, function<void(ProtoCritter&)>>> critter_protos {
            {string {"UnitTestClientCritter"}, [](ProtoCritter&) {}},
            {string {"UnitTestModelCritter"}, [&proto_engine](ProtoCritter& proto) { proto.SetModelName(proto_engine.Hashes.ToHashedString("Models/RuntimeInstance.fo3d")); }},
        };
        auto proto_blob = BakerTests::MakeMultiProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, critter_protos);
        auto script_blob = MakeClientScriptBinary(compiler_resources);

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ClientEngineRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-client", metadata_blob);
        runtime_source->AddFile("ClientEngineTest.fopro-bin-client", proto_blob);
        runtime_source->AddFile("ClientEngineTest.fos-bin-client", script_blob);

        for (auto& [resource_path, resource_data] : extra_resources) {
            runtime_source->AddFile(resource_path, std::move(resource_data));
        }

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));
        return resources;
    }

    static auto MakeClientEngine(GlobalSettings& settings, FileSystem resources) -> refcount_ptr<ClientEngine>
    {
        return SafeAlloc::MakeRefCounted<ClientEngine>(&settings, std::move(resources), &GetApp()->MainWindow);
    }

    static auto MakeClientEngine(GlobalSettings& settings) -> refcount_ptr<ClientEngine>
    {
        return MakeClientEngine(settings, MakeClientTestResources());
    }

    // A minimal effect, baked through the real EffectBaker so the runtime accepts it. Registering one as an
    // offscreen effect is what makes the offscreen surface bindings usable at all
    static auto MakeBakedEffectResources(string_view effect_path) -> vector<pair<string, vector<uint8_t>>>
    {
        FO_STACK_TRACE_ENTRY();

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
    static void WriteRuntimeModelBoneHeader(DataWriter& writer, string_view name, bool attached_mesh)
    {
        FO_STACK_TRACE_ENTRY();

        writer.WriteString(name);
        writer.Write<mat44>(mat44 {1.0f});
        writer.Write<mat44>(mat44 {1.0f});
        writer.Write<uint8_t>(attached_mesh ? uint8_t {1} : uint8_t {0});
    }

    static auto MakeRuntimeModelMesh(const function<void(DataWriter&)>& write_root) -> vector<uint8_t>
    {
        FO_STACK_TRACE_ENTRY();

        vector<uint8_t> data;
        DataWriter writer {data};
        WriteModelMeshHeader(writer);
        write_root(writer);
        return data;
    }

    static auto MakeRuntimeModelMeshWithVertex(const Vertex3D& vertex, uint32_t skin_bones_count = 1) -> vector<uint8_t>
    {
        FO_STACK_TRACE_ENTRY();

        return MakeRuntimeModelMesh([&](DataWriter& writer) {
            WriteRuntimeModelBoneHeader(writer, "Root", true);
            array<Vertex3D, 1> vertices {vertex};
            writer.Write<uint32_t>(numeric_cast<uint32_t>(vertices.size()));
            writer.WriteObjectArray(const_span<Vertex3D> {vertices});
            writer.Write<uint32_t>(uint32_t {0});
            writer.WriteString({});
            writer.Write<uint32_t>(skin_bones_count);

            for (uint32_t i = 0; i < skin_bones_count; i++) {
                writer.WriteString({});
            }

            writer.Write<uint32_t>(skin_bones_count);

            for (uint32_t i = 0; i < skin_bones_count; i++) {
                writer.Write<mat44>(mat44 {1.0f});
            }

            writer.Write<uint32_t>(uint32_t {0});
        });
    }

    // A real triangle, so the model-info baker can compute static bounds from it. The origin moves the whole
    // triangle, which lets a test place it far outside the bounds and tell a swept mesh from a skipped one
    static auto MakeRuntimeModelTriangleMesh(vec3 origin = vec3 {}) -> vector<uint8_t>
    {
        FO_STACK_TRACE_ENTRY();

        return MakeRuntimeModelMesh([origin](DataWriter& writer) {
            WriteRuntimeModelBoneHeader(writer, "Root", true);

            array<Vertex3D, 3> vertices {};
            vertices[0].Position = origin;
            vertices[1].Position = origin + vec3 {1.0f, 0.0f, 0.0f};
            vertices[2].Position = origin + vec3 {0.0f, 1.0f, 0.0f};

            for (Vertex3D& vertex : vertices) {
                vertex.BlendWeights[0] = 1.0f;
                vertex.BlendIndices[0] = 0.0f;
            }

            writer.Write<uint32_t>(numeric_cast<uint32_t>(vertices.size()));
            writer.WriteObjectArray(const_span<Vertex3D> {vertices});

            array<ModelMeshIndexData, 3> indices {0, 1, 2};
            writer.Write<uint32_t>(numeric_cast<uint32_t>(indices.size()));
            writer.WriteObjectArray(const_span<ModelMeshIndexData> {indices});

            writer.WriteString({});
            writer.Write<uint32_t>(uint32_t {1});
            writer.WriteString({});
            writer.Write<uint32_t>(uint32_t {1});
            writer.Write<mat44>(mat44 {1.0f});
            writer.Write<uint32_t>(uint32_t {0});
        });
    }

    // The second bone carries its own offset, so half the corners move by a different matrix and the posed
    // silhouette is genuinely skeleton-driven
    static auto MakeSkinnedRuntimeModelMesh() -> vector<uint8_t>
    {
        FO_STACK_TRACE_ENTRY();

        auto root_bone = SafeAlloc::MakeUnique<ModelMeshBoneData>();
        root_bone->Name = "Root";
        root_bone->TransformationMatrix = mat44 {1.0f};
        root_bone->GlobalTransformationMatrix = mat44 {1.0f};

        auto limb_bone = SafeAlloc::MakeUnique<ModelMeshBoneData>();
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
        DataWriter writer {blob};
        WriteModelMeshData(writer, data, "SkinnedSpriteBoundsModel");
        return blob;
    }

    static void WriteRuntimeModelDescriptionPrefix(DataWriter& writer, string_view base_model = "Models/UnusedBase.fbx")
    {
        FO_STACK_TRACE_ENTRY();

        writer.WriteBytes({MODEL_DESCRIPTION_MAGIC.data(), MODEL_DESCRIPTION_MAGIC.size()});
        writer.Write<uint16_t>(MODEL_DESCRIPTION_SCHEMA_VERSION);
        writer.Write<uint16_t>(MODEL_DESCRIPTION_SUPPORTED_FLAGS);
        writer.WriteString(base_model);
        writer.Write<uint8_t>(uint8_t {0});
        writer.Write<uint8_t>(uint8_t {0});
        writer.Write<uint8_t>(uint8_t {0});
        writer.Write<int32_t>(0);
        writer.Write<int32_t>(0);
        writer.Write<int32_t>(0);
        writer.Write<int32_t>(0);
        writer.WriteString({});
    }

    static void WriteRuntimeModelDescriptionLinkPrefix(DataWriter& writer)
    {
        FO_STACK_TRACE_ENTRY();

        writer.Write<int32_t>(0);
        writer.Write<int32_t>(0);
        writer.WriteString({});
        writer.WriteString({});
        writer.Write<uint8_t>(uint8_t {0});

        for (size_t i = 0; i < 10; i++) {
            writer.Write<float32_t>(0.0f);
        }

        writer.Write<uint32_t>(uint32_t {0});
    }

    static void WriteRuntimeModelDescriptionLink(DataWriter& writer)
    {
        FO_STACK_TRACE_ENTRY();

        WriteRuntimeModelDescriptionLinkPrefix(writer);
        writer.Write<uint32_t>(uint32_t {0});
        writer.Write<uint32_t>(uint32_t {0});
        writer.Write<uint32_t>(uint32_t {0});
        writer.Write<uint32_t>(uint32_t {0});
    }

    // The runtime requires the baked animation-info document: a plain config keyed by the model resource
    // name, carrying the bounds version, the twelve model/view bounds keys and one duration record
    static auto MakeUnitTestModelAnimationInfo(string_view model_path) -> vector<uint8_t>
    {
        FO_STACK_TRACE_ENTRY();

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
    static auto MakeRuntimeModelDescription(string_view model_path, string_view mesh_path, const vector<uint8_t>& mesh_blob, string_view default_link_extra = {}) -> vector<uint8_t>
    {
        FO_STACK_TRACE_ENTRY();

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
        rig.AddBakedFile("Metadata.fometa-client", BakerTests::MakeEmptyMetadataBlob());

        ModelInfoBaker info_baker(rig.MakeContext(), [](string_view path, const File& file) -> ModelSourceAsset {
            ModelSourceAsset asset;
            asset.FileName = path;
            asset.WriteTime = file.GetWriteTime();
            asset.Skeleton.FileName = path;
            asset.Skeleton.Joints.emplace_back(ModelSkeletonJoint {.Name = "Root", .Hierarchy = {"Root"}, .RestLocalTransform = mat44 {1.0f}});

            // One real clip, so the runtime rig carries a timeline the instance can actually play
            ModelAnimationSource animation;
            animation.FileName = path;
            animation.Name = "Base";
            animation.Duration = 1.0f;

            ModelAnimationJointSource joint;
            joint.OutputName = "Root";
            joint.Hierarchy = {"Root"};
            joint.Translation.Times = {0.0f, 0.5f, 1.0f};
            joint.Translation.Values = {vec3 {0.0f, 0.0f, 0.0f}, vec3 {0.0f, 1.0f, 0.0f}, vec3 {0.0f, 0.0f, 0.0f}};
            joint.Rotation.Times = {0.0f, 1.0f};
            joint.Rotation.Values = {quaternion {1.0f, 0.0f, 0.0f, 0.0f}, quaternion {1.0f, 0.0f, 0.0f, 0.0f}};
            joint.Scale.Times = {0.0f, 1.0f};
            joint.Scale.Values = {vec3 {1.0f, 1.0f, 1.0f}, vec3 {1.0f, 1.0f, 1.0f}};
            animation.Joints.emplace_back(std::move(joint));

            asset.Animations.emplace_back(std::move(animation));
            return asset;
        });

        info_baker.BakeFiles(rig.GetAllSourceFiles(), "");

        REQUIRE(rig.Outputs.count(string {model_path}) == 1);
        return rig.Outputs.at(string {model_path});
    }

#endif
}

#if FO_ENABLE_3D
TEST_CASE("ClientEngineLoadsModelMeshBakerOutputThroughRuntimeParser")
{
    constexpr string_view model_path = "Models/RuntimeParserTriangle.obj";

    BakerTests::TestRig rig;
    rig.AddSourceFile(model_path, R"(o RuntimeParserTriangle
v 0 0 0
v 1 0 0
v 0 1 0
f 1 2 3
)");

    ModelMeshBaker baker(rig.MakeContext());
    baker.BakeFiles(rig.GetAllSourceFiles(), model_path);

    auto output_it = rig.Outputs.find(string(model_path));
    REQUIRE(output_it != rig.Outputs.end());

    auto resources = MakeClientTestResources({{string {model_path}, output_it->second}});
    REQUIRE(resources.IsFileExists(model_path));

    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, std::move(resources));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    auto factory = client->SprMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>();
    REQUIRE(factory);
    CHECK_NOTHROW(factory->GetModelMngr()->PreloadModel(model_path));
}

TEST_CASE("ClientEngineRejectsMalformedBakedModelCountsAndBounds")
{
    vector<pair<string, vector<uint8_t>>> malformed_resources;

    malformed_resources.emplace_back("Models/VertexCountBomb.fbx", MakeRuntimeModelMesh([](DataWriter& writer) {
        WriteRuntimeModelBoneHeader(writer, "Root", true);
        writer.Write<uint32_t>(std::numeric_limits<uint32_t>::max());
    }));

    malformed_resources.emplace_back("Models/IndexCountBomb.fbx", MakeRuntimeModelMesh([](DataWriter& writer) {
        WriteRuntimeModelBoneHeader(writer, "Root", true);
        writer.Write<uint32_t>(uint32_t {0});
        writer.Write<uint32_t>(std::numeric_limits<uint32_t>::max());
    }));

    malformed_resources.emplace_back("Models/IndexOutOfBounds.fbx", MakeRuntimeModelMesh([](DataWriter& writer) {
        WriteRuntimeModelBoneHeader(writer, "Root", true);
        array<Vertex3D, 1> vertices {};
        writer.Write<uint32_t>(numeric_cast<uint32_t>(vertices.size()));
        writer.WriteObjectArray(const_span<Vertex3D> {vertices});
        array<vindex_t, 1> indices {vindex_t {1}};
        writer.Write<uint32_t>(numeric_cast<uint32_t>(indices.size()));
        writer.WriteObjectArray(const_span<vindex_t> {indices});
        writer.WriteString({});
        writer.Write<uint32_t>(uint32_t {1});
        writer.WriteString({});
        writer.Write<uint32_t>(uint32_t {1});
        writer.Write<mat44>(mat44 {1.0f});
        writer.Write<uint32_t>(uint32_t {0});
    }));

    malformed_resources.emplace_back("Models/SkinCountBomb.fbx", MakeRuntimeModelMesh([](DataWriter& writer) {
        WriteRuntimeModelBoneHeader(writer, "Root", true);
        writer.Write<uint32_t>(uint32_t {0});
        writer.Write<uint32_t>(uint32_t {0});
        writer.WriteString({});
        writer.Write<uint32_t>(numeric_cast<uint32_t>(MODEL_MAX_BONES + 1));
    }));

    malformed_resources.emplace_back("Models/SkinOffsetMismatch.fbx", MakeRuntimeModelMesh([](DataWriter& writer) {
        WriteRuntimeModelBoneHeader(writer, "Root", true);
        writer.Write<uint32_t>(uint32_t {0});
        writer.Write<uint32_t>(uint32_t {0});
        writer.WriteString({});
        writer.Write<uint32_t>(uint32_t {1});
        writer.WriteString({});
        writer.Write<uint32_t>(uint32_t {0});
    }));

    Vertex3D valid_skin_vertex {};
    valid_skin_vertex.BlendWeights[0] = 1.0f;

    Vertex3D non_finite_skin_weight = valid_skin_vertex;
    non_finite_skin_weight.BlendWeights[0] = std::numeric_limits<float32_t>::quiet_NaN();
    malformed_resources.emplace_back("Models/NonFiniteSkinWeight.fbx", MakeRuntimeModelMeshWithVertex(non_finite_skin_weight));

    Vertex3D out_of_range_skin_weight = valid_skin_vertex;
    out_of_range_skin_weight.BlendWeights[0] = -0.25f;
    malformed_resources.emplace_back("Models/OutOfRangeSkinWeight.fbx", MakeRuntimeModelMeshWithVertex(out_of_range_skin_weight));

    Vertex3D non_finite_skin_index = valid_skin_vertex;
    non_finite_skin_index.BlendIndices[0] = std::numeric_limits<float32_t>::infinity();
    malformed_resources.emplace_back("Models/NonFiniteSkinIndex.fbx", MakeRuntimeModelMeshWithVertex(non_finite_skin_index));

    Vertex3D non_integral_skin_index = valid_skin_vertex;
    non_integral_skin_index.BlendIndices[0] = 0.5f;
    malformed_resources.emplace_back("Models/NonIntegralSkinIndex.fbx", MakeRuntimeModelMeshWithVertex(non_integral_skin_index));

    Vertex3D out_of_range_skin_index = valid_skin_vertex;
    out_of_range_skin_index.BlendIndices[0] = 1.0f;
    malformed_resources.emplace_back("Models/OutOfRangeSkinIndex.fbx", MakeRuntimeModelMeshWithVertex(out_of_range_skin_index));

    Vertex3D invalid_skin_weight_sum = valid_skin_vertex;
    invalid_skin_weight_sum.BlendWeights[0] = 0.5f;
    malformed_resources.emplace_back("Models/InvalidSkinWeightSum.fbx", MakeRuntimeModelMeshWithVertex(invalid_skin_weight_sum));

    malformed_resources.emplace_back("Models/ChildCountBomb.fbx", MakeRuntimeModelMesh([](DataWriter& writer) {
        WriteRuntimeModelBoneHeader(writer, "Root", false);
        writer.Write<uint32_t>(std::numeric_limits<uint32_t>::max());
    }));

    malformed_resources.emplace_back("Models/HierarchyDepthBomb.fbx", MakeRuntimeModelMesh([](DataWriter& writer) {
        for (uint32_t depth = 0; depth <= MODEL_MESH_MAX_HIERARCHY_DEPTH; depth++) {
            WriteRuntimeModelBoneHeader(writer, "Bone", false);
            writer.Write<uint32_t>(depth < MODEL_MESH_MAX_HIERARCHY_DEPTH ? uint32_t {1} : uint32_t {0});
        }
    }));

    {
        vector<uint8_t> data;
        DataWriter writer {data};
        writer.WriteBytes({MODEL_DESCRIPTION_MAGIC.data(), MODEL_DESCRIPTION_MAGIC.size()});
        writer.Write<uint16_t>(MODEL_DESCRIPTION_SCHEMA_VERSION);
        writer.Write<uint16_t>(MODEL_DESCRIPTION_SUPPORTED_FLAGS);
        writer.Write<uint32_t>(std::numeric_limits<uint32_t>::max());
        malformed_resources.emplace_back("Models/DescriptionStringBomb.fo3d", std::move(data));
    }

    {
        vector<uint8_t> data;
        DataWriter writer {data};
        WriteRuntimeModelDescriptionPrefix(writer);
        WriteRuntimeModelDescriptionLink(writer);
        writer.Write<uint32_t>(std::numeric_limits<uint32_t>::max());
        malformed_resources.emplace_back("Models/DescriptionLinksBomb.fo3d", std::move(data));
    }

    {
        vector<uint8_t> data;
        DataWriter writer {data};
        WriteRuntimeModelDescriptionPrefix(writer);
        WriteRuntimeModelDescriptionLinkPrefix(writer);
        writer.Write<uint32_t>(std::numeric_limits<uint32_t>::max());
        malformed_resources.emplace_back("Models/DescriptionNestedCountBomb.fo3d", std::move(data));
    }

    array<pair<string_view, string_view>, 16> expected_failures {{
        {"Models/VertexCountBomb.fbx", "vertex count exceeds maximum addressable count"},
        {"Models/IndexCountBomb.fbx", "mesh indices"},
        {"Models/IndexOutOfBounds.fbx", "outside vertex count"},
        {"Models/SkinCountBomb.fbx", "skin bone count exceeds maximum"},
        {"Models/SkinOffsetMismatch.fbx", "skin bone offset count mismatch"},
        {"Models/NonFiniteSkinWeight.fbx", "non-finite skin weight"},
        {"Models/OutOfRangeSkinWeight.fbx", "skin weight outside [0, 1]"},
        {"Models/NonFiniteSkinIndex.fbx", "non-finite skin index"},
        {"Models/NonIntegralSkinIndex.fbx", "non-integral skin index"},
        {"Models/OutOfRangeSkinIndex.fbx", "skin index outside valid range"},
        {"Models/InvalidSkinWeightSum.fbx", "skin-weight sum"},
        {"Models/ChildCountBomb.fbx", "child count exceeds maximum"},
        {"Models/HierarchyDepthBomb.fbx", "hierarchy depth"},
        {"Models/DescriptionStringBomb.fo3d", "String length exceeds remaining buffer"},
        {"Models/DescriptionLinksBomb.fo3d", "links"},
        {"Models/DescriptionNestedCountBomb.fo3d", "disabled meshes"},
    }};

    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, MakeClientTestResources(std::move(malformed_resources)));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    auto factory = client->SprMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>();
    REQUIRE(factory);
    auto model_mngr = factory->GetModelMngr();

    for (const auto& [model_path, expected_failure] : expected_failures) {
        INFO(model_path);
        CHECK_THROWS_WITH(model_mngr->PreloadModel(model_path), Catch::Matchers::ContainsSubstring(std::string {expected_failure}));
    }
}

TEST_CASE("ModelSpriteBoundsFollowEveryStateChangeThatMovesTheEnvelope")
{
    // Pinned differentially against a fresh instance rather than hardcoded rectangles, because the frame is derived
    // through per-instance state that can go stale, and a stale rectangle clips the model with nothing logged
    constexpr string_view model_path = "Models/SkinnedSpriteBounds.fbx";

    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, MakeClientTestResources({{string {model_path}, MakeSkinnedRuntimeModelMesh()}}));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    auto factory = client->SprMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>();
    REQUIRE(factory);
    auto model_mngr = factory->GetModelMngr();

    struct SpriteBoundsStep
    {
        string_view Name {};
        function<void(ptr<ModelInstance>)> Apply {};
    };

    vector<SpriteBoundsStep> steps {
        {"rest pose", [](ptr<ModelInstance>) {}},
        {"scaled up", [](ptr<ModelInstance> model) { model->SetScale(1.7f, 1.7f, 1.7f); }},
        {"camera tilt", [](ptr<ModelInstance> model) { model->SetRotation(0.4f, 0.0f, 0.25f); }},
        {"turned", [](ptr<ModelInstance> model) { model->SetDir(mdir {120}, false); }},
        {"shadow off", [](ptr<ModelInstance> model) { model->EnableShadow(false); }},
        {"scaled back down", [](ptr<ModelInstance> model) { model->SetScale(0.6f, 0.6f, 0.6f); }},
    };

    // model_path is a constexpr string_view, but passing it by value and by reference odr-uses it, so it
    // has to be captured rather than relied on as a constant expression
    auto make_model = [&model_mngr, model_path]() {
        auto model = model_mngr->CreateModel(model_path);
        FO_VERIFY_AND_THROW(model, "Skinned test model was not created", model_path);
        auto owned = model.take_not_null();

        owned->StartMeshGeneration();
        owned->PrepareFrameLayout();
        owned->SetupFrame(owned->GetDrawSize(), owned->GetFramePivot());
        return owned;
    };

    auto measure = [](ptr<ModelInstance> model) -> optional<ModelSpriteBounds> {
        model->PoseSpriteFrame(false);
        return model->GetSpriteBounds();
    };

    auto same_bounds = [](const ModelSpriteBounds& first, const ModelSpriteBounds& second) { //
        return first.Rect == second.Rect && first.PoseRect == second.PoseRect && first.RequiredFrameSize == second.RequiredFrameSize && first.Pivot == second.Pivot;
    };

    auto warm_model = make_model();

    // The hit path on its own: measuring twice without touching anything must reproduce the first answer exactly
    optional<ModelSpriteBounds> first_bounds = measure(warm_model.as_ptr());
    optional<ModelSpriteBounds> repeated_bounds = measure(warm_model.as_ptr());

    REQUIRE(first_bounds);
    REQUIRE(repeated_bounds);
    CHECK(same_bounds(*first_bounds, *repeated_bounds));

    optional<ModelSpriteBounds> previous_bounds;
    size_t moved_steps = 0;

    for (size_t step_index = 0; step_index < steps.size(); step_index++) {
        INFO("step " << step_index << " (" << steps[step_index].Name << ")");

        steps[step_index].Apply(warm_model.as_ptr());
        optional<ModelSpriteBounds> warm_bounds = measure(warm_model.as_ptr());
        auto cold_model = make_model();

        for (size_t applied_index = 0; applied_index <= step_index; applied_index++) {
            steps[applied_index].Apply(cold_model.as_ptr());
        }

        optional<ModelSpriteBounds> cold_bounds = measure(cold_model.as_ptr());

        REQUIRE(warm_bounds);
        REQUIRE(cold_bounds);
        CHECK(same_bounds(*warm_bounds, *cold_bounds));

        if (previous_bounds && !same_bounds(*cold_bounds, *previous_bounds)) {
            moved_steps++;
        }

        previous_bounds = cold_bounds;
    }

    // Guard the guard: a step that leaves the envelope where it was cannot tell a stale cache from a correct one, so
    // the sequence above only tests anything as long as it keeps moving the rectangle
    CHECK(moved_steps + 1 >= steps.size() - 1);
}
#endif

#if FO_ENABLE_3D
TEST_CASE("ModelDefaultLinkDisablesItsOwnMeshes")
{
    // Baked model and animation bounds are calculated with the default link's disabled meshes excluded, so a
    // runtime that left them enabled would sweep geometry the baked layout never budgeted for
    constexpr string_view MESH_PATH = "Models/DefaultLinkDisabled.fbx";
    constexpr string_view MODEL_PATH = "Models/DefaultLinkDisabled.fo3d";

    // Far outside the declared bounds, so a mesh that still reached the sweep could not hide inside the layout frame
    vector<uint8_t> mesh_blob = MakeRuntimeModelTriangleMesh(vec3 {12.0f, 12.0f, 0.0f});

    vector<pair<string, vector<uint8_t>>> model_resources;
    model_resources.emplace_back(string {"ModelAnimationInfo.foinfo"}, MakeUnitTestModelAnimationInfo(MODEL_PATH));
    model_resources.emplace_back(string {MESH_PATH}, mesh_blob);
    model_resources.emplace_back(string {MODEL_PATH}, MakeRuntimeModelDescription(MODEL_PATH, MESH_PATH, mesh_blob, "DisableMesh All\n"));

    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, MakeClientTestResources(std::move(model_resources)));

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    auto factory = client->SprMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>();
    REQUIRE(factory);

    auto model_mngr = factory->GetModelMngr();

    REQUIRE_NOTHROW(model_mngr->PreloadModel(MODEL_PATH));

    auto model = model_mngr->CreateModel(MODEL_PATH);
    REQUIRE(static_cast<bool>(model));

    // The sweep only measures generated combined meshes, so without this the frame would stay at the layout size
    // no matter what the default link did
    model->StartMeshGeneration();
    model->PrepareFrameLayout();

    // Guard the fixture: the layout must come from the declared +/-1 bounds, or the far triangle is already
    // inside the frame and the check below would pass without proving anything
    isize32 layout_size = model->GetDrawSize();
    int32_t bounds_span_limit = iround<int32_t>(6.0f * client->Settings->ModelProjFactor);
    REQUIRE(layout_size.width <= bounds_span_limit);
    REQUIRE(layout_size.height <= bounds_span_limit);

    model->SetupFrame(layout_size, model->GetFramePivot());
    model->PoseSpriteFrame(true);

    auto bounds = model->GetSpriteBounds();
    REQUIRE(bounds);
    CHECK(bounds->RequiredFrameSize == layout_size);
}

TEST_CASE("ModelManagerInstantiatesABakedModel")
{
    // The 3D instance surface was assumed to need a GPU, but the headless Null renderer serves it: with a
    // baked mesh and a valid baked description the manager builds a real ModelInstance
    constexpr string_view MESH_PATH = "Models/RuntimeInstance.fbx";
    constexpr string_view MODEL_PATH = "Models/RuntimeInstance.fo3d";

    Vertex3D vertex {};
    vertex.BlendWeights[0] = 1.0f;
    vertex.BlendIndices[0] = 0.0f;

    vector<pair<string, vector<uint8_t>>> model_resources;
    ignore_unused(vertex);
    vector<uint8_t> mesh_blob = MakeRuntimeModelTriangleMesh();

    model_resources.emplace_back(string {"ModelAnimationInfo.foinfo"}, MakeUnitTestModelAnimationInfo(MODEL_PATH));
    model_resources.emplace_back(string {MESH_PATH}, mesh_blob);
    model_resources.emplace_back(string {MODEL_PATH}, MakeRuntimeModelDescription(MODEL_PATH, MESH_PATH, mesh_blob));

    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, MakeClientTestResources(std::move(model_resources)));

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    auto factory = client->SprMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>();
    REQUIRE(factory);

    auto model_mngr = factory->GetModelMngr();

    REQUIRE_NOTHROW(model_mngr->PreloadModel(MODEL_PATH));

    auto model = model_mngr->CreateModel(MODEL_PATH);
    REQUIRE(static_cast<bool>(model));

    SECTION("TheInstanceAnswersItsGeometryAndAnimationState")
    {
        ignore_unused(model->GetInformation());
        ignore_unused(model->GetStateAnim());
        ignore_unused(model->GetActionAnim());
        ignore_unused(model->GetMovingAnim());
        ignore_unused(model->GetDrawSize());
        ignore_unused(model->GetLightingSize());
        ignore_unused(model->GetSpriteBounds());
        ignore_unused(model->GetDrawRect());
        ignore_unused(model->GetFramePivot());
        ignore_unused(model->NeedForceDraw());
        ignore_unused(model->NeedDraw());
        ignore_unused(model->HasAnimation(static_cast<CritterStateAnim>(1), static_cast<CritterActionAnim>(1)));

        ipos32 screen_pos = model->Convert3dTo2d(vec3 {0.0f, 0.0f, 0.0f});
        vec3 world_pos = model->Convert2dTo3d(screen_pos);
        CHECK(std::isfinite(world_pos.x));
        CHECK(std::isfinite(world_pos.y));
        CHECK(std::isfinite(world_pos.z));

        auto state_anim = static_cast<CritterStateAnim>(1);
        auto action_anim = static_cast<CritterActionAnim>(1);
        ignore_unused(model->ResolveAnimation(state_anim, action_anim));
    }

    SECTION("TheInstanceAcceptsOrientationScaleAndMovementState")
    {
        REQUIRE_NOTHROW(model->SetupFrame(isize32 {128, 128}, ipos32 {64, 96}));
        REQUIRE_NOTHROW(model->PrepareFrameLayout());
        REQUIRE_NOTHROW(model->RequestRedraw());

        for (int32_t dir = 0; dir < 6; dir++) {
            REQUIRE_NOTHROW(model->SetDir(mdir {numeric_cast<uint8_t>(dir)}, false));
            REQUIRE_NOTHROW(model->SetLookDir(mdir {numeric_cast<uint8_t>(dir)}));
            REQUIRE_NOTHROW(model->SetMoveDir(mdir {numeric_cast<uint8_t>(dir)}, true));
        }

        REQUIRE_NOTHROW(model->SetRotation(0.0f, 30.0f, 0.0f));
        REQUIRE_NOTHROW(model->SetScale(1.0f, 1.0f, 1.0f));
        REQUIRE_NOTHROW(model->SetSpeed(2.0f));
        REQUIRE_NOTHROW(model->EnableShadow(true));
        REQUIRE_NOTHROW(model->EnableShadow(false));
        REQUIRE_NOTHROW(model->AddMoveOffset(ipos32 {4, 4}));
        REQUIRE_NOTHROW(model->SetMovementState(false, true, 10));
        REQUIRE_NOTHROW(model->SetMovementState(true, false, 0));

        ignore_unused(model->GetMoveDirAngle());
        ignore_unused(model->HasBodyRotation());
        ignore_unused(model->GetViewRect());
        ignore_unused(model->GetAttachPoints());
        ignore_unused(model->GetAnimDuration());
        ignore_unused(model->GetAnimDuration(static_cast<CritterStateAnim>(1), static_cast<CritterActionAnim>(1)));
        ignore_unused(model->IsAnimationPlaying());

        hstring root_bone = client->Hashes.ToHashedString("Root");
        ignore_unused(model->FindBone(root_bone));
        ignore_unused(model->GetBonePos(root_bone));
        ignore_unused(model->GetBoneSpritePos(root_bone));

        REQUIRE_NOTHROW(model->ClearAnimationCallbacks());
        REQUIRE_NOTHROW(model->SetAnimInitCallback([](CritterStateAnim&, CritterActionAnim&) { }));
    }

    SECTION("AFrameLargerThanTheMaximumRenderTextureIsRejected")
    {
        // Rejected while the model is still identifiable, instead of reaching the graphics API and failing there as
        // an anonymous invalid argument
        int32_t max_draw_width = AppRender::MAX_ATLAS_WIDTH / ModelInstance::FRAME_SCALE;
        int32_t max_draw_height = AppRender::MAX_ATLAS_HEIGHT / ModelInstance::FRAME_SCALE;

        REQUIRE(max_draw_width > 0);
        REQUIRE(max_draw_height > 0);
        REQUIRE_NOTHROW(model->SetupFrame(isize32 {max_draw_width, max_draw_height}, ipos32 {}));
        REQUIRE_THROWS(model->SetupFrame(isize32 {max_draw_width + 1, max_draw_height}, ipos32 {}));
        REQUIRE_THROWS(model->SetupFrame(isize32 {max_draw_width, max_draw_height + 1}, ipos32 {}));
    }

    SECTION("LayerValuesDriveTheAnimationDataAndLinkTransforms")
    {
        // Each link block applies only when its layer value is requested, so nothing under SetAnimData runs until
        // an animation plays with layers set
        model->SetupFrame(isize32 {128, 128}, ipos32 {64, 96});

        auto state_anim = static_cast<CritterStateAnim>(1);
        auto action_anim = static_cast<CritterActionAnim>(1);

        std::array<int32_t, MODEL_LAYERS_COUNT> layers {};

        auto play_with_layers = [&](ModelAnimFlags flags) {
            REQUIRE_NOTHROW(ignore_unused(model->PlayAnim(state_anim, action_anim, layers.data(), 0.0f, flags)));

            for (int32_t frame = 0; frame < 3; frame++) {
                REQUIRE_NOTHROW(model->PoseSpriteFrame(true));
            }
        };

        // Layer 1 / value 1 is the block that links a bone and scales, rotates and moves it
        layers[1] = 1;
        play_with_layers(ModelAnimFlags::Init);

        // Layer 1 / value 2 disables the second layer and every mesh under it
        layers[1] = 2;
        layers[2] = 1;
        play_with_layers(ModelAnimFlags::None);

        // The same walk again with the smoothing and rotation paths switched off
        layers[1] = 1;
        layers[2] = 1;
        play_with_layers(ModelAnimFlags::NoSmooth);
        play_with_layers(ModelAnimFlags::NoRotate);
        play_with_layers(ModelAnimFlags::PlayOnce);
        play_with_layers(ModelAnimFlags::Freeze);

        // Clearing the layers takes the removal side of the same walk
        layers[1] = 0;
        layers[2] = 0;
        play_with_layers(ModelAnimFlags::None);

        // A speed override and a missing animation are the two remaining arms of PlayAnim
        REQUIRE_NOTHROW(ignore_unused(model->PlayAnim(state_anim, action_anim, layers.data(), 0.5f, ModelAnimFlags::None)));
        CHECK_FALSE(model->PlayAnim(static_cast<CritterStateAnim>(9), static_cast<CritterActionAnim>(9), layers.data(), 0.0f, ModelAnimFlags::None));

        // Switching between the authored animations is what runs the cross-fade and the per-animation
        // speed and layer-value overrides, none of which a single-animation model can reach
        for (CritterActionAnim authored : {CritterActionAnim::Idle, CritterActionAnim::Walk, CritterActionAnim::Run, CritterActionAnim::TurnRight, CritterActionAnim::Walk, CritterActionAnim::Idle}) {
            CHECK(model->HasAnimation(state_anim, authored));
            REQUIRE_NOTHROW(ignore_unused(model->PlayAnim(state_anim, authored, layers.data(), 0.0f, ModelAnimFlags::None)));

            for (int32_t frame = 0; frame < 3; frame++) {
                REQUIRE_NOTHROW(model->PoseSpriteFrame(true));
            }

            ignore_unused(model->GetAnimDuration(state_anim, authored));
        }

        // Movement animations are chosen from the authored walk/run pair rather than played by name
        for (int32_t speed : {0, 5, 20}) {
            REQUIRE_NOTHROW(model->SetMovementState(true, speed != 0, speed));
            REQUIRE_NOTHROW(model->PoseSpriteFrame(true));
        }

        REQUIRE_NOTHROW(model->SetMovementState(false, false, 0));

        ignore_unused(model->GetSpriteBounds());
        ignore_unused(model->GetAttachPoints());
    }

    SECTION("PosingAdvancesTheAnimationAndBuildsTheFrame")
    {
        model->SetupFrame(isize32 {128, 128}, ipos32 {64, 96});

        for (int32_t frame = 0; frame < 4; frame++) {
            REQUIRE_NOTHROW(model->PoseSpriteFrame(true));
            std::this_thread::sleep_for(std::chrono::milliseconds {8});
        }

        ignore_unused(model->GetSpriteBounds());
        ignore_unused(model->GetDrawSize());
        ignore_unused(model->NeedDraw());
    }

    SECTION("TheInstanceDrawsThroughTheHeadlessRenderer")
    {
        model->SetupFrame(isize32 {128, 128}, ipos32 {64, 96});
        model->StartMeshGeneration();
        model->PoseSpriteFrame(true);

        // The Null renderer accepts the draw calls, so the mesh combine and submit paths run for real
        REQUIRE_NOTHROW(model->DrawSpriteFrame());

        mat44 proj = client->SprMngr.GetRender().CreateOrthoMatrix(0.0f, 128.0f, 0.0f, 128.0f, -10.0f, 10.0f);
        REQUIRE_NOTHROW(model->DrawInScene(proj, 1.0f));

        REQUIRE_NOTHROW(model->PrewarmParticles());
        REQUIRE_NOTHROW(model->PoseSpriteFrame(false));
        REQUIRE_NOTHROW(model->DrawSpriteFrame());
    }

    SECTION("TheAnimationViewerPreviewsAModelBackedCritter")
    {
        // The viewer selects through a list click or through its persisted "last critter" setting, so the
        // setting is seeded and restored around the run
        string saved_selected_proto;

        {
            SettingsStorage viewer_settings {"AnimationViewer"};
            saved_selected_proto = viewer_settings.GetString("SelectedProto");
            viewer_settings.SetString("SelectedProto", "UnitTestModelCritter");
        }

        auto restore_viewer_settings = scope_exit([&saved_selected_proto]() noexcept {
            safe_call([&saved_selected_proto] {
                SettingsStorage viewer_settings {"AnimationViewer"};
                viewer_settings.SetString("SelectedProto", saved_selected_proto);
            });
        });

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

        AnimationViewer viewer {client.as_ptr(), &client->SprMngr, &client->ResMngr, &client->GameTime};
        viewer.SetVisible(true);
        viewer.SetFillViewport(true);

        for (int32_t frame = 0; frame < 4; frame++) {
            ImGui::NewFrame();
            ImGui::LogToBuffer(12);
            REQUIRE_NOTHROW(viewer.Draw());
            ImGui::LogFinish();
            ImGui::Render();
            std::this_thread::sleep_for(std::chrono::milliseconds {8});
        }

        CHECK(viewer.IsVisible());
    }

    SECTION("TheSpriteFactoryWrapsTheModelAsASprite")
    {
        // Going through the sprite manager takes the model down the model-sprite path instead of the raw
        // instance one: atlas placement, per-frame update and the sprite-side draw
        shared_ptr<Sprite> sprite = client->SprMngr.LoadSprite(client->Hashes.ToHashedString(MODEL_PATH), AtlasType::MapSprites, true);
        REQUIRE(static_cast<bool>(sprite));

        CHECK(sprite->GetSize().width > 0);
        CHECK(sprite->GetSize().height > 0);

        REQUIRE_NOTHROW(sprite->Prewarm());
        REQUIRE_NOTHROW(sprite->PlayDefault());

        for (int32_t frame = 0; frame < 4; frame++) {
            REQUIRE_NOTHROW(sprite->Update());
            std::this_thread::sleep_for(std::chrono::milliseconds {8});
        }

        ignore_unused(sprite->IsHitTest(ipos32 {0, 0}));
        ignore_unused(sprite->IsPlaying());
        REQUIRE_NOTHROW(sprite->Stop());
    }
}
#endif

#if FO_ANGELSCRIPT_SCRIPTING
TEST_CASE("ScriptDebuggerEndpointServesItsTcpPort")
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
    int32_t pid_num = strvex(Platform::GetCurrentProcessIdStr()).to_int32();
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

TEST_CASE("ClientEngineStartsAndRegistersEntities")
{
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings);

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    CHECK_FALSE(client->IsConnecting());
    CHECK_FALSE(client->IsConnected());
    CHECK_FALSE(static_cast<bool>(client->GetCurPlayer()));
    CHECK_FALSE(static_cast<bool>(client->GetCurLocation()));
    CHECK_FALSE(static_cast<bool>(client->GetCurMap()));

    hstring critter_pid = client->Hashes.ToHashedString("UnitTestClientCritter");
    auto critter_proto = client->GetProtoCritter(critter_pid);
    REQUIRE(static_cast<bool>(critter_proto));

    auto player = SafeAlloc::MakeRefCounted<PlayerView>(client, ident_t {1001});
    auto critter = SafeAlloc::MakeRefCounted<CritterView>(client, ident_t {1002}, critter_proto);

    REQUIRE(client->GetEntity(player->GetId()) == player);
    REQUIRE(client->GetEntity(critter->GetId()) == critter);
    CHECK(critter->GetProtoId() == critter_pid);
    CHECK(critter->GetName() == "UnitTestClientCritter_1002");

    critter->DestroySelf();
    player->DestroySelf();

    CHECK_FALSE(static_cast<bool>(client->GetEntity(ident_t {1002})));
    CHECK_FALSE(static_cast<bool>(client->GetEntity(ident_t {1001})));
}

TEST_CASE("ClientEngineScriptModuleInitAndLoopAreCallable")
{
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings);

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    auto get_func_name = [&client](string_view name) { return client->Hashes.ToHashedString(name); };

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

TEST_CASE("ClientEngineScheduledCallbacksDoNotRunNestedZeroDelayInSamePass")
{
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings);

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    int32_t callback_count = 0;

    client->ScheduleDelayedCallback(timespan::zero, [&client, &callback_count] {
        callback_count++;

        client->ScheduleDelayedCallback(timespan::zero, [&callback_count] { callback_count++; });
    });

    client->ProcessScheduledCallbacks();
    CHECK(callback_count == 1);

    client->ProcessScheduledCallbacks();
    CHECK(callback_count == 2);
}

TEST_CASE("ClientEngineMethodRefTypeOps")
{
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings);

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    auto get_func_name = [&client](string_view name) { return client->Hashes.ToHashedString(name); };

    int32_t result = 0;
    REQUIRE(client->CallFunc(get_func_name("ClientEngineTest::UnitTestMapSpriteHolderRefType"), result));
    CHECK(result == 0);
}

TEST_CASE("ResourceManagerLoadsLegacyCritterAnimations")
{
    // The legacy path only runs under art/critters/ and builds sprite names by index letter, then casts the result
    // to a SpriteSheet — hence the multi-frame fixtures
    constexpr string_view FRM_IND = "_abcdefghijklmnopqrstuvwxyz0123456789";
    constexpr string_view MODEL_NAME = "art/critters/utxx.frm";
    constexpr string_view MODEL_STEM = "art/critters/ut";

    vector<pair<string, vector<uint8_t>>> anim_resources;

    for (int32_t state_anim = 0; state_anim < 14; state_anim++) {
        for (int32_t action_anim = 0; action_anim < 14; action_anim++) {
            string spr_name = strex("{}{}{}.fofrm", MODEL_STEM, FRM_IND[numeric_cast<size_t>(state_anim)], FRM_IND[numeric_cast<size_t>(action_anim)]).str();
            anim_resources.emplace_back(std::move(spr_name), BakerTests::MakeMultiFrameBakedSprite(3));
        }
    }

    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, MakeClientTestResources(std::move(anim_resources)));

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    hstring model_name = client->Hashes.ToHashedString(MODEL_NAME);

    SECTION("AnimationPairsResolveThroughTheLegacyLoader")
    {
        int32_t resolved = 0;

        for (int32_t state_anim = 1; state_anim < 14; state_anim++) {
            for (int32_t action_anim = 1; action_anim < 14; action_anim++) {
                if (client->ResMngr.GetCritterAnimFrames(model_name, static_cast<CritterStateAnim>(state_anim), static_cast<CritterActionAnim>(action_anim), mdir {0})) {
                    resolved++;
                }
            }
        }

        CHECK(resolved > 0);
    }

    SECTION("RepeatedLookupsComeFromTheCache")
    {
        nptr<const SpriteSheet> first = client->ResMngr.GetCritterAnimFrames(model_name, static_cast<CritterStateAnim>(1), static_cast<CritterActionAnim>(1), mdir {0});
        nptr<const SpriteSheet> second = client->ResMngr.GetCritterAnimFrames(model_name, static_cast<CritterStateAnim>(1), static_cast<CritterActionAnim>(1), mdir {0});
        CHECK(first == second);
    }

    SECTION("AModelWithNoSpritesAnswersEmpty")
    {
        hstring missing = client->Hashes.ToHashedString("art/critters/nosuchxx.frm");
        CHECK_FALSE(static_cast<bool>(client->ResMngr.GetCritterAnimFrames(missing, static_cast<CritterStateAnim>(1), static_cast<CritterActionAnim>(1), mdir {0})));
    }
}

TEST_CASE("FontManagerMeasuresAndSplitsText")
{
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, MakeClientTestResources(MakeUnitTestFontResources()));

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    client->FontMngr.BindFoFont(FontType::Default, "UnitTestFont.fofnt", AtlasType::IfaceSprites, false, false, 1.0f);

    SECTION("MetricsAnswerForABoundFont")
    {
        CHECK(client->FontMngr.GetLineHeight(FontType::Default) > 0);
        CHECK(client->FontMngr.GetLinesHeight({200, 100}, "one line", FontType::Default) > 0);
        CHECK(client->FontMngr.GetLinesCount({200, 100}, "one line", FontType::Default) == 1);
        CHECK(client->FontMngr.GetLinesCount({40, 200}, "a sentence long enough to wrap over several lines", FontType::Default) > 1);

        CHECK(client->FontMngr.HaveLetter(FontType::Default, uint32_t {'A'}));
        CHECK_FALSE(client->FontMngr.HaveLetter(FontType::Default, uint32_t {0x4E2D}));
    }

    SECTION("SplittingPaginatesWhatDoesNotFitTheRect")
    {
        // Splitting yields rect-sized pages, not individual lines, so text that fits comes back as one entry
        vector<string> single = client->FontMngr.SplitLines(irect32 {0, 0, 200, 100}, "one line", FontType::Default);
        CHECK(single.size() == 1);

        vector<string> explicit_breaks = client->FontMngr.SplitLines(irect32 {0, 0, 200, 10}, "first\nsecond\nthird", FontType::Default);
        CHECK(explicit_breaks.size() > 1);

        vector<string> wrapped = client->FontMngr.SplitLines(irect32 {0, 0, 40, 10}, "a sentence long enough to wrap over several lines", FontType::Default);
        CHECK(wrapped.size() > 1);

        vector<string> empty = client->FontMngr.SplitLines(irect32 {0, 0, 200, 100}, "", FontType::Default);
        CHECK(empty.empty());
    }

    SECTION("TheBinaryBmfLoaderProducesAUsableFont")
    {
        client->FontMngr.BindBmfFont(FontType::Default, "UnitTestFont.fnt", AtlasType::IfaceSprites, 1.0f);

        CHECK(client->FontMngr.GetLineHeight(FontType::Default) > 0);
        CHECK(client->FontMngr.HaveLetter(FontType::Default, uint32_t {'A'}));
        CHECK(client->FontMngr.GetLinesCount({200, 100}, "bmf line", FontType::Default) == 1);
    }

    SECTION("UnboundSlotsAndMissingFilesAreRejected")
    {
        CHECK_THROWS_AS(client->FontMngr.GetLineHeight(static_cast<FontType>(7)), FontManagerException);
        CHECK_THROWS_AS(client->FontMngr.BindFoFont(FontType::Default, "NoSuchFont.fofnt", AtlasType::IfaceSprites, false, false, 1.0f), FontManagerException);
        CHECK_THROWS_AS(client->FontMngr.BindBmfFont(FontType::Default, "NoSuchFont.fnt", AtlasType::IfaceSprites, 1.0f), FontManagerException);

        // Skip-if-loaded short-circuits before the resource is even looked up
        CHECK_NOTHROW(client->FontMngr.BindFoFont(FontType::Default, "NoSuchFont.fofnt", AtlasType::IfaceSprites, false, true, 1.0f));
    }

    SECTION("EffectOverrideAndTeardownAreAddressable")
    {
        CHECK_NOTHROW(client->FontMngr.SetFontEffect(FontType::Default, nullptr));
        CHECK_NOTHROW(client->FontMngr.ClearFonts());
        CHECK_THROWS_AS(client->FontMngr.GetLineHeight(FontType::Default), FontManagerException);
    }
}

TEST_CASE("ClientEngineGlobalScriptBindings")
{
    auto settings = MakeClientTestSettings();
    auto client_resources = MakeUnitTestFontResources();
    client_resources.emplace_back("Quad.png", BakerTests::MakeMinimalBakedSprite(2, 2));
    auto client = MakeClientEngine(settings, MakeClientTestResources(std::move(client_resources)));

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    // The scripts below change the resolution, which writes through to the process-global app window and would
    // otherwise leave every later test computing ratios against the changed size
    isize32 saved_screen_size = GetApp()->MainWindow.GetScreenSize();
    auto restore_screen_size = scope_exit([saved_screen_size]() noexcept { safe_call([saved_screen_size] { GetApp()->MainWindow.SetScreenSize(saved_screen_size); }); });

    auto run_script = [&client](string_view name) {
        int32_t result = -1;
        INFO(name);
        REQUIRE(client->CallFunc(client->Hashes.ToHashedString(name), result));
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
    REQUIRE(client->CallFunc(client->Hashes.ToHashedString("ClientEngineTest::UnitTestGetClientRejectionCount"), rejection_count));
    // Only four probes must reject; the rest legitimately answer instead of throwing, reporting a bool, queueing
    // nothing, or accepting a pack that resolves to no entries
    CHECK(rejection_count == 8 + 16 + 128 + 256);
}

TEST_CASE("MultiFrameSpritesPlayAndCopy")
{
    // A single-frame sprite resolves to an atlas sprite, so sheet playback has nothing to run on until the fixture
    // serves a real multi-frame one
    auto settings = MakeClientTestSettings();

    vector<pair<string, vector<uint8_t>>> sprite_resources;
    sprite_resources.emplace_back(string {"AnimSheet.png"}, BakerTests::MakeMultiFrameBakedSprite(4, 2, 2, 40));

    auto client = MakeClientEngine(settings, MakeClientTestResources(std::move(sprite_resources)));

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    auto sheet = client->SprMngr.LoadSprite(client->Hashes.ToHashedString("AnimSheet.png"), AtlasType::IfaceSprites);
    REQUIRE(sheet);
    CHECK(sheet->GetSize() == isize32 {2, 2});

    SECTION("PlaybackAdvancesThroughTheFramesAndWrapsWhenLooped")
    {
        REQUIRE_NOTHROW(sheet->PlayDefault());
        CHECK(sheet->IsPlaying());

        // The frame clock is driven by the game time, so the update is run over enough frames to wrap
        for (int32_t frame = 0; frame < 24; frame++) {
            client->GameTime.FrameAdvance(false);
            ignore_unused(sheet->Update());
        }

        CHECK(sheet->IsPlaying());

        // Seeking addresses a frame directly rather than waiting for the clock
        for (float32_t normalized_time : {0.0f, 0.25f, 0.5f, 0.99f, 1.0f}) {
            REQUIRE_NOTHROW(sheet->SetTime(normalized_time));
            ignore_unused(sheet->Update());
        }

        // A one-shot run stops at the end instead of wrapping
        REQUIRE_NOTHROW(sheet->Play({}, false, false));

        for (int32_t frame = 0; frame < 24; frame++) {
            client->GameTime.FrameAdvance(false);
            ignore_unused(sheet->Update());
        }

        // Reversed playback walks the same frames the other way
        REQUIRE_NOTHROW(sheet->Play({}, true, true));

        for (int32_t frame = 0; frame < 12; frame++) {
            client->GameTime.FrameAdvance(false);
            ignore_unused(sheet->Update());
        }

        REQUIRE_NOTHROW(sheet->Stop());
        CHECK_FALSE(sheet->IsPlaying());
    }

    SECTION("ACopyPlaysIndependentlyOfTheSpriteItWasMadeFrom")
    {
        auto copy = sheet->MakeCopy();
        REQUIRE(copy);
        CHECK(copy != sheet);

        copy->PlayDefault();
        CHECK(copy->IsPlaying());
        CHECK_FALSE(sheet->IsPlaying());

        for (int32_t frame = 0; frame < 12; frame++) {
            client->GameTime.FrameAdvance(false);
            ignore_unused(copy->Update());
        }

        CHECK(copy->IsPlaying());
    }
}

TEST_CASE("AtlasSpriteFillDataSupportsBakedMeshes")
{
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings);

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    frect32 atlas_rect = {0.25f, 0.5f, 0.5f, 0.25f};
    frect32 draw_rect = {100.0f, 200.0f, 20.0f, 40.0f};
    ucolor color_left = {10, 20, 30, 40};
    ucolor color_right = {110, 120, 130, 140};

    SECTION("Absent mesh keeps the legacy quad")
    {
        auto sprite = SafeAlloc::MakeShared<AtlasSprite>(&client->SprMngr, isize32 {10, 10}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {});
        auto draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);

        size_t index_count = sprite->FillData(draw_buf, draw_rect, {color_left, color_right});

        REQUIRE(index_count == 6);
        REQUIRE(draw_buf->VertCount == 4);
        REQUIRE(draw_buf->IndCount == 6);
        CHECK(draw_buf->Indices[0] == 0);
        CHECK(draw_buf->Indices[1] == 1);
        CHECK(draw_buf->Indices[2] == 3);
        CHECK(draw_buf->Indices[3] == 1);
        CHECK(draw_buf->Indices[4] == 2);
        CHECK(draw_buf->Indices[5] == 3);
        CHECK(draw_buf->Vertices[0].Color == color_left);
        CHECK(draw_buf->Vertices[1].Color == color_left);
        CHECK(draw_buf->Vertices[2].Color == color_right);
        CHECK(draw_buf->Vertices[3].Color == color_right);
    }

    SECTION("Explicit empty mesh emits no draw data")
    {
        auto sprite = SafeAlloc::MakeShared<AtlasSprite>(&client->SprMngr, isize32 {10, 10}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {}, SpriteMeshData {});
        auto draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);

        size_t index_count = sprite->FillData(draw_buf, draw_rect, {color_left, color_right});

        CHECK(index_count == 0);
        CHECK_FALSE(sprite->ResolveRegion({0.0f, 0.0f}, {1.0f, 1.0f}, draw_rect).has_value());
        CHECK(draw_buf->VertCount == 0);
        CHECK(draw_buf->IndCount == 0);
    }

    SECTION("Mesh maps positions UVs indices and horizontal light colors")
    {
        SpriteMeshData mesh;
        mesh.SourceSize = {10, 10};
        mesh.Vertices = {{0, 0}, {5, 10}, {10, 0}};
        mesh.Indices = {0, 1, 2};

        auto sprite = SafeAlloc::MakeShared<AtlasSprite>(&client->SprMngr, isize32 {10, 10}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)});
        auto draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
        draw_buf->Vertices.resize(2);
        draw_buf->VertCount = 2;
        draw_buf->Indices.resize(1);
        draw_buf->Indices[0] = 0;
        draw_buf->IndCount = 1;

        size_t index_count = sprite->FillData(draw_buf, draw_rect, {color_left, color_right});

        REQUIRE(index_count == 3);
        REQUIRE(draw_buf->VertCount == 5);
        REQUIRE(draw_buf->IndCount == 4);
        CHECK(draw_buf->Indices[1] == 2);
        CHECK(draw_buf->Indices[2] == 3);
        CHECK(draw_buf->Indices[3] == 4);

        const Vertex2D& left = draw_buf->Vertices[2];
        const Vertex2D& center = draw_buf->Vertices[3];
        const Vertex2D& right = draw_buf->Vertices[4];
        CHECK(left.PosX == Catch::Approx(100.0f));
        CHECK(left.PosY == Catch::Approx(200.0f));
        CHECK(left.TexU == Catch::Approx(0.25f));
        CHECK(left.TexV == Catch::Approx(0.5f));
        CHECK(left.Color == color_left);
        CHECK(center.PosX == Catch::Approx(110.0f));
        CHECK(center.PosY == Catch::Approx(240.0f));
        CHECK(center.TexU == Catch::Approx(0.5f));
        CHECK(center.TexV == Catch::Approx(0.75f));
        CHECK(center.Color == (ucolor {60, 70, 80, 90}));
        CHECK(right.PosX == Catch::Approx(120.0f));
        CHECK(right.PosY == Catch::Approx(200.0f));
        CHECK(right.TexU == Catch::Approx(0.75f));
        CHECK(right.TexV == Catch::Approx(0.5f));
        CHECK(right.Color == color_right);
    }

    SECTION("Cropped mesh preserves source-relative horizontal light colors")
    {
        SpriteMeshData mesh;
        mesh.SourceSize = {10, 10};
        mesh.SourceOffset = {2, 0};
        mesh.Vertices = {{0, 0}, {3, 10}, {6, 0}};
        mesh.Indices = {0, 1, 2};

        auto sprite = SafeAlloc::MakeShared<AtlasSprite>(&client->SprMngr, isize32 {6, 10}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)});
        auto draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);

        CHECK(sprite->GetSize() == isize32 {10, 10});
        CHECK(sprite->GetOffset() == ipos32 {});
        REQUIRE(sprite->FillData(draw_buf, draw_rect, {color_left, color_right}) == 3);
        REQUIRE(draw_buf->VertCount == 3);
        CHECK(draw_buf->Vertices[0].PosX == Catch::Approx(104.0f));
        CHECK(draw_buf->Vertices[1].PosX == Catch::Approx(110.0f));
        CHECK(draw_buf->Vertices[2].PosX == Catch::Approx(116.0f));
        CHECK(draw_buf->Vertices[0].TexU == Catch::Approx(0.25f));
        CHECK(draw_buf->Vertices[1].TexU == Catch::Approx(0.5f));
        CHECK(draw_buf->Vertices[2].TexU == Catch::Approx(0.75f));
        CHECK(draw_buf->Vertices[0].Color == (ucolor {30, 40, 50, 60}));
        CHECK(draw_buf->Vertices[1].Color == (ucolor {60, 70, 80, 90}));
        CHECK(draw_buf->Vertices[2].Color == (ucolor {90, 100, 110, 120}));
    }

    SECTION("Cropped mesh region preserves logical source coordinates")
    {
        SpriteMeshData mesh;
        mesh.SourceSize = {10, 10};
        mesh.SourceOffset = {2, 3};
        mesh.Vertices = {{0, 0}, {6, 0}, {0, 5}};
        mesh.Indices = {0, 1, 2};

        auto sprite = SafeAlloc::MakeShared<AtlasSprite>(&client->SprMngr, isize32 {6, 5}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)});
        auto draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
        optional<AtlasSpriteRegion> region = sprite->ResolveRegion({0.0f, 0.0f}, {1.0f, 1.0f}, draw_rect);

        REQUIRE(region.has_value());
        CHECK(region->DrawRect.x == Catch::Approx(104.0f));
        CHECK(region->DrawRect.y == Catch::Approx(212.0f));
        CHECK(region->DrawRect.width == Catch::Approx(12.0f));
        CHECK(region->DrawRect.height == Catch::Approx(20.0f));
        CHECK(region->TextureRect.x == Catch::Approx(0.25f));
        CHECK(region->TextureRect.y == Catch::Approx(0.5f));
        CHECK(region->TextureRect.width == Catch::Approx(0.5f));
        CHECK(region->TextureRect.height == Catch::Approx(0.25f));
        REQUIRE(sprite->FillRegionData(draw_buf, {0.0f, 0.0f}, {1.0f, 1.0f}, draw_rect, color_left) == 6);
        REQUIRE(draw_buf->VertCount == 4);
        CHECK(draw_buf->Vertices[0].PosX == Catch::Approx(104.0f));
        CHECK(draw_buf->Vertices[0].PosY == Catch::Approx(232.0f));
        CHECK(draw_buf->Vertices[0].TexU == Catch::Approx(0.25f));
        CHECK(draw_buf->Vertices[0].TexV == Catch::Approx(0.75f));
        CHECK(draw_buf->Vertices[1].PosX == Catch::Approx(104.0f));
        CHECK(draw_buf->Vertices[1].PosY == Catch::Approx(212.0f));
        CHECK(draw_buf->Vertices[1].TexU == Catch::Approx(0.25f));
        CHECK(draw_buf->Vertices[1].TexV == Catch::Approx(0.5f));
        CHECK(draw_buf->Vertices[2].PosX == Catch::Approx(116.0f));
        CHECK(draw_buf->Vertices[2].PosY == Catch::Approx(212.0f));
        CHECK(draw_buf->Vertices[2].TexU == Catch::Approx(0.75f));
        CHECK(draw_buf->Vertices[2].TexV == Catch::Approx(0.5f));
    }

    SECTION("Expanded mesh region clips atlas padding outside the logical source")
    {
        SpriteMeshData mesh;
        mesh.SourceSize = {10, 10};
        mesh.SourceOffset = {-2, -1};
        mesh.Vertices = {{0, 0}, {14, 0}, {0, 13}};
        mesh.Indices = {0, 1, 2};

        auto sprite = SafeAlloc::MakeShared<AtlasSprite>(&client->SprMngr, isize32 {14, 13}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)});
        auto draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
        optional<AtlasSpriteRegion> region = sprite->ResolveRegion({0.0f, 0.0f}, {1.0f, 1.0f}, draw_rect);

        REQUIRE(region.has_value());
        CHECK(region->DrawRect.x == Catch::Approx(100.0f));
        CHECK(region->DrawRect.y == Catch::Approx(200.0f));
        CHECK(region->DrawRect.width == Catch::Approx(20.0f));
        CHECK(region->DrawRect.height == Catch::Approx(40.0f));
        CHECK(region->TextureRect.x == Catch::Approx(0.25f + 0.5f * 2.0f / 14.0f));
        CHECK(region->TextureRect.y == Catch::Approx(0.5f + 0.25f / 13.0f));
        CHECK(region->TextureRect.width == Catch::Approx(0.5f * 10.0f / 14.0f));
        CHECK(region->TextureRect.height == Catch::Approx(0.25f * 10.0f / 13.0f));
        REQUIRE(sprite->FillRegionData(draw_buf, {0.0f, 0.0f}, {1.0f, 1.0f}, draw_rect, color_left) == 6);
        REQUIRE(draw_buf->VertCount == 4);
        CHECK(draw_buf->Vertices[0].PosX == Catch::Approx(100.0f));
        CHECK(draw_buf->Vertices[0].PosY == Catch::Approx(240.0f));
        CHECK(draw_buf->Vertices[0].TexU == Catch::Approx(0.25f + 0.5f * 2.0f / 14.0f));
        CHECK(draw_buf->Vertices[0].TexV == Catch::Approx(0.5f + 0.25f * 11.0f / 13.0f));
        CHECK(draw_buf->Vertices[2].PosX == Catch::Approx(120.0f));
        CHECK(draw_buf->Vertices[2].PosY == Catch::Approx(200.0f));
        CHECK(draw_buf->Vertices[2].TexU == Catch::Approx(0.25f + 0.5f * 12.0f / 14.0f));
        CHECK(draw_buf->Vertices[2].TexV == Catch::Approx(0.5f + 0.25f / 13.0f));
    }

    SECTION("Partial logical region intersects the cropped atlas frame")
    {
        SpriteMeshData mesh;
        mesh.SourceSize = {10, 10};
        mesh.SourceOffset = {2, 3};
        mesh.Vertices = {{0, 0}, {6, 0}, {0, 5}};
        mesh.Indices = {0, 1, 2};

        auto sprite = SafeAlloc::MakeShared<AtlasSprite>(&client->SprMngr, isize32 {6, 5}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)});
        optional<AtlasSpriteRegion> region = sprite->ResolveRegion({0.1f, 0.2f}, {0.5f, 0.6f}, draw_rect);

        REQUIRE(region.has_value());
        CHECK(region->DrawRect.x == Catch::Approx(105.0f));
        CHECK(region->DrawRect.y == Catch::Approx(210.0f));
        CHECK(region->DrawRect.width == Catch::Approx(15.0f));
        CHECK(region->DrawRect.height == Catch::Approx(30.0f));
        CHECK(region->TextureRect.x == Catch::Approx(0.25f));
        CHECK(region->TextureRect.y == Catch::Approx(0.5f));
        CHECK(region->TextureRect.width == Catch::Approx(0.25f));
        CHECK(region->TextureRect.height == Catch::Approx(0.15f));
    }

    SECTION("Live atlas allocation observes sprite mesh metadata for dump lifetime")
    {
        TextureAtlasLayout layout {{12, 12}};
        auto atlas_allocation = layout.Allocate({12, 12});
        REQUIRE(atlas_allocation);
        nptr<TextureAtlasLayout::Allocation> allocation_observer = atlas_allocation.as_nptr();
        SpriteMeshData mesh;
        mesh.SourceSize = {10, 10};
        mesh.Vertices = {{0, 0}, {5, 5}, {10, 0}};
        mesh.Indices = {0, 1, 2};

        {
            auto sprite = SafeAlloc::MakeShared<AtlasSprite>(&client->SprMngr, isize32 {10, 10}, ipos32 {}, nullptr, std::move(atlas_allocation), atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)});
            auto draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);

            REQUIRE(allocation_observer->GetSpriteMesh());
            CHECK(allocation_observer->GetSpriteMesh()->Vertices.size() == 3);
            CHECK(sprite->FillData(draw_buf, draw_rect, {color_left, color_right}) == 3);
        }

        CHECK_FALSE(allocation_observer->IsActive());
        CHECK(allocation_observer->GetSpriteMesh() == nullptr);
    }

    SECTION("Moving an atlas sprite rebinds the allocation mesh observer")
    {
        TextureAtlasLayout layout {{12, 12}};
        auto atlas_allocation = layout.Allocate({12, 12});
        REQUIRE(atlas_allocation);
        nptr<TextureAtlasLayout::Allocation> allocation_observer = atlas_allocation.as_nptr();
        SpriteMeshData mesh;
        mesh.SourceSize = {10, 10};
        mesh.Vertices = {{0, 0}, {5, 5}, {10, 0}};
        mesh.Indices = {0, 1, 2};

        {
            AtlasSprite source {&client->SprMngr, isize32 {10, 10}, ipos32 {}, nullptr, std::move(atlas_allocation), atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)}};
            nptr<const SpriteMeshData> source_mesh = allocation_observer->GetSpriteMesh();
            AtlasSprite moved {std::move(source)};

            REQUIRE(allocation_observer->GetSpriteMesh());
            CHECK(allocation_observer->GetSpriteMesh() != source_mesh);
            auto draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
            CHECK(moved.FillData(draw_buf, draw_rect, {color_left, color_right}) == 3);
        }

        CHECK_FALSE(allocation_observer->IsActive());
        CHECK(allocation_observer->GetSpriteMesh() == nullptr);
    }
}

TEST_CASE("DefaultSpriteFactoryValidatesBakedMeshPayload")
{
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings);

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    SpriteMeshData mesh;
    mesh.SourceSize = {2, 2};
    mesh.Vertices = {{0, 0}, {2, 0}, {0, 2}};
    mesh.Indices = {0, 1, 2};

    vector<uint8_t> valid_blob = BakerTests::MakeMinimalBakedSprite(2, 2, SpriteMeshKind::Mesh, mesh);
    SpriteMeshData cropped_mesh;
    cropped_mesh.SourceSize = {4, 3};
    cropped_mesh.SourceOffset = {1, -1};
    cropped_mesh.Vertices = {{0, 0}, {3, 0}, {0, 3}};
    cropped_mesh.Indices = {0, 1, 2};
    constexpr size_t mesh_kind_offset = 20 + 2 * 2 * sizeof(ucolor);
    constexpr size_t mesh_vertex_count_offset = mesh_kind_offset + 1;
    constexpr size_t mesh_index_count_offset = mesh_vertex_count_offset + sizeof(uint16_t);
    constexpr size_t mesh_source_size_offset = mesh_index_count_offset + sizeof(uint32_t);
    constexpr size_t mesh_source_offset_offset = mesh_source_size_offset + sizeof(uint16_t) * 2;
    constexpr size_t mesh_vertices_offset = mesh_source_offset_offset + sizeof(int32_t) * 2;
    constexpr size_t mesh_indices_offset = mesh_vertices_offset + 3 * sizeof(uint16_t) * 2;

    auto write_u16 = [](vector<uint8_t>& data, size_t offset, uint16_t value) {
        data[offset] = numeric_cast<uint8_t>(value & 0xFF);
        data[offset + 1] = numeric_cast<uint8_t>(value >> 8);
    };
    auto write_u32 = [](vector<uint8_t>& data, size_t offset, uint32_t value) {
        data[offset] = numeric_cast<uint8_t>(value & 0xFF);
        data[offset + 1] = numeric_cast<uint8_t>((value >> 8) & 0xFF);
        data[offset + 2] = numeric_cast<uint8_t>((value >> 16) & 0xFF);
        data[offset + 3] = numeric_cast<uint8_t>(value >> 24);
    };

    auto source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("PolygonSpriteResources");
    source->AddFile("Quad.png", BakerTests::MakeMinimalBakedSprite(2, 2));
    source->AddFile("Empty.png", BakerTests::MakeMinimalBakedSprite(2, 2, SpriteMeshKind::Empty));
    source->AddFile("ValidMesh.png", valid_blob);
    source->AddFile("CroppedMesh.png", BakerTests::MakeMinimalBakedSprite(3, 3, SpriteMeshKind::Mesh, cropped_mesh));

    vector<uint8_t> bad_version = valid_blob;
    bad_version[1]++;
    source->AddFile("BadVersion.png", std::move(bad_version));

    vector<uint8_t> bad_kind = valid_blob;
    bad_kind[mesh_kind_offset] = 0xFF;
    source->AddFile("BadKind.png", std::move(bad_kind));

    vector<uint8_t> bad_vertex_count = valid_blob;
    write_u16(bad_vertex_count, mesh_vertex_count_offset, uint16_t {2});
    source->AddFile("BadVertexCount.png", std::move(bad_vertex_count));

    vector<uint8_t> bad_index_count = valid_blob;
    write_u32(bad_index_count, mesh_index_count_offset, uint32_t {4});
    source->AddFile("BadIndexCount.png", std::move(bad_index_count));

    vector<uint8_t> implausible_index_count = valid_blob;
    write_u32(implausible_index_count, mesh_index_count_offset, uint32_t {21});
    source->AddFile("ImplausibleIndexCount.png", std::move(implausible_index_count));

    vector<uint8_t> bad_source_size = valid_blob;
    write_u16(bad_source_size, mesh_source_size_offset, uint16_t {0});
    source->AddFile("BadSourceSize.png", std::move(bad_source_size));

    vector<uint8_t> bad_source_offset = valid_blob;
    write_u32(bad_source_offset, mesh_source_offset_offset, uint32_t {2});
    source->AddFile("BadSourceOffset.png", std::move(bad_source_offset));

    vector<uint8_t> bad_coordinate = valid_blob;
    write_u16(bad_coordinate, mesh_vertices_offset, uint16_t {3});
    source->AddFile("BadCoordinate.png", std::move(bad_coordinate));

    vector<uint8_t> bad_index = valid_blob;
    write_u16(bad_index, mesh_indices_offset, uint16_t {3});
    source->AddFile("BadIndex.png", std::move(bad_index));

    vector<uint8_t> degenerate_triangle = valid_blob;
    write_u16(degenerate_triangle, mesh_vertices_offset + 2 * sizeof(uint16_t) * 2, uint16_t {1});
    write_u16(degenerate_triangle, mesh_vertices_offset + 2 * sizeof(uint16_t) * 2 + sizeof(uint16_t), uint16_t {0});
    source->AddFile("DegenerateTriangle.png", std::move(degenerate_triangle));

    SpriteMeshData inconsistent_winding_mesh;
    inconsistent_winding_mesh.Vertices = {{0, 0}, {2, 0}, {0, 2}, {2, 2}};
    inconsistent_winding_mesh.Indices = {0, 1, 2, 1, 2, 3};
    source->AddFile("InconsistentWinding.png", BakerTests::MakeMinimalBakedSprite(2, 2, SpriteMeshKind::Mesh, inconsistent_winding_mesh));

    vector<uint8_t> trailing_data = valid_blob;
    trailing_data.emplace_back(uint8_t {0});
    source->AddFile("TrailingData.png", std::move(trailing_data));

    vector<uint8_t> truncated_payload = valid_blob;
    truncated_payload.resize(mesh_indices_offset);
    source->AddFile("TruncatedPayload.png", std::move(truncated_payload));

    client->SprMngr.GetResources()->AddCustomSource(std::move(source));
    DefaultSpriteFactory factory {&client->SprMngr};
    auto load = [&client, &factory](string_view path) { return factory.LoadSprite(client->Hashes.ToHashedString(path), AtlasType::MapSprites); };

    auto valid_sprite = load("ValidMesh.png");
    REQUIRE(static_cast<bool>(valid_sprite));
    auto valid_draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
    CHECK(valid_sprite->FillData(valid_draw_buf, frect32 {0.0f, 0.0f, 2.0f, 2.0f}, {ucolor {0, 0, 0}, ucolor {255, 255, 255}}) == 3);

    auto cropped_sprite = load("CroppedMesh.png");
    REQUIRE(cropped_sprite);
    CHECK(cropped_sprite->GetSize() == cropped_mesh.SourceSize);
    CHECK(cropped_sprite->GetOffset() == ipos32 {0, 1});
    CHECK_FALSE(cropped_sprite->IsHitTest({0, 0}));
    CHECK(cropped_sprite->IsHitTest({1, 0}));
    CHECK(cropped_sprite->IsHitTest({3, 1}));
    CHECK_FALSE(cropped_sprite->IsHitTest({3, 2}));
    auto cropped_draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
    REQUIRE(cropped_sprite->FillData(cropped_draw_buf, frect32 {0.0f, 0.0f, 4.0f, 3.0f}, {ucolor {0, 0, 0}, ucolor {255, 255, 255}}) == 3);
    REQUIRE(cropped_draw_buf->VertCount == 3);
    CHECK(cropped_draw_buf->Vertices[0].PosX == Catch::Approx(1.0f));
    CHECK(cropped_draw_buf->Vertices[0].PosY == Catch::Approx(-1.0f));
    CHECK(cropped_draw_buf->Vertices[1].PosX == Catch::Approx(4.0f));
    CHECK(cropped_draw_buf->Vertices[1].PosY == Catch::Approx(-1.0f));
    CHECK(cropped_draw_buf->Vertices[2].PosX == Catch::Approx(1.0f));
    CHECK(cropped_draw_buf->Vertices[2].PosY == Catch::Approx(2.0f));

    auto restored_image = client->SprMngr.LoadSpriteAsQuad(client->Hashes.ToHashedString("CroppedMesh.png"), AtlasType::IfaceSprites);
    REQUIRE(restored_image);
    CHECK(restored_image->GetSize() == cropped_mesh.SourceSize);
    CHECK(restored_image->GetAtlasRect().width * restored_image->GetAtlas()->GetTexture()->SizeData[0] == Catch::Approx(4.0f));
    CHECK(restored_image->GetAtlasRect().height * restored_image->GetAtlas()->GetTexture()->SizeData[1] == Catch::Approx(3.0f));
    optional<AtlasSpriteRegion> restored_region = restored_image->ResolveRegion({0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 4.0f, 3.0f});
    REQUIRE(restored_region.has_value());
    CHECK(restored_region->DrawRect == frect32 {0.0f, 0.0f, 4.0f, 3.0f});
    CHECK(restored_region->TextureRect == restored_image->GetAtlasRect());
    auto restored_draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
    CHECK(restored_image->FillData(restored_draw_buf, frect32 {0.0f, 0.0f, 4.0f, 3.0f}, {ucolor {0, 0, 0}, ucolor {255, 255, 255}}) == 6);

    auto quad_sprite = load("Quad.png");
    REQUIRE(static_cast<bool>(quad_sprite));
    auto quad_draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
    CHECK(quad_sprite->FillData(quad_draw_buf, frect32 {0.0f, 0.0f, 2.0f, 2.0f}, {ucolor {0, 0, 0}, ucolor {255, 255, 255}}) == 6);

    auto empty_sprite = load("Empty.png");
    REQUIRE(static_cast<bool>(empty_sprite));
    auto empty_draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
    CHECK(empty_sprite->FillData(empty_draw_buf, frect32 {0.0f, 0.0f, 2.0f, 2.0f}, {ucolor {0, 0, 0}, ucolor {255, 255, 255}}) == 0);

    CHECK_THROWS(load("BadVersion.png"));
    CHECK_THROWS(load("BadKind.png"));
    CHECK_THROWS(load("BadVertexCount.png"));
    CHECK_THROWS(load("BadIndexCount.png"));
    CHECK_THROWS(load("ImplausibleIndexCount.png"));
    CHECK_THROWS(load("BadSourceSize.png"));
    CHECK_THROWS(load("BadSourceOffset.png"));
    CHECK_THROWS(load("BadCoordinate.png"));
    CHECK_THROWS(load("BadIndex.png"));
    CHECK_THROWS(load("DegenerateTriangle.png"));
    CHECK_THROWS(load("InconsistentWinding.png"));
    CHECK_THROWS(load("TrailingData.png"));
    CHECK_THROWS(load("TruncatedPayload.png"));
}

TEST_CASE("SpriteManagerMapsPolygonAtlasPatternsAndPaddedEffects")
{
    SpriteMeshData mesh;
    mesh.SourceSize = {4, 3};
    mesh.SourceOffset = {1, 0};
    mesh.Vertices = {{0, 0}, {3, 0}, {0, 3}};
    mesh.Indices = {0, 1, 2};

    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, MakeClientTestResources({{"PatternMesh.png", BakerTests::MakeMinimalBakedSprite(3, 3, SpriteMeshKind::Mesh, mesh)}}));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    RecordingQuadEffect effect;

    auto sprite = client->SprMngr.LoadSprite("PatternMesh.png", AtlasType::IfaceSprites);
    REQUIRE(sprite);
    auto atlas_sprite = sprite.dyn_cast<AtlasSprite>();
    REQUIRE(atlas_sprite);
    sprite->SetDrawEffect(make_nptr(&effect));

    client->SprMngr.DrawSpritePattern(sprite, {10, 20}, {7, 5}, {4, 3}, ucolor {255, 255, 255, 255});
    client->SprMngr.Flush();

    REQUIRE(effect.Draws.size() == 1);
    const RecordedQuadDraw& pattern_draw = effect.Draws.front();
    REQUIRE(pattern_draw.Vertices.size() == 16);
    REQUIRE(pattern_draw.Indices.size() == 24);
    CHECK(pattern_draw.StartIndex == 0);
    REQUIRE(pattern_draw.IndicesToDraw.has_value());
    CHECK(*pattern_draw.IndicesToDraw == 24);
    CHECK(pattern_draw.CustomTexture == atlas_sprite->GetBatchTexture());

    frect32 atlas_rect = atlas_sprite->GetAtlasRect();
    auto check_quad = [](const RecordedQuadDraw& draw, size_t vertex, frect32 draw_rect, frect32 texture_rect) {
        REQUIRE(vertex + 3 < draw.Vertices.size());
        CHECK(draw.Vertices[vertex + 0].PosX == Catch::Approx(draw_rect.x));
        CHECK(draw.Vertices[vertex + 0].PosY == Catch::Approx(draw_rect.y + draw_rect.height));
        CHECK(draw.Vertices[vertex + 0].TexU == Catch::Approx(texture_rect.x));
        CHECK(draw.Vertices[vertex + 0].TexV == Catch::Approx(texture_rect.y + texture_rect.height));
        CHECK(draw.Vertices[vertex + 1].PosX == Catch::Approx(draw_rect.x));
        CHECK(draw.Vertices[vertex + 1].PosY == Catch::Approx(draw_rect.y));
        CHECK(draw.Vertices[vertex + 1].TexU == Catch::Approx(texture_rect.x));
        CHECK(draw.Vertices[vertex + 1].TexV == Catch::Approx(texture_rect.y));
        CHECK(draw.Vertices[vertex + 2].PosX == Catch::Approx(draw_rect.x + draw_rect.width));
        CHECK(draw.Vertices[vertex + 2].PosY == Catch::Approx(draw_rect.y));
        CHECK(draw.Vertices[vertex + 2].TexU == Catch::Approx(texture_rect.x + texture_rect.width));
        CHECK(draw.Vertices[vertex + 2].TexV == Catch::Approx(texture_rect.y));
        CHECK(draw.Vertices[vertex + 3].PosX == Catch::Approx(draw_rect.x + draw_rect.width));
        CHECK(draw.Vertices[vertex + 3].PosY == Catch::Approx(draw_rect.y + draw_rect.height));
        CHECK(draw.Vertices[vertex + 3].TexU == Catch::Approx(texture_rect.x + texture_rect.width));
        CHECK(draw.Vertices[vertex + 3].TexV == Catch::Approx(texture_rect.y + texture_rect.height));
    };

    check_quad(pattern_draw, 0, {11.0f, 20.0f, 3.0f, 3.0f}, atlas_rect);
    check_quad(pattern_draw, 4, {15.0f, 20.0f, 2.0f, 3.0f}, {atlas_rect.x, atlas_rect.y, atlas_rect.width * 2.0f / 3.0f, atlas_rect.height});
    check_quad(pattern_draw, 8, {11.0f, 23.0f, 3.0f, 2.0f}, {atlas_rect.x, atlas_rect.y, atlas_rect.width, atlas_rect.height * 2.0f / 3.0f});
    check_quad(pattern_draw, 12, {15.0f, 23.0f, 2.0f, 2.0f}, {atlas_rect.x, atlas_rect.y, atlas_rect.width * 2.0f / 3.0f, atlas_rect.height * 2.0f / 3.0f});

    effect.Draws.clear();
    REQUIRE(client->SprMngr.DrawSpriteRegion(sprite, {0.0f, 0.0f}, {1.0f, 1.0f}, {50.0f, 60.0f}, {4.0f, 3.0f}, ucolor {255, 255, 255, 255}));
    client->SprMngr.Flush();
    REQUIRE(effect.Draws.size() == 1);
    const RecordedQuadDraw& region_draw = effect.Draws.front();
    REQUIRE(region_draw.Vertices.size() == 4);
    REQUIRE(region_draw.Indices.size() == 6);
    check_quad(region_draw, 0, {51.0f, 60.0f, 3.0f, 3.0f}, atlas_rect);

    effect.Draws.clear();
    effect.SpriteBorderBuf.reset();
    constexpr int32_t padding = 2;
    client->SprMngr.DrawSpriteWithEffect(sprite, {100, 200}, ucolor {255, 255, 255, 255}, make_ptr(&effect), padding);

    REQUIRE(effect.Draws.size() == 1);
    const RecordedQuadDraw& effect_draw = effect.Draws.front();
    REQUIRE(effect_draw.Vertices.size() == 4);
    REQUIRE(effect_draw.Indices.size() == 6);
    CHECK(effect_draw.StartIndex == 0);
    CHECK_FALSE(effect_draw.IndicesToDraw.has_value());
    CHECK(effect_draw.CustomTexture == atlas_sprite->GetBatchTexture());

    float32_t texture_padding_x = atlas_sprite->GetAtlas()->GetTexture()->SizeData[2] * numeric_cast<float32_t>(padding);
    float32_t texture_padding_y = atlas_sprite->GetAtlas()->GetTexture()->SizeData[3] * numeric_cast<float32_t>(padding);
    frect32 effect_draw_rect {99.0f, 198.0f, 7.0f, 7.0f};
    frect32 effect_texture_rect {atlas_rect.x - texture_padding_x, atlas_rect.y - texture_padding_y, atlas_rect.width + texture_padding_x * 2.0f, atlas_rect.height + texture_padding_y * 2.0f};

    CHECK(effect_draw.Vertices[0].PosX == Catch::Approx(effect_draw_rect.x));
    CHECK(effect_draw.Vertices[0].PosY == Catch::Approx(effect_draw_rect.y + effect_draw_rect.height));
    CHECK(effect_draw.Vertices[0].TexU == Catch::Approx(effect_texture_rect.x));
    CHECK(effect_draw.Vertices[0].TexV == Catch::Approx(effect_texture_rect.y + effect_texture_rect.height));
    CHECK(effect_draw.Vertices[1].PosX == Catch::Approx(effect_draw_rect.x));
    CHECK(effect_draw.Vertices[1].PosY == Catch::Approx(effect_draw_rect.y));
    CHECK(effect_draw.Vertices[1].TexU == Catch::Approx(effect_texture_rect.x));
    CHECK(effect_draw.Vertices[1].TexV == Catch::Approx(effect_texture_rect.y));
    CHECK(effect_draw.Vertices[2].PosX == Catch::Approx(effect_draw_rect.x + effect_draw_rect.width));
    CHECK(effect_draw.Vertices[2].PosY == Catch::Approx(effect_draw_rect.y));
    CHECK(effect_draw.Vertices[2].TexU == Catch::Approx(effect_texture_rect.x + effect_texture_rect.width));
    CHECK(effect_draw.Vertices[2].TexV == Catch::Approx(effect_texture_rect.y));
    CHECK(effect_draw.Vertices[3].PosX == Catch::Approx(effect_draw_rect.x + effect_draw_rect.width));
    CHECK(effect_draw.Vertices[3].PosY == Catch::Approx(effect_draw_rect.y + effect_draw_rect.height));
    CHECK(effect_draw.Vertices[3].TexU == Catch::Approx(effect_texture_rect.x + effect_texture_rect.width));
    CHECK(effect_draw.Vertices[3].TexV == Catch::Approx(effect_texture_rect.y + effect_texture_rect.height));

    REQUIRE(effect.SpriteBorderBuf.has_value());
    CHECK(effect.SpriteBorderBuf->SpriteBorder[0] == Catch::Approx(atlas_rect.x));
    CHECK(effect.SpriteBorderBuf->SpriteBorder[1] == Catch::Approx(atlas_rect.y));
    CHECK(effect.SpriteBorderBuf->SpriteBorder[2] == Catch::Approx(atlas_rect.x + atlas_rect.width));
    CHECK(effect.SpriteBorderBuf->SpriteBorder[3] == Catch::Approx(atlas_rect.y + atlas_rect.height));
    sprite->SetDrawEffect(nullptr);
}

TEST_CASE("SpriteWireframeRendersThroughPrimitiveOverlay")
{
    auto settings = MakeClientTestSettings();
    settings.DrawWireframe = true;
    auto client = MakeClientEngine(settings);

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    SpriteMeshData mesh;
    mesh.SourceSize = {10, 10};
    mesh.Vertices = {{0, 0}, {5, 10}, {10, 0}};
    mesh.Indices = {0, 1, 2};

    auto [atlas, atlas_allocation, atlas_pos] = client->SprMngr.GetAtlasMngr()->FindAtlasPlace(AtlasType::OneImage, {10, 10});
    CHECK(atlas->GetSize() == isize32 {12, 12});
    CHECK(atlas_pos == ipos32 {1, 1});
    CHECK(atlas_allocation->GetPosition() == ipos32 {0, 0});
    CHECK(atlas_allocation->GetSize() == isize32 {12, 12});
    frect32 sprite_atlas_rect {
        numeric_cast<float32_t>(atlas_pos.x) / numeric_cast<float32_t>(atlas->GetSize().width),
        numeric_cast<float32_t>(atlas_pos.y) / numeric_cast<float32_t>(atlas->GetSize().height),
        10.0f / numeric_cast<float32_t>(atlas->GetSize().width),
        10.0f / numeric_cast<float32_t>(atlas->GetSize().height),
    };
    auto sprite = SafeAlloc::MakeShared<AtlasSprite>(&client->SprMngr, isize32 {10, 10}, ipos32 {}, atlas, std::move(atlas_allocation), sprite_atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)});

    client->SprMngr.DrawSprite(sprite, {2, 3}, ucolor {255, 255, 255});
    CHECK_NOTHROW(client->SprMngr.Flush());
}

TEST_CASE("ClientEngineRunsMainLoopHeadlessly")
{
    // The ImGui sweep below writes under `Workspace/`, relative to whatever directory the binary was launched
    // from, and `ImGui::LogToFile` asserts on a file it cannot open — aborting the frame mid-sweep
    (void)fs_create_directories("Workspace");

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
    REQUIRE(client->CallFunc(client->Hashes.ToHashedString("ClientEngineTest::UnitTestGetLoopCalls"), loop_calls));
    CHECK(loop_calls > 0);

    int32_t render_calls = 0;
    REQUIRE(client->CallFunc(client->Hashes.ToHashedString("ClientEngineTest::UnitTestGetRenderCalls"), render_calls));
    CHECK(render_calls > 0);

    // Re-run with script drawing enabled so the render-pass-only draw bindings execute
    REQUIRE(client->CallFunc<void>(client->Hashes.ToHashedString("ClientEngineTest::UnitTestEnableRenderDrawing")));

    for (int32_t frame = 0; frame < 2; frame++) {
        ImGui::NewFrame();
        REQUIRE_NOTHROW(client->MainLoop());
        ImGui::Render();
    }

    // An empty label must surface as a script exception rather than an unaddressable widget; the sweep runs inside
    // the render pass because the accessor exists only while a frame is open
    int32_t probe_count = 0;
    int32_t rejections = 0;
    REQUIRE(client->CallFunc(client->Hashes.ToHashedString("ClientEngineTest::UnitTestImGuiEmptyIdProbeCount"), probe_count));
    REQUIRE(client->CallFunc(client->Hashes.ToHashedString("ClientEngineTest::UnitTestGetImGuiEmptyIdRejections"), rejections));
    CHECK(probe_count > 0);
    CHECK(rejections > 0);
    CHECK(rejections % probe_count == 0);

    int32_t sprite_rejections = 0;
    REQUIRE(client->CallFunc(client->Hashes.ToHashedString("ClientEngineTest::UnitTestGetImGuiSpriteRejections"), sprite_rejections));
    CHECK(sprite_rejections == 4);

    // The surface guard keeps ImGui balanced when a probe throws, but a throw still means a binding
    // misbehaved - the engine swallows it inside the event, so it is only visible through this counter
    int32_t surface_failures = 0;
    REQUIRE(client->CallFunc(client->Hashes.ToHashedString("ClientEngineTest::UnitTestGetImGuiSurfaceFailures"), surface_failures));

    string failed_stage;
    REQUIRE(client->CallFunc(client->Hashes.ToHashedString("ClientEngineTest::UnitTestGetImGuiFailedStage"), failed_stage));
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
