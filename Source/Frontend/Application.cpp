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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Application.h"
#include "ConfigFile.h"
#include "DiskFileSystem.h"
#include "FileSystem.h"
#include "Log.h"
#include "StringUtils.h"
#include "Version-Include.h"
#include "WinApi-Include.h"

#include "SDL.h"
#include "SDL_audio.h"
#include "SDL_events.h"
#include "SDL_video.h"
#include "imgui.h"

#if FO_WINDOWS || FO_LINUX || FO_MAC
#if !FO_WINDOWS
#if __has_include(<libunwind.h>)
#define BACKWARD_HAS_LIBUNWIND 1
#elif __has_include(<bfd.h>)
#define BACKWARD_HAS_BFD 1
#endif
#endif
#include "backward.hpp"
#endif

Application* App;

#if FO_WINDOWS && FO_DEBUG
static _CrtMemState CrtMemState;
#endif

static ImGuiKey KeycodeToImGuiKey(int keycode);

// Todo: move all these statics to App class fields
static Renderer* ActiveRenderer {};
static RenderType ActiveRendererType {};
static RenderTexture* RenderTargetTex {};

static vector<InputEvent>* EventsQueue {};
static vector<InputEvent>* NextFrameEventsQueue {};

static SDL_AudioDeviceID AudioDeviceId {};
static SDL_AudioSpec AudioSpec {};
static AppAudio::AudioStreamCallback* AudioStreamWriter {};

static uint MaxAtlasWidth {};
static uint MaxAtlasHeight {};
static uint MaxBones {};
const uint& AppRender::MAX_ATLAS_WIDTH {MaxAtlasWidth};
const uint& AppRender::MAX_ATLAS_HEIGHT {MaxAtlasHeight};
const uint& AppRender::MAX_BONES {MaxBones};
const int AppAudio::AUDIO_FORMAT_U8 {AUDIO_U8};
const int AppAudio::AUDIO_FORMAT_S16 {AUDIO_S16};

void InitApp(int argc, char** argv, string_view name_appendix)
{
    // Ensure that we call init only once
    static std::once_flag once;
    auto first_call = false;
    std::call_once(once, [&first_call] { first_call = true; });
    if (!first_call) {
        throw AppInitException("InitApp must be called only once");
    }

    // Unhandled exceptions handler
#if FO_WINDOWS || FO_LINUX || FO_MAC
    {
        [[maybe_unused]] static backward::SignalHandling sh;
        assert(sh.loaded());
    }
#endif

    CreateGlobalData();

    App = new Application(argc, argv, name_appendix);
}

void ExitApp(bool success)
{
    const auto code = success ? EXIT_SUCCESS : EXIT_FAILURE;
#if !FO_WEB && !FO_MAC && !FO_IOS && !FO_ANDROID
    std::quick_exit(code);
#else
    std::exit(code);
#endif
}

void ReportExceptionAndExit(const std::exception& ex)
{
    if (!BreakIntoDebugger(ex.what())) {
        WriteLog(LogType::Error, "\n{}\n", ex.what());
        CreateDumpMessage("FatalException", ex.what());
        MessageBox::ShowErrorMessage("Fatal Error", ex.what(), GetStackTrace());
    }

    ExitApp(false);
}

void ReportExceptionAndContinue(const std::exception& ex)
{
    if (BreakIntoDebugger(ex.what())) {
        return;
    }

    WriteLog(LogType::Error, "\n{}\n", ex.what());

#if FO_DEBUG
    MessageBox::ShowErrorMessage("Error", ex.what(), GetStackTrace());
#endif
}

auto RenderEffect::IsSame(string_view name, string_view defines) const -> bool
{
    return _str(name).compareIgnoreCase(_effectName) && defines == _effectDefines;
}

auto RenderEffect::CanBatch(const RenderEffect* other) const -> bool
{
    if (_name != other->_name) {
        return false;
    }
    if (MainTex != other->MainTex) {
        return false;
    }
    if (BorderBuf || AnimBuf || RandomValueBuf) {
        return false;
    }
#if FO_ENABLE_3D
    if (ModelBuf) {
        return false;
    }
#endif
    if (CustomTexBuf && std::mismatch(std::begin(CustomTex), std::end(CustomTex), std::begin(other->CustomTex)).first != std::end(CustomTex)) {
        return false;
    }
    return true;
}

static unordered_map<SDL_Keycode, KeyCode>* KeysMap {};
static unordered_map<int, MouseButton>* MouseButtonsMap {};

