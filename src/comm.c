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
#include <dirent.h>
#include "comm.h"
#include "log.h"
#include "file.h"
#include "sync.h"
#include "utils.h"

int __counter_client_port;
struct comm_entity __server_entity;
struct comm_client __clients[COMM_MAX_CLIENT];

pthread_mutex_t __client_handling_mutex = PTHREAD_MUTEX_INITIALIZER;

int __client_create(struct sockaddr_in client_sockaddr, char username[COMM_MAX_CLIENT], int port, int receive_port);
void __client_remove(int port);
int __client_propagate_synchronization(struct comm_client *client, char *file, int self);
int __client_propagate_deletion(struct comm_client *client, char *file, int self);

int __command_response_download(struct comm_client *client, char *file)
{
    char path[COMM_PARAMETER_LENGTH];
    sync_get_user_file_path("sync_dir", client->username, file, path, COMM_PARAMETER_LENGTH);
    int result;

    if((result = comm_send_file(&(client->entity), path)) == 0)
    {
        log_info("comm", "Socket %d: '%s' done downloading", client->entity.socket_instance, client->username);

        return 0;
    }

    return result;
}

int __command_response_upload(struct comm_client *client, char *file)
{
    char path[COMM_PARAMETER_LENGTH];
    sync_get_user_file_path("sync_dir", client->username, file, path, COMM_PARAMETER_LENGTH);
    int result;

    if((result = comm_receive_file(&(client->entity), path)) == 0)
    {
        log_info("comm", "Socket %d: '%s' done uploading", client->entity.socket_instance, client->username);

        __client_propagate_synchronization(client, file, 1);

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
        log_info("comm", "Socket %d: '%s' deleted file", client->entity.socket_instance, client->username);

        __client_propagate_deletion(client, file, 0);

        return 0;
    }

    return -1;
}

int __command_response_list_server(struct comm_client *client)
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
        log_error("comm", "Socket %d: Could not open current directory: %s", client->entity.socket_instance, path);
        return 0;
    }

    FILE *file = NULL;
    file = fopen(path_write, "wb");

    if(file == NULL)
    {
        log_error("comm", "Socket %d: Could not open the file in '%s'", client->entity.socket_instance, path);
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
    
    if(comm_send_file(&(client->entity), path_write) == 0)
    {
        log_info("comm", "Socket %d: '%s' done listing", client->entity.socket_instance, client->username);
        file_delete(path_write);
    }

    closedir(dr);

    return 0;
}

int __command_response_get_sync_dir_download_all(struct comm_client *client, char *path)
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
        log_error("comm", "Socket %d: Could not open current directory: %s", client->entity.socket_instance, path);
        return 0;
    }

    FILE *file = NULL;
    file = fopen(path_write, "w");

    if(file == NULL)
    {
        log_error("comm", "Socket %d: Could not open the file in '%s'", client->entity.socket_instance, path);
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

    if(comm_send_file(&(client->entity), path_write) == 0)
    {
        log_info("comm", "Socket %d: '%s' done listing files to download", client->entity.socket_instance, client->username);
        file_delete(path_write);
    }

    closedir(dr);

    return 0;
}

int __command_response_get_sync_dir(struct comm_client *client)
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

    __command_response_get_sync_dir_download_all(client, path_write);

    closedir(dr);

    return 0;
}

int __command_response_logout(struct comm_client *client)
{
    log_info("comm", "Socket %d: '%s' signed out", client->entity.socket_instance, client->username);

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
        return __command_response_list_server(client);
    }
    else if(strcmp(operation, "get_sync_dir") == 0)
    {
        return __command_response_get_sync_dir(client);
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
            log_error("comm", "Could not create socket instance");

            return -1;
        }

        __clients[client_slot].port = port;
        __clients[client_slot].entity.sockaddr = client_sockaddr;
        __clients[client_slot].entity.idx_buffer = -1;

        __clients[client_slot].receiver_entity.socket_instance = utils_create_socket();
        if(__clients[client_slot].receiver_entity.socket_instance == -1)
        {
            log_error("comm", "Could not create receiver socket instance");

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
        close(__clients[client_slot].entity.socket_instance);
        __clients[client_slot].valid = 0;
    }

    pthread_mutex_unlock(&__client_handling_mutex);
}

int __client_propagate_synchronization(struct comm_client *client, char *file, int self)
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

            if(comm_send_command(&(client->receiver_entity), command) == 0)
            {
                if(comm_send_file(&(client->receiver_entity), path) == 0)
                {
                    log_info("comm", "Client propagated synchronization");
                }
            }
        }
    }

    pthread_mutex_unlock(&__client_handling_mutex);

    return 0;
}

