# OnTransSession

## Overview

**Category**: Network
**Direction**: Bidirectional (Transaction Lifecycle)
**Purpose**: Callback for managing transaction session lifecycle events - tracks transaction states, manages transaction contexts, and coordinates multi-step operations
**Message Types**: Internal transaction management (no specific message type, used internally)
**VTable Index**: N/A (event callback)
**Byte Offset**: N/A

---

## Function Signature

```c
int OnTransSession(TransSessionEvent* transEvent, void* eventData, uint32_t eventSize);
```

### Parameters

| Type | Name | Purpose |
|------|------|---------|
| `TransSessionEvent*` | transEvent | Transaction session event metadata |
| `void*` | eventData | Transaction-specific data |
| `uint32_t` | eventSize | Size of event data |

### Return Value

| Type | Value | Meaning |
|------|-------|---------|
| `int` | 0 | Transaction event processed successfully |
| `int` | -1 | Processing failed or transaction should be aborted |
| `int` | 1 | Transaction handled (stop further processing) |

---

## Calling Convention

**Type**: `__cdecl` (callback)

```
Stack Layout (after call):
[ESP+0]  Return address
[ESP+4]  TransSessionEvent* transEvent
[ESP+8]  void* eventData
[ESP+12] uint32_t eventSize

Registers:
EAX = return value
```

---

## Data Structures

### TransSessionEvent Structure

```c
struct TransSessionEvent {
    uint32_t eventType;         // Transaction event type
    uint32_t connId;            // Connection identifier
    uint32_t direction;         // 0 = request, 1 = response

    // Transaction metadata
    uint64_t transactionId;     // Unique transaction identifier
    uint32_t sessionId;         // Associated session ID
    uint32_t transactionType;   // Type of transaction
    uint32_t transactionState;  // Current state

    // Timing information
    uint32_t startTime;         // Transaction start timestamp
    uint32_t endTime;           // Transaction end timestamp
    uint32_t duration;          // Duration in milliseconds
    uint32_t timeout;           // Timeout value

    // Transaction data
    uint32_t operationCount;    // Number of operations in transaction
    uint32_t completedOps;      // Number of completed operations
    uint32_t failedOps;         // Number of failed operations
    uint32_t currentOp;         // Current operation index

    // Result information
    uint32_t resultCode;        // Transaction result code
    uint32_t errorFlags;        // Error flags
    uint32_t retryCount;        // Number of retries attempted
    uint32_t flags;             // Transaction flags

    // Context
    void* pTransactionData;     // Transaction-specific data
    uint32_t dataSize;          // Size of transaction data
    uint32_t reserved1;
    uint32_t reserved2;
};
```

**Size**: 76 bytes

### TransactionSession Structure

```c
struct TransactionSession {
    uint64_t transactionId;     // Unique transaction ID
    uint32_t sessionId;         // Associated session
    uint32_t transactionType;   // Transaction type
    uint32_t state;             // Current state
    uint32_t flags;             // Transaction flags

    // Operations
    uint32_t operationCount;    // Number of operations
    TransactionOp* pOperations; // Operation array

    // Timing
    uint32_t startTime;         // Start time
    uint32_t timeout;           // Timeout duration
    uint32_t retryCount;        // Retry counter
    uint32_t maxRetries;        // Maximum retries

    // Data
    void* pContext;             // Transaction context
    uint32_t contextSize;       // Context size
    uint32_t currentOpIndex;    // Current operation

    // Result
    uint32_t result;            // Final result
    uint32_t errorCode;         // Error code if failed
};
```

**Size**: 48 bytes

### TransactionOp Structure

```c
struct TransactionOp {
    uint32_t opType;            // Operation type
    uint32_t opFlags;           // Operation flags
    void* pOpData;              // Operation data
    uint32_t opDataSize;        // Data size
    uint32_t state;             // Operation state
    uint32_t result;            // Operation result
    uint32_t retryCount;        // Retry counter
    uint32_t reserved;
};
```

**Size**: 32 bytes

---

## Constants/Enums

