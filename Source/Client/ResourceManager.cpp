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

#include "ResourceManager.h"
#include "3dStuff.h"
#include "DataSource.h"
#include "FileSystem.h"

FO_BEGIN_NAMESPACE();

static constexpr uint32 ANIM_FLAG_FIRST_FRAME = 0x01;
static constexpr uint32 ANIM_FLAG_LAST_FRAME = 0x02;

ResourceManager::ResourceManager(FileSystem& resources, SpriteManager& spr_mngr, AnimationResolver& anim_name_resolver) :
    _resources {&resources},
    _sprMngr {&spr_mngr},
    _animNameResolver {&anim_name_resolver}
{
    FO_STACK_TRACE_ENTRY();
}

void ResourceManager::IndexFiles()
{
    FO_STACK_TRACE_ENTRY();

    for (const auto* sound_ext : {"wav", "acm", "ogg"}) {
        const auto sound_files = _resources->FilterFiles(sound_ext);

        for (const auto& file_header : sound_files) {
            _soundNames.emplace(strex(file_header.GetPath()).erase_file_extension().lower(), file_header.GetPath());
        }
    }

    auto any_spr = _sprMngr->LoadSprite("CritterStub.png", AtlasType::MapSprites);
    auto atlas_spr = dynamic_ptr_cast<AtlasSprite>(std::move(any_spr));
    FO_RUNTIME_ASSERT(atlas_spr);
    _critterDummyAnimFrames = SafeAlloc::MakeShared<SpriteSheet>(*_sprMngr, 1, 100, 1);
    _critterDummyAnimFrames->_spr[0] = std::move(atlas_spr);
    FO_RUNTIME_ASSERT(_critterDummyAnimFrames);

    _itemHexDummyAnim = _sprMngr->LoadSprite("ItemStub.png", AtlasType::MapSprites);
    FO_RUNTIME_ASSERT(_itemHexDummyAnim);
}

auto ResourceManager::GetItemDefaultSpr() -> shared_ptr<Sprite>
{
    FO_STACK_TRACE_ENTRY();

    return _itemHexDummyAnim;
}

auto ResourceManager::GetCritterDummyFrames() -> const SpriteSheet*
{
    FO_STACK_TRACE_ENTRY();

    return _critterDummyAnimFrames.get();
}

void ResourceManager::CleanupCritterFrames()
{
    FO_STACK_TRACE_ENTRY();

    _critterFrames.clear();
}

static auto AnimMapId(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim) -> uint32
{
    FO_STACK_TRACE_ENTRY();

    const uint32 dw[4] = {model_name.as_uint32(), static_cast<uint32>(state_anim), static_cast<uint32>(action_anim), 1};
    return Hashing::MurmurHash2(dw, sizeof(dw));
}

static auto FalloutAnimMapId(hstring model_name, uint32 state_anim, uint32 action_anim) -> uint32
{
    FO_STACK_TRACE_ENTRY();

    const uint32 dw[4] = {model_name.as_uint32(), numeric_cast<uint32>(state_anim), numeric_cast<uint32>(action_anim), numeric_cast<uint32>(0xFFFFFFFF)};
    return Hashing::MurmurHash2(dw, sizeof(dw));
}

