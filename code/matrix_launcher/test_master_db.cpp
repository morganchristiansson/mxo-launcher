/**
 * Test Master Database Structure Setup
 * 
 * This tests if our master database structure is being created correctly
 * without actually calling client.dll functions.
 */

#include <windows.h>
#include <iostream>
#include <iomanip>
#include "master_database.h"

// ============================================================================
// Global Variables
// ============================================================================

static MasterDatabase g_MasterDB;
static APIObject g_PrimaryObject;
static APIObject g_SecondaryObject;
static void* g_PrimaryVTable[30] = {0};
static void* g_SecondaryVTable[30] = {0};

// ============================================================================
// Test Functions
// ============================================================================

int __thiscall Test_Initialize(APIObject* obj, void* config) {
    std::cout << "  [Test_Initialize] Called with obj=" << obj << ", config=" << config << "\n";
    return 0;
}

void __thiscall Test_Shutdown(APIObject* obj) {
    std::cout << "  [Test_Shutdown] Called with obj=" << obj << "\n";
}

uint32_t __thiscall Test_GetState(APIObject* obj) {
    std::cout << "  [Test_GetState] Called with obj=" << obj << "\n";
    return 1;
}

// ============================================================================
// Main Test
// ============================================================================

int main() {
    std::cout << "=== Master Database Structure Test ===\n\n";
    
    // Initialize vtables
    std::cout << "1. Setting up vtables...\n";
    g_PrimaryVTable[0] = (void*)Test_Initialize;
    g_PrimaryVTable[1] = (void*)Test_Shutdown;
    g_PrimaryVTable[3] = (void*)Test_GetState;
    
    std::cout << "  Primary VTable at: " << g_PrimaryVTable << "\n";
    std::cout << "  vtable[0] (Initialize): " << g_PrimaryVTable[0] << "\n";
    std::cout << "  vtable[1] (Shutdown): " << g_PrimaryVTable[1] << "\n";
    std::cout << "  vtable[3] (GetState): " << g_PrimaryVTable[3] << "\n";
    
    // Initialize primary object
    std::cout << "\n2. Setting up primary object...\n";
    g_PrimaryObject.pVTable = g_PrimaryVTable;
    g_PrimaryObject.objectId = 1;
    g_PrimaryObject.objectState = 0;
    g_PrimaryObject.flags = 0;
    
    std::cout << "  Primary Object at: " << &g_PrimaryObject << "\n";
    std::cout << "  pVTable: " << g_PrimaryObject.pVTable << "\n";
    std::cout << "  objectId: " << g_PrimaryObject.objectId << "\n";
    
    // Initialize master database
    std::cout << "\n3. Setting up master database...\n";
    g_MasterDB.pVTable = g_PrimaryVTable;
    g_MasterDB.refCount = 1;
    g_MasterDB.stateFlags = 0x0001;
    g_MasterDB.pPrimaryObject = &g_PrimaryObject;
    g_MasterDB.primaryData1 = 0;
    g_MasterDB.primaryData2 = 0;
    
    std::cout << "  Master Database at: " << &g_MasterDB << "\n";
    std::cout << "  pVTable: " << g_MasterDB.pVTable << "\n";
    std::cout << "  refCount: " << g_MasterDB.refCount << "\n";
    std::cout << "  stateFlags: 0x" << std::hex << g_MasterDB.stateFlags << std::dec << "\n";
    std::cout << "  pPrimaryObject: " << g_MasterDB.pPrimaryObject << "\n";
    
    // Verify structure layout
    std::cout << "\n4. Verifying structure layout...\n";
    std::cout << "  sizeof(MasterDatabase) = " << sizeof(MasterDatabase) << " (expected: 36)\n";
    std::cout << "  sizeof(APIObject) = " << sizeof(APIObject) << " (expected: ~44)\n";
    
    // Check offsets
    std::cout << "\n5. Checking offsets...\n";
    std::cout << "  MasterDatabase.pVTable offset: " << offsetof(MasterDatabase, pVTable) << " (expected: 0)\n";
    std::cout << "  MasterDatabase.refCount offset: " << offsetof(MasterDatabase, refCount) << " (expected: 4)\n";
    std::cout << "  MasterDatabase.stateFlags offset: " << offsetof(MasterDatabase, stateFlags) << " (expected: 8)\n";
    std::cout << "  MasterDatabase.pPrimaryObject offset: " << offsetof(MasterDatabase, pPrimaryObject) << " (expected: 12)\n";
    
    std::cout << "  APIObject.pVTable offset: " << offsetof(APIObject, pVTable) << " (expected: 0)\n";
    std::cout << "  APIObject.objectId offset: " << offsetof(APIObject, objectId) << " (expected: 4)\n";
    std::cout << "  APIObject.objectState offset: " << offsetof(APIObject, objectState) << " (expected: 8)\n";
    
    // Try calling a vtable function
    std::cout << "\n6. Testing vtable function call...\n";
    APIObject* obj = (APIObject*)g_MasterDB.pPrimaryObject;
    
    if (obj && obj->pVTable) {
        void** vtable = (void**)obj->pVTable;
        InitializeFunc initFunc = (InitializeFunc)vtable[0];
        
        std::cout << "  Calling vtable[0] function...\n";
        int result = initFunc(obj, nullptr);
        std::cout << "  Result: " << result << "\n";
    } else {
        std::cout << "  ERROR: Object or vtable is null!\n";
    }
    
    // Test loading client.dll
    std::cout << "\n7. Loading client.dll...\n";
    HMODULE hClient = LoadLibraryA("client.dll");
    
    if (!hClient) {
        DWORD err = GetLastError();
        std::cout << "  ERROR: Could not load client.dll (error " << err << ")\n";
        std::cout << "  This is OK for testing structure setup.\n";
    } else {
        std::cout << "  SUCCESS: client.dll loaded at " << hClient << "\n";
        
        // Get SetMasterDatabase export
        SetMasterDatabase_t setMasterDB = (SetMasterDatabase_t)GetProcAddress(hClient, "SetMasterDatabase");
        
        if (setMasterDB) {
            std::cout << "  SetMasterDatabase found at: " << (void*)setMasterDB << "\n";
            std::cout << "\n  Calling SetMasterDatabase(&g_MasterDB)...\n";
            setMasterDB(&g_MasterDB);
            std::cout << "  SetMasterDatabase returned successfully!\n";
        } else {
            std::cout << "  ERROR: SetMasterDatabase not found!\n";
        }
        
        FreeLibrary(hClient);
    }
    
    std::cout << "\n=== Test Complete ===\n";
    return 0;
}
