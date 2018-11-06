#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <math.h>
#include "comm.h"
#include "log.h"
#include "file.h"
#include "sync.h"
#include <dirent.h>

int __socket_instance;
int __counter_client_port;
struct comm_client __clients[COMM_MAX_CLIENT];

pthread_mutex_t __client_handling_mutex = PTHREAD_MUTEX_INITIALIZER;

int __client_create(struct sockaddr_in client_sockaddr, char username[COMM_MAX_CLIENT], int port);
void __client_remove(int port);
int __client_propagate(struct comm_client *client, char *file, char *action, int self);

void __server_init_sockaddr(struct sockaddr_in *sockaddr, int port);
int __server_create_socket(struct sockaddr_in *server_sockaddr);

int __send_packet(int *socket_instance, struct sockaddr_in *client_sockaddr, struct comm_packet *packet);
int __receive_packet(int *socket_instance, struct sockaddr_in *client_sockaddr, struct comm_packet *packet);
int __send_ack(int *socket_instance, struct sockaddr_in *client_sockaddr);
int __receive_ack(int *socket_instance, struct sockaddr_in *client_sockaddr);
int __send_data(int *socket_instance, struct sockaddr_in *client_sockaddr, struct comm_packet *packet);
int __receive_data(int *socket_instance, struct sockaddr_in *client_sockaddr, struct comm_packet *packet);
int __send_command(int *socket_instance, struct sockaddr_in *client_sockaddr, char buffer[COMM_PPAYLOAD_LENGTH]);
int __receive_command(int *socket_instance, struct sockaddr_in *client_sockaddr, char buffer[COMM_PPAYLOAD_LENGTH]);
int __send_file(int *socket_instance, struct sockaddr_in *sockaddr, char path[FILE_PATH_LENGTH]);
int __receive_file(int *socket_instance, struct sockaddr_in *sockaddr, char path[FILE_PATH_LENGTH]);

int __command_response_download(struct comm_client *client, char *file)
{
    char path[COMM_PARAMETER_LENGTH];
    sync_get_user_file_path("sync_dir", client->username, file, path, COMM_PARAMETER_LENGTH);
    int result;

    if((result = __send_file(&(client->socket_instance), client->sockaddr, path)) == 0)
    {
        log_info("comm", "Socket %d: '%s' done downloading", client->socket_instance, client->username);

        return 0;
    }

    return result;
}

int __command_response_upload(struct comm_client *client, char *file)
{
    char path[COMM_PARAMETER_LENGTH];
    sync_get_user_file_path("sync_dir", client->username, file, path, COMM_PARAMETER_LENGTH);
    int result;

    if((result = __receive_file(&(client->socket_instance), client->sockaddr, path)) == 0)
    {
        log_info("comm", "Socket %d: '%s' done uploading", client->socket_instance, client->username);

        __client_propagate(client, file, "download", 1);

        return 0;
    }

    return -1;
}

int __command_response_delete(struct comm_client *client, char *file)
{
    char path[COMM_PARAMETER_LENGTH];
    sync_get_user_file_path("sync_dir", client->username, file, path, COMM_PARAMETER_LENGTH);

    if(file_delete(path) == 0)
    {
        log_info("comm", "Socket %d: '%s' deleted file", client->socket_instance, client->username);

        __client_propagate(client, file, "delete", 0);

        return 0;
    }

    return -1;
}

