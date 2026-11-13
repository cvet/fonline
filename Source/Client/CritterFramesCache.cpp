// FOnline Engine
// SPDX-License-Identifier: MIT

#include "CritterFramesCache.h"

#include "ModelAnimation.h"

FO_BEGIN_NAMESPACE

CritterFramesCache::CritterFramesCache(ptr<AnimationResolver> anim_name_resolver) :
    _animNameResolver {anim_name_resolver}
{
}

void CritterFramesCache::CleanupCritterFrames()
{
    _critterFrames.clear();
}

auto CritterFramesCache::GetCritterAnimFrames(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, mdir dir) -> nptr<const SpriteSheet>
{
    // Substitutions keep the original request as the cache key
    auto key {tuple {model_name, state_anim, action_anim}};

    if (auto it {_critterFrames.find(key)}; it != _critterFrames.end()) {
        auto anim {it->second.as_nptr()};

        if (anim) {
            auto dir_anim {anim->GetDirection(dir)};

            if (dir_anim) {
                return dir_anim;
            }

            return anim;
        }

        return nullptr;
    }

    CritterStateAnim base_state_anim {state_anim};
    CritterActionAnim base_action_anim {action_anim};
    shared_ptr<SpriteSheet> anim {};

    while (true) {
        anim = _animNameResolver->ResolveCritterAnimationSprite(model_name, state_anim, action_anim);

        hstring base_model_name {model_name};
        hstring model_name_ {model_name};
        CritterStateAnim state_anim_ {state_anim};
        CritterActionAnim action_anim_ {action_anim};

        if (!anim && _animNameResolver->ResolveCritterAnimationSubstitute(base_model_name, base_state_anim, base_action_anim, model_name, state_anim, action_anim)) {
            if (model_name_ != model_name || state_anim != state_anim_ || action_anim != action_anim_) {
                continue;
            }
        }

        break;
    }

    if (anim) {
        for (int32_t d {0}; d < anim->GetDirectionCount(); d++) {
            auto dir_anim {anim->GetDirection(hdir(d))};
            FO_VERIFY_AND_THROW(dir_anim, "Missing animation direction");
            dir_anim->_stateAnim = state_anim;
            dir_anim->_actionAnim = action_anim;
        }
    }

    auto it {_critterFrames.try_emplace(key, std::move(anim)).first};
    auto anim_loaded {it->second.as_nptr()};

    if (anim_loaded) {
        auto dir_anim {anim_loaded->GetDirection(dir)};

        if (dir_anim) {
            return dir_anim;
        }

        return anim_loaded;
    }

    return nullptr;
}

FO_END_NAMESPACE
