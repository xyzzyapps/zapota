#include "ggml-backend.h"
#include <string.h>

enum ggml_backend_buffer_usage ggml_backend_buffer_get_usage(ggml_backend_buffer_t buffer) {
    (void)buffer;
    return GGML_BACKEND_BUFFER_USAGE_ANY;
}

void ggml_backend_tensor_memset(struct ggml_tensor * tensor, uint8_t value, size_t offset, size_t size) {
    if (tensor && tensor->data) {
        memset((char *)tensor->data + offset, value, size);
    }
}

void ggml_backend_tensor_set(struct ggml_tensor * tensor, const void * data, size_t offset, size_t size) {
    if (tensor && tensor->data) {
        memcpy((char *)tensor->data + offset, data, size);
    }
}
