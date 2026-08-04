#pragma once

/**
 * @brief Tuning constants shared by every character: sheet geometry, animation
 *        cadence, hitbox, speeds, walk cycle and step height.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 *
 * EntityStore copies base speeds into Speed at spawn. Hitbox copies its default dimensions;
 * NPC collision queries use the constants directly. Collision and support systems also read
 * the overlap tolerance and maximum step height directly.
 *
 * Units are world pixels and seconds. Pixel values are calibrated for 16 px tiles and do not
 * scale with the ProjectManifest tile dimensions.
 */
namespace CharacterConstants
{

inline constexpr int SPRITE_WIDTH = 32;   ///< Sheet cell width in pixels.
inline constexpr int SPRITE_HEIGHT = 32;  ///< Sheet cell height in pixels.
/// Floating-point sheet cell width in pixels.
inline constexpr float SPRITE_WIDTH_F = static_cast<float>(SPRITE_WIDTH);
/// Floating-point sheet cell height in pixels.
inline constexpr float SPRITE_HEIGHT_F = static_cast<float>(SPRITE_HEIGHT);

/**
 * @brief Animation frames per direction in a sprite row (the 3 cells walked
 * through by `WALK_SEQUENCE`; distinct from `WALK_SEQUENCE_LENGTH` = 4).
 */
inline constexpr int WALK_FRAME_COUNT = 3;
/// Seconds per walk/animation frame at the 1.0x cadence reference.
inline constexpr float ANIM_FRAME_DURATION = 0.15f;
/**
 * @brief AABB floating-point tolerance, in pixels.
 *
 * Shrinks every feet-anchored box inward on all sides before an overlap test, so two boxes
 * resting edge-to-edge do not register as colliding.
 */
inline constexpr float COLLISION_EPS = 0.05f;

/**
 * @brief Feet-anchored collision box, shared by the player and NPCs. It covers exactly one tile.
 *
 * Anchored at bottom-center: the box extends `HALF_HITBOX_WIDTH` to each side of the feet and
 * `HITBOX_HEIGHT` straight up. These are the defaults for the `Hitbox` component.
 */
inline constexpr float HITBOX_WIDTH = 16.0f;  ///< Full box width, pixels.
inline constexpr float HITBOX_HEIGHT = 16.0f;
inline constexpr float HALF_HITBOX_WIDTH = HITBOX_WIDTH / 2.0f;    ///< Half-width; 8px each side.
inline constexpr float HALF_HITBOX_HEIGHT = HITBOX_HEIGHT / 2.0f;  ///< Half-height; 8px.

/**
 * @brief Base walk speeds (px/s) and the multipliers layered on top of them.
 *
 * The base is written onto an entity's `Speed` component at spawn. A mode multiplier and
 * then the developer-console `speedMultiplier` are applied on top per frame; the modes are
 * mutually exclusive, resolving bicycle > run > walk.
 */
inline constexpr float PLAYER_BASE_SPEED = 50.0f;  ///< Player walk speed, px/s.
inline constexpr float NPC_BASE_SPEED = 25.0f;     ///< NPC patrol speed, px/s; half the player.
inline constexpr float RUN_SPEED_MULTIPLIER = 1.75f;
inline constexpr float BICYCLE_SPEED_MULTIPLIER = 2.25f;

/**
 * @brief Sheet columns for left contact, neutral, right contact, and neutral.
 *
 * Column 0 is the idle pose and occurs between the two contact frames.
 */
inline constexpr int WALK_SEQUENCE[4] = {1, 0, 2, 0};
inline constexpr int WALK_SEQUENCE_LENGTH = 4;

/**
 * @brief Maximum connected support step in pixels.
 *
 * | Edge                   | Height checked                   |
 * |------------------------|----------------------------------|
 * | Ground to elevation    | Absolute destination height      |
 * | Elevation to elevation | Destination minus current height |
 * | Elevation to ground    | Absolute source height           |
 *
 * A rejected height blocks the entire move.
 */
inline constexpr int MAX_STEP_HEIGHT = 8;
}  // namespace CharacterConstants
