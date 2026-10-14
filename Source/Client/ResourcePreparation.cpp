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

#include "ResourcePreparation.h"

FO_BEGIN_NAMESPACE

ResourcePreparation::ResourcePreparation(ptr<WorkScheduler> scheduler) :
    ResourcePreparation(scheduler, Limits {}, ReadSpriteResource)
{
}

ResourcePreparation::ResourcePreparation(ptr<WorkScheduler> scheduler, Limits limits, Decoder decoder) :
    _scheduler {scheduler},
    _limits {limits},
    _decoder {safe_alloc::make_shared<Decoder>(std::move(decoder))}
{
    FO_VERIFY_AND_THROW(scheduler->IsParallel(), "Resource preparation requires existing workers");
    FO_VERIFY_AND_THROW(limits.Requests > 0 && limits.Requests <= WorkScheduler::MAX_BACKGROUND_TASKS, "Invalid resource preparation request limit", limits.Requests);
    FO_VERIFY_AND_THROW(*_decoder, "Resource preparation requires a CPU decoder");
    _jobs.reserve(limits.Requests);
}

ResourcePreparation::~ResourcePreparation()
{
    Shutdown();
}

void ResourcePreparation::ValidateOwner() const
{
    FO_VERIFY_AND_THROW(std::this_thread::get_id() == _ownerThread, "Resource preparation belongs to its constructing thread");
}

auto ResourcePreparation::GetDiagnostics() const -> Diagnostics
{
    ValidateOwner();
    return {_jobs.size(), _reservedBytes, _submitted, _consumed};
}

auto ResourcePreparation::FindRequest(const Key& key) const -> uint64_t
{
    ValidateOwner();

    for (const auto& job : _jobs) {
        const auto& identity = job->Identity;

        if (!job->Cancelled.load(std::memory_order_acquire) && identity.Path == key.Path && identity.Type == key.Type && identity.AtlasIntent == key.AtlasIntent && identity.Generation == key.Generation && identity.Epoch == key.Epoch) {
            return job->Ticket;
        }
    }

    return 0;
}

auto ResourcePreparation::Poll(uint64_t ticket) const -> Completion
{
    ValidateOwner();

    for (const auto& job : _jobs) {
        if (job->Ticket != ticket) {
            continue;
        }

        if (job->Cancelled.load(std::memory_order_acquire)) {
            return Completion::Cancelled;
        }

        if (!IsFinished(*job)) {
            return Completion::Pending;
        }

        if (job->AcquisitionError || (job->Task && _scheduler->PollBackground(job->Task).Exception)) {
            return Completion::Failed;
        }

        if (job->Missing) {
            return Completion::Missing;
        }

        return job->BudgetDeclined ? Completion::BudgetDeclined : Completion::Ready;
    }

    return Completion::Stale;
}

auto ResourcePreparation::EstimateReservation(const Key& key, const_span<uint8_t> bytes, size_t capacity) const -> optional<pair<size_t, size_t>>
{
    if (key.Path.size() > _limits.PathBytes || bytes.size() > _limits.EncodedBytes) {
        return std::nullopt;
    }

    auto decoded = EstimateSpriteResourceStorage(bytes, _limits.DecodedBytes);

    if (!decoded) {
        return std::nullopt;
    }

    size_t reservation = sizeof(shared_ptr_storage_block<Job>) + sizeof(shared_ptr_storage_block<Decoder>);
    reservation += sizeof(shared_ptr_storage_block<WorkScheduler::BackgroundTask>) + sizeof(shared_ptr_storage_block<WorkScheduler::BackgroundGroup>);
    reservation += WorkScheduler::EstimateBackgroundStorage() + _jobs.capacity() * sizeof(shared_ptr<Job>);
    auto add = [&](size_t size) {
        if (reservation > _limits.Bytes || size > _limits.Bytes - reservation) {
            return false;
        }

        reservation += size;
        return true;
    };

    // Shared scheduler/lane tables are charged in every slot, conservatively including the first admission
    if (!add(key.Path.capacity()) || !add(1) || !add(capacity) || !add(*decoded) || reservation > _limits.Bytes - _reservedBytes) {
        return std::nullopt;
    }

    return pair {reservation, *decoded};
}

