// FOnline Engine
// SPDX-License-Identifier: MIT

#include "ItemHexView.h"

#include "Client.h"
#include "EffectManager.h"
#include "MapSprite.h"
#include "MapView.h"

FO_BEGIN_NAMESPACE

// A stalled frame must not fling a flying item across the map or reverse it
constexpr float32_t FlyMoveMaxFrameDeltaMs {100.0f};

ItemHexView::ItemHexView(ptr<MapView> map, ident_t id, ptr<const ProtoItem> proto, nptr<const Properties> props) :
    ItemView(map->GetEngine(), id, proto, props),
    HexView(map)
{
}

void ItemHexView::OnDestroySelf()
{
    ItemView::OnDestroySelf();

    // Release atlas-backed sprites at logical destroy: a wrapper-retained entity must reach a memory-only final
    // destructor, because its dtor can run after the SpriteManager is gone
    _spr = nullptr;
    _anim = nullptr;
}

void ItemHexView::Init()
{
    if (GetIsTile()) {
        SetDrawEffect(GetIsRoofTile() ? _engine->EffectMngr.Effects.Roof : _engine->EffectMngr.Effects.Tile);
    }
    else if (GetDrawFlatten()) {
        SetDrawEffect(_engine->EffectMngr.Effects.Flat.get());
    }
    else {
        SetDrawEffect(_engine->EffectMngr.Effects.Generic);
    }

    RefreshAnim();
    RefreshAlpha();
}

void ItemHexView::SetupSprite(ptr<MapSprite> mspr)
{
    HexView::SetupSprite(mspr);

    mspr->SetElevation(GetIsTile() && GetIsRoofTile() ? numeric_cast<int16_t>(_engine->Settings->Geometry.MapRoofElevation) : GetElevation());
    mspr->SetColor(GetColorize() ? GetColorizeColor() : ucolor::clear);
    mspr->SetEggAppearence(GetEggType());
    mspr->SetEggStructure(GetIsWall() || (GetIsTile() && GetIsRoofTile()));

    if (!GetNoLightInfluence()) {
        _map->SetupSpriteLight(mspr, GetCorner());
    }

    if (!mspr->IsHidden() && GetHideSprite()) {
        mspr->SetHidden(true);
    }
}

auto ItemHexView::GetAnim() const -> ptr<const Sprite>
{
    FO_VERIFY_AND_THROW(_anim, "Item has no animation sprite");
    return _anim;
}

void ItemHexView::Process()
{
    if (IsFading()) {
        ProcessFading();
    }

    if (_isMoving) {
        nanotime time {_engine->GameTime.GetFrameTime()};

        if (_moveVisualOnly) {
            float32_t elapsed {(time - _moveStartTime).to_ms<float32_t>()};
            float32_t progress {_moveDurationMs > 0.0f ? std::min(elapsed / _moveDurationMs, 1.0f) : 1.0f};

            _moveCurOffset = _moveStepOffset * (_moveWholeDist * progress);
            RefreshOffs();

            if (progress >= 1.0f) {
                _isMoving = false;
                _moveVisualOnly = false;
            }
        }
        else {
            timespan elapsed {time - _moveUpdateLastTime};
            _moveUpdateLastTime = time;
            float32_t dt {std::min(elapsed.to_ms<float32_t>(), FlyMoveMaxFrameDeltaMs)};

            if (dt > 0.0f) {
                _moveCurOffset += _moveStepOffset * _moveSpeed * dt;
                RefreshOffs();

                float32_t dist {(_moveCurOffset - _moveStartOffset).dist()};

                if (dist >= _moveWholeDist) {
                    _moveCurOffset = _moveStartOffset + _moveStepOffset * _moveWholeDist;
                    _isMoving = false;
                }

                int32_t proc {_moveWholeDist > 0.0f ? iround<int32_t>(dist / _moveWholeDist * 100.0f) : 100};
                auto step_hex {_moveSteps[_moveSteps.size() * std::min(proc, 99) / 100]};

                if (auto hex {GetHex()}; hex != step_hex) {
                    const auto [x, y] {GeometryHelper::GetHexOffset(hex, step_hex)};

                    _moveStartOffset.x -= numeric_cast<float32_t>(x);
                    _moveStartOffset.y -= numeric_cast<float32_t>(y);
                    _moveCurOffset.x -= numeric_cast<float32_t>(x);
                    _moveCurOffset.y -= numeric_cast<float32_t>(y);
                    RefreshOffs();

                    _map->MoveItem(this, step_hex);
                }
            }
        }
    }
}

void ItemHexView::MoveToHex(mpos hex, float32_t speed)
{
    auto cur_hex {GetHex()};

    if (cur_hex == hex) {
        _isMoving = false;
        _moveVisualOnly = false;
        return;
    }

    _isMoving = true;
    _moveVisualOnly = false;
    _moveSpeed = speed;

    _moveSteps.clear();
    _moveSteps.emplace_back(cur_hex);
    _map->TraceBullet(cur_hex, hex, 0, 0.0f, nullptr, CritterFindType::Any, nullptr, nullptr, &_moveSteps, false);

    ipos32 pos_offset {GeometryHelper::GetHexOffset(cur_hex, hex)};
    pos_offset.y += _engine->Random(5, 25); // Center of body

    _moveStepOffset = GeometryHelper::GetStepsCoords({}, pos_offset);
    _moveWholeDist = fpos32(pos_offset).dist();

    _moveStartOffset = fpos32(_sprOffset);
    _moveCurOffset = _moveStartOffset;
    _moveDir = mdir(iround<int32_t>(GeometryHelper::GetDirAngle(cur_hex, hex)));
    _moveUpdateLastTime = _engine->GameTime.GetFrameTime();
}

