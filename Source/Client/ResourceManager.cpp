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

#include "ResourceManager.h"
#include "3dStuff.h"
#include "DataSource.h"
#include "FileSystem.h"
#include "GenericUtils.h"
#include "StringUtils.h"

static constexpr uint ANIM_FLAG_FIRST_FRAME = 0x01;
static constexpr uint ANIM_FLAG_LAST_FRAME = 0x02;

ResourceManager::ResourceManager(FileSystem& resources, SpriteManager& spr_mngr, AnimationResolver& anim_name_resolver) :
    _resources {resources},
    _sprMngr {spr_mngr},
    _animNameResolver {anim_name_resolver}
{
    STACK_TRACE_ENTRY();
}

void ResourceManager::IndexFiles()
{
    STACK_TRACE_ENTRY();

    for (const auto* splash_ext : {"rix", "png", "jpg"}) {
        auto splashes = _resources.FilterFiles(splash_ext, "Splash/", true);
        while (splashes.MoveNext()) {
            auto file_header = splashes.GetCurFileHeader();
            if (std::find(_splashNames.begin(), _splashNames.end(), file_header.GetPath()) == _splashNames.end()) {
                _splashNames.emplace_back(file_header.GetPath());
            }
        }
    }

    for (const auto* sound_ext : {"wav", "acm", "ogg"}) {
        auto sounds = _resources.FilterFiles(sound_ext);
        while (sounds.MoveNext()) {
            auto file_header = sounds.GetCurFileHeader();
            _soundNames.emplace(strex(file_header.GetPath()).eraseFileExtension().lower(), file_header.GetPath());
        }
    }

    auto&& spr = dynamic_pointer_cast<AtlasSprite>(_sprMngr.LoadSprite("CritterStub.png", AtlasType::MapSprites));
    RUNTIME_ASSERT(spr);
    _critterDummyAnimFrames = std::make_unique<SpriteSheet>(_sprMngr, 1, 100, 1);
    _critterDummyAnimFrames->Size = spr->Size;
    _critterDummyAnimFrames->Spr[0] = std::move(spr);
    RUNTIME_ASSERT(_critterDummyAnimFrames);

    _itemHexDummyAnim = _sprMngr.LoadSprite("ItemStub.png", AtlasType::MapSprites);
    RUNTIME_ASSERT(_itemHexDummyAnim);
}

auto ResourceManager::GetItemDefaultSpr() -> const shared_ptr<Sprite>&
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _itemHexDummyAnim;
}

auto ResourceManager::GetCritterDummyFrames() -> const SpriteSheet*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _critterDummyAnimFrames.get();
}

void ResourceManager::CleanupCritterFrames()
{
    STACK_TRACE_ENTRY();

    _critterFrames.clear();
}

static auto AnimMapId(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim) -> uint
{
    STACK_TRACE_ENTRY();

    const uint dw[4] = {model_name.as_uint(), static_cast<uint>(state_anim), static_cast<uint>(action_anim), 1};
    return Hashing::MurmurHash2(dw, sizeof(dw));
}

static auto FalloutAnimMapId(hstring model_name, uint state_anim, uint action_anim) -> uint
{
    STACK_TRACE_ENTRY();

    const uint dw[4] = {model_name.as_uint(), static_cast<uint>(state_anim), static_cast<uint>(action_anim), static_cast<uint>(-1)};
    return Hashing::MurmurHash2(dw, sizeof(dw));
}

