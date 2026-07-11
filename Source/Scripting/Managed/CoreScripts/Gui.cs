#nullable enable

using System.Collections.Generic;
using System.Threading.Tasks;
using FOnline;

namespace FOnline
{
    public static partial class Gui
    {
#if CLIENT

        ///@ Enum GuiScreen None = 0

        ///@ Enum CursorType Default = 0

        ///@ Enum MessageBoxType None = 0
        ///@ Enum MessageBoxType Default
        ///@ Enum MessageBoxType All

        ///@ Setting Client bool Gui.MsgboxInvert

        public static CursorType Cursor = CursorType.Default;

        public static CursorType DraggableCursor = CursorType.Default;

        public static ident CursorData;

        // Anchor styles
        ///@ Enum AnchorStyle None = 0
        ///@ Enum AnchorStyle Left = 1
        ///@ Enum AnchorStyle Right = 2
        ///@ Enum AnchorStyle Top = 4
        ///@ Enum AnchorStyle Bottom = 8

        // Dock styles
        ///@ Enum DockStyle None = 0
        ///@ Enum DockStyle Left = 1
        ///@ Enum DockStyle Right = 2
        ///@ Enum DockStyle Top = 3
        ///@ Enum DockStyle Bottom = 4
        ///@ Enum DockStyle Fill = 5

        ///@ Event Client Game OnScreenChange(bool show, GuiScreen screenNum, string=>any params)
        ///@ Event Client Game OnConsoleMessage(string message)

        public interface IDropMenu
        {
            void Draw();
            void MouseDown(MouseButton button);
            void MouseUp(MouseButton button);
            bool PreMouseMove(ipos offset);
            void PostMouseMove(ipos offset);
            void InputLost();
            void Loop();
            };

        public delegate Screen CreateScreenFunc();

        public delegate bool DragAndDropHandler(Object obj, Object? target);

        // Inheritance
        // Object
        //   Panel
        //     Screen
        //     Grid
        //       ItemView
        //     Button
        //       CheckBox
        //         RadioButton
        //   Text
        //     TextInput
        //       Console
        //     MessageBox

        // Callbacks
        // Object
        //   void OnConstruct()
        //   void OnInit()
        //   void OnShow()
        //   void OnShow(dict<string, any> params)
        //   void OnHide()
        //   void OnAppear()
        //   void OnDisappear()
        //   void OnDraw()
        //   void OnPostDraw()
        //   void OnMove(ipos deltaPos)
        //   void OnMouseDown(MouseButton button)
        //   void OnMouseUp(MouseButton button, bool lost)
        //   void OnMousePressed(MouseButton button)
        //   void OnLMousePressed()
        //   void OnRMousePressed()
        //   void OnMouseClick(MouseButton button)
        //   void OnLMouseClick()
        //   void OnRMouseClick()
        //   void OnMouseMove()
        //   void OnGlobalMouseDown(MouseButton button)
        //   void OnGlobalMouseUp(MouseButton button)
        //   void OnGlobalMousePressed(MouseButton button)
        //   void OnGlobalMouseClick(MouseButton button)
        //   void OnGlobalMouseMove()
        //   void OnInput()
        //   void OnInput(KeyCode key)
        //   void OnInput(string text)
        //   void OnInput(KeyCode key, string text)
        //   void OnGlobalInput(KeyCode key, string text)
        //   void OnActiveChanged()
        //   void OnFocusChanged()
        //   void OnHoverChanged()
        //   void OnDragChanged()
        //   void OnResizeGrid(Object cell, int cellIndex)
        //   void OnDrawItem(Item item, Object cell, int cellIndex)
        //   void OnRefreshText()
        // CheckBox
        //   void OnCheckedChanged()
        // ItemView
        //   Item[] OnGetItems() - return all items for display
        //   int OnCheckItem(Item item) - return slot index if UseSorting == false; sorting value if UseSorting == true; < 0 to discard item

        // API
        // Object
        //   bool Active
        //   bool ActiveSelf
        //   string Name
        //   ipos Pos
        //   ipos AbsolutePos
        //   isize Size
        //   AnchorStyle Anchor
        //   DockStyle Dock
        //   bool IsDraggable
        //   bool IsNotHittable
        //   bool IsNotCatchable
        //   bool CheckTransparentOnHit
        //   bool FocusGroup
        //   bool IsFocused
        //   bool IsPressed
        //   bool IsHovered
        //   bool IsDragged
        //   int ChildCount
        //   Object Parent
        //   Screen Screen
        //   Grid Grid
        //   int CellIndex
        //   void Init(Object parent)
        //   void Remove()
        //   void SetActive(bool active)
        //   void SetPosition(ipos pos)
        //   void SetPosition(int x, int y)
        //   void SetPosition(string iniKey)
        //   void SetSize(isize size)
        //   void SetSize(int width, int height)
        //   void SetAnchor(AnchorStyle anchorStyles)
        //   void SetDock(DockStyle dockStyle)
        //   void SetColor(ucolor color)
        //   void SetDraggable(bool enabled)
        //   void SetNotHittable(bool enabled)
        //   void SetNotCatchable(bool enabled)
        //   void SetCheckTransparentOnHit(bool enabled)
        //   void SetFocusGroup(bool enabled)
        //   Object? FindMouseHit()
        //   Object? FindHit(ipos pos)
        //   bool IsMouseHit()
        //   bool IsHit(ipos pos)
        //   void GetWholeSize(ref ipos& centerPos, ref isize& wholeSize, bool onlyChidren = false)
        //   void Draw(ipos pos)
        //   void Move(ipos deltaPos)
        //   void StartDragging()
        //   void MouseClick(MouseButton button)
        //   void Input(KeyCode key, string text)
        //   Panel FindPanel(string name)
        //   Text FindText(string name)
        //   TextInput FindTextInput(string name)
        //   Button FindButton(string name)
        //   Object Find(string name, bool deepFind = true, int skip = 0)
        //   int Count(string name, bool deepCount = true)
        //   Object GetChild(int index)
        // Panel : Object
        //   Sprite::Sprite BackgroundImage
        //   SpriteLayout BackgroundImageLayout
        //   void SetBackgroundImage(string imageName, SpriteLayout imageLayout = SpriteLayout::None)
        //   void SetBackgroundImage(hstring imageName, SpriteLayout imageLayout = SpriteLayout::None)
        //   void SetCropContent(bool enabled)
        //   void SetAutoScroll(bool ver, bool hor)
        //   int VerticalScrollValue
        //   int HorizontalScrollValue
        //   int VerticalScrollRange
        //   int HorizontalScrollRange
        //   void ModifyScroll(int ver, int hor)
        //   bool CanModifyScroll(int ver, int hor)
        //   void SetScrollValue(int ver, int hor)
        // Text : Object
        //   string Text (overridable)
        //   int TextFont
        //   ucolor TextColor
        //   ucolor TextColorFocused
        //   int TextFlags
        //   void SetText(string text, int font, int flags)
        //   void SetText(string text)
        //   void SetTextWithResize(string text)
        //   void SetTextFont(int font)
        //   void SetTextFlags(int flags)
        //   void SetTextColor(ucolor color)
        //   void SetTextFocusedColor(ucolor color)
        // TextInput : Text : Object
        //   int InputLength
        //   bool IsTextPassword
        //   string PasswordChar
        //   void SetInputLength(int length)
        //   void SetInputPassword(string passwordChar)
        //   void SetCarriage(bool enable)
        // Button : Panel : Object
        //   Sprite::Sprite PressedImage
        //   SpriteLayout PressedImageLayout
        //   Sprite::Sprite HoverImage
        //   SpriteLayout HoverImageLayout
        //   bool IsSwitched
        //   bool IsDisabled
        //   void SetPressedImage(string imageName, SpriteLayout imageLayout = SpriteLayout::None)
        //   void SetPressedImage(hstring imageName, SpriteLayout imageLayout = SpriteLayout::None)
        //   void SetHoverImage(string imageName, SpriteLayout imageLayout = SpriteLayout::None)
        //   void SetHoverImage(hstring imageName, SpriteLayout imageLayout = SpriteLayout::None)
        //   void SetDisabledImage(string imageName, SpriteLayout imageLayout = SpriteLayout::None)
        //   void SetDisabledImage(hstring imageName, SpriteLayout imageLayout = SpriteLayout::None)
        //   void SetSwitch(bool enabled)
        //   void SetCondition(bool enabled)
        // CheckBox : Button : Panel : Object
        //   bool IsChecked
        //   void SetChecked(bool checked)
        // RadioButton : CheckBox : Button : Panel : Object
        // Screen : Panel : Object
        //   GuiScreen Num
        //   bool IsModal
        //   bool IsMultiinstance
        //   bool IsCloseOnMiss
        //   bool IsCanMove
        //   bool IsMoveIgnoreBorders
        //   CursorType[] AvailableCursors
        //   CursorType Cursor
        //   bool IsOnTop
        //   void SetModal(bool enabled)
        //   void SetMultiinstance(bool enabled)
        //   void SetCloseOnMiss(bool enabled)
        //   void SetCanMove(bool enabled, bool ignoreBorders)
        //   void SetAvailableCursors(CursorType[] cursors)
        // Grid : Panel : Object
        //   string CellPrototype
        //   int GridSize
        //   int Columns
        //   Object[] Cells
        //   void ResizeGrid(int size)
        //   void RefreshContentPositions()
        //   void SetCellPrototype(string name)
        //   void SetGridSize(int size)
        //   void SetColumns(int length)
        //   void SetPadding(ipos pos)
        //   void SetPadding(int x, int y)
        // MessageBox : Text : Object
        //   string[] MessageTexts
        //   int[] MessageTypes
        //   string[] MessageTimes
        //   MessageBoxType[] DisplayedMessages
        //   bool InvertMessages
        //   void AddMessage(string text, MessageBoxType type = MessageBoxType::Default)
        //   void SetDisplayedMessages(MessageBoxType[] messageTypes)
        //   void ChangeDisplayedMessage(MessageBoxType messageType, bool enable)
        //   void SetInvertMessages(bool invert)
        //   void ClearMessages()
        // Console : TextInput : Text : Object
        //   string HistoryStorageName
        //   string[] History
        //   int HistoryMaxLength
        //   void Activate()
        //   void Deactivate()
        //   void SendText()
        //   void Toggle() // Automatically manage calls of Activate / Deactivate / SendText
        //   void SetHistoryStorage(string storageName)
        //   void SetHistoryMaxLength(int length)
        // ItemView : Grid : Panel : Object
        //   int UserData
        //   int UserDataExt
        //   bool UseSorting
        //   Item[] Items
        //   Item? GetItem(int cellIndex)
        //   void Resort()
        //   void SetUserData(int data)
        //   void SetUserDataExt(int data)
        //   void SetUseSorting(bool enable)

        public partial class Object
        {
            public bool Active
            {
                get
                {
                    return _Active;
                }
            }

            public bool ActiveSelf
            {
                get
                {
                    return _ActiveSelf;
                }
            }

            public string Name
            {
                get
                {
                    return _Name;
                }
            }

            public ipos Pos
            {
                get
                {
                    Object? parent = _Parent;
                    return parent != null ? _AbsolutePos - parent._AbsolutePos : _AbsolutePos;
                }
            }

            public ipos AbsolutePos
            {
                get
                {
                    return _AbsolutePos;
                }
            }

            public isize Size
            {
                get
                {
                    return _Size;
                }
            }

            public AnchorStyle Anchor
            {
                get
                {
                    return _Anchor;
                }
            }

            public DockStyle Dock
            {
                get
                {
                    return _Dock;
                }
            }

            public ucolor Color
            {
                get
                {
                    return _Color;
                }
            }

            public bool IsDraggable
            {
                get
                {
                    return _IsDraggable;
                }
            }

            public bool IsNotHittable
            {
                get
                {
                    return _IsNotHittable;
                }
            }

            public bool IsNotCatchable
            {
                get
                {
                    return _IsNotCatchable;
                }
            }

            public bool CheckTransparentOnHit
            {
                get
                {
                    return _CheckTransparentOnHit;
                }
            }

            public bool FocusGroup
            {
                get
                {
                    return _FocusGroup;
                }
            }

            public bool IsFocused
            {
                get
                {
                    return _IsFocused;
                }
            }

            public bool IsPressed
            {
                get
                {
                    return _IsPressed;
                }
            }

            public bool IsHovered
            {
                get
                {
                    return _IsHovered;
                }
            }

            public bool IsDragged
            {
                get
                {
                    Object? parent = _Parent;
                    return _IsDragged || (parent != null && parent.IsDragged);
                }
            }

            public int ChildCount
            {
                get
                {
                    return _Children.Count;
                }
            }

            public Object? Parent
            {
                get
                {
                    return _Parent;
                }
            }

            public Screen? Screen
            {
                get
                {
                    Object? parent = _Parent;
                    return parent != null ? parent.Screen : (this as Screen);
                }
            }

            public Grid? Grid
            {
                get
                {
                    return _Grid;
                }
            }

            public int CellIndex
            {
                get
                {
                    return _CellIndex;
                }
            }

            public bool _ActiveSelf;
            public bool _Active;
            public bool _HasOnDraw;
            public bool _NeedDraw;
            public string _Name = default!;
            public ipos _BasePos;
            public isize _BaseSize;
            public ipos _AbsolutePos;
            public isize _Size;
            public AnchorStyle _Anchor;
            public DockStyle _Dock;
            public ucolor _Color;
            public bool _IsDragged;
            public bool _IsDraggable;
            public bool _IsNotHittable;
            public bool _IsNotCatchable;
            public bool _CheckTransparentOnHit;
            public bool _FocusGroup;
            public bool _DeferredMousePressed;
            public bool _IsFocused;
            public bool _IsPressed;
            public bool _IsHovered;
            public MouseButton _PressedButton;
            public ipos _PressedPos;
            public Object? _Parent;
            public List<Object> _Children = default!;
            public Grid? _Grid;
            public int _CellIndex;
            public bool _Highlighted;

            // Callbacks
            public virtual void OnConstruct()
            {
            }

            public virtual void OnInit()
            {
            }

            public virtual void OnShow()
            {
            }

            public virtual void OnShow(Dictionary<string, string> @params)
            {
            }

            public virtual void OnHide()
            {
            }

            public virtual void OnAppear()
            {
            }

            public virtual void OnDisappear()
            {
            }

            public virtual void OnRemove()
            {
            }

            public virtual void OnDraw()
            {
            }

            public virtual void OnPostDraw()
            {
            }

            public virtual void OnMove(ipos deltaPos)
            {
            }

            public virtual void OnMouseDown(MouseButton button)
            {
            }

            public virtual void OnMouseUp(MouseButton button, bool lost)
            {
            }

            public virtual void OnMousePressed(MouseButton button)
            {
            }

            public virtual void OnLMousePressed()
            {
            }

            public virtual void OnRMousePressed()
            {
            }

            public virtual void OnMouseClick(MouseButton button)
            {
            }

            public virtual void OnLMouseClick()
            {
            }

            public virtual void OnRMouseClick()
            {
            }

            public virtual void OnMouseMove()
            {
            }

            public virtual void OnGlobalMouseDown(MouseButton button)
            {
            }

            public virtual void OnGlobalMouseUp(MouseButton button)
            {
            }

            public virtual void OnGlobalMousePressed(MouseButton button)
            {
            }

            public virtual void OnGlobalMouseClick(MouseButton button)
            {
            }

            public virtual void OnGlobalMouseMove()
            {
            }

            public virtual void OnInput()
            {
            }

            public virtual void OnInput(KeyCode key)
            {
            }

            public virtual void OnInput(string text)
            {
            }

            public virtual void OnInput(KeyCode key, string text)
            {
            }

            public virtual void OnGlobalInput(KeyCode key, string text)
            {
            }

            public virtual void OnActiveChanged()
            {
            }

            public virtual void OnFocusChanged()
            {
            }

            public virtual void OnHoverChanged()
            {
            }

            public virtual void OnDragChanged()
            {
            }

            public virtual void OnResizeGrid(Object cell, int cellIndex)
            {
            }

