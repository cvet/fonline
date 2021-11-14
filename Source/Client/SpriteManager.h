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

// Todo: improve DirectX rendering
// Todo: maybe restrict fps at 60?
// Todo: optimize sprite atlas filling

#pragma once

#include "Common.h"

#include "3dStuff.h"
#include "Application.h"
#include "ClientScripting.h"
#include "EffectManager.h"
#include "FileSystem.h"
#include "Settings.h"
#include "Sprites.h"
#include "Timer.h"

static constexpr auto ANY_FRAMES_POOL_SIZE = 2000;
static constexpr auto MAX_STORED_PIXEL_PICKS = 100;
static constexpr auto MAX_FRAMES = 50;

// Font flags
static constexpr uint FT_NOBREAK = 0x0001;
static constexpr uint FT_NOBREAK_LINE = 0x0002;
static constexpr uint FT_CENTERX = 0x0004;
static constexpr uint FT_CENTERY_ENGINE = 0x1000; // Temporary workaround
static constexpr uint FT_CENTERY = 0x0008 | FT_CENTERY_ENGINE;
static constexpr uint FT_CENTERR = 0x0010;
static constexpr uint FT_BOTTOM = 0x0020;
static constexpr uint FT_UPPER = 0x0040;
static constexpr uint FT_NO_COLORIZE = 0x0080;
static constexpr uint FT_ALIGN = 0x0100;
static constexpr uint FT_BORDERED = 0x0200;
static constexpr auto FT_SKIPLINES(uint l) -> uint
{
    return 0x0400 | (l << 16);
}
static constexpr auto FT_SKIPLINES_END(uint l) -> uint
{
    return 0x0800 | (l << 16);
}

// Colors
// Todo: improve client rendering brightness
static constexpr auto PACK_COLOR(int r, int g, int b, int a) -> uint
{
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
    a = std::clamp(a, 0, 255);
    return COLOR_RGBA(a, r, g, b);
}
static constexpr auto COLOR_GAME_RGB(int r, int g, int b) -> uint
{
    return PACK_COLOR(r, g, b, 255);
}
#define COLOR_WITH_LIGHTING(c) PACK_COLOR(((c) >> 16) & 0xFF, ((c) >> 8) & 0xFF, (c)&0xFF, ((c) >> 24) & 0xFF)
#define COLOR_SCRIPT_SPRITE(c) ((c) ? COLOR_WITH_LIGHTING(c) : COLOR_WITH_LIGHTING(COLOR_IFACE_FIXED))
#define COLOR_SCRIPT_TEXT(c) ((c) ? COLOR_WITH_LIGHTING(c) : COLOR_WITH_LIGHTING(COLOR_TEXT))
#define COLOR_CHANGE_ALPHA(v, a) ((((v) | 0xFF000000) ^ 0xFF000000) | ((uint)(a)&0xFF) << 24)
static constexpr auto COLOR_IFACE_FIXED = COLOR_GAME_RGB(103, 95, 86);
static constexpr auto COLOR_IFACE = COLOR_WITH_LIGHTING(COLOR_IFACE_FIXED);
static constexpr auto COLOR_IFACE_RED = (COLOR_IFACE | (0xFF << 16));
static constexpr auto COLOR_CRITTER_NAME = COLOR_GAME_RGB(0xAD, 0xAD, 0xB9);
static constexpr auto COLOR_TEXT = COLOR_GAME_RGB(60, 248, 0);
static constexpr auto COLOR_TEXT_WHITE = COLOR_GAME_RGB(0xFF, 0xFF, 0xFF);
static constexpr auto COLOR_TEXT_DWHITE = COLOR_GAME_RGB(0xBF, 0xBF, 0xBF);
static constexpr auto COLOR_TEXT_RED = COLOR_GAME_RGB(0xC8, 0, 0);

