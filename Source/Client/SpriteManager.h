#pragma once

#include "Common.h"
#include "FileUtils.h"
#include "GraphicApi.h"
#include "GraphicStructures.h"

// Font flags
#define FT_NOBREAK (0x0001)
#define FT_NOBREAK_LINE (0x0002)
#define FT_CENTERX (0x0004)
#define FT_CENTERY (0x0008 | FT_CENTERY_ENGINE)
#define FT_CENTERY_ENGINE (0x1000) // Temporary workaround
#define FT_CENTERR (0x0010)
#define FT_BOTTOM (0x0020)
#define FT_UPPER (0x0040)
#define FT_NO_COLORIZE (0x0080)
#define FT_ALIGN (0x0100)
#define FT_BORDERED (0x0200)
#define FT_SKIPLINES(l) (0x0400 | ((l) << 16))
#define FT_SKIPLINES_END(l) (0x0800 | ((l) << 16))

// Colors
#define COLOR_LIGHT(c) \
    SpriteManager::PackColor((((c) >> 16) & 0xFF) + GameOpt.Light, (((c) >> 8) & 0xFF) + GameOpt.Light, \
        ((c)&0xFF) + GameOpt.Light, (((c) >> 24) & 0xFF))
#define COLOR_SCRIPT_SPRITE(c) ((c) ? COLOR_LIGHT(c) : COLOR_LIGHT(COLOR_IFACE_FIX))
#define COLOR_SCRIPT_TEXT(c) ((c) ? COLOR_LIGHT(c) : COLOR_LIGHT(COLOR_TEXT))
#define COLOR_CHANGE_ALPHA(v, a) ((((v) | 0xFF000000) ^ 0xFF000000) | ((uint)(a)&0xFF) << 24)
#define COLOR_IFACE_FIX COLOR_GAME_RGB(103, 95, 86)
#define COLOR_IFACE \
    SpriteManager::PackColor(((COLOR_IFACE_FIX >> 16) & 0xFF) + GameOpt.Light, \
        ((COLOR_IFACE_FIX >> 8) & 0xFF) + GameOpt.Light, (COLOR_IFACE_FIX & 0xFF) + GameOpt.Light)
#define COLOR_GAME_RGB(r, g, b) SpriteManager::PackColor((r) + GameOpt.Light, (g) + GameOpt.Light, (b) + GameOpt.Light)
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
#define DRAW_ORDER_ITEM_AUTO(i) \
    (i->GetIsFlat() ? (!i->IsAnyScenery() ? DRAW_ORDER_FLAT_ITEM : DRAW_ORDER_FLAT_SCENERY) : \
                      (!i->IsAnyScenery() ? DRAW_ORDER_ITEM : DRAW_ORDER_SCENERY))
#define DRAW_ORDER_CRIT_AUTO(c) (c->IsDead() && !c->GetIsNoFlatten() ? DRAW_ORDER_DEAD_CRITTER : DRAW_ORDER_CRITTER)

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

// Primitives
#define PRIMITIVE_POINTLIST (1)
#define PRIMITIVE_LINELIST (2)
#define PRIMITIVE_LINESTRIP (3)
#define PRIMITIVE_TRIANGLELIST (4)
#define PRIMITIVE_TRIANGLESTRIP (5)
#define PRIMITIVE_TRIANGLEFAN (6)

class EffectManager;
class Animation3dManager;
class Animation3d;
using Animation3dVec = vector<Animation3d*>;
class Sprites;
class Sprite;

#define RES_ATLAS_STATIC (1)
#define RES_ATLAS_DYNAMIC (2)
#define RES_ATLAS_SPLASH (3)
#define RES_ATLAS_TEXTURES (4)

class SpriteManager
{
private:
public:
    SpriteManager(EffectManager& effect_mngr);
    ~SpriteManager();

    void Preload3dModel(const string& model_name);
    void BeginScene(uint clear_color);
    void EndScene();
    void OnResolutionChanged();
    void SetAlwaysOnTop(bool enable);
    EffectManager& GetEffectManager() { return effectMngr; }