auto ResourceManager::GetCritterAnimFrames(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, uint8 dir) -> const SpriteSheet*
{
    FO_STACK_TRACE_ENTRY();

    // Check already loaded
    const auto id = AnimMapId(model_name, state_anim, action_anim);

    if (const auto it = _critterFrames.find(id); it != _critterFrames.end()) {
        auto* anim_ = it->second.get();

        if (anim_ != nullptr) {
            if (anim_->GetDir(dir) != nullptr) {
                return anim_->GetDir(dir);
            }

            return anim_;
        }

        return nullptr;
    }

    // Process loading
    const auto base_state_anim = state_anim;
    const auto base_action_anim = action_anim;
    shared_ptr<SpriteSheet> anim;

    while (true) {
        // Load
        if (!!model_name && strex(model_name).starts_with("art/critters/")) {
            // Hardcoded
            anim = LoadFalloutAnimFrames(model_name, state_anim, action_anim);
        }
        else {
            // Script specific
            int32 pass_base = 0;

            while (true) {
                int32 pass = pass_base;
                uint32 flags = 0;
                int32 ox = 0;
                int32 oy = 0;
                string anim_name;

                if (_animNameResolver->ResolveCritterAnimationFrames(model_name, state_anim, action_anim, pass, flags, ox, oy, anim_name)) {
                    if (!anim_name.empty()) {
                        anim = dynamic_ptr_cast<SpriteSheet>(_sprMngr->LoadSprite(anim_name, AtlasType::MapSprites, true));

                        // Fix by dirs
                        for (int32 d = 0; anim && d < anim->_dirCount; d++) {
                            auto* dir_anim = anim->GetDir(d);

                            // Process flags
                            if (flags != 0) {
                                if (IsBitSet(flags, ANIM_FLAG_FIRST_FRAME) || IsBitSet(flags, ANIM_FLAG_LAST_FRAME)) {
                                    const auto first = IsBitSet(flags, ANIM_FLAG_FIRST_FRAME);

                                    // Append offsets
                                    if (!first) {
                                        for (int32 i = 0; i < dir_anim->GetFramesCount() - 1; i++) {
                                            dir_anim->_sprOffset[dir_anim->GetFramesCount() - 1].x += dir_anim->_sprOffset[i].x;
                                            dir_anim->_sprOffset[dir_anim->GetFramesCount() - 1].y += dir_anim->_sprOffset[i].y;
                                        }
                                    }

                                    // Change size
                                    dir_anim->_spr[0] = first ? dir_anim->GetSpr(0)->MakeCopy() : dir_anim->GetSpr(dir_anim->GetFramesCount() - 1)->MakeCopy();
                                    dir_anim->_sprOffset[0].x = first ? dir_anim->_sprOffset[0].x : dir_anim->_sprOffset[dir_anim->GetFramesCount() - 1].x;
                                    dir_anim->_sprOffset[0].y = first ? dir_anim->_sprOffset[0].y : dir_anim->_sprOffset[dir_anim->GetFramesCount() - 1].y;
                                    dir_anim->_framesCount = 1;
                                }
                            }

                            // Add offsets
                            ox = oy = 0; // Todo: why I disable offset adding?

                            if (ox != 0 || oy != 0) {
                                for (int32 i = 0; i < dir_anim->GetFramesCount(); i++) {
                                    auto* spr = dir_anim->GetSpr(i);
                                    bool fixed = false;

                                    for (int32 j = 0; j < i; j++) {
                                        if (dir_anim->GetSpr(j) == spr) {
                                            fixed = true;
                                            break;
                                        }
                                    }

                                    if (!fixed) {
                                        spr->SetOffset(spr->GetOffset() + ipos32(ox, oy));
                                    }
                                }
                            }
                        }
                    }

                    // If pass changed and animation not loaded than try again
                    if (!anim && pass != pass_base) {
                        pass_base = pass;
                        continue;
                    }

                    // Done
                    break;
                }
            }
        }

        // Find substitute animation
        const auto base_model_name = model_name;
        const auto model_name_ = model_name;
        const auto state_anim_ = state_anim;
        const auto action_anim_ = action_anim;

        if (anim == nullptr && _animNameResolver->ResolveCritterAnimationSubstitute(base_model_name, base_state_anim, base_action_anim, model_name, state_anim, action_anim)) {
            if (model_name_ != model_name || state_anim != state_anim_ || action_anim != action_anim_) {
                continue;
            }
        }

        // Exit from loop
        break;
    }

    // Store resulted animation indices
    if (anim != nullptr) {
        for (int32 d = 0; d < anim->_dirCount; d++) {
            anim->GetDir(d)->_stateAnim = state_anim;
            anim->GetDir(d)->_actionAnim = action_anim;
        }
    }

    auto* anim_ = anim.get();

    _critterFrames.emplace(id, std::move(anim));

    if (anim_ != nullptr) {
        if (anim_->GetDir(dir) != nullptr) {
            return anim_->GetDir(dir);
        }

        return anim_;
    }

    return nullptr;
}

