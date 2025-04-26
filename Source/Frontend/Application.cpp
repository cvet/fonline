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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "ImGuiStuff.h"
#include "Log.h"
#include "Platform.h"
#include "StringUtils.h"
#include "Version-Include.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_audio.h"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_video.h"

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

#include "WinApiUndef-Include.h"

Application* App;

#if FO_WINDOWS && FO_DEBUG
static _CrtMemState CrtMemState;
#endif

static ImGuiKey KeycodeToImGuiKey(SDL_Keycode keycode);

// Todo: move all these statics to App class fields
static unique_ptr<Renderer> ActiveRenderer {};
static RenderType ActiveRendererType {};
static RenderTexture* RenderTargetTex {};

static unique_ptr<vector<InputEvent>> EventsQueue {};
static unique_ptr<vector<InputEvent>> NextFrameEventsQueue {};

static SDL_AudioStream* AudioStream {};
static SDL_AudioSpec AudioSpec {};
static unique_ptr<AppAudio::AudioStreamCallback> AudioStreamWriter {};
static unique_ptr<vector<uint8>> AudioStreamBuf {};

static unique_ptr<RenderTexture> FontTex {};

static int MaxAtlasWidth {};
static int MaxAtlasHeight {};
static int MaxBones {};
const int& AppRender::MAX_ATLAS_WIDTH {MaxAtlasWidth};
const int& AppRender::MAX_ATLAS_HEIGHT {MaxAtlasHeight};
const int& AppRender::MAX_BONES {MaxBones};
const int AppAudio::AUDIO_FORMAT_U8 {SDL_AUDIO_U8};
const int AppAudio::AUDIO_FORMAT_S16 {SDL_AUDIO_S16};

static auto WindowPosToScreenPos(ipos pos) -> ipos
{
    const auto vp = ActiveRenderer->GetViewPort();

    const auto screen_x = iround(static_cast<float>(pos.x - vp.Left) / static_cast<float>(vp.Width()) * static_cast<float>(App->Settings.ScreenWidth));
    const auto screen_y = iround(static_cast<float>(pos.y - vp.Top) / static_cast<float>(vp.Height()) * static_cast<float>(App->Settings.ScreenHeight));

    return {screen_x, screen_y};
}

static auto ScreenPosToWindowPos(ipos pos) -> ipos
{
    const auto vp = ActiveRenderer->GetViewPort();

    const auto win_x = vp.Left + iround(static_cast<float>(pos.x) / static_cast<float>(App->Settings.ScreenWidth) * static_cast<float>(vp.Width()));
    const auto win_y = vp.Top + iround(static_cast<float>(pos.y) / static_cast<float>(App->Settings.ScreenHeight) * static_cast<float>(vp.Height()));

    return {win_x, win_y};
}

void InitApp(int argc, char** argv, bool client_mode)
{
    STACK_TRACE_ENTRY();

    SetThisThreadName("Main");

    // Ensure that we call init only once
    static std::once_flag once;
    auto first_call = false;
    std::call_once(once, [&first_call] { first_call = true; });

    if (!first_call) {
        throw AppInitException("InitApp must be called only once");
    }

    const auto need_fork = [&] {
        for (int i = 0; i < argc; i++) {
            if (string_view(argv[i]) == "--fork") {
                return true;
            }
        }
        return false;
    };
    if (need_fork()) {
        Platform::ForkProcess();
    }

    // Unhandled exceptions handler
#if FO_WINDOWS || FO_LINUX || FO_MAC
    if (!IsRunInDebugger()) {
        [[maybe_unused]] static backward::SignalHandling sh;
        assert(sh.loaded());
    }
#endif

    CreateGlobalData();

#if FO_TRACY
    TracySetProgramName(FO_GAME_NAME);
#endif

#if !FO_WEB
    if (const auto exe_path = Platform::GetExePath()) {
        LogToFile(strex("{}.log", strex(exe_path.value()).extractFileName().eraseFileExtension()));
    }
    else {
        LogToFile(strex("{}.log", FO_DEV_NAME));
    }
#endif

    WriteLog("Starting {}", FO_GAME_NAME);

    App = SafeAlloc::MakeRaw<Application>(argc, argv, client_mode);
}

void ExitApp(bool success) noexcept
{
    STACK_TRACE_ENTRY();

    const auto code = success ? EXIT_SUCCESS : EXIT_FAILURE;
#if !FO_WEB && !FO_MAC && !FO_IOS && !FO_ANDROID
    std::quick_exit(code);
#else
    std::exit(code);
#endif
}

auto RenderEffect::CanBatch(const RenderEffect* other) const -> bool
{
    STACK_TRACE_ENTRY();

    if (_name != other->_name) {
        return false;
    }
    if (Usage != other->Usage) {
        return false;
    }
    if (MainTex != other->MainTex) {
        return false;
    }
    return true;
}

static unique_ptr<unordered_map<SDL_Keycode, KeyCode>> KeysMap {};
static unique_ptr<unordered_map<int, MouseButton>> MouseButtonsMap {};

