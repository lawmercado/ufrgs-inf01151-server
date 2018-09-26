#ifndef __SYNC_H__
#define __SYNC_H__

int sync_init(char* dir_path, char *username);

void sync_get_user_dir_path(char *dir_path, char *username, char *result_path);

void sync_get_user_file_path(char *dir_path, char *username, char *file, char *result_path);

int sync_list_files(char *username, char* dir_path);

#endif
