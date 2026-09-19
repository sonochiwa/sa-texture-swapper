#include "watcher.h"

#include <windows.h>

#include <vector>


namespace {
constexpr uint64_t kDebounceMs = 300;
}

bool Watcher::Start(const std::wstring& root) {
    if (running_)
        return true;

    stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!stopEvent_)
        return false;

    running_ = true;
    thread_  = std::thread(&Watcher::Run, this, root);
    return true;
}

void Watcher::Stop() {
    if (!running_)
        return;

    running_ = false;
    if (stopEvent_)
        SetEvent(stopEvent_);
    if (thread_.joinable())
        thread_.join();
    if (stopEvent_) {
        CloseHandle(stopEvent_);
        stopEvent_ = nullptr;
    }
}

bool Watcher::ConsumePending() {
    const uint64_t last = lastChange_.load();
    if (last == 0)
        return false;
    if (GetTickCount64() - last < kDebounceMs)
        return false;
    lastChange_.store(0);
    return true;
}

void Watcher::Run(std::wstring root) {
    HANDLE dir = CreateFileW(root.c_str(), FILE_LIST_DIRECTORY,
                             FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                             OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                             nullptr);
    if (dir == INVALID_HANDLE_VALUE) {
        return;
    }


    HANDLE ioEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ioEvent) {
        CloseHandle(dir);
        return;
    }

    std::vector<uint8_t> buffer(64 * 1024);
    const DWORD          filter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                         FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE;

    while (running_) {
        OVERLAPPED overlapped{};
        overlapped.hEvent = ioEvent;
        ResetEvent(ioEvent);

        if (!ReadDirectoryChangesW(dir, buffer.data(), static_cast<DWORD>(buffer.size()), TRUE,
                                   filter, nullptr, &overlapped, nullptr)) {
            break;
        }

        HANDLE      handles[2] = {stopEvent_, ioEvent};
        const DWORD wait       = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        if (wait != WAIT_OBJECT_0 + 1) {
            CancelIo(dir);
            WaitForSingleObject(ioEvent, 1000);
            break;
        }

        DWORD transferred = 0;
        if (GetOverlappedResult(dir, &overlapped, &transferred, FALSE))
            lastChange_.store(GetTickCount64());
    }

    CloseHandle(ioEvent);
    CloseHandle(dir);
}
