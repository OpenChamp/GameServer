#pragma once

#include <component.hpp>
#include <string>

/**
 * Metadata component for UI/Visual Data.
 * Stores metadata such as name, description, icon, and model for the entity.
 */
struct MetadataComponent : public Component {
    std::string name;
    std::string description;
    std::string icon;
    std::string model;

    COMPONENT_TYPE_ID(MetadataComponent, 4001)
};
