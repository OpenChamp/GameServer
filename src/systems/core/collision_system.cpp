#include <systems/core/collision_system.hpp>
#include <systems/entity_manager.hpp>
#include <components/movement.hpp>
#include <libs/log.hpp>
#include <cmath>

void CollisionSystem::update(const SystemContext& ctx) {
    // Get all entities with movement components
    auto moving_entities = ctx.entity_manager.get_entities_with_component<Movement>();
    
    // Check all pairs of entities for collisions
    for (size_t i = 0; i < moving_entities.size(); ++i) {
        Entity* entity_a = moving_entities[i];
        Movement* move_a = entity_a->get_component<Movement>();
        
        if (!move_a) continue;
        
        float radius_a = move_a->collision_radius;
        
        // Check against all other entities
        for (size_t j = i + 1; j < moving_entities.size(); ++j) {
            Entity* entity_b = moving_entities[j];
            Movement* move_b = entity_b->get_component<Movement>();
            
            if (!move_b) continue;
            
            float radius_b = move_b->collision_radius;
            float min_distance = radius_a + radius_b;
            
            float actual_distance = distance(move_a->position, move_b->position);
            
            // If entities are overlapping, push them apart
            if (actual_distance < min_distance) {
                // Calculate separation direction
                Vec2 separation = (move_b->position - move_a->position);
                if (actual_distance > 0.001f) {
                    separation = separation.normalized();
                } else {
                    // Fallback if entities are at exact same position
                    separation = Vec2(1.0f, 0.0f);
                }
                
                float overlap = min_distance - actual_distance;
                float push_per_entity = overlap / 2.0f + 0.001f;  // Split push equally, add small buffer
                
                // Push both entities apart
                move_a->position = move_a->position - (separation * push_per_entity);
                move_b->position = move_b->position + (separation * push_per_entity);
                
                LOG_DEBUG("Separated entities %u and %u by %.3f units", 
                         entity_a->get_id(), entity_b->get_id(), overlap);
            }
        }
    }
}

bool CollisionSystem::would_collide(const Entity& entity, const Vec2& proposed_position, EntityManager& entity_manager) {
    const Movement* entity_move = entity.get_component<Movement>();
    if (!entity_move) {
        return false;
    }
    
    float entity_radius = entity_move->collision_radius;
    
    // Check collision with all other moving entities
    auto moving_entities = entity_manager.get_entities_with_component<Movement>();
    for (auto* other_entity : moving_entities) {
        if (other_entity->get_id() == entity.get_id()) {
            continue;  // Skip self
        }
        
        const Movement* other_move = other_entity->get_component<Movement>();
        if (!other_move) {
            continue;
        }
        
        float other_radius = other_move->collision_radius;
        float min_distance = entity_radius + other_radius;
        
        float actual_distance = distance(proposed_position, other_move->position);
        
        if (actual_distance < min_distance) {
            return true;  // Collision detected
        }
    }
    
    return false;  // No collision
}

Entity* CollisionSystem::get_colliding_entity(const Entity& entity, const Vec2& proposed_position, EntityManager& entity_manager) {
    const Movement* entity_move = entity.get_component<Movement>();
    if (!entity_move) {
        return nullptr;
    }
    
    float entity_radius = entity_move->collision_radius;
    
    // Check collision with all other moving entities
    auto moving_entities = entity_manager.get_entities_with_component<Movement>();
    for (auto* other_entity : moving_entities) {
        if (other_entity->get_id() == entity.get_id()) {
            continue;  // Skip self
        }
        
        const Movement* other_move = other_entity->get_component<Movement>();
        if (!other_move) {
            continue;
        }
        
        float other_radius = other_move->collision_radius;
        float min_distance = entity_radius + other_radius;
        
        float actual_distance = distance(proposed_position, other_move->position);
        
        if (actual_distance < min_distance) {
            return other_entity;  // Return first colliding entity
        }
    }
    
    return nullptr;  // No collision
}

void CollisionSystem::push_entity(Entity& pushed_entity, const Vec2& push_direction, float push_distance) {
    Movement* move = pushed_entity.get_component<Movement>();
    if (!move) {
        return;
    }
    
    move->position = move->position + (push_direction * push_distance);
    LOG_DEBUG("Pushed entity %u by %.3f units", pushed_entity.get_id(), push_distance);
}

void CollisionSystem::push_colliding_entities(const Entity& entity, const Vec2& proposed_position, EntityManager& entity_manager) {
    const Movement* entity_move = entity.get_component<Movement>();
    if (!entity_move) {
        return;
    }
    
    float entity_radius = entity_move->collision_radius;
    Vec2 current_pos = entity_move->position;
    Vec2 push_direction = (proposed_position - current_pos);
    
    if (push_direction.length() > 0.001f) {
        push_direction = push_direction.normalized();
    } else {
        return;  // No movement, nothing to push
    }
    
    // Find and push all colliding entities
    auto moving_entities = entity_manager.get_entities_with_component<Movement>();
    for (auto* other_entity : moving_entities) {
        if (other_entity->get_id() == entity.get_id()) {
            continue;  // Skip self
        }
        
        Movement* other_move = other_entity->get_component<Movement>();
        if (!other_move) {
            continue;
        }
        
        float other_radius = other_move->collision_radius;
        float min_distance = entity_radius + other_radius;
        
        // Check if this entity is colliding with the proposed position
        float actual_distance = distance(proposed_position, other_move->position);
        
        if (actual_distance < min_distance) {
            // Push the other entity in the direction of movement
            float push_distance = min_distance - actual_distance + 0.01f;  // Small buffer to separate them
            push_entity(*other_entity, push_direction, push_distance);
        }
    }
}

float CollisionSystem::distance(const Vec2& a, const Vec2& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}
