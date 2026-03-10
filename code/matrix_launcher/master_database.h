/**
 * Master Database Interface
 * 
 * This defines the interface between launcher.exe and client.dll
 * Based on reverse engineering documentation in ../../docs/api_surface/MASTER_DATABASE.md
 */

#ifndef MASTER_DATABASE_H
#define MASTER_DATABASE_H

#include <windows.h>
#include <cstdint>

// Forward declarations
struct APIObject;
struct MasterDatabase;

// ============================================================================
// Master Database Structure (36 bytes)
// ============================================================================

struct MasterDatabase {
    void* pVTable;              // Offset 0x00: Virtual function table pointer
    uint32_t refCount;          // Offset 0x04: Reference count
    uint32_t stateFlags;        // Offset 0x08: State flags
    void* pPrimaryObject;       // Offset 0x0C: Primary API object
    uint32_t primaryData1;      // Offset 0x10: Primary object data
    uint32_t primaryData2;      // Offset 0x14: Primary object data
    void* pSecondaryObject;     // Offset 0x18: Secondary API object
    uint32_t secondaryData1;    // Offset 0x1C: Secondary object data
    uint32_t secondaryData2;    // Offset 0x20: Secondary object data
};  // Total: 36 bytes (0x24)

// ============================================================================
// API Object Structure
// ============================================================================

struct APIObject {
    void* pVTable;              // Offset 0x00: Virtual function table
    uint32_t objectId;          // Offset 0x04: Object identifier
    uint32_t objectState;       // Offset 0x08: Object state
    uint32_t flags;             // Offset 0x0C: Object flags
    void* pInternalData;        // Offset 0x10: Internal data pointer
    uint32_t dataSize;          // Offset 0x14: Size of internal data
    void* pCallback1;           // Offset 0x18: Callback function pointer 1
    void* pCallback2;           // Offset 0x1C: Callback function pointer 2
    void* pCallback3;           // Offset 0x20: Callback function pointer 3
    void* pCallbackData;        // Offset 0x24: Callback user data
    uint32_t callbackFlags;     // Offset 0x28: Callback flags
};

// ============================================================================
// VTable Function Types
// ============================================================================

// Lifecycle functions
typedef int (__thiscall *InitializeFunc)(APIObject* obj, void* config);
typedef void (__thiscall *ShutdownFunc)(APIObject* obj);
typedef int (__thiscall *ResetFunc)(APIObject* obj);
typedef uint32_t (__thiscall *GetStateFunc)(APIObject* obj);
typedef int (__thiscall *RegisterCallbackFunc)(APIObject* obj, void* callback, void* userData);
typedef int (__thiscall *SetEventHandlerFunc)(APIObject* obj, uint32_t eventType, void* handler);
typedef uint32_t (__thiscall *GetApplicationStateFunc)(APIObject* obj);

// ============================================================================
// Client.dll Exported Functions
// ============================================================================

typedef void (*ErrorClientDLL_t)(const char* message, int severity);
typedef int (*InitClientDLL_t)(void* param1, void* param2, void* param3, 
                                void* param4, void* param5, int param6, 
                                int param7, void* param8);
typedef void (*RunClientDLL_t)(void);
typedef void (*SetMasterDatabase_t)(void* pMasterDatabase);
typedef void (*TermClientDLL_t)(void);

// ============================================================================
// Launcher Exported Functions (for client.dll to call)
// ============================================================================

// This is what launcher.exe exports (ordinal 1)
extern "C" __declspec(dllexport) void __stdcall SetMasterDatabase(void* pMasterDatabase);

#endif // MASTER_DATABASE_H