            public virtual void OnDrawItem(Item item, Object cell, int cellIndex)
            {
            }

            public virtual void OnRefreshText()
            {
            }

            public virtual void OnCleanup()
            {
            }

            public Object()
            {
                _Children = new List<Object>();
            }

            public void Init(Object? parent)
            {
                _ActiveSelf = true;
                _Name = GetType().Name;
                _Highlighted = false;

                Object? oldParent = _Parent;
                if (oldParent != null) {
                    oldParent._Children.RemoveAt(oldParent._Children.IndexOf(this));
                }
                _Parent = parent;
                if (parent != null) {
                    parent._Children.Add(this);
                }
                _RefreshActive();

                _Construct();
                OnConstruct();

                _RefreshNeedDraw();
                if (parent != null) {
                    parent._RefreshNeedDraw();
                }

                Screen? screen = Screen;
                if (screen != null && screen._IsRegistered) {
                    _Init();
                }
            }

            public virtual void _Construct()
            {
                // Virtual
            }

            public virtual void _FixClone()
            {
                _Parent = null;
                _Children = new List<Object>();
                _Highlighted = false;
            }

            public virtual void _Init()
            {
                Object? parent = _Parent;
                _Active = _ActiveSelf && (parent == null || parent._Active);
                for (int i = 0; i < _Children.Count; i++) {
                    _Children[i]._Init();
                }
                OnInit();
                _RefreshPosition();
            }

            public virtual void _Show(Dictionary<string, string> @params)
            {
                OnShow();
                OnShow(@params);
                for (int i = 0; i < _Children.Count; i++) {
                    _Children[i]._Show(@params);
                }
            }

            public virtual void _Hide()
            {
                OnHide();
                for (int i = 0; i < _Children.Count; i++) {
                    _Children[i]._Hide();
                }
            }

            public virtual void _Appear()
            {
                OnAppear();
                for (int i = 0; i < _Children.Count; i++) {
                    _Children[i]._Appear();
                }
            }

            public virtual void _Disappear()
            {
                OnDisappear();
                for (int i = 0; i < _Children.Count; i++) {
                    _Children[i]._Disappear();
                }
            }

            public void _DragChanged()
            {
                OnDragChanged();
                for (int i = 0; i < _Children.Count; i++) {
                    _Children[i]._DragChanged();
                }
            }

            public void Remove()
            {
                Object? oldParent = _Parent;
                if (oldParent != null) {
                    oldParent._Children.RemoveAt(oldParent._Children.IndexOf(this));
                    _Parent = null;
                }

                _Remove();

                _ActiveSelf = false;
                _RefreshActive();

                if (oldParent != null) {
                    oldParent._RefreshNeedDraw();
                }
            }

            public virtual void _Remove()
            {
                OnRemove();
                for (int i = 0; i < _Children.Count; i++) {
                    _Children[i]._Remove();
                }
            }

            public void _RefreshPositionRecursive()
            {
                _RefreshPosition();

                for (int i = 0; i < _Children.Count; i++) {
                    _Children[i]._RefreshPositionRecursive();
                }
            }

            public void _RefreshTextRecursive()
            {
                for (int i = 0; i < _Children.Count; i++) {
                    _Children[i]._RefreshTextRecursive();
                }

                OnRefreshText();
            }

            public void _DrawCallback()
            {
                // Draw item callback
                if (_Grid != null) {
                    ItemView? itemView = (_Grid as ItemView);
                    if (itemView != null) {
                        Item? cellItem = itemView.GetItem(_CellIndex);
                        if (cellItem != null) {
                            OnDrawItem(cellItem, this, _CellIndex);
                        }
                    }
                }

                // Common draw callback
                if (_HasOnDraw) {
                    OnDraw();
                }
            }

            public virtual void _Draw(bool callCallback)
            {
                if (!_ActiveSelf || !_NeedDraw) {
                    return;
                }

                if (callCallback) {
                    _DrawCallback();
                }

                // _Children
                int childCount = _Children.Count;
                for (int i = 0; i < childCount; i++) {
                    Object child = _Children[i];
                    if (child._ActiveSelf && child._NeedDraw && !child._IsDragged) {
                        child._Draw(true);
                    }
                }

                if (callCallback) {
                    OnPostDraw();
                }
            }
            protected void _DrawBase(bool callCallback)
            {
                if (!_ActiveSelf || !_NeedDraw) {
                    return;
                }

                if (callCallback) {
                    _DrawCallback();
                }

                // _Children
                int childCount = _Children.Count;
                for (int i = 0; i < childCount; i++) {
                    Object child = _Children[i];
                    if (child._ActiveSelf && child._NeedDraw && !child._IsDragged) {
                        child._Draw(true);
                    }
                }

                if (callCallback) {
                    OnPostDraw();
                }
            }

            public virtual bool _HasSelfDraw()
            {
                return _HasOnDraw || _Grid != null;
            }

            public void _RefreshNeedDraw()
            {
                bool need = _HasSelfDraw();
                if (!need) {
                    for (int i = 0; i < _Children.Count; i++) {
                        if (_Children[i]._NeedDraw) {
                            need = true;
                            break;
                        }
                    }
                }

                if (_NeedDraw == need) {
                    return;
                }

                _NeedDraw = need;

                Object? parent = _Parent;
                if (parent != null) {
                    parent._RefreshNeedDraw();
                }
            }

            public void Draw(ipos pos)
            {
                ipos deltaPos = pos - _AbsolutePos;
                _Move(deltaPos, false, false);
                _Draw(true);
                _Move(-deltaPos, false, false);
            }

            public void Move(ipos deltaPos)
            {
                _Move(deltaPos, true, true);
            }

            public void _Move(ipos deltaPos, bool callCallback, bool moveBasePos)
            {
                _AbsolutePos += deltaPos;

                if (moveBasePos) {
                    _BasePos += deltaPos;
                }

                for (int i = 0; i < _Children.Count; i++) {
                    _Children[i]._Move(deltaPos, false, false);
                }

                if (callCallback) {
                    _MoveCallback(deltaPos);
                }
            }

            public void _MoveCallback(ipos deltaPos)
            {
                OnMove(deltaPos);
                for (int i = 0; i < _Children.Count; i++) {
                    _Children[i]._MoveCallback(deltaPos);
                }
            }

            public bool _IsCatchable()
            {
                bool result = !_IsNotCatchable && !_IsDraggable;
                Object? parent = Parent;
                return result && (parent != null ? parent._IsCatchable() : true);
            }

            public Object? FindMouseHit()
            {
                return FindHit(Game.MousePos);
            }

            public virtual Object? FindHit(ipos pos)
            {
                // Inactive subtrees can never produce a hit (IsHit walks ancestors and bails on inactive)
                if (!_ActiveSelf) {
                    return null;
                }

                // Check children
                for (int i = _Children.Count - 1; i >= 0; i--) {
                    Object? obj = _Children[i].FindHit(pos);
                    if (obj != null) {
                        return obj;
                    }
                }

                if (_IsHitSelf(pos)) {
                    return this;
                }

                // No collision found
                return null;
            }

            public bool IsMouseHit()
            {
                return IsHit(Game.MousePos);
            }

            public bool IsHit(ipos pos)
            {
                if (!_IsHitSelf(pos)) {
                    return false;
                }

                Object? ancestor = _Parent;
                while (ancestor != null) {
                    Object? obj = ancestor;
                    if (obj == null) {
                        break;
                    }

                    if (!obj._ActiveSelf) {
                        return false;
                    }

                    Panel? panel = (obj as Panel);
                    if (panel != null && panel._CropContent &&
                        (pos.x < obj._AbsolutePos.x || pos.x >= obj._AbsolutePos.x + obj._Size.width || pos.y < obj._AbsolutePos.y ||
                         pos.y >= obj._AbsolutePos.y + obj._Size.height)) {
                        return false;
                    }

                    ancestor = obj._Parent;
                }

                return true;
            }

            public virtual bool _IsHitSelf(ipos pos)
            {
                if (!_ActiveSelf || _IsNotHittable || _Size.width <= 0 || _Size.height <= 0) {
                    return false;
                }
                if (pos.x < _AbsolutePos.x || pos.x >= _AbsolutePos.x + _Size.width || pos.y < _AbsolutePos.y || pos.y >= _AbsolutePos.y + _Size.height) {
                    return false;
                }
                return true;
            }

            public void _GetWholeSizeRect(List<int> rect)
            {
                int l = _AbsolutePos.x;
                int t = _AbsolutePos.y;
                int r = l + Size.width;
                int b = t + Size.height;

                if (l < rect[0]) {
                    rect[0] = (int)(l);
                }
                if (t < rect[1]) {
                    rect[1] = (int)(t);
                }
                if (r > rect[2]) {
                    rect[2] = (int)(r);
                }
                if (b > rect[3]) {
                    rect[3] = (int)(b);
                }

                for (int i = 0; i < _Children.Count; i++) {
                    _Children[i]._GetWholeSizeRect(rect);
                }
            }

            public void GetWholeSize(ref ipos centerPos, ref isize wholeSize, bool onlyChidren = false)
            {
                List<int> rect = new List<int>{1000000000, 1000000000, -1000000000, -1000000000};
                if (!onlyChidren) {
                    _GetWholeSizeRect(rect);
                }
                else if (_Children.Count > 0) {
                    for (int i = 0; i < _Children.Count; i++) {
                        _Children[i]._GetWholeSizeRect(rect);
                    }
                }
                else {
                    rect[0] = (int)(rect[1] = rect[2] = rect[3] = 0);
                }
                wholeSize.width = rect[2] - rect[0];
                wholeSize.height = rect[3] - rect[1];
                centerPos.x = rect[0] + wholeSize.width / 2;
                centerPos.y = rect[1] + wholeSize.height / 2;
            }

            public void _MouseDown(MouseButton button)
            {
                _IsPressed = true;
                _PressedButton = button;
                _PressedPos = Game.MousePos;
                OnMouseDown(button);

                if (_IsDraggable && _PressedButton == MouseButton.Left) {
                    bool draggableCursor = (DraggableCursor != CursorType.Default && Cursor == DraggableCursor);
                    if (draggableCursor) {
                        _IsDragged = true;
                        _DragChanged();
                    }
                }

                if (!_DeferredMousePressed) {
                    Screen? screen = Screen;
                    Game.Verify(screen != null, "GUI object must be on a screen for mouse press");
                    screen!._GlobalMousePressed(button);
                    _MousePressed(button);
                }
            }

            public void StartDragging()
            {
                if (_IsDraggable && !_IsDragged && _IsPressed && _PressedButton == MouseButton.Left && !IsDragged) {
                    _IsDragged = true;
                    _DragChanged();
                }
            }

            public virtual void _MousePressed(MouseButton button)
            {
                OnMousePressed(button);
                if (button == MouseButton.Left) {
                    OnLMousePressed();
                }
                else if (button == MouseButton.Right) {
                    OnRMousePressed();
                }

                _MousePressedUnder(button);
            }

            public virtual void _MousePressedUnder(MouseButton button)
            {
                Object? parent = Parent;
                if (parent != null) {
                    parent._MousePressedUnder(button);
                }
            }

            public void _MouseUp(bool lost)
            {
                _IsPressed = false;

                if (_DeferredMousePressed) {
                    Screen? screen = Screen;
                    Game.Verify(screen != null, "GUI object must be on a screen for mouse release");
                    screen!._GlobalMousePressed(_PressedButton);
                    _MousePressed(_PressedButton);
                }

                OnMouseUp(_PressedButton, lost);

                if (_IsDragged) {
                    _IsDragged = false;
                    _DragChanged();
                }
            }

            public virtual void MouseClick(MouseButton button)
            {
                OnMouseClick(button);
                if (button == MouseButton.Left) {
                    OnLMouseClick();
                }
                else if (button == MouseButton.Right) {
                    OnRMouseClick();
                }
            }

            public void _MouseMove()
            {
                OnMouseMove();
            }

            public void _GlobalMouseDown(MouseButton button)
            {
                OnGlobalMouseDown(button);
                for (int i = 0; i < _Children.Count; i++) {
                    _Children[i]._GlobalMouseDown(button);
                }
            }

            public void _GlobalMouseUp(MouseButton button)
            {
                OnGlobalMouseUp(button);
                for (int i = 0; i < _Children.Count; i++) {
                    _Children[i]._GlobalMouseUp(button);
                }
            }

            public void _GlobalMousePressed(MouseButton button)
            {
                OnGlobalMousePressed(button);
                for (int i = 0; i < _Children.Count; i++) {
                    _Children[i]._GlobalMousePressed(button);
                }
            }

            public virtual void _GlobalMouseClick(MouseButton button)
            {
                OnGlobalMouseClick(button);
                for (int i = 0; i < _Children.Count; i++) {
                    _Children[i]._GlobalMouseClick(button);
                }
            }

            public virtual void _GlobalMouseMove()
            {
                OnGlobalMouseMove();
                for (int i = 0; i < _Children.Count; i++) {
                    _Children[i]._GlobalMouseMove();
                }
            }

            public virtual void Input(KeyCode key, string text)
            {
                OnInput();
                if (text.Length == 0) {
                    OnInput(key);
                }
                else {
                    OnInput(text);
                }
                OnInput(key, text);
            }

            public void _GlobalInput(KeyCode key, string text)
            {
                OnGlobalInput(key, text);
                for (int i = 0; i < _Children.Count; i++) {
                    _Children[i]._GlobalInput(key, text);
                }
            }

            public virtual void _Focus()
            {
                if (!_IsFocused) {
                    _IsFocused = true;
                    OnFocusChanged();
                }
            }

            public virtual void _Unfocus()
            {
                if (_IsFocused) {
                    _IsFocused = false;
                    OnFocusChanged();
                }
            }

            public virtual void _Hover()
            {
                if (!_IsHovered) {
                    _IsHovered = true;
                    OnHoverChanged();
                }
            }

            public virtual void _Unhover()
            {
                if (_IsHovered) {
                    _IsHovered = false;
                    OnHoverChanged();
                }
            }

            public Object _Clone(Object parent)
            {
                Object newObject;
                newObject = Reflection.Clone(this);
                newObject._FixClone();
                newObject._Parent = parent;
                parent._Children.Add(newObject);
                newObject.OnConstruct();
                for (int i = 0; i < _Children.Count; i++) {
                    _Children[i]._Clone(newObject);
                }
                return newObject;
            }

            public Panel? FindPanel(string name)
            {
                return (Find(name) as Panel)!;
            }

            public Text? FindText(string name)
            {
                return (Find(name) as Text)!;
            }

            public TextInput? FindTextInput(string name)
            {
                return (Find(name) as TextInput)!;
            }

            public Button? FindButton(string name)
            {
                return (Find(name) as Button)!;
            }

            public Object? Find(string name, bool deepFind = true, int skip = 0)
            {
                return _Find(name, deepFind, ref skip);
            }

            public Object? _Find(in string name, bool deepFind, ref int skip)
            {
                int len = _Children.Count;
                for (int i = 0; i < len; i++) {
                    Object child = _Children[i];
                    if (child._Name == name) {
                        if (skip <= 0) {
                            return child;
                        }
                        else {
                            skip--;
                        }
                    }
                    if (deepFind) {
                        Object? obj = child._Find(name, deepFind, ref skip);
                        if (obj != null) {
                            return obj;
                        }
                    }
                }
                return null;
            }

            public int Count(string name, bool deepCount = true)
            {
                int result = 0;
                _Count(name, deepCount, ref result);
                return (int)(result);
            }

            public void _Count(in string name, bool deepCount, ref int result)
            {
                int len = _Children.Count;
                for (int i = 0; i < len; i++) {
                    Object child = _Children[i];
                    if (child._Name == name) {
                        result++;
                    }
                    if (deepCount) {
                        child._Count(name, deepCount, ref result);
                    }
                }
            }

            public Object GetChild(int index)
            {
                return _Children[index];
            }