Application::Application(int argc, char** argv, bool client_mode) :
    Settings(argc, argv, client_mode)
{
    STACK_TRACE_ENTRY();

    SDL_SetMemoryFunctions(&MemMalloc, &MemCalloc, &MemRealloc, &MemFree);

    SDL_SetHint(SDL_HINT_APP_NAME, FO_GAME_NAME);
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "0");

    if (Settings.NullRenderer) {
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "dummy");
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    }

    if constexpr (FO_ANDROID) {
        SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "0");
    }

    // Initialize input events
    if (!SDL_InitSubSystem(SDL_INIT_EVENTS)) {
        throw AppInitException("SDL_InitSubSystem SDL_INIT_EVENTS failed", SDL_GetError());
    }

    EventsQueue = SafeAlloc::MakeUnique<vector<InputEvent>>();
    NextFrameEventsQueue = SafeAlloc::MakeUnique<vector<InputEvent>>();

    KeysMap = SafeAlloc::MakeUnique<unordered_map<SDL_Keycode, KeyCode>>(unordered_map<SDL_Keycode, KeyCode> {
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
    });

    MouseButtonsMap = SafeAlloc::MakeUnique<unordered_map<int, MouseButton>>(unordered_map<int, MouseButton> {
        {SDL_BUTTON_LEFT, MouseButton::Left},
        {SDL_BUTTON_RIGHT, MouseButton::Right},
        {SDL_BUTTON_MIDDLE, MouseButton::Middle},
        {SDL_BUTTON_MASK(4), MouseButton::Ext0},
        {SDL_BUTTON_MASK(5), MouseButton::Ext1},
        {SDL_BUTTON_MASK(6), MouseButton::Ext2},
        {SDL_BUTTON_MASK(7), MouseButton::Ext3},
        {SDL_BUTTON_MASK(8), MouseButton::Ext4},
    });

    // Initialize audio
    if (!Settings.DisableAudio) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            const auto stream_callback = [](void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount) {
                UNUSED_VARIABLE(userdata);
                UNUSED_VARIABLE(total_amount);

                if (additional_amount > 0) {
                    if (static_cast<size_t>(additional_amount) > AudioStreamBuf->size()) {
                        AudioStreamBuf->resize(static_cast<size_t>(additional_amount) * 2);
                    }

                    const auto silence = static_cast<uint8>(SDL_GetSilenceValueForFormat(AudioSpec.format));

                    MemFill(AudioStreamBuf->data(), silence, additional_amount);

                    if (*AudioStreamWriter) {
                        (*AudioStreamWriter)(silence, {AudioStreamBuf->data(), static_cast<size_t>(additional_amount)});
                    }

                    SDL_PutAudioStreamData(stream, AudioStreamBuf->data(), additional_amount);
                }
            };

            auto* audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr, stream_callback, nullptr);

            if (audio_stream != nullptr) {
                if (SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(audio_stream), &AudioSpec, nullptr)) {
                    if (SDL_ResumeAudioStreamDevice(audio_stream)) {
                        AudioStream = audio_stream;
                        AudioStreamWriter = SafeAlloc::MakeUnique<AppAudio::AudioStreamCallback>();
                        AudioStreamBuf = SafeAlloc::MakeUnique<vector<uint8>>(vector<uint8>());
                    }
                    else {
                        WriteLog("SDL resume audio device failed, error {}", SDL_GetError());
                    }
                }
                else {
                    WriteLog("SDL get audio device format failed, error {}", SDL_GetError());
                }
            }
            else {
                WriteLog("SDL open audio device stream failed, error {}", SDL_GetError());
            }
        }
        else {
            WriteLog("SDL init audio subsystem failed, error {}", SDL_GetError());
        }
    }

    // First choose render type by user preference
    if (Settings.NullRenderer) {
        ActiveRendererType = RenderType::Null;
        ActiveRenderer = SafeAlloc::MakeUnique<Null_Renderer>();
    }
#if FO_HAVE_OPENGL
    else if (Settings.ForceOpenGL) {
        ActiveRendererType = RenderType::OpenGL;
        ActiveRenderer = SafeAlloc::MakeUnique<OpenGL_Renderer>();
    }
#endif
#if FO_HAVE_DIRECT_3D
    else if (Settings.ForceDirect3D) {
        ActiveRendererType = RenderType::Direct3D;
        ActiveRenderer = SafeAlloc::MakeUnique<Direct3D_Renderer>();
    }
#endif
#if FO_HAVE_METAL
    else if (Settings.ForceMetal) {
        ActiveRendererType = RenderType::Metal;
        throw NotImplementedException(LINE_STR);
    }
#endif
#if FO_HAVE_VULKAN
    else if (Settings.ForceVulkan) {
        ActiveRendererType = RenderType::Vulkan;
        throw NotImplementedException(LINE_STR);
    }
#endif

    // If none of selected then evaluate automatic selection
#if FO_HAVE_DIRECT_3D
    if (!ActiveRenderer) {
        ActiveRendererType = RenderType::Direct3D;
        ActiveRenderer = SafeAlloc::MakeUnique<Direct3D_Renderer>();
    }
#endif
#if FO_HAVE_METAL
    if (!ActiveRenderer) {
        ActiveRendererType = RenderType::Metal;
    }
#endif
#if FO_HAVE_VULKAN
    if (!ActiveRenderer) {
        ActiveRendererType = RenderType::Vulkan;
    }
#endif
#if FO_HAVE_OPENGL
    if (!ActiveRenderer) {
        ActiveRendererType = RenderType::OpenGL;
        ActiveRenderer = SafeAlloc::MakeUnique<OpenGL_Renderer>();
    }
