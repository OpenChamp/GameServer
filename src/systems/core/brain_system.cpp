#include <systems/core/brain_system.hpp>
#include <systems/util/targeting_utility.hpp>
#include <components/intent.hpp>
#include <components/entity_state.hpp>
#include <components/stats.hpp>
#include <components/npc_component.hpp>
#include <components/attack.hpp>
#include <components/movement.hpp>
#include <components/pathfinding.hpp>
#include <libs/log.hpp>
#include <cmath>

void BrainSystem::update(const SystemContext& ctx) {
    // Get all entities with IntentComponent
    auto entities_with_intent = ctx.entity_manager.get_entities_with_component<IntentComponent>();
    
    for (auto* entity : entities_with_intent) {
        process_entity_intent(ctx, entity);
    }
}

void BrainSystem::process_entity_intent(const SystemContext& ctx, Entity* entity) {
    auto* intent_comp = entity->get_component<IntentComponent>();
    
    if(intent_comp->type == IntentType::ATTACK_TARGET) {
        handle_attack_intent(ctx, entity);
    }

    if(intent_comp->type == IntentType::MOVE_TO_POSITION) {
        handle_move_intent(ctx, entity);
    }
}

void BrainSystem::handle_attack_intent(const SystemContext& ctx, Entity* entity) {
    auto* intent = entity->get_component<IntentComponent>();
    auto* stats = entity->get_component<Stats>();
    auto* attack = entity->get_component<AttackComponent>();
    auto* state = entity->get_component<EntityStateComponent>();

    // Continue attacking if entity is in the middle of an attack
    if(attack->target.has_value()) {
        if(attack->target.value() == intent->target_entity_id) {
            // Already in the middle of an attack on the correct target
            return;
        }

        // Attacking the wrong target, update attack target to new intent
        LOG_DEBUG("Entity %d has changed target from %d to %d", entity->get_id(), state->target_entity_id, intent->target_entity_id);
    }

    Entity* target = ctx.entity_manager.get_entity(intent->target_entity_id);
    Movement* target_movement = target->get_component<Movement>();
    Movement* entity_movement = entity->get_component<Movement>();

    // Move into range if too far away
    if((target_movement->position - entity_movement->position).length() > stats->attack_range) {
        attack->target = std::nullopt; // Cancel any ongoing attack
        entity_movement->target = target_movement->position;
        return;
    }

    // Start attack if in range
    if(!attack->target.has_value() || attack->target.value() != target->get_id()) {
        entity_movement->target = std::nullopt; // Stop moving
        attack->target = intent->target_entity_id;
    }
}

void BrainSystem::handle_move_intent(const SystemContext& ctx, Entity* entity) {
    Movement* movement = entity->get_component<Movement>();
    IntentComponent* intent = entity->get_component<IntentComponent>();

    // Set the movement target if required
    if(!movement->target.has_value() || movement->target.value() != intent->target_position) {
        movement->target = intent->target_position;
    }
}