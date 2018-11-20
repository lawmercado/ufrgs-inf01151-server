#ifndef __COMM_H__
#define __COMM_H__

#include <pthread.h>
#include <netdb.h>
#include "file.h"

#define COMM_COMMAND_LENGTH 64
#define COMM_USERNAME_LENGTH 64
#define COMM_PARAMETER_LENGTH 128
#define COMM_MAX_CLIENT 128
#define COMM_RECEIVE_BUFFER_LENGTH 10

#define COMM_TIMEOUT 20000
#define COMM_MAX_TIMEOUTS 5
#define COMM_ERROR_TIMEOUT -2

#define COMM_PPAYLOAD_LENGTH 512
#define COMM_PTYPE_DATA 0
#define COMM_PTYPE_CMD 1
#define COMM_PTYPE_ACK 2

struct comm_packet {
    uint16_t type; // Packet type (COMM_PTYPE_*)
    uint16_t seqn; // Sequence number
    uint32_t total_size; // Number of fragments
    uint16_t length; // Payload length
    char payload[COMM_PPAYLOAD_LENGTH];
};

struct comm_entity {
    int socket_instance;
    struct sockaddr_in sockaddr;
    struct comm_packet buffer[COMM_RECEIVE_BUFFER_LENGTH];
    int idx_buffer;
};

struct comm_client {
    char username[COMM_USERNAME_LENGTH];
    pthread_t thread;
    int port;
    int receiver_port;
    int valid;
    struct comm_entity entity;
    struct comm_entity receiver_entity;
};

int comm_send_data(struct comm_entity *from, struct comm_packet *packet);

int comm_receive_data(struct comm_entity *from, struct comm_packet *packet);

int comm_send_command(struct comm_entity *from, char buffer[COMM_PPAYLOAD_LENGTH]);

int comm_receive_command(struct comm_entity *from, char buffer[COMM_PPAYLOAD_LENGTH]);

int comm_send_file(struct comm_entity *to, char path[FILE_PATH_LENGTH]);

int comm_receive_file(struct comm_entity *from, char path[FILE_PATH_LENGTH]);

int comm_init(struct comm_entity entity);

#endif
