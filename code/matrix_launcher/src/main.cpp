/**
 * Matrix Online Launcher - Proof of Concept
 * 
 * A Windows executable launcher that loads external client.dll.
 * Built for cross-compilation from Linux using MinGW-w64.
 */

#include <iostream>
#include <windows.h>

/**
 * Load and execute external client.dll.
 * 
 * @param dllPath Path to the client DLL (relative or absolute)
 * @return true if DLL loaded successfully, false otherwise
 */
bool LoadClientDll(const char* dllPath)
{
    // Convert path to wide string for Windows API
    wchar_t widePath[4096];
    int len = MultiByteToWideChar(CP_UTF8, 0, dllPath, -1, widePath, sizeof(widePath) / sizeof(wchar_t));
    if (len == 0)
        return false;
    
    // Load the external DLL using standard Windows API
    HMODULE hClient = LoadLibraryW(widePath);
    if (!hClient)
    {
        std::cout << "Failed to load client.dll: " << dllPath << std::endl;
        return false;
    }
    
    std::cout << "Successfully loaded client.dll" << std::endl;
    
    // Call the DLL's main function if available
    // The client.dll should export a function named "DllMain"
    typedef BOOL (__stdcall *DllMain_t)(HMODULE, DWORD, LPVOID);
    DllMain_t pDllMain = (DllMain_t)GetProcAddress(hClient, "DllMain");
    
    if (pDllMain)
    {
        std::cout << "Found DllMain function in client.dll" << std::endl;
        // Execute the DLL's main function
        pDllMain(hClient, DLL_PROCESS_ATTACH, nullptr);
    }
    
    // Free the DLL when done
    FreeLibrary(hClient);
    return true;
}

/**
 * Main entry point for the launcher.
 */
int main(int argc, char* argv[])
{
    std::cout << "=========================================" << std::endl;
    std::cout << "Matrix Online Launcher" << std::endl;
    std::cout << "Version: 1.0.0" << std::endl;
    std::cout << "=========================================" << std::endl;
    
    std::cout << "\nLoading external client.dll..." << std::endl;
    
    // Load the client.dll from the current directory
    if (!LoadClientDll("client.dll"))
    {
        std::cout << "Failed to load client.dll. Exiting." << std::endl;
        return 1;
    }
    
    std::cout << "=========================================" << std::endl;
    std::cout << "Matrix Online Launcher completed." << std::endl;
    std::cout << "=========================================" << std::endl;
    
    return 0;
}
