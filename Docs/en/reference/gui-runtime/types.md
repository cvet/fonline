---
title: GUI Runtime Types
document_id: generated-gui-runtime-types
locale: en
generated: true
---

# GUI Runtime Types

> Generated reference. Do not edit directly. Update `BuildTools/GuiRuntimeInterface.json`, then run `python BuildTools/docs_gui_runtime.py --write`.

[Index](index.md) | [Types](types.md) | [Screen API](screen-api.md) | [Lifecycle](lifecycle.md) | [Layout](layout-rendering.md) | [Input](input.md) | [Integration](integration-validation.md) | [Canonical JSON](../../../generated/gui-runtime.json) | [Guide](../../how-to/runtime/gui.md)

| Stable ID | Type | Base | Members | Callbacks | Role |
| --- | --- | --- | --- | --- | --- |
| <a id="entry-gui-runtime-type-object-f26b45999e"></a><code>gui-runtime.type.object</code> | <code>Object</code> | - | 56 | 36 | Base tree node for activity, geometry, hierarchy, hit testing, focus, dragging, callbacks, lookup, and recursive drawing. |
| <a id="entry-gui-runtime-type-panel-ba52e2409f"></a><code>gui-runtime.type.panel</code> | <code>Panel</code> | <code>Object</code> | 16 | 0 | Object with a sprite background, optional 9-slice frame, crop/scissor, and animated vertical or horizontal scrolling. |
| <a id="entry-gui-runtime-type-text-b7ebcc2a6b"></a><code>gui-runtime.type.text</code> | <code>Text</code> | <code>Object</code> | 12 | 0 | Drawable text node using a bound FontType, FontFlag set, colors, skip-lines state, and optional height measurement. |
| <a id="entry-gui-runtime-type-text-input-e905911387"></a><code>gui-runtime.type.text-input</code> | <code>TextInput</code> | <code>Text</code> | 6 | 0 | Focused editable text node with length, password, carriage, clipboard, and on-screen-keyboard behavior. |
| <a id="entry-gui-runtime-type-button-a91e38fc0a"></a><code>gui-runtime.type.button</code> | <code>Button</code> | <code>Panel</code> | 14 | 0 | Panel with pressed, hover, disabled, switched, and condition state. |
| <a id="entry-gui-runtime-type-check-box-edcfced6be"></a><code>gui-runtime.type.check-box</code> | <code>CheckBox</code> | <code>Button</code> | 2 | 1 | Button with checked state and an OnCheckedChanged callback. |
| <a id="entry-gui-runtime-type-radio-button-4c0157ef47"></a><code>gui-runtime.type.radio-button</code> | <code>RadioButton</code> | <code>CheckBox</code> | 0 | 0 | CheckBox that clears switched sibling radio buttons under the same parent. |
| <a id="entry-gui-runtime-type-screen-fa6503ba94"></a><code>gui-runtime.type.screen</code> | <code>Screen</code> | <code>Panel</code> | 14 | 0 | Top-level panel with registration id, modal/multiinstance/close-on-miss behavior, cursor set, movement policy, and top-of-stack state. |
| <a id="entry-gui-runtime-type-grid-8070fde4e8"></a><code>gui-runtime.type.grid</code> | <code>Grid</code> | <code>Panel</code> | 11 | 0 | Panel that clones a named prototype node into indexed cells and lays them out by columns and padding. |
| <a id="entry-gui-runtime-type-message-box-e1b7304252"></a><code>gui-runtime.type.message-box</code> | <code>MessageBox</code> | <code>Text</code> | 10 | 0 | Text-derived timestamped message feed with type filtering, inversion, scrolling, and a project-configurable default inversion setting. |
| <a id="entry-gui-runtime-type-console-779de0e59e"></a><code>gui-runtime.type.console</code> | <code>Console</code> | <code>TextInput</code> | 9 | 0 | TextInput-derived command console with activation, history persistence, and submit behavior. |
| <a id="entry-gui-runtime-type-item-view-c7d1736089"></a><code>gui-runtime.type.item-view</code> | <code>ItemView</code> | <code>Grid</code> | 9 | 2 | Grid that obtains client Item handles, filters or sorts them, binds live handles to cells, and invokes item drawing callbacks. |

## Object

Base: none
Source: [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos)

