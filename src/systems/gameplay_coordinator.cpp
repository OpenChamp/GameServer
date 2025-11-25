#include <systems/gameplay_coordinator.hpp>
#include <algorithm>
#include <libs/log.hpp>

GameplayCoordinator::GameplayCoordinator() 
    : wave_system_(nullptr) {
    // Systems initialized with default constructors
    // WaveSystem will be initialized via initialize_wave_system() before use
    
    // Link systems that depend on each other
    attack_execution_system_.set_combat_system(&combat_system_);
}

void GameplayCoordinator::initialize_wave_system(EntityManager* entity_manager, NetworkService* network_service,
                                                NavigationService* navigation_service, const Map* map) {
    wave_system_ = std::make_unique<WaveSystem>();
    wave_system_->initialize(entity_manager, network_service, navigation_service, map);
}

void GameplayCoordinator::update(const SystemContext& ctx) {
    // Execute systems in dependency order
    
    // INPUT SYSTEMS
    input_system_.update(ctx);
    npc_system_.update(ctx);
    
    // ENGINE SYSTEMS
    wave_system_->update(ctx);

    // MOVEMENT & PHYSICS SYSTEMS
    movement_system_.update(ctx);
    collision_system_.update(ctx);
    
    // COMBAT SYSTEMS
    auto_attack_system_.update(ctx);  // Must run before CombatSystem and AttackExecutionSystem
    attack_execution_system_.update(ctx);  // Must run after CombatSystem
    // PLAYER SYSTEMS

    // Synchronize to clients
    network_sync_system_.update(ctx);
    
    // Cleanup dead entities (after network sync so death state reaches clients first)
    cleanup_dead_entities(ctx.entity_manager);
}

void GameplayCoordinator::mark_for_cleanup(EntityID entity_id) {
    // Check if already marked
    auto it = std::find(entities_marked_for_cleanup_.begin(), 
                       entities_marked_for_cleanup_.end(), 
                       entity_id);
    if (it == entities_marked_for_cleanup_.end()) {
        entities_marked_for_cleanup_.push_back(entity_id);
        LOG_DEBUG("Entity %u marked for cleanup", entity_id);
    }
}

void GameplayCoordinator::cleanup_dead_entities(EntityManager& entity_manager) {
    // Actually remove entities that were marked last frame
    // Allows Client Sync before removal
    std::vector<EntityID> to_remove = entities_marked_for_cleanup_;
    entities_marked_for_cleanup_.clear();
    
    for (EntityID entity_id : to_remove) {
        if (entity_manager.destroy_entity(entity_id)) {
            LOG_INFO("Cleaned up dead entity %u", entity_id);
        } else {
            LOG_WARN("Failed to cleanup entity %u (already removed?)", entity_id);
        }
    }
    
    // Check for newly dead entities and mark them for cleanup next frame
    auto entities_with_state = entity_manager.get_entities_with_component<EntityStateComponent>();
    for (auto* entity : entities_with_state) {
        if (!entity) continue;
        
        auto* state = entity->get_component<EntityStateComponent>();
        if (!state || state->current_state != EntityState::DEAD) {
            continue;
        }
        
        // Cleanup entity next frame
        EntityID entity_id = entity->get_id();
        auto it = std::find(entities_marked_for_cleanup_.begin(), 
                           entities_marked_for_cleanup_.end(), 
                           entity_id);
        if (it == entities_marked_for_cleanup_.end()) {
            mark_for_cleanup(entity_id);
        }
    }
}
