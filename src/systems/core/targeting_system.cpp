#include <systems/core/targeting_system.hpp>
#include <libs/log.hpp>
#include <cmath>
#include <algorithm>
#include <limits>

std::vector<TargetingSystem::TargetingEvent> TargetingSystem::update(
    EntityManager& entity_manager,
    float delta_time)
{
    std::vector<TargetingEvent> events;
    
    // Get all entities in the manager
    const auto& all_entities = entity_manager.get_all_entities();
    
    // Process each entity that has AutoAttack component
    for (const auto& entity_pair : all_entities) {
        Entity* entity = entity_pair.second.get();
        if (!entity || !entity->has_component<AutoAttack>()) {
            continue;
        }
        
        // Process auto-attack for this entity
        auto entity_events = process_auto_attack(entity_manager, entity->get_id(), delta_time);
        events.insert(events.end(), entity_events.begin(), entity_events.end());
    }
    
    return events;
}

std::vector<EntityID> TargetingSystem::get_targets_in_range(
    EntityManager& entity_manager,
    EntityID entity_id,
    bool include_allies) const
{
    std::vector<EntityID> targets;
    
    Entity* observer = entity_manager.get_entity(entity_id);
    if (!observer || !observer->has_component<Movement>() || !observer->has_component<Stats>()) {
        return targets;
    }
    
    Movement* observer_movement = observer->get_component<Movement>();
    Stats* observer_stats = observer->get_component<Stats>();
    
    const auto& all_entities = entity_manager.get_all_entities();
    
    for (const auto& entity_pair : all_entities) {
        Entity* potential_target = entity_pair.second.get();
        
        // Skip self
        if (potential_target->get_id() == entity_id) {
            continue;
        }
        
        // Skip entities without movement or stats
        if (!potential_target->has_component<Movement>() || !potential_target->has_component<Stats>()) {
            continue;
        }
        
        // Skip if not in vision range
        Movement* target_movement = potential_target->get_component<Movement>();
        if (!is_in_vision_range(observer_movement->position, target_movement->position, observer_stats->vision_range)) {
            continue;
        }
        
        // Skip if not eligible for combat
        if (!can_engage_in_combat(entity_manager, entity_id, potential_target->get_id())) {
            continue;
        }
        
        targets.push_back(potential_target->get_id());
    }
    
    return targets;
}

bool TargetingSystem::is_in_attack_range(
    const Vec3& attacker_position,
    const Vec3& target_position,
    float attack_range) const
{
    float distance = calculate_distance(attacker_position, target_position);
    return distance <= attack_range;
}

bool TargetingSystem::is_in_vision_range(
    const Vec3& observer_position,
    const Vec3& target_position,
    float vision_range) const
{
    float distance = calculate_distance(observer_position, target_position);
    return distance <= vision_range;
}

bool TargetingSystem::acquire_target(
    EntityManager& entity_manager,
    EntityID entity_id,
    EntityID target_id)
{
    Entity* entity = entity_manager.get_entity(entity_id);
    if (!entity || !entity->has_component<AutoAttack>()) {
        return false;
    }
    
    // Validate target exists
    Entity* target = entity_manager.get_entity(target_id);
    if (!target) {
        return false;
    }
    
    // Validate can engage in combat
    if (!can_engage_in_combat(entity_manager, entity_id, target_id)) {
        return false;
    }
    
    AutoAttack* auto_attack = entity->get_component<AutoAttack>();
    auto_attack->target_id = target_id;
    auto_attack->is_attacking = true;
    
    LOG_INFO("Targeting: Entity (ID %u) acquired target (ID %u)", entity_id, target_id);
    return true;
}

bool TargetingSystem::release_target(
    EntityManager& entity_manager,
    EntityID entity_id)
{
    Entity* entity = entity_manager.get_entity(entity_id);
    if (!entity || !entity->has_component<AutoAttack>()) {
        return false;
    }
    
    AutoAttack* auto_attack = entity->get_component<AutoAttack>();
    if (auto_attack->target_id == INVALID_TARGET_ID) {
        return false;
    }
    
    EntityID old_target = auto_attack->target_id;
    auto_attack->target_id = INVALID_TARGET_ID;
    auto_attack->is_attacking = false;
    
    LOG_INFO("Targeting: Entity (ID %u) released target (ID %u)", entity_id, old_target);
    return true;
}

bool TargetingSystem::can_engage_in_combat(
    EntityManager& entity_manager,
    EntityID entity_id,
    EntityID target_id) const
{
    Entity* entity = entity_manager.get_entity(entity_id);
    Entity* target = entity_manager.get_entity(target_id);
    
    if (!entity || !target) {
        return false;
    }
    
    // Both must have Stats and Movement for combat
    if (!entity->has_component<Stats>() || !target->has_component<Stats>()) {
        return false;
    }
    
    if (!entity->has_component<Movement>() || !target->has_component<Movement>()) {
        return false;
    }
    
    // Check if target is alive
    Stats* target_stats = target->get_component<Stats>();
    if (target_stats->health <= 0.0f) {
        return false;
    }
    
    // Check if attacker is alive
    Stats* entity_stats = entity->get_component<Stats>();
    if (entity_stats->health <= 0.0f) {
        return false;
    }
    
    // TODO: Add team/faction checks here when team system is implemented
    // For now, all minions can attack each other
    
    return true;
}

