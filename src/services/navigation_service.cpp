#include <services/navigation_service.hpp>
#include <services/astar_pathfinding.hpp>

#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <condition_variable>

#include <libs/log.hpp>

#include <components/map.hpp>

// Comparator for priority queue: higher priority (player input) comes first
struct PathRequestComparator {
    bool operator()(const PathRequest& a, const PathRequest& b) const {
        // In priority_queue, returning true means 'a' should come AFTER 'b'
        // So we return true if a has LOWER priority than b
        return a.priority() < b.priority();
    }
};

struct NavigationService::NavServiceBackend {
    std::thread worker;
    Map map;
    std::atomic<bool> running{false};
    std::priority_queue<PathRequest, std::vector<PathRequest>, PathRequestComparator> requests;
    std::queue<PathResult> results;
    std::queue<PathResult> waitingResults;
    std::mutex mtx;

    NavServiceBackend() {
        // Don't start thread yet - wait for map to be assigned
    }
    
    void start_worker() {
        running = true;
        
        // Verify grid was built during map loading
        if (map.grid_width > 0 && map.grid_height > 0) {
            LOG_INFO("NavigationService: Using precomputed grid (size: %dx%d, cell_size: %.1f)",
                     map.grid_width, map.grid_height, map.grid_cell_size);
        } else {
            LOG_ERROR("NavigationService: Navmesh grid not precomputed. Closing navigation service.");
            running = false;
            return;
        }
        
        worker = std::thread([this]() {
            LOG_INFO("Spinning off NavigationService backend thread");
            start_work();
        });
    }

    ~NavServiceBackend() {
        running = false;
        if (worker.joinable()) worker.join();
    }

    void start_work() {
        std::unique_lock lock(mtx, std::defer_lock);

        while (running) {
            if(!lock.try_lock()) {
                // could not lock, try again
                continue;
            }

            // =================================================================
            // THE LOCK IS LOCKED AND NEEDS TO BE UNLOCKED ASAP
            // =================================================================

            if(requests.empty()) {
                // nothing to do, so unlock and try again
                lock.unlock();
                continue;
            }

            PathRequest req = requests.top();
            requests.pop();

            // we have our request, we can unlock for now and do the pathing
            lock.unlock();

            // =================================================================
            // the lock is unlocked, take your time
            // =================================================================

            // Create local copies of the navmesh data for thread-safe pathfinding
            std::vector<Vec2> navmesh_vertices = this->map.vertices;
            std::vector<std::vector<uint32_t>> navmesh_polygons = this->map.polygons;

            // Create grid structure reference from map component
            AStarPathfinder::NavGrid grid;
            grid.grid_cells = this->map.grid_cells;
            grid.grid_origin = this->map.grid_origin;
            grid.grid_cell_size = this->map.grid_cell_size;
            grid.grid_width = this->map.grid_width;
            grid.grid_height = this->map.grid_height;

            std::vector<Vec2> path_2d = AStarPathfinder::FindPath(
                req.current_position,
                req.destination,
                navmesh_vertices,
                navmesh_polygons,
                req.entity_pathing_radius,
                &grid  // Pass grid from map component
            );

            // Store the 2D path directly
            PathResult res = PathResult();
            res.entity_id = req.entity_id;
            res.path = path_2d;
            
            if (res.path.empty()) {
                LOG_WARN("Navigation: Entity %u path from (%.1f, %.1f) to (%.1f, %.1f) returned EMPTY PATH", 
                         req.entity_id, req.current_position.x, req.current_position.y, req.destination.x, req.destination.y);
            } else {
                LOG_DEBUG("Navigation: Entity %u path with %zu waypoints", req.entity_id, res.path.size());
            }

            // we have our result, back to the main thread
            if(!lock.try_lock()) {
                // we couldn't lock, so cache this result for later
                // if we don't cache and submit this later, the request will be lost
                waitingResults.push(res);
                continue;
            }

            // =================================================================
            // THE LOCK IS LOCKED AND NEEDS TO BE UNLOCKED ASAP
            // =================================================================

            // first lets clean up anything we didn't get to before
            while(!waitingResults.empty()) {
                results.push(waitingResults.front());
                waitingResults.pop();
            }

            // now we push the new result at the end
            results.push(res);

            lock.unlock();

            // =================================================================
            // the lock is unlocked, take your time
            // =================================================================
        }
    }
};


NavigationService::NavigationService(Map map_object) {
    impl_ = std::make_unique<NavServiceBackend>();
    impl_->map = std::move(map_object);
    impl_->start_worker();  // Start thread after map is assigned
}

NavigationService::~NavigationService() = default;

bool NavigationService::MakeRequest(PathRequest request) {
    std::unique_lock lock(impl_->mtx, std::defer_lock);
    
    if(!lock.try_lock()) {
        return false;
    }

    // =================================================================
    // THE LOCK IS LOCKED AND NEEDS TO BE UNLOCKED ASAP
    // =================================================================
    impl_->requests.push(request);

    lock.unlock();
    // =================================================================
    // the lock is unlocked, take your time
    // =================================================================

    return true;
}
    
std::optional<PathResult> NavigationService::GetResult() {
    std::unique_lock lock(impl_->mtx, std::defer_lock);
    
    if(!lock.try_lock()) {
        return std::nullopt;
    }
    
    // =================================================================
    // THE LOCK IS LOCKED AND NEEDS TO BE UNLOCKED ASAP
    // =================================================================
    if(impl_->results.empty()) {
        lock.unlock();
        return std::nullopt;
    }

    PathResult result = impl_->results.front();
    impl_->results.pop();

    lock.unlock();
    // =================================================================
    // the lock is unlocked, take your time
    // =================================================================

    return std::make_optional(result);
}