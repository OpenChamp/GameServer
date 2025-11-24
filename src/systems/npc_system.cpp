#include "npc_system.hpp"

#include "components/entity_state.hpp"
#include "components/npc_component.hpp"

void NPCSystem::update(const SystemContext& ctx) {
    for(auto npc_entity : ctx.entity_manager.get_entities_with_component<NPCComponent>()) {
        update_entity(ctx, npc_entity);
    }
}

void NPCSystem::update_entity(const SystemContext& ctx, Entity* npc_entity) {
    Movement* npc_movement = npc_entity->get_component<Movement>();
    Stats* npc_stats = npc_entity->get_component<Stats>();
    EntityStateComponent* npc_state = npc_entity->get_component<EntityStateComponent>();
    PathfindingComponent* npc_path = npc_entity->get_component<PathfindingComponent>();
    NPCComponent* npc_comp = npc_entity->get_component<NPCComponent>();
    
    if(!npc_movement || !npc_stats || !npc_state || !npc_path || !npc_comp) {
        LOG_WARN("NPCSystem attempted to update entity %d but it's missing (a) relevant component(s)", npc_entity->get_id());
        return;
    }

    if(npc_comp->npc_type == NPCType::MINION) {
        if(npc_state->current_state == EntityState::ATTACKING) {
            Vec2 current_waypoint = npc_path->waypoints[npc_path->current_waypoint_index];

            if((current_waypoint - npc_movement->position).length() > npc_comp->chase_distance) {
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
                if((npc_movement->position - other_movement->position).length() <= npc_stats->attack_range) {
                    npc_state->current_state = EntityState::ATTACKING;
                    npc_state->target_entity_id = other_entity->get_id();
                    npc_state->state_duration_ms = 0.0f;
                    LOG_INFO("Entity %d is attacking entity %d", npc_entity->get_id(), other_entity->get_id());
                    return; // done updating this entity
                }
            }
        }
    }
}