// Sprite layers
static constexpr auto DRAW_ORDER_FLAT = 0;
static constexpr auto DRAW_ORDER = 20;
static constexpr auto DRAW_ORDER_TILE = DRAW_ORDER_FLAT + 0;
static constexpr auto DRAW_ORDER_TILE_END = DRAW_ORDER_FLAT + 4;
static constexpr auto DRAW_ORDER_HEX_GRID = DRAW_ORDER_FLAT + 5;
static constexpr auto DRAW_ORDER_FLAT_SCENERY = DRAW_ORDER_FLAT + 8;
static constexpr auto DRAW_ORDER_LIGHT = DRAW_ORDER_FLAT + 9;
static constexpr auto DRAW_ORDER_DEAD_CRITTER = DRAW_ORDER_FLAT + 10;
static constexpr auto DRAW_ORDER_FLAT_ITEM = DRAW_ORDER_FLAT + 13;
static constexpr auto DRAW_ORDER_TRACK = DRAW_ORDER_FLAT + 16;
static constexpr auto DRAW_ORDER_SCENERY = DRAW_ORDER + 3;
static constexpr auto DRAW_ORDER_ITEM = DRAW_ORDER + 6;
static constexpr auto DRAW_ORDER_CRITTER = DRAW_ORDER + 9;
static constexpr auto DRAW_ORDER_RAIN = DRAW_ORDER + 12;
static constexpr auto DRAW_ORDER_LAST = 39;

// Egg types
static constexpr auto EGG_ALWAYS = 1;
static constexpr auto EGG_X = 2;
static constexpr auto EGG_Y = 3;
static constexpr auto EGG_X_AND_Y = 4;
static constexpr auto EGG_X_OR_Y = 5;

// Contour types
static constexpr auto CONTOUR_RED = 1;
static constexpr auto CONTOUR_YELLOW = 2;
static constexpr auto CONTOUR_CUSTOM = 3;

enum class AtlasType
{
    Static,
    Dynamic,
    Splash,
    MeshTextures,
};

struct RenderTarget
{
    unique_ptr<RenderTexture> MainTex {};
    RenderEffect* DrawEffect {};
    bool ScreenSized {};
    vector<tuple<int, int, uint>> LastPixelPicks {};
};

struct TextureAtlas
{
    struct SpaceNode
    {
        SpaceNode(int x, int y, uint w, uint h);
        auto FindPosition(uint w, uint h, int& x, int& y) -> bool;

        int PosX {};
        int PosY {};
        uint Width {};
        uint Height {};
        bool Busy {};
        unique_ptr<SpaceNode> Child1 {};
        unique_ptr<SpaceNode> Child2 {};
    };

    AtlasType Type {};
    RenderTarget* RT {};
    RenderTexture* MainTex {};
    uint Width {};
    uint Height {};
    unique_ptr<SpaceNode> RootNode {};
    int CurX {};
    int CurY {};
    uint LineMaxH {};
    uint LineCurH {};
    uint LineW {};
};

struct SpriteInfo
{
    TextureAtlas* Atlas {};
    FRect SprRect {};
    ushort Width {};
    ushort Height {};
    short OffsX {};
    short OffsY {};
    RenderEffect* DrawEffect {};
    bool UsedForModel {};
    ModelInstance* Model {};
    uchar* Data {};
    AtlasType DataAtlasType {};
    bool DataAtlasOneImage {};
};

struct AnyFrames
{
    [[nodiscard]] auto GetSprId(uint num_frm) const -> uint;
    [[nodiscard]] auto GetNextX(uint num_frm) const -> short;
    [[nodiscard]] auto GetNextY(uint num_frm) const -> short;
    [[nodiscard]] auto GetCurSprId(uint tick) const -> uint;
    [[nodiscard]] auto GetCurSprIndex(uint tick) const -> uint;
    [[nodiscard]] auto GetDir(int dir) -> AnyFrames*;

    uint Ind[MAX_FRAMES] {}; // Sprite Ids
    short NextX[MAX_FRAMES] {};
    short NextY[MAX_FRAMES] {};
    uint CntFrm {};
    uint Ticks {}; // Time of playing animation
    uint Anim1 {};
    uint Anim2 {};
    hash NameHash {};
    int DirCount {1};
    AnyFrames* Dirs[7] {}; // 7 additional for square hexes, 5 for hexagonal
};
static_assert(std::is_standard_layout_v<AnyFrames>);

