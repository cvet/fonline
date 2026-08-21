---
title: Типы GUI Runtime
document_id: generated-gui-runtime-types
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-gui-runtime-types","locale":"ru","source_path":"Docs/en/reference/gui-runtime/types.md","source_sha256":"a4a718698ac0d82e1129d66f8d3c310e4e8c3642fcfa5fd9e094df22db722fef"} -->

# Типы GUI Runtime

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/GuiRuntimeInterface.json`, затем выполните `python BuildTools/docs_gui_runtime.py --write`.

[Индекс](index.md) | [Типы](types.md) | [API экранов](screen-api.md) | [Жизненный цикл](lifecycle.md) | [Компоновка](layout-rendering.md) | [Ввод](input.md) | [Интеграция](integration-validation.md) | [Канонический JSON](../../../generated/gui-runtime.json) | [Руководство](../../how-to/runtime/gui.md)

| Стабильный ID | Тип | Базовый тип | Члены | Callbacks | Роль |
| --- | --- | --- | --- | --- | --- |
| <a id="entry-gui-runtime-type-object-f26b45999e"></a><code>gui-runtime.type.object</code> | <code>Object</code> | - | 56 | 36 | Базовый узел дерева для активности, геометрии, иерархии, hit testing, фокуса, перетаскивания, callback-функций, поиска и рекурсивной отрисовки. |
| <a id="entry-gui-runtime-type-panel-ba52e2409f"></a><code>gui-runtime.type.panel</code> | <code>Panel</code> | <code>Object</code> | 17 | 0 | Объект со спрайтовым фоном, необязательной рамкой 9-slice, crop/scissor и анимированной вертикальной или горизонтальной прокруткой. |
| <a id="entry-gui-runtime-type-text-b7ebcc2a6b"></a><code>gui-runtime.type.text</code> | <code>Text</code> | <code>Object</code> | 12 | 0 | Отрисовываемый текстовый узел с привязанным FontType, набором FontFlag, цветами, состоянием skip-lines и необязательным измерением высоты. |
| <a id="entry-gui-runtime-type-text-input-e905911387"></a><code>gui-runtime.type.text-input</code> | <code>TextInput</code> | <code>Text</code> | 6 | 0 | Фокусируемый редактируемый текстовый узел с длиной, паролем, carriage, буфером обмена и экранной клавиатурой. |
| <a id="entry-gui-runtime-type-button-a91e38fc0a"></a><code>gui-runtime.type.button</code> | <code>Button</code> | <code>Panel</code> | 14 | 0 | Panel с состояниями pressed, hover, disabled, switched и condition. |
| <a id="entry-gui-runtime-type-check-box-edcfced6be"></a><code>gui-runtime.type.check-box</code> | <code>CheckBox</code> | <code>Button</code> | 2 | 1 | Button с checked-state и callback-функцией OnCheckedChanged. |
| <a id="entry-gui-runtime-type-radio-button-4c0157ef47"></a><code>gui-runtime.type.radio-button</code> | <code>RadioButton</code> | <code>CheckBox</code> | 0 | 0 | CheckBox, который сбрасывает switched у соседних radio-кнопок одного родителя. |
| <a id="entry-gui-runtime-type-screen-fa6503ba94"></a><code>gui-runtime.type.screen</code> | <code>Screen</code> | <code>Panel</code> | 14 | 0 | Верхнеуровневая panel с регистрационным id, поведением modal/multiinstance/close-on-miss, набором курсоров, политикой перемещения и состоянием вершины стека. |
| <a id="entry-gui-runtime-type-grid-8070fde4e8"></a><code>gui-runtime.type.grid</code> | <code>Grid</code> | <code>Panel</code> | 11 | 0 | Panel, которая клонирует именованный прототип узла в индексированные ячейки и размещает их по столбцам и отступам. |
| <a id="entry-gui-runtime-type-message-box-e1b7304252"></a><code>gui-runtime.type.message-box</code> | <code>MessageBox</code> | <code>Text</code> | 10 | 0 | Производная от Text лента сообщений с временными метками, фильтрацией типов, инверсией, прокруткой и настраиваемой проектом политикой инверсии по умолчанию. |
| <a id="entry-gui-runtime-type-console-779de0e59e"></a><code>gui-runtime.type.console</code> | <code>Console</code> | <code>TextInput</code> | 9 | 0 | Производная от TextInput командная консоль с активацией, сохранением истории и отправкой. |
| <a id="entry-gui-runtime-type-item-view-c7d1736089"></a><code>gui-runtime.type.item-view</code> | <code>ItemView</code> | <code>Grid</code> | 9 | 2 | Grid, которая получает клиентские handle Item, фильтрует или сортирует их, связывает живые handle с ячейками и вызывает callback-функции отрисовки предметов. |

## Object

Базовый тип: отсутствует
Источник: [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos)

Базовый узел дерева для активности, геометрии, иерархии, hit testing, фокуса, перетаскивания, callback-функций, поиска и рекурсивной отрисовки.

### Члены

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

### Callback-функции

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

Базовый тип: <code>Object</code>
Источник: [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos)

Объект со спрайтовым фоном, необязательной рамкой 9-slice, crop/scissor и анимированной вертикальной или горизонтальной прокруткой.

### Члены

- <code>Sprite::Sprite BackgroundImage</code>
- <code>SpriteLayout BackgroundImageLayout</code>
- <code>void SetBackgroundImage(string imageName, SpriteLayout imageLayout = SpriteLayout::None)</code>
- <code>void SetBackgroundImage(hstring imageName, SpriteLayout imageLayout = SpriteLayout::None)</code>
- <code>void SetFrameImage(string imageName)</code>
- <code>void SetFrameImage(hstring imageName)</code>
- <code>void SetCapInsets(int left, int top, int right, int bottom)</code>
- <code>void SetCropContent(bool enabled)</code>
- <code>bool CropContent</code>
- <code>void SetAutoScroll(bool ver, bool hor)</code>
- <code>int VerticalScrollValue</code>
- <code>int HorizontalScrollValue</code>
- <code>int VerticalScrollRange</code>
- <code>int HorizontalScrollRange</code>
- <code>void ModifyScroll(int ver, int hor)</code>
- <code>bool CanModifyScroll(int ver, int hor)</code>
- <code>void SetScrollValue(int ver, int hor)</code>

### Callback-функции

- Этот тип не объявляет callback-функций.

## Text

Базовый тип: <code>Object</code>
Источник: [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos)

Отрисовываемый текстовый узел с привязанным FontType, набором FontFlag, цветами, состоянием skip-lines и необязательным измерением высоты.

### Члены

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

### Callback-функции

- Этот тип не объявляет callback-функций.

## TextInput

Базовый тип: <code>Text</code>
Источник: [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos)

Фокусируемый редактируемый текстовый узел с длиной, паролем, carriage, буфером обмена и экранной клавиатурой.

### Члены

- <code>int InputLength</code>
- <code>bool IsTextPassword</code>
- <code>string PasswordChar</code>
- <code>void SetInputLength(int length)</code>
- <code>void SetInputPassword(string passwordChar)</code>
- <code>void SetCarriage(bool enable)</code>

### Callback-функции

- Этот тип не объявляет callback-функций.

## Button

Базовый тип: <code>Panel</code>
Источник: [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos)

Panel с состояниями pressed, hover, disabled, switched и condition.

### Члены

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

### Callback-функции

- Этот тип не объявляет callback-функций.

## CheckBox

Базовый тип: <code>Button</code>
Источник: [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos)

Button с checked-state и callback-функцией OnCheckedChanged.

### Члены

- <code>bool IsChecked</code>
- <code>void SetChecked(bool checked)</code>

### Callback-функции

- <code>void OnCheckedChanged()</code>

## RadioButton

Базовый тип: <code>CheckBox</code>
Источник: [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos)

CheckBox, который сбрасывает switched у соседних radio-кнопок одного родителя.

### Члены


### Callback-функции

- Этот тип не объявляет callback-функций.

## Screen

Базовый тип: <code>Panel</code>
Источник: [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos)

Верхнеуровневая panel с регистрационным id, поведением modal/multiinstance/close-on-miss, набором курсоров, политикой перемещения и состоянием вершины стека.

### Члены

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

### Callback-функции

- Этот тип не объявляет callback-функций.

## Grid

Базовый тип: <code>Panel</code>
Источник: [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos)

Panel, которая клонирует именованный прототип узла в индексированные ячейки и размещает их по столбцам и отступам.

### Члены

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

### Callback-функции

- Этот тип не объявляет callback-функций.

## MessageBox

Базовый тип: <code>Text</code>
Источник: [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos)

Производная от Text лента сообщений с временными метками, фильтрацией типов, инверсией, прокруткой и настраиваемой проектом политикой инверсии по умолчанию.

### Члены

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

### Callback-функции

- Этот тип не объявляет callback-функций.

## Console

Базовый тип: <code>TextInput</code>
Источник: [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos)

Производная от TextInput командная консоль с активацией, сохранением истории и отправкой.

### Члены

- <code>string HistoryStorageName</code>
- <code>string[] History</code>
- <code>int HistoryMaxLength</code>
- <code>void Activate()</code>
- <code>void Deactivate()</code>
- <code>void SendText()</code>
- <code>void Toggle() // Automatically manage calls of Activate / Deactivate / SendText</code>
- <code>void SetHistoryStorage(string storageName)</code>
- <code>void SetHistoryMaxLength(int length)</code>

### Callback-функции

- Этот тип не объявляет callback-функций.

## ItemView

Базовый тип: <code>Grid</code>
Источник: [Source/Scripting/AngelScript/CoreScripts/Gui.fos](https://github.com/cvet/fonline/blob/master/Source/Scripting/AngelScript/CoreScripts/Gui.fos)

Grid, которая получает клиентские handle Item, фильтрует или сортирует их, связывает живые handle с ячейками и вызывает callback-функции отрисовки предметов.

### Члены

- <code>int UserData</code>
- <code>int UserDataExt</code>
- <code>bool UseSorting</code>
- <code>Item[] Items</code>
- <code>Item? GetItem(int cellIndex)</code>
- <code>void Resort()</code>
- <code>void SetUserData(int data)</code>
- <code>void SetUserDataExt(int data)</code>
- <code>void SetUseSorting(bool enable)</code>

### Callback-функции

- <code>Item[] OnGetItems() - return all items for display</code>
- <code>int OnCheckItem(Item item) - return slot index if UseSorting == false; sorting value if UseSorting == true; &lt; 0 to discard item</code>
