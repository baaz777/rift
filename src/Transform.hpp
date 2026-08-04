#pragma once

#include <glm/glm.hpp>

/**
 * @struct Transform
 * @brief Bottom-center feet anchor in world pixels, shared by sorting and collision.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 */
struct Transform
{
    glm::vec2 position{0.0f, 0.0f};
};
