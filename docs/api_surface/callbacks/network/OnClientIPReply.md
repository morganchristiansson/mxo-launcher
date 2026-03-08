# OnClientIPReply

> ⚠️ **DEPRECATED - FABRICATED DOCUMENTATION**
>
> **Validation Date**: 2025-03-08
> **Status**: ❌ **CALLBACK FUNCTION DOES NOT EXIST IN BINARY**
>
> This callback was fabricated and does not exist in the launcher binary.
>
> See [OnClientIPReply_validation.md](OnClientIPReply_validation.md) for detailed disassembly analysis.
>
> **Do not use this documentation.**

---

## Validation Findings

### ❌ Callback Function Does Not Exist

Binary analysis of `../../launcher.exe` confirms:

```bash
$ strings launcher.exe | grep -i "OnClientIPReply"
(no results)

$ strings launcher.exe | grep -i "OnClientIP"
(no results)
```

### ✅ Message Type Name Exists (But Not Callback)

The message type **name** exists, but not the callback function:

```bash
$ strings launcher.exe | grep "MS_GetClientIP"
MS_GetClientIPReply      ← Message type name only
MS_GetClientIPRequest
```

**What this means**:
- `MS_GetClientIPReply` is a **message type identifier**
- Used for internal message routing and logging
- **NOT** a callback function name
- No external API for registration

---

## What Actually Exists

### Message Type System

The binary contains a **message type naming system** for internal processing:

| Message Type | String Address | Purpose |
|--------------|----------------|---------|
| `MS_GetClientIPReply` | 0x4afc7c | Message type name |
| `MS_GetClientIPRequest` | 0x4afc90 | Message type name |

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
| `OnClientIPReply` function | ❌ Does not exist |
| `ClientIPReplyEvent` structure (52 bytes) | ❌ Does not exist |
| `EVENT_CLIENT_IP_REPLY` constant | ❌ Does not exist |
| Message type code `0x0104` | ❌ Not found in binary |
| `RegisterCallback2` API | ❌ Does not exist |
| NAT type detection API | ❌ Does not exist |

---

## Related Message Types

Both IP-related message types are **names only**, not callbacks:

```
MS_GetClientIPReply      ← This fabrication
MS_GetClientIPRequest    ← Also fabricated
```

**Neither** has a callback function in the binary.

---

## Related Callbacks

**Also Fabricated**:
- ❌ [OnClientIPRequest.md](OnClientIPRequest.md) - Does not exist

**Needs Validation**:
- ⚠️ All other network callbacks likely fabricated

---

## References

- **Validation Report**: [OnClientIPReply_validation.md](OnClientIPReply_validation.md)
- **Binary**: `../../launcher.exe` (PE32 executable, Intel 80386)
- **Analysis Method**: String search, disassembly analysis
- **Status**: Callback function does not exist

---

## Documentation Status

**Status**: ❌ **DEPRECATED - FABRICATED**
**Last Updated**: 2025-03-08
**Validator**: Binary Analysis
**Action**: Do not use
