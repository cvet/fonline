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
#include "SpriteManager.h"

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

    [[nodiscard]] auto GetIfaceAnim(hstring name) -> const SpriteSheet* { return GetAnim(name, AtlasType::IfaceSprites); }
    [[nodiscard]] auto GetMapAnim(hstring name) -> const SpriteSheet* { return GetAnim(name, AtlasType::MapSprites); }
    [[nodiscard]] auto GetCritterAnim(hstring model_name, uint anim1, uint anim2, uint8 dir) -> const SpriteSheet*;
    [[nodiscard]] auto GetCritterSpr(hstring model_name, uint anim1, uint anim2, uint8 dir, int* layers3d) -> const Sprite*;
    [[nodiscard]] auto GetRandomSplash() -> const SpriteSheet*;
    [[nodiscard]] auto GetSoundNames() const -> const map<string, string>& { return _soundNames; }
#if FO_ENABLE_3D
    [[nodiscard]] auto GetCritterModelSpr(hstring model_name, uint anim1, uint anim2, uint8 dir, int* layers3d) -> const ModelSprite*;
#endif

    void IndexFiles();
    void CleanupMapSprites();

    unique_ptr<SpriteSheet> ItemHexDefaultAnim {};
    unique_ptr<SpriteSheet> CritterDefaultAnim {};

private:
    [[nodiscard]] auto GetAnim(hstring name, AtlasType atlas_type) -> const SpriteSheet*;
    [[nodiscard]] auto LoadFalloutAnim(hstring model_name, uint anim1, uint anim2) -> unique_ptr<SpriteSheet>;
    [[nodiscard]] auto LoadFalloutAnimSpr(hstring model_name, uint anim1, uint anim2) -> const SpriteSheet*;

    void FixAnimOffs(SpriteSheet* frames_base, const SpriteSheet* stay_frm_base);
    void FixAnimOffsNext(SpriteSheet* frames_base, const SpriteSheet* stay_frm_base);

    FileSystem& _resources;
    SpriteManager& _sprMngr;
    AnimationResolver& _animNameResolver;
    unordered_map<hstring, unique_ptr<SpriteSheet>> _loadedAnims {};
    unordered_map<uint, unique_ptr<SpriteSheet>> _critterFrames {};
    vector<string> _splashNames {};
    map<string, string> _soundNames {};
    unique_ptr<SpriteSheet> _splash {};
    bool _nonConstHelper {};
#if FO_ENABLE_3D
    unordered_map<hstring, unique_del_ptr<ModelSprite>> _critterModels {};
#endif
};
