#pragma once

#include <string>
#include <string_view>
#include <vector>

/**
 * @brief Opt-in frame trace of render sections and GPU submissions.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 *
 * drawCount differences count submissions between events. Call only from the render thread.
 * OpenGL records batch flushes and draw entry points; Vulkan records section markers only.
 */
namespace DrawTracer
{
/**
 * @struct Event
 * @brief Trace event in capture order.
 * @ingroup Rendering
 */
struct Event
{
    int drawCount;
    std::string label;  ///< Free-form description (section name or flush reason).
};

/**
 * @fn void DrawTracer::SetEnabled(bool enabled)
 * @brief Disabling frees storage and discards both current and completed events.
 * @author Alex (<https://github.com/lextpf>)
 */
void SetEnabled(bool enabled);

/**
 * @fn bool DrawTracer::IsEnabled()
 * @brief Whether capture is on.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Call sites guard label formatting with this so the snprintf cost disappears when tracing is off.
 *
 */
bool IsEnabled();

/**
 * @fn void DrawTracer::BeginFrame()
 * @brief Swap the live event list to "last completed frame" and clear the live buffer for the next
 * frame's events.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Safe to call when disabled (no-op).
 *
 */
void BeginFrame();

/**
 * @fn void DrawTracer::Mark(std::string_view label, int currentDrawCount)
 * @brief Copies an event into the current frame; drops events after 50,000.
 * @author Alex (<https://github.com/lextpf>)
 *
 * currentDrawCount is the renderer's cumulative draw-call count.
 */
void Mark(std::string_view label, int currentDrawCount);

/**
 * @fn const std::vector<Event>& DrawTracer::LastFrameEvents()
 * @brief Read the events captured during the most recently completed frame.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Returns an empty vector before the first BeginFrame swap.
 *
 */
const std::vector<Event>& LastFrameEvents();

/**
 * @fn void DrawTracer::Clear()
 * @brief Drop any captured events without changing the enabled state.
 * @author Alex (<https://github.com/lextpf>)
 */
void Clear();
}  // namespace DrawTracer
