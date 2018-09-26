#ifndef __FILE_H__
#define __FILE_H__

#define MAX_FILENAME_LENGTH 255
#define MAX_PATH_LENGTH 4096
#define MAX_TIMESTAMP_LENGTH 20

typedef struct {
    char m[MAX_TIMESTAMP_LENGTH];
    char a[MAX_TIMESTAMP_LENGTH];
    char c[MAX_TIMESTAMP_LENGTH];
} MACTimestamp;

/**
 * Write a file in the specified path
 *
 * @param char* path The path of the file
 * @param char* buffer The content of the file
 * @param int length The buffer size of the file
 * @return 0 if no errors, -1 otherwise
 */
int file_write_buffer(char path[MAX_PATH_LENGTH], char *buffer, int length);

/**
 * Fills a MACTimestamp struct with the corresponding times for the file
 *
 * @param char* path The path of the file
 * @param MACTimestamp* mac The struct to initialize
 * @return 0 if no errors, -1 otherwise
 */
int file_mac(char path[MAX_PATH_LENGTH], MACTimestamp *mac);

/**
 * Gets the file size for the specified file
 *
 * @param char* path The path of the file
 * @return the file size if no errors, -1 otherwise
 */
int file_size(char path[MAX_PATH_LENGTH]);

int file_create_dir(char path[MAX_PATH_LENGTH]);

#endif
