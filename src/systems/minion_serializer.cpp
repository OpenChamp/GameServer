#include "minion_serializer.hpp"
#include "components/movement.hpp"
#include "components/stats.hpp"
#include "components/minion.hpp"
#include "packet_validator.hpp"
#include <log.hpp>
#include <cstring>

std::vector<uint8_t> MinionSerializer::serialize_minions(EntityManager& entity_manager) {
    auto minions = collect_minions(entity_manager);
    
    if (minions.empty()) {
        return {};
    }
    
    // Calculate packet size
    // 1 byte (type) + 4 bytes (count) + (21 bytes per minion)
    size_t packet_size = 1 + 4 + (minions.size() * 21);
    
    std::vector<uint8_t> data(packet_size);
    size_t offset = 0;
    
    // Write packet type
    data[offset++] = (uint8_t)(PACKET_TYPE::MINION_STATE);
    
    // Write minion count
    uint32_t count = minions.size();
    std::memcpy(data.data() + offset, &count, sizeof(uint32_t));
    offset += 4;
    
    // Write each minion
    for (const auto& minion : minions) {
        // Entity ID
        std::memcpy(data.data() + offset, &minion.entity_id, sizeof(uint32_t));
        offset += 4;
        
        // Position (x, y, z)
        std::memcpy(data.data() + offset, &minion.x, sizeof(float));
        offset += 4;
        std::memcpy(data.data() + offset, &minion.y, sizeof(float));
        offset += 4;
        std::memcpy(data.data() + offset, &minion.z, sizeof(float));
        offset += 4;
        
        // Health
        std::memcpy(data.data() + offset, &minion.health, sizeof(float));
        offset += 4;
        
        // State
        data[offset++] = minion.state;
    }
    
    LOG_DEBUG("Serialized %zu minions into packet (size: %zu bytes)", minions.size(), packet_size);
    return data;
}

std::vector<MinionSerializer::SerializedMinion> MinionSerializer::collect_minions(EntityManager& entity_manager) {
    std::vector<SerializedMinion> result;
    
    // Get all entities
    auto& all_entities = entity_manager.get_all_entities();
    
    // Iterate through all entities
    for (auto& [entity_id, entity] : all_entities) {
        // Check if this entity has all required minion components
        Movement* movement = entity.get_component<Movement>();
        Stats* stats = entity.get_component<Stats>();
        Minion* minion = entity.get_component<Minion>();
        
        if (movement && stats && minion) {
            // This is a valid minion entity
            SerializedMinion serialized;
            serialized.entity_id = entity_id;
            serialized.x = movement->position.x;
            serialized.y = movement->position.y;
            serialized.z = movement->position.z;
            serialized.health = stats->health;
            
            // Determine state
            if (minion->is_dead) {
                serialized.state = 1;  // dead
            } else if (minion->has_reached_destination) {
                serialized.state = 2;  // reached destination
            } else {
                serialized.state = 0;  // alive and moving
            }
            
            result.push_back(serialized);
        }
    }
    
    return result;
}