Application::Application(int argc, char** argv, string_view name_appendix) : Settings(argc, argv)
{
    _name.append(FO_DEV_NAME);
    if (!name_appendix.empty()) {
        _name.append("_");
        _name.append(name_appendix);
    }

#if !FO_WEB
    LogToFile(_str("{}.log", _name));
#endif

    if (SDL_InitSubSystem(SDL_INIT_TIMER) != 0) {
        throw AppInitException("SDL_InitSubSystem SDL_INIT_TIMER failed", SDL_GetError());
    }

    // Initialize input events
    if (SDL_InitSubSystem(SDL_INIT_EVENTS) != 0) {
        throw AppInitException("SDL_InitSubSystem SDL_INIT_EVENTS failed", SDL_GetError());
    }

    EventsQueue = new vector<InputEvent>();
    NextFrameEventsQueue = new vector<InputEvent>();

    KeysMap = new unordered_map<SDL_Keycode, KeyCode> {
        {0, KeyCode::None},
        {SDL_SCANCODE_ESCAPE, KeyCode::Escape},
        {SDL_SCANCODE_1, KeyCode::C1},
        {SDL_SCANCODE_2, KeyCode::C2},
        {SDL_SCANCODE_3, KeyCode::C3},
        {SDL_SCANCODE_4, KeyCode::C4},
        {SDL_SCANCODE_5, KeyCode::C5},
        {SDL_SCANCODE_6, KeyCode::C6},
        {SDL_SCANCODE_7, KeyCode::C7},
        {SDL_SCANCODE_8, KeyCode::C8},
        {SDL_SCANCODE_9, KeyCode::C9},
        {SDL_SCANCODE_0, KeyCode::C0},
        {SDL_SCANCODE_MINUS, KeyCode::Minus},
        {SDL_SCANCODE_EQUALS, KeyCode::Equals},
        {SDL_SCANCODE_BACKSPACE, KeyCode::Back},
        {SDL_SCANCODE_TAB, KeyCode::Tab},
        {SDL_SCANCODE_Q, KeyCode::Q},
        {SDL_SCANCODE_W, KeyCode::W},
        {SDL_SCANCODE_E, KeyCode::E},
        {SDL_SCANCODE_R, KeyCode::R},
        {SDL_SCANCODE_T, KeyCode::T},
        {SDL_SCANCODE_Y, KeyCode::Y},
        {SDL_SCANCODE_U, KeyCode::U},
        {SDL_SCANCODE_I, KeyCode::I},
        {SDL_SCANCODE_O, KeyCode::O},
        {SDL_SCANCODE_P, KeyCode::P},
        {SDL_SCANCODE_LEFTBRACKET, KeyCode::Lbracket},
        {SDL_SCANCODE_RIGHTBRACKET, KeyCode::Rbracket},
        {SDL_SCANCODE_RETURN, KeyCode::Return},
        {SDL_SCANCODE_LCTRL, KeyCode::Lcontrol},
        {SDL_SCANCODE_A, KeyCode::A},
        {SDL_SCANCODE_S, KeyCode::S},
        {SDL_SCANCODE_D, KeyCode::D},
        {SDL_SCANCODE_F, KeyCode::F},
        {SDL_SCANCODE_G, KeyCode::G},
        {SDL_SCANCODE_H, KeyCode::H},
        {SDL_SCANCODE_J, KeyCode::J},
        {SDL_SCANCODE_K, KeyCode::K},
        {SDL_SCANCODE_L, KeyCode::L},
        {SDL_SCANCODE_SEMICOLON, KeyCode::Semicolon},
        {SDL_SCANCODE_APOSTROPHE, KeyCode::Apostrophe},
        {SDL_SCANCODE_GRAVE, KeyCode::Grave},
        {SDL_SCANCODE_LSHIFT, KeyCode::Lshift},
        {SDL_SCANCODE_BACKSLASH, KeyCode::Backslash},
        {SDL_SCANCODE_Z, KeyCode::Z},
        {SDL_SCANCODE_X, KeyCode::X},
        {SDL_SCANCODE_C, KeyCode::C},
        {SDL_SCANCODE_V, KeyCode::V},
        {SDL_SCANCODE_B, KeyCode::B},
        {SDL_SCANCODE_N, KeyCode::N},
        {SDL_SCANCODE_M, KeyCode::M},
        {SDL_SCANCODE_COMMA, KeyCode::Comma},
        {SDL_SCANCODE_PERIOD, KeyCode::Period},
        {SDL_SCANCODE_SLASH, KeyCode::Slash},
        {SDL_SCANCODE_RSHIFT, KeyCode::Rshift},
        {SDL_SCANCODE_KP_MULTIPLY, KeyCode::Multiply},
        {SDL_SCANCODE_LALT, KeyCode::Lmenu},
        {SDL_SCANCODE_SPACE, KeyCode::Space},
        {SDL_SCANCODE_CAPSLOCK, KeyCode::Capital},
        {SDL_SCANCODE_F1, KeyCode::F1},
        {SDL_SCANCODE_F2, KeyCode::F2},
        {SDL_SCANCODE_F3, KeyCode::F3},
        {SDL_SCANCODE_F4, KeyCode::F4},
        {SDL_SCANCODE_F5, KeyCode::F5},
        {SDL_SCANCODE_F6, KeyCode::F6},
        {SDL_SCANCODE_F7, KeyCode::F7},
        {SDL_SCANCODE_F8, KeyCode::F8},
        {SDL_SCANCODE_F9, KeyCode::F9},
        {SDL_SCANCODE_F10, KeyCode::F10},
        {SDL_SCANCODE_NUMLOCKCLEAR, KeyCode::Numlock},
        {SDL_SCANCODE_SCROLLLOCK, KeyCode::Scroll},
        {SDL_SCANCODE_KP_7, KeyCode::Numpad7},
        {SDL_SCANCODE_KP_8, KeyCode::Numpad8},
        {SDL_SCANCODE_KP_9, KeyCode::Numpad9},
        {SDL_SCANCODE_KP_MINUS, KeyCode::Subtract},
        {SDL_SCANCODE_KP_4, KeyCode::Numpad4},
        {SDL_SCANCODE_KP_5, KeyCode::Numpad5},
        {SDL_SCANCODE_KP_6, KeyCode::Numpad6},
        {SDL_SCANCODE_KP_PLUS, KeyCode::Add},
        {SDL_SCANCODE_KP_1, KeyCode::Numpad1},
        {SDL_SCANCODE_KP_2, KeyCode::Numpad2},
        {SDL_SCANCODE_KP_3, KeyCode::Numpad3},
        {SDL_SCANCODE_KP_0, KeyCode::Numpad0},
        {SDL_SCANCODE_KP_PERIOD, KeyCode::Decimal},
        {SDL_SCANCODE_F11, KeyCode::F11},
        {SDL_SCANCODE_F12, KeyCode::F12},
        {SDL_SCANCODE_KP_ENTER, KeyCode::Numpadenter},
        {SDL_SCANCODE_RCTRL, KeyCode::Rcontrol},
        {SDL_SCANCODE_KP_DIVIDE, KeyCode::Divide},
        {SDL_SCANCODE_SYSREQ, KeyCode::Sysrq},
        {SDL_SCANCODE_RALT, KeyCode::Rmenu},
        {SDL_SCANCODE_PAUSE, KeyCode::Pause},
        {SDL_SCANCODE_HOME, KeyCode::Home},
        {SDL_SCANCODE_UP, KeyCode::Up},
        {SDL_SCANCODE_PAGEUP, KeyCode::Prior},
        {SDL_SCANCODE_LEFT, KeyCode::Left},
        {SDL_SCANCODE_RIGHT, KeyCode::Right},
        {SDL_SCANCODE_END, KeyCode::End},
        {SDL_SCANCODE_DOWN, KeyCode::Down},
        {SDL_SCANCODE_PAGEDOWN, KeyCode::Next},
        {SDL_SCANCODE_INSERT, KeyCode::Insert},
        {SDL_SCANCODE_DELETE, KeyCode::Delete},
        {SDL_SCANCODE_LGUI, KeyCode::Lwin},
        {SDL_SCANCODE_RGUI, KeyCode::Rwin},
        {510, KeyCode::Text},
        {511, KeyCode::ClipboardPaste},
    };

    MouseButtonsMap = new unordered_map<int, MouseButton> {
        {SDL_BUTTON_LEFT, MouseButton::Left},
        {SDL_BUTTON_RIGHT, MouseButton::Right},
        {SDL_BUTTON_MIDDLE, MouseButton::Middle},
        {SDL_BUTTON(4), MouseButton::Ext0},
        {SDL_BUTTON(5), MouseButton::Ext1},
        {SDL_BUTTON(6), MouseButton::Ext2},
        {SDL_BUTTON(7), MouseButton::Ext3},
        {SDL_BUTTON(8), MouseButton::Ext4},
    };

    // Initialize audio
    if (!Settings.DisableAudio) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            AudioStreamWriter = new AppAudio::AudioStreamCallback();

            SDL_AudioSpec desired;
            std::memset(&desired, 0, sizeof(desired));
#if FO_WEB
            desired.format = AUDIO_F32;
            desired.freq = 48000;
            desired.channels = 2;
#else
            desired.format = AUDIO_S16;
            desired.freq = 44100;
#endif
            desired.callback = [](void*, Uint8* stream, int) {
                std::memset(stream, AudioSpec.silence, AudioSpec.size);
                if (*AudioStreamWriter) {
                    (*AudioStreamWriter)(stream);
                }
            };

            AudioDeviceId = SDL_OpenAudioDevice(nullptr, 0, &desired, &AudioSpec, SDL_AUDIO_ALLOW_ANY_CHANGE);
            if (AudioDeviceId >= 2) {
                SDL_PauseAudioDevice(AudioDeviceId, 0);
            }
            else {
                WriteLog("SDL open audio device failed, error {}", SDL_GetError());
            }
        }
        else {
            WriteLog("SDL init audio subsystem failed, error {}", SDL_GetError());
        }
    }

    // First choose render type by user preference
    if (Settings.NullRenderer) {
        ActiveRendererType = RenderType::Null;
        ActiveRenderer = new Null_Renderer();
    }
