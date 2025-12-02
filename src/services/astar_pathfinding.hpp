#pragma once

#include <vector>
#include <queue>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <limits>
#include <optional>
#include <cstdint>

#include <libs/math.hpp>

/**
 * A* Pathfinding for 2D polygon-based navmeshes.
 * Works with a navmesh represented as vertices and polygon indices.
 */
class AStarPathfinder {
public:
    /**
     * Polygon grid data structure for spatial acceleration.
     * This is pure data - no methods, just storage.
     * Populated at map load time by MapSystem::build_polygon_grid().
     */
    struct NavGrid {
        std::vector<std::vector<uint32_t>> grid_cells;  // grid_cells[y * grid_width + x] = {polygon_ids}
        Vec2 grid_origin;                                // World position of grid origin (bottom-left)
        float grid_cell_size = 50.0f;                    // Size of each grid cell in world units
        int grid_width = 0;                              // Number of cells in X direction
        int grid_height = 0;                             // Number of cells in Y direction

        bool IsBuilt() const { return grid_width > 0 && grid_height > 0; }
    };

    /**
     * Find a path from start to goal in the navmesh.
     * 
     * @param start_pos Starting position (2D)
     * @param goal_pos Goal position (2D)
     * @param vertices List of all navmesh vertices
     * @param polygons List of polygons (each polygon is a list of vertex indices)
     * @param entity_radius The radius of the entity for collision checking
     * @param nav_grid Nav Grid for faster queries
     * @return Vector of 2D waypoints along the path, or empty if no path found
     */
    static std::vector<Vec2> FindPath(
        const Vec2& start_pos,
        const Vec2& goal_pos,
        const std::vector<Vec2>& vertices,
        const std::vector<std::vector<uint32_t>>& polygons,
        float entity_radius = 0.0f,
        const NavGrid* nav_grid = nullptr
    );

private:
    struct Node {
        uint32_t polygon_id;
        float g_cost;  // Cost from start
        float h_cost;  // Heuristic to goal
        uint32_t parent_polygon;
        bool is_start;
        bool is_goal;

        float f_cost() const { return g_cost + h_cost; }

        bool operator>(const Node& other) const {
            return f_cost() > other.f_cost();
        }
    };

    /**
     * Check if a point is inside a polygon.
     */
    static bool PointInPolygon(const Vec2& point, const std::vector<Vec2>& polygon_vertices);

    /**
     * Get the centroid of a polygon.
     */
    static Vec2 GetPolygonCentroid(const std::vector<Vec2>& polygon_vertices);

    /**
     * Check if two polygons are adjacent (share an edge).
     */
    static bool ArePolygonsAdjacent(
        const std::vector<uint32_t>& poly1,
        const std::vector<uint32_t>& poly2
    );

    /**
     * Get the shared edge midpoint between two adjacent polygons.
     */
    static Vec2 GetSharedEdgeMidpoint(
        const std::vector<uint32_t>& poly1,
        const std::vector<uint32_t>& poly2,
        const std::vector<Vec2>& vertices
    );

    /**
     * Simple heuristic (Euclidean distance).
     */
    static float Heuristic(const Vec2& a, const Vec2& b);

    /**
     * Check if a line segment intersects with any polygon edges (for collision detection).
     * Returns the closest intersection point if found, or nullopt if clear.
     */
    static std::optional<Vec2> LineSegmentIntersectsPolygons(
        const Vec2& a,
        const Vec2& b,
        const std::vector<Vec2>& vertices,
        const std::vector<std::vector<uint32_t>>& polygons,
        float entity_radius
    );

    /**
     * Check if a line segment intersects with another line segment.
     */
    static bool LineSegmentsIntersect(
        const Vec2& p1,
        const Vec2& p2,
        const Vec2& p3,
        const Vec2& p4,
        Vec2& intersection
    );

    /**
     * Smooth the path by removing unnecessary waypoints.
     * Uses line-of-sight test to create a simplified path.
     */
    static std::vector<Vec2> SmoothPath(
        const std::vector<Vec2>& raw_path,
        const std::vector<Vec2>& vertices,
        const std::vector<std::vector<uint32_t>>& polygons,
        float entity_radius
    );
};
