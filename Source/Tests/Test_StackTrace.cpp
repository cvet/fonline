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
        explicit ScopedScriptStackTraceProvider(stack_trace::script_provider provider) noexcept { stack_trace::set_script_provider(std::move(provider)); }
        ScopedScriptStackTraceProvider(const ScopedScriptStackTraceProvider&) = delete;
        ScopedScriptStackTraceProvider(ScopedScriptStackTraceProvider&&) noexcept = delete;
        auto operator=(const ScopedScriptStackTraceProvider&) -> ScopedScriptStackTraceProvider& = delete;
        auto operator=(ScopedScriptStackTraceProvider&&) noexcept -> ScopedScriptStackTraceProvider& = delete;
        ~ScopedScriptStackTraceProvider() noexcept { stack_trace::set_script_provider({}); }
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
}

TEST_CASE("StackTrace")
{
    // Make sure no leaked provider from a prior test pollutes the suite
    stack_trace::set_script_provider({});
    stack_trace::clear_resolved_cache();

    SECTION("ProviderRegistrationIsObservable")
    {
        CHECK_FALSE(stack_trace::has_script_provider());

        stack_trace::set_script_provider([](std::vector<stack_trace::script_layer>&) { });
        CHECK(stack_trace::has_script_provider());

        stack_trace::set_script_provider({});
        CHECK_FALSE(stack_trace::has_script_provider());
    }

    SECTION("SingleLayerCapturesScriptFramesInProvidedOrder")
    {
        ScopedScriptStackTraceProvider scope([](std::vector<stack_trace::script_layer>& layers) {
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
        ScopedScriptStackTraceProvider scope([](std::vector<stack_trace::script_layer>& layers) {
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
        ScopedScriptStackTraceProvider scope([](std::vector<stack_trace::script_layer>& layers) { layers.push_back(MakeLayer({MakeScriptFrame("Boss", "Scripts/Boss.fos", 7)})); });

        auto formatted = stack_trace::format(stack_trace::get());

        CHECK(formatted.find("- [Script] Boss (Boss.fos line 7)") != std::string::npos);
    }

    SECTION("ResolveStackTracePlacesScriptBeforeNativeWhenNoBirthAnchor")
    {
        ScopedScriptStackTraceProvider scope([](std::vector<stack_trace::script_layer>& layers) {
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

    SECTION("CaptureOverflowDegradesGracefullyAndPushesScriptBeforeNatives")
    {
        // Both traces fill the cap from the top of deeper stacks, so they share no bottom and anchoring honestly
        // fails; the degraded output is deliberate rather than a guessed anchor
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

        REQUIRE(resolved.size() == stack_trace::MAX_NATIVE_FRAMES + 1);
        CHECK(resolved[0].type == stack_trace::frame::frame_type::script);
        CHECK(resolved[0].function == "OrphanedScript");

        for (uint32_t i = 1; i < resolved.size(); i++) {
            CHECK(resolved[i].type == stack_trace::frame::frame_type::native);
        }
    }

    SECTION("LayerWithoutBirthAnchorEmitsScriptOnlyAndDeferNativeToTail")
    {
        // BirthNativeFrameCount == 0: layer is recorded but the resolver can't anchor it in
        // the native trace, so all native frames go after every script layer
        stack_trace::data st {};
        st.native_frames[0] = static_cast<stack_trace::native_frame_address>(0xCAFE);
        st.native_frames[1] = static_cast<stack_trace::native_frame_address>(0xBABE);
        st.native_frame_count = 2;

        std::vector<stack_trace::script_layer> layers;
        layers.push_back(MakeLayer({MakeScriptFrame("OnlyScript", "Scripts/Only.fos", 1)}));
        // No BirthNativeFrameCount set - left at default 0
        st.script_layers = std::make_shared<const std::vector<stack_trace::script_layer>>(std::move(layers));

        auto resolved = stack_trace::resolve(st);

        REQUIRE(resolved.size() == 3);
        CHECK(resolved[0].type == stack_trace::frame::frame_type::script);
        CHECK(resolved[1].type == stack_trace::frame::frame_type::native);
        CHECK(resolved[2].type == stack_trace::frame::frame_type::native);
    }

    SECTION("GetStackTraceEntryReturnsFramesByDepth")
    {
        ScopedScriptStackTraceProvider scope([](std::vector<stack_trace::script_layer>& layers) {
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
        ScopedScriptStackTraceProvider scope([](std::vector<stack_trace::script_layer>& layers) { layers.push_back(MakeLayer({MakeScriptFrame("Only", "Scripts/Only.fos", 1)})); });

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
        ScopedScriptStackTraceProvider scope([](std::vector<stack_trace::script_layer>&) {
            // A misbehaving provider must not crash the capture path even if it throws —
            // stack_trace::get defensively swallows the exception so the contract is preserved
            throw std::runtime_error("provider failure");
        });

        auto st = stack_trace::get();
        // Capture survived and produced a valid object. script_layers stays null because the
        // provider didn't append anything before throwing
        CHECK_FALSE(st.script_layers);
    }

    stack_trace::set_script_provider({});
    stack_trace::clear_resolved_cache();
}

FO_END_NAMESPACE
