#pragma once

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class Game;
class IRenderer;
class Console;

/**
 * @class ConsoleBuffer
 * @brief Console scrollback, ASCII input and command history.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 */
class ConsoleBuffer
{
public:
    /**
     * @brief Retained scrollback limit; adding lines evicts the oldest.
     *
     * A draw trace can exceed this limit.
     */
    static constexpr std::size_t MAX_LINES = 8192;

    /// Maximum number of submitted commands retained for up/down recall.
    static constexpr std::size_t MAX_HISTORY = 64;

    /// One scrollback line: text + display color.
    struct Line
    {
        std::string text;
        glm::vec3 color{1.0f, 1.0f, 1.0f};
    };

    /**
     * @fn void ConsoleBuffer::Print(std::string text, glm::vec3 color = glm::vec3(1.0f))
     * @brief Appends output and resets scrolling to the newest line.
     * @author Alex (<https://github.com/lextpf>)
     */
    void Print(std::string text, glm::vec3 color = glm::vec3(1.0f));

    /**
     * @fn void ConsoleBuffer::PrintError(std::string text)
     * @brief Append a red error line.
     * @author Alex (<https://github.com/lextpf>)
     */
    void PrintError(std::string text);

    /**
     * @fn void ConsoleBuffer::Clear()
     * @brief Drop all scrollback (does not clear input or history).
     * @author Alex (<https://github.com/lextpf>)
     */
    void Clear();

    /**
     * @fn void ConsoleBuffer::OnChar(std::uint32_t codepoint)
     * @brief Inserts U+0020 through U+007E and resets history navigation.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Other codepoints are ignored.
     */
    void OnChar(std::uint32_t codepoint);

    /**
     * @fn void ConsoleBuffer::OnBackspace()
     * @brief Erase the character before the cursor (no-op if at start).
     * @author Alex (<https://github.com/lextpf>)
     */
    void OnBackspace();

    /**
     * @fn void ConsoleBuffer::OnBackspaceWord()
     * @brief Deletes the preceding space/dot boundaries, then one word.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Ctrl+backspace repeats this sequence:
     * `time.weather clear` -> `time.weather ` -> `time.` -> `""`.
     */
    void OnBackspaceWord();

    /**
     * @fn void ConsoleBuffer::OnDelete()
     * @brief Erase the character at the cursor (no-op if at end).
     * @author Alex (<https://github.com/lextpf>)
     */
    void OnDelete();

    /**
     * @fn void ConsoleBuffer::OnLeft()
     * @brief Move cursor one position left (clamped at 0).
     * @author Alex (<https://github.com/lextpf>)
     */
    void OnLeft();

    /**
     * @fn void ConsoleBuffer::OnRight()
     * @brief Move cursor one position right (clamped at length).
     * @author Alex (<https://github.com/lextpf>)
     */
    void OnRight();

    /**
     * @fn void ConsoleBuffer::OnHome()
     * @brief Move cursor to start of input.
     * @author Alex (<https://github.com/lextpf>)
     */
    void OnHome();

    /**
     * @fn void ConsoleBuffer::OnEnd()
     * @brief Move cursor to end of input.
     * @author Alex (<https://github.com/lextpf>)
     */
    void OnEnd();

    /**
     * @fn std::string ConsoleBuffer::OnEnter()
     * @brief Returns and clears input; resets history navigation.
     * @author Alex (<https://github.com/lextpf>)
     */
    std::string OnEnter();

    /**
     * @fn void ConsoleBuffer::SetInputLine(std::string text)
     * @brief Replaces input and places the cursor at the end.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SetInputLine(std::string text);

    /**
     * @fn void ConsoleBuffer::RecordHistory(std::string command)
     * @brief Records history, skipping empty input and the immediately previous duplicate.
     * @author Alex (<https://github.com/lextpf>)
     */
    void RecordHistory(std::string command);

    /**
     * @fn std::optional<std::string> ConsoleBuffer::HistoryPrev()
     * @brief Returns the preceding history entry, or nullopt at the oldest entry.
     * @author Alex (<https://github.com/lextpf>)
     */
    std::optional<std::string> HistoryPrev();

    /**
     * @fn std::optional<std::string> ConsoleBuffer::HistoryNext()
     * @brief Returns the next history entry, or an empty string when leaving history.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Returns nullopt when history navigation is inactive.
     */
    std::optional<std::string> HistoryNext();

