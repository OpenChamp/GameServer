#pragma once

#include "component.hpp"

enum class DamageType {
    PHYSICAL,
    MAGICAL,
    TRUE_DAMAGE
};

/**
 * Stats component for entities.
 * Contains all gameplay statistics and attributes.
 */
struct Stats : public Component {
    // === Core Stats ===
    float max_health = 1.0f;
    float health = 1.0f;
    float max_mana = 0.0f;
    float mana = 0.0f;
    float move_speed = 0.0f;
    int level = 1;
    
    // === Offensive Stats ===
    float attack_range = 1.0f;
    float attack_speed = 1.0f; 
    DamageType auto_damage_type = DamageType::PHYSICAL; // Default auto-attack damage type
    float crit_chance = 0.0f;
    int crit_bonus = 0;
    int true_bonus = 0;
    float magic_power = 1.0f;
    float physical_power = 1.0f;
    float projectile_speed = 5.0f;
    
    // === Defensive Stats ===
    int armor = 0; // Percentage physical damage reduction
    int magic_resist = 0; // Percentage magic damage reduction
    int dodge = 0; // Percentage chance to dodge
    
    // === Scaling & Utility Stats ===
    float health_regen = 1.0f;
    float mana_regen = 1.0f;
    float life_steal = 0.0f; // Percentage of physical damage dealt returned as health
    float spell_vamp = 0.0f; // Percentage of magic damage dealt returned as health
    float omni_vamp = 0.0f; // Percentage of damage dealt returned as health
    float leech = 0.0f; // Percentage of damage dealt returned as mana
    float vision_range = 1.0f;

    // === Team & Faction ===
    uint8_t team_id = 0; // [0 = Neutral, 1 = Team 1, 2 = Team 2] -- cmkrist 15/11/2025
    
    COMPONENT_TYPE_ID(Stats, 2002)
};
