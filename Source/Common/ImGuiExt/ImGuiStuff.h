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
//

#pragma once

#include "Common.h"

#include "Entity.h"
#include "Properties.h"

#include "imgui.h"
#include "imgui_internal.h"

namespace ImGuiExt
{
    void Init();
    auto LoadIniSettingsIfContext(std::string_view ini_data) -> bool;
}

FO_BEGIN_NAMESPACE

class BaseEngine;

///@ ExportEntity ImGui ScriptImGui ScriptImGui Global // Global script receiver bound to the current engine's Dear ImGui context; individual methods define their active-frame and balanced-scope requirements.
class ScriptImGui : public Entity
{
public:
    explicit ScriptImGui(ptr<BaseEngine> engine);
    ScriptImGui(const ScriptImGui&) = delete;
    ScriptImGui(ScriptImGui&&) noexcept = delete;
    auto operator=(const ScriptImGui&) = delete;
    auto operator=(ScriptImGui&&) noexcept = delete;
    ~ScriptImGui() override = default;

    [[nodiscard]] auto GetName() const noexcept -> string_view override { return "ImGui"; }
    [[nodiscard]] auto IsGlobal() const noexcept -> bool override { return true; }
    [[nodiscard]] auto GetEngine() noexcept -> ptr<BaseEngine> { return _engine; }

private:
    ptr<BaseEngine> _engine;
};

// Window creation and interaction flags forwarded to the embedded Dear ImGui runtime.
///@ ExportEnum
enum class ImGui_WindowFlags : uint32_t
{
    None = 0, // ImGuiWindowFlags_None
    NoTitleBar = 1, // ImGuiWindowFlags_NoTitleBar
    NoResize = 2, // ImGuiWindowFlags_NoResize
    NoMove = 4, // ImGuiWindowFlags_NoMove
    NoScrollbar = 8, // ImGuiWindowFlags_NoScrollbar
    NoScrollWithMouse = 16, // ImGuiWindowFlags_NoScrollWithMouse
    NoCollapse = 32, // ImGuiWindowFlags_NoCollapse
    AlwaysAutoResize = 64, // ImGuiWindowFlags_AlwaysAutoResize
    NoBackground = 128, // ImGuiWindowFlags_NoBackground
    NoSavedSettings = 256, // ImGuiWindowFlags_NoSavedSettings
    NoMouseInputs = 512, // ImGuiWindowFlags_NoMouseInputs
    MenuBar = 1024, // ImGuiWindowFlags_MenuBar
    HorizontalScrollbar = 2048, // ImGuiWindowFlags_HorizontalScrollbar
    NoFocusOnAppearing = 4096, // ImGuiWindowFlags_NoFocusOnAppearing
    NoBringToFrontOnFocus = 8192, // ImGuiWindowFlags_NoBringToFrontOnFocus
    AlwaysVerticalScrollbar = 16384, // ImGuiWindowFlags_AlwaysVerticalScrollbar
    AlwaysHorizontalScrollbar = 32768, // ImGuiWindowFlags_AlwaysHorizontalScrollbar
    NoNavInputs = 65536, // ImGuiWindowFlags_NoNavInputs
    NoNavFocus = 131072, // ImGuiWindowFlags_NoNavFocus
    UnsavedDocument = 262144, // ImGuiWindowFlags_UnsavedDocument
    NoNav = 196608, // ImGuiWindowFlags_NoNav
    NoDecoration = 43, // ImGuiWindowFlags_NoDecoration
    NoInputs = 197120, // ImGuiWindowFlags_NoInputs
};

// Child-window sizing, framing, padding, and navigation flags forwarded to Dear ImGui.
///@ ExportEnum
enum class ImGui_ChildFlags : uint32_t
{
    None = 0, // ImGuiChildFlags_None
    Border = 1, // ImGuiChildFlags_Borders
    AlwaysUseWindowPadding = 2, // ImGuiChildFlags_AlwaysUseWindowPadding
    ResizeX = 4, // ImGuiChildFlags_ResizeX
    ResizeY = 8, // ImGuiChildFlags_ResizeY
    AutoResizeX = 16, // ImGuiChildFlags_AutoResizeX
    AutoResizeY = 32, // ImGuiChildFlags_AutoResizeY
    AlwaysAutoResize = 64, // ImGuiChildFlags_AlwaysAutoResize
    FrameStyle = 128, // ImGuiChildFlags_FrameStyle
    NavFlattened = 256, // ImGuiChildFlags_NavFlattened
};

// Conditions controlling when a queued Dear ImGui state assignment takes effect.
///@ ExportEnum
enum class ImGui_Cond : uint32_t
{
    None = 0, // ImGuiCond_None
    Always = 1, // ImGuiCond_Always
    Once = 2, // ImGuiCond_Once
    FirstUseEver = 4, // ImGuiCond_FirstUseEver
    Appearing = 8, // ImGuiCond_Appearing
};

