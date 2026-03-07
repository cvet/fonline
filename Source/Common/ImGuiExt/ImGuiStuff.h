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
}

FO_BEGIN_NAMESPACE

class BaseEngine;

///@ ExportEntity ImGui ScriptImGui ScriptImGui Global
class ScriptImGui : public Entity
{
public:
    explicit ScriptImGui(BaseEngine* engine);
    ScriptImGui(const ScriptImGui&) = delete;
    ScriptImGui(ScriptImGui&&) noexcept = delete;
    auto operator=(const ScriptImGui&) = delete;
    auto operator=(ScriptImGui&&) noexcept = delete;
    ~ScriptImGui() override = default;

    [[nodiscard]] auto GetName() const noexcept -> string_view override { return "ImGui"; }
    [[nodiscard]] auto IsGlobal() const noexcept -> bool override { return true; }
    [[nodiscard]] auto GetEngine() noexcept -> BaseEngine* { return _engine.get(); }

private:
    raw_ptr<BaseEngine> _engine;
};

///@ ExportEnum
enum class ImGui_WindowFlags : uint32
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

///@ ExportEnum
enum class ImGui_ChildFlags : uint32
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

///@ ExportEnum
enum class ImGui_Cond : uint32
{
    None = 0, // ImGuiCond_None
    Always = 1, // ImGuiCond_Always
    Once = 2, // ImGuiCond_Once
    FirstUseEver = 4, // ImGuiCond_FirstUseEver
    Appearing = 8, // ImGuiCond_Appearing
};

///@ ExportEnum
enum class ImGui_SelectableFlags : uint32
{
    None = 0, // ImGuiSelectableFlags_None
    NoAutoClosePopups = 1, // ImGuiSelectableFlags_NoAutoClosePopups
    SpanAllColumns = 2, // ImGuiSelectableFlags_SpanAllColumns
    AllowDoubleClick = 4, // ImGuiSelectableFlags_AllowDoubleClick
    Disabled = 8, // ImGuiSelectableFlags_Disabled
    AllowOverlap = 16, // ImGuiSelectableFlags_AllowOverlap
};

///@ ExportEnum
enum class ImGui_TreeNodeFlags : uint32
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

///@ ExportEnum
enum class ImGui_FocusedFlags : uint32
{
    None = 0, // ImGuiFocusedFlags_None
    ChildWindows = 1, // ImGuiFocusedFlags_ChildWindows
    RootWindow = 2, // ImGuiFocusedFlags_RootWindow
    AnyWindow = 4, // ImGuiFocusedFlags_AnyWindow
    NoPopupHierarchy = 8, // ImGuiFocusedFlags_NoPopupHierarchy
    RootAndChildWindows = 3, // ImGuiFocusedFlags_RootAndChildWindows
};

///@ ExportEnum
enum class ImGui_HoveredFlags : uint32
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

///@ ExportEnum
enum class ImGui_TableFlags : uint32
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

///@ ExportEnum
enum class ImGui_TableColumnFlags : uint32
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

///@ ExportEnum
enum class ImGui_TableRowFlags : uint32
{
    None = 0, // ImGuiTableRowFlags_None
    Headers = 1, // ImGuiTableRowFlags_Headers
};

///@ ExportEnum
enum class ImGui_TableBgTarget : uint32
{
    None = 0, // ImGuiTableBgTarget_None
    RowBg0 = 1, // ImGuiTableBgTarget_RowBg0
    RowBg1 = 2, // ImGuiTableBgTarget_RowBg1
    CellBg = 3, // ImGuiTableBgTarget_CellBg
};

///@ ExportEnum
enum class ImGui_TabBarFlags : uint32
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

///@ ExportEnum
enum class ImGui_TabItemFlags : uint32
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

///@ ExportEnum
enum class ImGui_ComboFlags : uint32
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

///@ ExportEnum
enum class ImGui_InputTextFlags : uint32
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

