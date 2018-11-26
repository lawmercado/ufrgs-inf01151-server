#ifndef __REPL_H__
#define __REPL_H__

#include <limits.h>
#include "comm.h"

#define REPL_MAX_SERVER 3

struct repl_server {
    int id;
    int primary;
    int down;
    char host[HOST_NAME_MAX];
    struct comm_entity entity;
    struct comm_entity replication_entity;
};

int repl_init();

int repl_load_servers();

int repl_is_primary(int id);

int repl_backup_set_is_primary_down();

int repl_get_primary_address(char host[HOST_NAME_MAX]);

int repl_primary_send_ping();

int repl_primary_send_login(char *username, char *address, int port);

int repl_primary_send_logout(char *username, int port);

int repl_primary_send_sync_dir(char *username);

int repl_primary_send_upload(char *username, char *file);

int repl_primary_send_delete(char *username, char *file);

int repl_backup_start_election(int id);

int repl_set_is_election_ongoing();

int repl_set_is_not_election_ongoing();

int repl_backup_send_election_answer(int to);

int repl_backup_send_coordinator(int elected);

int repl_backup_set_new_primary(int elected);

int repl_set_election_answered();

int repl_set_election_not_answered();

int repl_is_election_answered();

int repl_is_election_ongoing();

#endif