// Selection, spanning, overlap, and activation behavior for Dear ImGui selectable items.
///@ ExportEnum
enum class ImGui_SelectableFlags : uint32_t
{
    None = 0, // ImGuiSelectableFlags_None
    NoAutoClosePopups = 1, // ImGuiSelectableFlags_NoAutoClosePopups
    SpanAllColumns = 2, // ImGuiSelectableFlags_SpanAllColumns
    AllowDoubleClick = 4, // ImGuiSelectableFlags_AllowDoubleClick
    Disabled = 8, // ImGuiSelectableFlags_Disabled
    AllowOverlap = 16, // ImGuiSelectableFlags_AllowOverlap
};

// Expansion, framing, selection, spanning, and navigation flags for Dear ImGui tree nodes.
///@ ExportEnum
enum class ImGui_TreeNodeFlags : uint32_t
{
    None = 0, // ImGuiTreeNodeFlags_None
    Selected = 1, // ImGuiTreeNodeFlags_Selected
    Framed = 2, // ImGuiTreeNodeFlags_Framed
    AllowOverlap = 4, // ImGuiTreeNodeFlags_AllowOverlap
    NoTreePushOnOpen = 8, // ImGuiTreeNodeFlags_NoTreePushOnOpen
    NoAutoOpenOnLog = 16, // ImGuiTreeNodeFlags_NoAutoOpenOnLog
    DefaultOpen = 32, // ImGuiTreeNodeFlags_DefaultOpen
    OpenOnDoubleClick = 64, // ImGuiTreeNodeFlags_OpenOnDoubleClick
    OpenOnArrow = 128, // ImGuiTreeNodeFlags_OpenOnArrow
    Leaf = 256, // ImGuiTreeNodeFlags_Leaf
    Bullet = 512, // ImGuiTreeNodeFlags_Bullet
    FramePadding = 1024, // ImGuiTreeNodeFlags_FramePadding
    SpanAvailWidth = 2048, // ImGuiTreeNodeFlags_SpanAvailWidth
    SpanFullWidth = 4096, // ImGuiTreeNodeFlags_SpanFullWidth
    SpanLabelWidth = 8192, // ImGuiTreeNodeFlags_SpanLabelWidth
    SpanAllColumns = 16384, // ImGuiTreeNodeFlags_SpanAllColumns
    CollapsingHeader = 26, // ImGuiTreeNodeFlags_CollapsingHeader
};

// Scope and hierarchy filters used by Dear ImGui focus queries.
///@ ExportEnum
enum class ImGui_FocusedFlags : uint32_t
{
    None = 0, // ImGuiFocusedFlags_None
    ChildWindows = 1, // ImGuiFocusedFlags_ChildWindows
    RootWindow = 2, // ImGuiFocusedFlags_RootWindow
    AnyWindow = 4, // ImGuiFocusedFlags_AnyWindow
    NoPopupHierarchy = 8, // ImGuiFocusedFlags_NoPopupHierarchy
    RootAndChildWindows = 3, // ImGuiFocusedFlags_RootAndChildWindows
};

// Blocking, overlap, timing, and hierarchy filters used by Dear ImGui hover queries.
///@ ExportEnum
enum class ImGui_HoveredFlags : uint32_t
{
    None = 0, // ImGuiHoveredFlags_None
    ChildWindows = 1, // ImGuiHoveredFlags_ChildWindows
    RootWindow = 2, // ImGuiHoveredFlags_RootWindow
    AnyWindow = 4, // ImGuiHoveredFlags_AnyWindow
    NoPopupHierarchy = 8, // ImGuiHoveredFlags_NoPopupHierarchy
    AllowWhenBlockedByPopup = 32, // ImGuiHoveredFlags_AllowWhenBlockedByPopup
    AllowWhenBlockedByActiveItem = 128, // ImGuiHoveredFlags_AllowWhenBlockedByActiveItem
    AllowWhenOverlappedByItem = 256, // ImGuiHoveredFlags_AllowWhenOverlappedByItem
    AllowWhenOverlappedByWindow = 512, // ImGuiHoveredFlags_AllowWhenOverlappedByWindow
    AllowWhenDisabled = 1024, // ImGuiHoveredFlags_AllowWhenDisabled
    NoNavOverride = 2048, // ImGuiHoveredFlags_NoNavOverride
    AllowWhenOverlapped = 768, // ImGuiHoveredFlags_AllowWhenOverlapped
    RectOnly = 928, // ImGuiHoveredFlags_RectOnly
    ForTooltip = 4096, // ImGuiHoveredFlags_ForTooltip
};

