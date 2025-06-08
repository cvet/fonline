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

// Todo: optimize sprite atlas filling

#pragma once

#include "Common.h"

#include "3dStuff.h"
#include "Application.h"
#include "EffectManager.h"
#include "FileSystem.h"
#include "Geometry.h"
#include "MapSprite.h"
#include "RenderTarget.h"
#include "Settings.h"
#include "TextureAtlas.h"
#include "VisualParticles.h"

FO_BEGIN_NAMESPACE();

// Font flags
// Todo: convert FT_ font flags to enum
static constexpr uint32 FT_NOBREAK = 0x0001;
static constexpr uint32 FT_NOBREAK_LINE = 0x0002;
static constexpr uint32 FT_CENTERX = 0x0004;
static constexpr uint32 FT_CENTERY_ENGINE = 0x1000; // Todo: fix FT_CENTERY_ENGINE workaround
static constexpr uint32 FT_CENTERY = 0x0008 | FT_CENTERY_ENGINE;
static constexpr uint32 FT_CENTERR = 0x0010;
static constexpr uint32 FT_BOTTOM = 0x0020;
static constexpr uint32 FT_UPPER = 0x0040;
static constexpr uint32 FT_NO_COLORIZE = 0x0080;
static constexpr uint32 FT_ALIGN = 0x0100;
static constexpr uint32 FT_BORDERED = 0x0200;
static constexpr auto FT_SKIPLINES(uint32 l) -> uint32
{
    return 0x0400 | (l << 16);
}
static constexpr auto FT_SKIPLINES_END(uint32 l) -> uint32
{
    return 0x0800 | (l << 16);
}

// Colors
static constexpr auto COLOR_SPRITE = ucolor {128, 128, 128};
static constexpr auto COLOR_SPRITE_RED = ucolor {255, 128, 128};
static constexpr auto COLOR_TEXT = ucolor {60, 248, 0};
static constexpr auto COLOR_TEXT_WHITE = ucolor {255, 255, 255};
static constexpr auto COLOR_TEXT_DWHITE = ucolor {191, 191, 191};
static constexpr auto COLOR_TEXT_RED = ucolor {200, 0, 0};

class AtlasSprite;

class Sprite : public std::enable_shared_from_this<Sprite>
{
    friend class SpriteManager;

public:
    explicit Sprite(SpriteManager& spr_mngr);
    Sprite(const Sprite&) = delete;
    Sprite(Sprite&&) noexcept = default;
    auto operator=(const Sprite&) = delete;
    auto operator=(Sprite&&) noexcept -> Sprite& = delete;
    virtual ~Sprite() = default;

    [[nodiscard]] virtual auto IsHitTest(ipos32 pos) const -> bool;
    [[nodiscard]] virtual auto GetBatchTex() const -> RenderTexture* { return nullptr; }
    [[nodiscard]] virtual auto GetViewSize() const -> optional<IRect> { return std::nullopt; }
    [[nodiscard]] virtual auto IsDynamicDraw() const -> bool { return false; }
    [[nodiscard]] virtual auto IsCopyable() const -> bool { return false; }
    [[nodiscard]] virtual auto MakeCopy() const -> shared_ptr<Sprite> { throw InvalidCallException(FO_LINE_STR); }
    [[nodiscard]] virtual auto IsPlaying() const -> bool { return false; }

    virtual auto FillData(RenderDrawBuffer* dbuf, const FRect& pos, const tuple<ucolor, ucolor>& colors) const -> size_t = 0;
    virtual void Prewarm() { }
    virtual void SetTime(float32 normalized_time) { ignore_unused(normalized_time); }
    virtual void SetDir(uint8 dir) { ignore_unused(dir); }
    virtual void SetDirAngle(short dir_angle) { ignore_unused(dir_angle); }
    virtual void PlayDefault() { Play({}, true, false); }
    virtual void Play(hstring anim_name, bool looped, bool reversed) { ignore_unused(anim_name, looped, reversed); }
    virtual void Stop() { }
    virtual auto Update() -> bool { return false; }

    // Todo: incapsulate sprite data
    isize Size {};
    ipos32 Offset {};
    RenderEffect* DrawEffect {};

protected:
    void StartUpdate();

    SpriteManager& _sprMngr;
};

class SpriteFactory
{
public:
    SpriteFactory() = default;
    SpriteFactory(const SpriteFactory&) = delete;
    SpriteFactory(SpriteFactory&&) noexcept = default;
    auto operator=(const SpriteFactory&) = delete;
    auto operator=(SpriteFactory&&) noexcept -> SpriteFactory& = delete;
    virtual ~SpriteFactory() = default;

    [[nodiscard]] virtual auto GetExtensions() const -> vector<string> = 0;

    virtual auto LoadSprite(hstring path, AtlasType atlas_type) -> shared_ptr<Sprite> = 0;
    virtual void Update() { }
    virtual void ClenupCache() { }
};

