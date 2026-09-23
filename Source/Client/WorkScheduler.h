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

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(WorkSchedulerException);

// Optional CPU parallelism, selected at startup by Client.WorkerThreads and never by a build option: one binary
// runs both modes, and zero workers starts no thread and builds no queue at all

// It schedules bounded batches of independent CPU items and nothing else. The application thread stays the sole
// owner of entities, scripts, input, GPU work and resource publication - see Docs/ClientMultithreading.md
class WorkScheduler final
{
public:
    // Asks the machine how many helpers to start instead of naming a number
    static constexpr int32_t AUTO_WORKER_THREADS = -1;
    // A client asking for more than this is a configuration mistake rather than a machine worth saturating
    static constexpr int32_t MAX_WORKER_THREADS = 64;

    struct Diagnostics
    {
        int32_t WorkerCount {};
        uint64_t ParallelBatches {};
        uint64_t ParallelItems {};
        uint64_t ParallelChunks {};
    };

    // 0 stays serial, AUTO asks the machine, a positive value is taken as written. Anything else throws: a client
    // that silently ran serial after being told to use eight workers would look enabled while doing nothing
    static auto ResolveWorkerCount(int32_t configured_worker_threads) -> int32_t;

    explicit WorkScheduler(string_view name, int32_t worker_count);
    WorkScheduler(const WorkScheduler&) = delete;
    WorkScheduler(WorkScheduler&&) noexcept = delete;
    auto operator=(const WorkScheduler&) -> WorkScheduler& = delete;
    auto operator=(WorkScheduler&&) noexcept -> WorkScheduler& = delete;
    ~WorkScheduler();

    [[nodiscard]] auto IsParallel() const noexcept -> bool { return _workerCount > 0; }
    [[nodiscard]] auto GetWorkerCount() const noexcept -> int32_t { return _workerCount; }
    [[nodiscard]] auto GetDiagnostics() const noexcept -> Diagnostics;

    // The branch an eligible stage takes: false means call the kernel directly, in the caller own loop, without
    // building a batch at all. Below the threshold a batch costs more than the work it spreads
    auto ShouldRunParallel(size_t item_count, size_t min_parallel_items) const noexcept -> bool;

    // Runs item_work over 0..item_count-1 across the workers and the calling thread, and returns only once every
    // item has completed. The first exception an item throws is rethrown here, on the owner, after the batch drained
    void RunBatch(string_view batch_name, size_t item_count, size_t min_items_per_chunk, const function<void(size_t)>& item_work);

private:
    struct Batch
    {
        nptr<const function<void(size_t)>> ItemWork {};
        size_t ItemCount {};
        size_t ChunkCount {};
        size_t ItemsPerChunk {};
    };

    void RunClaimedChunks(const Batch& batch) noexcept;
    void RunChunkItems(const Batch& batch, size_t chunk_index);
    void WorkerEntry(int32_t worker_index) noexcept;
    void StopWorkers() noexcept;

    string _name;
    int32_t _workerCount {};
    vector<thread> _workers {};

    // The batch is published under the mutex and copied by every worker the notify wakes; only the chunk cursor is
    // an atomic. It is closed once every participant left, so a late worker cannot claim into the batch after it
    mutable mutex _mutex {};
    std::condition_variable_any _workSignal {};
    mutable std::condition_variable_any _doneSignal {};
    Batch _batch FO_TSA_GUARDED_BY(_mutex) {};
    uint64_t _batchGeneration FO_TSA_GUARDED_BY(_mutex) {};
    int32_t _activeParticipants FO_TSA_GUARDED_BY(_mutex) {};
    bool _batchActive FO_TSA_GUARDED_BY(_mutex) {};
    bool _finish FO_TSA_GUARDED_BY(_mutex) {};
    std::atomic<size_t> _nextChunk {};
    std::atomic<bool> _batchFailed {};
    mutex _failureLocker {};
    std::exception_ptr _batchException FO_TSA_GUARDED_BY(_failureLocker) {};
    std::atomic<uint64_t> _parallelBatches {};
    std::atomic<uint64_t> _parallelItems {};
    std::atomic<uint64_t> _parallelChunks {};
};

FO_END_NAMESPACE
