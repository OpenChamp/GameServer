#pragma once

#include <cstdint>
#include <vector>
#include <string>

#include <systems/math.hpp>
#include <systems/packet_validator.hpp>

// Forward declarations
struct Stats;

/**
 * Handles serialization of game packets into byte arrays.
 * All methods are static and return packets as std::vector<uint8_t>.
 * No networking is performed by this class.
 */
class SerializationSystem {
public:
    /**
     * Serialize a packet with just a type.
     * @param packet_type The type of packet to serialize
     * @return Packet data as a vector of bytes
     */
    static std::vector<uint8_t> serialize_packet(PACKET_TYPE packet_type);

    /**
     * Serialize a packet with type and string data.
     * @param packet_type The type of packet to serialize
     * @param data The string data to include in the packet
     * @return Packet data as a vector of bytes
     */
    static std::vector<uint8_t> serialize_packet(PACKET_TYPE packet_type, const std::string& data);

    /**
     * Serialize an entity position update packet.
     * @param entity_id The ID of the entity
     * @param position The position of the entity
     * @return Packet data as a vector of bytes
     */
    static std::vector<uint8_t> serialize_entity_position(uint32_t entity_id, const Vec2& position);

    /**
     * Serialize an entity spawn packet.
     * @param entity_id The ID of the entity being spawned
     * @param position The spawn position of the entity
     * @param team_id The team the entity belongs to
     * @param entity_type The type of entity (e.g., "melee_minion", "ranged_minion")
     * @return Packet data as a vector of bytes
     */
    static std::vector<uint8_t> serialize_entity_spawn(uint32_t entity_id, const Vec2& position, uint8_t team_id, const std::string& entity_type);

    /**
     * Serialize an entity stats update packet (health, mana, level).
     * @param entity_id The ID of the entity
     * @param stats The Stats component containing health, mana, and level
     * @return Packet data as a vector of bytes
     */
    static std::vector<uint8_t> serialize_entity_stats(uint32_t entity_id, const struct Stats& stats);

private:
    /**
     * Helper function to serialize a 32-bit unsigned integer in little-endian format
     * @param value The value to serialize
     * @param output Vector to append the bytes to
     */
    static void serialize_uint32(uint32_t value, std::vector<uint8_t>& output);

    /**
     * Helper function to serialize a 16-bit unsigned integer in big-endian format
     * @param value The value to serialize
     * @param output Vector to append the bytes to
     */
    static void serialize_uint16_be(uint16_t value, std::vector<uint8_t>& output);

    /**
     * Helper function to serialize a float (32-bit) in little-endian format
     * @param value The value to serialize
     * @param output Vector to append the bytes to
     */
    static void serialize_float(float value, std::vector<uint8_t>& output);
};
