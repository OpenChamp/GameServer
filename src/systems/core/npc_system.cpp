#include "npc_system.hpp"

#include "components/entity_state.hpp"
#include "components/npc_component.hpp"
#include "components/auto_attack.hpp"
#include <cfloat>

void NPCSystem::update(const SystemContext& ctx) {
    for(auto npc_entity : ctx.entity_manager.get_entities_with_component<NPCComponent>()) {
        // Update state cooldown timers for all NPCs
        NPCComponent* npc_comp = npc_entity->get_component<NPCComponent>();
        if (npc_comp) {
            npc_comp->update_state_cooldown(ctx.delta_time_ms);
        }
        update_entity(ctx, npc_entity);
    }
}

void NPCSystem::update_entity(const SystemContext& /*ctx*/, Entity* npc_entity) {
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
        // Stop attacking if the minion is too far away from its path
        // This enforces map-boundary behavior - minions don't chase forever
        if(npc_state->current_state == EntityState::ATTACKING && npc_path->waypoints.size() > 0) {
            Vec2 current_waypoint = npc_path->waypoints[npc_path->current_waypoint_index];

            if((current_waypoint - npc_movement->position).length() > npc_comp->chase_distance) {
                // Only change state if cooldown allows it
                if (npc_comp->can_change_state()) {
                    npc_state->current_state = EntityState::MOVING;
                    npc_state->target_entity_id = INVALID_ENTITY_ID;
                    npc_state->state_duration_ms = 0.0f;
                    npc_comp->on_state_changed();
                    LOG_DEBUG("Entity %d returning to MOVING (too far from waypoint)", npc_entity->get_id());
                }
            }
        }
    }
}