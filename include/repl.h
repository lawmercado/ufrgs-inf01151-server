#ifndef __REPL_H__
#define __REPL_H__

#include "comm.h"

#define REPL_MAX_SERVER 3

struct repl_server {
    int id;
    int primary;
    struct comm_entity entity;
};

int repl_init();

int repl_load_servers(struct repl_server servers[REPL_MAX_SERVER]);

int repl_send_clients(struct repl_server servers[REPL_MAX_SERVER], struct comm_client clients[COMM_MAX_CLIENT]);

int repl_receive_clients(struct comm_client clients[COMM_MAX_CLIENT]);

int repl_receive_file(struct comm_client client, char file[FILE_NAME_LENGTH]);

int repl_send_ping(struct repl_server servers[REPL_MAX_SERVER]);

int repl_receive_ping();

#endif
