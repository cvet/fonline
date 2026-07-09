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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "3dStuff.h"
#include "EffectManager.h"
#include "FileSystem.h"
#include "Geometry.h"
#include "MapSprite.h"
#include "RenderTarget.h"
#include "Settings.h"
#include "TextureAtlas.h"
#include "VisualParticles.h"

FO_BEGIN_NAMESPACE

namespace Color
{
    static constexpr auto Neutral = ucolor {128, 128, 128};
    static constexpr auto TextWhite = ucolor {255, 255, 255};
}

class IAppWindow;
class IAppRender;
class IAppInput;
class SpriteManager;
class AtlasSprite;

///@ ExportEnum
enum class TransparentEggSlot : uint8_t
{
    Primary = 0,
    Secondary = 1,
};

class Sprite : public enable_shared_from_this<Sprite>
{
public:
    explicit Sprite(ptr<SpriteManager> spr_mngr, isize32 size, ipos32 offset);
    Sprite(const Sprite&) = delete;
    Sprite(Sprite&&) noexcept = default;
    auto operator=(const Sprite&) = delete;
    auto operator=(Sprite&&) noexcept -> Sprite& = delete;
    virtual ~Sprite() = default;

    [[nodiscard]] auto GetSize() const noexcept -> isize32 { return _size; }
    [[nodiscard]] auto GetOffset() const noexcept -> ipos32 { return _offset; }
    [[nodiscard]] auto GetDrawEffectOr(ptr<RenderEffect> defaultEffect) const noexcept -> ptr<RenderEffect>
    {
        FO_NO_STACK_TRACE_ENTRY();

        if (_drawEffect) {
            return _drawEffect.as_ptr();
        }

        return defaultEffect;
    }
    [[nodiscard]] virtual auto IsHitTest(ipos32 pos) const -> bool;
    [[nodiscard]] virtual auto GetBatchTexture() const -> nptr<const RenderTexture> { return nullptr; }
    [[nodiscard]] virtual auto GetViewSize() const -> optional<irect32> { return std::nullopt; }
    [[nodiscard]] virtual auto IsDirectDraw() const -> bool { return false; }
    [[nodiscard]] virtual auto IsCopyable() const -> bool { return false; }
    [[nodiscard]] virtual auto MakeCopy() const -> shared_ptr<Sprite> { throw InvalidCallException(FO_LINE_STR); }
    [[nodiscard]] virtual auto IsPlaying() const -> bool { return false; }
    [[nodiscard]] virtual auto GetTime() const -> float32_t { return 0.0f; }

    void SetOffset(ipos32 offset) noexcept { _offset = offset; }
    void SetDrawEffect(nptr<RenderEffect> effect) const noexcept { _drawEffect = effect; }
    virtual auto FillData(ptr<RenderDrawBuffer> dbuf, const frect32& pos, const tuple<ucolor, ucolor>& colors) const -> size_t = 0;
    virtual void Prewarm() { }
    virtual void SetTime(float32_t normalized_time) { ignore_unused(normalized_time); }
    virtual void SetDir(mdir dir) { ignore_unused(dir); }
    virtual void PlayDefault() { Play({}, true, false); }
    virtual void Play(hstring anim_name, bool looped, bool reversed) { ignore_unused(anim_name, looped, reversed); }
    virtual void Stop() { }
    virtual auto Update() -> bool { return false; }
    virtual void DrawInScene(fpos32 scene_pos, float32_t depth) const { ignore_unused(scene_pos, depth); }

protected:
    void StartUpdate();

    ptr<SpriteManager> _sprMngr;
    isize32 _size;
    ipos32 _offset;
    mutable nptr<RenderEffect> _drawEffect {};
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
    nptr<const ipos32> PointOffset {};
    nptr<const ucolor> PPointColor {};
    fpos32 TexUV {}; // Custom data for shader
    fpos32 EggData {}; // Custom data for shader
    float32_t PointPosZ {}; // Free-form per-vertex scalar forwarded to Vertex2D::PosZ → shader InPosition.z
};
static_assert(std::is_standard_layout_v<PrimitivePoint>);

struct DipData
{
    nptr<const RenderTexture> MainTexture {};
    nptr<RenderEffect> SourceEffect {};
    size_t IndicesCount {};
};

