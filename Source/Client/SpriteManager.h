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

#define ANY_FRAMES_POOL_SIZE (2000)
#define MAX_STORED_PIXEL_PICKS (100)
#define MAX_FRAMES (50)

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
#define COLOR_LIGHT(c) SpriteManager::PackColor(((c) >> 16) & 0xFF, ((c) >> 8) & 0xFF, (c)&0xFF, ((c) >> 24) & 0xFF)
#define COLOR_SCRIPT_SPRITE(c) ((c) ? COLOR_LIGHT(c) : COLOR_LIGHT(COLOR_IFACE_FIX))
#define COLOR_SCRIPT_TEXT(c) ((c) ? COLOR_LIGHT(c) : COLOR_LIGHT(COLOR_TEXT))
#define COLOR_CHANGE_ALPHA(v, a) ((((v) | 0xFF000000) ^ 0xFF000000) | ((uint)(a)&0xFF) << 24)
#define COLOR_IFACE_FIX COLOR_GAME_RGB(103, 95, 86)
#define COLOR_IFACE COLOR_LIGHT(COLOR_IFACE_FIX)
#define COLOR_GAME_RGB(r, g, b) SpriteManager::PackColor((r), (g), (b), 255)
#define COLOR_IFACE_RED (COLOR_IFACE | (0xFF << 16))
#define COLOR_CRITTER_NAME COLOR_GAME_RGB(0xAD, 0xAD, 0xB9)
#define COLOR_TEXT COLOR_GAME_RGB(60, 248, 0)
#define COLOR_TEXT_WHITE COLOR_GAME_RGB(0xFF, 0xFF, 0xFF)
#define COLOR_TEXT_DWHITE COLOR_GAME_RGB(0xBF, 0xBF, 0xBF)
#define COLOR_TEXT_RED COLOR_GAME_RGB(0xC8, 0, 0)

// Sprite layers
#define DRAW_ORDER_FLAT (0)
#define DRAW_ORDER (20)
#define DRAW_ORDER_TILE (DRAW_ORDER_FLAT + 0)
#define DRAW_ORDER_TILE_END (DRAW_ORDER_FLAT + 4)
#define DRAW_ORDER_HEX_GRID (DRAW_ORDER_FLAT + 5)
#define DRAW_ORDER_FLAT_SCENERY (DRAW_ORDER_FLAT + 8)
#define DRAW_ORDER_LIGHT (DRAW_ORDER_FLAT + 9)
#define DRAW_ORDER_DEAD_CRITTER (DRAW_ORDER_FLAT + 10)
#define DRAW_ORDER_FLAT_ITEM (DRAW_ORDER_FLAT + 13)
#define DRAW_ORDER_TRACK (DRAW_ORDER_FLAT + 16)
#define DRAW_ORDER_SCENERY (DRAW_ORDER + 3)
#define DRAW_ORDER_ITEM (DRAW_ORDER + 6)
#define DRAW_ORDER_CRITTER (DRAW_ORDER + 9)
#define DRAW_ORDER_RAIN (DRAW_ORDER + 12)
#define DRAW_ORDER_LAST (39)
#define DRAW_ORDER_ITEM_AUTO(i) ((i)->GetIsFlat() ? (!(i)->IsAnyScenery() ? DRAW_ORDER_FLAT_ITEM : DRAW_ORDER_FLAT_SCENERY) : (!(i)->IsAnyScenery() ? DRAW_ORDER_ITEM : DRAW_ORDER_SCENERY))
#define DRAW_ORDER_CRIT_AUTO(c) ((c)->IsDead() && !(c)->GetIsNoFlatten() ? DRAW_ORDER_DEAD_CRITTER : DRAW_ORDER_CRITTER)

// Sprites cutting
#define SPRITE_CUT_HORIZONTAL (1)
#define SPRITE_CUT_VERTICAL (2)
#define SPRITE_CUT_CUSTOM (3) // Todo

// Egg types
#define EGG_ALWAYS (1)
#define EGG_X (2)
#define EGG_Y (3)
#define EGG_X_AND_Y (4)
#define EGG_X_OR_Y (5)

// Contour types
#define CONTOUR_RED (1)
#define CONTOUR_YELLOW (2)
#define CONTOUR_CUSTOM (3)

class Animation3dManager;
class Animation3d;
using Animation3dVec = vector<Animation3d*>;
class Sprites;
class Sprite;

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
    uint CurX {};
    uint CurY {};
    uint LineMaxH {};
    uint LineCurH {};
    uint LineW {};
};

struct SpriteInfo
{
    TextureAtlas* Atlas {};
    RectF SprRect {};
    short Width {};
    short Height {};
    short OffsX {};
    short OffsY {};
    RenderEffect* DrawEffect {};
    bool UsedForAnim3d {};
    Animation3d* Anim3d {};
    uchar* Data {};
    AtlasType DataAtlasType {};
    bool DataAtlasOneImage {};
};
using SprInfoVec = vector<SpriteInfo*>;