            public void _RefreshPosition()
            {
                // Base data
                _Size = _BaseSize;
                Object? parent = _Parent;
                ipos parentAbsolutePos = parent != null ? parent._AbsolutePos : default(ipos);
                isize parentSize = parent != null ? parent._Size : new isize((int)(Settings.View_ScreenWidth), (int)(Settings.View_ScreenHeight));
                isize parentBaseSize = parent != null ? parent._BaseSize : _BaseSize;

                // Dock
                ipos newPos = default;
                if (_Dock != DockStyle.None) {
                    if (_Dock == DockStyle.Left) {
                        newPos = parentAbsolutePos;
                        _Size.height = parentSize.height;
                    }
                    else if (_Dock == DockStyle.Right) {
                        newPos.x = parentAbsolutePos.x + parentSize.width - _Size.width;
                        newPos.y = parentAbsolutePos.y;
                        _Size.height = parentSize.height;
                    }
                    else if (_Dock == DockStyle.Top) {
                        newPos = parentAbsolutePos;
                        _Size.width = parentSize.width;
                    }
                    else if (_Dock == DockStyle.Bottom) {
                        newPos.x = parentAbsolutePos.x;
                        newPos.y = parentAbsolutePos.y + parentSize.height - _Size.height;
                        _Size.width = parentSize.width;
                    }
                    else if (_Dock == DockStyle.Fill) {
                        newPos.x = parentAbsolutePos.x;
                        newPos.y = parentAbsolutePos.y;
                        _Size.width = parentSize.width;
                        _Size.height = parentSize.height;
                    }
                    else {
                        newPos = _BasePos + parentAbsolutePos;
                    }
                }
                // Anchor
                else {
                    if ((_Anchor & AnchorStyle.Left) != 0) {
                        newPos.x = parentAbsolutePos.x + _BasePos.x;
                    }
                    else if ((_Anchor & AnchorStyle.Right) != 0) {
                        newPos.x = parentAbsolutePos.x + _BasePos.x + (parentSize.width - parentBaseSize.width);
                    }
                    else {
                        newPos.x = parentAbsolutePos.x + _BasePos.x + (parentSize.width - parentBaseSize.width) / 2;
                    }

                    if ((_Anchor & AnchorStyle.Top) != 0) {
                        newPos.y = parentAbsolutePos.y + _BasePos.y;
                    }
                    else if ((_Anchor & AnchorStyle.Bottom) != 0) {
                        newPos.y = parentAbsolutePos.y + _BasePos.y + (parentSize.height - parentBaseSize.height);
                    }
                    else {
                        newPos.y = parentAbsolutePos.y + _BasePos.y + (parentSize.height - parentBaseSize.height) / 2;
                    }
                }

                // Move control
                if (newPos != _AbsolutePos) {
                    _Move(newPos - _AbsolutePos, false, false);
                }
            }

            public virtual void _SizeChanged()
            {
                // Internal callback
            }

            // Options
            public void SetName(string name)
            {
                _Name = name;
            }

            public void SetActive(bool active)
            {
                if (_ActiveSelf != active) {
                    _ActiveSelf = active;
                    Object? parent = _Parent;
                    _Active = _ActiveSelf && (parent == null || parent._Active);
                    _ActiveChanged();
                }
            }

            public virtual void _ActiveChanged()
            {
                OnActiveChanged();
                for (int i = 0; i < _Children.Count; i++) {
                    _Children[i]._Active = _Children[i]._ActiveSelf && _Active;
                    _Children[i]._ActiveChanged();
                }
            }

            public void _RefreshActive()
            {
                Object? parent = _Parent;
                bool newActive = _ActiveSelf && (parent == null || parent._Active);
                if (_Active == newActive) {
                    return;
                }
                _Active = newActive;
                for (int i = 0; i < _Children.Count; i++) {
                    _Children[i]._RefreshActive();
                }
            }

            public void SetPosition(ipos pos)
            {
                if (_BasePos == pos) {
                    return;
                }

                _BasePos = pos;
                _RefreshPosition();
            }

            public void SetPosition(int x, int y)
            {
                SetPosition(new ipos(x, y));
            }

            public void SetSize(isize size)
            {
                if (_BaseSize == size) {
                    return;
                }

                _BaseSize = size;
                _RefreshPosition();
                _SizeChanged();
            }

            public void SetSize(int width, int height)
            {
                SetSize(new isize(width, height));
            }

            public void SetAnchor(AnchorStyle anchorStyles)
            {
                if (_Anchor == anchorStyles) {
                    return;
                }

                _Anchor = anchorStyles;
                _RefreshPosition();
            }

            public void SetDock(DockStyle dockStyle)
            {
                if (_Dock == dockStyle) {
                    return;
                }

                _Dock = dockStyle;
                _RefreshPosition();
            }

            public void SetColor(ucolor color)
            {
                _Color = color;
            }

            public void SetDraggable(bool enabled)
            {
                _IsDraggable = enabled;
            }

            public void SetNotHittable(bool enabled)
            {
                _IsNotHittable = enabled;
            }

            public void SetNotCatchable(bool enabled)
            {
                _IsNotCatchable = enabled;
            }

            public void SetCheckTransparentOnHit(bool enabled)
            {
                _CheckTransparentOnHit = enabled;
            }

            public void SetFocusGroup(bool enabled)
            {
                _FocusGroup = enabled;
            }

            public void SetDeferredMousePressed(bool enabled)
            {
                _DeferredMousePressed = enabled;
            }

            public void SetHasOnDraw(bool enabled)
            {
                if (_HasOnDraw == enabled) {
                    return;
                }

                _HasOnDraw = enabled;
                _RefreshNeedDraw();
            }

        }

        public const int PanelScrollAnimationDurationMs = 120;

        public partial class Panel : Object
        {
            public Sprite? BackgroundImage
            {
                get
                {
                    return _BackgroundImage;
                }
            }

            public SpriteLayout BackgroundImageLayout
            {
                get
                {
                    Sprite? backgroundImage = _BackgroundImage;
                    return backgroundImage != null ? backgroundImage.Layout : SpriteLayout.None;
                }
            }

            public bool IsVerticalScroll
            {
                get
                {
                    return _IsVerticalScroll;
                }
            }

            public bool IsHorizontalScroll
            {
                get
                {
                    return _IsHorizontalScroll;
                }
            }

            public int VerticalScrollPercent
            {
                get
                {
                    return _VerticalScrollPercent;
                }
            }

            public int HorizontalScrollPercent
            {
                get
                {
                    return _HorizontalScrollPercent;
                }
            }

            public int VerticalScrollValue
            {
                get
                {
                    return _VerticalScroll;
                }
            }

            public int HorizontalScrollValue
            {
                get
                {
                    return _HorizontalScroll;
                }
            }

            public int VerticalScrollRange
            {
                get
                {
                    int maxVerScroll = 0;
                    int maxHorScroll = 0;
                    _GetScrollableSize(ref maxVerScroll, ref maxHorScroll);
                    return maxVerScroll;
                }
            }

            public int HorizontalScrollRange
            {
                get
                {
                    int maxVerScroll = 0;
                    int maxHorScroll = 0;
                    _GetScrollableSize(ref maxVerScroll, ref maxHorScroll);
                    return maxHorScroll;
                }
            }

            public Sprite? _BackgroundImage;
            public bool _CropContent;
            public bool _IsVerticalScroll;
            public bool _IsHorizontalScroll;
            public int _VerticalScroll;
            public int _HorizontalScroll;
            public int _VerticalScrollPercent;
            public int _HorizontalScrollPercent;
            public Tween.ValueTween _VerticalScrollTween = default!;
            public Tween.ValueTween _HorizontalScrollTween = default!;

            public override void _Construct()
            {
                base._Construct();
                _ResetScrollTweens();
            }

            public override void _FixClone()
            {
                Sprite? backgroundImage = _BackgroundImage;

                base._FixClone();

                _BackgroundImage = _CloneImage(backgroundImage);
                _ResetScrollTweens();
            }

            public override void _Remove()
            {
                base._Remove();
                _DisposeImage(ref _BackgroundImage);
            }

            public void _ResetScrollTweens()
            {
                _VerticalScrollTween = new Tween.ValueTween();
                _HorizontalScrollTween = new Tween.ValueTween();
                _VerticalScrollTween.SnapTo(_VerticalScroll);
                _HorizontalScrollTween.SnapTo(_HorizontalScroll);
            }

            public override bool _IsHitSelf(ipos pos)
            {
                if (!base._IsHitSelf(pos)) {
                    return false;
                }
                Sprite? backgroundImage = _BackgroundImage;
                if (_CheckTransparentOnHit && backgroundImage != null) {
                    return Game.IsSpriteHit(backgroundImage.Id, pos - _AbsolutePos);
                }
                return true;
            }

            public override Object? FindHit(ipos pos)
            {
                if (!_ActiveSelf) {
                    return null;
                }

                // Cropping panel outside its rect cannot contain any hittable descendant
                if (_CropContent &&
                    (pos.x < _AbsolutePos.x || pos.x >= _AbsolutePos.x + _Size.width || pos.y < _AbsolutePos.y || pos.y >= _AbsolutePos.y + _Size.height)) {
                    return null;
                }

                for (int i = _Children.Count - 1; i >= 0; i--) {
                    Object? obj = _Children[i].FindHit(pos);
                    if (obj != null) {
                        return obj;
                    }
                }

                if (_IsHitSelf(pos)) {
                    return this;
                }

                return null;
            }

            public override void _Draw(bool callCallback)
            {
                if (!_ActiveSelf || !_NeedDraw) {
                    return;
                }

                _RefreshSmoothScroll();

                bool crop = _CropContent;
                if (crop) {
                    Game.PushDrawScissor(AbsolutePos, Size);
                }

                if (callCallback) {
                    _DrawCallback();
                }

                // Normal sprite
                if (_BackgroundImage != null) {
                    _DrawImage(_BackgroundImage);
                }

                base._Draw(false);

                if (callCallback) {
                    OnPostDraw();
                }

                if (crop) {
                    Game.PopDrawScissor();
                }
            }

            public override bool _HasSelfDraw()
            {
                return _HasOnDraw || _BackgroundImage != null || _Grid != null;
            }

            public void _DrawImage(Sprite image)
            {
                if (_Size != new isize()) {
                    image.Draw(_AbsolutePos, _Size, -1, -1, SpriteLayout.None, _Color);
                }
            }

            public void SetBackgroundImage(string imageName, SpriteLayout imageLayout = SpriteLayout.None)
            {
                _SetImage(ref _BackgroundImage, imageName, imageLayout);
            }

            public void SetBackgroundImage(hstring imageName, SpriteLayout imageLayout = SpriteLayout.None)
            {
                _SetImage(ref _BackgroundImage, imageName, imageLayout);
            }

            public void _SetImage(ref Sprite? curImage, string imageName, SpriteLayout imageLayout)
            {
                _SetImage(ref curImage, imageName.hstr(), imageLayout);
            }

            public void _SetImage(ref Sprite? curImage, hstring imageName, SpriteLayout imageLayout)
            {
                if (curImage != null && curImage.LoadedName == imageName) {
                    return;
                }

                _DisposeImage(ref curImage);
                curImage = null;

                if (imageName.Value != 0) {
                    Sprite spr = new Sprite();
                    if (spr.Load(imageName)) {
                        curImage = spr;
                    }
                }

                if (curImage != null) {
                    curImage.Layout = imageLayout;
                }

                if (curImage != null && (imageLayout == SpriteLayout.None || _BaseSize == new isize())) {
                    SetSize(curImage.Size);
                }

                _RefreshNeedDraw();
            }

            public static void _DisposeImage(ref Sprite? image)
            {
                Sprite? oldImage = image;
                image = null;
                oldImage?.Dispose();
            }

            public static Sprite? _CloneImage(Sprite? image)
            {
                if (image == null || image.Id == 0 || image.LoadedName == default(hstring)) {
                    return null;
                }

                Sprite clonedImage = new Sprite();
                if (!clonedImage.Load(image.LoadedName)) {
                    return null;
                }

                clonedImage.Layout = image.Layout;
                clonedImage.Hidden = image.Hidden;
                clonedImage.Color = image.Color;
                return clonedImage;
            }

            public override void _MousePressedUnder(MouseButton button)
            {
                if (_IsVerticalScroll && (button == MouseButton.WheelUp || button == MouseButton.WheelDown)) {
                    int dy = 0;

                    if (button == MouseButton.WheelUp) {
                        dy = -1;
                    }
                    else if (button == MouseButton.WheelDown) {
                        dy = 1;
                    }

                    if (dy != 0) {
                        _ModifyScroll(dy, 0);
                    }

                    // Stop processing
                    return;
                }

                base._MousePressedUnder(button);
            }

            public void ModifyScroll(int ver, int hor)
            {
                _ModifyScroll(ver, hor);
            }

            public bool CanModifyScroll(int ver, int hor)
            {
                return _ModifyScroll(ver, hor, true);
            }

            public void SetScrollValue(int ver, int hor)
            {
                if (!_IsVerticalScroll && !_IsHorizontalScroll) {
                    return;
                }

                int newVer = (_IsVerticalScroll ? ver : 0);
                int newHor = (_IsHorizontalScroll ? hor : 0);

                if (newVer != _VerticalScroll || newHor != _HorizontalScroll || _VerticalScrollTween.Active || _HorizontalScrollTween.Active) {
                    _SetScrollValue(newVer, newHor, false, true);
                }
            }

            public void SetScrollPercent(int verPercent, int horPercent)
            {
                if (!_IsVerticalScroll && !_IsHorizontalScroll) {
                    return;
                }

                int maxVerScroll = 0, maxHorScroll = 0;
                _GetScrollableSize(ref maxVerScroll, ref maxHorScroll);

                int newVer = (maxVerScroll > 0 ? maxVerScroll * verPercent / 100 : 0);
                int newHor = (maxHorScroll > 0 ? maxHorScroll * horPercent / 100 : 0);

                if (newVer != _VerticalScroll || newHor != _HorizontalScroll || _VerticalScrollTween.Active || _HorizontalScrollTween.Active) {
                    _SetScrollValue(newVer, newHor, false, true);
                }
            }

            public int _GetAdaptiveScrollStep(int scrollRange)
            {
                int adaptiveStep = scrollRange / 20;
                return Math.Clamp(adaptiveStep, 10, 120);
            }

            public void _ClampScrollValue(ref int ver, ref int hor)
            {
                int maxVerScroll = 0, maxHorScroll = 0;
                _GetScrollableSize(ref maxVerScroll, ref maxHorScroll);

                if (_IsVerticalScroll) {
                    if (ver > maxVerScroll) {
                        ver = maxVerScroll;
                    }
                    if (ver < 0) {
                        ver = 0;
                    }
                }
                else {
                    ver = 0;
                }

                if (_IsHorizontalScroll) {
                    if (hor > maxHorScroll) {
                        hor = maxHorScroll;
                    }
                    if (hor < 0) {
                        hor = 0;
                    }
                }
                else {
                    hor = 0;
                }
            }

            public void _SetScrollTarget(int ver, int hor)
            {
                if (!_IsVerticalScroll && !_IsHorizontalScroll) {
                    return;
                }

                int targetVer = (_IsVerticalScroll ? ver : 0);
                int targetHor = (_IsHorizontalScroll ? hor : 0);
                _ClampScrollValue(ref targetVer, ref targetHor);

                if (_IsVerticalScroll) {
                    if (targetVer != _VerticalScrollTween.Target || !_VerticalScrollTween.Active) {
                        _VerticalScrollTween.MoveTo(_VerticalScroll, targetVer, PanelScrollAnimationDurationMs);
                    }

                    if (!_VerticalScrollTween.Active) {
                        _VerticalScrollTween.SnapTo(_VerticalScroll);
                    }
                }

                if (_IsHorizontalScroll) {
                    if (targetHor != _HorizontalScrollTween.Target || !_HorizontalScrollTween.Active) {
                        _HorizontalScrollTween.MoveTo(_HorizontalScroll, targetHor, PanelScrollAnimationDurationMs);
                    }

                    if (!_HorizontalScrollTween.Active) {
                        _HorizontalScrollTween.SnapTo(_HorizontalScroll);
                    }
                }
            }

