#pragma once

#include <array>
#include <glm/glm.hpp>
#include <vector>

class Tilemap;

/**
 * @class PatrolRoute
 * @brief Patrol waypoints from navigable tiles without collision.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 *
 * BFS selects a compact reachable region, then a cycle walk or DFS builds the route.
 * A complete DFS returns to its start and loops; truncation can require ping-pong traversal.
 * Neighbors are probed right, left, down, up; asterisks in the trace mark backtracking.
 *
 * The BFS visited mask includes queued tiles beyond the cap. A capped ring can therefore
 * be marked closed even when its last and first waypoints are not adjacent.
 *
 * Initialize allocates a whole-map visited mask; the cycle branch allocates a second.
 * Working time and space scale with map area, independently of the waypoint cap.
 *
 * BFS favors nearby connected tiles, avoiding an initial DFS branch that spends the whole cap far
 * from the start. A region whose reached tiles all have degree two uses a direct cycle walk. Other
 * regions record DFS backtracks so the patrol can return through branches.
 * ```mermaid
 * flowchart LR
 *     classDef start fill:#1e3a5f,stroke:#3b82f6,color:#e2e8f0
 *     classDef decision fill:#4a3520,stroke:#f59e0b,color:#e2e8f0
 *     classDef process fill:#134e3a,stroke:#10b981,color:#e2e8f0
 *     classDef result fill:#2e1f5e,stroke:#8b5cf6,color:#e2e8f0
 *     classDef fail fill:#4c1d24,stroke:#ef4444,color:#e2e8f0
 *
 *     A[Initialize]:::start --> A2{"Tilemap non-null and<br/>start tile walkable?"}:::decision
 *     A2 -->|No| X["Return false<br/>route left untouched"]:::fail
 *     A2 -->|Yes| B[BFS: Collect<br/>reachable tiles]:::process
 *     B --> C{"3+ tiles AND every tile has<br/>exactly 2 BFS-reached neighbors?"}:::decision
 *     C -->|Yes| D[Simple cycle<br/>Walk the ring]:::result
 *     C -->|No| E[DFS with backtracking<br/>Visit collected tiles]:::result
 *     D --> F[Loop mode<br/>A-B-C-A-B-C]:::result
 *     E --> G{"End adjacent to<br/>or equal to start?"}:::decision
 *     G -->|Yes| F
 *     G -->|No| H[Ping-pong mode<br/>A-B-C-B-A]:::result
 *     F --> W{"At least 2<br/>waypoints?"}:::decision
 *     H --> W
 *     W -->|Yes| R["Return true"]:::result
 *     W -->|No| Y["Clear waypoints<br/>Return false"]:::fail
 * ```
 *
 * @verbatim
 *   Simple cycle (loop mode):      Branch (DFS backtracking):
 *
 *       A - B                           A - B
 *       |   |                               |
 *       D - C                               C
 *
 *   A connects to B and D              B connects to A and C
 *   All tiles have 2 neighbors         A has only 1 neighbor
 * @endverbatim
 *
 * ```mermaid
 * flowchart LR
 *     classDef visited fill:#134e3a,stroke:#10b981,color:#e2e8f0
 *     classDef backtrack fill:#4a3520,stroke:#f59e0b,color:#e2e8f0
 *
 *     subgraph "T-shaped map"
 *         direction LR
 *         T1[A]:::visited
 *         T2[B]:::visited
 *         T3[C]:::visited
 *         T4[D]:::visited
 *         T1 --- T2
 *         T3 --- T2
 *         T2 --- T4
 *     end
 *
 *     subgraph "Generated path"
 *         direction LR
 *         P1[A] --> P2[B] --> P3[D] --> P4["B*"]:::backtrack
 *         P4 --> P5[C] --> P6["B*"]:::backtrack --> P7["A*"]:::backtrack
 *     end
 * ```
 *
 * ```mermaid
 * stateDiagram-v2
 *     direction LR
 *
 *     classDef loop fill:#134e3a,stroke:#10b981,color:#e2e8f0
 *     classDef pingpong fill:#2e1f5e,stroke:#8b5cf6,color:#e2e8f0
 *     classDef waypoint fill:#1e3a5f,stroke:#3b82f6,color:#e2e8f0
 *
 *     state "Loop mode" as Loop {
 *         direction LR
 *         L0: Waypoint 0
 *         L1: Waypoint 1
 *         L2: Waypoint 2
 *         LN: Waypoint N-1
 *         L0 --> L1
 *         L1 --> L2
 *         L2 --> LN: ...
 *         LN --> L0: wrap
 *     }
 *
 *     state "Ping-pong mode" as PingPong {
 *         direction LR
 *         P0: Waypoint 0
 *         P1: Waypoint 1
 *         PN: Waypoint N-1
 *         P0 --> P1: forward
 *         P1 --> PN: ...
 *         PN --> P1: reverse
 *         P1 --> P0: ...
 *     }
 *
 *     class Loop loop
 *     class PingPong pingpong
 * ```
 *
 * $$
 * T_{Initialize} = \Theta(W \cdot H)
 * $$
 */
