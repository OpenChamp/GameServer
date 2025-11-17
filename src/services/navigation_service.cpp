#include <services/navigation_service.hpp>
#include <services/astar_pathfinding.hpp>

#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <condition_variable>

#include <libs/log.hpp>
#include <libs/frame_timer.h>

#include <components/map.hpp>

struct NavigationService::NavServiceBackend {
    std::thread worker;
    Map map;
    std::atomic<bool> running{true};
    std::queue<PathRequest> requests;
    std::queue<PathResult> results;
    std::queue<PathResult> waitingResults;
    std::mutex mtx;
    // TODO what frame rate should this run at? infinite? - ploinky 14/11/2025
    FrameTimer frame_timer = FrameTimer(30);

    NavServiceBackend() {
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

            PathRequest req = requests.front();
            requests.pop();

            // we have our request, we can unlock for now and do the pathing
            lock.unlock();

            // =================================================================
            // the lock is unlocked, take your time
            // =================================================================

            // Perform A* pathfinding on the 2D navmesh
            Vec2 start_2d(req.current_position.x, req.current_position.z);
            Vec2 goal_2d(req.destination.x, req.destination.z);

            // Create local copies of the navmesh data for thread-safe pathfinding
            std::vector<Vec2> navmesh_vertices = this->map.vertices;
            std::vector<std::vector<uint32_t>> navmesh_polygons = this->map.polygons;

            std::vector<Vec2> path_2d = AStarPathfinder::FindPath(
                start_2d,
                goal_2d,
                navmesh_vertices,
                navmesh_polygons,
                req.entity_pathing_radius
            );

            // Convert the 2D path back to 3D (keeping Y from destination for now)
            PathResult res = PathResult();
            res.entity_id = req.entity_id;
            res.path.reserve(path_2d.size());
            for (const auto& waypoint_2d : path_2d) {
                res.path.push_back(Vec3(waypoint_2d.x, req.destination.y, waypoint_2d.y));
            }
            
            if (res.path.empty()) {
                LOG_WARN("Navigation: Entity %u path from (%.1f, %.1f) to (%.1f, %.1f) returned EMPTY PATH", 
                         req.entity_id, start_2d.x, start_2d.y, goal_2d.x, goal_2d.y);
            } else {
                LOG_DEBUG("Navigation: Entity %u path with %zu waypoints", req.entity_id, res.path.size());
            }

            // we have our result, so let's hand it back to the main thread now
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