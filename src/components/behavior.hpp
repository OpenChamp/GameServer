#pragma once

#include "component.hpp"

#include <vector>
#include <variant>
#include <string>

#include "systems/math.hpp"
#include "component_registry.hpp"

class ICondition {
};

class DistanceFromPathCondition : public ICondition{
public:
    std::vector<Vec2> path;
    float max_distance;
};

class WaypointBehavior {
public:
    std::vector<Vec2> path;
};

class AttackBehavior {
public:
    float aggro_distance;
};

class Behavior {
public:
    std::string id;
    std::vector<std::variant<
        WaypointBehavior,
        AttackBehavior
    >> behaviors;
};

struct BehaviorComponent : public Component {
    Behavior brain;

    COMPONENT_TYPE_ID(BehaviorComponent, ComponentTypes::BEHAVIOR)
};