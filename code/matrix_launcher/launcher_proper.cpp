/**
 * Matrix Online Launcher - Proper InitClientDLL Implementation
 * 
 * This implements the correct initialization sequence based on reverse engineering:
 * 1. Create Master Database structure with vtables
 * 2. Load client.dll
 * 3. Call SetMasterDatabase to pass API to client
 * 4. Call InitClientDLL with proper parameters
 * 5. Call RunClientDLL to start game
 * 
 * Based on documentation in:
 * - ../../docs/api_surface/MASTER_DATABASE.md
 * - ../../docs/api_surface/client_dll_api_discovery.md
 * - ../../docs/api_surface/data_passing_mechanisms.md
 */

#include <windows.h>
#include <iostream>
#include <string>
#include <direct.h>
#include "master_database.h"

// ============================================================================
// Global Variables
// ============================================================================

// Master database and API objects
static MasterDatabase g_MasterDB;
static APIObject g_PrimaryObject;
static APIObject g_SecondaryObject;

// VTables (arrays of function pointers)
static void* g_PrimaryVTable[30] = {0};   // Primary object vtable
static void* g_SecondaryVTable[30] = {0}; // Secondary object vtable

// Client.dll function pointers
static InitClientDLL_t g_InitClientDLL = nullptr;
static RunClientDLL_t g_RunClientDLL = nullptr;
static TermClientDLL_t g_TermClientDLL = nullptr;
static ErrorClientDLL_t g_ErrorClientDLL = nullptr;
static SetMasterDatabase_t g_SetMasterDatabase_Client = nullptr;

// ============================================================================
// Launcher API Functions (called by client.dll via vtables)
// ============================================================================

// Primary Object VTable Functions

int __thiscall Launcher_Initialize(APIObject* obj, void* config) {
    std::cout << "[Launcher] Initialize called (vtable[0])\n";
    obj->objectState = 0x0001;  // INIT_COMPLETE
    return 0;  // Success
}

void __thiscall Launcher_Shutdown(APIObject* obj) {
    std::cout << "[Launcher] Shutdown called (vtable[1])\n";
    obj->objectState = 0x0000;  // NOT_INITIALIZED
}

int __thiscall Launcher_Reset(APIObject* obj) {
    std::cout << "[Launcher] Reset called (vtable[2])\n";
    obj->objectState = 0x0000;
    return 0;
}

uint32_t __thiscall Launcher_GetState(APIObject* obj) {
    std::cout << "[Launcher] GetState called (vtable[3]) -> " << obj->objectState << "\n";
    return obj->objectState;
}

int __thiscall Launcher_RegisterCallback(APIObject* obj, void* callback, void* userData) {
    std::cout << "[Launcher] RegisterCallback called (vtable[4])\n";
    obj->pCallback1 = callback;
    obj->pCallbackData = userData;
    return 1;  // Callback ID
}

int __thiscall Launcher_SetEventHandler(APIObject* obj, uint32_t eventType, void* handler) {
    std::cout << "[Launcher] SetEventHandler called (vtable[23], event=" << eventType << ")\n";
    obj->pCallback2 = handler;
    return 0;
}

uint32_t __thiscall Launcher_GetApplicationState(APIObject* obj) {
    std::cout << "[Launcher] GetApplicationState called (vtable[22])\n";
    return 1;  // Running
}

// Placeholder for unimplemented vtable functions
void __thiscall Launcher_Unimplemented(APIObject* obj) {
    std::cout << "[Launcher] Unimplemented vtable function called\n";
}

// ============================================================================
// Master Database Initialization
// ============================================================================