// Layout, borders, sizing, scrolling, sorting, and clipping behavior for Dear ImGui tables.
///@ ExportEnum
enum class ImGui_TableFlags : uint32_t
{
    None = 0, // ImGuiTableFlags_None
    Resizable = 1, // ImGuiTableFlags_Resizable
    Reorderable = 2, // ImGuiTableFlags_Reorderable
    Hideable = 4, // ImGuiTableFlags_Hideable
    Sortable = 8, // ImGuiTableFlags_Sortable
    NoSavedSettings = 16, // ImGuiTableFlags_NoSavedSettings
    ContextMenuInBody = 32, // ImGuiTableFlags_ContextMenuInBody
    RowBg = 64, // ImGuiTableFlags_RowBg
    BordersInnerH = 128, // ImGuiTableFlags_BordersInnerH
    BordersOuterH = 256, // ImGuiTableFlags_BordersOuterH
    BordersInnerV = 512, // ImGuiTableFlags_BordersInnerV
    BordersOuterV = 1024, // ImGuiTableFlags_BordersOuterV
    BordersH = 384, // ImGuiTableFlags_BordersH
    BordersV = 1536, // ImGuiTableFlags_BordersV
    BordersInner = 640, // ImGuiTableFlags_BordersInner
    BordersOuter = 1280, // ImGuiTableFlags_BordersOuter
    Borders = 1920, // ImGuiTableFlags_Borders
    NoBordersInBody = 2048, // ImGuiTableFlags_NoBordersInBody
    NoBordersInBodyUntilResize = 4096, // ImGuiTableFlags_NoBordersInBodyUntilResize
    SizingFixedFit = 8192, // ImGuiTableFlags_SizingFixedFit
    SizingFixedSame = 16384, // ImGuiTableFlags_SizingFixedSame
    SizingStretchProp = 24576, // ImGuiTableFlags_SizingStretchProp
    SizingStretchSame = 32768, // ImGuiTableFlags_SizingStretchSame
    NoHostExtendX = 65536, // ImGuiTableFlags_NoHostExtendX
    NoHostExtendY = 131072, // ImGuiTableFlags_NoHostExtendY
    NoKeepColumnsVisible = 262144, // ImGuiTableFlags_NoKeepColumnsVisible
    PreciseWidths = 524288, // ImGuiTableFlags_PreciseWidths
    NoClip = 1048576, // ImGuiTableFlags_NoClip
    PadOuterX = 2097152, // ImGuiTableFlags_PadOuterX
    NoPadOuterX = 4194304, // ImGuiTableFlags_NoPadOuterX
    NoPadInnerX = 8388608, // ImGuiTableFlags_NoPadInnerX
    ScrollX = 16777216, // ImGuiTableFlags_ScrollX
    ScrollY = 33554432, // ImGuiTableFlags_ScrollY
    SortMulti = 67108864, // ImGuiTableFlags_SortMulti
    SortTristate = 134217728, // ImGuiTableFlags_SortTristate
    HighlightHoveredColumn = 268435456, // ImGuiTableFlags_HighlightHoveredColumn
};

// Per-column visibility, sizing, ordering, sorting, and status flags for Dear ImGui tables.
///@ ExportEnum
enum class ImGui_TableColumnFlags : uint32_t
{
    None = 0, // ImGuiTableColumnFlags_None
    Disabled = 1, // ImGuiTableColumnFlags_Disabled
    DefaultHide = 2, // ImGuiTableColumnFlags_DefaultHide
    DefaultSort = 4, // ImGuiTableColumnFlags_DefaultSort
    WidthStretch = 8, // ImGuiTableColumnFlags_WidthStretch
    WidthFixed = 16, // ImGuiTableColumnFlags_WidthFixed
    NoResize = 32, // ImGuiTableColumnFlags_NoResize
    NoReorder = 64, // ImGuiTableColumnFlags_NoReorder
    NoHide = 128, // ImGuiTableColumnFlags_NoHide
    NoClip = 256, // ImGuiTableColumnFlags_NoClip
    NoSort = 512, // ImGuiTableColumnFlags_NoSort
    NoSortAscending = 1024, // ImGuiTableColumnFlags_NoSortAscending
    NoSortDescending = 2048, // ImGuiTableColumnFlags_NoSortDescending
    NoHeaderLabel = 4096, // ImGuiTableColumnFlags_NoHeaderLabel
    NoHeaderWidth = 8192, // ImGuiTableColumnFlags_NoHeaderWidth
    PreferSortAscending = 16384, // ImGuiTableColumnFlags_PreferSortAscending
    PreferSortDescending = 32768, // ImGuiTableColumnFlags_PreferSortDescending
    IndentEnable = 65536, // ImGuiTableColumnFlags_IndentEnable
    IndentDisable = 131072, // ImGuiTableColumnFlags_IndentDisable
    AngledHeader = 262144, // ImGuiTableColumnFlags_AngledHeader
    IsEnabled = 16777216, // ImGuiTableColumnFlags_IsEnabled
    IsVisible = 33554432, // ImGuiTableColumnFlags_IsVisible
    IsSorted = 67108864, // ImGuiTableColumnFlags_IsSorted
    IsHovered = 134217728, // ImGuiTableColumnFlags_IsHovered
};

