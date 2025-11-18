#include "serialization_system.hpp"
#include "components/stats.hpp"

std::vector<uint8_t> SerializationSystem::serialize_packet(PACKET_TYPE packet_type) {
    std::vector<uint8_t> packet_data;
    packet_data.push_back(static_cast<uint8_t>(packet_type));
    return packet_data;
}

std::vector<uint8_t> SerializationSystem::serialize_packet(PACKET_TYPE packet_type, const std::string& data) {
    // Verify data size fits in uint16_t
    if (data.size() > uint16_t(-1)) {
        return std::vector<uint8_t>();  // Return empty vector on error
    }

    std::vector<uint8_t> packet_data;
    packet_data.push_back(static_cast<uint8_t>(packet_type));
    
    // 2 bytes for length (Big-endian)
    uint16_t data_length = static_cast<uint16_t>(data.size());
    serialize_uint16_be(data_length, packet_data);
    
    // Add data
    packet_data.insert(packet_data.end(), data.begin(), data.end());
    
    return packet_data;
}

std::vector<uint8_t> SerializationSystem::serialize_entity_position(uint32_t entity_id, const Vec2& position) {
    std::vector<uint8_t> packet_data;
    packet_data.push_back(static_cast<uint8_t>(PACKET_TYPE::ENTITY_POSITION));
    
    // Entity ID (4 bytes, little-endian)
    serialize_uint32(entity_id, packet_data);
    
    // Position X (4 bytes float, little-endian)
    serialize_float(position.x, packet_data);
    
    // Position Y (4 bytes float, little-endian)
    serialize_float(position.y, packet_data);
    
    return packet_data;
}

std::vector<uint8_t> SerializationSystem::serialize_entity_spawn(uint32_t entity_id, const Vec2& position, uint8_t team_id, const std::string& entity_type) {
    std::vector<uint8_t> packet_data;
    packet_data.push_back(static_cast<uint8_t>(PACKET_TYPE::ENTITY_SPAWN));
    
    // Entity ID (4 bytes, little-endian)
    serialize_uint32(entity_id, packet_data);
    
    // Position X (4 bytes float, little-endian)
    serialize_float(position.x, packet_data);
    
    // Position Y (4 bytes float, little-endian)
    serialize_float(position.y, packet_data);
    
    // Team ID (1 byte)
    packet_data.push_back(team_id);
    
    // Entity type string length (4 bytes, little-endian)
    uint32_t type_length = static_cast<uint32_t>(entity_type.length());
    serialize_uint32(type_length, packet_data);
    
    // Entity type string data
    for (char c : entity_type) {
        packet_data.push_back(static_cast<uint8_t>(c));
    }
    
    return packet_data;
}

void SerializationSystem::serialize_uint32(uint32_t value, std::vector<uint8_t>& output) {
    output.push_back((value & 0xFF));
    output.push_back((value >> 8) & 0xFF);
    output.push_back((value >> 16) & 0xFF);
    output.push_back((value >> 24) & 0xFF);
}

void SerializationSystem::serialize_uint16_be(uint16_t value, std::vector<uint8_t>& output) {
    output.push_back((value >> 8) & 0xFF);
    output.push_back(value & 0xFF);
}

void SerializationSystem::serialize_float(float value, std::vector<uint8_t>& output) {
    uint32_t int_value = reinterpret_cast<uint32_t&>(value);
    serialize_uint32(int_value, output);
}

std::vector<uint8_t> SerializationSystem::serialize_entity_stats(uint32_t entity_id, const Stats& stats) {
    std::vector<uint8_t> packet_data;
    packet_data.push_back(static_cast<uint8_t>(PACKET_TYPE::ENTITY_STATS));
    
    // Entity ID (4 bytes, little-endian)
    serialize_uint32(entity_id, packet_data);
    
    // Health (4 bytes float, little-endian)
    serialize_float(stats.health, packet_data);
    
    // Max Health (4 bytes float, little-endian)
    serialize_float(stats.max_health, packet_data);
    
    // Mana (4 bytes float, little-endian)
    serialize_float(stats.mana, packet_data);
    
    // Max Mana (4 bytes float, little-endian)
    serialize_float(stats.max_mana, packet_data);
    
    // Level (4 bytes int, little-endian as uint32_t)
    serialize_uint32(static_cast<uint32_t>(stats.level), packet_data);
    
    return packet_data;
}
