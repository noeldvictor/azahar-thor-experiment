// Copyright 2023-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <thread>
#include "common/microprofile.h"
#include "common/settings.h"
#include "common/thread.h"
#include "core/frontend/emu_window.h"
#include "video_core/frame_profile.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_platform.h"
#include "video_core/renderer_vulkan/vk_present_window.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_swapchain.h"
#include "vk_platform.h"

#include <vk_mem_alloc.h>

MICROPROFILE_DEFINE(Vulkan_WaitPresent, "Vulkan", "Wait For Present", MP_RGB(128, 128, 128));

namespace Vulkan {

namespace {

bool CanBlitToSwapchain(const vk::PhysicalDevice& physical_device, vk::Format format) {
    const vk::FormatProperties props{physical_device.getFormatProperties(format)};
    return static_cast<bool>(props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst);
}

constexpr bool ShouldBlitToSwapchain(bool blit_supported, u32 frame_width, u32 frame_height,
                                     u32 swapchain_width, u32 swapchain_height) {
    return blit_supported && (frame_width != swapchain_width || frame_height != swapchain_height);
}

static_assert(!ShouldBlitToSwapchain(true, 1920, 1080, 1920, 1080));
static_assert(ShouldBlitToSwapchain(true, 1280, 720, 1920, 1080));
static_assert(!ShouldBlitToSwapchain(false, 1280, 720, 1920, 1080));

constexpr bool CanRenderDirectToSwapchain(u32 frame_width, u32 frame_height, u32 swapchain_width,
                                          u32 swapchain_height) {
    return frame_width == swapchain_width && frame_height == swapchain_height;
}

static_assert(CanRenderDirectToSwapchain(1920, 1080, 1920, 1080));
static_assert(!CanRenderDirectToSwapchain(1280, 720, 1920, 1080));

[[nodiscard]] vk::ImageSubresourceLayers MakeImageSubresourceLayers() {
    return vk::ImageSubresourceLayers{
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
}

[[nodiscard]] vk::ImageBlit MakeImageBlit(s32 frame_width, s32 frame_height, s32 swapchain_width,
                                          s32 swapchain_height) {
    return vk::ImageBlit{
        .srcSubresource = MakeImageSubresourceLayers(),
        .srcOffsets =
            std::array{
                vk::Offset3D{
                    .x = 0,
                    .y = 0,
                    .z = 0,
                },
                vk::Offset3D{
                    .x = frame_width,
                    .y = frame_height,
                    .z = 1,
                },
            },
        .dstSubresource = MakeImageSubresourceLayers(),
        .dstOffsets =
            std::array{
                vk::Offset3D{
                    .x = 0,
                    .y = 0,
                    .z = 0,
                },
                vk::Offset3D{
                    .x = swapchain_width,
                    .y = swapchain_height,
                    .z = 1,
                },
            },
    };
}

[[nodiscard]] vk::ImageCopy MakeImageCopy(u32 frame_width, u32 frame_height, u32 swapchain_width,
                                          u32 swapchain_height) {
    return vk::ImageCopy{
        .srcSubresource = MakeImageSubresourceLayers(),
        .srcOffset =
            vk::Offset3D{
                .x = 0,
                .y = 0,
                .z = 0,
            },
        .dstSubresource = MakeImageSubresourceLayers(),
        .dstOffset =
            vk::Offset3D{
                .x = 0,
                .y = 0,
                .z = 0,
            },
        .extent =
            vk::Extent3D{
                .width = std::min(frame_width, swapchain_width),
                .height = std::min(frame_height, swapchain_height),
                .depth = 1,
            },
    };
}

} // Anonymous namespace

PresentWindow::PresentWindow(Frontend::EmuWindow& emu_window_, const Instance& instance_,
                             Scheduler& scheduler_, bool low_refresh_rate_)
    : emu_window{emu_window_}, instance{instance_}, scheduler{scheduler_},
      low_refresh_rate{low_refresh_rate_},
      surface{CreateSurface(instance.GetInstance(), emu_window)}, next_surface{surface},
      swapchain{instance, emu_window.GetFramebufferLayout().width,
                emu_window.GetFramebufferLayout().height, surface, low_refresh_rate_},
      graphics_queue{instance.GetGraphicsQueue()}, present_renderpass{CreateRenderpass(false)},
#ifdef ANDROID
      direct_present_renderpass{CreateRenderpass(true)},
#endif
      vsync_enabled{Settings::values.use_vsync.GetValue()},
      blit_supported{
          CanBlitToSwapchain(instance.GetPhysicalDevice(), swapchain.GetSurfaceFormat().format)},
      use_present_thread{Settings::values.async_presentation.GetValue()},
      last_render_surface{emu_window.GetWindowInfo().render_surface} {

    swap_chain.resize(swapchain.GetImageCount());
    const vk::Device device = instance.GetDevice();
    for (u32 i = 0; i < swap_chain.size(); ++i) {
        Frame& frame = swap_chain[i];
        frame.image_acquired = device.createSemaphore({});
        free_queue.push(&frame);
        if (instance.HasDebuggingToolAttached()) {
            SetObjectName(device, frame.image_acquired, "Frame Semaphore: image_acquired {}", i);
        }
    }
#ifdef ANDROID
    CreateDirectFramebuffers();
#endif

    if (use_present_thread) {
        present_thread = std::jthread([this](std::stop_token token) { PresentThread(token); });
    }
}

PresentWindow::~PresentWindow() {
    scheduler.Finish();
    const vk::Device device = instance.GetDevice();
#ifdef ANDROID
    DestroyDirectFramebuffers();
    device.destroyRenderPass(direct_present_renderpass);
#endif
    // If the window is destroyed before the next_surface is
    // consumed, make sure to destroy it here to prevent a
    // resource leak.
    if (next_surface && next_surface != surface) {
        instance.GetInstance().destroySurfaceKHR(next_surface);
        next_surface = vk::SurfaceKHR{};
    }
    device.destroyRenderPass(present_renderpass);
    for (auto& frame : swap_chain) {
        device.destroyFramebuffer(frame.fallback_framebuffer);
        device.destroyImageView(frame.image_view);
        device.destroySemaphore(frame.image_acquired);
        if (frame.image) {
            vmaDestroyImage(instance.GetAllocator(), frame.image, frame.allocation);
        }
    }
}

void PresentWindow::RecreateFrame(Frame* frame, u32 width, u32 height) {
    vk::Device device = instance.GetDevice();
    if (frame->fallback_framebuffer) {
        device.destroyFramebuffer(frame->fallback_framebuffer);
    }
    if (frame->image_view) {
        device.destroyImageView(frame->image_view);
    }
    if (frame->image) {
        vmaDestroyImage(instance.GetAllocator(), frame->image, frame->allocation);
    }

    const vk::Format format = swapchain.GetSurfaceFormat().format;
    const vk::ImageCreateInfo image_info = {
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
    };

    const VmaAllocationCreateInfo alloc_info = {
        .flags = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .requiredFlags = 0,
        .preferredFlags = 0,
        .pool = VK_NULL_HANDLE,
        .pUserData = nullptr,
    };

    VkImage unsafe_image{};
    VkImageCreateInfo unsafe_image_info = static_cast<VkImageCreateInfo>(image_info);

    VkResult result = vmaCreateImage(instance.GetAllocator(), &unsafe_image_info, &alloc_info,
                                     &unsafe_image, &frame->allocation, nullptr);
    if (result != VK_SUCCESS) [[unlikely]] {
        LOG_CRITICAL(Render_Vulkan, "Failed allocating texture with error {}", result);
        UNREACHABLE();
    }
    frame->image = vk::Image{unsafe_image};

    const vk::ImageViewCreateInfo view_info = {
        .image = frame->image,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    frame->image_view = device.createImageView(view_info);

    const vk::FramebufferCreateInfo framebuffer_info = {
        .renderPass = present_renderpass,
        .attachmentCount = 1,
        .pAttachments = &frame->image_view,
        .width = width,
        .height = height,
        .layers = 1,
    };
    frame->fallback_framebuffer = instance.GetDevice().createFramebuffer(framebuffer_info);
    frame->framebuffer = frame->fallback_framebuffer;
    frame->renderpass = present_renderpass;
    frame->direct_present = false;

    frame->width = width;
    frame->height = height;

    scheduler.Record([image = frame->image](vk::CommandBuffer cmdbuf) {
        const vk::ImageMemoryBarrier init_barrier = {
            .srcAccessMask = vk::AccessFlagBits::eNone,
            .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eGeneral,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                               vk::PipelineStageFlagBits::eColorAttachmentOutput,
                               vk::DependencyFlagBits::eByRegion, {}, {}, init_barrier);
    });
}

Frame* PresentWindow::GetRenderFrame() {
    MICROPROFILE_SCOPE(Vulkan_WaitPresent);

    // Wait for free presentation frames
    std::unique_lock lock{free_mutex};
    {
        VideoCore::ScopedFrameProfileTimer timer{
            VideoCore::FrameProfileEvent::PresentQueueWaitNanoseconds};
        free_cv.wait(lock, [this] { return !free_queue.empty(); });
    }

    // Take the frame from the queue. Its previous render and transfer are ordered before any
    // future reuse on the same graphics queue by the post-copy image barrier.
    Frame* frame = free_queue.front();
    free_queue.pop();
    lock.unlock();

    // A frame's acquire semaphore cannot be signaled again until its previous queue wait has
    // completed. The scheduler timeline is normally already past this tick by the time the frame
    // returns from presentation, making this a cheap correctness check rather than a per-frame
    // fence cycle.
    if (frame->submit_tick != 0) {
        scheduler.Wait(frame->submit_tick);
    }
    frame->framebuffer = frame->fallback_framebuffer;
    frame->renderpass = present_renderpass;
    frame->direct_present = false;
    return frame;
}

#ifdef ANDROID
bool PresentWindow::TryPrepareDirectPresent(Frame* frame) {
    std::scoped_lock lock{swapchain_mutex};
    // A copy-path frame acquires its image on the worker at submission time. If a later frame
    // acquired directly here first, the two direct images would hold the compositor's
    // dequeued-buffer limit and the worker would wait for an image that only the present of
    // those later frames can free, which the rebuild bound then breaks (E.X. Troopers,
    // 2026-09-18). So while any copy acquire is pending, every frame takes the copy path and
    // acquires in submission order. The emulation thread never waits here.
    const auto take_copy_path = [this, frame] {
        frame->copy_acquire_pending = true;
        ++pending_copy_acquires;
        return false;
    };
    const vk::Extent2D extent = swapchain.GetExtent();
    if (pending_copy_acquires != 0 || swapchain.NeedsRecreation() ||
        !CanRenderDirectToSwapchain(frame->width, frame->height, extent.width, extent.height) ||
        direct_framebuffers.size() != swapchain.GetImageCount()) {
        return take_copy_path();
    }

    AcquiredSwapchainImage acquired_image;
    if (swapchain.AcquireNextImage(frame->image_acquired, acquired_image) !=
        SwapchainAcquireResult::Success) {
        return take_copy_path();
    }

    frame->present_image = acquired_image.image;
    frame->present_image_index = acquired_image.index;
    frame->present_ready = acquired_image.present_ready;
    frame->present_generation = swapchain_generation;
    frame->present_valid = true;
    frame->framebuffer = direct_framebuffers[acquired_image.index];
    frame->renderpass = direct_present_renderpass;
    frame->direct_present = true;
    return true;
}
#endif

void PresentWindow::Present(Frame* frame) {
    VideoCore::AddFrameProfileEvent(VideoCore::FrameProfileEvent::PresentCombinedSubmissions);
    const auto prepare = [this, frame](vk::CommandBuffer cmdbuf) {
        if (frame->direct_present) {
            VideoCore::AddFrameProfileEvent(VideoCore::FrameProfileEvent::PresentFrames);
            VideoCore::AddFrameProfileEvent(VideoCore::FrameProfileEvent::PresentDirectRenders);
            VideoCore::AddFrameProfileEvent(VideoCore::FrameProfileEvent::PresentPixels,
                                            static_cast<u64>(frame->width) * frame->height);
        } else {
            PrepareForPresent(cmdbuf, frame);
        }
        return Scheduler::SubmissionSemaphores{
            .signal = frame->present_valid ? frame->present_ready : vk::Semaphore{},
            .wait = frame->present_valid ? frame->image_acquired : vk::Semaphore{},
            .wait_stage = frame->direct_present
                              ? vk::PipelineStageFlagBits::eColorAttachmentOutput
                              : vk::PipelineStageFlagBits::eTransfer,
        };
    };

    if (!use_present_thread) {
        scheduler.FlushWithDynamicSubmission(
            prepare, [frame](vk::CommandBuffer, u64 submit_tick) {
                frame->submit_tick = submit_tick;
            });
        scheduler.WaitWorker();
        std::scoped_lock lock{swapchain_mutex};
        FinishPresent(frame);
        free_queue.push(frame);
        return;
    }

    scheduler.FlushWithDynamicSubmission(
        prepare, [this, frame](vk::CommandBuffer, u64 submit_tick) {
            // Publish the acquire-semaphore reuse tick before the frame becomes visible to the
            // presentation thread. The queue mutex provides the cross-thread ordering.
            frame->submit_tick = submit_tick;
            {
                std::scoped_lock lock{queue_mutex};
                present_queue.push(frame);
            }
            frame_cv.notify_one();
        });
}

void PresentWindow::WaitPresent() {
    if (!use_present_thread) {
        return;
    }

    // Wait for the present queue to be empty
    {
        std::unique_lock queue_lock{queue_mutex};
        frame_cv.wait(queue_lock, [this] { return present_queue.empty(); });
    }

    // The above condition will be satisfied when the last frame is taken from the queue.
    // To ensure that frame has been presented as well take hold of the swapchain
    // mutex.
    std::scoped_lock swapchain_lock{swapchain_mutex};
}

void PresentWindow::PresentThread(std::stop_token token) {
    Common::SetCurrentThreadName("VulkanPresent");
    while (!token.stop_requested()) {
        std::unique_lock lock{queue_mutex};

        // Wait for presentation frames
        Common::CondvarWait(frame_cv, lock, token, [this] { return !present_queue.empty(); });
        if (token.stop_requested()) {
            return;
        }

        // Take the frame and notify anyone waiting
        Frame* frame = present_queue.front();
        present_queue.pop();
        frame_cv.notify_one();

        // By exchanging the lock ownership we take the swapchain lock
        // before the queue lock goes out of scope. This way the swapchain
        // lock in WaitPresent is guaranteed to occur after here.
        std::exchange(lock, std::unique_lock{swapchain_mutex});

        FinishPresent(frame);

        // Free the frame for reuse
        {
            std::scoped_lock fl{free_mutex};
            free_queue.push(frame);
        }
        free_cv.notify_one();
    }
}

void PresentWindow::NotifySurfaceChanged() {
#ifdef ANDROID
    std::scoped_lock lock{recreate_surface_mutex};

    // surfaceChanged() may notify us that a surface has changed
    // for the same surface multiple times. If that is the case
    // skip creating the surface again as that would cause a
    // vulkan ErrorNativeWindowInUseKHR.
    void* const render_surface = emu_window.GetWindowInfo().render_surface;
    if (render_surface == last_render_surface) {
        return;
    }
    last_render_surface = render_surface;

    // If an earlier notification produced a surface that CopyToSwapchain() has not consumed yet,
    // release it rather than just overwritting its handle and causing a leak.
    if (next_surface && next_surface != surface) {
        instance.GetInstance().destroySurfaceKHR(next_surface);
        next_surface = vk::SurfaceKHR{};
    }

    next_surface = CreateSurface(instance.GetInstance(), emu_window);
    recreate_surface_cv.notify_one();
#endif
}

void PresentWindow::RecreateSwapchain(Frame* frame) {
#ifdef ANDROID
    {
        std::unique_lock lock{recreate_surface_mutex};
        // NotifySurfaceChanged() delivers a replacement surface only when the frontend hands
        // over a different window. An out-of-date or stalled swapchain on a window that is
        // still alive is rebuilt on the current surface. Waiting here for a surface that never
        // comes left every thread idle (E.X. Troopers, 2026-09-18). Only a destroyed window
        // waits for the next surfaceChanged().
        const bool window_alive = emu_window.GetWindowInfo().render_surface != nullptr;
        if (!window_alive || !surface) {
            recreate_surface_cv.wait(lock, [this]() { return surface != next_surface; });
        }
        if (next_surface != surface) {
            surface = next_surface;
        }
    }
#endif
    std::scoped_lock submit_lock{scheduler.submit_mutex};
    graphics_queue.waitIdle();
#ifdef ANDROID
    DestroyDirectFramebuffers();
#endif
    swapchain.Create(frame->width, frame->height, surface, low_refresh_rate);
#ifdef ANDROID
    CreateDirectFramebuffers();
#endif
    ++swapchain_generation;
    LOG_INFO(Render_Vulkan, "Swapchain rebuilt (generation {})", swapchain_generation);
}

void PresentWindow::PrepareForPresent(vk::CommandBuffer cmdbuf, Frame* frame) {
    const auto restore_frame = [cmdbuf, frame] {
        const vk::ImageMemoryBarrier restore_barrier = {
            .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
            .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
            .oldLayout = vk::ImageLayout::eTransferSrcOptimal,
            .newLayout = vk::ImageLayout::eGeneral,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = frame->image,
            .subresourceRange{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                               vk::PipelineStageFlagBits::eColorAttachmentOutput,
                               vk::DependencyFlagBits::eByRegion, {}, {}, restore_barrier);
    };

    std::unique_lock swapchain_lock{swapchain_mutex};
    frame->present_valid = false;
    frame->direct_present = false;
    // Runs with swapchain_lock held on every exit of the acquire below.
    const auto finish_copy_acquire = [this, frame] {
        if (std::exchange(frame->copy_acquire_pending, false)) {
            --pending_copy_acquires;
        }
    };

#ifndef ANDROID
    const bool use_vsync = Settings::values.use_vsync.GetValue();
    const bool size_changed =
        swapchain.GetWidth() != frame->width || swapchain.GetHeight() != frame->height;
    const bool vsync_changed = vsync_enabled != use_vsync;
    if (vsync_changed || size_changed) [[unlikely]] {
        vsync_enabled = use_vsync;
        finish_copy_acquire();
        restore_frame();
        return;
    }
#endif

    AcquiredSwapchainImage acquired_image;
    // Acquire retries in a row. When the compositor keeps every image the acquire never
    // succeeds. After a bound of about one second the swapchain is rebuilt instead of spinning
    // forever; a normal frame waits a few milliseconds here at most.
    u32 acquire_retries = 0;
    constexpr u32 acquire_retry_limit = 1000;
    for (;;) {
        switch (swapchain.AcquireNextImage(frame->image_acquired, acquired_image)) {
        case SwapchainAcquireResult::Success:
            frame->present_image = acquired_image.image;
            frame->present_image_index = acquired_image.index;
            frame->present_ready = acquired_image.present_ready;
            frame->present_generation = swapchain_generation;
            frame->present_valid = true;
            break;
        case SwapchainAcquireResult::Recreate:
            finish_copy_acquire();
            restore_frame();
            return;
        case SwapchainAcquireResult::Retry:
            if (++acquire_retries >= acquire_retry_limit) {
                LOG_WARNING(Render_Vulkan,
                            "Swapchain acquire stalled for {} retries; rebuilding the swapchain",
                            acquire_retries);
                swapchain.MarkForRecreation();
                finish_copy_acquire();
                restore_frame();
                return;
            }
            // A blocking acquire must not retain the host-synchronization lock needed by the
            // present thread to return prior images. This slow recovery path sleeps after the
            // finite Vulkan timeout rather than burning a CPU core in a zero-timeout spin.
            swapchain_lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
            swapchain_lock.lock();
            continue;
        }
        break;
    }
    finish_copy_acquire();

    const vk::Extent2D extent = swapchain.GetExtent();
    VideoCore::AddFrameProfileEvent(VideoCore::FrameProfileEvent::PresentFrames);
    VideoCore::AddFrameProfileEvent(VideoCore::FrameProfileEvent::PresentPixels,
                                    static_cast<u64>(extent.width) * extent.height);
    const std::array pre_barriers = {
        vk::ImageMemoryBarrier{
            .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
            .dstAccessMask = vk::AccessFlagBits::eTransferRead,
            .oldLayout = vk::ImageLayout::eTransferSrcOptimal,
            .newLayout = vk::ImageLayout::eTransferSrcOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = frame->image,
            .subresourceRange{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        },
        vk::ImageMemoryBarrier{
            .srcAccessMask = vk::AccessFlagBits::eNone,
            .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eTransferDstOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = frame->present_image,
            .subresourceRange{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        },
    };

    // The final composition and swapchain copy now share one command buffer and one queue submit.
    // Make the color output visible to transfer while preparing the acquired image for its first
    // transfer write. The binary acquire wait is scoped to Transfer at submission time.
    cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput |
                               vk::PipelineStageFlagBits::eTopOfPipe,
                           vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlagBits::eByRegion,
                           {}, {}, pre_barriers);

    if (ShouldBlitToSwapchain(blit_supported, frame->width, frame->height, extent.width,
                              extent.height)) {
        VideoCore::AddFrameProfileEvent(VideoCore::FrameProfileEvent::PresentBlits);
        cmdbuf.blitImage(frame->image, vk::ImageLayout::eTransferSrcOptimal, frame->present_image,
                         vk::ImageLayout::eTransferDstOptimal,
                         MakeImageBlit(frame->width, frame->height, extent.width, extent.height),
                         Settings::values.filter_mode.GetValue() ? vk::Filter::eLinear
                                                                 : vk::Filter::eNearest);
    } else {
        VideoCore::AddFrameProfileEvent(VideoCore::FrameProfileEvent::PresentCopies);
        cmdbuf.copyImage(frame->image, vk::ImageLayout::eTransferSrcOptimal, frame->present_image,
                         vk::ImageLayout::eTransferDstOptimal,
                         MakeImageCopy(frame->width, frame->height, extent.width, extent.height));
    }

    const std::array post_barriers = {
        vk::ImageMemoryBarrier{
            .srcAccessMask = vk::AccessFlagBits::eTransferRead,
            .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
            .oldLayout = vk::ImageLayout::eTransferSrcOptimal,
            .newLayout = vk::ImageLayout::eGeneral,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = frame->image,
            .subresourceRange{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        },
        vk::ImageMemoryBarrier{
            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = vk::AccessFlagBits::eNone,
            .oldLayout = vk::ImageLayout::eTransferDstOptimal,
            .newLayout = vk::ImageLayout::ePresentSrcKHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = frame->present_image,
            .subresourceRange{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        },
    };

    cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                           vk::PipelineStageFlagBits::eColorAttachmentOutput |
                               vk::PipelineStageFlagBits::eBottomOfPipe,
                           vk::DependencyFlagBits::eByRegion, {}, {}, post_barriers);
}

void PresentWindow::FinishPresent(Frame* frame) {
    if (!frame->present_valid) [[unlikely]] {
        if (swapchain.NeedsRecreation()) {
            RecreateSwapchain(frame);
        }
        return;
    }
    if (frame->present_generation != swapchain_generation) [[unlikely]] {
        // The image index belongs to a swapchain that was rebuilt after the acquire. Its
        // semaphores went with it; presenting the index into the new swapchain is invalid.
        frame->present_valid = false;
        return;
    }
    std::scoped_lock submit_lock{scheduler.submit_mutex};
    swapchain.Present(frame->present_image_index);
    frame->present_valid = false;
}

vk::RenderPass PresentWindow::CreateRenderpass(bool direct_to_swapchain) {
    const vk::AttachmentReference color_ref = {
        .attachment = 0,
        .layout = vk::ImageLayout::eGeneral,
    };

    const vk::SubpassDescription subpass = {
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = 1u,
        .pColorAttachments = &color_ref,
        .pResolveAttachments = 0,
        .pDepthStencilAttachment = nullptr,
    };

    const vk::AttachmentDescription color_attachment = {
        .format = swapchain.GetSurfaceFormat().format,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout =
            direct_to_swapchain ? vk::ImageLayout::eUndefined : vk::ImageLayout::eGeneral,
        .finalLayout = direct_to_swapchain ? vk::ImageLayout::ePresentSrcKHR
                                           : vk::ImageLayout::eTransferSrcOptimal,
    };

    const vk::SubpassDependency acquire_dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .srcAccessMask = vk::AccessFlagBits::eNone,
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
        .dependencyFlags = vk::DependencyFlagBits::eByRegion,
    };

    const vk::RenderPassCreateInfo renderpass_info = {
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = direct_to_swapchain ? 1u : 0u,
        .pDependencies = direct_to_swapchain ? &acquire_dependency : nullptr,
    };

    return instance.GetDevice().createRenderPass(renderpass_info);
}

#ifdef ANDROID
void PresentWindow::CreateDirectFramebuffers() {
    if (swapchain.NeedsRecreation()) {
        return;
    }

    const vk::Device device = instance.GetDevice();
    const vk::Format format = swapchain.GetSurfaceFormat().format;
    const vk::Extent2D extent = swapchain.GetExtent();
    const auto& images = swapchain.GetImages();

    direct_image_views.reserve(images.size());
    direct_framebuffers.reserve(images.size());
    for (const vk::Image image : images) {
        const vk::ImageViewCreateInfo view_info = {
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .subresourceRange{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        direct_image_views.push_back(device.createImageView(view_info));

        const vk::ImageView image_view = direct_image_views.back();
        const vk::FramebufferCreateInfo framebuffer_info = {
            .renderPass = direct_present_renderpass,
            .attachmentCount = 1,
            .pAttachments = &image_view,
            .width = extent.width,
            .height = extent.height,
            .layers = 1,
        };
        direct_framebuffers.push_back(device.createFramebuffer(framebuffer_info));
    }
}

void PresentWindow::DestroyDirectFramebuffers() {
    const vk::Device device = instance.GetDevice();
    for (const vk::Framebuffer framebuffer : direct_framebuffers) {
        device.destroyFramebuffer(framebuffer);
    }
    for (const vk::ImageView image_view : direct_image_views) {
        device.destroyImageView(image_view);
    }
    direct_framebuffers.clear();
    direct_image_views.clear();
}
#endif

} // namespace Vulkan