struct PrimitivePoint
{
    ipos32 PointPos {};
    ucolor PointColor {};
    const ipos32* PointOffset {};
    const ucolor* PPointColor {};
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
    friend class Sprite;

public:
    SpriteManager() = delete;
    SpriteManager(RenderSettings& settings, AppWindow* window, FileSystem& resources, GameTimer& game_time, EffectManager& effect_mngr, HashResolver& hash_resolver);
    SpriteManager(const SpriteManager&) = delete;
    SpriteManager(SpriteManager&&) noexcept = delete;
    auto operator=(const SpriteManager&) = delete;
    auto operator=(SpriteManager&&) noexcept = delete;
    ~SpriteManager();

    [[nodiscard]] auto ToHashedString(string_view str) -> hstring { FO_NON_CONST_METHOD_HINT_ONELINE() return _hashResolver.ToHashedString(str); }
    [[nodiscard]] auto GetResources() -> FileSystem& { FO_NON_CONST_METHOD_HINT_ONELINE() return _resources; }
    [[nodiscard]] auto GetRtMngr() -> RenderTargetManager& { return _rtMngr; }
    [[nodiscard]] auto GetAtlasMngr() -> TextureAtlasManager& { return _atlasMngr; }
    [[nodiscard]] auto GetTimer() const -> GameTimer& { return _gameTimer; }
    [[nodiscard]] auto GetWindow() -> AppWindow* { FO_NON_CONST_METHOD_HINT_ONELINE() return _window; }
    [[nodiscard]] auto GetWindowSize() const -> isize;
    [[nodiscard]] auto GetScreenSize() const -> isize;
    [[nodiscard]] auto IsFullscreen() const -> bool;
    [[nodiscard]] auto IsWindowFocused() const -> bool;
    [[nodiscard]] auto SpriteHitTest(const Sprite* spr, ipos32 pos, bool with_zoom) const -> bool;
    [[nodiscard]] auto IsEggTransp(ipos32 pos) const -> bool;
    [[nodiscard]] auto CheckEggAppearence(mpos hex, EggAppearenceType appearence) const -> bool;
    [[nodiscard]] auto LoadSprite(string_view path, AtlasType atlas_type, bool no_warn_if_not_exists = false) -> shared_ptr<Sprite>;
    [[nodiscard]] auto LoadSprite(hstring path, AtlasType atlas_type, bool no_warn_if_not_exists = false) -> shared_ptr<Sprite>;

    void SetWindowSize(isize size);
    void SetScreenSize(isize size);
    void ToggleFullscreen();
    void SetMousePosition(ipos32 pos);
    void MinimizeWindow();
    void BlinkWindow();
    void SetAlwaysOnTop(bool enable);

    void RegisterSpriteFactory(unique_ptr<SpriteFactory>&& factory);
    auto GetSpriteFactory(std::type_index ti) -> SpriteFactory*;
    void CleanupSpriteCache();

    void PushScissor(irect32 rect);
    void PopScissor();

    void PrepareSquare(vector<PrimitivePoint>& points, const IRect& r, ucolor color) const;
    void PrepareSquare(vector<PrimitivePoint>& points, fpos lt, fpos rt, fpos lb, fpos rb, ucolor color) const;

    void BeginScene();
    void EndScene();

    void SetSpritesZoom(float32 zoom);

    void DrawSprite(const Sprite* spr, ipos32 pos, ucolor color);
    void DrawSpriteSize(const Sprite* spr, ipos32 pos, isize size, bool fit, bool center, ucolor color);
    void DrawSpriteSizeExt(const Sprite* spr, fpos pos, fsize size, bool fit, bool center, bool stretch, ucolor color);
    void DrawSpritePattern(const Sprite* spr, ipos32 pos, isize size, isize spr_size, ucolor color);
    void DrawSprites(MapSpriteList& mspr_list, bool collect_contours, bool use_egg, DrawOrderType draw_oder_from, DrawOrderType draw_oder_to, ucolor color);
    void DrawPoints(const vector<PrimitivePoint>& points, RenderPrimitiveType prim, const float32* zoom = nullptr, const fpos* offset = nullptr, RenderEffect* custom_effect = nullptr);
    void DrawTexture(const RenderTexture* tex, bool alpha_blend, const IRect* region_from = nullptr, const IRect* region_to = nullptr, RenderEffect* custom_effect = nullptr);
    void DrawRenderTarget(const RenderTarget* rt, bool alpha_blend, const IRect* region_from = nullptr, const IRect* region_to = nullptr);
    void Flush();

    void DrawContours();
    void InitializeEgg(hstring egg_name, AtlasType atlas_type);
    void SetEgg(mpos hex, const MapSprite* mspr);
    void EggNotValid() { _eggValid = false; }

private:
    [[nodiscard]] auto ApplyColorBrightness(ucolor color) const -> ucolor;

    void RefreshScissor();
    void EnableScissor();
    void DisableScissor();

    void CollectContour(ipos32 pos, const Sprite* spr, ucolor contour_color);

