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

// Todo: optimize sprite atlas filling

#pragma once

#include "Common.h"

#include "3dStuff.h"
#include "Application.h"
#include "EffectManager.h"
#include "FileSystem.h"
#include "MapSprite.h"
#include "Settings.h"
#include "VisualParticles.h"

static constexpr auto MAX_STORED_PIXEL_PICKS = 100;

// Font flags
// Todo: convert FT_ font flags to enum
static constexpr uint FT_NOBREAK = 0x0001;
static constexpr uint FT_NOBREAK_LINE = 0x0002;
static constexpr uint FT_CENTERX = 0x0004;
static constexpr uint FT_CENTERY_ENGINE = 0x1000; // Todo: fix FT_CENTERY_ENGINE workaround
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
static constexpr auto COLOR_SPRITE = COLOR_RGB(128, 128, 128);
static constexpr auto COLOR_SPRITE_RED = COLOR_SPRITE | 255 << 16;
static constexpr auto COLOR_TEXT = COLOR_RGB(60, 248, 0);
static constexpr auto COLOR_TEXT_WHITE = COLOR_RGB(255, 255, 255);
static constexpr auto COLOR_TEXT_DWHITE = COLOR_RGB(191, 191, 191);
static constexpr auto COLOR_TEXT_RED = COLOR_RGB(200, 0, 0);
static constexpr auto COLOR_SCRIPT_SPRITE(uint color) -> uint
{
    return color != 0 ? color : COLOR_SPRITE;
}
static constexpr auto COLOR_SCRIPT_TEXT(uint color) -> uint
{
    return color != 0 ? color : COLOR_TEXT;
}

enum class AtlasType
{
    IfaceSprites,
    MapSprites,
    MeshTextures,
    OneImage,
};

class GameTimer;

struct RenderTarget
{
    enum class SizeType
    {
        Custom,
        Screen,
        Map,
    };

    unique_ptr<RenderTexture> MainTex {};
    RenderEffect* CustomDrawEffect {};
    SizeType Size {};
    int BaseWidth {};
    int BaseHeight {};
    vector<tuple<int, int, uint>> LastPixelPicks {};
};

class TextureAtlas final
{
public:
    class SpaceNode final
    {
    public:
        SpaceNode(int x, int y, int width, int height);
        SpaceNode(const SpaceNode&) = delete;
        SpaceNode(SpaceNode&&) noexcept = default;
        auto operator=(const SpaceNode&) = delete;
        auto operator=(SpaceNode&&) noexcept -> SpaceNode& = default;
        ~SpaceNode() = default;

        [[nodiscard]] auto IsBusyRecursively() const noexcept -> bool;

        auto FindPosition(int width, int height) -> SpaceNode*;
        void Free() noexcept;

        int PosX {};
        int PosY {};
        int Width {};
        int Height {};
        bool Busy {};
        vector<unique_ptr<SpaceNode>> Children {};
    };

    TextureAtlas() = default;
    TextureAtlas(const TextureAtlas&) = delete;
    TextureAtlas(TextureAtlas&&) noexcept = default;
    auto operator=(const TextureAtlas&) = delete;
    auto operator=(TextureAtlas&&) noexcept -> TextureAtlas& = default;
    ~TextureAtlas() = default;

    // Todo: incapsulate texture atlas & atlas space node data
    AtlasType Type {};
    RenderTarget* RTarg {};
    RenderTexture* MainTex {};
    int Width {};
    int Height {};
    unique_ptr<SpaceNode> RootNode {};
};

class Sprite
{
public:
    Sprite() = default;
    Sprite(const Sprite&) = delete;
    Sprite(Sprite&&) noexcept = default;
    auto operator=(const Sprite&) = delete;
    auto operator=(Sprite&&) noexcept -> Sprite& = default;
    virtual ~Sprite() = default;

    virtual auto FillData(RenderDrawBuffer* dbuf, const FRect& pos, const tuple<uint, uint>& colors) const -> size_t = 0;

    // Todo: incapsulate sprite data
    int Width {};
    int Height {};
    int OffsX {};
    int OffsY {};
    TextureAtlas* Atlas {};
    RenderEffect* DrawEffect {};
};

class SpriteRef final : public Sprite
{
public:
    explicit SpriteRef(const Sprite* ref);
    SpriteRef(const SpriteRef&) = delete;
    SpriteRef(SpriteRef&&) noexcept = default;
    auto operator=(const SpriteRef&) = delete;
    auto operator=(SpriteRef&&) noexcept -> SpriteRef& = default;
    ~SpriteRef() override = default;

    auto FillData(RenderDrawBuffer* dbuf, const FRect& pos, const tuple<uint, uint>& colors) const -> size_t override;

private:
    const Sprite* _ref {};
};

