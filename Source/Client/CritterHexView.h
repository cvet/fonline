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
// Copyright (c) 2006 - 2024, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "CritterView.h"
#include "DefaultSprites.h"
#include "EntityProtos.h"
#include "HexView.h"
#include "ModelSprites.h"
#include "SpriteManager.h"

class ItemView;

class CritterHexView final : public CritterView, public HexView
{
public:
    CritterHexView() = delete;
    CritterHexView(MapView* map, ident_t id, const ProtoCritter* proto, const Properties* props = nullptr);
    CritterHexView(const CritterHexView&) = delete;
    CritterHexView(CritterHexView&&) noexcept = delete;
    auto operator=(const CritterHexView&) = delete;
    auto operator=(CritterHexView&&) noexcept = delete;
    ~CritterHexView() override = default;

    [[nodiscard]] auto IsMoving() const noexcept -> bool { return !Moving.Steps.empty(); }
    [[nodiscard]] auto IsNeedReset() const noexcept -> bool;
    [[nodiscard]] auto GetActionAnim() const noexcept -> CritterActionAnim;
    [[nodiscard]] auto IsAnimAvailable(CritterStateAnim state_anim, CritterActionAnim action_anim) const -> bool;
    [[nodiscard]] auto IsAnim() const noexcept -> bool { return !_animSequence.empty(); }
    [[nodiscard]] auto GetViewRect() const -> IRect;
    [[nodiscard]] auto IsNameVisible() const noexcept -> bool;
#if FO_ENABLE_3D
    [[nodiscard]] auto IsModel() const noexcept -> bool { return _model != nullptr; }
    [[nodiscard]] auto GetModel() noexcept -> ModelInstance* { NON_CONST_METHOD_HINT_ONELINE() return _model; }
#endif

    void Init();
    auto AddInvItem(ident_t id, const ProtoItem* proto, CritterItemSlot slot, const Properties* props) -> ItemView* override;
    auto AddInvItem(ident_t id, const ProtoItem* proto, CritterItemSlot slot, const vector<vector<uint8>>& props_data) -> ItemView* override;
    void DeleteInvItem(ItemView* item, bool animate) override;
    void ChangeDir(uint8 dir);
    void ChangeDirAngle(int dir_angle);
    void ChangeLookDirAngle(int dir_angle);
    void ChangeMoveDirAngle(int dir_angle);
    void Animate(CritterStateAnim state_anim, CritterActionAnim action_anim, Entity* context_item);
    void AnimateStay();
    void Action(CritterAction action, int action_data, Entity* context_item, bool local_call);
    void Process();
    void ResetOk();
    void ClearAnim();
    void AddExtraOffs(int ext_ox, int ext_oy);
    void RefreshOffs();
    void DrawName();
    auto GetNameTextPos(int& x, int& y) const -> bool;
    void ClearMove();
    void MoveAttachedCritters();
#if FO_ENABLE_3D
    void RefreshModel();
#endif

    struct
    {
        uint16 Speed {};
        vector<uint8> Steps {};
        vector<uint16> ControlSteps {};
        time_point StartTime {};
        time_duration OffsetTime {};
        uint16 StartHexX {};
        uint16 StartHexY {};
        uint16 EndHexX {};
        uint16 EndHexY {};
        float WholeTime {};
        float WholeDist {};
        int16 StartOx {};
        int16 StartOy {};
        int16 EndOx {};
        int16 EndOy {};
    } Moving {};

private:
    struct CritterAnim
    {
        const SpriteSheet* AnimFrames {};
        time_duration AnimDuration {};
        uint BeginFrm {};
        uint EndFrm {};
        CritterStateAnim StateAnim {};
        CritterActionAnim ActionAnim {};
        Entity* ContextItem {};
    };

#if FO_ENABLE_3D
    [[nodiscard]] auto GetModelLayersData() const -> const int*;
#endif
    [[nodiscard]] auto GetCurAnim() -> CritterAnim*;

    void OnDestroySelf() override;
    void SetupSprite(MapSprite* mspr) override;
    void ProcessMoving();
    void NextAnim(bool erase_front);
    void SetAnimSpr(const SpriteSheet* anim, uint frm_index);

    bool _needReset {};
    time_point _resetTime {};

    uint _curFrmIndex {};
    time_point _animStartTime {};
    CritterAnim _stayAnim {};
    vector<CritterAnim> _animSequence {};

    time_point _fidgetTime {};

    int _oxAnim {};
    int _oyAnim {};
    float _oxExt {};
    float _oyExt {};
    float _oxExtSpeed {};
    float _oyExtSpeed {};
    time_point _offsExtNextTime {};

#if FO_ENABLE_3D
    shared_ptr<ModelSprite> _modelSpr {};
    ModelInstance* _model {};
#endif
};
