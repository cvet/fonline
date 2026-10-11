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

// Window creation and interaction flags forwarded to the embedded Dear ImGui runtime
///@ ExportEnum
enum class ImGui_WindowFlags : uint32_t
{
    None = 0, // Applies no optional window behavior flags
    NoTitleBar = 1, // Disable title-bar
    NoResize = 2, // Disable user resizing with the lower-right grip
    NoMove = 4, // Disable user moving the window
    NoScrollbar = 8, // Disable scrollbars (window can still scroll with mouse or programmatically)
    NoScrollWithMouse = 16, // Disable user vertically scrolling with mouse wheel. On child window, mouse wheel will be forwarded to the parent unless NoScrollbar is also set
    NoCollapse = 32, // Disable user collapsing window by double-clicking on it. Also referred to as Window Menu Button (e.g. within a docking node)
    AlwaysAutoResize = 64, // Resize every window to its content every frame
    NoBackground = 128, // Disable drawing background color (WindowBg, etc.) and outside border. Similar as using SetNextWindowBgAlpha(0.0f)
    NoSavedSettings = 256, // Never load/save settings in .ini file
    NoMouseInputs = 512, // Disable catching mouse, hovering test with pass through
    MenuBar = 1024, // Has a menu-bar
    HorizontalScrollbar = 2048, // Allow horizontal scrollbar to appear (off by default). You may use SetNextWindowContentSize(ImVec2(width,0.0f)); prior to calling Begin() to specify width. Read code in imgui_demo in the "Horizontal Scrolling" section
    NoFocusOnAppearing = 4096, // Disable taking focus when transitioning from hidden to visible state
    NoBringToFrontOnFocus = 8192, // Disable bringing window to front when taking focus (e.g. clicking on it or programmatically giving it focus)
    AlwaysVerticalScrollbar = 16384, // Always show vertical scrollbar (even if ContentSize.y < Size.y)
    AlwaysHorizontalScrollbar = 32768, // Always show horizontal scrollbar (even if ContentSize.x < Size.x)
    NoNavInputs = 65536, // No keyboard/gamepad navigation within the window
    NoNavFocus = 131072, // No focusing toward this window with keyboard/gamepad navigation (e.g. skipped by Ctrl+Tab)
    UnsavedDocument = 262144, // Display a dot next to the title. When used in a tab/docking context, tab is selected when clicking the X + closure is not assumed (will wait for user to stop submitting the tab). Otherwise closure is assumed when pressing the X, so if you keep submitting the tab may reappear at end of tab bar
    NoNav = 196608, // Disables both navigation input within the window and navigation focus toward it
    NoDecoration = 43, // Disables the title bar, resizing, scrollbars, and collapsing controls
    NoInputs = 197120, // Disables mouse input, navigation input, and navigation focus for the window
};

// Child-window sizing, framing, padding, and navigation flags forwarded to Dear ImGui
///@ ExportEnum
enum class ImGui_ChildFlags : uint32_t
{
    None = 0, // Applies no optional child-window behavior flags
    Border = 1, // Draws an outer border and enables the child window's standard window padding
    AlwaysUseWindowPadding = 2, // Uses style.WindowPadding even when the child window has no border
    ResizeX = 4, // Allows resizing from the right layout border and persists the width unless window settings are disabled
    ResizeY = 8, // Allows resizing from the bottom layout border and persists the height unless window settings are disabled
    AutoResizeX = 16, // Derives the child-window width from its measured content
    AutoResizeY = 32, // Derives the child-window height from its measured content
    AlwaysAutoResize = 64, // With automatic sizing enabled, measures hidden content, always reports the child as visible, and disables clipping optimization; this expensive mode is not recommended for routine use
    FrameStyle = 128, // Styles the child window as a framed item by using frame colors, rounding, border size, and padding
    NavFlattened = 256, // Beta behavior that shares focus scope and allows keyboard or gamepad navigation across the parent border and sibling child windows
};

// Conditions controlling when a queued Dear ImGui state assignment takes effect
///@ ExportEnum
enum class ImGui_Cond : uint32_t
{
    None = 0, // No condition (always set the variable), same as _Always
    Always = 1, // No condition (always set the variable), same as _None
    Once = 2, // Set the variable once per runtime session (only the first call will succeed)
    FirstUseEver = 4, // Set the variable if the object/window has no persistently saved data (no entry in .ini file)
    Appearing = 8, // Set the variable if the object/window is appearing after being hidden/inactive (or the first time)
};