class AtlasSprite : public Sprite
{
public:
    AtlasSprite() = default;
    AtlasSprite(const AtlasSprite&) = delete;
    AtlasSprite(AtlasSprite&&) noexcept = default;
    auto operator=(const AtlasSprite&) = delete;
    auto operator=(AtlasSprite&&) noexcept -> AtlasSprite& = default;
    ~AtlasSprite() override;

    auto FillData(RenderDrawBuffer* dbuf, const FRect& pos, const tuple<uint, uint>& colors) const -> size_t override;

    TextureAtlas::SpaceNode* AtlasNode {};
    FRect AtlasRect {};
    vector<bool> HitTestData {};
};

class ParticleSprite final : public AtlasSprite
{
public:
    ParticleSprite() = default;
    ParticleSprite(const ParticleSprite&) = delete;
    ParticleSprite(ParticleSprite&&) noexcept = default;
    auto operator=(const ParticleSprite&) = delete;
    auto operator=(ParticleSprite&&) noexcept -> ParticleSprite& = default;
    ~ParticleSprite() override = default;

    unique_ptr<ParticleSystem> Particle {};
};

#if FO_ENABLE_3D
class ModelSprite final : public AtlasSprite
{
public:
    ModelSprite() = default;
    ModelSprite(const ModelSprite&) = delete;
    ModelSprite(ModelSprite&&) noexcept = default;
    auto operator=(const ModelSprite&) = delete;
    auto operator=(ModelSprite&&) noexcept -> ModelSprite& = default;
    ~ModelSprite() override = default;

    unique_ptr<ModelInstance> Model {};
};
#endif

class SpriteSheet final
{
public:
    SpriteSheet(uint frames, uint ticks, uint dirs);
    SpriteSheet(const SpriteSheet&) = delete;
    SpriteSheet(SpriteSheet&&) noexcept = default;
    auto operator=(const SpriteSheet&) = delete;
    auto operator=(SpriteSheet&&) noexcept -> SpriteSheet& = default;
    ~SpriteSheet() = default;

    [[nodiscard]] auto GetSpr(uint num_frm = 0) const -> const Sprite*;
    [[nodiscard]] auto GetSpr(uint num_frm = 0) -> Sprite*;
    [[nodiscard]] auto GetCurSpr(time_point time) const -> const Sprite*;
    [[nodiscard]] auto GetDir(uint dir) const -> const SpriteSheet*;
    [[nodiscard]] auto GetDir(uint dir) -> SpriteSheet*;

    // Todo: incapsulate sprite sheet data
    vector<unique_ptr<Sprite>> Spr {};
    vector<IPoint> SprOffset {};
    uint CntFrm {}; // Todo: Spr.size()
    uint WholeTicks {};
    uint Anim1 {};
    uint Anim2 {};
    hstring Name {};
    uint DirCount {};
    unique_ptr<SpriteSheet> Dirs[GameSettings::MAP_DIR_COUNT - 1] {};
};

struct PrimitivePoint
{
    int PointX {};
    int PointY {};
    uint PointColor {};
    int* PointOffsX {};
    int* PointOffsY {};
};
static_assert(std::is_standard_layout_v<PrimitivePoint>);

struct DipData
{
    RenderTexture* MainTex {};
    RenderEffect* SourceEffect {};
    size_t IndCount {};
};

class SpriteManager final
{
public:
    SpriteManager() = delete;
    SpriteManager(RenderSettings& settings, AppWindow* window, FileSystem& resources, EffectManager& effect_mngr);
    SpriteManager(const SpriteManager&) = delete;
    SpriteManager(SpriteManager&&) noexcept = delete;
    auto operator=(const SpriteManager&) = delete;
    auto operator=(SpriteManager&&) noexcept = delete;
    ~SpriteManager();

    [[nodiscard]] auto GetWindow() { NON_CONST_METHOD_HINT_ONELINE() return _window; }
    [[nodiscard]] auto GetWindowSize() const -> tuple<int, int>;
    [[nodiscard]] auto GetScreenSize() const -> tuple<int, int>;
    [[nodiscard]] auto IsWindowFocused() const -> bool;
    [[nodiscard]] auto CreateRenderTarget(bool with_depth, RenderTarget::SizeType size, int width, int height, bool linear_filtered) -> RenderTarget*;
    [[nodiscard]] auto GetRenderTargetPixel(RenderTarget* rt, int x, int y) const -> uint;
    [[nodiscard]] auto GetDrawRect(const MapSprite* mspr) const -> IRect;
    [[nodiscard]] auto GetViewRect(const MapSprite* mspr) const -> IRect;
    [[nodiscard]] auto IsPixNoTransp(const Sprite* spr, int offs_x, int offs_y, bool with_zoom) const -> bool;
    [[nodiscard]] auto IsEggTransp(int pix_x, int pix_y) const -> bool;
    [[nodiscard]] auto CheckEggAppearence(uint16 hx, uint16 hy, EggAppearenceType egg_appearence) const -> bool;
    [[nodiscard]] auto LoadAnimation(string_view fname, AtlasType atlas_type) -> unique_ptr<SpriteSheet>;

