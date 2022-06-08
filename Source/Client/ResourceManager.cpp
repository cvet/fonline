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

ResourceManager::ResourceManager(FileSystem& file_sys, SpriteManager& spr_mngr, AnimationResolver& anim_name_resolver, NameResolver& name_resolver) : _fileSys {file_sys}, _sprMngr {spr_mngr}, _animNameResolver {anim_name_resolver}, _nameResolver {name_resolver}
{
}

void ResourceManager::IndexFiles()
{
    {
        auto allFiles = _fileSys.FilterFiles("", "", true);
        while (allFiles.MoveNext()) {
            auto file_header = allFiles.GetCurFileHeader();
            const auto h1 = _nameResolver.ToHashedString(file_header.GetPath());
            UNUSED_VARIABLE(h1);
            const auto h2 = _nameResolver.ToHashedString(file_header.GetName());
            UNUSED_VARIABLE(h2);
        }
    }

    for (const auto* splash_ext : {"rix", "png", "jpg"}) {
        auto splashes = _fileSys.FilterFiles(splash_ext, "Splash/", true);
        while (splashes.MoveNext()) {
            auto file_header = splashes.GetCurFileHeader();
            if (std::find(_splashNames.begin(), _splashNames.end(), file_header.GetPath()) == _splashNames.end()) {
                _splashNames.emplace_back(file_header.GetPath());
            }
        }
    }

    for (const auto* sound_ext : {"wav", "acm", "ogg"}) {
        auto sounds = _fileSys.FilterFiles(sound_ext, "", true);
        while (sounds.MoveNext()) {
            auto file_header = sounds.GetCurFileHeader();
            _soundNames.emplace(_str("{}", file_header.GetName()).lower().str(), file_header.GetPath());
        }
    }
}

void ResourceManager::FreeResources(AtlasType atlas_type)
{
    RUNTIME_ASSERT(atlas_type == AtlasType::Static || atlas_type == AtlasType::Dynamic);

    _sprMngr.DestroyAtlases(atlas_type);

    for (auto it = _loadedAnims.begin(); it != _loadedAnims.end();) {
        if (it->second.ResType == atlas_type) {
            _sprMngr.DestroyAnyFrames(it->second.Anim);
            it = _loadedAnims.erase(it);
        }
        else {
            ++it;
        }
    }

    if (atlas_type == AtlasType::Static) {
        _sprMngr.ClearFonts();
    }

    if (atlas_type == AtlasType::Dynamic) {
        for (auto& [fst, snd] : _critterFrames) {
            _sprMngr.DestroyAnyFrames(snd);
        }
        _critterFrames.clear();
    }
}

void ResourceManager::ReinitializeDynamicAtlas()
{
    FreeResources(AtlasType::Dynamic);
    _sprMngr.PushAtlasType(AtlasType::Dynamic);
    _sprMngr.InitializeEgg("TransparentEgg.png");
    _sprMngr.DestroyAnyFrames(CritterDefaultAnim);
    _sprMngr.DestroyAnyFrames(ItemHexDefaultAnim);
    CritterDefaultAnim = _sprMngr.LoadAnimation("CritterStub.png", true, false);
    ItemHexDefaultAnim = _sprMngr.LoadAnimation("ItemStub.png", true, false);
    _sprMngr.PopAtlasType();
}

auto ResourceManager::GetAnim(hstring name, AtlasType atlas_type) -> AnyFrames*
{
    // Find already loaded
    const auto it = _loadedAnims.find(name);
    if (it != _loadedAnims.end()) {
        return it->second.Anim;
    }

    _sprMngr.PushAtlasType(atlas_type);
    auto* anim = _sprMngr.LoadAnimation(name, false, true);
    _sprMngr.PopAtlasType();

    if (anim == nullptr) {
        return nullptr;
    }

    anim->Name = name;

    _loadedAnims.insert(std::make_pair(name, LoadedAnim {atlas_type, anim}));
    return anim;
}