void InitializeMasterDatabase() {
    std::cout << "\n=== Initializing Master Database ===\n";
    
    // Initialize primary object vtable
    g_PrimaryVTable[0] = (void*)Launcher_Initialize;          // 0x00
    g_PrimaryVTable[1] = (void*)Launcher_Shutdown;            // 0x04
    g_PrimaryVTable[2] = (void*)Launcher_Reset;               // 0x08
    g_PrimaryVTable[3] = (void*)Launcher_GetState;            // 0x0C
    g_PrimaryVTable[4] = (void*)Launcher_RegisterCallback;    // 0x10
    
    // VTable offsets from documentation
    g_PrimaryVTable[22] = (void*)Launcher_GetApplicationState;  // 0x58
    g_PrimaryVTable[23] = (void*)Launcher_SetEventHandler;      // 0x5C
    
    // Fill remaining with placeholder
    for (int i = 5; i < 30; i++) {
        if (g_PrimaryVTable[i] == nullptr) {
            g_PrimaryVTable[i] = (void*)Launcher_Unimplemented;
        }
    }
    
    // Initialize secondary object vtable (same for now)
    for (int i = 0; i < 30; i++) {
        g_SecondaryVTable[i] = g_PrimaryVTable[i];
    }
    
    // Initialize primary object
    g_PrimaryObject.pVTable = g_PrimaryVTable;
    g_PrimaryObject.objectId = 1;
    g_PrimaryObject.objectState = 0;
    g_PrimaryObject.flags = 0;
    g_PrimaryObject.pInternalData = nullptr;
    g_PrimaryObject.dataSize = 0;
    g_PrimaryObject.pCallback1 = nullptr;
    g_PrimaryObject.pCallback2 = nullptr;
    g_PrimaryObject.pCallback3 = nullptr;
    g_PrimaryObject.pCallbackData = nullptr;
    g_PrimaryObject.callbackFlags = 0;
    
    // Initialize secondary object
    g_SecondaryObject.pVTable = g_SecondaryVTable;
    g_SecondaryObject.objectId = 2;
    g_SecondaryObject.objectState = 0;
    g_SecondaryObject.flags = 0;
    g_SecondaryObject.pInternalData = nullptr;
    g_SecondaryObject.dataSize = 0;
    g_SecondaryObject.pCallback1 = nullptr;
    g_SecondaryObject.pCallback2 = nullptr;
    g_SecondaryObject.pCallback3 = nullptr;
    g_SecondaryObject.pCallbackData = nullptr;
    g_SecondaryObject.callbackFlags = 0;
    
    // Initialize master database
    g_MasterDB.pVTable = g_PrimaryVTable;  // Use primary vtable as identifier
    g_MasterDB.refCount = 1;
    g_MasterDB.stateFlags = 0x0001;  // INITIALIZED
    g_MasterDB.pPrimaryObject = &g_PrimaryObject;
    g_MasterDB.primaryData1 = 0;
    g_MasterDB.primaryData2 = 0;
    g_MasterDB.pSecondaryObject = &g_SecondaryObject;
    g_MasterDB.secondaryData1 = 0;
    g_MasterDB.secondaryData2 = 0;
    
    std::cout << "Master Database initialized at: " << &g_MasterDB << "\n";
    std::cout << "Primary Object at: " << &g_PrimaryObject << "\n";
    std::cout << "Secondary Object at: " << &g_SecondaryObject << "\n";
    std::cout << "Primary VTable at: " << g_PrimaryVTable << "\n";
}

// ============================================================================
// Launcher Export (for client.dll to call back)
// ============================================================================

extern "C" __declspec(dllexport) void __stdcall SetMasterDatabase(void* pMasterDatabase) {
    std::cout << "\n[Launcher EXPORT] SetMasterDatabase called from client.dll\n";
    std::cout << "  Client provided master database at: " << pMasterDatabase << "\n";
    
    // Store the client's master database pointer
    // In the original launcher, this would be used for communication
    // For now, we just acknowledge it
}

// ============================================================================
// Main Launcher
// ============================================================================