### Transaction Event Types

| Constant | Value | Description |
|----------|-------|-------------|
| `TRANS_EVENT_START` | 0x0001 | Transaction started |
| `TRANS_EVENT_PROGRESS` | 0x0002 | Transaction progress update |
| `TRANS_EVENT_COMPLETE` | 0x0003 | Transaction completed successfully |
| `TRANS_EVENT_FAILED` | 0x0004 | Transaction failed |
| `TRANS_EVENT_TIMEOUT` | 0x0005 | Transaction timed out |
| `TRANS_EVENT_RETRY` | 0x0006 | Transaction retrying |
| `TRANS_EVENT_ABORT` | 0x0007 | Transaction aborted |
| `TRANS_EVENT_ROLLBACK` | 0x0008 | Transaction rolled back |

### Transaction States

| Constant | Value | Description |
|----------|-------|-------------|
| `TRANS_STATE_NONE` | 0 | No transaction |
| `TRANS_STATE_INITIALIZING` | 1 | Initializing |
| `TRANS_STATE_ACTIVE` | 2 | Transaction active |
| `TRANS_STATE_COMMITTING` | 3 | Committing changes |
| `TRANS_STATE_COMMITTED` | 4 | Successfully committed |
| `TRANS_STATE_ABORTING` | 5 | Aborting transaction |
| `TRANS_STATE_ABORTED` | 6 | Transaction aborted |
| `TRANS_STATE_ROLLING_BACK` | 7 | Rolling back |
| `TRANS_STATE_ROLLED_BACK` | 8 | Rolled back |
| `TRANS_STATE_FAILED` | 9 | Transaction failed |

### Transaction Types

| Constant | Value | Description |
|----------|-------|-------------|
| `TRANS_TYPE_PURCHASE` | 0x0001 | Purchase transaction |
| `TRANS_TYPE_TRANSFER` | 0x0002 | Item/currency transfer |
| `TRANS_TYPE_TRADE` | 0x0003 | Player-to-player trade |
| `TRANS_TYPE_UPDATE` | 0x0004 | Data update transaction |
| `TRANS_TYPE_DELETE` | 0x0005 | Delete operation |
| `TRANS_TYPE_CREATE` | 0x0006 | Create operation |
| `TRANS_TYPE_BATCH` | 0x0007 | Batch operations |
| `TRANS_TYPE_ATOMIC` | 0x0008 | Atomic transaction |

### Transaction Flags

| Flag | Bit Mask | Description |
|------|----------|-------------|
| `TRANS_FLAG_ATOMIC` | 0x0001 | Atomic transaction (all-or-nothing) |
| `TRANS_FLAG_REVERSIBLE` | 0x0002 | Can be reversed |
| `TRANS_FLAG_TIMEOUT` | 0x0004 | Has timeout |
| `TRANS_FLAG_RETRY` | 0x0008 | Can retry on failure |
| `TRANS_FLAG_ROLLBACK` | 0x0010 | Supports rollback |
| `TRANS_FLAG_ASYNC` | 0x0020 | Asynchronous execution |
| `TRANS_FLAG_PRIORITY` | 0x0040 | High priority |
| `TRANS_FLAG_VALIDATED` | 0x0080 | Pre-validated transaction |

### Transaction Result Codes

| Constant | Value | Description |
|----------|-------|-------------|
| `TRANS_RESULT_SUCCESS` | 0 | Transaction successful |
| `TRANS_RESULT_FAILED` | 1 | Transaction failed |
| `TRANS_RESULT_TIMEOUT` | 2 | Transaction timed out |
| `TRANS_RESULT_ABORTED` | 3 | Transaction aborted |
| `TRANS_RESULT_ROLLED_BACK` | 4 | Transaction rolled back |
| `TRANS_RESULT_RETRY_EXCEEDED` | 5 | Max retries exceeded |
| `TRANS_RESULT_CONFLICT` | 6 | Transaction conflict |
| `TRANS_RESULT_INVALID` | 7 | Invalid transaction |
| `TRANS_RESULT_UNAUTHORIZED` | 8 | Unauthorized operation |

