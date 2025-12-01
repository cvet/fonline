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

#pragma once

#include "Common.h"

#include "3dStuff.h"
#include "DefaultSprites.h"
#include "ModelSprites.h"
#include "SpriteManager.h"

FO_BEGIN_NAMESPACE();

class ResourceManager final
{
public:
    ResourceManager() = delete;
    ResourceManager(FileSystem& resources, SpriteManager& spr_mngr, AnimationResolver& anim_name_resolver);
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) noexcept = delete;
    auto operator=(const ResourceManager&) = delete;
    auto operator=(ResourceManager&&) noexcept = delete;
    ~ResourceManager() = default;

    [[nodiscard]] auto GetItemDefaultSpr() -> shared_ptr<Sprite>;
    [[nodiscard]] auto GetCritterAnimFrames(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, uint8 dir) -> const SpriteSheet*;
    [[nodiscard]] auto GetCritterDummyFrames() -> const SpriteSheet*;
    [[nodiscard]] auto GetCritterPreviewSpr(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, uint8 dir, const int32* layers3d) -> const Sprite*;
    [[nodiscard]] auto GetSoundNames() const -> const map<string, string>&;

    void IndexFiles();
    void CleanupCritterFrames();

private:
    [[nodiscard]] auto LoadFalloutAnimFrames(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim) -> shared_ptr<SpriteSheet>;
    [[nodiscard]] auto LoadFalloutAnimSubFrames(hstring model_name, uint32 state_anim, uint32 action_anim) -> const SpriteSheet*;
#if FO_ENABLE_3D
    [[nodiscard]] auto GetCritterPreviewModelSpr(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, uint8 dir, const int32* layers3d) -> const ModelSprite*;
#endif

    void FixAnimFramesOffs(SpriteSheet* frames_base, const SpriteSheet* stay_frm_base);
    void FixAnimFramesOffsNext(SpriteSheet* frames_base, const SpriteSheet* stay_frm_base);

    raw_ptr<FileSystem> _resources;
    raw_ptr<SpriteManager> _sprMngr;
    raw_ptr<AnimationResolver> _animNameResolver;
    unordered_map<uint32, shared_ptr<SpriteSheet>> _critterFrames {};
    shared_ptr<SpriteSheet> _critterDummyAnimFrames {};
    shared_ptr<Sprite> _itemHexDummyAnim {};
    map<string, string> _soundNames {};
    bool _nonConstHelper {};
#if FO_ENABLE_3D
    unordered_map<hstring, shared_ptr<ModelSprite>> _critterModels {};
#endif
};

FO_END_NAMESPACE();
