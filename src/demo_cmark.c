/**
 * @file demo_cmark.c
 * @brief CommonMark (cmark) Markdown Parser Demonstration.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cmark.h"

int main(void) {
    printf("====================================================\n");
    printf("CommonMark (cmark) Parser Demonstration\n");
    printf("====================================================\n");
    printf("cmark Version: %s\n\n", cmark_version_string());

    const char *markdown_input = 
        "# Zig Multi-Platform Build\n\n"
        "Cross-compiling C libraries seamlessly across:\n"
        "- **Windows** (x86_64 / ARM64)\n"
        "- **Linux** (musl / glibc)\n"
        "- **macOS** (Intel / Apple Silicon)\n\n"
        "```c\n"
        "printf(\"Hello from cmark rendered HTML!\\n\");\n"
        "```\n";

    printf("[Markdown Input]:\n%s\n", markdown_input);

    char *html_output = cmark_markdown_to_html(markdown_input, strlen(markdown_input), CMARK_OPT_DEFAULT);
    if (!html_output) {
        fprintf(stderr, "Failed to render Markdown to HTML\n");
        return 1;
    }

    printf("[Rendered HTML Output]:\n%s\n", html_output);
    free(html_output);

    printf("cmark demonstration completed successfully.\n");
    return 0;
}
