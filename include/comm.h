#ifndef __COMM_H__
#define __COMM_H__

#include <netdb.h>

#define COMM_COMMAND_LENGTH 64
#define COMM_USERNAME_LENGTH 64
#define COMM_PARAMETER_LENGTH 128
#define COMM_MAX_CLIENT 128
#define COMM_TIMEOUT 20000

#define COMM_PPAYLOAD_LENGTH 256
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

struct comm_client {
    char username[COMM_USERNAME_LENGTH];
    int socket_instance;
    struct sockaddr_in *sockaddr;
    int port; // Port where we should receive the data from the client
    int valid;
};

int comm_init(int port);

#endif
