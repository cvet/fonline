// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "DefaultSprites.h"

FO_BEGIN_NAMESPACE

class AnimationResolver;

class CritterFramesCache final
{
public:
    explicit CritterFramesCache(ptr<AnimationResolver> anim_name_resolver);
    CritterFramesCache(const CritterFramesCache&) = delete;
    CritterFramesCache(CritterFramesCache&&) noexcept = delete;
    auto operator=(const CritterFramesCache&) = delete;
    auto operator=(CritterFramesCache&&) noexcept = delete;
    ~CritterFramesCache() = default;

    auto GetCritterAnimFrames(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, mdir dir) -> nptr<const SpriteSheet>;
    void CleanupCritterFrames();

private:
    ptr<AnimationResolver> _animNameResolver;
    unordered_map<tuple<hstring, CritterStateAnim, CritterActionAnim>, shared_ptr<SpriteSheet>> _critterFrames {};
};

FO_END_NAMESPACE