#endif

    if (!ActiveRenderer) {
        throw AppInitException("No renderer selected");
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
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
            throw AppInitException("SDL_InitSubSystem SDL_INIT_VIDEO failed", SDL_GetError());
        }

        if (Settings.ClientMode) {
            SDL_DisableScreenSaver();
        }

        if (Settings.ClientMode && Settings.HideNativeCursor) {
            SDL_HideCursor();
        }

        if (_isTablet) {
            const auto display_id = SDL_GetPrimaryDisplay();
            const SDL_DisplayMode* display_mode = SDL_GetCurrentDisplayMode(display_id);
            Settings.ScreenWidth = std::max(display_mode->w, display_mode->h);
            Settings.ScreenHeight = std::min(display_mode->w, display_mode->h);

            const auto ratio = static_cast<float>(Settings.ScreenWidth) / static_cast<float>(Settings.ScreenHeight);
            Settings.ScreenHeight = 768;
            Settings.ScreenWidth = iround(static_cast<float>(Settings.ScreenHeight) * ratio);

            Settings.Fullscreen = true;
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

        Settings.Fullscreen = false;
#endif

        MainWindow._windowHandle = CreateInternalWindow({Settings.ScreenWidth, Settings.ScreenHeight});
        _allWindows.emplace_back(&MainWindow);

        ActiveRenderer->Init(Settings, MainWindow._windowHandle);

        if (Settings.ClientMode && Settings.AlwaysOnTop) {
            MainWindow.AlwaysOnTop(true);
        }

        SDL_StartTextInput(static_cast<SDL_Window*>(MainWindow._windowHandle));

        // Skip SDL allocations from profiling
#if FO_WINDOWS && FO_DEBUG
        ::_CrtMemCheckpoint(&CrtMemState);
#endif

        // Init Dear ImGui
        ImGuiExt::Init();

        ImGuiIO& io = ImGui::GetIO();
        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();

        io.BackendPlatformName = "fonline";
        io.WantSaveIniSettings = false;
        io.IniFilename = nullptr;

        const string sdl_backend = SDL_GetCurrentVideoDriver();
        vector<string> global_mouse_whitelist = {"windows", "cocoa", "x11", "DIVE", "VMAN"};
        _mouseCanUseGlobalState = std::any_of(global_mouse_whitelist.begin(), global_mouse_whitelist.end(), [&sdl_backend](auto& entry) { return strex(sdl_backend).startsWith(entry); });

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

        platform_io.Platform_GetClipboardTextFn = [](ImGuiContext*) -> const char* { return App->Input.GetClipboardText().c_str(); };
        platform_io.Platform_SetClipboardTextFn = [](ImGuiContext*, const char* text) { App->Input.SetClipboardText(text); };
        platform_io.Platform_ClipboardUserData = nullptr;

#if FO_WINDOWS
        if (ActiveRendererType != RenderType::Null) {
            const auto sdl_windows_props = SDL_GetWindowProperties(static_cast<SDL_Window*>(MainWindow._windowHandle));
            ImGui::GetMainViewport()->PlatformHandleRaw = static_cast<HWND>(SDL_GetPointerProperty(sdl_windows_props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
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

        auto font_tex = ActiveRenderer->CreateTexture({width, height}, true, false);
        font_tex->UpdateTextureRegion({}, {width, height}, reinterpret_cast<const ucolor*>(pixels));
        io.Fonts->TexID = reinterpret_cast<ImTextureID>(font_tex.get());
        FontTex = std::move(font_tex);

        // Default effect
        if (Settings.EmbeddedResources != "@Disabled") {
            FileSystem base_fs;
            base_fs.AddDataSource(Settings.EmbeddedResources);

            if (base_fs.ReadFileHeader("Effects/ImGui_Default.fofx")) {
                _imguiEffect = ActiveRenderer->CreateEffect(EffectUsage::ImGui, "Effects/ImGui_Default.fofx", [&base_fs](string_view path) -> string {
                    const auto file = base_fs.ReadFile(path);
                    RUNTIME_ASSERT_STR(file, "ImGui_Default effect not found");
                    return file.GetStr();
                });
            }
        }

        _imguiDrawBuf = ActiveRenderer->CreateDrawBuffer(false);
    }

    // Start timings
    _timeFrequency = SDL_GetPerformanceFrequency();
    _time = SDL_GetPerformanceCounter();
}

void Application::OpenLink(string_view link)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    SDL_OpenURL(string(link).c_str());
}

void Application::SetImGuiEffect(unique_ptr<RenderEffect> effect)
{
    STACK_TRACE_ENTRY();

    _imguiEffect = std::move(effect);
}

#if FO_IOS
void Application::SetMainLoopCallback(void (*callback)(void*))
{
    STACK_TRACE_ENTRY();

    SDL_SetiOSAnimationCallback(static_cast<SDL_Window*>(MainWindow._windowHandle), 1, callback, nullptr);
}
#endif

auto Application::CreateChildWindow(isize size) -> AppWindow*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(size);

    throw NotImplementedException(LINE_STR);

    /*auto* sdl_window = CreateInternalWindow(width, height);
    auto* window = SafeAlloc::MakeRaw<AppWindow>();
    window->_windowHandle = sdl_window;
    _allWindows.emplace_back(window);
    return window;*/
}

auto Application::CreateInternalWindow(isize size) -> WindowInternalHandle*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    // Dummy window pointer
    if (Settings.NullRenderer) {
        return SafeAlloc::MakeRaw<int>(42);
    }

    // Initialize window
    SDL_PropertiesID props = SDL_CreateProperties();

    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, 1);

    if (!Settings.ClientMode || Settings.WindowResizable) {
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, 1);
    }

#if FO_HAVE_OPENGL
    if (ActiveRendererType == RenderType::OpenGL) {
#if !FO_WEB
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, 1);
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
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#endif
    }
#endif

#if FO_HAVE_METAL
    if (ActiveRendererType == RenderType::Metal) {
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_METAL_BOOLEAN, 1);
    }
#endif

#if FO_HAVE_VULKAN
    if (ActiveRendererType == RenderType::Vulkan) {
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN, 1);
    }