int main(int argc, char* argv[]) {
    // Show current working directory
    char cwd[MAX_PATH];
    _getcwd(cwd, MAX_PATH);
    std::cout << "Matrix Online Launcher - Proper InitClientDLL\n";
    std::cout << "Working directory: " << cwd << "\n";
    std::cout << "argc: " << argc << ", argv[0]: " << (argv[0] ? argv[0] : "NULL") << "\n";
    
    // Step 0: Pre-load ALL dependencies before client.dll
    // This prevents hangs/crashes during client.dll's DllMain
    std::cout << "\n=== Pre-loading all dependencies ===\n";
    
    const char* preload_dlls[] = {
        "MFC71.dll",
        "MSVCR71.dll",
        "dbghelp.dll",
        "r3d9.dll",
        "binkw32.dll",
        "pythonMXO.dll",
        "dllMediaPlayer.dll",
        "dllWebBrowser.dll",
        NULL
    };
    
    for (int i = 0; preload_dlls[i]; i++) {
        HMODULE h = LoadLibraryA(preload_dlls[i]);
        if (h) {
            std::cout << "  " << preload_dlls[i] << " loaded at: " << h << "\n";
        } else {
            std::cerr << "  WARNING: Could not load " << preload_dlls[i] 
                      << " (error " << GetLastError() << ")\n";
        }
    }
    
    // Step 1: Initialize Master Database
    InitializeMasterDatabase();
    
    // Step 2: Load client.dll
    std::cout << "\n=== Loading client.dll ===\n";
    HMODULE hClient = LoadLibraryA("client.dll");
    
    if (!hClient) {
        DWORD err = GetLastError();
        std::cerr << "FAILED to load client.dll (error " << err << ")\n";
        return 1;
    }
    
    std::cout << "client.dll loaded at: " << hClient << "\n";
    
    // Step 3: Get exported functions
    std::cout << "\n=== Getting client.dll exports ===\n";
    
    g_InitClientDLL = (InitClientDLL_t)GetProcAddress(hClient, "InitClientDLL");
    g_RunClientDLL = (RunClientDLL_t)GetProcAddress(hClient, "RunClientDLL");
    g_TermClientDLL = (TermClientDLL_t)GetProcAddress(hClient, "TermClientDLL");
    g_ErrorClientDLL = (ErrorClientDLL_t)GetProcAddress(hClient, "ErrorClientDLL");
    g_SetMasterDatabase_Client = (SetMasterDatabase_t)GetProcAddress(hClient, "SetMasterDatabase");
    
    std::cout << "InitClientDLL: " << (void*)g_InitClientDLL << "\n";
    std::cout << "RunClientDLL: " << (void*)g_RunClientDLL << "\n";
    std::cout << "TermClientDLL: " << (void*)g_TermClientDLL << "\n";
    std::cout << "ErrorClientDLL: " << (void*)g_ErrorClientDLL << "\n";
    std::cout << "SetMasterDatabase: " << (void*)g_SetMasterDatabase_Client << "\n";
    
    if (!g_InitClientDLL || !g_RunClientDLL) {
        std::cerr << "ERROR: Missing required exports\n";
        FreeLibrary(hClient);
        return 1;
    }
    
    // Step 4: Call SetMasterDatabase FIRST (client.dll side)
    // This passes our API to client.dll
    if (g_SetMasterDatabase_Client) {
        std::cout << "\n=== Calling SetMasterDatabase (client.dll) ===\n";
        std::cout << "Passing master database to client.dll...\n";
        g_SetMasterDatabase_Client(&g_MasterDB);
        std::cout << "SetMasterDatabase returned\n";
    } else {
        std::cerr << "WARNING: SetMasterDatabase not found in client.dll\n";
    }
    
    // Step 5: Call InitClientDLL
    std::cout << "\n=== Calling InitClientDLL ===\n";
    
    // Based on data_passing_mechanisms.md, InitClientDLL takes multiple parameters
    // For now, we'll try minimal parameters
    // TODO: Reverse engineer exact parameter meanings
    
    int result = 0;
    
    // Try calling with minimal parameters (may crash - need to RE the function)
    std::cout << "Calling InitClientDLL with parameters...\n";
    std::cout << "WARNING: Using placeholder parameters - may crash!\n";
    std::cout.flush();
    
    // Based on documentation, InitClientDLL signature is:
    // int InitClientDLL(void* param1, void* param2, void* param3, 
    //                    void* param4, void* param5, void* param6);
    
    // Try with null parameters first
    result = g_InitClientDLL(
        nullptr,  // param1: Window handle or instance
        nullptr,  // param2: Command line or configuration
        nullptr,  // param3: Width/parent window
        nullptr,  // param4: Height/context
        nullptr,  // param5: Additional parameter
        nullptr   // param6: Additional parameter
    );
    
    std::cout << "InitClientDLL returned: " << result << "\n";
    
    if (result != 0) {
        std::cerr << "InitClientDLL failed with result: " << result << "\n";
        FreeLibrary(hClient);
        return 1;
    }
    
    // Step 6: Call RunClientDLL
    std::cout << "\n=== Calling RunClientDLL ===\n";
    std::cout << "Game window should appear...\n";
    std::cout.flush();
    
    g_RunClientDLL();  // This should block until game exits
    
    // Step 7: Cleanup
    std::cout << "\n=== Cleanup ===\n";
    if (g_TermClientDLL) {
        g_TermClientDLL();
    }
    
    FreeLibrary(hClient);
    std::cout << "Done.\n";
    return 0;
}
