/**
 * Test Theory 1: Call SetMasterDatabase OURSELVES before InitClientDLL
 *
 * Hypothesis: launcher.exe calls client.dll!SetMasterDatabase export
 */

#include <windows.h>
#include <iostream>
#include <cstdio>
#include "master_database.h"

static FILE* g_LogFile = NULL;
static MasterDatabase g_OurMasterDB;
static APIObject g_PrimaryObject;
static void* g_PrimaryVTable[30] = {0};

// VTable functions (minimal implementations)
int __thiscall Launcher_Initialize(APIObject* obj, void* config) {
    fprintf(g_LogFile, "[Launcher] Initialize called\n");
    return 0;
}

void __thiscall Launcher_Shutdown(APIObject* obj) {
    fprintf(g_LogFile, "[Launcher] Shutdown called\n");
}

uint32_t __thiscall Launcher_GetState(APIObject* obj) {
    return 1;
}

int __thiscall Launcher_RegisterCallback(APIObject* obj, void* callback, void* userData) {
    fprintf(g_LogFile, "[Launcher] RegisterCallback: %p\n", callback);
    return 1;
}

int __thiscall Launcher_SetEventHandler(APIObject* obj, uint32_t eventType, void* handler) {
    fprintf(g_LogFile, "[Launcher] SetEventHandler: event=%u\n", eventType);
    return 0;
}

// Export in case client.dll tries to call it back
extern "C" __declspec(dllexport) void __stdcall SetMasterDatabase(void* pMasterDatabase) {
    fprintf(g_LogFile, "\n=== OUR SetMasterDatabase CALLED BY CLIENT.DLL ===\n");
    fprintf(g_LogFile, "Client provided: %p\n", pMasterDatabase);
    fflush(g_LogFile);
    std::cout << "=== OUR SetMasterDatabase CALLED BY CLIENT.DLL ===" << std::endl;
}

int main(int argc, char* argv[]) {
    g_LogFile = fopen("/tmp/test_theory1.log", "w");

    fprintf(g_LogFile, "Test Theory 1: Call SetMasterDatabase ourselves\n");
    fprintf(g_LogFile, "================================================\n\n");

    // Pre-load dependencies
    const char* preload_dlls[] = {
        "MFC71.dll", "MSVCR71.dll", "dbghelp.dll",
        "r3d9.dll", "binkw32.dll", NULL
    };

    fprintf(g_LogFile, "=== Pre-loading dependencies ===\n");
    for (int i = 0; preload_dlls[i]; i++) {
        HMODULE h = LoadLibraryA(preload_dlls[i]);
        fprintf(g_LogFile, "  %s: %s\n", preload_dlls[i], h ? "OK" : "FAILED");
    }

    // Create Master Database
    fprintf(g_LogFile, "\n=== Creating Master Database ===\n");
    g_PrimaryVTable[0] = (void*)Launcher_Initialize;
    g_PrimaryVTable[1] = (void*)Launcher_Shutdown;
    g_PrimaryVTable[3] = (void*)Launcher_GetState;
    g_PrimaryVTable[4] = (void*)Launcher_RegisterCallback;
    g_PrimaryVTable[23] = (void*)Launcher_SetEventHandler;

    g_PrimaryObject.pVTable = g_PrimaryVTable;
    g_OurMasterDB.pVTable = g_PrimaryVTable;
    g_OurMasterDB.refCount = 1;
    g_OurMasterDB.stateFlags = 0x0001;
    g_OurMasterDB.pPrimaryObject = &g_PrimaryObject;

    fprintf(g_LogFile, "Master Database ready at: %p\n", &g_OurMasterDB);

    // Load client.dll
    fprintf(g_LogFile, "\n=== Loading client.dll ===\n");
    HMODULE hClient = LoadLibraryA("client.dll");

    if (!hClient) {
        fprintf(g_LogFile, "ERROR: Could not load client.dll\n");
        fclose(g_LogFile);
        return 1;
    }

    fprintf(g_LogFile, "client.dll loaded at: %p\n", hClient);

    // Get exports
    fprintf(g_LogFile, "\n=== Getting exports ===\n");
    auto setMasterDB = (void (*)(void*))GetProcAddress(hClient, "SetMasterDatabase");
    auto init = (InitClientDLL_t)GetProcAddress(hClient, "InitClientDLL");
    auto run = (RunClientDLL_t)GetProcAddress(hClient, "RunClientDLL");
    auto term = (TermClientDLL_t)GetProcAddress(hClient, "TermClientDLL");

    fprintf(g_LogFile, "SetMasterDatabase: %p\n", setMasterDB);
    fprintf(g_LogFile, "InitClientDLL: %p\n", init);
    fprintf(g_LogFile, "RunClientDLL: %p\n", run);

    if (!setMasterDB || !init || !run) {
        fprintf(g_LogFile, "ERROR: Missing exports\n");
        fclose(g_LogFile);
        return 1;
    }

    // THEORY 1 TEST: Call SetMasterDatabase OURSELVES before InitClientDLL
    fprintf(g_LogFile, "\n=== THEORY 1: Calling SetMasterDatabase ourselves ===\n");
    fprintf(g_LogFile, "Passing our master database: %p\n", &g_OurMasterDB);
    fflush(g_LogFile);

    setMasterDB(&g_OurMasterDB);

    fprintf(g_LogFile, "SetMasterDatabase returned!\n");
    fflush(g_LogFile);

    // Now call InitClientDLL
    fprintf(g_LogFile, "\n=== Calling InitClientDLL ===\n");
    fflush(g_LogFile);

    int result = init(
        nullptr,
        nullptr,
        hClient,
        nullptr,
        &g_OurMasterDB,
        0,
        0,
        nullptr
    );

    fprintf(g_LogFile, "InitClientDLL returned: %d\n", result);
    fflush(g_LogFile);

    if (result != 0) {
        fprintf(g_LogFile, "InitClientDLL failed!\n");
        fclose(g_LogFile);
        return 1;
    }

    // Call RunClientDLL
    fprintf(g_LogFile, "\n=== Calling RunClientDLL ===\n");
    fflush(g_LogFile);

    run();

    // Cleanup
    if (term) term();
    FreeLibrary(hClient);
    fclose(g_LogFile);

    return 0;
}
