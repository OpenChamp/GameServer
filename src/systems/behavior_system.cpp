#include "behavior_system.hpp"

#include "services/navigation_service.hpp"
#include "components/entity_state.hpp"

void BehaviorSystem::update(const SystemContext& ctx) {
    auto entities_with_behavior = ctx.entity_manager.get_entities_with_component<BehaviorComponent>();

    for(Entity* entity : entities_with_behavior) {
        BehaviorComponent* behaviorComponent = entity->get_component<BehaviorComponent>();

        std::vector<std::variant<WaypointBehavior, AttackBehavior>>& behaviors = behaviorComponent->brain.behaviors;

        for(std::variant<WaypointBehavior, AttackBehavior>& behaviorVariant : behaviors) {
            if(std::holds_alternative<AttackBehavior>(behaviorVariant)) {
                EntityStateComponent* state_component = entity->get_component<EntityStateComponent>();
                if(state_component->current_state == EntityState::ATTACKING) {
                    // continue attacking, so we're done here
                    break;
                }

                AttackBehavior& behavior = std::get<AttackBehavior>(behaviorVariant);
                auto entities_with_stats = ctx.entity_manager.get_entities_with_component<Stats>();

                for(Entity* stats_ent : entities_with_stats) {
                    Stats* stats = stats_ent->get_component<Stats>();
                    Stats* my_stats = entity->get_component<Stats>();
                    if(stats->team_id == my_stats->team_id) {
                        // TODO find a better way to find legal targets - ploinky 20/11/2025
                        continue;
                    }

                    if(std::abs((stats_ent->get_component<Movement>()->position - entity->get_component<Movement>()->position).length()) <= behavior.aggro_distance) {
                        LOG_INFO("Entity %d is aggro and wants to attack entity %d", entity->get_id(), stats_ent->get_id());
                        state_component->current_state = EntityState::ATTACKING;
                        state_component->target_entity_id = stats_ent->get_id();
                        state_component->state_duration_ms = 0.0f;
                    }
                }
                continue;
            }

            if(std::holds_alternative<WaypointBehavior>(behaviorVariant)) {
                WaypointBehavior& behavior = std::get<WaypointBehavior>(behaviorVariant);

                PathfindingComponent* pathfinding = entity->get_component<PathfindingComponent>();
                if(!pathfinding) {
                    LOG_WARN("Entity %d has waypoint behavior but no pathfinding component", entity->get_id());
                    continue;
                }

                if(pathfinding->waypoints.empty() || Vec2(pathfinding->waypoints.back().x, pathfinding->waypoints.back().z) != behavior.path.front()) {
                    PathRequest request;
                    request.entity_id = entity->get_id();
                    request.current_position = Vec3(entity->get_component<Movement>()->position.x, 0, entity->get_component<Movement>()->position.y);
                    request.destination = Vec3(behavior.path.front().x, 0, behavior.path.front().y);
                    request.entity_pathing_radius = 0.5f;
                    
                    EntityStateComponent* ent_state = entity->get_component<EntityStateComponent>();
                    if (ctx.navigation_service->MakeRequest(request)) {
                        // Transition to pathfinding state
                        ent_state->current_state = EntityState::PATHFINDING_WAITING;
                        ent_state->state_duration_ms = 0.0f;
                        ent_state->target_positions.clear();
                        
                        LOG_INFO("Entity %u requested pathfinding to (%.2f, %.2f)", entity->get_id(), behavior.path.front().x, behavior.path.front().y);
                    } else {
                        LOG_WARN("Entity %u failed to request pathfinding", entity->get_id());
                    }
                }
                continue;
            }
        }
    }
}