#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "comm.h"
#include "log.h"
#include "utils.h"
#include "sync.h"
#include "server.h"

struct comm_entity __server_entity;
int __counter_client_port = -1;
struct comm_client __clients[COMM_MAX_CLIENT];

pthread_mutex_t __client_handling_mutex = PTHREAD_MUTEX_INITIALIZER;

int __client_create(struct sockaddr_in client_sockaddr, char username[COMM_MAX_CLIENT], int port, int receive_port);
void __client_remove(int port);

int main(int argc, char *argv[])
{
    if(argc != 2)
    {
    	fprintf(stderr, "Usage: ./server port\n");
    	exit(1);
  	}

	int port = atoi(argv[1]);

	server_setup(port);

	return 0;
}

int __client_get_slot_by_port(int port)
{
    int i;

    for(i = 0; i < COMM_MAX_CLIENT; i++)
    {
        if(__clients[i].valid == 1 && __clients[i].port == port)
        {
            return i;
        }
    }

    return -1;
}

int __server_propagate_synchronization_to_related_clients(struct comm_client *client, char *file, int self)
{
    pthread_mutex_lock(&__client_handling_mutex);

    int i;

    for(i = 0; i < COMM_MAX_CLIENT; i++)
    {
        if(strcmp(__clients[i].username, client->username) == 0)
        {
            if(!self && __client_get_slot_by_port(client->port) == i)
            {
                continue;
            }

            char command[COMM_PPAYLOAD_LENGTH] = "";
            char path[FILE_PATH_LENGTH] = "";

            sprintf(command, "synchronize %s", file);

            sync_get_user_file_path("sync_dir", client->username, file, path, FILE_PATH_LENGTH);

            if(comm_send_command(&(__clients[i].receiver_entity), command) == 0)
            {
                if(comm_send_file(&(__clients[i].receiver_entity), path) == 0)
                {
                    log_info("server","Client propagated synchronization");
				}
                else
				{
					pthread_mutex_unlock(&__client_handling_mutex);

					return -1;			
				}
            }
			else
			{
				pthread_mutex_unlock(&__client_handling_mutex);

				return -1;			
			}
        }
    }

    pthread_mutex_unlock(&__client_handling_mutex);

    return 0;
}

int __server_propagate_deletion_to_related_clients(struct comm_client *client, char *file, int self)
{
    pthread_mutex_lock(&__client_handling_mutex);

    int i;

    for(i = 0; i < COMM_MAX_CLIENT; i++)
    {
        if(strcmp(__clients[i].username, client->username) == 0)
        {
            if(!self && __client_get_slot_by_port(client->port) == i)
            {
                continue;
            }

            char command[COMM_PPAYLOAD_LENGTH] = "";

            sprintf(command, "delete %s", file);
            
            if(comm_send_command(&(__clients[i].receiver_entity), command) == 0)
            {
                log_info("server", "Client propagated deletion");
            }
			else
			{
				pthread_mutex_unlock(&__client_handling_mutex);

				return -1;			
			}
        }
    }

    pthread_mutex_unlock(&__client_handling_mutex);

    return 0;
}

int __client_handle_command(struct comm_client *client, char *command)
{
    char operation[COMM_COMMAND_LENGTH], parameter[COMM_PARAMETER_LENGTH];

    sscanf(command, "%s %[^\n\t]s", operation, parameter);

    log_debug("server","Command read '%s %s'", operation, parameter);

    if(strcmp(operation, "download") == 0)
    {
        return comm_response_download(client, parameter);
    }
    else if(strcmp(operation, "upload") == 0)
    {
        if(comm_response_upload(client, parameter) == 0)
		{
			return __server_propagate_synchronization_to_related_clients(client, parameter, 1);
		}
    }
    else if(strcmp(operation, "delete") == 0)
    {
        if(comm_response_delete(client, parameter) == 0)
		{
			return __server_propagate_deletion_to_related_clients(client, parameter, 0);
		}
    }
    else if(strcmp(operation, "list_server") == 0)
    {
        return comm_response_list_server(client);
    }
    else if(strcmp(operation, "get_sync_dir") == 0)
    {
        return comm_response_get_sync_dir(client);
    }
    else if(strcmp(operation, "logout") == 0)
    {
        log_info("comm", "Socket %d: '%s' signed out", client->entity.socket_instance, client->username);

    	__client_remove(client->port);
    }

    return 0;
}

int __client_get_empty_list_slot()
{
    int i;

    for(i = 0; i < COMM_MAX_CLIENT; i++)
    {
        if(__clients[i].valid == 0)
        {
            return i;
        }
    }

    return -1;
}

