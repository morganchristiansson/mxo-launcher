# client.dll!SetMasterDatabase - CORRECT Usage

## Date: 2025-03-11
## Status: CORRECTED

---

## CRITICAL: Pass NULL to SetMasterDatabase

### ❌ WRONG (Old Documentation)
```c
MasterDatabase db;
// ... initialize all fields ...
SetMasterDatabase(&db);  // CRASHES!
```

### ✅ CORRECT (Validated via Disassembly)
```c
SetMasterDatabase(NULL);   // Works!
```

---

## Why Non-NULL Crashes

From disassembly at `client.dll+0x6229d760`:

1. **If param != NULL**: Treats it as linked-list node, modifies offsets 0x00-0x08
2. **Corrupts structure**: Overwrites pPrimaryObject at offset 0x0C
3. **Crashes**: When it tries to call vtable[0] on corrupted pointer

The offsets 0x00, 0x04, 0x08 are treated as linked-list pointers:
- `next` node pointer
- `prev` node pointer  
- list data

Not as MasterDatabase fields!

---

## Correct Initialization Sequence

```c
// 1. Pre-load dependencies
LoadLibraryA("MFC71.dll");
LoadLibraryA("MSVCR71.dll");
LoadLibraryA("dbghelp.dll");
LoadLibraryA("r3d9.dll");
LoadLibraryA("binkw32.dll");

// 2. Load client.dll
HMODULE hClient = LoadLibraryA("client.dll");

// 3. Get exports
auto setMasterDB = GetProcAddress(hClient, "SetMasterDatabase");
auto init = GetProcAddress(hClient, "InitClientDLL");
auto run = GetProcAddress(hClient, "RunClientDLL");

// 4. Call SetMasterDatabase with NULL
setMasterDB(NULL);  // ✅ Creates internal MasterDatabase at 0x629f14a0

// 5. Init client
int result = init(argc, argv, hClient, NULL, NULL, 0, 0, NULL);

// 6. Inject global objects (see GLOBAL_OBJECTS.md)
// - TimerObject at 0x629f1748
// - StateObject at 0x62999968

// 7. Run
if (result == 0) {
    run();
}
```

---

## Key Files

| File | Description | Status |
|------|-------------|--------|
| `CORRECT_USAGE.md` | This document | ✅ Current |
| `MASTER_DATABASE.md` | OLD - says pass structure | ❌ Superceded |
| `GLOBAL_OBJECTS.md` | Objects to inject after init | ✅ Current |
| `TEST_RESULTS.md` | Actual test results | ✅ Current |

---

## Related

- `../../launcher.exe/SetMasterDatabase/` - Launcher side docs
- Source of truth: disassembly `client.dll+0x6229d760`

---

**Status**: Documentation corrected based on runtime validation
