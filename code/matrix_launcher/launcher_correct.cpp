/**
 * Matrix Online Launcher - Correct Implementation
 *
 * CRITICAL DISCOVERY: SetMasterDatabase is EXPORTED by launcher.exe,
 * not imported! Client.dll calls our SetMasterDatabase to register its API.
 *
 * Flow:
 * 1. launcher.exe loads client.dll
 * 2. launcher.exe calls InitClientDLL(params...)
 * 3. Inside InitClientDLL, client.dll calls launcher.exe!SetMasterDatabase
 * 4. launcher.exe calls RunClientDLL()
 */

#include <windows.h>
#include <iostream>
#include <cstdint>
#include "master_database.h"

// ============================================================================
// Global Variables
// ============================================================================

// Master database that client.dll will provide to us
static void* g_ClientMasterDatabase = NULL;

// Our master database that we provide to client.dll
static MasterDatabase g_OurMasterDB;
static APIObject g_PrimaryObject;
static APIObject g_SecondaryObject;
static void* g_PrimaryVTable[30] = {0};

// Client.dll function pointers
static InitClientDLL_t g_InitClientDLL = nullptr;
static RunClientDLL_t g_RunClientDLL = nullptr;
static TermClientDLL_t g_TermClientDLL = nullptr;

// ============================================================================
// VTable Functions (Client.dll will call these)
// ============================================================================

int __thiscall Launcher_Initialize(APIObject* obj, void* config) {
    std::cout << "[Launcher] Initialize called\n";
    return 0;
}

void __thiscall Launcher_Shutdown(APIObject* obj) {
    std::cout << "[Launcher] Shutdown called\n";
}

uint32_t __thiscall Launcher_GetState(APIObject* obj) {
    return 1;  // Running
}

int __thiscall Launcher_RegisterCallback(APIObject* obj, void* callback, void* userData) {
    std::cout << "[Launcher] RegisterCallback: " << callback << "\n";
    return 1;
}

int __thiscall Launcher_SetEventHandler(APIObject* obj, uint32_t eventType, void* handler) {
    std::cout << "[Launcher] SetEventHandler: event=" << eventType << ", handler=" << handler << "\n";
    return 0;
}

// ============================================================================
// EXPORTED FUNCTION - Client.dll calls this!
// ============================================================================

extern "C" __declspec(dllexport) void __stdcall SetMasterDatabase(void* pMasterDatabase) {
    std::cout << "\n=== SetMasterDatabase CALLED BY CLIENT.DLL ===\n";
    std::cout << "Client provided master database: " << pMasterDatabase << "\n";

    // Store the master database client.dll gave us
    g_ClientMasterDatabase = pMasterDatabase;

    // Client.dll has now registered its API with us
    std::cout << "API registration complete!\n\n";
}

// ============================================================================
// Initialization
// ============================================================================

void InitializeOurMasterDatabase() {
    std::cout << "Creating our Master Database for client.dll...\n";

    // Setup vtable
    g_PrimaryVTable[0] = (void*)Launcher_Initialize;
    g_PrimaryVTable[1] = (void*)Launcher_Shutdown;
    g_PrimaryVTable[3] = (void*)Launcher_GetState;
    g_PrimaryVTable[4] = (void*)Launcher_RegisterCallback;
    g_PrimaryVTable[23] = (void*)Launcher_SetEventHandler;

    // Setup primary object
    g_PrimaryObject.pVTable = g_PrimaryVTable;
    g_PrimaryObject.objectId = 1;
    g_PrimaryObject.objectState = 0;

    // Setup master database
    g_OurMasterDB.pVTable = g_PrimaryVTable;
    g_OurMasterDB.refCount = 1;
    g_OurMasterDB.stateFlags = 0x0001;
    g_OurMasterDB.pPrimaryObject = &g_PrimaryObject;

    std::cout << "Master Database ready at: " << &g_OurMasterDB << "\n";
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "Matrix Online Launcher - Correct Implementation\n";
    std::cout << "Based on reverse engineering discovery\n\n";

    // Step 0: Pre-load dependencies
    std::cout << "=== Pre-loading dependencies ===\n";
    const char* preload_dlls[] = {
        "MFC71.dll", "MSVCR71.dll", "dbghelp.dll",
        "r3d9.dll", "binkw32.dll", NULL
    };

    for (int i = 0; preload_dlls[i]; i++) {
        HMODULE h = LoadLibraryA(preload_dlls[i]);
        std::cout << "  " << preload_dlls[i] << ": " << (h ? "OK" : "FAILED") << "\n";
    }

    // Step 1: Create our Master Database (client.dll will call SetMasterDatabase)
    std::cout << "\n=== Creating Master Database ===\n";
    InitializeOurMasterDatabase();

    // Step 2: Load client.dll
    std::cout << "\n=== Loading client.dll ===\n";
    HMODULE hClient = LoadLibraryA("client.dll");

    if (!hClient) {
        std::cerr << "ERROR: Could not load client.dll (error " << GetLastError() << ")\n";
        return 1;
    }

    std::cout << "client.dll loaded at: " << hClient << "\n";

    // Step 3: Get exported functions
    std::cout << "\n=== Getting exports ===\n";

    g_InitClientDLL = (InitClientDLL_t)GetProcAddress(hClient, "InitClientDLL");
    g_RunClientDLL = (RunClientDLL_t)GetProcAddress(hClient, "RunClientDLL");
    g_TermClientDLL = (TermClientDLL_t)GetProcAddress(hClient, "TermClientDLL");

    std::cout << "InitClientDLL: " << (void*)g_InitClientDLL << "\n";
    std::cout << "RunClientDLL: " << (void*)g_RunClientDLL << "\n";
    std::cout << "TermClientDLL: " << (void*)g_TermClientDLL << "\n";

    if (!g_InitClientDLL || !g_RunClientDLL) {
        std::cerr << "ERROR: Missing required exports\n";
        FreeLibrary(hClient);
        return 1;
    }

    // Step 4: Call InitClientDLL with 8 parameters
    // Based on disassembly, the parameters come from various sources
    // For now, try with minimal parameters

    std::cout << "\n=== Calling InitClientDLL ===\n";
    std::cout << "NOTE: Client.dll should call our SetMasterDatabase during this!\n";
    std::cout.flush();

    int result = g_InitClientDLL(
        nullptr,            // param1: unknown
        nullptr,            // param2: unknown
        hClient,            // param3: client.dll handle
        nullptr,            // param4: unknown
        &g_OurMasterDB,     // param5: our master database
        0,                  // param6: version/build info
        0,                  // param7: flags
        nullptr             // param8: unknown
    );

    std::cout << "\nInitClientDLL returned: " << result << "\n";

    if (result != 0) {
        std::cerr << "InitClientDLL failed!\n";
        FreeLibrary(hClient);
        return 1;
    }

    // Step 5: Call RunClientDLL
    std::cout << "\n=== Calling RunClientDLL ===\n";
    std::cout << "Game window should appear...\n";
    std::cout.flush();

    g_RunClientDLL();

    // Step 6: Cleanup
    std::cout << "\n=== Cleanup ===\n";
    if (g_TermClientDLL) {
        g_TermClientDLL();
    }

    FreeLibrary(hClient);
    std::cout << "Done.\n";
    return 0;
}