Base tree node for activity, geometry, hierarchy, hit testing, focus, dragging, callbacks, lookup, and recursive drawing.

### Members

- <code>bool Active</code>
- <code>bool ActiveSelf</code>
- <code>string Name</code>
- <code>ipos Pos</code>
- <code>ipos AbsolutePos</code>
- <code>isize Size</code>
- <code>AnchorStyle Anchor</code>
- <code>DockStyle Dock</code>
- <code>bool IsDraggable</code>
- <code>bool IsNotHittable</code>
- <code>bool IsNotCatchable</code>
- <code>bool CheckTransparentOnHit</code>
- <code>bool FocusGroup</code>
- <code>bool IsFocused</code>
- <code>bool IsPressed</code>
- <code>bool IsHovered</code>
- <code>bool IsDragged</code>
- <code>int ChildCount</code>
- <code>Object Parent</code>
- <code>Screen Screen</code>
- <code>Grid Grid</code>
- <code>int CellIndex</code>
- <code>void Init(Object parent)</code>
- <code>void Remove()</code>
- <code>void SetActive(bool active)</code>
- <code>void SetPosition(ipos pos)</code>
- <code>void SetPosition(int x, int y)</code>
- <code>void SetPosition(string iniKey)</code>
- <code>void SetSize(isize size)</code>
- <code>void SetSize(int width, int height)</code>
- <code>void SetAnchor(AnchorStyle anchorStyles)</code>
- <code>void SetDock(DockStyle dockStyle)</code>
- <code>void SetColor(ucolor color)</code>
- <code>void SetDraggable(bool enabled)</code>
- <code>void SetNotHittable(bool enabled)</code>
- <code>void SetNotCatchable(bool enabled)</code>
- <code>void SetCheckTransparentOnHit(bool enabled)</code>
- <code>void SetFocusGroup(bool enabled)</code>
- <code>void SetHasOnDraw(bool enabled)</code>
- <code>Object? FindMouseHit()</code>
- <code>Object? FindHit(ipos pos)</code>
- <code>bool IsMouseHit()</code>
- <code>bool IsHit(ipos pos)</code>
- <code>void GetWholeSize(ipos&amp; centerPos, isize&amp; wholeSize, bool onlyChidren = false)</code>
- <code>void Draw(ipos pos)</code>
- <code>void Move(ipos deltaPos)</code>
- <code>void StartDragging()</code>
- <code>void MouseClick(MouseButton button)</code>
- <code>void Input(KeyCode key, string text)</code>
- <code>Panel FindPanel(string name)</code>
- <code>Text FindText(string name)</code>
- <code>TextInput FindTextInput(string name)</code>
- <code>Button FindButton(string name)</code>
- <code>Object Find(string name, bool deepFind = true, int skip = 0)</code>
- <code>int Count(string name, bool deepCount = true)</code>
- <code>Object GetChild(int index)</code>

### Callbacks

- <code>void OnConstruct()</code>
- <code>void OnInit()</code>
- <code>void OnShow()</code>
- <code>void OnShow(dict&lt;string, any&gt; params)</code>
- <code>void OnHide()</code>
- <code>void OnAppear()</code>
- <code>void OnDisappear()</code>
- <code>void OnDraw()</code>
- <code>void OnPostDraw()</code>
- <code>void OnMove(ipos deltaPos)</code>
- <code>void OnMouseDown(MouseButton button)</code>
- <code>void OnMouseUp(MouseButton button, bool lost)</code>
- <code>void OnMousePressed(MouseButton button)</code>
- <code>void OnLMousePressed()</code>
- <code>void OnRMousePressed()</code>
- <code>void OnMouseClick(MouseButton button)</code>
- <code>void OnLMouseClick()</code>
- <code>void OnRMouseClick()</code>
- <code>void OnMouseMove()</code>
- <code>void OnGlobalMouseDown(MouseButton button)</code>
- <code>void OnGlobalMouseUp(MouseButton button)</code>
- <code>void OnGlobalMousePressed(MouseButton button)</code>
- <code>void OnGlobalMouseClick(MouseButton button)</code>
- <code>void OnGlobalMouseMove()</code>
- <code>void OnInput()</code>
- <code>void OnInput(KeyCode key)</code>
- <code>void OnInput(string text)</code>
- <code>void OnInput(KeyCode key, string text)</code>
- <code>void OnGlobalInput(KeyCode key, string text)</code>
- <code>void OnActiveChanged()</code>
- <code>void OnFocusChanged()</code>
- <code>void OnHoverChanged()</code>
- <code>void OnDragChanged()</code>
- <code>void OnResizeGrid(Object cell, int cellIndex)</code>
- <code>void OnDrawItem(Item item, Object cell, int cellIndex)</code>
- <code>void OnRefreshText()</code>