int __command_response_list_server(int *socket_instance, struct comm_client *client)
{
    int file_counter = 0;
    char path[COMM_PARAMETER_LENGTH];
    char path_write[COMM_PARAMETER_LENGTH];

    bzero(path, COMM_PARAMETER_LENGTH);
    bzero(path_write, COMM_PARAMETER_LENGTH);

    strcat(path,"sync_dir/");
    strcat(path, client->username);

    strcat(path_write, path);
    strcat(path_write,"/temp.txt");

    struct dirent *de;  // Pointer for directory entry

    // opendir() returns a pointer of DIR type.
    DIR *dr = opendir(path);

    if (dr == NULL)  // opendir returns NULL if couldn't open directory
    {
        log_error("comm", "Socket %d: Could not open current directory: %s", *socket_instance, path);
        return 0;
    }

    FILE *file = NULL;
    file = fopen(path_write, "wb");

    if(file == NULL)
    {
        log_error("comm", "Socket %d: Could not open the file in '%s'", *socket_instance, path);
        return -1;
    }

    struct file_status file_temp;

    while ((de = readdir(dr)) != NULL)
    {

        if(strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0 || strcmp(de->d_name, "temp.txt") == 0)
        {
            continue;
        }

        char file_path[FILE_NAME_LENGTH];

        bzero((void *)&(file_temp), sizeof(file_temp));
        bzero(file_path , FILE_NAME_LENGTH);

        strcat(file_path, path);
        strcat(file_path, "/");
        strcat(file_path, de->d_name);

        strcpy(file_temp.file_name, de->d_name);

        file_mac(file_path, &(file_temp.file_mac));

        //printf("M: %s | A: %s | C: %s | '%s'\n", file_temp.file_mac.m, file_temp.file_mac.a, file_temp.file_mac.c, file_temp.file_name);

        fwrite(&file_temp, sizeof(file_temp), 1, file);

        file_counter++;

    }

    if(file_counter == 0)
    {
        bzero((void *)&(file_temp), sizeof(file_temp));

        strcpy(file_temp.file_name, "DiretorioVazio");

        fwrite(&file_temp, sizeof(file_temp), 1, file);
    }

    fclose(file);
    
    if(__send_file(socket_instance, client->sockaddr, path_write) == 0)
    {
        log_info("comm", "Socket %d: '%s' done listing", *socket_instance, client->username);
        file_delete(path_write);
    }

    closedir(dr);

    return 0;
}

int __command_response_get_sync_dir_download_all(int *socket_instance, struct comm_client *client, char *path)
{
    int file_counter = 0;

    char path_write[COMM_PARAMETER_LENGTH];
    bzero(path_write, COMM_PARAMETER_LENGTH);

    strcat(path_write, path);
    strcat(path_write,"/temp.txt");

    struct dirent *de;  // Pointer for directory entry

    // opendir() returns a pointer of DIR type.
    DIR *dr = opendir(path);

    if (dr == NULL)  // opendir returns NULL if couldn't open directory
    {
        log_error("comm", "Socket %d: Could not open current directory: %s", *socket_instance, path);
        return 0;
    }

    FILE *file = NULL;
    file = fopen(path_write, "w");

    if(file == NULL)
    {
        log_error("comm", "Socket %d: Could not open the file in '%s'", *socket_instance, path);
        return -1;
    }

    while ((de = readdir(dr)) != NULL)
    {
        if(strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0 || strcmp(de->d_name, "temp.txt") == 0)
        {
            continue;
        }

        char file_name[FILE_NAME_LENGTH];
        bzero(file_name, FILE_NAME_LENGTH);

        strcpy(file_name, de->d_name);

        fprintf(file, "%s", file_name);
        fprintf(file, "\n");

        file_counter++;

    }

    if(file_counter == 0)
    {
        fprintf(file, "DiretorioVazio");
    }

    fclose(file);

    if(__send_file(socket_instance, client->sockaddr, path_write) == 0)
    {
        log_info("comm", "Socket %d: '%s' done listing files to download", *socket_instance, client->username);
        file_delete(path_write);
    }

    closedir(dr);

    return 0;
}