auto ResourceManager::LoadFalloutAnimFrames(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim) -> shared_ptr<SpriteSheet>
{
    FO_STACK_TRACE_ENTRY();

    // Convert from common to fallout specific
    int32 f_state_anim = 0;
    int32 f_action_anim = 0;
    int32 f_state_anim_ex = 0;
    int32 f_action_anim_ex = 0;
    uint32 flags = 0;

    if (_animNameResolver->ResolveCritterAnimationFallout(model_name, state_anim, action_anim, f_state_anim, f_action_anim, f_state_anim_ex, f_action_anim_ex, flags)) {
        // Load
        const auto* anim = LoadFalloutAnimSubFrames(model_name, f_state_anim, f_action_anim);

        if (anim == nullptr) {
            return nullptr;
        }

        // Merge
        if (f_state_anim_ex != 0 && f_action_anim_ex != 0) {
            const auto* animex = LoadFalloutAnimSubFrames(model_name, f_state_anim_ex, f_action_anim_ex);

            if (animex == nullptr) {
                return nullptr;
            }

            const auto frames_count = anim->GetFramesCount() + animex->GetFramesCount();
            auto anim_merge_base = SafeAlloc::MakeShared<SpriteSheet>(*_sprMngr, frames_count, anim->GetWholeTicks() + animex->GetWholeTicks(), anim->GetDirCount());

            for (int32 d = 0; d < anim->_dirCount; d++) {
                auto* anim_merge = anim_merge_base->GetDir(d);
                const auto* anim_ = anim->GetDir(d);
                const auto* animex_ = animex->GetDir(d);

                for (int32 i = 0; i < anim_->GetFramesCount(); i++) {
                    anim_merge->_spr[i] = anim_->GetSpr(i)->MakeCopy();
                    anim_merge->_sprOffset[i] = anim_->_sprOffset[i];
                }
                for (int32 i = 0; i < animex_->GetFramesCount(); i++) {
                    anim_merge->_spr[i + anim_->GetFramesCount()] = animex_->GetSpr(i)->MakeCopy();
                    anim_merge->_sprOffset[i + anim_->GetFramesCount()] = animex_->_sprOffset[i];
                }

                int32 ox = 0;
                int32 oy = 0;

                for (int32 i = 0; i < anim_->GetFramesCount(); i++) {
                    ox += anim_->_sprOffset[i].x;
                    oy += anim_->_sprOffset[i].y;
                }

                anim_merge->_sprOffset[anim_->GetFramesCount()].x -= ox;
                anim_merge->_sprOffset[anim_->GetFramesCount()].x -= oy;
            }

            return anim_merge_base;
        }

        // Clone
        const auto frames_count = !IsBitSet(flags, ANIM_FLAG_FIRST_FRAME | ANIM_FLAG_LAST_FRAME) ? anim->GetFramesCount() : 1;
        auto anim_clone_base = SafeAlloc::MakeShared<SpriteSheet>(*_sprMngr, frames_count, anim->GetWholeTicks(), anim->GetDirCount());

        for (int32 d = 0; d < anim->_dirCount; d++) {
            auto* anim_clone = anim_clone_base->GetDir(d);
            const auto* anim_ = anim->GetDir(d);

            if (!IsBitSet(flags, ANIM_FLAG_FIRST_FRAME | ANIM_FLAG_LAST_FRAME)) {
                for (int32 i = 0; i < anim_->GetFramesCount(); i++) {
                    anim_clone->_spr[i] = anim_->GetSpr(i)->MakeCopy();
                    anim_clone->_sprOffset[i] = anim_->_sprOffset[i];
                }
            }
            else {
                anim_clone->_spr[0] = anim_->GetSpr(IsBitSet(flags, ANIM_FLAG_FIRST_FRAME) ? 0 : anim_->GetFramesCount() - 1)->MakeCopy();
                anim_clone->_sprOffset[0] = anim_->_sprOffset[IsBitSet(flags, ANIM_FLAG_FIRST_FRAME) ? 0 : anim_->GetFramesCount() - 1];

                // Append offsets
                if (IsBitSet(flags, ANIM_FLAG_LAST_FRAME)) {
                    for (int32 i = 0; i < anim_->GetFramesCount() - 1; i++) {
                        anim_clone->_sprOffset[0].x += anim_->_sprOffset[i].x;
                        anim_clone->_sprOffset[0].y += anim_->_sprOffset[i].y;
                    }
                }
            }
        }

        return anim_clone_base;
    }

    return nullptr;
}

