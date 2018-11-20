#include <stdio.h>
#include <stdlib.h>
#include "comm.h"
#include "log.h"
#include "utils.h"
#include "server.h"

struct comm_entity __server_entity;

int main(int argc, char *argv[])
{
    if(argc != 2)
    {
    	fprintf(stderr, "Usage: ./server port\n");
    	exit(1);
  	}

	int port = atoi(argv[1]);

	server_setup(port);

	return 0;
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

    log_info("comm", "Server is socket %d", __server_entity.socket_instance);

	comm_init(__server_entity);

	return 0;
}