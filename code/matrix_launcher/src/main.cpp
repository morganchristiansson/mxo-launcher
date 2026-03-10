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

    // 0. Pre-load ALL dependencies before client.dll
    // This prevents hangs during client.dll's DllMain
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
            std::cerr << "  WARNING: Could not load " << preload_dlls[i] << " (error " << GetLastError() << ")\n";
        }
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

    // Now load client.dll (should work after pre-loading dependencies)
    std::cout << "\n=== ATTEMPTING LoadLibraryW(\"client.dll\") ===\n";
    std::cout << "This is where client.dll's DllMain will execute...\n";
    std::cout.flush();

    HMODULE hClient = LoadLibraryW(L"client.dll");
    
    std::cout << "=== LoadLibraryW RETURNED: " << (hClient ? "SUCCESS" : "FAILED") << " ===\n";
    std::cout.flush();
    
    if (!hClient) {
        DWORD err = GetLastError();
        std::cerr << "LoadLibraryW failed with error: " << err << " (0x" << std::hex << err << ")\n";

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
                break;
            case 193:
                std::cerr << "ERROR 193: Bad EXE format.\n";
                std::cerr << "  -> Wrong architecture (32-bit vs 64-bit)?\n";
                break;
        }

        std::cerr << "\nTIP: Run from game directory: cd ~/MxO_7.6005 && wine launcher.exe\n";
        return 1;
    }
    std::cout << "=== LoadLibraryW SUCCESS ===\n";
    std::cout << "client.dll loaded at: " << hClient << "\n";
    std::cout << "\n=== GETTING EXPORTS ===\n";
    std::cout.flush();

    // 3. Get the real entry points (from MxOemu forum knowledge)
    using InitClientDLL_t   = void (*)(void* /* interface1 */, void* /* interface2 */, ... /* many more */);
    using RunClientDLL_t    = void (*)();
    using TermClientDLL_t   = void (*)();

    auto pInit = (InitClientDLL_t)  GetProcAddress(hClient, "InitClientDLL");
    auto pRun  = (RunClientDLL_t)   GetProcAddress(hClient, "RunClientDLL");
    auto pTerm = (TermClientDLL_t)  GetProcAddress(hClient, "TermClientDLL");
    
    std::cout << "InitClientDLL: " << (void*)pInit << "\n";
    std::cout << "RunClientDLL: " << (void*)pRun << "\n";
    std::cout << "TermClientDLL: " << (void*)pTerm << "\n";
    std::cout.flush();

    if (!pInit || !pRun) {
        std::cerr << "ERROR: Missing required exports (InitClientDLL / RunClientDLL)\n";
        FreeLibrary(hClient);
        return 1;
    }

    std::cout << "\n=== CALLING InitClientDLL ===\n";
    std::cout << "WARNING: Using dummy parameters - may crash!\n";
    std::cout.flush();
    pInit(nullptr, nullptr /* placeholder — crash expected here until RE */);

    std::cout << "\n=== CALLING RunClientDLL ===\n";
    std::cout << "Game window should appear...\n";
    std::cout.flush();
    pRun();  // This should block until game exit

    // 6. Cleanup
    std::cout << "\n=== Cleanup ===\n";
    std::cout << "Game exited — calling TermClientDLL\n";
    if (pTerm) pTerm();

    FreeLibrary(hClient);
    std::cout << "Done.\n";
    return 0;
}
