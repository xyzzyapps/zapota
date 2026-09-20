/**
 * @file sqlite_demo.c
 * @brief Cross-platform SQLite demonstration.
 *
 * Demonstrates opening an embedded SQLite database, creating a table schema,
 * inserting records via prepared statements with parameter bindings, and
 * executing query loops with row extraction.
 */

#include <stdio.h>
#include <stdlib.h>
#include "sqlite3.h"

/**
 * @brief Returns target OS string.
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
 * @brief Returns target CPU architecture.
 */
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
    printf("====================================================\n");
    printf("SQLite Embedded Database Demonstration\n");
    printf("====================================================\n");
    printf("SQLite Version:      %s\n", sqlite3_libversion());
    printf("Target OS:           %s\n", get_target_os());
    printf("Target Architecture: %s\n", get_target_arch());
    printf("====================================================\n\n");

    sqlite3 *db = NULL;
    int rc = sqlite3_open(":memory:", &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    printf("[1] Database opened successfully (:memory:)\n");

    // 1. Create table
    const char *sql_create = 
        "CREATE TABLE benchmarks ("
        "  id INTEGER PRIMARY KEY,"
        "  name TEXT NOT NULL,"
        "  category TEXT NOT NULL,"
        "  score REAL NOT NULL"
        ");";

    char *err_msg = NULL;
    rc = sqlite3_exec(db, sql_create, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error in CREATE TABLE: %s\n", err_msg);
        sqlite3_close(db);
        return 1;
    }
    printf("[2] Table 'benchmarks' created successfully.\n");

    // 2. Insert sample records using prepared statements
    const char *sql_insert = "INSERT INTO benchmarks (id, name, category, score) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *stmt = NULL;

    struct {
        int id;
        const char *name;
        const char *category;
        double score;
    } sample_data[] = {
        { 1, "Native Zig Compiler", "Build Toolchain", 99.8 },
        { 2, "H2O picohttpparser", "Networking", 98.5 },
        { 3, "Embedded SQLite Engine", "Database", 97.2 },
        { 4, "PDCurses TUI Engine", "User Interface", 94.0 },
        { 5, "Raylib Graphics Engine", "Visualization", 96.5 }
    };

    size_t num_records = sizeof(sample_data) / sizeof(sample_data[0]);

    for (size_t i = 0; i < num_records; ++i) {
        rc = sqlite3_prepare_v2(db, sql_insert, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Failed to prepare insert statement: %s\n", sqlite3_errmsg(db));
            break;
        }

        sqlite3_bind_int(stmt, 1, sample_data[i].id);
        sqlite3_bind_text(stmt, 2, sample_data[i].name, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, sample_data[i].category, -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 4, sample_data[i].score);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            fprintf(stderr, "Failed to insert row %zu: %s\n", i + 1, sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }
    printf("[3] Inserted %zu rows via prepared statements.\n\n", num_records);

    // 3. Query records
    printf("--------------------------------------------------------------------\n");
    printf("| %-4s | %-26s | %-18s | %-7s |\n", "ID", "Name", "Category", "Score");
    printf("--------------------------------------------------------------------\n");

    const char *sql_select = "SELECT id, name, category, score FROM benchmarks;";
    rc = sqlite3_prepare_v2(db, sql_select, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            const char *name = (const char*)sqlite3_column_text(stmt, 1);
            const char *category = (const char*)sqlite3_column_text(stmt, 2);
            double score = sqlite3_column_double(stmt, 3);

            printf("| %-4d | %-26s | %-18s | %-7.1f |\n", id, name, category, score);
        }
        sqlite3_finalize(stmt);
    }
    printf("--------------------------------------------------------------------\n");

    sqlite3_close(db);
    printf("\n[4] Database closed cleanly.\n");
    return 0;
}
