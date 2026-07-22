#pragma once

#include <entt/entt.hpp>

#include <glm/glm.hpp>

#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class CameraController;
class ConsoleBuffer;
class ConsoleCommandRegistry;
class DialogueManager;
class Editor;
class Game;
class GameStateManager;
class IRenderer;
class ParticleSystem;
class TimeManager;
class Tilemap;
class WeatherDirector;

/**
 * @struct CommandContext
 * @brief Aggregates the state references a console command may operate on.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 *
 * Console::RegisterDefaultCommands builds this context for each invocation. Pointer members
 * are nullable; each command checks its required services. The output reference is always
 * valid. Resolve playerEntity through npcs because the handle can be null or stale.
 *
 * Command functions receive arguments without the command name. They return true on success,
 * or false after writing a diagnostic to out. Tests can supply only the services a command needs.
 *
 * @warning Do not retain the context after its handler returns. Backend switches replace
 * the renderer, and map loads replace tilemap contents. Rebuild references from Game for
 * the next invocation.
 */
struct CommandContext
{
    ConsoleBuffer& out;
    entt::entity playerEntity = entt::null;  ///< Player entity; resolve via `npcs` (the world).
    GameStateManager* gameState = nullptr;
    TimeManager* time = nullptr;
    Tilemap* tilemap = nullptr;
    /**
     * @brief ECS world registry: the player entity and every NPC. Required by player.*,
     * character.*,
     * appearance.*, move.* and npc.* alike. Resolve `playerEntity` through it.
     */
    entt::registry* npcs = nullptr;
    /**
     * @brief Command table used by help and by tab completion. Const because a handler may only
     * read the catalog, never register into it mid-dispatch.
     */
    const ConsoleCommandRegistry* registry = nullptr;
    Editor* editor = nullptr;
    CameraController* camera = nullptr;  ///< Camera (position, zoom, follow, free mode).
    /**
     * @brief Active renderer: sprite/texture uploads (character.*, npc.spawn) and backend identity
     * queries (renderer.info). Recreated by renderer.set, so never cache it.
     */
    IRenderer* renderer = nullptr;
    Game* game = nullptr;  ///< Game (cross-cutting ops like renderer.set).
    bool* postFXEnabled = nullptr;
    DialogueManager* dialogue = nullptr;  ///< Branching dialogue manager (current node, options).
    ParticleSystem* particles = nullptr;  ///< Particle system (single-shot spawn, list, kill).
    WeatherDirector* weatherDirector = nullptr;  ///< Weather transitions (time.weather routing).
};

