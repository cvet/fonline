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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

FO_BEGIN_NAMESPACE

enum class AnimFrameFlag : uint32_t
{
    None = 0,
    FirstFrame = 0x01,
    LastFrame = 0x02,
};
static constexpr isize32 DUMMY_SPRITE_SIZE {1, 1};
static constexpr ucolor DUMMY_SPRITE_COLOR {255, 255, 255, 255};

static auto MakeBuiltInDummyAtlasSprite(ptr<SpriteManager> spr_mngr, AtlasType atlas_type) -> shared_ptr<AtlasSprite>
{
    FO_STACK_TRACE_ENTRY();

    auto [atlas, atlas_node, pos] = spr_mngr->GetAtlasMngr()->FindAtlasPlace(atlas_type, DUMMY_SPRITE_SIZE);
    auto tex = atlas->GetTexture();
    const const_span<ucolor> dummy_color {&DUMMY_SPRITE_COLOR, 1};

    tex->UpdateTextureRegion(pos, DUMMY_SPRITE_SIZE, dummy_color);
    tex->UpdateTextureRegion({pos.x, pos.y - 1}, {1, 1}, dummy_color);
    tex->UpdateTextureRegion({pos.x, pos.y + 1}, {1, 1}, dummy_color);

    const ucolor vertical_border[3] = {DUMMY_SPRITE_COLOR, DUMMY_SPRITE_COLOR, DUMMY_SPRITE_COLOR};
    tex->UpdateTextureRegion({pos.x - 1, pos.y - 1}, {1, 3}, {vertical_border, 3});
    tex->UpdateTextureRegion({pos.x + 1, pos.y - 1}, {1, 3}, {vertical_border, 3});

    atlas->GetRenderTarget()->ClearLastPixelPicks();

    frect32 atlas_rect;
    atlas_rect.x = numeric_cast<float32_t>(pos.x) / numeric_cast<float32_t>(atlas->GetSize().width);
    atlas_rect.y = numeric_cast<float32_t>(pos.y) / numeric_cast<float32_t>(atlas->GetSize().height);
    atlas_rect.width = 1.0f / numeric_cast<float32_t>(atlas->GetSize().width);
    atlas_rect.height = 1.0f / numeric_cast<float32_t>(atlas->GetSize().height);

    vector<bool> hit_test_data(1, spr_mngr->CheckHitTest(numeric_cast<int32_t>(DUMMY_SPRITE_COLOR.comp.a)));
    return SafeAlloc::MakeShared<AtlasSprite>(spr_mngr, DUMMY_SPRITE_SIZE, ipos32 {}, atlas, atlas_node, atlas_rect, std::move(hit_test_data));
}

ResourceManager::ResourceManager(ptr<RenderSettings> settings, ptr<FileSystem> resources, ptr<SpriteManager> spr_mngr, ptr<AnimationResolver> anim_name_resolver) :
    _settings {settings},
    _resources {resources},
    _sprMngr {spr_mngr},
    _animNameResolver {anim_name_resolver}
{
    FO_STACK_TRACE_ENTRY();
}

void ResourceManager::IndexFiles()
{
    FO_STACK_TRACE_ENTRY();

    constexpr array<string_view, 3> sound_extensions = {"wav", "acm", "ogg"};

    for (const string_view sound_ext : sound_extensions) {
        const auto sound_files = _resources->FilterFiles(sound_ext);

        for (const auto& file_header : sound_files) {
            _soundNames.emplace(strex(file_header.GetPath()).erase_file_extension().lower(), string(file_header.GetPath()));
        }
    }

    shared_ptr<Sprite> any_spr = !_settings->CritterStubSpriteName.empty() ? _sprMngr->LoadSprite(_settings->CritterStubSpriteName, AtlasType::MapSprites, true) : shared_ptr<Sprite> {};

    if (!any_spr) {
        any_spr = MakeBuiltInDummyAtlasSprite(_sprMngr, AtlasType::MapSprites);
    }

    shared_ptr<AtlasSprite> atlas_spr = dynamic_ptr_cast<AtlasSprite>(std::move(any_spr));
    FO_VERIFY_AND_THROW(atlas_spr, "Missing required atlas sprite");
    _critterDummyAnimFrames = SafeAlloc::MakeShared<SpriteSheet>(_sprMngr, 1, 100, 1);
    _critterDummyAnimFrames->_spr[0] = std::move(atlas_spr);
    FO_VERIFY_AND_THROW(_critterDummyAnimFrames, "Critter dummy animation frames are null");

    _itemHexDummyAnim = !_settings->ItemStubSpriteName.empty() ? _sprMngr->LoadSprite(_settings->ItemStubSpriteName, AtlasType::MapSprites, true) : nullptr;

    if (!_itemHexDummyAnim) {
        _itemHexDummyAnim = MakeBuiltInDummyAtlasSprite(_sprMngr, AtlasType::MapSprites);
    }

    FO_VERIFY_AND_THROW(_itemHexDummyAnim, "Item hex dummy animation is null");
}