    void SetWindowSize(int w, int h);
    void SetScreenSize(int w, int h);
    void SwitchFullscreen();
    void SetMousePosition(int x, int y);
    void MinimizeWindow();
    auto EnableFullscreen() -> bool;
    auto DisableFullscreen() -> bool;
    void BlinkWindow();
    void SetAlwaysOnTop(bool enable);
    void BeginScene(uint clear_color);
    void EndScene();
    void PushRenderTarget(RenderTarget* rt);
    void PopRenderTarget();
    void DrawRenderTarget(const RenderTarget* rt, bool alpha_blend, const IRect* region_from = nullptr, const IRect* region_to = nullptr);
    void DrawTexture(const RenderTexture* tex, bool alpha_blend, const IRect* region_from = nullptr, const IRect* region_to = nullptr, RenderEffect* custom_effect = nullptr);
    void ClearCurrentRenderTarget(uint color, bool with_depth = false);
    void DeleteRenderTarget(RenderTarget* rt);
    void DumpAtlases() const;
    void PrepareSquare(vector<PrimitivePoint>& points, const IRect& r, uint color);
    void PrepareSquare(vector<PrimitivePoint>& points, IPoint lt, IPoint rt, IPoint lb, IPoint rb, uint color);
    void PushScissor(int l, int t, int r, int b);
    void PopScissor();
    void SetSpritesZoom(float zoom) noexcept;
    void Flush();
    void DrawSprite(const Sprite* spr, int x, int y, uint color);
    void DrawSpriteSize(const Sprite* spr, int x, int y, int w, int h, bool zoom_up, bool center, uint color);
    void DrawSpriteSizeExt(const Sprite* spr, int x, int y, int w, int h, bool zoom_up, bool center, bool stretch, uint color);
    void DrawSpritePattern(const Sprite* spr, int x, int y, int w, int h, int spr_width, int spr_height, uint color);
    void DrawSprites(MapSpriteList& mspr_list, bool collect_contours, bool use_egg, DrawOrderType draw_oder_from, DrawOrderType draw_oder_to, uint color);
    void DrawPoints(const vector<PrimitivePoint>& points, RenderPrimitiveType prim, const float* zoom = nullptr, const FPoint* offset = nullptr, RenderEffect* custom_effect = nullptr);

    void DrawContours();
    void InitializeEgg(string_view egg_name, AtlasType atlas_type);
    void SetEgg(uint16 hx, uint16 hy, const MapSprite* mspr);
    void EggNotValid() { _eggValid = false; }

    void InitParticleSubsystem(GameTimer& game_time);
    auto LoadParticle(string_view name, AtlasType atlas_type) -> unique_del_ptr<ParticleSprite>;
    void DrawParticleToAtlas(ParticleSprite* particle_spr);

#if FO_ENABLE_3D
    void Init3dSubsystem(GameTimer& game_time, NameResolver& name_resolver, AnimationResolver& anim_name_resolver);
    void Preload3dModel(string_view model_name);
    auto LoadModel(string_view fname, AtlasType atlas_type) -> unique_del_ptr<ModelSprite>;
    void DrawModelToAtlas(ModelSprite* model_spr);
    void DrawModel(int x, int y, ModelSprite* model_spr, uint color);
#endif

private:
    void AllocateRenderTargetTexture(RenderTarget* rt, bool linear_filtered, bool with_depth);

    auto CreateAtlas(AtlasType atlas_type, int request_width, int request_height) -> TextureAtlas*;
    auto FindAtlasPlace(const AtlasSprite* atlas_spr, AtlasType atlas_type, int& x, int& y) -> pair<TextureAtlas*, TextureAtlas::SpaceNode*>;
    void FillAtlas(AtlasSprite* atlas_spr, AtlasType atlas_type, int width, int height, const uint* data);

    void RefreshScissor();
    void EnableScissor();
    void DisableScissor();

    void CollectContour(int x, int y, const Sprite* spr, uint contour_color);

    auto Load2dAnimation(string_view fname, AtlasType atlas_type) -> unique_ptr<SpriteSheet>;
#if FO_ENABLE_3D
    auto Load3dAnimation(string_view fname, AtlasType atlas_type) -> unique_ptr<SpriteSheet>;
#endif

    auto LoadTexture(string_view path, unordered_map<string, unique_ptr<AtlasSprite>>& collection, AtlasType atlas_type) -> pair<RenderTexture*, FRect>;

