#include "Logger.hpp"

#include <entt/ext/config.h>

#include <cstdlib>

[[noreturn]] void RiftEnTTAssert(const char* message) noexcept
{
    Logger::Error("ECS", message);
    std::abort();
}
