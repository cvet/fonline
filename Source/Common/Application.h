//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Settings.h"

constexpr int EFFECT_TEXTURES = 8;
constexpr int EFFECT_SCRIPT_VALUES = 16;
constexpr int EFFECT_MAX_PASSES = 4;
constexpr int MODEL_MAX_BONES = 54;
constexpr int BONES_PER_VERTEX = 4;

DECLARE_EXCEPTION(AppInitException);

enum class RenderPrimitiveType
{
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
    TriangleFan,
};

enum class BlendEquationType // Default is FuncAdd
{
    FuncAdd,
    FuncSubtract,
    FuncReverseSubtract,
    Max,
    Min,
};

enum class BlendFuncType // Default is SrcAlpha InvSrcAlpha
{
    Zero,
    One,
    SrcColor,
    InvSrcColor,
    DstColor,
    InvDstColor,
    SrcAlpha,
    InvSrcAlpha,
    DstAlpha,
    InvDstAlpha,
    ConstantColor,
    InvConstantColor,
    SrcAlphaSaturate,
};

struct Vertex2D
{
    float X, Y;
    uint Diffuse;
    float TU, TV;
    float TUEgg, TVEgg;
};
static_assert(std::is_standard_layout_v<Vertex2D>);
static_assert(sizeof(Vertex2D) == 28);
using Vertex2DVec = vector<Vertex2D>;

struct Vertex3D
{
    vec3 Position;
    vec3 Normal;
    float TexCoord[2];
    float TexCoordBase[2];
    vec3 Tangent;
    vec3 Bitangent;
    float BlendWeights[BONES_PER_VERTEX];
    float BlendIndices[BONES_PER_VERTEX];
};
static_assert(std::is_standard_layout_v<Vertex3D>);
static_assert(sizeof(Vertex3D) == 96);
using Vertex3DVec = vector<Vertex3D>;

class RenderTexture final : public RefCounter
{
    friend class Application;

public:
    struct Impl;

    RenderTexture(const RenderTexture&) = delete;
    RenderTexture(RenderTexture&&) noexcept = delete;
    auto operator=(const RenderTexture&) = delete;
    auto operator=(RenderTexture&&) noexcept = delete;
    ~RenderTexture() override;

    const uint Width {};
    const uint Height {};
    const float SizeData[4] {}; // Width, Height, TexelWidth, TexelHeight
    const bool LinearFiltered {};
    const bool WithDepth {};

private:
    RenderTexture() = default;

    unique_ptr<Impl> _pImpl {};
};

class RenderEffect final : public RefCounter
{
    friend class Application;

public:
    struct Impl;

    struct ProjBuffer
    {
        float ProjMatrix[16] {}; // mat44
    };

    struct MainTexBuffer
    {
        float MainTexSize[4] {}; // vec4
    };

    struct TimeBuffer
    {
        float RealTime {};
        float GameTime {};
        float Padding[2] {};
    };

    struct MapSpriteBuffer
    {
        float EggTexSize[4] {}; // vec4
        float ZoomFactor {};
        float Padding[3] {};
    };

    struct BorderBuffer
    {
        float SpriteBorder[4] {}; // vec4
    };

    // Todo: split ModelBuffer by number of supported bones (1, 5, 10, 20, 35, 54)
    struct ModelBuffer
    {
        float LightColor[4] {}; // vec4
        float GroundPosition[4] {}; // vec4
        float WorldMatrices[16 * MODEL_MAX_BONES] {}; // mat44
    };

    struct CustomTexBuffer
    {
        float AtlasOffset[4 * EFFECT_TEXTURES] {}; // vec4
        float Size[4 * EFFECT_TEXTURES] {}; // vec4
    };

    struct AnimBuffer
    {
        float NormalizedTime {};
        float AbsoluteTime {};
        float Padding[2] {};
    };

    struct RandomValueBuffer
    {
        float Value[4] {};
    };

    struct ScriptValueBuffer
    {
        float Value[EFFECT_SCRIPT_VALUES] {};
    };

    static_assert(sizeof(ProjBuffer) % 16 == 0 && sizeof(ProjBuffer) == 64);
    static_assert(sizeof(MainTexBuffer) % 16 == 0 && sizeof(MainTexBuffer) == 16);
    static_assert(sizeof(TimeBuffer) % 16 == 0 && sizeof(TimeBuffer) == 16);
    static_assert(sizeof(MapSpriteBuffer) % 16 == 0 && sizeof(MapSpriteBuffer) == 32);
    static_assert(sizeof(BorderBuffer) % 16 == 0 && sizeof(BorderBuffer) == 16);
    static_assert(sizeof(ModelBuffer) % 16 == 0 && sizeof(ModelBuffer) == 3488);
    static_assert(sizeof(CustomTexBuffer) % 16 == 0 && sizeof(CustomTexBuffer) == 256);
    static_assert(sizeof(AnimBuffer) % 16 == 0 && sizeof(AnimBuffer) == 16);
    static_assert(sizeof(RandomValueBuffer) % 16 == 0 && sizeof(RandomValueBuffer) == 16);
    static_assert(sizeof(ScriptValueBuffer) % 16 == 0 && sizeof(ScriptValueBuffer) == 64);
    // Total size: 4000
    // We must fit to 4096, that value guaranteed by GL_MAX_VERTEX_UNIFORM_COMPONENTS (1024 * sizeof(float))