int __client_propagate_deletion(struct comm_client *client, char *file, int self)
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
            
            if(comm_send_command(&(client->receiver_entity), command) == 0)
            {
                log_info("comm", "Client propagated deletion");
            }
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
            log_info("comm", "Client '%s', on port %d with socket %d", __clients[i].username, __clients[i].port, __clients[i].entity.socket_instance);
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

        log_info("comm", "Server waiting connections...");

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
                    log_debug("comm", "Could not connect logged");
                }

                __client_print_list();
            }
        }
    }
}

int comm_init(struct comm_entity entity)
{
    __server_entity = entity;
    __counter_client_port = utils_get_port((struct sockaddr *)&(entity.sockaddr));

    __server_wait_connection();

    return 0;
}

int __send_packet(struct comm_entity *to, struct comm_packet *packet)
{
    int status = sendto(to->socket_instance, (void *)packet, sizeof(struct comm_packet), 0, (struct sockaddr *)&(to->sockaddr), sizeof(struct sockaddr));

    if(status < 0)
    {
        log_error("comm", "Socket: %d, No packet could be sent", to->socket_instance);

        return -1;
    }

    return 0;
}

int __receive_packet(struct comm_entity *to, struct comm_packet *packet)
{
    struct pollfd fd;
    fd.fd = to->socket_instance;
    fd.events = POLLIN;

    int poll_status = poll(&fd, 1, COMM_TIMEOUT);

    if(poll_status == 0)
    {
        log_debug("comm", "Socket: %d, Connection timed out", to->socket_instance);

        return COMM_ERROR_TIMEOUT;
    }
    else if(poll_status == -1)
    {
        log_error("comm", "Socket: %d, Polling error");

        return -1;
    }
    else
    {
        socklen_t from_length = sizeof(to->sockaddr);
        int status = recvfrom(to->socket_instance, (void *)packet, sizeof(*packet), 0, (struct sockaddr *)&(to->sockaddr), &from_length);

        if(status < 0)
        {
            log_error("comm", "Socket: %d, No packet was received");

            return -1;
        }

        return 0;
    }
}

int __send_ack(struct comm_entity *to)
{
    log_debug("comm", "Socket: %d, Sending ack!", to->socket_instance);

    struct comm_packet packet;

    packet.type = COMM_PTYPE_ACK;
    bzero(packet.payload, COMM_PPAYLOAD_LENGTH);
    packet.seqn = 0;
    packet.length = 0;
    packet.total_size = 1;

    if(__send_packet(to, &packet) != 0)
    {
        log_error("comm", "Socket: %d, Ack could not be sent!", to->socket_instance);

        return -1;
    }

    return 0;
}

int __receive_ack(struct comm_entity *from)
{
    log_debug("comm", "Socket: %d, Receiving ack!", from->socket_instance);

    struct comm_packet packet;
    int status = __receive_packet(from, &packet);

    if(status == 0)
    {
        if(packet.type != COMM_PTYPE_ACK)
        {
            log_error("comm", "Socket: %d, The received packet is not an ack.", from->socket_instance);

            return -1;
        }
    }

    return status;
}

int __is_packet_already_in_buffer(struct comm_entity *entity, struct comm_packet *packet)
{
    struct comm_packet *last_packet = &(entity->buffer[entity->idx_buffer]);

    return
    (
        (last_packet->type == packet->type) &&
        (last_packet->seqn == packet->seqn) &&
        (last_packet->length == packet->length) &&
        (last_packet->total_size == packet->total_size) &&
        (strcmp(last_packet->payload, packet->payload) == 0)
    );
}

int __save_packet_in_buffer(struct comm_entity *entity, struct comm_packet *packet)
{
    if(entity->idx_buffer == (COMM_RECEIVE_BUFFER_LENGTH - 1))
    {
        log_error("comm", "Socket %d, buffer is full!", entity->socket_instance);

        return -1;
    }
    
    entity->idx_buffer = entity->idx_buffer + 1;
    entity->buffer[entity->idx_buffer] = *packet;

    return 0;
 }

int __get_packet_in_buffer(struct comm_entity *entity, struct comm_packet *packet)
{
    if(entity->idx_buffer == -1)
    {
        log_error("comm", "Socket %d, buffer is empty!", entity->socket_instance);

        return -1;
    }
    
    *packet = entity->buffer[entity->idx_buffer];
    entity->idx_buffer = entity->idx_buffer - 1;

    return 0;
}

int __reliable_send_packet(struct comm_entity *to, struct comm_packet *packet)
{
    int status = -1;
    int ack_status = -1;

    do
    {
        status = __send_packet(to, packet);

        if(status != 0)
        {
            return -1;
        }

        ack_status = __receive_ack(to);

    } while(ack_status == COMM_ERROR_TIMEOUT);

    return ack_status;
}

