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
    Vector Position;
    Vector Normal;
    float TexCoord[2];
    float TexCoordBase[2];
    Vector Tangent;
    Vector Bitangent;
    float BlendWeights[BONES_PER_VERTEX];
    float BlendIndices[BONES_PER_VERTEX];
};
static_assert(std::is_standard_layout_v<Vertex3D>);
static_assert(sizeof(Vertex3D) == 96);
using Vertex3DVec = vector<Vertex3D>;

class RenderTexture : public Pointable
{
    friend class App;

public:
    struct Impl;

    ~RenderTexture();

    const uint Width {};
    const uint Height {};
    const float SizeData[4] {}; // Width, Height, TexelWidth, TexelHeight
    const float Samples {};
    const bool LinearFiltered {};
    const bool Multisampled {};
    const bool WithDepth {};

private:
    RenderTexture() = default;

    unique_ptr<Impl> pImpl {};
};

class RenderEffect : public Pointable
{
    friend class App;

public:
    struct Impl;

    struct ProjBuffer
    {
        float ProjMatrix[16] {}; // mat4x4
    };

    struct MainTexBuffer
    {
        float MainTexSize[4] {}; // vec4
        float MainTexSamples {};
        int MainTexSamplesInt {};
        float Padding[2] {};
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
        float WorldMatrices[16 * MODEL_MAX_BONES] {}; // mat4x4
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
    static_assert(sizeof(MainTexBuffer) % 16 == 0 && sizeof(MainTexBuffer) == 32);
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

    ~RenderEffect();
    bool IsSame(const string& name, const string& defines);
    bool CanBatch(RenderEffect* other);

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

    unique_ptr<Impl> pImpl {};
    string effectName {};
    string effectDefines {};
    hash nameHash {};
    uint passCount {};
    bool isMultisampled {};
    bool isShadow[EFFECT_MAX_PASSES] {};
    BlendFuncType blendFuncParam1[EFFECT_MAX_PASSES] {};
    BlendFuncType blendFuncParam2[EFFECT_MAX_PASSES] {};
    BlendEquationType blendEquation[EFFECT_MAX_PASSES] {};
};

class RenderMesh : public Pointable
{
    friend class App;

public:
    struct Impl;

    ~RenderMesh();

    bool DataChanged {};
    Vertex3DVec Vertices {};
    UShortVec Indices {};
    bool DisableCulling {};

private:
    RenderMesh() = default;

    unique_ptr<Impl> pImpl {};
};

enum class KeyCode
{
#define KEY_CODE(name, index, code) name = index,
#include "KeyCodes_Include.h"
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
    struct MouseMove
    {
        int MouseX {};
        int MouseY {};
        int DeltaX {};
        int DeltaY {};
    };

    struct MouseDown
    {
        MouseButton Button {};
    };

    struct MouseUp
    {
        MouseButton Button {};
    };

    struct MouseWheel
    {
        int Delta {};
    };

    struct KeyDown
    {
        KeyCode Code {};
        string Text {};
    };

    struct KeyUp
    {
        KeyCode Code {};
    };

    enum EventType
    {
        NoneEvent,
        MouseMoveEvent,
        MouseDownEvent,
        MouseUpEvent,
        MouseWheelEvent,
        KeyDownEvent,
        KeyUpEvent,
    };

    variant<int, MouseMove, MouseDown, MouseUp, MouseWheel, KeyDown, KeyUp> Data {};
};

class App : public StaticClass
{
public:
    static void Init(GlobalSettings& settings);
    static void BeginFrame();
    static void EndFrame();

    static EventObserver<> OnFrameBegin;
    static EventObserver<> OnFrameEnd;
    static EventObserver<> OnPause;
    static EventObserver<> OnResume;
    static EventObserver<> OnLowMemory;
    static EventObserver<> OnQuit;

    class Window
    {
    public:
        static void GetSize(int& w, int& h);
        static void SetSize(int w, int h);
        static void GetPosition(int& x, int& y);
        static void SetPosition(int x, int y);
        static void GetMousePosition(int& x, int& y);
        static void SetMousePosition(int x, int y);
        static bool IsFocused();
        static void Minimize();
        static bool IsFullscreen();
        static bool ToggleFullscreen(bool enable);
        static void Blink();
        static void AlwaysOnTop(bool enable);
    };

    class Render
    {
    public:
        using RenderEffectLoader = std::function<vector<uchar>(const string&)>;

        static RenderTexture* CreateTexture(
            uint width, uint height, bool linear_filtered, bool multisampled, bool with_depth);
        static uint GetTexturePixel(RenderTexture* tex, int x, int y);
        static vector<uint> GetTextureRegion(RenderTexture* tex, int x, int y, uint w, uint h);
        static void UpdateTextureRegion(RenderTexture* tex, const Rect& r, const uint* data);
        static void SetRenderTarget(RenderTexture* tex);
        static RenderTexture* GetRenderTarget();
        static void ClearRenderTarget(uint color);
        static void ClearRenderTargetDepth();
        static void EnableScissor(int x, int y, uint w, uint h);
        static void DisableScissor();
        static RenderEffect* CreateEffect(const string& name, const string& defines, RenderEffectLoader file_loader);
        static void DrawQuads(const Vertex2DVec& vbuf, const UShortVec& ibuf, RenderEffect* effect, RenderTexture* tex);
        static void DrawPrimitive(
            const Vertex2DVec& vbuf, const UShortVec& ibuf, RenderEffect* effect, RenderPrimitiveType prim);
        static void DrawMesh(RenderMesh* mesh, RenderEffect* effect);

        static constexpr uint MAX_ATLAS_SIZE = 4096;
        static constexpr uint MIN_ATLAS_SIZE = 2048;
        static const uint MaxAtlasWidth;
        static const uint MaxAtlasHeight;
        static const uint MaxBones;
    };

    class Input
    {
    public:
        static bool PollEvent(InputEvent& event);
        static void PushEvent(InputEvent event);
        static void SetClipboardText(const string& text);
        static string GetClipboardText();

        static constexpr uint DROP_FILE_STRIP_LENGHT = 2048;
    };

    class Audio
    {
    public:
        using AudioStreamCallback = std::function<void(uchar*)>;

        static bool IsEnabled();
        static uint GetStreamSize();
        static uchar GetSilence();
        static void SetSource(AudioStreamCallback stream_callback);
        static bool ConvertAudio(int format, int channels, int rate, vector<uchar>& buf);
        static void MixAudio(uchar* output, uchar* buf, int volume);
        static void LockDevice();
        static void UnlockDevice();

        static const int AUDIO_FORMAT_U8;
        static const int AUDIO_FORMAT_S16;
    };
};