/**
 * @fn bool Cmd_Help(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief help - List every registered command; extra arguments are ignored.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_Help(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_Clear(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief clear - Drop the scrollback (input line and history survive).
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_Clear(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_Teleport(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief teleport &lt;tx&gt; &lt;ty&gt; - Move the player to a tile coord.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_Teleport(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_FlagSet(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief flag.set &lt;name&gt; &lt;value&gt; - Set a game-state flag.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_FlagSet(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_FlagGet(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief flag.get &lt;name&gt; - Print one game-state flag.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_FlagGet(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_TimeSet(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief time.set &lt;hours&gt; - Set in-game time (0.0-24.0).
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_TimeSet(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_TimeAdd(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief time.add &lt;hours&gt; - Offset time of day (signed, wraps 0..24).
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_TimeAdd(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_TimeFreeze(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief time.freeze (on|off|toggle) - Pause/resume the day-night cycle.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_TimeFreeze(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_MapLoad(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief map.load &lt;filename&gt; - Switch maps.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_MapLoad(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_StateDump(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief state.dump - Print player tile, time, NPC count, and quests.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_StateDump(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_NoClip(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief noclip (on|off) - Toggle player tile/NPC collision.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_NoClip(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_Editor(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief editor (on|off|toggle) - Toggle level editor mode.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_Editor(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_AppearanceCopy(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief appearance.copy - Copy the appearance of the nearest NPC within 32px.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_AppearanceCopy(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_AppearanceRestore(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief appearance.restore - Restore the original character appearance.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_AppearanceRestore(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_CharacterSet(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief character.set &lt;type&gt; - Switch player character (a CharacterType name).
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_CharacterSet(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_CharacterNext(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief character.next - Cycle to the next player character.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_CharacterNext(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_RendererSet(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief renderer.set &lt;opengl|vulkan&gt; - Switch backend at runtime.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_RendererSet(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_DebugInfo(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief debug.info (on|off|toggle) - Toggle the FPS/coords HUD.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_DebugInfo(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_DebugOverlays(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief debug.overlays (on|off|toggle) - Collision/nav/anchor overlays.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_DebugOverlays(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_ParticlesToggle(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief particles (on|off|toggle) - All particle rendering (weather + zones).
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_ParticlesToggle(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_World3D(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief world3d (on|off|toggle) - Render gameplay through the 3D camera path.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_World3D(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_CamPreset(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief cam.preset &lt;classic|ds|free&gt; - Select the 3D camera preset.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_CamPreset(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_CamYaw(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief cam.yaw &lt;degrees&gt; - Orbit the 3D camera horizontally (implies Free).
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_CamYaw(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_CamPitch(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief cam.pitch &lt;degrees&gt; - 3D camera elevation above the horizon (implies Free).
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_CamPitch(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_TimeNext(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief time.next - Advance to the next time-of-day preset.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_TimeNext(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_PostFX(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief postfx (on|off|toggle) - Bloom/grading/vignette/grain master switch.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_PostFX(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_PlayerSpeed(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief player.speed (multiplier) - Scale movement speed (1.0 = normal).
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_PlayerSpeed(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_PlayerPos(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief player.pos - Print the player's tile, world pixel, and facing.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_PlayerPos(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_PlayerBicycle(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief player.bicycle (on|off|toggle) - Toggle bicycle mode.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_PlayerBicycle(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_PlayerRun(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief player.run (on|off|toggle) - Toggle running mode.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_PlayerRun(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_MoveAccel(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief move.accel (px/s^2) - Acceleration rate (momentum ramp-up).
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_MoveAccel(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_MoveDecel(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief move.decel (px/s^2) - Deceleration rate (momentum ramp-down).
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_MoveDecel(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_MoveLookahead(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief move.lookahead (px) - Camera look-ahead along the travel direction.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_MoveLookahead(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_MoveDump(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief move.dump - Print the current accel/decel/look-ahead values.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_MoveDump(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_NpcList(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief npc.list - List NPCs (idx, name, type, tile, AI state).
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_NpcList(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_NpcTp(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief npc.tp &lt;idx&gt; &lt;tx&gt; &lt;ty&gt; - Teleport an NPC by index.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_NpcTp(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_NpcSpawn(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief npc.spawn &lt;type&gt; &lt;tx&gt; &lt;ty&gt; - Spawn an NPC at a tile.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_NpcSpawn(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_NpcDespawn(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief npc.despawn &lt;idx&gt; - Remove an NPC (refused while it is in dialogue).
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_NpcDespawn(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_NpcFreeze(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief npc.freeze &lt;idx|all&gt; (on|off|toggle) - Halt NPC AI.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_NpcFreeze(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_NpcDialog(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief npc.dialog &lt;idx&gt; &lt;text...&gt; - Set an NPC's simple dialogue text.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_NpcDialog(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_DialogueActive(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief dialogue.active - Print the current dialogue node and visible options.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_DialogueActive(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_DialogueEnd(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief dialogue.end - Force-close any active dialogue (simple or tree).
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_DialogueEnd(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_DialogueSkip(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief dialogue.skip - Advance tree dialogue (confirm the current selection).
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_DialogueSkip(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_FlagList(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief flag.list - Dump every game-state flag.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_FlagList(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_FlagUnset(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief flag.unset &lt;name&gt; - Remove a flag.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_FlagUnset(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_TimeScale(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief time.scale &lt;multiplier&gt; - Time progression rate (1.0 = normal).
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_TimeScale(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_TimeWeather(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief time.weather &lt;name&gt; (seconds) - Set weather, blended (0 = instant).
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_TimeWeather(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_WeatherOverlay(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief weather.overlay &lt;name|off&gt; - Set/clear the manual sky overlay.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_WeatherOverlay(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_WeatherIntensity(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief weather.intensity &lt;0.0-1.0&gt; - Density / effect strength.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_WeatherIntensity(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_WeatherNext(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief weather.next (seconds) - Cycle to the next weather (0 = instant).
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_WeatherNext(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_WeatherRandom(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief weather.random (seconds) - Pick a random weather (0 = instant).
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_WeatherRandom(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_WeatherForecast(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief weather.forecast (days) - Upcoming fronts/night events (default 3, cap 7).
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_WeatherForecast(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_WeatherAuto(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief weather.auto (on|off) - Forecast autonomy; no arg prints the state.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_WeatherAuto(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_WeatherStatus(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief weather.status - Current weather, transition, auto/hold flags, wind.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_WeatherStatus(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_WeatherWind(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief weather.wind - Gusted wind direction/strength readout.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_WeatherWind(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_LightAdd(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief light.add &lt;x&gt; &lt;y&gt; (r g b) (radius) (schedule) - Place a light.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_LightAdd(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_LightClear(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief light.clear - Remove every WorldLight.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_LightClear(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_LightList(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief light.list - Lights with index, position, color, radius, schedule.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_LightList(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_LightRemove(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief light.remove &lt;index&gt; - Remove one WorldLight by index.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_LightRemove(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_TimeStatus(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief time.status - Print time, period, weather, day count, moon phase.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_TimeStatus(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_ParticleSpawn(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief particle.spawn &lt;type&gt; &lt;wx&gt; &lt;wy&gt; - Spawn at world pixels.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_ParticleSpawn(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_ParticleList(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief particle.list - Count active particles by type and list zones.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_ParticleList(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_ParticleKillAll(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief particle.kill_all - Remove every active particle.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_ParticleKillAll(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_CameraFreecam(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief camera.freecam (on|off|toggle) - Decouple the camera from the player.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_CameraFreecam(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_CameraZoom(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief camera.zoom &lt;factor&gt; - Set camera zoom (0.1-10.0).
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_CameraZoom(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_CameraFollow(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief camera.follow (on|off|toggle) - Re-attach the camera to the player.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_CameraFollow(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_CameraInfo(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief camera.info - Dump pos, zoom, freecam, follow, and tilt.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_CameraInfo(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_MapSave(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief map.save (path) - Save the current map to JSON.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_MapSave(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_MapSize(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief map.size - Print map dimensions in tiles and pixels.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_MapSize(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_MapCollision(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief map.collision &lt;tx&gt; &lt;ty&gt; - Query the collision flag at a tile.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_MapCollision(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_Perf(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief perf - Print FPS, frame time, and draw-call count.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_Perf(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_RendererTrace(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief renderer.trace (on|off|dump|clear) - Per-frame draw-call trace.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_RendererTrace(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_ConsoleCopy(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief console.copy - Copy the whole scrollback to the OS clipboard.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_ConsoleCopy(std::span<const std::string_view> args, CommandContext& ctx);

/**
 * @fn bool Cmd_LayersList(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief layers.list - Every tilemap layer (name, order, fill, animations).
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_LayersList(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_TileInfo(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief tile.info &lt;tx&gt; &lt;ty&gt; - Inspect one tile across all layers.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_TileInfo(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_TileFind(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief tile.find &lt;tileID&gt; (layer) - Find tiles by ID.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_TileFind(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_MapStats(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief map.stats - Cells, collision %, nav %, layer/struct/light counts.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_MapStats(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_TilesetInfo(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief tileset.info - Tileset texture and tile dimensions.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_TilesetInfo(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_AnimList(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief anim.list - Animated tile definitions and their use counts.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_AnimList(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_StructList(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief struct.list - List all no-projection structures.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_StructList(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_StructInfo(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief struct.info &lt;id&gt; - Detail for one structure.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_StructInfo(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_StructGoto(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief struct.goto &lt;id&gt; - Snap the camera to a structure.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_StructGoto(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_ZoneList(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief zone.list - List particle zones.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_ZoneList(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_ZoneGoto(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief zone.goto &lt;idx&gt; - Snap the camera to a zone center.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_ZoneGoto(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_LightGoto(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief light.goto &lt;idx&gt; - Snap the camera to a world light.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_LightGoto(std::span<const std::string_view> args, CommandContext& ctx);

/**
 * @fn bool Cmd_NavPath(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief nav.path &lt;fx&gt; &lt;fy&gt; &lt;tx&gt; &lt;ty&gt; - BFS path on the nav grid.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_NavPath(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_NavReachable(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief nav.reachable &lt;tx&gt; &lt;ty&gt; - Reachable tile count plus bounds.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_NavReachable(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_NpcPath(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief npc.path &lt;idx&gt; - Print one NPC's patrol waypoints.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_NpcPath(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_NpcGoto(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief npc.goto &lt;idx&gt; - Snap the camera to an NPC.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_NpcGoto(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_NpcNearest(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief npc.nearest - The NPC nearest the player by tile distance.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_NpcNearest(std::span<const std::string_view> args, CommandContext& ctx);

/**
 * @fn bool Cmd_QuestList(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief quest.list - List active and completed quests.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_QuestList(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_QuestGive(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief quest.give &lt;name&gt; (description...) - Accept a quest.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_QuestGive(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_QuestComplete(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief quest.complete &lt;name&gt; - Mark a quest complete.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_QuestComplete(std::span<const std::string_view> args, CommandContext& ctx);

/**
 * @fn bool Cmd_Version(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief version - Engine version, build config, and build date.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_Version(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_RendererInfo(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief renderer.info - Active backend plus GPU vendor/device/driver.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_RendererInfo(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_MemStats(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief mem.stats - Approximate memory usage.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_MemStats(std::span<const std::string_view> args, CommandContext& ctx);
/**
 * @fn bool Cmd_ConfigDump(std::span<const std::string_view> args, CommandContext& ctx)
 * @brief config.dump - Emit the current toggles as a replayable command list.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_ConfigDump(std::span<const std::string_view> args, CommandContext& ctx);

/**
 * @brief Session-scoped player-position marks.
 *
 * The bookmark map is owned by Console, not carried in CommandContext, so it is
 * passed as an extra parameter to keep these free functions trivially testable
 * without a Console. It is never persisted to disk.
 */