#endif

    if (_isTablet) {
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, 1);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN, 1);
    }

    if (Settings.WindowCentered) {
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
    }

    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, FO_GAME_NAME);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, size.width);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, size.height);

    auto* sdl_window = SDL_CreateWindowWithProperties(props);

    if (sdl_window == nullptr) {
        throw AppInitException("Window creation failed", SDL_GetError());
    }

    if (!_isTablet) {
        SDL_SetWindowFullscreenMode(sdl_window, nullptr);

        if (Settings.ClientMode && Settings.Fullscreen) {
            SDL_SetWindowFullscreen(sdl_window, true);
        }
    }

    const auto display_id = SDL_GetDisplayForWindow(sdl_window);
    const SDL_DisplayMode* display_mode = SDL_GetCurrentDisplayMode(display_id);
    const_cast<int&>(Settings.MonitorWidth) = display_mode->w;
    const_cast<int&>(Settings.MonitorHeight) = display_mode->h;

    return sdl_window;
}

void Application::BeginFrame()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(RenderTargetTex == nullptr);
    ActiveRenderer->ClearRenderTarget(Settings.ClientMode ? ucolor {0, 0, 0} : ucolor {150, 150, 150});

    ImGuiIO& io = ImGui::GetIO();

    if (!NextFrameEventsQueue->empty()) {
        EventsQueue->insert(EventsQueue->end(), NextFrameEventsQueue->begin(), NextFrameEventsQueue->end());
        NextFrameEventsQueue->clear();
    }

    SDL_PumpEvents();

    SDL_Event sdl_event;

    while (SDL_PollEvent(&sdl_event)) {
        switch (sdl_event.type) {
        case SDL_EVENT_MOUSE_MOTION: {
            InputEvent::MouseMoveEvent ev;
            const auto screen_pos = WindowPosToScreenPos({iround(sdl_event.motion.x), iround(sdl_event.motion.y)});
            ev.MouseX = screen_pos.x;
            ev.MouseY = screen_pos.y;
            const auto vp = ActiveRenderer->GetViewPort();
            const auto x_ratio = static_cast<float>(App->Settings.ScreenWidth) / static_cast<float>(vp.Width());
            const auto y_ratio = static_cast<float>(App->Settings.ScreenHeight) / static_cast<float>(vp.Height());
            ev.DeltaX = iround(sdl_event.motion.xrel * x_ratio);
            ev.DeltaY = iround(sdl_event.motion.yrel * y_ratio);
            EventsQueue->emplace_back(ev);

            io.AddMousePosEvent(static_cast<float>(ev.MouseX), static_cast<float>(ev.MouseY));
        } break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            if (sdl_event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
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
                io.AddMouseButtonEvent(mouse_button, (sdl_event.type == SDL_EVENT_MOUSE_BUTTON_DOWN));
                _mouseButtonsDown = sdl_event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? (_mouseButtonsDown | (1 << mouse_button)) : (_mouseButtonsDown & ~(1 << mouse_button));
            }
        } break;
        case SDL_EVENT_FINGER_MOTION: {
            throw InvalidCallException("SDL_FINGERMOTION");
        }
        case SDL_EVENT_FINGER_DOWN: {
            throw InvalidCallException("SDL_FINGERDOWN");
        }
        case SDL_EVENT_FINGER_UP: {
            throw InvalidCallException("SDL_FINGERUP");
        }
        case SDL_EVENT_MOUSE_WHEEL: {
            InputEvent::MouseWheelEvent ev;
            ev.Delta = iround(sdl_event.wheel.y);
            EventsQueue->emplace_back(ev);

            float wheel_x = sdl_event.wheel.x > 0 ? 1.0f : (sdl_event.wheel.x < 0 ? -1.0f : 0.0f);
            float wheel_y = sdl_event.wheel.y > 0 ? 1.0f : (sdl_event.wheel.y < 0 ? -1.0f : 0.0f);
            io.AddMouseWheelEvent(wheel_x, wheel_y);
        } break;
        case SDL_EVENT_KEY_UP:
        case SDL_EVENT_KEY_DOWN: {
            if (sdl_event.type == SDL_EVENT_KEY_DOWN) {
                InputEvent::KeyDownEvent ev;
                ev.Code = (*KeysMap)[sdl_event.key.scancode];
                EventsQueue->emplace_back(ev);

                if (ev.Code == KeyCode::Escape && io.KeyShift) {
                    RequestQuit();
                }
            }
            else {
                InputEvent::KeyUpEvent ev;
                ev.Code = (*KeysMap)[sdl_event.key.scancode];
                EventsQueue->emplace_back(ev);
            }

            const auto sdl_key_mods = sdl_event.key.mod;
            io.AddKeyEvent(ImGuiMod_Ctrl, (sdl_key_mods & SDL_KMOD_CTRL) != 0);
            io.AddKeyEvent(ImGuiMod_Shift, (sdl_key_mods & SDL_KMOD_SHIFT) != 0);
            io.AddKeyEvent(ImGuiMod_Alt, (sdl_key_mods & SDL_KMOD_ALT) != 0);
            io.AddKeyEvent(ImGuiMod_Super, (sdl_key_mods & SDL_KMOD_GUI) != 0);

            ImGuiKey key = KeycodeToImGuiKey(sdl_event.key.key);
            io.AddKeyEvent(key, sdl_event.type == SDL_EVENT_KEY_DOWN);
            io.SetKeyEventNativeData(key, static_cast<int>(sdl_event.key.key), sdl_event.key.scancode, sdl_event.key.scancode);
        } break;
        case SDL_EVENT_TEXT_INPUT: {
            InputEvent::KeyDownEvent ev1;
            ev1.Code = KeyCode::Text;
            ev1.Text = sdl_event.text.text;
            EventsQueue->emplace_back(ev1);
            InputEvent::KeyUpEvent ev2;
            ev2.Code = KeyCode::Text;
            EventsQueue->emplace_back(ev2);

            io.AddInputCharactersUTF8(sdl_event.text.text);
        } break;
        case SDL_EVENT_DROP_TEXT: {
            InputEvent::KeyDownEvent ev1;
            ev1.Code = KeyCode::Text;
            ev1.Text = sdl_event.drop.data;
            EventsQueue->emplace_back(ev1);
            InputEvent::KeyUpEvent ev2;
            ev2.Code = KeyCode::Text;
            EventsQueue->emplace_back(ev2);
        } break;
        case SDL_EVENT_DROP_FILE: {
            if (auto file = DiskFileSystem::OpenFile(sdl_event.drop.data, false)) {
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
                    ev1.Text = strex("{}\n{}{}", sdl_event.drop.data, buf, stripped ? "..." : "");
                    EventsQueue->emplace_back(ev1);
                    InputEvent::KeyUpEvent ev2;
                    ev2.Code = KeyCode::Text;
                    EventsQueue->emplace_back(ev2);
                }
            }
        } break;
        case SDL_EVENT_TEXT_EDITING: {
        } break;
        case SDL_EVENT_DID_ENTER_FOREGROUND: {
            _onPauseDispatcher();
        } break;
        case SDL_EVENT_DID_ENTER_BACKGROUND: {
            _onResumeDispatcher();
        } break;
        case SDL_EVENT_LOW_MEMORY: {
            _onLowMemoryDispatcher();
        } break;
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
            RequestQuit();
        } break;
        case SDL_EVENT_WINDOW_MOUSE_ENTER: {
            _pendingMouseLeaveFrame = 0;
        } break;
        case SDL_EVENT_WINDOW_MOUSE_LEAVE: {
            _pendingMouseLeaveFrame = ImGui::GetFrameCount() + 1;
        } break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED: {
            io.AddFocusEvent(true);
        } break;
        case SDL_EVENT_WINDOW_FOCUS_LOST: {
            io.AddFocusEvent(false);
        } break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
            auto* resized_window = SDL_GetWindowFromID(sdl_event.window.windowID);
            RUNTIME_ASSERT(resized_window);

            int width = 0;
            int height = 0;
            SDL_GetWindowSizeInPixels(resized_window, &width, &height);
            ActiveRenderer->OnResizeWindow({width, height});

            for (auto* window : copy(_allWindows)) {
                if (static_cast<SDL_Window*>(window->_windowHandle) == resized_window) {
                    window->_onWindowSizeChangedDispatcher();
                }
            }
        } break;
        case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED: {
            auto* window = SDL_GetWindowFromID(sdl_event.window.windowID);
            const auto display_id = SDL_GetDisplayForWindow(window);
            const SDL_DisplayMode* display_mode = SDL_GetCurrentDisplayMode(display_id);
            const_cast<int&>(Settings.MonitorWidth) = display_mode->w;
            const_cast<int&>(Settings.MonitorHeight) = display_mode->h;
        } break;
        case SDL_EVENT_QUIT: {
            RequestQuit();
        } break;
        case SDL_EVENT_TERMINATING: {
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
    if (ActiveRendererType != RenderType::Null) {
        io.DisplaySize = ImVec2(static_cast<float>(App->Settings.ScreenWidth), static_cast<float>(App->Settings.ScreenHeight));
    }

    // Setup time step
    uint64 cur_time = SDL_GetPerformanceCounter();
    io.DeltaTime = static_cast<float>(static_cast<double>(cur_time - _time) / static_cast<double>(_timeFrequency));
    _time = cur_time;

    // Mouse state
    if (ActiveRendererType != RenderType::Null) {
        if (_pendingMouseLeaveFrame != 0 && _pendingMouseLeaveFrame >= ImGui::GetFrameCount() && _mouseButtonsDown == 0) {
            io.AddMousePosEvent(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
            _pendingMouseLeaveFrame = 0;
        }

        SDL_SetWindowMouseGrab(static_cast<SDL_Window*>(MainWindow._windowHandle), MainWindow._grabbed);

#if FO_WINDOWS || FO_LINUX || FO_MAC
        const bool is_app_focused = static_cast<SDL_Window*>(MainWindow._windowHandle) == SDL_GetKeyboardFocus();
#else
        const bool is_app_focused = (SDL_GetWindowFlags(static_cast<SDL_Window*>(MainWindow._windowHandle)) & SDL_WINDOW_INPUT_FOCUS) != 0;
#endif

        if (is_app_focused) {
            if (io.WantSetMousePos) {
                Input.SetMousePosition({iround(io.MousePos.x), iround(io.MousePos.y)}, &MainWindow);
            }

            if (_mouseCanUseGlobalState && _mouseButtonsDown == 0) {
                float mouse_x_global;
                float mouse_y_global;
                SDL_GetGlobalMouseState(&mouse_x_global, &mouse_y_global);

                int window_x;
                int window_y;
                SDL_GetWindowPosition(static_cast<SDL_Window*>(MainWindow._windowHandle), &window_x, &window_y);

                const auto screen_pos = WindowPosToScreenPos({iround(mouse_x_global) - window_x, iround(mouse_y_global) - window_y});
                io.AddMousePosEvent(static_cast<float>(screen_pos.x), static_cast<float>(screen_pos.y));
            }
        }
    }

    ImGui::NewFrame();

    _onFrameBeginDispatcher();
}

void Application::EndFrame()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(RenderTargetTex == nullptr);

    // Skip unprocessed events
    EventsQueue->clear();

    // Render ImGui
    ImGui::Render();

    const auto* draw_data = ImGui::GetDrawData();

    const auto fb_width = static_cast<int>(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    const auto fb_height = static_cast<int>(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);

    if (_imguiEffect && _imguiDrawBuf && fb_width > 0 && fb_height > 0) {
        // Scissor/clipping
        const auto clip_off = draw_data->DisplayPos;
        const auto clip_scale = draw_data->FramebufferScale;

        // Render command lists
        for (int cmd = 0; cmd < draw_data->CmdListsCount; cmd++) {
            const auto* cmd_list = draw_data->CmdLists[cmd];

            _imguiDrawBuf->Vertices.resize(cmd_list->VtxBuffer.Size);
            _imguiDrawBuf->VertCount = _imguiDrawBuf->Vertices.size();

            for (int i = 0; i < cmd_list->VtxBuffer.Size; i++) {
                auto& v = _imguiDrawBuf->Vertices[i];
                const auto& iv = cmd_list->VtxBuffer[i];
                v.PosX = iv.pos.x;
                v.PosY = iv.pos.y;
                v.TexU = iv.uv.x;
                v.TexV = iv.uv.y;
                v.Color = ucolor {iv.col};
            }

            _imguiDrawBuf->Indices.resize(cmd_list->IdxBuffer.Size);
            _imguiDrawBuf->IndCount = _imguiDrawBuf->Indices.size();

            for (int i = 0; i < cmd_list->IdxBuffer.Size; i++) {
                _imguiDrawBuf->Indices[i] = static_cast<vindex_t>(cmd_list->IdxBuffer[i]);
            }

            _imguiDrawBuf->Upload(_imguiEffect->Usage);

            for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
                const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
                RUNTIME_ASSERT(pcmd->UserCallback == nullptr);

                const auto clip_rect_l = static_cast<int>((pcmd->ClipRect.x - clip_off.x) * clip_scale.x);
                const auto clip_rect_t = static_cast<int>((pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                const auto clip_rect_r = static_cast<int>((pcmd->ClipRect.z - clip_off.x) * clip_scale.x);
                const auto clip_rect_b = static_cast<int>((pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

                if (clip_rect_l < fb_width && clip_rect_t < fb_height && clip_rect_r >= 0 && clip_rect_b >= 0) {
                    ActiveRenderer->EnableScissor({clip_rect_l, clip_rect_t, clip_rect_r - clip_rect_l, clip_rect_b - clip_rect_t});
                    _imguiEffect->DrawBuffer(_imguiDrawBuf.get(), pcmd->IdxOffset, pcmd->ElemCount, reinterpret_cast<const RenderTexture*>(pcmd->TextureId));
                    ActiveRenderer->DisableScissor();
                }
            }
        }
    }

    // Frame complete, swap buffers
    ActiveRenderer->Present();

    _onFrameEndDispatcher();

#if FO_TRACY
    FrameMark;
#endif
}

void Application::RequestQuit() noexcept
{
    STACK_TRACE_ENTRY();

    if (bool expected = false; _quit.compare_exchange_strong(expected, true)) {
        WriteLog("Quit requested");

        _quitEvent.notify_all();
    }
}

void Application::WaitForRequestedQuit()
{
    STACK_TRACE_ENTRY();

    auto locker = std::unique_lock {_quitLocker};

    while (!_quit) {
        _quitEvent.wait(locker);
    }
}

auto AppWindow::GetSize() const -> isize
{
    STACK_TRACE_ENTRY();

    int width = 1000;
    int height = 1000;

    if (ActiveRendererType != RenderType::Null) {
        SDL_GetWindowSizeInPixels(static_cast<SDL_Window*>(_windowHandle), &width, &height);
    }

    return {width, height};
}

void AppWindow::SetSize(isize size)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (ActiveRendererType != RenderType::Null) {
        SDL_SetWindowSize(static_cast<SDL_Window*>(_windowHandle), size.width, size.height);
    }
}

auto AppWindow::GetScreenSize() const -> isize
{
    STACK_TRACE_ENTRY();

    return {App->Settings.ScreenWidth, App->Settings.ScreenHeight};
}

void AppWindow::SetScreenSize(isize size)
{
    STACK_TRACE_ENTRY();

    if (size.width != App->Settings.ScreenWidth || size.height != App->Settings.ScreenHeight) {
        App->Settings.ScreenWidth = size.width;
        App->Settings.ScreenHeight = size.height;

        _onScreenSizeChangedDispatcher();
    }
}

auto AppWindow::GetPosition() const -> ipos
{
    STACK_TRACE_ENTRY();

    int x = 0;
    int y = 0;

    if (ActiveRendererType != RenderType::Null) {
        SDL_GetWindowPosition(static_cast<SDL_Window*>(_windowHandle), &x, &y);
    }

    return {x, y};
}

void AppWindow::SetPosition(ipos pos)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (ActiveRendererType != RenderType::Null) {
        SDL_SetWindowPosition(static_cast<SDL_Window*>(_windowHandle), pos.x, pos.y);
    }
}

auto AppWindow::IsFocused() const -> bool
{
    STACK_TRACE_ENTRY();

    if (ActiveRendererType != RenderType::Null) {
        return (SDL_GetWindowFlags(static_cast<SDL_Window*>(_windowHandle)) & SDL_WINDOW_INPUT_FOCUS) != 0;
    }

    return true;
}

void AppWindow::Minimize()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (ActiveRendererType != RenderType::Null) {
        SDL_MinimizeWindow(static_cast<SDL_Window*>(_windowHandle));
    }
}

auto AppWindow::IsFullscreen() const -> bool
{
    STACK_TRACE_ENTRY();

    if (ActiveRendererType != RenderType::Null) {
        return (SDL_GetWindowFlags(static_cast<SDL_Window*>(_windowHandle)) & SDL_WINDOW_FULLSCREEN) != 0;
    }

    return false;
}

auto AppWindow::ToggleFullscreen(bool enable) -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (ActiveRendererType == RenderType::Null) {
        return false;
    }

    const auto is_fullscreen = IsFullscreen();

    if (!is_fullscreen && enable) {
        if (SDL_SetWindowFullscreen(static_cast<SDL_Window*>(_windowHandle), true)) {
            RUNTIME_ASSERT(IsFullscreen());
            return true;
        }
    }
    else if (is_fullscreen && !enable) {
        if (SDL_SetWindowFullscreen(static_cast<SDL_Window*>(_windowHandle), false)) {
            RUNTIME_ASSERT(!IsFullscreen());
            return true;
        }
    }

    return false;
}