// Selection, spanning, overlap, and activation behavior for Dear ImGui selectable items
///@ ExportEnum
enum class ImGui_SelectableFlags : uint32_t
{
    None = 0, // Applies no optional selectable-item behavior flags
    NoAutoClosePopups = 1, // Clicking this doesn't close parent popup window (overrides ImGuiItemFlags_AutoClosePopups)
    SpanAllColumns = 2, // Frame will span all columns of its container table (text will still fit in current column)
    AllowDoubleClick = 4, // Generate press events on double clicks too
    Disabled = 8, // Cannot be selected, display grayed out text
    AllowOverlap = 16, // Hit testing will allow subsequent widgets to overlap this one. Require previous frame HoveredId to match before being usable. Shortcut to calling SetNextItemAllowOverlap()
};

// Expansion, framing, selection, spanning, and navigation flags for Dear ImGui tree nodes
///@ ExportEnum
enum class ImGui_TreeNodeFlags : uint32_t
{
    None = 0, // Applies no optional tree-node behavior flags
    Selected = 1, // Draw as selected
    Framed = 2, // Draw frame with background (e.g. for CollapsingHeader)
    AllowOverlap = 4, // Hit testing will allow subsequent widgets to overlap this one. Require previous frame HoveredId to match before being usable. Shortcut to calling SetNextItemAllowOverlap()
    NoTreePushOnOpen = 8, // Don't do a TreePush() when open (e.g. for CollapsingHeader) = no extra indent nor pushing on ID stack
    NoAutoOpenOnLog = 16, // Don't automatically and temporarily open node when Logging is active (by default logging will automatically open tree nodes)
    DefaultOpen = 32, // Default node to be open
    OpenOnDoubleClick = 64, // Open on double-click instead of simple click (default for multi-select unless any _OpenOnXXX behavior is set explicitly). Both behaviors may be combined
    OpenOnArrow = 128, // Open when clicking on the arrow part (default for multi-select unless any _OpenOnXXX behavior is set explicitly). Both behaviors may be combined
    Leaf = 256, // No collapsing, no arrow (use as a convenience for leaf nodes). Note: will always open a tree/id scope and return true. If you never use that scope, add ImGuiTreeNodeFlags_NoTreePushOnOpen
    Bullet = 512, // Display a bullet instead of arrow. IMPORTANT: node can still be marked open/close if you don't set the _Leaf flag!
    FramePadding = 1024, // Use FramePadding (even for an unframed text node) to vertically align text baseline to regular widget height. Equivalent to calling AlignTextToFramePadding() before the node
    SpanAvailWidth = 2048, // Extend hit box to the right-most edge, even if not framed. This is not the default in order to allow adding other items on the same line without using AllowOverlap mode
    SpanFullWidth = 4096, // Extend hit box to the left-most and right-most edges (cover the indent area)
    SpanLabelWidth = 8192, // Narrow hit box + narrow hovering highlight, will only cover the label text
    SpanAllColumns = 16384, // Frame will span all columns of its container table (label will still fit in current column)
    CollapsingHeader = 26, // Combines framing with no tree-stack push and no automatic opening during logging
};

// Scope and hierarchy filters used by Dear ImGui focus queries
///@ ExportEnum
enum class ImGui_FocusedFlags : uint32_t
{
    None = 0, // Tests focus only for the current window without expanding the query scope
    ChildWindows = 1, // Return true if any children of the window is focused
    RootWindow = 2, // Test from root window (top most parent of the current hierarchy)
    AnyWindow = 4, // Return true if any window is focused. Important: If you are trying to tell how to dispatch your low-level inputs, do NOT use this. Use 'io.WantCaptureMouse' instead! Please read the FAQ!
    NoPopupHierarchy = 8, // Do not consider popup hierarchy (do not treat popup emitter as parent of popup) (when used with _ChildWindows or _RootWindow)
    RootAndChildWindows = 3, // Tests the root window and all of its child windows for focus
};

