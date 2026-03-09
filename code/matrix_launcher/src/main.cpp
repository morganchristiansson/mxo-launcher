/**
 * Matrix Online Launcher - Improved POC
 * Cross-compile from Linux: x86_64-w64-mingw32-g++ -o launcher.exe launcher.cpp -static -mwindows
 * 
 * IMPORTANT: This launcher MUST be run from the game directory!
 *   cd ~/MxO_7.6005
 *   wine ../code/matrix_launcher/launcher.exe
 */

#include <windows.h>
#include <iostream>
#include <string>
#include <direct.h>  // for _getcwd
#include "dll_deployer.h"

// Exported function that client.dll imports from the launcher process
// This must be exported so client.dll can find it during DllMain
extern "C" __declspec(dllexport) void* SetMasterDatabase(void* db) {
    std::cout << "SetMasterDatabase called with: " << db << "\n";
    // TODO: Implement proper database interface
    // For now, just return the pointer to satisfy the import
    return db;
}

/**
 * Check if critical DLL dependencies exist in the current directory
 */
void check_dependencies() {
    const char* critical_dlls[] = {
        "client.dll",
        "msvcr71.dll",   // Required for malloc in DllMain
        "msvcp71.dll",
        "MFC71.dll",
        "binkw32.dll",
        "r3d9.dll",
        "pythonMXO.dll",
        "mxowrap.dll",
        "dllMediaPlayer.dll",
        "dllWebBrowser.dll"
    };
    
    std::cout << "\n=== Checking dependencies ===\n";
    int missing = 0;
    
    for (size_t i = 0; i < sizeof(critical_dlls)/sizeof(critical_dlls[0]); i++) {
        DWORD attr = GetFileAttributesA(critical_dlls[i]);
        if (attr == INVALID_FILE_ATTRIBUTES) {
            std::cerr << "  MISSING: " << critical_dlls[i] << "\n";
            missing++;
        } else {
            std::cout << "  OK: " << critical_dlls[i] << "\n";
        }
    }
    
    if (missing > 0) {
        std::cerr << "\nWARNING: " << missing << " critical DLL(s) missing!\n";
        std::cerr << "Make sure you run this launcher FROM the game directory.\n\n";
    }
}

