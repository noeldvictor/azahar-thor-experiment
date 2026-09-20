// Copyright 2024-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/settings.h"
#include "video_core/frame_profile.h"
#include "video_core/rasterizer_cache/pixel_format.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include <algorithm>
#include <string>
#include "video_core/renderer_vulkan/vk_render_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_texture_runtime.h"

namespace Vulkan {

constexpr u32 MinDrawsToFlush = 20;

using VideoCore::PixelFormat;
using VideoCore::SurfaceType;

RenderManager::RenderManager(const Instance& instance, Scheduler& scheduler)
    : instance{instance}, scheduler{scheduler} {}

RenderManager::~RenderManager() = default;

void RenderManager::BeginRendering(const Framebuffer* framebuffer,
                                   Common::Rectangle<u32> draw_rect) {
    const vk::Rect2D render_area = {
        .offset{
            .x = static_cast<s32>(draw_rect.left),
            .y = static_cast<s32>(draw_rect.bottom),
        },
        .extent{
            .width = draw_rect.GetWidth(),
            .height = draw_rect.GetHeight(),
        },
    };
    const RenderPass new_pass = {
        .framebuffer = framebuffer->Handle(),
        .render_pass = framebuffer->RenderPass(),
        .render_area = render_area,
        .clear = {},
        .do_clear = false,
    };
    if (pass.render_pass && !(pass == new_pass)) {
        ClassifyRestart(new_pass, framebuffer->Images());
    }
    images = framebuffer->Images();
    aspects = framebuffer->Aspects();
    shadow_rendering = framebuffer->shadow_rendering;
    BeginRendering(new_pass);
}

void RenderManager::ClassifyRestart(const RenderPass& new_pass,
                                    const std::array<vk::Image, 2>& new_images) const {
    using VideoCore::FrameProfileEvent;
    const auto contains = [](const vk::Rect2D& outer, const vk::Rect2D& inner) {
        return inner.offset.x >= outer.offset.x && inner.offset.y >= outer.offset.y &&
               inner.offset.x + static_cast<s32>(inner.extent.width) <=
                   outer.offset.x + static_cast<s32>(outer.extent.width) &&
               inner.offset.y + static_cast<s32>(inner.extent.height) <=
                   outer.offset.y + static_cast<s32>(outer.extent.height);
    };
    if (pass.framebuffer != new_pass.framebuffer || pass.render_pass != new_pass.render_pass) {
        if (images[0] != new_images[0]) {
            VideoCore::AddFrameProfileEvent(FrameProfileEvent::RenderPassRestartColorSwitch);
        } else if ((images[1] == VK_NULL_HANDLE) != (new_images[1] == VK_NULL_HANDLE)) {
            VideoCore::AddFrameProfileEvent(FrameProfileEvent::RenderPassRestartDepthToggle);
        } else {
            VideoCore::AddFrameProfileEvent(FrameProfileEvent::RenderPassRestartSameImages);
        }
    } else if (pass.render_area != new_pass.render_area) {
        if (contains(pass.render_area, new_pass.render_area)) {
            VideoCore::AddFrameProfileEvent(FrameProfileEvent::RenderPassRestartAreaShrink);
        } else if (contains(new_pass.render_area, pass.render_area)) {
            VideoCore::AddFrameProfileEvent(FrameProfileEvent::RenderPassRestartAreaGrow);
        } else {
            VideoCore::AddFrameProfileEvent(FrameProfileEvent::RenderPassRestartAreaOther);
        }
    } else {
        VideoCore::AddFrameProfileEvent(FrameProfileEvent::RenderPassRestartClear);
    }
}

void RenderManager::BeginRendering(const RenderPass& new_pass) {
    if (pass == new_pass) [[likely]] {
        VideoCore::AddFrameProfileEvent(VideoCore::FrameProfileEvent::RenderPassReuses);
        num_draws++;
        return;
    }

#if THOR_FRAME_PROFILING
    if (pass.render_pass && pass_trace.size() < MaxTimedPasses) {
        pass_trace.push_back({images[0], pass.render_area.extent.width,
                              pass.render_area.extent.height, num_draws});
    }
#endif
    num_draws = 0;

    EndRendering();
    VideoCore::AddFrameProfileEvent(VideoCore::FrameProfileEvent::RenderPassBegins);
#if THOR_FRAME_PROFILING
    if (!timestamp_pool) {
        const vk::QueryPoolCreateInfo pool_info = {
            .queryType = vk::QueryType::eTimestamp,
            .queryCount = MaxTimedPasses * 2,
        };
        timestamp_pool = instance.GetDevice().createQueryPoolUnique(pool_info);
        scheduler.Record([pool = *timestamp_pool](vk::CommandBuffer cmdbuf) {
            cmdbuf.resetQueryPool(pool, 0, MaxTimedPasses * 2);
        });
    }
    if (timestamp_index < MaxTimedPasses) {
        scheduler.Record([pool = *timestamp_pool, slot = timestamp_index](
                             vk::CommandBuffer cmdbuf) {
            cmdbuf.writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, pool, slot * 2);
        });
    }
#endif
    scheduler.Record([info = new_pass](vk::CommandBuffer cmdbuf) {
        const vk::RenderPassBeginInfo renderpass_begin_info = {
            .renderPass = info.render_pass,
            .framebuffer = info.framebuffer,
            .renderArea = info.render_area,
            .clearValueCount = info.do_clear ? 1u : 0u,
            .pClearValues = &info.clear,
        };
        cmdbuf.beginRenderPass(renderpass_begin_info, vk::SubpassContents::eInline);
    });