// Blocking, overlap, timing, and hierarchy filters used by Dear ImGui hover queries
///@ ExportEnum
enum class ImGui_HoveredFlags : uint32_t
{
    None = 0, // Return true if directly over the item/window, not obstructed by another window, not obstructed by an active popup or modal blocking inputs under them
    ChildWindows = 1, // IsWindowHovered() only: Return true if any children of the window is hovered
    RootWindow = 2, // IsWindowHovered() only: Test from root window (top most parent of the current hierarchy)
    AnyWindow = 4, // IsWindowHovered() only: Return true if any window is hovered
    NoPopupHierarchy = 8, // IsWindowHovered() only: Do not consider popup hierarchy (do not treat popup emitter as parent of popup) (when used with _ChildWindows or _RootWindow)
    AllowWhenBlockedByPopup = 32, // Return true even if a popup window is normally blocking access to this item/window
    AllowWhenBlockedByActiveItem = 128, // Return true even if an active item is blocking access to this item/window. Useful for Drag and Drop patterns
    AllowWhenOverlappedByItem = 256, // IsItemHovered() only: Return true even if the item uses AllowOverlap mode and is overlapped by another hoverable item
    AllowWhenOverlappedByWindow = 512, // IsItemHovered() only: Return true even if the position is obstructed or overlapped by another window
    AllowWhenDisabled = 1024, // IsItemHovered() only: Return true even if the item is disabled
    NoNavOverride = 2048, // IsItemHovered() only: Disable using keyboard/gamepad navigation state when active, always query mouse
    AllowWhenOverlapped = 768, // Allows a hover result when another item or window overlaps the tested rectangle
    RectOnly = 928, // Uses the item or window rectangle while ignoring popup, active-item, and overlap blocking
    ForTooltip = 4096, // Shortcut for standard flags when using IsItemHovered() + SetTooltip() sequence
};

// Layout, borders, sizing, scrolling, sorting, and clipping behavior for Dear ImGui tables
///@ ExportEnum
enum class ImGui_TableFlags : uint32_t
{
    None = 0, // Applies no optional table behavior flags
    Resizable = 1, // Enable resizing columns
    Reorderable = 2, // Enable reordering columns in header row. (Need calling TableSetupColumn() + TableHeadersRow() to display headers, or using ImGuiTableFlags_ContextMenuInBody to access context-menu without headers)
    Hideable = 4, // Enable hiding/disabling columns in context menu
    Sortable = 8, // Enable sorting. Call TableGetSortSpecs() to obtain sort specs. Also see ImGuiTableFlags_SortMulti and ImGuiTableFlags_SortTristate
    NoSavedSettings = 16, // Disable persisting columns order, width, visibility and sort settings in the .ini file
    ContextMenuInBody = 32, // Right-click on columns body/contents will also display table context menu. By default it is available in TableHeadersRow()
    RowBg = 64, // Set each RowBg color with ImGuiCol_TableRowBg or ImGuiCol_TableRowBgAlt (equivalent of calling TableSetBgColor with ImGuiTableBgFlags_RowBg0 on each row manually)
    BordersInnerH = 128, // Draw horizontal borders between rows
    BordersOuterH = 256, // Draw horizontal borders at the top and bottom
    BordersInnerV = 512, // Draw vertical borders between columns
    BordersOuterV = 1024, // Draw vertical borders on the left and right sides
    BordersH = 384, // Draw horizontal borders
    BordersV = 1536, // Draw vertical borders
    BordersInner = 640, // Draw inner borders
    BordersOuter = 1280, // Draw outer borders
    Borders = 1920, // Draw all borders
    NoBordersInBody = 2048, // [ALPHA] Disable vertical borders in columns Body (borders will always appear in Headers). -> May move to style
    NoBordersInBodyUntilResize = 4096, // [ALPHA] Disable vertical borders in columns Body until hovered for resize (borders will always appear in Headers). -> May move to style
    SizingFixedFit = 8192, // Columns default to _WidthFixed or _WidthAuto (if resizable or not resizable), matching contents width
    SizingFixedSame = 16384, // Columns default to _WidthFixed or _WidthAuto (if resizable or not resizable), matching the maximum contents width of all columns. Implicitly enable ImGuiTableFlags_NoKeepColumnsVisible
    SizingStretchProp = 24576, // Columns default to _WidthStretch with default weights proportional to each columns contents widths
    SizingStretchSame = 32768, // Columns default to _WidthStretch with default weights all equal, unless overridden by TableSetupColumn()
    NoHostExtendX = 65536, // Make outer width auto-fit to columns, overriding outer_size.x value. Only available when ScrollX/ScrollY are disabled and Stretch columns are not used
    NoHostExtendY = 131072, // Make outer height stop exactly at outer_size.y (prevent auto-extending table past the limit). Only available when ScrollX/ScrollY are disabled. Data below the limit will be clipped and not visible
    NoKeepColumnsVisible = 262144, // Disable keeping column always minimally visible when ScrollX is off and table gets too small. Not recommended if columns are resizable
    PreciseWidths = 524288, // Disable distributing remainder width to stretched columns (width allocation on a 100-wide table with 3 columns: Without this flag: 33,33,34. With this flag: 33,33,33). With larger number of columns, resizing will appear to be less smooth
    NoClip = 1048576, // Disable clipping rectangle for every individual columns (reduce draw command count, items will be able to overflow into other columns). Generally incompatible with TableSetupScrollFreeze()
    PadOuterX = 2097152, // Default if BordersOuterV is on. Enable outermost padding. Generally desirable if you have headers
    NoPadOuterX = 4194304, // Default if BordersOuterV is off. Disable outermost padding
    NoPadInnerX = 8388608, // Disable inner padding between columns (double inner padding if BordersOuterV is on, single inner padding if BordersOuterV is off)
    ScrollX = 16777216, // Enable horizontal scrolling. Require 'outer_size' parameter of BeginTable() to specify the container size. Changes default sizing policy. Because this creates a child window, ScrollY is currently generally recommended when using ScrollX
    ScrollY = 33554432, // Enable vertical scrolling. Require 'outer_size' parameter of BeginTable() to specify the container size
    SortMulti = 67108864, // Hold shift when clicking headers to sort on multiple column. TableGetSortSpecs() may return specs where (SpecsCount > 1)
    SortTristate = 134217728, // Allow no sorting, disable default sorting. TableGetSortSpecs() may return specs where (SpecsCount == 0)
    HighlightHoveredColumn = 268435456, // Highlight column headers when hovered (may evolve into a fuller highlight)
};

