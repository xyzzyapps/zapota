/**
 * @file demo_hiredis.c
 * @brief hiredis RESP command formatting + optional local Redis ping.
 */

#include "hiredis.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#endif

int main(void) {
    printf("====================================================\n");
    printf("hiredis Redis Client Demonstration\n");
    printf("====================================================\n");

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
#endif

    char *cmd = NULL;
    const int cmd_len = redisFormatCommand(&cmd, "SET %s %s", "zapota", "ok");
    if (cmd_len < 0 || !cmd) {
        fprintf(stderr, "redisFormatCommand failed\n");
        return 1;
    }
    printf("[1] Formatted RESP command (%d bytes):\n%.*s\n", cmd_len, cmd_len, cmd);
    hi_free(cmd);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 200000;
    redisContext *c = redisConnectWithTimeout("127.0.0.1", 6379, tv);
    if (c == NULL || c->err) {
        printf("[2] No Redis at 127.0.0.1:6379 (%s)\n", c ? c->errstr : "alloc failed");
        if (c) redisFree(c);
        printf("hiredis demonstration completed (format-only).\n");
#ifdef _WIN32
        WSACleanup();
#endif
        return 0;
    }

    redisReply *reply = redisCommand(c, "PING");
    if (reply && reply->type == REDIS_REPLY_STATUS) {
        printf("[2] PING -> %s\n", reply->str);
    } else {
        printf("[2] PING failed\n");
    }
    freeReplyObject(reply);
    redisFree(c);
    printf("hiredis demonstration completed successfully.\n");
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
