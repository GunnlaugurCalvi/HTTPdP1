#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <ctype.h>
#include <stdbool.h>
#include <glib.h>
#include <poll.h>
//#include <libexplain/poll.h> would be beautiful if this would work
int main(int argc, char *argv[])
{
	//Initialize and declare variables
	int sock = 0, connfd = 0;
	int portNumber;
	ssize_t val;
	struct sockaddr_in server, client;
	struct pollfd fd;
	int timeout_msecs = 500;
	char buf[1025];
	time_t t;
	struct tm *timeZone = NULL;
	
	//Input is valid
	if(argc != 2){
		printf("[ERR] Invalid input: Must take only 1 parameter(portnumber) \n");
		exit(EXIT_FAILURE);
	}

	if(argc == 2){
		portNumber = atoi(argv[1]);
		printf("using portNumber %d\n", portNumber);
	}

	//Create socket
	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("Socket error\n");
		exit(EXIT_FAILURE);
	}
	//Allocate memory for the server and buffer
	memset(&server, '0', sizeof(server));
	memset(buf, '0', sizeof(buf));

	server.sin_family = AF_INET;			//WE ARE FAM ILY
	server.sin_addr.s_addr = htonl(INADDR_ANY);	//Long integer -> Network byte order
	server.sin_port = htons(portNumber);		//Set port number for the server

	//Bind socket to socket address, exit if it fails
	if(bind(sock, (struct sockaddr*) &server, sizeof(server)) < 0){
			perror("Bind error\n");
			exit(EXIT_FAILURE);
	}
	//Listen to the socket, allow queue of maximum 1
	if(listen(sock, 1) < 0){
		perror("Listen error\n");
		exit(EXIT_FAILURE);		
	}
	
	while(true){
		socklen_t clientlen = (socklen_t) sizeof(client);
		//Acceppt connection on socket, no restrictions
		if((val = poll(&fd, sock, timeout_msecs) < 0)){
			//[explain_poll]Explains underlying cause of error in more detail
			//fprintf(stderr, "%s\n", explain_poll(&fd, sock, timeout_msecs));
			perror("Poll error\n");
			exit(EXIT_FAILURE);
		}
		if((connfd = accept(sock, (struct sockaddr*) &client, &clientlen) < 0)){
			perror("Accept error\n");
			exit(EXIT_FAILURE);	
		}
		//Start the clock
		t = time(NULL);
		timeZone = localtime(&t);
	
		//Get the client IP and port in human readable form
		char *clientIP = inet_ntoa(client.sin_addr);
		unsigned short cPort = ntohs(client.sin_port);
		fprintf(stdout,"Client IP and port %s:%d\n", clientIP, cPort);	
		fflush(stdout);
		//Write out time in ISO 8601 format!
		//Could not use the g_time_val_to_iso8601() because it is in version 2.12
		if(strftime(buf,sizeof(buf), "%FT%T\n", timeZone) == 0){
			fprintf(stderr, "Strftime error\n");
			exit(EXIT_FAILURE);
		}
		write(connfd, buf, strlen(buf));
		
		shutdown(connfd, SHUT_RDWR);
		close(connfd);
	}
	return 0;
}
