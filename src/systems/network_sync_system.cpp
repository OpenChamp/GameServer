#include "network_sync_system.hpp"
#include "serialization_system.hpp"
#include "components/movement.hpp"
#include <log.hpp>

void NetworkSyncSystem::update(EntityManager& entity_manager, NetworkService* network_service) {
    if (!network_service) {
        return;
    }
    
    // Get all entities with movement components and broadcast their positions
    auto moving_entities = entity_manager.get_entities_with_component<Movement>();
    
    for (auto* entity : moving_entities) {
        Movement* move_comp = entity->get_component<Movement>();
        if (move_comp) {
            std::vector<uint8_t> packet = SerializationSystem::serialize_entity_position(
                entity->get_id(), 
                move_comp->position
            );
            network_service->broadcast_packet(packet);
        }
    }
}