struct AnyFrames
{
    auto GetSprId(uint num_frm) -> uint;
    auto GetNextX(uint num_frm) -> short;
    auto GetNextY(uint num_frm) -> short;
    auto GetCurSprId(uint tick) -> uint;
    [[nodiscard]] auto GetCurSprIndex(uint tick) const -> uint;
    auto GetDir(int dir) -> AnyFrames*;

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
using AnimMap = map<uint, AnyFrames*>;
using AnimVec = vector<AnyFrames*>;

struct PrepPoint
{
    int PointX {};
    int PointY {};
    uint PointColor {};
    short* PointOffsX {};
    short* PointOffsY {};
};
using PointVec = vector<PrepPoint>;

struct DipData
{
    RenderTexture* MainTex {};
    RenderEffect* SourceEffect {};
    uint SpritesCount {};
    RectF SpriteBorder {};
};
using DipDataVec = vector<DipData>;

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

    void GetWindowSize(int& w, int& h) const;
    void SetWindowSize(int w, int h);
    void GetWindowPosition(int& x, int& y) const;
    void SetWindowPosition(int x, int y);
    void GetMousePosition(int& x, int& y) const;
    void SetMousePosition(int x, int y);
    [[nodiscard]] auto IsWindowFocused() const -> bool;
    void MinimizeWindow();
    auto EnableFullscreen() -> bool;
    auto DisableFullscreen() -> bool;
    void BlinkWindow();
    void SetAlwaysOnTop(bool enable);

    void Preload3dModel(const string& model_name) const;
    void BeginScene(uint clear_color);
    void EndScene();
    void OnResolutionChanged();

    [[nodiscard]] auto CreateRenderTarget(bool with_depth, bool multisampled, bool screen_sized, uint width, uint height, bool linear_filtered) -> RenderTarget*;
    void PushRenderTarget(RenderTarget* rt);
    void PopRenderTarget();
    void DrawRenderTarget(RenderTarget* rt, bool alpha_blend, const Rect* region_from, const Rect* region_to);
    [[nodiscard]] auto GetRenderTargetPixel(RenderTarget* rt, int x, int y) const -> uint;
    void ClearCurrentRenderTarget(uint color);
    void ClearCurrentRenderTargetDepth();

private:
    RenderSettings& _settings;
    FileManager& _fileMngr;
    EffectManager& _effectMngr;
    GameTimer& _gameTime;
    unique_ptr<Animation3dManager> _anim3dMngr {};
    Matrix _projectionMatrixCm {};
    RenderTarget* _rtMain {};
    RenderTarget* _rtContours {};
    RenderTarget* _rtContoursMid {};
    vector<RenderTarget*> _rt3D {};
    vector<RenderTarget*> _rtStack {};
    vector<unique_ptr<RenderTarget>> _rtAll {};

    // Texture atlases
public:
    void PushAtlasType(AtlasType atlas_type);
    void PushAtlasType(AtlasType atlas_type, bool one_image);
    void PopAtlasType();
    void AccumulateAtlasData();
    void FlushAccumulatedAtlasData();
    [[nodiscard]] auto IsAccumulateAtlasActive() const -> bool;
    void DestroyAtlases(AtlasType atlas_type);
    void DumpAtlases();

private:
    [[nodiscard]] auto CreateAtlas(int w, int h) -> TextureAtlas*;
    [[nodiscard]] auto FindAtlasPlace(SpriteInfo* si, int& x, int& y) -> TextureAtlas*;
    [[nodiscard]] auto RequestFillAtlas(SpriteInfo* si, uint w, uint h, uchar* data) -> uint;
    void FillAtlas(SpriteInfo* si);

    vector<tuple<AtlasType, bool>> _atlasStack {};
    vector<unique_ptr<TextureAtlas>> _allAtlases {};
    bool _accumulatorActive {};
    SprInfoVec _accumulatorSprInfo {};

    // Load sprites
public:
    [[nodiscard]] auto LoadAnimation(const string& fname, bool use_dummy, bool frm_anim_pix) -> AnyFrames*;
    [[nodiscard]] auto ReloadAnimation(AnyFrames* anim, const string& fname) -> AnyFrames*;
    [[nodiscard]] auto LoadPure3dAnimation(const string& fname, bool auto_redraw) -> Animation3d*;
    void RefreshPure3dAnimationSprite(Animation3d* anim3d);
    void FreePure3dAnimation(Animation3d* anim3d);
    [[nodiscard]] auto CreateAnyFrames(uint frames, uint ticks) -> AnyFrames*;
    void CreateAnyFramesDirAnims(AnyFrames* anim, uint dirs);
    void DestroyAnyFrames(AnyFrames* anim);

