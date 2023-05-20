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
            _soundNames.emplace(_str(file_header.GetPath()).eraseFileExtension().lower().str(), file_header.GetPath());
        }
    }

    auto&& spr = dynamic_pointer_cast<AtlasSprite>(_sprMngr.LoadSprite("CritterStub.png", AtlasType::MapSprites));
    RUNTIME_ASSERT(spr);
    _critterDummyAnimFrames = std::make_unique<SpriteSheet>(_sprMngr, 1, 100, 1);
    _critterDummyAnimFrames->Width = spr->Width;
    _critterDummyAnimFrames->Height = spr->Height;
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

static auto AnimMapId(hstring model_name, uint anim1, uint anim2, bool is_fallout) -> uint
{
    STACK_TRACE_ENTRY();

    const uint dw[4] = {model_name.as_uint(), anim1, anim2, is_fallout ? static_cast<uint>(-1) : 1};
    return Hashing::MurmurHash2(dw, sizeof(dw));
}

auto ResourceManager::GetCritterAnimFrames(hstring model_name, uint anim1, uint anim2, uint8 dir) -> const SpriteSheet*
{
    STACK_TRACE_ENTRY();

    // Check already loaded
    const auto id = AnimMapId(model_name, anim1, anim2, false);

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
    const auto anim1_base = anim1;
    const auto anim2_base = anim2;
    shared_ptr<SpriteSheet> anim;

    while (true) {
        // Load
        if (!!model_name && _str(model_name).startsWith("art/critters/")) {
            // Hardcoded
            anim = LoadFalloutAnimFrames(model_name, anim1, anim2);
        }
        else {
            // Script specific
            uint pass_base = 0;
            while (true) {
                auto pass = pass_base;
                uint flags = 0;
                auto ox = 0;
                auto oy = 0;
                string str;
                if (_animNameResolver.ResolveCritterAnimation(model_name, anim1, anim2, pass, flags, ox, oy, str)) {
                    if (!str.empty()) {
                        anim = dynamic_pointer_cast<SpriteSheet>(_sprMngr.LoadSprite(str, AtlasType::MapSprites));

                        // Fix by dirs
                        for (uint d = 0; anim != nullptr && d < anim->DirCount; d++) {
                            auto* dir_anim = anim->GetDir(d);

                            // Process flags
                            if (flags != 0u) {
                                if (IsBitSet(flags, ANIM_FLAG_FIRST_FRAME) || IsBitSet(flags, ANIM_FLAG_LAST_FRAME)) {
                                    const auto first = IsBitSet(flags, ANIM_FLAG_FIRST_FRAME);

                                    // Append offsets
                                    if (!first) {
                                        for (uint i = 0; i < dir_anim->CntFrm - 1; i++) {
                                            dir_anim->SprOffset[dir_anim->CntFrm - 1].X += dir_anim->SprOffset[i].X;
                                            dir_anim->SprOffset[dir_anim->CntFrm - 1].Y += dir_anim->SprOffset[i].Y;
                                        }
                                    }

                                    // Change size
                                    dir_anim->Spr[0] = first ? dir_anim->GetSpr(0)->MakeCopy() : dir_anim->GetSpr(dir_anim->CntFrm - 1)->MakeCopy();
                                    dir_anim->SprOffset[0].X = first ? dir_anim->SprOffset[0].X : dir_anim->SprOffset[dir_anim->CntFrm - 1].X;
                                    dir_anim->SprOffset[0].Y = first ? dir_anim->SprOffset[0].Y : dir_anim->SprOffset[dir_anim->CntFrm - 1].Y;
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
                                        spr->OffsX += ox;
                                        spr->OffsY += oy;
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
        const auto anim1_ = anim1;
        const auto anim2_ = anim2;
        if (anim == nullptr && _animNameResolver.ResolveCritterAnimationSubstitute(base_model_name, anim1_base, anim2_base, model_name, anim1, anim2)) {
            if (model_name_ != model_name || anim1 != anim1_ || anim2 != anim2_) {
                continue;
            }
        }

        // Exit from loop
        break;
    }

    // Store resulted animation indices
    if (anim != nullptr) {
        for (uint d = 0; d < anim->DirCount; d++) {
            anim->GetDir(d)->Anim1 = anim1;
            anim->GetDir(d)->Anim2 = anim2;
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

auto ResourceManager::LoadFalloutAnimFrames(hstring model_name, uint anim1, uint anim2) -> shared_ptr<SpriteSheet>
{
    STACK_TRACE_ENTRY();

    // Convert from common to fallout specific
    uint anim1_ex = 0;
    uint anim2_ex = 0;
    uint flags = 0;
    if (_animNameResolver.ResolveCritterAnimationFallout(model_name, anim1, anim2, anim1_ex, anim2_ex, flags)) {
        // Load
        const auto* anim = LoadFalloutAnimSubFrames(model_name, anim1, anim2);
        if (anim == nullptr) {
            return nullptr;
        }

        // Merge
        if (anim1_ex != 0 && anim2_ex != 0) {
            const auto* animex = LoadFalloutAnimSubFrames(model_name, anim1_ex, anim2_ex);
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
                    ox += anim_->SprOffset[i].X;
                    oy += anim_->SprOffset[i].Y;
                }

                anim_merge->SprOffset[anim_->CntFrm].X -= ox;
                anim_merge->SprOffset[anim_->CntFrm].X -= oy;
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
                            anim_clone->SprOffset[0].X += anim_->SprOffset[i].X;
                            anim_clone->SprOffset[0].Y += anim_->SprOffset[i].Y;
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

            spr->OffsX += stay_spr->OffsX;
            spr->OffsY += stay_spr->OffsY;
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
            next_x += stay_frm->SprOffset[i].X;
            next_y += stay_frm->SprOffset[i].Y;
        }

        for (uint i = 0; i < frames->CntFrm; i++) {
            auto* spr = frames->GetSpr(i);

            spr->OffsX += next_x;
            spr->OffsY += next_y;
        }
    }
}

auto ResourceManager::LoadFalloutAnimSubFrames(hstring model_name, uint anim1, uint anim2) -> const SpriteSheet*
{
    STACK_TRACE_ENTRY();

#define LOADSPR_ADDOFFS(a1, a2) FixAnimFramesOffs(anim_, LoadFalloutAnimSubFrames(model_name, a1, a2))
#define LOADSPR_ADDOFFS_NEXT(a1, a2) FixAnimFramesOffsNext(anim_, LoadFalloutAnimSubFrames(model_name, a1, a2))

    // Fallout animations
    static constexpr auto ANIM1_FALLOUT_UNARMED = 1;
    static constexpr auto ANIM1_FALLOUT_DEAD = 2;
    static constexpr auto ANIM1_FALLOUT_KNOCKOUT = 3;
    static constexpr auto ANIM1_FALLOUT_KNIFE = 4;
    static constexpr auto ANIM1_FALLOUT_MINIGUN = 12;
    static constexpr auto ANIM1_FALLOUT_ROCKET_LAUNCHER = 13;
    static constexpr auto ANIM1_FALLOUT_AIM = 14;
    static constexpr auto ANIM2_FALLOUT_STAY = 1;
    static constexpr auto ANIM2_FALLOUT_WALK = 2;
    static constexpr auto ANIM2_FALLOUT_SHOW = 3;
    static constexpr auto ANIM2_FALLOUT_PREPARE_WEAPON = 8;
    static constexpr auto ANIM2_FALLOUT_TURNOFF_WEAPON = 9;
    static constexpr auto ANIM2_FALLOUT_SHOOT = 10;
    static constexpr auto ANIM2_FALLOUT_BURST = 11;
    static constexpr auto ANIM2_FALLOUT_FLAME = 12;
    static constexpr auto ANIM2_FALLOUT_KNOCK_FRONT = 1; // Only with ANIM1_FALLOUT_DEAD
    static constexpr auto ANIM2_FALLOUT_KNOCK_BACK = 2;
    static constexpr auto ANIM2_FALLOUT_STANDUP_BACK = 8; // Only with ANIM1_FALLOUT_KNOCKOUT
    static constexpr auto ANIM2_FALLOUT_RUN = 20;
    static constexpr auto ANIM2_FALLOUT_DEAD_FRONT = 1; // Only with ANIM1_FALLOUT_DEAD
    static constexpr auto ANIM2_FALLOUT_DEAD_BACK = 2;
    static constexpr auto ANIM2_FALLOUT_DEAD_FRONT2 = 15;
    static constexpr auto ANIM2_FALLOUT_DEAD_BACK2 = 16;

    const auto it = _critterFrames.find(AnimMapId(model_name, anim1, anim2, true));
    if (it != _critterFrames.end()) {
        return it->second.get();
    }

    // Try load fofrm
    static constexpr char FRM_IND[] = "_abcdefghijklmnopqrstuvwxyz0123456789";
    RUNTIME_ASSERT(anim1 < sizeof(FRM_IND));
    RUNTIME_ASSERT(anim2 < sizeof(FRM_IND));

    shared_ptr<SpriteSheet> anim;

    // Try load from fofrm
    {
        const string spr_name = _str("{}{}{}.fofrm", model_name, FRM_IND[anim1], FRM_IND[anim2]);
        anim = dynamic_pointer_cast<SpriteSheet>(_sprMngr.LoadSprite(spr_name, AtlasType::MapSprites));
    }

    // Try load fallout frames
    if (!anim) {
        const string spr_name = _str("{}{}{}.frm", model_name, FRM_IND[anim1], FRM_IND[anim2]);
        anim = dynamic_pointer_cast<SpriteSheet>(_sprMngr.LoadSprite(spr_name, AtlasType::MapSprites));
    }

    auto* anim_ = _critterFrames.emplace(AnimMapId(model_name, anim1, anim2, true), std::move(anim)).first->second.get();

    if (anim_ == nullptr) {
        return nullptr;
    }

    if (anim1 == ANIM1_FALLOUT_AIM) {
        return anim_; // Aim, 'N'
    }

    // Empty offsets
    if (anim1 == ANIM1_FALLOUT_UNARMED) {
        if (anim2 == ANIM2_FALLOUT_STAY || anim2 == ANIM2_FALLOUT_WALK || anim2 == ANIM2_FALLOUT_RUN) {
            return anim_;
        }
        LOADSPR_ADDOFFS(ANIM1_FALLOUT_UNARMED, ANIM2_FALLOUT_STAY);
    }

    // Weapon offsets
    if (anim1 >= ANIM1_FALLOUT_KNIFE && anim1 <= ANIM1_FALLOUT_ROCKET_LAUNCHER) {
        if (anim2 == ANIM2_FALLOUT_SHOW) {
            LOADSPR_ADDOFFS(ANIM1_FALLOUT_UNARMED, ANIM2_FALLOUT_STAY);
        }
        else if (anim2 == ANIM2_FALLOUT_WALK) {
            return anim_;
        }
        else if (anim2 == ANIM2_FALLOUT_STAY) {
            LOADSPR_ADDOFFS(ANIM1_FALLOUT_UNARMED, ANIM2_FALLOUT_STAY);
            LOADSPR_ADDOFFS_NEXT(anim1, ANIM2_FALLOUT_SHOW);
        }
        else if (anim2 == ANIM2_FALLOUT_SHOOT || anim2 == ANIM2_FALLOUT_BURST || anim2 == ANIM2_FALLOUT_FLAME) {
            LOADSPR_ADDOFFS(anim1, ANIM2_FALLOUT_PREPARE_WEAPON);
            LOADSPR_ADDOFFS_NEXT(anim1, ANIM2_FALLOUT_PREPARE_WEAPON);
        }
        else if (anim2 == ANIM2_FALLOUT_TURNOFF_WEAPON) {
            if (anim1 == ANIM1_FALLOUT_MINIGUN) {
                LOADSPR_ADDOFFS(anim1, ANIM2_FALLOUT_BURST);
                LOADSPR_ADDOFFS_NEXT(anim1, ANIM2_FALLOUT_BURST);
            }
            else {
                LOADSPR_ADDOFFS(anim1, ANIM2_FALLOUT_SHOOT);
                LOADSPR_ADDOFFS_NEXT(anim1, ANIM2_FALLOUT_SHOOT);
            }
        }
        else {
            LOADSPR_ADDOFFS(anim1, ANIM2_FALLOUT_STAY);
        }
    }

    // Dead & Ko offsets
    if (anim1 == ANIM1_FALLOUT_DEAD) {
        if (anim2 == ANIM2_FALLOUT_DEAD_FRONT2) {
            LOADSPR_ADDOFFS(ANIM1_FALLOUT_DEAD, ANIM2_FALLOUT_DEAD_FRONT);
            LOADSPR_ADDOFFS_NEXT(ANIM1_FALLOUT_DEAD, ANIM2_FALLOUT_DEAD_FRONT);
        }
        else if (anim2 == ANIM2_FALLOUT_DEAD_BACK2) {
            LOADSPR_ADDOFFS(ANIM1_FALLOUT_DEAD, ANIM2_FALLOUT_DEAD_BACK);
            LOADSPR_ADDOFFS_NEXT(ANIM1_FALLOUT_DEAD, ANIM2_FALLOUT_DEAD_BACK);
        }
        else {
            LOADSPR_ADDOFFS(ANIM1_FALLOUT_UNARMED, ANIM2_FALLOUT_STAY);
        }
    }

    // Ko rise offsets
    if (anim1 == ANIM1_FALLOUT_KNOCKOUT) {
        uint8 anim2_ = ANIM2_FALLOUT_KNOCK_FRONT;
        if (anim2 == ANIM2_FALLOUT_STANDUP_BACK) {
            anim2_ = ANIM2_FALLOUT_KNOCK_BACK;
        }
        LOADSPR_ADDOFFS(ANIM1_FALLOUT_DEAD, anim2_);
        LOADSPR_ADDOFFS_NEXT(ANIM1_FALLOUT_DEAD, anim2_);
    }

    return anim_;

#undef LOADSPR_ADDOFFS
#undef LOADSPR_ADDOFFS_NEXT
}

auto ResourceManager::GetCritterPreviewSpr(hstring model_name, uint anim1, uint anim2, uint8 dir, const int* layers3d) -> const Sprite*
{
    STACK_TRACE_ENTRY();

    const string ext = _str(model_name).getFileExtension();
    if (ext != "fo3d") {
        const auto* frames = GetCritterAnimFrames(model_name, anim1, anim2, dir);
        return frames != nullptr ? frames : _critterDummyAnimFrames.get();
    }
    else {
#if FO_ENABLE_3D
        const auto* model_spr = GetCritterPreviewModelSpr(model_name, anim1, anim2, dir, layers3d);
        return model_spr != nullptr ? static_cast<const Sprite*>(model_spr) : static_cast<const Sprite*>(_critterDummyAnimFrames.get());
#else
        UNUSED_VARIABLE(layers3d);
        throw NotEnabled3DException("3D submodule not enabled");
#endif
    }
}

#if FO_ENABLE_3D
auto ResourceManager::GetCritterPreviewModelSpr(hstring model_name, uint anim1, uint anim2, uint8 dir, const int* layers3d) -> const ModelSprite*
{
    STACK_TRACE_ENTRY();

    if (const auto it = _critterModels.find(model_name); it != _critterModels.end()) {
        auto&& model_spr = it->second;

        model_spr->GetModel()->SetDir(dir, false);
        model_spr->GetModel()->SetAnimation(anim1, anim2, layers3d, ANIMATION_STAY | ANIMATION_NO_SMOOTH);

        model_spr->DrawToAtlas();

        return model_spr.get();
    }

    auto&& model_spr = dynamic_pointer_cast<ModelSprite>(_sprMngr.LoadSprite(model_name, AtlasType::MapSprites));
    if (!model_spr) {
        return nullptr;
    }

    auto* model = model_spr->GetModel();

    model->SetAnimation(anim1, anim2, layers3d, ANIMATION_STAY | ANIMATION_NO_SMOOTH);
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
