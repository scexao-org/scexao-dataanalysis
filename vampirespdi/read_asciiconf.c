#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "read_asciiconf.h"


// Define the block size for reallocation.
// The array will grow by this many elements each time it runs out of space.
// This is more efficient than reallocating for every single new item.
#define BLOCK_SIZE 8




char* trim_whitespace(char* str) {
    if (!str) return NULL;
    char *end;

    // Trim leading space
    while (isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == 0) { // All spaces?
        return str;
    }

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }

    // Write new null terminator
    *(end + 1) = '\0';

    return str;
}



KeyValuePair* parse_config(const char* filename, int* final_count) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        *final_count = 0;
        return NULL;
    }

    KeyValuePair* array = NULL;
    int count = 0;
    int capacity = 0;

    char line_buffer[1024];

    while (fgets(line_buffer, sizeof(line_buffer), file)) {
        // --- NEW: Handle inline comments first ---
        char* comment_text = NULL;
        char* comment_start = strchr(line_buffer, '#');
        if (comment_start != NULL) {
            // A comment was found. Duplicate the comment text for later.
            comment_text = strdup(trim_whitespace(comment_start + 1));
            // Terminate the line at the comment, so the rest of the parsing ignores it.
            *comment_start = '\0';
        }

        // Trim whitespace from the (potentially shortened) line
        char* line = trim_whitespace(line_buffer);

        // Ignore empty lines (or lines that are now empty after stripping comments)
        if (strlen(line) == 0) {
            // If we allocated memory for a comment but the line is otherwise empty, free it.
            free(comment_text);
            continue;
        }

        // --- Resize the array if necessary ---
        if (count >= capacity) {
            int new_capacity = capacity + BLOCK_SIZE;
            printf("--> Resizing array: capacity from %d to %d\n", capacity, new_capacity);
            KeyValuePair* temp = realloc(array, new_capacity * sizeof(KeyValuePair));
            if (temp == NULL) {
                perror("Failed to reallocate memory");
                free(comment_text);
                free_config(array, count);
                fclose(file);
                *final_count = 0;
                return NULL;
            }
            array = temp;
            capacity = new_capacity;
        }

        // --- Parse the key and value from the line ---
        char* key_end = line;
        while (*key_end != '\0' && !isspace((unsigned char)*key_end)) {
            key_end++;
        }

        char* value_start = key_end;
        while (*value_start != '\0' && isspace((unsigned char)*value_start)) {
            value_start++;
        }

        if (*key_end != '\0') {
            *key_end = '\0';
        }

        // --- Handle quoted vs. unquoted values ---
        if (*value_start == '\'') {
            value_start++;
            char* value_end = strchr(value_start, '\'');
            if (value_end != NULL) {
                *value_end = '\0';
            }
        } else {
            // Trim trailing whitespace from the value if it's not quoted
            value_start = trim_whitespace(value_start);
        }

        // --- Store the results ---
        array[count].key = strdup(line);
        array[count].value = strdup(value_start);
        array[count].comment = comment_text; // Assign the duplicated comment (or NULL)

        if (array[count].key == NULL || array[count].value == NULL) {
            fprintf(stderr, "Error: strdup failed. Out of memory?\n");
            free(array[count].key);
            free(array[count].value);
            free(array[count].comment); // Free comment text if allocated
            free_config(array, count);
            fclose(file);
            *final_count = 0;
            return NULL;
        }

        count++;
    }

    fclose(file);
    *final_count = count;

    if (count > 0 && count < capacity) {
        KeyValuePair* temp = realloc(array, count * sizeof(KeyValuePair));
        if (temp != NULL) {
            array = temp;
        }
    }

    printf("Successfully parsed %d key-value pairs:\n", *final_count);
    printf("------------------------------------------------------------------\n");
    for (int i = 0; i < *final_count; i++) {
        printf("Pair %d:  Key='%-20s' Value='%-25s'", i + 1, array[i].key, array[i].value);
        if (array[i].comment) {
            printf(" Comment='%s'", array[i].comment);
        }
        printf("\n");
    }
    printf("------------------------------------------------------------------\n\n");


    return array;
}

void free_config(KeyValuePair* config_array, int count) {
    if (config_array == NULL) {
        return;
    }

    for (int i = 0; i < count; i++) {
        free(config_array[i].key);
        free(config_array[i].value);
        // Free the comment only if it's not NULL
        if (config_array[i].comment != NULL) {
            free(config_array[i].comment);
        }
    }

    free(config_array);
}
