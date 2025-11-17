#include "combat_system.hpp"
#include "components/movement.hpp"
#include "components/entity_state.hpp"
#include "components/network_entity.hpp"
#include <log.hpp>
#include <cmath>
#include <random>
#include <algorithm>

void CombatSystem::update(const SystemContext& ctx) {
    // Process all entities with auto-attack behavior
    auto attacking_entities = ctx.entity_manager.get_entities_with_component<AutoAttackComponent>();
    for (auto* entity : attacking_entities) {
        if (entity) {
            update_auto_attack(ctx, *entity, ctx.delta_time_ms);
        }
    }
}

std::vector<CombatSystem::DamageEvent> CombatSystem::apply_damage(
    EntityManager& entity_manager,
    EntityID attacker_id,
    EntityID target_id,
    float base_damage,
    DamageType damage_type)
{
    std::vector<DamageEvent> events;
    
    // Get target entity and stats
    Entity* target_entity = entity_manager.get_entity(target_id);
    if (!target_entity || !target_entity->has_component<Stats>()) {
        LOG_WARN("Combat: Target entity (ID %u) not found or has no Stats component", target_id);
        return events;
    }
    
    Stats* target_stats = target_entity->get_component<Stats>();
    
    // Get attacker entity and stats (for lifesteal/spell vamp)
    Entity* attacker_entity = entity_manager.get_entity(attacker_id);
    Stats* attacker_stats = nullptr;
    if (attacker_entity) {
        attacker_stats = attacker_entity->get_component<Stats>();
    }
    
    // Calculate damage reduction
    float reduced_damage = base_damage;
    bool was_critical = false;
    
    if (damage_type == DamageType::PHYSICAL) {
        reduced_damage = calculate_damage_reduction(base_damage, target_stats->armor);
    } else if (damage_type == DamageType::MAGICAL) {
        reduced_damage = calculate_damage_reduction(base_damage, target_stats->magic_resist);
    }
    // TRUE_DAMAGE ignores all resistances
    
    // Apply critical hit multiplier from attacker
    if (attacker_stats) {
        float crit_multiplier = calculate_critical_hit(*attacker_stats);
        was_critical = crit_multiplier > 1.0f;
        reduced_damage *= crit_multiplier;
    }
    
    // Apply dodge chance
    if (std::rand() % 100 < target_stats->dodge) {
        LOG_INFO("Combat: Entity (ID %u) dodged attack from entity (ID %u)", target_id, attacker_id);
        DamageEvent event;
        event.attacker_id = attacker_id;
        event.target_id = target_id;
        event.damage_dealt = 0.0f;
        event.damage_raw = base_damage;
        event.damage_type = damage_type;
        event.was_critical_hit = false;
        events.push_back(event);
        return events;
    }
    
    // Apply damage to target
    float actual_damage = reduced_damage;
    target_stats->health -= actual_damage;
    
    // Update entity state if dead
    if (target_stats->health <= 0.0f) {
        if (auto* entity_state = target_entity->get_component<EntityStateComponent>()) {
            entity_state->current_state = EntityState::DEAD;
        }
        // Mark network entity as changed so death state syncs to clients
        if (auto* network_entity = target_entity->get_component<NetworkEntityComponent>()) {
            network_entity->force_full_sync_next_frame = true;
        }
    }
    
    // Calculate lifesteal/spell vamp
    if (attacker_stats) {
        float lifesteal_amount = 0.0f;
        
        if (damage_type == DamageType::PHYSICAL) {
            lifesteal_amount = actual_damage * (attacker_stats->life_steal / 100.0f);
        } else if (damage_type == DamageType::MAGICAL) {
            lifesteal_amount = actual_damage * (attacker_stats->spell_vamp / 100.0f);
        }
        
        // Omni vamp applies to all damage types
        lifesteal_amount += actual_damage * (attacker_stats->omni_vamp / 100.0f);
        
        if (lifesteal_amount > 0.0f) {
            attacker_stats->health = std::min(attacker_stats->health + lifesteal_amount, attacker_stats->max_health);
        }
        
        // Apply leech (mana restoration)
        float leech_amount = actual_damage * (attacker_stats->leech / 100.0f);
        if (leech_amount > 0.0f) {
            attacker_stats->mana = std::min(attacker_stats->mana + leech_amount, attacker_stats->max_mana);
        }
    }
    
    // Log damage event
    std::string crit_str = was_critical ? " (CRITICAL!)" : "";
    std::string damage_type_str = 
        (damage_type == DamageType::PHYSICAL) ? "PHYSICAL" :
        (damage_type == DamageType::MAGICAL) ? "MAGICAL" : "TRUE";
    
    LOG_INFO("Combat: Entity (ID %u) dealt %.1f %s damage to entity (ID %u) (health: %.1f/%.1f)%s",
             attacker_id, actual_damage, damage_type_str.c_str(), target_id,
             std::max(0.0f, target_stats->health), target_stats->max_health, crit_str.c_str());
    
    // Create damage event
    DamageEvent event;
    event.attacker_id = attacker_id;
    event.target_id = target_id;
    event.damage_dealt = actual_damage;
    event.damage_raw = base_damage;
    event.damage_type = damage_type;
    event.was_critical_hit = was_critical;
    events.push_back(event);
    
    return events;
}

