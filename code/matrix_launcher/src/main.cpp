/**
 * Matrix Online Launcher - Proof of Concept
 * 
 * A Windows executable launcher that loads external client.dll.
 * Built for cross-compilation from Linux using MinGW-w64.
 */

#include <iostream>
#include <windows.h>

// Windows DLL Loading Function Pointers
typedef HMODULE (__stdcall *LoadLibraryW_t)(LPCWSTR);
typedef FARPROC (__stdcall *GetProcAddress_t)(HMODULE, LPCSTR);
typedef BOOL (__stdcall *FreeLibrary_t)(HMODULE);

// Global DLL Loaders
static LoadLibraryW_t g_LoadLibraryW = nullptr;
static GetProcAddress_t g_GetProcAddress = nullptr;
static FreeLibrary_t g_FreeLibrary = nullptr;

/**
 * Initialize Windows DLL loading functions.
 * Called before loading any external DLLs.
 */
bool InitializeDllLoaders()
{
    // Load user32.dll for basic Windows API calls
    HMODULE hUser32 = g_LoadLibraryW(L"user32.dll");
    if (!hUser32)
        return false;
    
    // Get function pointers from user32.dll
    g_GetProcAddress = (GetProcAddress_t)g_GetProcAddress(hUser32, "GetProcAddress");
    g_FreeLibrary = (FreeLibrary_t)g_GetProcAddress(hUser32, "FreeLibrary");
    
    if (!g_GetProcAddress || !g_FreeLibrary)
    {
        g_FreeLibrary(hUser32);
        return false;
    }
    
    return true;
}

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
    
    // Load the external DLL
    HMODULE hClient = g_LoadLibraryW(widePath);
    if (!hClient)
    {
        std::cout << "Failed to load client.dll: " << dllPath << std::endl;
        return false;
    }
    
    std::cout << "Successfully loaded client.dll" << std::endl;
    
    // Call the DLL's main function if available
    // The client.dll should export a function named "DllMain"
    typedef BOOL (__stdcall *DllMain_t)(HMODULE, DWORD, LPVOID);
    DllMain_t pDllMain = (DllMain_t)g_GetProcAddress(hClient, "DllMain");
    
    if (pDllMain)
    {
        std::cout << "Found DllMain function in client.dll" << std::endl;
        // Execute the DLL's main function
        pDllMain(hClient, DLL_PROCESS_ATTACH, nullptr);
    }
    
    // Free the DLL when done
    g_FreeLibrary(hClient);
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
    
    std::cout << "\nInitializing Windows DLL Loading..." << std::endl;
    
    if (!InitializeDllLoaders())
    {
        std::cout << "Failed to initialize DLL loaders." << std::endl;
        return 1;
    }
    
    std::cout << "Loading external client.dll..." << std::endl;
    
    // Load the client.dll from the parent directory
    if (!LoadClientDll("../../client.dll"))
    {
        std::cout << "Failed to load client.dll. Exiting." << std::endl;
        return 1;
    }
    
    std::cout << "=========================================" << std::endl;
    std::cout << "Matrix Online Launcher completed." << std::endl;
    std::cout << "=========================================" << std::endl;
    
    return 0;
}