void ResourceManager::FixAnimFramesOffs(SpriteSheet* frames_base, const SpriteSheet* stay_frm_base)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (stay_frm_base == nullptr) {
        return;
    }

    for (int32 d = 0; d < stay_frm_base->_dirCount; d++) {
        auto* frames = frames_base->GetDir(d);
        const auto* stay_frm = stay_frm_base->GetDir(d);
        const auto* stay_spr = stay_frm->GetSpr(0);

        for (int32 i = 0; i < frames->GetFramesCount(); i++) {
            auto* spr = frames->GetSpr(i);
            spr->SetOffset(spr->GetOffset() + stay_spr->GetOffset());
        }
    }
}

void ResourceManager::FixAnimFramesOffsNext(SpriteSheet* frames_base, const SpriteSheet* stay_frm_base)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (stay_frm_base == nullptr) {
        return;
    }

    for (int32 d = 0; d < stay_frm_base->_dirCount; d++) {
        auto* frames = frames_base->GetDir(d);
        const auto* stay_frm = stay_frm_base->GetDir(d);
        ipos32 next_offset;

        for (int32 i = 0; i < stay_frm->GetFramesCount(); i++) {
            next_offset += stay_frm->_sprOffset[i];
        }

        for (int32 i = 0; i < frames->GetFramesCount(); i++) {
            auto* spr = frames->GetSpr(i);
            spr->SetOffset(spr->GetOffset() + next_offset);
        }
    }
}

