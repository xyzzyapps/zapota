/**
 * @file demo_wslay.c
 * @brief RFC 6455 WebSocket frame encode/decode via wslay (in-memory).
 */

#include <wslay/wslay.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

struct memio {
    uint8_t buf[256];
    size_t len;
    size_t off;
};

static ssize_t send_cb(const uint8_t *data, size_t len, int flags, void *user_data) {
    (void)flags;
    struct memio *io = (struct memio *)user_data;
    if (io->len + len > sizeof(io->buf)) return WSLAY_ERR_CALLBACK_FAILURE;
    memcpy(io->buf + io->len, data, len);
    io->len += len;
    return (ssize_t)len;
}

static ssize_t recv_cb(uint8_t *buf, size_t len, int flags, void *user_data) {
    (void)flags;
    struct memio *io = (struct memio *)user_data;
    const size_t avail = io->len - io->off;
    if (avail == 0) return 0;
    const size_t n = avail < len ? avail : len;
    memcpy(buf, io->buf + io->off, n);
    io->off += n;
    return (ssize_t)n;
}

static int genmask_cb(uint8_t *buf, size_t len, void *user_data) {
    (void)user_data;
    memset(buf, 0x37, len);
    return 0;
}

int main(void) {
    printf("====================================================\n");
    printf("wslay WebSocket Frame Demonstration\n");
    printf("====================================================\n");
    printf("[1] wslay " WSLAY_VERSION "\n");

    struct memio io;
    memset(&io, 0, sizeof(io));

    struct wslay_frame_callbacks cbs = { send_cb, recv_cb, genmask_cb };
    wslay_frame_context_ptr ctx = NULL;
    if (wslay_frame_context_init(&ctx, &cbs, &io) != 0) {
        fprintf(stderr, "wslay_frame_context_init failed\n");
        return 1;
    }

    const char *hello = "Hello";
    struct wslay_frame_iocb iocb;
    memset(&iocb, 0, sizeof(iocb));
    iocb.fin = 1;
    iocb.opcode = WSLAY_TEXT_FRAME;
    iocb.mask = 1;
    iocb.payload_length = 5;
    iocb.data = (const uint8_t *)hello;
    iocb.data_length = 5;

    const ssize_t sent = wslay_frame_send(ctx, &iocb);
    if (sent != 5) {
        fprintf(stderr, "wslay_frame_send failed: %zd\n", sent);
        wslay_frame_context_free(ctx);
        return 1;
    }
    printf("[2] Sent masked TEXT frame, wire %zu bytes:", io.len);
    for (size_t i = 0; i < io.len; i++) printf(" %02x", io.buf[i]);
    printf("\n");

    wslay_frame_context_free(ctx);
    ctx = NULL;
    io.off = 0;
    if (wslay_frame_context_init(&ctx, &cbs, &io) != 0) {
        fprintf(stderr, "recv context init failed\n");
        return 1;
    }

    struct wslay_frame_iocb got;
    memset(&got, 0, sizeof(got));
    const ssize_t recvd = wslay_frame_recv(ctx, &got);
    if (recvd != 5 || got.data_length != 5 || memcmp(got.data, "Hello", 5) != 0) {
        fprintf(stderr, "wslay_frame_recv failed (n=%zd)\n", recvd);
        wslay_frame_context_free(ctx);
        return 1;
    }
    printf("[3] Received TEXT payload: %.*s (fin=%u opcode=%u masked=%u)\n",
           (int)got.data_length, (const char *)got.data, got.fin, got.opcode, got.mask);

    wslay_frame_context_free(ctx);
    printf("wslay demonstration completed successfully.\n");
    return 0;
}