    void OnScreenSizeChanged();

    RenderSettings& _settings;
    AppWindow* _window;
    FileSystem& _resources;
    EffectManager& _effectMngr;
    EventUnsubscriber _eventUnsubscriber {};

    RenderTarget* _rtMain {};
    RenderTarget* _rtContours {};
    RenderTarget* _rtContoursMid {};
    vector<RenderTarget*> _rtIntermediate {};
    vector<RenderTarget*> _rtStack {};
    vector<unique_ptr<RenderTarget>> _rtAll {};

    vector<unique_ptr<TextureAtlas>> _allAtlases {};

    vector<DipData> _dipQueue {};
    RenderDrawBuffer* _spritesDrawBuf {};
    RenderDrawBuffer* _primitiveDrawBuf {};
    RenderDrawBuffer* _flushDrawBuf {};
    RenderDrawBuffer* _contourDrawBuf {};
    size_t _flushVertCount {};

    vector<int> _scissorStack {};
    IRect _scissorRect {};

    bool _contoursAdded {};
    bool _contourClearMid {};

    bool _eggValid {};
    uint16 _eggHx {};
    uint16 _eggHy {};
    int _eggX {};
    int _eggY {};
    unique_ptr<AtlasSprite> _sprEgg {};
    vector<uint> _eggData {};

    vector<uint> _borderBuf {};

    float _spritesZoom {1.0f};

    int _windowSizeDiffX {};
    int _windowSizeDiffY {};

    unique_ptr<ParticleManager> _particleMngr {};
    unordered_map<string, unique_ptr<AtlasSprite>> _loadedParticleTextures {};
    vector<ParticleSprite*> _autoDrawParticles {};

#if FO_ENABLE_3D
    unique_ptr<ModelManager> _modelMngr {};
    unordered_map<string, unique_ptr<AtlasSprite>> _loadedMeshTextures {};
    vector<ModelSprite*> _autoDrawModels {};
#endif

    bool _nonConstHelper {};

    // Todo: move fonts stuff to separate module
public:
    [[nodiscard]] auto GetLinesCount(int width, int height, string_view str, int num_font) -> int;
    [[nodiscard]] auto GetLinesHeight(int width, int height, string_view str, int num_font) -> int;
    [[nodiscard]] auto GetLineHeight(int num_font) -> int;
    [[nodiscard]] auto GetTextInfo(int width, int height, string_view str, int num_font, uint flags, int& tw, int& th, int& lines) -> bool;
    [[nodiscard]] auto HaveLetter(int num_font, uint letter) -> bool;

    auto LoadFontFO(int index, string_view font_name, AtlasType atlas_type, bool not_bordered, bool skip_if_loaded) -> bool;
    auto LoadFontBmf(int index, string_view font_name, AtlasType atlas_type) -> bool;
    void SetDefaultFont(int index, uint color);
    void SetFontEffect(int index, RenderEffect* effect);
    void DrawStr(const IRect& r, string_view str, uint flags, uint color, int num_font);
    auto SplitLines(const IRect& r, string_view cstr, int num_font) -> vector<string>;
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
            int16 PosX {};
            int16 PosY {};
            int16 Width {};
            int16 Height {};
            int16 OffsX {};
            int16 OffsY {};
            int16 XAdvance {};
            FRect TexUV {};
            FRect TexBorderedUV {};
        };

        RenderEffect* DrawEffect {};
        RenderTexture* FontTex {};
        RenderTexture* FontTexBordered {};
        map<uint, Letter> Letters {};
        int SpaceWidth {};
        int LineHeight {};
        int YAdvance {};
        unique_ptr<SpriteSheet> ImageNormal {};
        unique_ptr<SpriteSheet> ImageBordered {};
        bool MakeGray {};
    };

    // Todo: optimize text formatting - cache previous results
    struct FontFormatInfo
    {
        FontData* CurFont {};
        uint Flags {};
        IRect Region {};
        char Str[FONT_BUF_LEN] {};
        char* PStr {};
        int LinesAll {1};
        int LinesInRect {};
        int CurX {};
        int CurY {};
        int MaxCurX {};
        uint ColorDots[FONT_BUF_LEN] {};
        int LineWidth[FONT_MAX_LINES] {};
        int LineSpaceWidth[FONT_MAX_LINES] {};
        int OffsColDots {};
        uint DefColor {COLOR_TEXT};
        vector<string>* StrLines {};
        bool IsError {};
    };

    [[nodiscard]] auto GetFont(int num) -> FontData*;

    void BuildFont(int index);
    void FormatText(FontFormatInfo& fi, int fmt_type);

    vector<unique_ptr<FontData>> _allFonts {};
    int _defFontIndex {-1};
    uint _defFontColor {};
    FontFormatInfo _fontFormatInfoBuf {};
};
