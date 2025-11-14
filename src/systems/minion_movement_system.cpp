#include "minion_movement_system.hpp"
#include "components/navmesh.hpp"
#include "components/minion.hpp"
#include "components/movement.hpp"
#include <cmath>
#include <log.hpp>

float MinionMovementSystem::distance(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

Vec3 MinionMovementSystem::normalize(const Vec3& v) {
    float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < 0.0001f) {
        return Vec3(0.0f, 0.0f, 0.0f);
    }
    return v / len;
}

void MinionMovementSystem::update(EntityManager& entity_manager, float delta_time, const Entity* map_entity) {
    if (!map_entity) {
        return;
    }
    
    // Get the navmesh component
    const NavMesh* navmesh = map_entity->get_component<NavMesh>();
    if (!navmesh || navmesh->spawnpoints.empty()) {
        return;
    }
    
    // Iterate through all entities
    std::vector<Entity*> minions = entity_manager.get_entities_with_component<Minion>();
    for (Entity* entity : minions) {
        if (!entity->has_component<Movement>()) {
            continue;
        }
        
        Movement* movement = entity->get_component<Movement>();
        Minion* minion = entity->get_component<Minion>();
        
        // Skip if already dead or reached destination
        if (minion->is_dead || minion->has_reached_destination) {
            continue;
        }
        
        // Get target position
        int target_idx = minion->target_spawnpoint;
        if (target_idx < 0 || target_idx >= (int)navmesh->spawnpoints.size()) {
            continue;
        }
        
        Vec3 target_pos = navmesh->spawnpoints[target_idx];
        
        // Calculate direction to target
        Vec3 direction = target_pos - movement->position;
        float dist_to_target = distance(movement->position, target_pos);
        
        // Check if reached destination (within 0.5 units)
        if (dist_to_target < 0.5f) {
            movement->position = target_pos;
            movement->velocity = Vec3(0.0f, 0.0f, 0.0f);
            movement->is_moving = false;
            minion->has_reached_destination = true;
            
            LOG_INFO("Minion (ID %u) reached destination at (%.1f, %.1f, %.1f)",
                     entity->get_id(), target_pos.x, target_pos.y, target_pos.z);
            continue;
        }
        
        // Move toward target
        Vec3 move_direction = normalize(direction);
        Vec3 new_position = movement->position + move_direction * movement->move_speed * delta_time;
        
        // Keep minion within navmesh bounds (optional constraint)
        // For now, allow movement along the path
        
        movement->position = new_position;
        movement->velocity = move_direction * movement->move_speed;
    }
}
