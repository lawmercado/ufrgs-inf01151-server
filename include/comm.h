#ifndef COMMUNICATION_H_

#define COMMUNICATION_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


#define BUFFER_SIZE 256
#define MAXIDSIZE 25

typedef struct frame {
    int id_msg;
    int ack;
    char buffer[BUFFER_SIZE];
    char user[MAXIDSIZE];
} Frame;

typedef struct textMessage {
    int SenderId;
    int RecepientId;
    char buffer[BUFFER_SIZE];
} TextMessage;

void send_file();

void receive_file();

void delete_file();

void list_server();

int send_cmd();

int receive_cmd();

int send_state();

int receive_state();

int send_packet();

int receive_packet();

int open_server(char *hostname, int port);

int close_server(int sockfd);

int wait_connection(char *hostname, int port, int sockfd);


 

#endif