            public void _RefreshSmoothScroll()
            {
                if (!_IsVerticalScroll && !_IsHorizontalScroll) {
                    return;
                }

                if (!_VerticalScrollTween.Active && !_HorizontalScrollTween.Active) {
                    _SetScrollValue(_VerticalScroll, _HorizontalScroll, false, true);
                    return;
                }

                int curVer = _VerticalScroll;
                int curHor = _HorizontalScroll;
                int targetVer = (_VerticalScrollTween.Active ? _VerticalScrollTween.Target : _VerticalScroll);
                int targetHor = (_HorizontalScrollTween.Active ? _HorizontalScrollTween.Target : _HorizontalScroll);

                _ClampScrollValue(ref targetVer, ref targetHor);

                if (_VerticalScrollTween.Active && targetVer != _VerticalScrollTween.Target) {
                    _VerticalScrollTween.MoveTo(_VerticalScroll, targetVer, PanelScrollAnimationDurationMs);
                }
                if (_HorizontalScrollTween.Active && targetHor != _HorizontalScrollTween.Target) {
                    _HorizontalScrollTween.MoveTo(_HorizontalScroll, targetHor, PanelScrollAnimationDurationMs);
                }

                if (_VerticalScrollTween.Active) {
                    curVer = _VerticalScrollTween.Update();
                }

                if (_HorizontalScrollTween.Active) {
                    curHor = _HorizontalScrollTween.Update();
                }

                _SetScrollValue(curVer, curHor, false, false);

                if (!_VerticalScrollTween.Active) {
                    _VerticalScrollTween.SnapTo(_VerticalScroll);
                }
                if (!_HorizontalScrollTween.Active) {
                    _HorizontalScrollTween.SnapTo(_HorizontalScroll);
                }
            }

            public bool _SetScrollValue(int ver, int hor, bool onlyCheck = false, bool syncTarget = true)
            {
                if (!_IsVerticalScroll && !_IsHorizontalScroll) {
                    return false;
                }

                int oldVer = _VerticalScroll;
                int oldHor = _HorizontalScroll;
                int curVer = (_IsVerticalScroll ? ver : 0);
                int curHor = (_IsHorizontalScroll ? hor : 0);

                _ClampScrollValue(ref curVer, ref curHor);

                int maxVerScroll = 0, maxHorScroll = 0;
                _GetScrollableSize(ref maxVerScroll, ref maxHorScroll);

                if (!onlyCheck || (ver == _VerticalScroll && hor == _HorizontalScroll)) {
                    _VerticalScrollPercent = (maxVerScroll > 0 ? curVer * 100 / maxVerScroll : 0);
                    _HorizontalScrollPercent = (maxHorScroll > 0 ? curHor * 100 / maxHorScroll : 0);
                }

                if (!onlyCheck && syncTarget) {
                    _VerticalScrollTween.SnapTo(curVer);
                    _HorizontalScrollTween.SnapTo(curHor);
                }

                if (curVer != oldVer || curHor != oldHor) {
                    if (!onlyCheck) {
                        _VerticalScroll = curVer;
                        _HorizontalScroll = curHor;
                        for (int i = 0; i < _Children.Count; i++) {
                            _Children[i]._Move(new ipos(-(curHor - oldHor), -(curVer - oldVer)), true, false);
                        }
                    }

                    return true;
                }

                return false;
            }

            public bool _ModifyScroll(int ver, int hor, bool onlyCheck = false)
            {
                if (!_IsVerticalScroll && !_IsHorizontalScroll) {
                    return false;
                }

                int maxVerScroll = 0, maxHorScroll = 0;
                _GetScrollableSize(ref maxVerScroll, ref maxHorScroll);

                int verStep = 0;
                int horStep = 0;

                if (_IsVerticalScroll && ver != 0) {
                    verStep = (ver < 0 ? -1 : 1) * _GetAdaptiveScrollStep(maxVerScroll);
                }
                if (_IsHorizontalScroll && hor != 0) {
                    horStep = (hor < 0 ? -1 : 1) * _GetAdaptiveScrollStep(maxHorScroll);
                }

                int baseVer = (_VerticalScrollTween.Active ? _VerticalScrollTween.Target : _VerticalScroll);
                int baseHor = (_HorizontalScrollTween.Active ? _HorizontalScrollTween.Target : _HorizontalScroll);
                int targetVer = baseVer + verStep;
                int targetHor = baseHor + horStep;
                _ClampScrollValue(ref targetVer, ref targetHor);

                if (onlyCheck) {
                    return targetVer != baseVer || targetHor != baseHor;
                }

                if (targetVer != baseVer || targetHor != baseHor) {
                    _SetScrollTarget(targetVer, targetHor);
                    return true;
                }

                return false;
            }

            public void _GetScrollableSize(ref int maxVerScroll, ref int maxHorScroll)
            {
                if (!_IsVerticalScroll && !_IsHorizontalScroll) {
                    return;
                }

                ipos centerPos = default;
                isize wholeSize = default;
                GetWholeSize(ref centerPos, ref wholeSize, true);

                if (_IsVerticalScroll) {
                    maxVerScroll = wholeSize.height - Size.height + ((centerPos.y + _VerticalScroll) - wholeSize.height / 2 - AbsolutePos.y);
                }
                if (_IsHorizontalScroll) {
                    maxHorScroll = wholeSize.width - Size.width + ((centerPos.x + _HorizontalScroll) - wholeSize.width / 2 - AbsolutePos.x);
                }
            }

            public void SetCropContent(bool enabled)
            {
                _CropContent = enabled;
            }

            public void SetAutoScroll(bool ver, bool hor)
            {
                _IsVerticalScroll = ver;
                _IsHorizontalScroll = hor;
            }

            protected void _MouseClickBase(MouseButton button)
            {
                base.MouseClick(button);
            }

        }

        public partial class Text : Object
        {
            public virtual string Text_
            {
                get
                {
                    return _Text;
                }
                set
                {
                    _Text = value;
                }
            }

            public FontType TextFont
            {
                get
                {
                    return _TextFont;
                }
            }

            public ucolor TextColor
            {
                get
                {
                    return _TextColor;
                }
            }

            public ucolor TextColorFocused
            {
                get
                {
                    return _TextColorFocused;
                }
            }

            public FontFlag TextFlags
            {
                get
                {
                    return _TextFlags;
                }
            }

            public string _Text = "";
            public FontType _TextFont;
            public ucolor _TextColor;
            public ucolor _TextColorFocused;
            public FontFlag _TextFlags;
            public int _TextSkipLines;

            public override void _Construct()
            {
                base._Construct();

                _TextFont = FontType.Default;
                _TextColor = global::FOnline.Color.Text;
                _TextFlags = FontFlag.None;
            }

            public override void _Draw(bool callCallback)
            {
                if (!_ActiveSelf || !_NeedDraw) {
                    return;
                }

                if (callCallback) {
                    _DrawCallback();
                }

                // Text_
                string text = Text_;
                if (text.Length > 0) {
                    ucolor color = (_IsFocused && _TextColorFocused != default(ucolor) ? _TextColorFocused : _TextColor);
                    Game.DrawText(text, _AbsolutePos, _Size, color, new TextFormat(_TextFont, _TextFlags, _TextSkipLines));
                }

                base._Draw(false);

                if (callCallback) {
                    OnPostDraw();
                }
            }

            public override bool _HasSelfDraw()
            {
                return true;
            }

            public void SetText(string text, FontType font, FontFlag flags)
            {
                if (text.Length != 0) {
                    _Text = text;
                }
                else {
                    _Text = "";
                }

                _TextFont = font;
                _TextFlags = flags;
            }

            public void SetText(string text)
            {
                if (text.Length != 0) {
                    _Text = text;
                }
                else {
                    _Text = "";
                }
            }

            public void SetTextWithResize(string text)
            {
                isize resultSize = default;
                int lines = 0;
                Game.GetTextInfo(text, new isize(Size.width, 1000), new TextFormat(_TextFont, _TextFlags, _TextSkipLines), ref resultSize, ref lines);
                SetSize(new isize(_Size.width, resultSize.height + 5));
                SetText(text);
            }

            public void SetTextFont(FontType font)
            {
                _TextFont = font;
            }

            public void SetTextFlags(FontFlag flags)
            {
                _TextFlags = flags;
            }

            public void SetTextColor(ucolor color)
            {
                _TextColor = color;
            }

            public void SetTextFocusedColor(ucolor color)
            {
                _TextColorFocused = color;
            }

            protected void _InputBase(KeyCode key, string text)
            {
                base.Input(key, text);
            }

        }

        public static readonly timespan PasswordShowTime = Time.Milliseconds(1000);

        public partial class TextInput : Text
        {
            public int InputLength
            {
                get
                {
                    return _InputLength;
                }
            }

            public bool IsTextPassword
            {
                get
                {
                    return _IsTextPassword;
                }
            }

            public string PasswordChar
            {
                get
                {
                    return _PasswordChar;
                }
            }

            public int _InputLength;
            public bool _IsTextPassword;
            public string _PasswordChar = "";
            public nanotime _PasswordTime;
            public int _CarriagePos;

            public override void _Construct()
            {
                base._Construct();

                SetCarriage(false);
                _TextColorFocused = global::FOnline.Color.TextFocused;
                _IsNotCatchable = true;
            }

            public override void _Focus()
            {
                Game.SetScreenKeyboard(true);

                base._Focus();
            }

            public override void _Unfocus()
            {
                Console? curConsole = CurConsole;
                if (curConsole == null || !curConsole.Active) {
                    Game.SetScreenKeyboard(false);
                }

                base._Unfocus();
            }

            public override void _Draw(bool callCallback)
            {
                if (!_ActiveSelf || !_NeedDraw) {
                    return;
                }

                if (callCallback) {
                    _DrawCallback();
                }

                // Text_
                string text = _Text;
                if (_IsTextPassword && text.Length > 0) {
                    string rawText = text;
                    text = "";
                    for (int i = 0; i < rawText.Length; i++) {
                        text += _PasswordChar;
                    }
                    if (Game.FrameTime - _PasswordTime <= PasswordShowTime) {
                        text = text.Substring(0, text.Length - 1) + (rawText[text.Length - 1]) + text.Substring((text.Length - 1) + 1);
                    }
                }
                if (_CarriagePos != -1 && _IsFocused) {
                    if (_CarriagePos < 0) {
                        _CarriagePos = 0;
                    }
                    if (_CarriagePos > text.Length) {
                        _CarriagePos = text.Length;
                    }
                    text = text.Substring(0, _CarriagePos) + (Game.FrameTime.milliseconds % 800 < 400 ? "!" : ".") + text.Substring(_CarriagePos);
                }
                if (text.Length > 0) {
                    ucolor color = (_IsFocused && _TextColorFocused != default(ucolor) ? _TextColorFocused : _TextColor);
                    Game.DrawText(text, _AbsolutePos, _Size, color, new TextFormat(_TextFont, _TextFlags, _TextSkipLines));
                }

                _DrawBase(false);

                if (callCallback) {
                    OnPostDraw();
                }
            }

            public override void Input(KeyCode key, string text)
            {
                int oldLen = _Text.Length;
                _ProcessKey(key, text);
                while (_InputLength != 0 && _Text.Length > _InputLength) {
                    _Text = _Text.Substring(0, _Text.Length - 1);
                }
                if (_IsTextPassword) {
                    _PasswordTime = (_Text.Length > oldLen ? Game.FrameTime : default(nanotime));
                }

                base.Input(key, text);
            }

            public void _ProcessKey(KeyCode key, string text)
            {
                if (global::FOnline.Input.IsCtrlDown() && !global::FOnline.Input.IsShiftDown() && !global::FOnline.Input.IsAltDown()) {
                    if (key == KeyCode.C || key == KeyCode.X) {
                        Game.SetClipboardText(_Text);

                        if (key == KeyCode.X) {
                            _Text = "";
                        }
                    }
                }

                if (_CarriagePos != -1) {
                    if (_CarriagePos < 0) {
                        _CarriagePos = 0;
                    }
                    else if (_CarriagePos > _Text.Length) {
                        _CarriagePos = _Text.Length;
                    }

                    if (key == KeyCode.Back) {
                        if (_CarriagePos > 0) {
                            _Text = _Text.Substring(0, _CarriagePos - 1) + ("") + _Text.Substring((_CarriagePos - 1) + 1);
                            _CarriagePos--;
                        }
                    }
                    else if (key == KeyCode.Delete) {
                        if (_CarriagePos < _Text.Length) {
                            _Text = _Text.Substring(0, _CarriagePos) + ("") + _Text.Substring((_CarriagePos) + 1);
                        }
                    }
                    else if (key == KeyCode.Right) {
                        if (_CarriagePos < _Text.Length) {
                            _CarriagePos++;
                        }
                    }
                    else if (key == KeyCode.Left) {
                        if (_CarriagePos > 0) {
                            _CarriagePos--;
                        }
                    }
                    else if (key == KeyCode.Home) {
                        _CarriagePos = 0;
                    }
                    else if (key == KeyCode.End) {
                        _CarriagePos = _Text.Length;
                    }
                    else if (text.Length != 0) {
                        _Text = _Text.Substring(0, _CarriagePos) + text + _Text.Substring(_CarriagePos);
                        _CarriagePos += text.Length;
                    }
                }
                else {
                    if (key == KeyCode.Back) {
                        if (_Text.Length > 0) {
                            _Text = _Text.Substring(0, _Text.Length - 1) + ("") + _Text.Substring((_Text.Length - 1) + 1);
                        }
                    }
                    else if (text.Length != 0) {
                        _Text += text;
                    }
                }
            }

            public void SetInputLength(int length)
            {
                _InputLength = length;
            }

            public void SetInputPassword(string passwordChar)
            {
                _IsTextPassword = (passwordChar.Length == 1);
                _PasswordChar = passwordChar;
                _PasswordTime = default(nanotime);
            }

            public void SetCarriage(bool enable)
            {
                _CarriagePos = (enable ? _Text.Length : -1);
            }

        }

        public partial class Button : Panel
        {
            public bool IsDisabled
            {
                get
                {
                    return _IsDisabled;
                }
            }

            public bool IsSwitched
            {
                get
                {
                    return _IsSwitched;
                }
            }

            public Sprite? PressedImage
            {
                get
                {
                    return _PressedImage;
                }
            }

            public SpriteLayout PressedImageLayout
            {
                get
                {
                    Sprite? image = _PressedImage;
                    return image != null ? image.Layout : SpriteLayout.None;
                }
            }

            public Sprite? HoverImage
            {
                get
                {
                    return _HoverImage;
                }
            }

            public SpriteLayout HoverImageLayout
            {
                get
                {
                    Sprite? image = _HoverImage;
                    return image != null ? image.Layout : SpriteLayout.None;
                }
            }

            public Sprite? DisabledImage
            {
                get
                {
                    return _DisabledImage;
                }
            }

            public SpriteLayout DisabledImageLayout
            {
                get
                {
                    Sprite? image = _DisabledImage;
                    return image != null ? image.Layout : SpriteLayout.None;
                }
            }

            public bool _IsDisabled;
            public bool _IsSwitched;
            public Sprite? _PressedImage;
            public Sprite? _HoverImage;
            public Sprite? _DisabledImage;

            public override void _Construct()
            {
                base._Construct();

                _DeferredMousePressed = true;
                _IsNotCatchable = true;
            }

            public override void _FixClone()
            {
                Sprite? pressedImage = _PressedImage;
                Sprite? hoverImage = _HoverImage;
                Sprite? disabledImage = _DisabledImage;

                base._FixClone();

                _PressedImage = _CloneImage(pressedImage);
                _HoverImage = _CloneImage(hoverImage);
                _DisabledImage = _CloneImage(disabledImage);
            }