int __command_response_get_sync_dir(int *socket_instance, struct comm_client *client)
{
    char path[COMM_PARAMETER_LENGTH];
    char path_write[COMM_PARAMETER_LENGTH];

    bzero(path, COMM_PARAMETER_LENGTH);
    bzero(path_write, COMM_PARAMETER_LENGTH);

    strcat(path,"sync_dir");

    strcat(path_write,path);
    strcat(path_write,"/");
    strcat(path_write, client->username);

    log_debug("comm", "path: %s", path);

    struct dirent *de;  // Pointer for directory entry

    // opendir() returns a pointer of DIR type.
    DIR *dr = opendir(path);

    if (dr == NULL)  // opendir returns NULL if couldn't open directory
    {
        log_error("comm", "Could not open current directory: %s", path);
        return 0;
    }

    int created = 0;

    while ((de = readdir(dr)) != NULL)
    {
        if(strcmp(de->d_name, client->username) == 0)
        {
            created = 1;
        }
    }

    if(created)
    {
        log_debug("comm", "sync_dir_%s exists!", client->username);
    }
    else
    {
        log_debug("comm", "sync_dir_%s doesn't exists!", client->username);

        strcat(path, "/");
        strcat(path, client->username);

        file_create_dir(path);
    }

    __command_response_get_sync_dir_download_all(socket_instance, client, path_write);

    closedir(dr);

    return 0;
}

int __command_response_synchronize(struct comm_client *client)
{
    pthread_mutex_lock(&__client_handling_mutex);
    struct comm_packet packet;

    packet.type = COMM_PTYPE_DATA;
    bzero(packet.payload, COMM_PPAYLOAD_LENGTH);

    if(strlen(client->to_sync_file) > 0)
    {
        sprintf(packet.payload, "%s %s", client->to_sync_action, client->to_sync_file);
    }
    else
    {
        strcpy(packet.payload, "NoFile");
    }

    if(__send_data(&client->socket_instance, client->sockaddr, &packet) == 0)
    {
        if(strlen(client->to_sync_file) > 0)
        {
            bzero(client->to_sync_file, FILE_NAME_LENGTH);
            bzero(client->to_sync_action, COMM_COMMAND_LENGTH);
        }

        pthread_mutex_unlock(&__client_handling_mutex);

        return 0;
    }

    pthread_mutex_unlock(&__client_handling_mutex);

    return -1;
}

int __command_response_logout(struct comm_client *client)
{
    log_info("comm", "Client '%s' signed out", client->username);

    __client_remove(client->port);

    return 0;
}

int __client_handle_command(struct comm_client *client, char *command)
{
    char operation[COMM_COMMAND_LENGTH], parameter[COMM_PARAMETER_LENGTH];

    sscanf(command, "%s %[^\n\t]s", operation, parameter);

    log_debug("comm", "Command read '%s %s'", operation, parameter);

    if(strcmp(operation, "download") == 0)
    {
        return __command_response_download(client, parameter);
    }
    else if(strcmp(operation, "upload") == 0)
    {
        return __command_response_upload(client, parameter);
    }
    else if(strcmp(operation, "delete") == 0)
    {
        return __command_response_delete(client, parameter);
    }
    else if(strcmp(operation, "list_server") == 0)
    {
        return __command_response_list_server(&(client->socket_instance), client);
    }
    else if(strcmp(operation, "get_sync_dir") == 0)
    {
        return __command_response_get_sync_dir(&(client->socket_instance), client);
    }
    else if(strcmp(operation, "synchronize") == 0)
    {
        return __command_response_synchronize(client);
    }
    else if(strcmp(operation, "logout") == 0)
    {
        return __command_response_logout(client);
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

        if(__receive_command(&__clients[client_slot].socket_instance, __clients[client_slot].sockaddr, command) == 0)
        {
            __client_handle_command(&__clients[client_slot], command);
        }

    } while(__clients[client_slot].valid == 1);

    pthread_exit(0);
}

