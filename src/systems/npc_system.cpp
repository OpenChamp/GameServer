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
    
    if(!npc_movement || !npc_stats) {
        LOG_WARN("NPCSystem attempted to update pathfinding entity %d without movement component", npc_entity->get_id());
        return;
    }

    if(npc_state->current_state == EntityState::ATTACKING) {
        // TODO check if we're too far off our lane and need to return
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

            // TODO this check needs to be in an attack component,
            // and it needs a second fields "acquisition_range" or something similar - ploinky 20/11/2025
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