// Per-column visibility, sizing, ordering, sorting, and status flags for Dear ImGui tables
///@ ExportEnum
enum class ImGui_TableColumnFlags : uint32_t
{
    None = 0, // Applies no optional table-column behavior flags
    Disabled = 1, // Overriding/master disable flag: hide column, won't show in context menu (unlike calling TableSetColumnEnabled() which manipulates the user accessible state)
    DefaultHide = 2, // Default as a hidden/disabled column
    DefaultSort = 4, // Default as a sorting column
    WidthStretch = 8, // Column will stretch. Preferable with horizontal scrolling disabled (default if table sizing policy is _SizingStretchSame or _SizingStretchProp)
    WidthFixed = 16, // Column will not stretch. Preferable with horizontal scrolling enabled (default if table sizing policy is _SizingFixedFit and table is resizable)
    NoResize = 32, // Disable manual resizing
    NoReorder = 64, // Disable manual reordering this column, this will also prevent other columns from crossing over this column
    NoHide = 128, // Disable ability to hide/disable this column
    NoClip = 256, // Disable clipping for this column (all NoClip columns will render in a same draw command)
    NoSort = 512, // Disable ability to sort on this field (even if ImGuiTableFlags_Sortable is set on the table)
    NoSortAscending = 1024, // Disable ability to sort in the ascending direction
    NoSortDescending = 2048, // Disable ability to sort in the descending direction
    NoHeaderLabel = 4096, // TableHeadersRow() will submit an empty label for this column. Convenient for some small columns. Name will still appear in context menu or in angled headers. You may append into this cell by calling TableSetColumnIndex() right after the TableHeadersRow() call
    NoHeaderWidth = 8192, // Disable header text width contribution to automatic column width
    PreferSortAscending = 16384, // Make the initial sort direction Ascending when first sorting on this column (default)
    PreferSortDescending = 32768, // Make the initial sort direction Descending when first sorting on this column
    IndentEnable = 65536, // Use current Indent value when entering cell (default for column 0)
    IndentDisable = 131072, // Ignore current Indent value when entering cell (default for columns > 0). Indentation changes _within_ the cell will still be honored
    AngledHeader = 262144, // TableHeadersRow() will submit an angled header row for this column. Note this will add an extra row
    IsEnabled = 16777216, // Status: is enabled == not hidden by user/api (referred to as "Hide" in _DefaultHide and _NoHide) flags
    IsVisible = 33554432, // Status: is visible == is enabled AND not clipped by scrolling
    IsSorted = 67108864, // Status: is currently part of the sort specs
    IsHovered = 134217728, // Status: is hovered by mouse
};

// Per-row header and background behavior for Dear ImGui tables
///@ ExportEnum
enum class ImGui_TableRowFlags : uint32_t
{
    None = 0, // Applies no optional table-row behavior flags
    Headers = 1, // Identify header row (set default background color + width of its contents accounted differently for auto column width)
};

