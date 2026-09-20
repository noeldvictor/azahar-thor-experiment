// Copyright 2024-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <chrono>
#include <mutex>

#include "common/math_util.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace VideoCore {
enum class PixelFormat : u32;
}

namespace Vulkan {

class Instance;
class Scheduler;
class Framebuffer;

struct RenderPass {
    vk::Framebuffer framebuffer;
    vk::RenderPass render_pass;
    vk::Rect2D render_area;
    vk::ClearValue clear;
    u32 do_clear;

    bool operator==(const RenderPass& other) const noexcept {
        return std::tie(framebuffer, render_pass, render_area, do_clear) ==
                   std::tie(other.framebuffer, other.render_pass, other.render_area,
                            other.do_clear) &&
               std::memcmp(&clear, &other.clear, sizeof(vk::ClearValue)) == 0;
    }
};

class RenderManager {
    static constexpr u32 NumColorFormats = static_cast<u32>(VideoCore::PixelFormat::NumColorFormat);
    static constexpr u32 NumDepthFormats = static_cast<u32>(VideoCore::PixelFormat::NumDepthFormat);

public:
    explicit RenderManager(const Instance& instance, Scheduler& scheduler);
    ~RenderManager();

    /// Begins a new renderpass with the provided framebuffer as render target.
    void BeginRendering(const Framebuffer* framebuffer, Common::Rectangle<u32> draw_rect);

    /// Begins a new renderpass with the provided render state.
    void BeginRendering(const RenderPass& new_pass);

    /// Exits from any currently active renderpass instance
    void EndRendering();

    /// Returns true while a renderpass instance is open
    bool HasActivePass() const noexcept {
        return pass.render_pass != VK_NULL_HANDLE;
    }

    /// Returns the images of the open renderpass: color first, then depth
    const std::array<vk::Image, 2>& ActiveImages() const noexcept {
        return images;
    }

    /// Returns the renderpass associated with the color-depth format pair
    vk::RenderPass GetRenderpass(VideoCore::PixelFormat color, VideoCore::PixelFormat depth,
                                 bool is_clear);

    /// Logs the render pass sequence of one frame, at most once per second. Diagnostic only.
    void ReportPassTrace();

    /// Records that this frame has rendered into an image, and answers whether it has.
    /// Used to find the read-after-write edges between passes.
    void MarkWritten(vk::Image image);
    [[nodiscard]] bool WasWrittenThisFrame(vk::Image image) const;
    void NotePassReadsTarget();

    /// Classify the draw about to be recorded into the open pass. A post-process quad is a draw
    /// that samples another render target, neither tests nor writes depth, and covers the target.
    /// A pass made only of these is a candidate for running below the global resolution scale.
    void NoteDrawPostQuad(bool is_post_quad);

    /// Per-draw shape, so the heavy passes can be told apart from the post-processing ones.
    void NoteDrawShape(bool samples_rt, bool depth_used, bool small_vertex_count, bool full_cover,
                       bool blended, u32 vertices);

    /// Counts draws since the last forced pass restart. See Settings pass_restart_every.
    u32 draws_since_forced_restart{};

private:
    /// Creates a renderpass configured appropriately and stores it in cached_renderpasses
    vk::UniqueRenderPass CreateRenderPass(vk::Format color, vk::Format depth,
                                          vk::AttachmentLoadOp load_op) const;

private:
    const Instance& instance;
    Scheduler& scheduler;
    vk::UniqueRenderPass cached_renderpasses[NumColorFormats + 1][NumDepthFormats + 1][2];
    std::mutex cache_mutex;
    std::array<vk::Image, 2> images;

    /// Counts why a new pass could not reuse the current one. Profiling builds only.
    void ClassifyRestart(const RenderPass& new_pass,
                         const std::array<vk::Image, 2>& new_images) const;
    std::array<vk::ImageAspectFlags, 2> aspects;
    bool shadow_rendering{};
    RenderPass pass{};
    u32 num_draws{};

    /// One entry per render pass of the current frame, for ReportPassTrace().
    struct TracedPass {
        vk::Image color;
        u32 width;
        u32 height;
        u32 draws;
    };
    std::vector<TracedPass> pass_trace;
    std::chrono::steady_clock::time_point last_trace_log{};

    /// GPU timestamps around each render pass, profiling builds only. Two timestamps per pass.
    static constexpr u32 MaxTimedPasses = 512;
    vk::UniqueQueryPool timestamp_pool;
    u32 timestamp_index{};
    bool timestamps_ready{};
    /// Counts fragment shader invocations per render pass, which is the only way to see how
    /// much of the frame is overdraw.
    vk::UniqueQueryPool fragment_pool;
    /// The render area of the pass that owns each query slot, recorded when the query
    /// begins so a fragment count can be attributed to the right target size.
    std::vector<std::pair<u32, u32>> slot_area;
    // Per timed pass: how many of its draws matched the post-process quad shape, and how many
    // draws it had in total. A pass where the two are equal is entirely post-processing.
    std::vector<std::pair<u32, u32>> slot_post;
    u32 pass_post_draws{};
    // Frame-wide histogram of draw shape, reset with the rest of the per-frame counters.
    u32 shape_draws{}, shape_samples_rt{}, shape_depth{}, shape_quad{}, shape_cover{},
        shape_blended{};
    u64 shape_vertices{};
    u32 pass_total_draws{};
    /// Images this frame has rendered into, and whether the open pass has read one.
    std::vector<vk::Image> written_this_frame;
    bool pass_read_target{};
};

} // namespace Vulkan
