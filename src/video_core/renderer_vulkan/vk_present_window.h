// Copyright 2023-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include "common/polyfill_thread.h"
#include "video_core/renderer_vulkan/vk_swapchain.h"

VK_DEFINE_HANDLE(VmaAllocation)

namespace Frontend {
class EmuWindow;
}

namespace Vulkan {

class Instance;
class Swapchain;
class Scheduler;
class RenderManager;

struct Frame {
    u32 width{};
    u32 height{};
    VmaAllocation allocation{};
    vk::Framebuffer fallback_framebuffer{};
    vk::Framebuffer framebuffer{};
    vk::RenderPass renderpass{};
    vk::Image image{};
    vk::ImageView image_view{};
    vk::Image present_image{};
    u32 present_image_index{};
    vk::Semaphore image_acquired{};
    vk::Semaphore present_ready{};
    u64 submit_tick{};
    bool present_valid{};
    bool direct_present{};
    // Swapchain generation that owns present_image_index. A rebuilt swapchain invalidates it.
    u32 present_generation{};
    // LibRetro owns its presentation command buffers and synchronization separately.
    vk::Semaphore render_ready{};
    vk::Fence present_done{};
    vk::CommandBuffer cmdbuf{};
};

class PresentWindow final {
public:
    explicit PresentWindow(Frontend::EmuWindow& emu_window, const Instance& instance,
                           Scheduler& scheduler, bool low_refresh_rate);
    ~PresentWindow();

    /// Waits for all queued frames to finish presenting.
    void WaitPresent();

    /// Returns the last used render frame.
    Frame* GetRenderFrame();

#ifdef ANDROID
    /// Uses an acquired, exact-size Android swapchain image as the final color attachment when
    /// immediately available. Falls back without waiting when acquisition would block.
    bool TryPrepareDirectPresent(Frame* frame);
#endif

    /// Recreates the render frame to match provided parameters.
    void RecreateFrame(Frame* frame, u32 width, u32 height);

    /// Submits current render work and queues the provided frame for presentation.
    void Present(Frame* frame);

    /// This is called to notify the rendering backend of a surface change
    void NotifySurfaceChanged();

    [[nodiscard]] vk::RenderPass Renderpass() const noexcept {
        return present_renderpass;
    }

    u32 ImageCount() const noexcept {
        return swapchain.GetImageCount();
    }

    vk::Format GetSurfaceFormat() const noexcept {
        return swapchain.GetSurfaceFormat().format;
    }

private:
    void PresentThread(std::stop_token token);

    void PrepareForPresent(vk::CommandBuffer cmdbuf, Frame* frame);

    void FinishPresent(Frame* frame);

    void RecreateSwapchain(Frame* frame);

    vk::RenderPass CreateRenderpass(bool direct_to_swapchain);

#ifdef ANDROID
    void CreateDirectFramebuffers();

    void DestroyDirectFramebuffers();
#endif

private:
    Frontend::EmuWindow& emu_window;
    const Instance& instance;
    Scheduler& scheduler;
    bool low_refresh_rate;
    vk::SurfaceKHR surface;
    vk::SurfaceKHR next_surface{};
    Swapchain swapchain;
    vk::Queue graphics_queue;
    vk::RenderPass present_renderpass;
#ifdef ANDROID
    vk::RenderPass direct_present_renderpass;
    std::vector<vk::ImageView> direct_image_views;
    std::vector<vk::Framebuffer> direct_framebuffers;
#endif
    std::vector<Frame> swap_chain;
    std::queue<Frame*> free_queue;
    std::queue<Frame*> present_queue;
    std::condition_variable free_cv;
    std::condition_variable recreate_surface_cv;
    std::condition_variable_any frame_cv;
    std::mutex swapchain_mutex;
    std::mutex recreate_surface_mutex;
    std::mutex queue_mutex;
    std::mutex free_mutex;
    std::jthread present_thread;
    bool vsync_enabled{};
    bool blit_supported;
    bool use_present_thread{true};
    void* last_render_surface{};
    // Incremented by every swapchain rebuild. Guarded by swapchain_mutex.
    u32 swapchain_generation{};
};

} // namespace Vulkan
