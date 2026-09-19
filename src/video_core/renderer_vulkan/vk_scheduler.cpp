// Copyright 2023-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <mutex>
#include <utility>
#include "common/microprofile.h"
#include "common/thread.h"
#include "video_core/frame_profile.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#ifdef HAVE_LIBRETRO
#include "citra_libretro/libretro_vk.h"
#endif

MICROPROFILE_DEFINE(Vulkan_WaitForWorker, "Vulkan", "Wait for worker", MP_RGB(255, 192, 192));
MICROPROFILE_DEFINE(Vulkan_Submit, "Vulkan", "Submit Exectution", MP_RGB(255, 192, 255));

namespace Vulkan {

namespace {

std::unique_ptr<MasterSemaphore> MakeMasterSemaphore(const Instance& instance) {
#ifdef HAVE_LIBRETRO
    return CreateLibRetroMasterSemaphore(instance);
#else
    if (instance.IsTimelineSemaphoreSupported()) {
        return std::make_unique<MasterSemaphoreTimeline>(instance);
    } else {
        return std::make_unique<MasterSemaphoreFence>(instance);
    }
#endif
}

} // Anonymous namespace

void Scheduler::CommandChunk::ExecuteAll(vk::CommandBuffer cmdbuf) {
    auto command = first;
    while (command != nullptr) {
        auto next = command->GetNext();
        command->Execute(cmdbuf);
        command->~Command();
        command = next;
    }
    submit = false;
    command_offset = 0;
    first = nullptr;
    last = nullptr;
}

Scheduler::Scheduler(const Instance& instance)
    : master_semaphore{MakeMasterSemaphore(instance)},
      command_pool{instance, master_semaphore.get()}, use_worker_thread{true} {
    AllocateWorkerCommandBuffers();
    if (use_worker_thread) {
        AcquireNewChunk();
        worker_thread = std::jthread([this](std::stop_token token) { WorkerThread(token); });
    }
}

Scheduler::~Scheduler() = default;

void Scheduler::Flush(vk::Semaphore signal, vk::Semaphore wait,
                      vk::PipelineStageFlags wait_stage) {
    // When flushing, we only send data to the worker thread; no waiting is necessary.
    VideoCore::AddFrameProfileEvent(VideoCore::FrameProfileEvent::SchedulerFlushes);
    SubmitExecution(signal, wait, wait_stage);
}

void Scheduler::FlushIfPending() {
    if (chunk->Empty()) {
        VideoCore::AddFrameProfileEvent(
            VideoCore::FrameProfileEvent::SchedulerEmptyFlushesSkipped);
        return;
    }
    Flush();
}

void Scheduler::Finish(vk::Semaphore signal, vk::Semaphore wait,
                       vk::PipelineStageFlags wait_stage) {
    // When finishing, we need to wait for the submission to have executed on the device.
    VideoCore::AddFrameProfileEvent(VideoCore::FrameProfileEvent::SchedulerFinishes);
    const u64 presubmit_tick = CurrentTick();
    SubmitExecution(signal, wait, wait_stage);
    Wait(presubmit_tick);
}

void Scheduler::WaitWorker() {
    if (!use_worker_thread) {
        return;
    }

    MICROPROFILE_SCOPE(Vulkan_WaitForWorker);
    VideoCore::AddFrameProfileEvent(VideoCore::FrameProfileEvent::SchedulerWorkerDrains);
    VideoCore::ScopedFrameProfileTimer timer{
        VideoCore::FrameProfileEvent::SchedulerWorkerDrainNanoseconds};
    DispatchWork();

    // Ensure the queue is drained.
    {
        std::unique_lock ql{queue_mutex};
        event_cv.wait(ql, [this] { return work_queue.empty(); });
    }

    // Now wait for execution to finish.
    // This needs to be done in the same order as WorkerThread.
    std::scoped_lock el{execution_mutex};
}

void Scheduler::Wait(u64 tick) {
    VideoCore::AddFrameProfileEvent(VideoCore::FrameProfileEvent::SchedulerWaits);
    // Ticks the GPU has already passed are the common case: the stream buffers ask about every
    // allocation they recycle. Answer those from the cached tick without a timer or a driver
    // call, and measure only the waits that really block.
    if (master_semaphore->IsFree(tick)) {
        return;
    }
    VideoCore::AddFrameProfileEvent(VideoCore::FrameProfileEvent::SchedulerBlockingWaits);
    VideoCore::ScopedFrameProfileTimer timer{
        VideoCore::FrameProfileEvent::SchedulerBlockingWaitNanoseconds};
    if (tick >= master_semaphore->CurrentTick()) {
        // Make sure we are not waiting for the current tick without signalling
        Flush();
    }
    master_semaphore->Wait(tick);
}

void Scheduler::DispatchWork() {
    if (!use_worker_thread || chunk->Empty()) {
        return;
    }

    on_dispatch();

    {
        std::scoped_lock ql{queue_mutex};
        work_queue.push(std::move(chunk));
    }

    event_cv.notify_all();
    AcquireNewChunk();
}

void Scheduler::WorkerThread(std::stop_token stop_token) {
    Common::SetCurrentThreadName("VulkanWorker");

    const auto TryPopQueue{[this](auto& work) -> bool {
        if (work_queue.empty()) {
            return false;
        }

        work = std::move(work_queue.front());
        work_queue.pop();
        event_cv.notify_all();
        return true;
    }};

    while (!stop_token.stop_requested()) {
        std::unique_ptr<CommandChunk> work;

        {
            std::unique_lock lk{queue_mutex};

            // Wait for work.
            Common::CondvarWait(event_cv, lk, stop_token, [&] { return TryPopQueue(work); });

            // If we've been asked to stop, we're done.
            if (stop_token.stop_requested()) {
                return;
            }

            // Exchange lock ownership so that we take the execution lock before
            // the queue lock goes out of scope. This allows us to force execution
            // to complete in the next step.
            std::exchange(lk, std::unique_lock{execution_mutex});

            // Perform the work, tracking whether the chunk was a submission
            // before executing.
            const bool has_submit = work->HasSubmit();
            work->ExecuteAll(current_cmdbuf);

            // If the chunk was a submission, reallocate the command buffer.
            if (has_submit) {
                AllocateWorkerCommandBuffers();
            }
        }

        {
            std::scoped_lock rl{reserve_mutex};

            // Recycle the chunk back to the reserve.
            chunk_reserve.emplace_back(std::move(work));
        }
    }
}

void Scheduler::AllocateWorkerCommandBuffers() {
    const vk::CommandBufferBeginInfo begin_info = {
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    };

    current_cmdbuf = command_pool.Commit();
    current_cmdbuf.begin(begin_info);
}

u64 Scheduler::PrepareSubmission() {
    VideoCore::AddFrameProfileEvent(VideoCore::FrameProfileEvent::SchedulerSubmissions);
    state = StateFlags::AllDirty;
    const u64 signal_value = master_semaphore->NextTick();
    on_submit();

    return signal_value;
}

void Scheduler::ExecuteSubmission(vk::CommandBuffer cmdbuf, vk::Semaphore signal_semaphore,
                                  vk::Semaphore wait_semaphore, vk::PipelineStageFlags wait_stage,
                                  u64 signal_value) {
    MICROPROFILE_SCOPE(Vulkan_Submit);
    std::scoped_lock lock{submit_mutex};
    master_semaphore->SubmitWork(cmdbuf, wait_semaphore, signal_semaphore, wait_stage,
                                 signal_value);
}

void Scheduler::DispatchSubmission(u64 signal_value) {
    master_semaphore->RefreshOnSubmit(signal_value);

    if (!use_worker_thread) {
        AllocateWorkerCommandBuffers();
    } else {
        chunk->MarkSubmit();
        DispatchWork();
    }
}

void Scheduler::SubmitExecution(vk::Semaphore signal_semaphore, vk::Semaphore wait_semaphore,
                                vk::PipelineStageFlags wait_stage) {
    const u64 signal_value = PrepareSubmission();

    Record([signal_semaphore, wait_semaphore, wait_stage, signal_value,
            this](vk::CommandBuffer cmdbuf) {
        ExecuteSubmission(cmdbuf, signal_semaphore, wait_semaphore, wait_stage, signal_value);
    });

    DispatchSubmission(signal_value);
}

void Scheduler::AcquireNewChunk() {
    std::scoped_lock lock{reserve_mutex};
    if (chunk_reserve.empty()) {
        chunk = std::make_unique<CommandChunk>();
        return;
    }

    chunk = std::move(chunk_reserve.back());
    chunk_reserve.pop_back();
}

} // namespace Vulkan
