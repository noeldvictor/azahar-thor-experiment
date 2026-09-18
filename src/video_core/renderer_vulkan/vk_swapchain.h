// Copyright 2023-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <mutex>
#include <vector>
#include "common/common_types.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan {

class Instance;
class Scheduler;

struct AcquiredSwapchainImage {
    u32 index{};
    vk::Image image{};
    vk::Semaphore present_ready{};
};

enum class SwapchainAcquireResult {
    Success,
    Retry,
    Recreate,
};

class Swapchain {
public:
    explicit Swapchain(const Instance& instance, u32 width, u32 height, vk::SurfaceKHR surface,
                       bool low_refresh_rate);
    ~Swapchain();

    /// Creates (or recreates) the swapchain with a given size.
    void Create(u32 width, u32 height, vk::SurfaceKHR surface, bool low_refresh_rate);

    /// Acquires the next image in the swapchain.
    SwapchainAcquireResult AcquireNextImage(vk::Semaphore acquire_semaphore,
                                            AcquiredSwapchainImage& acquired_image);

    /// Presents a previously acquired image.
    void Present(u32 image_index);

    vk::SurfaceKHR GetSurface() const {
        return surface;
    }

    vk::SurfaceFormatKHR GetSurfaceFormat() const {
        return surface_format;
    }

    vk::SwapchainKHR GetHandle() const {
        return swapchain;
    }

    u32 GetWidth() const {
        return width;
    }

    u32 GetHeight() const {
        return height;
    }

    u32 GetImageCount() const {
        return image_count;
    }

    /// Forces the next present to rebuild the swapchain, for example after a stalled acquire.
    void MarkForRecreation() {
        needs_recreation = true;
    }

    bool NeedsRecreation() const {
        return needs_recreation;
    }

    vk::Extent2D GetExtent() const {
        return extent;
    }

    const std::vector<vk::Image>& GetImages() const {
        return images;
    }

private:
    /// Selects the best available swapchain image format
    void FindPresentFormat();

    /// Sets the best available present mode
    void SetPresentMode();

    /// Sets the surface properties according to device capabilities
    void SetSurfaceProperties();

    /// Destroys current swapchain resources
    void Destroy();

    /// Performs creation of image views and framebuffers from the swapchain images
    void SetupImages();

    /// Creates the image acquired and present ready semaphores
    void RefreshSemaphores();

private:
    const Instance& instance;
    vk::SwapchainKHR swapchain{VK_NULL_HANDLE};
    vk::SurfaceKHR surface{};
    vk::SurfaceFormatKHR surface_format;
    vk::PresentModeKHR present_mode;
    vk::Extent2D extent;
    vk::SurfaceTransformFlagBitsKHR transform;
    vk::CompositeAlphaFlagBitsKHR composite_alpha;
    std::vector<vk::Image> images;
    std::vector<vk::Semaphore> present_ready;
    u32 width = 0;
    u32 height = 0;
    u32 image_count = 0;
    bool needs_recreation = true;
    bool low_refresh_rate;
};

} // namespace Vulkan
