#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include <typeinfo>
#include <stdexcept>

#include "components/component.hpp"
#include <systems/data_loader.hpp>

/**
 * Represents a unique entity ID in the ECS system.
 */
using EntityID = uint32_t;
constexpr EntityID INVALID_ENTITY_ID = 0;

/**
 * Entity class that holds multiple components.
 * Each entity is uniquely identified and can contain any number of components.
 */
class Entity {
public:
    /**
     * Constructor
     * @param id Unique entity identifier
     */
    explicit Entity(EntityID id) : id_(id) {}
    
    /**
     * Get the entity's unique ID.
     * @return Entity ID
     */
    EntityID get_id() const { return id_; }
    
    /**
     * Add a component to this entity.
     * If a component of this type already exists, it will be replaced.
     * @param component Unique pointer to the component
     */
    void add_component(std::unique_ptr<Component> component) {
        uint32_t type_id = component->get_type_id();
        components_[type_id] = std::move(component);
    }
    
    /**
     * Get a component by type.
     * @tparam T Component type
     * @return Pointer to the component, or nullptr if not found
     */
    template<typename T>
    T* get_component() {
        uint32_t type_id = T::TYPE_ID;
        auto it = components_.find(type_id);
        if (it == components_.end()) {
            return nullptr;
        }
        return (T*)(it->second.get());
    }
    
    /**
     * Get a component by type (const version).
     * @tparam T Component type
     * @return Const pointer to the component, or nullptr if not found
     */
    template<typename T>
    const T* get_component() const {
        uint32_t type_id = T::TYPE_ID;
        auto it = components_.find(type_id);
        if (it == components_.end()) {
            return nullptr;
        }
        return (const T*)(it->second.get());
    }
    
    /**
     * Check if entity has a component of a given type.
     * @tparam T Component type
     * @return true if component exists
     */
    template<typename T>
    bool has_component() const {
        return components_.count(T::TYPE_ID) > 0;
    }
    
    /**
     * Remove a component by type.
     * @tparam T Component type
     * @return true if component was removed, false if it didn't exist
     */
    template<typename T>
    bool remove_component() {
        return components_.erase(T::TYPE_ID) > 0;
    }
    
    /**
     * Get number of components in this entity.
     * @return Component count
     */
    size_t component_count() const {
        return components_.size();
    }

private:
    EntityID id_;
    std::unordered_map<uint32_t, std::unique_ptr<Component>> components_;
};

/**
 * Entity manager for managing all entities and their components.
 * Provides factory methods and query capabilities for the ECS system.
 */
class EntityManager {
public:
    EntityManager() : next_entity_id_(1) {
        // TODO probably use system independent path separator - ploinky 14/11/2025
        for(std::string file_name : DataLoader::list_files_from_directory("data/entities", ".xml")) {
            EntityTemplate entity_template = DataLoader::load_entity_template(file_name);
            entity_template_cache.emplace(entity_template.id, entity_template);
        }
    }
    
    /**
     * Create a new entity.
     * @return Reference to the newly created entity
     */
    Entity& create_entity() {
        EntityID id = next_entity_id_++;
        auto [it, inserted] = entities_.emplace(id, Entity(id));
        return it->second;
    }
    
    Entity& create_entity_from_template(std::string entity_type_id) {
        Entity& new_entity = create_entity();

        auto map_it = entity_template_cache.find(entity_type_id);
        if(map_it == entity_template_cache.end()) {
            LOG_ERROR("No entity template found for type %s", entity_type_id.c_str());
            return new_entity;
        }

        for(auto comp : map_it->second.component_templates) {
            std::unique_ptr<Component> comp_copy = comp->clone();
            new_entity.add_component(std::move(comp_copy));
        }

        return new_entity;
    }
    
    /**
     * Get an entity by ID.
     * @param id Entity ID
     * @return Pointer to entity, or nullptr if not found
     */
    Entity* get_entity(EntityID id) {
        auto it = entities_.find(id);
        if (it == entities_.end()) {
            return nullptr;
        }
        return &it->second;
    }
    
    /**
     * Get an entity by ID (const version).
     * @param id Entity ID
     * @return Const pointer to entity, or nullptr if not found
     */
    const Entity* get_entity(EntityID id) const {
        auto it = entities_.find(id);
        if (it == entities_.end()) {
            return nullptr;
        }
        return &it->second;
    }
    
    /**
     * Destroy an entity by ID.
     * @param id Entity ID
     * @return true if entity was destroyed, false if it didn't exist
     */
    bool destroy_entity(EntityID id) {
        return entities_.erase(id) > 0;
    }
    
    /**
     * Get total number of entities.
     * @return Entity count
     */
    size_t entity_count() const {
        return entities_.size();
    }
    
    /**
     * Get all entities that have a specific component.
     * @tparam T Component type
     * @return Vector of entity pointers that have component T
     */
    template<typename T>
    std::vector<Entity*> get_entities_with_component() {
        std::vector<Entity*> result;
        for (auto& [id, entity] : entities_) {
            if (entity.has_component<T>()) {
                result.push_back(&entity);
            }
        }
        return result;
    }
    
    /**
     * Clear all entities.
     */
    void clear() {
        entities_.clear();
        next_entity_id_ = 1;
    }
    
    /**
     * Get all entities.
     * @return Reference to the map of all entities
     */
    std::unordered_map<EntityID, Entity>& get_all_entities() {
        return entities_;
    }

private:
    std::unordered_map<EntityID, Entity> entities_;
    EntityID next_entity_id_;

    // caches entity templates by their entity type (for cloning)
    std::map<std::string, EntityTemplate> entity_template_cache;
};
