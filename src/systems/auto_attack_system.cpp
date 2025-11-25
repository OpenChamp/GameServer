#include "auto_attack_system.hpp"
#include "components/movement.hpp"
#include "components/entity_state.hpp"
#include "components/network_entity.hpp"
#include <log.hpp>
#include <cmath>
#include <random>
#include <algorithm>

void AutoAttackSystem::update(const SystemContext& ctx) {
    // Process all entities with auto-attack behavior
    auto attacking_entities = ctx.entity_manager.get_entities_with_component<AutoAttackComponent>();
    for (auto* entity : attacking_entities) {
        if (entity) {
            update_auto_attack(ctx, *entity, ctx.delta_time_ms);
        }
    }
}

void AutoAttackSystem::update_auto_attack(const SystemContext& ctx, Entity& entity, float delta_time_ms) {
    auto* auto_attack = entity.get_component<AutoAttackComponent>();
    auto* attack = entity.get_component<AttackComponent>();
    auto* target = entity.get_component<TargetComponent>();
    auto* stats = entity.get_component<Stats>();
    auto* entity_state = entity.get_component<EntityStateComponent>();
    
    if (!auto_attack || !attack || !target || !stats) {
        return;  // Missing required components
    }
    
    // Don't process if entity is dead
    if (is_dead(entity)) {
        if (target->is_target_valid()) {
            target->clear_target();
        }
        return;
    }
    
    // Update combat timer
    auto_attack->update_combat_timer(delta_time_ms);
    
    // Update attack cooldown
    update_attack_cooldown(*attack, delta_time_ms);
    
    // Don't process if auto-attacking disabled
    if (!auto_attack->should_auto_attack()) {
        // If we had a target but auto-attack is disabled, clear it
        if (target->is_target_valid()) {
            target->clear_target();
        }
        return;
    }
    
    // Search for new target if needed
    if (!target->is_target_valid() && auto_attack->should_search_targets()) {
        // Search for target on first search (last_target_search_ms == 0) or after interval has passed
        if (target->last_target_search_ms == 0.0f || (auto_attack->time_since_attack_ms >= target->target_search_interval_ms)) {
            EntityID new_target = find_best_target(ctx, entity, *auto_attack, *target);
            if (new_target != INVALID_ENTITY_ID) {
                target->set_target(new_target);
                // Tell entity_state who to move toward via target_entity_id
                if (entity_state) {
                    entity_state->target_entity_id = new_target;
                }
                LOG_INFO("Entity (ID %u) acquired new target: %u", entity.get_id(), new_target);
                
                // Update distance immediately so target validation works
                Entity* target_entity = ctx.entity_manager.get_entity(new_target);
                if (target_entity) {
                    auto* target_move = target_entity->get_component<Movement>();
                    auto* my_move = entity.get_component<Movement>();
                    if (target_move && my_move) {
                        float dx = target_move->position.x - my_move->position.x;
                        float dy = target_move->position.y - my_move->position.y;
                        float distance = std::sqrt(dx * dx + dy * dy);
                        target->update_target_distance(distance, stats->attack_range);
                    }
                }
            }
            // Mark search as done - reset timer
            target->last_target_search_ms = auto_attack->time_since_attack_ms;
        }
    }
    
    // Process valid targets
    if (target->is_target_valid()) {
        Entity* target_entity = ctx.entity_manager.get_entity(target->current_target);
        
        if (target_entity && !is_dead(*target_entity)) {
            // Calculate distance to target
            auto* target_move = target_entity->get_component<Movement>();
            auto* my_move = entity.get_component<Movement>();
            
            if (target_move && my_move) {
                float dx = target_move->position.x - my_move->position.x;
                float dy = target_move->position.y - my_move->position.y;
                float distance = std::sqrt(dx * dx + dy * dy);
                
                target->update_target_distance(distance, stats->attack_range);
                
                // Attack if in range and ready
                if (target->target_in_range && is_attack_ready(*attack)) {
                    // Begin attack animation
                    begin_attack_animation(*attack);
                    
                    // Transition to ATTACKING state - movement system will handle kiting
                    if (entity_state && entity_state->current_state != EntityState::ATTACKING) {
                        entity_state->current_state = EntityState::ATTACKING;
                        entity_state->state_duration_ms = 0.0f;
                    }
                    
                    // Calculate damage
                    float base_damage = stats->physical_power + auto_attack->bonus_damage;
                    base_damage *= auto_attack->base_damage_multiplier;
                    
                    // Store attack data for CombatSystem to process
                    // (CombatSystem will call apply_damage when appropriate)
                    attack->pending_damage = base_damage;
                    attack->pending_damage_type = auto_attack->damage_type;
                    attack->pending_target = target->current_target;
                    attack->attack_animation_duration_ms = auto_attack->attack_animation_duration_ms;
                    
                    // Start cooldown
                    start_attack_cooldown(*attack, stats->attack_speed);
                    auto_attack->on_attack();
                    
                    LOG_INFO("Entity (ID %u) initiated auto-attack on target (ID %u)",
                            entity.get_id(), target->current_target);
                } else if (!target->target_in_range) {
                    // Out of range - MovementSystem will handle approaching via HOLDING_FOR_TARGET
                    // Just update the target_entity_id so it knows who to move toward
                    if (entity_state) {
                        entity_state->target_entity_id = target->current_target;
                    }
                }
            }
        } else {
            // Target is dead or invalid, clear it
            // MovementSystem will handle state transitions based on pathfinding
            target->clear_target();
        }
    }
}

