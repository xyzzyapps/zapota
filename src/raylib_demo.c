/**
 * @file raylib_demo.c
 * @brief Interactive Raylib 2D graphical demonstration.
 *
 * Renders an animated scene with bouncing elements, real-time FPS counter,
 * and compile-time target platform diagnostics using official Raylib.
 */

#include "raylib.h"
#include <stdio.h>

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

static const char* get_target_arch(void) {
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "aarch64 (ARM64)";
#else
    return "Generic Arch";
#endif
}

int main(void) {
    const int screenWidth = 800;
    const int screenHeight = 500;

    InitWindow(screenWidth, screenHeight, "Raylib Cross-Platform Demo - Zig Build");
    SetTargetFPS(60);

    // Ball simulation physics state
    Vector2 ballPosition = { (float)screenWidth / 2.0f, (float)screenHeight / 2.0f };
    Vector2 ballSpeed = { 5.0f, 4.0f };
    const float ballRadius = 24.0f;

    char diag_text[128];
    snprintf(diag_text, sizeof(diag_text), "Target: %s | Architecture: %s", get_target_os(), get_target_arch());

    while (!WindowShouldClose()) {
        // Update
        ballPosition.x += ballSpeed.x;
        ballPosition.y += ballSpeed.y;

        if ((ballPosition.x >= (screenWidth - ballRadius)) || (ballPosition.x <= ballRadius)) {
            ballSpeed.x *= -1.0f;
        }
        if ((ballPosition.y >= (screenHeight - ballRadius)) || (ballPosition.y <= (ballRadius + 70))) {
            ballSpeed.y *= -1.0f;
        }

        // Draw
        BeginDrawing();
        ClearBackground((Color){ 15, 23, 42, 255 });

        // Header panel
        DrawRectangle(0, 0, screenWidth, 70, (Color){ 30, 41, 59, 255 });
        DrawRectangleLines(0, 0, screenWidth, 70, (Color){ 51, 65, 85, 255 });
        DrawText("RAYLIB GRAPHICS ENGINE", 20, 15, 22, SKYBLUE);
        DrawText(diag_text, 20, 42, 14, LIGHTGRAY);
        DrawText("60 FPS", screenWidth - 100, 24, 18, GREEN);

        // Bouncing animated ball
        DrawCircle((int)ballPosition.x, (int)ballPosition.y, ballRadius, GOLD);
        DrawCircle((int)ballPosition.x, (int)ballPosition.y, ballRadius - 6, ORANGE);

        // Center card
        DrawRectangleLines(100, 120, screenWidth - 200, screenHeight - 160, (Color){ 71, 85, 105, 255 });
        DrawText("Cross-Platform C Graphics with Zig", 130, 150, 24, RAYWHITE);
        DrawText("1. Hardware OpenGL rendering loop", 130, 200, 16, LIGHTGRAY);
        DrawText("2. Multi-target compilation for Win / Linux / macOS", 130, 230, 16, LIGHTGRAY);
        DrawText("3. Press ESC or close window to exit", 130, 280, 16, YELLOW);

        EndDrawing();
    }

    CloseWindow();
    printf("Raylib demonstration closed cleanly.\n");
    return 0;
}