    void GetWindowSize(int& w, int& h);
    void SetWindowSize(int w, int h);
    void GetWindowPosition(int& x, int& y);
    void SetWindowPosition(int x, int y);
    void GetMousePosition(int& x, int& y);
    void SetMousePosition(int x, int y);
    bool IsWindowFocused();
    void MinimizeWindow();
    bool EnableFullscreen();
    bool DisableFullscreen();
    void BlinkWindow();

private:
    SDL_Window* mainWindow {};
    EffectManager& effectMngr;
    Animation3dManager* anim3dMngr {};
    Matrix projectionMatrixCM {};
    bool sceneBeginned {};
    GLint baseFBO {};

    // Render targets
public:
    RenderTarget* CreateRenderTarget(bool depth, bool multisampling, bool screen_size, uint width, uint height,
        bool tex_linear, Effect* effect = nullptr, RenderTarget* rt_refresh = nullptr);
    void CleanRenderTarget(RenderTarget* rt);
    void DeleteRenderTarget(RenderTarget*& rt);
    void PushRenderTarget(RenderTarget* rt);
    void PopRenderTarget();
    void DrawRenderTarget(
        RenderTarget* rt, bool alpha_blend, const Rect* region_from = nullptr, const Rect* region_to = nullptr);
    uint GetRenderTargetPixel(RenderTarget* rt, int x, int y);
    void ClearCurrentRenderTarget(uint color);
    void ClearCurrentRenderTargetDepth();
    void RefreshViewport();
    RenderTarget* Get3dRenderTarget(uint width, uint height);

private:
    RenderTarget* rtMain {};
    RenderTarget* rtContours {};
    RenderTarget* rtContoursMid {};
    RenderTargetVec rt3D {};
    RenderTargetVec rtStack {};
    RenderTargetVec rtAll {};

    // Texture atlases
public:
    void PushAtlasType(int atlas_type, bool one_image = false);
    void PopAtlasType();
    void AccumulateAtlasData();
    void FlushAccumulatedAtlasData();
    bool IsAccumulateAtlasActive();
    void DestroyAtlases(int atlas_type);
    void DumpAtlases();
    void SaveTexture(Texture* tex, const string& fname, bool flip); // tex == NULL is back buffer

private:
    TextureAtlas* CreateAtlas(int w, int h);
    TextureAtlas* FindAtlasPlace(SpriteInfo* si, int& x, int& y);
    uint RequestFillAtlas(SpriteInfo* si, uint w, uint h, uchar* data);
    void FillAtlas(SpriteInfo* si);

    int atlasWidth {};
    int atlasHeight {};
    IntVec atlasTypeStack {};
    BoolVec atlasOneImageStack {};
    TextureAtlasVec allAtlases {};
    bool accumulatorActive {};
    SprInfoVec accumulatorSprInfo {};

    // Load sprites
public:
    AnyFrames* LoadAnimation(const string& fname, bool use_dummy = false, bool frm_anim_pix = false);
    AnyFrames* ReloadAnimation(AnyFrames* anim, const string& fname);
    Animation3d* LoadPure3dAnimation(const string& fname, bool auto_redraw);
    void RefreshPure3dAnimationSprite(Animation3d* anim3d);
    void FreePure3dAnimation(Animation3d* anim3d);
    AnyFrames* CreateAnyFrames(uint frames, uint ticks);
    void CreateAnyFramesDirAnims(AnyFrames* anim);
    void DestroyAnyFrames(AnyFrames* anim);

    AnyFrames* DummyAnimation {};

private:
    AnyFrames* LoadAnimation2d(const string& fname);
    AnyFrames* LoadAnimation3d(const string& fname);
    bool Render3d(Animation3d* anim3d);

    SprInfoVec sprData {};
    Animation3dVec autoRedrawAnim3d {};
    MemoryPool<sizeof(AnyFrames), ANY_FRAMES_POOL_SIZE> anyFramesPool {};

    // Draw
public:
    static uint PackColor(int r, int g, int b, int a = 255);
    void SetSpritesColor(uint c) { baseColor = c; }
    uint GetSpritesColor() { return baseColor; }
    SprInfoVec& GetSpritesInfo() { return sprData; }
    SpriteInfo* GetSpriteInfo(uint id) { return sprData[id]; }
    void GetDrawRect(Sprite* prep, Rect& rect);
    uint GetPixColor(uint spr_id, int offs_x, int offs_y, bool with_zoom = true);
    bool IsPixNoTransp(uint spr_id, int offs_x, int offs_y, bool with_zoom = true);
    bool IsEggTransp(int pix_x, int pix_y);

