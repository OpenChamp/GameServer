#include <systems/util/combat_system.hpp>
#include "components/entity_state.hpp"
#include "components/network_entity.hpp"
#include <log.hpp>
#include <cmath>
#include <random>
#include <algorithm>

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
    
    // Prevent overkill damage on already-dead entities
    if (target_stats->health <= 0.0f) {
        LOG_DEBUG("Combat: Attempting to damage already-dead entity %u, ignoring", target_id);
        return events;
    }
    
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
    
    // Clamp health to never go below 0
    if (target_stats->health < 0.0f) {
        target_stats->health = 0.0f;
    }
    
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
