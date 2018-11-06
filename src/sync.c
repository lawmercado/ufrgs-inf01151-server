#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include "sync.h"
#include "file.h"
#include "log.h"

int sync_init(char *username, char* dir_path)
{
    char path[FILE_PATH_LENGTH];

    bzero(path, FILE_PATH_LENGTH);

    strcat(path, dir_path);
    strcat(path, "/");
    strcat(path, username);

    if(file_exists(path) != 0)
    {
        return file_create_dir(path);
    }

    return 0;
}

void sync_get_user_dir_path(char *dir_path, char *username, char *result_path, int length)
{
    bzero(result_path, length);

    strcat(result_path, dir_path);
    strcat(result_path, "/");
    strcat(result_path, username);
}

void sync_get_user_file_path(char *dir_path, char *username, char *file, char *result_path, int length)
{
    sync_get_user_dir_path(dir_path, username, result_path, length);

    strcat(result_path, "/");
    strcat(result_path, file);
}

int sync_list_files(char *username, char* dir_path)
{
    DIR *watched_dir;
    struct dirent *entry;
    struct file_mactimestamp entryMAC;
    char path[FILE_PATH_LENGTH];

    sync_get_user_dir_path(dir_path, username, path, FILE_PATH_LENGTH);

    watched_dir = opendir(path);

    if(watched_dir)
    {
        while((entry = readdir(watched_dir)) != NULL)
        {
            if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }

            sync_get_user_file_path(dir_path, username, entry->d_name, path, FILE_PATH_LENGTH);

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
