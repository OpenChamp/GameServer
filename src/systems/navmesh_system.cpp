#include "navmesh_system.hpp"
#include "components/navmesh.hpp"
#include "components/component.hpp"
#include <log.hpp>

EntityID NavMeshSystem::map_entity_id_ = INVALID_ENTITY_ID;

Entity& NavMeshSystem::initialize_map(EntityManager& entity_manager) {
    LOG_INFO("Initializing NavMesh system");
    
    // Create map entity
    Entity& map_entity = entity_manager.create_entity();
    map_entity_id_ = map_entity.get_id();
    
    // Create and attach navmesh component
    auto navmesh = std::make_unique<NavMesh>();
    navmesh->width = 100.0f;
    navmesh->height = 20.0f;
    navmesh->y_level = 0.0f;
    
    // Set up two spawnpoints at opposite ends of the navmesh
    // Spawnpoint 0: at x=0 (left side)
    navmesh->spawnpoints.push_back(Vec3(0.0f, 0.0f, 10.0f));
    
    // Spawnpoint 1: at x=100 (right side)
    navmesh->spawnpoints.push_back(Vec3(100.0f, 0.0f, 10.0f));
    
    map_entity.add_component(std::move(navmesh));
    
    LOG_INFO("Map entity created with ID %u", map_entity_id_);
    LOG_INFO("NavMesh: 100x20, Spawnpoint 0: (0, 0, 10), Spawnpoint 1: (100, 0, 10)");
    
    return map_entity;
}

EntityID NavMeshSystem::get_map_entity_id() {
    return map_entity_id_;
}