int __client_create(struct sockaddr_in client_sockaddr, char username[COMM_MAX_CLIENT], int port)
{
    pthread_mutex_lock(&__client_handling_mutex);

    int client_slot = __client_get_empty_list_slot();
    struct sockaddr_in server_sockaddr;

    __server_init_sockaddr(&server_sockaddr, port);

    if(client_slot != -1)
    {
        strncpy(__clients[client_slot].username, username, strlen(username));
        __clients[client_slot].socket_instance = __server_create_socket(&server_sockaddr);

        if(__clients[client_slot].socket_instance == -1)
        {
            log_error("comm", "Could not create socket instance");

            return -1;
        }

        __clients[client_slot].port = port;
        __clients[client_slot].sockaddr = &client_sockaddr;
        __clients[client_slot].valid = 1;

        bzero(__clients[client_slot].to_sync_file, FILE_NAME_LENGTH);

        pthread_mutex_unlock(&__client_handling_mutex);

        pthread_create(&__clients[client_slot].thread, NULL, __client_handler, (void *)&client_slot);

        return client_slot;
    }

    pthread_mutex_unlock(&__client_handling_mutex);

    return -1;
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

void __client_remove(int port)
{
    pthread_mutex_lock(&__client_handling_mutex);

    int client_slot = __client_get_slot_by_port(port);

    if(client_slot != -1)
    {
        close(__clients[client_slot].socket_instance);
        __clients[client_slot].valid = 0;
        __clients[client_slot].sockaddr = NULL;
    }

    pthread_mutex_unlock(&__client_handling_mutex);
}

int __client_propagate(struct comm_client *client, char *file, char *action, int self)
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

            log_info("comm", "Propagation of file %s, action %s", file, action);

            strcpy(__clients[i].to_sync_file, file);
            strcpy(__clients[i].to_sync_action, action);
        }
    }

    pthread_mutex_unlock(&__client_handling_mutex);

    return 0;
}