    pass = new_pass;
}

void RenderManager::ReportPassTrace() {
#if !THOR_FRAME_PROFILING
    return;
#else
    const auto now = std::chrono::steady_clock::now();
    if (now - last_trace_log < std::chrono::seconds{1} || pass_trace.empty()) {
        pass_trace.clear();
        return;
    }
    last_trace_log = now;

    // Give each distinct colour target of this frame a small number, so the sequence shows the
    // switching pattern rather than a list of handles.
    std::vector<vk::Image> targets;
    std::string sequence;
    u32 total_draws = 0;
    for (const TracedPass& entry : pass_trace) {
        const auto it = std::find(targets.begin(), targets.end(), entry.color);
        std::size_t id = static_cast<std::size_t>(std::distance(targets.begin(), it));
        if (it == targets.end()) {
            targets.push_back(entry.color);
        }
        total_draws += entry.draws;
        if (sequence.size() < 1200) {
            sequence += fmt::format("{}({}x{}):{} ", id, entry.width, entry.height, entry.draws);
        }
    }
    LOG_INFO(Render_Vulkan, "ThorPasses passes={} targets={} draws={} seq={}", pass_trace.size(),
             targets.size(), total_draws, sequence);

    // GPU time per pass. The stall is acceptable once per second in a profiling build.
    if (timestamps_ready && timestamp_pool) {
        const u32 timed = std::min<u32>(timestamp_index, MaxTimedPasses);
        scheduler.Finish();
        std::vector<u64> stamps(timed * 2, 0);
        const vk::Result result = instance.GetDevice().getQueryPoolResults(
            *timestamp_pool, 0, timed * 2, stamps.size() * sizeof(u64), stamps.data(),
            sizeof(u64), vk::QueryResultFlagBits::e64);
        if (result == vk::Result::eSuccess) {
            const double period =
                instance.GetPhysicalDevice().getProperties().limits.timestampPeriod;
            double total_ms = 0.0;
            std::vector<std::pair<double, std::size_t>> by_pass;
            for (u32 i = 0; i < timed; i++) {
                const double ms = (stamps[i * 2 + 1] - stamps[i * 2]) * period / 1.0e6;
                total_ms += ms;
                by_pass.emplace_back(ms, i);
            }
            std::sort(by_pass.begin(), by_pass.end(),
                      [](const auto& a, const auto& b) { return a.first > b.first; });
            std::string top;
            for (std::size_t i = 0; i < by_pass.size() && i < 8; i++) {
                const std::size_t idx = by_pass[i].second;
                const TracedPass& entry =
                    idx < pass_trace.size() ? pass_trace[idx] : TracedPass{};
                top += fmt::format("{:.2f}ms({}x{},{}draws) ", by_pass[i].first, entry.width,
                                   entry.height, entry.draws);
            }
            // Passes overlap on the GPU, so these intervals include queued work and must not
            // be read as the cost of one pass. Use them to compare like with like across a
            // change, never to attribute a share of the frame.
            LOG_INFO(Render_Vulkan, "ThorPassTime timed={} sum_overlapping_ms={:.2f} top={}",
                     timed, total_ms, top);
        }
        scheduler.Record([pool = *timestamp_pool](vk::CommandBuffer cmdbuf) {
            cmdbuf.resetQueryPool(pool, 0, MaxTimedPasses * 2);
        });
    }
    timestamp_index = 0;
    timestamps_ready = false;
    pass_trace.clear();
#endif
}