bool CombatSystem::apply_heal(
    EntityManager& entity_manager,
    EntityID target_id,
    float heal_amount)
{
    Entity* target_entity = entity_manager.get_entity(target_id);
    if (!target_entity || !target_entity->has_component<Stats>()) {
        return false;
    }
    
    Stats* target_stats = target_entity->get_component<Stats>();
    float old_health = target_stats->health;
    target_stats->health = std::min(target_stats->health + heal_amount, target_stats->max_health);
    
    float actual_heal = target_stats->health - old_health;
    LOG_INFO("Combat: Entity (ID %u) healed for %.1f health (health: %.1f/%.1f)",
             target_id, actual_heal, target_stats->health, target_stats->max_health);
    
    return true;
}

bool CombatSystem::restore_mana(
    EntityManager& entity_manager,
    EntityID target_id,
    float mana_amount)
{
    Entity* target_entity = entity_manager.get_entity(target_id);
    if (!target_entity || !target_entity->has_component<Stats>()) {
        return false;
    }
    
    Stats* target_stats = target_entity->get_component<Stats>();
    float old_mana = target_stats->mana;
    target_stats->mana = std::min(target_stats->mana + mana_amount, target_stats->max_mana);
    
    float actual_restore = target_stats->mana - old_mana;
    LOG_INFO("Combat: Entity (ID %u) restored %.1f mana (mana: %.1f/%.1f)",
             target_id, actual_restore, target_stats->mana, target_stats->max_mana);
    
    return true;
}

bool CombatSystem::is_dead(
    EntityManager& entity_manager,
    EntityID entity_id) const
{
    Entity* entity = entity_manager.get_entity(entity_id);
    if (!entity || !entity->has_component<Stats>()) {
        return true;
    }
    
    const Stats* stats = entity->get_component<Stats>();
    return stats->health <= 0.0f;
}

float CombatSystem::calculate_damage_reduction(float base_damage, int resistance) const
{
    // Damage reduction formula: damage * (100 / (100 + resistance))
    // This ensures that 100 resistance reduces damage by 50%, 200 resistance by 66%, etc.
    if (resistance < 0) {
        // Negative resistance increases damage taken
        return base_damage * (100.0f / (100.0f + std::abs(resistance)));
    }
    return base_damage * (100.0f / (100.0f + resistance));
}

float CombatSystem::calculate_critical_hit(const Stats& attacker)
{
    // Random number between 0 and 99
    int roll = std::rand() % 100;
    
    if (roll < attacker.crit_chance) {
        // Critical hit! Apply crit bonus
        // Bonus format: 100 = +100% damage = 2x multiplier
        return 1.0f + (attacker.crit_bonus / 100.0f);
    }
    
    return 1.0f;
}

