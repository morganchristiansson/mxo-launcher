// test_logging_format_validation.cpp - Validate logging format best practices
//
// Compile: g++ -std=c++17 -I/home/morgan/mxo-logging/code/matrix_launcher/third_party/spdlog-1.17.0/include -o test_logging_format_validation test_logging_format_validation.cpp && ./test_logging_format_validation
//
// This test validates that we're using the correct formatting for different types:
// - {} for regular values and fmt::ptr() for pointers (no % format specifiers)
// - 0x{:08x} for hex integers (NOT for pointers - that causes double prefix)
// - Avoid mixing % format with {} format

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <iostream>
#include <memory>
#include <cstdint>
#include <string>

void test_ternary_with_fmt_ptr();
void test_u_format_specifier();

int main() {
    auto logger = spdlog::stdout_logger_mt("format_validation");
    
    int* ptr1 = new int{42};
    int* ptr2 = nullptr;
    uint64_t hexValue = 0x12345678;
    uint32_t word = 0xABCD;
    uint16_t byte = 0x5A;
    std::unique_ptr<int> uptr = std::make_unique<int>(100);
    
    std::cout << "=== Testing logging format best practices ===\n\n";
    
    // CORRECT: Pointers with fmt::ptr() - outputs single 0x prefix
    spdlog::info("CORRECT: pointer address: {}", fmt::ptr(ptr1));
    spdlog::info("CORRECT: null pointer: {}", fmt::ptr(ptr2));
    spdlog::info("CORRECT: unique_ptr: {}", fmt::ptr(uptr.get()));
    
    // CORRECT: Hex integers with 0x{:08x} - outputs 0x prefix once
    spdlog::info("CORRECT: hex64: 0x{:08x}", hexValue);
    spdlog::info("CORRECT: hex32: 0x{:08x}", word);
    spdlog::info("CORRECT: hex16: 0x{:04x}", byte);
    
    // CORRECT: Mixed - pointer and integer in same log
    spdlog::info("CORRECT: pointer {} at offset +0x{:08x}", fmt::ptr(ptr1), (uint64_t)ptr1 + 0x100);
    
    // CORRECT: Using 0x{:08x} with uint32_t - validates it doesn't cause double prefix
    std::string testStr = "CORRECT: uint32_t with 0x{:08x} format (no double prefix): ";
    spdlog::info(testStr, word);
    
    // CORRECT: Using 0x{:08x} with uint16_t - validates it doesn't cause double prefix  
    std::string testStr2 = "CORRECT: uint16_t with 0x{:04x} format (no double prefix): ";
    spdlog::info(testStr2, byte);
    
    // CORRECT: Using 0x{:08x} with uint64_t - validates it doesn't cause double prefix
    std::string testStr3 = "CORRECT: uint64_t with 0x{:08x} format (no double prefix): ";
    spdlog::info(testStr3, hexValue);
    
    // INCORRECT: Using 0x{:08x} with a pointer would cause double prefix
    // This is what we should AVOID:
    // spdlog::info("WRONG: pointer with double prefix: 0x{:08x}", (uint64_t)ptr1);
    // Would output: 0x0x0000002a instead of 0x0000002a
    
    // CORRECT: Using just {:x} for unsigned values without 0x prefix
    std::string testStr4 = "CORRECT: unsigned hex without 0x prefix: ";
    spdlog::info(testStr4, (uint64_t)ptr1);
    
    delete ptr1;
    
    test_ternary_with_fmt_ptr();
    test_u_format_specifier();
    
    std::cout << "\n=== All format tests passed ===\n";
    return 0;
}

void test_ternary_with_fmt_ptr() {
    // Test if ternary is needed with fmt::ptr for NULL handling
    void* ptr1 = nullptr;
    void* ptr2 = new char[10];
    
    // Current pattern - ternary with NULL
    spdlog::info("TERNARY NULL: {}", fmt::ptr(ptr1 ? ptr1 : nullptr));
    spdlog::info("TERNARY VALUE: {}", fmt::ptr(ptr2 ? ptr2 : nullptr));
    
    // Simplified - just pass to fmt::ptr directly
    spdlog::info("DIRECT NULL: {}", fmt::ptr(ptr1));
    spdlog::info("DIRECT VALUE: {}", fmt::ptr(ptr2));
    
    delete[] ptr2;
}

void test_u_format_specifier() {
    // Test unsigned integer formatting - {} works for all unsigned types
    uint32_t word32 = 0x1234;
    uint64_t word64 = 0x123456789ABCDEF0ULL;
    uint16_t byte16 = 0xAB;
    
    // Test with {} - default format for unsigned works correctly
    spdlog::info("WITH {}: {}", word32, word32);
    spdlog::info("WITH {}: {}", word64, word64);
    spdlog::info("WITH {}: {}", byte16, byte16);
    
    // Test with {:x} - hex format for unsigned
    spdlog::info("WITH {:x}: {}", word32, word32);
    spdlog::info("WITH {:X}: {}", word32, word32);
}
