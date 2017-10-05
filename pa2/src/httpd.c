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
#include <stdbool.h>
//#include <glib.h>


int main(int argc, char *argv[])
{
	//Initialize and declare variables
	int sock = 0, connfd = 0;
	int portNumber;
	struct sockaddr_in server, client;
	char buf[1025];
	time_t sec;

	portNumber = atoi(argv[1]);
	printf("using portNumber %d\n", portNumber);

	//Create socket
	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("Socket Error");
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
			perror("Bind error");
			exit(EXIT_FAILURE);
	}
	//Listen to the socket, allow queue of maximum 1
	listen(sock, 1);
	while(true){
		socklen_t clientlen = (socklen_t) sizeof(client);
		//Acceppt connection on socket, no restrictions
		connfd = accept(sock, (struct sockaddr*) &client, &clientlen);
		//Start the clock
		sec = time(NULL);
		//Write out time and filedescriptor
		snprintf(buf, sizeof(buf), "%.24s\r\n", ctime(&sec));
		write(connfd, buf, strlen(buf));
		

		close(connfd);
		sleep(1);
	}
	return 0;
}
