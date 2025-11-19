#include <services/astar_pathfinding.hpp>
#include <log.hpp>

std::vector<Vec2> AStarPathfinder::FindPath(
    const Vec2& start_pos,
    const Vec2& goal_pos,
    const std::vector<Vec2>& vertices,
    const std::vector<std::vector<uint32_t>>& polygons,
    float entity_radius
) {
    // Handle edge cases
    if (vertices.empty() || polygons.empty()) {
        return {};
    }

    if (start_pos == goal_pos) {
        return {start_pos};
    }

    // If start and goal are very close, return direct path
    if (start_pos.distance_to(goal_pos) < 1.0f) {
        return {start_pos, goal_pos};
    }

    // Find which polygon contains the start position
    int start_polygon = -1;
    for (int i = 0; i < static_cast<int>(polygons.size()); ++i) {
        std::vector<Vec2> poly_verts;
        for (uint32_t vertex_idx : polygons[i]) {
            if (vertex_idx < vertices.size()) {
                poly_verts.push_back(vertices[vertex_idx]);
            }
        }
        if (PointInPolygon(start_pos, poly_verts)) {
            start_polygon = i;
            break;
        }
    }

    // Find which polygon contains the goal position
    int goal_polygon = -1;
    for (int i = 0; i < static_cast<int>(polygons.size()); ++i) {
        std::vector<Vec2> poly_verts;
        for (uint32_t vertex_idx : polygons[i]) {
            if (vertex_idx < static_cast<int>(vertices.size())) {
                poly_verts.push_back(vertices[vertex_idx]);
            }
        }
        if (PointInPolygon(goal_pos, poly_verts)) {
            goal_polygon = i;
            break;
        }
    }

    LOG_DEBUG("A*: start_polygon=%d, goal_polygon=%d for path (%.1f, %.1f) -> (%.1f, %.1f)", 
             start_polygon, goal_polygon, start_pos.x, start_pos.y, goal_pos.x, goal_pos.y);

    // If either position is not in a polygon, try to find the closest polygon
    if (start_polygon == -1) {
        float closest_dist = std::numeric_limits<float>::max();
        for (int i = 0; i < static_cast<int>(polygons.size()); ++i) {
            Vec2 centroid = GetPolygonCentroid(std::vector<Vec2>{});
            for (uint32_t vertex_idx : polygons[i]) {
                if (vertex_idx < vertices.size()) {
                    centroid.x += vertices[vertex_idx].x;
                    centroid.y += vertices[vertex_idx].y;
                }
            }
            centroid.x /= polygons[i].size();
            centroid.y /= polygons[i].size();

            float dist = start_pos.distance_to(centroid);
            if (dist < closest_dist) {
                closest_dist = dist;
                start_polygon = i;
            }
        }
    }

    if (goal_polygon == -1) {
        float closest_dist = std::numeric_limits<float>::max();
        for (int i = 0; i < static_cast<int>(polygons.size()); ++i) {
            Vec2 centroid;
            for (uint32_t vertex_idx : polygons[i]) {
                if (vertex_idx < vertices.size()) {
                    centroid.x += vertices[vertex_idx].x;
                    centroid.y += vertices[vertex_idx].y;
                }
            }
            centroid.x /= polygons[i].size();
            centroid.y /= polygons[i].size();

            float dist = goal_pos.distance_to(centroid);
            if (dist < closest_dist) {
                closest_dist = dist;
                goal_polygon = i;
            }
        }
    }

    if (start_polygon == -1 || goal_polygon == -1) {
        // Fallback: return direct path since we couldn't locate in polygons
        // This can happen if spawnpoints are outside the navmesh or on edges
        return {start_pos, goal_pos};
    }

    // A* algorithm on polygon graph
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open_set;
    std::unordered_map<uint32_t, float> g_costs;
    std::unordered_map<uint32_t, uint32_t> came_from;
    std::vector<bool> in_closed_set(polygons.size(), false);

    // Initialize start node
    Node start_node;
    start_node.polygon_id = start_polygon;
    start_node.g_cost = 0.0f;
    start_node.h_cost = Heuristic(start_pos, goal_pos);
    start_node.parent_polygon = UINT32_MAX;
    start_node.is_start = true;
    start_node.is_goal = (start_polygon == goal_polygon);

    open_set.push(start_node);
    g_costs[start_polygon] = 0.0f;

    while (!open_set.empty()) {
        Node current = open_set.top();
        open_set.pop();

        if (current.polygon_id == goal_polygon) {
            // Reconstruct path
            std::vector<Vec2> waypoints;
            waypoints.push_back(goal_pos);

            uint32_t current_poly = goal_polygon;
            while (current_poly != start_polygon) {
                auto it = came_from.find(current_poly);
                if (it == came_from.end()) {
                    break; // Path broken
                }
                uint32_t parent_poly = it->second;

                // Get edge midpoint between current and parent
                Vec2 edge_point = GetSharedEdgeMidpoint(
                    polygons[current_poly],
                    polygons[parent_poly],
                    vertices
                );
                waypoints.push_back(edge_point);
                current_poly = parent_poly;
            }

            waypoints.push_back(start_pos);
            std::reverse(waypoints.begin(), waypoints.end());

            // Smooth the path
            return SmoothPath(waypoints, vertices, polygons, entity_radius);
        }

        if (in_closed_set[current.polygon_id]) {
            continue;
        }
        in_closed_set[current.polygon_id] = true;

        // Check all neighbors
        for (uint32_t neighbor_id = 0; neighbor_id < polygons.size(); ++neighbor_id) {
            if (in_closed_set[neighbor_id]) {
                continue;
            }

            if (!ArePolygonsAdjacent(polygons[current.polygon_id], polygons[neighbor_id])) {
                continue;
            }

            // Calculate cost to neighbor
            Vec2 current_center = GetPolygonCentroid(
                [&]() {
                    std::vector<Vec2> verts;
                    for (uint32_t idx : polygons[current.polygon_id]) {
                        if (idx < vertices.size()) {
                            verts.push_back(vertices[idx]);
                        }
                    }
                    return verts;
                }()
            );

            Vec2 neighbor_center = GetPolygonCentroid(
                [&]() {
                    std::vector<Vec2> verts;
                    for (uint32_t idx : polygons[neighbor_id]) {
                        if (idx < vertices.size()) {
                            verts.push_back(vertices[idx]);
                        }
                    }
                    return verts;
                }()
            );

            float move_cost = current_center.distance_to(neighbor_center);
            float new_g_cost = g_costs[current.polygon_id] + move_cost;

            auto it = g_costs.find(neighbor_id);
            if (it == g_costs.end() || new_g_cost < it->second) {
                g_costs[neighbor_id] = new_g_cost;
                came_from[neighbor_id] = current.polygon_id;

                Node neighbor_node;
                neighbor_node.polygon_id = neighbor_id;
                neighbor_node.g_cost = new_g_cost;
                neighbor_node.h_cost = Heuristic(neighbor_center, goal_pos);
                neighbor_node.parent_polygon = current.polygon_id;
                neighbor_node.is_start = false;
                neighbor_node.is_goal = (neighbor_id == goal_polygon);

                open_set.push(neighbor_node);
            }
        }
    }

    // No path found via A*, return direct path as fallback
    // This ensures minions can at least move in the right direction
    return {start_pos, goal_pos};
}