            public override void _Draw(bool callCallback)
            {
                if (!_ActiveSelf || !_NeedDraw) {
                    return;
                }

                if (callCallback) {
                    _DrawCallback();
                }

                if (!_IsDisabled) {
                    // Pressed image
                    bool isPressed = ((_IsPressed && _PressedButton == MouseButton.Left) || _IsSwitched);
                    if (isPressed && _PressedImage != null) {
                        _DrawImage(_PressedImage);
                    }
                    // Hover image
                    else if (_IsHovered && _HoverImage != null) {
                        _DrawImage(_HoverImage);
                    }
                    // Normal image
                    else if (_BackgroundImage != null) {
                        _DrawImage(_BackgroundImage);
                    }
                }
                else {
                    // Disabled image
                    if (_DisabledImage != null) {
                        _DrawImage(_DisabledImage);
                    }
                    // Normal image
                    else if (_BackgroundImage != null) {
                        _DrawImage(_BackgroundImage);
                    }
                }

                _DrawBase(false);

                if (callCallback) {
                    OnPostDraw();
                }
            }

            public override bool _HasSelfDraw()
            {
                return _HasOnDraw || _BackgroundImage != null || _PressedImage != null || _HoverImage != null || _DisabledImage != null || _Grid != null;
            }

            public override void _Remove()
            {
                base._Remove();
                _DisposeImage(ref _PressedImage);
                _DisposeImage(ref _HoverImage);
                _DisposeImage(ref _DisabledImage);
            }

            public override void MouseClick(MouseButton button)
            {
                if (_IsDisabled) {
                    return;
                }

                base.MouseClick(button);
            }

            public override void _MousePressed(MouseButton button)
            {
                if (_IsDisabled) {
                    return;
                }

                base._MousePressed(button);
            }

            public void SetPressedImage(string imageName, SpriteLayout imageLayout = SpriteLayout.None)
            {
                _SetImage(ref _PressedImage, imageName, imageLayout);
            }

            public void SetPressedImage(hstring imageName, SpriteLayout imageLayout = SpriteLayout.None)
            {
                _SetImage(ref _PressedImage, imageName, imageLayout);
            }

            public void SetHoverImage(string imageName, SpriteLayout imageLayout = SpriteLayout.None)
            {
                _SetImage(ref _HoverImage, imageName, imageLayout);
            }

            public void SetHoverImage(hstring imageName, SpriteLayout imageLayout = SpriteLayout.None)
            {
                _SetImage(ref _HoverImage, imageName, imageLayout);
            }

            public void SetDisabledImage(string imageName, SpriteLayout imageLayout = SpriteLayout.None)
            {
                _SetImage(ref _DisabledImage, imageName, imageLayout);
            }

            public void SetDisabledImage(hstring imageName, SpriteLayout imageLayout = SpriteLayout.None)
            {
                _SetImage(ref _DisabledImage, imageName, imageLayout);
            }

            public void SetSwitch(bool enabled)
            {
                _IsSwitched = enabled;
            }

            public void SetCondition(bool enabled)
            {
                _IsDisabled = !enabled;
            }

        }

        public partial class CheckBox : Button
        {
            public bool IsChecked
            {
                get
                {
                    return _IsSwitched;
                }
            }

            // Callbacks
            public virtual void OnCheckedChanged()
            {
            }

            public override void MouseClick(MouseButton button)
            {
                if (_IsDisabled) {
                    return;
                }

                if (button == MouseButton.Left) {
                    SetChecked(!_IsSwitched);
                }

                _MouseClickBase(button);
            }

            public virtual void SetChecked(bool @checked)
            {
                if (_IsSwitched != @checked) {
                    _IsSwitched = @checked;
                    OnCheckedChanged();
                }
            }

        }

        public partial class RadioButton : CheckBox
        {
            public override void MouseClick(MouseButton button)
            {
                if (_IsDisabled || _IsSwitched) {
                    return;
                }

                if (button == MouseButton.Left) {
                    SetChecked(true);
                }

                _MouseClickBase(button);
            }

            public override void SetChecked(bool @checked)
            {
                Object? parent = _Parent;
                if (@checked && parent != null) {
                    for (int i = 0; i < parent._Children.Count; i++) {
                        RadioButton? button = (parent._Children[i] as RadioButton);
                        if (button != null && button._IsSwitched) {
                            button._IsSwitched = false;
                            button.OnCheckedChanged();
                        }
                    }
                }

                if (_IsSwitched != @checked) {
                    _IsSwitched = @checked;
                    OnCheckedChanged();
                }
            }

        }

        public partial class Screen : Panel
        {
            public GuiScreen Num
            {
                get
                {
                    return _Num;
                }
            }

            public bool IsModal
            {
                get
                {
                    return _IsModal;
                }
            }

            public bool IsMultiinstance
            {
                get
                {
                    return _IsMultiinstance;
                }
            }

            public bool IsCloseOnMiss
            {
                get
                {
                    return _IsCloseOnMiss;
                }
            }

            public List<CursorType> AvailableCursors
            {
                get
                {
                    return _AvailableCursors;
                }
            }

            public CursorType Cursor
            {
                get
                {
                    return _Cursor;
                }
            }

            public bool IsCanMove
            {
                get
                {
                    return _IsCanMove;
                }
            }

            public bool IsMoveIgnoreBorders
            {
                get
                {
                    return _IsMoveIgnoreBorders;
                }
            }

            public bool IsOnTop
            {
                get
                {
                    return _IsOnTop;
                }
            }

            public GuiScreen _Num;
            public bool _IsRegistered;
            public bool _IsModal;
            public bool _IsMultiinstance;
            public bool _IsCloseOnMiss;
            public List<CursorType> _AvailableCursors = default!;
            public CursorType _Cursor;
            public bool _IsCanMove;
            public bool _IsMoveIgnoreBorders;
            public bool _IsOnTop;

            // Workaround for input state accessing from shared code
            public List<bool> _InputKeyPressed = default!;
            public List<bool> _InputMousePressed = default!;

            public override void _Construct()
            {
                _AvailableCursors = new List<CursorType>();
                _InputKeyPressed = new List<bool>();
                _InputMousePressed = new List<bool>();

                base._Construct();
            }

            public override void _FixClone()
            {
                _AvailableCursors = _AvailableCursors.clone();

                base._FixClone();
            }

            public override void _Show(Dictionary<string, string> @params)
            {
                // Make screen active
                _ActiveSelf = true;
                _RefreshActive();

                // Set default cursor
                _Cursor = _AvailableCursors.Count > 0 ? AvailableCursors[0] : CursorType.Default;

                // Base behaviour
                base._Show(@params);
            }

            public override void _Hide()
            {
                // Make screen active
                _ActiveSelf = false;
                _RefreshActive();

                // Base behaviour
                base._Hide();
            }

            public override void _Appear()
            {
                // On top
                _IsOnTop = true;

                // Set screen cursor
                Gui.Cursor = _Cursor;

                // Base behaviour
                base._Appear();
            }

            public override void _Disappear()
            {
                // Not on top more
                _IsOnTop = false;

                // Store cursor
                _Cursor = Gui.Cursor;
                if (_AvailableCursors.IndexOf(_Cursor) == -1) {
                    _Cursor = _AvailableCursors.Count > 0 ? AvailableCursors[0] : CursorType.Default;
                }

                // Base behaviour
                base._Disappear();
            }

            public override void _GlobalMouseClick(MouseButton button)
            {
                if (button == MouseButton.Right) {
                    if (_AvailableCursors.Count > 0) {
                        CursorType curCursor = Gui.Cursor;
                        int curCursorIndex = _AvailableCursors.IndexOf(curCursor);
                        if (curCursorIndex != -1) {
                            curCursorIndex++;
                            if (curCursorIndex >= _AvailableCursors.Count) {
                                curCursorIndex = 0;
                            }
                            Gui.Cursor = _AvailableCursors[curCursorIndex];
                        }
                    }
                }
                else {
                    base._GlobalMouseClick(button);
                }
            }

            public override void _GlobalMouseMove()
            {
                // Process moving
                if (_IsCanMove) {
                    Object? pressedObj = _FindPressed(this);
                    if (pressedObj != null && pressedObj._PressedButton == MouseButton.Left && pressedObj._IsCatchable()) {
                        ipos lastPos = _AbsolutePos;
                        ipos newPos = _AbsolutePos + (Game.MousePos - pressedObj._PressedPos);
                        pressedObj._PressedPos = Game.MousePos;

                        // Check screen borders
                        if (!_IsMoveIgnoreBorders) {
                            Object? parent = _Parent;
                            ipos parentAbsolutePos = parent != null ? parent._AbsolutePos : default(ipos);
                            isize parentSize = parent != null ? parent._Size : new isize((int)(Settings.View_ScreenWidth), (int)(Settings.View_ScreenHeight));
                            ipos prevPos = newPos;

                            if (newPos.x < parentAbsolutePos.x) {
                                newPos.x = parentAbsolutePos.x;
                            }
                            if (newPos.y < parentAbsolutePos.y) {
                                newPos.y = parentAbsolutePos.y;
                            }
                            if (newPos.x + _Size.width > parentSize.width) {
                                newPos.x = parentSize.width - _Size.width;
                            }
                            if (newPos.y + _Size.height > parentSize.height) {
                                newPos.y = parentSize.height - _Size.height;
                            }

                            pressedObj._PressedPos += newPos - prevPos;
                        }

                        // Callback
                        if (lastPos != newPos) {
                            ipos deltaPos = newPos - lastPos;
                            _Move(deltaPos, true, true);
                        }
                    }
                }

                base._GlobalMouseMove();
            }

            public Object? _FindPressed(Object obj)
            {
                if (obj._IsPressed) {
                    return obj;
                }
                for (int i = 0; i < obj._Children.Count; i++) {
                    Object? pressedObj = _FindPressed(obj._Children[i]);
                    if (pressedObj != null) {
                        return pressedObj;
                    }
                }
                return null;
            }

            // Options
            public void SetModal(bool enabled)
            {
                _IsModal = enabled;
            }

            public void SetMultiinstance(bool enabled)
            {
                _IsMultiinstance = enabled;
            }

            public void SetCloseOnMiss(bool enabled)
            {
                _IsCloseOnMiss = enabled;
            }

            public void SetAvailableCursors(List<CursorType> cursors)
            {
                _AvailableCursors = cursors;
            }

            public void SetCanMove(bool enabled, bool ignoreBorders)
            {
                _IsCanMove = enabled;
                _IsMoveIgnoreBorders = ignoreBorders;
            }

        }

        public partial class Grid : Panel
        {
            public string CellPrototype
            {
                get
                {
                    return _CellPrototype;
                }
            }

            public int GridSize
            {
                get
                {
                    return _GridSize;
                }
            }

            public int Columns
            {
                get
                {
                    return _Columns;
                }
            }

            public ipos Padding
            {
                get
                {
                    return _Padding;
                }
            }

            public List<Object> Cells
            {
                get
                {
                    return _Cells;
                }
            }

            public string _CellPrototype = default!;
            public int _GridSize;
            public int _Columns;
            public ipos _Padding;
            public List<Object> _Cells = default!;

            public override void _Construct()
            {
                base._Construct();

                _Cells = new List<Object>();
            }

            public override void _FixClone()
            {
                _Cells = _Cells.clone();

                base._FixClone();
            }

            public override void _Init()
            {
                base._Init();

                if (_CellPrototype.Length != 0) {
                    SetCellPrototype(_CellPrototype);
                }

                if (_GridSize > 0) {
                    ResizeGrid(_GridSize);
                }
            }

            public virtual void ResizeGrid(int size)
            {
                // Refresh grid size
                _GridSize = size;

                // Find cell prototype
                if (_CellPrototype.Length == 0) {
                    return;
                }

                Object? parent = Parent;
                Object? cellPrototype = null;
                if (_CellPrototype.Substring(0, 1) != ".") {
                    cellPrototype = Find(_CellPrototype);
                }
                else if (parent != null) {
                    cellPrototype = parent.Find(_CellPrototype.Substring(1), false);
                }
                if (cellPrototype == null) {
                    return;
                }

                // Get cell index
                int childIndex = -1;
                Object? cellParent = cellPrototype._Parent;
                if (cellParent != null && cellParent == this) {
                    childIndex = cellParent._Children.IndexOf(cellPrototype);
                }

                // Remove current instances
                for (int i = 0; i < _Children.Count;) {
                    if (_Cells.IndexOf(_Children[i]) != -1) {
                        Object cell = _Children[i];
                        _Children.RemoveAt(i);
                        _Cells.Remove(cell);
                        _SetCellIndex(cell, null!, -1);
                        cell._Parent = null;
                        cell._Remove();
                        cell._ActiveSelf = false;
                        cell._RefreshActive();
                    }
                    else {
                        i++;
                    }
                }
                _Cells.Clear();

                // Create new intsances
                cellPrototype._ActiveSelf = true;
                cellPrototype._RefreshActive();
                List<Object> cellInstances = new List<Object>();

                for (int i = 0; i < _GridSize; i++) {
                    Object cellInstance = cellPrototype._Clone(this);
                    _Children.RemoveAt(_Children.Count - 1);
                    _Children.Insert(++childIndex, cellInstance);
                    _Cells.Add(cellInstance);
                    _SetCellIndex(cellInstance, this, cellInstances.Count);
                    cellInstances.Add(cellInstance);
                }

                cellPrototype._ActiveSelf = false;
                cellPrototype._RefreshActive();

                // Callbacks
                for (int i = 0; i < cellInstances.Count; i++) {
                    OnResizeGrid(cellInstances[i], i);
                    _ResizeGrid(cellInstances[i], cellInstances[i], i);
                }

                // Init
                for (int i = 0; i < cellInstances.Count; i++) {
                    cellInstances[i]._Init();
                }

                // Move
                int col = 0;
                int row = 0;
                int rowHeight = 0;
                int shiftX = -_HorizontalScroll;
                int shiftY = -_VerticalScroll;

                for (int i = 0; i < cellInstances.Count; i++) {
                    cellInstances[i]._Move(new ipos(shiftX, shiftY), false, true);
                    shiftX += cellInstances[i]._Size.width + _Padding.x;

                    if (rowHeight < cellInstances[i]._Size.height + _Padding.y) {
                        rowHeight = cellInstances[i]._Size.height + _Padding.y;
                    }

                    if (++col >= _Columns) {
                        col = 0;
                        row++;
                        shiftX = 0;
                        shiftY += rowHeight;
                        rowHeight = 0;
                    }
                }

                _ModifyScroll(0, 0);
            }

            public void RefreshContentPositions()
            {
                if (_Cells.Count == 0) {
                    return;
                }

                int col = 0;
                int row = 0;
                int rowHeight = 0;
                int shiftX = -_HorizontalScroll;
                int shiftY = -_VerticalScroll;
                int baseX = _Cells[0]._BasePos.x - _HorizontalScroll;
                int baseY = _Cells[0]._BasePos.y - _VerticalScroll;

                for (int i = 0; i < _Cells.Count; i++) {
                    int ox = shiftX - _Cells[i]._BasePos.x + baseX;
                    int oy = shiftY - _Cells[i]._BasePos.y + baseY;

                    if (ox != 0 || oy != 0) {
                        _Cells[i]._Move(new ipos(ox, oy), false, true);
                    }

                    shiftX += _Cells[i]._Size.width + _Padding.x;

                    if (rowHeight < _Cells[i]._Size.height + _Padding.y) {
                        rowHeight = _Cells[i]._Size.height + _Padding.y;
                    }

                    if (++col >= _Columns) {
                        col = 0;
                        row++;
                        shiftX = 0;
                        shiftY += rowHeight;
                        rowHeight = 0;
                    }
                }

                _ModifyScroll(0, 0);
            }

            public void _SetCellIndex(Object obj, Grid grid, int cellIndex)
            {
                obj._Grid = grid;
                obj._CellIndex = cellIndex;

                for (int i = 0; i < obj._Children.Count; i++) {
                    _SetCellIndex(obj._Children[i], grid, cellIndex);
                }

                obj._RefreshNeedDraw();
            }

            public void _ResizeGrid(Object obj, Object cell, int cellIndex)
            {
                obj.OnResizeGrid(cell, cellIndex);

                for (int i = 0; i < obj._Children.Count; i++) {
                    _ResizeGrid(obj._Children[i], cell, cellIndex);
                }
            }