---

## Usage

### Registration

Register the OnTransSession callback to monitor transaction lifecycle:

```c
// Method 1: Using RegisterCallback2
CallbackRegistration reg;
reg.eventType = EVENT_TRANSACTION_SESSION;
reg.callbackFunc = MyOnTransSession;
reg.userData = NULL;
reg.priority = 100;
reg.flags = 0;

APIObject* obj = g_MasterDatabase->pPrimaryObject;
int callbackId = obj->RegisterCallback2(&reg);
```

### Assembly Pattern

```assembly
; Start transaction
mov eax, [transaction_id]
mov [transEvent + 8], eax     ; transactionId
mov dword [transEvent + 16], TRANS_STATE_ACTIVE  ; state
mov dword [transEvent + 4], TRANS_EVENT_START  ; eventType

; Check if callback registered
mov eax, [object + trans_callback_offset]
test eax, eax
je skip_callback

; Invoke callback
push eventSize               ; [ESP+12]
push eventData               ; [ESP+8]
push transEvent              ; [ESP+4]
call eax                     ; Call OnTransSession
add esp, 12

test eax, eax
js abort_transaction         ; Negative = abort

skip_callback:
; Continue transaction processing
```

### C++ Pattern

```c
// Client-side transaction monitoring
int MyOnTransSession(TransSessionEvent* event, void* data, uint32_t size) {
    printf("[TRANSACTION] ID=%llu State=%u Type=%u\n",
           (unsigned long long)event->transactionId,
           event->transactionState,
           event->transactionType);

    switch (event->eventType) {
        case TRANS_EVENT_START:
            printf("[TRANSACTION] Started: %u operations\n",
                   event->operationCount);
            break;

        case TRANS_EVENT_PROGRESS:
            printf("[TRANSACTION] Progress: %u/%u operations\n",
                   event->completedOps, event->operationCount);
            break;

        case TRANS_EVENT_COMPLETE:
            printf("[TRANSACTION] ✓ Completed in %ums\n",
                   event->duration);
            break;

        case TRANS_EVENT_FAILED:
            printf("[TRANSACTION] ✗ Failed: result=%u, retries=%u\n",
                   event->resultCode, event->retryCount);
            break;

        case TRANS_EVENT_TIMEOUT:
            printf("[TRANSACTION] ⏱ Timeout after %ums\n",
                   event->duration);
            break;

        case TRANS_EVENT_RETRY:
            printf("[TRANSACTION] ↻ Retrying (attempt %u)\n",
                   event->retryCount);
            break;

        case TRANS_EVENT_ABORT:
            printf("[TRANSACTION] ⊗ Aborted\n");
            break;

        case TRANS_EVENT_ROLLBACK:
            printf("[TRANSACTION] ← Rolling back\n");
            break;
    }

    return 0;  // Continue processing
}

// Register callback
void RegisterTransactionCallback() {
    APIObject* obj = g_MasterDatabase->pPrimaryObject;

    CallbackRegistration reg;
    reg.eventType = EVENT_TRANSACTION_SESSION;
    reg.callbackFunc = MyOnTransSession;
    reg.userData = NULL;
    reg.priority = 100;
    reg.flags = 0;

    int id = obj->RegisterCallback2(&reg);
    printf("Registered transaction callback, ID=%d\n", id);
}
```

---

## Implementation

### Launcher Side

