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
        explicit ScopedScriptStackTraceProvider(stack_trace::script_provider provider) noexcept { stack_trace::set_script_provider("Test", std::move(provider)); }
        ScopedScriptStackTraceProvider(const ScopedScriptStackTraceProvider&) = delete;
        ScopedScriptStackTraceProvider(ScopedScriptStackTraceProvider&&) noexcept = delete;
        auto operator=(const ScopedScriptStackTraceProvider&) -> ScopedScriptStackTraceProvider& = delete;
        auto operator=(ScopedScriptStackTraceProvider&&) noexcept -> ScopedScriptStackTraceProvider& = delete;
        ~ScopedScriptStackTraceProvider() noexcept { stack_trace::set_script_provider("Test", {}); }
    };

    auto MakeScriptFrame(std::string function, std::string file, uint32_t line) -> stack_trace::frame
    {
        stack_trace::frame frame;
        frame.type = stack_trace::frame::frame_type::script;
        frame.function = std::move(function);
        frame.file = std::move(file);
        frame.line = line;
        return frame;
    }

    auto MakeLayer(std::initializer_list<stack_trace::frame> frames) -> stack_trace::script_layer
    {
        stack_trace::script_layer layer;
        layer.script_frames.assign(frames);
        return layer;
    }

    struct CapturedFrames
    {
        std::array<stack_trace::native_frame_address, stack_trace::MAX_NATIVE_FRAMES> frames {};
        uint32_t count {};
        bool truncated {};
    };

    // Resolves from a call made after the capture has reused the stack below the saving frame
    FO_NO_INLINE void ResolveFromDeeperCall(const stack_trace::resume_point& point, CapturedFrames& out)
    {
        stack_trace::resolve_resume_point(point, out.frames, out.count, out.truncated);
    }

    // One frame saves a resume point and takes an eager capture, then resolves the point while it is still active
    FO_NO_INLINE void SaveCaptureAndResolve(CapturedFrames& eager, CapturedFrames& resolved)
    {
        stack_trace::resume_point point;
        (void)stack_trace::save_resume_point(&point);
        stack_trace::capture_native_frames(eager.frames, eager.count, eager.truncated, 0);
        ResolveFromDeeperCall(point, resolved);
    }

    auto ResolveFunctionName(stack_trace::native_frame_address addr) -> std::string
    {
        stack_trace::data st {};
        st.native_frames[0] = addr;
        st.native_frame_count = 1;
        return stack_trace::resolve(st).front().function;
    }
}

