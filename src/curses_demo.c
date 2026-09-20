/**
 * @file curses_demo.c
 * @brief Interactive PDCurses / TUI demonstration.
 *
 * Renders a terminal dashboard with status windows, color attributes,
 * diagnostic telemetry, and keyboard interaction.
 */

#include "curses.h"
#include <stdio.h>
#include <stdlib.h>

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

int main(int argc, char *argv[]) {
    int auto_quit = 0;
    if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 't') {
        auto_quit = 1; // test mode: non-blocking single draw
    }

    initscr();
    start_color();
    cbreak();
    noecho();
    curs_set(0);

    init_pair(1, COLOR_CYAN, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    init_pair(4, COLOR_RED, COLOR_BLACK);

    WINDOW *header_win = newwin(5, 76, 1, 2);
    WINDOW *main_win = newwin(12, 76, 6, 2);

    box(header_win, 0, 0);
    wattron(header_win, COLOR_PAIR(1));
    mvwprintw(header_win, 1, 20, "PDCURSES TUI CROSS-PLATFORM DASHBOARD");
    wattroff(header_win, COLOR_PAIR(1));
    mvwprintw(header_win, 2, 12, "Orchestrated with Zig Build System across Win / Mac / Linux");
    mvwprintw(header_win, 3, 22, "Target: %s | Arch: %s", get_target_os(), get_target_arch());

    box(main_win, 0, 0);
    wattron(main_win, COLOR_PAIR(2));
    mvwprintw(main_win, 1, 3, "[ SYSTEM MODULES STATUS ]");
    wattroff(main_win, COLOR_PAIR(2));

    mvwprintw(main_win, 3, 5, "1. Hello World Demo    --> [ OK ] (Compile-time Platform Detection)");
    mvwprintw(main_win, 4, 5, "2. H2O HTTP Server     --> [ OK ] (picohttpparser REST + HTML)");
    mvwprintw(main_win, 5, 5, "3. SQLite Database     --> [ OK ] (In-memory SQL & Prepared Stmts)");
    mvwprintw(main_win, 6, 5, "4. PDCurses TUI Engine --> [ OK ] (Virtual ANSI Color Windows)");
    mvwprintw(main_win, 7, 5, "5. Raylib Graphics     --> [ OK ] (Hardware-Accelerated 2D/3D)");

    wattron(main_win, COLOR_PAIR(3));
    mvwprintw(main_win, 9, 5, "Commands: [R]efresh, [Q]uit");
    wattroff(main_win, COLOR_PAIR(3));

    wrefresh(header_win);
    wrefresh(main_win);

    if (!auto_quit) {
        int ch;
        while ((ch = getch()) != 'q' && ch != 'Q' && ch != 27) {
            // Loop until 'q' or ESC
            wrefresh(header_win);
            wrefresh(main_win);
        }
    }

    delwin(header_win);
    delwin(main_win);
    endwin();

    printf("\nPDCurses TUI demonstration terminated cleanly.\n");
    return 0;
}
