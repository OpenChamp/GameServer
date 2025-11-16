// Simple unit test framework for GameServer
// Tests can be compiled and run independently

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "../src/systems/entity_manager.hpp"
#include "../src/components/movement.hpp"
#include "../src/components/stats.hpp"
#include "../src/systems/packet_validator.hpp"
#include "../src/entities/player.hpp"

// Test utilities
int tests_passed = 0;
int tests_failed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) \
    try { \
        test_##name(); \
        std::cout << "✓ " << #name << std::endl; \
        tests_passed++; \
    } catch (const std::exception& e) { \
        std::cout << "✗ " << #name << ": " << e.what() << std::endl; \
        tests_failed++; \
    }

#define ASSERT(condition, message) \
    if (!(condition)) { \
        throw std::runtime_error(std::string(#condition) + " - " + message); \
    }

#define ASSERT_EQ(a, b) ASSERT((a) == (b), "Values not equal")
#define ASSERT_NE(a, b) ASSERT((a) != (b), "Values should not be equal")
#define ASSERT_TRUE(a) ASSERT(a, "Expected true")
#define ASSERT_FALSE(a) ASSERT(!(a), "Expected false")

// ===== Entity Manager Tests =====

TEST(entity_creation) {
    EntityManager manager;
    Entity& entity = manager.create_entity();
    ASSERT_EQ(entity.get_id(), 1u);
    ASSERT_EQ(manager.entity_count(), 1u);
}

TEST(entity_destruction) {
    EntityManager manager;
    Entity& entity = manager.create_entity();
    EntityID id = entity.get_id();
    
    bool destroyed = manager.destroy_entity(id);
    ASSERT_TRUE(destroyed);
    ASSERT_FALSE(manager.get_entity(id));
}

TEST(component_addition) {
    EntityManager manager;
    Entity& entity = manager.create_entity();
    
    auto movement = std::make_unique<Movement>();
    movement->position = Vec2(1.0f, 3.0f);
    entity.add_component(std::move(movement));
    
    ASSERT_TRUE(entity.has_component<Movement>());
}

TEST(component_retrieval) {
    EntityManager manager;
    Entity& entity = manager.create_entity();
    
    auto movement = std::make_unique<Movement>();
    movement->position = Vec2(5.0f, 15.0f);
    entity.add_component(std::move(movement));
    
    Movement* retrieved = entity.get_component<Movement>();
    ASSERT_NE(retrieved, nullptr);
    ASSERT_EQ(retrieved->position.x, 5.0f);
    ASSERT_EQ(retrieved->position.y, 10.0f);
}

TEST(component_removal) {
    EntityManager manager;
    Entity& entity = manager.create_entity();
    
    auto movement = std::make_unique<Movement>();
    entity.add_component(std::move(movement));
    
    ASSERT_TRUE(entity.has_component<Movement>());
    bool removed = entity.remove_component<Movement>();
    ASSERT_TRUE(removed);
    ASSERT_FALSE(entity.has_component<Movement>());
}

TEST(multiple_components) {
    EntityManager manager;
    Entity& entity = manager.create_entity();
    
    auto movement = std::make_unique<Movement>();
    auto stats = std::make_unique<Stats>();
    stats->health = 75.0f;
    
    entity.add_component(std::move(movement));
    entity.add_component(std::move(stats));
    
    ASSERT_EQ(entity.component_count(), 2u);
    ASSERT_TRUE(entity.has_component<Movement>());
    ASSERT_TRUE(entity.has_component<Stats>());
}

TEST(entity_query) {
    EntityManager manager;
    
    Entity& e1 = manager.create_entity();
    e1.add_component(std::make_unique<Movement>());
    
    Entity& e2 = manager.create_entity();
    e2.add_component(std::make_unique<Movement>());
    e2.add_component(std::make_unique<Stats>());
    
    Entity& e3 = manager.create_entity();
    e3.add_component(std::make_unique<Stats>());
    
    auto with_movement = manager.get_entities_with_component<Movement>();
    ASSERT_EQ(with_movement.size(), 2u);
    
    auto with_stats = manager.get_entities_with_component<Stats>();
    ASSERT_EQ(with_stats.size(), 2u);
}

// ===== Packet Validator Tests =====

TEST(packet_validation_invalid_empty) {
    ASSERT_FALSE(PacketValidator::validate_packet(nullptr, 0));
}

TEST(packet_validation_valid_minimal) {
    uint8_t packet[1] = {(uint8_t)PACKET_TYPE::PLAYER_READY};
    ASSERT_FALSE(PacketValidator::validate_packet(packet, 1));  // Too short for PLAYER_READY
}

TEST(packet_validation_valid_ready) {
    uint8_t packet[2] = {(uint8_t)PACKET_TYPE::PLAYER_READY, 1};
    ASSERT_TRUE(PacketValidator::validate_packet(packet, 2));
}

TEST(packet_ready_extraction) {
    uint8_t packet[2] = {(uint8_t)PACKET_TYPE::PLAYER_READY, 1};
    bool ready = false;
    
    bool success = PacketValidator::extract_ready_status(packet, 2, ready);
    ASSERT_TRUE(success);
    ASSERT_TRUE(ready);
}

TEST(packet_ready_extraction_not_ready) {
    uint8_t packet[2] = {(uint8_t)PACKET_TYPE::PLAYER_READY, 0};
    bool ready = true;  // Set to true initially
    
    bool success = PacketValidator::extract_ready_status(packet, 2, ready);
    ASSERT_TRUE(success);
    ASSERT_FALSE(ready);
}

// ===== Player Tests =====

TEST(player_creation) {
    Player player("test_client_123");
    ASSERT_EQ(player.client_id, "test_client_123");
    ASSERT_FALSE(player.is_ready);
}

TEST(player_stale_check) {
    Player player("test_client");
    ASSERT_FALSE(player.is_stale(1000));  // Fresh player
    
    // Can't easily test stale without std::this_thread::sleep_for
    // This test just ensures the API works
}

TEST(player_activity_update) {
    Player player("test_client");
    auto first_time = player.last_activity;
    
    player.update_activity();
    auto second_time = player.last_activity;
    
    ASSERT_NE(first_time, second_time);
}

// ===== Math (Vec3) Tests =====

TEST(vec3_construction) {
    Vec3 v(1.0f, 2.0f, 3.0f);
    ASSERT_EQ(v.x, 1.0f);
    ASSERT_EQ(v.y, 2.0f);
    ASSERT_EQ(v.z, 3.0f);
}

TEST(vec3_addition) {
    Vec3 a(1.0f, 2.0f, 3.0f);
    Vec3 b(4.0f, 5.0f, 6.0f);
    Vec3 result = a + b;
    
    ASSERT_EQ(result.x, 5.0f);
    ASSERT_EQ(result.y, 7.0f);
    ASSERT_EQ(result.z, 9.0f);
}

TEST(vec3_subtraction) {
    Vec3 a(10.0f, 20.0f, 30.0f);
    Vec3 b(1.0f, 2.0f, 3.0f);
    Vec3 result = a - b;
    
    ASSERT_EQ(result.x, 9.0f);
    ASSERT_EQ(result.y, 18.0f);
    ASSERT_EQ(result.z, 27.0f);
}

TEST(vec3_length) {
    Vec3 v(3.0f, 4.0f, 0.0f);
    float len = v.length();
    ASSERT_EQ(len, 5.0f);
}

TEST(vec3_normalize) {
    Vec3 v(3.0f, 4.0f, 0.0f);
    v.normalize();
    
    float len = v.length();
    ASSERT_TRUE(len > 0.999f && len < 1.001f);  // Close to 1.0
}

TEST(vec3_dot_product) {
    Vec3 a(1.0f, 0.0f, 0.0f);
    Vec3 b(0.0f, 1.0f, 0.0f);
    ASSERT_EQ(a.dot(b), 0.0f);
}

int main() {
    std::cout << "\n===== Running GameServer Tests =====" << std::endl;
    std::cout << std::endl;
    
    // Entity Manager Tests
    std::cout << "Entity Manager Tests:" << std::endl;
    RUN_TEST(entity_creation);
    RUN_TEST(entity_destruction);
    RUN_TEST(component_addition);
    RUN_TEST(component_retrieval);
    RUN_TEST(component_removal);
    RUN_TEST(multiple_components);
    RUN_TEST(entity_query);
    
    std::cout << "\nPacket Validator Tests:" << std::endl;
    RUN_TEST(packet_validation_invalid_empty);
    RUN_TEST(packet_validation_valid_minimal);
    RUN_TEST(packet_validation_valid_ready);
    RUN_TEST(packet_ready_extraction);
    RUN_TEST(packet_ready_extraction_not_ready);
    
    std::cout << "\nPlayer Tests:" << std::endl;
    RUN_TEST(player_creation);
    RUN_TEST(player_stale_check);
    RUN_TEST(player_activity_update);
    
    std::cout << "\nMath (Vec3) Tests:" << std::endl;
    RUN_TEST(vec3_construction);
    RUN_TEST(vec3_addition);
    RUN_TEST(vec3_subtraction);
    RUN_TEST(vec3_length);
    RUN_TEST(vec3_normalize);
    RUN_TEST(vec3_dot_product);
    
    std::cout << std::endl << "===== Test Results =====" << std::endl;
    std::cout << "Passed: " << tests_passed << std::endl;
    std::cout << "Failed: " << tests_failed << std::endl;
    std::cout << "Total:  " << (tests_passed + tests_failed) << std::endl;
    
    return tests_failed > 0 ? 1 : 0;
}