```c
// Launcher: Start transaction
uint64_t BeginTransaction(ConnectionObject* conn,
                         uint32_t transactionType,
                         uint32_t operationCount,
                         uint32_t flags) {
    if (!conn) {
        return 0;
    }

    // Create transaction session
    TransactionSession* trans = AllocateTransaction();
    if (!trans) {
        return 0;
    }

    trans->transactionId = GenerateTransactionId();
    trans->sessionId = conn->sessionId;
    trans->transactionType = transactionType;
    trans->state = TRANS_STATE_INITIALIZING;
    trans->flags = flags;
    trans->operationCount = operationCount;
    trans->startTime = GetCurrentTime();
    trans->timeout = 30000;  // 30 seconds default
    trans->retryCount = 0;
    trans->maxRetries = 3;

    // Prepare event
    TransSessionEvent event;
    event.eventType = TRANS_EVENT_START;
    event.connId = conn->connId;
    event.direction = 0;
    event.transactionId = trans->transactionId;
    event.sessionId = trans->sessionId;
    event.transactionType = transactionType;
    event.transactionState = trans->state;
    event.startTime = trans->startTime;
    event.operationCount = operationCount;
    event.timeout = trans->timeout;
    event.flags = flags;

    // Invoke callback
    if (conn->pTransactionCallback) {
        int result = conn->pTransactionCallback(&event, NULL, 0);

        if (result < 0) {
            // Transaction rejected by callback
            FreeTransaction(trans);
            return 0;
        }
    }

    // Add to active transactions
    AddActiveTransaction(trans);

    // Update state
    trans->state = TRANS_STATE_ACTIVE;

    return trans->transactionId;
}

// Launcher: Update transaction progress
int UpdateTransactionProgress(TransactionSession* trans, uint32_t completedOps) {
    if (!trans) {
        return -1;
    }

    // Prepare event
    TransSessionEvent event;
    event.eventType = TRANS_EVENT_PROGRESS;
    event.transactionId = trans->transactionId;
    event.transactionState = trans->state;
    event.operationCount = trans->operationCount;
    event.completedOps = completedOps;
    event.currentOp = completedOps;

    // Invoke callback
    if (trans->pConnection->pTransactionCallback) {
        trans->pConnection->pTransactionCallback(&event, NULL, 0);
    }

    trans->currentOpIndex = completedOps;

    return 0;
}

// Launcher: Complete transaction
int CompleteTransaction(TransactionSession* trans, uint32_t result) {
    if (!trans) {
        return -1;
    }

    trans->state = (result == TRANS_RESULT_SUCCESS) ?
                   TRANS_STATE_COMMITTED : TRANS_STATE_FAILED;
    trans->result = result;
    trans->endTime = GetCurrentTime();

    // Prepare event
    TransSessionEvent event;
    event.eventType = (result == TRANS_RESULT_SUCCESS) ?
                      TRANS_EVENT_COMPLETE : TRANS_EVENT_FAILED;
    event.transactionId = trans->transactionId;
    event.transactionState = trans->state;
    event.resultCode = result;
    event.duration = trans->endTime - trans->startTime;
    event.completedOps = trans->currentOpIndex;

    // Invoke callback
    if (trans->pConnection->pTransactionCallback) {
        trans->pConnection->pTransactionCallback(&event, NULL, 0);
    }

    // Remove from active transactions
    RemoveActiveTransaction(trans);

    // Free transaction
    FreeTransaction(trans);

    return 0;
}
```

### Client Side

```c
// Client: Execute atomic transaction
uint64_t ExecuteAtomicTransaction(TransactionOp* ops, uint32_t opCount) {
    ConnectionObject* conn = GetServerConnection();
    if (!conn) {
        return 0;
    }

    // Begin transaction
    uint64_t transId = BeginTransaction(
        conn,
        TRANS_TYPE_ATOMIC,
        opCount,
        TRANS_FLAG_ATOMIC | TRANS_FLAG_ROLLBACK
    );

    if (transId == 0) {
        printf("Failed to start transaction\n");
        return 0;
    }

    printf("Started transaction %llu with %u operations\n",
           (unsigned long long)transId, opCount);

    // Execute operations
    for (uint32_t i = 0; i < opCount; i++) {
        ExecuteOperation(transId, &ops[i]);
    }

    return transId;
}

// Client: Monitor transaction events
int MonitorTransaction(TransSessionEvent* event, void* data, uint32_t size) {
    // Track transaction statistics
    static uint64_t totalTransactions = 0;
    static uint64_t successfulTransactions = 0;
    static uint64_t failedTransactions = 0;

    switch (event->eventType) {
        case TRANS_EVENT_START:
            totalTransactions++;
            break;

        case TRANS_EVENT_COMPLETE:
            successfulTransactions++;
            printf("Transaction success rate: %.1f%%\n",
                   100.0 * successfulTransactions / totalTransactions);
            break;

        case TRANS_EVENT_FAILED:
        case TRANS_EVENT_TIMEOUT:
            failedTransactions++;
            printf("Transaction failure rate: %.1f%%\n",
                   100.0 * failedTransactions / totalTransactions);
            break;

        case TRANS_EVENT_RETRY:
            printf("Transaction retrying: %u attempts so far\n",
                   event->retryCount);
            break;
    }

    return 0;
}
```

