#include <services/navigation_service.hpp>

#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <condition_variable>

#include <libs/log.hpp>
#include <libs/frame_timer.h>

struct NavigationService::NavServiceBackend {
    std::thread worker;
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

            // TODO actually implement pathing lol - ploinky 14/11/2025
            PathResult res = PathResult();
            res.entity_id = req.entity_id;
            res.path = {};

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


NavigationService::NavigationService() {
    impl_ = std::make_unique<NavServiceBackend>();
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
    
bool NavigationService::GetResult(PathResult* resultPtr) {
    std::unique_lock lock(impl_->mtx, std::defer_lock);
    
    if(!lock.try_lock()) {
        return false;
    }
    
    // =================================================================
    // THE LOCK IS LOCKED AND NEEDS TO BE UNLOCKED ASAP
    // =================================================================
    if(impl_->results.empty()) {
        lock.unlock();
        return false;
    }

    *resultPtr = impl_->results.front();
    impl_->results.pop();

    lock.unlock();
    // =================================================================
    // the lock is unlocked, take your time
    // =================================================================

    return true;
}