void AppWindow::Blink()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (ActiveRendererType == RenderType::Null) {
        return;
    }

    SDL_FlashWindow(static_cast<SDL_Window*>(_windowHandle), SDL_FLASH_UNTIL_FOCUSED);
}

void AppWindow::AlwaysOnTop(bool enable)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (ActiveRendererType == RenderType::Null) {
        return;
    }

    SDL_SetWindowAlwaysOnTop(static_cast<SDL_Window*>(_windowHandle), enable);
}

void AppWindow::GrabInput(bool enable)
{
    STACK_TRACE_ENTRY();

    _grabbed = enable;
}

void AppWindow::Destroy()
{
    STACK_TRACE_ENTRY();

    if (ActiveRendererType == RenderType::Null) {
        return;
    }

    if (_windowHandle != nullptr && this != &App->MainWindow) {
        SDL_DestroyWindow(static_cast<SDL_Window*>(_windowHandle));
        _windowHandle = nullptr;
    }
}

auto AppRender::CreateTexture(isize size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return ActiveRenderer->CreateTexture(size, linear_filtered, with_depth);
}

void AppRender::SetRenderTarget(RenderTexture* tex)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    ActiveRenderer->SetRenderTarget(tex);
    RenderTargetTex = tex;
}

auto AppRender::GetRenderTarget() -> RenderTexture*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return RenderTargetTex;
}

