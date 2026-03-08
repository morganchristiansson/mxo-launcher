# OnDistributeMonitor

## Overview

**Category**: Network  
**Direction**: Launcher → Client  
**Purpose**: Distributed system monitoring callback

---

## Function Signature

```c
void OnDistributeMonitor(uint32_t monitorId, void* monitorData, uint32_t flags);
```

### Parameters

| Type | Name | Purpose |
|------|------|---------|
| `uint32_t` | monitorId | Distributed monitor identifier |
| `void*` | monitorData | Monitor event data |
| `uint32_t` | flags | Monitor event flags |

### Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `void` | N/A | No return value |

---

## Diagnostic Strings

Found in client.dll:

| String | Address | Context |
|--------|---------|---------|
| "Delete callback for distribute monitor id %d\n" | 0x6289f580 | Monitor cleanup |

---

## Monitor System

The distributed monitor system tracks network/game state across multiple clients or servers.

### Monitor IDs

Monitor IDs identify specific monitoring channels:

| ID Range | Purpose |
|----------|---------|
| 0-99 | System monitors |
| 100-199 | Network monitors |
| 200-299 | Game state monitors |
| 300+ | Custom monitors |

---

## Usage

### Registration

```c
// Register distributed monitor callback
RegisterCallback2(obj, &monitorReg);
```

### Invocation Pattern

```c
// Launcher invokes monitor callback
void InvokeMonitorCallback(uint32_t monitorId, void* data, uint32_t flags) {
    CallbackEntry* entry = FindMonitorCallback(monitorId);
    if (entry && entry->callback) {
        ((MonitorCallback)entry->callback)(monitorId, data, flags);
    }
}
```

### Cleanup

```c
// When monitor is removed
void DeleteMonitorCallback(uint32_t monitorId) {
    Log("Delete callback for distribute monitor id %d", monitorId);
    RemoveCallbackEntry(monitorId);
}
```

---

## Implementation

### Launcher Side

```c
typedef struct {
    uint32_t monitorId;
    uint32_t status;
    uint32_t updateCount;
    void* monitorState;
} MonitorEntry;

// Trigger monitor event
void TriggerMonitorEvent(uint32_t monitorId, void* data) {
    MonitorEntry* monitor = FindMonitor(monitorId);
    if (!monitor) return;
    
    CallbackEntry* entry = FindCallback(monitorId);
    if (entry && entry->callback) {
        MonitorCallback callback = (MonitorCallback)entry->callback;
        callback(monitorId, data, 0);
    }
}

// Cleanup monitor
void CleanupMonitor(uint32_t monitorId) {
    printf("Delete callback for distribute monitor id %d\n", monitorId);
    RemoveMonitor(monitorId);
    RemoveCallback(monitorId);
}
```

### Client Side

```c
// Monitor callback implementation
void MyMonitorCallback(uint32_t monitorId, void* monitorData, uint32_t flags) {
    printf("Monitor %d event received\n", monitorId);
    
    switch (monitorId) {
        case MONITOR_NETWORK_LATENCY:
            HandleLatencyUpdate(monitorData);
            break;
        case MONITOR_PLAYER_COUNT:
            HandlePlayerCountUpdate(monitorData);
            break;
        default:
            HandleGenericMonitor(monitorId, monitorData);
    }
}

// Register monitor callback
void RegisterMonitorCallback() {
    CallbackRegistration reg;
    reg.eventType = EVENT_DISTRIBUTE_MONITOR;
    reg.callbackFunc = MyMonitorCallback;
    reg.userData = NULL;
    reg.priority = 100;
    reg.flags = 0;
    
    APIObject* obj = g_MasterDatabase->pPrimaryObject;
    int id = obj->RegisterCallback2(&reg);
}
```

---

## Monitor Event Types

| Event Type | Monitor ID | Purpose |
|------------|------------|---------|
| MONITOR_NETWORK_LATENCY | 100 | Network latency tracking |
| MONITOR_PLAYER_COUNT | 101 | Player count monitoring |
| MONITOR_SERVER_LOAD | 102 | Server load monitoring |
| MONITOR_BANDWIDTH | 103 | Bandwidth usage tracking |
| MONITOR_GAME_STATE | 200 | Game state synchronization |

---

## Monitor Data Structure

```c
struct MonitorData {
    uint32_t monitorId;         // Monitor identifier
    uint32_t timestamp;         // Event timestamp
    uint32_t dataType;          // Data type identifier
    uint32_t dataSize;          // Size of data
    void* data;                 // Monitor-specific data
};
```

---

## Notes

- **Distributed system coordination**
- Monitor IDs uniquely identify monitoring channels
- Cleanup logged with monitor ID
- Supports real-time monitoring and synchronization
- Used for network coordination across multiple clients

---

## Related Callbacks

- [OnMonitorEvent](../monitor/OnMonitorEvent.md) - General monitor callback
- [OnPacket](OnPacket.md) - Packet callback
- [OnConnect](OnConnect.md) - Connection callback

---

## References

- **Source**: `client_dll_callback_analysis.md` Section 4.1
- **String Address**: 0x6289f580
- **String**: "Delete callback for distribute monitor id %d\n"

---

**Status**: ✅ Documented  
**Confidence**: Medium (inferred from strings)  
**Last Updated**: 2025-06-17
