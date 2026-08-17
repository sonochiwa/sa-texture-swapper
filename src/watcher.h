#pragma once

#include <atomic>
#include <string>
#include <thread>

// Watches the textures folder and reports (debounced) that something changed.
class Watcher {
public:
    ~Watcher() { Stop(); }

    bool Start(const std::wstring& root);
    void Stop();

    // Called from the game thread: true once the folder has been quiet for a moment
    // after a change.
    bool ConsumePending();

private:
    void Run(std::wstring root);

    std::thread           thread_;
    void*                 stopEvent_  = nullptr;
    std::atomic<uint64_t> lastChange_{0}; // GetTickCount64 of the last event, 0 = idle
    std::atomic<bool>     running_{false};
};
