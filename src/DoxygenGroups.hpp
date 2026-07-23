// documentation-only header

/**
 * @brief documentation groups; arrows mean drives or reads from.
 * @author Alex (https://github.com/lextpf)
 *
 * ```mermaid
 * flowchart LR
 * classDef core fill:#1e3a5f,stroke:#3b82f6,color:#e2e8f0
 * classDef render fill:#2e1f5e,stroke:#8b5cf6,color:#e2e8f0
 * classDef world fill:#134e3a,stroke:#10b981,color:#e2e8f0
 * classDef entity fill:#4a3520,stroke:#f59e0b,color:#e2e8f0
 * classDef input fill:#164e54,stroke:#06b6d4,color:#e2e8f0
 * classDef dialogue fill:#4c1d3d,stroke:#ec4899,color:#e2e8f0
 * classDef effects fill:#3b2a55,stroke:#a78bfa,color:#e2e8f0
 * classDef editor fill:#3f3a1c,stroke:#eab308,color:#e2e8f0
 *
 * Core["Core Engine"]:::core
 * Rendering["Rendering System"]:::render
 * World["World System"]:::world
 * Entities["Entity System"]:::entity
 * Input["Input System"]:::input
 * Dialogue["Dialogue System"]:::dialogue
 * Effects["Visual Effects"]:::effects
 * Editor["Editor and Tools"]:::editor
 *
 * Core --> Rendering
 * Core --> World
 * Core --> Entities
 * Core --> Input
 * Entities --> World
 * Rendering --> World
 * Entities --> Dialogue
 * Input --> Dialogue
 * Input --> Editor
 * Editor --> World
 * Editor --> Entities
 * Effects --> World
 * Effects --> Rendering
 * ```
 */

/**
 * @addtogroup Core Core engine
 * @brief game lifecycle, state and subsystem orchestration.
 *
 * the frame delta is sampled before event polling and clamped before updates.
 *
 * @code{.cpp}
 * while (running)
 * {
 *     float deltaTime = currentTime - lastFrameTime;
 *     glfwPollEvents();
 *     deltaTime = std::min(deltaTime, MAX_DELTA_TIME);
 *     ProcessInput(deltaTime);
 *     Update(deltaTime);
 *     Render();
 * }
 * @endcode
 */

/**
 * @addtogroup Rendering Rendering system
 * @brief runtime OpenGL and Vulkan backends.
 *
 * IRenderer selects the backend. flat rendering uses world pixels with a top-left origin
 * and painter order at z = 0. the worked transforms below describe the flat path.
 *
 * @code
 * // Visible world area shrinks when zoomed in
 * worldWidth = 320 / 2.0 = 160 world units visible
 *
 * // Screen center (0.5) maps to middle of visible area
 * worldX = 0.5 * 160 + 100 = 180
 *
 * // World position 180 is tile 11 (180 / 16 = 11.25, floor = 11)
 * tileX = floor(180 / 16) = 11
 * @endcode
 *
 * @code{.cpp}
 * // Get mouse position in screen pixels
 * double mouseX, mouseY;
 * glfwGetCursorPos(window, &mouseX, &mouseY);
 *
 * // Calculate visible world dimensions (affected by zoom)
 * float baseWidth = tilesVisibleX * tileSize;  // e.g., 20 * 16 = 320
 * float worldWidth = baseWidth / cameraZoom;   // Shrinks when zoomed in
 *
 * // Transform screen -> world
 * float worldX = (mouseX / screenWidth) * worldWidth + cameraX;
 * float worldY = (mouseY / screenHeight) * worldHeight + cameraY;
 *
 * // Transform world -> tile (floor for correct negative handling)
 * int tileX = static_cast<int>(std::floor(worldX / tileSize));
 * int tileY = static_cast<int>(std::floor(worldY / tileSize));
 * @endcode
 *
 * $$
 * \vec{p}_{screen} = \vec{p}_{world} - \vec{p}_{camera}
 * $$
 *
 * $$
 * \vec{p}_{ndc} = M_{projection} \cdot \vec{p}_{screen}
 * $$
 *
 * $$
 * M_{ortho} = \begin{bmatrix}
 *   \frac{2}{w} & 0 & 0 & -1 \\
 *   0 & \frac{-2}{h} & 0 & 1 \\
 *   0 & 0 & -1 & 0 \\
 *   0 & 0 & 0 & 1
 * \end{bmatrix}
 * $$
 *
 * $$
 * \begin{pmatrix} x_{clip} \\ y_{clip} \\ z_{clip} \\ 1 \end{pmatrix}
 * = M_{ortho} \begin{pmatrix} x \\ y \\ z \\ 1 \end{pmatrix}
 * = \begin{pmatrix} \frac{2x}{w} - 1 \\ 1 - \frac{2y}{h} \\ -z \\ 1 \end{pmatrix}
 * \Bigg|_{z=0}
 * = \begin{pmatrix} \frac{2x}{w} - 1 \\ 1 - \frac{2y}{h} \\ 0 \\ 1 \end{pmatrix}
 * $$
 *
 * $$
 * world_x = \frac{screen_x}{screen\_width} \times \frac{base\_width}{zoom} + camera_x
 * $$
 *
 * $$
 * tile_x = \lfloor \frac{world_x}{tile\_size} \rfloor
 * $$
 */