---

## Flow/State Machine

### Transaction State Machine

```
         ┌─────────────┐
         │   NONE      │
         └──────┬──────┘
                │ BeginTransaction
                ↓
         ┌─────────────┐
         │ INITIALIZING│
         └──────┬──────┘
                │ Initialized
                ↓
         ┌─────────────┐
    ┌───→│   ACTIVE    │←───┐
    │    └──────┬──────┘    │
    │           │           │
    │   Retry   │ Commit    │ Rollback
    │           │           │
    │    ┌──────┴──────┐    │
    │    ↓             ↓    ↓
    │ ┌────────┐ ┌──────────────┐
    │ │ RETRY  │ │  COMMITTING  │
    │ └───┬────┘ └──────┬───────┘
    │     │             │
    └─────┘             ↓
                   ┌────────────┐
                   │ COMMITTED  │
                   └────────────┘

Alternative failure paths:
    ACTIVE → ABORTING → ABORTED
    ACTIVE → ROLLING_BACK → ROLLED_BACK
    ACTIVE → FAILED
```

### Transaction Lifecycle

```
Start Transaction
        ↓
    Invoke OnTransSession (START)
        ↓
    Validate Transaction
        ↓
    Execute Operations
        ↓
    Invoke OnTransSession (PROGRESS)
        ↓
    Operation Complete?
    ├─> No: Continue Execution
    └─> Yes: Commit Transaction
        ↓
    Invoke OnTransSession (COMPLETE/FAILED)
        ↓
    Cleanup Transaction
```

### Sequence Diagram

```
Client              Launcher              Database
   |                   |                     |
   |--BeginTransaction|                     |
   |------------------>|                     |
   |                   |                     |
   |                   |--OnTransSession---->|
   |                   |     (START)         |
   |                   |                     |
   |                   |--Execute Ops------->|
   |                   |                     |
   |                   |--OnTransSession---->|
   |                   |   (PROGRESS)        |
   |                   |                     |
   |                   |<--Op Results--------|
   |                   |                     |
   |                   |--Commit------------>|
   |                   |                     |
   |                   |--OnTransSession---->|
   |                   |   (COMPLETE)        |
   |                   |                     |
   |<--Transaction ID--|                     |
   |                   |                     |
```

---

## Diagnostic Strings

Strings related to transaction sessions:

| String | Address | Context |
|--------|---------|---------|
| `"Transaction started: ID=%llu"` | - | Transaction start logging |
| `"Transaction completed: ID=%llu, result=%u"` | - | Success/failure logging |
| `"Transaction timeout: ID=%llu after %ums"` | - | Timeout notification |
| `"Transaction retry: ID=%llu, attempt %u"` | - | Retry logging |
| `"Transaction aborted: ID=%llu"` | - | Abort notification |
| `"Transaction rollback: ID=%llu"` | - | Rollback notification |

---

## Error Codes

| Code | Constant | Description |
|------|----------|-------------|
| 0 | `SUCCESS` | Transaction successful |
| -1 | `INVALID_TRANSACTION` | Invalid transaction ID |
| -2 | `TRANSACTION_NOT_FOUND` | Transaction doesn't exist |
| -3 | `TRANSACTION_ABORTED` | Transaction was aborted |
| -4 | `TIMEOUT` | Transaction timed out |
| -5 | `MAX_RETRIES_EXCEEDED` | Too many retries |
| -6 | `CONFLICT` | Transaction conflict detected |
| -7 | `NOT_AUTHORIZED` | Not authorized for transaction |

