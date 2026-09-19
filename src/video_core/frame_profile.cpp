// Copyright 2026 Azahar Thor Experiment
// Licensed under GPLv2 or any later version

#include "video_core/frame_profile.h"

#if THOR_FRAME_PROFILING

#include <array>
#include <atomic>
#include <chrono>

#include "common/logging/log.h"

namespace VideoCore {
namespace {

using Clock = std::chrono::steady_clock;
constexpr u32 ReportSwapPeriod = 300;
constexpr std::size_t EventCount = static_cast<std::size_t>(FrameProfileEvent::Count);

std::array<std::atomic<u64>, EventCount> counters{};
u32 swaps_until_report = ReportSwapPeriod;
Clock::time_point report_start = Clock::now();

constexpr std::size_t Index(FrameProfileEvent event) noexcept {
    return static_cast<std::size_t>(event);
}

double PerSwap(const std::array<u64, EventCount>& values, FrameProfileEvent event, u64 swaps) {
    return swaps == 0 ? 0.0 : static_cast<double>(values[Index(event)]) / swaps;
}

double MillisecondsPerSwap(const std::array<u64, EventCount>& values, FrameProfileEvent event,
                           u64 swaps) {
    return PerSwap(values, event, swaps) / 1'000'000.0;
}

} // Anonymous namespace

void AddFrameProfileEvent(FrameProfileEvent event, u64 amount) noexcept {
    counters[Index(event)].fetch_add(amount, std::memory_order_relaxed);
}

void ReportFrameProfileWindow() {
    if (--swaps_until_report != 0) {
        return;
    }
    swaps_until_report = ReportSwapPeriod;

    std::array<u64, EventCount> values{};
    for (std::size_t i = 0; i < EventCount; ++i) {
        values[i] = counters[i].exchange(0, std::memory_order_relaxed);
    }

    const auto now = Clock::now();
    const double seconds = std::chrono::duration<double>(now - report_start).count();
    report_start = now;

    const u64 swaps = values[Index(FrameProfileEvent::SwapCalls)];
    const u64 draws = values[Index(FrameProfileEvent::DrawBatches)];
    const u64 accelerated = values[Index(FrameProfileEvent::AcceleratedDraws)];
    const double accelerated_percent =
        draws == 0 ? 0.0 : 100.0 * static_cast<double>(accelerated) / draws;

    LOG_INFO(Render_Vulkan,
             "ThorFrameProfile window_s={:.3f} swaps={} presented={} duplicate_prepare_skipped={} "
             "immediate_vertices={} draw_batches={} accelerated={} accelerated_pct={:.2f} "
             "software={} accelerated_geometry={}",
             seconds, swaps, values[Index(FrameProfileEvent::PresentFrames)],
             values[Index(FrameProfileEvent::DuplicateFramePreparationsSkipped)],
             values[Index(FrameProfileEvent::ImmediateVertices)], draws, accelerated,
             accelerated_percent, values[Index(FrameProfileEvent::SoftwareDraws)],
             values[Index(FrameProfileEvent::AcceleratedGeometryDraws)]);
    LOG_INFO(Render_Vulkan,
             "ThorFrameProfile fallback hw_off={} gs={} primitive_state={} topology={} backend={}",
             values[Index(FrameProfileEvent::FallbackHwShaderDisabled)],
             values[Index(FrameProfileEvent::FallbackGeometryShader)],
             values[Index(FrameProfileEvent::FallbackPrimitiveState)],
             values[Index(FrameProfileEvent::FallbackTopology)],
             values[Index(FrameProfileEvent::FallbackBackend)]);
    LOG_INFO(
        Render_Vulkan,
        "ThorFrameProfile scheduler submit_per_swap={:.3f} flush_per_swap={:.3f} "
        "empty_flushes_skipped={} finish={} waits={} wait_ms_per_swap={:.3f} worker_drains={} "
        "worker_drain_ms_per_swap={:.3f}",
        PerSwap(values, FrameProfileEvent::SchedulerSubmissions, swaps),
        PerSwap(values, FrameProfileEvent::SchedulerFlushes, swaps),
        values[Index(FrameProfileEvent::SchedulerEmptyFlushesSkipped)],
        values[Index(FrameProfileEvent::SchedulerFinishes)],
        values[Index(FrameProfileEvent::SchedulerWaits)],
        MillisecondsPerSwap(values, FrameProfileEvent::SchedulerWaitNanoseconds, swaps),
        values[Index(FrameProfileEvent::SchedulerWorkerDrains)],
        MillisecondsPerSwap(values, FrameProfileEvent::SchedulerWorkerDrainNanoseconds, swaps));
    LOG_INFO(Render_Vulkan,
             "ThorFrameProfile blocking blocking_waits_per_swap={:.3f} "
             "blocking_wait_ms_per_swap={:.3f} stream_wraps_per_swap={:.3f}",
             PerSwap(values, FrameProfileEvent::SchedulerBlockingWaits, swaps),
             MillisecondsPerSwap(values, FrameProfileEvent::SchedulerBlockingWaitNanoseconds,
                                 swaps),
             PerSwap(values, FrameProfileEvent::StreamBufferWraps, swaps));
    LOG_INFO(Render_Vulkan,
             "ThorFrameProfile draw_ms_per_swap sync={:.2f} framebuffer={:.2f} textures={:.2f} "
             "shader={:.2f} lut={:.2f} uniforms={:.2f} pass={:.2f} submit={:.2f}",
             MillisecondsPerSwap(values, FrameProfileEvent::DrawSyncStateNanoseconds, swaps),
             MillisecondsPerSwap(values, FrameProfileEvent::DrawFramebufferNanoseconds, swaps),
             MillisecondsPerSwap(values, FrameProfileEvent::DrawTextureUnitsNanoseconds, swaps),
             MillisecondsPerSwap(values, FrameProfileEvent::DrawShaderNanoseconds, swaps),
             MillisecondsPerSwap(values, FrameProfileEvent::DrawLutNanoseconds, swaps),
             MillisecondsPerSwap(values, FrameProfileEvent::DrawUniformsNanoseconds, swaps),
             MillisecondsPerSwap(values, FrameProfileEvent::DrawPassNanoseconds, swaps),
             MillisecondsPerSwap(values, FrameProfileEvent::DrawSubmitNanoseconds, swaps));
    LOG_INFO(Render_Vulkan,
             "ThorFrameProfile renderpass begin_per_swap={:.3f} reuse_per_swap={:.3f} "
             "end_per_swap={:.3f} end_image_barriers_per_swap={:.3f} mali_flushes={}",
             PerSwap(values, FrameProfileEvent::RenderPassBegins, swaps),
             PerSwap(values, FrameProfileEvent::RenderPassReuses, swaps),
             PerSwap(values, FrameProfileEvent::RenderPassEnds, swaps),
             PerSwap(values, FrameProfileEvent::RenderPassImageBarriers, swaps),
             values[Index(FrameProfileEvent::MaliRenderPassFlushes)]);
    LOG_INFO(Render_Vulkan,
             "ThorFrameProfile renderpass_restart color_switch_per_swap={:.3f} "
             "depth_toggle_per_swap={:.3f} same_images_per_swap={:.3f} "
             "area_shrink_per_swap={:.3f} area_grow_per_swap={:.3f} area_other_per_swap={:.3f} "
             "clear_per_swap={:.3f}",
             PerSwap(values, FrameProfileEvent::RenderPassRestartColorSwitch, swaps),
             PerSwap(values, FrameProfileEvent::RenderPassRestartDepthToggle, swaps),
             PerSwap(values, FrameProfileEvent::RenderPassRestartSameImages, swaps),
             PerSwap(values, FrameProfileEvent::RenderPassRestartAreaShrink, swaps),
             PerSwap(values, FrameProfileEvent::RenderPassRestartAreaGrow, swaps),
             PerSwap(values, FrameProfileEvent::RenderPassRestartAreaOther, swaps),
             PerSwap(values, FrameProfileEvent::RenderPassRestartClear, swaps));
    LOG_INFO(Render_Vulkan,
             "ThorFrameProfile texture upload_per_swap={:.3f} upload_kib_per_swap={:.3f} "
             "custom_uploads={} custom_kib={:.3f} download_per_swap={:.3f} "
             "download_kib_per_swap={:.3f} copies={} copy_mpix={:.3f} blits={} blit_mpix={:.3f} "
             "scale_blits={} scale_blit_mpix={:.3f}",
             PerSwap(values, FrameProfileEvent::TextureUploads, swaps),
             PerSwap(values, FrameProfileEvent::TextureUploadBytes, swaps) / 1024.0,
             values[Index(FrameProfileEvent::CustomTextureUploads)],
             values[Index(FrameProfileEvent::CustomTextureUploadBytes)] / 1024.0,
             PerSwap(values, FrameProfileEvent::TextureDownloads, swaps),
             PerSwap(values, FrameProfileEvent::TextureDownloadBytes, swaps) / 1024.0,
             values[Index(FrameProfileEvent::TextureCopies)],
             values[Index(FrameProfileEvent::TextureCopyPixels)] / 1'000'000.0,
             values[Index(FrameProfileEvent::TextureBlits)],
             values[Index(FrameProfileEvent::TextureBlitPixels)] / 1'000'000.0,
             values[Index(FrameProfileEvent::TextureScaleBlits)],
             values[Index(FrameProfileEvent::TextureScaleBlitPixels)] / 1'000'000.0);
    LOG_INFO(Render_Vulkan,
             "ThorFrameProfile cache_paths texture_copy={} texture_copy_mpix={:.3f} "
             "display_transfer={} display_transfer_mpix={:.3f} validation_copy={} "
             "validation_copy_mpix={:.3f} validation_blit={} validation_blit_mpix={:.3f} "
             "invalidations={} dirty_updates={} dirty_updates_elided={}",
             values[Index(FrameProfileEvent::AcceleratedTextureCopies)],
             values[Index(FrameProfileEvent::AcceleratedTextureCopyPixels)] / 1'000'000.0,
             values[Index(FrameProfileEvent::AcceleratedDisplayTransfers)],
             values[Index(FrameProfileEvent::AcceleratedDisplayTransferPixels)] / 1'000'000.0,
             values[Index(FrameProfileEvent::SurfaceValidationCopies)],
             values[Index(FrameProfileEvent::SurfaceValidationCopyPixels)] / 1'000'000.0,
             values[Index(FrameProfileEvent::SurfaceValidationBlits)],
             values[Index(FrameProfileEvent::SurfaceValidationBlitPixels)] / 1'000'000.0,
             values[Index(FrameProfileEvent::RasterizerInvalidations)],
             values[Index(FrameProfileEvent::DirtyRegionUpdates)],
             values[Index(FrameProfileEvent::DirtyRegionUpdatesElided)]);
    LOG_INFO(Render_Vulkan,
             "ThorFrameProfile present blits={} copies={} direct={} mpix={:.3f} "
             "queue_wait_ms_per_swap={:.3f} combined_submissions={}",
             values[Index(FrameProfileEvent::PresentBlits)],
             values[Index(FrameProfileEvent::PresentCopies)],
             values[Index(FrameProfileEvent::PresentDirectRenders)],
             values[Index(FrameProfileEvent::PresentPixels)] / 1'000'000.0,
             MillisecondsPerSwap(values, FrameProfileEvent::PresentQueueWaitNanoseconds, swaps),
             values[Index(FrameProfileEvent::PresentCombinedSubmissions)]);
}

ScopedFrameProfileTimer::ScopedFrameProfileTimer(FrameProfileEvent event_) noexcept
    : event{event_}, start{Clock::now()} {}

ScopedFrameProfileTimer::~ScopedFrameProfileTimer() {
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start);
    AddFrameProfileEvent(event, static_cast<u64>(elapsed.count()));
}

} // namespace VideoCore

#endif