void AppRender::ClearRenderTarget(optional<ucolor> color, bool depth, bool stencil)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    ActiveRenderer->ClearRenderTarget(color, depth, stencil);
}

void AppRender::EnableScissor(irect rect)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    ActiveRenderer->EnableScissor(rect);
}

void AppRender::DisableScissor()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    ActiveRenderer->DisableScissor();
}

auto AppRender::CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return ActiveRenderer->CreateDrawBuffer(is_static);
}

auto AppRender::CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return ActiveRenderer->CreateEffect(usage, name, loader);
}

auto AppRender::CreateOrthoMatrix(float left, float right, float bottom, float top, float nearp, float farp) -> mat44
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return ActiveRenderer->CreateOrthoMatrix(left, right, bottom, top, nearp, farp);
}

auto AppRender::IsRenderTargetFlipped() -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return ActiveRenderer->IsRenderTargetFlipped();
}

auto AppInput::GetMousePosition() const -> ipos
{
    STACK_TRACE_ENTRY();

    float x = 100;
    float y = 100;

    if (ActiveRendererType != RenderType::Null) {
        SDL_GetMouseState(&x, &y);
    }

    return WindowPosToScreenPos({iround(x), iround(y)});
}

void AppInput::SetMousePosition(ipos pos, const AppWindow* relative_to)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (ActiveRendererType != RenderType::Null) {
        App->Settings.MousePos = pos;

        SDL_SetEventEnabled(SDL_EVENT_MOUSE_MOTION, false);

        if (relative_to != nullptr) {
            pos = ScreenPosToWindowPos(pos);

            SDL_WarpMouseInWindow(static_cast<SDL_Window*>(relative_to->_windowHandle), static_cast<float>(pos.x), static_cast<float>(pos.y));
        }
        else {
            SDL_WarpMouseGlobal(static_cast<float>(pos.x), static_cast<float>(pos.y));
        }

        SDL_SetEventEnabled(SDL_EVENT_MOUSE_MOTION, true);
    }
}