void RenderManager::EndRendering() {
    if (!pass.render_pass) {
        return;
    }

    VideoCore::AddFrameProfileEvent(VideoCore::FrameProfileEvent::RenderPassEnds);

#if THOR_FRAME_PROFILING
    if (timestamp_pool && timestamp_index < MaxTimedPasses) {
        scheduler.Record([pool = *timestamp_pool, slot = timestamp_index](
                             vk::CommandBuffer cmdbuf) {
            cmdbuf.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, pool, slot * 2 + 1);
        });
        timestamp_index++;
        timestamps_ready = true;
    }
#endif

    scheduler.Record([images = images, aspects = aspects,
                      shadow_rendering = shadow_rendering,
                      skip_barriers = Settings::values.skip_pass_barriers.GetValue()](
                         vk::CommandBuffer cmdbuf) {
        if (skip_barriers) {
            cmdbuf.endRenderPass();
            return;
        }
        u32 num_barriers = 0;
        vk::PipelineStageFlags pipeline_flags{};
        vk::AccessFlags src_access_flags{};
        std::array<vk::ImageMemoryBarrier, 2> barriers;
        for (u32 i = 0; i < images.size(); i++) {
            if (!images[i]) {
                continue;
            }
            const bool is_color = static_cast<bool>(aspects[i] & vk::ImageAspectFlagBits::eColor);
            if (is_color) {
                pipeline_flags |= shadow_rendering
                                      ? vk::PipelineStageFlagBits::eFragmentShader
                                      : vk::PipelineStageFlagBits::eColorAttachmentOutput;
                src_access_flags = shadow_rendering ? vk::AccessFlagBits::eShaderWrite
                                                    : vk::AccessFlagBits::eColorAttachmentWrite;
            } else {
                pipeline_flags |= vk::PipelineStageFlagBits::eEarlyFragmentTests |
                                  vk::PipelineStageFlagBits::eLateFragmentTests;
                src_access_flags = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
            }
            barriers[num_barriers++] = vk::ImageMemoryBarrier{
                .srcAccessMask = src_access_flags,
                .dstAccessMask =
                    vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eTransferRead,
                .oldLayout = vk::ImageLayout::eGeneral,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = images[i],
                .subresourceRange{
                    .aspectMask = aspects[i],
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = VK_REMAINING_ARRAY_LAYERS,
                },
            };
        }
        cmdbuf.endRenderPass();
        if (num_barriers == 0) {
            return;
        }
        VideoCore::AddFrameProfileEvent(VideoCore::FrameProfileEvent::RenderPassImageBarriers,
                                        num_barriers);
        cmdbuf.pipelineBarrier(pipeline_flags,
                               vk::PipelineStageFlagBits::eFragmentShader |
                                   vk::PipelineStageFlagBits::eTransfer,
                               vk::DependencyFlagBits::eByRegion, 0, nullptr, 0, nullptr,
                               num_barriers, barriers.data());
    });

    // Reset state.
    pass.render_pass = VK_NULL_HANDLE;
    images = {};
    aspects = {};
    shadow_rendering = false;

    // The Mali guide recommends flushing at the end of each major renderpass
    // Testing has shown this has a significant effect on rendering performance
    if (num_draws > MinDrawsToFlush && instance.ShouldFlush()) {
        VideoCore::AddFrameProfileEvent(VideoCore::FrameProfileEvent::MaliRenderPassFlushes);
        scheduler.Flush();
        num_draws = 0;
    }
}

