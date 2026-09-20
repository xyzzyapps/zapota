/**
 * demo_duktape.c - Duktape JavaScript engine demo
 *
 * Demonstrates embedding Duktape 2.7.0 for lightweight JS scripting:
 *   - Heap creation
 *   - Evaluating arithmetic and string JS expressions
 *   - Calling a JS function from C
 *   - JSON serialization from JS
 */

#include "duktape.h"
#include <stdio.h>
#include <stdlib.h>

/* Fatal error handler required by Duktape */
static void duk_fatal_handler(void *udata, const char *msg) {
    (void)udata;
    fprintf(stderr, "Duktape fatal error: %s\n", msg ? msg : "(null)");
    abort();
}

int main(void) {
    printf("=== Duktape JS Engine Demo ===\n");

    /* Create a Duktape heap */
    duk_context *ctx = duk_create_heap(NULL, NULL, NULL, NULL, duk_fatal_handler);
    if (!ctx) {
        fprintf(stderr, "Failed to create Duktape heap\n");
        return 1;
    }

    /* Evaluate a simple arithmetic expression */
    duk_eval_string(ctx, "1 + 2 + 3 * 7");
    printf("1 + 2 + 3 * 7 = %d\n", (int)duk_get_int(ctx, -1));
    duk_pop(ctx);

    /* Evaluate a string expression */
    duk_eval_string(ctx, "'Hello from ' + 'JavaScript!'");
    printf("String: %s\n", duk_get_string(ctx, -1));
    duk_pop(ctx);

    /* Define and call a JS function */
    duk_eval_string(ctx,
        "(function fibonacci(n) {"
        "  if (n <= 1) return n;"
        "  return fibonacci(n-1) + fibonacci(n-2);"
        "})(10)"
    );
    printf("fibonacci(10) = %d\n", (int)duk_get_int(ctx, -1));
    duk_pop(ctx);

    /* JSON construction and serialization */
    duk_eval_string(ctx,
        "JSON.stringify({"
        "  name: 'zapota',"
        "  version: '0.16',"
        "  features: ['cross-compile', 'c-libraries', 'no-deps']"
        "})"
    );
    printf("JSON: %s\n", duk_get_string(ctx, -1));
    duk_pop(ctx);

    /* Math operations using JS built-ins */
    duk_eval_string(ctx, "Math.PI.toFixed(6)");
    printf("Math.PI = %s\n", duk_get_string(ctx, -1));
    duk_pop(ctx);

    duk_destroy_heap(ctx);

    printf("Duktape demo complete.\n");
    return 0;
}