// Table background channel targeted by a Dear ImGui cell or row color assignment
///@ ExportEnum
enum class ImGui_TableBgTarget : uint32_t
{
    None = 0, // Selects no table background color target
    RowBg0 = 1, // Set row background color 0 (generally used for background, automatically set when ImGuiTableFlags_RowBg is used)
    RowBg1 = 2, // Set row background color 1 (generally used for selection marking)
    CellBg = 3, // Set cell background color (top-most color)
};

// Reordering, fitting, selection, and tooltip behavior for Dear ImGui tab bars
///@ ExportEnum
enum class ImGui_TabBarFlags : uint32_t
{
    None = 0, // Applies no optional tab-bar behavior flags
    Reorderable = 1, // Allow manually dragging tabs to re-order them + New tabs are appended at the end of list
    AutoSelectNewTabs = 2, // Automatically select new tabs when they appear
    TabListPopupButton = 4, // Shows a button that opens the tab-list popup and lets the user select a tab
    NoCloseWithMiddleMouseButton = 8, // Disable behavior of closing tabs (that are submitted with p_open != NULL) with middle mouse button. You may handle this behavior manually on user's side with if (IsItemHovered() && IsMouseClicked(2)) *p_open = false
    NoTabListScrollingButtons = 16, // Disable scrolling buttons (apply when fitting policy is ImGuiTabBarFlags_FittingPolicyScroll)
    NoTooltip = 32, // Disable tooltips when hovering a tab
    DrawSelectedOverline = 64, // Draw selected overline markers over selected tab
    FittingPolicyMixed = 128, // Shrink down tabs when they don't fit, until width is style.TabMinWidthShrink, then enable scrolling. Setting TabMinWidthShrink to FLT_MAX makes this behave like ImGuiTabBarFlags_FittingPolicyScroll
    FittingPolicyShrink = 256, // Shrink down tabs when they don't fit
    FittingPolicyScroll = 512, // Enable scrolling buttons when tabs don't fit
};

// Visibility, closure, ordering, and tooltip behavior for individual Dear ImGui tabs
///@ ExportEnum
enum class ImGui_TabItemFlags : uint32_t
{
    None = 0, // Applies no optional tab-item behavior flags
    UnsavedDocument = 1, // Display a dot next to the title + set ImGuiTabItemFlags_NoAssumedClosure
    SetSelected = 2, // Trigger flag to programmatically make the tab selected when calling BeginTabItem()
    NoCloseWithMiddleMouseButton = 4, // Disable behavior of closing tabs (that are submitted with p_open != NULL) with middle mouse button. You may handle this behavior manually on user's side with if (IsItemHovered() && IsMouseClicked(2)) *p_open = false
    NoPushId = 8, // Don't call PushID()/PopID() on BeginTabItem()/EndTabItem()
    NoTooltip = 16, // Disable tooltip for the given tab
    NoReorder = 32, // Disable reordering this tab or having another tab cross over this tab
    Leading = 64, // Enforce the tab position to the left of the tab bar (after the tab list popup button)
    Trailing = 128, // Enforce the tab position to the right of the tab bar (before the scrolling buttons)
    NoAssumedClosure = 256, // Tab is selected when trying to close + closure is not immediately assumed (will wait for user to stop submitting the tab). Otherwise closure is assumed when pressing the X, so if you keep submitting the tab may reappear at end of tab bar
};

// Popup height, alignment, and preview behavior for Dear ImGui combo boxes
///@ ExportEnum
enum class ImGui_ComboFlags : uint32_t
{
    None = 0, // Applies no optional combo-box behavior flags
    PopupAlignLeft = 1, // Align the popup toward the left by default
    HeightSmall = 2, // Max ~4 items visible. Tip: If you want your combo popup to be a specific size you can use SetNextWindowSizeConstraints() prior to calling BeginCombo()
    HeightRegular = 4, // Max ~8 items visible (default)
    HeightLarge = 8, // Max ~20 items visible
    HeightLargest = 16, // As many fitting items as possible
    NoArrowButton = 32, // Display on the preview box without the square arrow button
    NoPreview = 64, // Display only a square arrow button
    WidthFitPreview = 128, // Width dynamically calculated from preview contents
};