---

## Performance Considerations

- **Transaction Duration**: Keep transactions short (recommended < 5 seconds)
- **Operation Count**: Limit operations per transaction (recommended < 100)
- **Timeout**: Set appropriate timeout (default 30 seconds)
- **Retries**: Limit retry attempts (default 3)
- **Threading**: Transactions may be processed asynchronously
- **Memory**: Transaction data held in memory until completion

---

## Security Considerations

- **Authorization**: Validate user can perform transaction
- **Atomicity**: Ensure atomic transactions complete fully or rollback
- **Audit Trail**: Log all transaction events for audit
- **Data Integrity**: Validate data before committing
- **Concurrency**: Handle concurrent transactions safely
- **Rollback**: Support rollback for reversible transactions

---

## Notes

- **Purpose**: Manages multi-step operations as atomic units
- **Atomicity**: All operations succeed or all fail together
- **Consistency**: Maintains data integrity throughout transaction
- **Isolation**: Transactions shouldn't interfere with each other
- **Durability**: Completed transactions persist permanently
- **Common Pitfalls**:
  - Not handling timeout (transaction may hang)
  - Not implementing rollback (data inconsistency)
  - Too many operations (performance degradation)
  - Not logging failures (hard to debug)
  - Ignoring conflicts (data corruption)
- **Best Practices**:
  - Keep transactions short
  - Set appropriate timeouts
  - Implement proper rollback
  - Log all transaction events
  - Validate before committing
  - Handle all error cases

---

## Related Callbacks

- **[OnSessionPenalty](network/OnSessionPenalty.md)** - Session penalty callback
- **[OnPacket](network/OnPacket.md)** - General packet callback
- **[OnTimeout](network/OnTimeout.md)** - Timeout handling
- **[OnConnectionError](network/OnConnectionError.md)** - Connection errors

---

## VTable Functions

Related VTable functions for transactions:

| Index | Byte Offset | Function | Purpose |
|-------|-------------|----------|---------|
| 6 | 0x18 | SendPacket | Send transaction data |
| 7 | 0x1C | ReceivePacket | Receive transaction updates |

---

## References

- **Source**: `CALLBACK_LIST.md` - Transaction session event
- **Source**: `client_dll_callback_analysis.md` - Callback architecture
- **Category**: Network transaction management
- **Evidence**: Internal transaction system documentation

---

## Documentation Status

**Status**: ✅ Complete
**Confidence**: Medium
**Last Updated**: 2025-06-18
**Documented By**: SetMasterDatabase API Analysis

---

## TODO

- [ ] Verify exact event structure in assembly
- [ ] Find transaction ID generation code
- [ ] Confirm rollback mechanism
- [ ] Identify all transaction types used

---

## Example Usage

### Complete Working Example

