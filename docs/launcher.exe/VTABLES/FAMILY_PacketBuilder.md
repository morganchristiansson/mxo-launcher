# Packet Builder Family (CLTLoginMediator Packet Builder Inheritance)

## Overview

The Packet Builder family is a group of classes that share similar internal field layouts for managing packet/message construction, but have **distinct vtables**. They are related compositionally rather than through C++ inheritance - derived classes call base initializers then override the vtable pointer.

Key insight from static-RE:
- `CLTLoginMediatorSlotRecord_Initialize` (0x4398b0) calls `CLTLoginMediatorPacketBuilderEnvelope_Initialize` (0x439840) first, then sets its own vtable to `0x004b5328`
- This pattern means they share envelope fields but provide different virtual method behavior

## Class Hierarchy

```
┌─────────────────────────────────────────────────────────────┐
│ CMessageConnectionMessageRef (vtable 0x004ba23c)          │
│ - Outer message reference with ref counting                │
│ - Contains pointer to MessageStorage                    │
└─────────────────────────────────────────────────────────────┘
                           │
                           ▼ uses
┌─────────────────────────────────────────────────────────────┐
│ mxo::PacketBuilder_0x4af2a4 (vtable 0x004af2a4)       │
│ - Shared packet builder envelope pattern                   │
│ - Uses CMessageConnectionMessageRef internally       │
│ - Size: 50 bytes (0x32)                           │
│                                                     │
│ Fields:                                            │
│ +0x00: vtable pointer                             │
│ +0x04: nopatchLauncherVersionValue              │
│ +0x08: messageRef (CMessageConnectionMessageRef*) │
│ +0x0c: ownerReadyFlag                            │
│ +0x10: payloadBegin                              │
│ +0x14: payloadLength                           │
│ +0x16: statusByte                              │
│ +0x1c: characterIdLow                         │
│ +0x20: characterIdHigh                        │
│ +0x24: worldId                                │
└─────────────────────────────────────────────────────────────┘
        ▲                            ▲
        │ shares fields             │ distinct vtable
        │ compositionally           │
┌───────┴───────┐          ┌────────┴────────┐
│              │          │                │
▼              ▼          ▼                ▼
┌──────────────────────┐  ┌────────────────────────────────────┐
│ CLTLoginMediator     │  │ CLTLoginMediatorSlotRecord_0x4b5328 │
│ (vtable 0x004b01c8) │  │ (vtable 0x004b5328)               │
│                     │  │                                    │
│ - Main login         │  │ - Character slot record              │
│   mediator         │  │ - Additional fields:               │
│ - Auth + margin    │  │   +0x14: heapString (std::string)  │
│   connection       │  │   +0x1c: globalCharacterIdLow    │
│   owner           │  │   +0x20: globalCharacterIdHigh   │
│                   │  │   +0x18: status                  │
│                   │  │   +0x24: worldId                 │
└──────────────────────┘  └────────────────────────────────────┘
```

## VTable Addresses

| Class | VTable Address | Notes |
|-------|---------------|-------|
| PacketBuilder_0x4af2a4 | 0x004af2a4 | Shared envelope base (ctypes:OOAnalyzer) |
| CLTLoginMediatorSlotRecord_0x4b5328 | 0x004b5328 | Character slot record |
| CLTLoginMediator | 0x004b01c8 | Main login mediator | 

## Initialization Pattern

```
CLTLoginMediatorSlotRecord::Initialize():
    1. Call CLTLoginMediatorPacketBuilderEnvelope_Initialize()  [0x439840]
       - Sets vtable to 0x004af2a4 temporarily
       - Acquires CMessageConnectionMessageRef
    2. Set vtable to 0x004b5328 (our own vtable)
    3. Zero-initialize payload fields
```

## Related VTables (Separate but Related)

These classes use the same message construction infrastructure but have different vtables:

| Address | Class | Description |
|---------|-------|------------|
| 0x004b01c8 | CLTLoginMediator | Main login mediator |
| 0x004b5328 | CLTLoginMediatorSlotRecord_0x4b5328 | Per-character slot |
| 0x004af2a4 | PacketBuilder_0x4af2a4 | Shared envelope |
| 0x004ba23c | CMessageConnectionMessageRef | Message reference |
| 0x004ba208 | CMessageConnectionMessageStorage | Message payload storage |
| 0x004b211c | CRefCountedReadOperationBase | TCP read operation base |

## Source Implementation

The source code models this as separate classes with similar fields rather than C++ inheritance due to include chain dependencies:

- `matrixstaging/game/src/libltclientlogin/loginmediator_base.h` - `SlotRecordState_0x4b5328`
- `matrixstaging/runtime/src/libltmessaging/messageconnection.h` - `PacketBuilder_0x4af2a4` definition, `CMessageConnectionMessageStorage`
- `matrixstaging/runtime/src/liblttcp/lttcpconnection.h` - `CRefCountedReadOperationBase`

## References

- anchor: launcher.exe:0x439840 = CLTLoginMediatorPacketBuilderEnvelope_Initialize
- anchor: launcher.exe:0x4398b0 = CLTLoginMediatorSlotRecord_Initialize  
- anchor: launcher.exe:0x42f820 = CRefCountedReadOperationBase methods
- anchor: launcher.exe:0x455bd0 = CMessageConnectionMessage_CreateRef