auto ResourceManager::GetItemDefaultSpr() -> shared_ptr<Sprite>
{
    FO_STACK_TRACE_ENTRY();

    return _itemHexDummyAnim;
}

auto ResourceManager::GetCritterDummyFrames() -> ptr<const SpriteSheet>
{
    FO_STACK_TRACE_ENTRY();

    return _critterDummyAnimFrames.as_ptr();
}

void ResourceManager::CleanupCritterFrames()
{
    FO_STACK_TRACE_ENTRY();

    _critterFrames.clear();
}

static auto AnimMapId(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim) -> hstring::hash_t
{
    FO_STACK_TRACE_ENTRY();

    const hstring::hash_t parts[4] = {model_name.as_hash(), static_cast<hstring::hash_t>(state_anim), static_cast<hstring::hash_t>(action_anim), static_cast<hstring::hash_t>(1)};
    ptr<const hstring::hash_t> parts_data = parts;
    return HashStorage::DefaultHash(make_span(parts_data, sizeof(parts)));
}

static auto FalloutAnimMapId(hstring model_name, uint32_t state_anim, uint32_t action_anim) -> hstring::hash_t
{
    FO_STACK_TRACE_ENTRY();

    const hstring::hash_t parts[4] = {model_name.as_hash(), numeric_cast<hstring::hash_t>(state_anim), numeric_cast<hstring::hash_t>(action_anim), std::numeric_limits<hstring::hash_t>::max()};
    ptr<const hstring::hash_t> parts_data = parts;
    return HashStorage::DefaultHash(make_span(parts_data, sizeof(parts)));
}

