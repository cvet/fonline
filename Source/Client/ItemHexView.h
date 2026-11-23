// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "Geometry.h"
#include "HexView.h"
#include "ItemView.h"
#include "SpriteManager.h"

FO_BEGIN_NAMESPACE

class ItemHexView final : public ItemView, public HexView
{
public:
    ItemHexView() = delete;
    ItemHexView(ptr<MapView> map, ident_t id, ptr<const ProtoItem> proto, nptr<const Properties> props = nullptr);
    ItemHexView(const ItemHexView&) = delete;
    ItemHexView(ItemHexView&&) noexcept = delete;
    auto operator=(const ItemHexView&) = delete;
    auto operator=(ItemHexView&&) noexcept = delete;
    ~ItemHexView() override = default;

    [[nodiscard]] auto GetEggType() const noexcept -> EggAppearenceType;
    [[nodiscard]] auto IsNeedProcess() const -> bool { return _isMoving || IsFading(); }
    [[nodiscard]] auto GetAnim() const -> ptr<const Sprite>;
    [[nodiscard]] auto IsMoving() const noexcept -> bool { return _isMoving; }
    [[nodiscard]] auto HasMultihexEntries() const noexcept -> bool { return !!_multihexEntries; }
    [[nodiscard]] auto GetMultihexEntries() const noexcept -> nptr<const vector<mpos>> { return _multihexEntries ? nptr<const vector<mpos>> {&*_multihexEntries} : nullptr; }

    void Init();
    void Process();
    void RefreshAlpha();
    void PlayAnim(hstring anim_name, bool looped, bool reversed);
    void StopAnim();
    void SetAnimPhase(float32_t normalized_phase);
    void SetAnimDir(mdir dir);
    void RefreshAnim();
    void MoveToHex(mpos hex, float32_t speed);
    void RefreshOffs();
    void SetMultihexEntries(vector<mpos> entries);

private:
    void OnDestroySelf() override;
    void SetupSprite(ptr<MapSprite> mspr) override;

    shared_ptr<Sprite> _anim {};
    hstring _animName {};
    bool _animLooped {};
    bool _animReversed {};
    bool _animStopped {};
    float32_t _animPhase {};
    mdir _animDir {};

    bool _isMoving {};
    float32_t _moveSpeed {};
    fpos32 _moveStepOffset {};
    fpos32 _moveStartOffset {};
    fpos32 _moveCurOffset {};
    float32_t _moveWholeDist {};
    nanotime _moveUpdateLastTime {};
    mdir _moveDir {};
    vector<mpos> _moveSteps {};

    optional<vector<mpos>> _multihexEntries {};
};

FO_END_NAMESPACE
