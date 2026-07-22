#include "AnimationState.hpp"
#include "Appearance.hpp"
#include "Dialogue.hpp"
#include "DialogueHandle.hpp"
#include "Elevation.hpp"
#include "Facing.hpp"
#include "Hitbox.hpp"
#include "Identity.hpp"
#include "Motor.hpp"
#include "MotorParams.hpp"
#include "NpcIdle.hpp"
#include "NpcSprite.hpp"
#include "NpcTag.hpp"
#include "Patrol.hpp"
#include "PatrolRoute.hpp"
#include "PlayerInputState.hpp"
#include "PlayerModes.hpp"
#include "PlayerMovementState.hpp"
#include "PlayerSprite.hpp"
#include "PlayerTag.hpp"
#include "Speed.hpp"
#include "TextureHandle.hpp"
#include "Transform.hpp"

#include <type_traits>

namespace
{
template <class T>
inline constexpr bool component_layout_ready = std::is_aggregate_v<T> && !std::is_polymorphic_v<T>;
}  // namespace

// components remain plain, non-polymorphic aggregates so their data layout stays explicit.
static_assert(component_layout_ready<Transform>);
static_assert(component_layout_ready<Elevation>);
static_assert(component_layout_ready<Facing>);
static_assert(component_layout_ready<AnimationState>);
static_assert(component_layout_ready<Identity>);
static_assert(component_layout_ready<MotorParams>);
static_assert(component_layout_ready<Motor>);
static_assert(component_layout_ready<PlayerMovementState>);
static_assert(component_layout_ready<NpcIdle>);
static_assert(component_layout_ready<Patrol>);
static_assert(component_layout_ready<PlayerModes>);
static_assert(component_layout_ready<PlayerInputState>);
static_assert(component_layout_ready<Appearance>);
static_assert(component_layout_ready<Dialogue>);
static_assert(component_layout_ready<DialogueHandle>);
static_assert(component_layout_ready<Speed>);
static_assert(component_layout_ready<Hitbox>);
static_assert(component_layout_ready<NpcSprite>);
static_assert(component_layout_ready<PlayerSprite>);
static_assert(component_layout_ready<TextureHandle>);

// tags remain empty because EnTT omits empty values from each() callbacks.
static_assert(std::is_empty_v<PlayerTag>);
static_assert(std::is_empty_v<NpcTag>);

// PatrolRoute owns a vector, so it uses move semantics instead of the aggregate policy.
static_assert(std::is_move_constructible_v<PatrolRoute>);
static_assert(std::is_swappable_v<PatrolRoute>);
