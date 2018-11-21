#ifndef __REPL_H__
#define __REPL_H__

#include "comm.h"

#define REPL_MAX_SERVER 3

struct repl_server {
    int id;
    int primary;
    int down;
    struct comm_entity entity;
    struct comm_entity replication_entity;
};

int repl_init();

int repl_load_servers();

int repl_is_primary(int id);

int repl_set_is_primary_down();

int repl_send_ping();

int repl_send_login(char *username, int port);

int repl_send_logout(char *username, int port);

int repl_synchornize_dir(char *username);

int repl_send_upload(char *username, char *file);

int repl_send_delete(char *username, char *file);

int repl_start_election(int id);

int repl_set_is_ongoing_election();

int repl_set_is_not_ongoing_election();

int repl_send_answer(int to);

int repl_send_coordinator(int elected);

int repl_set_new_primary(int elected);

int repl_set_answered();

int repl_set_not_answered();

int repl_is_answered();

int repl_is_ongoing_election();

#endif