auto ResourceManager::LoadFalloutAnimSubFrames(hstring model_name, uint32 state_anim, uint32 action_anim) -> const SpriteSheet*
{
    FO_STACK_TRACE_ENTRY();

#define LOADSPR_ADDOFFS(a1, a2) FixAnimFramesOffs(anim_, LoadFalloutAnimSubFrames(model_name, a1, a2))
#define LOADSPR_ADDOFFS_NEXT(a1, a2) FixAnimFramesOffsNext(anim_, LoadFalloutAnimSubFrames(model_name, a1, a2))

    // Fallout animations
    static constexpr uint32 ANIM1_FALLOUT_UNARMED = 1;
    static constexpr uint32 ANIM1_FALLOUT_DEAD = 2;
    static constexpr uint32 ANIM1_FALLOUT_KNOCKOUT = 3;
    static constexpr uint32 ANIM1_FALLOUT_KNIFE = 4;
    static constexpr uint32 ANIM1_FALLOUT_MINIGUN = 12;
    static constexpr uint32 ANIM1_FALLOUT_ROCKET_LAUNCHER = 13;
    static constexpr uint32 ANIM1_FALLOUT_AIM = 14;

    static constexpr uint32 ANIM2_FALLOUT_STAY = 1;
    static constexpr uint32 ANIM2_FALLOUT_WALK = 2;
    static constexpr uint32 ANIM2_FALLOUT_SHOW = 3;
    static constexpr uint32 ANIM2_FALLOUT_PREPARE_WEAPON = 8;
    static constexpr uint32 ANIM2_FALLOUT_TURNOFF_WEAPON = 9;
    static constexpr uint32 ANIM2_FALLOUT_SHOOT = 10;
    static constexpr uint32 ANIM2_FALLOUT_BURST = 11;
    static constexpr uint32 ANIM2_FALLOUT_FLAME = 12;
    static constexpr uint32 ANIM2_FALLOUT_KNOCK_FRONT = 1; // Only with ANIM1_FALLOUT_DEAD
    static constexpr uint32 ANIM2_FALLOUT_KNOCK_BACK = 2;
    static constexpr uint32 ANIM2_FALLOUT_STANDUP_BACK = 8; // Only with ANIM1_FALLOUT_KNOCKOUT
    static constexpr uint32 ANIM2_FALLOUT_RUN = 20;
    static constexpr uint32 ANIM2_FALLOUT_DEAD_FRONT = 1; // Only with ANIM1_FALLOUT_DEAD
    static constexpr uint32 ANIM2_FALLOUT_DEAD_BACK = 2;
    static constexpr uint32 ANIM2_FALLOUT_DEAD_FRONT2 = 15;
    static constexpr uint32 ANIM2_FALLOUT_DEAD_BACK2 = 16;

    const auto it = _critterFrames.find(FalloutAnimMapId(model_name, state_anim, action_anim));
    if (it != _critterFrames.end()) {
        return it->second.get();
    }

    // Try load fofrm
    static constexpr char FRM_IND[] = "_abcdefghijklmnopqrstuvwxyz0123456789";
    FO_RUNTIME_ASSERT(state_anim < sizeof(FRM_IND));
    FO_RUNTIME_ASSERT(action_anim < sizeof(FRM_IND));

    shared_ptr<SpriteSheet> anim;

    string shorten_model_name = strex(model_name).erase_file_extension();
    shorten_model_name = shorten_model_name.substr(0, shorten_model_name.length() - 2);

    // Try load from fofrm
    {
        const string spr_name = strex("{}{}{}.fofrm", shorten_model_name, FRM_IND[state_anim], FRM_IND[action_anim]);
        anim = dynamic_ptr_cast<SpriteSheet>(_sprMngr->LoadSprite(spr_name, AtlasType::MapSprites, true));
    }

    // Try load fallout frames
    if (!anim) {
        const string spr_name = strex("{}{}{}.frm", shorten_model_name, FRM_IND[state_anim], FRM_IND[action_anim]);
        anim = dynamic_ptr_cast<SpriteSheet>(_sprMngr->LoadSprite(spr_name, AtlasType::MapSprites, true));
    }

    auto* anim_ = _critterFrames.emplace(FalloutAnimMapId(model_name, state_anim, action_anim), std::move(anim)).first->second.get();

    if (anim_ == nullptr) {
        return nullptr;
    }

    if (state_anim == ANIM1_FALLOUT_AIM) {
        return anim_; // Aim, 'N'
    }

    // Empty offsets
    if (state_anim == ANIM1_FALLOUT_UNARMED) {
        if (action_anim == ANIM2_FALLOUT_STAY || action_anim == ANIM2_FALLOUT_WALK || action_anim == ANIM2_FALLOUT_RUN) {
            return anim_;
        }
        LOADSPR_ADDOFFS(ANIM1_FALLOUT_UNARMED, ANIM2_FALLOUT_STAY);
    }

    // Weapon offsets
    if (state_anim >= ANIM1_FALLOUT_KNIFE && state_anim <= ANIM1_FALLOUT_ROCKET_LAUNCHER) {
        if (action_anim == ANIM2_FALLOUT_SHOW) {
            LOADSPR_ADDOFFS(ANIM1_FALLOUT_UNARMED, ANIM2_FALLOUT_STAY);
        }
        else if (action_anim == ANIM2_FALLOUT_WALK) {
            return anim_;
        }
        else if (action_anim == ANIM2_FALLOUT_STAY) {
            LOADSPR_ADDOFFS(ANIM1_FALLOUT_UNARMED, ANIM2_FALLOUT_STAY);
            LOADSPR_ADDOFFS_NEXT(state_anim, ANIM2_FALLOUT_SHOW);
        }
        else if (action_anim == ANIM2_FALLOUT_SHOOT || action_anim == ANIM2_FALLOUT_BURST || action_anim == ANIM2_FALLOUT_FLAME) {
            LOADSPR_ADDOFFS(state_anim, ANIM2_FALLOUT_PREPARE_WEAPON);
            LOADSPR_ADDOFFS_NEXT(state_anim, ANIM2_FALLOUT_PREPARE_WEAPON);
        }
        else if (action_anim == ANIM2_FALLOUT_TURNOFF_WEAPON) {
            if (state_anim == ANIM1_FALLOUT_MINIGUN) {
                LOADSPR_ADDOFFS(state_anim, ANIM2_FALLOUT_BURST);
                LOADSPR_ADDOFFS_NEXT(state_anim, ANIM2_FALLOUT_BURST);
            }
            else {
                LOADSPR_ADDOFFS(state_anim, ANIM2_FALLOUT_SHOOT);
                LOADSPR_ADDOFFS_NEXT(state_anim, ANIM2_FALLOUT_SHOOT);
            }
        }
        else {
            LOADSPR_ADDOFFS(state_anim, ANIM2_FALLOUT_STAY);
        }
    }

    // Dead & Ko offsets
    if (state_anim == ANIM1_FALLOUT_DEAD) {
        if (action_anim == ANIM2_FALLOUT_DEAD_FRONT2) {
            LOADSPR_ADDOFFS(ANIM1_FALLOUT_DEAD, ANIM2_FALLOUT_DEAD_FRONT);
            LOADSPR_ADDOFFS_NEXT(ANIM1_FALLOUT_DEAD, ANIM2_FALLOUT_DEAD_FRONT);
        }
        else if (action_anim == ANIM2_FALLOUT_DEAD_BACK2) {
            LOADSPR_ADDOFFS(ANIM1_FALLOUT_DEAD, ANIM2_FALLOUT_DEAD_BACK);
            LOADSPR_ADDOFFS_NEXT(ANIM1_FALLOUT_DEAD, ANIM2_FALLOUT_DEAD_BACK);
        }
        else {
            LOADSPR_ADDOFFS(ANIM1_FALLOUT_UNARMED, ANIM2_FALLOUT_STAY);
        }
    }

    // Ko rise offsets
    if (state_anim == ANIM1_FALLOUT_KNOCKOUT) {
        auto anim2_ = ANIM2_FALLOUT_KNOCK_FRONT;
        if (action_anim == ANIM2_FALLOUT_STANDUP_BACK) {
            anim2_ = ANIM2_FALLOUT_KNOCK_BACK;
        }
        LOADSPR_ADDOFFS(ANIM1_FALLOUT_DEAD, anim2_);
        LOADSPR_ADDOFFS_NEXT(ANIM1_FALLOUT_DEAD, anim2_);
    }

    return anim_;

#undef LOADSPR_ADDOFFS
#undef LOADSPR_ADDOFFS_NEXT
}