    /**
     * @fn void ConsoleBuffer::ResetHistoryIndex()
     * @brief Clears history navigation after input edits or submission.
     * @author Alex (<https://github.com/lextpf>)
     */
    void ResetHistoryIndex();

    /**
     * @fn void ConsoleBuffer::Scroll(int deltaLines)
     * @brief Moves the scroll offset by signed lines; positive means older.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Clamps to [0, Lines().size()], so scrolling can leave only the oldest line visible.
     */
    void Scroll(int deltaLines);

    /**
     * @fn void ConsoleBuffer::ResetScroll()
     * @brief Pin scroll to the bottom of the buffer.
     * @author Alex (<https://github.com/lextpf>)
     */
    void ResetScroll();

    /**
     * @fn void ConsoleBuffer::ScrollTo(int offsetFromBottom)
     * @brief Sets lines above the bottom, clamped to the line count.
     * @author Alex (<https://github.com/lextpf>)
     */
    void ScrollTo(int offsetFromBottom);

    [[nodiscard]] const std::deque<Line>& Lines() const { return m_Lines; }

    [[nodiscard]] const std::string& Input() const { return m_Input; }
    /**
     * @fn std::size_t ConsoleBuffer::CursorPos() const
     * @brief Cursor byte index from zero through Input.size(), inclusive.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] std::size_t CursorPos() const { return m_CursorPos; }
    /**
     * @fn int ConsoleBuffer::ScrollOffset() const
     * @brief Lines scrolled up from the newest line; 0 means pinned to the bottom.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] int ScrollOffset() const { return m_ScrollOffset; }
    /**
     * @fn const std::vector<std::string>& ConsoleBuffer::History() const
     * @brief Recorded command history, oldest first.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] const std::vector<std::string>& History() const { return m_History; }
    /**
     * @fn std::optional<std::size_t> ConsoleBuffer::HistoryIndex() const
     * @brief Index into History being walked, or nullopt when not navigating history.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] std::optional<std::size_t> HistoryIndex() const { return m_HistoryIdx; }

private:
    std::deque<Line> m_Lines;
    std::string m_Input;
    std::size_t m_CursorPos = 0;
    std::vector<std::string> m_History;
    std::optional<std::size_t> m_HistoryIdx;
    int m_ScrollOffset = 0;
};

/**
 * @class ConsoleCommandRegistry
 * @brief Named handlers in alphabetical order for help and completion.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 */
class ConsoleCommandRegistry
{
public:
    using Handler = std::function<void(std::span<const std::string_view> args, Console& console)>;

    /**
     * @brief Completion callback for a zero-based argument index after the verb.
     *
     * Return an empty vector for unsupported indices. Render calls this each frame while the
     * dropdown is open; keep it cheap. Captured state must outlive Console.
     */
    using ArgCompletionProvider = std::function<std::vector<std::string>(std::size_t argIndex)>;

    /// One registered command: canonical name, help text, handler, and optional extras.
    struct Command
    {
        std::string name;
        /// Printed verbatim after the command name; use argument grammar followed by a summary.
        std::string description;
        Handler handler;
        /// Alternate names included in lookup and completion.
        std::vector<std::string> aliases;
        /// Argument completion provider; may be null.
        ArgCompletionProvider argCompletions;
    };

    /**
     * @fn void ConsoleCommandRegistry::Register(std::string name, std::string description, \
     *     Handler handler, std::vector<std::string> aliases = {}, ArgCompletionProvider \
     *     argCompletions = nullptr)
     * @brief Registers or replaces a command; rejects empty names.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Aliases belong to the canonical entry. argCompletions may be null.
     */
    void Register(std::string name,
                  std::string description,
                  Handler handler,
                  std::vector<std::string> aliases = {},
                  ArgCompletionProvider argCompletions = nullptr);

    /**
     * @fn void ConsoleCommandRegistry::SetArgCompletions(std::string_view name, \
     *     ArgCompletionProvider argCompletions)
     * @brief Sets argument completion by canonical name; unknown names are ignored.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SetArgCompletions(std::string_view name, ArgCompletionProvider argCompletions);

    /**
     * @fn const Command* ConsoleCommandRegistry::Lookup(std::string_view name) const
     * @brief Exact-case lookup by canonical name, then alias; returns nullptr if absent.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Canonical lookup is O(log n); aliases use a linear scan. If aliases collide,
     * the command with the alphabetically first canonical name wins.
     *
     * @return A borrowed command, or nullptr if absent. Registering the same canonical
     * name replaces its contents.
     */
    [[nodiscard]] const Command* Lookup(std::string_view name) const;