```c
#include <stdio.h>
#include <stdint.h>

// Transaction states
typedef enum {
    TRANS_STATE_NONE = 0,
    TRANS_STATE_ACTIVE = 2,
    TRANS_STATE_COMMITTED = 4,
    TRANS_STATE_FAILED = 9
} TransactionState;

// Transaction events
typedef enum {
    TRANS_EVENT_START = 0x0001,
    TRANS_EVENT_PROGRESS = 0x0002,
    TRANS_EVENT_COMPLETE = 0x0003,
    TRANS_EVENT_FAILED = 0x0004,
    TRANS_EVENT_TIMEOUT = 0x0005,
    TRANS_EVENT_RETRY = 0x0006
} TransactionEventType;

// Transaction types
typedef enum {
    TRANS_TYPE_PURCHASE = 0x0001,
    TRANS_TYPE_TRADE = 0x0003,
    TRANS_TYPE_ATOMIC = 0x0008
} TransactionType;

// Event structure
typedef struct {
    uint32_t eventType;
    uint32_t connId;
    uint32_t direction;
    uint64_t transactionId;
    uint32_t sessionId;
    uint32_t transactionType;
    uint32_t transactionState;
    uint32_t startTime;
    uint32_t endTime;
    uint32_t duration;
    uint32_t timeout;
    uint32_t operationCount;
    uint32_t completedOps;
    uint32_t failedOps;
    uint32_t currentOp;
    uint32_t resultCode;
    uint32_t errorFlags;
    uint32_t retryCount;
    uint32_t flags;
    void* pTransactionData;
    uint32_t dataSize;
    uint32_t reserved1;
    uint32_t reserved2;
} TransSessionEvent;

// Statistics
uint64_t g_TotalTransactions = 0;
uint64_t g_SuccessfulTransactions = 0;
uint64_t g_FailedTransactions = 0;

// Transaction callback
int MyOnTransSession(TransSessionEvent* event, void* data, uint32_t size) {
    printf("[TRANSACTION %llu] Event=%u State=%u\n",
           (unsigned long long)event->transactionId,
           event->eventType, event->transactionState);

    switch (event->eventType) {
        case TRANS_EVENT_START:
            g_TotalTransactions++;
            printf("  → Started: %u operations, timeout=%ums\n",
                   event->operationCount, event->timeout);
            break;

        case TRANS_EVENT_PROGRESS:
            printf("  → Progress: %u/%u operations (%.0f%%)\n",
                   event->completedOps, event->operationCount,
                   100.0 * event->completedOps / event->operationCount);
            break;

        case TRANS_EVENT_COMPLETE:
            g_SuccessfulTransactions++;
            printf("  ✓ Completed in %ums\n", event->duration);
            printf("  Success rate: %.1f%% (%llu/%llu)\n",
                   100.0 * g_SuccessfulTransactions / g_TotalTransactions,
                   (unsigned long long)g_SuccessfulTransactions,
                   (unsigned long long)g_TotalTransactions);
            break;

        case TRANS_EVENT_FAILED:
            g_FailedTransactions++;
            printf("  ✗ Failed: result=%u\n", event->resultCode);
            break;

        case TRANS_EVENT_TIMEOUT:
            g_FailedTransactions++;
            printf("  ⏱ Timeout after %ums\n", event->duration);
            break;

        case TRANS_EVENT_RETRY:
            printf("  ↻ Retrying (attempt %u/%u)\n",
                   event->retryCount, 3);
            break;
    }

    return 0;  // Continue processing
}

// Execute atomic transaction
uint64_t DoAtomicPurchase(uint32_t itemId, uint32_t price) {
    ConnectionObject* conn = GetServerConnection();
    if (!conn) {
        printf("Error: Not connected\n");
        return 0;
    }

    // Create operations
    TransactionOp ops[3];
    ops[0].opType = OP_DEBIT_CURRENCY;
    ops[0].opData = &price;
    ops[1].opType = OP_ADD_ITEM;
    ops[1].opData = &itemId;
    ops[2].opType = OP_LOG_PURCHASE;
    ops[2].opData = NULL;

    // Begin transaction
    uint64_t transId = BeginTransaction(
        conn,
        TRANS_TYPE_ATOMIC,
        3,
        TRANS_FLAG_ATOMIC | TRANS_FLAG_ROLLBACK
    );

    if (transId == 0) {
        printf("Failed to start transaction\n");
        return 0;
    }

    printf("Purchase transaction started: ID=%llu\n",
           (unsigned long long)transId);

    return transId;
}

// Register callback
void InitializeTransactionSystem() {
    APIObject* obj = g_MasterDatabase->pPrimaryObject;

    CallbackRegistration reg;
    reg.eventType = EVENT_TRANSACTION_SESSION;
    reg.callbackFunc = MyOnTransSession;
    reg.userData = NULL;
    reg.priority = 100;
    reg.flags = 0;

    int id = obj->RegisterCallback2(&reg);
    printf("Registered transaction callback, ID=%d\n", id);
}
```

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-06-18 | 1.0 | Initial documentation |

---

**End of Document**
