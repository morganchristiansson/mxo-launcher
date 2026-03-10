/**
 * Matrix Online Launcher - With File Logging
 * To verify SetMasterDatabase is actually being called
 */

#include <windows.h>
#include <iostream>
#include <cstdio>
#include <cstdint>
#include "master_database.h"

// ============================================================================
// Global Variables
// ============================================================================

static void* g_ClientMasterDatabase = NULL;
static FILE* g_LogFile = NULL;
static MasterDatabase g_OurMasterDB;
static APIObject g_PrimaryObject;
static void* g_PrimaryVTable[30] = {0};

static InitClientDLL_t g_InitClientDLL = nullptr;
static RunClientDLL_t g_RunClientDLL = nullptr;
static TermClientDLL_t g_TermClientDLL = nullptr;

// ============================================================================
// VTable Functions
// ============================================================================

int __thiscall Launcher_Initialize(APIObject* obj, void* config) {
    fprintf(g_LogFile, "[Launcher] Initialize called\n");
    fflush(g_LogFile);
    return 0;
}

void __thiscall Launcher_Shutdown(APIObject* obj) {
    fprintf(g_LogFile, "[Launcher] Shutdown called\n");
    fflush(g_LogFile);
}

uint32_t __thiscall Launcher_GetState(APIObject* obj) {
    fprintf(g_LogFile, "[Launcher] GetState called\n");
    fflush(g_LogFile);
    return 1;
}

int __thiscall Launcher_RegisterCallback(APIObject* obj, void* callback, void* userData) {
    fprintf(g_LogFile, "[Launcher] RegisterCallback: %p\n", callback);
    fflush(g_LogFile);
    return 1;
}

int __thiscall Launcher_SetEventHandler(APIObject* obj, uint32_t eventType, void* handler) {
    fprintf(g_LogFile, "[Launcher] SetEventHandler: event=%u, handler=%p\n", eventType, handler);
    fflush(g_LogFile);
    return 0;
}

// ============================================================================
// EXPORTED FUNCTION - Client.dll calls this!
// ============================================================================

extern "C" __declspec(dllexport) void __stdcall SetMasterDatabase(void* pMasterDatabase) {
    fprintf(g_LogFile, "\n=== SetMasterDatabase CALLED BY CLIENT.DLL ===\n");
    fprintf(g_LogFile, "Client provided master database: %p\n", pMasterDatabase);
    fflush(g_LogFile);

    g_ClientMasterDatabase = pMasterDatabase;

    fprintf(g_LogFile, "API registration complete!\n\n");
    fflush(g_LogFile);

    // Also print to stdout
    std::cout << "=== SetMasterDatabase CALLED BY CLIENT.DLL ===" << std::endl;
    std::cout << "Client provided master database: " << pMasterDatabase << std::endl;
    std::cout.flush();
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    // Open log file
    g_LogFile = fopen("/tmp/launcher_log.txt", "w");
    if (!g_LogFile) {
        g_LogFile = stderr;
    }

    fprintf(g_LogFile, "Matrix Online Launcher - With File Logging\n");
    fprintf(g_LogFile, "==========================================\n\n");
    fflush(g_LogFile);

    std::cout << "Matrix Online Launcher - With File Logging\n";
    std::cout << "==========================================\n\n";

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
    fflush(g_LogFile);

    // Create Master Database
    fprintf(g_LogFile, "\n=== Creating Master Database ===\n");
    g_PrimaryVTable[0] = (void*)Launcher_Initialize;
    g_PrimaryVTable[1] = (void*)Launcher_Shutdown;
    g_PrimaryVTable[3] = (void*)Launcher_GetState;
    g_PrimaryVTable[4] = (void*)Launcher_RegisterCallback;
    g_PrimaryVTable[23] = (void*)Launcher_SetEventHandler;

    g_PrimaryObject.pVTable = g_PrimaryVTable;
    g_PrimaryObject.objectId = 1;
    g_PrimaryObject.objectState = 0;

    g_OurMasterDB.pVTable = g_PrimaryVTable;
    g_OurMasterDB.refCount = 1;
    g_OurMasterDB.stateFlags = 0x0001;
    g_OurMasterDB.pPrimaryObject = &g_PrimaryObject;

    fprintf(g_LogFile, "Master Database ready at: %p\n", &g_OurMasterDB);
    fflush(g_LogFile);

    // Load client.dll
    fprintf(g_LogFile, "\n=== Loading client.dll ===\n");
    HMODULE hClient = LoadLibraryA("client.dll");

    if (!hClient) {
        fprintf(g_LogFile, "ERROR: Could not load client.dll (error %lu)\n", GetLastError());
        fclose(g_LogFile);
        return 1;
    }

    fprintf(g_LogFile, "client.dll loaded at: %p\n", hClient);
    fflush(g_LogFile);

    // Get exports
    fprintf(g_LogFile, "\n=== Getting exports ===\n");
    g_InitClientDLL = (InitClientDLL_t)GetProcAddress(hClient, "InitClientDLL");
    g_RunClientDLL = (RunClientDLL_t)GetProcAddress(hClient, "RunClientDLL");
    g_TermClientDLL = (TermClientDLL_t)GetProcAddress(hClient, "TermClientDLL");

    fprintf(g_LogFile, "InitClientDLL: %p\n", g_InitClientDLL);
    fprintf(g_LogFile, "RunClientDLL: %p\n", g_RunClientDLL);
    fprintf(g_LogFile, "TermClientDLL: %p\n", g_TermClientDLL);
    fflush(g_LogFile);

    if (!g_InitClientDLL || !g_RunClientDLL) {
        fprintf(g_LogFile, "ERROR: Missing required exports\n");
        fclose(g_LogFile);
        return 1;
    }

    // Call InitClientDLL
    fprintf(g_LogFile, "\n=== Calling InitClientDLL ===\n");
    fprintf(g_LogFile, "NOTE: Client.dll should call our SetMasterDatabase during this!\n");
    fflush(g_LogFile);

    int result = g_InitClientDLL(
        nullptr,            // param1
        nullptr,            // param2
        hClient,            // param3: client.dll handle
        nullptr,            // param4
        &g_OurMasterDB,     // param5: our master database
        0,                  // param6: version/build info
        0,                  // param7: flags
        nullptr             // param8
    );

    fprintf(g_LogFile, "\nInitClientDLL returned: %d\n", result);
    fflush(g_LogFile);

    if (result != 0) {
        fprintf(g_LogFile, "InitClientDLL failed!\n");
        fclose(g_LogFile);
        return 1;
    }

    // Check if SetMasterDatabase was called
    if (g_ClientMasterDatabase) {
        fprintf(g_LogFile, "SetMasterDatabase WAS called! DB=%p\n", g_ClientMasterDatabase);
    } else {
        fprintf(g_LogFile, "SetMasterDatabase was NOT called\n");
    }
    fflush(g_LogFile);

    // Call RunClientDLL
    fprintf(g_LogFile, "\n=== Calling RunClientDLL ===\n");
    fflush(g_LogFile);

    g_RunClientDLL();

    // Cleanup
    fprintf(g_LogFile, "\n=== Cleanup ===\n");
    if (g_TermClientDLL) {
        g_TermClientDLL();
    }

    FreeLibrary(hClient);
    fprintf(g_LogFile, "Done.\n");
    fclose(g_LogFile);

    return 0;
}