// Per-row header and background behavior for Dear ImGui tables.
///@ ExportEnum
enum class ImGui_TableRowFlags : uint32_t
{
    None = 0, // ImGuiTableRowFlags_None
    Headers = 1, // ImGuiTableRowFlags_Headers
};

// Table background channel targeted by a Dear ImGui cell or row color assignment.
///@ ExportEnum
enum class ImGui_TableBgTarget : uint32_t
{
    None = 0, // ImGuiTableBgTarget_None
    RowBg0 = 1, // ImGuiTableBgTarget_RowBg0
    RowBg1 = 2, // ImGuiTableBgTarget_RowBg1
    CellBg = 3, // ImGuiTableBgTarget_CellBg
};

// Reordering, fitting, selection, and tooltip behavior for Dear ImGui tab bars.
///@ ExportEnum
enum class ImGui_TabBarFlags : uint32_t
{
    None = 0, // ImGuiTabBarFlags_None
    Reorderable = 1, // ImGuiTabBarFlags_Reorderable
    AutoSelectNewTabs = 2, // ImGuiTabBarFlags_AutoSelectNewTabs
    TabListPopupButton = 4, // ImGuiTabBarFlags_TabListPopupButton
    NoCloseWithMiddleMouseButton = 8, // ImGuiTabBarFlags_NoCloseWithMiddleMouseButton
    NoTabListScrollingButtons = 16, // ImGuiTabBarFlags_NoTabListScrollingButtons
    NoTooltip = 32, // ImGuiTabBarFlags_NoTooltip
    DrawSelectedOverline = 64, // ImGuiTabBarFlags_DrawSelectedOverline
    FittingPolicyMixed = 128, // ImGuiTabBarFlags_FittingPolicyMixed
    FittingPolicyShrink = 256, // ImGuiTabBarFlags_FittingPolicyShrink
    FittingPolicyScroll = 512, // ImGuiTabBarFlags_FittingPolicyScroll
};

// Visibility, closure, ordering, and tooltip behavior for individual Dear ImGui tabs.
///@ ExportEnum
enum class ImGui_TabItemFlags : uint32_t
{
    None = 0, // ImGuiTabItemFlags_None
    UnsavedDocument = 1, // ImGuiTabItemFlags_UnsavedDocument
    SetSelected = 2, // ImGuiTabItemFlags_SetSelected
    NoCloseWithMiddleMouseButton = 4, // ImGuiTabItemFlags_NoCloseWithMiddleMouseButton
    NoPushId = 8, // ImGuiTabItemFlags_NoPushId
    NoTooltip = 16, // ImGuiTabItemFlags_NoTooltip
    NoReorder = 32, // ImGuiTabItemFlags_NoReorder
    Leading = 64, // ImGuiTabItemFlags_Leading
    Trailing = 128, // ImGuiTabItemFlags_Trailing
    NoAssumedClosure = 256, // ImGuiTabItemFlags_NoAssumedClosure
};

// Popup height, alignment, and preview behavior for Dear ImGui combo boxes.
///@ ExportEnum
enum class ImGui_ComboFlags : uint32_t
{
    None = 0, // ImGuiComboFlags_None
    PopupAlignLeft = 1, // ImGuiComboFlags_PopupAlignLeft
    HeightSmall = 2, // ImGuiComboFlags_HeightSmall
    HeightRegular = 4, // ImGuiComboFlags_HeightRegular
    HeightLarge = 8, // ImGuiComboFlags_HeightLarge
    HeightLargest = 16, // ImGuiComboFlags_HeightLargest
    NoArrowButton = 32, // ImGuiComboFlags_NoArrowButton
    NoPreview = 64, // ImGuiComboFlags_NoPreview
    WidthFitPreview = 128, // ImGuiComboFlags_WidthFitPreview
};

