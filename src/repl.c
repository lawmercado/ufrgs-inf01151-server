#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "log.h"
#include "repl.h"
#include "sync.h"
#include "utils.h"

struct repl_server servers[REPL_MAX_SERVER];

pthread_mutex_t __repl_handling_mutex = PTHREAD_MUTEX_INITIALIZER;

int repl_init()
{
    int i;

    for(i = 0; i < REPL_MAX_SERVER; i++)
    {
        servers[i].id = -1;
        servers[i].primary = -1;
    }

    return 0;
}

int repl_load_servers()
{
    FILE *f;
    int idx = 0;

    f = fopen("known_servers.txt", "r");

    if(f == NULL)
    {
        log_error("repl", "Could not fin the known replication servers file");

        return -1;
    }

    while(!feof(f) && idx < REPL_MAX_SERVER)
    {
        int id = -1, port = -1, type = -1;
        char host[50];

        fscanf(f, "%d %s %d %d", &id, host, &port, &type);

        log_info("repl", "Known server: %d %s %d %d", id, host, port, type);

        servers[idx].id = id;
        servers[idx].primary = type == 1;
        servers[idx].entity.idx_buffer = -1;
        servers[idx].entity.socket_instance = utils_create_socket();
        utils_init_sockaddr_to_host(&(servers[idx].entity.sockaddr), port, host);

        idx++;
    }

    fclose(f);

    return 0;
}

int repl_is_primary(int id)
{
    int i = 0;

	for(i = 0; i < REPL_MAX_SERVER && servers[i].id != -1; i++)
	{
		if(id == servers[i].id && servers[i].primary)
		{
            return 1;
		}
	}

    return 0;
}

int repl_send_ping()
{
    pthread_mutex_lock(&__repl_handling_mutex);
    int i;
    int status = -1;

    for(i = 0; i < REPL_MAX_SERVER && servers[i].id != -1; i++)
    {
        if(servers[i].primary)
        {
            continue;
        }

        log_info("repl", "Sending ping to backup server %d", servers[i].id);

        char command[COMM_PPAYLOAD_LENGTH] = "";

        sprintf(command, "ping");

        if(comm_send_command(&(servers[i].entity), command) != 0)
        {
            status = servers[i].id;

            pthread_mutex_unlock(&__repl_handling_mutex);
            return status;
        }
    }

    pthread_mutex_unlock(&__repl_handling_mutex);
    return 0;
}

int repl_send_login(char *username, int port)
{
    pthread_mutex_lock(&__repl_handling_mutex);
    int i;

    char command[COMM_PPAYLOAD_LENGTH];

    sprintf(command, "login %s %d", username, port);

    for(i = 0; i < REPL_MAX_SERVER && servers[i].id != -1; i++)
    {
        if(servers[i].primary)
        {
            continue;
        }

        log_info("repl", "Sending command '%s' to backup server %d", command, servers[i].id);

        if(comm_send_command(&(servers[i].entity), command) == 0)
        {
            log_info("repl", "Command sent");
        }
    }

    pthread_mutex_unlock(&__repl_handling_mutex);
    return 0;
}

int repl_send_logout(char *username, int port)
{
    pthread_mutex_lock(&__repl_handling_mutex);
    int i;

    char command[COMM_PPAYLOAD_LENGTH];

    sprintf(command, "logout %s %d", username, port);

    for(i = 0; i < REPL_MAX_SERVER && servers[i].id != -1; i++)
    {
        if(servers[i].primary)
        {
            continue;
        }

        log_info("repl", "Sending command '%s' to backup server %d", command, servers[i].id);

        if(comm_send_command(&(servers[i].entity), command) == 0)
        {
            log_info("repl", "Command sent");
        }
    }

    pthread_mutex_unlock(&__repl_handling_mutex);
    return 0;
}

int repl_synchornize_dir(char *username)
{
    DIR *watched_dir;
    struct dirent *entry;
    char path[FILE_PATH_LENGTH];

    sync_get_user_dir_path("sync_dir", username, path, FILE_PATH_LENGTH);
    
    watched_dir = opendir(path);

    if(watched_dir)
    {
        while((entry = readdir(watched_dir)) != NULL)
        {
            if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }

            if(repl_send_upload(username, entry->d_name) != 0)
            {
                log_error("repl", "Error while synchronizing dir");
            }
        }

        closedir(watched_dir);
    }
    else
    {
        return -1;
    }

    return 0;
}

int repl_send_upload(char *username, char *file)
{
    pthread_mutex_lock(&__repl_handling_mutex);
    int i;

    for(i = 0; i < REPL_MAX_SERVER && servers[i].id != -1; i++)
    {
        if(servers[i].primary)
        {
            continue;
        }

        log_info("repl", "Sending upload to backup server %d", servers[i].id);

        if(comm_upload(&(servers[i].entity), username, file) == 0)
        {
            log_info("repl", "Uploaded sucessful");
        }
    }

    pthread_mutex_unlock(&__repl_handling_mutex);
    return 0;
}

int repl_send_delete(char *username, char *file)
{
    pthread_mutex_lock(&__repl_handling_mutex);
    int i;

    for(i = 0; i < REPL_MAX_SERVER && servers[i].id != -1; i++)
    {
        if(servers[i].primary)
        {
            continue;
        }

        log_info("repl", "Sending delete to backup server %d", servers[i].id);

        if(comm_delete(&(servers[i].entity), username, file) == 0)
        {
            log_info("repl", "Deletion sucessful");
        }
    }

    pthread_mutex_unlock(&__repl_handling_mutex);
    return 0;
}