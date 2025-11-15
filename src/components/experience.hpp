#pragma once

#include "component.hpp"

struct ExperienceComponent : public Component {
    float current_exp = 0.0f;        // Current experience points
    float required_exp = 1.0f;       // Experience required for next level
    float exp_growth_rate = 1.2f;    // Growth rate for required experience per level

    COMPONENT_TYPE_ID(ExperienceComponent, 2010)
};