// Editing, filtering, submission, callback, and read-only behavior for Dear ImGui text input.
///@ ExportEnum
enum class ImGui_InputTextFlags : uint32_t
{
    None = 0, // ImGuiInputTextFlags_None
    CharsDecimal = 1, // ImGuiInputTextFlags_CharsDecimal
    CharsHexadecimal = 2, // ImGuiInputTextFlags_CharsHexadecimal
    CharsScientific = 4, // ImGuiInputTextFlags_CharsScientific
    CharsUppercase = 8, // ImGuiInputTextFlags_CharsUppercase
    CharsNoBlank = 16, // ImGuiInputTextFlags_CharsNoBlank
    EnterReturnsTrue = 64, // ImGuiInputTextFlags_EnterReturnsTrue
    ReadOnly = 512, // ImGuiInputTextFlags_ReadOnly
    AutoSelectAll = 4096, // ImGuiInputTextFlags_AutoSelectAll
    ParseEmptyRefVal = 8192, // ImGuiInputTextFlags_ParseEmptyRefVal
    DisplayEmptyRefVal = 16384, // ImGuiInputTextFlags_DisplayEmptyRefVal
};

// Mouse-button selection and popup-stack policies for opening or closing Dear ImGui popups.
///@ ExportEnum
enum class ImGui_PopupFlags : uint32_t
{
    None = 0, // ImGuiPopupFlags_None
    MouseButtonLeft = 4, // ImGuiPopupFlags_MouseButtonLeft
    MouseButtonRight = 8, // ImGuiPopupFlags_MouseButtonRight
    MouseButtonMiddle = 12, // ImGuiPopupFlags_MouseButtonMiddle
    NoReopen = 32, // ImGuiPopupFlags_NoReopen
    NoOpenOverExistingPopup = 128, // ImGuiPopupFlags_NoOpenOverExistingPopup
    NoOpenOverItems = 256, // ImGuiPopupFlags_NoOpenOverItems
    AnyPopupId = 1024, // ImGuiPopupFlags_AnyPopupId
    AnyPopupLevel = 2048, // ImGuiPopupFlags_AnyPopupLevel
    AnyPopup = 3072, // ImGuiPopupFlags_AnyPopup
};

// Mouse-button identifiers accepted by the Dear ImGui script bindings.
///@ ExportEnum
enum class ImGui_MouseButton : int32_t
{
    Left = 0, // ImGuiMouseButton_Left
    Right = 1, // ImGuiMouseButton_Right
    Middle = 2, // ImGuiMouseButton_Middle
};

// Cardinal directions and the no-direction sentinel used by Dear ImGui navigation and layout APIs.
///@ ExportEnum
enum class ImGui_Dir : int32_t
{
    None = -1, // ImGuiDir_None
    Left = 0, // ImGuiDir_Left
    Right = 1, // ImGuiDir_Right
    Up = 2, // ImGuiDir_Up
    Down = 3, // ImGuiDir_Down
};

// Clamping and input behavior for Dear ImGui sliders and drag controls.
///@ ExportEnum
enum class ImGui_SliderFlags : uint32_t
{
    None = 0, // ImGuiSliderFlags_None
    Logarithmic = 32, // ImGuiSliderFlags_Logarithmic
    NoRoundToFormat = 64, // ImGuiSliderFlags_NoRoundToFormat
    NoInput = 128, // ImGuiSliderFlags_NoInput
    WrapAround = 256, // ImGuiSliderFlags_WrapAround
    ClampOnInput = 512, // ImGuiSliderFlags_ClampOnInput
    ClampZeroRange = 1024, // ImGuiSliderFlags_ClampZeroRange
    NoSpeedTweaks = 2048, // ImGuiSliderFlags_NoSpeedTweaks
    AlwaysClamp = 1536, // ImGuiSliderFlags_AlwaysClamp
};

// Mouse-button, overlap, and activation behavior for low-level Dear ImGui buttons.
///@ ExportEnum
enum class ImGui_ButtonFlags : uint32_t
{
    None = 0, // ImGuiButtonFlags_None
    MouseButtonLeft = 1, // ImGuiButtonFlags_MouseButtonLeft
    MouseButtonRight = 2, // ImGuiButtonFlags_MouseButtonRight
    MouseButtonMiddle = 4, // ImGuiButtonFlags_MouseButtonMiddle
    EnableNav = 8, // ImGuiButtonFlags_EnableNav
};

