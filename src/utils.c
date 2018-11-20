#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "utils.h"
#include "log.h"

void utils_init_sockaddr(struct sockaddr_in *sockaddr, int port, in_addr_t acceptFrom)
{
    sockaddr->sin_family = AF_INET;
	sockaddr->sin_port = htons(port);
	sockaddr->sin_addr.s_addr = acceptFrom;
	bzero((void *)&(sockaddr->sin_zero), sizeof(sockaddr->sin_zero));
}

int utils_init_sockaddr_to_host(struct sockaddr_in *sockaddr, int port, char *host)
{
    struct hostent *h;

    if((h = gethostbyname(host)) == NULL)
    {
        log_error("comm", "Could not find the '%s' host", host);

        return -1;
    }

    sockaddr->sin_family = AF_INET;
	sockaddr->sin_port = htons(port);
	sockaddr->sin_addr = *((struct in_addr *) h->h_addr_list[0]);
	bzero((void *)&(sockaddr->sin_zero), sizeof(sockaddr->sin_zero));

    return 0;
}

int utils_create_socket()
{
    int socket_instance;

    if((socket_instance = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
		log_error("utils", "Could not create a socket instance");

        return -1;
	}

    return socket_instance;
}

int utils_create_binded_socket(struct sockaddr_in *sockaddr)
{
    int socket_instance = utils_create_socket(sockaddr);

    if(socket_instance == -1)
    {
        return -1;
    }

	if(bind(socket_instance, (struct sockaddr *)sockaddr, sizeof(struct sockaddr)) < 0)
    {
		log_error("utils", "Could not create binded socket");

        close(socket_instance);

		return -1;
	}

    return socket_instance;
}

int utils_get_port(struct sockaddr *sockaddr)
{
    if (sockaddr->sa_family == AF_INET)
    {
        return ntohs((((struct sockaddr_in*)sockaddr)->sin_port));
    }

    return ntohs((((struct sockaddr_in6*)sockaddr)->sin6_port));
}