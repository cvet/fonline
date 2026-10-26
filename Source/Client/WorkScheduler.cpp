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

#include "WorkScheduler.h"

FO_BEGIN_NAMESPACE

auto WorkScheduler::ReadWorkerCountInputs() -> WorkerCountInputs
{
    return WorkerCountInputs {
        .LogicalCores = numeric_cast<int32_t>(std::thread::hardware_concurrency()),
        .ThreadsSupported = !FO_WEB,
        .MobileCpu = FO_ANDROID || FO_IOS,
    };
}

auto WorkScheduler::ChooseWorkerCount(const WorkerCountInputs& inputs, const WorkerCountLimits& limits) -> WorkerCountChoice
{
    // Checked before the machine is, so a bad cap fails on every platform and not only where it would bite
    FO_VERIFY_AND_THROW(limits.MaxWorkers >= 0 && limits.MaxWorkers <= MAX_WORKER_THREADS, "Worker count cap is outside the supported range", limits.MaxWorkers, MAX_WORKER_THREADS);
    FO_VERIFY_AND_THROW(limits.MaxMobileWorkers >= 0 && limits.MaxMobileWorkers <= MAX_WORKER_THREADS, "Mobile worker count cap is outside the supported range", limits.MaxMobileWorkers, MAX_WORKER_THREADS);
    FO_VERIFY_AND_THROW(limits.HeadroomMinCores >= 0, "Headroom core threshold must not be negative", limits.HeadroomMinCores);

    // The browser build is compiled without -pthread, so the switch can ask for workers there but not get them
    if (!inputs.ThreadsSupported) {
        return {.WorkerCount = 0, .Reason = "this build cannot start threads"};
    }
    if (inputs.LogicalCores <= 0) {
        return {.WorkerCount = 0, .Reason = "the machine did not report its core count"};
    }

    // The application thread runs a share of every batch itself, so it keeps a core of its own
    int32_t spare_cores = inputs.LogicalCores - 1;
    bool keeps_headroom = inputs.LogicalCores >= limits.HeadroomMinCores;

    if (keeps_headroom) {
        spare_cores--;
    }

    if (spare_cores <= 0) {
        return {.WorkerCount = 0, .Reason = "no core is left over beside the application thread"};
    }

    if (inputs.MobileCpu && limits.MaxMobileWorkers < limits.MaxWorkers && spare_cores > limits.MaxMobileWorkers) {
        return {.WorkerCount = limits.MaxMobileWorkers, .Reason = limits.MaxMobileWorkers == 0 ? "the mobile worker cap is zero" : "capped for a mobile CPU"};
    }
    if (spare_cores > limits.MaxWorkers) {
        return {.WorkerCount = limits.MaxWorkers, .Reason = limits.MaxWorkers == 0 ? "the worker cap is zero" : "capped at the worker maximum"};
    }

    if (keeps_headroom) {
        return {.WorkerCount = spare_cores, .Reason = "one core kept for the application thread and one for driver, audio and runtime"};
    }

    return {.WorkerCount = spare_cores, .Reason = "one core kept for the application thread"};
}

WorkScheduler::WorkScheduler(string_view name, int32_t worker_count) :
    _name {name},
    _workerCount {worker_count}
{
    FO_TRACE_ZONE(Threading);

    FO_VERIFY_AND_THROW(worker_count >= 0 && worker_count <= MAX_WORKER_THREADS, "Client work scheduler worker count is outside the supported range", name, worker_count, MAX_WORKER_THREADS);

    // Zero workers is the direct path and starts nothing: no thread, no queue, no scratch storage
    if (worker_count == 0) {
        return;
    }

    _workers.reserve(numeric_cast<size_t>(worker_count));

    try {
        for (int32_t i = 0; i < worker_count; i++) {
            _workers.emplace_back(run_thread(strex("{}-{}", _name, i), [this, i] { WorkerEntry(i); }));
        }
    }
    catch (...) {
        // The workers already started reference *this through their lambda and an unwinding constructor never runs
        // the destructor, so they are stopped and joined before this storage goes away
        StopWorkers();
        throw;
    }
}

WorkScheduler::~WorkScheduler()
{
    StopWorkers();
}

auto WorkScheduler::EstimateBackgroundStorage() -> size_t
{
    return sizeof(BackgroundState) + sizeof(shared_ptr_storage_block<uint8_t>) + MAX_BACKGROUND_TASKS * (sizeof(BackgroundWork) * 2 + sizeof(shared_ptr<BackgroundTask>));
}

