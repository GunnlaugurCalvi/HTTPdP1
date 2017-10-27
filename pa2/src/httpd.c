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
#include <poll.h>
#include <errno.h>

#define OK "200 OK"
#define CREATED "201 Created"
#define NOT_IMPLEMENTED "501 Not Implemented"
#define HTTP_VERSION_NOT_SUPPORTED "505 HTTP Version not supported"


const gint BUFSIZE = 2048;
const gint MESSAGESIZE = 4096;

//Helper functions
void buildResponse(GString *headerResponse, char msg[], gsize contentLen, struct sockaddr_in client, gchar *respondCode);		
void getIsoDate(char *buf);
void buildHead(GString *headerResponse);
void buildBody(GString *resp, char msg[], struct sockaddr_in cli, bool isGoods);
void logToFile(char msg[], struct sockaddr_in cli, gchar *respondCode);
void getData(GString *resp, char msg[]);
void closeConnection(struct pollfd fds, bool free);

int main(int argc, char *argv[])
{
	//Initialize and declare variables
	gint sockfd = 0, connfd = 0, reuse, portNumber, timeout_msecs = 30000;
	int nfds = 1, currentNfds = 0, i, j;
	gsize conLen;
	struct sockaddr_in server, client;
	struct pollfd fds[100];
	char buf[BUFSIZE], msg[MESSAGESIZE];
	bool triggerClose = false, freeNfds = false;
	char *clientIP;
	unsigned short cPort;

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
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("Socket error\n");
		exit(EXIT_FAILURE);
	}

	//Allow socket to be reuseable
	if((reuse = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&sockfd, sizeof(sockfd))) < 0){
		perror("Setsockfdopt error\n");
		close(sockfd);
		exit(EXIT_FAILURE);	
	}
	
	//Set the sockfd to be nonblocking, also will
	//the other sockets be nonblocking because they 
	//inherit that state from the listening socket
	if((reuse = ioctl(sockfd, FIONBIO, (void *)&sockfd)) < 0){
		perror("Ioctl error\n");
		close(sockfd);
		exit(EXIT_FAILURE);
	}


	//Allocate memory for the server and buffer
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;			//WE ARE FAM ILY
	server.sin_addr.s_addr = htonl(INADDR_ANY);	//Long integer -> Network byte order
	server.sin_port = htons(portNumber);		//Set port number for the server

	//Bind socket to socket address, exit and close socket if it fails
	if((reuse = bind(sockfd, (struct sockaddr*) &server, sizeof(server))) < 0){
			perror("Bind error\n");
			close(sockfd);
			exit(EXIT_FAILURE);
	}
	

	//Listen to the socket, allow queue of maximum 10
	if((reuse = listen(sockfd, 10)) < 0){
		perror("Listen error\n");
		close(sockfd);
		exit(EXIT_FAILURE);		
	}
	
	//Allocating the pollfd 
	memset(fds, 0, sizeof(fds));
	
	//Set up the first listening socket
	//POLLIN allows data other than high-priority
	//may be read without blocking
	fds[0].fd = sockfd;
	fds[0].events = POLLIN;
	while(true){
		socklen_t clientlen = (socklen_t) sizeof(client);
		GString *resp = g_string_new(NULL);
		GString *headerResponse = g_string_new(NULL);

		reuse = poll(fds, nfds, timeout_msecs);

		//Poll failed	
		if(reuse < 0){
			perror("Poll error\n");
			break;
		}

		//This condition will happened if it has waited longer
		//than 30 seconds
		if(reuse == 0){
			perror("Poll timeout!\n");
			break;
			//close instead break
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
				perror("Fds error!\n");
				exit(EXIT_FAILURE);	
			}

			//Accept all connections that are 
			//in our queue and are on the listening port
			if(fds[i].fd == sockfd){
			
				while(true){
	
					//Accept the connections that are incoming until we
					//get EWOULDBLOCK that says we have accepted all 
					//connections, else we will terminate 
					if((connfd = accept(sockfd, (struct sockaddr*) &client, &clientlen)) < 0){
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
				//from this socketi
				while(true){
					reuse = recv(fds[i].fd, msg, sizeof(msg), 0);
				
					//Recv failure
					if(reuse < 0){
						if(errno != EWOULDBLOCK){
							perror("Recv error\n");
							triggerClose = true;
						}
						break;
					}
					
					//connection closed
					if(reuse == 0){
						triggerClose = true;
						break;
					}
					msg[reuse] = '\0';
					//printf("Recv number --> %d", reuse);
					//Getting the HTTP request type and respond accordingly
					if(g_str_has_prefix(msg, "GET")){
						buildHead(resp);
						buildBody(resp, msg, client, false);
					
						conLen = resp->len;
						buildResponse(headerResponse, msg, conLen, client, OK);

						logToFile(msg, client, OK);
					}
					else if(g_str_has_prefix(msg, "POST")){
						buildHead(resp);
						buildBody(resp, msg, client, true);
						conLen = resp->len;
						buildResponse(headerResponse, msg, conLen, client, CREATED);
						logToFile(msg, client, CREATED);			
					}
					else if(g_str_has_prefix(msg, "HEAD")){
						conLen = resp->len;
						buildResponse(headerResponse, msg, conLen, client, OK);
						logToFile(msg, client, OK);
					}
					else{
						conLen = resp->len;
						buildResponse(headerResponse, msg, conLen, client, NOT_IMPLEMENTED);
						logToFile(msg, client, NOT_IMPLEMENTED);
					}

					g_string_append(headerResponse, resp->str);
		
		
					//Get the client IP and port in human readable form
					clientIP = inet_ntoa(client.sin_addr);
					cPort = ntohs(client.sin_port);	
					fprintf(stdout,"Client IP and port %s:%d\n", clientIP, cPort);	
					fflush(stdout);		

					//Sending response to client
					/*if((reuse = write(fds[i].fd, buf, strlen(buf))) < 0){
						perror("Write error\n");
						triggerClose = true;
						break;
					}*/
					if((reuse = write(fds[i].fd, headerResponse->str, headerResponse->len)) < 0){
						perror("Write error\n");
						triggerClose = true;
						break;
					}	
				}
				//Closes all broken/(to be closed) connections 
				if(triggerClose){		
					shutdown(fds[i].fd, SHUT_RDWR);
					close(fds[i].fd);
					fds[i].fd = -1;
					freeNfds = true;
				}
				//Removes finished connections and frees space
				//for more connections in the fd array
				if(freeNfds){
					freeNfds = false;
					for(i = 0; i < nfds; i++){
						if(fds[i].fd == -1){
							for(j = i; j < nfds; j++){
								fds[j].fd = fds[j+1].fd;
							}		
							nfds -= 1;
						}
					}
				}
			}
		}
		g_string_free(resp, 1);
		g_string_free(headerResponse, 1);	
	}
	//Closes finished connections
	for(i = 0; i < nfds; i++){
		if(fds[i].fd >= 0){
			shutdown(fds[i].fd, SHUT_RDWR);
			close(fds[i].fd);
		}
	}
	return 0;
}
//Here we log all requests the server gets to a server.log
void logToFile(char msg[], struct sockaddr_in client, gchar *respondCode){
	
	
	char date[BUFSIZE];
	getIsoDate(date);
	char *clientIP = inet_ntoa(client.sin_addr);
	unsigned short clientPort = ntohs(client.sin_port);
	GString *requestMethod = g_string_new("");
	GString *requestURL = g_string_new("http://");	
	GString *statusCode = g_string_new("");

	gchar **splitter = g_strsplit(msg, "Host: ", -1);
	gchar **sCode = splitter;	
	
	gchar **sCodeSplitter = g_strsplit(sCode[0], " ", -1);
	g_string_append(statusCode, g_strstrip(sCodeSplitter[0]));
	
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
	else if(g_str_has_prefix(msg, "HEAD")){
		g_string_append(requestMethod, "HEAD");
	}
	else{
		g_string_append(requestMethod, statusCode->str);
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
	g_string_free(requestMethod, 1);
	g_string_free(requestURL, 1);
	g_string_free(statusCode, 1);
	g_strfreev(sCode);	
	g_strfreev(sCodeSplitter);	
	//g_strfreev(splitter);	
	g_strfreev(url);
	g_strfreev(urlSplitter);	
}
//Here we create a response to the request
void buildResponse(GString *headerResponse, char msg[], gsize contentLen, struct sockaddr_in client, gchar *respondCode){
	gchar **HTTPsplitter = g_strsplit(msg, " ", -1);
	gchar **getVersion = g_strsplit(HTTPsplitter[2], "\n", -1);
	
//	gchar **getStatus = g_strsplit()


	if(g_str_has_prefix(getVersion[0], "HTTP/1.1")){
		g_string_append(headerResponse, "HTTP/1.1 ");
		g_string_append(headerResponse, respondCode);
		g_string_append(headerResponse, "\r\n");
	}
	else if(g_str_has_prefix(getVersion[0], "HTTP/1.0")){
		g_string_append(headerResponse, "HTTP/1.0 ");
		g_string_append(headerResponse, respondCode);
		g_string_append(headerResponse, "\r\n");
	}
	else{
		g_string_append(headerResponse, "HTTP/1.0 ");
		g_string_append(headerResponse, HTTP_VERSION_NOT_SUPPORTED);
		g_string_append(headerResponse, "\r\n");
		logToFile(msg, client, HTTP_VERSION_NOT_SUPPORTED);
	}
	
	char isodate[BUFSIZE];
	gchar gsizeToUnsigned[10];
	//Write out time in ISO 8601 format!
	getIsoDate(isodate);
	g_string_append(headerResponse, "Content-Type: text/html; charset=utf-8\r\n");
	g_string_append(headerResponse, "Date: ");
	g_string_append(headerResponse, isodate);
	g_string_append(headerResponse, "\r\n");
	if(g_str_has_prefix(getVersion[0], "HTTP/1.1")){
		g_string_append(headerResponse, "Connection: keep-alive\r\n");
		g_string_append(headerResponse, "Keep-Alive: timeout=30, max=100\r\n");
	}
	else{
		g_string_append(headerResponse, "Connection: close\r\n");
	}

	if(!g_str_has_prefix(msg, "HEAD")){
		g_string_append(headerResponse, "Content-Length: ");
		sprintf(gsizeToUnsigned, "%lu", contentLen);
		g_string_append(headerResponse, gsizeToUnsigned);
		g_string_append(headerResponse, "\r\n\r\n");
	}
	else{
		g_string_append(headerResponse, "\r\n");
	}

	g_strfreev(HTTPsplitter);
	g_strfreev(getVersion);
}
//Gets the current date
void getIsoDate(char *buf){
	//Start the clock
	struct tm *timeZone;
	time_t t = time(&t);
	timeZone = localtime(&t);

	if(strftime(buf, BUFSIZE,"%FT%T", timeZone) == 0){
		fprintf(stderr, "Strftime error\n");
		exit(EXIT_FAILURE);
	}
}
//Builds the HTML  head
void buildHead(GString *resp){
	g_string_append(resp, "<!DOCTYPE html>\r\n<html>\r\n");
	g_string_append(resp, "<head>\r\n");
	g_string_append(resp, "<title> GINA IS NOT APACHE</title>\r\n");
	g_string_append(resp, "\n</head>\r\n");
}
//Builds the HTML body
void buildBody(GString *resp, char msg[], struct sockaddr_in client, bool isGoods){

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

	if(isGoods){
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
//Gets message body from POST request
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
//#define METHOD_NOT_ALLOWED "405 Method Not Allowed"
