#include "comm.h"

int main(int argc, char *argv[])
{
    if(argc != 1)
    {
    	fprintf(stderr, "Usage: ./server port\n");
    	exit(1);
  	}

	int port = atoi(argv[2]);

    printf("COMM INIT %d\n", comm_init(port));

	return 0;
}
