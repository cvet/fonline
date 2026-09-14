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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#include "catch_amalgamated.hpp"

#include <iostream>
#include <sstream>

#include "BaseLogging.h"
#include "StackTrace.h"

FO_BEGIN_NAMESPACE

namespace
{
    // RAII helper so each section starts with a clean provider regardless of the previous one
    struct ScopedScriptStackTraceProvider
    {
        explicit ScopedScriptStackTraceProvider(ScriptStackTraceProvider provider) noexcept { SetScriptStackTraceProvider("Test", std::move(provider)); }
        ScopedScriptStackTraceProvider(const ScopedScriptStackTraceProvider&) = delete;
        ScopedScriptStackTraceProvider(ScopedScriptStackTraceProvider&&) noexcept = delete;
        auto operator=(const ScopedScriptStackTraceProvider&) -> ScopedScriptStackTraceProvider& = delete;
        auto operator=(ScopedScriptStackTraceProvider&&) noexcept -> ScopedScriptStackTraceProvider& = delete;
        ~ScopedScriptStackTraceProvider() noexcept { SetScriptStackTraceProvider("Test", {}); }
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
    // Make sure no leaked provider from a prior test pollutes the suite
    SetScriptStackTraceProvider("Test", {});
    SetScriptStackTraceProvider("TestOther", {});
    ClearResolvedStackTraceCache();

    SECTION("ProviderRegistrationIsObservable")
    {
        CHECK_FALSE(HasScriptStackTraceProvider("Test"));

        SetScriptStackTraceProvider("Test", [](const StackTraceData&, std::vector<ScriptStackTraceLayer>&) { });
        CHECK(HasScriptStackTraceProvider("Test"));
        CHECK_FALSE(HasScriptStackTraceProvider("TestOther"));

        SetScriptStackTraceProvider("Test", {});
        CHECK_FALSE(HasScriptStackTraceProvider("Test"));
    }

    SECTION("ProviderSeesTheCapturedNativeFrames")
    {
        uint32_t seen_native_frames = 0;

        ScopedScriptStackTraceProvider scope([&seen_native_frames](const StackTraceData& st, std::vector<ScriptStackTraceLayer>&) { seen_native_frames = st.NativeFrameCount; });

        auto st = GetStackTrace();

        CHECK(seen_native_frames == st.NativeFrameCount);
    }

    SECTION("LayersOfSeveralProvidersNestByBirthDepth")
    {
        // Each backend knows only its own entries, and the one entered deeper captured the longer birth stack
        auto make_provider = [](std::string function, uint32_t birth_count) {
            return [function = std::move(function), birth_count](const StackTraceData&, std::vector<ScriptStackTraceLayer>& layers) {
                ScriptStackTraceLayer layer = MakeLayer({MakeScriptFrame(function, "Scripts/Nested.fos", 1)});
                layer.BirthNativeFrameCount = birth_count;
                layers.push_back(std::move(layer));
            };
        };

        ScopedScriptStackTraceProvider outer_scope(make_provider("OuterBackend", 2));
        SetScriptStackTraceProvider("TestOther", make_provider("InnerBackend", 5));

        auto st = GetStackTrace();

        SetScriptStackTraceProvider("TestOther", {});

        REQUIRE(st.ScriptLayers);
        REQUIRE(st.ScriptLayers->size() == 2);
        CHECK((*st.ScriptLayers)[0].ScriptFrames[0].Function == "InnerBackend");
        CHECK((*st.ScriptLayers)[1].ScriptFrames[0].Function == "OuterBackend");
    }