// A direct-to-scene sprite (e.g. particle system) deferred to a single pass after the sprite batch, so its
// own shader does not split the batch. Occlusion stays correct via the shared scene depth buffer.
struct DirectDrawSprite
{
    nptr<const Sprite> Spr {};
    fpos32 ScenePos {};
    float32_t Depth {};
};

class SpriteManager final
{
    friend class FontManager;
    friend class Sprite;

public:
    static constexpr size_t EGG_SLOT_COUNT = 2;

    SpriteManager() = delete;
    SpriteManager(ptr<RenderSettings> settings, ptr<IAppWindow> window, ptr<FileSystem> resources, ptr<GameTimer> game_time, ptr<EffectManager> effect_mngr, ptr<HashResolver> hash_resolver);
    SpriteManager(const SpriteManager&) = delete;
    SpriteManager(SpriteManager&&) noexcept = delete;
    auto operator=(const SpriteManager&) = delete;
    auto operator=(SpriteManager&&) noexcept = delete;
    ~SpriteManager() = default;

    [[nodiscard]] auto ToHashedString(string_view str) -> hstring { return _hashResolver->ToHashedString(str); }
    [[nodiscard]] auto GetResources() noexcept -> ptr<FileSystem> { return _resources; }
    [[nodiscard]] auto GetRtMngr() const noexcept -> const RenderTargetManager& { return _rtMngr; }
    [[nodiscard]] auto GetRtMngr() noexcept -> RenderTargetManager& { return _rtMngr; }
    [[nodiscard]] auto GetMainRenderTarget() noexcept -> nptr<RenderTarget> { return _rtMain; }
    [[nodiscard]] auto GetMainRenderTarget() const noexcept -> nptr<const RenderTarget> { return _rtMain; }
    [[nodiscard]] auto GetAtlasMngr() noexcept -> ptr<TextureAtlasManager> { return &_atlasMngr; }
    [[nodiscard]] auto GetTimer() const noexcept -> const GameTimer& { return *_gameTimer; }
    [[nodiscard]] auto GetWindow() noexcept -> ptr<IAppWindow> { return _window; }
    [[nodiscard]] auto GetRender() noexcept -> IAppRender& { return *_render; }
    [[nodiscard]] auto GetRender() const noexcept -> const IAppRender& { return *_render; }
    [[nodiscard]] auto GetInput() noexcept -> ptr<IAppInput> { return _input; }
    [[nodiscard]] auto GetWindowSize() const -> isize32;
    [[nodiscard]] auto GetScreenSize() const -> isize32;
    [[nodiscard]] auto IsFullscreen() const -> bool;
    [[nodiscard]] auto IsWindowFocused() const -> bool;
    [[nodiscard]] auto Random(int32_t min_value, int32_t max_value) -> int32_t;
    [[nodiscard]] auto CheckHitTest(int32_t value) const -> bool { return value > _settings->SpriteHitValue; }
    [[nodiscard]] auto SpriteHitTest(ptr<const Sprite> spr, ipos32 pos) const -> bool;
    [[nodiscard]] auto IsEggTransp(ipos32 pos, mpos hex, EggAppearenceType appearence) const -> bool;
    [[nodiscard]] auto LoadSprite(string_view path, AtlasType atlas_type, bool no_warn_if_not_exists = false) -> shared_ptr<Sprite>;
    [[nodiscard]] auto LoadSprite(hstring path, AtlasType atlas_type, bool no_warn_if_not_exists = false) -> shared_ptr<Sprite>;

    void SetWindowSize(isize32 size);
    void SetScreenSize(isize32 size);
    void ToggleFullscreen();
    void SetMousePosition(ipos32 pos);
    void MinimizeWindow();
    void BlinkWindow();
    void SetAlwaysOnTop(bool enable);

    void RegisterSpriteFactory(unique_ptr<SpriteFactory> factory);
    auto GetSpriteFactory(std::type_index ti) -> nptr<SpriteFactory>;
    void CleanupSpriteCache();

    void PushScissor(irect32 rect);
    void PopScissor();

    void PrepareSquare(vector<PrimitivePoint>& points, irect32 r, ucolor color) const;
    void PrepareSquare(vector<PrimitivePoint>& points, fpos32 lt, fpos32 rt, fpos32 lb, fpos32 rb, ucolor color) const;

    void BeginScene();
    void EndScene();

