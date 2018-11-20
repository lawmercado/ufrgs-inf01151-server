#include <stdio.h>
#include <stdlib.h>
#include "repl.h"

struct repl_server servers[REPL_MAX_SERVER];

int repl_init()
{
    int i;

    for(i = 0; i < REPL_MAX_SERVER; i++)
    {
        servers[i].id = -1;
        servers[i].primary = -1;
    }
}

int repl_load_servers(struct repl_server servers[REPL_MAX_SERVER])
{
    FILE *f;
    int idx = 0;

    f = fopen("../known_servers.txt", "r");

    while(!feof(f) && idx < REPL_MAX_SERVER)
    {
        int id, port;
        char host[50], type;

        fscanf(f, "%d %s %d %c", id, host, port, type);

        servers[idx].id = id;
        servers[idx].primary = type == 'p';
        // Inicializar sockaddr
    }

    fclose(f);
}