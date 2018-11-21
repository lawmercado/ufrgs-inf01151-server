#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "comm.h"
#include "log.h"
#include "utils.h"
#include "sync.h"
#include "repl.h"
#include "server.h"

struct comm_entity __server_entity;
int __counter_client_port = -1;
struct comm_client __clients[COMM_MAX_CLIENT];

pthread_mutex_t __client_handling_mutex = PTHREAD_MUTEX_INITIALIZER;

int __id = -1;

pthread_t pinger_thread;

int __client_create(struct sockaddr_in client_sockaddr, char username[COMM_MAX_CLIENT], int port, int receive_port);
void __client_remove(int port);

void *__pinger()
{
	while(1)
	{
		repl_send_ping();

		sleep(2);
	}
}

int __server_setup_as_primary()
{
    if(pthread_create(&pinger_thread, NULL, __pinger, NULL) != 0)
    {
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if(argc != 3)
    {
    	fprintf(stderr, "Usage: ./server port id\n");
    	exit(1);
  	}

	int port = atoi(argv[1]);
	__id = atoi(argv[2]);

	repl_init();
	repl_load_servers();

	log_info("server", "Am I primary? %d", repl_is_primary(__id));

    if(repl_is_primary(__id))
    {
        if(__server_setup_as_primary() == 0)
        {
            log_info("server", "Set up as primary!");
        }
    }

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

int __client_get_slot_by_receiver_port(int port)
{
    int i;

    for(i = 0; i < COMM_MAX_CLIENT; i++)
    {
        if(__clients[i].valid == 1 && __clients[i].receiver_port == port)
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
            if(repl_is_primary(__id))
            {
                repl_send_upload(client->username, parameter);
            }

			return __server_propagate_synchronization_to_related_clients(client, parameter, 1);
		}
    }
    else if(strcmp(operation, "delete") == 0)
    {
        if(comm_response_delete(client, parameter) == 0)
		{
            if(repl_is_primary(__id))
            {
                repl_send_delete(client->username, parameter);
            }

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

        if(repl_is_primary(__id))
        {
            repl_send_logout(client->username, client->receiver_port);
        }

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
        __clients[client_slot].backup = 0;
        
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

int __client_unknown_create(char username[COMM_MAX_CLIENT], int port, int receive_port)
{
    pthread_mutex_lock(&__client_handling_mutex);

    int client_slot = __client_get_empty_list_slot();

    if(client_slot != -1)
    {
        strncpy(__clients[client_slot].username, username, strlen(username));
        __clients[client_slot].valid = 1;
        __clients[client_slot].backup = 1;
        __clients[client_slot].port = port;
        __clients[client_slot].receiver_port = receive_port;


        pthread_mutex_unlock(&__client_handling_mutex);

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
        __clients[client_slot].backup = 0;
    }

    pthread_mutex_unlock(&__client_handling_mutex);
}

void __client_unknown_remove(int port)
{
    pthread_mutex_lock(&__client_handling_mutex);

    int client_slot = __client_get_slot_by_receiver_port(port);

    if(client_slot != -1)
    {
        __clients[client_slot].valid = 0;
        __clients[client_slot].backup = 0;
    }

    pthread_mutex_unlock(&__client_handling_mutex);
}

void __client_print_list()
{
    int i;

    for(i = 0; i < COMM_MAX_CLIENT; i++)
    {
        if(__clients[i].valid == 1 && __clients[i].backup == 0)
        {
            log_info("server","Client '%s', on port %d with socket %d", __clients[i].username, __clients[i].port, __clients[i].entity.socket_instance);
        }
        else if(__clients[i].backup == 1)
        {
            log_info("server","Backup client '%s', on port %d", __clients[i].username, __clients[i].port);
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
        __clients[i].backup = 0;
    }

    pthread_mutex_unlock(&__client_handling_mutex);
}

void __primary_server_command_handling()
{
    char receive_buffer[COMM_PPAYLOAD_LENGTH];
    struct comm_packet packet;

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

            if(repl_is_primary(__id))
            {
                sprintf(packet.payload, "%d", __counter_client_port);

                if(comm_send_data(&(__server_entity), &packet) != 0)
                {
                    __counter_client_port--;
                }
            }
            
            if(__client_create(__server_entity.sockaddr, username, __counter_client_port, port) < 0)
            {
                log_debug("server","Could not log user");
            }
            else
            {
                repl_send_login(username, port);
                repl_synchornize_dir(username);
            }

            __client_print_list();
        }
    }
}

void __backup_server_command_handling()
{
    char receive_buffer[COMM_PPAYLOAD_LENGTH];
    struct comm_entity primary_entity;

    primary_entity = __server_entity;

    if(comm_receive_command(&(primary_entity), receive_buffer) == 0)
    {
        char operation[COMM_COMMAND_LENGTH], username[COMM_USERNAME_LENGTH], parameter[COMM_PARAMETER_LENGTH];

        bzero(operation, COMM_COMMAND_LENGTH);
        bzero(username, COMM_USERNAME_LENGTH);

        sscanf(receive_buffer, "%s %s %s", operation, username, parameter);

        if(strcmp(operation, "login") == 0)
        {
            int port = atoi(parameter);

            __counter_client_port++;

            if(__client_unknown_create(username, __counter_client_port, port) < 0)
            {
                log_debug("server","Could not create client");
            }

            __client_print_list();

            char path[FILE_PATH_LENGTH];
            sync_get_user_dir_path("sync_dir", username, path, FILE_PATH_LENGTH);

            if(file_exists(path))
            {
                file_clear_dir(path);
            }
            else
            {
                file_create_dir(path);
            }
        }
        else if(strcmp(operation, "logout") == 0)
        {
            int port = atoi(parameter);

            log_info("server", "Backup client logout");
            
            __client_unknown_remove(port);

            __client_print_list();
        }
        else if(strcmp(operation, "upload") == 0)
        {
            char path[FILE_PATH_LENGTH];
            sync_get_user_file_path("sync_dir", username, parameter, path, FILE_PATH_LENGTH);

            if(comm_receive_file(&(primary_entity), path) == 0)
            {
                log_info("comm", "File upload backuped");
            }
        }
        else if(strcmp(operation, "delete") == 0)
        {
            char path[FILE_PATH_LENGTH];
            sync_get_user_file_path("sync_dir", username, parameter, path, FILE_PATH_LENGTH);

            if(file_delete(path) == 0)
            {
                log_info("comm", "File deletion beckuped");
            }
        }
        else if(strcmp(operation, "ping") == 0)
        {
            log_info("server", "Received primary server ping");
        }
        else if(strcmp(operation, "election") == 0)
        {
            log_info("server", "Received election message");
            repl_set_is_primary_down();
            repl_set_is_ongoing_election();

            int to = atoi(username);

            repl_send_answer(to);
        }
        else if(strcmp(operation, "answer") == 0)
        {
            log_info("server", "Received answer message");

            repl_set_answered();
        }
        else if(strcmp(operation, "coordinator") == 0)
        {
            int elected = atoi(username);

            log_info("server", "Received coordinator message! Elected %d", elected);

            repl_set_new_primary(elected);

            repl_set_not_answered(); // Further elections
            repl_set_is_not_ongoing_election();
        }
    }
    else
    {
        if(!repl_is_ongoing_election())
        {
            log_info("server", "I identified a primary server timeout");
            repl_set_is_primary_down();
            repl_start_election(__id);
        }
        else if(!repl_is_answered() && repl_is_ongoing_election())
        {
            log_info("server", "I did not receive any command.");
            log_info("server", "I WON THE ELECTIONS");

            if(repl_send_coordinator(__id) == 0)
            {
                repl_set_new_primary(__id);

                repl_set_is_not_ongoing_election();

                if(__server_setup_as_primary() == 0)
                {
                    log_info("server", "Set up as primary!");

                    // LEVANTAR CLIENTES
                }
            }
        }
        else
        {
            log_error("server", "I did not receive any coordinator!");
        }
    }
}

void __server_wait_connection()
{
    while(1)
    {
        if(repl_is_primary(__id))
        {
            log_info("server", "Server waiting commands...");

            __primary_server_command_handling();
        }
        else
        {
            log_info("server", "Server waiting replication...");

            __backup_server_command_handling();
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
	__server_wait_connection();

	return 0;
}