/**
 * @fn bool Cmd_BookmarkSet(std::span<const std::string_view> args, CommandContext& ctx, \
 *     std::unordered_map<std::string, glm::ivec2>& bookmarks)
 * @brief bookmark.set &lt;name&gt; - Save the player's current tile.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_BookmarkSet(std::span<const std::string_view> args,
                     CommandContext& ctx,
                     std::unordered_map<std::string, glm::ivec2>& bookmarks);
/**
 * @fn bool Cmd_BookmarkTp(std::span<const std::string_view> args, CommandContext& ctx, \
 *     std::unordered_map<std::string, glm::ivec2>& bookmarks)
 * @brief bookmark.tp &lt;name&gt; - Teleport to a saved bookmark.
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_BookmarkTp(std::span<const std::string_view> args,
                    CommandContext& ctx,
                    std::unordered_map<std::string, glm::ivec2>& bookmarks);
/**
 * @fn bool Cmd_BookmarkList(std::span<const std::string_view> args, CommandContext& ctx, const \
 *     std::unordered_map<std::string, glm::ivec2>& bookmarks)
 * @brief bookmark.list - List saved bookmarks (alphabetical).
 * @author Alex (<https://github.com/lextpf>)
 */
bool Cmd_BookmarkList(std::span<const std::string_view> args,
                      CommandContext& ctx,
                      const std::unordered_map<std::string, glm::ivec2>& bookmarks);