EntityID AutoAttackSystem::find_best_target(const SystemContext& ctx, const Entity& attacker,
                                           const AutoAttackComponent& auto_attack_comp,
                                           TargetComponent& target_comp) const {
    if (!attacker.has_component<Movement>() || !attacker.has_component<Stats>()) {
        return INVALID_ENTITY_ID;
    }
    
    const auto* attacker_move = attacker.get_component<Movement>();
    // attacker_stats not needed for find_best_target - only for target validation
    
    EntityID best_target = INVALID_ENTITY_ID;
    float best_score = -1e9f;
    
    // Get all entities with Movement (potential targets)
    auto all_entities = ctx.entity_manager.get_entities_with_component<Movement>();
    
    for (auto* potential_target : all_entities) {
        if (!potential_target || potential_target->get_id() == attacker.get_id()) {
            continue;  // Skip self
        }
        
        // Check if valid target
        if (!is_valid_target(ctx, attacker, *potential_target)) {
            continue;
        }
        
        // Calculate distance
        auto* target_move = potential_target->get_component<Movement>();
        if (!target_move) continue;
        
        float dx = target_move->position.x - attacker_move->position.x;
        float dy = target_move->position.y - attacker_move->position.y;
        float distance = std::sqrt(dx * dx + dy * dy);
        
        // Skip if out of aggro range
        if (distance > auto_attack_comp.aggression_range) {
            continue;
        }
        
        // Calculate target score based on targeting mode
        float score = 0.0f;
        
        auto* target_stats = potential_target->get_component<Stats>();
        
        switch (target_comp.targeting_mode) {
            case TargetComponent::TargetPriority::NEAREST:
                score = -distance;  // Negative distance = higher score for closer
                break;
                
            case TargetComponent::TargetPriority::LOWEST_HEALTH:
                if (target_stats) {
                    score = -(target_stats->health);  // Negative health = target low HP
                }
                break;
                
            case TargetComponent::TargetPriority::HIGHEST_THREAT:
                if (target_stats) {
                    score = target_stats->physical_power + target_stats->magic_power;
                }
                break;
                
            case TargetComponent::TargetPriority::HIGHEST_DAMAGE:
                if (target_stats) {
                    score = target_stats->physical_power;
                }
                break;
                
            default:
                score = -distance;
                break;
        }
        
        // Prefer closer targets as tiebreaker
        if (auto_attack_comp.prioritize_closest) {
            score -= (distance * 0.1f);
        }
        
        if (score > best_score) {
            best_score = score;
            best_target = potential_target->get_id();
        }
    }
    
    return best_target;
}

bool AutoAttackSystem::is_valid_target(const SystemContext& /*ctx*/, const Entity& attacker,
                                       const Entity& potential_target) const {
    // Must have stats
    if (!potential_target.has_component<Stats>()) {
        return false;
    }
    
    const auto* attacker_stats = attacker.get_component<Stats>();
    const auto* target_stats = potential_target.get_component<Stats>();
    
    if (!attacker_stats || !target_stats) {
        return false;
    }
    
    // Can't target dead units
    if (target_stats->health <= 0.0f) {
        return false;
    }
    
    // Can't target neutral observers
    if (target_stats->team_id == 255) {
        return false;
    }
    
    // Can't target same team (unless friendly fire enabled)
    if (attacker_stats->team_id != 0 && target_stats->team_id == attacker_stats->team_id) {
        return false;
    }
    
    // Target must be on opposite team or attacker neutral
    if (attacker_stats->team_id == 0 && target_stats->team_id == 0) {
        return false;  // Can't target neutrals as neutral
    }
    
    return true;
}

bool AutoAttackSystem::is_dead(const Entity& entity) const {
    if (!entity.has_component<Stats>()) {
        return true;
    }
    
    const Stats* stats = entity.get_component<Stats>();
    return stats->health <= 0.0f;
}

void AutoAttackSystem::update_attack_cooldown(AttackComponent& attack, float delta_time_ms) const {
    if (attack.attack_cooldown_ms > 0.0f) {
        attack.attack_cooldown_ms -= delta_time_ms;
        if (attack.attack_cooldown_ms < 0.0f) {
            attack.attack_cooldown_ms = 0.0f;
            attack.can_attack = true;
        }
    }
}

void AutoAttackSystem::start_attack_cooldown(AttackComponent& attack, float attack_speed_stat) const {
    if (attack_speed_stat > 0.0f) {
        attack.attack_cooldown_ms = (1000.0f / attack_speed_stat);  // Convert to milliseconds
    } else {
        attack.attack_cooldown_ms = 1000.0f;  // Default 1 second
    }
    attack.can_attack = false;
    attack.attacks_this_frame = 0;
}

bool AutoAttackSystem::is_attack_ready(const AttackComponent& attack) const {
    return attack.attack_cooldown_ms <= 0.0f && attack.can_attack;
}

void AutoAttackSystem::begin_attack_animation(AttackComponent& attack) const {
    attack.attack_in_progress = true;
    attack.attack_animation_progress = 0.0f;
    attack.attacks_this_frame++;
    attack.total_attacks++;
}

bool AutoAttackSystem::update_attack_animation(AttackComponent& attack, float delta_time_ms, float animation_duration_ms) const {
    if (!attack.attack_in_progress) return false;
    
    attack.attack_animation_progress += delta_time_ms / animation_duration_ms;
    
    if (attack.attack_animation_progress >= 1.0f) {
        attack.attack_animation_progress = 1.0f;
        attack.attack_in_progress = false;
        return true;  // Attack complete
    }
    
    return false;
}