// Picker mode, channel visibility, data format, preview, and input behavior for Dear ImGui color editors.
///@ ExportEnum
enum class ImGui_ColorEditFlags : uint32_t
{
    None = 0, // ImGuiColorEditFlags_None
    NoAlpha = 2, // ImGuiColorEditFlags_NoAlpha
    NoPicker = 4, // ImGuiColorEditFlags_NoPicker
    NoOptions = 8, // ImGuiColorEditFlags_NoOptions
    NoSmallPreview = 16, // ImGuiColorEditFlags_NoSmallPreview
    NoInputs = 32, // ImGuiColorEditFlags_NoInputs
    NoTooltip = 64, // ImGuiColorEditFlags_NoTooltip
    NoLabel = 128, // ImGuiColorEditFlags_NoLabel
    NoSidePreview = 256, // ImGuiColorEditFlags_NoSidePreview
    NoDragDrop = 512, // ImGuiColorEditFlags_NoDragDrop
    NoBorder = 1024, // ImGuiColorEditFlags_NoBorder
    AlphaOpaque = 4096, // ImGuiColorEditFlags_AlphaOpaque
    AlphaNoBg = 8192, // ImGuiColorEditFlags_AlphaNoBg
    AlphaPreviewHalf = 16384, // ImGuiColorEditFlags_AlphaPreviewHalf
    AlphaBar = 262144, // ImGuiColorEditFlags_AlphaBar
    HDR = 524288, // ImGuiColorEditFlags_HDR
    DisplayRGB = 1048576, // ImGuiColorEditFlags_DisplayRGB
    DisplayHSV = 2097152, // ImGuiColorEditFlags_DisplayHSV
    DisplayHex = 4194304, // ImGuiColorEditFlags_DisplayHex
    Uint8 = 8388608, // ImGuiColorEditFlags_Uint8
    Float = 16777216, // ImGuiColorEditFlags_Float
    PickerHueBar = 33554432, // ImGuiColorEditFlags_PickerHueBar
    PickerHueWheel = 67108864, // ImGuiColorEditFlags_PickerHueWheel
    InputRGB = 134217728, // ImGuiColorEditFlags_InputRGB
    InputHSV = 268435456, // ImGuiColorEditFlags_InputHSV
    DefaultOptions = 177209344, // ImGuiColorEditFlags_DefaultOptions_
};

// Indexed Dear ImGui style-color slots used by scripted theme customization.
///@ ExportEnum
enum class ImGui_Col : int32_t
{
    Text = 0, // ImGuiCol_Text
    TextDisabled = 1, // ImGuiCol_TextDisabled
    WindowBg = 2, // ImGuiCol_WindowBg
    ChildBg = 3, // ImGuiCol_ChildBg
    PopupBg = 4, // ImGuiCol_PopupBg
    Border = 5, // ImGuiCol_Border
    FrameBg = 7, // ImGuiCol_FrameBg
    FrameBgHovered = 8, // ImGuiCol_FrameBgHovered
    FrameBgActive = 9, // ImGuiCol_FrameBgActive
    TitleBg = 10, // ImGuiCol_TitleBg
    TitleBgActive = 11, // ImGuiCol_TitleBgActive
    MenuBarBg = 13, // ImGuiCol_MenuBarBg
    ScrollbarBg = 14, // ImGuiCol_ScrollbarBg
    ScrollbarGrab = 15, // ImGuiCol_ScrollbarGrab
    CheckMark = 18, // ImGuiCol_CheckMark
    SliderGrab = 20, // ImGuiCol_SliderGrab
    SliderGrabActive = 21, // ImGuiCol_SliderGrabActive
    Button = 22, // ImGuiCol_Button
    ButtonHovered = 23, // ImGuiCol_ButtonHovered
    ButtonActive = 24, // ImGuiCol_ButtonActive
    Header = 25, // ImGuiCol_Header
    HeaderHovered = 26, // ImGuiCol_HeaderHovered
    HeaderActive = 27, // ImGuiCol_HeaderActive
    Separator = 28, // ImGuiCol_Separator
    SeparatorHovered = 29, // ImGuiCol_SeparatorHovered
    SeparatorActive = 30, // ImGuiCol_SeparatorActive
    ResizeGrip = 31, // ImGuiCol_ResizeGrip
    ResizeGripHovered = 32, // ImGuiCol_ResizeGripHovered
    ResizeGripActive = 33, // ImGuiCol_ResizeGripActive
    Tab = 36, // ImGuiCol_Tab
    TabHovered = 35, // ImGuiCol_TabHovered
    TabSelected = 37, // ImGuiCol_TabSelected
    TabDimmed = 39, // ImGuiCol_TabDimmed
    TabDimmedSelected = 40, // ImGuiCol_TabDimmedSelected
    TableHeaderBg = 46, // ImGuiCol_TableHeaderBg
    TableRowBg = 49, // ImGuiCol_TableRowBg
    TableRowBgAlt = 50, // ImGuiCol_TableRowBgAlt
};

