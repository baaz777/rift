#pragma once

#include <cstdint>
#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>

/**
 * @enum LogLevel
 * @brief Severity order used by the minimum-level filter.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 */
enum class LogLevel : std::uint8_t
{
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

/**
 * @class Logger
 * @brief Timestamped console and file logging for a single-threaded engine.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 *
 *
 * Initialize after console redirection to enable ANSI color on the correct handles.
 * Initialize truncates rift.project.log; Shutdown closes it. Console output remains
 * available before initialization, after shutdown and when the file cannot open.
 * Serialize calls that read or change logger state; there is no internal synchronization.
 * Formatted helpers build the message before applying the level filter. Filtering skips
 * timestamp and output-line formatting, but still evaluates and formats message arguments.
 *
 * Trace through Info use stdout; Warn through Fatal use stderr. Both flush each line.
 * Signal and SEH handlers append directly to the same path and must not call Logger.
 * Fatal writes at the highest severity; it does not terminate the process.
 *
 * @verbatim
 *   [12:34:56.789] [ERROR] [Dialogue] Start node not found in NPC tree
 *   |------------| |-----| |--------| |---------------------------------
 *     timestamp     level   subsystem   message (verbatim, never padded)
 *       14 ch        7 ch     10 ch
 *                      |         '-- SUBSYSTEM_FIELD_WIDTH = 10, brackets included.
 *                      |             Names longer than MAX_SUBSYSTEM_NAME_LENGTH (8)
 *                      |             are truncated; shorter ones right-padded.
 *                      '-- LEVEL_FIELD_WIDTH = 7, the width of [ERROR] / [FATAL].
 *                          [INFO] and [WARN] get a trailing space to match.
 * @endverbatim
 *
 * @verbatim
 *   Trace   Debug   Info      Warn   Error   Fatal
 *   ----------------------    ---------------------
 *          std::cout                 std::cerr
 * @endverbatim
 *
 * @code{.cpp}
 * Logger::InfoF("Tilemap", "Loaded '{}' ({}x{})", path, width, height);
 * @endcode
 *
 * @code
 * namespace
 * {
 * constexpr const char* LOG_SUBSYSTEM = "Game";
 * }
 *
 * void DoStuff()
 * {
 *     Logger::Info(LOG_SUBSYSTEM, "Doing stuff");
 * }
 * @endcode
 */
class Logger
{
public:
    Logger() = delete;

    Logger(const Logger&) = delete;

    Logger& operator=(const Logger&) = delete;

    /// working-directory path shared with signal and SEH handlers; keep their literals consistent.
    static constexpr const char* LOG_FILE_PATH = "rift.project.log";

    /**
     * @fn bool Logger::Initialize()
     * @brief Open the log file and configure console color support.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Truncate the file on the first call after construction or Shutdown. A failed file open
     * still marks the logger initialized so console logging can continue. Call Shutdown
     * before retrying.
     *
     * @return Whether the file opened, or true if the logger was already initialized.
     */
    static bool Initialize();

    /**
     * @fn void Logger::Shutdown()
     * @brief Close the file; repeated calls are safe and console logging continues.
     * @author Alex (<https://github.com/lextpf>)
     */
    static void Shutdown();

    /**
     * @fn void Logger::SetMinLevel(LogLevel level)
     * @brief Filter lower levels before output; formatted helpers still build their message first.
     * @author Alex (<https://github.com/lextpf>)
     */
    static void SetMinLevel(LogLevel level);

    static LogLevel GetMinLevel();

    /**
     * @fn bool Logger::IsInitialized()
     * @brief True between Initialize and Shutdown, including when file opening failed.
     * @author Alex (<https://github.com/lextpf>)
     */
    static bool IsInitialized();

    static void Trace(std::string_view subsystem, std::string_view message);

    static void Debug(std::string_view subsystem, std::string_view message);

    static void Info(std::string_view subsystem, std::string_view message);

    static void Warn(std::string_view subsystem, std::string_view message);

    static void Error(std::string_view subsystem, std::string_view message);

    static void Fatal(std::string_view subsystem, std::string_view message);

    static void Log(LogLevel level, std::string_view subsystem, std::string_view message);

    template <typename... Args>
    static void TraceF(std::string_view subsystem, std::format_string<Args...> fmt, Args&&... args)
    {
        Trace(subsystem, std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void DebugF(std::string_view subsystem, std::format_string<Args...> fmt, Args&&... args)
    {
        Debug(subsystem, std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void InfoF(std::string_view subsystem, std::format_string<Args...> fmt, Args&&... args)
    {
        Info(subsystem, std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void WarnF(std::string_view subsystem, std::format_string<Args...> fmt, Args&&... args)
    {
        Warn(subsystem, std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void ErrorF(std::string_view subsystem, std::format_string<Args...> fmt, Args&&... args)
    {
        Error(subsystem, std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void FatalF(std::string_view subsystem, std::format_string<Args...> fmt, Args&&... args)
    {
        Fatal(subsystem, std::format(fmt, std::forward<Args>(args)...));
    }

    /// bracketed tag width in characters.
    static constexpr std::size_t LEVEL_FIELD_WIDTH = 7;

    /// bracketed subsystem width in characters.
    static constexpr std::size_t SUBSYSTEM_FIELD_WIDTH = 10;

    /// Maximum name length excluding brackets.
    static constexpr std::size_t MAX_SUBSYSTEM_NAME_LENGTH = SUBSYSTEM_FIELD_WIDTH - 2;

    /**
     * @fn std::string_view Logger::LevelTagPadded(LogLevel level)
     * @brief Pad the severity tag to LEVEL_FIELD_WIDTH characters.
     * @author Alex (<https://github.com/lextpf>)
     */
    static std::string_view LevelTagPadded(LogLevel level);

    /**
     * @fn std::string Logger::SubsystemTagPadded(std::string_view subsystem)
     * @brief Truncate names to MAX_SUBSYSTEM_NAME_LENGTH and pad to SUBSYSTEM_FIELD_WIDTH.
     * @author Alex (<https://github.com/lextpf>)
     */
    static std::string SubsystemTagPadded(std::string_view subsystem);

private:
    /**
     * @fn void Logger::Emit(LogLevel level, std::string_view subsystem, std::string_view message)
     * @brief Caller must apply the minimum-level filter first.
     * @author Alex (<https://github.com/lextpf>)
     */
    static void Emit(LogLevel level, std::string_view subsystem, std::string_view message);

    /**
     * @fn std::string Logger::FormatTimestamp()
     * @brief Local timestamp in HH:MM:SS.mmm format.
     * @author Alex (<https://github.com/lextpf>)
     */
    static std::string FormatTimestamp();

    /**
     * @fn const char* Logger::AnsiColorFor(LogLevel level)
     * @brief Return an ANSI escape without checking VT support; invalid levels return an empty
     * string.
     * @author Alex (<https://github.com/lextpf>)
     */
    static const char* AnsiColorFor(LogLevel level);

    /**
     * @fn bool Logger::TryEnableVirtualTerminal()
     * @brief True only if both stdout and stderr enable virtual-terminal processing.
     * @author Alex (<https://github.com/lextpf>)
     */
    static bool TryEnableVirtualTerminal();

    static LogLevel s_MinLevel;
    static bool s_Initialized;
    static bool s_VtEnabled;
    static std::ofstream s_File;
};
