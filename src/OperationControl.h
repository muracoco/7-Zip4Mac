// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <condition_variable>
#include <mutex>
#include <chrono>

// Shared by worker jobs. Pause blocks only the worker, and Cancel always
// wakes it, including when its owner is being destroyed.
class OperationControl {
public:
    bool checkpoint() {
        std::unique_lock<std::mutex> lock(mutex);
        changed.wait(lock, [this] { return !paused || cancelled; });
        return cancelled;
    }
    void cancel() {
        { std::lock_guard<std::mutex> lock(mutex); cancelled = true; paused = false; }
        changed.notify_all();
    }
    void setPaused(bool value) {
        { std::lock_guard<std::mutex> lock(mutex); paused = value && !cancelled; }
        changed.notify_all();
    }
    bool isCancelled() { std::lock_guard<std::mutex> lock(mutex); return cancelled; }
    bool isPaused() { std::lock_guard<std::mutex> lock(mutex); return paused; }
    void setBackground(bool value) {
        { std::lock_guard<std::mutex> lock(mutex); background = value; }
        changed.notify_all();
    }
    bool isBackground() { std::lock_guard<std::mutex> lock(mutex); return background; }
    void waitPausedChange(bool previousBackground) {
        std::unique_lock<std::mutex> lock(mutex);
        changed.wait_for(lock, std::chrono::milliseconds(50), [this, previousBackground] { return !paused || cancelled || background != previousBackground; });
    }
private:
    std::mutex mutex;
    std::condition_variable changed;
    bool paused = false, cancelled = false, background = false;
};