bool TargetingSystem::is_target_valid(
    EntityManager& entity_manager,
    EntityID entity_id,
    EntityID target_id) const
{
    // Validate entity still exists
    Entity* entity = entity_manager.get_entity(entity_id);
    if (!entity || !entity->has_component<AutoAttack>()) {
        return false;
    }
    
    // Check if we can still engage in combat
    if (!can_engage_in_combat(entity_manager, entity_id, target_id)) {
        return false;
    }
    
    // Check if target is still in vision range
    Entity* target = entity_manager.get_entity(target_id);
    if (!target || !target->has_component<Movement>()) {
        return false;
    }
    
    Movement* entity_movement = entity->get_component<Movement>();
    Movement* target_movement = target->get_component<Movement>();
    Stats* entity_stats = entity->get_component<Stats>();
    
    if (!is_in_vision_range(entity_movement->position, target_movement->position, entity_stats->vision_range)) {
        return false;
    }
    
    return true;
}

EntityID TargetingSystem::get_closest_target(
    EntityManager& entity_manager,
    EntityID entity_id) const
{
    std::vector<EntityID> targets = get_targets_in_range(entity_manager, entity_id);
    
    if (targets.empty()) {
        return INVALID_ENTITY_ID;
    }
    
    Entity* entity = entity_manager.get_entity(entity_id);
    if (!entity || !entity->has_component<Movement>()) {
        return INVALID_ENTITY_ID;
    }
    
    Movement* entity_movement = entity->get_component<Movement>();
    
    EntityID closest_id = targets[0];
    float closest_distance = std::numeric_limits<float>::max();
    
    for (EntityID target_id : targets) {
        Entity* target = entity_manager.get_entity(target_id);
        if (!target || !target->has_component<Movement>()) {
            continue;
        }
        
        Movement* target_movement = target->get_component<Movement>();
        float distance = calculate_distance(entity_movement->position, target_movement->position);
        
        if (distance < closest_distance) {
            closest_distance = distance;
            closest_id = target_id;
        }
    }
    
    return closest_id;
}

float TargetingSystem::calculate_distance(const Vec3& pos1, const Vec3& pos2) const
{
    Vec3 delta = pos1 - pos2;
    return std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
}

std::vector<TargetingSystem::TargetingEvent> TargetingSystem::process_auto_attack(
    EntityManager& entity_manager,
    EntityID entity_id,
    float delta_time)
{
    std::vector<TargetingEvent> events;
    
    Entity* entity = entity_manager.get_entity(entity_id);
    if (!entity) {
        return events;
    }
    
    AutoAttack* auto_attack = entity->get_component<AutoAttack>();
    if (!auto_attack || !auto_attack->auto_attack_enabled) {
        return events;
    }
    
    // Update attack cooldown
    if (auto_attack->attack_cooldown > 0.0f) {
        auto_attack->attack_cooldown -= delta_time;
    }
    
    // Check current target validity
    if (auto_attack->target_id != INVALID_TARGET_ID) {
        if (!is_target_valid(entity_manager, entity_id, auto_attack->target_id)) {
            // Target became invalid, release it
            release_target(entity_manager, entity_id);
            TargetingEvent event;
            event.entity_id = entity_id;
            event.target_id = INVALID_TARGET_ID;
            event.acquired = false;
            events.push_back(event);
        }
    }
    
    // If no valid target, try to find one
    if (auto_attack->target_id == INVALID_TARGET_ID && auto_attack->auto_attack_enabled) {
        EntityID new_target = find_new_target(entity_manager, entity_id);
        if (new_target != INVALID_ENTITY_ID) {
            acquire_target(entity_manager, entity_id, new_target);
            TargetingEvent event;
            event.entity_id = entity_id;
            event.target_id = new_target;
            event.acquired = true;
            events.push_back(event);
        }
    }
    
    return events;
}

EntityID TargetingSystem::find_new_target(
    EntityManager& entity_manager,
    EntityID entity_id)
{
    Entity* entity = entity_manager.get_entity(entity_id);
    if (!entity) {
        return INVALID_ENTITY_ID;
    }
    
    // Get closest target within vision range
    EntityID closest_target = get_closest_target(entity_manager, entity_id);
    
    if (closest_target != INVALID_ENTITY_ID) {
        LOG_DEBUG("Targeting: Entity (ID %u) found new target (ID %u)", entity_id, closest_target);
    }
    
    return closest_target;
}