void *__client_handler(void *arg)
{
    int client_slot = *((int *) arg);

    do
    {
        char command[COMM_PPAYLOAD_LENGTH];
        bzero(command, COMM_PPAYLOAD_LENGTH);

        if(comm_receive_command(&(__clients[client_slot].entity), command) == 0)
        {
            __client_handle_command(&__clients[client_slot], command);
        }

    } while(__clients[client_slot].valid == 1);

    pthread_exit(0);
}

int __client_create(struct sockaddr_in client_sockaddr, char username[COMM_MAX_CLIENT], int port, int receive_port)
{
    pthread_mutex_lock(&__client_handling_mutex);

    int client_slot = __client_get_empty_list_slot();
    struct sockaddr_in server_sockaddr;

    utils_init_sockaddr(&server_sockaddr, port, INADDR_ANY);

    if(client_slot != -1)
    {
        strncpy(__clients[client_slot].username, username, strlen(username));
        __clients[client_slot].valid = 1;
        
        __clients[client_slot].entity.socket_instance = utils_create_binded_socket(&server_sockaddr);
        if(__clients[client_slot].entity.socket_instance == -1)
        {
            log_error("server","Could not create socket instance");

            return -1;
        }

        __clients[client_slot].port = port;
        __clients[client_slot].entity.sockaddr = client_sockaddr;
        __clients[client_slot].entity.idx_buffer = -1;

        __clients[client_slot].receiver_entity.socket_instance = utils_create_socket();
        if(__clients[client_slot].receiver_entity.socket_instance == -1)
        {
            log_error("server","Could not create receiver socket instance");

            return -1;
        }

        __clients[client_slot].receiver_port = receive_port;
        __clients[client_slot].receiver_entity.sockaddr = client_sockaddr;
        utils_init_sockaddr(&(__clients[client_slot].receiver_entity.sockaddr), receive_port, client_sockaddr.sin_addr.s_addr);
        __clients[client_slot].receiver_entity.idx_buffer = -1;

        pthread_mutex_unlock(&__client_handling_mutex);

        pthread_create(&__clients[client_slot].thread, NULL, __client_handler, (void *)&client_slot);

        return client_slot;
    }

    pthread_mutex_unlock(&__client_handling_mutex);

    return -1;
}

void __client_remove(int port)
{
    pthread_mutex_lock(&__client_handling_mutex);

    int client_slot = __client_get_slot_by_port(port);

    if(client_slot != -1)
    {
        close(__clients[client_slot].entity.socket_instance);
        __clients[client_slot].valid = 0;
    }

    pthread_mutex_unlock(&__client_handling_mutex);
}

void __client_print_list()
{
    int i;

    for(i = 0; i < COMM_MAX_CLIENT; i++)
    {
        if(__clients[i].valid == 1)
        {
            log_info("server","Client '%s', on port %d with socket %d", __clients[i].username, __clients[i].port, __clients[i].entity.socket_instance);
        }
    }
}

void __client_setup_list()
{
    pthread_mutex_lock(&__client_handling_mutex);

    int i;

    for(i = 0; i < COMM_MAX_CLIENT; i++)
    {
        __clients[i].valid = 0;
    }

    pthread_mutex_unlock(&__client_handling_mutex);
}

void __server_wait_connection()
{
    while(1)
    {
        char receive_buffer[COMM_PPAYLOAD_LENGTH];
        struct comm_packet packet;

        log_info("server","Server waiting connections...");

        if(comm_receive_command(&(__server_entity), receive_buffer) == 0)
        {
            char operation[COMM_COMMAND_LENGTH], username[COMM_USERNAME_LENGTH];
            int port;

            bzero(operation, COMM_COMMAND_LENGTH);
            bzero(username, COMM_USERNAME_LENGTH);
            bzero(packet.payload, COMM_PPAYLOAD_LENGTH);

            sscanf(receive_buffer, "%s %s %d", operation, username, &port);

            if(strcmp(operation, "login") == 0)
            {
                __counter_client_port++;

                sprintf(packet.payload, "%d", __counter_client_port);

                if(comm_send_data(&(__server_entity), &packet) != 0)
                {
                    __counter_client_port--;
                }
                
                if(__client_create(__server_entity.sockaddr, username, __counter_client_port, port) < 0)
                {
                    log_debug("server","Could not connect logged");
                }

                __client_print_list();
            }
        }
    }
}

int server_setup(int port)
{
	utils_init_sockaddr(&(__server_entity.sockaddr), port, INADDR_ANY);

    __server_entity.socket_instance = utils_create_binded_socket(&(__server_entity.sockaddr));
    __server_entity.idx_buffer = -1;

	if(__server_entity.socket_instance == -1)
	{
		return -1;
	}

    log_info("server","Server is socket %d", __server_entity.socket_instance);

	__counter_client_port = port;

	comm_init(__server_entity);

	__server_wait_connection();

	return 0;
}
