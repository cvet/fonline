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

#include "BaseLogging.h"
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
    ClearResolvedStackTraceCache();

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
        ScopedScriptStackTraceProvider scope([](std::vector<ScriptStackTraceLayer>& layers) {
            layers.push_back(MakeLayer({
                MakeScriptFrame("InnerFunc", "Scripts/Inner.fos", 17),
                MakeScriptFrame("OuterFunc", "Scripts/Outer.fos", 5),
            }));
        });

        auto st = GetStackTrace();

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
        ScopedScriptStackTraceProvider scope([](std::vector<ScriptStackTraceLayer>& layers) {
            layers.push_back(MakeLayer({
                MakeScriptFrame("ChildCtx_Top", "Scripts/Child.fos", 42),
                MakeScriptFrame("ChildCtx_Bottom", "Scripts/Child.fos", 1),
            }));
            layers.push_back(MakeLayer({
                MakeScriptFrame("ParentCtx_Frame", "Scripts/Parent.fos", 99),
            }));
        });

        auto formatted = FormatStackTrace(GetStackTrace());

        auto child_top_pos = formatted.find("ChildCtx_Top");
        auto child_bottom_pos = formatted.find("ChildCtx_Bottom");
        auto parent_pos = formatted.find("ParentCtx_Frame");

        REQUIRE(child_top_pos != std::string::npos);
        REQUIRE(child_bottom_pos != std::string::npos);
        REQUIRE(parent_pos != std::string::npos);
        CHECK(child_top_pos < child_bottom_pos);
        CHECK(child_bottom_pos < parent_pos);
    }

    SECTION("FormatTagsScriptFrames")
    {
        ScopedScriptStackTraceProvider scope([](std::vector<ScriptStackTraceLayer>& layers) { layers.push_back(MakeLayer({MakeScriptFrame("Boss", "Scripts/Boss.fos", 7)})); });

        auto formatted = FormatStackTrace(GetStackTrace());

        CHECK(formatted.find("- [Script] Boss (Boss.fos line 7)") != std::string::npos);
    }

    SECTION("ResolveStackTracePlacesScriptBeforeNativeWhenNoBirthAnchor")
    {
        ScopedScriptStackTraceProvider scope([](std::vector<ScriptStackTraceLayer>& layers) {
            layers.push_back(MakeLayer({
                MakeScriptFrame("ScriptA", "Scripts/A.fos", 1),
                MakeScriptFrame("ScriptB", "Scripts/B.fos", 2),
            }));
        });

        auto resolved = ResolveStackTrace(GetStackTrace());

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
        // layers whose BirthNativeFrames bottom-align with the trace. The resolver must
        // produce: native(0..anchor_child), child-script, native(anchor_child..anchor_parent),
        // parent-script, native-tail(anchor_parent..end).
        //
        // Bottom alignment matters: in production, BirthNativeFrames is captured at
        // RequestContext and shares its bottom with any later trace taken inside the
        // launched Execute(). Anchor = first native trace frame above the matched bottom.
        StackTraceData st {};
        // Pretend native frames addresses 0xA0, 0xB0, ..., 0xA0 = top, 0x80 = main.
        std::array<NativeStackFrameAddress, 5> pcs {
            static_cast<NativeStackFrameAddress>(0xA0), // child's native bridge code
            static_cast<NativeStackFrameAddress>(0xA1), // child's Execute()
            static_cast<NativeStackFrameAddress>(0xB0), // parent's native bridge code (= anchor for child layer)
            static_cast<NativeStackFrameAddress>(0xB1), // parent's Execute()         (= anchor for parent layer)
            static_cast<NativeStackFrameAddress>(0x80), // main()
        };
        for (size_t i = 0; i < pcs.size(); i++) {
            st.NativeFrames[i] = pcs[i];
        }
        st.NativeFrameCount = static_cast<uint32_t>(pcs.size());

        ScriptStackTraceLayer child;
        child.ScriptFrames.push_back(MakeScriptFrame("ChildScript", "Scripts/Child.fos", 10));
        // Child layer was launched at the 0xB0 frame; its birth stack matches the trace
        // bottom from 0xB0 down through main.
        child.BirthNativeFrames[0] = static_cast<NativeStackFrameAddress>(0xB0);
        child.BirthNativeFrames[1] = static_cast<NativeStackFrameAddress>(0xB1);
        child.BirthNativeFrames[2] = static_cast<NativeStackFrameAddress>(0x80);
        child.BirthNativeFrameCount = 3;

        ScriptStackTraceLayer parent;
        parent.ScriptFrames.push_back(MakeScriptFrame("ParentScript", "Scripts/Parent.fos", 20));
        // Parent layer was launched at 0xB1; its birth stack matches the trace bottom
        // from 0xB1 down through main.
        parent.BirthNativeFrames[0] = static_cast<NativeStackFrameAddress>(0xB1);
        parent.BirthNativeFrames[1] = static_cast<NativeStackFrameAddress>(0x80);
        parent.BirthNativeFrameCount = 2;

        std::vector<ScriptStackTraceLayer> layers;
        layers.push_back(std::move(child));
        layers.push_back(std::move(parent));
        st.ScriptLayers = std::make_shared<const std::vector<ScriptStackTraceLayer>>(std::move(layers));

        auto resolved = ResolveStackTrace(st);

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

    SECTION("DeepNativeStackAtCapacityAnchorsLayerCorrectly")
    {
        // Native trace fills the cap exactly. Layer's BirthNativeFrames is a proper
        // bottom-aligned suffix of the trace, so anchoring must still work at the edge of
        // the cap without an off-by-one.
        StackTraceData st {};
        st.NativeFrameCount = STACK_TRACE_MAX_NATIVE_FRAMES;

        for (uint32_t i = 0; i < STACK_TRACE_MAX_NATIVE_FRAMES; i++) {
            st.NativeFrames[i] = static_cast<NativeStackFrameAddress>(0x1000 + i);
        }

        // Pretend the script layer was launched 50 frames into the trace, so its birth
        // chain spans the bottom 78 frames (indices 50..127 in the trace).
        constexpr uint32_t launch_index = 50;
        constexpr uint32_t birth_count = STACK_TRACE_MAX_NATIVE_FRAMES - launch_index;

        ScriptStackTraceLayer layer;
        layer.ScriptFrames.push_back(MakeScriptFrame("DeepScript", "Scripts/Deep.fos", 99));
        layer.BirthNativeFrameCount = birth_count;

        for (uint32_t i = 0; i < birth_count; i++) {
            layer.BirthNativeFrames[i] = st.NativeFrames[launch_index + i];
        }

        std::vector<ScriptStackTraceLayer> layers;
        layers.push_back(std::move(layer));
        st.ScriptLayers = std::make_shared<const std::vector<ScriptStackTraceLayer>>(std::move(layers));

        auto resolved = ResolveStackTrace(st);

        // Expected: 50 deeper natives -> the script -> 78 tail natives.
        REQUIRE(resolved.size() == STACK_TRACE_MAX_NATIVE_FRAMES + 1);

        for (uint32_t i = 0; i < launch_index; i++) {
            CHECK(resolved[i].Type == StackTraceFrame::FrameType::Native);
        }

        CHECK(resolved[launch_index].Type == StackTraceFrame::FrameType::Script);
        CHECK(resolved[launch_index].Function == "DeepScript");

        for (uint32_t i = launch_index + 1; i < resolved.size(); i++) {
            CHECK(resolved[i].Type == StackTraceFrame::FrameType::Native);
        }
    }

    SECTION("FormatStackTraceMarksTruncationInHeader")
    {
        StackTraceData st {};
        st.NativeFrames[0] = static_cast<NativeStackFrameAddress>(0xCAFE);
        st.NativeFrameCount = 1;
        st.NativeTruncated = true;

        auto formatted = FormatStackTrace(st);

        // Only the header changes when truncated; the rest of the rendering is unaffected.
        CHECK(formatted.find("Stack trace (most recent call first, truncated at ") == 0);
        CHECK(formatted.find("128 frames):") != std::string::npos);

        st.NativeTruncated = false;
        auto formatted_clean = FormatStackTrace(st);
        CHECK(formatted_clean.find("Stack trace (most recent call first):") == 0);
        CHECK(formatted_clean.find("truncated") == std::string::npos);
    }

    SECTION("ResolvedNativeFramesAreCachedGlobally")
    {
        StackTraceData st {};
        st.NativeFrames[0] = static_cast<NativeStackFrameAddress>(0xCAFE);
        st.NativeFrames[1] = static_cast<NativeStackFrameAddress>(0xBABE);
        st.NativeFrames[2] = static_cast<NativeStackFrameAddress>(0xCAFE);
        st.NativeFrameCount = 3;

        REQUIRE(GetResolvedStackTraceCacheSize() == 0);

        auto resolved_first = ResolveStackTrace(st);

        REQUIRE(resolved_first.size() == 3);
        CHECK(GetResolvedStackTraceCacheSize() == 2);

        auto resolved_second = ResolveStackTrace(st);

        REQUIRE(resolved_second.size() == 3);
        CHECK(GetResolvedStackTraceCacheSize() == 2);
        CHECK(resolved_second[0].Function == resolved_first[0].Function);
        CHECK(resolved_second[1].Function == resolved_first[1].Function);
        CHECK(resolved_second[2].Function == resolved_first[2].Function);
    }

    SECTION("CaptureNativeStackFramesReportsNoTruncationForShallowStack")
    {
        // A normal capture inside a unit test thread is well below the 128-frame cap,
        // so the truncation flag must come back clean.
        std::array<NativeStackFrameAddress, STACK_TRACE_MAX_NATIVE_FRAMES> frames {};
        uint32_t count = 0;
        bool truncated = true; // start with the wrong value to make sure capture clears it.

        CaptureNativeStackFrames(frames, count, truncated, 0);

        CHECK_FALSE(truncated);
#if FO_MEMORY_SANITIZER
        CHECK(count == 0);
#else
        CHECK(count > 0);
        CHECK(count < STACK_TRACE_MAX_NATIVE_FRAMES);
#endif
    }

    SECTION("CaptureOverflowDegradesGracefullyAndPushesScriptBeforeNatives")
    {
        // Simulates the real-world cap-overflow: both birth and current trace each filled
        // STACK_TRACE_MAX_NATIVE_FRAMES with addresses near the TOP of their respective
        // (deeper) physical stacks, so the captured arrays do NOT share a common bottom.
        // Bottom-aligned matching honestly fails to anchor here — by design we degrade
        // by emitting the script layer before the (un-anchored) native chunk instead of
        // guessing a wrong-but-plausible anchor through a top-down search.
        StackTraceData st {};
        st.NativeFrameCount = STACK_TRACE_MAX_NATIVE_FRAMES;

        for (uint32_t i = 0; i < STACK_TRACE_MAX_NATIVE_FRAMES; i++) {
            st.NativeFrames[i] = static_cast<NativeStackFrameAddress>(0x2000 + i);
        }

        ScriptStackTraceLayer layer;
        layer.ScriptFrames.push_back(MakeScriptFrame("OrphanedScript", "Scripts/Orphan.fos", 1));
        layer.BirthNativeFrameCount = STACK_TRACE_MAX_NATIVE_FRAMES;

        // Birth uses a disjoint address range -> nothing aligns at the bottom.
        for (uint32_t i = 0; i < STACK_TRACE_MAX_NATIVE_FRAMES; i++) {
            layer.BirthNativeFrames[i] = static_cast<NativeStackFrameAddress>(0x9000 + i);
        }

        std::vector<ScriptStackTraceLayer> layers;
        layers.push_back(std::move(layer));
        st.ScriptLayers = std::make_shared<const std::vector<ScriptStackTraceLayer>>(std::move(layers));

        auto resolved = ResolveStackTrace(st);

        REQUIRE(resolved.size() == STACK_TRACE_MAX_NATIVE_FRAMES + 1);
        CHECK(resolved[0].Type == StackTraceFrame::FrameType::Script);
        CHECK(resolved[0].Function == "OrphanedScript");

        for (uint32_t i = 1; i < resolved.size(); i++) {
            CHECK(resolved[i].Type == StackTraceFrame::FrameType::Native);
        }
    }

    SECTION("LayerWithoutBirthAnchorEmitsScriptOnlyAndDeferNativeToTail")
    {
        // BirthNativeFrameCount == 0: layer is recorded but the resolver can't anchor it in
        // the native trace, so all native frames go after every script layer.
        StackTraceData st {};
        st.NativeFrames[0] = static_cast<NativeStackFrameAddress>(0xCAFE);
        st.NativeFrames[1] = static_cast<NativeStackFrameAddress>(0xBABE);
        st.NativeFrameCount = 2;

        std::vector<ScriptStackTraceLayer> layers;
        layers.push_back(MakeLayer({MakeScriptFrame("OnlyScript", "Scripts/Only.fos", 1)}));
        // No BirthNativeFrameCount set - left at default 0.
        st.ScriptLayers = std::make_shared<const std::vector<ScriptStackTraceLayer>>(std::move(layers));

        auto resolved = ResolveStackTrace(st);

        REQUIRE(resolved.size() == 3);
        CHECK(resolved[0].Type == StackTraceFrame::FrameType::Script);
        CHECK(resolved[1].Type == StackTraceFrame::FrameType::Native);
        CHECK(resolved[2].Type == StackTraceFrame::FrameType::Native);
    }

    SECTION("GetStackTraceEntryReturnsFramesByDepth")
    {
        ScopedScriptStackTraceProvider scope([](std::vector<ScriptStackTraceLayer>& layers) {
            layers.push_back(MakeLayer({
                MakeScriptFrame("DepthZero", "Scripts/Z.fos", 1),
                MakeScriptFrame("DepthOne", "Scripts/O.fos", 2),
            }));
        });

        auto top = GetStackTraceEntry(0);
        auto next = GetStackTraceEntry(1);

        REQUIRE(top.has_value());
        REQUIRE(next.has_value());
        CHECK(top->Function == "DepthZero");
        CHECK(next->Function == "DepthOne");
    }

    SECTION("GetStackTraceEntryReturnsNulloptForOutOfRange")
    {
        ScopedScriptStackTraceProvider scope([](std::vector<ScriptStackTraceLayer>& layers) { layers.push_back(MakeLayer({MakeScriptFrame("Only", "Scripts/Only.fos", 1)})); });

        auto missing = GetStackTraceEntry(10000);
        CHECK_FALSE(missing.has_value());
    }

    SECTION("FormatStackTraceWithNoFramesReturnsHeaderOnly")
    {
        StackTraceData st {};

        auto formatted = FormatStackTrace(st);

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

        std::string log_contents = captured.str();

        CHECK(log_contents.find("Stack trace (most recent call first):\n") == 0);
        CHECK(log_contents.find("- [Script] FuncA (a.fos line 11)\n") != std::string::npos);
        CHECK(log_contents.find("- [Script] FuncB (b.fos line 22)\n") != std::string::npos);
        CHECK(log_contents.ends_with("\n"));
    }

    SECTION("ProviderExceptionsDoNotEscape")
    {
        ScopedScriptStackTraceProvider scope([](std::vector<ScriptStackTraceLayer>&) {
            // A misbehaving provider must not crash the capture path even if it throws —
            // GetStackTrace defensively swallows the exception so the contract is preserved.
            throw std::runtime_error("provider failure");
        });

        auto st = GetStackTrace();
        // Capture survived and produced a valid object. ScriptLayers stays null because the
        // provider didn't append anything before throwing.
        CHECK_FALSE(st.ScriptLayers);
    }

    SetScriptStackTraceProvider({});
    ClearResolvedStackTraceCache();
}

FO_END_NAMESPACE
