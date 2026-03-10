/**
 * Test SetMasterDatabase Call - Diagnostic Version
 * 
 * This prints exactly what we're passing to SetMasterDatabase
 */

#include <windows.h>
#include <iostream>
#include <iomanip>
#include "master_database.h"

// Global variables
static MasterDatabase g_MasterDB;
static APIObject g_PrimaryObject;
static APIObject g_SecondaryObject;
static void* g_PrimaryVTable[30] = {0};

// Simple vtable functions
int __thiscall Test_Init(APIObject* obj, void* config) { return 0; }
void __thiscall Test_Shutdown(APIObject* obj) {}
uint32_t __thiscall Test_GetState(APIObject* obj) { return 1; }

int main() {
    std::cout << "=== SetMasterDatabase Diagnostic Test ===\n\n";
    
    // Setup vtable
    g_PrimaryVTable[0] = (void*)Test_Init;
    g_PrimaryVTable[1] = (void*)Test_Shutdown;
    g_PrimaryVTable[3] = (void*)Test_GetState;
    
    // Setup primary object
    g_PrimaryObject.pVTable = g_PrimaryVTable;
    g_PrimaryObject.objectId = 1;
    g_PrimaryObject.objectState = 0;
    g_PrimaryObject.flags = 0;
    
    // Setup master database
    g_MasterDB.pVTable = g_PrimaryVTable;
    g_MasterDB.refCount = 1;
    g_MasterDB.stateFlags = 0x0001;
    g_MasterDB.pPrimaryObject = &g_PrimaryObject;
    g_MasterDB.primaryData1 = 0;
    g_MasterDB.primaryData2 = 0;
    g_MasterDB.pSecondaryObject = &g_SecondaryObject;
    g_MasterDB.secondaryData1 = 0;
    g_MasterDB.secondaryData2 = 0;
    
    // Print what we're passing
    std::cout << "MasterDatabase structure we're passing:\n";
    std::cout << "  Address: " << &g_MasterDB << "\n";
    std::cout << "  Size: " << sizeof(MasterDatabase) << " bytes\n\n";
    
    std::cout << "Field values:\n";
    std::cout << "  [0x00] pVTable:          " << g_MasterDB.pVTable << "\n";
    std::cout << "  [0x04] refCount:         " << g_MasterDB.refCount << "\n";
    std::cout << "  [0x08] stateFlags:       0x" << std::hex << g_MasterDB.stateFlags << std::dec << "\n";
    std::cout << "  [0x0C] pPrimaryObject:   " << g_MasterDB.pPrimaryObject << "\n";
    std::cout << "  [0x10] primaryData1:     " << g_MasterDB.primaryData1 << "\n";
    std::cout << "  [0x14] primaryData2:     " << g_MasterDB.primaryData2 << "\n";
    std::cout << "  [0x18] pSecondaryObject: " << g_MasterDB.pSecondaryObject << "\n";
    std::cout << "  [0x1C] secondaryData1:   " << g_MasterDB.secondaryData1 << "\n";
    std::cout << "  [0x20] secondaryData2:   " << g_MasterDB.secondaryData2 << "\n";
    
    std::cout << "\nPrimary Object:\n";
    std::cout << "  Address: " << &g_PrimaryObject << "\n";
    std::cout << "  [0x00] pVTable:     " << g_PrimaryObject.pVTable << "\n";
    std::cout << "  [0x04] objectId:    " << g_PrimaryObject.objectId << "\n";
    std::cout << "  [0x08] objectState: " << g_PrimaryObject.objectState << "\n";
    
    // Pre-load dependencies
    std::cout << "\n=== Pre-loading dependencies ===\n";
    const char* preload_dlls[] = {
        "MFC71.dll", "MSVCR71.dll", "dbghelp.dll", 
        "r3d9.dll", "binkw32.dll", NULL
    };
    
    for (int i = 0; preload_dlls[i]; i++) {
        HMODULE h = LoadLibraryA(preload_dlls[i]);
        std::cout << "  " << preload_dlls[i] << ": " << (h ? "OK" : "FAILED") << "\n";
    }
    
    // Load client.dll
    std::cout << "\n=== Loading client.dll ===\n";
    HMODULE hClient = LoadLibraryA("client.dll");
    
    if (!hClient) {
        std::cerr << "ERROR: Could not load client.dll (error " << GetLastError() << ")\n";
        return 1;
    }
    
    std::cout << "client.dll loaded at: " << hClient << "\n";
    
    // Get SetMasterDatabase
    SetMasterDatabase_t setMasterDB = (SetMasterDatabase_t)GetProcAddress(hClient, "SetMasterDatabase");
    
    if (!setMasterDB) {
        std::cerr << "ERROR: SetMasterDatabase not found!\n";
        FreeLibrary(hClient);
        return 1;
    }
    
    std::cout << "SetMasterDatabase at: " << (void*)setMasterDB << "\n";
    
    // Call SetMasterDatabase
    std::cout << "\n=== Calling SetMasterDatabase ===\n";
    std::cout << "Passing pointer: " << &g_MasterDB << "\n";
    std::cout << "Value at pointer: " << *(void**)&g_MasterDB << " (should be vtable)\n";
    std::cout.flush();
    
    setMasterDB(&g_MasterDB);
    
    std::cout << "\nSetMasterDatabase returned successfully!\n";
    
    FreeLibrary(hClient);
    std::cout << "Done.\n";
    return 0;
}
