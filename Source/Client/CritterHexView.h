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

#pragma once

#include "Common.h"

#include "3dStuff.h"
#include "Application.h"
#include "CritterView.h"
#include "EntityProtos.h"
#include "SpriteManager.h"

class MapView;
class ItemView;

class CritterHexView final : public CritterView
{
public:
    CritterHexView() = delete;
    CritterHexView(MapView* map, ident_t id, const ProtoCritter* proto);
    CritterHexView(const CritterHexView&) = delete;
    CritterHexView(CritterHexView&&) noexcept = delete;
    auto operator=(const CritterHexView&) = delete;
    auto operator=(CritterHexView&&) noexcept = delete;
    ~CritterHexView() override = default;

    [[nodiscard]] auto GetMap() -> MapView* { return _map; }
    [[nodiscard]] auto GetMap() const -> const MapView* { return _map; }
    [[nodiscard]] auto IsHaveLightSources() const -> bool;
    [[nodiscard]] auto IsMoving() const -> bool { return !Moving.Steps.empty(); }
    [[nodiscard]] auto IsNeedReset() const -> bool;
    [[nodiscard]] auto GetAnim2() const -> uint;
    [[nodiscard]] auto IsAnimAvailable(uint anim1, uint anim2) const -> bool;
    [[nodiscard]] auto IsAnim() const -> bool { return !_animSequence.empty(); }
    [[nodiscard]] auto IsFinishing() const -> bool { return _finishingTime != 0; }
    [[nodiscard]] auto IsFinished() const -> bool;
    [[nodiscard]] auto GetViewRect() const -> IRect;
    [[nodiscard]] auto GetAttackDist() -> uint;
#if FO_ENABLE_3D
    [[nodiscard]] auto IsModel() const -> bool { return _model != nullptr; }
    [[nodiscard]] auto GetModel() -> ModelInstance* { NON_CONST_METHOD_HINT_ONELINE() return _model.get(); }
#endif

    void Init() override;
    void Finish() override;
    auto AddItem(ident_t id, const ProtoItem* proto, uint8 slot, const vector<vector<uint8>>& properties_data) -> ItemView* override;
    void DeleteItem(ItemView* item, bool animate) override;
    void ChangeDir(uint8 dir);
    void ChangeDirAngle(int dir_angle);
    void ChangeLookDirAngle(int dir_angle);
    void ChangeMoveDirAngle(int dir_angle);
    void Animate(uint anim1, uint anim2, Entity* context_item);
    void AnimateStay();
    void Action(int action, int action_ext, Entity* context_item, bool local_call);
    void Process();
    void ResetOk();
    void ClearAnim();
    void AddExtraOffs(int ext_ox, int ext_oy);
    void RefreshOffs();
    void SetText(string_view str, uint color, uint text_delay);
    void DrawTextOnHead();
    void GetNameTextPos(int& x, int& y) const;
    void GetNameTextInfo(bool& name_visible, int& x, int& y, int& w, int& h, int& lines) const;
    void ClearMove();
#if FO_ENABLE_3D
    void RefreshModel();
#endif

    uint SprId {};
    int SprOx {};
    int SprOy {};
    uint8 Alpha {};

    RenderEffect* DrawEffect {};
    bool Visible {true};
    Sprite* SprDraw {};
    bool SprDrawValid {};
    uint FadingTick {};

    struct
    {
        uint16 Speed {};
        vector<uint8> Steps {};
        vector<uint16> ControlSteps {};
        uint StartTick {};
        uint OffsetTick {};
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
        uint16 RealHexX {};
        uint16 RealHexY {};
    } Moving {};

private:
    struct CritterAnim
    {
        AnyFrames* Anim {};
        uint AnimTick {};
        uint BeginFrm {};
        uint EndFrm {};
        uint IndAnim1 {};
        uint IndAnim2 {};
        Entity* ContextItem {};
    };

#if FO_ENABLE_3D
    [[nodiscard]] auto GetModelLayersData() const -> const int*;
#endif
    [[nodiscard]] auto GetFadeAlpha() -> uint8;
    [[nodiscard]] auto GetCurAnim() -> CritterAnim*;

    void SetFade(bool fade_up);
    void ProcessMoving();
    void NextAnim(bool erase_front);
    void SetAnimSpr(const AnyFrames* anim, uint frm_index);

    MapView* _map;

    bool _needReset {};
    uint _resetTick {};

    uint _curFrmIndex {};
    uint _animStartTick {};
    CritterAnim _stayAnim {};
    vector<CritterAnim> _animSequence {};

    uint _finishingTime {};
    bool _fadingEnabled {};
    bool _fadeUp {};

    uint _tickFidget {};

    string _strTextOnHead {};
    uint _tickStartText {};
    uint _tickTextDelay {};
    uint _textOnHeadColor {COLOR_TEXT};

    int _oxAnim {};
    int _oyAnim {};
    float _oxExt {};
    float _oyExt {};
    float _oxExtSpeed {};
    float _oyExtSpeed {};
    uint _offsExtNextTick {};

#if FO_ENABLE_3D
    unique_del_ptr<ModelInstance> _model {};
#endif
};
