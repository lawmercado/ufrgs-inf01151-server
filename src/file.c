#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <string.h>
#include "file.h"
#include "log.h"

/**
 * Write a file in the specified path
 *
 * @param char* path The path of the file
 * @param char* buffer The content of the file
 * @param int length The buffer size of the file
 * @return 0 if no errors, -1 otherwise
 */
int file_write_buffer(char path[MAX_PATH_LENGTH], char *buffer, int length)
{
    FILE *file = NULL;
    int bytes_writen = 0, has_errors = 0;

    log_debug("file", "Writing in '%s'", path);

    file = fopen(path, "wb");

    if(file == NULL)
    {
        log_debug("file", "Could not open '%s' for writing", path);

        return -1;
    }

    bytes_writen = fwrite(buffer, sizeof(char), length, file);
    has_errors = bytes_writen != length;

    fclose(file);

    !has_errors ? log_debug("file", "Write sucessfull") : log_error("file", "Write unsucessfull for '%s'", path);

    return !has_errors ? 0 : -1;
}

/**
 * Fills a MACTimestamp struct with the corresponding times for the file
 *
 * @param char* path The path of the file
 * @param MACTimestamp* mac The struct to initialize
 * @return 0 if no errors, -1 otherwise
 */
int file_mac(char path[MAX_PATH_LENGTH], MACTimestamp *mac)
{
    struct stat st;

    if(stat(path, &st) == 0)
    {
        strftime(mac->m, MAX_TIMESTAMP_LENGTH, "%Y %b %d %H:%M", localtime(&st.st_mtime));
        strftime(mac->a, MAX_TIMESTAMP_LENGTH, "%Y %b %d %H:%M", localtime(&st.st_atime));
        strftime(mac->c, MAX_TIMESTAMP_LENGTH, "%Y %b %d %H:%M", localtime(&st.st_ctime));

        return 0;
    }

    return -1;
}

/**
 * Gets the file size for the specified file
 *
 * @param char* path The path of the file
 * @return the file size if no errors, -1 otherwise
 */
int file_size(char path[MAX_PATH_LENGTH])
{
    struct stat st;

    if(stat(path, &st) == 0)
    {
        return st.st_size;
    }

    return -1;
}

int file_exists(char path[MAX_PATH_LENGTH])
{
    struct stat st;

    return stat(path, &st);
}

int file_create_dir(char path[MAX_PATH_LENGTH])
{
    return mkdir(path, 0700);
}

int file_get_name_from_path(char *path, char *filename)
{
    int i = 0;
    for(i = strlen(path); i != 0; i--)
    {
        if (path[i] == '/')
        {
            strncpy(filename, path+i+1, ((strlen(path)-i)+1));
            printf("%s\n", filename);
            return 0;
        }
    }
    return -1;
}