auto ResourcePreparation::Request(Key key, const_span<uint8_t> bytes) -> RequestResult
{
    ValidateOwner();
    ReapCancelled();

    if (_stopped) {
        return {.Status = Admission::Stopped};
    }

    if (key.Generation <= _cancelledThroughGeneration) {
        return {.Status = Admission::Stale};
    }

    if (uint64_t ticket = FindRequest(key); ticket != 0) {
        return {Admission::Coalesced, ticket};
    }

    if (_jobs.size() == _limits.Requests) {
        return {.Status = Admission::Full};
    }

    if (!EstimateReservation(key, bytes, bytes.size())) {
        return {.Status = Admission::BudgetDeclined};
    }

    Input input;
    input.Identity = std::move(key);
    input.Bytes.resize(bytes.size());
    std::copy(bytes.begin(), bytes.end(), input.Bytes.begin());
    return Request(std::move(input));
}

auto ResourcePreparation::Request(Input input) -> RequestResult
{
    FO_TRACE_ZONE(Threading);

    ValidateOwner();
    ReapCancelled();

    if (_stopped) {
        return {.Status = Admission::Stopped};
    }

    if (input.Identity.Generation <= _cancelledThroughGeneration) {
        return {.Status = Admission::Stale};
    }

    if (uint64_t ticket = FindRequest(input.Identity); ticket != 0) {
        return {Admission::Coalesced, ticket};
    }

    if (_jobs.size() == _limits.Requests) {
        return {.Status = Admission::Full};
    }

    auto estimate = EstimateReservation(input.Identity, input.Bytes, input.Bytes.capacity());

    if (!estimate) {
        return {.Status = Admission::BudgetDeclined};
    }

    FO_VERIFY_AND_THROW(_nextTicket != std::numeric_limits<uint64_t>::max(), "Resource preparation ticket sequence exhausted");
    auto job = safe_alloc::make_shared<Job>();
    job->Identity = std::move(input.Identity);
    job->Bytes = std::move(input.Bytes);
    job->Missing = input.Missing;
    job->AcquisitionError = std::move(input.AcquisitionError);
    job->Ticket = ++_nextTicket;
    job->Reservation = estimate->first;

    if (!job->Missing && !job->AcquisitionError) {
        for (auto& previous : _jobs) {
            if (!previous->Cancelled.load(std::memory_order_acquire) && previous->Identity.Generation == job->Identity.Generation && previous->Identity.Epoch == job->Identity.Epoch && previous->Group) {
                job->Group = previous->Group;
                break;
            }
        }

        if (!job->Group) {
            job->Group = _scheduler->CreateBackgroundGroup();
        }

        auto decoder = _decoder;
        size_t decoded_reservation = estimate->second;
        auto work = [job, decoder, decoded_reservation]() mutable {
            if (job->Cancelled.load(std::memory_order_acquire)) {
                return;
            }

            auto data = (*decoder)(job->Bytes);

            if (MeasureSpriteResourceStorage(data, decoded_reservation)) {
                job->Data.emplace(std::move(data));
            }
            else {
                job->BudgetDeclined = true;
            }
        };
        static_assert(sizeof(work) <= FUNCTION_INLINE_TARGET_SIZE);
        static_assert(std::is_nothrow_move_constructible_v<decltype(work)>);
        job->Task = _scheduler->TrySubmitBackground(job->Group, std::move(work));

        if (!job->Task) {
            return {.Status = Admission::Full};
        }
    }

    _jobs.emplace_back(job);
    _reservedBytes += job->Reservation;
    _submitted++;
    return {Admission::Accepted, job->Ticket};
}

