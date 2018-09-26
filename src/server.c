#include <stdio.h>
#include <stdlib.h>
#include "comm.h"
#include "log.h"

int main(int argc, char *argv[])
{
    if(argc != 2)
    {
    	fprintf(stderr, "Usage: ./server port\n");
    	exit(1);
  	}

	int port = atoi(argv[1]);

    comm_init(port);

	return 0;
}
