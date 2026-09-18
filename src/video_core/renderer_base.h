// Copyright 2014-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include "common/common_types.h"
#include "core/frontend/framebuffer_layout.h"
#include "video_core/rasterizer_interface.h"

namespace Frontend {
class EmuWindow;
}

namespace Core {
class System;
}

namespace VideoCore {

enum class ScreenId : u32 {
    TopLeft,
    TopRight,
    Bottom,
};

/// Returns whether drawing this layout can sample the top-screen right-eye image.
constexpr bool PresentationNeedsRightEye(const Layout::FramebufferLayout& layout,
                                         Settings::MonoRenderOption mono_render_option) {
    const bool presents_top = layout.top_screen_enabled || (layout.additional_screen_enabled &&
                                                            !layout.additional_screen_is_bottom);
    if (!presents_top) {
        return false;
    }

    return layout.render_3d_mode != Settings::StereoRenderOption::Off ||
           mono_render_option == Settings::MonoRenderOption::RightEye;
}

/// Returns whether a host output needs the guest render targets prepared for this swap. Screenshots
/// are explicit captures and therefore override duplicate-frame suppression.
constexpr bool NeedsFramePreparation(bool should_present, bool screenshot_pending,
                                     bool frame_dumping, bool skip_duplicate_frame) noexcept {
    return screenshot_pending ||
           (!skip_duplicate_frame && (should_present || frame_dumping));
}

static_assert(NeedsFramePreparation(true, false, false, false));
static_assert(!NeedsFramePreparation(true, false, false, true));
static_assert(NeedsFramePreparation(true, true, false, true));
static_assert(!NeedsFramePreparation(false, false, true, true));
static_assert(NeedsFramePreparation(false, false, true, false));
static_assert(!NeedsFramePreparation(false, false, false, false));

/// Returns whether Eco Turbo should cap Android host presentation. A zero limit means fully
/// uncapped emulation and needs the same protection as an explicit turbo limit above normal speed.
constexpr bool EcoPresentationCapActive(double frame_limit, bool eco_turbo) noexcept {
    return eco_turbo && (frame_limit == 0.0 || frame_limit > 100.0);
}

static_assert(!EcoPresentationCapActive(100.0, true));
static_assert(EcoPresentationCapActive(200.0, true));
static_assert(EcoPresentationCapActive(0.0, true));
static_assert(!EcoPresentationCapActive(0.0, false));

struct RendererSettings {
    // Screenshot
    std::atomic_bool screenshot_requested{false};
    void* screenshot_bits{};
    std::function<void(bool)> screenshot_complete_callback;
    Layout::FramebufferLayout screenshot_framebuffer_layout;
    // Renderer
    std::atomic_bool bg_color_update_requested{false};
    std::atomic_bool shader_update_requested{false};
};

class RendererBase : NonCopyable {
public:
    explicit RendererBase(Core::System& system, Frontend::EmuWindow& window,
                          Frontend::EmuWindow* secondary_window);
    virtual ~RendererBase();

    /// Returns the rasterizer owned by the renderer
    virtual VideoCore::RasterizerInterface* Rasterizer() = 0;

    /// Finalize rendering the guest frame and draw into the presentation texture
    virtual void SwapBuffers() = 0;

    /// Draws the latest frame to the window waiting timeout_ms for a frame to arrive (Renderer
    /// specific implementation)
    virtual void TryPresent(int timeout_ms, bool is_secondary) = 0;
    virtual void TryPresent(int timeout_ms) {
        TryPresent(timeout_ms, false);
    }

    /// Prepares for video dumping (e.g. create necessary buffers, etc)
    virtual void PrepareVideoDumping() {}

    /// Cleans up after video dumping is ended
    virtual void CleanupVideoDumping() {}

    /// This is called to notify the rendering backend of a surface change
    // if second == true then it is the second screen
    virtual void NotifySurfaceChanged(bool second) {}

    /// Returns the resolution scale factor relative to the native 3DS screen resolution
    u32 GetResolutionScaleFactor();

    /// Updates the framebuffer layout of the contained render window handle.
    void UpdateCurrentFramebufferLayout(bool is_portrait_mode = {});

    /// Ends the current frame
    void EndFrame();

    f32 GetCurrentFPS() const {
        return current_fps;
    }

    u64 GetCurrentFrame() const {
        return current_frame;
    }

    Frontend::EmuWindow& GetRenderWindow() {
        return render_window;
    }

    const Frontend::EmuWindow& GetRenderWindow() const {
        return render_window;
    }

    [[nodiscard]] RendererSettings& Settings() {
        return settings;
    }

    [[nodiscard]] const RendererSettings& Settings() const {
        return settings;
    }

    /// Returns true if a screenshot is being processed
    [[nodiscard]] bool IsScreenshotPending() const;

    /// Request a screenshot of the next frame
    void RequestScreenshot(void* data, std::function<void(bool)> callback,
                           const Layout::FramebufferLayout& layout);

protected:
    /// Limit Android turbo presentation to the normal 3DS refresh rate while keeping emulation at
    /// the requested speed. This skips only host layout, composition, and presentation passes.
    bool ShouldPresentFrame();

    Core::System& system;
    RendererSettings settings;
    Frontend::EmuWindow& render_window;    /// Reference to the render window handle.
    Frontend::EmuWindow* secondary_window; /// Reference to the secondary render window handle.

protected:
    f32 current_fps = 0.0f; /// Current framerate, should be set by the renderer
    u64 current_frame = 0;  /// Current frame, should be set by the renderer
#ifdef ANDROID
    std::chrono::steady_clock::time_point eco_turbo_budget_update =
        std::chrono::steady_clock::now();
    double eco_turbo_present_budget = 1.0;
#endif
};

} // namespace VideoCore