struct PrimitivePoint
{
    int PointX {};
    int PointY {};
    uint PointColor {};
    short* PointOffsX {};
    short* PointOffsY {};
};
using PrimitivePoints = vector<PrimitivePoint>;

struct DipData
{
    RenderTexture* MainTex {};
    RenderEffect* SourceEffect {};
    uint SpritesCount {};
    FRect SpriteBorder {};
};

class SpriteManager final
{
public:
    SpriteManager() = delete;
    SpriteManager(RenderSettings& settings, FileManager& file_mngr, EffectManager& effect_mngr, ClientScriptSystem& script_sys, GameTimer& game_time);
    SpriteManager(const SpriteManager&) = delete;
    SpriteManager(SpriteManager&&) noexcept = delete;
    auto operator=(const SpriteManager&) = delete;
    auto operator=(SpriteManager&&) noexcept = delete;
    ~SpriteManager();

    [[nodiscard]] auto GetWindowSize() const -> tuple<int, int>;
    [[nodiscard]] auto GetWindowPosition() const -> tuple<int, int>;
    [[nodiscard]] auto GetMousePosition() const -> tuple<int, int>;
    [[nodiscard]] auto IsWindowFocused() const -> bool;
    [[nodiscard]] auto CreateRenderTarget(bool with_depth, bool screen_sized, uint width, uint height, bool linear_filtered) -> RenderTarget*;
    [[nodiscard]] auto GetRenderTargetPixel(RenderTarget* rt, int x, int y) const -> uint;
    [[nodiscard]] auto GetSpritesColor() const -> uint { return _baseColor; }
    [[nodiscard]] auto GetSpritesInfo() -> vector<SpriteInfo*>& { return _sprData; }
    [[nodiscard]] auto GetSpriteInfo(uint id) -> const SpriteInfo* { return _sprData[id]; }
    [[nodiscard]] auto GetSpriteInfoForEditing(uint id) -> SpriteInfo* { return _sprData[id]; }
    [[nodiscard]] auto GetDrawRect(Sprite* prep) const -> IRect;
    [[nodiscard]] auto GetPixColor(uint spr_id, int offs_x, int offs_y, bool with_zoom) const -> uint;
    [[nodiscard]] auto IsPixNoTransp(uint spr_id, int offs_x, int offs_y, bool with_zoom) const -> bool;
    [[nodiscard]] auto IsEggTransp(int pix_x, int pix_y) const -> bool;
    [[nodiscard]] auto CompareHexEgg(ushort hx, ushort hy, int egg_type) const -> bool;
    [[nodiscard]] auto IsAccumulateAtlasActive() const -> bool;

    [[nodiscard]] auto LoadAnimation(string_view fname, bool use_dummy, bool frm_anim_pix) -> AnyFrames*;
    [[nodiscard]] auto ReloadAnimation(AnyFrames* anim, string_view fname) -> AnyFrames*;
    [[nodiscard]] auto LoadModel(string_view fname, bool auto_redraw) -> ModelInstance*;
    [[nodiscard]] auto CreateAnyFrames(uint frames, uint ticks) -> AnyFrames*;