            public void SetCellPrototype(string name)
            {
                _CellPrototype = name;

                if (_CellPrototype.Length != 0) {
                    Object? parent = Parent;
                    Object? cellPrototype = null;
                    if (_CellPrototype.Substring(0, 1) != ".") {
                        cellPrototype = Find(_CellPrototype);
                    }
                    else if (parent != null) {
                        cellPrototype = parent.Find(_CellPrototype.Substring(1), false);
                    }

                    if (cellPrototype != null) {
                        cellPrototype._ActiveSelf = false;
                        cellPrototype._RefreshActive();
                    }
                }
            }

            public void SetGridSize(int size)
            {
                _GridSize = size;
            }

            public void SetColumns(int length)
            {
                _Columns = length;
            }

            public void SetPadding(ipos pos)
            {
                _Padding = pos;
            }

            public void SetPadding(int x, int y)
            {
                SetPadding(new ipos(x, y));
            }

        }

        public partial class MessageBox : Text
        {
            public List<string> MessageTexts
            {
                get
                {
                    return _MessageTexts;
                }
            }

            public List<MessageBoxType> MessageTypes
            {
                get
                {
                    return _MessageTypes;
                }
            }

            public List<string> MessageTimes
            {
                get
                {
                    return _MessageTimes;
                }
            }

            public List<MessageBoxType> DisplayedMessages
            {
                get
                {
                    return _DisplayedMessages;
                }
            }

            public bool InvertMessages
            {
                get
                {
                    return _InvertMessages;
                }
            }

            public List<string> _MessageTexts = default!;
            public List<MessageBoxType> _MessageTypes = default!;
            public List<string> _MessageTimes = default!;
            public List<MessageBoxType> _DisplayedMessages = default!;
            public bool _InvertMessages;
            public int _Scroll;
            public int _MaxScroll;
            public int _ScrollLines;
            public Sprite _ScrollUp = default!;
            public Sprite _ScrollDown = default!;
            public bool _CursorHidden;

            public override void _Construct()
            {
                base._Construct();

                _MessageTexts = new List<string>();
                _MessageTypes = new List<MessageBoxType>();
                _MessageTimes = new List<string>();
                _DisplayedMessages = [MessageBoxType.All];
                _IsNotCatchable = true;

                _ScrollUp = new Sprite("MiniArrowUp.png".hstr());
                _ScrollDown = new Sprite("MiniArrowDown.png".hstr());
            }

            public override void _FixClone()
            {
                _MessageTexts = _MessageTexts.clone();
                _MessageTypes = _MessageTypes.clone();
                _MessageTimes = _MessageTimes.clone();
                _DisplayedMessages = _DisplayedMessages.clone();

                base._FixClone();
            }

            public override void _Show(Dictionary<string, string> @params)
            {
                base._Show(@params);

                _InvertMessages = Settings.Gui_MsgboxInvert;
                _GenerateText();
            }

            public override void _Draw(bool callCallback)
            {
                if (!_ActiveSelf || !_NeedDraw) {
                    return;
                }

                if (!_InvertMessages) {
                    _TextFlags = (FontFlag)(FontFlag.KeepTail | FontFlag.AlignBottom);
                    _TextSkipLines = _ScrollLines;
                }
                else {
                    _TextFlags = FontFlag.None;
                    _TextSkipLines = _ScrollLines;
                }

                base._Draw(callCallback);

                if (_IsHovered) {
                    Sprite spr = Game.MousePos.y < _AbsolutePos.y + _Size.height / 2 ? _ScrollUp : _ScrollDown;
                    spr.Draw(new ipos(Game.MousePos.x - spr.Size.width / 2, Game.MousePos.y - spr.Size.height / 2));
                }
            }

            public override void _Remove()
            {
                if (_CursorHidden) {
                    _CursorHidden = false;
                    Settings.Gui_HideCursor = false;
                }

                base._Remove();
            }

            public override void _Hover()
            {
                base._Hover();

                if (!Settings.Gui_HideCursor) {
                    _CursorHidden = true;
                    Settings.Gui_HideCursor = true;
                }
            }

            public override void _Unhover()
            {
                base._Unhover();

                if (_CursorHidden) {
                    _CursorHidden = false;
                    Settings.Gui_HideCursor = false;
                }
            }

            public override void _SizeChanged()
            {
                _GenerateText();

                base._SizeChanged();
            }

            public override void _ActiveChanged()
            {
                if (Active) {
                    _GenerateText();
                }

                base._ActiveChanged();
            }

            public override void _MousePressed(MouseButton button)
            {
                if (button == MouseButton.Left || button == MouseButton.WheelUp || button == MouseButton.WheelDown) {
                    if (button == MouseButton.WheelUp || (button == MouseButton.Left && Game.MousePos.y < _AbsolutePos.y + _Size.height / 2)) {
                        if (_InvertMessages && _Scroll > 0) {
                            _Scroll--;
                        }
                        if (!_InvertMessages && _Scroll < _MaxScroll) {
                            _Scroll++;
                        }
                    }
                    else if (button == MouseButton.WheelDown || (button == MouseButton.Left && Game.MousePos.y >= _AbsolutePos.y + _Size.height / 2)) {
                        if (_InvertMessages && _Scroll < _MaxScroll) {
                            _Scroll++;
                        }
                        if (!_InvertMessages && _Scroll > 0) {
                            _Scroll--;
                        }
                    }
                    _GenerateText();
                }

                base._MousePressed(button);
            }

            public void AddMessage(string text, MessageBoxType type = MessageBoxType.Default)
            {
                // Set text
                _MessageTexts.Add(text);
                _MessageTypes.Add(type);

                // Set time
                int year = 0;
                int month = 0;
                int day = 0;
                int hour = 0;
                int minute = 0;
                int second = 0;
                int millisecond = 0;
                int microsecond = 0;
                int nanosecond = 0;
                nanotime time = Game.GetPrecisionTime();
                Game.UnpackTime(time, ref year, ref month, ref day, ref hour, ref minute, ref second, ref millisecond, ref microsecond, ref nanosecond);

                string messageTime = (hour <= 9 ? "0" : "") + hour + ":" + (minute <= 9 ? "0" : "") + minute + ":" + (second <= 9 ? "0" : "") + second + " ";
                _MessageTimes.Add(messageTime);

                // Generate mess box
                if (_DisplayedMessages.IndexOf(type) != -1 || _DisplayedMessages.IndexOf(MessageBoxType.All) != -1) {
                    if (_Scroll > 0 && _IsHovered) {
                        _Scroll++;
                    }
                    else {
                        _Scroll = 0;
                    }
                }
                _GenerateText();
            }

            public void _GenerateText()
            {
                if (!Active) {
                    return;
                }

                _Text = "";
                if (_MessageTexts.Count == 0) {
                    return;
                }

                int maxLines = Game.GetTextLines(_Size, _TextFont);
                if (maxLines <= 0) {
                    _MaxScroll = 0;
                    _ScrollLines = 0;
                    return;
                }

                _ScrollLines = -1;
                int lines = 0;
                for (int i = _MessageTexts.Count - 1; i >= 0; i--) {
                    string messageText = _MessageTexts[i];
                    MessageBoxType messageType = _MessageTypes[i];
                    string messageTime = _MessageTimes[i];

                    // Skip if not need to display
                    if (_DisplayedMessages.IndexOf(messageType) == -1 && _DisplayedMessages.IndexOf(MessageBoxType.All) == -1) {
                        continue;
                    }

                    // Skip scrolled lines
                    int curLines = lines;
                    isize resultSize = default;
                    int skipLines = 0;
                    Game.GetTextInfo(messageText, new isize(_Size.width, 1000), new TextFormat(_TextFont, FontFlag.None, 0), ref resultSize, ref skipLines);
                    lines += skipLines;

                    if (_ScrollLines < 0) {
                        if (lines <= _Scroll) {
                            continue;
                        }
                        _ScrollLines = _Scroll - curLines;
                    }

                    if (curLines - _Scroll < maxLines) {
                        // Add to message box
                        if (_InvertMessages) {
                            _Text += messageText + "\n";
                        }
                        else {
                            _Text = messageText + "\n" + _Text;
                        }
                    }
                    else {
                        break;
                    }
                }
                _MaxScroll = lines - maxLines;
                if (_ScrollLines < 0) {
                    _ScrollLines = 0;
                }
            }

            public void SetDisplayedMessages(List<MessageBoxType> messageTypes)
            {
                _DisplayedMessages = messageTypes.clone();
                _GenerateText();
            }

            public void ChangeDisplayedMessage(MessageBoxType messageType, bool enable)
            {
                bool generateText = false;
                int curIndex = _DisplayedMessages.IndexOf(messageType);

                if (enable && curIndex == -1) {
                    _DisplayedMessages.Add(messageType);
                    generateText = true;
                }
                else if (!enable && curIndex != -1) {
                    _DisplayedMessages.RemoveAt(curIndex);
                    generateText = true;
                }

                if (generateText) {
                    _GenerateText();
                }
            }

            public void SetInvertMessages(bool invert)
            {
                _InvertMessages = invert;
                _GenerateText();
            }

            public void ClearMessages()
            {
                _MessageTexts.Clear();
                _MessageTypes.Clear();
                _MessageTimes.Clear();
                _GenerateText();
            }

        }

        public const string ConsoleDataPrefix = "Console";

        public partial class Console : TextInput
        {
            public bool DisableDeactivation
            {
                get
                {
                    return _DisableDeactivation;
                }
            }

            public string HistoryStorageName
            {
                get
                {
                    return _HistoryStorageName;
                }
            }

            public List<string> History
            {
                get
                {
                    return _History;
                }
            }

            public int HistoryMaxLength
            {
                get
                {
                    return _HistoryMaxLength;
                }
            }

            public bool _DisableDeactivation;
            public string _HistoryStorageName = default!;
            public string _HistoryActualStorageName = default!;
            public List<string> _History = default!;
            public int _HistoryMaxLength;
            public int _HistoryCur;

            public override void _Construct()
            {
                base._Construct();

                _History = new List<string>();

                SetCarriage(true);
                _HistoryStorageName = "Main";
            }

            public override void _FixClone()
            {
                _History = _History.clone();

                base._FixClone();
            }

            public override void _Show(Dictionary<string, string> @params)
            {
                CurConsole = this;

                base._Show(@params);
            }

            public override void _ActiveChanged()
            {
                Game.SetScreenKeyboard(Active);

                base._ActiveChanged();
            }

            public void Toggle()
            {
                if (!Active) {
                    // Activate console
                    Activate();
                }
                else if (_Text.Length == 0) {
                    // Deactivate console
                    if (!_DisableDeactivation) {
                        Deactivate();
                    }
                }
                else {
                    // Send text
                    SendText();
                }
            }

            public void Activate()
            {
                // Activate console
                Object? parent = Parent;
                if (Active || parent == null || !parent.Active) {
                    return;
                }

                // Load history
                string actualStorageName = "";
                if (_HistoryStorageName.Length != 0) {
                    actualStorageName = ConsoleDataPrefix + "_" + _HistoryStorageName;
                }

                if (_HistoryActualStorageName != actualStorageName) {
                    _History.Clear();
                    _HistoryActualStorageName = actualStorageName;
                    if (_HistoryActualStorageName != "") {
                        Serializator data = new Serializator();
                        if (data.LoadFromCache(_HistoryActualStorageName) > 0) {
                            data.Get(ref _History);
                        }
                    }
                }

                _HistoryCur = _History.Count;

                // Raise callbacks
                SetActive(true);
            }

            public void Deactivate()
            {
                // Deactivate console
                if (_ActiveSelf) {
                    SetActive(false);
                }
            }

            public void SendText()
            {
                if (!Active) {
                    return;
                }

                // Modify history
                _History.Add(_Text);
                for (int i = 0; i < _History.Count - 1;) {
                    if (_History[i] == _History[^1]) {
                        _History.RemoveAt(i);
                    }
                    else {
                        i++;
                    }
                }

                // Trim history length
                int historyMaxLength = (_HistoryMaxLength != 0 ? _HistoryMaxLength : Settings.Input_ConsoleHistorySize);
                while (_History.Count > historyMaxLength) {
                    _History.RemoveAt(0);
                }
                _HistoryCur = _History.Count;

                // Save history
                if (_HistoryActualStorageName != "") {
                    // Filter commands
                    List<string> filteredHistory = new List<string>();

                    for (int i = 0; i < _History.Count; i++) {
                        if (_History[i].StartsWith("~")) {
                            filteredHistory.Add(_History[i]);
                        }
                    }

                    Serializator data = new Serializator();
                    data.Set(filteredHistory);
                    data.SaveToCache(_HistoryActualStorageName);
                }

                // Send
                Game.OnConsoleMessage.Fire(_Text);

                // Clear text
                _Text = "";
                _CarriagePos = 0;
            }

            public void _ConsoleInput(KeyCode key, string text)
            {
                if (Active) {
                    if (key == KeyCode.Up && _HistoryCur > 0) {
                        _HistoryCur--;
                        _Text = _History[_HistoryCur];
                        _CarriagePos = _Text.Length;
                    }
                    else if (key == KeyCode.Down) {
                        if (_HistoryCur + 1 < _History.Count) {
                            _HistoryCur++;
                            _Text = _History[_HistoryCur];
                            _CarriagePos = _Text.Length;
                        }
                        else {
                            _HistoryCur = _History.Count;
                            _Text = "";
                            _CarriagePos = 0;
                        }
                    }
                    else {
                        base.Input(key, text);
                    }
                }

                if (key == KeyCode.Return || key == KeyCode.Numpadenter) {
                    Toggle();
                }
            }

            public override void Input(KeyCode key, string text)
            {
                _InputBase(key, text);
            }

            public override void _Draw(bool callCallback)
            {
                if (!_ActiveSelf || !_NeedDraw) {
                    return;
                }

                if (callCallback) {
                    _DrawCallback();
                }

                string text = Text_;

                if (_CarriagePos < 0) {
                    _CarriagePos = 0;
                }
                if (_CarriagePos > text.Length) {
                    _CarriagePos = text.Length;
                }

                text = text.Substring(0, _CarriagePos) + (Game.FrameTime.milliseconds % 800 < 400 ? "!" : ".") + text.Substring(_CarriagePos);
                Game.DrawText(text, _AbsolutePos, _Size, _TextColor, new TextFormat(_TextFont, _TextFlags, _TextSkipLines));

                _DrawBase(false);

                if (callCallback) {
                    OnPostDraw();
                }
            }

            public void SetDisableDeactivation(bool enable)
            {
                _DisableDeactivation = enable;
            }

            public void SetHistoryStorage(string storageName)
            {
                // Set storage name
                _HistoryStorageName = storageName;
            }

            public void SetHistoryMaxLength(int length)
            {
                _HistoryMaxLength = length;
            }

        }

        public partial class ItemView : Grid
        {
            public int UserData
            {
                get
                {
                    return _UserData;
                }
            }

            public int UserDataExt
            {
                get
                {
                    return _UserDataExt;
                }
            }

            public bool UseSorting
            {
                get
                {
                    return _UseSorting;
                }
            }

            public int _UserData;
            public int _UserDataExt;
            public bool _UseSorting;
            public List<Item?> _Items = default!;
            public int _ItemsGridSize;
            public bool _Dirty;

            public override void _Construct()
            {
                base._Construct();

                _Items = [];
                _IsNotCatchable = true;
            }

            public override void _FixClone()
            {
                _Items = _Items.clone();

                base._FixClone();
            }

            public override void _ActiveChanged()
            {
                if (_Dirty && Active) {
                    _Dirty = false;
                    Resort();
                }

                base._ActiveChanged();
            }

            // Callbacks
            public virtual List<Item>? OnGetItems()
            {
                return null;
            }

            public virtual int OnCheckItem(Item item)
            {
                return -1;
            }

            public override void _Init()
            {
                _ItemsGridSize = _GridSize;

                base._Init();
            }

            public Item? GetItem(int cellIndex)
            {
                Item? item = _Items[cellIndex];
                return item != null && !item.IsDestroyed ? item : null;
            }

