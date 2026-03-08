/**
 * Matrix Online Launcher - Proof of Concept
 * 
 * A minimal Windows executable launcher.
 * Built for cross-compilation from Linux using MinGW-w64.
 */

#include <iostream>

// Check if building for Windows
#if defined(_WIN32) || defined(__MINGW32__)
    #define BUILD_WINDOWS 1
#else
    #define BUILD_WINDOWS 0
#endif

int main(int argc, char* argv[])
{
    std::cout << "==========================================" << std::endl;
    std::cout << "Matrix Online Launcher" << std::endl;
    std::cout << "Version: 1.0.0" << std::endl;
    std::cout << "==========================================" << std::endl;
    
#if BUILD_WINDOWS
    std::cout << "\nNote: This is a Windows executable." << std::endl;
    std::cout << "To run on Linux, use Wine:" << std::endl;
    std::cout << "  wine launcher.exe" << std::endl;
#endif
    
    return 0;
}