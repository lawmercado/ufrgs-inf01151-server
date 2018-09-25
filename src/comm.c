#include "comm.h"
#include "log.h"

/*void __create_socket(struct sockaddr_in *server_sock, int port){

	server_sock->sin_family = AF_INET;
	server_sock->sin_port = htons(port);
	server_sock->sin_addr.s_addr = INADDR_ANY;
	bzero(&(server_sock->sin_zero), 8);

}

int open_server(char *hostname, int port)
{
	int sockfd;
	struct sockaddr_in server_sock;

	__create_socket(&server_sock, port);

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
		printf("ERROR opening socket");
		exit(1);
	}

	printf("\nHostname: %s\nPort: %d\nSocket id: %d\n", hostname, port, sockfd);


	if (bind(sockfd, (struct sockaddr *) &server_sock, sizeof(struct sockaddr)) < 0){
		printf("ERROR on binding");
		exit(1);
	}

	wait_connection(hostname, port, sockfd);

	return sockfd;

}

int close_server(int sockfd){

	close(sockfd);
	return 0;

}

int __send_packet(int *id_msg, char *buffer, int sockfd, struct sockaddr_in *serv){
    Frame packet;

	int status = 0;

	socklen_t tolen = sizeof(struct sockaddr_in);

	memcpy(packet.buffer, buffer, BUFFER_SIZE);
	packet.ack = 0;
	packet.id_msg = *id_msg;

	while(packet.ack != 1){
		status = sendto(sockfd, &packet, sizeof(packet), 0, (const struct sockaddr *) serv, sizeof(struct sockaddr_in));
		if(status < 0){
			printf("\n[Error sendto]: Sending packet fault!\n");
			return -1;
		}
		else{
			printf("\n[Sendto ok]: Sending packet: socket id: %d, msg id: %d!\n", sockfd, packet.id_msg);
		}

		status = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *) serv, &tolen);
		if(status < 2){

			printf("\n[Error recvfrom]: Receiving ack fault!\n");
			return -2;
		}

		packet.ack = 1;


	}

	printf("Got an ack: %s\n", buffer);


	*id_msg = *id_msg +1;
	return 0;

}

int __receive_packet(int *id_msg, char *buffer, int sockfd, struct sockaddr_in *serv){
    Frame packet;
	int status = 0;
	int ack = 0;

	socklen_t fromlen = sizeof(struct sockaddr_in);

	do{
		status = recvfrom(sockfd, newMsg, sizeof(*newMsg), 0, (struct sockaddr *) serv, &fromlen);
		if(status < 0){
			printf("\n[Error recvfrom]: Receiving packet fault!\n");
			return -1;
		}
		else{
			printf("\n[Recvfrom ok]: Receiving packet: socket id: %d, msg id: !\n", sockfd);
		}

		if(sendto(sockfd, &newMsg, sizeof(newMsg), 0, (const struct sockaddr *) serv, sizeof(struct sockaddr_in)) < 2){

			printf("\n[Error sendto]: Sending ack fault!\n");
			return -2;
		}
		else
		{
			printf("\n[Sendto ok]: Sending ack: socket id: %d!\n", sockfd);
		}

		ack = 1;
	}
	while(ack != 1);

	printf("Got an ack: %s\n", newMsg->buffer);


	return 0;

}

int wait_connection(char *hostname, int port, int sockfd)
{
	struct sockaddr_in cli_sock;
	socklen_t clilen;
	int msg_counter;


	TextMessage newMsg;

	clilen = sizeof(struct sockaddr_in);

	while (1) {

		bzero(newMsg.buffer, BUFFER_SIZE);

		if(__receive_packet(&newMsg, sockfd, &cli_sock) < 0)
		{
			/// error
			printf("ERROR on recvfrom");
			exit(1);
		}
		else
		{
			printf("Received a datagram: %s\n", newMsg.buffer);
		}

	}

	return 0;
}
*/

int __socket_instance;
int __counter_client_port;

void __init_sockaddr(struct sockaddr_in *sockaddr, int port)
{
    sockaddr->sin_family = AF_INET;
	sockaddr->sin_port = htons(port);
	sockaddr->sin_addr.s_addr = INADDR_ANY;
	bzero((void *)&(sockaddr->sin_zero), sizeof(sockaddr->sin_zero));
}

int __send_packet(struct sockaddr_in *server_sockaddr, struct comm_packet *packet)
{
    int status;

    status = sendto(__socket_instance, (void *)packet, sizeof(struct comm_packet), 0, (struct sockaddr *)server_sockaddr, sizeof(struct sockaddr));

    if(status < 0)
    {
        return -1;
    }

    return 0;
}

