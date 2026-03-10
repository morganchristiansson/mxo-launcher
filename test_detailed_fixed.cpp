/**
 * Fixed version with proper structure layout
 */

#include <windows.h>
#include <iostream>
#include <cstdio>
#include <cstdint>
#include <cstring>

// Forward declarations
struct MasterDatabase;
struct APIObject;

// VTable function types
typedef int (__thiscall *InitFunc)(APIObject*, void*);
typedef void (__thiscall *ShutdownFunc)(APIObject*);
typedef uint32_t (__thiscall *GetStateFunc)(APIObject*);
typedef int (__thiscall *RegisterCallbackFunc)(APIObject*, void*, void*);
typedef int (__thiscall *SetEventHandlerFunc)(APIObject*, uint32_t, void*);

// MasterDatabase structure - 36 bytes
struct MasterDatabase {
    void* pIdentifier;           // 0x00: Unique identifier (not vtable!)
    uint32_t refCount;           // 0x04: Reference count
    uint32_t stateFlags;         // 0x08: State flags
    APIObject* pPrimaryObject;   // 0x0C: Primary API object
    uint32_t primaryData1;       // 0x10
    uint32_t primaryData2;       // 0x14
    APIObject* pSecondaryObject; // 0x18: Secondary API object
    uint32_t secondaryData1;     // 0x1C
    uint32_t secondaryData2;     // 0x20
}; // Total: 36 bytes (0x24)

// APIObject structure
struct APIObject {
    void* pVTable;               // 0x00: Virtual function table
    uint32_t objectId;           // 0x04: Object ID
    uint32_t objectState;        // 0x08: Object state
    uint32_t flags;              // 0x0C: Flags
    void* pInternalData;         // 0x10: Internal data
    uint32_t dataSize;           // 0x14: Data size
    void* pCallback1;            // 0x18: Callback 1
    void* pCallback2;            // 0x1C: Callback 2
    void* pCallback3;            // 0x20: Callback 3
    void* pCallbackData;         // 0x24: Callback user data
    uint32_t callbackFlags;      // 0x28: Callback flags
    uint32_t reserved1;          // 0x2C
    uint32_t reserved2;          // 0x30
    CRITICAL_SECTION criticalSection; // 0x34: Critical section!
};

// Typedefs for client.dll functions
typedef void (*SetMasterDatabaseFunc)(void*);
typedef int (*InitClientDLLFunc)(void*, void*, HMODULE, void*, MasterDatabase*, uint32_t, uint32_t, void*);
typedef void (*RunClientDLLFunc)();
typedef void (*TermClientDLLFunc)();

// Global objects
static FILE* g_LogFile = NULL;
static MasterDatabase g_OurMasterDB;
static APIObject g_PrimaryObject;
static APIObject g_SecondaryObject;
static void* g_PrimaryVTable[30] = {0};
static void* g_SecondaryVTable[30] = {0};
static uint32_t g_Identifier = 0x12345678; // Unique identifier

// Critical section for APIObject
static CRITICAL_SECTION g_ObjectCriticalSection;

// VTable implementations
int __thiscall Launcher_Initialize(APIObject* obj, void* config) {
    fprintf(g_LogFile, "[CALLBACK] vtable[0] Initialize called!\n");
    fprintf(g_LogFile, "  obj=%p, config=%p\n", obj, config);
    if (obj) {
        fprintf(g_LogFile, "  obj->pVTable=%p, obj->objectId=0x%08X\n", 
                obj->pVTable, obj->objectId);
    }
    fflush(g_LogFile);
    return 0;
}

void __thiscall Launcher_Shutdown(APIObject* obj) {
    fprintf(g_LogFile, "[CALLBACK] vtable[1] Shutdown called!\n");
    fprintf(g_LogFile, "  obj=%p\n", obj);
    fflush(g_LogFile);
}

uint32_t __thiscall Launcher_GetState(APIObject* obj) {
    fprintf(g_LogFile, "[CALLBACK] vtable[3] GetState called!\n");
    fflush(g_LogFile);
    return 1;
}

