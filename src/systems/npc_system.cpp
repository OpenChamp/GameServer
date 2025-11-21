#include "npc_system.hpp"

#include "components/entity_state.hpp"

void NPCSystem::update(const SystemContext& ctx) {
    // TODO properly pick npc entities only - ploinky 20/11/2025
    for(auto npc_entity : ctx.entity_manager.get_entities_with_component<PathfindingComponent>()) {
        update_entity(ctx, npc_entity);
    }
}

void NPCSystem::update_entity(const SystemContext& ctx, Entity* npc_entity) {
    Movement* npc_movement = npc_entity->get_component<Movement>();
    Stats* npc_stats = npc_entity->get_component<Stats>();
    EntityStateComponent* npc_state = npc_entity->get_component<EntityStateComponent>();
    PathfindingComponent* npc_path = npc_entity->get_component<PathfindingComponent>();
    
    if(!npc_movement || !npc_stats || !npc_state || !npc_path) {
        LOG_WARN("NPCSystem attempted to update entity %d but it's missing (a) relevant component(s)", npc_entity->get_id());
        return;
    }

    if(npc_state->current_state == EntityState::ATTACKING) {
        // TODO if we're "flattening the navmesh" then this should really be a Vec2 - ploinky 21/11/2025
        Vec3 current_waypoint = npc_path->waypoints[npc_path->current_waypoint_index];
        Vec3 current_position = Vec3(npc_movement->position.x, 0, npc_movement->position.y);

        // TODO Having this "stay aggrod" distance in code is a bad idea.
        // This should be in data, because you may want different chasing distances per map. - ploinky 21/11/2025
        if(std::abs((current_waypoint - current_position).length()) > 30.0f) {
            npc_state->current_state = EntityState::MOVING;
            npc_state->target_entity_id = INVALID_ENTITY_ID;
            npc_state->state_duration_ms = 0.0f;
            return; // done updating this entity
        }
    }

    if(npc_state->current_state == EntityState::MOVING || npc_state->current_state == EntityState::PATHFINDING_WAITING) {
        // TODO how do I find all entities in my general area that are attackable? - ploinky 20/11/2025
        for(auto other_entity : ctx.entity_manager.get_entities_with_component<Stats>()) {
            if(other_entity == npc_entity) {
                continue;
            }

            Movement* other_movement = other_entity->get_component<Movement>();
            if(!other_movement) {
                continue; // continue checking other entities in range
            }

            // TODO This check needs to be against a different field in an attack component,
            // "acquisition_range" or something similar - ploinky 20/11/2025
            if(std::abs((npc_movement->position - other_movement->position).length()) <= npc_stats->attack_range) {
                npc_state->current_state = EntityState::ATTACKING;
                npc_state->target_entity_id = other_entity->get_id();
                npc_state->state_duration_ms = 0.0f;
                LOG_INFO("Entity %d is attacking entity %d", npc_entity->get_id(), other_entity->get_id());
                return; // done updating this entity
            }
        }
    }
}