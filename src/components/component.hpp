#pragma once

#include <cstdint>
#include <memory>

/**
 * Base class for all ECS components.
 * Components are pure data containers with no logic.
 */
class Component
{
public:
    virtual ~Component() = default;

    /**
     * Get the component type ID.
     * Should be overridden in derived classes.
     * @return Type ID of this component
     */
    virtual uint32_t get_type_id() const = 0;

    /**
     * Clone this component.
     * @return Unique pointer to a copy of this component
     */
    virtual std::unique_ptr<Component> clone() const = 0;
};

/**
 * Macro to simplify component type ID registration.
 *
 *   COMPONENT_TYPE_ID(TransformComponent, 1001)
 */
#define COMPONENT_TYPE_ID(ClassName, TypeId)                  \
    static const uint32_t TYPE_ID = TypeId;                   \
    uint32_t get_type_id() const override { return TYPE_ID; } \
    std::unique_ptr<Component> clone() const override         \
    {                                                         \
        return std::make_unique<ClassName>(*this);            \
    }
