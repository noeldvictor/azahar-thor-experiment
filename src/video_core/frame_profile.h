// Copyright 2026 Azahar Thor Experiment
// Licensed under GPLv2 or any later version

#pragma once

#include "common/common_types.h"

#ifndef THOR_FRAME_PROFILING
#define THOR_FRAME_PROFILING 0
#endif

#if THOR_FRAME_PROFILING
#include <chrono>
#endif

namespace VideoCore {

// These counters intentionally describe host work rather than guessed performance. They are
// compiled out of ordinary builds and enabled only in a dedicated profiling APK.
enum class FrameProfileEvent : u32 {
    SwapCalls,
    PresentFrames,
    DuplicateFramePreparationsSkipped,
    PresentBlits,
    PresentCopies,
    PresentDirectRenders,
    PresentPixels,
    PresentQueueWaitNanoseconds,
    PresentCombinedSubmissions,
    SchedulerFlushes,
    SchedulerEmptyFlushesSkipped,
    SchedulerFinishes,
    SchedulerSubmissions,
    SchedulerWaits,
    SchedulerWaitNanoseconds,
    SchedulerBlockingWaits,
    SchedulerBlockingWaitNanoseconds,
    StreamBufferWraps,
    DrawSyncStateNanoseconds,
    DrawFramebufferNanoseconds,
    DrawTextureUnitsNanoseconds,
    DrawShaderNanoseconds,
    DrawLutNanoseconds,
    DrawUniformsNanoseconds,
    DrawPassNanoseconds,
    DrawSubmitNanoseconds,
    SchedulerWorkerDrains,
    SchedulerWorkerDrainNanoseconds,
    RenderPassBegins,
    RenderPassReuses,
    RenderPassEnds,
    RenderPassImageBarriers,
    RenderPassRestartColorSwitch,
    RenderPassRestartDepthToggle,
    RenderPassRestartSameImages,
    RenderPassRestartAreaShrink,
    RenderPassRestartAreaGrow,
    RenderPassRestartAreaOther,
    RenderPassRestartClear,
    MaliRenderPassFlushes,
    TextureUploads,
    TextureUploadBytes,
    CustomTextureUploads,
    CustomTextureUploadBytes,
    TextureDownloads,
    TextureDownloadBytes,
    TextureCopies,
    TextureCopyPixels,
    TextureBlits,
    TextureBlitPixels,
    TextureScaleBlits,
    TextureScaleBlitPixels,
    AcceleratedTextureCopies,
    AcceleratedTextureCopyPixels,
    AcceleratedDisplayTransfers,
    AcceleratedDisplayTransferPixels,
    SurfaceValidationCopies,
    SurfaceValidationCopyPixels,
    SurfaceValidationBlits,
    SurfaceValidationBlitPixels,
    RasterizerInvalidations,
    DirtyRegionUpdates,
    DirtyRegionUpdatesElided,
    ImmediateVertices,
    DrawBatches,
    AcceleratedDraws,
    AcceleratedGeometryDraws,
    SoftwareDraws,
    FallbackHwShaderDisabled,
    FallbackGeometryShader,
    FallbackPrimitiveState,
    FallbackTopology,
    FallbackBackend,
    /// A draw that samples the image it is drawing into. The driver has to flush between
    /// such draws, which costs far more than the draw itself.
    TextureAliasesAttachment,
    /// A draw whose pipeline differs from the one the previous draw used.
    PipelineChanges,
    Count,
};

#if THOR_FRAME_PROFILING

void AddFrameProfileEvent(FrameProfileEvent event, u64 amount = 1) noexcept;

/// Emits and resets one aggregate window after a bounded number of SwapBuffers calls.
void ReportFrameProfileWindow();

class ScopedFrameProfileTimer {
public:
    explicit ScopedFrameProfileTimer(FrameProfileEvent event_) noexcept;
    ~ScopedFrameProfileTimer();

    ScopedFrameProfileTimer(const ScopedFrameProfileTimer&) = delete;
    ScopedFrameProfileTimer& operator=(const ScopedFrameProfileTimer&) = delete;

private:
    FrameProfileEvent event;
    std::chrono::steady_clock::time_point start;
};

#else

inline void AddFrameProfileEvent(FrameProfileEvent, u64 = 1) noexcept {}
inline void ReportFrameProfileWindow() {}

class ScopedFrameProfileTimer {
public:
    explicit ScopedFrameProfileTimer(FrameProfileEvent) noexcept {}
};

#endif

} // namespace VideoCore
