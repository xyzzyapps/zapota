/**
 * @file main.c
 * @brief Cross-platform Hello World demonstration compiled with zig cc.
 *
 * This program outputs a friendly greeting along with platform and architecture
 * diagnostics determined at compile time.
 */

#include <stdio.h>

/**
 * @brief Returns the target Operating System name determined via preprocessor macros.
 * @return const char* String representing the target OS.
 */
static const char* get_target_os(void) {
#if defined(_WIN32) || defined(_WIN64)
    return "Windows";
#elif defined(__APPLE__) && defined(__MACH__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown OS";
#endif
}

/**
 * @brief Returns the target CPU architecture name determined via preprocessor macros.
 * @return const char* String representing the CPU architecture.
 */
static const char* get_target_arch(void) {
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64 (64-bit AMD/Intel)";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "aarch64 (64-bit ARM)";
#elif defined(__i386__) || defined(_M_IX86)
    return "i386 (32-bit x86)";
#elif defined(__arm__) || defined(_M_ARM)
    return "arm (32-bit ARM)";
#else
    return "Unknown Architecture";
#endif
}

/**
 * @brief Returns the C standard library type or compiler identifier.
 * @return const char* String representing compiler/libc info.
 */
static const char* get_compiler_info(void) {
#if defined(__clang__)
    return "Clang / zig cc toolchain";
#elif defined(__GNUC__)
    return "GCC compatible";
#elif defined(_MSC_VER)
    return "MSVC compatible";
#else
    return "Standard C Compiler";
#endif
}

/**
 * @brief Application entry point.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return int Exit status code (0 for success).
 */
int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("========================================\n");
    printf("Hello, World from zig cc cross-compiler!\n");
    printf("========================================\n");
    printf("Target OS:           %s\n", get_target_os());
    printf("Target Architecture: %s\n", get_target_arch());
    printf("Toolchain Info:      %s\n", get_compiler_info());
    printf("========================================\n");

    return 0;
}