auto WorkScheduler::CreateBackgroundGroup() -> shared_ptr<BackgroundGroup>
{
    FO_VERIFY_AND_THROW(std::this_thread::get_id() == _ownerThread, "Background groups belong to the scheduler owner");

    if (!IsParallel()) {
        return nullptr;
    }

    scoped_lock locker {_mutex};
    FO_VERIFY_AND_THROW(!_finish, "Client work scheduler is stopped", _name);
    FO_VERIFY_AND_THROW(_nextBackgroundGroupId != std::numeric_limits<uint64_t>::max(), "Background group sequence exhausted");

    if (!_background) {
        auto state = safe_alloc::make_unique<BackgroundState>();
        state->Identity = safe_alloc::make_shared<uint8_t>();
        state->Queued.reserve(MAX_BACKGROUND_TASKS);
        state->Active.reserve(MAX_BACKGROUND_TASKS);
        _background = std::move(state);
    }

    auto group = safe_alloc::make_shared<BackgroundGroup>();
    group->_id = ++_nextBackgroundGroupId;
    group->_identity = _background->Identity;
    return group;
}

auto WorkScheduler::TrySubmitBackground(shared_ptr<BackgroundGroup> group, function<void()> work) -> shared_ptr<BackgroundTask>
{
    FO_VERIFY_AND_THROW(std::this_thread::get_id() == _ownerThread, "Background submission belongs to the scheduler owner");

    if (!IsParallel()) {
        return nullptr;
    }

    FO_VERIFY_AND_THROW(group && work, "Background submission requires a group and owned work");
    shared_ptr<BackgroundTask> task;

    {
        scoped_lock locker {_mutex};
        FO_VERIFY_AND_THROW(_background && group->_identity == _background->Identity, "Background group belongs to another scheduler");
        FO_VERIFY_AND_THROW(!_batchActive, "Background submission cannot nest inside a foreground batch");

        if (_finish || group->_cancelled.load(std::memory_order_acquire) || _background->Queued.size() + _background->Active.size() >= MAX_BACKGROUND_TASKS) {
            return nullptr;
        }

        FO_VERIFY_AND_THROW(_nextBackgroundTaskId != std::numeric_limits<uint64_t>::max(), "Background task sequence exhausted");
        task = safe_alloc::make_shared<BackgroundTask>();
        task->_id = ++_nextBackgroundTaskId;
        task->_group = group;

        _background->Queued.push_back({task, std::move(work)});
        group->_pending.fetch_add(1, std::memory_order_release);
    }

    _workSignal.notify_one();
    return task;
}

auto WorkScheduler::PollBackground(const shared_ptr<BackgroundTask>& task) const -> BackgroundResult
{
    FO_VERIFY_AND_THROW(std::this_thread::get_id() == _ownerThread, "Background polling belongs to the scheduler owner");
    FO_VERIFY_AND_THROW(task, "Background task is absent");
    BackgroundStatus status = task->_status.load(std::memory_order_acquire);

    // The completion release publishes the exception and the owned work's writes together
    return {status, status == BackgroundStatus::Completed ? task->_exception : std::exception_ptr {}};
}