class PatrolRoute
{
public:
    PatrolRoute() = default;

    /**
     * @fn bool Initialize(int startTileX, int startTileY, const Tilemap* tilemap, int \
     *     maxRouteLength = 100)
     * @brief Build a route from connected navigable tiles without collision.
     * @author Alex (<https://github.com/lextpf>)
     *
     * A null map or invalid start leaves the existing route unchanged. Once the start passes
     * validation, the route is replaced; fewer than two generated waypoints leave it empty.
     * Backtracking consumes waypoint slots, so the cap also limits repeated tiles.
     *
     * @param startTileX Starting tile column; must be navigable and collision-free.
     * @param startTileY Starting tile row; must be navigable and collision-free.
     * @param tilemap Borrowed map; null leaves the existing route unchanged.
     * @param maxRouteLength Positive limit on generated waypoint entries.
     * @return True when at least two waypoints are available.
     */
    bool Initialize(int startTileX,
                    int startTileY,
                    const Tilemap* tilemap,
                    int maxRouteLength = 100);

    /**
     * @fn bool GetNextWaypoint(int& tileX, int& tileY)
     * @brief Return the current target and advance the traversal cursor.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Closed routes wrap. Open routes reverse without repeating either endpoint.
     *
     * @return False for an empty route, leaving both output coordinates unchanged.
     */
    bool GetNextWaypoint(int& tileX, int& tileY);

    int GetCurrentWaypointIndex() const { return m_CurrentWaypointIndex; }

    bool IsValid() const { return !m_Waypoints.empty(); }

    /**
     * @fn bool IsClosed() const
     * @brief True loops; false traverses back and forth.
     * @author Alex (<https://github.com/lextpf>)
     */
    bool IsClosed() const { return m_IsClosed; }

    size_t GetWaypointCount() const { return m_Waypoints.size(); }

    [[nodiscard]] const std::vector<glm::ivec2>& GetWaypoints() const { return m_Waypoints; }

    /**
     * @fn void Reset()
     * @brief Clear waypoints and traversal state.
     * @author Alex (<https://github.com/lextpf>)
     */
    void Reset()
    {
        m_Waypoints.clear();
        m_CurrentWaypointIndex = 0;
        m_IsClosed = false;
        m_PingPongForward = true;
    }

private:
    /**
     * @fn void DFSTraversal(glm::ivec2 current, std::vector<bool>& visited, \
     *     std::vector<glm::ivec2>& path, const Tilemap* tilemap, size_t maxLength)
     * @brief Append a DFS walk including backtracks.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Requires a non-null tilemap, an in-bounds walkable current tile, and a visited mask
     * with exactly mapWidth * mapHeight entries. Existing path entries are retained.
     */
    void DFSTraversal(glm::ivec2 current,
                      std::vector<bool>& visited,
                      std::vector<glm::ivec2>& path,
                      const Tilemap* tilemap,
                      size_t maxLength);

    struct NeighborResult
    {
        std::array<glm::ivec2, 4> tiles;
        int count = 0;
    };

    /**
     * @fn NeighborResult GetValidNeighbors(int tileX, int tileY, const Tilemap* tilemap) const
     * @brief Cardinal neighbors in right, left, down, up order.
     * @author Alex (<https://github.com/lextpf>)
     */
    NeighborResult GetValidNeighbors(int tileX, int tileY, const Tilemap* tilemap) const;

    /**
     * @fn bool IsValidTile(int tileX, int tileY, const Tilemap* tilemap) const
     * @brief False for null tilemap, out-of-bounds, non-navigable or collision-blocked tiles.
     * @author Alex (<https://github.com/lextpf>)
     */
    bool IsValidTile(int tileX, int tileY, const Tilemap* tilemap) const;

    /**
     * @fn bool AreAdjacent(const glm::ivec2& a, const glm::ivec2& b) const
     * @brief Cardinal adjacency requires manhattan distance 1.
     * @author Alex (<https://github.com/lextpf>)
     */
    bool AreAdjacent(const glm::ivec2& a, const glm::ivec2& b) const;

    std::vector<glm::ivec2> m_Waypoints;
    int m_CurrentWaypointIndex{0};
    bool m_IsClosed{false};
    bool m_PingPongForward{true};
};
