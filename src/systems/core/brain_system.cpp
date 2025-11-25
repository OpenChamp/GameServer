#include <systems/core/brain_system.hpp>
#include <systems/util/targeting_utility.hpp>
#include <components/intent.hpp>
#include <components/entity_state.hpp>
#include <components/stats.hpp>
#include <components/npc_component.hpp>
#include <components/auto_attack.hpp>
#include <components/attack.hpp>
#include <components/movement.hpp>
#include <components/pathfinding.hpp>
#include <libs/log.hpp>
#include <cmath>

void BrainSystem::update(const SystemContext& ctx) {
    // Get all entities with IntentComponent
    auto entities_with_intent = ctx.entity_manager.get_entities_with_component<IntentComponent>();
    
    for (auto* entity : entities_with_intent) {
        if (!entity) continue;
        
        process_entity_intent(ctx, entity);
    }
}

void BrainSystem::process_entity_intent(const SystemContext& ctx, Entity* entity) {
    auto* intent_comp = entity->get_component<IntentComponent>();
    auto* stats = entity->get_component<Stats>();
    auto* entity_state = entity->get_component<EntityStateComponent>();
    auto* npc_comp = entity->get_component<NPCComponent>();
    auto* attack = entity->get_component<AttackComponent>();
    
    if (!intent_comp || !stats || !entity_state) {
        return;
    }
    
    // Handle attack cooldown logic for ATTACK_TARGET intent
    if (attack && intent_comp->type == IntentType::ATTACK_TARGET) {
        // If attack is not ready, update cooldown timer
        if (!attack->can_attack) {
            if (attack->cooldown_timer_ms > 0.0f) {
                attack->cooldown_timer_ms -= ctx.delta_time_ms;
                if (attack->cooldown_timer_ms < 0.0f) {
                    attack->cooldown_timer_ms = 0.0f;
                }
            }
            if (attack->cooldown_timer_ms == 0.0f) {
                attack->can_attack = true;
            }
        }
        // If attack was just used, set cooldown timer
        if (attack->can_attack && attack->attack_in_progress) {
            attack->cooldown_timer_ms = attack->attack_cooldown_ms;
            attack->can_attack = false;
        }
    }

    // Check current intent
    switch (intent_comp->type) {
        // MOVEMENT LOGIC
        case IntentType::MOVE_TO_OBJECTIVE:
        case IntentType::MOVE_TO_POSITION:
        case IntentType::MOVE_TO_SPAWNPOINT: {
            // Check entity profile for aggression
            if (npc_comp) {
                using Personality = decltype(npc_comp->personality);
                if (npc_comp->personality == Personality::AGGRESSIVE ||
                    npc_comp->personality == Personality::ZEALOUS ||
                    npc_comp->personality == Personality::BALANCED)
                {
                    // Look for nearby enemies to attack
                    EntityID target = TargetingUtility::find_best_target(
                        ctx.entity_manager,
                        entity->get_id(),
                        npc_comp->aggression_range,
                        TargetComponent::TargetPriority::NEAREST,
                        true  // prioritize_closest
                    );

                    if (target != INVALID_ENTITY_ID) {
                        // Switch to ATTACK_TARGET intent
                        intent_comp->type = IntentType::ATTACK_TARGET;
                        intent_comp->target_entity_id = target;
                        LOG_DEBUG("Entity %u: Switching to ATTACK_TARGET intent (target %u)", 
                                  entity->get_id(), target);
                        // Don't return - fall through to process ATTACK_TARGET case immediately
                    }
                }
            }
            
            // Only set MOVING state if we didn't find a target to attack
            if (intent_comp->type != IntentType::ATTACK_TARGET) {
                entity_state->current_state = EntityState::MOVING;
            }
            break;
        }
        case IntentType::ATTACK_TARGET:
        default:
            break;
    }
    
    // Process ATTACK_TARGET intent (separate from movement intents)
    if (intent_comp->type == IntentType::ATTACK_TARGET) {
        // Check if target is valid and still alive
        bool target_valid = false;
        if (intent_comp->target_entity_id != INVALID_ENTITY_ID) {
            // Check if target entity exists
            Entity* target_entity = ctx.entity_manager.get_entity(intent_comp->target_entity_id);
            if (target_entity) {
                // Check if target is still alive
                Stats* target_stats = target_entity->get_component<Stats>();
                if (target_stats && target_stats->health > 0.0f) {
                    target_valid = true;
                }
            }
        }
        
        if (target_valid) {
            // Target is alive, proceed with attack logic
            float distance = TargetingUtility::calculate_distance(entity->get_component<Movement>()->position, intent_comp->target_entity_id, ctx.entity_manager);
            if (distance >= stats->attack_range) {
                // Out of range - move closer
                entity_state->current_state = EntityState::MOVING;
            } else {
                // In range - attack
                // BrainSystem only manages the state and intent
                // CombatSystem will read ATTACKING state + ATTACK_TARGET intent to initiate actual attacks
                entity_state->current_state = EntityState::ATTACKING;
                entity_state->target_entity_id = intent_comp->target_entity_id;
                LOG_DEBUG("Entity %u: Transitioned to ATTACKING state for target %u",
                          entity->get_id(), intent_comp->target_entity_id);
            }
        } else {
            // Target is dead or invalid - revert to moving toward objective
            intent_comp->type = IntentType::MOVE_TO_OBJECTIVE;
            intent_comp->target_entity_id = INVALID_ENTITY_ID;
            entity_state->current_state = EntityState::MOVING;
            
            // Clear any pending pathfinding waypoints from the attack path
            auto* pathfinding = entity->get_component<PathfindingComponent>();
            if (pathfinding) {
                pathfinding->waypoints.clear();
                pathfinding->current_waypoint_index = 0;
            }
            
            LOG_DEBUG("Entity %u: Attack target is dead or invalid, reverting to MOVE_TO_OBJECTIVE", 
                      entity->get_id());
        }
    }
}