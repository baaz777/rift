#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

/**
 * @enum ManifestDiagnosticSeverity
 * @brief Severity for project manifest parser and validator diagnostics.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 */
enum class ManifestDiagnosticSeverity
{
    Warning,  ///< Non-fatal issue; startup may continue with fallback behavior.
    Error,    ///< Fatal manifest issue; startup should stop.
};

/**
 * @struct ManifestDiagnostic
 * @brief One parser or validation diagnostic.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 */
struct ManifestDiagnostic
{
    ManifestDiagnosticSeverity severity = ManifestDiagnosticSeverity::Warning;
    std::string fieldPath;  ///< JSON field path or asset category that triggered the diagnostic.
    std::string message;
};

/**
 * @struct ManifestValidationResult
 * @brief Ordered parse and validation diagnostics; loading appends without clearing.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 */
struct ManifestValidationResult
{
    std::vector<ManifestDiagnostic> diagnostics;

    void Add(ManifestDiagnosticSeverity severity, std::string fieldPath, std::string message);

    void AddWarning(std::string fieldPath, std::string message);

    void AddError(std::string fieldPath, std::string message);

    [[nodiscard]] bool HasErrors() const;

    [[nodiscard]] bool HasWarnings() const;
};

/**
 * @struct PlayerCharacterManifest
 * @brief Sprite paths for one player character.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 *
 * Walking and Running are required. Bicycle is optional. Other role keys are retained.
 */
struct PlayerCharacterManifest
{
    std::map<std::string, std::string> sprites;  ///< Sprite role name to relative asset path.

    PlayerCharacterManifest() = default;
    PlayerCharacterManifest(const PlayerCharacterManifest&) = default;
    PlayerCharacterManifest(PlayerCharacterManifest&&) noexcept = default;
    PlayerCharacterManifest& operator=(const PlayerCharacterManifest&) = default;
    PlayerCharacterManifest& operator=(PlayerCharacterManifest&&) noexcept = default;
};

/**
 * @struct ProjectManifest
 * @brief Startup assets and defaults loaded from JSON.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 *
 * Relative paths use the manifest directory, or the working directory for built-in defaults.
 * Missing keys retain field defaults. Supplied lists and objects replace defaults even when empty.
 * defaultMapSize supplies defaultMapWidth/defaultMapHeight; particles supplies particleSprites.
 *
 * @code{.json}
 * {
 *   "formatVersion": 1,
 *   "startupRenderer": "OpenGL",
 *   "defaultMap": "rift.save.json",
 *   "tileWidth": 16,
 *   "tileHeight": 16,
 *   "defaultMapSize": { "width": 125, "height": 125 },
 *   "tilesets": ["assets/tiles/outdoor.png"],
 *   "npcSprites": ["assets/non-player/guard.png"],
 *   "fonts": ["assets/fonts/pixel.ttf"],
 *   "particles": { "smoke2": "assets/particles/smoke2.png" },
 *   "playerCharacters": {
 *     "BW1_MALE": { "Walking": "...", "Running": "...", "Bicycle": "..." }
 *   }
 * }
 * @endcode
 */
struct ProjectManifest
{
    static constexpr int CURRENT_FORMAT_VERSION = 1;
    static constexpr const char* DEFAULT_FILENAME = "rift.project.json";

    int formatVersion = CURRENT_FORMAT_VERSION;
    bool loadedFromFile = false;
    std::filesystem::path sourcePath;
    std::filesystem::path baseDirectory;

    /// OpenGL or Vulkan, matched case-insensitively; preserve authored casing.
    std::string startupRenderer = "OpenGL";
    std::string defaultMap = "rift.save.json";  ///< Save/map path used at startup and editor save.
    int tileWidth = 16;                         ///< Tile width in pixels; must be positive.
    int tileHeight = 16;                        ///< Tile height in pixels; must be positive.

    int defaultMapWidth = 125;

    int defaultMapHeight = 125;

    std::vector<std::string> tilesets;
    std::vector<std::string> npcSprites;
    std::vector<std::string> fonts;
    std::map<std::string, PlayerCharacterManifest> playerCharacters;

    /**
     * @brief Particle name to asset path; derive the _strip sibling. Missing entries use procedural
     * sprites.
     */
    std::map<std::string, std::string> particleSprites;

    ProjectManifest() = default;
    ProjectManifest(const ProjectManifest&) = default;
    ProjectManifest(ProjectManifest&&) noexcept = default;
    ProjectManifest& operator=(const ProjectManifest&) = default;
    ProjectManifest& operator=(ProjectManifest&&) noexcept = default;

    /**
     * @fn std::filesystem::path ProjectManifest::ResolvePath(const std::string& path) const
     * @brief Resolve an asset path against the configured base directory.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Relative paths are joined to baseDirectory, or the current working directory when
     * baseDirectory is empty. Absolute paths keep their root. Normalization is lexical;
     * symbolic links and asset existence are not checked.
     *
     * @return The normalized path. Empty input resolves to the base directory.
     */
    [[nodiscard]] std::filesystem::path ResolvePath(const std::string& path) const;

    [[nodiscard]] std::string ResolvePathString(const std::string& path) const;

    [[nodiscard]] std::vector<std::string> ResolvePathStrings(
        const std::vector<std::string>& paths) const;

    /**
     * @fn ManifestValidationResult ProjectManifest::Validate() const
     * @brief Validate schema and asset existence.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Require a supported version and renderer, positive dimensions, at least one tileset
     * and player character, and every required tileset/Walking/Running file.
     * Missing map, NPC, font, particle and bicycle files warn; startup can use fallbacks.
     * Filesystem queries can throw; those failures are not converted to diagnostics.
     */
    [[nodiscard]] ManifestValidationResult Validate() const;

    /**
     * @fn ProjectManifest ProjectManifest::BuiltInFallback()
     * @brief Use the working directory as baseDirectory; callers must validate these defaults.
     * @author Alex (<https://github.com/lextpf>)
     */
    static ProjectManifest BuiltInFallback();

    /**
     * @fn std::optional<ProjectManifest> ProjectManifest::LoadFromFile(const \
     *     std::filesystem::path& path, ManifestValidationResult& result)
     * @brief Read a manifest and append its parse and validation diagnostics.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Parsing starts from field defaults. Present lists and objects replace those defaults.
     * A parsed manifest can still contain validation errors; inspect result.HasErrors()
     * before using it. The supplied result is never cleared.
     *
     * @return The parsed manifest, or nullopt on open failure, invalid JSON, or a non-object root.
     */
    static std::optional<ProjectManifest> LoadFromFile(const std::filesystem::path& path,
                                                       ManifestValidationResult& result);

    /**
     * @fn ProjectManifest ProjectManifest::LoadDefaultOrFallback(ManifestValidationResult& result)
     * @brief Use the first existing manifest in the working directory, then its parent.
     * @author Alex (<https://github.com/lextpf>)
     *
     * The first existing file decides the result, even if it cannot be read or parsed.
     * After that failure, return unvalidated defaults with the error; do not try the parent.
     * If neither candidate exists, validate the built-in defaults and append a warning.
     * Inspect result.HasErrors() in every case before starting the game.
     */
    static ProjectManifest LoadDefaultOrFallback(ManifestValidationResult& result);
};