    RenderEffect(const RenderEffect&) = delete;
    RenderEffect(RenderEffect&&) noexcept = delete;
    auto operator=(const RenderEffect&) = delete;
    auto operator=(RenderEffect&&) noexcept = delete;
    ~RenderEffect() override;

    [[nodiscard]] auto IsSame(const string& name, const string& defines) const -> bool;
    [[nodiscard]] auto CanBatch(const RenderEffect* other) const -> bool;

    ProjBuffer ProjBuf {};
    optional<MainTexBuffer> MainTexBuf {};
    optional<TimeBuffer> TimeBuf {};
    optional<MapSpriteBuffer> MapSpriteBuf {};
    optional<BorderBuffer> BorderBuf {};
    optional<ModelBuffer> ModelBuf {};
    optional<CustomTexBuffer> CustomTexBuf {};
    optional<AnimBuffer> AnimBuf {};
    optional<RandomValueBuffer> RandomValueBuf {};
    optional<ScriptValueBuffer> ScriptValueBuf {};
    RenderTexture* MainTex {};
    RenderTexture* EggTex {};
    RenderTexture* CustomTex[EFFECT_TEXTURES] {};
    bool DisableShadow {};
    bool DisableBlending {};

private:
    RenderEffect() = default;

    unique_ptr<Impl> _pImpl {};
    string _effectName {};
    string _effectDefines {};
    hash _nameHash {};
    uint _passCount {};
    bool _isShadow[EFFECT_MAX_PASSES] {};
    BlendFuncType _blendFuncParam1[EFFECT_MAX_PASSES] {};
    BlendFuncType _blendFuncParam2[EFFECT_MAX_PASSES] {};
    BlendEquationType _blendEquation[EFFECT_MAX_PASSES] {};
};

class RenderMesh final : public RefCounter
{
    friend class Application;

public:
    struct Impl;

    RenderMesh(const RenderMesh&) = delete;
    RenderMesh(RenderMesh&&) noexcept = delete;
    auto operator=(const RenderMesh&) = delete;
    auto operator=(RenderMesh&&) noexcept = delete;
    ~RenderMesh() override;

    bool DataChanged {};
    Vertex3DVec Vertices {};
    vector<ushort> Indices {};
    bool DisableCulling {};

private:
    RenderMesh() = default;

    unique_ptr<Impl> _pImpl {};
};

enum class KeyCode
{
#define KEY_CODE(name, index, code) name = (index),
#include "KeyCodes-Include.h"
};

enum class MouseButton
{
    Left = 0,
    Right = 1,
    Middle = 2,
    Ext0 = 5,
    Ext1 = 6,
    Ext2 = 7,
    Ext3 = 8,
    Ext4 = 9,
};

struct InputEvent
{
    enum class EventType
    {
        NoneEvent,
        MouseMoveEvent,
        MouseDownEvent,
        MouseUpEvent,
        MouseWheelEvent,
        KeyDownEvent,
        KeyUpEvent,
    } Type {};

    struct MouseMoveEvent
    {
        int MouseX {};
        int MouseY {};
        int DeltaX {};
        int DeltaY {};
    } MouseMove {};

    struct MouseDownEvent
    {
        MouseButton Button {};
    } MouseDown {};

    struct MouseUpEvent
    {
        MouseButton Button {};
    } MouseUp {};

    struct MouseWheelEvent
    {
        int Delta {};
    } MouseWheel {};

    struct KeyDownEvent
    {
        KeyCode Code {};
        string Text {};
    } KeyDown {};

    struct KeyUpEvent
    {
        KeyCode Code {};
    } KeyUp {};

    InputEvent() = default;
    explicit InputEvent(MouseMoveEvent ev) : Type {EventType::MouseMoveEvent}, MouseMove {ev} { }
    explicit InputEvent(MouseDownEvent ev) : Type {EventType::MouseDownEvent}, MouseDown {ev} { }
    explicit InputEvent(MouseUpEvent ev) : Type {EventType::MouseUpEvent}, MouseUp {ev} { }
    explicit InputEvent(MouseWheelEvent ev) : Type {EventType::MouseWheelEvent}, MouseWheel {ev} { }
    explicit InputEvent(KeyDownEvent ev) : Type {EventType::KeyDownEvent}, KeyDown {std::move(ev)} { }
    explicit InputEvent(KeyUpEvent ev) : Type {EventType::KeyUpEvent}, KeyUp {ev} { }
};

