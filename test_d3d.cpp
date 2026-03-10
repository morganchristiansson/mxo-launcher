#include <windows.h>
#include <iostream>

int main() {
    std::cout << "=== Testing Direct3D Context ===\n";
    
    // Check if we can create a D3D device
    HMODULE hD3D9 = LoadLibraryA("d3d9.dll");
    if (hD3D9) {
        std::cout << "d3d9.dll loaded: " << hD3D9 << "\n";
    } else {
        std::cout << "d3d9.dll FAILED to load\n";
    }
    
    return 0;
}
