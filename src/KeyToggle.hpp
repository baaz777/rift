#pragma once

#include <GLFW/glfw3.h>

/**
 * @brief Detect one press edge for a group of alternative keys.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Input
 *
 * Poll after GLFW events, once per frame while the input handler is active. Any pressed key
 * latches the group until a poll observes all keys released. Pressing a second key while another
 * remains held does not produce a new edge. Skipped polls do not update the latch.
 *
 * @code{.cpp}
 * static KeyToggle<GLFW_KEY_E> eKey;
 * if (eKey.JustPressed(window)) { ... }
 * @endcode
 *
 * @code{.cpp}
 * static KeyToggle<GLFW_KEY_UP, GLFW_KEY_W> upKey;
 * if (upKey.JustPressed(window)) { ... }
 * @endcode
 */
template <int... Keys>
struct KeyToggle
{
    bool pressed = false;

    /**
     * @fn bool KeyToggle::JustPressed(GLFWwindow* window)
     * @brief Update the shared latch and report a press edge; window must be valid.
     * @author Alex (<https://github.com/lextpf>)
     */
    bool JustPressed(GLFWwindow* window)
    {
        if ((... || (glfwGetKey(window, Keys) == GLFW_PRESS)) && !pressed)
        {
            pressed = true;
            return true;
        }

        if ((... && (glfwGetKey(window, Keys) == GLFW_RELEASE)))
            pressed = false;
        return false;
    }
};