    void SetWindowSize(int w, int h);
    void SetWindowPosition(int x, int y);
    void SetMousePosition(int x, int y);
    void MinimizeWindow();
    auto EnableFullscreen() -> bool;
    auto DisableFullscreen() -> bool;
    void BlinkWindow();
    void SetAlwaysOnTop(bool enable);
    void Preload3dModel(string_view model_name) const;
    void BeginScene(uint clear_color);
    void EndScene();
    void OnResolutionChanged();
    void PushRenderTarget(RenderTarget* rt);
    void PopRenderTarget();
    void DrawRenderTarget(RenderTarget* rt, bool alpha_blend, const IRect* region_from, const IRect* region_to);
    void ClearCurrentRenderTarget(uint color);
    void ClearCurrentRenderTargetDepth();
    void PushAtlasType(AtlasType atlas_type);
    void PushAtlasType(AtlasType atlas_type, bool one_image);
    void PopAtlasType();
    void AccumulateAtlasData();
    void FlushAccumulatedAtlasData();
    void DestroyAtlases(AtlasType atlas_type);
    void DumpAtlases();
    void RefreshModelSprite(ModelInstance* model);
    void FreeModel(ModelInstance* model);
    void CreateAnyFramesDirAnims(AnyFrames* anim, uint dirs);
    void DestroyAnyFrames(AnyFrames* anim);
    void SetSpritesColor(uint c) { _baseColor = c; }
    void PrepareSquare(PrimitivePoints& points, const IRect& r, uint color);
    void PrepareSquare(PrimitivePoints& points, IPoint lt, IPoint rt, IPoint lb, IPoint rb, uint color);
    void PushScissor(int l, int t, int r, int b);
    void PopScissor();
    void Flush();
    void DrawSprite(uint id, int x, int y, uint color);
    void DrawSprite(AnyFrames* frames, int x, int y, uint color);
    void DrawSpriteSize(AnyFrames* frames, int x, int y, int w, int h, bool zoom_up, bool center, uint color);
    void DrawSpriteSize(uint id, int x, int y, int w, int h, bool zoom_up, bool center, uint color);
    void DrawSpriteSizeExt(uint id, int x, int y, int w, int h, bool zoom_up, bool center, bool stretch, uint color);
    void DrawSpritePattern(uint id, int x, int y, int w, int h, int spr_width, int spr_height, uint color);
    void DrawSprites(Sprites& dtree, bool collect_contours, bool use_egg, int draw_oder_from, int draw_oder_to, bool prerender, int prerender_ox, int prerender_oy);
    void DrawPoints(PrimitivePoints& points, RenderPrimitiveType prim, const float* zoom, FPoint* offset, RenderEffect* custom_effect);
    void Draw3d(int x, int y, ModelInstance* model, uint color);
    void DrawContours();
    void InitializeEgg(string_view egg_name);
    void SetEgg(ushort hx, ushort hy, Sprite* spr);
    void EggNotValid() { _eggValid = false; }

    AnyFrames* DummyAnimation {};

private:
    [[nodiscard]] auto CreateAtlas(uint w, uint h) -> TextureAtlas*;
    [[nodiscard]] auto FindAtlasPlace(SpriteInfo* si, int& x, int& y) -> TextureAtlas*;
    [[nodiscard]] auto RequestFillAtlas(SpriteInfo* si, uint w, uint h, uchar* data) -> uint;
    [[nodiscard]] auto Load2dAnimation(string_view fname) -> AnyFrames*;
    [[nodiscard]] auto Load3dAnimation(string_view fname) -> AnyFrames*;

    void FillAtlas(SpriteInfo* si);
    void RenderModel(ModelInstance* model);
    void RefreshScissor();
    void EnableScissor();
    void DisableScissor();
    void CollectContour(int x, int y, const SpriteInfo* si, const Sprite* spr);