bool AStarPathfinder::PointInPolygon(const Vec2& point, const std::vector<Vec2>& polygon_vertices) {
    if (polygon_vertices.size() < 3) {
        return false;
    }

    // Ray casting algorithm
    int inside = 0;
    size_t n = polygon_vertices.size();

    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const Vec2& xi = polygon_vertices[i];
        const Vec2& xj = polygon_vertices[j];

        if ((xi.y > point.y) != (xj.y > point.y) &&
            point.x < (xj.x - xi.x) * (point.y - xi.y) / (xj.y - xi.y) + xi.x) {
            inside = !inside;
        }
    }

    return inside != 0;
}

Vec2 AStarPathfinder::GetPolygonCentroid(const std::vector<Vec2>& polygon_vertices) {
    Vec2 centroid;
    if (polygon_vertices.empty()) {
        return centroid;
    }

    for (const auto& vertex : polygon_vertices) {
        centroid.x += vertex.x;
        centroid.y += vertex.y;
    }

    centroid.x /= polygon_vertices.size();
    centroid.y /= polygon_vertices.size();

    return centroid;
}

bool AStarPathfinder::ArePolygonsAdjacent(
    const std::vector<uint32_t>& poly1,
    const std::vector<uint32_t>& poly2
) {
    // Two polygons are adjacent if they share at least 2 vertices (an edge)
    int shared_vertices = 0;
    for (uint32_t v1 : poly1) {
        for (uint32_t v2 : poly2) {
            if (v1 == v2) {
                shared_vertices++;
                if (shared_vertices >= 2) {
                    return true;
                }
            }
        }
    }
    return false;
}

