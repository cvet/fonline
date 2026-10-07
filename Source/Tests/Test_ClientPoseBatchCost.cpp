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

#include "Common.h"

#if FO_ENABLE_3D

#include "ModelAnimation.h"
#include "ModelAnimationData.h"
#include "WorkScheduler.h"

#include <json.hpp>

FO_BEGIN_NAMESPACE

static auto ReadPoseBatchCostInteger(const nlohmann::json& input, string_view key, int32_t minimum, int32_t maximum) -> int32_t
{
    const auto& value = input.at(std::string {key});
    REQUIRE(value.is_number_integer());
    int64_t number = value.get<int64_t>();
    REQUIRE(number >= minimum);
    REQUIRE(number <= maximum);
    return numeric_cast<int32_t>(number);
}

static auto MakePoseBatchCostPoses(ptr<const ModelAnimationRuntimeRig> rig, size_t count) -> vector<unique_ptr<ModelAnimationRuntimePose>>
{
    vector<unique_ptr<ModelAnimationRuntimePose>> poses;
    poses.reserve(count);

    for (size_t index = 0; index < count; index++) {
        poses.emplace_back(safe_alloc::make_unique<ModelAnimationRuntimePose>(rig));
    }

    return poses;
}

static void EvaluatePoseBatchCostFrame(vector<unique_ptr<ModelAnimationRuntimePose>>& poses, const ModelAnimationRuntimeRig& rig, int32_t clip_index, uint32_t frame, nptr<WorkScheduler> scheduler)
{
    float32_t duration = rig.GetClip(numeric_cast<size_t>(clip_index)).GetDuration();
    auto evaluate_one = [&](size_t index) {
        float32_t phase = numeric_cast<float32_t>((frame + numeric_cast<uint32_t>(index) * 17) % 240) / 240.0f;
        array<ModelAnimationRuntimePose::TrackInput, 2> body_tracks {};
        array<ModelAnimationRuntimePose::TrackInput, 2> movement_tracks {};
        body_tracks[0] = ModelAnimationRuntimePose::TrackInput {.ClipIndex = clip_index, .Enabled = true, .Position = phase * duration, .Weight = 1.0f};
        poses[index]->Evaluate(body_tracks, movement_tracks, mat44 {1.0f});
    };

    if (scheduler) {
        scheduler->RunBatch("PoseBatchCost", poses.size(), 1, evaluate_one);
    }
    else {
        for (size_t index = 0; index < poses.size(); index++) {
            evaluate_one(index);
        }
    }
}

static auto CheckPoseBatchCostOutputs(const vector<unique_ptr<ModelAnimationRuntimePose>>& actual, const vector<unique_ptr<ModelAnimationRuntimePose>>& expected) -> uint64_t
{
    uint64_t fingerprint = 14695981039346656037ULL;

    for (size_t index = 0; index < actual.size(); index++) {
        const_span<mat44> actual_matrices = actual[index]->GetWorldMatrices();
        const_span<mat44> expected_matrices = expected[index]->GetWorldMatrices();
        REQUIRE(actual_matrices.size() == expected_matrices.size());
        CAPTURE(index);
        REQUIRE(std::memcmp(actual_matrices.data(), expected_matrices.data(), actual_matrices.size_bytes()) == 0);

        for (const mat44& matrix : actual_matrices) {
            for (uint8_t byte : std::bit_cast<array<uint8_t, sizeof(mat44)>>(matrix)) {
                fingerprint = (fingerprint ^ byte) * 1099511628211ULL;
            }
        }
    }

    return fingerprint;
}

