/**
 * @file http_server.c
 * @brief Cross-platform C HTTP Server powered by H2O's picohttpparser.
 *
 * Runs natively on Windows (Winsock2), Linux, and macOS (POSIX sockets).
 * Exposes REST API endpoints and an interactive HTML status dashboard.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "picohttpparser.h"

#if defined(_WIN32) || defined(_WIN64)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET socket_t;
    #define INVALID_SOCK INVALID_SOCKET
    #define CLOSE_SOCK(s) closesocket(s)
#else
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    typedef int socket_t;
    #define INVALID_SOCK (-1)
    #define CLOSE_SOCK(s) close(s)
#endif

#define DEFAULT_PORT 8080
#define BUFFER_SIZE 4096
#define MAX_HEADERS 32

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

/**
 * @brief Initializes network subsystem for current OS.
 */
static int init_networking(void) {
#if defined(_WIN32) || defined(_WIN64)
    WSADATA wsa;
    return (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) ? 0 : -1;
#else
    return 0;
#endif
}

/**
 * @brief Cleans up network subsystem.
 */
static void cleanup_networking(void) {
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
}

/**
 * @brief Handles an incoming HTTP connection using H2O picohttpparser.
 */
static void handle_client(socket_t client_sock) {
    char buffer[BUFFER_SIZE];
    int bytes_read = (int)recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) {
        CLOSE_SOCK(client_sock);
        return;
    }
    buffer[bytes_read] = '\0';

    const char *method;
    size_t method_len;
    const char *path;
    size_t path_len;
    int minor_version;
    struct phr_header headers[MAX_HEADERS];
    size_t num_headers = MAX_HEADERS;

    int parse_res = phr_parse_request(buffer, (size_t)bytes_read,
                                      &method, &method_len,
                                      &path, &path_len,
                                      &minor_version,
                                      headers, &num_headers, 0);

    if (parse_res < 0) {
        const char *bad_req = "HTTP/1.1 400 Bad Request\r\nContent-Length: 15\r\n\r\n400 Bad Request";
        send(client_sock, bad_req, (int)strlen(bad_req), 0);
        CLOSE_SOCK(client_sock);
        return;
    }

    char response[BUFFER_SIZE];
    char body[BUFFER_SIZE];

    if (path_len >= 11 && strncmp(path, "/api/status", 11) == 0) {
        // JSON API Endpoint
        time_t now = time(NULL);
        snprintf(body, sizeof(body),
                 "{\n"
                 "  \"status\": \"online\",\n"
                 "  \"server\": \"H2O-picohttpparser-C-Server\",\n"
                 "  \"target_os\": \"%s\",\n"
                 "  \"target_arch\": \"%s\",\n"
                 "  \"timestamp\": %ld,\n"
                 "  \"http_minor_version\": %d,\n"
                 "  \"headers_received\": %zu\n"
                 "}\n",
                 get_target_os(), get_target_arch(), (long)now, minor_version, num_headers);

        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %zu\r\n"
                 "Connection: close\r\n"
                 "\r\n"
                 "%s",
                 strlen(body), body);
    } else {
        // HTML Dashboard
        snprintf(body, sizeof(body),
                 "<!DOCTYPE html>\n"
                 "<html>\n"
                 "<head><title>H2O C HTTP Server</title></head>\n"
                 "<body style='font-family: sans-serif; max-width: 600px; margin: 40px auto; padding: 20px; background: #0f172a; color: #f8fafc;'>\n"
                 "<h1 style='color: #38bdf8;'>H2O / C Cross-Platform HTTP Server</h1>\n"
                 "<p>Powered by H2O's <code>picohttpparser</code> compiled with <code>zig cc</code>.</p>\n"
                 "<ul>\n"
                 "<li><strong>Target OS:</strong> %s</li>\n"
                 "<li><strong>Architecture:</strong> %s</li>\n"
                 "<li><strong>REST API:</strong> <a style='color: #60a5fa;' href='/api/status'>/api/status</a></li>\n"
                 "</ul>\n"
                 "</body>\n"
                 "</html>\n",
                 get_target_os(), get_target_arch());

        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: text/html\r\n"
                 "Content-Length: %zu\r\n"
                 "Connection: close\r\n"
                 "\r\n"
                 "%s",
                 strlen(body), body);
    }

    send(client_sock, response, (int)strlen(response), 0);
    CLOSE_SOCK(client_sock);
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) port = DEFAULT_PORT;
    }

    if (init_networking() != 0) {
        fprintf(stderr, "Failed to initialize socket subsystem\n");
        return 1;
    }

    socket_t server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == INVALID_SOCK) {
        fprintf(stderr, "Socket creation failed\n");
        cleanup_networking();
        return 1;
    }

    int opt = 1;
    #if defined(_WIN32) || defined(_WIN64)
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    #else
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    #endif

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons((unsigned short)port);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "Bind failed on port %d\n", port);
        CLOSE_SOCK(server_sock);
        cleanup_networking();
        return 1;
    }

    if (listen(server_sock, 10) < 0) {
        fprintf(stderr, "Listen failed\n");
        CLOSE_SOCK(server_sock);
        cleanup_networking();
        return 1;
    }

    printf("====================================================\n");
    printf("H2O C HTTP Server running on http://127.0.0.1:%d\n", port);
    printf("Platform: %s (%s)\n", get_target_os(), get_target_arch());
    printf("API Endpoint: http://127.0.0.1:%d/api/status\n", port);
    printf("====================================================\n");
    fflush(stdout);

    // If running in test/demo mode or single request mode, accept connections
    // Accept up to 10 connections for interactive demo, or infinite loop
    while (1) {
        struct sockaddr_in client_addr;
        #if defined(_WIN32) || defined(_WIN64)
        int client_len = sizeof(client_addr);
        #else
        socklen_t client_len = sizeof(client_addr);
        #endif

        socket_t client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock != INVALID_SOCK) {
            handle_client(client_sock);
        }
    }

    CLOSE_SOCK(server_sock);
    cleanup_networking();
    return 0;
}