// Editing, filtering, submission, callback, and read-only behavior for Dear ImGui text input
///@ ExportEnum
enum class ImGui_InputTextFlags : uint32_t
{
    None = 0, // Applies no optional text-input behavior flags
    CharsDecimal = 1, // Allow 0123456789.+-*/
    CharsHexadecimal = 2, // Allow 0123456789ABCDEFabcdef
    CharsScientific = 4, // Allow 0123456789.+-*/eE (Scientific notation input)
    CharsUppercase = 8, // Turn a..z into A..Z
    CharsNoBlank = 16, // Filter out spaces, tabs
    EnterReturnsTrue = 64, // Return 'true' when Enter is pressed (as opposed to every time the value was modified). Consider disabling LiveEdit! or using IsItemDeactivatedAfterEdit() instead!
    ReadOnly = 512, // Read-only mode
    Password = 1024, // Password mode, display all characters as '*', disable copy
    AutoSelectAll = 4096, // Select entire text when first taking mouse focus
    ParseEmptyRefVal = 8192, // InputFloat(), InputInt(), InputScalar() etc. only: parse empty string as zero value
    DisplayEmptyRefVal = 16384, // InputFloat(), InputInt(), InputScalar() etc. only: when value is zero, do not display it. Generally used with ImGuiInputTextFlags_ParseEmptyRefVal
};

// Mouse-button selection and popup-stack policies for opening or closing Dear ImGui popups
///@ ExportEnum
enum class ImGui_PopupFlags : uint32_t
{
    None = 0, // Applies the default popup mouse-button and stack-level behavior
    MouseButtonLeft = 4, // For BeginPopupContext*(): open on Left Mouse release. Only one button allowed!
    MouseButtonRight = 8, // For BeginPopupContext*(): open on Right Mouse release. Only one button allowed! (default)
    MouseButtonMiddle = 12, // For BeginPopupContext*(): open on Middle Mouse release. Only one button allowed!
    NoReopen = 32, // For OpenPopup*(), BeginPopupContext*(): don't reopen same popup if already open (won't reposition, won't reinitialize navigation)
    NoOpenOverExistingPopup = 128, // For OpenPopup*(), BeginPopupContext*(): don't open if there's already a popup at the same level of the popup stack
    NoOpenOverItems = 256, // For BeginPopupContextWindow(): don't return true when hovering items, only when hovering empty space
    AnyPopupId = 1024, // For IsPopupOpen(): ignore the ImGuiID parameter and test for any popup
    AnyPopupLevel = 2048, // For IsPopupOpen(): search/test at any level of the popup stack (default test in the current level)
    AnyPopup = 3072, // Tests any popup identifier at any level of the popup stack
};

// Mouse-button identifiers accepted by the Dear ImGui script bindings
///@ ExportEnum
enum class ImGui_MouseButton : int32_t
{
    Left = 0, // Identifies the left mouse button
    Right = 1, // Identifies the right mouse button
    Middle = 2, // Identifies the middle mouse button
};

// Cardinal directions and the no-direction sentinel used by Dear ImGui navigation and layout APIs
///@ ExportEnum
enum class ImGui_Dir : int32_t
{
    None = -1, // Indicates that no cardinal direction is selected
    Left = 0, // Selects the left cardinal direction
    Right = 1, // Selects the right cardinal direction
    Up = 2, // Selects the upward cardinal direction
    Down = 3, // Selects the downward cardinal direction
};

// Clamping and input behavior for Dear ImGui sliders and drag controls
///@ ExportEnum
enum class ImGui_SliderFlags : uint32_t
{
    None = 0, // Applies no optional slider or drag-control behavior flags
    Logarithmic = 32, // Make the widget logarithmic (linear otherwise). Consider using ImGuiSliderFlags_NoRoundToFormat with this if using a format-string with small amount of digits
    NoRoundToFormat = 64, // Disable rounding underlying value to match precision of the display format string (e.g. %.3f values are rounded to those 3 digits)
    NoInput = 128, // Disable Ctrl+Click or Enter key allowing to input text directly into the widget
    WrapAround = 256, // Enable wrapping around from max to min and from min to max. Only supported by DragXXX() functions for now
    ClampOnInput = 512, // Clamp value to min/max bounds when input manually with Ctrl+Click. By default Ctrl+Click allows going out of bounds
    ClampZeroRange = 1024, // Clamp even if min==max==0.0f. Otherwise due to legacy reason DragXXX functions don't clamp with those values. When your clamping limits are dynamic you almost always want to use it
    NoSpeedTweaks = 2048, // Disable keyboard modifiers altering tweak speed. Useful if you want to alter tweak speed yourself based on your own logic
    AlwaysClamp = 1536, // Clamps manual input and also clamps a zero-width range where minimum and maximum are both zero
};

