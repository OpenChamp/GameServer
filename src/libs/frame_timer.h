#pragma once

#include <chrono>

/**
 * FrameTimer that can time frames to within fractions of a millisecond.
 * Create an instance with the desired tick rate, e.g. for 60HZ:
 * 
 * `FrameTimer frame_timer = FrameTimer(60);`
 * 
 * Wherever you need to time your frame:
 *
 * `if(frame_timer.is_frame()) { // A frame has elapsed }`
 */
class FrameTimer {
public:
    /**
     * @param tick_rate The tick rate of this frame timer in HZ
     */
    FrameTimer(int tick_rate) {
        tick_rate_ = tick_rate;
        frame_time_ = 1000.0f / (float)tick_rate_;
        last_time_ = std::chrono::high_resolution_clock::now();
        elapsed_time_ = std::chrono::nanoseconds(0);
    }

    /**
     * Indicates how long one frame is according to this timer
     * @returns The duration of one frame in milliseconds (e.g. 33.33333 for 30HZ)
     */
    float frame_duration_in_ms() {
        return frame_time_;
    }

    /**
     * Check if one frame worth of time has elapsed since the last time this function returned true.
     * If this functions returns true, it also resets the elapsed time and starts counting
     * a new frame immediately!
     * @returns true if one frame has passed
     */

    bool is_frame() {
        std::chrono::time_point now = std::chrono::high_resolution_clock::now();
        elapsed_time_ += now - last_time_;
        last_time_ = now;

        if(elapsed_time_ >= std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<float, std::milli>(frame_time_))) {
            elapsed_time_ -= std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<float, std::milli>(frame_time_));
            return true;
        }
        
        return false;
    }
    
private:
    // Tick rate in hz
    int tick_rate_;

    // Time per frame in ms
    float frame_time_;

    // Last high time is_frame was called
    std::chrono::high_resolution_clock::time_point last_time_;

    // Time elapsed since the last frame
    std::chrono::nanoseconds elapsed_time_;
};