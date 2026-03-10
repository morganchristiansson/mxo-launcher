#include <windows.h>
#include <iostream>

int main() {
    std::cout << "Attempting to load mxowrap.dll...\n";

    HMODULE hMxo = LoadLibraryA("mxowrap.dll");
    if (!hMxo) {
        DWORD err = GetLastError();
        std::cerr << "LoadLibrary failed with error: " << err << " (0x" << std::hex << err << ")\n";

        switch (err) {
            case 126: std::cerr << "DLL or dependency not found\n"; break;
            case 127: std::cerr << "Procedure not found\n"; break;
            case 1114: std::cerr << "DllMain returned FALSE\n"; break;
            default: break;
        }
        return 1;
    }

    std::cout << "mxowrap.dll loaded successfully at: " << hMxo << "\n";

    // Try to find exports
    FARPROC proc = GetProcAddress(hMxo, "PatchCtl_PatchCheck");
    if (proc) {
        std::cout << "Found PatchCtl_PatchCheck at: " << (void*)proc << "\n";
    } else {
        std::cout << "PatchCtl_PatchCheck not found (not exported)\n";
    }

    FreeLibrary(hMxo);
    std::cout << "Unloaded successfully\n";
    return 0;
}
