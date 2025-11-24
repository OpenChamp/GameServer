#pragma once

#include "component.hpp"
#include "component_registry.hpp"

enum class NPCType {
    MINION
};

struct NPCComponent : public Component {
    NPCType npc_type;

    COMPONENT_TYPE_ID(NPCComponent, ComponentTypes::NPC)
};