#pragma once

/**
 * @struct Speed
 * @brief Movement speed before mode and console multipliers.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 *
 * EntityStore replaces the default with CharacterConstants spawn speeds.
 * Player animation cadence uses 50 px/s as its 1.0x reference.
 */
struct Speed
{
    float value{100.0f};  ///< base movement speed in px/s; overwritten at spawn.
};
