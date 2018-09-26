#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include "sync.h"
#include "file.h"
#include "log.h"

/**
 * Initializes the user directory
 *
 * @param char* username The username
 * @return 0 if no errors, -1 otherwise
 */
int sync_init(char *dir_path)
{
    
}

/**
 * Stop the synchronization process
 *
 */
void sync_stop()
{
    pthread_mutex_lock(&__event_handling_mutex);

    while(__is_event_processing)
    {
        pthread_cond_wait(&__events_done_processing, &__event_handling_mutex);
    }

    __stop_event_handling = 1;

    pthread_mutex_unlock(&__event_handling_mutex);

    pthread_join(__watcher_thread, NULL);

    __watcher_unset();

    log_debug("sync", "Synchronization process ended");
}

/**
 * Updates the file in the synchronized directory
 *
 * @param char* name The name of the file
 * @param char* buffer The content of the file
 * @param int length The buffer size of the file
 * @return 0 if no errors, -1 otherwise
 */
int sync_update_file(char name[MAX_FILENAME_LENGTH], char *buffer, int length)
{
    char path[MAX_PATH_LENGTH];

    sync_stop();

    bzero((void *)path, MAX_PATH_LENGTH);

    strcat(path, __watched_dir_path);
    strcat(path, "/");
    strcat(path, name);

    if(file_write_buffer(path, buffer, length) != 0)
    {
        log_error("sync", "Could not update the file '%s'", name);

        return -1;
    }

    if(sync_init(__watched_dir_path) != 0)
    {
        log_error("sync", "Could not initialize the synchronization process");

        return -1;
    }

    return 0;
}

/**
 * List the content of the watched directory
 *
 * @return 0 if no errors, -1 otherwise
 */
int sync_list_files()
{
    DIR *watched_dir;
    struct dirent *entry;
    char path[MAX_PATH_LENGTH];
    MACTimestamp entryMAC;

    watched_dir = opendir(__watched_dir_path);

    if(watched_dir)
    {
        while((entry = readdir(watched_dir)) != NULL)
        {
            if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }

            bzero((void *)path, MAX_PATH_LENGTH);
            strcat(path, __watched_dir_path);
            strcat(path, "/");
            strcat(path, entry->d_name);

            if(file_mac(path, &entryMAC) == 0)
            {
                printf("M: %s | A: %s | C: %s | '%s'\n", entryMAC.m, entryMAC.a, entryMAC.c, entry->d_name);
            }
        }

        closedir(watched_dir);

        return 0;
    }

    return -1;
}