Vec2 AStarPathfinder::GetSharedEdgeMidpoint(
    const std::vector<uint32_t>& poly1,
    const std::vector<uint32_t>& poly2,
    const std::vector<Vec2>& vertices
) {
    // Find the two shared vertices and return the midpoint
    std::vector<Vec2> shared_vertices;

    for (uint32_t v1 : poly1) {
        for (uint32_t v2 : poly2) {
            if (v1 == v2 && v1 < vertices.size()) {
                shared_vertices.push_back(vertices[v1]);
                break;
            }
        }
    }

    if (shared_vertices.size() >= 2) {
        return (shared_vertices[0] + shared_vertices[1]) * 0.5f;
    }

    // Fallback to centroids
    std::vector<Vec2> verts1, verts2;
    for (uint32_t idx : poly1) {
        if (idx < vertices.size()) verts1.push_back(vertices[idx]);
    }
    for (uint32_t idx : poly2) {
        if (idx < vertices.size()) verts2.push_back(vertices[idx]);
    }

    Vec2 center1 = GetPolygonCentroid(verts1);
    Vec2 center2 = GetPolygonCentroid(verts2);
    return (center1 + center2) * 0.5f;
}

float AStarPathfinder::Heuristic(const Vec2& a, const Vec2& b) {
    return a.distance_to(b);
}

std::optional<Vec2> AStarPathfinder::LineSegmentIntersectsPolygons(
    const Vec2& a,
    const Vec2& b,
    const std::vector<Vec2>& vertices,
    const std::vector<std::vector<uint32_t>>& polygons,
    float entity_radius
) {
    float closest_dist_squared = std::numeric_limits<float>::max();
    std::optional<Vec2> closest_intersection;

    // Check against all polygon edges
    for (const auto& polygon : polygons) {
        for (size_t i = 0; i < polygon.size(); ++i) {
            uint32_t v1_idx = polygon[i];
            uint32_t v2_idx = polygon[(i + 1) % polygon.size()];

            if (v1_idx >= vertices.size() || v2_idx >= vertices.size()) {
                continue;
            }

            Vec2 p1 = vertices[v1_idx];
            Vec2 p2 = vertices[v2_idx];

            Vec2 intersection;
            if (LineSegmentsIntersect(a, b, p1, p2, intersection)) {
                // Check distance from line segment to intersection
                float dist_sq = a.distance_squared_to(intersection);
                if (dist_sq < closest_dist_squared) {
                    closest_dist_squared = dist_sq;
                    closest_intersection = intersection;
                }
            }
        }
    }

    return closest_intersection;
}

bool AStarPathfinder::LineSegmentsIntersect(
    const Vec2& p1,
    const Vec2& p2,
    const Vec2& p3,
    const Vec2& p4,
    Vec2& intersection
) {
    float x1 = p1.x, y1 = p1.y;
    float x2 = p2.x, y2 = p2.y;
    float x3 = p3.x, y3 = p3.y;
    float x4 = p4.x, y4 = p4.y;

    float denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    if (std::abs(denom) < 1e-10f) {
        return false; // Parallel or coincident
    }

    float t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
    float u = -((x1 - x2) * (y1 - y3) - (y1 - y2) * (x1 - x3)) / denom;

    if (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f) {
        intersection.x = x1 + t * (x2 - x1);
        intersection.y = y1 + t * (y2 - y1);
        return true;
    }

    return false;
}

std::vector<Vec2> AStarPathfinder::SmoothPath(
    const std::vector<Vec2>& raw_path,
    const std::vector<Vec2>& vertices,
    const std::vector<std::vector<uint32_t>>& polygons,
    float entity_radius
) {
    if (raw_path.size() < 2) {
        return raw_path;
    }
    
    if (raw_path.size() < 3) {
        return raw_path;
    }

    std::vector<Vec2> smoothed;
    smoothed.push_back(raw_path[0]);

    size_t current = 0;
    while (current < raw_path.size() - 1) {
        // Try to find the farthest point we can reach directly
        size_t farthest = current + 1;
        for (size_t i = current + 2; i < raw_path.size(); ++i) {
            if (!LineSegmentIntersectsPolygons(
                raw_path[current],
                raw_path[i],
                vertices,
                polygons,
                entity_radius
            )) {
                farthest = i;
            } else {
                break; // Stop searching once we hit an obstacle
            }
        }

        if (farthest != current + 1) {
            current = farthest;
            smoothed.push_back(raw_path[current]);
        } else {
            current++;
            smoothed.push_back(raw_path[current]);
        }
    }

    if (smoothed.back() != raw_path.back()) {
        smoothed.push_back(raw_path.back());
    }

    return smoothed;
}
