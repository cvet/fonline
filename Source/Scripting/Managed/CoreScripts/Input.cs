#nullable enable

using System.Collections.Generic;
using FOnline;

namespace FOnline
{
    public static partial class Input
    {
#if CLIENT

        public delegate bool KeyDownCallback(KeyCode key, string text);

        public delegate void KeyUpCallback(KeyCode key);

        public static List<bool> KeyPressed = new List<bool>();

        public static List<bool> MousePressed = new List<bool>();

        public static List<KeyDownCallback> KeyDownCallbacks = new List<KeyDownCallback>();

        public static List<KeyUpCallback> KeyUpCallbacks = new List<KeyUpCallback>();

        public static void AddKeyDownCallback(KeyDownCallback callback)
        {
            KeyDownCallbacks.Add(callback);
        }

        public static void AddKeyUpCallback(KeyUpCallback callback)
        {
            KeyUpCallbacks.Add(callback);
        }

        public static bool IsKeyPressed(KeyCode key)
        {
            return KeyPressed[(int)key];
        }

        public static bool IsMousePressed(MouseButton button)
        {
            return MousePressed[(int)button];
        }

        public static GamepadState GetGamepadState()
        {
            return Game.GetGamepadState();
        }

        public static bool IsCtrlDown()
        {
            return KeyPressed[(int)KeyCode.Rcontrol] || KeyPressed[(int)KeyCode.Lcontrol];
        }

        public static bool IsAltDown()
        {
            return KeyPressed[(int)KeyCode.Lmenu] || KeyPressed[(int)KeyCode.Rmenu];
        }

        public static bool IsShiftDown()
        {
            return KeyPressed[(int)KeyCode.Lshift] || KeyPressed[(int)KeyCode.Rshift];
        }

        public static List<bool> GetKeyPressed()
        {
            return KeyPressed;
        }

        public static List<bool> GetMousePressed()
        {
            return MousePressed;
        }

        [ModuleInit]
        public static void InitInput()
        {
            Game.OnMouseDown.Subscribe(OnMouseDown);
            Game.OnMouseUp.Subscribe(OnMouseUp);
            Game.OnMouseMove.Subscribe(OnMouseMove);
            Game.OnKeyDown.Subscribe(OnKeyDown);
            Game.OnKeyUp.Subscribe(OnKeyUp);
            Game.OnInputLost.Subscribe(OnInputLost);

            KeyPressed.resize(0x100);
            MousePressed.resize((int)MouseButton.Ext4 + 1);
        }

        public static void ReleaseKeys()
        {
            for (int i = 0; i < KeyPressed.Count; i++) {
                if (KeyPressed[i]) {
                    KeyUp((KeyCode)(i));
                }
            }
        }

        public static void ReleaseMouse()
        {
            for (int i = 0; i < MousePressed.Count; i++) {
                if (MousePressed[i]) {
                    MouseUp((MouseButton)(i));
                }
            }
        }

        [Event]
        public static void OnMouseDown(MouseButton button)
        {
            MouseDown(button);
        }

        [Event]
        public static void OnMouseUp(MouseButton button)
        {
            MouseUp(button);
        }

        [Event]
        public static void OnMouseMove(ipos offset)
        {
            MouseMove(offset);
        }

        [Event]
        public static void OnKeyDown(KeyCode key, string text)
        {
            KeyDown(key, text);
        }

        [Event]
        public static void OnKeyUp(KeyCode key)
        {
            KeyUp(key);
        }

        [Event]
        public static void OnInputLost()
        {
            InputLost();
        }

        public static void MouseDown(MouseButton button)
        {
            if (Game.IsConnecting()) {
                return;
            }

            MousePressed[(int)button] = true;

            Gui.EngineCallback_MouseDown(button);
        }

        public static void MouseUp(MouseButton button)
        {
            if (MousePressed[(int)button]) {
                Gui.EngineCallback_MouseUp(button);
            }

            MousePressed[(int)button] = false;
        }

        public static void MouseMove(ipos offset)
        {
            Gui.EngineCallback_MouseMove(offset);
        }

        public static void KeyDown(KeyCode key, string text)
        {
            // Avoid repeating
            if (KeyPressed[(int)key]) {
                if (key != KeyCode.Text && key != KeyCode.Space && key != KeyCode.Back && key != KeyCode.Delete && key != KeyCode.Left && key != KeyCode.Right) {
                    return;
                }
            }
            KeyPressed[(int)key] = true;

            // Handlers
            bool handled = false;
            for (int i = 0; !handled && i < KeyDownCallbacks.Count; i++) {
                handled = KeyDownCallbacks[i](key, text);
            }
            if (!handled) {
                Gui.EngineCallback_KeyDown(key, text);
            }
        }

        public static void KeyUp(KeyCode key)
        {
            // Avoid repeating
            if (!KeyPressed[(int)key]) {
                return;
            }
            KeyPressed[(int)key] = false;

            // Handlers
            for (int i = 0; i < KeyUpCallbacks.Count; i++) {
                KeyUpCallbacks[i](key);
            }
            Gui.EngineCallback_KeyUp(key);
        }

        // Called on mouse/keyboard input lost (alt-tab, minimize, lost focus).
        public static void InputLost()
        {
            // Reset states
            ReleaseKeys();
            ReleaseMouse();

            // Gui:: handler
            Gui.EngineCallback_InputLost();
        }

#endif
    }
}
