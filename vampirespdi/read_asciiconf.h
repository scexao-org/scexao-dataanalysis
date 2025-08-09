
#ifndef VAMPIRESPDI_READ_ASCIICONF_H
#define VAMPIRESPDI_READ_ASCIICONF_H


// A structure to hold a single key-value pair and an optional comment.
// Key, value, and comment are dynamically allocated strings.
typedef struct {
    char *key;
    char *value;
    char *comment; // New field for inline comments
} KeyValuePair;



/**
 * @brief Trims leading and trailing whitespace from a string, in-place.
 * @param str The string to trim.
 * @return A pointer to the beginning of the trimmed string.
 */
char* trim_whitespace(char* str);

/**
 * @brief Parses a key-value configuration file.
 * @param filename The path to the configuration file.
 * @param count A pointer to an integer that will store the final number of key-value pairs read.
 * @return A pointer to a dynamically allocated array of KeyValuePair structs, or NULL on failure.
 * The caller is responsible for freeing this memory using free_config().
 */
KeyValuePair* parse_config(const char* filename, int* count);

/**
 * @brief Frees all memory associated with the configuration data.
 * @param config_array The array of KeyValuePair structs.
 * @param count The number of elements in the array.
 */
void free_config(KeyValuePair* config_array, int count);

#endif
