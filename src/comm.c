#include "comm.h"

void __create_socket(struct sockaddr_in *server_sock, int port){

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

	/*
	ssize_t sendto (
		int s,				=> descriptor for socket
		const void * msg,	=> pointer to the message that we want to send
		size_t len, 		=> length of message
		int flags			=>
		const struct sockaddr *to > pointer to a sockaddr object that specifies the addess of the target
		socklen_t tolen 	=> object that specifies the size of the target
	)

	struct Frame {
    	int id_msg;
    	int ack;
    	char buffer[256];
    	char user[25];
	};
	*/

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

int __receive_packet(TextMessage *newMsg, int sockfd, struct sockaddr_in *serv){

	/*
	ssize_t sendto (
		int s,				=> descriptor for socket
		const void * msg,	=> pointer to the message that we want to send
		size_t len, 		=> length of message
		int flags			=> 
		const struct sockaddr *to > pointer to a sockaddr object that specifies the addess of the target
		socklen_t tolen 	=> object that specifies the size of the target
	)
	*/

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