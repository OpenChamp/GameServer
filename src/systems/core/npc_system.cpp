#include "npc_system.hpp"

#include "components/npc_component.hpp"
#include "intent.hpp"

void NPCSystem::update(const SystemContext& ctx) {
    auto npc_entities = ctx.entity_manager.get_entities_with_component<NPCComponent>();

    for(auto entity : npc_entities) {
        if(entity->get_component<NPCComponent>()->npc_type == NPCType::MINION) {
            handle_minion_update(ctx, entity);
        }
    }
}


void NPCSystem::handle_minion_update(const SystemContext& ctx, Entity* entity) {
    NPCComponent* npc_component = entity->get_component<NPCComponent>();
    IntentComponent* intent = entity->get_component<IntentComponent>();
    Movement* movement = entity->get_component<Movement>();
    Stats* stats = entity->get_component<Stats>();

    // Currently attacking an enemy unit
    if(intent->type == IntentType::ATTACK_TARGET) {
        Entity* target = ctx.entity_manager.get_entity(intent->target_entity_id);

        if(!target || !target->get_component<Stats>() || target->get_component<Stats>()->health <= 0.0f) {
            // Current target is dead, continue pathing
            intent->type = IntentType::MOVE_TO_POSITION;
            intent->target_position = npc_component->objective;
        }

        // Make sure entity is not chasing too far away
        if((npc_component->objective - movement->position).length() >= npc_component->chase_distance) {
            intent->type = IntentType::MOVE_TO_POSITION;
            intent->target_position = npc_component->objective;
            return;
        }

        // Not chasing too far, keep attacking
        return;
    }

    // Not attacking anything, so keep following the path but aggress on targets in range
    if(intent->type == IntentType::MOVE_TO_POSITION) {
        float distance_to_target = npc_component->aggression_range;
        EntityID id = INVALID_ENTITY_ID;

        // If an enemy entity is in range, attack them instead of running further
        // TODO get entities in vicinity, not all of them... - ploinky 27.11.2025
        for (const auto& entity_pair : ctx.entity_manager.get_all_entities()) {
            const Entity& other_entity = entity_pair.second;

            // Do not attack allies, entities without stats or self
            if(other_entity.get_id() == entity->get_id()
                || !other_entity.get_component<Stats>()
                || (other_entity.get_component<Stats>()->team_id == stats->team_id)) {
                    continue;
            }

            const Movement* other_movement = other_entity.get_component<Movement>();
            float distance_to_other = (other_movement->position - movement->position).length();

            // Found enemy, check range
            if(distance_to_other <= distance_to_target) {
                id = other_entity.get_id();
                distance_to_target = distance_to_other;
            }
        }

        // Found a target in aggression range, attack
        if(id != INVALID_ENTITY_ID) {
            intent->type = IntentType::ATTACK_TARGET;
            intent->target_entity_id = id;
            return;
        }
    }

    // If nothing else, back to objective
    if(intent->type == IntentType::NONE) {
        intent->type = IntentType::MOVE_TO_POSITION;
        intent->target_position = npc_component->objective;
        return;
    }
}