static auto AnimMapId(hstring model_name, uint anim1, uint anim2, bool is_fallout) -> uint
{
    const uint dw[4] = {model_name.as_uint(), anim1, anim2, is_fallout ? static_cast<uint>(-1) : 1};
    return Hashing::MurmurHash2(dw, sizeof(dw));
}

auto ResourceManager::GetCritterAnim(hstring model_name, uint anim1, uint anim2, uchar dir) -> AnyFrames*
{
    // Make animation id
    auto id = AnimMapId(model_name, anim1, anim2, false);

    // Check already loaded
    if (const auto it = _critterFrames.find(id); it != _critterFrames.end()) {
        return it->second != nullptr ? it->second->GetDir(dir) : nullptr;
    }

    // Process loading
    const auto anim1_base = anim1;
    const auto anim2_base = anim2;
    AnyFrames* anim = nullptr;
    while (true) {
        // Load
        if (!!model_name && _str(model_name).startsWith("art/critters/")) {
            // Hardcoded
            anim = LoadFalloutAnim(model_name, anim1, anim2);
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
                        _sprMngr.PushAtlasType(AtlasType::Dynamic);
                        anim = _sprMngr.LoadAnimation(str, false, true);
                        _sprMngr.PopAtlasType();

                        // Fix by dirs
                        for (auto d = 0; anim != nullptr && d < anim->DirCount; d++) {
                            auto* dir_anim = anim->GetDir(d);

                            // Process flags
                            if (flags != 0u) {
                                if (IsBitSet(flags, ANIM_FLAG_FIRST_FRAME) || IsBitSet(flags, ANIM_FLAG_LAST_FRAME)) {
                                    const auto first = IsBitSet(flags, ANIM_FLAG_FIRST_FRAME);

                                    // Append offsets
                                    if (!first) {
                                        for (uint i = 0; i < dir_anim->CntFrm - 1; i++) {
                                            dir_anim->NextX[dir_anim->CntFrm - 1] = static_cast<short>(dir_anim->NextX[dir_anim->CntFrm - 1] + dir_anim->NextX[i]);
                                            dir_anim->NextY[dir_anim->CntFrm - 1] = static_cast<short>(dir_anim->NextY[dir_anim->CntFrm - 1] + dir_anim->NextY[i]);
                                        }
                                    }

                                    // Change size
                                    dir_anim->Ind[0] = first ? dir_anim->Ind[0] : dir_anim->Ind[dir_anim->CntFrm - 1];
                                    dir_anim->NextX[0] = first ? dir_anim->NextX[0] : dir_anim->NextX[dir_anim->CntFrm - 1];
                                    dir_anim->NextY[0] = first ? dir_anim->NextY[0] : dir_anim->NextY[dir_anim->CntFrm - 1];
                                    dir_anim->CntFrm = 1;
                                }
                            }

                            // Add offsets
                            ox = oy = 0; // Todo: why I disable offset adding?
                            if (ox != 0 || oy != 0) {
                                for (uint i = 0; i < dir_anim->CntFrm; i++) {
                                    const auto spr_id = dir_anim->Ind[i];
                                    auto fixed = false;
                                    for (uint j = 0; j < i; j++) {
                                        if (dir_anim->Ind[j] == spr_id) {
                                            fixed = true;
                                            break;
                                        }
                                    }
                                    if (!fixed) {
                                        auto* si = _sprMngr.GetSpriteInfoForEditing(spr_id);
                                        si->OffsX += ox;
                                        si->OffsY += oy;
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
        for (auto d = 0; d < anim->DirCount; d++) {
            anim->GetDir(d)->Anim1 = anim1;
            anim->GetDir(d)->Anim2 = anim2;
        }
    }

    // Store
    _critterFrames.insert(std::make_pair(id, anim));
    return anim != nullptr ? anim->GetDir(dir) : nullptr;
}

auto ResourceManager::LoadFalloutAnim(hstring model_name, uint anim1, uint anim2) -> AnyFrames*
{
    // Convert from common to fallout specific
    uint anim1_ex = 0;
    uint anim2_ex = 0;
    uint flags = 0;
    if (_animNameResolver.ResolveCritterAnimationFallout(model_name, anim1, anim2, anim1_ex, anim2_ex, flags)) {
        // Load
        auto* anim = LoadFalloutAnimSpr(model_name, anim1, anim2);
        if (anim == nullptr) {
            return nullptr;
        }

        // Merge
        if (anim1_ex != 0u && anim2_ex != 0u) {
            auto* animex = LoadFalloutAnimSpr(model_name, anim1_ex, anim2_ex);
            if (animex == nullptr) {
                return nullptr;
            }

            auto* anim_merge_base = _sprMngr.CreateAnyFrames(anim->CntFrm + animex->CntFrm, anim->Ticks + animex->Ticks);
            for (auto d = 0; d < anim->DirCount; d++) {
                if (d == 1) {
                    _sprMngr.CreateAnyFramesDirAnims(anim_merge_base, anim->DirCount);
                }

                auto* anim_merge = anim_merge_base->GetDir(d);
                const auto* anim_ = anim->GetDir(d);
                const auto* animex_ = animex->GetDir(d);

                std::memcpy(anim_merge->Ind, anim_->Ind, anim_->CntFrm * sizeof(uint));
                std::memcpy(anim_merge->NextX, anim_->NextX, anim_->CntFrm * sizeof(short));
                std::memcpy(anim_merge->NextY, anim_->NextY, anim_->CntFrm * sizeof(short));
                std::memcpy(anim_merge->Ind + anim_->CntFrm, animex_->Ind, animex_->CntFrm * sizeof(uint));
                std::memcpy(anim_merge->NextX + anim_->CntFrm, animex_->NextX, animex_->CntFrm * sizeof(short));
                std::memcpy(anim_merge->NextY + anim_->CntFrm, animex_->NextY, animex_->CntFrm * sizeof(short));

                short ox = 0;
                short oy = 0;
                for (uint i = 0; i < anim_->CntFrm; i++) {
                    ox = static_cast<short>(ox + anim_->NextX[i]);
                    oy = static_cast<short>(oy + anim_->NextY[i]);
                }

                anim_merge->NextX[anim_->CntFrm] = static_cast<short>(anim_merge->NextX[anim_->CntFrm] - ox);
                anim_merge->NextY[anim_->CntFrm] = static_cast<short>(anim_merge->NextY[anim_->CntFrm] - oy);
            }
            return anim_merge_base;
        }

        // Clone
        if (anim != nullptr) {
            auto* anim_clone_base = _sprMngr.CreateAnyFrames(!IsBitSet(flags, ANIM_FLAG_FIRST_FRAME | ANIM_FLAG_LAST_FRAME) ? anim->CntFrm : 1, anim->Ticks);
            for (auto d = 0; d < anim->DirCount; d++) {
                if (d == 1) {
                    _sprMngr.CreateAnyFramesDirAnims(anim_clone_base, anim->DirCount);
                }

                auto* anim_clone = anim_clone_base->GetDir(d);
                const auto* anim_ = anim->GetDir(d);

                if (!IsBitSet(flags, ANIM_FLAG_FIRST_FRAME | ANIM_FLAG_LAST_FRAME)) {
                    std::memcpy(anim_clone->Ind, anim_->Ind, anim_->CntFrm * sizeof(uint));
                    std::memcpy(anim_clone->NextX, anim_->NextX, anim_->CntFrm * sizeof(short));
                    std::memcpy(anim_clone->NextY, anim_->NextY, anim_->CntFrm * sizeof(short));
                }
                else {
                    anim_clone->Ind[0] = anim_->Ind[IsBitSet(flags, ANIM_FLAG_FIRST_FRAME) ? 0 : anim_->CntFrm - 1];
                    anim_clone->NextX[0] = anim_->NextX[IsBitSet(flags, ANIM_FLAG_FIRST_FRAME) ? 0 : anim_->CntFrm - 1];
                    anim_clone->NextY[0] = anim_->NextY[IsBitSet(flags, ANIM_FLAG_FIRST_FRAME) ? 0 : anim_->CntFrm - 1];

                    // Append offsets
                    if (IsBitSet(flags, ANIM_FLAG_LAST_FRAME)) {
                        for (uint i = 0; i < anim_->CntFrm - 1; i++) {
                            anim_clone->NextX[0] = static_cast<short>(anim_clone->NextX[0] + anim_->NextX[i]);
                            anim_clone->NextY[0] = static_cast<short>(anim_clone->NextY[0] + anim_->NextY[i]);
                        }
                    }
                }
            }
            return anim_clone_base;
        }
        return nullptr;
    }
    return nullptr;
}

void ResourceManager::FixAnimOffs(AnyFrames* frames_base, AnyFrames* stay_frm_base)
{
    NON_CONST_METHOD_HINT();

    if (stay_frm_base == nullptr) {
        return;
    }

    for (auto d = 0; d < stay_frm_base->DirCount; d++) {
        const auto* frames = frames_base->GetDir(d);
        const auto* stay_frm = stay_frm_base->GetDir(d);

        const auto* stay_si = _sprMngr.GetSpriteInfo(stay_frm->Ind[0]);
        if (stay_si == nullptr) {
            return;
        }

        for (uint i = 0; i < frames->CntFrm; i++) {
            auto* si = _sprMngr.GetSpriteInfoForEditing(frames->Ind[i]);
            if (si == nullptr) {
                continue;
            }

            si->OffsX = static_cast<short>(si->OffsX + stay_si->OffsX);
            si->OffsY = static_cast<short>(si->OffsY + stay_si->OffsY);
        }
    }
}

void ResourceManager::FixAnimOffsNext(AnyFrames* frames_base, AnyFrames* stay_frm_base)
{
    NON_CONST_METHOD_HINT();

    if (stay_frm_base == nullptr) {
        return;
    }

    for (auto d = 0; d < stay_frm_base->DirCount; d++) {
        auto* frames = frames_base->GetDir(d);
        auto* stay_frm = stay_frm_base->GetDir(d);

        const auto* stay_si = _sprMngr.GetSpriteInfo(stay_frm->Ind[0]);
        if (stay_si == nullptr) {
            return;
        }

        short ox = 0;
        short oy = 0;
        for (uint i = 0; i < stay_frm->CntFrm; i++) {
            ox = static_cast<short>(stay_frm->NextX[i] + ox);
            oy = static_cast<short>(stay_frm->NextY[i] + oy);
        }

        for (uint i = 0; i < frames->CntFrm; i++) {
            auto* si = _sprMngr.GetSpriteInfoForEditing(frames->Ind[i]);
            if (si == nullptr) {
                continue;
            }

            si->OffsX = static_cast<short>(si->OffsX + ox);
            si->OffsY = static_cast<short>(si->OffsY + oy);
        }
    }
}

auto ResourceManager::LoadFalloutAnimSpr(hstring model_name, uint anim1, uint anim2) -> AnyFrames*
{
#define LOADSPR_ADDOFFS(a1, a2) FixAnimOffs(frames, LoadFalloutAnimSpr(model_name, a1, a2))
#define LOADSPR_ADDOFFS_NEXT(a1, a2) FixAnimOffsNext(frames, LoadFalloutAnimSpr(model_name, a1, a2))

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
        return it->second;
    }

    // Load file
    static char frm_ind[] = "_abcdefghijklmnopqrstuvwxyz0123456789";
    _sprMngr.PushAtlasType(AtlasType::Dynamic);

    // Try load fofrm
    string spr_name = _str("{}{}{}.fofrm", model_name, frm_ind[anim1], frm_ind[anim2]);
    auto* frames = _sprMngr.LoadAnimation(spr_name, false, false);

    // Try load fallout frames
    if (frames == nullptr) {
        spr_name = _str("{}{}{}.frm", model_name, frm_ind[anim1], frm_ind[anim2]);
        frames = _sprMngr.LoadAnimation(spr_name, false, true);
    }
    _sprMngr.PopAtlasType();

    _critterFrames.insert(std::make_pair(AnimMapId(model_name, anim1, anim2, true), frames));
    if (frames == nullptr) {
        return nullptr;
    }

    if (anim1 == ANIM1_FALLOUT_AIM) {
        return frames; // Aim, 'N'
    }

    // Empty offsets
    if (anim1 == ANIM1_FALLOUT_UNARMED) {
        if (anim2 == ANIM2_FALLOUT_STAY || anim2 == ANIM2_FALLOUT_WALK || anim2 == ANIM2_FALLOUT_RUN) {
            return frames;
        }
        LOADSPR_ADDOFFS(ANIM1_FALLOUT_UNARMED, ANIM2_FALLOUT_STAY);
    }

    // Weapon offsets
    if (anim1 >= ANIM1_FALLOUT_KNIFE && anim1 <= ANIM1_FALLOUT_ROCKET_LAUNCHER) {
        if (anim2 == ANIM2_FALLOUT_SHOW) {
            LOADSPR_ADDOFFS(ANIM1_FALLOUT_UNARMED, ANIM2_FALLOUT_STAY);
        }
        else if (anim2 == ANIM2_FALLOUT_WALK) {
            return frames;
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
        uchar anim2_ = ANIM2_FALLOUT_KNOCK_FRONT;
        if (anim2 == ANIM2_FALLOUT_STANDUP_BACK) {
            anim2_ = ANIM2_FALLOUT_KNOCK_BACK;
        }
        LOADSPR_ADDOFFS(ANIM1_FALLOUT_DEAD, anim2_);
        LOADSPR_ADDOFFS_NEXT(ANIM1_FALLOUT_DEAD, anim2_);
    }

    return frames;

#undef LOADSPR_ADDOFFS
#undef LOADSPR_ADDOFFS_NEXT
}

#if FO_ENABLE_3D
auto ResourceManager::GetCritterModel(hstring model_name, uint anim1, uint anim2, uchar dir, int* layers3d) -> ModelInstance*
{
    if (_critterModels.count(model_name) != 0u) {
        _critterModels[model_name]->SetDir(dir);
        _critterModels[model_name]->SetAnimation(anim1, anim2, layers3d, ANIMATION_STAY | ANIMATION_NO_SMOOTH);
        return _critterModels[model_name];
    }

    _sprMngr.PushAtlasType(AtlasType::Dynamic);
    auto* model = _sprMngr.LoadModel(model_name, true);
    _sprMngr.PopAtlasType();

    if (model == nullptr) {
        return nullptr;
    }

    _critterModels[model_name] = model;

    model->SetAnimation(anim1, anim2, layers3d, ANIMATION_STAY | ANIMATION_NO_SMOOTH);
    model->SetDir(dir);
    model->StartMeshGeneration();
    return model;
}
#endif

auto ResourceManager::GetCritterSprId(hstring model_name, uint anim1, uint anim2, uchar dir, int* layers3d /* = NULL */) -> uint
{
    const string ext = _str().getFileExtension();
    if (ext != "fo3d") {
        const auto* anim = GetCritterAnim(model_name, anim1, anim2, dir);
        return anim != nullptr ? anim->GetSprId(0) : 0u;
    }
    else {
#if FO_ENABLE_3D
        const auto* model = GetCritterModel(model_name, anim1, anim2, dir, layers3d);
        return model != nullptr ? model->SprId : 0u;
#else
        throw NotEnabled3DException("3D submodule not enabled");
#endif
    }
}

auto ResourceManager::GetRandomSplash() -> AnyFrames*
{
    if (_splashNames.empty()) {
        return nullptr;
    }

    const auto rnd = GenericUtils::Random(0, static_cast<int>(_splashNames.size()) - 1);

    _sprMngr.PushAtlasType(AtlasType::Splash, true);
    _splash = _sprMngr.ReloadAnimation(_splash, _splashNames[rnd]);
    _sprMngr.PopAtlasType();

    return _splash;
}