auto ResourceManager::GetCritterPreviewSpr(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, uint8 dir, const int32* layers3d) -> const Sprite*
{
    FO_STACK_TRACE_ENTRY();

    const string ext = strex(model_name).get_file_extension();

    if (ext != "fo3d") {
        const auto* frames = GetCritterAnimFrames(model_name, state_anim, action_anim, dir);
        return frames != nullptr ? frames : _critterDummyAnimFrames.get();
    }
    else {
#if FO_ENABLE_3D
        const auto* model_spr = GetCritterPreviewModelSpr(model_name, state_anim, action_anim, dir, layers3d);
        return model_spr != nullptr ? static_cast<const Sprite*>(model_spr) : static_cast<const Sprite*>(_critterDummyAnimFrames.get());
#else
        ignore_unused(layers3d);
        throw NotEnabled3DException("3D submodule not enabled");
#endif
    }
}

#if FO_ENABLE_3D
auto ResourceManager::GetCritterPreviewModelSpr(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, uint8 dir, const int32* layers3d) -> const ModelSprite*
{
    FO_STACK_TRACE_ENTRY();

    if (const auto it = _critterModels.find(model_name); it != _critterModels.end()) {
        auto& model_spr = it->second;

        model_spr->GetModel()->SetDir(dir, false);
        model_spr->GetModel()->PlayAnim(state_anim, action_anim, layers3d, 0.0f, CombineEnum(ModelAnimFlags::Freeze, ModelAnimFlags::NoSmooth));

        model_spr->DrawToAtlas();

        return model_spr.get();
    }

    auto model_spr = dynamic_ptr_cast<ModelSprite>(_sprMngr->LoadSprite(model_name, AtlasType::MapSprites));

    if (!model_spr) {
        return nullptr;
    }

    auto* model = model_spr->GetModel();

    model->PlayAnim(state_anim, action_anim, layers3d, 0.0f, CombineEnum(ModelAnimFlags::Freeze, ModelAnimFlags::NoSmooth));
    model->SetDir(dir, false);
    model->PrewarmParticles();
    model->StartMeshGeneration();

    model_spr->DrawToAtlas();

    return _critterModels.emplace(model_name, std::move(model_spr)).first->second.get();
}
#endif

auto ResourceManager::GetSoundNames() const -> const map<string, string>&
{
    FO_STACK_TRACE_ENTRY();

    return _soundNames;
}

FO_END_NAMESPACE();
