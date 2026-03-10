#include <windows.h>
#include <iostream>

int main() {
    std::cout << "=== Test Load with DONT_RESOLVE_DLL_REFERENCES ===\n";
    
    HMODULE hClient = LoadLibraryExA("client.dll", NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (hClient) {
        std::cout << "client.dll loaded (no resolve): " << hClient << "\n";
        FreeLibrary(hClient);
    } else {
        std::cout << "client.dll failed: " << GetLastError() << "\n";
    }
    
    return 0;
}
