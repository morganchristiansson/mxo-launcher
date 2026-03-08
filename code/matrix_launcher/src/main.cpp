/**
 * Matrix Online Launcher - Improved POC
 * Cross-compile from Linux: x86_64-w64-mingw32-g++ -o launcher.exe launcher.cpp -static -mwindows
 */

#include <windows.h>
#include <iostream>
#include <string>

bool AddDllSearchPath(const wchar_t* subfolder) {
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) return false;

    std::wstring dir = exePath;
    size_t pos = dir.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return false;
    dir = dir.substr(0, pos);

    std::wstring fullPath = dir + L"\\" + subfolder;
    return SetDllDirectoryW(fullPath.c_str()) != 0;  // or use AddDllDirectory
}

int main(int argc, char* argv[]) {
    std::cout << "Matrix Online Launcher POC\n";

    // 1. Add p_dlls to DLL search path (critical!)
    if (!AddDllSearchPath(L"p_dlls")) {
        std::cerr << "Warning: Failed to add p_dlls to search path\n";
        // continue anyway — may still work if deps are elsewhere
    }

    // 2. Load client.dll
    HMODULE hClient = LoadLibraryW(L"client.dll");
    if (!hClient) {
        DWORD err = GetLastError();
        std::cerr << "LoadLibrary failed: " << err << " (0x" << std::hex << err << ")\n";
        return 1;
    }
    std::cout << "client.dll loaded successfully\n";

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

    std::cout << "Found InitClientDLL and RunClientDLL\n";

    // 4. Call Init (with dummies — will crash until you RE the real params!)
    std::cout << "Calling InitClientDLL (dummy pointers)...\n";
    pInit(nullptr, nullptr /* placeholder — crash expected here until RE */);

    // 5. Run the game loop
    std::cout << "Entering RunClientDLL (game should appear)...\n";
    pRun();  // This should block until game exit

    // 6. Cleanup
    std::cout << "Game exited — calling TermClientDLL\n";
    if (pTerm) pTerm();

    FreeLibrary(hClient);
    std::cout << "Done.\n";
    return 0;
}