vk::RenderPass RenderManager::GetRenderpass(VideoCore::PixelFormat color,
                                            VideoCore::PixelFormat depth, bool is_clear) {
    std::scoped_lock lock{cache_mutex};

    const u32 color_index =
        color == VideoCore::PixelFormat::Invalid ? NumColorFormats : static_cast<u32>(color);
    const u32 depth_index =
        depth == VideoCore::PixelFormat::Invalid
            ? NumDepthFormats
            : (static_cast<u32>(depth - VideoCore::PixelFormat::NumColorFormat));

    ASSERT_MSG(color_index <= NumColorFormats && depth_index <= NumDepthFormats,
               "Invalid color index {} and/or depth_index {}", color_index, depth_index);

    vk::UniqueRenderPass& renderpass = cached_renderpasses[color_index][depth_index][is_clear];
    if (!renderpass) {
        const vk::Format color_format = instance.GetTraits(color).native;
        const vk::Format depth_format = instance.GetTraits(depth).native;
        const vk::AttachmentLoadOp load_op =
            is_clear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad;
        renderpass = CreateRenderPass(color_format, depth_format, load_op);
    }

    return *renderpass;
}

vk::UniqueRenderPass RenderManager::CreateRenderPass(vk::Format color, vk::Format depth,
                                                     vk::AttachmentLoadOp load_op) const {
    u32 attachment_count = 0;
    std::array<vk::AttachmentDescription, 2> attachments;

    bool use_color = false;
    vk::AttachmentReference color_attachment_ref{};
    bool use_depth = false;
    vk::AttachmentReference depth_attachment_ref{};

    if (color != vk::Format::eUndefined) {
        attachments[attachment_count] = vk::AttachmentDescription{
            .format = color,
            .loadOp = load_op,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
            .initialLayout = vk::ImageLayout::eGeneral,
            .finalLayout = vk::ImageLayout::eGeneral,
        };

        color_attachment_ref = vk::AttachmentReference{
            .attachment = attachment_count++,
            .layout = vk::ImageLayout::eGeneral,
        };

        use_color = true;
    }

    if (depth != vk::Format::eUndefined) {
        attachments[attachment_count] = vk::AttachmentDescription{
            .format = depth,
            .loadOp = load_op,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .stencilLoadOp = load_op,
            .stencilStoreOp = vk::AttachmentStoreOp::eStore,
            .initialLayout = vk::ImageLayout::eGeneral,
            .finalLayout = vk::ImageLayout::eGeneral,
        };

        depth_attachment_ref = vk::AttachmentReference{
            .attachment = attachment_count++,
            .layout = vk::ImageLayout::eGeneral,
        };

        use_depth = true;
    }

    const vk::SubpassDescription subpass = {
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = use_color ? 1u : 0u,
        .pColorAttachments = &color_attachment_ref,
        .pResolveAttachments = 0,
        .pDepthStencilAttachment = use_depth ? &depth_attachment_ref : nullptr,
    };

    const vk::RenderPassCreateInfo renderpass_info = {
        .attachmentCount = attachment_count,
        .pAttachments = attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 0,
        .pDependencies = nullptr,
    };

    return instance.GetDevice().createRenderPassUnique(renderpass_info);
}

} // namespace Vulkan