int __thiscall Launcher_RegisterCallback(APIObject* obj, void* callback, void* userData) {
    fprintf(g_LogFile, "[CALLBACK] vtable[4] RegisterCallback: callback=%p, userData=%p\n", callback, userData);
    if (obj) {
        obj->pCallback1 = callback;
        obj->pCallbackData = userData;
    }
    fflush(g_LogFile);
    return 1;
}

int __thiscall Launcher_SetEventHandler(APIObject* obj, uint32_t eventType, void* handler) {
    fprintf(g_LogFile, "[CALLBACK] vtable[23] SetEventHandler: eventType=%u, handler=%p\n", eventType, handler);
    fflush(g_LogFile);
    return 0;
}

int main(int argc, char* argv[]) {
    g_LogFile = fopen("/tmp/detailed_log_fixed.txt", "w");

    fprintf(g_LogFile, "Fixed Call Flow Analysis\n");
    fprintf(g_LogFile, "========================\n\n");

    // Pre-load dependencies
    const char* preload_dlls[] = {
        "MFC71.dll", "MSVCR71.dll", "dbghelp.dll",
        "r3d9.dll", "binkw32.dll", NULL
    };

    for (int i = 0; preload_dlls[i]; i++) {
        HMODULE h = LoadLibraryA(preload_dlls[i]);
        fprintf(g_LogFile, "Preload: %s %s\n", preload_dlls[i], h ? "OK" : "FAILED");
    }

    // Initialize critical section
    InitializeCriticalSection(&g_ObjectCriticalSection);

    // Set up vtables
    fprintf(g_LogFile, "\n=== Setting Up VTables ===\n");
    g_PrimaryVTable[0] = (void*)Launcher_Initialize;
    g_PrimaryVTable[1] = (void*)Launcher_Shutdown;
    g_PrimaryVTable[3] = (void*)Launcher_GetState;
    g_PrimaryVTable[4] = (void*)Launcher_RegisterCallback;
    g_PrimaryVTable[23] = (void*)Launcher_SetEventHandler;
    
    // Copy to secondary vtable
    memcpy(g_SecondaryVTable, g_PrimaryVTable, sizeof(g_PrimaryVTable));

    fprintf(g_LogFile, "Primary VTable at: %p\n", g_PrimaryVTable);
    fprintf(g_LogFile, "Secondary VTable at: %p\n", g_SecondaryVTable);

    // Set up Primary APIObject
    fprintf(g_LogFile, "\n=== Setting Up Primary APIObject ===\n");
    memset(&g_PrimaryObject, 0, sizeof(g_PrimaryObject));
    g_PrimaryObject.pVTable = g_PrimaryVTable;
    g_PrimaryObject.objectId = 0x00000001;
    g_PrimaryObject.objectState = 0x00000001;
    g_PrimaryObject.flags = 0;
    g_PrimaryObject.pInternalData = NULL;
    g_PrimaryObject.dataSize = 0;
    g_PrimaryObject.pCallback1 = NULL;
    g_PrimaryObject.pCallback2 = NULL;
    g_PrimaryObject.pCallback3 = NULL;
    g_PrimaryObject.pCallbackData = NULL;
    g_PrimaryObject.callbackFlags = 0;
    memcpy(&g_PrimaryObject.criticalSection, &g_ObjectCriticalSection, sizeof(CRITICAL_SECTION));
    
    fprintf(g_LogFile, "Primary Object at: %p\n", &g_PrimaryObject);
    fprintf(g_LogFile, "  pVTable: %p\n", g_PrimaryObject.pVTable);
    fprintf(g_LogFile, "  objectId: 0x%08X\n", g_PrimaryObject.objectId);
    fprintf(g_LogFile, "  criticalSection at offset: 0x%02X\n", 
            (uint32_t)((char*)&g_PrimaryObject.criticalSection - (char*)&g_PrimaryObject));

    // Set up Secondary APIObject
    fprintf(g_LogFile, "\n=== Setting Up Secondary APIObject ===\n");
    memset(&g_SecondaryObject, 0, sizeof(g_SecondaryObject));
    g_SecondaryObject.pVTable = g_SecondaryVTable;
    g_SecondaryObject.objectId = 0x00000002;
    g_SecondaryObject.objectState = 0x00000001;
    memcpy(&g_SecondaryObject.criticalSection, &g_ObjectCriticalSection, sizeof(CRITICAL_SECTION));
    
    fprintf(g_LogFile, "Secondary Object at: %p\n", &g_SecondaryObject);

    // Set up Master Database - MINIMAL VERSION
    // Let client.dll initialize its own internal structures
    fprintf(g_LogFile, "\n=== Setting Up Master Database (Minimal) ===\n");
    memset(&g_OurMasterDB, 0, sizeof(g_OurMasterDB));
    // Only set the object pointers - everything else let client.dll init
    g_OurMasterDB.pPrimaryObject = &g_PrimaryObject;
    g_OurMasterDB.pSecondaryObject = &g_SecondaryObject;

    fprintf(g_LogFile, "Master DB at: %p (size: %zu bytes)\n", &g_OurMasterDB, sizeof(g_OurMasterDB));
    fprintf(g_LogFile, "  All other fields initialized to 0/NULL\n");
    fprintf(g_LogFile, "  pPrimaryObject: %p\n", g_OurMasterDB.pPrimaryObject);
    fprintf(g_LogFile, "  pSecondaryObject: %p\n", g_OurMasterDB.pSecondaryObject);

    // Load client.dll
    HMODULE hClient = LoadLibraryA("client.dll");
    if (!hClient) {
        fprintf(g_LogFile, "ERROR: Could not load client.dll\n");
        fclose(g_LogFile);
        return 1;
    }

    fprintf(g_LogFile, "\nclient.dll at: %p\n", hClient);

    // Get exports
    auto setMasterDB = (SetMasterDatabaseFunc)GetProcAddress(hClient, "SetMasterDatabase");
    auto init = (InitClientDLLFunc)GetProcAddress(hClient, "InitClientDLL");
    auto run = (RunClientDLLFunc)GetProcAddress(hClient, "RunClientDLL");
    auto term = (TermClientDLLFunc)GetProcAddress(hClient, "TermClientDLL");

    fprintf(g_LogFile, "Exports:\n");
    fprintf(g_LogFile, "  SetMasterDatabase: %p\n", setMasterDB);
    fprintf(g_LogFile, "  InitClientDLL: %p\n", init);
    fprintf(g_LogFile, "  RunClientDLL: %p\n", run);

    if (!setMasterDB || !init) {
        fclose(g_LogFile);
        return 1;
    }

    // Call SetMasterDatabase - TRY NULL FIRST
    fprintf(g_LogFile, "\n=== CALLING client.dll!SetMasterDatabase with NULL ===\n");
    fprintf(g_LogFile, "Testing if client.dll can create its own structures...\n");
    fflush(g_LogFile);

    setMasterDB(NULL);

    fprintf(g_LogFile, "\nReturned from client.dll!SetMasterDatabase(NULL)\n");

    // Call InitClientDLL
    fprintf(g_LogFile, "\n=== CALLING InitClientDLL ===\n");
    fflush(g_LogFile);

    int result = init(nullptr, nullptr, hClient, nullptr, &g_OurMasterDB, 0, 0, nullptr);

    fprintf(g_LogFile, "InitClientDLL returned: %d\n", result);

    if (result == 0) {
        fprintf(g_LogFile, "\n=== CALLING RunClientDLL ===\n");
        fflush(g_LogFile);
        run();
        fprintf(g_LogFile, "RunClientDLL completed\n");
    }

    if (term) {
        fprintf(g_LogFile, "\n=== CALLING TermClientDLL ===\n");
        term();
    }
    
    FreeLibrary(hClient);
    DeleteCriticalSection(&g_ObjectCriticalSection);
    fclose(g_LogFile);
    return 0;
}
