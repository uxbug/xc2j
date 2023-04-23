#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FIELDS 100
#define MAX_CHARS_PER_FIELD 100
#define BUFFER_SIZE 1048576 // 1MB

void clear_buffer(char *buffer, int length) {
    memset(buffer, 0, length);
}

void escape_string(char *dest, const char *src) {
    while (*src) {
        switch (*src) {
            case '\\':
            case '"':
            case '/':
                *dest++ = '\\';
                *dest++ = *src++;
                break;
            case '\b':
                *dest++ = '\\';
                *dest++ = 'b';
                src++;
                break;
            case '\f':
                *dest++ = '\\';
                *dest++ = 'f';
                src++;
                break;
            case '\n':
                *dest++ = '\\';
                *dest++ = 'n';
                src++;
                break;
            case '\r':
                *dest++ = '\\';
                *dest++ = 'r';
                src++;
                break;
            case '\t':
                *dest++ = '\\';
                *dest++ = 't';
                src++;
                break;
            default:
                if ((unsigned char)*src <= 0x1F) {  // Check for ASCII control characters
                    int n = snprintf(dest, 7, "\\u%04x", (unsigned char)*src);
                    dest += n;
                    src++;
                } else {
                    *dest++ = *src++;
                }
        }
    }
    *dest = '\0';
}

int main(int argc, char *argv[]) {
    // Check command line arguments
    if (argc != 3) {
        printf("Usage: %s input_file output_file\n", argv[0]);
        return 1;
    }

    char *input_filename = argv[1];
    char *output_filename = argv[2];

    // Open input file for reading
    FILE *input_file = fopen(input_filename, "r");
    if (!input_file) {
        printf("Failed to open input file \"%s\"\n", input_filename);
        return 1;
    }

    // Read the first line of the CSV file to get the field names
    char line[MAX_CHARS_PER_FIELD * MAX_FIELDS];
    if (!fgets(line, sizeof(line), input_file)) {
        printf("Failed to read field names from input file\n");
        return 1;
    }

    // Count the number of fields in the CSV file
    int num_fields = 0;
    char *field_names[MAX_FIELDS];
    char *token = strtok(line, ",");
    while (token != NULL && num_fields < MAX_FIELDS) {
        // Remove any trailing newline, carriage return, space or tab characters from the token string
        size_t len = strlen(token);
        while (len > 0 && (token[len-1] == '\n' || token[len-1] == '\r' || token[len-1] == ' ' || token[len-1] == '\t')) {
            token[--len] = '\0';
        }
        field_names[num_fields++] = strdup(token);
        token = strtok(NULL, ",");
    }

    // Open output file for writing
    FILE *output_file = fopen(output_filename, "w");
    if (!output_file) {
        printf("Failed to open output file \"%s\"\n", output_filename);
        return 1;
    }

    // Loop over the remaining lines of the CSV file and write JSON objects
    char buffer[BUFFER_SIZE];
    clear_buffer(buffer, BUFFER_SIZE);
    int pos = 0;
    while (fgets(line, sizeof(line), input_file)) {
        if (pos + BUFFER_SIZE / 2 > BUFFER_SIZE) {
            fwrite(buffer, sizeof(char), pos, output_file);
            clear_buffer(buffer, BUFFER_SIZE);
            pos = 0;
        }

        buffer[pos++] = '{';

        // Tokenize the line and write field values as JSON key/value pairs
        int field_index = 0;
        token = strtok(line, ",");
        while (token != NULL && field_index < num_fields) {
            // Remove any trailing newline, carriage return, space or tab characters from the token string
            size_t len = strlen(token);
            while (len > 0 && (token[len-1] == '\n' || token[len-1] == '\r' || token[len-1] == ' ' || token[len-1] == '\t')) {
                token[--len] = '\0';
            }
            char escaped_value[MAX_CHARS_PER_FIELD * 2 + 1];
            escape_string(escaped_value, token);
            int n = snprintf(buffer + pos, BUFFER_SIZE / 2, "\"%s\":\"%s\"", field_names[field_index], escaped_value);
            if (n < BUFFER_SIZE / 2 - pos) {
                pos += n;
                if (field_index < num_fields - 1) {
                    buffer[pos++] = ',';
                }
            } else {
                fwrite(buffer, sizeof(char), pos, output_file);
                clear_buffer(buffer, BUFFER_SIZE);
                pos = 0;
                n = snprintf(buffer, BUFFER_SIZE / 2, "\"%s\":\"%s\"", field_names[field_index], escaped_value);
                pos += n;
                if (field_index < num_fields - 1) {
                    buffer[pos++] = ',';
                }
            }
            token = strtok(NULL, ",");
            field_index++;
        }

        // Fill in null values if there are fewer fields than expected
        while (field_index < num_fields) {
            int n = snprintf(buffer + pos, BUFFER_SIZE / 2, "\"%s\":null", field_names[field_index]);
            if (n < BUFFER_SIZE / 2 - pos) {
                pos += n;
                if (field_index < num_fields - 1) {
                    buffer[pos++] = ',';
                }
            } else {
                fwrite(buffer, sizeof(char), pos, output_file);
                clear_buffer(buffer, BUFFER_SIZE);
                pos = 0;
                n = snprintf(buffer, BUFFER_SIZE / 2, "\"%s\":null", field_names[field_index]);
                pos += n;
                if (field_index < num_fields - 1) {
                    buffer[pos++] = ',';
                }
            }
            field_index++;
        }

        buffer[pos++] = '}';
        buffer[pos++] = '\n';

        // Check if this is the last line in the CSV file
        long curr_pos = ftell(input_file);
        if (fgets(line, sizeof(line), input_file)) {
            // There is at least one more line in the CSV file, so add a comma separator between JSON objects
            fseek(input_file, curr_pos, SEEK_SET);  // Move the file pointer back to the start of the next line
        } else {
            fwrite(buffer, sizeof(char), pos, output_file);
            clear_buffer(buffer, BUFFER_SIZE);
            pos = 0;
        }
    }

    fflush(output_file);

    // Clean up
    fclose(input_file);
    fclose(output_file);

    return 0;
}