    SECTION("SingleLayerCapturesScriptFramesInProvidedOrder")
    {
        ScopedScriptStackTraceProvider scope([](const StackTraceData&, std::vector<ScriptStackTraceLayer>& layers) {
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
        // Two layers: child (active) on top, parent below. No native anchors set
        ScopedScriptStackTraceProvider scope([](const StackTraceData&, std::vector<ScriptStackTraceLayer>& layers) {
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
        ScopedScriptStackTraceProvider scope([](const StackTraceData&, std::vector<ScriptStackTraceLayer>& layers) { layers.push_back(MakeLayer({MakeScriptFrame("Boss", "Scripts/Boss.fos", 7)})); });

        auto formatted = FormatStackTrace(GetStackTrace());

        CHECK(formatted.find("- [Script] Boss (Boss.fos line 7)") != std::string::npos);
    }

    SECTION("ResolveStackTracePlacesScriptBeforeNativeWhenNoBirthAnchor")
    {
        ScopedScriptStackTraceProvider scope([](const StackTraceData&, std::vector<ScriptStackTraceLayer>& layers) {
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
        // Hand-built so the layers bottom-align with the trace, which is what production produces: birth frames are
        // captured at RequestContext and share their bottom with any later trace inside the launched Execute()
        StackTraceData st {};
        // Pretend native frames addresses 0xA0, 0xB0, ..., 0xA0 = top, 0x80 = main
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
        // bottom from 0xB0 down through main
        child.BirthNativeFrames[0] = static_cast<NativeStackFrameAddress>(0xB0);
        child.BirthNativeFrames[1] = static_cast<NativeStackFrameAddress>(0xB1);
        child.BirthNativeFrames[2] = static_cast<NativeStackFrameAddress>(0x80);
        child.BirthNativeFrameCount = 3;

        ScriptStackTraceLayer parent;
        parent.ScriptFrames.push_back(MakeScriptFrame("ParentScript", "Scripts/Parent.fos", 20));
        // Parent layer was launched at 0xB1; its birth stack matches the trace bottom
        // from 0xB1 down through main
        parent.BirthNativeFrames[0] = static_cast<NativeStackFrameAddress>(0xB1);
        parent.BirthNativeFrames[1] = static_cast<NativeStackFrameAddress>(0x80);
        parent.BirthNativeFrameCount = 2;

        std::vector<ScriptStackTraceLayer> layers;
        layers.push_back(std::move(child));
        layers.push_back(std::move(parent));
        st.ScriptLayers = std::make_shared<const std::vector<ScriptStackTraceLayer>>(std::move(layers));

        auto resolved = ResolveStackTrace(st);

        // Most-recent first, so each script layer sits directly below the natives it called and above the frames
        // between its own anchor and the next one out
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
        // The trace fills the cap exactly, so anchoring has to hold at the edge without an off-by-one
        StackTraceData st {};
        st.NativeFrameCount = STACK_TRACE_MAX_NATIVE_FRAMES;

        for (uint32_t i = 0; i < STACK_TRACE_MAX_NATIVE_FRAMES; i++) {
            st.NativeFrames[i] = static_cast<NativeStackFrameAddress>(0x1000 + i);
        }

        // Pretend the script layer was launched 50 frames into the trace, so its birth
        // chain spans the bottom 78 frames (indices 50..127 in the trace)
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

        // Expected: 50 deeper natives -> the script -> 78 tail natives
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

        // Only the header changes when truncated; the rest of the rendering is unaffected
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
        // so the truncation flag must come back clean
        std::array<NativeStackFrameAddress, STACK_TRACE_MAX_NATIVE_FRAMES> frames {};
        uint32_t count = 0;
        bool truncated = true; // start with the wrong value to make sure capture clears it

        CaptureNativeStackFrames(frames, count, truncated, 0);

        CHECK_FALSE(truncated);
#if FO_MEMORY_SANITIZER || FO_THREAD_SANITIZER
        CHECK(count == 0);
#else
        CHECK(count > 0);
        CHECK(count < STACK_TRACE_MAX_NATIVE_FRAMES);
#endif
    }

    SECTION("CaptureOverflowReadsBelowTheLayerFromItsBirthFrames")
    {
        // Both traces fill the cap from the top of deeper stacks, so they share no bottom and anchoring honestly
        // fails; the trace is read to its end and the stack below the layer comes from the birth capture
        StackTraceData st {};
        st.NativeFrameCount = STACK_TRACE_MAX_NATIVE_FRAMES;

        for (uint32_t i = 0; i < STACK_TRACE_MAX_NATIVE_FRAMES; i++) {
            st.NativeFrames[i] = static_cast<NativeStackFrameAddress>(0x2000 + i);
        }

        ScriptStackTraceLayer layer;
        layer.ScriptFrames.push_back(MakeScriptFrame("OrphanedScript", "Scripts/Orphan.fos", 1));
        layer.BirthNativeFrameCount = STACK_TRACE_MAX_NATIVE_FRAMES;

        // Birth uses a disjoint address range -> nothing aligns at the bottom
        for (uint32_t i = 0; i < STACK_TRACE_MAX_NATIVE_FRAMES; i++) {
            layer.BirthNativeFrames[i] = static_cast<NativeStackFrameAddress>(0x9000 + i);
        }

        std::vector<ScriptStackTraceLayer> layers;
        layers.push_back(std::move(layer));
        st.ScriptLayers = std::make_shared<const std::vector<ScriptStackTraceLayer>>(std::move(layers));

        auto resolved = ResolveStackTrace(st);

        REQUIRE(resolved.size() == STACK_TRACE_MAX_NATIVE_FRAMES * 2 + 1);

        for (uint32_t i = 0; i < STACK_TRACE_MAX_NATIVE_FRAMES; i++) {
            CHECK(resolved[i].Type == StackTraceFrame::FrameType::Native);
        }

        CHECK(resolved[STACK_TRACE_MAX_NATIVE_FRAMES].Type == StackTraceFrame::FrameType::Script);
        CHECK(resolved[STACK_TRACE_MAX_NATIVE_FRAMES].Function == "OrphanedScript");

        for (uint32_t i = STACK_TRACE_MAX_NATIVE_FRAMES + 1; i < resolved.size(); i++) {
            CHECK(resolved[i].Type == StackTraceFrame::FrameType::Native);
        }
    }

    SECTION("TraceStoppedInsideScriptRuntimeContinuesAlongBirthFrames")
    {
        // An unwinder that cannot step through generated code ends the trace at the first such frame, so nothing
        // below the script entry is in the trace itself
        StackTraceData st {};
        std::array<NativeStackFrameAddress, 3> pcs {
            static_cast<NativeStackFrameAddress>(0xA0), // native throw site
            static_cast<NativeStackFrameAddress>(0xA1), // native function the script called
            static_cast<NativeStackFrameAddress>(0xF0), // generated script code, where unwinding stopped
        };

        for (size_t i = 0; i < pcs.size(); i++) {
            st.NativeFrames[i] = pcs[i];
        }

        st.NativeFrameCount = static_cast<uint32_t>(pcs.size());

        ScriptStackTraceLayer layer;
        layer.ScriptFrames.push_back(MakeScriptFrame("RuntimeScript", "Scripts/Runtime.cs", 5));
        layer.BirthNativeFrames[0] = static_cast<NativeStackFrameAddress>(0xB0); // entry that invoked the script
        layer.BirthNativeFrames[1] = static_cast<NativeStackFrameAddress>(0xB1);
        layer.BirthNativeFrames[2] = static_cast<NativeStackFrameAddress>(0x80); // main()
        layer.BirthNativeFrameCount = 3;
        layer.RuntimeNativeFrames.push_back(static_cast<NativeStackFrameAddress>(0xF0));

        std::vector<ScriptStackTraceLayer> layers;
        layers.push_back(std::move(layer));
        st.ScriptLayers = std::make_shared<const std::vector<ScriptStackTraceLayer>>(std::move(layers));

        auto resolved = ResolveStackTrace(st);
        auto native_name = [](NativeStackFrameAddress addr) {
            StackTraceData single {};
            single.NativeFrames[0] = addr;
            single.NativeFrameCount = 1;
            return ResolveStackTrace(single)[0].Function;
        };

        // The generated-code frame is described by the script frames, so it is not printed as a raw address
        REQUIRE(resolved.size() == 6);
        CHECK(resolved[0].Function == native_name(0xA0));
        CHECK(resolved[1].Function == native_name(0xA1));
        CHECK(resolved[2].Type == StackTraceFrame::FrameType::Script);
        CHECK(resolved[2].Function == "RuntimeScript");
        CHECK(resolved[3].Function == native_name(0xB0));
        CHECK(resolved[4].Function == native_name(0xB1));
        CHECK(resolved[5].Function == native_name(0x80));
    }

    SECTION("RuntimeFramesPlaceScriptBetweenCalledNativesAndRuntimeEntry")
    {
        // An unwinder that walks generated code leaves it in the trace: the natives above it were called by script, the
        // natives below it are the runtime entering script
        StackTraceData st {};
        std::array<NativeStackFrameAddress, 7> pcs {
            static_cast<NativeStackFrameAddress>(0xA0), // native function the script called
            static_cast<NativeStackFrameAddress>(0xF0), // generated script code
            static_cast<NativeStackFrameAddress>(0xF1), // generated script code
            static_cast<NativeStackFrameAddress>(0xC0), // runtime invoke
            static_cast<NativeStackFrameAddress>(0xB0), // entry that invoked the script
            static_cast<NativeStackFrameAddress>(0xB1),
            static_cast<NativeStackFrameAddress>(0x80), // main()
        };

        for (size_t i = 0; i < pcs.size(); i++) {
            st.NativeFrames[i] = pcs[i];
        }

        st.NativeFrameCount = static_cast<uint32_t>(pcs.size());

        ScriptStackTraceLayer layer;
        layer.ScriptFrames.push_back(MakeScriptFrame("JitScript", "Scripts/Jit.cs", 3));
        layer.BirthNativeFrames[0] = static_cast<NativeStackFrameAddress>(0xB0);
        layer.BirthNativeFrames[1] = static_cast<NativeStackFrameAddress>(0xB1);
        layer.BirthNativeFrames[2] = static_cast<NativeStackFrameAddress>(0x80);
        layer.BirthNativeFrameCount = 3;
        layer.RuntimeNativeFrames.push_back(static_cast<NativeStackFrameAddress>(0xF0));
        layer.RuntimeNativeFrames.push_back(static_cast<NativeStackFrameAddress>(0xF1));

        std::vector<ScriptStackTraceLayer> layers;
        layers.push_back(std::move(layer));
        st.ScriptLayers = std::make_shared<const std::vector<ScriptStackTraceLayer>>(std::move(layers));

        auto resolved = ResolveStackTrace(st);
        auto native_name = [](NativeStackFrameAddress addr) {
            StackTraceData single {};
            single.NativeFrames[0] = addr;
            single.NativeFrameCount = 1;
            return ResolveStackTrace(single)[0].Function;
        };

        REQUIRE(resolved.size() == 6);
        CHECK(resolved[0].Function == native_name(0xA0));
        CHECK(resolved[1].Type == StackTraceFrame::FrameType::Script);
        CHECK(resolved[1].Function == "JitScript");
        CHECK(resolved[2].Function == native_name(0xC0));
        CHECK(resolved[3].Function == native_name(0xB0));
        CHECK(resolved[4].Function == native_name(0xB1));
        CHECK(resolved[5].Function == native_name(0x80));
    }

    SECTION("UnwoundScriptFramesBecomeInnermostLayerAtCapturePoint")
    {
        StackTraceData st {};
        st.NativeFrames[0] = static_cast<NativeStackFrameAddress>(0xA0);
        st.NativeFrames[1] = static_cast<NativeStackFrameAddress>(0xB0);
        st.NativeFrameCount = 2;

        ScriptStackTraceLayer outer = MakeLayer({MakeScriptFrame("OuterScript", "Scripts/Outer.cs", 1)});
        outer.BirthNativeFrames[0] = static_cast<NativeStackFrameAddress>(0xB0);
        outer.BirthNativeFrameCount = 1;

        std::vector<ScriptStackTraceLayer> layers;
        layers.push_back(std::move(outer));
        st.ScriptLayers = std::make_shared<const std::vector<ScriptStackTraceLayer>>(std::move(layers));

        AddUnwoundScriptFrames(st, MakeLayer({MakeScriptFrame("ThrowSite", "Scripts/Thrown.cs", 9)}));

        REQUIRE(st.ScriptLayers);
        REQUIRE(st.ScriptLayers->size() == 2);
        CHECK((*st.ScriptLayers)[0].BirthNativeFrameCount == 2);

        auto resolved = ResolveStackTrace(st);

        REQUIRE(resolved.size() == 4);
        CHECK(resolved[0].Function == "ThrowSite");
        CHECK(resolved[1].Type == StackTraceFrame::FrameType::Native);
        CHECK(resolved[2].Function == "OuterScript");
        CHECK(resolved[3].Type == StackTraceFrame::FrameType::Native);
    }

    SECTION("CaughtScriptFramesReplaceLiveFramesAboveTheCatchingFrame")
    {
        StackTraceData st {};
        std::vector<ScriptStackTraceLayer> layers;
        layers.push_back(MakeLayer({
            MakeScriptFrame("ReportHelper", "Scripts/Report.cs", 1),
            MakeScriptFrame("Catcher", "Scripts/Catcher.cs", 20),
            MakeScriptFrame("Caller", "Scripts/Caller.cs", 30),
        }));
        st.ScriptLayers = std::make_shared<const std::vector<ScriptStackTraceLayer>>(std::move(layers));

        SpliceCaughtScriptFrames(st,
            MakeLayer({
                MakeScriptFrame("ThrowSite", "Scripts/Thrown.cs", 5),
                MakeScriptFrame("Catcher", "Scripts/Catcher.cs", 18),
            }));

        REQUIRE(st.ScriptLayers);
        REQUIRE(st.ScriptLayers->size() == 1);

        const auto& frames = (*st.ScriptLayers)[0].ScriptFrames;
        REQUIRE(frames.size() == 3);
        CHECK(frames[0].Function == "ThrowSite");
        CHECK(frames[1].Function == "Catcher");
        CHECK(frames[1].Line == 18);
        CHECK(frames[2].Function == "Caller");
    }

    SECTION("CaughtScriptFramesWithoutLiveCatcherPrecedeTheLiveFrames")
    {
        StackTraceData st {};
        std::vector<ScriptStackTraceLayer> layers;
        layers.push_back(MakeLayer({MakeScriptFrame("Unrelated", "Scripts/Unrelated.cs", 1)}));
        st.ScriptLayers = std::make_shared<const std::vector<ScriptStackTraceLayer>>(std::move(layers));

        SpliceCaughtScriptFrames(st, MakeLayer({MakeScriptFrame("ThrowSite", "Scripts/Thrown.cs", 5)}));

        const auto& frames = (*st.ScriptLayers)[0].ScriptFrames;
        REQUIRE(frames.size() == 2);
        CHECK(frames[0].Function == "ThrowSite");
        CHECK(frames[1].Function == "Unrelated");
    }

    SECTION("TraceReachingIntoBirthFramesReadsTheSharedPartOnce")
    {
        StackTraceData st {};
        std::array<NativeStackFrameAddress, 3> pcs {
            static_cast<NativeStackFrameAddress>(0xA0),
            static_cast<NativeStackFrameAddress>(0xB0),
            static_cast<NativeStackFrameAddress>(0xB1),
        };

        for (size_t i = 0; i < pcs.size(); i++) {
            st.NativeFrames[i] = pcs[i];
        }

        st.NativeFrameCount = static_cast<uint32_t>(pcs.size());

        ScriptStackTraceLayer layer;
        layer.ScriptFrames.push_back(MakeScriptFrame("SharedScript", "Scripts/Shared.cs", 7));
        layer.BirthNativeFrames[0] = static_cast<NativeStackFrameAddress>(0xB0);
        layer.BirthNativeFrames[1] = static_cast<NativeStackFrameAddress>(0xB1);
        layer.BirthNativeFrames[2] = static_cast<NativeStackFrameAddress>(0x80);
        layer.BirthNativeFrames[3] = static_cast<NativeStackFrameAddress>(0x81);
        layer.BirthNativeFrameCount = 4;

        std::vector<ScriptStackTraceLayer> layers;
        layers.push_back(std::move(layer));
        st.ScriptLayers = std::make_shared<const std::vector<ScriptStackTraceLayer>>(std::move(layers));

        auto resolved = ResolveStackTrace(st);

        REQUIRE(resolved.size() == 6);
        CHECK(resolved[0].Type == StackTraceFrame::FrameType::Native);
        CHECK(resolved[1].Type == StackTraceFrame::FrameType::Script);
        CHECK(resolved[1].Function == "SharedScript");

        for (size_t i = 2; i < resolved.size(); i++) {
            CHECK(resolved[i].Type == StackTraceFrame::FrameType::Native);
        }
    }

    SECTION("LayerWithoutBirthAnchorEmitsScriptOnlyAndDeferNativeToTail")
    {
        // BirthNativeFrameCount == 0: layer is recorded but the resolver can't anchor it in
        // the native trace, so all native frames go after every script layer
        StackTraceData st {};
        st.NativeFrames[0] = static_cast<NativeStackFrameAddress>(0xCAFE);
        st.NativeFrames[1] = static_cast<NativeStackFrameAddress>(0xBABE);
        st.NativeFrameCount = 2;

        std::vector<ScriptStackTraceLayer> layers;
        layers.push_back(MakeLayer({MakeScriptFrame("OnlyScript", "Scripts/Only.fos", 1)}));
        // No BirthNativeFrameCount set - left at default 0
        st.ScriptLayers = std::make_shared<const std::vector<ScriptStackTraceLayer>>(std::move(layers));

        auto resolved = ResolveStackTrace(st);

        REQUIRE(resolved.size() == 3);
        CHECK(resolved[0].Type == StackTraceFrame::FrameType::Script);
        CHECK(resolved[1].Type == StackTraceFrame::FrameType::Native);
        CHECK(resolved[2].Type == StackTraceFrame::FrameType::Native);
    }

    SECTION("GetStackTraceEntryReturnsFramesByDepth")
    {
        ScopedScriptStackTraceProvider scope([](const StackTraceData&, std::vector<ScriptStackTraceLayer>& layers) {
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
        ScopedScriptStackTraceProvider scope([](const StackTraceData&, std::vector<ScriptStackTraceLayer>& layers) { layers.push_back(MakeLayer({MakeScriptFrame("Only", "Scripts/Only.fos", 1)})); });

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
        ScopedScriptStackTraceProvider scope([](const StackTraceData&, std::vector<ScriptStackTraceLayer>&) {
            // A misbehaving provider must not crash the capture path even if it throws —
            // GetStackTrace defensively swallows the exception so the contract is preserved
            throw std::runtime_error("provider failure");
        });

        auto st = GetStackTrace();
        // Capture survived and produced a valid object. ScriptLayers stays null because the
        // provider didn't append anything before throwing
        CHECK_FALSE(st.ScriptLayers);
    }

    SetScriptStackTraceProvider("Test", {});
    SetScriptStackTraceProvider("TestOther", {});
    ClearResolvedStackTraceCache();
}

FO_END_NAMESPACE