    /**
     * @fn std::vector<std::string> ConsoleCommandRegistry::MatchPrefix( std::string_view prefix, \
     *     std::size_t maxCount = (std::numeric_limits<std::size_t>::max)()) const
     * @brief ASCII case-insensitive prefix matches in bytewise alphabetical order.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Includes aliases. An empty prefix matches all names; maxCount keeps the earliest matches.
     * Lookup still requires exact case.
     */
    [[nodiscard]] std::vector<std::string> MatchPrefix(
        std::string_view prefix,
        std::size_t maxCount = (std::numeric_limits<std::size_t>::max)()) const;

    /**
     * @brief Matched name and its canonical command.
     *
     * The canonical field is empty for a canonical name; an alias stores its target name.
     */
    struct MatchEntry
    {
        std::string name;
        std::string canonical;
    };

    /**
     * @fn std::vector<MatchEntry> ConsoleCommandRegistry::MatchPrefixDetailed( std::string_view \
     *     prefix, std::size_t maxCount = (std::numeric_limits<std::size_t>::max)()) const
     * @brief Prefix matches with canonical targets for alias display.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] std::vector<MatchEntry> MatchPrefixDetailed(
        std::string_view prefix,
        std::size_t maxCount = (std::numeric_limits<std::size_t>::max)()) const;

    [[nodiscard]] const std::map<std::string, Command>& All() const { return m_Commands; }

private:
    std::map<std::string, Command> m_Commands;
};

/**
 * @class Console
 * @brief Developer console overlay and command dispatch.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 *
 * RegisterDefaultCommands builds a fresh CommandContext for each invocation.
 * F12 opens Half or closes either open state. Tab on empty input swaps Half and Full.
 * Up/down prefer suggestions over history; Enter submits without accepting a suggestion.
 *
 * ```mermaid
 * stateDiagram-v2
 *     classDef closed fill:#1e3a5f,stroke:#3b82f6,color:#e2e8f0
 *     classDef half   fill:#4a3520,stroke:#f59e0b,color:#e2e8f0
 *     classDef full   fill:#134e3a,stroke:#10b981,color:#e2e8f0
 *
 *     state "Closed" as C:::closed
 *     state "Half (top 50%)" as H:::half
 *     state "Full (entire frame)" as F:::full
 *
 *     [*] --> C
 *     C --> H: F12 (Toggle) / Open
 *     H --> C: F12 (Toggle) / Close / Esc
 *     F --> C: F12 (Toggle) / Close / Esc
 *     H --> F: Tab (ToggleFullscreen)
 *     F --> H: Tab (ToggleFullscreen) / Open
 * ```
 *
 * ```mermaid
 * flowchart TD
 *     TAB["Tab"] --> TG{"input line empty?<br/>(Game::PumpConsoleKeys)"}
 *     TG -->|yes| TFS["ToggleFullscreen (Half &lt;-&gt; Full)"]
 *     TG -->|no| TAB2["OnTab: splice highlighted item"]
 *     UD["Up / Down"] --> UG{"suggestions non-empty?"}
 *     UG -->|yes| SEL["move dropdown selection"]
 *     UG -->|no| HIST["HistoryPrev / HistoryNext"]
 *     ENT["Enter"] --> SUB["RecordHistory then Submit<br/>(suggestion ignored)"]
 *     ESC["Esc"] --> CL["Close (dropdown state kept)"]
 * ```
 */
class Console
{
public:
    /// Half covers the top 50%; Full covers the framebuffer.
    enum class State : std::uint8_t
    {
        Closed,
        Half,
        Full
    };

    explicit Console(Game& game);