// Mouse-button, overlap, and activation behavior for low-level Dear ImGui buttons
///@ ExportEnum
enum class ImGui_ButtonFlags : uint32_t
{
    None = 0, // Applies no optional low-level button behavior flags
    MouseButtonLeft = 1, // React on left mouse button (default)
    MouseButtonRight = 2, // React on right mouse button
    MouseButtonMiddle = 4, // React on center mouse button
    EnableNav = 8, // InvisibleButton(): do not disable navigation/tabbing. Otherwise disabled by default
};

// Picker mode, channel visibility, data format, preview, and input behavior for Dear ImGui color editors
///@ ExportEnum
enum class ImGui_ColorEditFlags : uint32_t
{
    None = 0, // Applies no optional color-editor behavior flags
    NoAlpha = 2, // ColorEdit, ColorPicker, ColorButton: ignore Alpha component (will only read 3 components from the input pointer)
    NoPicker = 4, // ColorEdit: disable picker when clicking on color square
    NoOptions = 8, // ColorEdit: disable toggling options menu when right-clicking on inputs/small preview
    NoSmallPreview = 16, // ColorEdit, ColorPicker: disable color square preview next to the inputs. (e.g. to show only the inputs)
    NoInputs = 32, // ColorEdit, ColorPicker: disable inputs sliders/text widgets (e.g. to show only the small preview color square)
    NoTooltip = 64, // ColorEdit, ColorPicker, ColorButton: disable tooltip when hovering the preview
    NoLabel = 128, // ColorEdit, ColorPicker: disable display of inline text label (the label is still forwarded to the tooltip and picker)
    NoSidePreview = 256, // ColorPicker: disable bigger color preview on right side of the picker, use small color square preview instead
    NoDragDrop = 512, // ColorEdit: disable drag and drop target/source. ColorButton: disable drag and drop source
    NoBorder = 1024, // ColorButton: disable border (which is enforced by default)
    AlphaOpaque = 4096, // Hides alpha in the preview while still allowing ColorEdit4 and ColorPicker4 to edit it; for ColorButton this is equivalent to NoAlpha
    AlphaNoBg = 8192, // ColorEdit, ColorPicker, ColorButton: disable rendering a checkerboard background behind transparent color
    AlphaPreviewHalf = 16384, // ColorEdit, ColorPicker, ColorButton: display half opaque / half transparent preview
    AlphaBar = 262144, // ColorEdit, ColorPicker: show vertical alpha bar/gradient in picker
    HDR = 524288, // (WIP) ColorEdit: Currently only disable 0.0f..1.0f limits in RGBA edition (note: you probably want to use ImGuiColorEditFlags_Float flag as well)
    DisplayRGB = 1048576, // ColorEdit: override _display_ type among RGB/HSV/Hex. ColorPicker: select any combination using one or more of RGB/HSV/Hex
    DisplayHSV = 2097152, // Displays and edits color components in HSV form
    DisplayHex = 4194304, // Displays and edits the color as a hexadecimal value
    Uint8 = 8388608, // ColorEdit, ColorPicker, ColorButton: _display_ values formatted as 0..255
    Float = 16777216, // ColorEdit, ColorPicker, ColorButton: _display_ values formatted as 0.0f..1.0f floats instead of 0..255 integers. No round-trip of value via integers
    PickerHueBar = 33554432, // ColorPicker: bar for Hue, rectangle for Sat/Value
    PickerHueWheel = 67108864, // ColorPicker: wheel for Hue, triangle for Sat/Value
    InputRGB = 268435456, // ColorEdit, ColorPicker: input and output data in RGB format
    InputHSV = 536870912, // ColorEdit, ColorPicker: input and output data in HSV format
    DefaultOptions = 311427072, // Selects 8-bit RGB display and input with the hue-bar picker as the default color-editor options
};