// Indexed scalar and vector Dear ImGui style variables accepted by style-stack operations.
///@ ExportEnum
enum class ImGui_StyleVar : int32_t
{
    Alpha = 0, // ImGuiStyleVar_Alpha
    DisabledAlpha = 1, // ImGuiStyleVar_DisabledAlpha
    WindowPadding = 2, // ImGuiStyleVar_WindowPadding
    WindowRounding = 3, // ImGuiStyleVar_WindowRounding
    WindowBorderSize = 4, // ImGuiStyleVar_WindowBorderSize
    FramePadding = 11, // ImGuiStyleVar_FramePadding
    FrameRounding = 12, // ImGuiStyleVar_FrameRounding
    FrameBorderSize = 13, // ImGuiStyleVar_FrameBorderSize
    ItemSpacing = 14, // ImGuiStyleVar_ItemSpacing
    ItemInnerSpacing = 15, // ImGuiStyleVar_ItemInnerSpacing
    IndentSpacing = 16, // ImGuiStyleVar_IndentSpacing
    CellPadding = 17, // ImGuiStyleVar_CellPadding
    ScrollbarSize = 18, // ImGuiStyleVar_ScrollbarSize
    ScrollbarRounding = 19, // ImGuiStyleVar_ScrollbarRounding
    GrabMinSize = 21, // ImGuiStyleVar_GrabMinSize
    GrabRounding = 22, // ImGuiStyleVar_GrabRounding
    TabRounding = 25, // ImGuiStyleVar_TabRounding
    ButtonTextAlign = 36, // ImGuiStyleVar_ButtonTextAlign
    SelectableTextAlign = 37, // ImGuiStyleVar_SelectableTextAlign
};

