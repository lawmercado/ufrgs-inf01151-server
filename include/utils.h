#ifndef __UTILS_H__
#define __UTILS_H__

#include <netinet/in.h>

void utils_init_sockaddr(struct sockaddr_in *sockaddr, int port, in_addr_t acceptFrom);

int utils_init_sockaddr_to_host(struct sockaddr_in *sockaddr, int port, char *host);

int utils_create_socket();

int utils_create_binded_socket(struct sockaddr_in *sockaddr);

int utils_get_port(struct sockaddr *sockaddr);

#endif