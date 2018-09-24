#include "comm.h"

int main(int argc, char *argv[])
{

	int sockfd;

	if(argc != 3){
    	fprintf(stderr, "Usage: server hostname port\n");
    	exit(1);
  	}

	char *hostname = argv[1];
	int port = atoi(argv[2]);
    
    if((sockfd = open_server(hostname, port)) < 0)
	{
		///log error
	}

	close_server(sockfd);

	return 0;
}