int __reliable_receive_packet(struct comm_entity *from)
{
    struct comm_packet packet;
    int status = -1;
    int timeout_count = 0;

    do
    {
        status = __receive_packet(from, &packet);

        if(status == COMM_ERROR_TIMEOUT)
        {
            timeout_count++;

            if(timeout_count == COMM_MAX_TIMEOUTS)
            {
                return -1;
            }
        }
    } while(status == COMM_ERROR_TIMEOUT);

    if(!__is_packet_already_in_buffer(from, &packet))
    {
        if(__save_packet_in_buffer(from, &packet) == 0)
        {
            if(__send_ack(from) == 0)
            {
                return 0;
            }
        }
    }
    else
    {
        if(__send_ack(from) == 0)
        {
            return 0;
        }
    }

    return -1;
}

int comm_send_data(struct comm_entity *to, struct comm_packet *packet)
{
    log_debug("comm", "Socket: %d, Sending data!", to->socket_instance);

    packet->type = COMM_PTYPE_DATA;

    if(__reliable_send_packet(to, packet) != 0)
    {
        log_error("comm", "Socket: %d, Data could not be sent.", to->socket_instance);

        return -1;
    }

    return 0;
}

int comm_receive_data(struct comm_entity *from, struct comm_packet *packet)
{
    log_debug("comm", "Socket: %d, Receiving data!", from->socket_instance);

    if(__reliable_receive_packet(from) != 0)
    {
        log_error("comm", "Socket: %d, No data could be received.", from->socket_instance);

        return -1;
    }

    if(__get_packet_in_buffer(from, packet) != 0)
    {
        return -1;
    }

    if(packet->type != COMM_PTYPE_DATA)
    {
        log_error("comm", "Socket: %d, The received packet is not data.", from->socket_instance);

        return -1;
    }

    return 0;
}

int comm_send_command(struct comm_entity *to, char buffer[COMM_PPAYLOAD_LENGTH])
{
    log_debug("comm", "Socket: %d, Sending command!", to->socket_instance);

    struct comm_packet packet;

    packet.type = COMM_PTYPE_CMD;
    packet.length = strlen(buffer);
    packet.seqn = 0;
    packet.total_size = 1;
    bzero(packet.payload, COMM_PPAYLOAD_LENGTH);
    strncpy(packet.payload, buffer, strlen(buffer));

    if(__reliable_send_packet(to, &packet) != 0)
    {
        log_error("comm", "Socket: %d, Command could not be sent.", to->socket_instance);

        return -1;
    }

    return 0;
}

int comm_receive_command(struct comm_entity *from, char buffer[COMM_PPAYLOAD_LENGTH])
{
    log_debug("comm", "Socket: %d, Receiving command!", from->socket_instance);

    struct comm_packet packet;

    if(__reliable_receive_packet(from) != 0)
    {
        log_error("comm", "Socket: %d, No command could be received.", from->socket_instance);

        return -1;
    }

    if(__get_packet_in_buffer(from, &packet) != 0)
    {
        return -1;
    }

    if(packet.type != COMM_PTYPE_CMD)
    {
        log_error("comm", "Socket: %d, The received packet is not command.", from->socket_instance);

        return -1;
    }

    bzero(buffer, COMM_PPAYLOAD_LENGTH);
    strncpy(buffer, packet.payload, packet.length);

    return 0;
}

int comm_send_file(struct comm_entity *to, char path[FILE_PATH_LENGTH])
{
    FILE *file = NULL;
    int i;

    file = fopen(path, "rb");

    if(file == NULL)
    {
        log_error("comm", "Socket: %d, Could not open the file in '%s'", to->socket_instance, path);

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

        comm_send_data(to, &packet);
    }

    fclose(file);
    return 0;
}

int comm_receive_file(struct comm_entity *from, char path[FILE_PATH_LENGTH])
{
    FILE *file = NULL;
    int i;
    struct comm_packet packet;

    file = fopen(path, "wb");

    if(file == NULL)
    {
        log_error("comm", "Socket: %d, Could not open the file in '%s'", from->socket_instance, path);

        return -1;
    }

    if(comm_receive_data(from, &packet) == 0)
    {
        file_write_bytes(file, packet.payload, packet.length);

        for(i = 1; i < packet.total_size; i++)
        {
            comm_receive_data(from, &packet);
            file_write_bytes(file, packet.payload, packet.length);
        }

        fclose(file);

        return 0;
    }

    fclose(file);

    return -1;
}