TEST_CASE("ClientPoseBatchCost", "[.]")
{
    optional<string> input_text = fs::read_file_bounded("ClientPoseBatchCost.json", 4096);
    REQUIRE(input_text);
    nlohmann::json input = nlohmann::json::parse(input_text->begin(), input_text->end());
    REQUIRE(input.is_object());
    REQUIRE(input.size() == 9);
    REQUIRE(ReadPoseBatchCostInteger(input, "schema", 1, 1) == 1);
    int32_t pose_count = ReadPoseBatchCostInteger(input, "poseCount", 1, 1024);
    int32_t workers = ReadPoseBatchCostInteger(input, "workers", 0, WorkScheduler::MAX_WORKER_THREADS);
    int32_t clip_index = ReadPoseBatchCostInteger(input, "clipIndex", 0, MODEL_ANIMATION_RIG_MAX_CLIPS - 1);
    int32_t samples = ReadPoseBatchCostInteger(input, "samples", 2, 2000);
    int32_t iterations = ReadPoseBatchCostInteger(input, "iterations", 1, 64);
    int32_t warmup = ReadPoseBatchCostInteger(input, "warmup", 1, 512);
    REQUIRE(numeric_cast<int64_t>(pose_count) * samples * iterations <= 4000000);
    REQUIRE(input.at("rigFile").is_string());
    REQUIRE(input.at("nearestSampling").is_boolean());
    REQUIRE((workers == 0 || WorkScheduler::ReadWorkerCountInputs().ThreadsSupported));

    std::string rig_file = input.at("rigFile").get<std::string>();
    optional<string> rig_file_data = fs::read_file_bounded(rig_file, 256 * 1024 * 1024);
    REQUIRE(rig_file_data);
    const_span<uint8_t> rig_bytes {reinterpret_cast<const uint8_t*>(rig_file_data->data()), rig_file_data->size()};
    ModelAnimationRigData rig_data = ReadModelAnimationRigData(rig_bytes, rig_file);
    unique_ptr<ModelAnimationRuntimeRig> rig = LoadModelAnimationRuntimeRig(rig_bytes, rig_data.Skeleton.Metadata.SourceAsset, rig_data.BaseJointRemap.Metadata.SourceAsset, input.at("nearestSampling").get<bool>());
    REQUIRE(numeric_cast<size_t>(clip_index) < rig->GetClipCount());
    REQUIRE(rig->GetJointCount() > 0);

    vector<unique_ptr<ModelAnimationRuntimePose>> serial_poses = MakePoseBatchCostPoses(rig.get(), numeric_cast<size_t>(pose_count));
    vector<unique_ptr<ModelAnimationRuntimePose>> batch_poses = MakePoseBatchCostPoses(rig.get(), numeric_cast<size_t>(pose_count));
    WorkScheduler scheduler {"pose-batch-cost", workers};
    nptr<WorkScheduler> batch_scheduler = workers != 0 ? &scheduler : nullptr;

    for (uint32_t frame = 0; frame < numeric_cast<uint32_t>(warmup); frame++) {
        EvaluatePoseBatchCostFrame(serial_poses, *rig, clip_index, frame, nullptr);
        EvaluatePoseBatchCostFrame(batch_poses, *rig, clip_index, frame, batch_scheduler);
        (void)CheckPoseBatchCostOutputs(batch_poses, serial_poses);
    }

    string csv = "sample,first,serial_ns,batch_ns,output_hash\n";
    uint64_t last_fingerprint = 0;

    for (int32_t sample = 0; sample < samples; sample++) {
        auto measure = [&](vector<unique_ptr<ModelAnimationRuntimePose>>& poses, nptr<WorkScheduler> execution) -> int64_t {
            auto started = std::chrono::steady_clock::now();

            for (int32_t iteration = 0; iteration < iterations; iteration++) {
                uint32_t frame = numeric_cast<uint32_t>(warmup + sample * iterations + iteration);
                EvaluatePoseBatchCostFrame(poses, *rig, clip_index, frame, execution);
            }

            return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count();
        };
        int64_t serial_ns;
        int64_t batch_ns;

        // Alternate which lane gets the warmed cache; both consume the same fixed sequence of animation times
        if (sample % 2 == 0) {
            serial_ns = measure(serial_poses, nullptr);
            batch_ns = measure(batch_poses, batch_scheduler);
        }
        else {
            batch_ns = measure(batch_poses, batch_scheduler);
            serial_ns = measure(serial_poses, nullptr);
        }

        REQUIRE(serial_ns > 0);
        REQUIRE(batch_ns > 0);
        last_fingerprint = CheckPoseBatchCostOutputs(batch_poses, serial_poses);
        std::format_to(std::back_inserter(csv), "{},{},{},{},{:016x}\n", sample, sample % 2 == 0 ? "serial" : "batch", serial_ns, batch_ns, last_fingerprint);
    }

    WorkScheduler::Diagnostics diagnostics = scheduler.GetDiagnostics();
    uint64_t expected_batches = workers != 0 ? numeric_cast<uint64_t>(warmup + samples * iterations) : 0;
    REQUIRE(diagnostics.ParallelBatches == expected_batches);
    REQUIRE(diagnostics.ParallelItems == expected_batches * numeric_cast<uint64_t>(pose_count));
    bool samples_written = fs::write_file("samples.csv", csv);
    REQUIRE(samples_written);
    string result;
    std::format_to(std::back_inserter(result), "{{\"schema\":1,\"joints\":{},\"clips\":{},\"clipIndex\":{},\"rigSignature\":{},\"cacheSignature\":{},\"workers\":{},\"poseCount\":{},\"samples\":{},\"iterations\":{},\"warmup\":{},\"parallelBatches\":{},\"parallelItems\":{},\"lastOutputHash\":\"{:016x}\"}}\n", rig->GetJointCount(), rig->GetClipCount(), clip_index, rig->GetRigSignature(), rig->GetCacheSignature(), workers, pose_count, samples, iterations, warmup, diagnostics.ParallelBatches, diagnostics.ParallelItems, last_fingerprint);
    bool result_written = fs::write_file("result.json", result);
    REQUIRE(result_written);
}

FO_END_NAMESPACE

#endif
