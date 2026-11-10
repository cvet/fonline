// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "CritterView.h"
#include "DefaultSprites.h"
#include "EntityProtos.h"
#include "Geometry.h"
#include "HexView.h"
#include "Movement.h"
#include "SpriteManager.h"

FO_BEGIN_NAMESPACE

class ItemView;
#if FO_ENABLE_3D
class ModelInstance;
class ModelSprite;
#endif

class CritterHexView final : public CritterView, public HexView
{
public:
    CritterHexView() = delete;
    CritterHexView(ptr<MapView> map, ident_t id, ptr<const ProtoCritter> proto, nptr<const Properties> props = nullptr);
    CritterHexView(const CritterHexView&) = delete;
    CritterHexView(CritterHexView&&) noexcept = delete;
    auto operator=(const CritterHexView&) = delete;
    auto operator=(CritterHexView&&) noexcept = delete;
    ~CritterHexView() override = default;

    [[nodiscard]] auto IsMoving() const noexcept -> bool { return !!_moving; }
    [[nodiscard]] auto GetMoving() noexcept -> nptr<MovingContext> { return _moving; }
    [[nodiscard]] auto GetMoving() const noexcept -> nptr<const MovingContext> { return _moving; }
    // Which server plan this copy follows, and its extra path distance, independent of changes in speed
    [[nodiscard]] auto GetMovingServerUid() const noexcept -> uint32_t { return _movingServerUid; }
    [[nodiscard]] auto GetMovingServerDistanceShift() const noexcept -> float32_t { return _movingServerDistanceShift; }
    [[nodiscard]] auto IsAnimAvailable(CritterStateAnim state_anim, CritterActionAnim action_anim) -> bool;
    [[nodiscard]] auto IsAnimPlaying() const noexcept -> bool { return _curAnim.has_value(); }
    [[nodiscard]] auto GetViewRect() const -> irect32;
#if FO_ENABLE_3D
    [[nodiscard]] auto IsModel() const noexcept -> bool { return !!_model; }
    [[nodiscard]] auto GetModel() noexcept -> nptr<ModelInstance> { return _model; }
    [[nodiscard]] auto GetModel() const noexcept -> nptr<const ModelInstance> { return _model; }
#endif
    [[nodiscard]] auto GetNameTextPos(ipos32& pos) const -> bool;

    void Init();
    void ChangeDir(mdir dir);
    void ChangeLookDir(mdir dir);
    void ChangeMoveDir(mdir dir);
    void AppendAnim(CritterStateAnim state_anim, CritterActionAnim action_anim, nptr<Entity> context_item = nullptr);
    void StopAnim();
    void RefreshView(bool no_smooth = false);
    void Action(CritterAction action, int32_t action_data, nptr<Entity> context_item, bool local_call);
    void Process();
    void SynchronizeMoving();
    void NormalizeHexOffset();
    void AddExtraOffs(ipos32 offset);
    void RefreshOffs();
    void SetMoving(refcount_ptr<MovingContext> moving);
    // The current plan runs faster than its speed until it has made up the given time
    void SetMovingCatchUp(timespan time);
    void SetMovingServerPlan(uint32_t server_uid, float32_t distance_shift) noexcept;
    void StopMoving();
    void MoveAttachedCritters();
#if FO_ENABLE_3D
    void RefreshModel();
#endif

private:
    struct CritterAnim
    {
        CritterStateAnim StateAnim {};
        CritterActionAnim ActionAnim {};
        CritterAnimationFlags Flags {};
        float32_t NormalizedPosition {};
        float32_t Speed {1.0f};
        refcount_nptr<Entity> ContextItem {};
        nptr<const SpriteSheet> Frames {};
        int32_t FrameIndex {};
        timespan FramesDuration {};
    };

#if FO_ENABLE_3D
    auto GetModelLayersData() const -> ptr<const int32_t>;
#endif
    void OnDestroySelf() override;
    void SetupSprite(ptr<MapSprite> mspr) override;
    void ProcessMoving();
    void NextAnim();
    void PrepareAnim(CritterAnim& anim, CritterAnimationContext context, int32_t moving_speed);
    void UpdateAnimFrame(CritterAnim& anim);
    void SetAnimSpr(ptr<const SpriteSheet> anim, int32_t frm_index);

    void RefreshAnimOffset(CritterAnim& anim);

    // Integer-pixel displacement from the current MovingContext start to the linear position.
    // Hex snaps preserve it by shifting cur_hex and hex_offset in opposite directions
    auto EvaluateMovementDisplacement() const -> ipos32;

    refcount_nptr<MovingContext> _moving {};
    timespan _movingCatchUpLeft {};
    nanotime _movingCatchUpTime {};
    uint32_t _movingServerUid {};
    float32_t _movingServerDistanceShift {};
    // A plan waiting at its lease is drawn standing, not walking in place
    bool _movingHeld {};

    bool _needReset {};
    nanotime _resetTime {};

    nanotime _animStartTime {};
    CritterAnim _idle2dAnim {};
    vector<CritterAnim> _animSequence {};
    optional<CritterAnim> _curAnim {};

    ipos32 _offsAnim {};
    fpos32 _offsExt {};
    fpos32 _offsExtSpeed {};
    nanotime _offsExtNextTime {};

    // Align visual frames with distance along the path; sprite draw anchors stay separate.
    // Direction changes preserve phase; SetMoving and StopMoving reset it
    nptr<const SpriteSheet> _walkAnchorAnim {};
    ipos32 _walkAnchorDisp {};

#if FO_ENABLE_3D
    shared_ptr<ModelSprite> _modelSpr {};
    nptr<ModelInstance> _model {};
#endif
};

FO_END_NAMESPACE
