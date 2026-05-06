//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
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

#include "catch_amalgamated.hpp"

#include <iostream>
#include <sstream>

#include "StackTrace.h"

FO_BEGIN_NAMESPACE

namespace
{
    // RAII helper so each section starts with a clean provider regardless of the previous one.
    struct ScopedScriptStackTraceProvider
    {
        explicit ScopedScriptStackTraceProvider(ScriptStackTraceProvider provider) noexcept { SetScriptStackTraceProvider(std::move(provider)); }
        ScopedScriptStackTraceProvider(const ScopedScriptStackTraceProvider&) = delete;
        ScopedScriptStackTraceProvider(ScopedScriptStackTraceProvider&&) noexcept = delete;
        auto operator=(const ScopedScriptStackTraceProvider&) -> ScopedScriptStackTraceProvider& = delete;
        auto operator=(ScopedScriptStackTraceProvider&&) noexcept -> ScopedScriptStackTraceProvider& = delete;
        ~ScopedScriptStackTraceProvider() noexcept { SetScriptStackTraceProvider({}); }
    };

    auto MakeScriptFrame(std::string function, std::string file, uint32_t line) -> StackTraceFrame
    {
        StackTraceFrame frame;
        frame.Type = StackTraceFrame::FrameType::Script;
        frame.Function = std::move(function);
        frame.File = std::move(file);
        frame.Line = line;
        return frame;
    }

    auto MakeLayer(std::initializer_list<StackTraceFrame> frames) -> ScriptStackTraceLayer
    {
        ScriptStackTraceLayer layer;
        layer.ScriptFrames.assign(frames);
        return layer;
    }
}

