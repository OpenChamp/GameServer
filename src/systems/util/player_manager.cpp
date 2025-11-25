#include <systems/util/player_manager.hpp>
#include <components/client_info.hpp>
#include <components/readiness.hpp>
#include <components/network_metadata.hpp>
#include <components/player_owned.hpp>
#include <libs/log.hpp>

EntityID PlayerManager::on_client_connect(const std::string& client_id, EntityManager& entity_manager) {
    // Check if already connected
    if (client_to_entity_.find(client_id) != client_to_entity_.end()) {
        LOG_WARN("Client %s already connected", client_id.c_str());
        return INVALID_ENTITY_ID;
    }
    
    // Create player entity
    EntityID player_entity_id = create_player_entity(client_id, entity_manager);
    
    if (player_entity_id == INVALID_ENTITY_ID) {
        LOG_ERROR("Failed to create player entity for client %s", client_id.c_str());
        return INVALID_ENTITY_ID;
    }
    
    // Map client to entity
    client_to_entity_[client_id] = player_entity_id;
    
    LOG_INFO("Player connected: client_id=%s, entity_id=%u, total_players=%zu",
             client_id.c_str(), player_entity_id, client_to_entity_.size());
    
    return player_entity_id;
}

bool PlayerManager::on_client_disconnect(const std::string& client_id, EntityManager& entity_manager) {
    auto it = client_to_entity_.find(client_id);
    if (it == client_to_entity_.end()) {
        LOG_WARN("Disconnect request for unknown client: %s", client_id.c_str());
        return false;
    }
    
    EntityID player_entity_id = it->second;
    
    // Find and destroy the champion owned by this player
    EntityID champion_id = get_champion_entity_id(player_entity_id, entity_manager);
    if (champion_id != INVALID_ENTITY_ID) {
        if (!entity_manager.destroy_entity(champion_id)) {
            LOG_WARN("Failed to destroy champion entity %u", champion_id);
        }
    }
    
    // Destroy player entity
    if (!entity_manager.destroy_entity(player_entity_id)) {
        LOG_ERROR("Failed to destroy player entity %u", player_entity_id);
    }
    
    // Remove mapping
    client_to_entity_.erase(it);
    
    LOG_INFO("Player disconnected: client_id=%s, player=%u, champion=%u, remaining=%zu",
             client_id.c_str(), player_entity_id, champion_id, client_to_entity_.size());
    
    return true;
}

bool PlayerManager::set_player_ready(const std::string& client_id, bool is_ready, EntityManager& entity_manager) {
    auto it = client_to_entity_.find(client_id);
    if (it == client_to_entity_.end()) {
        LOG_WARN("Set ready for unknown client: %s", client_id.c_str());
        return false;
    }
    
    EntityID player_entity_id = it->second;
    Entity* player_entity = entity_manager.get_entity(player_entity_id);
    if (!player_entity) {
        LOG_ERROR("Player entity not found: %u", player_entity_id);
        return false;
    }
    
    ReadinessComponent* readiness = player_entity->get_component<ReadinessComponent>();
    if (!readiness) {
        LOG_ERROR("Player entity missing ReadinessComponent: %u", player_entity_id);
        return false;
    }
    
    readiness->is_ready = is_ready;
    
    LOG_INFO("Player %s is now %s", client_id.c_str(), is_ready ? "ready" : "not ready");
    
    return true;
}

bool PlayerManager::are_all_players_ready(EntityManager& entity_manager) const {
    if (client_to_entity_.empty()) {
        return false;
    }
    
    // Query all entities with ReadinessComponent
    auto ready_entities = entity_manager.get_entities_with_component<ReadinessComponent>();
    
    if (ready_entities.size() != client_to_entity_.size()) {
        LOG_WARN("Readiness check: not all players have ReadinessComponent (%zu entities vs %zu players)",
                 ready_entities.size(), client_to_entity_.size());
        return false;
    }
    
    // Check if all are marked ready
    for (const auto* entity : ready_entities) {
        const auto* readiness = entity->get_component<ReadinessComponent>();
        if (!readiness || !readiness->is_ready) {
            return false;
        }
    }
    
    return true;
}

