#include "gameplay_coordinator.hpp"

GameplayCoordinator::GameplayCoordinator() 
    : wave_system_(nullptr) {
    // Systems initialized with default constructors
    // WaveSystem will be initialized via initialize_wave_system() before use
}

void GameplayCoordinator::initialize_wave_system(EntityManager* entity_manager, NetworkService* network_service,
                                                NavigationService* navigation_service, const Map* map) {
    wave_system_ = std::make_unique<WaveSystem>();
    wave_system_->initialize(entity_manager, network_service, navigation_service, map);
}

void GameplayCoordinator::update(const SystemContext& ctx) {
    // Execute systems in dependency order
    
    // ENGINE SYSTEMS
    wave_system_->update(ctx);

    // ENTITY SYSTEMS
    movement_system_.update(ctx);
    combat_system_.update(ctx);
    // PLAYER SYSTEMS

    // 5. Synchronize to clients
    network_sync_system_.update(ctx);
}
