# launcher.exe Reimplementation - Critical Questions

## Date: 2025-03-11

---

## The Core Problem

I have been using **object injection as a hack** instead of understanding and replicating the actual launcher.exe behavior. This is fundamentally wrong for a proper reimplementation.

**The question is: What does launcher.exe ACTUALLY do?**

---

## Critical Unknowns

### 1. How Does launcher.exe Load client.dll?

**Needs investigation:**
- `LoadLibraryA()` or `LoadLibraryW()`?
- Does it verify signatures first?
- Does it check for specific file paths?
- Does it set up error handling before loading?

**Strings found:**
- `"client.dll"` exists in launcher.exe
- `"ErrorClientDLL", "TermClientDLL"` - error handling strings
- `"InitClientDLL", "RunClientDLL"` - export names

### 2. How Does launcher.exe Initialize client.dll?

**Critical question**: Does launcher.exe:
- **Option A**: Pass NULL to SetMasterDatabase (current hack)
- **Option B**: Create and pass a valid MasterDatabase structure
- **Option C**: Use callbacks to initialize incrementally

**Evidence:**
- launcher.exe is MFC-based (MFC71.dll import)
- Has SetMasterDatabase export at 0x143f0
- The function accepts parameter and handles NULL/non-NULL cases

### 3. Does launcher.exe Export SetMasterDatabase Just For client.dll?

**Question**: Is SetMasterDatabase in launcher.exe:
- **Export** (called by client.dll): Allows client to discovery launcher APIs
- **Import** (calls client.dll): Or does it import from client.dll?

`objdump` shows:
- launcher.exe **exports** SetMasterDatabase
- client.dll **imports** SetMasterDatabase
- This means launcher provides API, client discovers it

---

## What We Need To Find

### Tracing Requirements

Must find in launcher.exe disassembly:

1. **LoadLibrary call site**
   - Search for `LoadLibrary` pattern
   - Find which library (client.dll string reference)
   - Check return value handling

2. **GetProcAddress for InitClientDLL**
   - Find where it gets function pointer
   - See what parameters it sets up

3. **Actual call to InitClientDLL**
   - What are the actual parameters?
   - Does it pass argc/argv?
   - What is the `void*` param4?

4. **RunClientDLL call**
   - Does itEnter message loop after?
   - Or does RunClientDLL contain the loop?

5. **Most Importantly: SetMasterDatabase call**
   - Does launcher call client.dll!SetMasterDatabase?
   - What parameter does it pass?
   - Is this the key to proper initialization?

---

## Analysis Of client.dll Behavior

From disassembly:

### SetMasterDatabase(client.dll side)

```asm
6229d760: mov ebx, [esp+4]          ; Get parameter
6229d763: test ebx, ebx             ; Check if NULL
6229d765: je skip_linked_list       ; If NULL, skip
6229d767: ...                       ; Otherwise, treat as linked list
```

If NULL is passed: Creates internal structures
If non-NULL: Treats as linked-list, corrupts memory

**This suggests launcher.exe either:**
1. Passes NULL (our current hack)
2. Passes a special"linked-list node" format we don't understand

---

## Recommended Approach

### Phase 1: Complete launcher.exe Analysis

**Do NOT write more test code until we understand:**

1. **Entry point flow**: Find WinMain or DllMain
2. **client.dll loading**: Actual LoadLibrary call
3. **Function discovery**: GetProcAddress sequence
4. **InitClientDLL call**: Actual parameters and context
5. **SetMasterDatabase interaction**: If any

### Phase 2: Replicate Exactly

**Only after full analysis:**
1. Replicate initialization sequence exactly
2. Replicate parameter passing
3. Replicate callback setup
4. Proper reimplementation, not hacks

---

## Current Status

**Objects discovered (but possibly wrong approach):**
- 0x629f14a0 - MasterDatabase (auto-created)
- 0x629f1748 - TimerObject
- 0x62999968 - StateObject
- 0x629df7d0 - RunClientDLL main object

**But these might be created by launcher.exe before calling RunClientDLL!**

---

## Next Steps

### Immediate Actions Required:

1. **Find launcher.exe's LoadLibraryA/W call**
   - Command: `r2 -qc '/x ff15??90??4a; pd 20' launcher.exe`
   - Look for import thunk calls

2. **Find GetProcAddress usage**
   - Search for "InitClientDLL" string usage
   - Trace backward to find parameter setup

3. **Find actual InitClientDLL call**
   - What parameters are on stack?
   - What's in registers?

4. **Document real flow**
   - Sequence of calls
   - Parameter values
   - Return value handling

**Only then can we properly reimplement.**

---

## Acknowledgment

The injection approach was wrong. We need to understand launcher.exe's actual initialization sequence before attempting to reimplement it.

---

**Status**: Investigation required - searching for actual launcher.exe call sequence