///@ EnumValueDoc ImGui_WindowFlags None // Applies no optional window behavior flags.
///@ EnumValueDoc ImGui_WindowFlags NoNav // Disables both navigation input within the window and navigation focus toward it.
///@ EnumValueDoc ImGui_WindowFlags NoDecoration // Disables the title bar, resizing, scrollbars, and collapsing controls.
///@ EnumValueDoc ImGui_WindowFlags NoInputs // Disables mouse input, navigation input, and navigation focus for the window.
///@ EnumValueDoc ImGui_ChildFlags None // Applies no optional child-window behavior flags.
///@ EnumValueDoc ImGui_ChildFlags Border // Draws an outer border and enables the child window's standard window padding.
///@ EnumValueDoc ImGui_ChildFlags AlwaysUseWindowPadding // Uses style.WindowPadding even when the child window has no border.
///@ EnumValueDoc ImGui_ChildFlags ResizeX // Allows resizing from the right layout border and persists the width unless window settings are disabled.
///@ EnumValueDoc ImGui_ChildFlags ResizeY // Allows resizing from the bottom layout border and persists the height unless window settings are disabled.
///@ EnumValueDoc ImGui_ChildFlags AutoResizeX // Derives the child-window width from its measured content.
///@ EnumValueDoc ImGui_ChildFlags AutoResizeY // Derives the child-window height from its measured content.
///@ EnumValueDoc ImGui_ChildFlags AlwaysAutoResize // With automatic sizing enabled, measures hidden content, always reports the child as visible, and disables clipping optimization; this expensive mode is not recommended for routine use.
///@ EnumValueDoc ImGui_ChildFlags FrameStyle // Styles the child window as a framed item by using frame colors, rounding, border size, and padding.
///@ EnumValueDoc ImGui_ChildFlags NavFlattened // Beta behavior that shares focus scope and allows keyboard or gamepad navigation across the parent border and sibling child windows.
///@ EnumValueDoc ImGui_SelectableFlags None // Applies no optional selectable-item behavior flags.
///@ EnumValueDoc ImGui_TreeNodeFlags None // Applies no optional tree-node behavior flags.
///@ EnumValueDoc ImGui_TreeNodeFlags CollapsingHeader // Combines framing with no tree-stack push and no automatic opening during logging.
///@ EnumValueDoc ImGui_FocusedFlags None // Tests focus only for the current window without expanding the query scope.
///@ EnumValueDoc ImGui_FocusedFlags RootAndChildWindows // Tests the root window and all of its child windows for focus.
///@ EnumValueDoc ImGui_HoveredFlags AllowWhenOverlapped // Allows a hover result when another item or window overlaps the tested rectangle.
///@ EnumValueDoc ImGui_HoveredFlags RectOnly // Uses the item or window rectangle while ignoring popup, active-item, and overlap blocking.
///@ EnumValueDoc ImGui_TableFlags None // Applies no optional table behavior flags.
///@ EnumValueDoc ImGui_TableColumnFlags None // Applies no optional table-column behavior flags.
///@ EnumValueDoc ImGui_TableRowFlags None // Applies no optional table-row behavior flags.
///@ EnumValueDoc ImGui_TableBgTarget None // Selects no table background color target.
///@ EnumValueDoc ImGui_TabBarFlags None // Applies no optional tab-bar behavior flags.
///@ EnumValueDoc ImGui_TabBarFlags TabListPopupButton // Shows a button that opens the tab-list popup and lets the user select a tab.
///@ EnumValueDoc ImGui_TabItemFlags None // Applies no optional tab-item behavior flags.
///@ EnumValueDoc ImGui_ComboFlags None // Applies no optional combo-box behavior flags.
///@ EnumValueDoc ImGui_InputTextFlags None // Applies no optional text-input behavior flags.
///@ EnumValueDoc ImGui_PopupFlags None // Applies the default popup mouse-button and stack-level behavior.
///@ EnumValueDoc ImGui_PopupFlags AnyPopup // Tests any popup identifier at any level of the popup stack.
///@ EnumValueDoc ImGui_MouseButton Left // Identifies the left mouse button.
///@ EnumValueDoc ImGui_MouseButton Right // Identifies the right mouse button.
///@ EnumValueDoc ImGui_MouseButton Middle // Identifies the middle mouse button.
///@ EnumValueDoc ImGui_Dir None // Indicates that no cardinal direction is selected.
///@ EnumValueDoc ImGui_Dir Left // Selects the left cardinal direction.
///@ EnumValueDoc ImGui_Dir Right // Selects the right cardinal direction.
///@ EnumValueDoc ImGui_Dir Up // Selects the upward cardinal direction.
///@ EnumValueDoc ImGui_Dir Down // Selects the downward cardinal direction.
///@ EnumValueDoc ImGui_SliderFlags None // Applies no optional slider or drag-control behavior flags.
///@ EnumValueDoc ImGui_SliderFlags AlwaysClamp // Clamps manual input and also clamps a zero-width range where minimum and maximum are both zero.
///@ EnumValueDoc ImGui_ButtonFlags None // Applies no optional low-level button behavior flags.
///@ EnumValueDoc ImGui_ColorEditFlags None // Applies no optional color-editor behavior flags.
///@ EnumValueDoc ImGui_ColorEditFlags AlphaOpaque // Hides alpha in the preview while still allowing ColorEdit4 and ColorPicker4 to edit it; for ColorButton this is equivalent to NoAlpha.
///@ EnumValueDoc ImGui_ColorEditFlags DisplayHSV // Displays and edits color components in HSV form.
///@ EnumValueDoc ImGui_ColorEditFlags DisplayHex // Displays and edits the color as a hexadecimal value.
///@ EnumValueDoc ImGui_ColorEditFlags DefaultOptions // Selects 8-bit RGB display and input with the hue-bar picker as the default color-editor options.
///@ EnumValueDoc ImGui_Col Text // Selects the primary text color slot.
///@ EnumValueDoc ImGui_Col TextDisabled // Selects the disabled-text color slot.
///@ EnumValueDoc ImGui_Col Border // Selects the border color slot for windows, child windows, popups, and framed widgets.
///@ EnumValueDoc ImGui_Col FrameBgHovered // Selects the frame background color slot while the frame is hovered.
///@ EnumValueDoc ImGui_Col FrameBgActive // Selects the frame background color slot while the frame is active.
///@ EnumValueDoc ImGui_Col MenuBarBg // Selects the menu-bar background color slot.
///@ EnumValueDoc ImGui_Col ScrollbarBg // Selects the scrollbar-track background color slot.
///@ EnumValueDoc ImGui_Col ScrollbarGrab // Selects the normal scrollbar-grab color slot.
///@ EnumValueDoc ImGui_Col SliderGrab // Selects the normal slider-grab color slot.
///@ EnumValueDoc ImGui_Col SliderGrabActive // Selects the active slider-grab color slot.
///@ EnumValueDoc ImGui_Col Button // Selects the normal button color slot.
///@ EnumValueDoc ImGui_Col ButtonHovered // Selects the hovered button color slot.
///@ EnumValueDoc ImGui_Col ButtonActive // Selects the active button color slot.
///@ EnumValueDoc ImGui_Col HeaderHovered // Selects the hovered header color slot used by headers, tree nodes, and selectables.
///@ EnumValueDoc ImGui_Col HeaderActive // Selects the active header color slot used by headers, tree nodes, and selectables.
///@ EnumValueDoc ImGui_Col Separator // Selects the normal separator color slot.
///@ EnumValueDoc ImGui_Col SeparatorHovered // Selects the hovered separator color slot.
///@ EnumValueDoc ImGui_Col SeparatorActive // Selects the active separator color slot.
///@ EnumValueDoc ImGui_Col ResizeGripHovered // Selects the hovered resize-grip color slot.
///@ EnumValueDoc ImGui_Col ResizeGripActive // Selects the active resize-grip color slot.

inline void ImGuiTextUnformatted(string_view text)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (text.empty()) {
        ImGui::TextUnformatted("");
        return;
    }

    auto text_begin = make_nptr(text.data());
    auto text_end = text_begin.offset(text.size());
    ImGui::TextUnformatted(text_begin.get(), text_end.get());
}

[[nodiscard]] inline auto ToImU32(ucolor color) noexcept -> ImU32
{
    FO_NO_STACK_TRACE_ENTRY();

    return IM_COL32(color.comp.r, color.comp.g, color.comp.b, color.comp.a);
}

FO_END_NAMESPACE
