// Test loading mxowrap.dll via import table (like the original launcher)
// Compile with: i686-w64-mingw32-g++ -o test_mxowrap_import.exe test_mxowrap_import.cpp -L~/MxO_7.6005 -lmxowrap

#include <windows.h>
#include <iostream>

// Import from mxowrap.dll (exports DbgHelp functions)
extern "C" {
    __declspec(dllimport) BOOL WINAPI MiniDumpWriteDump(
        HANDLE hProcess,
        DWORD ProcessId,
        HANDLE hFile,
        MINIDUMP_TYPE DumpType,
        PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
        PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
        PMINIDUMP_CALLBACK_INFORMATION CallbackParam
    );
}

int main(int argc, char* argv[]) {
    std::cout << "Program started - mxowrap.dll should already be loaded via imports\n";

    // Get the module handle to confirm it loaded
    HMODULE hMxo = GetModuleHandleA("mxowrap.dll");
    if (hMxo) {
        std::cout << "mxowrap.dll is loaded at: " << hMxo << "\n";
    } else {
        std::cerr << "mxowrap.dll is NOT loaded! Error: " << GetLastError() << "\n";
        return 1;
    }

    // Try to call a function from mxowrap.dll
    std::cout << "MiniDumpWriteDump function is at: " << (void*)MiniDumpWriteDump << "\n";

    std::cout << "Test successful!\n";
    return 0;
}
