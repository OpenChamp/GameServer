#pragma once

#include "component.hpp"
#include "component_registry.hpp"

enum class NPCType {
    MINION
};

struct NPCComponent : public Component {
    NPCType npc_type;
    float chase_distance = 30.0f;
    float chase_timeout_ms = 5000.0f; // How long to keep chasing without hitting the target (not implemented)-- cmkrist 24/11/2025

    COMPONENT_TYPE_ID(NPCComponent, ComponentTypes::NPC)
};