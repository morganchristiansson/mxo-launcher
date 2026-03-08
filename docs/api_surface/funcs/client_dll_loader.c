/**
 * @file client_dll_loader.c
 * @brief Client.dll loading and initialization function
 * 
 * Function at 0x0040a420 - Loads client.dll and handles errors
 */

#include <windows.h>
#include <stdio.h>

// Global pointer to client.dll base address (located at 0x4d2c50)
static HMODULE g_hClientDll = NULL;

/**
 * @brief Load client.dll into memory
 * 
 * This function loads client.dll and stores the handle in a global variable.
 * If loading fails, it attempts to format and display an error message.
 * 
 * @return TRUE if client.dll loaded successfully, FALSE otherwise
 */
BOOL LoadClientDll(void)
{
    char* errorMessage = NULL;
    LPVOID formattedMessage = NULL;
    DWORD errorId = 0;
    
    // Load client.dll
    g_hClientDll = LoadLibraryA("client.dll");
    
    if (g_hClientDll == NULL) {
        // Failed to load - get the last error code
        SetLastError(GetLastError());
        
        // Attempt to format error message (MFC71 functions)
        formattedMessage = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            GetLastError(),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&errorMessage,
            0,
            NULL
        );
        
        if (formattedMessage != NULL) {
            // Display or log the error message
            // LocalFree the buffer when done
            LocalFree(formattedMessage);
        }
    }
    
    return (g_hClientDll != NULL) ? TRUE : FALSE;
}

/**
 * @brief Get the client.dll base address
 * 
 * @return HMODULE pointer to client.dll if loaded, NULL otherwise
 */
HMODULE GetClientDllHandle(void)
{
    return g_hClientDll;
}

// Function at 0x0040a420 (original assembly translated)
BOOL _client_dll_load_internal(void)
{
    char* var_18h = NULL;     // error message buffer
    char* var_14h = NULL;     // unused in this function
    char* var_10h = NULL;     // unused in this function
    char* var_ch = NULL;      // unused in this function
    char* var_4h = NULL;      // unused in this function
    
    // Push 0xffffffffffffffff (stack cookie)
    // Push 'i\x15I' (magic string - likely anti-debug/anti-tamper)
    
    // Allocate stack space for local variables
    
    // Load client.dll
    g_hClientDll = LoadLibraryA("client.dll");
    
    if (g_hClientDll != NULL) {
        // Success - DLL loaded
        return TRUE;
    }
    
    // Failure path: Get last error and format message
    DWORD errorCode = GetLastError();
    
    // Use MFC71 functions for error formatting
    // Ordinal_304 = FormatMessageA (FORMAT_MESSAGE_ALLOCATE_BUFFER)
    // Ordinal_578 = FormatMessageA (FORMAT_MESSAGE_FROM_SYSTEM)
    
    formattedMessage = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&var_18h,
        0,
        NULL
    );
    
    if (formattedMessage != NULL) {
        // Error message formatted - could be displayed or logged here
        // LocalFree(formattedMessage);
    }
    
    // Cleanup
    LocalFree(var_18h);
    
    return FALSE;
}

// End of file
