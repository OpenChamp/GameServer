#include <systems/core/attack_execution_system.hpp>
#include <systems/util/serialization_system.hpp>

#include <libs/log.hpp>

#include <components/auto_attack.hpp>
#include <components/entity_state.hpp>

#include <services/network_service.hpp>

void AttackExecutionSystem::update(const SystemContext& ctx) {
    if (!combat_system_) {
        LOG_WARN("AttackExecutionSystem: combat_system not set!");
        return;
    }

    // Process all entities with pending attacks
    auto attacking_entities = ctx.entity_manager.get_entities_with_component<AttackComponent>();
    for (auto* entity : attacking_entities) {
        if (entity) {
            auto* attack = entity->get_component<AttackComponent>();
            if (attack && attack->attack_in_progress) {
                execute_attack(ctx, *entity, *attack, ctx.delta_time_ms);
            }
        }
    }
}

bool AttackExecutionSystem::execute_attack(const SystemContext& ctx, Entity& entity,
                                          AttackComponent& attack, float delta_time_ms) {
    // Get auto-attack component for hit timing
    auto* auto_attack = entity.get_component<AutoAttackComponent>();
    if (!auto_attack) {
        auto_attack = nullptr;  // Optional component
    }

    float hit_timing_percent = auto_attack ? auto_attack->hit_timing_percent : 0.5f;
    float animation_duration = attack.attack_animation_duration_ms > 0.0f 
        ? attack.attack_animation_duration_ms 
        : 300.0f;

    // Store old progress to detect threshold crossing
    float old_progress = attack.attack_animation_progress;

    // Update animation progress
    attack.attack_animation_progress += delta_time_ms / animation_duration;

    // Check if we should apply damage (crossed the hit timing threshold)
    bool damage_applied = false;
    if (old_progress < hit_timing_percent && attack.attack_animation_progress >= hit_timing_percent) {
        // It's time to apply damage!
        // First check if attacker is still alive
        auto* attacker_stats = entity.get_component<Stats>();
        if (attacker_stats && attacker_stats->health > 0.0f && attack.pending_target != INVALID_ENTITY_ID) {
            auto damage_events = combat_system_->apply_damage(
                ctx.entity_manager,
                entity.get_id(),
                attack.pending_target,
                attack.pending_damage,
                attack.pending_damage_type
            );

            if (!damage_events.empty()) {
                LOG_INFO("Attack executed: entity %u dealt %.1f damage to entity %u",
                        entity.get_id(), damage_events[0].damage_dealt, attack.pending_target);
                
                // Broadcast combat event to all clients -- might need to be moved later -- cmkrist 24/11/2025
                if (ctx.network_service) {
                    const auto& event = damage_events[0];
                    auto combat_packet = SerializationSystem::serialize_combat_event(
                        entity.get_id(),
                        attack.pending_target,
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
    if (attack.attack_animation_progress >= 1.0f) {
        attack.attack_animation_progress = 1.0f;
        attack.attack_in_progress = false;
        
        // Clear pending attack data
        attack.pending_damage = 0.0f;
        attack.pending_target = INVALID_ENTITY_ID;
        attack.pending_damage_type = DamageType::PHYSICAL;
    }

    return damage_applied;
}

bool AttackExecutionSystem::is_damage_apply_time(float attack_progress, float hit_timing_percent) const {
    // Damage applies when we reach or pass the hit timing threshold
    return attack_progress >= hit_timing_percent;
}
