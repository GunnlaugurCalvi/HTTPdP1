#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <time.h>
#include <ctype.h>
#include <stdbool.h>
#include <glib.h>
#include <sys/poll.h>
#include <errno.h>
//#include <libexplain/poll.h> would be beautiful if this would work

#define OK "200 OK"
#define CREATED "201 Created"
#define METHOD_NOT_ALLOWED "405 Method Not Allowed"
#define HTTP_VERSION_NOT_SUPPORTED "505 HTTP Version not supported"


const gint BUFSIZE = 1024;
const int MESSAGESIZE = 4096;


//Helper functions
//void createHashTable(GHashTable *hashTable, char *msg);

void buildHeader(GString *headerResponse, char msg[], gsize contentLen, struct sockaddr_in client, gchar *respondCode);		
void getIsoDate(char *buf);
void buildHead(GString *headerResponse);
void buildBooty(GString *resp, char msg[], struct sockaddr_in cli, bool isGoods);
void LogToFile(char msg[], struct sockaddr_in cli, gchar *respondCode);
void getData(GString *resp, char msg[]);


int main(int argc, char *argv[])
{
	//Initialize and declare variables
	gint sock = 0, connfd = 0, reuse;
	int on = 1, nfds = 1, currentNfds = 0, i;
	gint portNumber, val;
	gsize conLen;
	struct sockaddr_in server, client;
	struct pollfd fds[100];
	gint timeout_msecs = 30000;
	char buf[BUFSIZE];
	char msg[MESSAGESIZE];
	bool isClosed = false;
	//GHashTable *hashTable;

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

	//Allow socket to be reuseable
	if((reuse = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on))) < 0){
		perror("Setsockopt error\n");
		close(sock);
		exit(EXIT_FAILURE);	
	}
	
	//Set the sock to be nonblocking, also will
	//the other sockets be nonblocking because they 
	//inherit that state from the listening socket
	if((reuse = ioctl(sock, FIONBIO, (char *)&on)) < 0){
		perror("Ioctl error\n");
		close(sock);
		exit(EXIT_FAILURE);
	}


	//Allocate memory for the server and buffer
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;			//WE ARE FAM ILY
	server.sin_addr.s_addr = htonl(INADDR_ANY);	//Long integer -> Network byte order
	server.sin_port = htons(portNumber);		//Set port number for the server

	//Bind socket to socket address, exit and close socket if it fails
	if((reuse = bind(sock, (struct sockaddr*) &server, sizeof(server))) < 0){
			perror("Bind error\n");
			close(sock);
			exit(EXIT_FAILURE);
	}
	

	//Listen to the socket, allow queue of maximum 5
	if((reuse = listen(sock, 5)) < 0){
		perror("Listen error\n");
		close(sock);
		exit(EXIT_FAILURE);		
	}
	
	//Allocatin the pollfd 
	memset(fds, 0, sizeof(fds));
	
	//Set up the first listening socket
	//POLLIN allows data other than high-priority
	//may be read without blocking
	fds[0].fd = sock;
	fds[0].events = POLLIN;
	while(true){
		socklen_t clientlen = (socklen_t) sizeof(client);
		GString *resp = g_string_new(NULL);
		GString *headerResponse = g_string_new(NULL);

		reuse = poll(fds, nfds, timeout_msecs);

		//Poll failed	
		if(reuse < 0){
			//[explain_poll]Explains underlying cause of error in more detail
			//fprintf(stderr, "%s\n", explain_poll(&fd, sock, timeout_msecs));
			perror("Poll error\n");
			break;
		}

		//This condition will happened if it has waited longer
		//than 30 seconds
		if(reuse == 0){
			perror("Poll timeout\n");
			break;
		}
		currentNfds = nfds;
		//Loop through the readable file descriptors
		for(i = 0; i < currentNfds; i++){
			
			//revents are the returned events
			//check if its POLLIN and if its not
			//than we shutdown
			if(fds[i].revents == 0){
				continue;
			}
			if(fds[i].revents != POLLIN){
				printf("Fds error!, revetns =  %d\n", fds[i].revents);
				exit(EXIT_FAILURE);	
			}

			//Accept all connections that are 
			//in our queue and are on the listening port
			if(fds[i].fd == sock){
			
				while(true){
	
					//Accept the connections that are incoming until we
					//get EWOULDBLOCK that says we have accepted all 
					//connections, else we will terminate 
					if((connfd = accept(sock, (struct sockaddr*) &client, &clientlen)) < 0){
						if(errno != EWOULDBLOCK){
							perror("Accept error\n");
							exit(EXIT_FAILURE);
						}
						break;
					}
					//Add the connection in to our array of fds
					fds[nfds].fd = connfd;
					fds[nfds].events = POLLIN;
					nfds += 1;

					if(connfd == -1){
						break;
					}
				}
			}
			else{
				
				//We check for existing connection
				//and receive all the incoming data
				//from this socket
				while(true){
					//Recv failure
					reuse = recv(fds[i].fd, msg, sizeof(msg), 0);
					if(reuse < 0){
						if(errno != EWOULDBLOCK){
							perror("Recv error\n");
							isClosed = true;
						}
						break;
					}
					
					//connection closed
					if(reuse == 0){
						printf("connection is closed for = %d\n", fds[i].fd);
						isClosed = true;
						break;
					}
				
					if(g_str_has_prefix(msg, "GET")){
						buildHead(resp);
						buildBooty(resp, msg, client, false);
					
						conLen = resp->len;
						buildHeader(headerResponse, msg, conLen, client, OK);

						LogToFile(msg, client, OK);
					}
					else if(g_str_has_prefix(msg, "POST")){
						buildHead(resp);
						buildBooty(resp, msg, client,true);
						conLen = resp->len;
						buildHeader(headerResponse, msg, conLen, client, CREATED);
						LogToFile(msg, client, CREATED);			
					}
					else if(g_str_has_prefix(msg, "HEAD")){
						conLen = resp->len;
						buildHeader(headerResponse, msg, conLen, client, OK);
						LogToFile(msg, client, OK);
					}
					else{
						conLen = resp->len;
						buildHeader(headerResponse, msg, conLen, client, METHOD_NOT_ALLOWED);
						LogToFile(msg, client, METHOD_NOT_ALLOWED);
					}

					g_string_append(headerResponse, resp->str);
		
		
					//Get the client IP and port in human readable form
					char *clientIP = inet_ntoa(client.sin_addr);
					unsigned short cPort = ntohs(client.sin_port);	
					fprintf(stdout,"Client IP and port %s:%d\n", clientIP, cPort);	
					fflush(stdout);		

					reuse = write(fds[i].fd, buf, strlen(buf));
					reuse = write(fds[i].fd, headerResponse->str, headerResponse->len);
						
					if(reuse < 0){
						perror("Write error\n");
						isClosed = true;
						break;
					}		
				}
				if(isClosed){		
					close(fds[i].fd);
					fds[i].fd = -1;
				}
			}
		}
		
		g_string_free(resp, 1);
		g_string_free(headerResponse, 1);	
	}
	for(i = 0; i < nfds; i++){
		if(fds[i].fd >= 0){
			shutdown(fds[i].fd, SHUT_RDWR);
			close(fds[i].fd);
		}
	}
	return 0;
}