#if FO_HAVE_OPENGL
    if (Settings.ForceOpenGL) {
        ActiveRendererType = RenderType::OpenGL;
        ActiveRenderer = new OpenGL_Renderer();
    }
#endif
#if FO_HAVE_DIRECT_3D
    if (Settings.ForceDirect3D) {
        ActiveRendererType = RenderType::Direct3D;
        ActiveRenderer = new Direct3D_Renderer();
    }
#endif
#if FO_HAVE_METAL
    if (Settings.ForceMetal) {
        ActiveRendererType = RenderType::Metal;
        throw NotImplementedException(LINE_STR);
    }
#endif
#if FO_HAVE_VULKAN
    if (Settings.ForceVulkan) {
        ActiveRendererType = RenderType::Vulkan;
        throw NotImplementedException(LINE_STR);
    }
#endif
#if FO_HAVE_GNM
    if (Settings.ForceGNM) {
        ActiveRendererType = RenderType::GNM;
        throw NotImplementedException(LINE_STR);
    }
#endif

    // If none of selected then evaluate automatic selection
    if (ActiveRenderer == nullptr) {
#if FO_HAVE_OPENGL
        ActiveRendererType = RenderType::OpenGL;
        ActiveRenderer = new OpenGL_Renderer();
#endif
#if FO_HAVE_DIRECT_3D
        ActiveRendererType = RenderType::Direct3D;
        ActiveRenderer = new Direct3D_Renderer();
#endif
#if FO_HAVE_METAL
        ActiveRendererType = RenderType::Metal;
#endif
#if FO_HAVE_VULKAN
        ActiveRendererType = RenderType::Vulkan;
#endif
#if FO_HAVE_GNM
        ActiveRendererType = RenderType::GNM;
#endif

        if (ActiveRenderer == nullptr) {
            throw AppInitException("No renderer selected");
        }
    }

    // Determine main window size
#if FO_IOS || FO_ANDROID
    _isTablet = true;
#endif
#if FO_WINDOWS && !FO_UWP
    _isTablet = ::GetSystemMetrics(SM_TABLETPC) != 0;
#endif

    // Initialize video system
    if (ActiveRendererType != RenderType::Null) {
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
            throw AppInitException("SDL_InitSubSystem SDL_INIT_VIDEO failed", SDL_GetError());
        }

        if (_isTablet) {
            SDL_DisplayMode mode;
            if (const auto r = SDL_GetCurrentDisplayMode(0, &mode); r != 0) {
                throw AppInitException("SDL_GetCurrentDisplayMode failed", SDL_GetError());
            }

            Settings.ScreenWidth = std::max(mode.w, mode.h);
            Settings.ScreenHeight = std::min(mode.w, mode.h);

            const auto ratio = static_cast<float>(Settings.ScreenWidth) / static_cast<float>(Settings.ScreenHeight);
            Settings.ScreenHeight = 768;
            Settings.ScreenWidth = std::lround(static_cast<float>(Settings.ScreenHeight) * ratio);

            Settings.FullScreen = true;
        }

#if FO_WEB
        // Adaptive size
        int window_w = EM_ASM_INT(return window.innerWidth || document.documentElement.clientWidth || document.getElementsByTagName('body')[0].clientWidth);
        int window_h = EM_ASM_INT(return window.innerHeight || document.documentElement.clientHeight || document.getElementsByTagName('body')[0].clientHeight);
        Settings.ScreenWidth = std::clamp(window_w - 200, 1024, 1920);
        Settings.ScreenHeight = std::clamp(window_h - 100, 768, 1080);

        // Fixed size
        int fixed_w = EM_ASM_INT(return 'foScreenWidth' in Module ? Module.foScreenWidth : 0);
        int fixed_h = EM_ASM_INT(return 'foScreenHeight' in Module ? Module.foScreenHeight : 0);
        if (fixed_w) {
            Settings.ScreenWidth = fixed_w;
        }
        if (fixed_h) {
            Settings.ScreenHeight = fixed_h;
        }
#endif

        MainWindow._windowHandle = CreateInternalWindow(Settings.ScreenWidth, Settings.ScreenHeight);

        ActiveRenderer->Init(Settings, MainWindow._windowHandle);

        // SDL_ShowCursor(0);

        if (Settings.AlwaysOnTop) {
            MainWindow.AlwaysOnTop(true);
        }

        // Skip SDL allocations from profiling
#if FO_WINDOWS && FO_DEBUG
        ::_CrtMemCheckpoint(&CrtMemState);
#endif

        // Init Dear ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();

        io.BackendPlatformName = "fonline";
        io.WantSaveIniSettings = false;
        io.IniFilename = nullptr;

        const string sdl_backend = SDL_GetCurrentVideoDriver();
        vector<string> global_mouse_whitelist = {"windows", "cocoa", "x11", "DIVE", "VMAN"};
        _mouseCanUseGlobalState = std::any_of(global_mouse_whitelist.begin(), global_mouse_whitelist.end(), [&sdl_backend](auto& entry) { return _str(sdl_backend).startsWith(entry); });

        if (Settings.ImGuiColorStyle == "Dark") {
            ImGui::StyleColorsDark();
        }
        else if (Settings.ImGuiColorStyle == "Classic") {
            ImGui::StyleColorsClassic();
        }
        else {
            ImGui::StyleColorsLight(); // Default theme
        }

        io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;

        io.GetClipboardTextFn = [](void*) -> const char* { return App->Input.GetClipboardText().c_str(); };
        io.SetClipboardTextFn = [](void*, const char* text) { App->Input.SetClipboardText(text); };
        io.ClipboardUserData = nullptr;

#if FO_WINDOWS
        SDL_SysWMinfo info;
        SDL_VERSION(&info.version);
        if (SDL_GetWindowWMInfo(static_cast<SDL_Window*>(MainWindow._windowHandle), &info) != 0) {
            ImGui::GetMainViewport()->PlatformHandleRaw = static_cast<void*>(info.info.win.window);
        }
#endif

        SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

        // Font texture
        unsigned char* pixels;
        int width;
        int height;
        int bytes_per_pixel;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bytes_per_pixel);
        RUNTIME_ASSERT(bytes_per_pixel == 4);

        auto* font_tex = ActiveRenderer->CreateTexture(width, height, true, false);
        font_tex->UpdateTextureRegion(IRect(0, 0, width - 1, height - 1), reinterpret_cast<const uint*>(pixels));
        io.Fonts->TexID = font_tex;

        // Default effect
        FileSystem base_fs;
        base_fs.AddDataSource(Settings.EmbeddedResources);
        _imguiEffect = ActiveRenderer->CreateEffect(EffectUsage::ImGui, "Effects/ImGui_Default", "", [&base_fs](string_view path) -> string {
            const auto file = base_fs.ReadFile(path);
            RUNTIME_ASSERT(file);
            return file.GetStr();
        });

        _imguiDrawBuf = ActiveRenderer->CreateDrawBuffer(false);
    }

    // Start timings
    _timeFrequency = SDL_GetPerformanceFrequency();
    _time = SDL_GetPerformanceCounter();
}

