#pragma once
#include "component.hpp"

enum class StructureType {
    TOWER,
    INHIBITOR,
    NEXUS,
    WARD,
    CUSTOM
};

struct Structure : public Component {
    StructureType type = StructureType::TOWER;
    uint32_t level = 1;                    // Tower level (affects stats)
    bool is_active = true;                 // Can attack/interact
    float activation_range = 30.0f;        // Range to detect enemies
    
    COMPONENT_TYPE_ID(Structure, 2010)
};