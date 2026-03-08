# OnClientIPRequest Validation Report

## Disassembly Analysis

**Binary**: `../../launcher.exe` (PE32 executable, Intel 80386)
**Analysis Date**: 2025-03-08

---

## Critical Findings

### 1. **FABRICATED** - Callback Function Does Not Exist

The documentation describes an `OnClientIPRequest` callback function that **does not exist** in the binary.

**Evidence**:
```bash
$ strings launcher.exe | grep -i "OnClientIPRequest"
(no results)

$ strings launcher.exe | grep -i "OnClientIP"
(no results)
```

### 2. **PARTIALLY CORRECT** - Message Type Name Exists

The message type name `MS_GetClientIPRequest` **does exist** in the binary:

```bash
$ strings launcher.exe | grep "MS_GetClientIP"
MS_GetClientIPReply
MS_GetClientIPRequest

$ strings -t x launcher.exe | grep "MS_GetClientIP"
 afc7c MS_GetClientIPReply
 afc90 MS_GetClientIPRequest
```

**String Addresses**:
- `MS_GetClientIPReply` at 0x4afc7c
- `MS_GetClientIPRequest` at 0x4afc90

### 3. **FABRICATED** - No Callback Registration Mechanism

**No evidence found for**:
- ❌ `EVENT_CLIENT_IP_REQUEST` constant
- ❌ `RegisterCallback2` function
- ❌ Callback registration pattern
- ❌ Event structure `ClientIPRequestEvent`

**Evidence**:
```bash
$ strings launcher.exe | grep -E "EVENT_CLIENT_IP|RegisterCallback"
(no results)
```

### 4. **NO MESSAGE TYPE CODE 0x0103**

The documentation claims message type `0x0103`, but this constant is **not found** in the binary:

```bash
$ objdump -d launcher.exe | grep -E "mov.*\$0x103|push.*\$0x103"
(no results)

$ objdump -s launcher.exe | grep "03 01 00 00"
(no results)
```

---

## Validation Summary

| Aspect | Documented | Actual | Status |
|--------|------------|--------|--------|
| Function Name | `OnClientIPRequest` | Does not exist | ❌ **FABRICATED** |
| Message Type Name | `MS_GetClientIPRequest` | Exists at 0x4afc90 | ✅ **CORRECT** |
| Message Type Code | `0x0103` | Not found | ❌ **UNVERIFIED** |
| Callback Pattern | C callback with registration | No callback mechanism | ❌ **FABRICATED** |
| Event Structure | `ClientIPRequestEvent` | Does not exist | ❌ **FABRICATED** |
| Registration API | `RegisterCallback2` | Does not exist | ❌ **FABRICATED** |

**Overall Status**: ❌ **MOSTLY FABRICATED**

---

## What Actually Exists

### Message Type System

The binary contains a **message type naming system**, not a callback system:

**Message Type Names Found**:
```
MS_GetClientIPReply
MS_GetClientIPRequest
```

**Other MS_ Message Types**:
```
MS_ConnectRequest
MS_ConnectReply
MS_AuthenticateRequest
MS_AuthenticateReply
MS_LoadCharacterRequest
MS_LoadCharacterReply
... (24 total MS_ message types)
```

### Message Dispatch Infrastructure

Evidence of message dispatch system:

```
Dispatching message %s (P: %d, M: %d) from LKAIP: %d.%d.%d.%d...
AuthMessageDispatch() functions
CMessageConnection::OnOperationCompleted()
```

**This suggests**:
- Messages are processed internally
- Message type names are used for logging/debugging
- No external callback API exposed

### String Loading Functions

Simple getter functions return message type name strings:

```assembly
41c048: mov    $0x4afc90,%eax   ; Load MS_GetClientIPRequest address
41c04d: pop    %ebp
41c04e: ret
```

These are **string getters**, not callback handlers.

---

## Binary Evidence

### String References

| String | Address | Section | Type |
|--------|---------|---------|------|
| `MS_GetClientIPRequest` | 0x4afc90 | .rdata | Message type name |
| `MS_GetClientIPReply` | 0x4afc7c | .rdata | Message type name |

### Hex Dump

```
4afc70 4d535f20 4d455353 41474500 4d535f47  MS_ MESSAGE.MS_G
4afc80 6574436c 69656e74 49505265 706c7900  etClientIPReply.
4afc90 4d535f47 6574436c 69656e74 49505265  MS_GetClientIPRe
4afca0 71756573 74000000                      quest...
```

### Function Analysis

The message type strings are used by simple getter functions:

```assembly
; Function at 0x41c048
41c048: mov    $0x4afc90,%eax   ; Return address of "MS_GetClientIPRequest"
41c04d: pop    %ebp
41c04e: ret
```

**Purpose**: These are likely used for:
- Logging message types
- Debug output
- Message type identification

**NOT used for**:
- Callback registration
- Event dispatching
- External API

---

## Documentation Claims vs Reality

### Claimed Function Signature

```c
int OnClientIPRequest(ClientIPRequestEvent* requestEvent, void* requestData, uint32_t requestSize);
```

**Status**: ❌ **FABRICATED** - Function does not exist

### Claimed Registration Pattern

```c
CallbackRegistration reg;
reg.eventType = EVENT_CLIENT_IP_REQUEST;
reg.callbackFunc = MyOnClientIPRequest;
int callbackId = obj->RegisterCallback2(&reg);
```