void __client_print_list()
{
    int i;

    for(i = 0; i < COMM_MAX_CLIENT; i++)
    {
        if(__clients[i].valid == 1)
        {
            log_info("comm", "Client '%s', on port %d with socket %d", __clients[i].username, __clients[i].port, __clients[i].socket_instance);
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

void __server_init_sockaddr(struct sockaddr_in *sockaddr, int port)
{
    sockaddr->sin_family = AF_INET;
	sockaddr->sin_port = htons(port);
	sockaddr->sin_addr.s_addr = INADDR_ANY;
	bzero((void *)&(sockaddr->sin_zero), sizeof(sockaddr->sin_zero));
}

int __server_create_socket(struct sockaddr_in *server_sockaddr)
{
    int socket_instance;

    if((socket_instance = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
		log_error("comm", "Could not create a socket instance");

        return -1;
	}

	if(bind(socket_instance, (struct sockaddr *)server_sockaddr, sizeof(struct sockaddr)) < 0)
    {
		log_error("comm", "Could not bind");

        close(socket_instance);
		return -1;
	}

    return socket_instance;
}

void __server_wait_connection()
{
    while(1)
    {
        char receive_buffer[COMM_PPAYLOAD_LENGTH];
        struct sockaddr_in client_sockaddr = {0};
        struct comm_packet packet;

        log_info("comm", "Server waiting connections...");

        if(__receive_command(&__socket_instance, &client_sockaddr, receive_buffer) == 0)
        {
            char operation[COMM_COMMAND_LENGTH], username[COMM_USERNAME_LENGTH];

            bzero(operation, COMM_COMMAND_LENGTH);
            bzero(username, COMM_USERNAME_LENGTH);
            bzero(packet.payload, COMM_PPAYLOAD_LENGTH);

            sscanf(receive_buffer, "%s %s", operation, username);

            if(strcmp(operation, "login") == 0)
            {
                __counter_client_port++;

                sprintf(packet.payload, "%d", __counter_client_port);

                if(__send_data(&__socket_instance, &client_sockaddr, &packet) != 0)
                {
                    __counter_client_port--;
                }

                if(__client_create(client_sockaddr, username, __counter_client_port) < 0)
                {
                    log_debug("comm", "Could not connect logged");
                }

                __client_print_list();
            }
        }
    }
}

int comm_init(int port)
{
    struct sockaddr_in sockaddr;

	__server_init_sockaddr(&sockaddr, port);

    __socket_instance = __server_create_socket(&sockaddr);

    log_info("comm", "Server is socket %d", __socket_instance);

    __counter_client_port = port;

    __server_wait_connection();

    return 0;
}

int __send_packet(int *socket_instance, struct sockaddr_in *client_sockaddr, struct comm_packet *packet)
{
    int status;

    status = sendto(*socket_instance, (void *)packet, sizeof(struct comm_packet), 0, (struct sockaddr *)client_sockaddr, sizeof(struct sockaddr));

    if(status < 0)
    {
        return -1;
    }

    return 0;
}

int __receive_packet(int *socket_instance, struct sockaddr_in *client_sockaddr, struct comm_packet *packet)
{
    int status;
    socklen_t from_length = sizeof(struct sockaddr_in);
    struct pollfd fd;
    int res;

    fd.fd = *socket_instance;
    fd.events = POLLIN;

    res = poll(&fd, 1, COMM_TIMEOUT);
    if(res == 0)
    {
        log_error("comm", "Socket: %d, Connection timed out", *socket_instance);

        return COMM_ERROR_TIMEOUT;
    }
    else if(res == -1)
    {
        log_error("comm", "Polling error");

        return -1;
    }
    else
    {
        status = recvfrom(*socket_instance, (void *)packet, sizeof(*packet), 0, (struct sockaddr *)client_sockaddr, &from_length);

        if(status < 0)
        {
            return -1;
        }

        return 0;
    }
}

int __send_ack(int *socket_instance, struct sockaddr_in *client_sockaddr)
{
    log_debug("comm", "Socket: %d, Sending ack!", client_sockaddr->sin_port);

    struct comm_packet packet;

    packet.type = COMM_PTYPE_ACK;
    bzero(packet.payload, COMM_PPAYLOAD_LENGTH);
    packet.seqn = 0;
    packet.length = 0;
    packet.total_size = 1;

    if(__send_packet(socket_instance, client_sockaddr, &packet) != 0)
    {
        log_error("comm", "Socket: %d, Ack could not be sent!", client_sockaddr->sin_port);

        return -1;
    }

    return 0;
}

int __receive_ack(int *socket_instance, struct sockaddr_in *client_sockaddr)
{
    log_debug("comm", "Socket: %d, Receiving ack!", client_sockaddr->sin_port);

    struct comm_packet packet;
    int status = __receive_packet(socket_instance, client_sockaddr, &packet);

    if(status == 0)
    {
        if(packet.type != COMM_PTYPE_ACK)
        {
            log_error("comm", "Socket: %d, The received packet is not an ack.", client_sockaddr->sin_port);

            return -1;
        }
    }

    return status;
}

int __send_data(int *socket_instance, struct sockaddr_in *client_sockaddr, struct comm_packet *packet)
{
    log_debug("comm", "Socket: %d, Sending data!", client_sockaddr->sin_port);

    packet->type = COMM_PTYPE_DATA;

    if(__send_packet(socket_instance, client_sockaddr, packet) != 0)
    {
        log_error("comm", "Socket: %d, Data could not be sent. Trying again!", client_sockaddr->sin_port);

        return -1;
    }

    int status = __receive_ack(socket_instance, client_sockaddr);

    while(status == COMM_ERROR_TIMEOUT)
    {
        log_error("comm", "Socket: %d, Data was not received by the destinatary. Trying again!", client_sockaddr->sin_port);
        __send_packet(socket_instance, client_sockaddr, packet);
        status = __receive_ack(socket_instance, client_sockaddr);
    }

    return status;
}

int __receive_data(int *socket_instance, struct sockaddr_in *client_sockaddr, struct comm_packet *packet)
{
    log_debug("comm", "Socket: %d, Receiving data!", client_sockaddr->sin_port);

    int status = __receive_packet(socket_instance, client_sockaddr, packet);
    int isValidPacket = packet->type == COMM_PTYPE_DATA;

    while(status == COMM_ERROR_TIMEOUT || !isValidPacket)
    {
        log_error("comm", "Socket: %d, Not received valid data packet. Trying again!", client_sockaddr->sin_port);

        status = __receive_packet(socket_instance, client_sockaddr, packet);

        if(status == 0)
        {
            if(packet->type == COMM_PTYPE_DATA)
            {
                isValidPacket = 1;
            }
            else
            {
                log_error("comm", "Socket: %d, The received packet is not data. Trying again!", client_sockaddr->sin_port);

                isValidPacket = 0;
            }
        }
        else if(status == -1)
        {
            return -1;
        }
    }

    return __send_ack(socket_instance, client_sockaddr);
}

int __send_command(int *socket_instance, struct sockaddr_in *client_sockaddr, char buffer[COMM_PPAYLOAD_LENGTH])
{
    log_debug("comm", "Socket: %d, Sending command!", *socket_instance);

    struct comm_packet packet;

    packet.type = COMM_PTYPE_CMD;
    packet.length = strlen(buffer);
    packet.seqn = 0;
    packet.total_size = 1;
    bzero(packet.payload, COMM_PPAYLOAD_LENGTH);
    strncpy(packet.payload, buffer, strlen(buffer));

    if(__send_packet(socket_instance, client_sockaddr, &packet) != 0)
    {
        log_error("comm", "Socket: %d, Command could not be sent. Trying again!", *socket_instance);

        return -1;
    }

    int status = __receive_ack(socket_instance, client_sockaddr);

    while(status == COMM_ERROR_TIMEOUT)
    {
        log_error("comm", "Socket: %d, Command was not received by the destinatary. Trying again!", *socket_instance);
        __send_packet(socket_instance, client_sockaddr, &packet);
        status = __receive_ack(socket_instance, client_sockaddr);
    }

    return status;
}

int __receive_command(int *socket_instance, struct sockaddr_in *client_sockaddr, char buffer[COMM_PPAYLOAD_LENGTH])
{
    log_debug("comm", "Socket: %d, Receiving command!", *socket_instance);

    struct comm_packet packet;

    int status = __receive_packet(socket_instance, client_sockaddr, &packet);
    int isValidPacket = packet.type == COMM_PTYPE_CMD;

    while(status == COMM_ERROR_TIMEOUT || !isValidPacket)
    {
        log_error("comm", "Socket: %d, Not received valid command packet. Trying again!", *socket_instance);

        status = __receive_packet(socket_instance, client_sockaddr, &packet);

        if(status == 0)
        {
            if(packet.type == COMM_PTYPE_CMD)
            {
                isValidPacket = 1;
            }
            else
            {
                log_error("comm", "Socket: %d, The received packet is not command. Trying again!", *socket_instance);

                isValidPacket = 0;
            }
        }
        else if(status == -1)
        {
            return -1;
        }
    }

    bzero(buffer, COMM_PPAYLOAD_LENGTH);
    strncpy(buffer, packet.payload, packet.length);

    return __send_ack(socket_instance, client_sockaddr);
}

int __send_file(int *socket_instance, struct sockaddr_in *sockaddr, char path[FILE_PATH_LENGTH])
{
    FILE *file = NULL;
    int i;

    file = fopen(path, "rb");

    if(file == NULL)
    {
        log_error("comm", "Socket: %d, Could not open the file in '%s'", *socket_instance, path);

        return -1;
    }

    int num_packets = (int) ceil(file_size(path) / (float) COMM_PPAYLOAD_LENGTH);

    struct comm_packet packet;

    for(i = 0; i < num_packets; i++)
    {
        int length = file_read_bytes(file, packet.payload, COMM_PPAYLOAD_LENGTH);

        packet.length = length;
        packet.total_size = num_packets;
        packet.seqn = i;

        __send_data(socket_instance, sockaddr, &packet);
    }

    fclose(file);
    return 0;
}

int __receive_file(int *socket_instance, struct sockaddr_in *sockaddr, char path[FILE_PATH_LENGTH])
{
    FILE *file = NULL;
    int i;
    struct comm_packet packet;

    file = fopen(path, "wb");

    if(file == NULL)
    {
        log_error("comm", "Socket: %d, Could not open the file in '%s'", *socket_instance, path);

        return -1;
    }

    if(__receive_data(socket_instance, sockaddr, &packet) == 0)
    {
        file_write_bytes(file, packet.payload, packet.length);

        for(i = 1; i < packet.total_size; i++)
        {
            __receive_data(socket_instance, sockaddr, &packet);
            file_write_bytes(file, packet.payload, packet.length);
        }

        fclose(file);

        return 0;
    }

    fclose(file);

    return -1;
}
