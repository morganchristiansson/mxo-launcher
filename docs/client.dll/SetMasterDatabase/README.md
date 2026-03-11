# SetMasterDatabase Integration Documentation

## Overview

This directory contains comprehensive documentation about the **SetMasterDatabase** integration mechanism between `launcher.exe` and `client.dll`. This is the **sole API boundary** between the two components.

## Key Files

| File | Description | Size |
|------|-------------|------|
| [`MASTER_DATABASE.md`](./MASTER_DATABASE.md) | **Complete implementation guide** - function signature, data structures, callbacks, full code examples | 34KB |
| [`client_dll_api_discovery.md`](./client_dll_api_discovery.md) | Runtime API discovery mechanism via `SetMasterDatabase` | 13KB |
| [`entry_point_analysis.md`](./entry_point_analysis.md) | Launcher.exe entry point and SetMasterDatabase export analysis | 8KB |
| [`CLIENT_DLL_EXPORTS.md`](./CLIENT_DLL_EXPORTS.md) | Client.dll exported functions including `SetMasterDatabase` | 5KB |
| [`data_structures_analysis.md`](./data_structures_analysis.md) | Master database and API object structure layouts | 10KB |
| [`client_dll_callback_analysis.md`](./client_dll_callback_analysis.md) | Callback system and bidirectional communication | 16KB |
| [`callback_registration_analysis.md`](./callback_registration_analysis.md) | Callback registration patterns and mechanisms | 8KB |
| [`SETMASTERDATABASE_ACTUAL_BEHAVIOR.md`](./SETMASTERDATABASE_ACTUAL_BEHAVIOR.md) | Actual runtime behavior analysis | 6KB |
| [`BREAKTHROUGH_SETMASTERSDATABASE_SOLVED.md`](./BREAKTHROUGH_SETMASTERSDATABASE_SOLVED.md) | Breakthrough findings | 3KB |

## Core Architecture

### Function Signature
```c
void __stdcall SetMasterDatabase(void* pMasterDatabase);
```

- **Address**: `0x004143f0` (launcher.exe) / `0x6229d760` (client.dll)
- **Ordinal**: 1 (only export from launcher.exe)
- **Parameter**: Pointer to 36-byte MasterDatabase structure

### Master Database Structure (36 bytes)
```c
struct MasterDatabase {
    void* pVTableOrIdentifier;   // Offset 0x00
    uint32_t refCount;            // Offset 0x04
    uint32_t stateFlags;          // Offset 0x08
    void* pPrimaryObject;         // Offset 0x0C
    uint32_t primaryData1;        // Offset 0x10
    uint32_t primaryData2;        // Offset 0x12
    void* pSecondaryObject;       // Offset 0x18
    uint32_t secondaryData1;      // Offset 0x1C
    uint32_t secondaryData2;      // Offset 0x20
};
```

### Key Characteristics
- ✅ **Zero static imports** - all discovery happens at runtime
- ✅ **VTable-based dispatch** - 50-100+ functions exposed
- ✅ **Bidirectional callbacks** - launcher→client and client→launcher
- ✅ **36-byte structure** - compact, efficient
- ✅ **Runtime registration** - flexible API updates

## Quick Start

1. **Read `MASTER_DATABASE.md`** for complete implementation guide
2. **Review data structures** in `data_structures_analysis.md`
3. **Understand callbacks** from `client_dll_callback_analysis.md`
4. **Check API discovery** in `client_dll_api_discovery.md`

## See Also

- [`../README.md`](../README.md) - Client.dll documentation root
- [`../../api_surface/`](../../api_surface/) - Original analysis location

---

**Status**: Complete documentation for SetMasterDatabase integration mechanism.