TEST_CASE("StackTrace")
{
    // Make sure no leaked provider from a prior test pollutes the suite.
    SetScriptStackTraceProvider({});

    SECTION("ProviderRegistrationIsObservable")
    {
        CHECK_FALSE(HasScriptStackTraceProvider());

        SetScriptStackTraceProvider([](std::vector<ScriptStackTraceLayer>&) { });
        CHECK(HasScriptStackTraceProvider());

        SetScriptStackTraceProvider({});
        CHECK_FALSE(HasScriptStackTraceProvider());
    }

    SECTION("SingleLayerCapturesScriptFramesInProvidedOrder")
    {
        const ScopedScriptStackTraceProvider scope([](std::vector<ScriptStackTraceLayer>& layers) {
            layers.push_back(MakeLayer({
                MakeScriptFrame("InnerFunc", "Scripts/Inner.fos", 17),
                MakeScriptFrame("OuterFunc", "Scripts/Outer.fos", 5),
            }));
        });

        const auto st = GetStackTrace();

        REQUIRE(st.ScriptLayers);
        REQUIRE(st.ScriptLayers->size() == 1);

        const auto& frames = (*st.ScriptLayers)[0].ScriptFrames;
        REQUIRE(frames.size() == 2);
        CHECK(frames[0].Function == "InnerFunc");
        CHECK(frames[0].File == "Scripts/Inner.fos");
        CHECK(frames[0].Line == 17);
        CHECK(frames[0].Type == StackTraceFrame::FrameType::Script);
        CHECK(frames[1].Function == "OuterFunc");
    }

    SECTION("MultiContextChainProducesMultipleLayersInnermostFirst")
    {
        // Two layers: child (active) on top, parent below. No native anchors set.
        const ScopedScriptStackTraceProvider scope([](std::vector<ScriptStackTraceLayer>& layers) {
            layers.push_back(MakeLayer({
                MakeScriptFrame("ChildCtx_Top", "Scripts/Child.fos", 42),
                MakeScriptFrame("ChildCtx_Bottom", "Scripts/Child.fos", 1),
            }));
            layers.push_back(MakeLayer({
                MakeScriptFrame("ParentCtx_Frame", "Scripts/Parent.fos", 99),
            }));
        });

        const auto formatted = FormatStackTrace(GetStackTrace());

        const auto child_top_pos = formatted.find("ChildCtx_Top");
        const auto child_bottom_pos = formatted.find("ChildCtx_Bottom");
        const auto parent_pos = formatted.find("ParentCtx_Frame");

        REQUIRE(child_top_pos != std::string::npos);
        REQUIRE(child_bottom_pos != std::string::npos);
        REQUIRE(parent_pos != std::string::npos);
        CHECK(child_top_pos < child_bottom_pos);
        CHECK(child_bottom_pos < parent_pos);
    }

    SECTION("FormatTagsScriptFrames")
    {
        const ScopedScriptStackTraceProvider scope([](std::vector<ScriptStackTraceLayer>& layers) { layers.push_back(MakeLayer({MakeScriptFrame("Boss", "Scripts/Boss.fos", 7)})); });

        const auto formatted = FormatStackTrace(GetStackTrace());

        CHECK(formatted.find("- [Script] Boss (Boss.fos line 7)") != std::string::npos);
    }

    SECTION("ResolveStackTracePlacesScriptBeforeNativeWhenNoBirthAnchor")
    {
        const ScopedScriptStackTraceProvider scope([](std::vector<ScriptStackTraceLayer>& layers) {
            layers.push_back(MakeLayer({
                MakeScriptFrame("ScriptA", "Scripts/A.fos", 1),
                MakeScriptFrame("ScriptB", "Scripts/B.fos", 2),
            }));
        });

        const auto resolved = ResolveStackTrace(GetStackTrace());

        REQUIRE(resolved.size() >= 2);
        CHECK(resolved[0].Type == StackTraceFrame::FrameType::Script);
        CHECK(resolved[1].Type == StackTraceFrame::FrameType::Script);

        for (size_t i = 2; i < resolved.size(); i++) {
            CHECK(resolved[i].Type == StackTraceFrame::FrameType::Native);
        }
    }

    SECTION("MultiLevelInterleavingSplicesNativeBetweenLayers")
    {
        // Synthesize a StackTraceData by hand — a fixed native trace with known PCs and two
        // layers whose BirthNativeFrames anchor INSIDE the native trace. The resolver must
        // produce: child-script, native(0..anchor_child-1), parent-script, native(anchor_child..anchor_parent-1), native-tail(anchor_parent..end).
        StackTraceData st {};
        // Pretend native frames addresses 0xA0, 0xB0, ..., 0xA0 = top, 0x80 = main.
        const std::array<void*, 5> pcs {
            reinterpret_cast<void*>(static_cast<uintptr_t>(0xA0)), // child's native bridge code
            reinterpret_cast<void*>(static_cast<uintptr_t>(0xA1)), // child's Execute()
            reinterpret_cast<void*>(static_cast<uintptr_t>(0xB0)), // parent's native bridge code (= anchor for child layer)
            reinterpret_cast<void*>(static_cast<uintptr_t>(0xB1)), // parent's Execute()         (= anchor for parent layer)
            reinterpret_cast<void*>(static_cast<uintptr_t>(0x80)), // main()
        };
        for (size_t i = 0; i < pcs.size(); i++) {
            st.NativeFrames[i] = pcs[i];
        }
        st.NativeFrameCount = static_cast<uint32_t>(pcs.size());

        ScriptStackTraceLayer child;
        child.ScriptFrames.push_back(MakeScriptFrame("ChildScript", "Scripts/Child.fos", 10));
        child.BirthNativeFrames[0] = reinterpret_cast<void*>(static_cast<uintptr_t>(0xB0));
        child.BirthNativeFrameCount = 1;

        ScriptStackTraceLayer parent;
        parent.ScriptFrames.push_back(MakeScriptFrame("ParentScript", "Scripts/Parent.fos", 20));
        parent.BirthNativeFrames[0] = reinterpret_cast<void*>(static_cast<uintptr_t>(0xB1));
        parent.BirthNativeFrameCount = 1;

        std::vector<ScriptStackTraceLayer> layers;
        layers.push_back(std::move(child));
        layers.push_back(std::move(parent));
        st.ScriptLayers = std::make_shared<const std::vector<ScriptStackTraceLayer>>(std::move(layers));

        const auto resolved = ResolveStackTrace(st);

        // Expected interleaving (most-recent first):
        //   [Native] PC 0xA0               <- deeper than the child layer (the throw chain)
        //   [Native] PC 0xA1
        //   [Script] ChildScript          <- innermost layer (called the natives above)
        //   [Native] PC 0xB0               <- between child anchor and parent anchor
        //   [Script] ParentScript         <- outer layer
        //   [Native] PC 0xB1               <- tail below parent anchor
        //   [Native] PC 0x80
        REQUIRE(resolved.size() == 7);
        CHECK(resolved[0].Type == StackTraceFrame::FrameType::Native);
        CHECK(resolved[1].Type == StackTraceFrame::FrameType::Native);
        CHECK(resolved[2].Type == StackTraceFrame::FrameType::Script);
        CHECK(resolved[2].Function == "ChildScript");
        CHECK(resolved[3].Type == StackTraceFrame::FrameType::Native);
        CHECK(resolved[4].Type == StackTraceFrame::FrameType::Script);
        CHECK(resolved[4].Function == "ParentScript");
        CHECK(resolved[5].Type == StackTraceFrame::FrameType::Native);
        CHECK(resolved[6].Type == StackTraceFrame::FrameType::Native);
    }

    SECTION("LayerWithoutBirthAnchorEmitsScriptOnlyAndDeferNativeToTail")
    {
        // BirthNativeFrameCount == 0: layer is recorded but the resolver can't anchor it in
        // the native trace, so all native frames go after every script layer.
        StackTraceData st {};
        st.NativeFrames[0] = reinterpret_cast<void*>(static_cast<uintptr_t>(0xCAFE));
        st.NativeFrames[1] = reinterpret_cast<void*>(static_cast<uintptr_t>(0xBABE));
        st.NativeFrameCount = 2;

        std::vector<ScriptStackTraceLayer> layers;
        layers.push_back(MakeLayer({MakeScriptFrame("OnlyScript", "Scripts/Only.fos", 1)}));
        // No BirthNativeFrameCount set - left at default 0.
        st.ScriptLayers = std::make_shared<const std::vector<ScriptStackTraceLayer>>(std::move(layers));

        const auto resolved = ResolveStackTrace(st);

        REQUIRE(resolved.size() == 3);
        CHECK(resolved[0].Type == StackTraceFrame::FrameType::Script);
        CHECK(resolved[1].Type == StackTraceFrame::FrameType::Native);
        CHECK(resolved[2].Type == StackTraceFrame::FrameType::Native);
    }

    SECTION("GetStackTraceEntryReturnsFramesByDepth")
    {
        const ScopedScriptStackTraceProvider scope([](std::vector<ScriptStackTraceLayer>& layers) {
            layers.push_back(MakeLayer({
                MakeScriptFrame("DepthZero", "Scripts/Z.fos", 1),
                MakeScriptFrame("DepthOne", "Scripts/O.fos", 2),
            }));
        });

        const auto top = GetStackTraceEntry(0);
        const auto next = GetStackTraceEntry(1);

        REQUIRE(top.has_value());
        REQUIRE(next.has_value());
        CHECK(top->Function == "DepthZero");
        CHECK(next->Function == "DepthOne");
    }

    SECTION("GetStackTraceEntryReturnsNulloptForOutOfRange")
    {
        const ScopedScriptStackTraceProvider scope([](std::vector<ScriptStackTraceLayer>& layers) { layers.push_back(MakeLayer({MakeScriptFrame("Only", "Scripts/Only.fos", 1)})); });

        const auto missing = GetStackTraceEntry(10000);
        CHECK_FALSE(missing.has_value());
    }

    SECTION("FormatStackTraceWithNoFramesReturnsHeaderOnly")
    {
        StackTraceData st {};

        const auto formatted = FormatStackTrace(st);

        CHECK(formatted == "Stack trace (most recent call first):");
    }

    SECTION("SafeWriteStackTraceWritesScriptAndNativeSections")
    {
        StackTraceData st {};
        std::vector<ScriptStackTraceLayer> layers;
        layers.push_back(MakeLayer({
            MakeScriptFrame("FuncA", "/tmp/a.fos", 11),
            MakeScriptFrame("FuncB", "/tmp/b.fos", 22),
        }));
        st.ScriptLayers = std::make_shared<const std::vector<ScriptStackTraceLayer>>(std::move(layers));

        std::ostringstream captured;
        std::streambuf* prev_buf = std::cout.rdbuf(captured.rdbuf());

        SafeWriteStackTrace(st);
        std::cout.rdbuf(prev_buf);

        const std::string log_contents = captured.str();

        CHECK(log_contents.find("Stack trace (most recent call first):\n") == 0);
        CHECK(log_contents.find("- [Script] FuncA (a.fos line 11)\n") != std::string::npos);
        CHECK(log_contents.find("- [Script] FuncB (b.fos line 22)\n") != std::string::npos);
        CHECK(log_contents.ends_with("\n"));
    }

    SECTION("ProviderExceptionsDoNotEscape")
    {
        const ScopedScriptStackTraceProvider scope([](std::vector<ScriptStackTraceLayer>&) {
            // A misbehaving provider must not crash the capture path even if it throws —
            // GetStackTrace defensively swallows the exception so the contract is preserved.
            throw std::runtime_error("provider failure");
        });

        const auto st = GetStackTrace();
        // Capture survived and produced a valid object. ScriptLayers stays null because the
        // provider didn't append anything before throwing.
        CHECK_FALSE(st.ScriptLayers);
    }

    SetScriptStackTraceProvider({});
}

FO_END_NAMESPACE