auto ResourcePreparation::Consume(uint64_t ticket, const function<void(SpriteResourceData&&)>& publish) -> bool
{
    FO_TRACE_ZONE(Threading);

    ValidateOwner();
    Completion completion = Poll(ticket);

    if (completion == Completion::Pending || completion == Completion::Cancelled || completion == Completion::Stale || completion == Completion::BudgetDeclined) {
        Release(ticket);
        return false;
    }

    auto it = std::find_if(_jobs.begin(), _jobs.end(), [ticket](const auto& job) { return job->Ticket == ticket; });
    FO_STRONG_ASSERT(it != _jobs.end(), "Completed preparation record is absent");
    auto job = *it;
    job->Publishing = true;
    auto release = scope_exit([&]() noexcept {
        job->Publishing = false;
        Release(ticket);
    });

    if (completion == Completion::Failed) {
        auto error = job->AcquisitionError ? job->AcquisitionError : _scheduler->PollBackground(job->Task).Exception;
        std::rethrow_exception(error);
    }

    if (completion == Completion::Ready) {
        FO_STRONG_ASSERT(job->Data.has_value(), "Prepared resource has no decoded data");
        publish(std::move(*job->Data));
    }

    _consumed++;
    return true;
}

void ResourcePreparation::Drain()
{
    FO_TRACE_ZONE(Threading);

    ValidateOwner();

    for (auto& job : _jobs) {
        if (job->Group) {
            _scheduler->DrainBackground(job->Group);
        }
    }

    ReapCancelled();
}

void ResourcePreparation::Release(uint64_t ticket)
{
    ValidateOwner();

    for (auto& job : _jobs) {
        if (job->Ticket == ticket) {
            job->Cancelled.store(true, std::memory_order_release);
            break;
        }
    }

    ReapCancelled();
}

void ResourcePreparation::CancelGeneration(uint64_t generation)
{
    ValidateOwner();
    _cancelledThroughGeneration = std::max(_cancelledThroughGeneration, generation);

    for (auto& job : _jobs) {
        if (job->Identity.Generation <= generation) {
            job->Cancelled.store(true, std::memory_order_release);

            if (job->Group) {
                _scheduler->CancelBackground(job->Group);
            }
        }
    }

    ReapCancelled();
}

void ResourcePreparation::CancelAll()
{
    ValidateOwner();

    for (auto& job : _jobs) {
        job->Cancelled.store(true, std::memory_order_release);

        if (job->Group) {
            _scheduler->CancelBackground(job->Group);
        }
    }

    ReapCancelled();
}

void ResourcePreparation::Shutdown()
{
    FO_TRACE_ZONE(Threading);

    ValidateOwner();
    FO_VERIFY_AND_THROW(std::none_of(_jobs.begin(), _jobs.end(), [](const auto& job) { return job->Publishing; }), "Resource preparation shutdown cannot nest inside publication");
    _stopped = true;
    CancelAll();
    Drain();
    FO_STRONG_ASSERT(_jobs.empty() && _reservedBytes == 0, "Resource preparation shutdown did not drain storage");
}

auto ResourcePreparation::IsFinished(const Job& job) const -> bool
{
    if (job.Publishing) {
        return false;
    }

    if (!job.Task) {
        return true;
    }

    auto status = _scheduler->PollBackground(job.Task).Status;
    return status == WorkScheduler::BackgroundStatus::Completed || status == WorkScheduler::BackgroundStatus::Cancelled;
}

void ResourcePreparation::ReapCancelled()
{
    for (auto it = _jobs.begin(); it != _jobs.end();) {
        if ((*it)->Cancelled.load(std::memory_order_acquire) && IsFinished(**it)) {
            FO_STRONG_ASSERT(_reservedBytes >= (*it)->Reservation, "Resource preparation reservation underflow");
            _reservedBytes -= (*it)->Reservation;
            it = _jobs.erase(it);
        }
        else {
            ++it;
        }
    }
}

FO_END_NAMESPACE