///@ ExportEnum
enum class ImGui_PopupFlags : uint32
{
    None = 0, // ImGuiPopupFlags_None
    MouseButtonRight = 1, // ImGuiPopupFlags_MouseButtonRight
    MouseButtonMiddle = 2, // ImGuiPopupFlags_MouseButtonMiddle
    NoReopen = 32, // ImGuiPopupFlags_NoReopen
    NoOpenOverExistingPopup = 128, // ImGuiPopupFlags_NoOpenOverExistingPopup
    NoOpenOverItems = 256, // ImGuiPopupFlags_NoOpenOverItems
    AnyPopupId = 1024, // ImGuiPopupFlags_AnyPopupId
    AnyPopupLevel = 2048, // ImGuiPopupFlags_AnyPopupLevel
    AnyPopup = 3072, // ImGuiPopupFlags_AnyPopup
};

///@ ExportEnum
enum class ImGui_MouseButton : int32
{
    Left = 0, // ImGuiMouseButton_Left
    Right = 1, // ImGuiMouseButton_Right
    Middle = 2, // ImGuiMouseButton_Middle
};

///@ ExportEnum
enum class ImGui_Dir : int32
{
    None = -1, // ImGuiDir_None
    Left = 0, // ImGuiDir_Left
    Right = 1, // ImGuiDir_Right
    Up = 2, // ImGuiDir_Up
    Down = 3, // ImGuiDir_Down
};

///@ ExportEnum
enum class ImGui_SliderFlags : uint32
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

///@ ExportEnum
enum class ImGui_ButtonFlags : uint32
{
    None = 0, // ImGuiButtonFlags_None
    MouseButtonLeft = 1, // ImGuiButtonFlags_MouseButtonLeft
    MouseButtonRight = 2, // ImGuiButtonFlags_MouseButtonRight
    MouseButtonMiddle = 4, // ImGuiButtonFlags_MouseButtonMiddle
    EnableNav = 8, // ImGuiButtonFlags_EnableNav
};

///@ ExportEnum
enum class ImGui_ColorEditFlags : uint32
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
    AlphaOpaque = 2048, // ImGuiColorEditFlags_AlphaOpaque
    AlphaNoBg = 4096, // ImGuiColorEditFlags_AlphaNoBg
    AlphaPreviewHalf = 8192, // ImGuiColorEditFlags_AlphaPreviewHalf
    AlphaBar = 65536, // ImGuiColorEditFlags_AlphaBar
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

///@ ExportEnum
enum class ImGui_Col : int32
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
    SliderGrab = 19, // ImGuiCol_SliderGrab
    SliderGrabActive = 20, // ImGuiCol_SliderGrabActive
    Button = 21, // ImGuiCol_Button
    ButtonHovered = 22, // ImGuiCol_ButtonHovered
    ButtonActive = 23, // ImGuiCol_ButtonActive
    Header = 24, // ImGuiCol_Header
    HeaderHovered = 25, // ImGuiCol_HeaderHovered
    HeaderActive = 26, // ImGuiCol_HeaderActive
    Separator = 27, // ImGuiCol_Separator
    SeparatorHovered = 28, // ImGuiCol_SeparatorHovered
    SeparatorActive = 29, // ImGuiCol_SeparatorActive
    ResizeGrip = 30, // ImGuiCol_ResizeGrip
    ResizeGripHovered = 31, // ImGuiCol_ResizeGripHovered
    ResizeGripActive = 32, // ImGuiCol_ResizeGripActive
    Tab = 35, // ImGuiCol_Tab
    TabHovered = 34, // ImGuiCol_TabHovered
    TabSelected = 36, // ImGuiCol_TabSelected
    TabDimmed = 38, // ImGuiCol_TabDimmed
    TabDimmedSelected = 39, // ImGuiCol_TabDimmedSelected
    TableHeaderBg = 45, // ImGuiCol_TableHeaderBg
    TableRowBg = 48, // ImGuiCol_TableRowBg
    TableRowBgAlt = 49, // ImGuiCol_TableRowBgAlt
};

///@ ExportEnum
enum class ImGui_StyleVar : int32
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
    TabRounding = 24, // ImGuiStyleVar_TabRounding
    ButtonTextAlign = 34, // ImGuiStyleVar_ButtonTextAlign
    SelectableTextAlign = 35, // ImGuiStyleVar_SelectableTextAlign
};

FO_END_NAMESPACE