// Indexed Dear ImGui style-color slots used by scripted theme customization
///@ ExportEnum
enum class ImGui_Col : int32_t
{
    Text = 0, // Selects the primary text color slot
    TextDisabled = 1, // Selects the disabled-text color slot
    WindowBg = 2, // Background of normal windows
    ChildBg = 3, // Background of child windows
    PopupBg = 4, // Background of popups, menus, tooltips windows
    Border = 5, // Selects the border color slot for windows, child windows, popups, and framed widgets
    FrameBg = 7, // Background of checkbox, radio button, plot, slider, text input
    FrameBgHovered = 8, // Selects the frame background color slot while the frame is hovered
    FrameBgActive = 9, // Selects the frame background color slot while the frame is active
    TitleBg = 10, // Title bar
    TitleBgActive = 11, // Title bar when focused
    MenuBarBg = 13, // Selects the menu-bar background color slot
    ScrollbarBg = 14, // Selects the scrollbar-track background color slot
    ScrollbarGrab = 15, // Selects the normal scrollbar-grab color slot
    CheckMark = 18, // Checkbox tick and RadioButton circle
    SliderGrab = 20, // Selects the normal slider-grab color slot
    SliderGrabActive = 21, // Selects the active slider-grab color slot
    Button = 22, // Selects the normal button color slot
    ButtonHovered = 23, // Selects the hovered button color slot
    ButtonActive = 24, // Selects the active button color slot
    Header = 25, // Header* colors are used for CollapsingHeader, TreeNode, Selectable, MenuItem
    HeaderHovered = 26, // Selects the hovered header color slot used by headers, tree nodes, and selectables
    HeaderActive = 27, // Selects the active header color slot used by headers, tree nodes, and selectables
    Separator = 28, // Selects the normal separator color slot
    SeparatorHovered = 29, // Selects the hovered separator color slot
    SeparatorActive = 30, // Selects the active separator color slot
    ResizeGrip = 31, // Resize grip in lower-right and lower-left corners of windows
    ResizeGripHovered = 32, // Selects the hovered resize-grip color slot
    ResizeGripActive = 33, // Selects the active resize-grip color slot
    Tab = 36, // Tab background, when tab-bar is focused & tab is unselected
    TabHovered = 35, // Tab background, when hovered
    TabSelected = 37, // Tab background, when tab-bar is focused & tab is selected
    TabDimmed = 39, // Tab background, when tab-bar is unfocused & tab is unselected
    TabDimmedSelected = 40, // Tab background, when tab-bar is unfocused & tab is selected
    TableHeaderBg = 46, // Table header background
    TableRowBg = 49, // Table row background (even rows)
    TableRowBgAlt = 50, // Table row background (odd rows)
};

// Indexed scalar and vector Dear ImGui style variables accepted by style-stack operations
///@ ExportEnum
enum class ImGui_StyleVar : int32_t
{
    Alpha = 0, // Global alpha applies to everything in Dear ImGui
    DisabledAlpha = 1, // Additional alpha multiplier applied by BeginDisabled(). Multiply over current value of Alpha
    WindowPadding = 2, // Padding within a window
    WindowRounding = 3, // Radius of window corners rounding. Set to 0.0f to have rectangular windows. Large values tend to lead to variety of artifacts and are not recommended
    WindowBorderSize = 4, // Thickness of border around windows. Generally set to 0.0f or 1.0f. (Other values are not well tested and more CPU/GPU costly)
    FramePadding = 11, // Padding within a framed rectangle (used by most widgets)
    FrameRounding = 12, // Radius of frame corners rounding. Set to 0.0f to have rectangular frame (used by most widgets)
    FrameBorderSize = 13, // Thickness of border around frames. Generally set to 0.0f or 1.0f. (Other values are not well tested and more CPU/GPU costly)
    ItemSpacing = 14, // Horizontal and vertical spacing between widgets/lines
    ItemInnerSpacing = 15, // Horizontal and vertical spacing between within elements of a composed widget (e.g. a slider and its label)
    IndentSpacing = 16, // Horizontal indentation when e.g. entering a tree node. Generally == (FontSize + FramePadding.x*2)
    CellPadding = 17, // Padding within a table cell. Cellpadding.x is locked for entire table. CellPadding.y may be altered between different rows
    ScrollbarSize = 18, // Width of the vertical scrollbar, Height of the horizontal scrollbar
    ScrollbarRounding = 19, // Radius of grab corners for scrollbar
    GrabMinSize = 21, // Minimum width/height of a grab box for slider/scrollbar
    GrabRounding = 22, // Radius of grabs corners rounding. Set to 0.0f to have rectangular slider grabs
    TabRounding = 25, // Radius of upper corners of a tab. Set to 0.0f to have rectangular tabs
    ButtonTextAlign = 38, // Alignment of button text when button is larger than text. Defaults to (0.5f, 0.5f) (centered)
    SelectableTextAlign = 39, // Alignment of selectable text. Defaults to (0.0f, 0.0f) (top-left aligned). It's generally important to keep this left-aligned if you want to lay multiple items on a same line
};

inline void ImGuiTextUnformatted(string_view text)
{
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
    return IM_COL32(color.comp.r, color.comp.g, color.comp.b, color.comp.a);
}

FO_END_NAMESPACE
