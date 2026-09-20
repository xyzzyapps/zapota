/**
 * @file demo_yaml.c
 * @brief LibYAML Stream Parser Demonstration.
 */

#include <stdio.h>
#include <string.h>
#include "yaml.h"

int main(void) {
    printf("====================================================\n");
    printf("LibYAML Document Stream Parser Demonstration\n");
    printf("====================================================\n");

    const char *yaml_input = 
        "project: zapota\n"
        "version: 1.0.0\n"
        "targets:\n"
        "  - windows\n"
        "  - linux\n"
        "  - macos\n"
        "status: ready\n";

    printf("[YAML Input]:\n%s\n", yaml_input);

    yaml_parser_t parser;
    yaml_event_t event;

    if (!yaml_parser_initialize(&parser)) {
        fprintf(stderr, "Failed to initialize YAML parser\n");
        return 1;
    }

    yaml_parser_set_input_string(&parser, (const unsigned char*)yaml_input, strlen(yaml_input));

    printf("[Parsed Tokens]:\n");
    int done = 0;
    while (!done) {
        if (!yaml_parser_parse(&parser, &event)) {
            fprintf(stderr, "Parser error %d\n", parser.error);
            break;
        }

        switch (event.type) {
            case YAML_SCALAR_EVENT:
                printf("  -> Scalar: %s\n", (char*)event.data.scalar.value);
                break;
            case YAML_SEQUENCE_START_EVENT:
                printf("  -> Sequence Start [\n");
                break;
            case YAML_SEQUENCE_END_EVENT:
                printf("  -> Sequence End ]\n");
                break;
            case YAML_MAPPING_START_EVENT:
                printf("  -> Mapping Start {\n");
                break;
            case YAML_MAPPING_END_EVENT:
                printf("  -> Mapping End }\n");
                break;
            case YAML_STREAM_END_EVENT:
                done = 1;
                break;
            default:
                break;
        }

        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);
    printf("\nLibYAML parsing completed successfully.\n");
    return 0;
}