#if FO_IOS
void Application::SetMainLoopCallback(void (*callback)(void*))
{
    SDL_iPhoneSetAnimationCallback(static_cast<SDL_Window*>(MainWindow._windowHandle), 1, callback, nullptr);
}
#endif

auto Application::GetName() const -> string_view
{
    return _name;
}

auto Application::CreateChildWindow(int width, int height) -> AppWindow*
{
    auto* sdl_window = CreateInternalWindow(width, height);
    auto* window = new AppWindow();
    window->_windowHandle = sdl_window;
    return window;
}

auto Application::CreateInternalWindow(int width, int height) -> WindowInternalHandle*
{
    NON_CONST_METHOD_HINT();

    // Initialize window
    Uint32 window_create_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;

#if FO_HAVE_OPENGL
    if (ActiveRendererType == RenderType::OpenGL) {
#if !FO_WEB
        window_create_flags |= SDL_WINDOW_OPENGL;
#endif

        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);

#if FO_OPENGL_ES
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#endif
    }
#endif

#if FO_HAVE_METAL
    if (ActiveRendererType == RenderType::Metal) {
        window_create_flags |= SDL_WINDOW_METAL;
    }
#endif

#if FO_HAVE_VULKAN
    if (ActiveRendererType == RenderType::Vulkan) {
        window_create_flags |= SDL_WINDOW_VULKAN;
    }
#endif

    if (Settings.FullScreen) {
        window_create_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    if (_isTablet) {
        window_create_flags |= SDL_WINDOW_FULLSCREEN;
        window_create_flags |= SDL_WINDOW_BORDERLESS;
    }

    int win_pos = SDL_WINDOWPOS_UNDEFINED;
    if (Settings.WindowCentered) {
        win_pos = SDL_WINDOWPOS_CENTERED;
    }

    auto* sdl_window = SDL_CreateWindow(Settings.WindowName.c_str(), win_pos, win_pos, Settings.ScreenWidth, Settings.ScreenHeight, window_create_flags);
    if (sdl_window == nullptr) {
        throw AppInitException("Window creation failed", SDL_GetError());
    }

    return sdl_window;
}

