#pragma once
#include <windows.h>
#include <evntrace.h>
#include <functional>
#include <thread>
#include <atomic>

class EtwConsumer {
public:
    EtwConsumer() = default;
    ~EtwConsumer();

    // Starts a real-time ETW session on Microsoft-Windows-DXGI.
    // onFps fires once per second with the present count (FPS).
    bool Start(std::function<void(uint32_t)> onFps);
    void Stop();
    ULONG LastError() const { return lastError_; }

private:
    static VOID WINAPI EventRecordCallback(PEVENT_RECORD record);
    static DWORD WINAPI BufferCallback(PEVENT_TRACE_LOGFILE buffer);

    std::thread worker_;
    std::thread traceThread_;
    std::atomic<bool> running_{ false };
    std::function<void(uint32_t)> onFps_;
    EVENT_TRACE_LOGFILEW trace_ = {};
    TRACEHANDLE sessionHandle_ = 0;
    TRACEHANDLE consumerHandle_ = INVALID_PROCESSTRACE_HANDLE;
    ULONG lastError_ = 0;

    std::atomic<uint64_t> eventCount_{ 0 };
};
