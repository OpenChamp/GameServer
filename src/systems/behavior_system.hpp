#pragma once

#include "system_context.hpp"
#include <cstdint>

class BehaviorSystem {
public:
    BehaviorSystem() = default;
    ~BehaviorSystem() = default;
    
    // Prevent copying
    BehaviorSystem(const BehaviorSystem&) = delete;
    BehaviorSystem& operator=(const BehaviorSystem&) = delete;
    
    // Allow moving
    BehaviorSystem(BehaviorSystem&&) = default;
    BehaviorSystem& operator=(BehaviorSystem&&) = default;
    
    void update(const SystemContext& ctx);
};