    /**
     * @fn bool Console::IsOpen() const
     * @brief True when the overlay is in Half or Full state.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] bool IsOpen() const { return m_State != State::Closed; }
    /**
     * @fn bool Console::IsFullscreen() const
     * @brief True when the overlay covers the full framebuffer.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] bool IsFullscreen() const { return m_State == State::Full; }

    [[nodiscard]] State GetState() const { return m_State; }
    /**
     * @fn void Console::Toggle()
     * @brief Opens Half when closed; otherwise closes the overlay.
     * @author Alex (<https://github.com/lextpf>)
     */
    void Toggle();
    /**
     * @fn void Console::ToggleFullscreen()
     * @brief Swaps Half and Full; does nothing while closed.
     * @author Alex (<https://github.com/lextpf>)
     */
    void ToggleFullscreen();
    /**
     * @fn void Console::Open()
     * @brief Sets Half from any state and scrolls to the newest line.
     * @author Alex (<https://github.com/lextpf>)
     */
    void Open();
    /**
     * @fn void Console::Close()
     * @brief Close the overlay and stop consuming console input.
     * @author Alex (<https://github.com/lextpf>)
     */
    void Close();

    /**
     * @fn void Console::OnChar(std::uint32_t codepoint)
     * @brief Forwards typed characters only while open.
     * @author Alex (<https://github.com/lextpf>)
     */
    void OnChar(std::uint32_t codepoint);

    /**
     * @fn void Console::OnEnter()
     * @brief Records and submits input, then resets suggestion selection.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Only Tab or a dropdown click inserts a suggestion.
     */
    void OnEnter();
    /**
     * @fn void Console::OnBackspace()
     * @brief Delete one code unit before the cursor.
     * @author Alex (<https://github.com/lextpf>)
     */
    void OnBackspace();
    /**
     * @fn void Console::OnBackspaceWord()
     * @brief Deletes the preceding space/dot boundaries, then one word.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Ctrl+backspace repeats this sequence:
     * `time.weather clear` -> `time.weather ` -> `time.` -> `""`.
     */
    void OnBackspaceWord();
    /**
     * @fn void Console::OnDelete()
     * @brief Delete one code unit at the cursor.
     * @author Alex (<https://github.com/lextpf>)
     */
    void OnDelete();
    /**
     * @fn void Console::OnTab()
     * @brief Inserts the selected suggestion and resets selection to the first row.
     * @author Alex (<https://github.com/lextpf>)
     */
    void OnTab();
    /**
     * @fn void Console::OnUp()
     * @brief Selects the previous suggestion; recalls history only when no suggestions exist.
     * @author Alex (<https://github.com/lextpf>)
     */
    void OnUp();
    /**
     * @fn void Console::OnDown()
     * @brief Selects the next suggestion; recalls history only when no suggestions exist.
     * @author Alex (<https://github.com/lextpf>)
     */
    void OnDown();
    /**
     * @fn void Console::OnLeft()
     * @brief Move the cursor left.
     * @author Alex (<https://github.com/lextpf>)
     */
    void OnLeft();
    /**
     * @fn void Console::OnRight()
     * @brief Move the cursor right.
     * @author Alex (<https://github.com/lextpf>)
     */
    void OnRight();
    /**
     * @fn void Console::OnHome()
     * @brief Move the cursor to the start of the line.
     * @author Alex (<https://github.com/lextpf>)
     */
    void OnHome();
    /**
     * @fn void Console::OnEnd()
     * @brief Move the cursor to the end of the line.
     * @author Alex (<https://github.com/lextpf>)
     */
    void OnEnd();
    /**
     * @fn void Console::OnEscape()
     * @brief Close the console. Does not dismiss the dropdown or reset its selection.
     * @author Alex (<https://github.com/lextpf>)
     */
    void OnEscape();
    /**
     * @fn void Console::OnScroll(double yoffset)
     * @brief Scroll console history by the wheel delta.
     * @author Alex (<https://github.com/lextpf>)
     */
    void OnScroll(double yoffset);

    /**
     * @fn void Console::Submit(std::string_view line)
     * @brief Echoes input, tokenizes it and dispatches the command.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Empty input still adds a scrollback line. Unknown verbs print an error.
     * Dispatch is synchronous; handlers must not retain argument views into line.
     * Submit does not record command history; OnEnter records it before calling Submit.
     */
    void Submit(std::string_view line);

    /**
     * @fn void Console::Render(IRenderer& renderer, int screenWidth, int screenHeight)
     * @brief Draws the overlay and caches rows and dropdown geometry.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Installs a top-left orthographic projection from framebuffer dimensions. Mouse handlers
     * and ScrollToOutputTop use the previous rendered layout and are inactive before the first
     * draw.
     */
    void Render(IRenderer& renderer, int screenWidth, int screenHeight);

