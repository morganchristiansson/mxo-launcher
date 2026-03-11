# SetMasterDatabase Deep Analysis

## The Real Flow

Looking at SetMasterDatabase assembly:

```assembly
; EBX = MasterDatabase pointer passed as parameter
; Global at 0x629f14a0 = internal MasterDatabase

6229d771: mov edi, [0x629f14a0]     ; Get internal MasterDatabase
6229d777: cmp ebx, [edi]             ; Compare with pIdentifier
6229d779: je skip_init              ; Skip if same

; Initialize primary object at offset 0x0C
6229d786: push ebx                   ; Push MasterDatabase pointer!
6229d787: mov ecx, esi              ; ESI = &internal_db->offset_0x0C
6229d789: call 0x6229cb40           ; Initialize linked list

; After init, call vtable[0]
6229d78e: mov eax, [edi]            ; Get pIdentifier (NULL in our case)
6229d790: test eax, eax
6229d792: je skip_vtable            ; Skip if NULL

; Call vtable[0] with pIdentifier
6229d794: mov ecx, [esi]            ; Get primary object pointer
6229d796: mov edx, [ecx]            ; Get vtable
6229d798: push eax                  ; Push pIdentifier
6229d799: call [edx]                ; Call vtable[0]
```

## The Problem

The init function (0x6229cb40) is being passed the **MasterDatabase pointer** (EBX), not the pIdentifier!

This means it's trying to treat our MasterDatabase structure as a linked-list node, accessing:
- [MasterDatabase+0x00] = pIdentifier (NULL)
- [MasterDatabase+0x04] = refCount (1)
- [MasterDatabase+0x08] = stateFlags (1)

Then it tries to do linked-list operations on these values!

## The Solution

The issue is that client.dll's SetMasterDatabase has a different contract than documented. It appears to:

1. Compare the passed pointer with internal state
2. If different, initialize using the passed pointer as a "node" to link into a list
3. The passed structure needs to have valid linked-list fields at offsets 0x00, 0x04, 0x08

We need to either:
- Provide a separate structure for the linked-list node
- OR understand that the MasterDatabase IS the linked-list node

Looking at the documentation again, it says offset 0x00 is "pVTableOrIdentifier" - this suggests it can be EITHER a vtable OR an identifier, but the real usage shows it's used as a linked-list node pointer!

## What We Should Do

The simplest fix: Just pass NULL for everything in MasterDatabase except pPrimaryObject and pSecondaryObject. Let client.dll initialize all the other fields.