void Application::BeginFrame()
{
    RUNTIME_ASSERT(RenderTargetTex == nullptr);
    ActiveRenderer->ClearRenderTarget(COLOR_RGB(150, 150, 150));

    ImGuiIO& io = ImGui::GetIO();

    if (!NextFrameEventsQueue->empty()) {
        EventsQueue->insert(EventsQueue->end(), NextFrameEventsQueue->begin(), NextFrameEventsQueue->end());
        NextFrameEventsQueue->clear();
    }

    SDL_PumpEvents();

    SDL_Event sdl_event;
    while (SDL_PollEvent(&sdl_event) != 0) {
        switch (sdl_event.type) {
        case SDL_MOUSEMOTION: {
            InputEvent::MouseMoveEvent ev;
            ev.MouseX = sdl_event.motion.x;
            ev.MouseY = sdl_event.motion.y;
            ev.DeltaX = sdl_event.motion.xrel;
            ev.DeltaY = sdl_event.motion.yrel;
            EventsQueue->emplace_back(ev);

            io.AddMousePosEvent(static_cast<float>(sdl_event.motion.x), static_cast<float>(sdl_event.motion.y));
        } break;
        case SDL_MOUSEBUTTONUP:
            [[fallthrough]];
        case SDL_MOUSEBUTTONDOWN: {
            if (sdl_event.type == SDL_MOUSEBUTTONDOWN) {
                InputEvent::MouseDownEvent ev;
                ev.Button = (*MouseButtonsMap)[sdl_event.button.button];
                EventsQueue->emplace_back(ev);
            }
            else {
                InputEvent::MouseUpEvent ev;
                ev.Button = (*MouseButtonsMap)[sdl_event.button.button];
                EventsQueue->emplace_back(ev);
            }

            int mouse_button = -1;
            if (sdl_event.button.button == SDL_BUTTON_LEFT) {
                mouse_button = 0;
            }
            else if (sdl_event.button.button == SDL_BUTTON_RIGHT) {
                mouse_button = 1;
            }
            else if (sdl_event.button.button == SDL_BUTTON_MIDDLE) {
                mouse_button = 2;
            }
            else if (sdl_event.button.button == SDL_BUTTON_X1) {
                mouse_button = 3;
            }
            else if (sdl_event.button.button == SDL_BUTTON_X2) {
                mouse_button = 4;
            }
            if (mouse_button != -1) {
                io.AddMouseButtonEvent(mouse_button, (sdl_event.type == SDL_MOUSEBUTTONDOWN));
                _mouseButtonsDown = (sdl_event.type == SDL_MOUSEBUTTONDOWN) ? (_mouseButtonsDown | (1 << mouse_button)) : (_mouseButtonsDown & ~(1 << mouse_button));
            }
        } break;
        case SDL_FINGERMOTION: {
            InputEvent::MouseMoveEvent ev;
            ev.MouseX = sdl_event.motion.x;
            ev.MouseY = sdl_event.motion.y;
            EventsQueue->emplace_back(ev);
        } break;
        case SDL_FINGERDOWN: {
            InputEvent::MouseMoveEvent ev1;
            SDL_GetMouseState(&ev1.MouseX, &ev1.MouseY);
            EventsQueue->emplace_back(ev1);
            InputEvent::MouseDownEvent ev2;
            ev2.Button = MouseButton::Left;
            EventsQueue->emplace_back(ev2);
        } break;
        case SDL_FINGERUP: {
            InputEvent::MouseUpEvent ev;
            ev.Button = MouseButton::Left;
            EventsQueue->emplace_back(ev);
        } break;
        case SDL_MOUSEWHEEL: {
            InputEvent::MouseWheelEvent ev;
            ev.Delta = -sdl_event.wheel.y;
            EventsQueue->emplace_back(ev);

            float wheel_x = (sdl_event.wheel.x > 0) ? 1.0f : (sdl_event.wheel.x < 0) ? -1.0f : 0.0f;
            float wheel_y = (sdl_event.wheel.y > 0) ? 1.0f : (sdl_event.wheel.y < 0) ? -1.0f : 0.0f;
            io.AddMouseWheelEvent(wheel_x, wheel_y);
        } break;
        case SDL_KEYUP:
            [[fallthrough]];
        case SDL_KEYDOWN: {
            if (sdl_event.type == SDL_KEYDOWN) {
                InputEvent::KeyDownEvent ev;
                ev.Code = (*KeysMap)[sdl_event.key.keysym.scancode];
                ev.Text = sdl_event.text.text;
                EventsQueue->emplace_back(ev);
            }
            else {
                InputEvent::KeyUpEvent ev;
                ev.Code = (*KeysMap)[sdl_event.key.keysym.scancode];
                EventsQueue->emplace_back(ev);
            }

            const auto sdl_key_mods = static_cast<SDL_Keymod>(sdl_event.key.keysym.mod);
            io.AddKeyEvent(ImGuiKey_ModCtrl, (sdl_key_mods & KMOD_CTRL) != 0);
            io.AddKeyEvent(ImGuiKey_ModShift, (sdl_key_mods & KMOD_SHIFT) != 0);
            io.AddKeyEvent(ImGuiKey_ModAlt, (sdl_key_mods & KMOD_ALT) != 0);
            io.AddKeyEvent(ImGuiKey_ModSuper, (sdl_key_mods & KMOD_GUI) != 0);

            ImGuiKey key = KeycodeToImGuiKey(sdl_event.key.keysym.sym);
            io.AddKeyEvent(key, sdl_event.type == SDL_KEYDOWN);
            io.SetKeyEventNativeData(key, sdl_event.key.keysym.sym, sdl_event.key.keysym.scancode, sdl_event.key.keysym.scancode);
        } break;
        case SDL_TEXTINPUT: {
            InputEvent::KeyDownEvent ev1;
            ev1.Code = KeyCode::Text;
            ev1.Text = sdl_event.text.text;
            EventsQueue->emplace_back(ev1);
            InputEvent::KeyUpEvent ev2;
            ev2.Code = KeyCode::Text;
            EventsQueue->emplace_back(ev2);

            io.AddInputCharactersUTF8(sdl_event.text.text);
        } break;
        case SDL_DROPTEXT: {
            InputEvent::KeyDownEvent ev1;
            ev1.Code = KeyCode::Text;
            ev1.Text = sdl_event.drop.file;
            EventsQueue->emplace_back(ev1);
            InputEvent::KeyUpEvent ev2;
            ev2.Code = KeyCode::Text;
            EventsQueue->emplace_back(ev2);
            SDL_free(sdl_event.drop.file);
        } break;
        case SDL_DROPFILE: {
            if (auto file = DiskFileSystem::OpenFile(sdl_event.drop.file, false)) {
                auto stripped = false;
                auto size = file.GetSize();
                if (size > AppInput::DROP_FILE_STRIP_LENGHT) {
                    stripped = true;
                    size = AppInput::DROP_FILE_STRIP_LENGHT;
                }

                char buf[AppInput::DROP_FILE_STRIP_LENGHT + 1];
                if (file.Read(buf, size)) {
                    buf[size] = 0;
                    InputEvent::KeyDownEvent ev1;
                    ev1.Code = KeyCode::Text;
                    ev1.Text = _str("{}\n{}{}", sdl_event.drop.file, buf, stripped ? "..." : "");
                    EventsQueue->emplace_back(ev1);
                    InputEvent::KeyUpEvent ev2;
                    ev2.Code = KeyCode::Text;
                    EventsQueue->emplace_back(ev2);
                }
            }
            SDL_free(sdl_event.drop.file);
        } break;
        case SDL_APP_DIDENTERFOREGROUND: {
            _onPauseDispatcher();
        } break;
        case SDL_APP_DIDENTERBACKGROUND: {
            _onResumeDispatcher();
        } break;
        case SDL_APP_LOWMEMORY: {
            _onLowMemoryDispatcher();
        } break;
        case SDL_WINDOWEVENT: {
            Uint8 window_event = sdl_event.window.event;
            if (window_event == SDL_WINDOWEVENT_CLOSE) {
                Settings.Quit = true;
            }
            else if (window_event == SDL_WINDOWEVENT_ENTER) {
                _pendingMouseLeaveFrame = 0;
            }
            else if (window_event == SDL_WINDOWEVENT_LEAVE) {
                _pendingMouseLeaveFrame = ImGui::GetFrameCount() + 1;
            }
            else if (window_event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                io.AddFocusEvent(true);
            }
            else if (window_event == SDL_WINDOWEVENT_FOCUS_LOST) {
                io.AddFocusEvent(false);
            }
            else if (window_event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                auto* resized_window = SDL_GetWindowFromID(sdl_event.window.windowID);
                RUNTIME_ASSERT(resized_window);

                for (auto* window : copy(_allWindows)) {
                    if (static_cast<SDL_Window*>(window->_windowHandle) == resized_window) {
                        window->_onWindowSizeChangedDispatcher();
                    }
                }
            }
        } break;
        case SDL_QUIT: {
            Settings.Quit = true;
        } break;
        case SDL_APP_TERMINATING: {
            _onQuitDispatcher();

#if FO_WINDOWS && FO_DEBUG
            DeleteGlobalData();

            ::_CrtMemDumpAllObjectsSince(&CrtMemState);
#endif

            ExitApp(true);
        }
        default:
            break;
        }
    }

    // Setup display size
    int w;
    int h;
    SDL_GetWindowSize(static_cast<SDL_Window*>(MainWindow._windowHandle), &w, &h);

    if ((SDL_GetWindowFlags(static_cast<SDL_Window*>(MainWindow._windowHandle)) & SDL_WINDOW_MINIMIZED) != 0u) {
        w = h = 0;
    }

    int display_w;
    int display_h;
    SDL_GL_GetDrawableSize(static_cast<SDL_Window*>(MainWindow._windowHandle), &display_w, &display_h);

    io.DisplaySize = ImVec2(static_cast<float>(w), static_cast<float>(h));
    if (w > 0 && h > 0) {
        io.DisplayFramebufferScale = ImVec2(static_cast<float>(display_w) / static_cast<float>(w), static_cast<float>(display_h) / static_cast<float>(h));
    }

    // Setup time step
    uint64 cur_time = SDL_GetPerformanceCounter();
    io.DeltaTime = static_cast<float>(static_cast<double>(cur_time - _time) / static_cast<double>(_timeFrequency));
    _time = cur_time;

    // Mouse state
    if (_pendingMouseLeaveFrame != 0 && _pendingMouseLeaveFrame >= ImGui::GetFrameCount() && _mouseButtonsDown == 0) {
        io.AddMousePosEvent(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
        _pendingMouseLeaveFrame = 0;
    }

#if FO_WINDOWS || FO_LINUX || FO_MAC
    SDL_CaptureMouse(_mouseButtonsDown != 0 ? SDL_TRUE : SDL_FALSE);
    const bool is_app_focused = static_cast<SDL_Window*>(MainWindow._windowHandle) == SDL_GetKeyboardFocus();
#else
    const bool is_app_focused = (SDL_GetWindowFlags(static_cast<SDL_Window*>(MainWindow._windowHandle)) & SDL_WINDOW_INPUT_FOCUS) != 0;
#endif
    if (is_app_focused) {
        if (io.WantSetMousePos) {
            Input.SetMousePosition(static_cast<int>(io.MousePos.x), static_cast<int>(io.MousePos.y), &MainWindow);
        }

        if (_mouseCanUseGlobalState && _mouseButtonsDown == 0) {
            int mouse_x_global;
            int mouse_y_global;
            SDL_GetGlobalMouseState(&mouse_x_global, &mouse_y_global);
            int window_x;
            int window_y;
            SDL_GetWindowPosition(static_cast<SDL_Window*>(MainWindow._windowHandle), &window_x, &window_y);
            io.AddMousePosEvent(static_cast<float>(mouse_x_global - window_x), static_cast<float>(mouse_y_global - window_y));
        }
    }

    ImGui::NewFrame();

    _onFrameBeginDispatcher();
}

void Application::EndFrame()
{
    RUNTIME_ASSERT(RenderTargetTex == nullptr);

    // Render ImGui
    ImGui::Render();

    const auto* draw_data = ImGui::GetDrawData();

    const auto fb_width = static_cast<int>(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    const auto fb_height = static_cast<int>(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);

    if (fb_width > 0 && fb_height > 0) {
        const auto l = draw_data->DisplayPos.x;
        const auto r = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
        const auto t = draw_data->DisplayPos.y;
        const auto b = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
        const float ortho_projection[4][4] = {
            {2.0f / (r - l), 0.0f, 0.0f, 0.0f},
            {0.0f, 2.0f / (t - b), 0.0f, 0.0f},
            {0.0f, 0.0f, -1.0f, 0.0f},
            {(r + l) / (l - r), (t + b) / (b - t), 0.0f, 1.0f},
        };
        std::memcpy(_imguiEffect->ProjBuf.ProjMatrix, ortho_projection, sizeof(ortho_projection));

        // Scissor/clipping
        const auto clip_off = draw_data->DisplayPos;
        const auto clip_scale = draw_data->FramebufferScale;

        // Render command lists
        for (int cmd = 0; cmd < draw_data->CmdListsCount; cmd++) {
            const auto* cmd_list = draw_data->CmdLists[cmd];

            _imguiDrawBuf->Vertices2D.resize(cmd_list->VtxBuffer.Size);
            for (int i = 0; i < cmd_list->VtxBuffer.Size; i++) {
                auto& v = _imguiDrawBuf->Vertices2D[i];
                const auto& iv = cmd_list->VtxBuffer[i];
                v.X = iv.pos.x;
                v.Y = iv.pos.y;
                v.TU = iv.uv.x;
                v.TV = iv.uv.y;
                v.Diffuse = iv.col;
            }

            _imguiDrawBuf->Indices.resize(cmd_list->IdxBuffer.Size);
            for (int i = 0; i < cmd_list->IdxBuffer.Size; i++) {
                _imguiDrawBuf->Indices[i] = cmd_list->IdxBuffer[i];
            }

            _imguiDrawBuf->DataChanged = true;

            for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
                const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
                RUNTIME_ASSERT(pcmd->UserCallback == nullptr);

                const auto clip_rect_x = static_cast<int>((pcmd->ClipRect.x - clip_off.x) * clip_scale.x);
                const auto clip_rect_y = static_cast<int>((pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                const auto clip_rect_z = static_cast<int>((pcmd->ClipRect.z - clip_off.x) * clip_scale.x);
                const auto clip_rect_w = static_cast<int>((pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

                if (clip_rect_x < fb_width && clip_rect_y < fb_height && clip_rect_z >= 0 && clip_rect_w >= 0) {
                    ActiveRenderer->EnableScissor(clip_rect_x, fb_height - clip_rect_w, clip_rect_z - clip_rect_x, clip_rect_w - clip_rect_y);
                    _imguiEffect->DrawBuffer(_imguiDrawBuf, pcmd->IdxOffset, pcmd->ElemCount, static_cast<RenderTexture*>(pcmd->TextureId));
                    ActiveRenderer->DisableScissor();
                }
            }
        }
    }

    // Frame complete, swap buffers
    ActiveRenderer->Present();

    _onFrameEndDispatcher();
}

auto AppWindow::GetSize() const -> tuple<int, int>
{
    auto w = 1000;
    auto h = 1000;
    if (ActiveRendererType != RenderType::Null) {
        SDL_GetWindowSize(static_cast<SDL_Window*>(_windowHandle), &w, &h);
    }
    return {w, h};
}

void AppWindow::SetSize(int w, int h)
{
    NON_CONST_METHOD_HINT();

    if (ActiveRendererType != RenderType::Null) {
        SDL_SetWindowSize(static_cast<SDL_Window*>(_windowHandle), w, h);
        _onWindowSizeChangedDispatcher();
    }
}

auto AppWindow::GetPosition() const -> tuple<int, int>
{
    auto x = 0;
    auto y = 0;
    if (ActiveRendererType != RenderType::Null) {
        SDL_GetWindowPosition(static_cast<SDL_Window*>(_windowHandle), &x, &y);
    }
    return {x, y};
}

void AppWindow::SetPosition(int x, int y)
{
    NON_CONST_METHOD_HINT();

    if (ActiveRendererType != RenderType::Null) {
        SDL_SetWindowPosition(static_cast<SDL_Window*>(_windowHandle), x, y);
    }
}

auto AppWindow::IsFocused() const -> bool
{
    if (ActiveRendererType != RenderType::Null) {
        return (SDL_GetWindowFlags(static_cast<SDL_Window*>(_windowHandle)) & SDL_WINDOW_INPUT_FOCUS) != 0u;
    }

    return true;
}

void AppWindow::Minimize()
{
    NON_CONST_METHOD_HINT();

    if (ActiveRendererType != RenderType::Null) {
        SDL_MinimizeWindow(static_cast<SDL_Window*>(_windowHandle));
    }
}

auto AppWindow::IsFullscreen() const -> bool
{
    if (ActiveRendererType != RenderType::Null) {
        return (SDL_GetWindowFlags(static_cast<SDL_Window*>(_windowHandle)) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0u;
    }

    return false;
}

auto AppWindow::ToggleFullscreen(bool enable) -> bool
{
    NON_CONST_METHOD_HINT();

    if (ActiveRendererType == RenderType::Null) {
        return false;
    }

    const auto is_fullscreen = IsFullscreen();
    if (!is_fullscreen && enable) {
        if (SDL_SetWindowFullscreen(static_cast<SDL_Window*>(_windowHandle), SDL_WINDOW_FULLSCREEN_DESKTOP) == 0) {
            RUNTIME_ASSERT(IsFullscreen());
            return true;
        }
    }
    else if (is_fullscreen && !enable) {
        if (SDL_SetWindowFullscreen(static_cast<SDL_Window*>(_windowHandle), 0) == 0) {
            RUNTIME_ASSERT(!IsFullscreen());
            return true;
        }
    }

    return false;
}

void AppWindow::Blink()
{
    NON_CONST_METHOD_HINT();

    if (ActiveRendererType == RenderType::Null) {
        return;
    }

#if FO_WINDOWS && !FO_UWP
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version)
    if (SDL_GetWindowWMInfo(static_cast<SDL_Window*>(_windowHandle), &info) != 0) {
        ::FlashWindow(info.info.win.window, 1);
    }
#endif
}

void AppWindow::AlwaysOnTop(bool enable)
{
    NON_CONST_METHOD_HINT();

    if (ActiveRendererType == RenderType::Null) {
        return;
    }

#if FO_WINDOWS && !FO_UWP
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version)
    if (SDL_GetWindowWMInfo(static_cast<SDL_Window*>(_windowHandle), &info) != 0) {
        ::SetWindowPos(info.info.win.window, enable ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
    }
#endif
}

void AppWindow::Destroy()
{
    NON_CONST_METHOD_HINT();

    if (_windowHandle != nullptr && this != &App->MainWindow) {
        SDL_DestroyWindow(static_cast<SDL_Window*>(_windowHandle));
        _windowHandle = nullptr;
    }
}

auto AppRender::CreateTexture(uint width, uint height, bool linear_filtered, bool with_depth) -> RenderTexture*
{
    return ActiveRenderer->CreateTexture(width, height, linear_filtered, with_depth);
}

void AppRender::SetRenderTarget(RenderTexture* tex)
{
    ActiveRenderer->SetRenderTarget(tex);
    RenderTargetTex = tex;
}

auto AppRender::GetRenderTarget() -> RenderTexture*
{
    return RenderTargetTex;
}

void AppRender::ClearRenderTarget(optional<uint> color, bool depth, bool stencil)
{
    ActiveRenderer->ClearRenderTarget(color, depth, stencil);
}

void AppRender::EnableScissor(int x, int y, uint w, uint h)
{
    ActiveRenderer->EnableScissor(x, y, w, h);
}

void AppRender::DisableScissor()
{
    ActiveRenderer->DisableScissor();
}

auto AppRender::CreateDrawBuffer(bool is_static) -> RenderDrawBuffer*
{
    return ActiveRenderer->CreateDrawBuffer(is_static);
}

auto AppRender::CreateEffect(EffectUsage usage, string_view name, string_view defines, const RenderEffectLoader& loader) -> RenderEffect*
{
    return ActiveRenderer->CreateEffect(usage, name, defines, loader);
}

auto AppInput::GetMousePosition() const -> tuple<int, int>
{
    auto x = 100;
    auto y = 100;
    if (ActiveRendererType != RenderType::Null) {
        SDL_GetMouseState(&x, &y);
    }
    return {x, y};
}

void AppInput::SetMousePosition(int x, int y, const AppWindow* relative_to)
{
    if (ActiveRendererType != RenderType::Null) {
        if (relative_to != nullptr) {
            SDL_WarpMouseInWindow(static_cast<SDL_Window*>(relative_to->_windowHandle), x, y);
        }
        else {
            SDL_WarpMouseGlobal(x, y);
        }
    }
}

auto AppInput::PollEvent(InputEvent& event) -> bool
{
    if (!EventsQueue->empty()) {
        event = EventsQueue->front();
        EventsQueue->erase(EventsQueue->begin());
        return true;
    }
    return false;
}

void AppInput::PushEvent(const InputEvent& event)
{
    NextFrameEventsQueue->push_back(event);
}

void AppInput::SetClipboardText(string_view text)
{
    SDL_SetClipboardText(string(text).c_str());
}

auto AppInput::GetClipboardText() -> const string&
{
    _clipboardTextStorage = SDL_GetClipboardText();
    return _clipboardTextStorage;
}

auto AppAudio::IsEnabled() -> bool
{
    return AudioDeviceId >= 2;
}

auto AppAudio::GetStreamSize() -> uint
{
    RUNTIME_ASSERT(IsEnabled());

    return AudioSpec.size;
}

auto AppAudio::GetSilence() -> uchar
{
    RUNTIME_ASSERT(IsEnabled());

    return AudioSpec.silence;
}

void AppAudio::SetSource(AudioStreamCallback stream_callback)
{
    RUNTIME_ASSERT(IsEnabled());

    LockDevice();
    *AudioStreamWriter = std::move(stream_callback);
    UnlockDevice();
}

struct AppAudio::AudioConverter
{
    int Format {};
    int Channels {};
    int Rate {};
    SDL_AudioCVT Cvt {};
    bool NeedConvert {};
};

auto AppAudio::ConvertAudio(int format, int channels, int rate, vector<uchar>& buf) -> bool
{
    RUNTIME_ASSERT(IsEnabled());

    auto get_converter = [this, format, channels, rate]() -> AudioConverter* {
        const auto it = std::find_if(_converters.begin(), _converters.end(), [format, channels, rate](AudioConverter* c) { return c->Format == format && c->Channels == channels && c->Rate == rate; });

        if (it == _converters.end()) {
            SDL_AudioCVT cvt;
            const auto r = SDL_BuildAudioCVT(&cvt, static_cast<SDL_AudioFormat>(format), static_cast<Uint8>(channels), rate, AudioSpec.format, AudioSpec.channels, AudioSpec.freq);
            if (r == -1) {
                return nullptr;
            }

            _converters.push_back(new AudioConverter {format, channels, rate, cvt, r == 1});
            return _converters.back();
        }

        return *it;
    };

    auto* converter = get_converter();
    if (converter == nullptr) {
        return false;
    }

    if (converter->NeedConvert) {
        converter->Cvt.len = static_cast<int>(buf.size());
        converter->Cvt.buf = buf.data();
        buf.resize(static_cast<size_t>(converter->Cvt.len) * converter->Cvt.len_mult);

        if (SDL_ConvertAudio(&converter->Cvt) != 0) {
            return false;
        }

        buf.resize(converter->Cvt.len_cvt);
    }

    return true;
}

void AppAudio::MixAudio(uchar* output, uchar* buf, int volume)
{
    RUNTIME_ASSERT(IsEnabled());

    volume = std::clamp(volume, 0, 100) * SDL_MIX_MAXVOLUME / 100;
    SDL_MixAudioFormat(output, buf, AudioSpec.format, AudioSpec.size, volume);
}

void AppAudio::LockDevice()
{
    RUNTIME_ASSERT(IsEnabled());

    SDL_LockAudioDevice(AudioDeviceId);
}

void AppAudio::UnlockDevice()
{
    RUNTIME_ASSERT(IsEnabled());

    SDL_UnlockAudioDevice(AudioDeviceId);
}

void MessageBox::ShowErrorMessage(string_view title, string_view message, string_view traceback)
{
#if FO_WEB || FO_ANDROID || FO_IOS
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, string(title).c_str(), string(message).c_str(), nullptr);

#else
    auto verb_message = string(message);

    if (!traceback.empty()) {
        verb_message += _str("\n{}", traceback);
    }

    SDL_MessageBoxButtonData copy_button;
    SDL_zero(copy_button);
    copy_button.buttonid = 0;
    copy_button.text = "Copy";

    SDL_MessageBoxButtonData close_button;
    SDL_zero(close_button);
    close_button.buttonid = 1;
    close_button.text = "Close";

    // Todo: fix workaround for strange behaviour of button focus
#if FO_WINDOWS
    copy_button.flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
    copy_button.flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
#else
    close_button.flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
    close_button.flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
#endif

    const auto title_str = string(title);

    const SDL_MessageBoxButtonData buttons[] = {close_button, copy_button};
    SDL_MessageBoxData data;
    SDL_zero(data);
    data.flags = SDL_MESSAGEBOX_ERROR;
    data.title = title_str.c_str();
    data.message = verb_message.c_str();
    data.numbuttons = 2;
    data.buttons = buttons;

    auto buttonid = 0;
    while (SDL_ShowMessageBox(&data, &buttonid) == 0 && buttonid == 0) {
        SDL_SetClipboardText(verb_message.c_str());
    }
#endif
}

static ImGuiKey KeycodeToImGuiKey(int keycode)
{
    switch (keycode) {
    case SDLK_TAB:
        return ImGuiKey_Tab;
    case SDLK_LEFT:
        return ImGuiKey_LeftArrow;
    case SDLK_RIGHT:
        return ImGuiKey_RightArrow;
    case SDLK_UP:
        return ImGuiKey_UpArrow;
    case SDLK_DOWN:
        return ImGuiKey_DownArrow;
    case SDLK_PAGEUP:
        return ImGuiKey_PageUp;
    case SDLK_PAGEDOWN:
        return ImGuiKey_PageDown;
    case SDLK_HOME:
        return ImGuiKey_Home;
    case SDLK_END:
        return ImGuiKey_End;
    case SDLK_INSERT:
        return ImGuiKey_Insert;
    case SDLK_DELETE:
        return ImGuiKey_Delete;
    case SDLK_BACKSPACE:
        return ImGuiKey_Backspace;
    case SDLK_SPACE:
        return ImGuiKey_Space;
    case SDLK_RETURN:
        return ImGuiKey_Enter;
    case SDLK_ESCAPE:
        return ImGuiKey_Escape;
    case SDLK_QUOTE:
        return ImGuiKey_Apostrophe;
    case SDLK_COMMA:
        return ImGuiKey_Comma;
    case SDLK_MINUS:
        return ImGuiKey_Minus;
    case SDLK_PERIOD:
        return ImGuiKey_Period;
    case SDLK_SLASH:
        return ImGuiKey_Slash;
    case SDLK_SEMICOLON:
        return ImGuiKey_Semicolon;
    case SDLK_EQUALS:
        return ImGuiKey_Equal;
    case SDLK_LEFTBRACKET:
        return ImGuiKey_LeftBracket;
    case SDLK_BACKSLASH:
        return ImGuiKey_Backslash;
    case SDLK_RIGHTBRACKET:
        return ImGuiKey_RightBracket;
    case SDLK_BACKQUOTE:
        return ImGuiKey_GraveAccent;
    case SDLK_CAPSLOCK:
        return ImGuiKey_CapsLock;
    case SDLK_SCROLLLOCK:
        return ImGuiKey_ScrollLock;
    case SDLK_NUMLOCKCLEAR:
        return ImGuiKey_NumLock;
    case SDLK_PRINTSCREEN:
        return ImGuiKey_PrintScreen;
    case SDLK_PAUSE:
        return ImGuiKey_Pause;
    case SDLK_KP_0:
        return ImGuiKey_Keypad0;
    case SDLK_KP_1:
        return ImGuiKey_Keypad1;
    case SDLK_KP_2:
        return ImGuiKey_Keypad2;
    case SDLK_KP_3:
        return ImGuiKey_Keypad3;
    case SDLK_KP_4:
        return ImGuiKey_Keypad4;
    case SDLK_KP_5:
        return ImGuiKey_Keypad5;
    case SDLK_KP_6:
        return ImGuiKey_Keypad6;
    case SDLK_KP_7:
        return ImGuiKey_Keypad7;
    case SDLK_KP_8:
        return ImGuiKey_Keypad8;
    case SDLK_KP_9:
        return ImGuiKey_Keypad9;
    case SDLK_KP_PERIOD:
        return ImGuiKey_KeypadDecimal;
    case SDLK_KP_DIVIDE:
        return ImGuiKey_KeypadDivide;
    case SDLK_KP_MULTIPLY:
        return ImGuiKey_KeypadMultiply;
    case SDLK_KP_MINUS:
        return ImGuiKey_KeypadSubtract;
    case SDLK_KP_PLUS:
        return ImGuiKey_KeypadAdd;
    case SDLK_KP_ENTER:
        return ImGuiKey_KeypadEnter;
    case SDLK_KP_EQUALS:
        return ImGuiKey_KeypadEqual;
    case SDLK_LCTRL:
        return ImGuiKey_LeftCtrl;
    case SDLK_LSHIFT:
        return ImGuiKey_LeftShift;
    case SDLK_LALT:
        return ImGuiKey_LeftAlt;
    case SDLK_LGUI:
        return ImGuiKey_LeftSuper;
    case SDLK_RCTRL:
        return ImGuiKey_RightCtrl;
    case SDLK_RSHIFT:
        return ImGuiKey_RightShift;
    case SDLK_RALT:
        return ImGuiKey_RightAlt;
    case SDLK_RGUI:
        return ImGuiKey_RightSuper;
    case SDLK_APPLICATION:
        return ImGuiKey_Menu;
    case SDLK_0:
        return ImGuiKey_0;
    case SDLK_1:
        return ImGuiKey_1;
    case SDLK_2:
        return ImGuiKey_2;
    case SDLK_3:
        return ImGuiKey_3;
    case SDLK_4:
        return ImGuiKey_4;
    case SDLK_5:
        return ImGuiKey_5;
    case SDLK_6:
        return ImGuiKey_6;
    case SDLK_7:
        return ImGuiKey_7;
    case SDLK_8:
        return ImGuiKey_8;
    case SDLK_9:
        return ImGuiKey_9;
    case SDLK_a:
        return ImGuiKey_A;
    case SDLK_b:
        return ImGuiKey_B;
    case SDLK_c:
        return ImGuiKey_C;
    case SDLK_d:
        return ImGuiKey_D;
    case SDLK_e:
        return ImGuiKey_E;
    case SDLK_f:
        return ImGuiKey_F;
    case SDLK_g:
        return ImGuiKey_G;
    case SDLK_h:
        return ImGuiKey_H;
    case SDLK_i:
        return ImGuiKey_I;
    case SDLK_j:
        return ImGuiKey_J;
    case SDLK_k:
        return ImGuiKey_K;
    case SDLK_l:
        return ImGuiKey_L;
    case SDLK_m:
        return ImGuiKey_M;
    case SDLK_n:
        return ImGuiKey_N;
    case SDLK_o:
        return ImGuiKey_O;
    case SDLK_p:
        return ImGuiKey_P;
    case SDLK_q:
        return ImGuiKey_Q;
    case SDLK_r:
        return ImGuiKey_R;
    case SDLK_s:
        return ImGuiKey_S;
    case SDLK_t:
        return ImGuiKey_T;
    case SDLK_u:
        return ImGuiKey_U;
    case SDLK_v:
        return ImGuiKey_V;
    case SDLK_w:
        return ImGuiKey_W;
    case SDLK_x:
        return ImGuiKey_X;
    case SDLK_y:
        return ImGuiKey_Y;
    case SDLK_z:
        return ImGuiKey_Z;
    case SDLK_F1:
        return ImGuiKey_F1;
    case SDLK_F2:
        return ImGuiKey_F2;
    case SDLK_F3:
        return ImGuiKey_F3;
    case SDLK_F4:
        return ImGuiKey_F4;
    case SDLK_F5:
        return ImGuiKey_F5;
    case SDLK_F6:
        return ImGuiKey_F6;
    case SDLK_F7:
        return ImGuiKey_F7;
    case SDLK_F8:
        return ImGuiKey_F8;
    case SDLK_F9:
        return ImGuiKey_F9;
    case SDLK_F10:
        return ImGuiKey_F10;
    case SDLK_F11:
        return ImGuiKey_F11;
    case SDLK_F12:
        return ImGuiKey_F12;
    default:
        break;
    }
    return ImGuiKey_None;
}