    void DrawSprite(ptr<const Sprite> spr, ipos32 pos, ucolor color);
    void DrawSpriteSize(ptr<const Sprite> spr, ipos32 pos, isize32 size, bool fit, bool center, ucolor color);
    void DrawSpriteSizeExt(ptr<const Sprite> spr, fpos32 pos, fsize32 size, bool fit, bool center, bool stretch, ucolor color);
    auto DrawSpriteRegion(ptr<const Sprite> spr, fpos32 uv0, fpos32 uv1, fpos32 pos, fsize32 size, ucolor color) -> bool;
    void DrawSpritePattern(ptr<const Sprite> spr, ipos32 pos, isize32 size, isize32 spr_size, ucolor color);
    void DrawSprites(MapSpriteList& mspr_list, irect32 draw_area, bool use_egg, DrawOrderType draw_oder_from, DrawOrderType draw_oder_to, ucolor color, ptr<RenderEffect> default_effect);
    void DrawSpriteWithEffect(ptr<const Sprite> spr, ipos32 pos, ucolor color, ptr<RenderEffect> effect, int32_t padding);
    void DrawPoints(const_span<PrimitivePoint> points, RenderPrimitiveType prim, nptr<const irect32> draw_area = nullptr, nptr<RenderEffect> custom_effect = nullptr);
    void DrawTexture(ptr<const RenderTexture> tex, bool alpha_blend, nptr<const frect32> region_from = nullptr, nptr<const irect32> region_to = nullptr, nptr<RenderEffect> custom_effect = nullptr);
    void DrawRenderTarget(ptr<RenderTarget> rt, bool alpha_blend, nptr<const frect32> region_from = nullptr, nptr<const irect32> region_to = nullptr);
    void Flush();

    void SetEgg(TransparentEggSlot slot, mpos hex, nptr<const MapSprite> mspr);
    void SetEgg(TransparentEggSlot slot, mpos hex, fpos32 center, fsize32 radius);
    void InvalidateEgg(TransparentEggSlot slot);
    void InvalidateEgg();

private:
    struct EggSlot
    {
        bool Valid {};
        mpos Hex {};
        fpos32 Center {};
        fsize32 Radius {};
        fpos32 DrawOffset {};
    };

    [[nodiscard]] auto ApplyColorBrightness(ucolor color) const -> ucolor;
    [[nodiscard]] auto CheckEggAppearence(TransparentEggSlot slot, mpos hex, EggAppearenceType appearence) const -> bool;
    [[nodiscard]] auto MakeAspectFitRect(isize32 source_size, isize32 target_size) const -> irect32;

    void RefreshScissor();
    void EnableScissor();
    void DisableScissor();

    ptr<RenderSettings> _settings;
    ptr<IAppWindow> _window;
    ptr<FileSystem> _resources;
    ptr<GameTimer> _gameTimer;
    RenderTargetManager _rtMngr;
    TextureAtlasManager _atlasMngr;
    ptr<IAppRender> _render;
    ptr<IAppInput> _input;
    ptr<EffectManager> _effectMngr;
    ptr<HashResolver> _hashResolver;
    std::mt19937 _randomGenerator {MakeSeededRandomGenerator()};

    vector<unique_ptr<SpriteFactory>> _spriteFactories {};
    unordered_map<string, ptr<SpriteFactory>> _spriteFactoryMap {};
    unordered_set<hstring> _nonFoundSprites {};
    unordered_map<pair<hstring, AtlasType>, shared_ptr<Sprite>> _copyableSpriteCache {};
    unordered_map<ptr<const Sprite>, weak_ptr<Sprite>> _updateSprites {};

    nptr<RenderTarget> _rtMain {};

    vector<DipData> _dipQueue {};
    vector<DirectDrawSprite> _directDrawSprites {};
    unique_ptr<RenderDrawBuffer> _spritesDrawBuf;
    unique_ptr<RenderDrawBuffer> _primitiveDrawBuf;
    unique_ptr<RenderDrawBuffer> _flushDrawBuf;
    unique_ptr<RenderDrawBuffer> _spriteEffectDrawBuf;
    size_t _flushVertCount {};

    vector<irect32> _scissorStack {};
    irect32 _scissorRect {};

    EggSlot _eggSlots[EGG_SLOT_COUNT] {};

    ipos32 _windowSizeDiff {};
    optional<isize32> _pendingWindowedSize {};

    EventUnsubscriber _eventUnsubscriber {};
};

FO_END_NAMESPACE
