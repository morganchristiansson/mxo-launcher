#include <windows.h>
#include <iostream>

int main() {
    // Feature 0x17 = 23 decimal
    BOOL result = IsProcessorFeaturePresent(0x17);
    std::cout << "IsProcessorFeaturePresent(0x17) = " << result << "\n";
    
    // Try a few others
    for (int i = 0; i < 30; i++) {
        if (IsProcessorFeaturePresent(i)) {
            std::cout << "Feature " << i << " is PRESENT\n";
        }
    }
    
    return 0;
}
