#pragma once

#include "Logger.hpp"

#include <vulkan/vulkan.h>
#include <stdexcept>
#include <string>

/**
 * @brief Logs and throws std::runtime_error for a failed Vulkan result.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 *
 * Requires LOG_SUBSYSTEM in scope. evaluates x once and records its source text.
 * Do not reference a local named result inside x; the macro shadows it before evaluation.
 *
 * @warning Do not use in destructors, noexcept functions, or c callbacks.
 */
#define VK_CHECK(x)                                                             \
    do                                                                          \
    {                                                                           \
        VkResult result = x;                                                    \
        if (result != VK_SUCCESS)                                               \
        {                                                                       \
            Logger::ErrorF(LOG_SUBSYSTEM,                                       \
                           "Vulkan error: " #x " failed at {}:{} - {}",         \
                           __FILE__,                                            \
                           __LINE__,                                            \
                           static_cast<int>(result));                           \
            throw std::runtime_error("Vulkan error: " #x " failed (VkResult " + \
                                     std::to_string(result) + ")");             \
        }                                                                       \
    } while (0)
