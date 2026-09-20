/**
 * demo_json_c.c - JSON-C parser/builder demo
 *
 * Demonstrates JSON-C library usage:
 *   - Building nested JSON objects and arrays
 *   - Serializing to a JSON string
 *   - Parsing a JSON string back into an object tree
 *   - Querying and verifying field values
 */

#include "json.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void) {
    printf("=== JSON-C Demo ===\n");

    /* --- Build a nested JSON object --- */
    struct json_object *root = json_object_new_object();
    struct json_object *meta = json_object_new_object();
    struct json_object *features = json_object_new_array();

    /* Populate meta sub-object */
    json_object_object_add(meta, "version", json_object_new_string("0.16.0"));
    json_object_object_add(meta, "targets", json_object_new_int(6));
    json_object_object_add(meta, "stable",  json_object_new_boolean(1));

    /* Populate features array */
    json_object_array_add(features, json_object_new_string("cross-compile"));
    json_object_array_add(features, json_object_new_string("no-libc-deps"));
    json_object_array_add(features, json_object_new_string("c-vendor-libs"));

    /* Assemble root object */
    json_object_object_add(root, "project",  json_object_new_string("zapota"));
    json_object_object_add(root, "meta",     meta);
    json_object_object_add(root, "features", features);

    /* Serialize to string */
    const char *serialized = json_object_to_json_string_ext(
        root, JSON_C_TO_STRING_PRETTY);
    printf("Serialized JSON:\n%s\n\n", serialized);

    /* --- Parse a JSON string --- */
    const char *raw_json =
        "{"
        "  \"language\": \"zig\","
        "  \"score\": 42,"
        "  \"tags\": [\"fast\", \"safe\", \"cross\"]"
        "}";

    struct json_object *parsed = json_tokener_parse(raw_json);
    if (!parsed) {
        fprintf(stderr, "Parse failed\n");
        json_object_put(root);
        return 1;
    }

    /* Query fields */
    struct json_object *lang_obj, *score_obj, *tags_obj;
    json_object_object_get_ex(parsed, "language", &lang_obj);
    json_object_object_get_ex(parsed, "score",    &score_obj);
    json_object_object_get_ex(parsed, "tags",     &tags_obj);

    const char *lang  = json_object_get_string(lang_obj);
    int         score = json_object_get_int(score_obj);

    printf("Parsed fields:\n");
    printf("  language = %s\n", lang);
    printf("  score    = %d\n", score);
    printf("  tags[0]  = %s\n", json_object_get_string(
        json_object_array_get_idx(tags_obj, 0)));
    printf("  tags[1]  = %s\n", json_object_get_string(
        json_object_array_get_idx(tags_obj, 1)));
    printf("  tags[2]  = %s\n", json_object_get_string(
        json_object_array_get_idx(tags_obj, 2)));

    /* Verify */
    assert(strcmp(lang, "zig") == 0);
    assert(score == 42);
    assert(json_object_array_length(tags_obj) == 3);

    printf("\nAll assertions passed.\n");

    /* Release references */
    json_object_put(root);
    json_object_put(parsed);

    printf("JSON-C demo complete.\n");
    return 0;
}