class Application final
{
    friend void InitApplication(GlobalSettings& settings);

    explicit Application(GlobalSettings& settings);

public:
    struct AppWindow
    {
        [[nodiscard]] auto GetSize() const -> tuple<int, int>;
        [[nodiscard]] auto GetPosition() const -> tuple<int, int>;
        [[nodiscard]] auto GetMousePosition() const -> tuple<int, int>;
        [[nodiscard]] auto IsFocused() const -> bool;
        [[nodiscard]] auto IsFullscreen() const -> bool;

        void SetSize(int w, int h);
        void SetPosition(int x, int y);
        void SetMousePosition(int x, int y);
        void Minimize();
        auto ToggleFullscreen(bool enable) -> bool;
        void Blink();
        void AlwaysOnTop(bool enable);

    private:
        int _nonConstHelper {};
    };

    struct AppRender
    {
        static constexpr uint MAX_ATLAS_SIZE = 4096;
        static constexpr uint MIN_ATLAS_SIZE = 2048;
        static const uint MAX_ATLAS_WIDTH;
        static const uint MAX_ATLAS_HEIGHT;
        static const uint MAX_BONES;

        using RenderEffectLoader = std::function<vector<uchar>(const string&)>;

        [[nodiscard]] auto GetTexturePixel(RenderTexture* tex, int x, int y) const -> uint;
        [[nodiscard]] auto GetTextureRegion(RenderTexture* tex, int x, int y, uint w, uint h) const -> vector<uint>;
        [[nodiscard]] auto GetRenderTarget() -> RenderTexture*;

        [[nodiscard]] auto CreateTexture(uint width, uint height, bool linear_filtered, bool with_depth) -> RenderTexture*;
        [[nodiscard]] auto CreateEffect(const string& name, const string& defines, const RenderEffectLoader& loader) -> RenderEffect*;

        void UpdateTextureRegion(RenderTexture* tex, const IRect& r, const uint* data);
        void SetRenderTarget(RenderTexture* tex);
        void ClearRenderTarget(uint color);
        void ClearRenderTargetDepth();
        void EnableScissor(int x, int y, uint w, uint h);
        void DisableScissor();
        void DrawQuads(const Vertex2DVec& vbuf, const vector<ushort>& ibuf, uint pos, RenderEffect* effect, RenderTexture* tex);
        void DrawPrimitive(const Vertex2DVec& vbuf, const vector<ushort>& ibuf, RenderEffect* effect, RenderPrimitiveType prim);
        void DrawMesh(RenderMesh* mesh, RenderEffect* effect);

    private:
        int _nonConstHelper {};
    };

    struct AppInput
    {
        static constexpr uint DROP_FILE_STRIP_LENGHT = 2048;

        [[nodiscard]] auto GetClipboardText() -> string;

        [[nodiscard]] auto PollEvent(InputEvent& event) -> bool;

        void PushEvent(const InputEvent& event);
        void SetClipboardText(const string& text);
    };

    struct AppAudio
    {
        static const int AUDIO_FORMAT_U8;
        static const int AUDIO_FORMAT_S16;

        using AudioStreamCallback = std::function<void(uchar*)>;

        [[nodiscard]] auto IsEnabled() -> bool;
        [[nodiscard]] auto GetStreamSize() -> uint;
        [[nodiscard]] auto GetSilence() -> uchar;

        [[nodiscard]] auto ConvertAudio(int format, int channels, int rate, vector<uchar>& buf) -> bool;

        void SetSource(AudioStreamCallback stream_callback);
        void MixAudio(uchar* output, uchar* buf, int volume);
        void LockDevice();
        void UnlockDevice();

    private:
        struct AudioConverter;
        vector<AudioConverter*> _converters {};
    };

    Application(const Application&) = delete;
    Application(Application&&) noexcept = delete;
    auto operator=(const Application&) = delete;
    auto operator=(Application&&) noexcept = delete;
    ~Application() = default;

#if FO_IOS
    void SetMainLoopCallback(void (*callback)(void*));
#endif
    void BeginFrame();
    void EndFrame();

    EventObserver<> OnFrameBegin {};
    EventObserver<> OnFrameEnd {};
    EventObserver<> OnPause {};
    EventObserver<> OnResume {};
    EventObserver<> OnLowMemory {};
    EventObserver<> OnQuit {};

    AppWindow Window;
    AppRender Render;
    AppInput Input;
    AppAudio Audio;

private:
    EventDispatcher<> _onFrameBeginDispatcher {OnFrameBegin};
    EventDispatcher<> _onFrameEndDispatcher {OnFrameEnd};
    EventDispatcher<> _onPauseDispatcher {OnPause};
    EventDispatcher<> _onResumeDispatcher {OnResume};
    EventDispatcher<> _onLowMemoryDispatcher {OnLowMemory};
    EventDispatcher<> _onQuitDispatcher {OnQuit};
};

extern void InitApplication(GlobalSettings& settings);
extern Application* App;