            public bool HasItemId(ident itemId)
            {
                for (int i = 0; i < _Items.Count; i++) {
                    Item? item = _Items[i];
                    if (item != null && !item.IsDestroyed && item.Id == itemId) {
                        return true;
                    }
                }

                return false;
            }

            public bool HasSourceItemId(ident itemId)
            {
                List<Item>? items = OnGetItems();
                if (items == null) {
                    return false;
                }

                for (int i = 0; i < items.Count; i++) {
                    Item item = items[i];

                    if (item.Id == itemId) {
                        return true;
                    }
                }

                return false;
            }

            public void Resort()
            {
                if (Active) {
                    ResizeGrid(0);
                }
                else {
                    _Dirty = true;
                }
            }

            public override void ResizeGrid(int size)
            {
                List<Item?> newItems = new List<Item?>();

                List<Item>? maybeItems = OnGetItems();
                List<Item> items = maybeItems != null ? maybeItems : new List<Item>();

                for (int i = 0; i < items.Count; i++) {
                    Game.Verify(!items[i].IsDestroyed, "Item is destroyed");
                }

                if (!_UseSorting) {
                    newItems.resize(_ItemsGridSize);
                    for (int i = 0; i < items.Count; i++) {
                        int itemIndex = OnCheckItem(items[i]);
                        if (itemIndex >= 0 && itemIndex < newItems.Count) {
                            newItems[itemIndex] = items[i];
                        }
                    }
                }
                else {
                    List<int> sortValues = new List<int>();
                    for (int i = 0; i < items.Count; i++) {
                        int sortValue = OnCheckItem(items[i]);
                        if (sortValue >= 0) {
                            bool added = false;
                            for (int j = 0; j < newItems.Count; j++) {
                                if (sortValues[j] > sortValue) {
                                    newItems.Insert(j, items[i]);
                                    sortValues.Insert(j, sortValue);
                                    added = true;
                                    break;
                                }
                            }
                            if (!added) {
                                newItems.Add(items[i]);
                                sortValues.Add(sortValue);
                            }
                        }
                    }
                }

                bool isChanged = newItems.Count != _Items.Count;
                for (int i = 0; i < newItems.Count && !isChanged; i++) {
                    Item? newItem = newItems[i];
                    Item? oldItem = _Items[i];
                    isChanged = !AreSameItemHandles(newItem, oldItem);
                }

                if (isChanged) {
                    _Items = newItems;
                    base.ResizeGrid(_Items.Count);
                }
            }

            public void SetUserData(int data)
            {
                _UserData = data;
            }

            public void SetUserDataExt(int data)
            {
                _UserDataExt = data;
            }

            public void SetUseSorting(bool enable)
            {
                _UseSorting = enable;
            }

            public bool AreSameItemHandles(Item? first, Item? second)
            {
                if (first == null || second == null) {
                    return first == null && second == null;
                }

                if (first.IsDestroyed || second.IsDestroyed) {
                    return false;
                }

                return first.Id == second.Id;
            }

        }

        // Implementation

        public static Dictionary<GuiScreen, CreateScreenFunc> ScreenCreators = new Dictionary<GuiScreen, CreateScreenFunc>();

        public static List<Screen> Screens = new List<Screen>();

        public static Screen? CursorScreen;

        public static List<Object> FocusedObjects = new List<Object>();

        public static Object? PressedObject;

        public static nanotime PressedObjectRepeatTime;

        public static Object? LastPressedObject;

        public static Object? HoveredObject;

        public static List<DragAndDropHandler> DragAndDropHandlers = new List<DragAndDropHandler>();

        public static IDropMenu? DropMenu;

        public static Console? CurConsole;

        //
        // Public API
        //

        public static void RegisterScreen(GuiScreen screenNum, CreateScreenFunc screenFunc)
        {
            UnregisterScreen(screenNum);

            if (screenNum == GuiScreen.None) {
                return;
            }

            ScreenCreators[screenNum] = screenFunc;

            // Precache
            Screen screen = CreateScreen(screenNum);

            if (screen.IsMultiinstance) {
                screen._Remove();
                Screens.RemoveAt(Screens.IndexOf(screen));
            }
        }

        public static void ShowScreen(GuiScreen screenNum)
        {
            ShowScreen(screenNum, new Dictionary<string, string>());
        }

        public static void ShowScreen(GuiScreen screenNum, Dictionary<string, string> @params)
        {
            Settings.Hex_ScrollMouseUp = false;
            Settings.Hex_ScrollMouseRight = false;
            Settings.Hex_ScrollMouseDown = false;
            Settings.Hex_ScrollMouseLeft = false;
            Settings.Hex_ScrollKeybUp = false;
            Settings.Hex_ScrollKeybRight = false;
            Settings.Hex_ScrollKeybDown = false;
            Settings.Hex_ScrollKeybLeft = false;

            // Manage multiinstance
            for (int i = Screens.Count - 1; i >= 0; i--) {
                // Find instance
                Screen screenLocal = Screens[i];
                if (screenLocal.Num != screenNum) {
                    continue;
                }

                // Move to top created instance
                if (!screenLocal.IsMultiinstance) {
                    ShowHideScreen(screenLocal, @params, true);
                    return;
                }
            }

            // Create new instance
            Screen screen = CreateScreen(screenNum);
            ShowHideScreen(screen, @params, true);
        }

        public static void HideScreen()
        {
            Screen? screen = GetActiveScreen();

            if (screen != null) {
                HideScreen(screen.Num);
            }
        }

        public static void HideScreen(GuiScreen screenNum)
        {
            for (int i = Screens.Count - 1; i >= 0; i--) {
                // Find instance
                Screen screen = Screens[i];

                if (!screen.Active || screen.Num != screenNum) {
                    continue;
                }

                HideScreen(screen);

                // Hide only one screen per call
                break;
            }
        }

        public static void HideScreen(Screen screen)
        {
            if (screen.Active) {
                ShowHideScreen(screen, null!, false);
            }
        }

        public static void HideAllScreens()
        {
            while (true) {
                Screen? screen = GetActiveScreen();

                if (screen != null) {
                    HideScreen(screen);
                }
                else {
                    break;
                }
            }
        }

        public static void BringToFront(GuiScreen screenNum)
        {
            Screen? screen = GetScreen(screenNum);

            if (screen != null) {
                BringToFront(screen);
            }
        }

        public static void BringToFront(Screen screen)
        {
            if (!screen.Active) {
                return;
            }

            int topmostIndex = -1;

            for (int i = Screens.Count - 1; i >= 0; i--) {
                if (!Screens[i].Active) {
                    continue;
                }

                if (topmostIndex == -1) {
                    topmostIndex = i;
                }

                if (Screens[i] == screen) {
                    if (i == topmostIndex) {
                        break; // Already topmost
                    }

                    Screen topmost = Screens[topmostIndex];
                    Screens.RemoveAt(i);
                    Screens.Add(screen);

                    topmost._Disappear();
                    screen._Appear();
                    break;
                }
            }
        }

        public static Screen? GetScreen(GuiScreen screenNum)
        {
            Screen? lastMultiinstanceScreen = null;

            for (int i = Screens.Count - 1; i >= 0; i--) {
                Screen screen = Screens[i];

                if (screen.Num == screenNum) {
                    if (!screen.IsMultiinstance) {
                        return screen;
                    }
                    else {
                        lastMultiinstanceScreen = screen;
                    }
                }
            }

            return lastMultiinstanceScreen;
        }

        public static Screen? GetActiveScreen()
        {
            for (int i = Screens.Count - 1; i >= 0; i--) {
                Screen screen = Screens[i];

                if (screen.Active) {
                    return screen;
                }
            }

            return null;
        }

        public static Screen? GetActiveScreen(GuiScreen screenNum)
        {
            for (int i = Screens.Count - 1; i >= 0; i--) {
                Screen screen = Screens[i];

                if (screen.Num == screenNum && screen.Active) {
                    return screen;
                }
            }

            return null;
        }

        public static bool IsScreenActive(GuiScreen screenNum)
        {
            return GetActiveScreen(screenNum) != null;
        }

        public static List<Screen> GetActiveScreens()
        {
            List<Screen> result = new List<Screen>();

            for (int i = 0; i < Screens.Count; i++) {
                Screen screen = Screens[i];
                if (screen.Active) {
                    result.Add(screen);
                }
            }

            return result;
        }

        public static List<Object> GetFocusedObjects()
        {
            for (int i = FocusedObjects.Count - 1; i >= 0; i--) {
                if (!FocusedObjects[i].Active) {
                    FocusedObjects[i]._Unfocus();
                    FocusedObjects.RemoveAt(i);
                }
            }

            return FocusedObjects.clone();
        }

        public static void SetFocusedObject(Object? obj)
        {
            // Unfocus current
            List<Object> objects = GetFocusedObjects();
            FocusedObjects.Clear();

            for (int i = 0; i < objects.Count; i++) {
                objects[i]._Unfocus();
            }

            if (obj == null) {
                return;
            }

            // Focus new
            Object? groupObj = obj.Parent;

            while (groupObj != null) {
                Object? cursor = groupObj;
                if (cursor == null || cursor.FocusGroup) {
                    break;
                }

                groupObj = cursor.Parent;
            }

            if (groupObj != null) {
                AddFocusRecursively(groupObj);
            }
            else {
                obj._Focus();
                FocusedObjects.Add(obj);
            }
        }

        public static void AddFocusRecursively(Object obj)
        {
            obj._Focus();
            FocusedObjects.Add(obj);

            for (int i = 0; i < obj._Children.Count; i++) {
                AddFocusRecursively(obj._Children[i]);
            }
        }

        public static Object? GetPressedObject()
        {
            Object? pressedObject = PressedObject;
            if (pressedObject != null && !pressedObject.Active) {
                PressedObject = null;
                pressedObject._MouseUp(true);
                LastPressedObject = null;
            }
            return PressedObject;
        }

        public static Object? GetLastPressedObject()
        {
            Object? lastPressedObject = LastPressedObject;
            if (lastPressedObject != null && !lastPressedObject.Active) {
                LastPressedObject = null;
            }
            return LastPressedObject;
        }

        public static Object? GetDraggedObject()
        {
            Object? pressedObject = PressedObject;
            if (pressedObject != null && !pressedObject.Active) {
                PressedObject = null;
                pressedObject._MouseUp(true);
            }
            Object? draggedObject = PressedObject;
            return draggedObject != null && draggedObject._IsDragged ? draggedObject : null;
        }

        public static Object? GetHoveredObject()
        {
            Object? hoveredObject = HoveredObject;
            if (hoveredObject != null && !hoveredObject.Active) {
                HoveredObject = null;
                hoveredObject._Unhover();
            }
            return HoveredObject;
        }

        public static void NextTextInput()
        {
            Screen? screen = GetActiveScreen();
            if (screen == null) {
                return;
            }

            TextInput? curTextInput = null;
            List<Object> objects = GetFocusedObjects();

            for (int i = 0; i < objects.Count && curTextInput == null; i++) {
                curTextInput = (objects[i] as TextInput);
            }

            FindNextTextInputSkipObj = curTextInput;
            TextInput? textInput = FindNextTextInput(screen);
            FindNextTextInputSkipObj = null;

            if (textInput == null && curTextInput != null) {
                textInput = FindNextTextInput(screen);
            }

            if (textInput != null && textInput != curTextInput) {
                SetFocusedObject(textInput);
            }
        }

        public static Object? FindNextTextInputSkipObj = null;

        public static TextInput? FindNextTextInput(Object obj)
        {
            if (FindNextTextInputSkipObj == null) {
                TextInput? textInput = (obj as TextInput);
                if (textInput != null) {
                    return textInput;
                }
            }
            else if (obj == FindNextTextInputSkipObj) {
                FindNextTextInputSkipObj = null;
            }

            for (int i = 0; i < obj._Children.Count; i++) {
                if (obj._Children[i]._ActiveSelf) {
                    TextInput? textInput = FindNextTextInput(obj._Children[i]);
                    if (textInput != null) {
                        return textInput;
                    }
                }
            }
            return null;
        }

        public static List<MessageBox> CollectMessageBoxes()
        {
            List<MessageBox> messageBoxes = new List<MessageBox>();
            for (int i = Screens.Count - 1; i >= 0; i--) {
                CollectMessageBoxes(Screens[i], messageBoxes);
            }
            return messageBoxes;
        }

        public static void CollectMessageBoxes(Object obj, List<MessageBox> messageBoxes)
        {
            MessageBox? mb = (obj as MessageBox);
            if (mb != null) {
                messageBoxes.Add(mb);
            }

            for (int i = 0; i < obj._Children.Count; i++) {
                CollectMessageBoxes(obj._Children[i], messageBoxes);
            }
        }

        public static bool IsConsoleActive()
        {
            for (int i = Screens.Count - 1; i >= 0; i--) {
                if (IsConsoleActive(Screens[i])) {
                    return true;
                }
            }
            return false;
        }

        public static bool IsConsoleActive(Object obj)
        {
            if (!obj.ActiveSelf) {
                return false;
            }

            Console? console = (obj as Console);
            if (console != null) {
                return true;
            }

            for (int i = 0; i < obj._Children.Count; i++) {
                if (IsConsoleActive(obj._Children[i])) {
                    return true;
                }
            }

            return false;
        }

        public static void AddDragAndDropHandler(DragAndDropHandler handler)
        {
            DragAndDropHandlers.Add(handler);
        }

        public static void SetDropMenu(IDropMenu dropMenu)
        {
            DropMenu = dropMenu;
        }

        public static bool CheckHit(ipos pos)
        {
            for (int i = 0; i < Screens.Count; i++) {
                if (Screens[i].Active && Screens[i].FindHit(pos) != null) {
                    return true;
                }
            }
            return false;
        }

        public static bool IsModalScreenActive()
        {
            for (int i = Screens.Count - 1; i >= 0; i--) {
                if (Screens[i].Active && Screens[i].IsModal) {
                    return true;
                }
            }
            return false;
        }

        //
        // Engine callbacks
        //

        public static void EngineCallback_Start()
        {
            // Register custom screens
            GuiScreens.InitializeScreens();
        }

        public static void CleanupObjectGraph(Object obj)
        {
            for (int i = obj._Children.Count - 1; i >= 0; i--) {
                CleanupObjectGraph(obj._Children[i]);
            }

            obj.OnCleanup();

            Grid? grid = (obj as Grid);
            if (grid != null) {
                grid._Cells.Clear();
            }

            ItemView? itemView = (obj as ItemView);
            if (itemView != null) {
                itemView._Items.Clear();
            }

            Screen? screen = (obj as Screen);
            if (screen != null) {
                screen._AvailableCursors.Clear();
                screen._InputKeyPressed.Clear();
                screen._InputMousePressed.Clear();
                screen._IsRegistered = false;
            }

            obj._Grid = null!;
            obj._CellIndex = 0;
            obj._Parent = null!;
            obj._Children.Clear();
            obj._ActiveSelf = false;
            obj._Active = false;
        }

        public static void EngineCallback_Finish()
        {
            EngineCallback_InputLost();

            HideAllScreens();

            DropMenu = null;
            CurConsole = null;
            DragAndDropHandlers.Clear();
            ScreenCreators = [];

            Screen? cursorScreen = CursorScreen;
            if (cursorScreen != null) {
                cursorScreen.Remove();
                CleanupObjectGraph(cursorScreen);
                CursorScreen = null;
            }

            for (int i = Screens.Count - 1; i >= 0; i--) {
                Screen screen = Screens[i];
                screen.Remove();
                CleanupObjectGraph(screen);
            }

            Screens.Clear();
            FocusedObjects.Clear();
            PressedObject = null;
            LastPressedObject = null;
            HoveredObject = null;
            FindNextTextInputSkipObj = null;
        }

        public static void EngineCallback_Draw()
        {
            for (int i = 0; i < Screens.Count; i++) {
                Screen screen = Screens[i];

                if (screen.Active) {
                    screen._Draw(true);
                }
            }
        }