## Panel

Base: <code>Object</code>
Source: [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos)

Object with a sprite background, optional 9-slice frame, crop/scissor, and animated vertical or horizontal scrolling.

### Members

- <code>Sprite::Sprite BackgroundImage</code>
- <code>SpriteLayout BackgroundImageLayout</code>
- <code>void SetBackgroundImage(string imageName, SpriteLayout imageLayout = SpriteLayout::None)</code>
- <code>void SetBackgroundImage(hstring imageName, SpriteLayout imageLayout = SpriteLayout::None)</code>
- <code>void SetFrameImage(string imageName)</code>
- <code>void SetFrameImage(hstring imageName)</code>
- <code>void SetCapInsets(int left, int top, int right, int bottom)</code>
- <code>void SetCropContent(bool enabled)</code>
- <code>void SetAutoScroll(bool ver, bool hor)</code>
- <code>int VerticalScrollValue</code>
- <code>int HorizontalScrollValue</code>
- <code>int VerticalScrollRange</code>
- <code>int HorizontalScrollRange</code>
- <code>void ModifyScroll(int ver, int hor)</code>
- <code>bool CanModifyScroll(int ver, int hor)</code>
- <code>void SetScrollValue(int ver, int hor)</code>

### Callbacks

- None declared by this type.

## Text

Base: <code>Object</code>
Source: [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos)

Drawable text node using a bound FontType, FontFlag set, colors, skip-lines state, and optional height measurement.

### Members

- <code>string Text (overridable)</code>
- <code>int TextFont</code>
- <code>ucolor TextColor</code>
- <code>ucolor TextColorFocused</code>
- <code>int TextFlags</code>
- <code>void SetText(string text, int font, int flags)</code>
- <code>void SetText(string text)</code>
- <code>void SetTextWithResize(string text)</code>
- <code>void SetTextFont(int font)</code>
- <code>void SetTextFlags(int flags)</code>
- <code>void SetTextColor(ucolor color)</code>
- <code>void SetTextFocusedColor(ucolor color)</code>

### Callbacks

- None declared by this type.

## TextInput

Base: <code>Text</code>
Source: [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos)

Focused editable text node with length, password, carriage, clipboard, and on-screen-keyboard behavior.

### Members

- <code>int InputLength</code>
- <code>bool IsTextPassword</code>
- <code>string PasswordChar</code>
- <code>void SetInputLength(int length)</code>
- <code>void SetInputPassword(string passwordChar)</code>
- <code>void SetCarriage(bool enable)</code>

### Callbacks

- None declared by this type.

## Button

Base: <code>Panel</code>
Source: [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos)

Panel with pressed, hover, disabled, switched, and condition state.

### Members

- <code>Sprite::Sprite PressedImage</code>
- <code>SpriteLayout PressedImageLayout</code>
- <code>Sprite::Sprite HoverImage</code>
- <code>SpriteLayout HoverImageLayout</code>
- <code>bool IsSwitched</code>
- <code>bool IsDisabled</code>
- <code>void SetPressedImage(string imageName, SpriteLayout imageLayout = SpriteLayout::None)</code>
- <code>void SetPressedImage(hstring imageName, SpriteLayout imageLayout = SpriteLayout::None)</code>
- <code>void SetHoverImage(string imageName, SpriteLayout imageLayout = SpriteLayout::None)</code>
- <code>void SetHoverImage(hstring imageName, SpriteLayout imageLayout = SpriteLayout::None)</code>
- <code>void SetDisabledImage(string imageName, SpriteLayout imageLayout = SpriteLayout::None)</code>
- <code>void SetDisabledImage(hstring imageName, SpriteLayout imageLayout = SpriteLayout::None)</code>
- <code>void SetSwitch(bool enabled)</code>
- <code>void SetCondition(bool enabled)</code>

### Callbacks

- None declared by this type.

## CheckBox

Base: <code>Button</code>
Source: [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos)