auto AppInput::PollEvent(InputEvent& ev) -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (!EventsQueue->empty()) {
        ev = EventsQueue->front();
        EventsQueue->erase(EventsQueue->begin());
        return true;
    }
    return false;
}

void AppInput::ClearEvents()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    EventsQueue->clear();
}

void AppInput::PushEvent(const InputEvent& ev, bool push_to_this_frame)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (push_to_this_frame) {
        EventsQueue->emplace_back(ev);
    }
    else {
        NextFrameEventsQueue->emplace_back(ev);
    }
}

void AppInput::SetClipboardText(string_view text)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    SDL_SetClipboardText(string(text).c_str());
}

auto AppInput::GetClipboardText() -> const string&
{
    STACK_TRACE_ENTRY();

    _clipboardTextStorage = SDL_GetClipboardText();
    return _clipboardTextStorage;
}

auto AppAudio::IsEnabled() const -> bool
{
    STACK_TRACE_ENTRY();

    return AudioStream != nullptr;
}

void AppAudio::SetSource(AudioStreamCallback stream_callback)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(IsEnabled());

    LockDevice();
    *AudioStreamWriter = std::move(stream_callback);
    UnlockDevice();
}

auto AppAudio::ConvertAudio(int format, int channels, int rate, vector<uint8>& buf) -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(IsEnabled());

    SDL_AudioSpec spec;
    spec.format = static_cast<SDL_AudioFormat>(format);
    spec.channels = static_cast<Uint8>(channels);
    spec.freq = rate;

    if (spec.format != AudioSpec.format || spec.channels != AudioSpec.channels || spec.freq != rate) {
        uint8* dst_data;
        int dst_len;

        if (!SDL_ConvertAudioSamples(&spec, buf.data(), static_cast<int>(buf.size()), &AudioSpec, &dst_data, &dst_len)) {
            return false;
        }

        buf.resize(static_cast<size_t>(dst_len));
        MemCopy(buf.data(), dst_data, buf.size());
    }

    return true;
}