    void PrepareSquare(PointVec& points, Rect r, uint color);
    void PrepareSquare(PointVec& points, Point lt, Point rt, Point lb, Point rb, uint color);
    void PushScissor(int l, int t, int r, int b);
    void PopScissor();
    bool Flush();

    bool DrawSprite(uint id, int x, int y, uint color = 0);
    bool DrawSprite(AnyFrames* frames, int x, int y, uint color = 0);
    bool DrawSpriteSize(AnyFrames* frames, int x, int y, int w, int h, bool zoom_up, bool center, uint color = 0);
    bool DrawSpriteSize(uint id, int x, int y, int w, int h, bool zoom_up, bool center, uint color = 0);
    bool DrawSpriteSizeExt(uint id, int x, int y, int w, int h, bool zoom_up, bool center, bool stretch, uint color);
    bool DrawSpritePattern(uint id, int x, int y, int w, int h, int spr_width = 0, int spr_height = 0, uint color = 0);
    bool DrawSprites(Sprites& dtree, bool collect_contours, bool use_egg, int draw_oder_from, int draw_oder_to,
        bool prerender = false, int prerender_ox = 0, int prerender_oy = 0);
    bool DrawPoints(
        PointVec& points, int prim, float* zoom = nullptr, PointF* offset = nullptr, Effect* effect = nullptr);
    bool Draw3d(int x, int y, Animation3d* anim3d, uint color);

private:
    struct VertexArray
    {
        GLuint VAO {};
        GLuint VBO {};
        GLuint IBO {};
        uint VCount {};
        uint ICount {};
        VertexArray* Next {};
    };

    void InitVertexArray(VertexArray* va, bool quads, uint count);
    void BindVertexArray(VertexArray* va);
    void EnableVertexArray(VertexArray* va, uint vertices_count);
    void DisableVertexArray(VertexArray*& va);
    void RefreshScissor();
    void EnableScissor();
    void DisableScissor();

    VertexArray* quadsVertexArray {};
    VertexArray* pointsVertexArray {};
    UShortVec quadsIndices {};
    UShortVec pointsIndices {};
    Vertex2DVec vBuffer {};
    DipDataVec dipQueue {};
    uint baseColor {};
    int drawQuadCount {};
    int curDrawQuad {};
    IntVec scissorStack {};
    Rect scissorRect {};

    // Contours
public:
    bool DrawContours();

private:
    bool CollectContour(int x, int y, SpriteInfo* si, Sprite* spr);

    bool contoursAdded {};

    // Transparent egg
public:
    void InitializeEgg(const string& egg_name);
    bool CompareHexEgg(ushort hx, ushort hy, int egg_type);
    void SetEgg(ushort hx, ushort hy, Sprite* spr);
    void EggNotValid() { eggValid = false; }

private:
    bool eggValid {};
    ushort eggHx {};
    ushort eggHy {};
    int eggX {};
    int eggY {};
    SpriteInfo* sprEgg {};
    uint* eggData {};
    int eggSprWidth {};
    int eggSprHeight {};
    float eggAtlasWidth {};
    float eggAtlasHeight {};

    // Fonts
public:
    void ClearFonts();
    void SetDefaultFont(int index, uint color);
    void SetFontEffect(int index, Effect* effect);
    bool LoadFontFO(int index, const string& font_name, bool not_bordered, bool skip_if_loaded = true);
    bool LoadFontBMF(int index, const string& font_name);
    void BuildFonts();
    bool DrawStr(const Rect& r, const string& str, uint flags, uint color = 0, int num_font = -1);
    int GetLinesCount(int width, int height, const string& str, int num_font = -1);
    int GetLinesHeight(int width, int height, const string& str, int num_font = -1);
    int GetLineHeight(int num_font = -1);
    void GetTextInfo(int width, int height, const string& str, int num_font, int flags, int& tw, int& th, int& lines);
    int SplitLines(const Rect& r, const string& cstr, int num_font, StrVec& str_vec);
    bool HaveLetter(int num_font, uint letter);

private:
    void BuildFont(int index);
};
