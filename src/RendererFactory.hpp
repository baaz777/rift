#pragma once

#include "IRenderer.hpp"
#include "RendererAPI.hpp"

#include <memory>

struct GLFWwindow;

/**
 * @brief Constructs the runtime rendering backend.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 */

/**
 * @fn bool IsRendererAvailable(RendererAPI api)
 * @brief Both compiled backends are available.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 */
bool IsRendererAvailable(RendererAPI api);

/**
 * @fn std::unique_ptr<IRenderer> CreateRenderer(RendererAPI& api, GLFWwindow* window)
 * @brief Falls back to OpenGL if Vulkan construction throws.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 *
 * Call Init and check its result before drawing. OpenGL construction failures propagate.
 *
 * @param api Updated to OpenGL on fallback.
 * @param window Vulkan window; OpenGL uses the current context.
 */
std::unique_ptr<IRenderer> CreateRenderer(RendererAPI& api, GLFWwindow* window);
