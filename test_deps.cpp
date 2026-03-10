#include <windows.h>
#include <iostream>

int main() {
    std::cout << "=== Pre-loading dependencies ===\n";
    
    const char* deps[] = {
        "MSVCR71.dll",
        "dbghelp.dll",
        "r3d9.dll",
        "binkw32.dll",
        "pythonMXO.dll",
        "dllMediaPlayer.dll",
        "dllWebBrowser.dll",
        NULL
    };
    
    for (int i = 0; deps[i]; i++) {
        std::cout << "Loading " << deps[i] << "... ";
        std::cout.flush();
        HMODULE h = LoadLibraryA(deps[i]);
        std::cout << (h ? "OK" : "FAIL") << "\n";
    }
    
    std::cout << "\n=== Now loading client.dll ===\n";
    HMODULE hClient = LoadLibraryA("client.dll");
    std::cout << "client.dll: " << (hClient ? "SUCCESS" : "FAILED") << "\n";
    
    return 0;
}
