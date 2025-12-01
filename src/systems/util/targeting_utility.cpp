#include <systems/util/targeting_utility.hpp>
#include <components/movement.hpp>
#include <components/stats.hpp>
#include <libs/log.hpp>
#include <cmath>
#include <algorithm>
#include <limits>

std::vector<EntityID> TargetingUtility::get_targets_in_range(
    EntityManager& entity_manager,
    EntityID entity_id,
    bool include_allies)
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
        Entity* potential_target = const_cast<Entity*>(&entity_pair.second);
        
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

bool TargetingUtility::is_in_attack_range(
    const Vec2& attacker_position,
    const Vec2& target_position,
    float attack_range)
{
    float distance = calculate_distance(attacker_position, target_position);
    return distance <= attack_range;
}

bool TargetingUtility::is_in_vision_range(
    const Vec2& observer_position,
    const Vec2& target_position,
    float vision_range)
{
    float distance = calculate_distance(observer_position, target_position);
    return distance <= vision_range;
}

bool TargetingUtility::can_engage_in_combat(
    EntityManager& entity_manager,
    EntityID entity_id,
    EntityID target_id)
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
    // For now, all minions can attack each other (different team_id)
    if (entity_stats->team_id == target_stats->team_id) {
        return false;  // Can't attack same team
    }
    
    return true;
}

bool TargetingUtility::is_target_valid(
    EntityManager& entity_manager,
    EntityID entity_id,
    EntityID target_id)
{
    // Validate entity still exists
    Entity* entity = entity_manager.get_entity(entity_id);
    if (!entity) {
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

EntityID TargetingUtility::get_closest_target(
    EntityManager& entity_manager,
    EntityID entity_id)
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

float TargetingUtility::calculate_distance(const Vec2& pos1, const Vec2& pos2)
{
    float dx = pos1.x - pos2.x;
    float dy = pos1.y - pos2.y;
    return std::sqrt(dx * dx + dy * dy);
}

float TargetingUtility::calculate_distance(const Vec2& pos1, const EntityID& entity_id, EntityManager& entity_manager)
{
    Entity* entity = entity_manager.get_entity(entity_id);
    if (!entity || !entity->has_component<Movement>()) {
        return std::numeric_limits<float>::max();
    }
    
    Movement* entity_movement = entity->get_component<Movement>();
    return calculate_distance(pos1, entity_movement->position);
}

EntityID TargetingUtility::find_best_target(
    EntityManager& entity_manager,
    EntityID attacker_id,
    float aggression_range,
    TargetComponent::TargetPriority targeting_mode,
    bool prioritize_closest)
{
    Entity* attacker = entity_manager.get_entity(attacker_id);
    if (!attacker || !attacker->has_component<Movement>() || !attacker->has_component<Stats>()) {
        return INVALID_ENTITY_ID;
    }
    
    const auto* attacker_move = attacker->get_component<Movement>();
    const auto* attacker_stats = attacker->get_component<Stats>();
    
    EntityID best_target = INVALID_ENTITY_ID;
    float best_score = -1e9f;
    
    // Get all entities with Movement (potential targets)
    auto all_entities = entity_manager.get_entities_with_component<Movement>();
    
    for (auto* potential_target : all_entities) {
        if (!potential_target || potential_target->get_id() == attacker_id) {
            continue;  // Skip self
        }
        
        // Check if valid target (alive, correct team, etc)
        auto* target_stats = potential_target->get_component<Stats>();
        if (!target_stats || target_stats->health <= 0.0f) {
            continue;  // Target must be alive
        }
        
        // Can't target neutral observers (team_id == 255)
        if (target_stats->team_id == 255) {
            continue;
        }
        
        // Team-based targeting validation
        // Can't target same team
        if (attacker_stats->team_id != 0 && target_stats->team_id == attacker_stats->team_id) {
            continue;
        }
        
        // Can't target neutrals as neutral
        if (attacker_stats->team_id == 0 && target_stats->team_id == 0) {
            continue;
        }
        
        // Calculate distance
        auto* target_move = potential_target->get_component<Movement>();
        if (!target_move) continue;
        
        float distance = calculate_distance(attacker_move->position, target_move->position);
        
        // Skip if out of aggro range
        if (distance > aggression_range) {
            continue;
        }
        
        // Calculate target score based on targeting mode
        float score = 0.0f;
        
        switch (targeting_mode) {
            case TargetComponent::TargetPriority::NEAREST:
                score = -distance;  // Negative distance = higher score for closer
                break;
                
            case TargetComponent::TargetPriority::LOWEST_HEALTH:
                score = -(target_stats->health);  // Negative health = target low HP
                break;
                
            case TargetComponent::TargetPriority::HIGHEST_THREAT:
                score = target_stats->physical_power + target_stats->magic_power;
                break;
                
            case TargetComponent::TargetPriority::HIGHEST_DAMAGE:
                score = target_stats->physical_power;
                break;
                
            case TargetComponent::TargetPriority::PRIORITY_TARGET:
                // PRIORITY_TARGET handled elsewhere, default to nearest
                score = -distance;
                break;
                
            default:
                score = -distance;
                break;
        }
        
        // Prefer closer targets as tiebreaker
        if (prioritize_closest) {
            score -= (distance * 0.1f);
        }
        
        if (score > best_score) {
            best_score = score;
            best_target = potential_target->get_id();
        }
    }
    
    return best_target;
}