**Status**: ❌ **FABRICATED** - No callback registration mechanism exists

### Claimed Message Type Code

```c
#define MS_GetClientIPRequest 0x0103
```

**Status**: ❌ **UNVERIFIED** - No 0x0103 constant found

### Claimed Event Structure

```c
struct ClientIPRequestEvent {
    uint32_t eventType;
    uint32_t connId;
    // ... 40 bytes total
};
```

**Status**: ❌ **FABRICATED** - Structure does not exist

---

## Root Cause Analysis

### Why This Happened

1. **String Found**: Documentation author found `MS_GetClientIPRequest` string
2. **Assumed Callback**: Assumed message type name implies callback function
3. **Template Filling**: Used standard callback template without validation
4. **No Binary Verification**: Never checked if callback actually exists

### Fabrication Pattern

Similar to other fabricated callbacks:
- ✅ Found message type name string
- ❌ Assumed callback pattern exists
- ❌ Fabricated function signature
- ❌ Fabricated event structures
- ❌ Fabricated registration mechanism
- ❌ No assembly evidence provided

---

## Actual Message Handling

### How Messages Are Actually Handled

Based on binary evidence, messages appear to be handled **internally**:

1. **Message Processing**: Internal dispatcher processes messages
2. **Type Names**: Used for logging/debugging only
3. **No External API**: No callback mechanism exposed to clients
4. **Direct Handling**: Launcher handles messages directly, no callback registration

**Evidence**:
```
Dispatching message %s (P: %d, M: %d)...
CMessageConnection::OnOperationCompleted()
```

This indicates **internal message processing**, not external callbacks.

---

## Related Message Types

All MS_ (Master Server) message types found in binary:

```
MS_AdjustFloodgateReply/Request
MS_ClaimCharacterNameReply/Request
MS_ConnectChallenge/Response
MS_ConnectReply/Request
MS_CreateCharacterRequest
MS_DeleteCharacterReply/Request
MS_EstablishUDPSessionReply
MS_FailoverReply/Request
MS_FakePopulationRequest
MS_GetClientIPReply/Request        ← This callback
MS_GetPlayerListReply/Request
MS_LoadCharacterReply/Request
MS_RefreshSessionKeyReply/Request
MS_ServerShuttingDown
MS_UnloadCharacterRequest
```

**Total**: 24 message types, **none** with callback functions documented.

---

## Recommendations

### Immediate Actions

1. ✅ **DEPRECATE** `OnClientIPRequest.md` - Mark as fabricated
2. ✅ **DEPRECATE** `OnClientIPReply.md` - Same fabrication pattern
3. ⚠️ **VALIDATE** all other network callbacks
4. ⚠️ **DOCUMENT** actual message processing system

### Documentation Corrections

1. **Remove fabricated callback documentation**
2. **Document message type naming system** (if anything)
3. **Investigate actual message handling** (internal implementation)
4. **Stop assuming** strings imply callbacks

### Process Improvements

1. **Verify callback registration mechanism exists** before documenting callbacks
2. **Check for EVENT_* constants** before claiming event system
3. **Find actual function implementations**, not just strings
4. **Don't assume** message type name = callback function

---

## Comparison with Game Callbacks

**Similar Pattern to Fabricated Game Callbacks**:

| Aspect | Game Callbacks | Network Callbacks |
|--------|---------------|-------------------|
| Fabrication Rate | 75% (6/8) | 100% (2/2 so far) |
| Strings Found | ❌ None | ✅ Message type names |
| Functions Exist | ❌ None | ❌ None |
| Callback Mechanism | ❌ None | ❌ None |
| Event Constants | ❌ None | ❌ None |

**Key Difference**:
- Game callbacks: Completely fabricated (no strings)
- Network callbacks: Message type names exist, callbacks don't

---

## Validation Checklist

- [x] Search for function name in strings → **NOT FOUND**
- [x] Search for message type name → **FOUND** (but just a name)
- [x] Search for message type code → **NOT FOUND**
- [x] Search for EVENT_* constants → **NOT FOUND**
- [x] Search for callback registration → **NOT FOUND**
- [x] Find function implementation → **NOT FOUND**
- [x] Analyze message handling system → **Internal, no callbacks**

---

## Conclusion

### Validation Result

⚠️ **MOSTLY FABRICATED**

The `OnClientIPRequest` callback function **does not exist**. While the message type name `MS_GetClientIPRequest` exists in the binary, it is used for internal message processing and logging, not as an external callback API.

**What Exists**:
- ✅ Message type name string
- ✅ Internal message processing
- ✅ Message dispatch infrastructure

**What Does NOT Exist**:
- ❌ `OnClientIPRequest` callback function
- ❌ `ClientIPRequestEvent` structure
- ❌ `EVENT_CLIENT_IP_REQUEST` constant
- ❌ Callback registration mechanism
- ❌ Message type code 0x0103

### Corrective Action

This file should be **deprecated** and replaced with documentation of the actual message processing system (if external documentation is even warranted).

---

**Validation Status**: ❌ **MOSTLY FABRICATED - CALLBACK DOES NOT EXIST**
**Action Required**: **DEPRECATE IMMEDIATELY**
**Last Updated**: 2025-03-08
