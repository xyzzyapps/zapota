#include "ggml-backend-impl.h"
#include "ggml-cpu-impl.h"
#include "ggml.h"

bool ggml_cpu_extra_compute_forward(struct ggml_compute_params * params, struct ggml_tensor * op) {
    (void)params; (void)op;
    return false;
}

bool ggml_cpu_extra_work_size(int n_threads, const struct ggml_tensor * op, size_t * size) {
    (void)n_threads; (void)op; (void)size;
    return false;
}