auto ResourceManager::GetCritterAnimFrames(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, uint8 dir) -> const SpriteSheet*
{
    STACK_TRACE_ENTRY();

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
        if (!!model_name && strex(model_name).startsWith("art/critters/")) {
            // Hardcoded
            anim = LoadFalloutAnimFrames(model_name, state_anim, action_anim);
        }
        else {
            // Script specific
            uint pass_base = 0;
            while (true) {
                auto pass = pass_base;
                uint flags = 0;
                auto ox = 0;
                auto oy = 0;
                string anim_name;

                if (_animNameResolver.ResolveCritterAnimation(model_name, state_anim, action_anim, pass, flags, ox, oy, anim_name)) {
                    if (!anim_name.empty()) {
                        anim = dynamic_pointer_cast<SpriteSheet>(_sprMngr.LoadSprite(anim_name, AtlasType::MapSprites, true));

                        // Fix by dirs
                        for (uint d = 0; anim != nullptr && d < anim->DirCount; d++) {
                            auto* dir_anim = anim->GetDir(d);

                            // Process flags
                            if (flags != 0) {
                                if (IsBitSet(flags, ANIM_FLAG_FIRST_FRAME) || IsBitSet(flags, ANIM_FLAG_LAST_FRAME)) {
                                    const auto first = IsBitSet(flags, ANIM_FLAG_FIRST_FRAME);

                                    // Append offsets
                                    if (!first) {
                                        for (uint i = 0; i < dir_anim->CntFrm - 1; i++) {
                                            dir_anim->SprOffset[dir_anim->CntFrm - 1].x += dir_anim->SprOffset[i].x;
                                            dir_anim->SprOffset[dir_anim->CntFrm - 1].y += dir_anim->SprOffset[i].y;
                                        }
                                    }

                                    // Change size
                                    dir_anim->Spr[0] = first ? dir_anim->GetSpr(0)->MakeCopy() : dir_anim->GetSpr(dir_anim->CntFrm - 1)->MakeCopy();
                                    dir_anim->SprOffset[0].x = first ? dir_anim->SprOffset[0].x : dir_anim->SprOffset[dir_anim->CntFrm - 1].x;
                                    dir_anim->SprOffset[0].y = first ? dir_anim->SprOffset[0].y : dir_anim->SprOffset[dir_anim->CntFrm - 1].y;
                                    dir_anim->CntFrm = 1;
                                }
                            }

                            // Add offsets
                            ox = oy = 0; // Todo: why I disable offset adding?
                            if (ox != 0 || oy != 0) {
                                for (uint i = 0; i < dir_anim->CntFrm; i++) {
                                    auto* spr = dir_anim->GetSpr(i);
                                    auto fixed = false;
                                    for (uint j = 0; j < i; j++) {
                                        if (dir_anim->GetSpr(j) == spr) {
                                            fixed = true;
                                            break;
                                        }
                                    }
                                    if (!fixed) {
                                        spr->Offset.x += ox;
                                        spr->Offset.y += oy;
                                    }
                                }
                            }
                        }
                    }

                    // If pass changed and animation not loaded than try again
                    if (anim == nullptr && pass != pass_base) {
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

        if (anim == nullptr && _animNameResolver.ResolveCritterAnimationSubstitute(base_model_name, base_state_anim, base_action_anim, model_name, state_anim, action_anim)) {
            if (model_name_ != model_name || state_anim != state_anim_ || action_anim != action_anim_) {
                continue;
            }
        }

        // Exit from loop
        break;
    }

    // Store resulted animation indices
    if (anim != nullptr) {
        for (uint d = 0; d < anim->DirCount; d++) {
            anim->GetDir(d)->StateAnim = state_anim;
            anim->GetDir(d)->ActionAnim = action_anim;
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
    STACK_TRACE_ENTRY();

    // Convert from common to fallout specific
    uint f_state_anim = 0;
    uint f_action_anim = 0;
    uint f_state_anim_ex = 0;
    uint f_action_anim_ex = 0;
    uint flags = 0;

    if (_animNameResolver.ResolveCritterAnimationFallout(model_name, state_anim, action_anim, f_state_anim, f_action_anim, f_state_anim_ex, f_action_anim_ex, flags)) {
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

            auto&& anim_merge_base = std::make_unique<SpriteSheet>(_sprMngr, anim->CntFrm + animex->CntFrm, anim->WholeTicks + animex->WholeTicks, anim->DirCount);

            for (uint d = 0; d < anim->DirCount; d++) {
                auto* anim_merge = anim_merge_base->GetDir(d);
                const auto* anim_ = anim->GetDir(d);
                const auto* animex_ = animex->GetDir(d);

                for (uint i = 0; i < anim_->CntFrm; i++) {
                    anim_merge->Spr[i] = anim_->GetSpr(i)->MakeCopy();
                    anim_merge->SprOffset[i] = anim_->SprOffset[i];
                }
                for (uint i = 0; i < animex_->CntFrm; i++) {
                    anim_merge->Spr[i + anim_->CntFrm] = animex_->GetSpr(i)->MakeCopy();
                    anim_merge->SprOffset[i + anim_->CntFrm] = animex_->SprOffset[i];
                }

                int ox = 0;
                int oy = 0;
                for (uint i = 0; i < anim_->CntFrm; i++) {
                    ox += anim_->SprOffset[i].x;
                    oy += anim_->SprOffset[i].y;
                }

                anim_merge->SprOffset[anim_->CntFrm].x -= ox;
                anim_merge->SprOffset[anim_->CntFrm].x -= oy;
            }

            return std::move(anim_merge_base);
        }

        // Clone
        if (anim != nullptr) {
            auto&& anim_clone_base = std::make_unique<SpriteSheet>(_sprMngr, !IsBitSet(flags, ANIM_FLAG_FIRST_FRAME | ANIM_FLAG_LAST_FRAME) ? anim->CntFrm : 1, anim->WholeTicks, anim->DirCount);

            for (uint d = 0; d < anim->DirCount; d++) {
                auto* anim_clone = anim_clone_base->GetDir(d);
                const auto* anim_ = anim->GetDir(d);

                if (!IsBitSet(flags, ANIM_FLAG_FIRST_FRAME | ANIM_FLAG_LAST_FRAME)) {
                    for (uint i = 0; i < anim_->CntFrm; i++) {
                        anim_clone->Spr[i] = anim_->GetSpr(i)->MakeCopy();
                        anim_clone->SprOffset[i] = anim_->SprOffset[i];
                    }
                }
                else {
                    anim_clone->Spr[0] = anim_->GetSpr(IsBitSet(flags, ANIM_FLAG_FIRST_FRAME) ? 0 : anim_->CntFrm - 1)->MakeCopy();
                    anim_clone->SprOffset[0] = anim_->SprOffset[IsBitSet(flags, ANIM_FLAG_FIRST_FRAME) ? 0 : anim_->CntFrm - 1];

                    // Append offsets
                    if (IsBitSet(flags, ANIM_FLAG_LAST_FRAME)) {
                        for (uint i = 0; i < anim_->CntFrm - 1; i++) {
                            anim_clone->SprOffset[0].x += anim_->SprOffset[i].x;
                            anim_clone->SprOffset[0].y += anim_->SprOffset[i].y;
                        }
                    }
                }
            }

            return std::move(anim_clone_base);
        }

        return nullptr;
    }

    return nullptr;
}

void ResourceManager::FixAnimFramesOffs(SpriteSheet* frames_base, const SpriteSheet* stay_frm_base)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (stay_frm_base == nullptr) {
        return;
    }

    for (uint d = 0; d < stay_frm_base->DirCount; d++) {
        auto* frames = frames_base->GetDir(d);
        const auto* stay_frm = stay_frm_base->GetDir(d);
        const auto* stay_spr = stay_frm->GetSpr(0);

        for (uint i = 0; i < frames->CntFrm; i++) {
            auto* spr = frames->GetSpr(i);

            spr->Offset.x += stay_spr->Offset.x;
            spr->Offset.y += stay_spr->Offset.y;
        }
    }
}

void ResourceManager::FixAnimFramesOffsNext(SpriteSheet* frames_base, const SpriteSheet* stay_frm_base)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (stay_frm_base == nullptr) {
        return;
    }

    for (uint d = 0; d < stay_frm_base->DirCount; d++) {
        auto* frames = frames_base->GetDir(d);
        const auto* stay_frm = stay_frm_base->GetDir(d);

        int next_x = 0;
        int next_y = 0;

        for (uint i = 0; i < stay_frm->CntFrm; i++) {
            next_x += stay_frm->SprOffset[i].x;
            next_y += stay_frm->SprOffset[i].y;
        }

        for (uint i = 0; i < frames->CntFrm; i++) {
            auto* spr = frames->GetSpr(i);

            spr->Offset.x += next_x;
            spr->Offset.y += next_y;
        }
    }
}

auto ResourceManager::LoadFalloutAnimSubFrames(hstring model_name, uint state_anim, uint action_anim) -> const SpriteSheet*
{
    STACK_TRACE_ENTRY();

#define LOADSPR_ADDOFFS(a1, a2) FixAnimFramesOffs(anim_, LoadFalloutAnimSubFrames(model_name, a1, a2))
#define LOADSPR_ADDOFFS_NEXT(a1, a2) FixAnimFramesOffsNext(anim_, LoadFalloutAnimSubFrames(model_name, a1, a2))

    // Fallout animations
    static constexpr uint ANIM1_FALLOUT_UNARMED = 1;
    static constexpr uint ANIM1_FALLOUT_DEAD = 2;
    static constexpr uint ANIM1_FALLOUT_KNOCKOUT = 3;
    static constexpr uint ANIM1_FALLOUT_KNIFE = 4;
    static constexpr uint ANIM1_FALLOUT_MINIGUN = 12;
    static constexpr uint ANIM1_FALLOUT_ROCKET_LAUNCHER = 13;
    static constexpr uint ANIM1_FALLOUT_AIM = 14;

    static constexpr uint ANIM2_FALLOUT_STAY = 1;
    static constexpr uint ANIM2_FALLOUT_WALK = 2;
    static constexpr uint ANIM2_FALLOUT_SHOW = 3;
    static constexpr uint ANIM2_FALLOUT_PREPARE_WEAPON = 8;
    static constexpr uint ANIM2_FALLOUT_TURNOFF_WEAPON = 9;
    static constexpr uint ANIM2_FALLOUT_SHOOT = 10;
    static constexpr uint ANIM2_FALLOUT_BURST = 11;
    static constexpr uint ANIM2_FALLOUT_FLAME = 12;
    static constexpr uint ANIM2_FALLOUT_KNOCK_FRONT = 1; // Only with ANIM1_FALLOUT_DEAD
    static constexpr uint ANIM2_FALLOUT_KNOCK_BACK = 2;
    static constexpr uint ANIM2_FALLOUT_STANDUP_BACK = 8; // Only with ANIM1_FALLOUT_KNOCKOUT
    static constexpr uint ANIM2_FALLOUT_RUN = 20;
    static constexpr uint ANIM2_FALLOUT_DEAD_FRONT = 1; // Only with ANIM1_FALLOUT_DEAD
    static constexpr uint ANIM2_FALLOUT_DEAD_BACK = 2;
    static constexpr uint ANIM2_FALLOUT_DEAD_FRONT2 = 15;
    static constexpr uint ANIM2_FALLOUT_DEAD_BACK2 = 16;

    const auto it = _critterFrames.find(FalloutAnimMapId(model_name, state_anim, action_anim));
    if (it != _critterFrames.end()) {
        return it->second.get();
    }

    // Try load fofrm
    static constexpr char FRM_IND[] = "_abcdefghijklmnopqrstuvwxyz0123456789";
    RUNTIME_ASSERT(static_cast<uint>(state_anim) < sizeof(FRM_IND));
    RUNTIME_ASSERT(static_cast<uint>(action_anim) < sizeof(FRM_IND));

    shared_ptr<SpriteSheet> anim;

    string shorten_model_name = strex(model_name).eraseFileExtension();
    shorten_model_name = shorten_model_name.substr(0, shorten_model_name.length() - 2);

    // Try load from fofrm
    {
        const string spr_name = strex("{}{}{}.fofrm", shorten_model_name, FRM_IND[static_cast<uint>(state_anim)], FRM_IND[static_cast<uint>(action_anim)]);
        anim = dynamic_pointer_cast<SpriteSheet>(_sprMngr.LoadSprite(spr_name, AtlasType::MapSprites, true));
    }

    // Try load fallout frames
    if (!anim) {
        const string spr_name = strex("{}{}{}.frm", shorten_model_name, FRM_IND[static_cast<uint>(state_anim)], FRM_IND[static_cast<uint>(action_anim)]);
        anim = dynamic_pointer_cast<SpriteSheet>(_sprMngr.LoadSprite(spr_name, AtlasType::MapSprites, true));
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

auto ResourceManager::GetCritterPreviewSpr(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, uint8 dir, const int* layers3d) -> const Sprite*
{
    STACK_TRACE_ENTRY();

    const string ext = strex(model_name).getFileExtension();
    if (ext != "fo3d") {
        const auto* frames = GetCritterAnimFrames(model_name, state_anim, action_anim, dir);
        return frames != nullptr ? frames : _critterDummyAnimFrames.get();
    }
    else {
#if FO_ENABLE_3D
        const auto* model_spr = GetCritterPreviewModelSpr(model_name, state_anim, action_anim, dir, layers3d);
        return model_spr != nullptr ? static_cast<const Sprite*>(model_spr) : static_cast<const Sprite*>(_critterDummyAnimFrames.get());
#else
        UNUSED_VARIABLE(layers3d);
        throw NotEnabled3DException("3D submodule not enabled");
#endif
    }
}

#if FO_ENABLE_3D
auto ResourceManager::GetCritterPreviewModelSpr(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, uint8 dir, const int* layers3d) -> const ModelSprite*
{
    STACK_TRACE_ENTRY();

    if (const auto it = _critterModels.find(model_name); it != _critterModels.end()) {
        auto&& model_spr = it->second;

        model_spr->GetModel()->SetDir(dir, false);
        model_spr->GetModel()->SetAnimation(state_anim, action_anim, layers3d, ANIMATION_STAY | ANIMATION_NO_SMOOTH);

        model_spr->DrawToAtlas();

        return model_spr.get();
    }

    auto&& model_spr = dynamic_pointer_cast<ModelSprite>(_sprMngr.LoadSprite(model_name, AtlasType::MapSprites));
    if (!model_spr) {
        return nullptr;
    }

    auto* model = model_spr->GetModel();

    model->SetAnimation(state_anim, action_anim, layers3d, ANIMATION_STAY | ANIMATION_NO_SMOOTH);
    model->SetDir(dir, false);
    model->PrewarmParticles();
    model->StartMeshGeneration();

    model_spr->DrawToAtlas();

    return _critterModels.emplace(model_name, std::move(model_spr)).first->second.get();
}
#endif

auto ResourceManager::GetRandomSplash() -> shared_ptr<Sprite>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (_splashNames.empty()) {
        return nullptr;
    }

    const auto rnd = GenericUtils::Random(0, static_cast<int>(_splashNames.size()) - 1);

    auto&& splash = _sprMngr.LoadSprite(_splashNames[rnd], AtlasType::OneImage);

    if (splash) {
        splash->PlayDefault();
    }

    return splash;
}

auto ResourceManager::GetSoundNames() const -> const map<string, string>&
{
    STACK_TRACE_ENTRY();

    return _soundNames;
}
