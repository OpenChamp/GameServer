#pragma once

#include "entity_manager.hpp"

/**
 * System to handle minion damage and death.
 * When a minion reaches the destination, it takes 100 damage and dies.
 */
class MinionDamageSystem {
public:
    /**
     * Update minion damage system.
     * Applies damage to minions that have reached their destination.
     * @param entity_manager Reference to the entity manager
     * @return List of entity IDs that died in this update
     */
    std::vector<EntityID> update(EntityManager& entity_manager);
    
private:
    static constexpr float DESTINATION_DAMAGE = 100.0f;
};