void CombatSystem::update_auto_attack(const SystemContext& ctx, Entity& entity, float delta_time_ms) {
    auto* auto_attack = entity.get_component<AutoAttackComponent>();
    auto* attack = entity.get_component<AttackComponent>();
    auto* target = entity.get_component<TargetComponent>();
    auto* stats = entity.get_component<Stats>();
    
    if (!auto_attack || !attack || !target || !stats) {
        return;  // Missing required components
    }
    
    // Update combat timer
    auto_attack->update_combat_timer(delta_time_ms);
    
    // Update attack cooldown
    update_attack_cooldown(*attack, delta_time_ms);
    
    // Don't process if auto-attacking disabled
    if (!auto_attack->should_auto_attack()) {
        return;
    }
    
    // Search for new target if needed
    if (!target->is_target_valid() && auto_attack->should_search_targets()) {
        if (target->should_search_for_target(0.0f)) {  // TODO: Pass actual game time
            EntityID new_target = find_best_target(ctx, entity, *auto_attack, *target);
            if (new_target != INVALID_ENTITY_ID) {
                target->set_target(new_target);
                LOG_INFO("Entity (ID %u) acquired new target: %u", entity.get_id(), new_target);
            }
            target->mark_target_searched(0.0f);
        }
    }
    
    // If we have a valid target and are ready to attack
    if (target->is_target_valid() && is_attack_ready(*attack)) {
        Entity* target_entity = ctx.entity_manager.get_entity(target->current_target);
        
        if (target_entity && !is_dead(ctx.entity_manager, target->current_target)) {
            // Calculate distance to target
            auto* target_move = target_entity->get_component<Movement>();
            auto* my_move = entity.get_component<Movement>();
            
            if (target_move && my_move) {
                float dx = target_move->position.x - my_move->position.x;
                float dy = target_move->position.y - my_move->position.y;
                float distance = std::sqrt(dx * dx + dy * dy);
                
                target->update_target_distance(distance, stats->attack_range);
                
                // Attack if in range
                if (target->target_in_range) {
                    // Begin attack animation
                    begin_attack_animation(*attack);
                    
                    // Calculate damage
                    float base_damage = stats->physical_power + auto_attack->bonus_damage;
                    base_damage *= auto_attack->base_damage_multiplier;
                    
                    // Apply damage
                    auto damage_events = apply_damage(ctx.entity_manager, entity.get_id(),
                                                     target->current_target, base_damage,
                                                     auto_attack->damage_type);
                    
                    if (!damage_events.empty()) {
                        // Start cooldown
                        start_attack_cooldown(*attack, stats->attack_speed);
                        auto_attack->on_attack();
                        
                        LOG_INFO("Entity (ID %u) auto-attacked target (ID %u) for %.1f damage",
                                entity.get_id(), target->current_target, damage_events[0].damage_dealt);
                    }
                }
            }
        } else {
            // Target is dead or invalid, clear it
            target->clear_target();
        }
    }
}

EntityID CombatSystem::find_best_target(const SystemContext& ctx, const Entity& attacker,
                                       const AutoAttackComponent& auto_attack_comp,
                                       TargetComponent& target_comp) const {
    if (!attacker.has_component<Movement>() || !attacker.has_component<Stats>()) {
        return INVALID_ENTITY_ID;
    }
    
    const auto* attacker_move = attacker.get_component<Movement>();
    const auto* attacker_stats = attacker.get_component<Stats>();
    
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

bool CombatSystem::is_valid_target(const SystemContext& ctx, const Entity& attacker,
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

void CombatSystem::update_attack_cooldown(AttackComponent& attack, float delta_time_ms) const {
    if (attack.attack_cooldown_ms > 0.0f) {
        attack.attack_cooldown_ms -= delta_time_ms;
        if (attack.attack_cooldown_ms < 0.0f) {
            attack.attack_cooldown_ms = 0.0f;
            attack.can_attack = true;
        }
    }
}

void CombatSystem::start_attack_cooldown(AttackComponent& attack, float attack_speed_stat) const {
    if (attack_speed_stat > 0.0f) {
        attack.attack_cooldown_ms = (1000.0f / attack_speed_stat);  // Convert to milliseconds
    } else {
        attack.attack_cooldown_ms = 1000.0f;  // Default 1 second
    }
    attack.can_attack = false;
    attack.attacks_this_frame = 0;
}

bool CombatSystem::is_attack_ready(const AttackComponent& attack) const {
    return attack.attack_cooldown_ms <= 0.0f && attack.can_attack;
}

void CombatSystem::begin_attack_animation(AttackComponent& attack) const {
    attack.attack_in_progress = true;
    attack.attack_animation_progress = 0.0f;
    attack.attacks_this_frame++;
    attack.total_attacks++;
}

bool CombatSystem::update_attack_animation(AttackComponent& attack, float delta_time_ms, float animation_duration_ms) const {
    if (!attack.attack_in_progress) return false;
    
    attack.attack_animation_progress += delta_time_ms / animation_duration_ms;
    
    if (attack.attack_animation_progress >= 1.0f) {
        attack.attack_animation_progress = 1.0f;
        attack.attack_in_progress = false;
        return true;  // Attack complete
    }
    
    return false;
}