        public static void EngineCallback_DrawCursor()
        {
            // Dragged object
            Object? draggedObject = GetDraggedObject();
            if (draggedObject != null) {
                ipos pos = default;
                isize size = default;
                draggedObject.GetWholeSize(ref pos, ref size);
                draggedObject.Draw(new ipos(Game.MousePos.x - size.width / 2, Game.MousePos.y - size.height / 2));
                return;
            }

            // Drop menu
            IDropMenu? dropMenu = DropMenu;
            if (dropMenu != null) {
                dropMenu.Draw();
            }

            // Cursor
            Screen? cursorScreen = CursorScreen;
            if (cursorScreen != null) {
                cursorScreen._Draw(true);
            }
        }

        public static void EngineCallback_MouseDown(MouseButton button)
        {
            if (button == MouseButton.Left && FocusedObjects.Count != 0) {
                SetFocusedObject(null!);
            }

            // Release mouse from current object
            Object? pressedObject = PressedObject;
            if (pressedObject != null) {
                PressedObject = null;
                pressedObject._MouseUp(true);
            }

            // Process mouse down
            Screen? screen = GetActiveScreen();
            if (screen == null) {
                return;
            }

            // Global callback
            screen._GlobalMouseDown(button);

            // Drop menu
            IDropMenu? dropMenu = DropMenu;
            if (dropMenu != null) {
                dropMenu.MouseDown(button);
            }

            // Check hit on current screen
            Object? hitObj = screen.FindMouseHit();
            if (hitObj != null) {
                // Handle pressed object
                PressedObject = hitObj;
                PressedObjectRepeatTime = Game.FrameTime + Time.Milliseconds(500);
                LastPressedObject = hitObj;
                hitObj._MouseDown(button);

                // Handle focused object
                if (button == MouseButton.Left) {
                    SetFocusedObject(hitObj);
                }
                return;
            }

            // Close on miss
            if (button == MouseButton.Left && screen.IsCloseOnMiss) {
                ShowHideScreen(screen, null!, false);
                return;
            }

            // Switch to another screen
            if (button == MouseButton.Left && !screen.IsModal) {
                int screenIndex = Screens.IndexOf(screen);

                for (int i = screenIndex - 1; i >= 0; i--) {
                    Screen nextScreen = Screens[i];
                    if (!nextScreen.Active) {
                        continue;
                    }

                    if (nextScreen.IsModal || nextScreen.FindMouseHit() != null) {
                        // Show screen to top and click on it
                        ShowHideScreen(nextScreen, null!, true);
                        EngineCallback_MouseDown(button);
                        return;
                    }
                }
            }
        }

        public static void EngineCallback_MouseUp(MouseButton button)
        {
            // Global handler
            Screen? screen = GetActiveScreen();
            if (screen != null) {
                screen._GlobalMouseUp(button);
            }

            // Release mouse from current object
            GetPressedObject(); // Refresh state
            Object? pressed = PressedObject;
            if (pressed != null && button == pressed._PressedButton) {
                Object obj = pressed;
                PressedObject = null;
                bool isDragged = obj._IsDragged;
                obj._MouseUp(false);

                if (obj.IsMouseHit()) {
                    if (screen != null) {
                        screen._GlobalMouseClick(button);
                    }
                    obj.MouseClick(button);
                }

                if (isDragged && screen != null) {
                    Object? target = screen.FindMouseHit();
                    for (int i = 0; i < DragAndDropHandlers.Count; i++) {
                        if (DragAndDropHandlers[i](obj, target)) {
                            break;
                        }
                    }
                }
            }

            // Drop menu
            IDropMenu? dropMenu = DropMenu;
            if (dropMenu != null) {
                dropMenu.MouseUp(button);
            }
        }

        public static void EngineCallback_MouseMove(ipos offset)
        {
            bool realMove = offset != default(ipos);

            // Drop menu
            IDropMenu? dropMenu = DropMenu;
            if (dropMenu != null) {
                if (dropMenu.PreMouseMove(offset)) {
                    return;
                }
            }

            // Move mouse for active screen
            Screen? screen = GetActiveScreen();
            Object? hoveredObj = null;
            if (screen != null) {
                if (realMove) {
                    screen._GlobalMouseMove();
                }
                hoveredObj = screen.FindMouseHit();
                if (realMove && hoveredObj != null) {
                    hoveredObj._MouseMove();
                }
            }

            // Change hover object
            GetHoveredObject(); // Refresh state
            Object? currentHovered = HoveredObject;
            if (currentHovered != hoveredObj) {
                if (currentHovered != null) {
                    HoveredObject = null;
                    currentHovered._Unhover();
                }
                HoveredObject = hoveredObj;
                if (hoveredObj != null) {
                    hoveredObj._Hover();
                }
            }

            // Drop menu
            if (dropMenu != null) {
                dropMenu.PostMouseMove(offset);
            }
        }

        public static void EngineCallback_KeyDown(KeyCode key, string text)
        {
            // Global input handler
            for (int i = Screens.Count - 1; i >= 0; i--) {
                if (Screens[i].Active) {
                    Screens[i]._GlobalInput(key, text);
                }
            }

            // Console input
            Console? curConsole = CurConsole;
            if (curConsole != null) {
                curConsole._ConsoleInput(key, text);
            }

            // Focused object input
            if (curConsole == null || !curConsole.Active) {
                Screen? screen = GetActiveScreen();
                List<Object> objects = GetFocusedObjects();
                for (int i = 0; i < objects.Count; i++) {
                    if (objects[i] != screen) {
                        objects[i].Input(key, text);
                    }
                }
            }
        }

        public static void EngineCallback_KeyUp(KeyCode key)
        {
            // ...
        }

        public static void EngineCallback_InputLost()
        {
            Object? hoveredObject = HoveredObject;
            if (hoveredObject != null) {
                HoveredObject = null;
                hoveredObject._Unhover();
            }

            SetFocusedObject(null!);

            Object? pressedObject = PressedObject;
            if (pressedObject != null) {
                PressedObject = null;
                pressedObject._MouseUp(true);
            }

            IDropMenu? dropMenu = DropMenu;
            if (dropMenu != null) {
                dropMenu.InputLost();
            }
        }

        public static void EngineCallback_Loop()
        {
            if (Game.IsConnecting()) {
                global::FOnline.Input.ReleaseMouse();
            }

            GetPressedObject(); // Refresh state

            Object? pressedObject = PressedObject;
            if (pressedObject != null) {
                nanotime tick = Game.FrameTime;

                if (tick >= PressedObjectRepeatTime && pressedObject.IsMouseHit()) {
                    PressedObjectRepeatTime = tick + Time.Milliseconds(40);
                    Screen? screen = GetActiveScreen();
                    if (screen != null) {
                        screen._GlobalMousePressed(pressedObject._PressedButton);
                    }
                    pressedObject._MousePressed(pressedObject._PressedButton);
                }
            }

            EngineCallback_MouseMove(new ipos(0, 0));

            // Drop menu
            IDropMenu? dropMenu = DropMenu;
            if (dropMenu != null) {
                dropMenu.Loop();
            }
        }

        public static void RefreshItemViewsByItemId(ident itemId)
        {
            if (itemId.value == 0) {
                return;
            }

            List<ItemView> itemViews = new List<ItemView>();
            CollectItemView(itemViews);

            for (int i = 0; i < itemViews.Count; i++) {
                ItemView itemView = itemViews[i];

                if (itemView.HasItemId(itemId) || itemView.HasSourceItemId(itemId)) {
                    itemView.Resort();
                }
            }
        }

        public static void RefreshItemViewsByUserDataExt(int userDataExt)
        {
            RefreshItemViewsByUserDataExts([userDataExt]);
        }

        public static void RefreshItemViewsByUserDataExts(List<int> userDataExts)
        {
            if (userDataExts.Count == 0) {
                return;
            }

            List<ItemView> itemViews = new List<ItemView>();
            CollectItemView(itemViews);

            for (int i = 0; i < itemViews.Count; i++) {
                if (userDataExts.IndexOf(itemViews[i].UserDataExt) != -1) {
                    itemViews[i].Resort();
                }
            }
        }

        public static void CollectItemView(List<ItemView> itemViews)
        {
            for (int i = Screens.Count - 1; i >= 0; i--) {
                CollectItemView(Screens[i], itemViews);
            }
        }

        public static void CollectItemView(Object obj, List<ItemView> itemViews)
        {
            ItemView? itemView = (obj as ItemView);
            if (itemView != null) {
                itemViews.Add(itemView);
            }

            for (int i = 0; i < obj._Children.Count; i++) {
                CollectItemView(obj._Children[i], itemViews);
            }
        }

        //
        // Custom callbacks
        //

        public static void Callback_OnResolutionChanged()
        {
            for (int i = 0; i < Screens.Count; i++) {
                Screens[i]._RefreshPositionRecursive();
            }
        }

        public static void Callback_OnLanguageChanged()
        {
            for (int i = 0; i < Screens.Count; i++) {
                Screens[i]._RefreshTextRecursive();
                Screens[i]._RefreshPositionRecursive();
            }
        }

        //
        // Internal
        //

        public static Screen CreateScreen(GuiScreen screenNum)
        {
            Game.Verify(ScreenCreators.ContainsKey(screenNum), "No screen creator registered for this screen", screenNum);
            CreateScreenFunc screenFunc = ScreenCreators[screenNum];
            Screen screen = screenFunc();
            screen._Num = screenNum;
            screen._ActiveSelf = false;
            screen._IsRegistered = true;
            screen._InputKeyPressed = global::FOnline.Input.GetKeyPressed();
            screen._InputMousePressed = global::FOnline.Input.GetMousePressed();

            if (screenNum != GuiScreen.Cursor) {
                Screens.Add(screen);
            }
            else {
                CursorScreen = screen;
                screen._ActiveSelf = true;
            }

            screen._Init();
            return screen;
        }

        public static void UnregisterScreen(GuiScreen screenNum)
        {
            for (int i = Screens.Count - 1; i >= 0; i--) {
                Screen screen = Screens[i];

                if (screen.Num == screenNum) {
                    if (screen.Active) {
                        ShowHideScreen(screen, null!, false);
                    }

                    if (!screen.IsMultiinstance) {
                        screen._Remove();
                        Screens.RemoveAt(i);
                    }

                    screen._IsRegistered = false;
                }
            }
        }

        public static void ShowHideScreen(Screen screen, Dictionary<string, string> @params, bool show)
        {
            // Clean hovered/focused/pressed objects
            Object? hoveredObject = HoveredObject;
            if (hoveredObject != null) {
                HoveredObject = null;
                hoveredObject._Unhover();
            }

            SetFocusedObject(null!);

            Object? pressedObject = PressedObject;
            if (pressedObject != null) {
                PressedObject = null;
                pressedObject._MouseUp(true);
            }

            if (show) {
                // Hide active screen
                Screen? activeScreen = GetActiveScreen();

                if (activeScreen != null && activeScreen != screen) {
                    activeScreen._Disappear();
                }

                // Focus and hover new elements
                TextInput? textInput = FindNextTextInput(screen);
                if (textInput != null) {
                    SetFocusedObject(textInput);
                }

                Object? newHovered = screen.FindMouseHit();
                HoveredObject = newHovered;
                if (newHovered != null) {
                    newHovered._Hover();
                }

                // Move to top
                if (Screens[^1] != screen) {
                    Screens.RemoveAt(Screens.IndexOf(screen));
                    Screens.Add(screen);
                }

                // Callback
                if (!screen.Active) {
                    screen.SetActive(true);
                }

                screen._Show(@params); // Params may change, so need call again
                screen._Appear();

                Game.OnScreenChange.Fire(true, screen.Num, @params);
            }
            else {
                // Ignore redundant hide
                if (!screen.Active) {
                    return;
                }

                // Callbacks
                screen._Disappear();
                screen._Hide();
                screen.SetActive(false);

                Game.OnScreenChange.Fire(false, screen.Num, @params);

                // Remove multiinstance
                if (screen.IsMultiinstance) {
                    screen._Remove();
                    CleanupObjectGraph(screen);
                    Screens.RemoveAt(Screens.IndexOf(screen));
                }

                // Appear behind screen
                if (screen.IsMultiinstance || screen == Screens[^1]) {
                    Screen? appearScreen = GetActiveScreen();

                    // Move to top
                    if (appearScreen != null) {
                        Screens.RemoveAt(Screens.IndexOf(appearScreen));
                        Screens.Add(appearScreen);

                        appearScreen._Appear();
                    }
                }
            }
        }

        public static int GetScreenCount(GuiScreen screenNum)
        {
            int count = 0;

            for (int i = Screens.Count - 1; i >= 0; i--) {
                if (Screens[i].Num == screenNum) {
                    count++;
                }
            }

            return (int)(count);
        }

        public static void Highlight(GuiScreen screenNum, string objName, timespan duration, int skip = 0, bool all = false)
        {
            if (all) {
                Screen? screen = GetScreen(screenNum);
                int count = screen != null ? screen.Count(objName) : 0;

                for (int i = 0; i < count; i++) {
                    Game.StartTimeEvent(Time.Asap(), (Callback_void_anyArray)HighlightProcess, [((int)(screenNum)).ToString(System.Globalization.CultureInfo.InvariantCulture), objName, (duration.milliseconds).ToString(System.Globalization.CultureInfo.InvariantCulture), (i).ToString(System.Globalization.CultureInfo.InvariantCulture)]);
                }
            }
            else {
                Game.StartTimeEvent(Time.Asap(), (Callback_void_anyArray)HighlightProcess, [((int)(screenNum)).ToString(System.Globalization.CultureInfo.InvariantCulture), objName, (duration.milliseconds).ToString(System.Globalization.CultureInfo.InvariantCulture), (skip).ToString(System.Globalization.CultureInfo.InvariantCulture)]);
            }
        }

        public static void Highlight(GuiScreen screenNum, string objName, int skip = 0, bool all = false)
        {
            Highlight(screenNum, objName, Time.Seconds(5), skip, all);
        }

        [TimeEvent]
        [Async]
        public static async void HighlightProcess(List<string> data)
        {
            GuiScreen screenNum = (GuiScreen)(int.Parse(data[0], System.Globalization.CultureInfo.InvariantCulture));
            string objName = data[1];
            timespan duration = Time.Milliseconds(int.Parse(data[2], System.Globalization.CultureInfo.InvariantCulture));
            int skip = int.Parse(data[3], System.Globalization.CultureInfo.InvariantCulture);

            Screen? screen = GetScreen(screenNum);
            if (screen == null) {
                return;
            }

            Object? obj = screen.Find(objName, skip: skip);
            Panel? panel = (obj as Panel);
            Text? text = (obj as Text);

            if (obj == null || (panel == null && text == null)) {
                return;
            }
            if (obj._Highlighted) {
                return;
            }

            obj._Highlighted = true;

            Sprite? panelSpr = panel != null ? panel.BackgroundImage : null;
            ucolor panelColor = panelSpr != null ? panelSpr.Color : default(ucolor);
            ucolor textColor = text != null ? text.TextColor : default(ucolor);
            nanotime startTime = Game.FrameTime;

            while (true) {
                timespan elapsedTime = Game.FrameTime - startTime;

                if (elapsedTime > duration) {
                    break;
                }

                float intensity = Math.RoundToInt(elapsedTime.milliseconds % 1000) / 500.0f;
                intensity = intensity < 1.0f ? intensity : 2.0f - intensity;

                ucolor color = panelSpr != null ? panelColor : textColor;
                color.red = (byte)(color.red + Math.RoundToInt((float)(255 - color.red) * intensity));
                color.green = (byte)(color.green + Math.RoundToInt((float)(255 - color.green) * intensity));
                color.blue = (byte)(color.blue + Math.RoundToInt((float)(255 - color.blue) * intensity));

                if (panelSpr != null) {
                    panelSpr.Color = color;
                }
                else if (text != null) {
                    text.SetTextColor(color);
                }

                await Game.YieldAsync(10);
            }

            if (panelSpr != null) {
                panelSpr.Color = panelColor;
            }
            else if (text != null) {
                text.SetTextColor(textColor);
            }

            obj._Highlighted = false;
        }

#endif
    }
}