TEST_CASE("StackTrace")
{
    // Make sure no leaked provider from a prior test pollutes the suite
    stack_trace::set_script_provider("Test", {});
    stack_trace::set_script_provider("TestOther", {});
    stack_trace::clear_resolved_cache();

    SECTION("ProviderRegistrationIsObservable")
    {
        CHECK_FALSE(stack_trace::has_script_provider("Test"));

        stack_trace::set_script_provider("Test", [](const stack_trace::data&, std::vector<stack_trace::script_layer>&) { });
        CHECK(stack_trace::has_script_provider("Test"));
        CHECK_FALSE(stack_trace::has_script_provider("TestOther"));

        stack_trace::set_script_provider("Test", {});
        CHECK_FALSE(stack_trace::has_script_provider("Test"));
    }

    SECTION("ProviderSeesTheCapturedNativeFrames")
    {
        uint32_t seen_native_frames = 0;

        ScopedScriptStackTraceProvider scope([&seen_native_frames](const stack_trace::data& st, std::vector<stack_trace::script_layer>&) { seen_native_frames = st.native_frame_count; });

        auto st = stack_trace::get();

        CHECK(seen_native_frames == st.native_frame_count);
    }

    SECTION("LayersOfSeveralProvidersNestByBirthDepth")
    {
        // Each backend knows only its own entries, and the one entered deeper captured the longer birth stack
        auto make_provider = [](std::string function, uint32_t birth_count) {
            return [function = std::move(function), birth_count](const stack_trace::data&, std::vector<stack_trace::script_layer>& layers) {
                stack_trace::script_layer layer = MakeLayer({MakeScriptFrame(function, "Scripts/Nested.fos", 1)});
                layer.birth_native_frame_count = birth_count;
                layers.push_back(std::move(layer));
            };
        };

        ScopedScriptStackTraceProvider outer_scope(make_provider("OuterBackend", 2));
        stack_trace::set_script_provider("TestOther", make_provider("InnerBackend", 5));

        auto st = stack_trace::get();

        stack_trace::set_script_provider("TestOther", {});

        REQUIRE(st.script_layers);
        REQUIRE(st.script_layers->size() == 2);
        CHECK((*st.script_layers)[0].script_frames[0].function == "InnerBackend");
        CHECK((*st.script_layers)[1].script_frames[0].function == "OuterBackend");
    }

    SECTION("SingleLayerCapturesScriptFramesInProvidedOrder")
    {
        ScopedScriptStackTraceProvider scope([](const stack_trace::data&, std::vector<stack_trace::script_layer>& layers) {
            layers.push_back(MakeLayer({
                MakeScriptFrame("InnerFunc", "Scripts/Inner.fos", 17),
                MakeScriptFrame("OuterFunc", "Scripts/Outer.fos", 5),
            }));
        });

        auto st = stack_trace::get();

        REQUIRE(st.script_layers);
        REQUIRE(st.script_layers->size() == 1);

        const auto& frames = (*st.script_layers)[0].script_frames;
        REQUIRE(frames.size() == 2);
        CHECK(frames[0].function == "InnerFunc");
        CHECK(frames[0].file == "Scripts/Inner.fos");
        CHECK(frames[0].line == 17);
        CHECK(frames[0].type == stack_trace::frame::frame_type::script);
        CHECK(frames[1].function == "OuterFunc");
    }

    SECTION("MultiContextChainProducesMultipleLayersInnermostFirst")
    {
        // Two layers: child (active) on top, parent below. No native anchors set
        ScopedScriptStackTraceProvider scope([](const stack_trace::data&, std::vector<stack_trace::script_layer>& layers) {
            layers.push_back(MakeLayer({
                MakeScriptFrame("ChildCtx_Top", "Scripts/Child.fos", 42),
                MakeScriptFrame("ChildCtx_Bottom", "Scripts/Child.fos", 1),
            }));
            layers.push_back(MakeLayer({
                MakeScriptFrame("ParentCtx_Frame", "Scripts/Parent.fos", 99),
            }));
        });

        auto formatted = stack_trace::format(stack_trace::get());

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
        ScopedScriptStackTraceProvider scope([](const stack_trace::data&, std::vector<stack_trace::script_layer>& layers) { layers.push_back(MakeLayer({MakeScriptFrame("Boss", "Scripts/Boss.fos", 7)})); });

        auto formatted = stack_trace::format(stack_trace::get());

        CHECK(formatted.find("- [Script] Boss (Boss.fos line 7)") != std::string::npos);
    }

    SECTION("ResolveStackTracePlacesScriptBeforeNativeWhenNoBirthAnchor")
    {
        ScopedScriptStackTraceProvider scope([](const stack_trace::data&, std::vector<stack_trace::script_layer>& layers) {
            layers.push_back(MakeLayer({
                MakeScriptFrame("ScriptA", "Scripts/A.fos", 1),
                MakeScriptFrame("ScriptB", "Scripts/B.fos", 2),
            }));
        });

        auto resolved = stack_trace::resolve(stack_trace::get());

        REQUIRE(resolved.size() >= 2);
        CHECK(resolved[0].type == stack_trace::frame::frame_type::script);
        CHECK(resolved[1].type == stack_trace::frame::frame_type::script);

        for (size_t i = 2; i < resolved.size(); i++) {
            CHECK(resolved[i].type == stack_trace::frame::frame_type::native);
        }
    }

    SECTION("MultiLevelInterleavingSplicesNativeBetweenLayers")
    {
        // Hand-built so the layers bottom-align with the trace, which is what production produces: birth frames are
        // captured at RequestContext and share their bottom with any later trace inside the launched Execute()
        stack_trace::data st {};
        // Pretend native frames addresses 0xA0, 0xB0, ..., 0xA0 = top, 0x80 = main
        std::array<stack_trace::native_frame_address, 5> pcs {
            static_cast<stack_trace::native_frame_address>(0xA0), // child's native bridge code
            static_cast<stack_trace::native_frame_address>(0xA1), // child's Execute()
            static_cast<stack_trace::native_frame_address>(0xB0), // parent's native bridge code (= anchor for child layer)
            static_cast<stack_trace::native_frame_address>(0xB1), // parent's Execute()         (= anchor for parent layer)
            static_cast<stack_trace::native_frame_address>(0x80), // main()
        };
        for (size_t i = 0; i < pcs.size(); i++) {
            st.native_frames[i] = pcs[i];
        }
        st.native_frame_count = static_cast<uint32_t>(pcs.size());

        stack_trace::script_layer child;
        child.script_frames.push_back(MakeScriptFrame("ChildScript", "Scripts/Child.fos", 10));
        // Child layer was launched at the 0xB0 frame; its birth stack matches the trace
        // bottom from 0xB0 down through main
        child.birth_native_frames[0] = static_cast<stack_trace::native_frame_address>(0xB0);
        child.birth_native_frames[1] = static_cast<stack_trace::native_frame_address>(0xB1);
        child.birth_native_frames[2] = static_cast<stack_trace::native_frame_address>(0x80);
        child.birth_native_frame_count = 3;

        stack_trace::script_layer parent;
        parent.script_frames.push_back(MakeScriptFrame("ParentScript", "Scripts/Parent.fos", 20));
        // Parent layer was launched at 0xB1; its birth stack matches the trace bottom
        // from 0xB1 down through main
        parent.birth_native_frames[0] = static_cast<stack_trace::native_frame_address>(0xB1);
        parent.birth_native_frames[1] = static_cast<stack_trace::native_frame_address>(0x80);
        parent.birth_native_frame_count = 2;

        std::vector<stack_trace::script_layer> layers;
        layers.push_back(std::move(child));
        layers.push_back(std::move(parent));
        st.script_layers = std::make_shared<const std::vector<stack_trace::script_layer>>(std::move(layers));

        auto resolved = stack_trace::resolve(st);

        // Most-recent first, so each script layer sits directly below the natives it called and above the frames
        // between its own anchor and the next one out
        REQUIRE(resolved.size() == 7);
        CHECK(resolved[0].type == stack_trace::frame::frame_type::native);
        CHECK(resolved[1].type == stack_trace::frame::frame_type::native);
        CHECK(resolved[2].type == stack_trace::frame::frame_type::script);
        CHECK(resolved[2].function == "ChildScript");
        CHECK(resolved[3].type == stack_trace::frame::frame_type::native);
        CHECK(resolved[4].type == stack_trace::frame::frame_type::script);
        CHECK(resolved[4].function == "ParentScript");
        CHECK(resolved[5].type == stack_trace::frame::frame_type::native);
        CHECK(resolved[6].type == stack_trace::frame::frame_type::native);
    }

    SECTION("DeepNativeStackAtCapacityAnchorsLayerCorrectly")
    {
        // The trace fills the cap exactly, so anchoring has to hold at the edge without an off-by-one
        stack_trace::data st {};
        st.native_frame_count = stack_trace::MAX_NATIVE_FRAMES;

        for (uint32_t i = 0; i < stack_trace::MAX_NATIVE_FRAMES; i++) {
            st.native_frames[i] = static_cast<stack_trace::native_frame_address>(0x1000 + i);
        }

        // Pretend the script layer was launched 50 frames into the trace, so its birth
        // chain spans the bottom 78 frames (indices 50..127 in the trace)
        constexpr uint32_t launch_index = 50;
        constexpr uint32_t birth_count = stack_trace::MAX_NATIVE_FRAMES - launch_index;

        stack_trace::script_layer layer;
        layer.script_frames.push_back(MakeScriptFrame("DeepScript", "Scripts/Deep.fos", 99));
        layer.birth_native_frame_count = birth_count;

        for (uint32_t i = 0; i < birth_count; i++) {
            layer.birth_native_frames[i] = st.native_frames[launch_index + i];
        }

        std::vector<stack_trace::script_layer> layers;
        layers.push_back(std::move(layer));
        st.script_layers = std::make_shared<const std::vector<stack_trace::script_layer>>(std::move(layers));

        auto resolved = stack_trace::resolve(st);

        // Expected: 50 deeper natives -> the script -> 78 tail natives
        REQUIRE(resolved.size() == stack_trace::MAX_NATIVE_FRAMES + 1);

        for (uint32_t i = 0; i < launch_index; i++) {
            CHECK(resolved[i].type == stack_trace::frame::frame_type::native);
        }

        CHECK(resolved[launch_index].type == stack_trace::frame::frame_type::script);
        CHECK(resolved[launch_index].function == "DeepScript");

        for (uint32_t i = launch_index + 1; i < resolved.size(); i++) {
            CHECK(resolved[i].type == stack_trace::frame::frame_type::native);
        }
    }

    SECTION("FormatStackTraceMarksTruncationInHeader")
    {
        stack_trace::data st {};
        st.native_frames[0] = static_cast<stack_trace::native_frame_address>(0xCAFE);
        st.native_frame_count = 1;
        st.native_truncated = true;

        auto formatted = stack_trace::format(st);

        // Only the header changes when truncated; the rest of the rendering is unaffected
        CHECK(formatted.find("Stack trace (most recent call first, truncated at ") == 0);
        CHECK(formatted.find("128 frames):") != std::string::npos);

        st.native_truncated = false;
        auto formatted_clean = stack_trace::format(st);
        CHECK(formatted_clean.find("Stack trace (most recent call first):") == 0);
        CHECK(formatted_clean.find("truncated") == std::string::npos);
    }

    SECTION("ResolvedNativeFramesAreCachedGlobally")
    {
        stack_trace::data st {};
        st.native_frames[0] = static_cast<stack_trace::native_frame_address>(0xCAFE);
        st.native_frames[1] = static_cast<stack_trace::native_frame_address>(0xBABE);
        st.native_frames[2] = static_cast<stack_trace::native_frame_address>(0xCAFE);
        st.native_frame_count = 3;

        REQUIRE(stack_trace::get_resolved_cache_size() == 0);

        auto resolved_first = stack_trace::resolve(st);

        REQUIRE(resolved_first.size() == 3);
        CHECK(stack_trace::get_resolved_cache_size() == 2);

        auto resolved_second = stack_trace::resolve(st);

        REQUIRE(resolved_second.size() == 3);
        CHECK(stack_trace::get_resolved_cache_size() == 2);
        CHECK(resolved_second[0].function == resolved_first[0].function);
        CHECK(resolved_second[1].function == resolved_first[1].function);
        CHECK(resolved_second[2].function == resolved_first[2].function);
    }

    SECTION("CaptureNativeStackFramesReportsNoTruncationForShallowStack")
    {
        // A normal capture inside a unit test thread is well below the 128-frame cap,
        // so the truncation flag must come back clean
        std::array<stack_trace::native_frame_address, stack_trace::MAX_NATIVE_FRAMES> frames {};
        uint32_t count = 0;
        bool truncated = true; // start with the wrong value to make sure capture clears it

        stack_trace::capture_native_frames(frames, count, truncated, 0);

        CHECK_FALSE(truncated);
#if FO_MEMORY_SANITIZER || FO_THREAD_SANITIZER
        CHECK(count == 0);
#else
        CHECK(count > 0);
        CHECK(count < stack_trace::MAX_NATIVE_FRAMES);
#endif
    }

    SECTION("ResumePointResolvesToTheStackOfItsSavingFrame")
    {
        CapturedFrames eager;
        CapturedFrames resolved;
        SaveCaptureAndResolve(eager, resolved);

        // A system unwinder may close the trace with a frame past the thread entry; the saved context stops at it
        uint32_t eager_count = eager.count;

        while (eager_count != 0 && eager.frames[eager_count - 1] == std::numeric_limits<stack_trace::native_frame_address>::max()) {
            eager_count--;
        }

        CHECK_FALSE(resolved.truncated);

        // Both lists start at the saving frame, at the two different calls made there, unless inlining folded a frame
        // away on one side, so they are compared at whichever offset lines them up
        uint32_t offset = resolved.count > 1 && eager_count != 0 && resolved.frames[1] == eager.frames[0] ? 1 : 0;

        REQUIRE(resolved.count == eager_count + offset);

        if (offset == 0 && resolved.count != 0) {
            CHECK(ResolveFunctionName(resolved.frames[0]) == ResolveFunctionName(eager.frames[0]));
        }

        for (uint32_t i = offset == 0 ? 1 : 0; i < eager_count; i++) {
            CHECK(resolved.frames[i + offset] == eager.frames[i]);
        }
    }

    SECTION("CaptureOverflowReadsBelowTheLayerFromItsBirthFrames")
    {
        // Both traces fill the cap from the top of deeper stacks, so they share no bottom and anchoring honestly
        // fails; the trace is read to its end and the stack below the layer comes from the birth capture
        stack_trace::data st {};
        st.native_frame_count = stack_trace::MAX_NATIVE_FRAMES;

        for (uint32_t i = 0; i < stack_trace::MAX_NATIVE_FRAMES; i++) {
            st.native_frames[i] = static_cast<stack_trace::native_frame_address>(0x2000 + i);
        }

        stack_trace::script_layer layer;
        layer.script_frames.push_back(MakeScriptFrame("OrphanedScript", "Scripts/Orphan.fos", 1));
        layer.birth_native_frame_count = stack_trace::MAX_NATIVE_FRAMES;

        // Birth uses a disjoint address range -> nothing aligns at the bottom
        for (uint32_t i = 0; i < stack_trace::MAX_NATIVE_FRAMES; i++) {
            layer.birth_native_frames[i] = static_cast<stack_trace::native_frame_address>(0x9000 + i);
        }

        std::vector<stack_trace::script_layer> layers;
        layers.push_back(std::move(layer));
        st.script_layers = std::make_shared<const std::vector<stack_trace::script_layer>>(std::move(layers));

        auto resolved = stack_trace::resolve(st);

        REQUIRE(resolved.size() == stack_trace::MAX_NATIVE_FRAMES * 2 + 1);

        for (uint32_t i = 0; i < stack_trace::MAX_NATIVE_FRAMES; i++) {
            CHECK(resolved[i].type == stack_trace::frame::frame_type::native);
        }

        CHECK(resolved[stack_trace::MAX_NATIVE_FRAMES].type == stack_trace::frame::frame_type::script);
        CHECK(resolved[stack_trace::MAX_NATIVE_FRAMES].function == "OrphanedScript");

        for (uint32_t i = stack_trace::MAX_NATIVE_FRAMES + 1; i < resolved.size(); i++) {
            CHECK(resolved[i].type == stack_trace::frame::frame_type::native);
        }
    }

    SECTION("TraceStoppedInsideScriptRuntimeContinuesAlongBirthFrames")
    {
        // An unwinder that cannot step through generated code ends the trace at the first such frame, so nothing
        // below the script entry is in the trace itself
        stack_trace::data st {};
        std::array<stack_trace::native_frame_address, 3> pcs {
            static_cast<stack_trace::native_frame_address>(0xA0), // native throw site
            static_cast<stack_trace::native_frame_address>(0xA1), // native function the script called
            static_cast<stack_trace::native_frame_address>(0xF0), // generated script code, where unwinding stopped
        };

        for (size_t i = 0; i < pcs.size(); i++) {
            st.native_frames[i] = pcs[i];
        }

        st.native_frame_count = static_cast<uint32_t>(pcs.size());

        stack_trace::script_layer layer;
        layer.script_frames.push_back(MakeScriptFrame("RuntimeScript", "Scripts/Runtime.cs", 5));
        layer.birth_native_frames[0] = static_cast<stack_trace::native_frame_address>(0xB0); // entry that invoked the script
        layer.birth_native_frames[1] = static_cast<stack_trace::native_frame_address>(0xB1);
        layer.birth_native_frames[2] = static_cast<stack_trace::native_frame_address>(0x80); // main()
        layer.birth_native_frame_count = 3;
        layer.runtime_native_frames.push_back(static_cast<stack_trace::native_frame_address>(0xF0));

        std::vector<stack_trace::script_layer> layers;
        layers.push_back(std::move(layer));
        st.script_layers = std::make_shared<const std::vector<stack_trace::script_layer>>(std::move(layers));

        auto resolved = stack_trace::resolve(st);
        auto native_name = [](stack_trace::native_frame_address addr) {
            stack_trace::data single {};
            single.native_frames[0] = addr;
            single.native_frame_count = 1;
            return stack_trace::resolve(single)[0].function;
        };

        // The generated-code frame is described by the script frames, so it is not printed as a raw address
        REQUIRE(resolved.size() == 6);
        CHECK(resolved[0].function == native_name(0xA0));
        CHECK(resolved[1].function == native_name(0xA1));
        CHECK(resolved[2].type == stack_trace::frame::frame_type::script);
        CHECK(resolved[2].function == "RuntimeScript");
        CHECK(resolved[3].function == native_name(0xB0));
        CHECK(resolved[4].function == native_name(0xB1));
        CHECK(resolved[5].function == native_name(0x80));
    }

    SECTION("RuntimeFramesPlaceScriptBetweenCalledNativesAndRuntimeEntry")
    {
        // An unwinder that walks generated code leaves it in the trace: the natives above it were called by script, the
        // natives below it are the runtime entering script
        stack_trace::data st {};
        std::array<stack_trace::native_frame_address, 7> pcs {
            static_cast<stack_trace::native_frame_address>(0xA0), // native function the script called
            static_cast<stack_trace::native_frame_address>(0xF0), // generated script code
            static_cast<stack_trace::native_frame_address>(0xF1), // generated script code
            static_cast<stack_trace::native_frame_address>(0xC0), // runtime invoke
            static_cast<stack_trace::native_frame_address>(0xB0), // entry that invoked the script
            static_cast<stack_trace::native_frame_address>(0xB1),
            static_cast<stack_trace::native_frame_address>(0x80), // main()
        };

        for (size_t i = 0; i < pcs.size(); i++) {
            st.native_frames[i] = pcs[i];
        }

        st.native_frame_count = static_cast<uint32_t>(pcs.size());

        stack_trace::script_layer layer;
        layer.script_frames.push_back(MakeScriptFrame("JitScript", "Scripts/Jit.cs", 3));
        layer.birth_native_frames[0] = static_cast<stack_trace::native_frame_address>(0xB0);
        layer.birth_native_frames[1] = static_cast<stack_trace::native_frame_address>(0xB1);
        layer.birth_native_frames[2] = static_cast<stack_trace::native_frame_address>(0x80);
        layer.birth_native_frame_count = 3;
        layer.runtime_native_frames.push_back(static_cast<stack_trace::native_frame_address>(0xF0));
        layer.runtime_native_frames.push_back(static_cast<stack_trace::native_frame_address>(0xF1));

        std::vector<stack_trace::script_layer> layers;
        layers.push_back(std::move(layer));
        st.script_layers = std::make_shared<const std::vector<stack_trace::script_layer>>(std::move(layers));

        auto resolved = stack_trace::resolve(st);
        auto native_name = [](stack_trace::native_frame_address addr) {
            stack_trace::data single {};
            single.native_frames[0] = addr;
            single.native_frame_count = 1;
            return stack_trace::resolve(single)[0].function;
        };

        REQUIRE(resolved.size() == 6);
        CHECK(resolved[0].function == native_name(0xA0));
        CHECK(resolved[1].type == stack_trace::frame::frame_type::script);
        CHECK(resolved[1].function == "JitScript");
        CHECK(resolved[2].function == native_name(0xC0));
        CHECK(resolved[3].function == native_name(0xB0));
        CHECK(resolved[4].function == native_name(0xB1));
        CHECK(resolved[5].function == native_name(0x80));
    }

    SECTION("UnwoundScriptFramesBecomeInnermostLayerAtCapturePoint")
    {
        stack_trace::data st {};
        st.native_frames[0] = static_cast<stack_trace::native_frame_address>(0xA0);
        st.native_frames[1] = static_cast<stack_trace::native_frame_address>(0xB0);
        st.native_frame_count = 2;

        stack_trace::script_layer outer = MakeLayer({MakeScriptFrame("OuterScript", "Scripts/Outer.cs", 1)});
        outer.birth_native_frames[0] = static_cast<stack_trace::native_frame_address>(0xB0);
        outer.birth_native_frame_count = 1;

        std::vector<stack_trace::script_layer> layers;
        layers.push_back(std::move(outer));
        st.script_layers = std::make_shared<const std::vector<stack_trace::script_layer>>(std::move(layers));

        stack_trace::add_unwound_script_frames(st, MakeLayer({MakeScriptFrame("ThrowSite", "Scripts/Thrown.cs", 9)}));

        REQUIRE(st.script_layers);
        REQUIRE(st.script_layers->size() == 2);
        CHECK((*st.script_layers)[0].birth_native_frame_count == 2);

        auto resolved = stack_trace::resolve(st);

        REQUIRE(resolved.size() == 4);
        CHECK(resolved[0].function == "ThrowSite");
        CHECK(resolved[1].type == stack_trace::frame::frame_type::native);
        CHECK(resolved[2].function == "OuterScript");
        CHECK(resolved[3].type == stack_trace::frame::frame_type::native);
    }

    SECTION("CaughtScriptFramesReplaceLiveFramesAboveTheCatchingFrame")
    {
        stack_trace::data st {};
        std::vector<stack_trace::script_layer> layers;
        layers.push_back(MakeLayer({
            MakeScriptFrame("ReportHelper", "Scripts/Report.cs", 1),
            MakeScriptFrame("Catcher", "Scripts/Catcher.cs", 20),
            MakeScriptFrame("Caller", "Scripts/Caller.cs", 30),
        }));
        st.script_layers = std::make_shared<const std::vector<stack_trace::script_layer>>(std::move(layers));

        stack_trace::splice_caught_script_frames(st,
            MakeLayer({
                MakeScriptFrame("ThrowSite", "Scripts/Thrown.cs", 5),
                MakeScriptFrame("Catcher", "Scripts/Catcher.cs", 18),
            }));

        REQUIRE(st.script_layers);
        REQUIRE(st.script_layers->size() == 1);

        const auto& frames = (*st.script_layers)[0].script_frames;
        REQUIRE(frames.size() == 3);
        CHECK(frames[0].function == "ThrowSite");
        CHECK(frames[1].function == "Catcher");
        CHECK(frames[1].line == 18);
        CHECK(frames[2].function == "Caller");
    }

    SECTION("CaughtScriptFramesWithoutLiveCatcherPrecedeTheLiveFrames")
    {
        stack_trace::data st {};
        std::vector<stack_trace::script_layer> layers;
        layers.push_back(MakeLayer({MakeScriptFrame("Unrelated", "Scripts/Unrelated.cs", 1)}));
        st.script_layers = std::make_shared<const std::vector<stack_trace::script_layer>>(std::move(layers));

        stack_trace::splice_caught_script_frames(st, MakeLayer({MakeScriptFrame("ThrowSite", "Scripts/Thrown.cs", 5)}));

        const auto& frames = (*st.script_layers)[0].script_frames;
        REQUIRE(frames.size() == 2);
        CHECK(frames[0].function == "ThrowSite");
        CHECK(frames[1].function == "Unrelated");
    }

    SECTION("TraceReachingIntoBirthFramesReadsTheSharedPartOnce")
    {
        stack_trace::data st {};
        std::array<stack_trace::native_frame_address, 3> pcs {
            static_cast<stack_trace::native_frame_address>(0xA0),
            static_cast<stack_trace::native_frame_address>(0xB0),
            static_cast<stack_trace::native_frame_address>(0xB1),
        };

        for (size_t i = 0; i < pcs.size(); i++) {
            st.native_frames[i] = pcs[i];
        }

        st.native_frame_count = static_cast<uint32_t>(pcs.size());

        stack_trace::script_layer layer;
        layer.script_frames.push_back(MakeScriptFrame("SharedScript", "Scripts/Shared.cs", 7));
        layer.birth_native_frames[0] = static_cast<stack_trace::native_frame_address>(0xB0);
        layer.birth_native_frames[1] = static_cast<stack_trace::native_frame_address>(0xB1);
        layer.birth_native_frames[2] = static_cast<stack_trace::native_frame_address>(0x80);
        layer.birth_native_frames[3] = static_cast<stack_trace::native_frame_address>(0x81);
        layer.birth_native_frame_count = 4;

        std::vector<stack_trace::script_layer> layers;
        layers.push_back(std::move(layer));
        st.script_layers = std::make_shared<const std::vector<stack_trace::script_layer>>(std::move(layers));

        auto resolved = stack_trace::resolve(st);

        REQUIRE(resolved.size() == 6);
        CHECK(resolved[0].type == stack_trace::frame::frame_type::native);
        CHECK(resolved[1].type == stack_trace::frame::frame_type::script);
        CHECK(resolved[1].function == "SharedScript");

        for (size_t i = 2; i < resolved.size(); i++) {
            CHECK(resolved[i].type == stack_trace::frame::frame_type::native);
        }
    }

    SECTION("LayerWithoutBirthAnchorEmitsScriptOnlyAndDeferNativeToTail")
    {
        // birth_native_frame_count == 0: layer is recorded but the resolver can't anchor it in
        // the native trace, so all native frames go after every script layer
        stack_trace::data st {};
        st.native_frames[0] = static_cast<stack_trace::native_frame_address>(0xCAFE);
        st.native_frames[1] = static_cast<stack_trace::native_frame_address>(0xBABE);
        st.native_frame_count = 2;

        std::vector<stack_trace::script_layer> layers;
        layers.push_back(MakeLayer({MakeScriptFrame("OnlyScript", "Scripts/Only.fos", 1)}));
        // No birth_native_frame_count set - left at default 0
        st.script_layers = std::make_shared<const std::vector<stack_trace::script_layer>>(std::move(layers));

        auto resolved = stack_trace::resolve(st);

        REQUIRE(resolved.size() == 3);
        CHECK(resolved[0].type == stack_trace::frame::frame_type::script);
        CHECK(resolved[1].type == stack_trace::frame::frame_type::native);
        CHECK(resolved[2].type == stack_trace::frame::frame_type::native);
    }

    SECTION("GetStackTraceEntryReturnsFramesByDepth")
    {
        ScopedScriptStackTraceProvider scope([](const stack_trace::data&, std::vector<stack_trace::script_layer>& layers) {
            layers.push_back(MakeLayer({
                MakeScriptFrame("DepthZero", "Scripts/Z.fos", 1),
                MakeScriptFrame("DepthOne", "Scripts/O.fos", 2),
            }));
        });

        auto top = stack_trace::get_entry(0);
        auto next = stack_trace::get_entry(1);

        REQUIRE(top.has_value());
        REQUIRE(next.has_value());
        CHECK(top->function == "DepthZero");
        CHECK(next->function == "DepthOne");
    }

    SECTION("GetStackTraceEntryReturnsNulloptForOutOfRange")
    {
        ScopedScriptStackTraceProvider scope([](const stack_trace::data&, std::vector<stack_trace::script_layer>& layers) { layers.push_back(MakeLayer({MakeScriptFrame("Only", "Scripts/Only.fos", 1)})); });

        auto missing = stack_trace::get_entry(10000);
        CHECK_FALSE(missing.has_value());
    }

    SECTION("FormatStackTraceWithNoFramesReturnsHeaderOnly")
    {
        stack_trace::data st {};

        auto formatted = stack_trace::format(st);

        CHECK(formatted == "Stack trace (most recent call first):");
    }

    SECTION("SafeWriteStackTraceWritesScriptAndNativeSections")
    {
        stack_trace::data st {};
        std::vector<stack_trace::script_layer> layers;
        layers.push_back(MakeLayer({
            MakeScriptFrame("FuncA", "/tmp/a.fos", 11),
            MakeScriptFrame("FuncB", "/tmp/b.fos", 22),
        }));
        st.script_layers = std::make_shared<const std::vector<stack_trace::script_layer>>(std::move(layers));

        std::ostringstream captured;
        std::streambuf* prev_buf = std::cout.rdbuf(captured.rdbuf());

        logging::safe_write_stack_trace(st);
        std::cout.rdbuf(prev_buf);

        std::string log_contents = captured.str();

        CHECK(log_contents.find("Stack trace (most recent call first):\n") == 0);
        CHECK(log_contents.find("- [Script] FuncA (a.fos line 11)\n") != std::string::npos);
        CHECK(log_contents.find("- [Script] FuncB (b.fos line 22)\n") != std::string::npos);
        CHECK(log_contents.ends_with("\n"));
    }

    SECTION("ProviderExceptionsDoNotEscape")
    {
        ScopedScriptStackTraceProvider scope([](const stack_trace::data&, std::vector<stack_trace::script_layer>&) {
            // A misbehaving provider must not crash the capture path even if it throws —
            // stack_trace::get defensively swallows the exception so the contract is preserved
            throw std::runtime_error("provider failure");
        });

        auto st = stack_trace::get();
        // Capture survived and produced a valid object. script_layers stays null because the
        // provider didn't append anything before throwing
        CHECK_FALSE(st.script_layers);
    }

    stack_trace::set_script_provider("Test", {});
    stack_trace::set_script_provider("TestOther", {});
    stack_trace::clear_resolved_cache();
}

FO_END_NAMESPACE
