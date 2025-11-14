#pragma once

#include <memory>
#include <vector>

#include <systems/math.hpp>

class PathRequest {
public:
    uint32_t entity_id;
    float entity_pathing_radius;
    Vec3 current_position;
    Vec3 destination;
};

class PathResult {
public:
    uint32_t entity_id;
    std::vector<Vec3> path;
};

class NavigationService {
public:
    NavigationService();
    ~NavigationService();

    /**
     * Queue up a new navigation request for the service to mull over.
     * @param request The request to queue
     * @return True if the request was successfully queued ( -> if not, try again later?)
     */
    bool MakeRequest(PathRequest request);
    
    /**
     * Load the result from the front of the queue if such a result is available.
     * Call this like
     * 
     * PathResult result;
     * if(navigation_service->GetResult(&result)) {
     *  // do things with result
     * }
     * 
     * @param resultPtr OUT variable, will contain the result if this function returns true
     * @return True if a result could be retrieved and is now stored in `resultPtr`, false otherwise ( -> do not try to do things with resultPtr...)
     */
    bool GetResult(PathResult* resultPtr);

private:
    struct NavServiceBackend;
    // PIMPL to keep thread header out of this header
    std::unique_ptr<NavServiceBackend> impl_;
};