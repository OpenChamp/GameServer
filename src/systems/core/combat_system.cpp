#include <systems/core/combat_system.hpp>
#include <systems/util/serialization_system.hpp>
#include <systems/util/targeting_utility.hpp>
#include <systems/util/component_utility.hpp>
#include <components/movement.hpp>
#include <components/entity_state.hpp>
#include <components/intent.hpp>
#include <components/network_entity.hpp>
#include <libs/log.hpp>
#include <cmath>
#include <random>
#include <algorithm>
#include <services/network_service.hpp>

void CombatSystem::update(const SystemContext& ctx) {
    // Process all entities with auto-attack behavior
    auto attacking_entities = ctx.entity_manager.get_entities_with_component<AttackComponent>();
    for (auto* entity : attacking_entities) {
        if (entity) {
            // Update auto-attack (target search, cooldown, attack initiation)
            update_auto_attack(ctx, *entity, ctx.delta_time_ms);
        }
    }
}

void CombatSystem::update_auto_attack(const SystemContext& ctx, Entity& entity, float delta_time_ms) {
    auto* attack = entity.get_component<AttackComponent>();
    auto* target = entity.get_component<TargetComponent>();
    auto* stats = entity.get_component<Stats>();
    auto* entity_state = entity.get_component<EntityStateComponent>();
    auto* intent = entity.get_component<IntentComponent>();
    
    if (!attack || !target || !stats) {
        return;  // Missing required components
    }
    
    // Don't process if entity is dead
    if (is_dead(entity)) {
        if (ComponentUtility::is_target_valid(*target)) {
            ComponentUtility::clear_target(*target);
        }
        return;
    }
    
    // Check if entity should initiate an attack
    if (attack-> target.has_value() && attack->can_attack && !attack->attack_in_progress) {
        
        // Get the target entity
        auto* target_entity = ctx.entity_manager.get_entity(intent->target_entity_id);
        auto* target_stats = target_entity ? target_entity->get_component<Stats>() : nullptr;
        
        if (target_stats && target_stats->health > 0.0f && intent->target_entity_id != INVALID_ENTITY_ID) {
            // Calculate damage based on attacker's physical power
            float damage = stats->physical_power * 1.0f;
            
            // Initiate attack
            attack->pending_target = intent->target_entity_id;
            attack->pending_damage = damage;
            attack->pending_damage_type = DamageType::PHYSICAL;
            attack->attack_in_progress = true;
            attack->attack_animation_progress = 0.0f;
            attack->can_attack = false;
            
            LOG_DEBUG("Entity %u: CombatSystem initiating attack on target %u with damage %.1f",
                      entity.get_id(), intent->target_entity_id, damage);
        }
    }

    float hit_timing_percent = attack ? attack->hit_timing_percent : 0.5f;
    float animation_duration = attack->attack_animation_duration_ms > 0.0f 
        ? attack->attack_animation_duration_ms 
        : 300.0f;

    // Store old progress to detect threshold crossing
    float old_progress = attack->attack_animation_progress;

    // Update animation progress
    attack->attack_animation_progress += delta_time_ms / animation_duration;

    // Check if we should apply damage (crossed the hit timing threshold)
    bool damage_applied = false;
    if (old_progress < hit_timing_percent && attack->attack_animation_progress >= hit_timing_percent) {
        // It's time to apply damage!
        // First check if attacker is still alive
        auto* attacker_stats = entity.get_component<Stats>();
        if (attacker_stats && attacker_stats->health > 0.0f && attack->pending_target != INVALID_ENTITY_ID) {
            auto damage_events = combat_calculator_.apply_damage(
                ctx.entity_manager,
                entity.get_id(),
                attack->pending_target,
                attack->pending_damage,
                attack->pending_damage_type
            );

            if (!damage_events.empty()) {
                // Broadcast combat event to all clients
                if (ctx.network_service) {
                    const auto& event = damage_events[0];
                    auto combat_packet = SerializationSystem::serialize_combat_event(
                        entity.get_id(),
                        attack->pending_target,
                        event.damage_dealt,
                        static_cast<uint8_t>(event.damage_type),
                        event.was_critical_hit
                    );
                    ctx.network_service->broadcast_packet(combat_packet);
                }
                damage_applied = true;
            }
        } else if (attacker_stats && attacker_stats->health <= 0.0f) {
            LOG_DEBUG("Cancelling attack: attacker %u is dead", entity.get_id());
        }
    }

    // Check if animation is complete
    if (attack->attack_animation_progress >= 1.0f) {
        attack->attack_animation_progress = 0.0f;
        attack->attack_in_progress = false;
        attack->can_attack = true;
        
        // Clear pending attack data
        attack->pending_damage = 0.0f;
        attack->pending_target = INVALID_ENTITY_ID;
        attack->pending_damage_type = DamageType::PHYSICAL;
    }
}

bool CombatSystem::is_dead(const Entity& entity) const {
    const Stats* stats = entity.get_component<Stats>();
    return stats && stats->health <= 0.0f;
}

bool CombatSystem::is_valid_target(const SystemContext& ctx, const Entity& attacker,
                                  const Entity& potential_target) const {
    return TargetingUtility::can_engage_in_combat(
        const_cast<EntityManager&>(ctx.entity_manager),
        attacker.get_id(),
        potential_target.get_id()
    );
}

void CombatSystem::update_attack_cooldown(AttackComponent& attack, float delta_time_ms) const {
    if (attack.cooldown_timer_ms > 0.0f) {
        attack.cooldown_timer_ms -= delta_time_ms;
        if (attack.cooldown_timer_ms < 0.0f) {
            attack.cooldown_timer_ms = 0.0f;
        }
    }
}

void CombatSystem::start_attack_cooldown(AttackComponent& attack, float attack_speed_stat) const {
    // attack_speed_stat is attacks per second
    if (attack_speed_stat > 0.0f) {
        attack.cooldown_timer_ms = 1000.0f / attack_speed_stat;
    }
}

bool CombatSystem::is_attack_ready(const AttackComponent& attack) const {
    return attack.can_attack && attack.cooldown_timer_ms <= 0.0f;
}

void CombatSystem::begin_attack_animation(AttackComponent& attack) const {
    attack.attack_in_progress = true;
    attack.attack_animation_progress = 0.0f;
}

bool CombatSystem::is_damage_apply_time(float attack_progress, float hit_timing_percent) const {
    // This is now handled in update_auto_attack by comparing old_progress vs new_progress
    return false;
}
