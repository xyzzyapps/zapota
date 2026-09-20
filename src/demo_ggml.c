#include "ggml.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== GGML Tensor Demo ===\n");
    printf("GGML Version: %s\n", ggml_version());

    struct ggml_init_params params = {
        .mem_size = 16 * 1024 * 1024,
        .mem_buffer = NULL,
        .no_alloc = false,
    };
    struct ggml_context * ctx = ggml_init(params);
    if (!ctx) {
        fprintf(stderr, "Failed to init ggml context\n");
        return 1;
    }

    /* Create 1D tensors of 4 float elements */
    struct ggml_tensor * a = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 4);
    struct ggml_tensor * b = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 4);

    float * a_data = (float *)a->data;
    float * b_data = (float *)b->data;
    for (int i = 0; i < 4; i++) {
        a_data[i] = (float)(i + 1);
        b_data[i] = (float)((i + 1) * 10);
    }

    /* Build computation graph: result = a + b */
    struct ggml_tensor * sum = ggml_add(ctx, a, b);

    /* Construct the computation graph */
    struct ggml_cgraph * gf = ggml_new_graph(ctx);
    ggml_build_forward_expand(gf, sum);

    printf("Graph initialized: %d node(s), capacity %d\n", ggml_graph_n_nodes(gf), ggml_graph_size(gf));
    printf("Tensor A [4]: [%.1f, %.1f, %.1f, %.1f]\n", a_data[0], a_data[1], a_data[2], a_data[3]);
    printf("Tensor B [4]: [%.1f, %.1f, %.1f, %.1f]\n", b_data[0], b_data[1], b_data[2], b_data[3]);

    /* Perform element-wise addition on the tensor buffers */
    float * sum_data = (float *)sum->data;
    for (int i = 0; i < 4; i++) {
        sum_data[i] = a_data[i] + b_data[i];
    }
    printf("Result A + B: [%.1f, %.1f, %.1f, %.1f]\n", sum_data[0], sum_data[1], sum_data[2], sum_data[3]);

    /* Create a 2D matrix tensor: 2x2 */
    struct ggml_tensor * mat = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, 2, 2);
    float * m_data = (float *)mat->data;
    m_data[0] = 1.0f; m_data[1] = 2.0f;
    m_data[2] = 3.0f; m_data[3] = 4.0f;
    printf("Matrix 2x2: [[%.1f, %.1f], [%.1f, %.1f]] (bytes: %zu)\n",
           m_data[0], m_data[1], m_data[2], m_data[3], ggml_nbytes(mat));

    ggml_free(ctx);
    printf("GGML context freed successfully.\n");
    return 0;
}
