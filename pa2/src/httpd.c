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

const gint BUFSIZE = 1024;
const int MESSAGESIZE = 4096;


//Helper functions
void createHashTable(GHashTable *hashTable, char *msg);
void buildHead(GString *resp);
void buildBooty(char resp[], char msg[], char *cli, unsigned short port, bool isGoods);
					

int main(int argc, char *argv[])
{
	//Initialize and declare variables
	gint sock = 0, connfd = 0;
	gint portNumber, val;
	struct sockaddr_in server, client;
	struct pollfd fds;
	gint timeout_msecs = 500;
	char buf[BUFSIZE];
	char msg[MESSAGESIZE];
	time_t t;
	struct tm *timeZone = NULL;
	FILE *logger;	
	GHashTable *hashTable;
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
	memset(&server, 0, sizeof(server));

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
		GString *resp = g_string_new("<!DOCTYPE html>\n<html>");

	
		/*if((val = poll(&fds, sock, timeout_msecs) < 0)){
			//[explain_poll]Explains underlying cause of error in more detail
			//fprintf(stderr, "%s\n", explain_poll(&fd, sock, timeout_msecs));
			perror("Poll error\n");
			exit(EXIT_FAILURE);
		}*/
	
		//Accept connection on socket, no restrictions
		if((connfd = accept(sock, (struct sockaddr*) &client, &clientlen)) < 0){
			perror("Accept error\n");
			exit(EXIT_FAILURE);	
		}
		//Recieve message
		if((val = recv(connfd, msg, sizeof(msg)-1, 0)) < 0){
			perror("Recv error\n");
			exit(EXIT_FAILURE);
		}
		
		fprintf(stdout,"REVEEEVD \n%s\n", msg);
		fflush(stdout);	
	
		//Get the client IP and port in human readable form
		char *clientIP = inet_ntoa(client.sin_addr);
		unsigned short cPort = ntohs(client.sin_port);

		
		if(g_str_has_prefix(msg, "GET")){
			fprintf(stdout, "GETGETGET\n");
			fflush(stdout);
			buildHead(resp);
			//buildBooty(resp, msg, clientIP, cPort, false);
		}


		//Initilize hash table 
		hashTable = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
		
		createHashTable(hashTable, msg);
		
		//Start the clock
		t = time(NULL);
		timeZone = localtime(&t);
	
		fprintf(stdout,"Client IP and port %s:%d\n", clientIP, cPort);	
		fflush(stdout);
		//Write out time in ISO 8601 format!
		//Could not use the g_time_val_to_iso8601() because it is in version 2.12
		if(strftime(buf,sizeof(buf), "%FT%T\n", timeZone) == 0){
			fprintf(stderr, "Strftime error\n");
			exit(EXIT_FAILURE);
		}
		write(connfd, buf, strlen(buf));
		
		send(connfd, resp->str, sizeof(resp->str), 0);
		g_string_free(resp, 1);
		shutdown(connfd, SHUT_RDWR);
		close(connfd);
	}
	return 0;
}


void createHashTable(GHashTable *hashTable, char *msg){

	//msg contains the string "\r\n" in the 
	//end of every line we need to cut that out
	
	gchar **header = g_strsplit(msg, "\r\n", -1);
	gchar **copyHeader = NULL;
	int counter;
	bool containsGoods = false;
	
 	for(counter = 0; header[counter] != '\0'; counter++){
		if(!g_strcmp0(header[counter], "")){
			containsGoods = true;
			continue;
		}
		else if(containsGoods == true){
			g_hash_table_insert(hashTable, g_strdup("Goods"), g_strndup(header[counter], strlen(header[counter])));
		}
		else{
			copyHeader = !counter ? g_strsplit(header[counter], " ", -1) : g_strsplit(header[counter], ": ", 2);

			if(copyHeader != NULL && copyHeader[0] != NULL && copyHeader[1] != NULL){
				if(!counter){
					g_hash_table_insert(hashTable, g_strdup("req-type"), g_strndup(copyHeader[0], strlen(copyHeader[0])));

					g_hash_table_insert(hashTable, g_strdup("url"), g_ascii_strdown(copyHeader[1], strlen(copyHeader[1])));
				}
				else{
					g_hash_table_insert(hashTable, g_ascii_strdown(copyHeader[0], strlen(copyHeader[0])), g_ascii_strdown(copyHeader[1], strlen(copyHeader[1])));
				}
			}
			g_strfreev(copyHeader);
		}
	}
	g_strfreev(header);
}

void buildHead(GString *resp){
	g_string_append(resp, "<head>\n</head>\n");
}
void buildBooty(char resp[], char msg[], char *cli, unsigned short port, bool isGoods){
	
}
	
