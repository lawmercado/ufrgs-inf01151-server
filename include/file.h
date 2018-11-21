#ifndef __FILE_H__
#define __FILE_H__

#define FILE_NAME_LENGTH 255
#define FILE_PATH_LENGTH 4096
#define FILE_TIMESTAMP_LENGTH 20

struct file_mactimestamp {
    char m[FILE_TIMESTAMP_LENGTH];
    char a[FILE_TIMESTAMP_LENGTH];
    char c[FILE_TIMESTAMP_LENGTH];
};

struct file_status {
    char file_name[FILE_NAME_LENGTH];
    struct file_mactimestamp file_mac;
};

int file_write_buffer(char path[FILE_PATH_LENGTH], char *buffer, int length);

int file_mac(char path[FILE_PATH_LENGTH], struct file_mactimestamp *mac);

int file_size(char path[FILE_PATH_LENGTH]);

int file_delete(char path[FILE_PATH_LENGTH]);

int file_get_name_from_path(char *path, char *filename);

int file_exists(char path[FILE_PATH_LENGTH]);

int file_create_dir(char path[FILE_PATH_LENGTH]);

int file_clear_dir(char *path);

int file_read_bytes(FILE *file, char *buffer, int length);

int file_write_bytes(FILE *file, char *buffer, int length);

int file_path(char* dir, char* file, char *dest, int length);

#endif
