/**
 * Detailed logging to see exact call flow
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
    fprintf(g_LogFile, "[CALLBACK] vtable[0] Initialize called!\n");
    fprintf(g_LogFile, "  obj=%p, config=%p\n", obj, config);
    fflush(g_LogFile);
    return 0;
}

void __thiscall Launcher_Shutdown(APIObject* obj) {
    fprintf(g_LogFile, "[CALLBACK] vtable[1] Shutdown called!\n");
    fflush(g_LogFile);
}

uint32_t __thiscall Launcher_GetState(APIObject* obj) {
    fprintf(g_LogFile, "[CALLBACK] vtable[3] GetState called!\n");
    fflush(g_LogFile);
    return 1;
}

int __thiscall Launcher_RegisterCallback(APIObject* obj, void* callback, void* userData) {
    fprintf(g_LogFile, "[CALLBACK] vtable[4] RegisterCallback: callback=%p, userData=%p\n", callback, userData);
    fflush(g_LogFile);
    return 1;
}

int __thiscall Launcher_SetEventHandler(APIObject* obj, uint32_t eventType, void* handler) {
    fprintf(g_LogFile, "[CALLBACK] vtable[23] SetEventHandler: eventType=%u, handler=%p\n", eventType, handler);
    fflush(g_LogFile);
    return 0;
}

// Our EXPORTED SetMasterDatabase - if client.dll calls it
extern "C" __declspec(dllexport) void __stdcall SetMasterDatabase(void* pMasterDatabase) {
    fprintf(g_LogFile, "\n========================================\n");
    fprintf(g_LogFile, "[EXPORT] OUR SetMasterDatabase CALLED!\n");
    fprintf(g_LogFile, "  Client provided: %p\n", pMasterDatabase);
    fprintf(g_LogFile, "========================================\n\n");
    fflush(g_LogFile);
    std::cout << "=== OUR SetMasterDatabase EXPORT CALLED! ===" << std::endl;
}

int main(int argc, char* argv[]) {
    g_LogFile = fopen("/tmp/detailed_log.txt", "w");

    fprintf(g_LogFile, "Detailed Call Flow Analysis\n");
    fprintf(g_LogFile, "===========================\n\n");

    // Pre-load dependencies
    const char* preload_dlls[] = {
        "MFC71.dll", "MSVCR71.dll", "dbghelp.dll",
        "r3d9.dll", "binkw32.dll", NULL
    };

    for (int i = 0; preload_dlls[i]; i++) {
        HMODULE h = LoadLibraryA(preload_dlls[i]);
        fprintf(g_LogFile, "Preload: %s %s\n", preload_dlls[i], h ? "OK" : "FAILED");
    }

    // Create Master Database
    fprintf(g_LogFile, "\n=== Creating Master Database ===\n");
    memset(&g_OurMasterDB, 0, sizeof(g_OurMasterDB));
    memset(&g_PrimaryObject, 0, sizeof(g_PrimaryObject));

    g_PrimaryVTable[0] = (void*)Launcher_Initialize;
    g_PrimaryVTable[1] = (void*)Launcher_Shutdown;
    g_PrimaryVTable[3] = (void*)Launcher_GetState;
    g_PrimaryVTable[4] = (void*)Launcher_RegisterCallback;
    g_PrimaryVTable[23] = (void*)Launcher_SetEventHandler;

    g_PrimaryObject.pVTable = g_PrimaryVTable;
    g_OurMasterDB.pVTable = g_PrimaryVTable;
    g_OurMasterDB.pPrimaryObject = &g_PrimaryObject;

    fprintf(g_LogFile, "Master DB at: %p\n", &g_OurMasterDB);
    fprintf(g_LogFile, "  pVTable: %p\n", g_OurMasterDB.pVTable);
    fprintf(g_LogFile, "  pPrimaryObject: %p\n", g_OurMasterDB.pPrimaryObject);
    fprintf(g_LogFile, "  refCount: %u\n", g_OurMasterDB.refCount);

    // Load client.dll
    HMODULE hClient = LoadLibraryA("client.dll");
    if (!hClient) {
        fprintf(g_LogFile, "ERROR: Could not load client.dll\n");
        fclose(g_LogFile);
        return 1;
    }

    fprintf(g_LogFile, "\nclient.dll at: %p\n", hClient);

    // Get exports
    auto setMasterDB = (void (*)(void*))GetProcAddress(hClient, "SetMasterDatabase");
    auto init = (InitClientDLL_t)GetProcAddress(hClient, "InitClientDLL");
    auto run = (RunClientDLL_t)GetProcAddress(hClient, "RunClientDLL");
    auto term = (TermClientDLL_t)GetProcAddress(hClient, "TermClientDLL");

    fprintf(g_LogFile, "Exports:\n");
    fprintf(g_LogFile, "  SetMasterDatabase: %p\n", setMasterDB);
    fprintf(g_LogFile, "  InitClientDLL: %p\n", init);
    fprintf(g_LogFile, "  RunClientDLL: %p\n", run);

    if (!setMasterDB || !init) {
        fclose(g_LogFile);
        return 1;
    }

    // Call SetMasterDatabase
    fprintf(g_LogFile, "\n=== CALLING client.dll!SetMasterDatabase ===\n");
    fprintf(g_LogFile, "Passing our master DB: %p\n", &g_OurMasterDB);
    fflush(g_LogFile);

    setMasterDB(&g_OurMasterDB);

    fprintf(g_LogFile, "\nReturned from client.dll!SetMasterDatabase\n");

    // Call InitClientDLL
    fprintf(g_LogFile, "\n=== CALLING InitClientDLL ===\n");
    fflush(g_LogFile);

    int result = init(nullptr, nullptr, hClient, nullptr, &g_OurMasterDB, 0, 0, nullptr);

    fprintf(g_LogFile, "InitClientDLL returned: %d\n", result);

    if (result == 0) {
        fprintf(g_LogFile, "\n=== CALLING RunClientDLL ===\n");
        fflush(g_LogFile);
        run();
    }

    if (term) term();
    FreeLibrary(hClient);
    fclose(g_LogFile);
    return 0;
}