    /**
     * @fn std::vector<std::string_view> Console::Tokenize(std::string_view line)
     * @brief Splits on runs of spaces and tabs into borrowed views.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Other whitespace stays inside tokens. Quotes and escapes have no special meaning.
     * The input storage must remain valid while any returned view is used.
     */
    [[nodiscard]] static std::vector<std::string_view> Tokenize(std::string_view line);

    /**
     * @fn ConsoleBuffer& Console::Buffer()
     * @brief Mutable output/input buffer used by command handlers.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] ConsoleBuffer& Buffer() { return m_Buffer; }

    [[nodiscard]] const ConsoleBuffer& Buffer() const { return m_Buffer; }
    /**
     * @fn Game& Console::GetGame()
     * @brief Game instance that owns this console.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] Game& GetGame() { return m_Game; }
    /**
     * @fn const ConsoleCommandRegistry& Console::Registry() const
     * @brief Registered command table.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] const ConsoleCommandRegistry& Registry() const { return m_Registry; }

    /**
     * @fn void Console::ScrollToOutputTop(std::size_t outputLineCount)
     * @brief Scrolls to the first line of the latest output block.
     * @author Alex (<https://github.com/lextpf>)
     *
     * outputLineCount is the block length in lines. Before the first Render, scrolls to bottom.
     */
    void ScrollToOutputTop(std::size_t outputLineCount);

    /**
     * @fn std::unordered_map<std::string, glm::ivec2>& Console::Bookmarks()
     * @brief Session bookmarks; discarded on Console destruction.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] std::unordered_map<std::string, glm::ivec2>& Bookmarks() { return m_Bookmarks; }
    [[nodiscard]] const std::unordered_map<std::string, glm::ivec2>& Bookmarks() const
    {
        return m_Bookmarks;
    }

    /**
     * @brief Suggestions and the input span they replace.
     *
     * Suggestions are alphabetical and capped. wordStart is a byte index; insertion uses
     * `input.substr(0, wordStart) + items[i]`. The canonicals vector parallels items: aliases
     * contain the target command; canonical names and argument values have empty entries.
     */
    struct SuggestionResult
    {
        std::vector<std::string> items;
        std::vector<std::string> canonicals;
        std::size_t wordStart = 0;
    };

    /**
     * @fn void Console::OnMouseHover(double mouseX, double mouseY)
     * @brief Mouse cursor moved over the suggestion dropdown.
     * @author Alex (<https://github.com/lextpf>)
     *
     * If the cursor is inside the box, snap `m_SuggestionIndex` to the row
     * under the cursor so hover-to-highlight matches what a click would commit.
     *
     * @param mouseX Cursor x in the pixel space passed to `Render`.
     * @param mouseY Cursor y in the same space: origin top-left, y downward.
     */
    void OnMouseHover(double mouseX, double mouseY);

    /**
     * @fn bool Console::OnMouseClick(double mouseX, double mouseY)
     * @brief Left-click at mouseX,mouseY.
     * @author Alex (<https://github.com/lextpf>)
     *
     * If the click landed inside the dropdown box, splice the clicked
     * suggestion into the input (same path as Tab) and return true so the
     * caller can swallow the click.
     *
     * @param mouseX Cursor x in the pixel space passed to `Render`.
     * @param mouseY Cursor y in the same space: origin top-left, y downward.
     * @return       True when the click was consumed by the dropdown.
     */
    bool OnMouseClick(double mouseX, double mouseY);

    /**
     * @fn bool Console::TryScrollDropdown(double mouseX, double mouseY, double yoffset)
     * @brief Mouse wheel hit-routing.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Returns true whenever the cursor is inside the last-drawn dropdown,
     * consuming the wheel event; the rows only move when the list overflows the
     * visible window, at 2 rows per notch (the scrollback moves 3, see
     * `OnScroll)`. Returns false otherwise, so the caller scrolls the
     * scrollback instead.
     *
     * @param mouseX  Cursor x in the pixel space passed to `Render`.
     * @param mouseY  Cursor y in the same space: origin top-left, y downward.
     * @param yoffset Wheel delta in notches; positive reveals earlier rows.
     * @return        True when the wheel event was consumed by the dropdown.
     */
    bool TryScrollDropdown(double mouseX, double mouseY, double yoffset);

private:
    /**
     * @fn void Console::RegisterDefaultCommands()
     * @brief Wire the built-in command set. Defined in ConsoleCommands.cpp.
     * @author Alex (<https://github.com/lextpf>)
     */
    void RegisterDefaultCommands();

