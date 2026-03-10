/**
 * Test with MINIMAL Master Database - all NULL except essential fields
 */

#include <windows.h>
#include <iostream>
#include <cstdio>
#include "master_database.h"

static FILE* g_LogFile = NULL;
static MasterDatabase g_OurMasterDB;
static APIObject g_PrimaryObject;
static void* g_PrimaryVTable[30] = {0};

int __thiscall Launcher_Initialize(APIObject* obj, void* config) {
    fprintf(g_LogFile, "[Launcher] Initialize\n");
    return 0;
}

void __thiscall Launcher_Shutdown(APIObject* obj) {}
uint32_t __thiscall Launcher_GetState(APIObject* obj) { return 1; }
int __thiscall Launcher_RegisterCallback(APIObject* obj, void* callback, void* userData) { return 1; }
int __thiscall Launcher_SetEventHandler(APIObject* obj, uint32_t eventType, void* handler) { return 0; }

extern "C" __declspec(dllexport) void __stdcall SetMasterDatabase(void* pMasterDatabase) {
    fprintf(g_LogFile, "\n=== SetMasterDatabase CALLED BY CLIENT.DLL ===\n");
    fprintf(g_LogFile, "Client provided: %p\n", pMasterDatabase);
    std::cout << "=== SetMasterDatabase CALLED! ===" << std::endl;
}

int main(int argc, char* argv[]) {
    g_LogFile = fopen("/tmp/test_null_db.log", "w");

    fprintf(g_LogFile, "Test with MINIMAL Master Database (mostly NULL)\n");
    fprintf(g_LogFile, "================================================\n\n");

    // Pre-load dependencies
    const char* preload_dlls[] = {
        "MFC71.dll", "MSVCR71.dll", "dbghelp.dll",
        "r3d9.dll", "binkw32.dll", NULL
    };

    for (int i = 0; preload_dlls[i]; i++) {
        HMODULE h = LoadLibraryA(preload_dlls[i]);
        fprintf(g_LogFile, "  %s: %s\n", preload_dlls[i], h ? "OK" : "FAILED");
    }

    // Create MINIMAL Master Database - almost everything NULL
    fprintf(g_LogFile, "\n=== Creating MINIMAL Master Database ===\n");
    memset(&g_OurMasterDB, 0, sizeof(g_OurMasterDB));
    memset(&g_PrimaryObject, 0, sizeof(g_PrimaryObject));
    memset(g_PrimaryVTable, 0, sizeof(g_PrimaryVTable));

    // Only set the vtable pointer - everything else NULL/0
    g_PrimaryVTable[0] = (void*)Launcher_Initialize;
    g_PrimaryVTable[1] = (void*)Launcher_Shutdown;
    g_PrimaryVTable[3] = (void*)Launcher_GetState;

    g_PrimaryObject.pVTable = g_PrimaryVTable;
    // All other fields are 0/NULL

    g_OurMasterDB.pVTable = g_PrimaryVTable;
    g_OurMasterDB.pPrimaryObject = &g_PrimaryObject;
    // refCount = 0 (NOT 1!)
    // stateFlags = 0
    // All other fields are NULL/0

    fprintf(g_LogFile, "Master Database at: %p\n", &g_OurMasterDB);
    fprintf(g_LogFile, "  pVTable: %p\n", g_OurMasterDB.pVTable);
    fprintf(g_LogFile, "  refCount: %u (ZERO!)\n", g_OurMasterDB.refCount);
    fprintf(g_LogFile, "  stateFlags: %u\n", g_OurMasterDB.stateFlags);
    fprintf(g_LogFile, "  pPrimaryObject: %p\n", g_OurMasterDB.pPrimaryObject);

    // Load client.dll
    HMODULE hClient = LoadLibraryA("client.dll");
    if (!hClient) {
        fprintf(g_LogFile, "ERROR: Could not load client.dll\n");
        fclose(g_LogFile);
        return 1;
    }

    fprintf(g_LogFile, "\nclient.dll loaded at: %p\n", hClient);

    // Get SetMasterDatabase export
    auto setMasterDB = (void (*)(void*))GetProcAddress(hClient, "SetMasterDatabase");
    auto init = (InitClientDLL_t)GetProcAddress(hClient, "InitClientDLL");
    auto run = (RunClientDLL_t)GetProcAddress(hClient, "RunClientDLL");
    auto term = (TermClientDLL_t)GetProcAddress(hClient, "TermClientDLL");

    fprintf(g_LogFile, "SetMasterDatabase: %p\n", setMasterDB);
    fprintf(g_LogFile, "InitClientDLL: %p\n", init);

    if (!setMasterDB || !init) {
        fprintf(g_LogFile, "ERROR: Missing exports\n");
        fclose(g_LogFile);
        return 1;
    }

    // Call SetMasterDatabase ourselves with MINIMAL structure
    fprintf(g_LogFile, "\n=== Calling SetMasterDatabase with MINIMAL DB ===\n");
    fflush(g_LogFile);

    setMasterDB(&g_OurMasterDB);
    fprintf(g_LogFile, "SetMasterDatabase returned successfully!\n");
    fflush(g_LogFile);

    // Try InitClientDLL
    fprintf(g_LogFile, "\n=== Calling InitClientDLL ===\n");
    fflush(g_LogFile);

    int result = init(nullptr, nullptr, hClient, nullptr, &g_OurMasterDB, 0, 0, nullptr);
    fprintf(g_LogFile, "InitClientDLL returned: %d\n", result);

    if (result == 0) {
        fprintf(g_LogFile, "\n=== Calling RunClientDLL ===\n");
        fflush(g_LogFile);
        run();
    }

    if (term) term();
    FreeLibrary(hClient);
    fclose(g_LogFile);
    return 0;
}
