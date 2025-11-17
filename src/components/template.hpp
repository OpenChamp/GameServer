#pragma once

#include <string>

#include "component.hpp"

/**
 * This component indicates the template from which the entity was created.
 * This is used to let client know what archetype to spawn.
 */
class TemplateComponent : public Component {
public:
    TemplateComponent(std::string template_id) : template_id(template_id) {};
    std::string template_id;

    COMPONENT_TYPE_ID(TemplateComponent, 2311)
};