int __receive_packet(struct sockaddr_in *server_sockaddr, struct comm_packet *packet)
{
    /*
    TODO: compare this with the server address to check origin
    char clienthost[100];
    char clientport[100];
    int result = getnameinfo(&from, length, clienthost, sizeof(clienthost), clientport, sizeof (clientport), NI_NUMERICHOST | NI_NUMERICSERV);

    if(result == 0)
    {
        if (from.sa_family == AF_INET)
            printf("Received from %s %s\n", clienthost, clientport);
    }*/

    int status;
    struct sockaddr from;
    socklen_t from_length;

    // Receives an ack from the server
    status = recvfrom(__socket_instance, (void *)packet, sizeof(*packet), 0, (struct sockaddr *)&from, &from_length);

    if(status < 0)
    {
        return -1;
    }

    return 0;
}

int __send_ack(struct sockaddr_in *server_sockaddr)
{
    struct comm_packet packet;

    packet.type = COMM_PTYPE_ACK;
    bzero(packet.payload, COMM_PPAYLOAD_LENGTH);

    if(__send_packet(server_sockaddr, &packet) != 0)
    {
        log_error("comm", "Ack could not be sent");

        return -1;
    }

    return 0;
}

int __receive_ack(struct sockaddr_in *server_sockaddr)
{
    struct comm_packet packet;

    if(__receive_packet(server_sockaddr, &packet) != 0)
    {
        log_error("comm", "Could not receive an ack");

        return -1;
    }

    if(packet.type != COMM_PTYPE_ACK)
    {
        log_error("comm", "The received packet is not an ack");

        return -1;
    }

    return 0;
}

int __send_data(struct sockaddr_in *server_sockaddr, char buffer[COMM_PPAYLOAD_LENGTH])
{
    struct comm_packet packet;

    packet.type = COMM_PTYPE_DATA;
    packet.length = strlen(buffer);
    bzero(packet.payload, COMM_PPAYLOAD_LENGTH);
    strncpy(packet.payload, buffer, strlen(buffer));

    if(__send_packet(server_sockaddr, &packet) != 0)
    {
        log_error("comm", "Data could not be sent");

        return -1;
    }

    return __receive_ack(server_sockaddr);
}

int __receive_data(struct sockaddr_in *server_sockaddr, char buffer[COMM_PPAYLOAD_LENGTH])
{
    struct comm_packet packet;

    if(__receive_packet(server_sockaddr, &packet) != 0)
    {
        log_error("comm", "Could not receive the data");

        return -1;
    }

    if( packet.type != COMM_PTYPE_DATA )
    {
        log_error("comm", "The received packet is not a data");

        return -1;
    }

    bzero(buffer, COMM_PPAYLOAD_LENGTH);
    strncpy(buffer, packet.payload, packet.length);

    return __send_ack(server_sockaddr);
}

int __send_command(struct sockaddr_in *server_sockaddr, char command[COMM_PPAYLOAD_LENGTH])
{
    struct comm_packet packet;

    packet.type = COMM_PTYPE_CMD;
    packet.length = strlen(command);
    bzero(packet.payload, COMM_PPAYLOAD_LENGTH);
    strncpy(packet.payload, command, strlen(command));

    if(__send_packet(server_sockaddr, &packet) != 0)
    {
        log_error("comm", "Command '%s' could not be sent", command);

        return -1;
    }

    return __receive_ack(server_sockaddr);
}

int __receive_command(struct sockaddr_in *server_sockaddr, char command[COMM_PPAYLOAD_LENGTH])
{
    struct comm_packet packet;

    if(__receive_packet(server_sockaddr, &packet) != 0)
    {
        log_error("comm", "Could not receive the command");

        return -1;
    }

    if( packet.type != COMM_PTYPE_CMD )
    {
        log_error("comm", "The received packet is not a command");

        return -1;
    }

    bzero(command, COMM_PPAYLOAD_LENGTH);
    strncpy(command, packet.payload, packet.length);

    return __send_ack(server_sockaddr);
}

void __wait_connection()
{
    while(1)
    {
    }
}

int comm_init(int port)
{
    struct sockaddr_in sockaddr;

	__init_sockaddr(&sockaddr, port);

    if((__socket_instance = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
		log_error("comm", "Could not create socket instance");

        return -1;
	}

	if(bind(__socket_instance, (struct sockaddr *) &sockaddr, sizeof(struct sockaddr)) < 0)
    {
		log_error("comm", "Could not bind");

        close(__socket_instance);
		return -1;
	}

    __counter_client_port = port + 1;

    __wait_connection();

    return 0;
}