Button with checked state and an OnCheckedChanged callback.

### Members

- <code>bool IsChecked</code>
- <code>void SetChecked(bool checked)</code>

### Callbacks

- <code>void OnCheckedChanged()</code>

## RadioButton

Base: <code>CheckBox</code>
Source: [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos)

CheckBox that clears switched sibling radio buttons under the same parent.

### Members


### Callbacks

- None declared by this type.

## Screen

Base: <code>Panel</code>
Source: [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos)

Top-level panel with registration id, modal/multiinstance/close-on-miss behavior, cursor set, movement policy, and top-of-stack state.

### Members

- <code>GuiScreen Num</code>
- <code>bool IsModal</code>
- <code>bool IsMultiinstance</code>
- <code>bool IsCloseOnMiss</code>
- <code>bool IsCanMove</code>
- <code>bool IsMoveIgnoreBorders</code>
- <code>CursorType[] AvailableCursors</code>
- <code>CursorType Cursor</code>
- <code>bool IsOnTop</code>
- <code>void SetModal(bool enabled)</code>
- <code>void SetMultiinstance(bool enabled)</code>
- <code>void SetCloseOnMiss(bool enabled)</code>
- <code>void SetCanMove(bool enabled, bool ignoreBorders)</code>
- <code>void SetAvailableCursors(CursorType[] cursors)</code>

### Callbacks

- None declared by this type.

## Grid

Base: <code>Panel</code>
Source: [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos)

Panel that clones a named prototype node into indexed cells and lays them out by columns and padding.

### Members

- <code>string CellPrototype</code>
- <code>int GridSize</code>
- <code>int Columns</code>
- <code>Object[] Cells</code>
- <code>void ResizeGrid(int size)</code>
- <code>void RefreshContentPositions()</code>
- <code>void SetCellPrototype(string name)</code>
- <code>void SetGridSize(int size)</code>
- <code>void SetColumns(int length)</code>
- <code>void SetPadding(ipos pos)</code>
- <code>void SetPadding(int x, int y)</code>

### Callbacks

- None declared by this type.

## MessageBox

Base: <code>Text</code>
Source: [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos)

Text-derived timestamped message feed with type filtering, inversion, scrolling, and a project-configurable default inversion setting.

### Members

- <code>string[] MessageTexts</code>
- <code>MessageBoxType[] MessageTypes</code>
- <code>string[] MessageTimes</code>
- <code>MessageBoxType[] DisplayedMessages</code>
- <code>bool InvertMessages</code>
- <code>void AddMessage(string text, MessageBoxType type = MessageBoxType::Default)</code>
- <code>void SetDisplayedMessages(MessageBoxType[] messageTypes)</code>
- <code>void ChangeDisplayedMessage(MessageBoxType messageType, bool enable)</code>
- <code>void SetInvertMessages(bool invert)</code>
- <code>void ClearMessages()</code>

### Callbacks

- None declared by this type.

## Console

Base: <code>TextInput</code>
Source: [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos)

TextInput-derived command console with activation, history persistence, and submit behavior.

### Members

- <code>string HistoryStorageName</code>
- <code>string[] History</code>
- <code>int HistoryMaxLength</code>
- <code>void Activate()</code>
- <code>void Deactivate()</code>
- <code>void SendText()</code>
- <code>void Toggle() // Automatically manage calls of Activate / Deactivate / SendText</code>
- <code>void SetHistoryStorage(string storageName)</code>
- <code>void SetHistoryMaxLength(int length)</code>

### Callbacks

- None declared by this type.

## ItemView

Base: <code>Grid</code>
Source: [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos)

Grid that obtains client Item handles, filters or sorts them, binds live handles to cells, and invokes item drawing callbacks.

### Members

- <code>int UserData</code>
- <code>int UserDataExt</code>
- <code>bool UseSorting</code>
- <code>Item[] Items</code>
- <code>Item? GetItem(int cellIndex)</code>
- <code>void Resort()</code>
- <code>void SetUserData(int data)</code>
- <code>void SetUserDataExt(int data)</code>
- <code>void SetUseSorting(bool enable)</code>

### Callbacks

- <code>Item[] OnGetItems() - return all items for display</code>
- <code>int OnCheckItem(Item item) - return slot index if UseSorting == false; sorting value if UseSorting == true; &lt; 0 to discard item</code>
