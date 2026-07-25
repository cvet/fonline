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

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

namespace ParticleTests
{
    // Effekseer upstream Dev/Cpp/Test/Resource/Simple_GeneratingPosition1.efkproj (MIT), normalized
    // by Effekseer Editor 1.80.5. Keep this compact source fixture independent of embedding projects.
    inline constexpr string_view SimpleGeneratingPositionProject = R"EFFEKSEER(<?xml version="1.0" encoding="utf-8"?>
<EffekseerProject>
  <Root>
    <Name>Root</Name>
    <Children>
      <Node>
        <CommonValues>
          <MaxGeneration>
            <Value>8</Value>
          </MaxGeneration>
          <Life>
            <Center>10</Center>
            <Max>10</Max>
            <Min>10</Min>
          </Life>
          <Generation>
            <GenerationTime>
              <Center>0.01</Center>
              <Max>0.01</Max>
              <Min>0.01</Min>
            </GenerationTime>
          </Generation>
        </CommonValues>
        <LocationValues>
          <Type>2</Type>
          <Easing>
            <End>
              <Y>
                <Center>10</Center>
                <Max>10</Max>
                <Min>10</Min>
              </Y>
            </End>
          </Easing>
        </LocationValues>
        <ScalingValues>
          <Type>4</Type>
          <SingleEasing>
            <End>
              <Center>9</Center>
              <Max>9</Max>
              <Min>9</Min>
            </End>
            <StartSpeed>30</StartSpeed>
            <EndSpeed>-30</EndSpeed>
          </SingleEasing>
        </ScalingValues>
        <GenerationLocationValues>
          <EffectsRotation>True</EffectsRotation>
          <Type>3</Type>
          <Circle>
            <Type>1</Type>
          </Circle>
        </GenerationLocationValues>
        <RendererCommonValues>
          <ColorTexture>Texture/Splash01.png</ColorTexture>
          <AlphaBlend>2</AlphaBlend>
          <FadeInType>1</FadeInType>
          <FadeIn>
            <Frame>2</Frame>
          </FadeIn>
          <FadeOutType>1</FadeOutType>
          <FadeOut>
            <Frame>10</Frame>
          </FadeOut>
        </RendererCommonValues>
        <DrawingValues>
          <ColorAll>
            <Fixed>
              <G>100</G>
              <B>10</B>
            </Fixed>
          </ColorAll>
          <Sprite>
            <Billboard>1</Billboard>
          </Sprite>
        </DrawingValues>
        <Name>Flash</Name>
        <Children />
      </Node>
    </Children>
  </Root>
  <Dynamic>
    <Inputs>
      <DynamicInput>
        <Input>0</Input>
      </DynamicInput>
      <DynamicInput>
        <Input>0</Input>
      </DynamicInput>
      <DynamicInput>
        <Input>0</Input>
      </DynamicInput>
      <DynamicInput>
        <Input>0</Input>
      </DynamicInput>
    </Inputs>
    <Equations />
  </Dynamic>
  <ProceduralModel>
    <ProceduralModels />
  </ProceduralModel>
  <ToolVersion>1.80.5</ToolVersion>
  <Version>3</Version>
  <StartFrame>0</StartFrame>
  <EndFrame>60</EndFrame>
  <IsLoop>True</IsLoop>
</EffekseerProject>
)EFFEKSEER";

    // Effekseer upstream Dev/Cpp/Test/Resource/Simple_Sprite_FixedYAxis.efk (MIT), exported by
    // Effekseer 1.70e. Keep this tiny cooked fixture self-contained so the reusable engine tests
    // do not depend on an embedding project's resource tree.
    inline constexpr string_view SimpleSpriteFixedYAxisEffectHex = "534b464507000000010000001700000054006500780074007500720065002f005000610072007400690063006c006500300031002e0070006e006700"
                                                                   "000000000000000000000000803fffffffff01000000020000002c000000640000000200000002000000020000000100000000000000000000004600"
                                                                   "0000460000000000803f0000000001000000480000000000a040000000000000a0400000a0c0000000000000a0c00ad7233c000000000ad7233c0ad7"
                                                                   "23bc000000000ad723bc0000000000000000000000000000000000000000000000000000000000000000000000000c00000000000000000000000000"
                                                                   "0000000000000c0000000000803f000080410000803f0000000000000000000000000000000000000000000000000000000000000000000000000200"
                                                                   "000001000000000000000100000000000000000000000000000000000000020000000000000001000000020000000000ffffffff808080ff0000ffff"
                                                                   "ff008080800000000000000000000000803f0000000001000000000000bf000000bf0000003f000000bf000000bf0000003f0000003f0000003f0000"
                                                                   "000000000000";

    // Project-authored with Effekseer Editor 1.80.5 from a minimal six-instance Sprite node:
    // random sphere generation, Clamp sampling, and NormalOrder Z-sort. Only the Z-sort enum is
    // patched below, so all three modes exercise the same modern cooked effect (SKFE version 1810).
    inline constexpr string_view ZSortSpriteEffectHex = "534b464512070000010000001700000054006500780074007500720065002f005000610072007400690063006c006500300031002e0070006e006700"
                                                        "000000000000000000000000000000000000000000000000000000000000040000000000000000000000000000000000000000000000010000000000"
                                                        "00000000803fffffffff00000000000000000000000000000000ffffffff0100000002000000010000000000000064000000ffffffffffffffffffff"
                                                        "ffffffffffffffffffffffffffffffffffffffffffffffffffff06000000020000000200000002000000000000000100000078000000780000000000"
                                                        "803f0000803f0000000000000000010000000100000000000000000000000f000000000000000000000010000000ffffffff00000000000000000000"
                                                        "000004000000000000000000803f00000000000000000000000000000000000000000000000000000000000000000000803f00000000000000000000"
                                                        "000000000000000000000000000000000000000000000000803f00000000000000000000000000000000000000000000000000000000000000000000"
                                                        "803f000000000000000000000000000000000000000000000000000000000000000010000000ffffffff000000000000000000000000000000001000"
                                                        "0000ffffffff0000803f0000803f0000803f00000000010000000000404000004040db0fc94000000000db0fc9400000000000000000000000000000"
                                                        "00000000803fffff7f7f01000000000000000000803f000000000100000000000000000000000000803f0000803f0000000000000000000000000000"
                                                        "00000000000000000000000000000000803f00000000ffffffffffffffffffffffffffffffffffffffffffffffff0100000001000000010000000100"
                                                        "000000000000010000000000000001000000000000000100000000000000010000000000000001000000000000000100000000000000000000000000"
                                                        "00000000000000000000000000000000000000000000ffffffff00000000000000000000000000000000000000000000803f00000000000000000000"
                                                        "00000000000000000000000000000000000002000000000000000000000000000000ffffffff0000000001000000000000bf000000bf0000003f0000"
                                                        "00bf000000bf0000003f0000003f0000003f000000000000000000000000";

    // Effekseer upstream TestData Effects/Performance/ManyRings2 at commit
    // ed93b472428ae0a15e9bf6cd102b7116ddd60050, authored with Editor 1.80.3. This is its
    // self-contained SKFE/1810 cooked payload. MakeModernRingEffect limits the original 4000
    // instances to a requested test count while preserving the Ring node and renderer parameters.
    inline constexpr string_view ModernRingEffectHex = "534b46451207000000000000000000000000000000000000000000000000000000000000000000000400000000000000000000000000000000000000"
                                                       "0000000001000000000000000000803fffffffff00000000000000000000000000000000ffffffff0100000004000000010000000000000064000000"
                                                       "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffa00f00000200000002000000020000000000000001000000"
                                                       "640000006400000017b7d13817b7d13800007ac500007ac5010000000100000000000000000000000f000000000000000100000060000000ffffffff"
                                                       "ffffffffffffffffffffffffffffffffffffffff000020410000204100002041000020c1000020c1000020c100000000000000000000000000000000"
                                                       "000000000000000000000000000000000000000000000000000000000000000004000000000000000000803f00000000000000000000000000000000"
                                                       "000000000000000000000000000000000000803f00000000000000000000000000000000000000000000000000000000000000000000803f00000000"
                                                       "000000000000000000000000000000000000000000000000000000000000803f00000000000000000000000000000000000000000000000000000000"
                                                       "0000000010000000ffffffff0000000000000000000000000000000010000000ffffffff0000803f0000803f0000803f000000000000000000000000"
                                                       "00000000000000000000000000000000000000000000000000000000000000000000803fffff7f7f00000000000000000000803f0000000001000000"
                                                       "00000000000000000000803f0000803f000000000000000000000000000000000000000000000000000000000000803fffffffffffffffffffffffff"
                                                       "ffffffffffffffffffffffffffffffff0100000001000000000000000100000000000000010000000000000001000000000000000100000000000000"
                                                       "01000000000000000100000000000000010000000000000000000000000000000000000000000000000000000000000000000000ffffffff00000000"
                                                       "000000000000000000000000000000000000803f00000000000000000000000000000000000000000000000000000000040000000000000002000000"
                                                       "0000000010000000000000000000b443000000000000004000000000000000000000803f00000000000000000000003f00000000ffffff0000000000"
                                                       "ffffffff00000000ffffff00000000000000000000000000";

    inline auto DecodeHexBytes(string_view hex) -> vector<uint8_t>
    {
        FO_VERIFY_AND_THROW(hex.size() % 2 == 0, "Hex fixture must contain complete bytes", hex.size());

        auto decode_nibble = [](char ch) -> uint8_t {
            if (ch >= '0' && ch <= '9') {
                return numeric_cast<uint8_t>(ch - '0');
            }
            if (ch >= 'a' && ch <= 'f') {
                return numeric_cast<uint8_t>(ch - 'a' + 10);
            }

            FO_VERIFY_AND_THROW(false, "Hex fixture contains an invalid digit", ch);
            return 0;
        };

        vector<uint8_t> result;
        result.reserve(hex.size() / 2);

        for (size_t offset = 0; offset < hex.size(); offset += 2) {
            result.emplace_back(numeric_cast<uint8_t>((decode_nibble(hex[offset]) << 4) | decode_nibble(hex[offset + 1])));
        }

        return result;
    }

    inline auto MakeSimpleSpriteFixedYAxisEffect() -> vector<uint8_t>
    {
        constexpr size_t texture_wrap_offset = 306;

        vector<uint8_t> result = DecodeHexBytes(SimpleSpriteFixedYAxisEffectHex);
        FO_VERIFY_AND_THROW(result.size() > texture_wrap_offset && result[texture_wrap_offset] == 0, "Effekseer test fixture layout changed");

        // The upstream fixture requests Repeat. FOnline atlases require Clamp, so this mirrors the
        // one-field authoring patch documented for the production runtime-smoke copy.
        result[texture_wrap_offset] = 1;
        return result;
    }

    inline auto MakeZSortSpriteEffect(int32_t z_sort) -> vector<uint8_t>
    {
        constexpr size_t fixture_size = 870;
        constexpr size_t z_sort_offset = 550;

        FO_VERIFY_AND_THROW(z_sort >= 0 && z_sort <= 2, "Effekseer Z-sort fixture mode is invalid", z_sort);

        vector<uint8_t> result = DecodeHexBytes(ZSortSpriteEffectHex);
        FO_VERIFY_AND_THROW(result.size() == fixture_size && result[z_sort_offset] == 1 && result[z_sort_offset + 1] == 0 && result[z_sort_offset + 2] == 0 && result[z_sort_offset + 3] == 0, "Effekseer Z-sort fixture layout changed");

        result[z_sort_offset] = numeric_cast<uint8_t>(z_sort);
        return result;
    }

    inline auto MakeModernRingEffect(int32_t max_generation = 1, int32_t z_sort = 0) -> vector<uint8_t>
    {
        constexpr size_t fixture_size = 924;
        constexpr size_t max_generation_offset = 156;
        constexpr size_t z_sort_offset = 580;

        FO_VERIFY_AND_THROW(max_generation > 0 && max_generation <= 4000, "Effekseer Ring fixture generation count is invalid", max_generation);
        FO_VERIFY_AND_THROW(z_sort >= 0 && z_sort <= 2, "Effekseer Ring fixture Z-sort mode is invalid", z_sort);

        vector<uint8_t> result = DecodeHexBytes(ModernRingEffectHex);
        FO_VERIFY_AND_THROW(result.size() == fixture_size && result[max_generation_offset] == 0xa0 && result[max_generation_offset + 1] == 0x0f && result[max_generation_offset + 2] == 0 && result[max_generation_offset + 3] == 0, "Effekseer Ring fixture layout changed");
        FO_VERIFY_AND_THROW(result[z_sort_offset - 8] == 0 && result[z_sort_offset - 7] == 0 && result[z_sort_offset - 6] == 0x80 && result[z_sort_offset - 5] == 0x3f && result[z_sort_offset - 4] == 0xff && result[z_sort_offset - 3] == 0xff && result[z_sort_offset - 2] == 0x7f && result[z_sort_offset - 1] == 0x7f && result[z_sort_offset] == 0 && result[z_sort_offset + 1] == 0 && result[z_sort_offset + 2] == 0 && result[z_sort_offset + 3] == 0, "Effekseer Ring fixture depth layout changed");

        uint32_t encoded_generation = numeric_cast<uint32_t>(max_generation);
        result[max_generation_offset] = numeric_cast<uint8_t>(encoded_generation & 0xff);
        result[max_generation_offset + 1] = numeric_cast<uint8_t>((encoded_generation >> 8) & 0xff);
        result[max_generation_offset + 2] = numeric_cast<uint8_t>((encoded_generation >> 16) & 0xff);
        result[max_generation_offset + 3] = numeric_cast<uint8_t>((encoded_generation >> 24) & 0xff);
        result[z_sort_offset] = numeric_cast<uint8_t>(z_sort);
        return result;
    }
}

FO_END_NAMESPACE