void ItemHexView::MoveToHexVisual(mpos hex, float32_t speed)
{
    MoveToHexVisualImpl(hex, speed, 0);
}

void ItemHexView::MoveToHexVisualForDuration(mpos hex, int32_t duration_ms)
{
    FO_VERIFY_AND_THROW(duration_ms > 0, "Visual movement duration must be positive", duration_ms);
    MoveToHexVisualImpl(hex, 0.0f, duration_ms);
}

void ItemHexView::MoveToHexVisualImpl(mpos hex, float32_t speed, int32_t duration_ms)
{
    auto cur_hex {GetHex()};

    if (cur_hex == hex) {
        _isMoving = false;
        _moveVisualOnly = false;
        return;
    }

    ipos32 pos_offset {GeometryHelper::GetHexOffset(cur_hex, hex)};
    float32_t whole_dist {fpos32(pos_offset).dist()};

    if (whole_dist <= 0.0f) {
        _isMoving = false;
        _moveVisualOnly = false;
        return;
    }

    _isMoving = true;
    _moveVisualOnly = true;
    _moveSpeed = duration_ms > 0 ? whole_dist / numeric_cast<float32_t>(duration_ms) : speed;

    // A pure visual trajectory keeps the item on its hex and slides the sprite along a straight line
    _moveSteps.clear();
    _moveStepOffset = fpos32(pos_offset) / whole_dist;
    _moveWholeDist = whole_dist;
    _moveBaseOffset = GetOffset();
    _moveStartOffset = fpos32 {};
    _moveCurOffset = fpos32 {};
    _moveDurationMs = duration_ms > 0 ? numeric_cast<float32_t>(duration_ms) : (speed > 0.0f ? whole_dist / speed : whole_dist);
    _moveStartTime = _engine->GameTime.GetFrameTime();
    _moveDir = mdir(iround<int32_t>(GeometryHelper::GetDirAngle(cur_hex, hex)));
    _moveUpdateLastTime = _moveStartTime;
}

void ItemHexView::RefreshAlpha()
{
    SetDefaultAlpha(GetColorize() ? GetColorizeColor().comp.a : 0xFF);
}

void ItemHexView::PlayAnim(hstring anim_name, bool looped, bool reversed)
{
    _animName = anim_name;
    _animLooped = looped;
    _animReversed = reversed;
    _animStopped = false;
    _animTime = reversed ? 0.0f : 1.0f;

    if (_anim) {
        _anim->Play(_animName, _animLooped, _animReversed);
    }
}

void ItemHexView::StopAnim()
{
    _animStopped = true;

    if (_anim) {
        _anim->Stop();
    }
}

void ItemHexView::SetAnimTime(float32_t normalized_time)
{
    _animTime = normalized_time;

    if (_anim) {
        _anim->SetTime(_animTime);
    }
}

void ItemHexView::SetAnimDir(mdir dir)
{
    _animDir = dir;

    if (_anim) {
        _anim->SetDirection(_animDir);
    }
}

void ItemHexView::RefreshAnim()
{
    bool is_anim_init {!_anim};
    auto pic_name {GetPicMap()};

    if (pic_name) {
        _anim = _engine->ResMngr.LoadSprite(pic_name, AtlasType::MapSprites);
    }
    else {
        _anim = nullptr;
    }

    if (_anim) {
        if (_map->IsMapperMode()) {
            _anim->Stop();
            _anim->SetTime(0.0f);
        }
        else if (is_anim_init) {
            _anim->PlayDefault();
        }
        else {
            _anim->SetTime(_animTime);
            _anim->SetDirection(_animDir);

            if (!_animStopped && _animLooped) {
                _anim->Play(_animName, _animLooped, _animReversed);
            }
        }
    }
    else {
        _anim = _engine->ResMngr.GetItemDefaultSpr();
    }

    _spr = _anim;
    RefreshOffs();
}

auto ItemHexView::GetEggType() const noexcept -> EggAppearenceType
{
    if (GetDisableEgg() || GetDrawFlatten()) {
        return EggAppearenceType::None;
    }

    switch (GetCorner()) {
    case CornerType::South:
        return EggAppearenceType::ByXOrY;
    case CornerType::North:
        return EggAppearenceType::ByXAndY;
    case CornerType::EastWest:
    case CornerType::West:
        return EggAppearenceType::ByY;
    case CornerType::East:
    case CornerType::NorthSouth:
        return EggAppearenceType::ByX;
    }

    return EggAppearenceType::None;
}

void ItemHexView::RefreshOffs()
{
    auto offset {_isMoving && _moveVisualOnly ? _moveBaseOffset : GetOffset()};
    _sprOffset = ipos32 {offset.x, offset.y};
    _rootOffset = ipos32 {offset.x, offset.y};

    if (_isMoving) {
        _sprOffset.x += iround<int32_t>(_moveCurOffset.x);
        _sprOffset.y += iround<int32_t>(_moveCurOffset.y);
    }
}

void ItemHexView::SetMultihexEntries(vector<mpos> entries)
{
    if (!entries.empty()) {
        if (!_multihexEntries) {
            _multihexEntries.emplace();
        }

        *_multihexEntries = std::move(entries);
    }
    else {
        _multihexEntries.reset();
    }
}

FO_END_NAMESPACE
