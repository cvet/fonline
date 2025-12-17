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

#include "ItemHexView.h"
#include "Client.h"
#include "EffectManager.h"
#include "MapSprite.h"
#include "MapView.h"

FO_BEGIN_NAMESPACE();

ItemHexView::ItemHexView(MapView* map, ident_t id, const ProtoItem* proto, const Properties* props) :
    ItemView(map->GetEngine(), id, proto, props),
    HexView(map)
{
    FO_STACK_TRACE_ENTRY();
}

void ItemHexView::Init()
{
    FO_STACK_TRACE_ENTRY();

    if (GetIsTile()) {
        SetDrawEffect(GetIsRoofTile() ? _engine->EffectMngr.Effects.Roof.get() : _engine->EffectMngr.Effects.Tile.get());
    }
    else {
        SetDrawEffect(_engine->EffectMngr.Effects.Generic.get());
    }

    RefreshAnim();
    RefreshAlpha();
}

void ItemHexView::SetupSprite(MapSprite* mspr)
{
    FO_STACK_TRACE_ENTRY();

    HexView::SetupSprite(mspr);

    mspr->SetColor(GetColorize() ? GetColorizeColor() : ucolor::clear);
    mspr->SetEggAppearence(GetEggType());

    if (GetBadItem()) {
        mspr->SetContour(ContourType::Red);
    }

    if (!GetNoLightInfluence()) {
        mspr->SetLight(GetCorner(), _map->GetLightData(), _map->GetSize());
    }

    if (!mspr->IsHidden() && GetHideSprite()) {
        mspr->SetHidden(true);
    }
}

void ItemHexView::Process()
{
    FO_STACK_TRACE_ENTRY();

    if (IsFading()) {
        ProcessFading();
    }

    if (_isMoving) {
        const auto time = _engine->GameTime.GetFrameTime();
        const auto dt = (time - _moveUpdateLastTime).to_ms<float32>();
        _moveUpdateLastTime = time;

        _moveCurOffset += _moveStepOffset * _moveSpeed * dt;
        RefreshOffs();

        const auto dist = (_moveCurOffset - _moveStartOffset).dist();

        if (dist >= _moveWholeDist) {
            _moveCurOffset = (_moveStartOffset + _moveStepOffset) * _moveWholeDist;
            _isMoving = false;
        }

        const auto proc = iround<int32>(dist / _moveWholeDist * 100.0f);
        const auto step_hex = _moveSteps[_moveSteps.size() * std::min(proc, 99) / 100];

        if (const auto hex = GetHex(); hex != step_hex) {
            const auto [x, y] = _engine->Geometry.GetHexOffset(hex, step_hex);

            _moveStartOffset.x -= numeric_cast<float32>(x);
            _moveStartOffset.y -= numeric_cast<float32>(y);
            _moveCurOffset.x -= numeric_cast<float32>(x);
            _moveCurOffset.y -= numeric_cast<float32>(y);
            RefreshOffs();

            _map->MoveItem(this, step_hex);
        }
    }
}

void ItemHexView::MoveToHex(mpos hex, float32 speed)
{
    FO_STACK_TRACE_ENTRY();

    const auto cur_hex = GetHex();

    if (cur_hex == hex) {
        _isMoving = false;
        return;
    }

    _isMoving = true;
    _moveSpeed = speed;

    _moveSteps.clear();
    _moveSteps.emplace_back(cur_hex);
    _map->TraceBullet(cur_hex, hex, 0, 0.0f, nullptr, CritterFindType::Any, nullptr, nullptr, &_moveSteps, false);

    auto pos_offset = _engine->Geometry.GetHexOffset(cur_hex, hex);
    pos_offset.y += GenericUtils::Random(5, 25); // Center of body

    _moveStepOffset = GenericUtils::GetStepsCoords({}, pos_offset);
    _moveWholeDist = fpos32(pos_offset).dist();

    _moveStartOffset = fpos32(_sprOffset);
    _moveCurOffset = _moveStartOffset;
    _moveDir = GeometryHelper::GetDir(cur_hex, hex);
    _moveUpdateLastTime = _engine->GameTime.GetFrameTime();
}

void ItemHexView::RefreshAlpha()
{
    FO_STACK_TRACE_ENTRY();

    SetDefaultAlpha(GetColorize() ? GetColorizeColor().comp.a : 0xFF);
}

void ItemHexView::PlayAnim(hstring anim_name, bool looped, bool reversed)
{
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

    _animStopped = true;

    if (_anim) {
        _anim->Stop();
    }
}

void ItemHexView::SetAnimTime(float32 normalized_time)
{
    FO_STACK_TRACE_ENTRY();

    _animTime = normalized_time;

    if (_anim) {
        _anim->SetTime(_animTime);
    }
}

void ItemHexView::SetAnimDir(uint8 dir)
{
    FO_STACK_TRACE_ENTRY();

    _animDir = dir;

    if (_anim) {
        _anim->SetDir(_animDir);
    }
}

void ItemHexView::RefreshAnim()
{
    FO_STACK_TRACE_ENTRY();

    const bool is_anim_init = !_anim;
    const auto pic_name = GetPicMap();

    if (pic_name) {
        _anim = _engine->SprMngr.LoadSprite(pic_name, AtlasType::MapSprites);
    }
    else {
        _anim = nullptr;
    }

    if (_anim) {
        if (is_anim_init) {
            _anim->PlayDefault();
        }
        else {
            _anim->SetTime(_animTime);
            _anim->SetDir(_animDir);

            if (!_animStopped && _animLooped) {
                _anim->Play(_animName, _animLooped, _animReversed);
            }
        }
    }
    else {
        _anim = _engine->ResMngr.GetItemDefaultSpr();
    }

    _spr = _anim.get();
    RefreshOffs();
}

auto ItemHexView::GetEggType() const noexcept -> EggAppearenceType
{
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

    const auto offset = GetOffset();

    _sprOffset = ipos32 {offset.x, offset.y};

    if (GetIsTile()) {
        if (GetIsRoofTile()) {
            _sprOffset.x += _engine->Settings.MapRoofOffsX;
            _sprOffset.y += _engine->Settings.MapRoofOffsY;
        }
        else {
            _sprOffset.x += _engine->Settings.MapTileOffsX;
            _sprOffset.y += _engine->Settings.MapTileOffsY;
        }
    }

    if (_isMoving) {
        _sprOffset.x += iround<int32>(_moveCurOffset.x);
        _sprOffset.y += iround<int32>(_moveCurOffset.y);
    }
}

void ItemHexView::SetMultihexEntries(vector<mpos> entries)
{
    FO_STACK_TRACE_ENTRY();

    if (!entries.empty()) {
        make_if_not_exists(_multihexEntries);
        *_multihexEntries = std::move(entries);
    }
    else {
        _multihexEntries.reset();
    }
}

FO_END_NAMESPACE();
