# OnClientIPRequest

> ⚠️ **DEPRECATED - FABRICATED DOCUMENTATION**
>
> **Validation Date**: 2025-03-08
> **Status**: ❌ **CALLBACK FUNCTION DOES NOT EXIST IN BINARY**
>
> This callback was fabricated and does not exist in the launcher binary.
>
> See [OnClientIPRequest_validation.md](OnClientIPRequest_validation.md) for detailed disassembly analysis.
>
> **Do not use this documentation.**

---

## Validation Findings

### ❌ Callback Function Does Not Exist

Binary analysis of `../../launcher.exe` confirms:

```bash
$ strings launcher.exe | grep -i "OnClientIPRequest"
(no results)

$ strings launcher.exe | grep -i "OnClientIP"
(no results)
```

### ✅ Message Type Name Exists (But Not Callback)

The message type **name** exists, but not the callback function:

```bash
$ strings launcher.exe | grep "MS_GetClientIP"
MS_GetClientIPReply
MS_GetClientIPRequest    ← Message type name only
```

**What this means**:
- `MS_GetClientIPRequest` is a **message type identifier**
- Used for internal message routing and logging
- **NOT** a callback function name
- No external API for registration

---

## What Actually Exists

### Message Type System

The binary contains a **message type naming system** for internal processing:

| Message Type | String Address | Purpose |
|--------------|----------------|---------|
| `MS_GetClientIPRequest` | 0x4afc90 | Message type name |
| `MS_GetClientIPReply` | 0x4afc7c | Message type name |

These are **message identifiers**, not callback functions.

### Internal Message Processing

Messages are processed **internally** without exposing callbacks:

```
Dispatching message %s (P: %d, M: %d)...
CMessageConnection::OnOperationCompleted()
```

**Key Point**: Message handling is internal to the launcher, not exposed via callbacks.

---

## Fabricated Elements

The following were fabricated without binary evidence:

| Element | Status |
|---------|--------|
| `OnClientIPRequest` function | ❌ Does not exist |
| `ClientIPRequestEvent` structure (40 bytes) | ❌ Does not exist |
| `EVENT_CLIENT_IP_REQUEST` constant | ❌ Does not exist |
| Message type code `0x0103` | ❌ Not found in binary |
| `RegisterCallback2` API | ❌ Does not exist |
| Callback registration pattern | ❌ Does not exist |

---

## Related Message Types

All MS_ (Master Server) message types in binary:

```
MS_GetClientIPReply/Request        ← This fabrication
MS_ConnectReply/Request
MS_AuthenticateReply/Request
MS_LoadCharacterReply/Request
... (24 total message types)
```

**None** have documented callback functions.

---

## Related Callbacks

**Also Fabricated**:
- ❌ [OnClientIPReply.md](OnClientIPReply.md) - Does not exist

**Needs Validation**:
- ⚠️ All other network callbacks likely fabricated

---

## References

- **Validation Report**: [OnClientIPRequest_validation.md](OnClientIPRequest_validation.md)
- **Binary**: `../../launcher.exe` (PE32 executable, Intel 80386)
- **Analysis Method**: String search, disassembly analysis
- **Status**: Callback function does not exist

---

## Documentation Status

**Status**: ❌ **DEPRECATED - FABRICATED**
**Last Updated**: 2025-03-08
**Validator**: Binary Analysis
**Action**: Do not use
