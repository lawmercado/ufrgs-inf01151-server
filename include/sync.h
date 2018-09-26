#ifndef __SYNC_H__
#define __SYNC_H__

/**
 * Initializes the user directory
 *
 * @param char* username The username
 * @return 0 if no errors, -1 otherwise
 */
int sync_init(char *username);

int sync_get_user_dir_path(char *username, char *path);

/**
 * Updates the file in the synchronized directory
 *
 * @param char* name The name of the file
 * @param char* buffer The content of the file
 * @param int length The buffer size of the file
 * @return 0 if no errors, -1 otherwise
 */
int sync_update_file(char *name, char *buffer, int length);

/**
 * List the content of the sync directory
 *
 * @return 0 if no errors, -1 otherwise
 */
int sync_list_files();

#endif