    RenderSettings& _settings;
    AppWindow* _window;
    FileSystem& _resources;
    GameTimer& _gameTimer;
    RenderTargetManager _rtMngr;
    TextureAtlasManager _atlasMngr;
    EffectManager& _effectMngr;
    HashResolver& _hashResolver;

    vector<unique_ptr<SpriteFactory>> _spriteFactories {};
    unordered_map<string, SpriteFactory*> _spriteFactoryMap {};
    unordered_set<hstring> _nonFoundSprites {};
    unordered_map<pair<hstring, AtlasType>, shared_ptr<Sprite>> _copyableSpriteCache {};
    unordered_map<const Sprite*, weak_ptr<Sprite>> _updateSprites {};

    RenderTarget* _rtMain {};
    RenderTarget* _rtContours {};
    RenderTarget* _rtContoursMid {};

    vector<DipData> _dipQueue {};
    unique_ptr<RenderDrawBuffer> _spritesDrawBuf {};
    unique_ptr<RenderDrawBuffer> _primitiveDrawBuf {};
    unique_ptr<RenderDrawBuffer> _flushDrawBuf {};
    unique_ptr<RenderDrawBuffer> _contourDrawBuf {};
    size_t _flushVertCount {};

    vector<irect32> _scissorStack {};
    irect32 _scissorRect {};

    bool _contoursAdded {};
    bool _contourClearMid {};

    bool _eggValid {};
    mpos _eggHex {};
    ipos32 _eggOffset {};
    shared_ptr<AtlasSprite> _sprEgg {};
    vector<ucolor> _eggData {};

    float32 _spritesZoom {1.0f};

    ipos32 _windowSizeDiff {};

    EventUnsubscriber _eventUnsubscriber {};

    bool _nonConstHelper {};

    // Todo: move fonts stuff to separate module
public:
    [[nodiscard]] auto GetLinesCount(isize size, string_view str, int32 num_font) -> int32;
    [[nodiscard]] auto GetLinesHeight(isize size, string_view str, int32 num_font) -> int32;
    [[nodiscard]] auto GetLineHeight(int32 num_font) -> int32;
    [[nodiscard]] auto GetTextInfo(isize size, string_view str, int32 num_font, uint32 flags, isize& result_size, int32& lines) -> bool;
    [[nodiscard]] auto HaveLetter(int32 num_font, uint32 letter) -> bool;

    auto LoadFontFO(int32 index, string_view font_name, AtlasType atlas_type, bool not_bordered, bool skip_if_loaded) -> bool;
    auto LoadFontBmf(int32 index, string_view font_name, AtlasType atlas_type) -> bool;
    void SetDefaultFont(int32 index);
    void SetFontEffect(int32 index, RenderEffect* effect);
    void DrawText(irect32 rect, string_view str, uint32 flags, ucolor color, int32 num_font);
    auto SplitLines(irect32 rect, string_view cstr, int32 num_font) -> vector<string>;
    void ClearFonts();

private:
    static constexpr int32 FONT_BUF_LEN = 0x5000;
    static constexpr int32 FONT_MAX_LINES = 1000;
    static constexpr int32 FORMAT_TYPE_DRAW = 0;
    static constexpr int32 FORMAT_TYPE_SPLIT = 1;
    static constexpr int32 FORMAT_TYPE_LCOUNT = 2;

    struct FontData
    {
        struct Letter
        {
            ipos32 Pos {};
            isize Size {};
            ipos32 Offset {};
            int32 XAdvance {};
            FRect TexPos {};
            FRect TexBorderedPos {};
        };

        RenderEffect* DrawEffect {};
        RenderTexture* FontTex {};
        RenderTexture* FontTexBordered {};
        unordered_map<uint32, Letter> Letters {};
        int32 SpaceWidth {};
        int32 LineHeight {};
        int32 YAdvance {};
        shared_ptr<AtlasSprite> ImageNormal {};
        shared_ptr<AtlasSprite> ImageBordered {};
        bool MakeGray {};
    };

    // Todo: optimize text formatting - cache previous results
    struct FontFormatInfo
    {
        FontData* CurFont {};
        uint32 Flags {};
        irect32 Rect {};
        char Str[FONT_BUF_LEN] {};
        char* PStr {};
        int32 LinesAll {1};
        int32 LinesInRect {};
        int32 CurX {};
        int32 CurY {};
        int32 MaxCurX {};
        ucolor ColorDots[FONT_BUF_LEN] {};
        int32 LineWidth[FONT_MAX_LINES] {};
        int32 LineSpaceWidth[FONT_MAX_LINES] {};
        int32 OffsColDots {};
        ucolor DefColor {COLOR_TEXT};
        vector<string>* StrLines {};
        bool IsError {};
    };

    [[nodiscard]] auto GetFont(int32 num) -> FontData*;

    void BuildFont(int32 index);
    void FormatText(FontFormatInfo& fi, int32 fmt_type);

    vector<unique_ptr<FontData>> _allFonts {};
    int32 _defFontIndex {};
    FontFormatInfo _fontFormatInfoBuf {};
};

FO_END_NAMESPACE();
