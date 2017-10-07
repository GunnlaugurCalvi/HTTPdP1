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
//void createHashTable(GHashTable *hashTable, char *msg);

void buildHeader(GString *headerResponse, gsize contentLen);			
void getIsoDate(char *buf);
void buildHead(GString *headerResponse);
void buildBooty(GString *resp, char msg[], struct sockaddr_in cli, char port[], bool isGoods);
//void LogToFile(GString *resp, char msg[], struct sockaddr_in cli);


int main(int argc, char *argv[])
{
	//Initialize and declare variables
	gint sock = 0, connfd = 0;
	gint portNumber, val;
	gsize conLen;
	struct sockaddr_in server, client;
	struct pollfd fds;
	gint timeout_msecs = 500;
	char buf[BUFSIZE];
	char msg[MESSAGESIZE];
	//struct tm *timeZone = NULL;
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
	if(listen(sock, 10) < 0){
		perror("Listen error\n");
		exit(EXIT_FAILURE);		
	}
	
	while(true){
		connfd = 0;
		socklen_t clientlen = (socklen_t) sizeof(client);
		GString *resp = g_string_new("<!DOCTYPE html>\r\n<html>\r\n");
		GString *headerResponse = g_string_new("HTTP/1.1 200 OK\r\n");
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
		
		if(g_str_has_prefix(msg, "GET")){
			buildHead(resp);
			buildBooty(resp, msg, client, argv[1], false);
			//LogToFile(resp, msg, client);
		}
		else{
			perror("Bad Request\n");
			exit(EXIT_FAILURE);
		}


		//Initilize hash table 
	//	hashTable = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
		
	//	createHashTable(hashTable, msg);
		
		//Get the client IP and port in human readable form
		char *clientIP = inet_ntoa(client.sin_addr);
		unsigned short cPort = ntohs(client.sin_port);	
		fprintf(stdout,"Client IP and port %s:%d\n", clientIP, cPort);	
		fflush(stdout);
		
		
		write(connfd, buf, strlen(buf));

		conLen = resp->len;

		buildHeader(headerResponse, conLen);

		g_string_append(headerResponse, resp->str);

		write(connfd, headerResponse->str, headerResponse->len);
		
		g_string_free(resp, 1);
		g_string_free(headerResponse, 1);	
	
		shutdown(connfd, SHUT_RDWR);
		close(connfd);
	}
	close(sock);
	return 0;
}
/*void LogToFile(GString *resp, char msg[], struct sockaddr_in client){
	
	
	char date[BUFSIZE];
	getIsoDate(date);
	char *clientIP = inet_ntoa(client.sin_addr);
	unsigned short clientPort = ntohs(client.sin_port);
	GString *requestMethod = g_string_new("");
	GString *requestURL = g_string_new("http://");
	
	gchar **splitter = g_strsplit(msg, ": ", -1);
	g_string_append(requestURL, splitter[2]);

	if(g_str_has_prefix(msg, "GET")){
		g_string_append(requestMethod, "GET");
	}
	else if(g_str_has_prefix(msg, "PUT")){
		g_string_append(requestMethod, "PUT");
	}
	else{
		g_string_append(requestMethod, "HEAD");
	}
	
	FILE *logger = fopen("server.log", "a");

	if(logger != NULL){
		fprintf(logger, "<%s>:<%s> <%d> <%s> <%s>:<200 OK>\n", date, clientIP, clientPort, requestMethod->str, requestURL->str);
	fclose(logger);
	}
	else{
		perror("File open error\n");
		return;	
	}
	g_strfreev(splitter);	
	g_string_free(requestMethod, 1);
	g_string_free(requestURL, 1);	
}*/
void buildHeader(GString *headerResponse, gsize contentLen){

	char isodate[BUFSIZE];
	char unBuf[10];
	//Write out time in ISO 8601 format!
	getIsoDate(isodate);
	g_string_append(headerResponse, "Content-Type: text/html; charset=utf-8\r\n");
	g_string_append(headerResponse, "Date: ");
	g_string_append(headerResponse, isodate);
	g_string_append(headerResponse, "\r\n");
	g_string_append(headerResponse, "Connection: keep-alive\r\n");
	g_string_append(headerResponse, "Keep-Alive: timeout=30, max=100\r\n");
	g_string_append(headerResponse, "Content-Length: ");
	sprintf(unBuf, "%u", contentLen);
	g_string_append(headerResponse, unBuf);
	g_string_append(headerResponse, "\r\n\r\n");

}

void getIsoDate(char *buf){
	//Start the clock
	struct tm *timeZone;
	time_t t = time(&t);
	timeZone = localtime(&t);

	//Could not use the g_time_val_to_iso8601() because it is in version 2.12
	if(strftime(buf, BUFSIZE,"%FT%T", timeZone) == 0){
		fprintf(stderr, "Strftime error\n");
		exit(EXIT_FAILURE);
	}
}
void buildHead(GString *resp){
	g_string_append(resp, "<head>\r\n YALL FINISHED OR YALL DONE \n</head>\r\n");
}
void buildBooty(GString *resp, char msg[], struct sockaddr_in client, char port[], bool isGoods){


	g_string_append(resp, "<body>");
	g_string_append(resp , "\r\n");
	g_string_append(resp, "http://");
	gchar **splitter = g_strsplit(msg, ": ", -1);
	
	//strips away leading and trailing whitespace from string
	g_string_append(resp, g_strstrip(splitter[1]));
	
	g_string_append(resp, " ");
	g_string_append(resp, inet_ntoa(client.sin_addr));
	g_string_append(resp, ":");
	g_string_append(resp, port);
	g_string_append(resp, "\n");

	//If it includes goods make dat gooshiii
	
	g_string_append(resp, "\n");
	g_string_append(resp, "BIG BUTT\n");
	g_string_append(resp, "</body>\r\n</html>");
	g_string_append(resp, "\r\n");
	
	g_strfreev(splitter);
}

/*void createHashTable(GHashTable *hashTable, char *msg){

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
}*/


