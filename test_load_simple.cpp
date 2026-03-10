#include <windows.h>
#include <iostream>

int main() {
    std::cout << "=== Simple LoadLibrary Test ===\n";
    
    // Try loading mxowrap.dll first
    std::cout << "\n1. Loading mxowrap.dll...\n";
    HMODULE hMxo = LoadLibraryA("mxowrap.dll");
    std::cout << "mxowrap.dll: " << (hMxo ? "SUCCESS" : "FAILED") << "\n";
    if (hMxo) FreeLibrary(hMxo);
    
    // Try loading client.dll
    std::cout << "\n2. Loading client.dll...\n";
    std::cout.flush();
    HMODULE hClient = LoadLibraryA("client.dll");
    std::cout << "client.dll: " << (hClient ? "SUCCESS" : "FAILED") << "\n";
    if (!hClient) {
        std::cout << "Error: " << GetLastError() << "\n";
    }
    
    return 0;
}