    /**
     * @fn SuggestionResult Console::ComputeSuggestions(std::size_t maxCount) const
     * @brief Compute the up-to-maxCount autocomplete suggestions for the current input line.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Suggests command names while typing the verb, and falls back to the
     * verb's `argCompletions` callback when typing positional arguments. Used
     * by both the dropdown renderer and Tab completion so the visible list and
     * the chosen completion stay in lockstep.
     */
    [[nodiscard]] SuggestionResult ComputeSuggestions(std::size_t maxCount) const;

    /**
     * @fn void Console::ClampSuggestionScroll(std::size_t itemCount)
     * @brief Slide m_SuggestionScroll so m_SuggestionIndex stays inside the visible window, then
     * clamp the scroll to a valid range.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Called any time the index or item count changes.
     */
    void ClampSuggestionScroll(std::size_t itemCount);

    /**
     * @brief Hard cap on suggestions so a degenerate prefix can't blow up the box.
     *
     * Larger than any realistic command list; raise if completion sources ever
     * return more.
     */
    static constexpr std::size_t kMaxSuggestions = 64;

    /**
     * @brief Maximum rows shown in the dropdown box at once.
     *
     * Items beyond this scroll behind the visible window.
     */
    static constexpr std::size_t kMaxVisibleSuggestions = 8;

    Game& m_Game;
    ConsoleBuffer m_Buffer;
    ConsoleCommandRegistry m_Registry;
    State m_State = State::Closed;
    /**
     * @brief Scrollback row count from the last Render(), cached so
     * ScrollToOutputTop can position the view without re-deriving the overlay
     * layout here.
     */
    int m_LastVisibleLines = 0;
    /**
     * @brief Index of the highlighted entry in the current dropdown (across the
     * full item list, not just the visible window).
     *
     * Reset to 0 on any input modification; clamped to the suggestion count
     * when read.
     */
    std::size_t m_SuggestionIndex = 0;
    /**
     * @brief First visible row in the dropdown's sliding window.
     *
     * Adjusted via arrow-key navigation, mouse wheel, and ClampSuggestionScroll.
     */
    std::size_t m_SuggestionScroll = 0;
    /**
     * @brief Geometry cached during Render() so input handlers (mouse hover,
     * click, wheel) can hit-test the dropdown without their own copy of the
     * layout math.
     *
     * Refreshed every frame; `visible` is false when the dropdown isn't drawn
     * this frame.
     */
    struct DropdownRect
    {
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;
        float rowH = 0.0f;
        float padTop = 0.0f;
        std::size_t topRow = 0;  ///< First visible item index.
        std::size_t visibleRows = 0;
        std::size_t totalItems = 0;
        bool visible = false;
    };
    DropdownRect m_LastDropdown;

    /**
     * @brief Session-scoped bookmark storage.
     *
     * Keyed by user-supplied name; value is the player's tile coordinates at
     * the time of `bookmark.set`. Empty by default; not persisted across
     * program runs.
     */
    std::unordered_map<std::string, glm::ivec2> m_Bookmarks;
};

/**
 * @fn constexpr Console::State NextConsoleState(Console::State s) noexcept
 * @brief Pure Closed -> Half -> Full -> Closed rotation over Console::State.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Exposed as a free function (and made `constexpr`) so it can be validated
 * without constructing a Console + Game pair, which would require a GL context
 * per the test-suite constraints.
 *
 * @warning This is not the hotkey behavior. No production code calls it - the
 * only callers are tests/ConsoleStateTests.cpp. The real transitions live in
 * `Console::Toggle` (F12, which only moves between `Closed` and `Half`) and
 * `Console::ToggleFullscreen` (Tab, which only swaps `Half` and `Full`), so
 * this three-step rotation is reachable from no input path.
 *
 * @param s Current state.
 * @return The next state in the rotation.
 */
[[nodiscard]] constexpr Console::State NextConsoleState(Console::State s) noexcept
{
    switch (s)
    {
        case Console::State::Closed:
            return Console::State::Half;
        case Console::State::Half:
            return Console::State::Full;
        case Console::State::Full:
            return Console::State::Closed;
    }
    return Console::State::Closed;
}