    RenderSettings& _settings;
    FileManager& _fileMngr;
    EffectManager& _effectMngr;
    GameTimer& _gameTime;
    unique_ptr<ModelManager> _modelMngr {};
    mat44 _projectionMatrixCm {};
    RenderTarget* _rtMain {};
    RenderTarget* _rtContours {};
    RenderTarget* _rtContoursMid {};
    vector<RenderTarget*> _rt3D {};
    vector<RenderTarget*> _rtStack {};
    vector<unique_ptr<RenderTarget>> _rtAll {};
    vector<tuple<AtlasType, bool>> _atlasStack {};
    vector<unique_ptr<TextureAtlas>> _allAtlases {};
    bool _accumulatorActive {};
    vector<SpriteInfo*> _accumulatorSprInfo {};
    vector<SpriteInfo*> _sprData {};
    vector<ModelInstance*> _autoRedrawModel {};
    MemoryPool<sizeof(AnyFrames), ANY_FRAMES_POOL_SIZE> _anyFramesPool {};
    vector<ushort> _quadsIndices {};
    vector<ushort> _pointsIndices {};
    Vertex2DVec _vBuffer {};
    vector<DipData> _dipQueue {};
    uint _baseColor {};
    int _drawQuadCount {};
    int _curDrawQuad {};
    vector<int> _scissorStack {};
    IRect _scissorRect {};
    bool _contoursAdded {};
    bool _eggValid {};
    ushort _eggHx {};
    ushort _eggHy {};
    int _eggX {};
    int _eggY {};
    const SpriteInfo* _sprEgg {};
    vector<uint> _eggData {};
    int _eggSprWidth {};
    int _eggSprHeight {};
    float _eggAtlasWidth {};
    float _eggAtlasHeight {};
    bool _nonConstHelper {};

    // Todo: move fonts stuff to separate module
public:
    [[nodiscard]] auto GetLinesCount(int width, int height, string_view str, int num_font) -> int;
    [[nodiscard]] auto GetLinesHeight(int width, int height, string_view str, int num_font) -> int;
    [[nodiscard]] auto GetLineHeight(int num_font) -> int;
    [[nodiscard]] auto GetTextInfo(int width, int height, string_view str, int num_font, uint flags, int& tw, int& th, int& lines) -> bool;
    [[nodiscard]] auto HaveLetter(int num_font, uint letter) -> bool;

    [[nodiscard]] auto SplitLines(const IRect& r, string_view cstr, int num_font) -> vector<string>;

    auto LoadFontFO(int index, string_view font_name, bool not_bordered, bool skip_if_loaded) -> bool;
    auto LoadFontBmf(int index, string_view font_name) -> bool;
    void SetDefaultFont(int index, uint color);
    void SetFontEffect(int index, RenderEffect* effect);
    void BuildFonts();
    void DrawStr(const IRect& r, string_view str, uint flags, uint color, int num_font);
    void ClearFonts();

private:
    static constexpr int FONT_BUF_LEN = 0x5000;
    static constexpr int FONT_MAX_LINES = 1000;
    static constexpr int FORMAT_TYPE_DRAW = 0;
    static constexpr int FORMAT_TYPE_SPLIT = 1;
    static constexpr int FORMAT_TYPE_LCOUNT = 2;

    struct FontData
    {
        struct Letter
        {
            short PosX {};
            short PosY {};
            short Width {};
            short Height {};
            short OffsX {};
            short OffsY {};
            short XAdvance {};
            FRect TexUV {};
            FRect TexBorderedUV {};
        };

        RenderEffect* DrawEffect {};
        bool Builded {};
        RenderTexture* FontTex {};
        RenderTexture* FontTexBordered {};
        map<uint, Letter> Letters {};
        int SpaceWidth {};
        int LineHeight {};
        int YAdvance {};
        AnyFrames* ImageNormal {};
        AnyFrames* ImageBordered {};
        bool MakeGray {};
    };

    struct FontFormatInfo
    {
        FontData* CurFont {};
        uint Flags {};
        IRect Region {};
        char Str[FONT_BUF_LEN] {};
        char* PStr {};
        int LinesAll {};
        int LinesInRect {};
        int CurX {};
        int CurY {};
        int MaxCurX {};
        uint ColorDots[FONT_BUF_LEN] {};
        int LineWidth[FONT_MAX_LINES] {};
        int LineSpaceWidth[FONT_MAX_LINES] {};
        int OffsColDots {};
        uint DefColor {};
        vector<string>* StrLines {};
        bool IsError {};
    };

    [[nodiscard]] auto GetFont(int num) -> FontData*;

    void BuildFont(int index);
    void FormatText(FontFormatInfo& fi, int fmt_type);

    vector<unique_ptr<FontData>> _allFonts {};
    int _defFontIndex {-1};
    uint _defFontColor {};
};