/**
 * @addtogroup World World system
 * @brief tile layers, collision and NPC navigation.
 *
 * tile storage is row-major. layer counts are dynamic. collision and navigation are
 * independent grids.
 *
 * $$
 * i = y \times w + x
 * $$
 *
 * $$
 * c = (A_{min} < B_{max}) \land (A_{max} > B_{min})
 * $$
 */

/**
 * @addtogroup Entities entity system
 * @brief player and NPC components and systems.
 *
 * positions are feet anchors at sprite bottom-center. support gates collision; visual
 * elevation can lag the logical height. CharacterConstants defines speed and frame cadence.
 *
 * $$
 * \vec{p}_{anchor} = (center_x, bottom_y)
 * $$
 *
 * $$
 * h_{min} = (anchor_x - \frac{w}{2}, anchor_y - h)
 * $$
 *
 * $$
 * h_{max} = (anchor_x + \frac{w}{2}, anchor_y)
 * $$
 */

/**
 * @addtogroup Input Input system
 * @brief keyboard and mouse dispatch.
 *
 * Game routes input to console, dialogue, editor and movement handlers. KeyToggle tracks
 * press edges. use console help for the command catalog and Editor for editing modes.
 *
 * $$
 * \vec{v} = \hat{d} \times speed \times \Delta t
 * $$
 */

/**
 * @addtogroup Dialogue Dialogue system
 * @brief branching conversations and flag consequences.
 *
 * DialogueStore owns trees; DialogueManager copies the active tree and filters options.
 * DialogueTypes documents JSON syntax.
 *
 * @code
 * Player presses F near NPC
 *         v
 * DialogueManager::StartDialogue()
 *         v
 * Display current DialogueNode text
 *         v
 * Show DialogueOptions (if any)
 *         v
 * Player selects option -> Jump to next node
 *         v
 * Repeat until end node or player exits
 * @endcode
 */

/**
 * @addtogroup Effects visual Effects system
 * @brief weather, particles and sky rendering.
 *
 * TimeManager and WeatherDirector supply time and weather state to effect rendering.
 *
 * $$
 * \alpha(t) = \begin{cases}
 *   \frac{t}{t_{fade}} & t < t_{fade} \\
 *   1.0 & t_{fade} \leq t \leq t_{life} - t_{fade} \\
 *   \frac{t_{life} - t}{t_{fade}} & t > t_{life} - t_{fade}
 * \end{cases}
 * $$
 */

/**
 * @addtogroup Editor Editor & tools
 * @brief map editing and undo history.
 *
 * EditorContext borrows one frame of game state. EditorCommand captures undo data; stroke
 * accumulators group drag mutations. the clipboard remains session-local.
 */