void WorkScheduler::CancelBackground(shared_ptr<BackgroundGroup> group)
{
    FO_TRACE_ZONE(Threading);

    FO_VERIFY_AND_THROW(std::this_thread::get_id() == _ownerThread, "Background cancellation belongs to the scheduler owner");
    FO_VERIFY_AND_THROW(group, "Background group is absent");
    vector<BackgroundWork> cancelled;
    cancelled.reserve(MAX_BACKGROUND_TASKS);

    {
        scoped_lock locker {_mutex};
        FO_VERIFY_AND_THROW(_background && group->_identity == _background->Identity, "Background group belongs to another scheduler");
        group->_cancelled.store(true, std::memory_order_release);

        for (auto it = _background->Queued.begin(); it != _background->Queued.end();) {
            if (it->Task->_group == group) {
                cancelled.emplace_back(std::move(*it));
                it = _background->Queued.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    RetireCancelledWork(std::move(cancelled));
    _doneSignal.notify_all();
}

void WorkScheduler::DrainBackground(const shared_ptr<BackgroundGroup>& group)
{
    FO_TRACE_ZONE(Threading);

    FO_VERIFY_AND_THROW(std::this_thread::get_id() == _ownerThread, "Background drain belongs to the scheduler owner");
    FO_VERIFY_AND_THROW(group, "Background group is absent");
    unique_lock locker {_mutex};
    FO_VERIFY_AND_THROW(_background && group->_identity == _background->Identity, "Background group belongs to another scheduler");
    FO_VERIFY_AND_THROW(!_batchActive, "Background drain cannot nest inside a foreground batch");

    while (group->_pending.load(std::memory_order_acquire) != 0) {
        _doneSignal.wait(locker);
    }
}

auto WorkScheduler::GetDiagnostics() const noexcept -> Diagnostics
{
    return Diagnostics {
        .WorkerCount = _workerCount,
        .ParallelBatches = _parallelBatches.load(std::memory_order_relaxed),
        .ParallelItems = _parallelItems.load(std::memory_order_relaxed),
        .ParallelChunks = _parallelChunks.load(std::memory_order_relaxed),
    };
}

auto WorkScheduler::ShouldRunParallel(size_t item_count, size_t min_parallel_items) const noexcept -> bool
{
    return _workerCount > 0 && item_count >= std::max<size_t>(min_parallel_items, 2);
}

void WorkScheduler::RunBatch(string_view batch_name, size_t item_count, size_t min_items_per_chunk, const function<void(size_t)>& item_work)
{
    FO_TRACE_ZONE(Threading);

    FO_VERIFY_AND_THROW(std::this_thread::get_id() == _ownerThread, "Client work batches belong to the scheduler owner", batch_name);
    FO_VERIFY_AND_THROW(item_work, "Client work batch has no item work", batch_name);

    if (item_count == 0) {
        return;
    }

    // A caller that reached here without asking ShouldRunParallel still gets its items run, in order, right here
    if (_workerCount == 0) {
        for (size_t item_index = 0; item_index < item_count; item_index++) {
            item_work(item_index);
        }

        return;
    }

    // Chunks are coarse on purpose: at most one per participating thread, and never below the caller own minimum,
    // so scheduling cost stays proportional to threads rather than to items
    size_t chunk_limit = std::max<size_t>(item_count / std::max<size_t>(min_items_per_chunk, 1), 1);
    size_t chunk_count = std::min(numeric_cast<size_t>(_workerCount) + 1, chunk_limit);
    size_t items_per_chunk = (item_count + chunk_count - 1) / chunk_count;
    Batch batch {
        .ItemWork = &item_work,
        .ItemCount = item_count,
        .ChunkCount = chunk_count,
        .ItemsPerChunk = items_per_chunk,
    };

#if FO_TRACE_CATEGORY_ENABLED(Threading)
    string batch_metadata = strex("items={};chunks={};workers={};minItemsPerChunk={}", item_count, chunk_count, _workerCount, min_items_per_chunk);
    ZoneText(batch_metadata.c_str(), batch_metadata.length());
#endif

    {
        scoped_lock locker {_mutex};

        FO_VERIFY_AND_THROW(!_batchActive, "Client work batch is already running", batch_name, _name);
        FO_VERIFY_AND_THROW(!_finish, "Client work scheduler is stopped", batch_name, _name);

        _nextChunk.store(0, std::memory_order_relaxed);
        _batchFailed.store(false, std::memory_order_relaxed);
        _batch = batch;
        _batchActive = true;
        _batchGeneration++;
    }

    _workSignal.notify_all();

    // The owner participates rather than supervises: with slow or busy workers it runs the whole batch itself, so
    // no chunk is ever left unclaimed
    RunClaimedChunks(batch);

    {
        FO_TRACE_ZONE_NAMED(Threading, "ClientWorkBatchJoin");

        unique_lock locker {_mutex};

        {
            FO_TRACE_ZONE_NAMED(Threading, "ClientWorkBatchWait");

            while (_activeParticipants != 0) {
                _doneSignal.wait(locker);
            }
        }

        // Closing the batch before the next one opens is what stops a worker that woke late from claiming a chunk
        // index against the batch after it
        _batchActive = false;
        _batch = Batch {};
    }

    _parallelBatches.fetch_add(1, std::memory_order_relaxed);
    _parallelItems.fetch_add(numeric_cast<uint64_t>(item_count), std::memory_order_relaxed);
    _parallelChunks.fetch_add(numeric_cast<uint64_t>(chunk_count), std::memory_order_relaxed);

    if (_batchFailed.load(std::memory_order_acquire)) {
        std::exception_ptr batch_exception;

        {
            scoped_lock locker {_failureLocker};

            batch_exception = std::exchange(_batchException, {});
        }

        _batchFailed.store(false, std::memory_order_relaxed);

        // Reported once, at the owner own consumption point, with the failing item exception intact
        FO_VERIFY_AND_THROW(batch_exception, "Client work batch reported a failure without an exception", batch_name, _name);

        std::rethrow_exception(batch_exception);
    }
}

void WorkScheduler::RunClaimedChunks(const Batch& batch) noexcept
{
    FO_TRACE_ZONE(Threading);

    while (true) {
        size_t chunk_index = _nextChunk.fetch_add(1, std::memory_order_acq_rel);

        if (chunk_index >= batch.ChunkCount) {
            return;
        }

        // A failed item ends the batch for everyone: the remaining chunks would only run against state the owner is
        // about to unwind anyway
        if (_batchFailed.load(std::memory_order_acquire)) {
            return;
        }

        try {
            RunChunkItems(batch, chunk_index);
        }
        catch (const std::exception& ex) {
            ignore_unused(ex);

            scoped_lock locker {_failureLocker};

            if (!_batchException) {
                _batchException = std::current_exception();
                _batchFailed.store(true, std::memory_order_release);
            }
        }
        catch (...) {
            FO_UNKNOWN_EXCEPTION();
        }
    }
}

void WorkScheduler::RunChunkItems(const Batch& batch, size_t chunk_index)
{
    size_t first_item = chunk_index * batch.ItemsPerChunk;
    size_t last_item = std::min(first_item + batch.ItemsPerChunk, batch.ItemCount);

    for (size_t item_index = first_item; item_index < last_item; item_index++) {
        (*batch.ItemWork)(item_index);
    }
}

void WorkScheduler::WorkerEntry(int32_t worker_index) noexcept
{
    set_this_thread_name(strex("{}-{}", _name, worker_index));

    uint64_t last_generation = 0;

    while (true) {
        Batch batch;
        optional<BackgroundWork> background_work;

        {
            unique_lock locker {_mutex};

            while (!_finish && (!_batchActive || _batchGeneration == last_generation) && (!_background || _background->Queued.empty())) {
                _workSignal.wait(locker);
            }

            if (_finish) {
                return;
            }

            if (_batchActive && _batchGeneration != last_generation) {
                last_generation = _batchGeneration;
                batch = _batch;
                _activeParticipants++;
            }
            else {
                background_work.emplace(std::move(_background->Queued.front()));
                _background->Queued.erase(_background->Queued.begin());
                _background->Active.emplace_back(background_work->Task);
                background_work->Task->_status.store(BackgroundStatus::Running, std::memory_order_release);
            }
        }

        if (background_work) {
            RunBackgroundWork(std::move(*background_work));
            continue;
        }

        RunClaimedChunks(batch);

        {
            scoped_lock locker {_mutex};

            _activeParticipants--;
        }

        _doneSignal.notify_all();
    }
}

void WorkScheduler::RunBackgroundWork(BackgroundWork work) noexcept
{
    FO_TRACE_ZONE(Threading);

    auto task = work.Task;

    if (!task->_group->_cancelled.load(std::memory_order_acquire)) {
        try {
            work.Work();
        }
        catch (const std::exception& ex) {
            ignore_unused(ex);
            task->_exception = std::current_exception();
        }
        catch (...) {
            FO_UNKNOWN_EXCEPTION();
        }
    }

    // Captures can own large payloads; release them before reporting quiescence and outside the scheduler mutex
    work.Work = nullptr;

    {
        scoped_lock locker {_mutex};
        auto it = std::find(_background->Active.begin(), _background->Active.end(), task);
        FO_STRONG_ASSERT(it != _background->Active.end(), "Active background task is absent");
        _background->Active.erase(it);

        bool cancelled = task->_group->_cancelled.load(std::memory_order_acquire);
        task->_status.store(cancelled ? BackgroundStatus::Cancelled : BackgroundStatus::Completed, std::memory_order_release);
        task->_group->_pending.fetch_sub(1, std::memory_order_release);
    }

    _doneSignal.notify_all();
}

void WorkScheduler::RetireCancelledWork(vector<BackgroundWork> work) noexcept
{
    for (auto& item : work) {
        item.Work = nullptr;
        item.Task->_status.store(BackgroundStatus::Cancelled, std::memory_order_release);
        item.Task->_group->_pending.fetch_sub(1, std::memory_order_release);
    }
}

void WorkScheduler::StopWorkers() noexcept
{
    FO_TRACE_ZONE(Threading);

    vector<BackgroundWork> cancelled;

    {
        scoped_lock locker {_mutex};

        _finish = true;

        if (_background) {
            for (auto& task : _background->Active) {
                task->_group->_cancelled.store(true, std::memory_order_release);
            }

            cancelled = std::move(_background->Queued);

            for (auto& work : cancelled) {
                work.Task->_group->_cancelled.store(true, std::memory_order_release);
            }
        }
    }

    RetireCancelledWork(std::move(cancelled));
    _workSignal.notify_all();

    for (auto& worker : _workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    _workers.clear();
}

FO_END_NAMESPACE

