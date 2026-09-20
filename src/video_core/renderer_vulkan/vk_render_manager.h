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
};

} // namespace Vulkan