    AnyFrames* DummyAnimation {};

private:
    [[nodiscard]] auto LoadAnimation2d(const string& fname) -> AnyFrames*;
    [[nodiscard]] auto LoadAnimation3d(const string& fname) -> AnyFrames*;
    void Render3d(Animation3d* anim3d);

    SprInfoVec _sprData {};
    Animation3dVec _autoRedrawAnim3d {};
    MemoryPool<sizeof(AnyFrames), ANY_FRAMES_POOL_SIZE> _anyFramesPool {};

    // Draw
public:
    static auto PackColor(int r, int g, int b, int a) -> uint;
    void SetSpritesColor(uint c) { _baseColor = c; }
    [[nodiscard]] auto GetSpritesColor() const -> uint { return _baseColor; }
    [[nodiscard]] auto GetSpritesInfo() -> SprInfoVec& { return _sprData; }
    [[nodiscard]] auto GetSpriteInfo(uint id) -> SpriteInfo* { return _sprData[id]; }
    [[nodiscard]] auto GetDrawRect(Sprite* prep) const -> Rect;
    [[nodiscard]] auto GetPixColor(uint spr_id, int offs_x, int offs_y, bool with_zoom) const -> uint;
    [[nodiscard]] auto IsPixNoTransp(uint spr_id, int offs_x, int offs_y, bool with_zoom) const -> bool;
    [[nodiscard]] auto IsEggTransp(int pix_x, int pix_y) const -> bool;

    void PrepareSquare(PointVec& points, const Rect& r, uint color);
    void PrepareSquare(PointVec& points, Point lt, Point rt, Point lb, Point rb, uint color);
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
    void DrawPoints(PointVec& points, RenderPrimitiveType prim, const float* zoom, PointF* offset, RenderEffect* custom_effect);
    void Draw3d(int x, int y, Animation3d* anim3d, uint color);

private:
    void RefreshScissor();
    void EnableScissor();
    void DisableScissor();

    UShortVec _quadsIndices {};
    UShortVec _pointsIndices {};
    Vertex2DVec _vBuffer {};
    DipDataVec _dipQueue {};
    uint _baseColor {};
    int _drawQuadCount {};
    int _curDrawQuad {};
    IntVec _scissorStack {};
    Rect _scissorRect {};

    // Contours
public:
    void DrawContours();

private:
    auto CollectContour(int x, int y, SpriteInfo* si, Sprite* spr) -> bool;

    bool _contoursAdded {};

    // Transparent egg
public:
    void InitializeEgg(const string& egg_name);
    [[nodiscard]] auto CompareHexEgg(ushort hx, ushort hy, int egg_type) const -> bool;
    void SetEgg(ushort hx, ushort hy, Sprite* spr);
    void EggNotValid() { _eggValid = false; }

private:
    bool _eggValid {};
    ushort _eggHx {};
    ushort _eggHy {};
    int _eggX {};
    int _eggY {};
    SpriteInfo* _sprEgg {};
    vector<uint> _eggData {};
    int _eggSprWidth {};
    int _eggSprHeight {};
    float _eggAtlasWidth {};
    float _eggAtlasHeight {};

    // Todo: move fonts stuff to separate module
public:
    void ClearFonts();
    void SetDefaultFont(int index, uint color);
    void SetFontEffect(int index, RenderEffect* effect);
    auto LoadFontFO(int index, const string& font_name, bool not_bordered, bool skip_if_loaded) -> bool;
    auto LoadFontBmf(int index, const string& font_name) -> bool;
    void BuildFonts();
    void DrawStr(const Rect& r, const string& str, uint flags, uint color, int num_font);
    auto GetLinesCount(int width, int height, const string& str, int num_font) -> int;
    auto GetLinesHeight(int width, int height, const string& str, int num_font) -> int;
    auto GetLineHeight(int num_font) -> int;
    void GetTextInfo(int width, int height, const string& str, int num_font, uint flags, int& tw, int& th, int& lines);
    auto SplitLines(const Rect& r, const string& cstr, int num_font, StrVec& str_vec) -> int;
    auto HaveLetter(int num_font, uint letter) -> bool;

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
            RectF TexUV {};
            RectF TexBorderedUV {};
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
        Rect Region {};
        char Str[FONT_BUF_LEN] {};
        char* PStr {};
        uint LinesAll {};
        uint LinesInRect {};
        int CurX {};
        int CurY {};
        int MaxCurX {};
        uint ColorDots[FONT_BUF_LEN] {};
        short LineWidth[FONT_MAX_LINES] {};
        ushort LineSpaceWidth[FONT_MAX_LINES] {};
        uint OffsColDots {};
        uint DefColor {};
        StrVec* StrLines {};
        bool IsError {};
    };

    [[nodiscard]] auto GetFont(int num) -> FontData*;
    void BuildFont(int index);
    static void FormatText(FontFormatInfo& fi, int fmt_type);

    vector<unique_ptr<FontData>> _allFonts {};
    int _defFontIndex {-1};
    uint _defFontColor {};
};