bool PlayerManager::is_full(int max_clients) const {
    return (int)client_to_entity_.size() >= max_clients;
}

size_t PlayerManager::get_player_count() const {
    return client_to_entity_.size();
}

EntityID PlayerManager::get_player_entity_id(const std::string& client_id) const {
    auto it = client_to_entity_.find(client_id);
    if (it == client_to_entity_.end()) {
        return INVALID_ENTITY_ID;
    }
    return it->second;
}

EntityID PlayerManager::get_champion_entity_id(EntityID player_entity_id, EntityManager& entity_manager) const {
    // Find champion owned by this player
    auto champions = entity_manager.get_entities_with_component<PlayerOwnedComponent>();
    for (auto* champion : champions) {
        auto* player_owned = champion->get_component<PlayerOwnedComponent>();
        if (player_owned && player_owned->owning_player_id == player_entity_id) {
            return champion->get_id();
        }
    }
    return INVALID_ENTITY_ID;
}

std::vector<EntityID> PlayerManager::get_all_player_entities() const {
    std::vector<EntityID> result;
    for (const auto& [client_id, entity_id] : client_to_entity_) {
        result.push_back(entity_id);
    }
    return result;
}

void PlayerManager::update_player_latency(const std::string& client_id, EntityManager& entity_manager, unsigned int latency_ms) {
    auto it = client_to_entity_.find(client_id);
    if (it == client_to_entity_.end()) {
        return;
    }
    
    Entity* player_entity = entity_manager.get_entity(it->second);
    if (!player_entity) {
        return;
    }
    
    auto* metadata = player_entity->get_component<NetworkMetadataComponent>();
    if (metadata) {
        metadata->latency_ms = latency_ms;
        metadata->update_activity();
    }
}

void PlayerManager::reset_all_players(EntityManager& entity_manager) {
    for (auto* entity : entity_manager.get_entities_with_component<ReadinessComponent>()) {
        auto* readiness = entity->get_component<ReadinessComponent>();
        if (readiness) {
            readiness->is_ready = false;
        }
    }
}

void PlayerManager::clear() {
    client_to_entity_.clear();
    next_player_id_ = 0;
}

EntityID PlayerManager::create_player_entity(const std::string& client_id, EntityManager& entity_manager) {
    // Create player entity from template
    Entity& player_entity = entity_manager.create_entity_from_template("player");
    EntityID player_id = player_entity.get_id();
    
    // Set client info
    auto client_comp = std::make_unique<ClientInfoComponent>();
    client_comp->client_id = client_id;
    client_comp->player_id = next_player_id_++;
    player_entity.add_component(std::move(client_comp));
    
    // Ensure readiness component exists
    if (!player_entity.has_component<ReadinessComponent>()) {
        auto readiness_comp = std::make_unique<ReadinessComponent>();
        player_entity.add_component(std::move(readiness_comp));
    }
    
    // Ensure network metadata exists
    if (!player_entity.has_component<NetworkMetadataComponent>()) {
        auto metadata_comp = std::make_unique<NetworkMetadataComponent>();
        player_entity.add_component(std::move(metadata_comp));
    }
    
    // Create champion entity from template
    Entity& champion_entity = entity_manager.create_entity_from_template("champion");
    EntityID champion_id = champion_entity.get_id();
    
    // Link champion to player
    auto player_owned_comp = std::make_unique<PlayerOwnedComponent>();
    player_owned_comp->owning_player_id = player_id;
    champion_entity.add_component(std::move(player_owned_comp));
    
    // Set champion team ID so minions don't target it (use 255 = neutral observer)
    auto* champion_stats = champion_entity.get_component<Stats>();
    if (champion_stats) {
        champion_stats->team_id = 255;  // Neutral observer - not a valid combat target
    }
    
    LOG_INFO("Created player (ID %u) with champion (ID %u) for client %s",
             player_id, champion_id, client_id.c_str());
    
    return player_id;
}
