#pragma once

[[noreturn]] void RiftEnTTAssert(const char* message) noexcept;

#define ENTT_ASSERT(condition, message) \
    ((condition) ? static_cast<void>(0) : RiftEnTTAssert(message))