void LogToFile(char msg[], struct sockaddr_in client, gchar *respondCode){
	
	
	char date[BUFSIZE];
	getIsoDate(date);
	char *clientIP = inet_ntoa(client.sin_addr);
	unsigned short clientPort = ntohs(client.sin_port);
	GString *requestMethod = g_string_new("");
	
	GString *requestURL = g_string_new("http://");	
	gchar **splitter = g_strsplit(msg, "Host: ", -1);
	gchar **url = g_strsplit(splitter[1], "\n", -1);
	gchar **urlSplitter = g_strsplit(msg, " ", -1);

	g_string_append(requestURL, g_strstrip(url[0]));

	g_string_append(requestURL, g_strstrip(urlSplitter[1]));

	if(g_str_has_prefix(msg, "GET")){
		g_string_append(requestMethod, "GET");
	}
	else if(g_str_has_prefix(msg, "POST")){
		g_string_append(requestMethod, "POST");
	}
	else{
		g_string_append(requestMethod, "HEAD");
	}
	
	FILE *logger = fopen("server.log", "a");

	if(logger != NULL){
		fprintf(logger, "<%s> : %s:%d <%s> <%s>:<%s>\n", date, clientIP, clientPort, requestMethod->str, requestURL->str, respondCode);
	fclose(logger);
	}
	else{
		perror("File open error\n");
		return;	
	}
	g_strfreev(splitter);	
	g_string_free(requestMethod, 1);
	g_string_free(requestURL, 1);
	g_strfreev(url);
	g_strfreev(urlSplitter);	
}
void buildHeader(GString *headerResponse, char msg[], gsize contentLen, struct sockaddr_in client, gchar *respondCode){
	gchar **HTTPsplitter = g_strsplit(msg, " ", -1);
	gchar **testy = g_strsplit(HTTPsplitter[2], "\n", -1);

	if(g_str_has_prefix(testy[0], "HTTP/1.1")){
		g_string_append(headerResponse, "HTTP/1.1 ");
		g_string_append(headerResponse, respondCode);
		g_string_append(headerResponse, "\r\n");
	}
	else if(g_str_has_prefix(testy[0], "HTTP/1.0")){
		g_string_append(headerResponse, "HTTP/1.0 ");
		g_string_append(headerResponse, respondCode);
		g_string_append(headerResponse, "\r\n");
	}
	else{
		g_string_append(headerResponse, "HTTP/1.0 ");
		g_string_append(headerResponse, HTTP_VERSION_NOT_SUPPORTED);
		g_string_append(headerResponse, "\r\n");
		LogToFile(msg, client, HTTP_VERSION_NOT_SUPPORTED);
	}
	
	char isodate[BUFSIZE];
	char unBuf[10];
	//Write out time in ISO 8601 format!
	getIsoDate(isodate);
	g_string_append(headerResponse, "Content-Type: text/html; charset=utf-8\r\n");
	g_string_append(headerResponse, "Date: ");
	g_string_append(headerResponse, isodate);
	g_string_append(headerResponse, "\r\n");
	if(g_str_has_prefix(testy[0], "HTTP/1.1")){
		g_string_append(headerResponse, "Connection: keep-alive\r\n");
		g_string_append(headerResponse, "Keep-Alive: timeout=30, max=100\r\n");
	}
	else{
		g_string_append(headerResponse, "Connection: close\r\n");
	}
	g_string_append(headerResponse, "Content-Length: ");
	sprintf(unBuf, "%u", contentLen);
	g_string_append(headerResponse, unBuf);
	g_string_append(headerResponse, "\r\n\r\n");

	g_strfreev(HTTPsplitter);
	g_strfreev(testy);
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
	g_string_append(resp, "<!DOCTYPE html>\r\n<html>\r\n");
	g_string_append(resp, "<head>\r\n");
	g_string_append(resp, "<title> GINA IS NOT APACHE</title>\r\n");
	g_string_append(resp, "\n</head>\r\n");
}
void buildBooty(GString *resp, char msg[], struct sockaddr_in client, bool isGoods){

	//Convert the client port number to string so we can append 
	//to our GString
	gchar *cPort = g_strdup_printf("%i", ntohs(client.sin_port));
	
	g_string_append(resp, "<body>");
	g_string_append(resp , "\r\n");
	g_string_append(resp, "http://");
	
	//Split the request to get our host link
	gchar **splitter = g_strsplit(msg, "Host: ", -1);
	gchar **url = g_strsplit(splitter[1], "\n", -1);

	gchar **urlSplitter = g_strsplit(msg, " ", -1);
	
	//strips away leading and trailing whitespace from string
	g_string_append(resp, g_strstrip(url[0]));
	//Get the requested site (f.x /index)
	g_string_append(resp, urlSplitter[1]);

	g_string_append(resp, " ");
	g_string_append(resp, inet_ntoa(client.sin_addr));
	g_string_append(resp, ":");
	g_string_append(resp, cPort);
	g_string_append(resp, "\n");

	if(isGoods == true){
		getData(resp, msg);	
	
	}	
	g_string_append(resp, "\n");
	g_string_append(resp, "</body>\r\n</html>");
	g_string_append(resp, "\r\n");
	
	g_free(cPort);
	g_strfreev(splitter);
	g_strfreev(url);
	g_strfreev(urlSplitter);
}

void getData(GString *resp, char msg[]){
	
	gchar **data = g_strsplit(msg, "\r\n\r\n", -1);
	gchar **dataSplitter = g_strsplit(msg, "Content-Length: ", -1);
	gchar **dataSize = g_strsplit(dataSplitter[1], "\n", 0);
	
	if(data[1] == NULL){
		return;
	}
	g_string_append(resp, data[1]);
	g_string_append(resp, "\n");

	g_strfreev(data);
	g_strfreev(dataSplitter);
	g_strfreev(dataSize);
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