int main(int argc, char* argv[]) {
    // Show current working directory
    char cwd[MAX_PATH];
    _getcwd(cwd, MAX_PATH);
    std::cout << "Matrix Online Launcher POC\n";
    std::cout << "Working directory: " << cwd << "\n";

    // Check dependencies first
    check_dependencies();

    // 0. Pre-load MSVCR71.dll and MFC71.dll to initialize their state
    // client.dll's DllMain calls malloc() from MSVCR71.dll, which needs the heap initialized
    // The original launcher links to both, so we need to load them in the right order
    std::cout << "\n=== Pre-loading runtime DLLs ===\n";
    
    HMODULE hMfc71 = LoadLibraryA("MFC71.dll");
    if (hMfc71) {
        std::cout << "MFC71.dll loaded at: " << hMfc71 << "\n";
    } else {
        std::cerr << "Warning: Could not load MFC71.dll (error " << GetLastError() << ")\n";
    }
    
    HMODULE hMsvcr71 = LoadLibraryA("MSVCR71.dll");
    if (hMsvcr71) {
        std::cout << "MSVCR71.dll loaded at: " << hMsvcr71 << "\n";
    } else {
        std::cerr << "ERROR: Could not load MSVCR71.dll (error " << GetLastError() << ")\n";
        std::cerr << "client.dll requires MSVCR71.dll for malloc() in DllMain!\n";
        return 1;
    }

    // 1. Deploy DLLs from p_dlls/ to game root (matches original launcher behavior)
    std::cout << "\n=== Deploying DLLs ===\n";
    int deployed = deploy_standard_dlls();
    if (deployed == 3) {
        std::cout << "All DLLs deployed successfully\n";
    } else if (deployed > 0) {
        std::cout << "Some DLLs deployed, some failed\n";
    } else {
        std::cerr << "DLL deployment failed!\n";
    }

    // 2. Load client.dll with detailed debugging
    std::cout << "\n=== Loading client.dll ===\n";

    // Debug: Check what malloc resolves to in the process
    typedef void* (*malloc_t)(size_t);
    malloc_t pMalloc = (malloc_t)GetProcAddress(hMsvcr71, "malloc");
    std::cout << "malloc from MSVCR71.dll: " << (void*)pMalloc << "\n";

    // Try a test malloc to see if the heap is working
    if (pMalloc) {
        void* test = pMalloc(128);
        std::cout << "Test malloc(128): " << test << "\n";
        if (test) {
            typedef void (*free_t)(void*);
            free_t pFree = (free_t)GetProcAddress(hMsvcr71, "free");
            if (pFree) pFree(test);
        } else {
            std::cerr << "WARNING: malloc(128) returned NULL! Heap may not be initialized.\n";
        }
    }

    // Load with LOAD_LIBRARY_AS_DATAFILE first to check base address
    HMODULE hClientData = LoadLibraryExW(L"client.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (hClientData) {
        std::cout << "LOAD_LIBRARY_AS_DATAFILE: " << hClientData << "\n";
        // The actual load address is the low 32 bits
        HMODULE actualBase = (HMODULE)((DWORD_PTR)hClientData & 0x7FFFFFFF);
        std::cout << "Actual base address: " << actualBase << "\n";
        std::cout << "Expected base: 0x62000000\n";
        if ((DWORD_PTR)actualBase != 0x62000000) {
            std::cout << "DLL was relocated from preferred base!\n";
        }
        FreeLibrary(hClientData);
    }

    // Now try normal load
    std::cout << "\nCalling LoadLibraryW...\n";

    // Debug: Check the IAT entry for malloc before loading client.dll
    // This is what client.dll's DllMain will call
    typedef void* (*malloc_t)(size_t);
    malloc_t clientMalloc = (malloc_t)GetProcAddress(hMsvcr71, "malloc");
    std::cout << "MSVCR71 malloc address: " << (void*)clientMalloc << "\n";

    HMODULE hClient = LoadLibraryW(L"client.dll");
    if (!hClient) {
        DWORD err = GetLastError();
        std::cerr << "LoadLibraryW failed with error: " << err << " (0x" << std::hex << err << ")\n";

        // Decode common error codes
        switch (err) {
            case 126:
                std::cerr << "ERROR 126: DLL or one of its dependencies not found.\n";
                std::cerr << "  -> Check that ALL required DLLs are in the game directory!\n";
                break;
            case 127:
                std::cerr << "ERROR 127: Procedure not found.\n";
                std::cerr << "  -> A function import could not be resolved.\n";
                break;
            case 1114:
                std::cerr << "ERROR 1114: DllMain returned FALSE.\n";
                std::cerr << "  -> DllMain initialization failed!\n";
                std::cerr << "  -> The DLL loaded, but DllMain returned FALSE during PROCESS_ATTACH.\n";
                break;
            case 193:
                std::cerr << "ERROR 193: Bad EXE format.\n";
                std::cerr << "  -> Wrong architecture (32-bit vs 64-bit)?\n";
                break;
            default:
                break;
        }

        std::cerr << "\nTIP: Run from game directory: cd ~/MxO_7.6005 && wine launcher.exe\n";

        // Try to get more info: can we load the DLL with DONT_RESOLVE_DLL_REFERENCES?
        // This loads the DLL but doesn't call DllMain or resolve imports
        std::cout << "\nAttempting diagnostic load (DONT_RESOLVE_DLL_REFERENCES)...\n";
        HMODULE hClientNoResolve = LoadLibraryExW(L"client.dll", NULL, DONT_RESOLVE_DLL_REFERENCES);
        if (hClientNoResolve) {
            std::cout << "  DLL loaded at: " << hClientNoResolve << "\n";
            std::cout << "  Expected base: 0x62000000\n";
            if ((DWORD_PTR)hClientNoResolve != 0x62000000) {
                std::cout << "  WARNING: DLL was relocated! (not at preferred base)\n";
            }

            // Check what's in the IAT for malloc (RVA 0x868278)
            DWORD_PTR* pIatMalloc = (DWORD_PTR*)((char*)hClientNoResolve + 0x868278);
            std::cout << "  IAT entry for malloc (RVA 0x868278): " << (void*)*pIatMalloc << "\n";
            std::cout << "  (Should be 0 or a pointer to hint/name table before resolution)\n";

            FreeLibrary(hClientNoResolve);
        } else {
            std::cerr << "  Even DONT_RESOLVE_DLL_REFERENCES failed: " << GetLastError() << "\n";
        }

        return 1;
    }
    std::cout << "client.dll loaded at: " << hClient << "\n";

    // 3. Get the real entry points (from MxOemu forum knowledge)
    using InitClientDLL_t   = void (*)(void* /* interface1 */, void* /* interface2 */, ... /* many more */);
    using RunClientDLL_t    = void (*)();
    using TermClientDLL_t   = void (*)();

    auto pInit = (InitClientDLL_t)  GetProcAddress(hClient, "InitClientDLL");
    auto pRun  = (RunClientDLL_t)   GetProcAddress(hClient, "RunClientDLL");
    auto pTerm = (TermClientDLL_t)  GetProcAddress(hClient, "TermClientDLL");

    if (!pInit || !pRun) {
        std::cerr << "Missing required exports (InitClientDLL / RunClientDLL)\n";
        FreeLibrary(hClient);
        return 1;
    }

    std::cout << "Found InitClientDLL at: " << (void*)pInit << "\n";
    std::cout << "Found RunClientDLL at: " << (void*)pRun << "\n";

    // 4. Call Init (with dummies — will crash until you RE the real params!)
    std::cout << "\n=== Calling InitClientDLL ===\n";
    std::cout << "WARNING: Using dummy parameters - crash likely!\n";
    pInit(nullptr, nullptr /* placeholder — crash expected here until RE */);

    // 5. Run the game loop
    std::cout << "\n=== Entering RunClientDLL ===\n";
    std::cout << "Game window should appear...\n";
    pRun();  // This should block until game exit

    // 6. Cleanup
    std::cout << "\n=== Cleanup ===\n";
    std::cout << "Game exited — calling TermClientDLL\n";
    if (pTerm) pTerm();

    FreeLibrary(hClient);
    std::cout << "Done.\n";
    return 0;
}
