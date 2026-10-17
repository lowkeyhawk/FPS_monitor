#include "etw_consumer.h"
#include <vector>

// MinGW doesn't define the full EVENT_RECORD layout — define the part we need.
// This matches the documented SDK layout; EventHeader is always the first member.
typedef struct _MY_EVENT_DESCRIPTOR {
    USHORT Id;
    UCHAR Version;
    UCHAR Channel;
    UCHAR Level;
    USHORT Opcode;
    USHORT Task;
    ULONG64 Keyword;
} MY_EVENT_DESCRIPTOR;

typedef struct _MY_EVENT_HEADER {
    USHORT Size;
    USHORT HeaderType;
    USHORT Flags;
    USHORT EventProperty;
    ULONG ThreadId;
    ULONG ProcessId;
    LARGE_INTEGER TimeStamp;
    GUID ProviderId;
    MY_EVENT_DESCRIPTOR EventDescriptor;
} MY_EVENT_HEADER;

typedef struct _MY_EVENT_RECORD {
    MY_EVENT_HEADER EventHeader;
} MY_EVENT_RECORD;

// MinGW doesn't always define these
#ifndef PROCESS_TRACE_MODE_REAL_TIME
#define PROCESS_TRACE_MODE_REAL_TIME 0x00000100
#endif
#ifndef PROCESS_TRACE_MODE_EVENT_RECORD
#define PROCESS_TRACE_MODE_EVENT_RECORD 0x10000000
#endif

#pragma comment(lib, "advapi32.lib")

static const wchar_t kSession[] = L"FpsOverlayEtw";

// Microsoft-Windows-DXGI provider (PresentMon uses this)
static constexpr GUID DxgiGuid =
    { 0xCA11C036, 0x0102, 0x4A2D, { 0xA6, 0xAD, 0xF0, 0x3C, 0xFE, 0xD5, 0xD3, 0xC9 } };
static constexpr USHORT kPresentStartId = 42; // Present_Start

static EtwConsumer* g_instance = nullptr;

static void InitProps(std::vector<BYTE>& buf) {
    buf.assign(sizeof(EVENT_TRACE_PROPERTIES) + sizeof(kSession), 0);
    auto* p = (EVENT_TRACE_PROPERTIES*)buf.data();
    p->Wnode.BufferSize = (ULONG)buf.size();
    p->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    p->Wnode.ClientContext = 1; // QPC clock
    p->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    p->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
}

EtwConsumer::~EtwConsumer() {
    Stop();
}

bool EtwConsumer::Start(std::function<void(uint32_t)> onFps) {
    onFps_ = std::move(onFps);
    g_instance = this;
    lastError_ = 0;

    std::vector<BYTE> buf;
    InitProps(buf);
    ULONG status = StartTraceW(&sessionHandle_, kSession,
                               (EVENT_TRACE_PROPERTIES*)buf.data());
    if (status == ERROR_ALREADY_EXISTS) {
        // Stale session from a previous crash — stop it, then retry
        InitProps(buf);
        ControlTraceW(0, kSession, (EVENT_TRACE_PROPERTIES*)buf.data(),
                      EVENT_TRACE_CONTROL_STOP);
        InitProps(buf);
        status = StartTraceW(&sessionHandle_, kSession,
                             (EVENT_TRACE_PROPERTIES*)buf.data());
    }
    if (status != ERROR_SUCCESS) {
        lastError_ = status;
        g_instance = nullptr;
        return false;
    }

    status = EnableTraceEx2(sessionHandle_, &DxgiGuid,
        EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_INFORMATION,
        0, 0, 0, nullptr);
    if (status != ERROR_SUCCESS) {
        lastError_ = status;
        Stop();
        return false;
    }

    // Configure the consumer
    memset(&trace_, 0, sizeof(trace_));
    trace_.LoggerName = const_cast<LPWSTR>(kSession);
    trace_.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME |
                              PROCESS_TRACE_MODE_EVENT_RECORD;
    trace_.EventRecordCallback = EventRecordCallback;

    consumerHandle_ = OpenTraceW(&trace_);
    if (consumerHandle_ == INVALID_PROCESSTRACE_HANDLE) {
        lastError_ = GetLastError();
        Stop();
        return false;
    }

    running_ = true;
    traceThread_ = std::thread([h = consumerHandle_]() mutable {
        ProcessTrace(&h, 1, nullptr, nullptr);
    });

    // Counting thread: snapshots the present counter every second
    worker_ = std::thread([this]() {
        while (running_) {
            Sleep(1000);
            uint64_t n = eventCount_.exchange(0);
            if (onFps_) onFps_((uint32_t)(n > 9999 ? 9999 : n));
        }
    });

    return true;
}

void EtwConsumer::Stop() {
    running_ = false;
    if (sessionHandle_) {
        EnableTraceEx2(sessionHandle_, &DxgiGuid,
            EVENT_CONTROL_CODE_DISABLE_PROVIDER, 0, 0, 0, 0, nullptr);
        std::vector<BYTE> buf;
        InitProps(buf);
        ControlTraceW(sessionHandle_, kSession,
            (EVENT_TRACE_PROPERTIES*)buf.data(), EVENT_TRACE_CONTROL_STOP);
        sessionHandle_ = 0;
    }
    if (traceThread_.joinable()) traceThread_.join();
    if (consumerHandle_ != INVALID_PROCESSTRACE_HANDLE) {
        CloseTrace(consumerHandle_);
        consumerHandle_ = INVALID_PROCESSTRACE_HANDLE;
    }
    if (worker_.joinable()) worker_.join();
    g_instance = nullptr;
}

VOID WINAPI EtwConsumer::EventRecordCallback(PEVENT_RECORD record) {
    if (!g_instance) return;
    auto* r = (MY_EVENT_RECORD*)record;
    if (r->EventHeader.EventDescriptor.Id == kPresentStartId &&
        IsEqualGUID(r->EventHeader.ProviderId, DxgiGuid)) {
        g_instance->eventCount_.fetch_add(1, std::memory_order_relaxed);
    }
}

DWORD WINAPI EtwConsumer::BufferCallback(PEVENT_TRACE_LOGFILE) {
    return TRUE;
}