void AppAudio::MixAudio(uint8* output, const uint8* buf, size_t len, int volume)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(IsEnabled());

    const float vlume_01 = static_cast<float>(std::clamp(volume, 0, 100)) / 100.0f;
    SDL_MixAudio(output, buf, AudioSpec.format, static_cast<Uint32>(len), vlume_01);
}

void AppAudio::LockDevice()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(IsEnabled());

    SDL_LockAudioStream(AudioStream);
}

void AppAudio::UnlockDevice()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(IsEnabled());

    SDL_UnlockAudioStream(AudioStream);
}

void MessageBox::ShowErrorMessage(string_view message, string_view traceback, bool fatal_error)
{
    STACK_TRACE_ENTRY();

    const char* title = fatal_error ? "Fatal Error" : "Error";

#if FO_WEB || FO_ANDROID || FO_IOS
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, string(message).c_str(), nullptr);

#else
    auto verb_message = string(message);

    if (!traceback.empty()) {
        verb_message += strex("\n\n{}", traceback);
    }

    static unordered_set<string> ignore_entries;
    static std::mutex ignore_entries_locker;

    if (!fatal_error) {
        auto locker = std::unique_lock {ignore_entries_locker};
        if (ignore_entries.count(verb_message) != 0) {
            return;
        }
    }

    SDL_MessageBoxButtonData copy_button;
    SDL_zero(copy_button);
    copy_button.buttonID = 0;
    copy_button.text = "Copy";

    SDL_MessageBoxButtonData ignore_all_button;
    SDL_zero(ignore_all_button);
    ignore_all_button.buttonID = 1;
    ignore_all_button.text = "Ignore All";

    SDL_MessageBoxButtonData ignore_button;
    SDL_zero(ignore_button);
    ignore_button.buttonID = 2;
    ignore_button.text = "Ignore";
    ignore_button.flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
    ignore_button.flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;

    SDL_MessageBoxButtonData exit_button;
    SDL_zero(exit_button);
    exit_button.buttonID = 2;
    exit_button.text = "Exit";
    exit_button.flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
    exit_button.flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;

    const SDL_MessageBoxButtonData buttons_with_ignore[] = {copy_button, ignore_all_button, ignore_button};
    const SDL_MessageBoxButtonData buttons_with_exit[] = {copy_button, exit_button};

    SDL_MessageBoxData data;
    SDL_zero(data);
    data.flags = SDL_MESSAGEBOX_ERROR | SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT;
    data.title = title;
    data.message = verb_message.c_str();
    data.numbuttons = fatal_error ? 2 : 3;
    data.buttons = fatal_error ? buttons_with_exit : buttons_with_ignore;

    int buttonid = 0;
    while (SDL_ShowMessageBox(&data, &buttonid)) {
        if (buttonid == 0) {
            SDL_SetClipboardText(verb_message.c_str());
        }
        else if (buttonid == 1) {
            auto locker = std::unique_lock {ignore_entries_locker};
            ignore_entries.emplace(verb_message);
            break;
        }
        else {
            break;
        }
    }
#endif
}

static ImGuiKey KeycodeToImGuiKey(SDL_Keycode keycode)
{
    STACK_TRACE_ENTRY();

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
    case SDLK_APOSTROPHE:
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
    case SDLK_GRAVE:
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
    case SDLK_A:
        return ImGuiKey_A;
    case SDLK_B:
        return ImGuiKey_B;
    case SDLK_C:
        return ImGuiKey_C;
    case SDLK_D:
        return ImGuiKey_D;
    case SDLK_E:
        return ImGuiKey_E;
    case SDLK_F:
        return ImGuiKey_F;
    case SDLK_G:
        return ImGuiKey_G;
    case SDLK_H:
        return ImGuiKey_H;
    case SDLK_I:
        return ImGuiKey_I;
    case SDLK_J:
        return ImGuiKey_J;
    case SDLK_K:
        return ImGuiKey_K;
    case SDLK_L:
        return ImGuiKey_L;
    case SDLK_M:
        return ImGuiKey_M;
    case SDLK_N:
        return ImGuiKey_N;
    case SDLK_O:
        return ImGuiKey_O;
    case SDLK_P:
        return ImGuiKey_P;
    case SDLK_Q:
        return ImGuiKey_Q;
    case SDLK_R:
        return ImGuiKey_R;
    case SDLK_S:
        return ImGuiKey_S;
    case SDLK_T:
        return ImGuiKey_T;
    case SDLK_U:
        return ImGuiKey_U;
    case SDLK_V:
        return ImGuiKey_V;
    case SDLK_W:
        return ImGuiKey_W;
    case SDLK_X:
        return ImGuiKey_X;
    case SDLK_Y:
        return ImGuiKey_Y;
    case SDLK_Z:
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