auto ResourceManager::GetCritterAnimFrames(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, mdir dir) -> nptr<const SpriteSheet>
{
    FO_STACK_TRACE_ENTRY();

    // Check already loaded
    const auto id = AnimMapId(model_name, state_anim, action_anim);

    if (const auto it = _critterFrames.find(id); it != _critterFrames.end()) {
        auto nullable_anim = it->second.as_nptr();

        if (nullable_anim) {
            auto anim = nullable_anim.as_ptr();
            auto dir_anim = anim->GetDir(dir);

            if (dir_anim) {
                return dir_anim;
            }

            return anim;
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
            int32_t pass_base = 0;

            while (true) {
                int32_t pass = pass_base;
                uint32_t flags = 0;
                int32_t ox = 0;
                int32_t oy = 0;
                string anim_name;

                if (_animNameResolver->ResolveCritterAnimationFrames(model_name, state_anim, action_anim, pass, flags, ox, oy, anim_name)) {
                    if (!anim_name.empty()) {
                        anim = dynamic_ptr_cast<SpriteSheet>(_sprMngr->LoadSprite(anim_name, AtlasType::MapSprites, true));

                        // Fix by dirs
                        const auto frame_flag = static_cast<AnimFrameFlag>(flags);

                        for (int32_t d = 0; anim && d < anim->_dirCount; d++) {
                            auto nullable_dir_anim = anim->GetDir(hdir(d));
                            FO_VERIFY_AND_THROW(nullable_dir_anim, "Missing animation direction");
                            auto dir_anim = nullable_dir_anim.as_ptr();

                            // Process flags
                            if (flags != 0) {
                                if (IsEnumSet(frame_flag, AnimFrameFlag::FirstFrame) || IsEnumSet(frame_flag, AnimFrameFlag::LastFrame)) {
                                    const auto first = IsEnumSet(frame_flag, AnimFrameFlag::FirstFrame);

                                    // Append offsets
                                    if (!first) {
                                        for (int32_t i = 0; i < dir_anim->GetFramesCount() - 1; i++) {
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
                            ox = oy = 0;

                            if (ox != 0 || oy != 0) {
                                for (int32_t i = 0; i < dir_anim->GetFramesCount(); i++) {
                                    auto spr = dir_anim->GetSpr(i);
                                    bool fixed = false;

                                    for (int32_t j = 0; j < i; j++) {
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

        if (!anim && _animNameResolver->ResolveCritterAnimationSubstitute(base_model_name, base_state_anim, base_action_anim, model_name, state_anim, action_anim)) {
            if (model_name_ != model_name || state_anim != state_anim_ || action_anim != action_anim_) {
                continue;
            }
        }

        // Exit from loop
        break;
    }

    // Store resulted animation indices
    if (anim) {
        for (int32_t d = 0; d < anim->_dirCount; d++) {
            auto nullable_dir_anim = anim->GetDir(hdir(d));
            FO_VERIFY_AND_THROW(nullable_dir_anim, "Missing animation direction");
            auto dir_anim = nullable_dir_anim.as_ptr();
            dir_anim->_stateAnim = state_anim;
            dir_anim->_actionAnim = action_anim;
        }
    }

    auto nullable_anim = anim.as_nptr();

    _critterFrames.emplace(id, std::move(anim));

    if (nullable_anim) {
        auto anim_loaded = nullable_anim.as_ptr();
        auto dir_anim = anim_loaded->GetDir(dir);

        if (dir_anim) {
            return dir_anim;
        }

        return anim_loaded;
    }

    return nullptr;
}

auto ResourceManager::LoadFalloutAnimFrames(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim) -> shared_ptr<SpriteSheet>
{
    FO_STACK_TRACE_ENTRY();

    // Convert from common to fallout specific
    int32_t f_state_anim = 0;
    int32_t f_action_anim = 0;
    int32_t f_state_anim_ex = 0;
    int32_t f_action_anim_ex = 0;
    uint32_t flags = 0;

    if (_animNameResolver->ResolveCritterAnimationFallout(model_name, state_anim, action_anim, f_state_anim, f_action_anim, f_state_anim_ex, f_action_anim_ex, flags)) {
        // Load
        auto nullable_anim = LoadFalloutAnimSubFrames(model_name, f_state_anim, f_action_anim);

        if (!nullable_anim) {
            return nullptr;
        }

        auto anim = nullable_anim.as_ptr();

        // Merge
        if (f_state_anim_ex != 0 && f_action_anim_ex != 0) {
            auto nullable_animex = LoadFalloutAnimSubFrames(model_name, f_state_anim_ex, f_action_anim_ex);

            if (!nullable_animex) {
                return nullptr;
            }

            auto animex = nullable_animex.as_ptr();

            const auto frames_count = anim->GetFramesCount() + animex->GetFramesCount();
            shared_ptr<SpriteSheet> anim_merge_base = SafeAlloc::MakeShared<SpriteSheet>(_sprMngr, frames_count, anim->GetWholeTicks() + animex->GetWholeTicks(), anim->GetDirCount());

            for (int32_t d = 0; d < anim->_dirCount; d++) {
                auto nullable_anim_merge = anim_merge_base->GetDir(hdir(d));
                auto nullable_anim_ = anim->GetDir(hdir(d));
                auto nullable_animex_ = animex->GetDir(hdir(d));
                FO_VERIFY_AND_THROW(nullable_anim_merge, "Missing merged animation direction");
                FO_VERIFY_AND_THROW(nullable_anim_, "Missing base animation direction");
                FO_VERIFY_AND_THROW(nullable_animex_, "Missing extra animation direction");
                auto anim_merge = nullable_anim_merge.as_ptr();
                auto anim_ = nullable_anim_.as_ptr();
                auto animex_ = nullable_animex_.as_ptr();

                for (int32_t i = 0; i < anim_->GetFramesCount(); i++) {
                    anim_merge->_spr[i] = anim_->GetSpr(i)->MakeCopy();
                    anim_merge->_sprOffset[i] = anim_->_sprOffset[i];
                }
                for (int32_t i = 0; i < animex_->GetFramesCount(); i++) {
                    anim_merge->_spr[i + anim_->GetFramesCount()] = animex_->GetSpr(i)->MakeCopy();
                    anim_merge->_sprOffset[i + anim_->GetFramesCount()] = animex_->_sprOffset[i];
                }

                int32_t ox = 0;
                int32_t oy = 0;

                for (int32_t i = 0; i < anim_->GetFramesCount(); i++) {
                    ox += anim_->_sprOffset[i].x;
                    oy += anim_->_sprOffset[i].y;
                }

                anim_merge->_sprOffset[anim_->GetFramesCount()].x -= ox;
                anim_merge->_sprOffset[anim_->GetFramesCount()].x -= oy;
            }

            return anim_merge_base;
        }

        // Clone
        const auto frame_flag = static_cast<AnimFrameFlag>(flags);
        const auto first_or_last_mask = CombineEnum(AnimFrameFlag::FirstFrame, AnimFrameFlag::LastFrame);
        const auto frames_count = !IsEnumSet(frame_flag, first_or_last_mask) ? anim->GetFramesCount() : 1;
        shared_ptr<SpriteSheet> anim_clone_base = SafeAlloc::MakeShared<SpriteSheet>(_sprMngr, frames_count, anim->GetWholeTicks(), anim->GetDirCount());

        for (int32_t d = 0; d < anim->_dirCount; d++) {
            auto nullable_anim_clone = anim_clone_base->GetDir(hdir(d));
            auto nullable_anim_ = anim->GetDir(hdir(d));
            FO_VERIFY_AND_THROW(nullable_anim_clone, "Missing cloned animation direction");
            FO_VERIFY_AND_THROW(nullable_anim_, "Missing base animation direction");
            auto anim_clone = nullable_anim_clone.as_ptr();
            auto anim_ = nullable_anim_.as_ptr();

            if (!IsEnumSet(frame_flag, first_or_last_mask)) {
                for (int32_t i = 0; i < anim_->GetFramesCount(); i++) {
                    anim_clone->_spr[i] = anim_->GetSpr(i)->MakeCopy();
                    anim_clone->_sprOffset[i] = anim_->_sprOffset[i];
                }
            }
            else {
                anim_clone->_spr[0] = anim_->GetSpr(IsEnumSet(frame_flag, AnimFrameFlag::FirstFrame) ? 0 : anim_->GetFramesCount() - 1)->MakeCopy();
                anim_clone->_sprOffset[0] = anim_->_sprOffset[IsEnumSet(frame_flag, AnimFrameFlag::FirstFrame) ? 0 : anim_->GetFramesCount() - 1];

                // Append offsets
                if (IsEnumSet(frame_flag, AnimFrameFlag::LastFrame)) {
                    for (int32_t i = 0; i < anim_->GetFramesCount() - 1; i++) {
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

void ResourceManager::FixAnimFramesOffs(ptr<SpriteSheet> frames_base, nptr<const SpriteSheet> nullable_stay_frm_base)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (!nullable_stay_frm_base) {
        return;
    }

    auto stay_frm_base = nullable_stay_frm_base.as_ptr();

    for (int32_t d = 0; d < stay_frm_base->_dirCount; d++) {
        auto nullable_frames = frames_base->GetDir(hdir(d));
        auto nullable_stay_frm = stay_frm_base->GetDir(hdir(d));
        FO_VERIFY_AND_THROW(nullable_frames, "Missing frames animation direction");
        FO_VERIFY_AND_THROW(nullable_stay_frm, "Missing stay frame animation direction");
        auto frames = nullable_frames.as_ptr();
        auto stay_frm = nullable_stay_frm.as_ptr();
        auto stay_spr = stay_frm->GetSpr(0);

        for (int32_t i = 0; i < frames->GetFramesCount(); i++) {
            auto spr = frames->GetSpr(i);
            spr->SetOffset(spr->GetOffset() + stay_spr->GetOffset());
        }
    }
}

void ResourceManager::FixAnimFramesOffsNext(ptr<SpriteSheet> frames_base, nptr<const SpriteSheet> nullable_stay_frm_base)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (!nullable_stay_frm_base) {
        return;
    }

    auto stay_frm_base = nullable_stay_frm_base.as_ptr();

    for (int32_t d = 0; d < stay_frm_base->_dirCount; d++) {
        auto nullable_frames = frames_base->GetDir(hdir(d));
        auto nullable_stay_frm = stay_frm_base->GetDir(hdir(d));
        FO_VERIFY_AND_THROW(nullable_frames, "Missing frames animation direction");
        FO_VERIFY_AND_THROW(nullable_stay_frm, "Missing stay frame animation direction");
        auto frames = nullable_frames.as_ptr();
        auto stay_frm = nullable_stay_frm.as_ptr();
        ipos32 next_offset;

        for (int32_t i = 0; i < stay_frm->GetFramesCount(); i++) {
            next_offset += stay_frm->_sprOffset[i];
        }

        for (int32_t i = 0; i < frames->GetFramesCount(); i++) {
            auto spr = frames->GetSpr(i);
            spr->SetOffset(spr->GetOffset() + next_offset);
        }
    }
}

auto ResourceManager::LoadFalloutAnimSubFrames(hstring model_name, uint32_t state_anim, uint32_t action_anim) -> nptr<const SpriteSheet>
{
    FO_STACK_TRACE_ENTRY();

#define LOADSPR_ADDOFFS(a1, a2) FixAnimFramesOffs(anim_loaded, LoadFalloutAnimSubFrames(model_name, a1, a2))
#define LOADSPR_ADDOFFS_NEXT(a1, a2) FixAnimFramesOffsNext(anim_loaded, LoadFalloutAnimSubFrames(model_name, a1, a2))

    // Fallout animations
    static constexpr uint32_t ANIM1_FALLOUT_UNARMED = 1;
    static constexpr uint32_t ANIM1_FALLOUT_DEAD = 2;
    static constexpr uint32_t ANIM1_FALLOUT_KNOCKOUT = 3;
    static constexpr uint32_t ANIM1_FALLOUT_KNIFE = 4;
    static constexpr uint32_t ANIM1_FALLOUT_MINIGUN = 12;
    static constexpr uint32_t ANIM1_FALLOUT_ROCKET_LAUNCHER = 13;
    static constexpr uint32_t ANIM1_FALLOUT_AIM = 14;

    static constexpr uint32_t ANIM2_FALLOUT_STAY = 1;
    static constexpr uint32_t ANIM2_FALLOUT_WALK = 2;
    static constexpr uint32_t ANIM2_FALLOUT_SHOW = 3;
    static constexpr uint32_t ANIM2_FALLOUT_PREPARE_WEAPON = 8;
    static constexpr uint32_t ANIM2_FALLOUT_TURNOFF_WEAPON = 9;
    static constexpr uint32_t ANIM2_FALLOUT_SHOOT = 10;
    static constexpr uint32_t ANIM2_FALLOUT_BURST = 11;
    static constexpr uint32_t ANIM2_FALLOUT_FLAME = 12;
    static constexpr uint32_t ANIM2_FALLOUT_KNOCK_FRONT = 1; // Only with ANIM1_FALLOUT_DEAD
    static constexpr uint32_t ANIM2_FALLOUT_KNOCK_BACK = 2;
    static constexpr uint32_t ANIM2_FALLOUT_STANDUP_BACK = 8; // Only with ANIM1_FALLOUT_KNOCKOUT
    static constexpr uint32_t ANIM2_FALLOUT_RUN = 20;
    static constexpr uint32_t ANIM2_FALLOUT_DEAD_FRONT = 1; // Only with ANIM1_FALLOUT_DEAD
    static constexpr uint32_t ANIM2_FALLOUT_DEAD_BACK = 2;
    static constexpr uint32_t ANIM2_FALLOUT_DEAD_FRONT2 = 15;
    static constexpr uint32_t ANIM2_FALLOUT_DEAD_BACK2 = 16;

    const auto it = _critterFrames.find(FalloutAnimMapId(model_name, state_anim, action_anim));
    if (it != _critterFrames.end()) {
        return it->second.as_nptr();
    }

    // Try load fofrm
    static constexpr char FRM_IND[] = "_abcdefghijklmnopqrstuvwxyz0123456789";
    FO_VERIFY_AND_THROW(state_anim < sizeof(FRM_IND), "Critter frame state animation index cannot be encoded in a FOFRM suffix", model_name, state_anim, sizeof(FRM_IND));
    FO_VERIFY_AND_THROW(action_anim < sizeof(FRM_IND), "Critter frame action animation index cannot be encoded in a FOFRM suffix", model_name, action_anim, sizeof(FRM_IND));

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

    auto nullable_anim = _critterFrames.emplace(FalloutAnimMapId(model_name, state_anim, action_anim), std::move(anim)).first->second.as_nptr();

    if (!nullable_anim) {
        return nullptr;
    }

    auto anim_loaded = nullable_anim.as_ptr();

    if (state_anim == ANIM1_FALLOUT_AIM) {
        return nullable_anim; // Aim, 'N'
    }

    // Empty offsets
    if (state_anim == ANIM1_FALLOUT_UNARMED) {
        if (action_anim == ANIM2_FALLOUT_STAY || action_anim == ANIM2_FALLOUT_WALK || action_anim == ANIM2_FALLOUT_RUN) {
            return nullable_anim;
        }
        LOADSPR_ADDOFFS(ANIM1_FALLOUT_UNARMED, ANIM2_FALLOUT_STAY);
    }

    // Weapon offsets
    if (state_anim >= ANIM1_FALLOUT_KNIFE && state_anim <= ANIM1_FALLOUT_ROCKET_LAUNCHER) {
        if (action_anim == ANIM2_FALLOUT_SHOW) {
            LOADSPR_ADDOFFS(ANIM1_FALLOUT_UNARMED, ANIM2_FALLOUT_STAY);
        }
        else if (action_anim == ANIM2_FALLOUT_WALK) {
            return nullable_anim;
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

    return nullable_anim;

#undef LOADSPR_ADDOFFS
#undef LOADSPR_ADDOFFS_NEXT
}

auto ResourceManager::GetCritterPreviewSpr(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, mdir dir, nptr<const int32_t> layers3d) -> ptr<const Sprite>
{
    FO_STACK_TRACE_ENTRY();

    const string ext = strex(model_name).get_file_extension();

    if (ext != "fo3d") {
        auto nullable_frames = GetCritterAnimFrames(model_name, state_anim, action_anim, dir);

        if (nullable_frames) {
            return nullable_frames.as_ptr();
        }

        return _critterDummyAnimFrames.as_ptr();
    }
    else {
#if FO_ENABLE_3D
        auto nullable_model_spr = GetCritterPreviewModelSpr(model_name, state_anim, action_anim, dir, layers3d);

        if (nullable_model_spr) {
            return nullable_model_spr.as_ptr();
        }

        return _critterDummyAnimFrames.as_ptr();
#else
        ignore_unused(layers3d);
        throw NotEnabled3DException("3D submodule not enabled");
#endif
    }
}

#if FO_ENABLE_3D
auto ResourceManager::GetCritterPreviewModelSpr(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, mdir dir, nptr<const int32_t> layers3d) -> nptr<const ModelSprite>
{
    FO_STACK_TRACE_ENTRY();

    if (const auto it = _critterModels.find(model_name); it != _critterModels.end()) {
        auto& model_spr = it->second;

        model_spr->GetModel()->SetDir(dir, false);
        model_spr->GetModel()->PlayAnim(state_anim, action_anim, layers3d, 0.0f, CombineEnum(ModelAnimFlags::Freeze, ModelAnimFlags::NoSmooth));

        model_spr->DrawToAtlas();

        return model_spr.as_nptr();
    }

    shared_ptr<Sprite> loaded_spr = _sprMngr->LoadSprite(model_name, AtlasType::MapSprites);
    shared_ptr<ModelSprite> model_spr = dynamic_ptr_cast<ModelSprite>(std::move(loaded_spr));

    if (!model_spr) {
        return nullptr;
    }

    auto model = model_spr->GetModel();

    model->PlayAnim(state_anim, action_anim, layers3d, 0.0f, CombineEnum(ModelAnimFlags::Freeze, ModelAnimFlags::NoSmooth));
    model->SetDir(dir, false);
    model->PrewarmParticles();
    model->StartMeshGeneration();

    model_spr->DrawToAtlas();

    return _critterModels.emplace(model_name, std::move(model_spr)).first->second.as_nptr();
}
#endif

auto ResourceManager::GetSoundNames() const -> const map<string, string>&
{
    FO_STACK_TRACE_ENTRY();

    return _soundNames;
}

FO_END_NAMESPACE
