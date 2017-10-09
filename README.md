# HTTPdP1

# ABOUT

This project is a simple HTTP server, programmed in C, done as an assignment(PA2) for the Computer Networking course in the Reykjavík University.
The source of the program itself is inside the directory src, this was done so because when the server runs and recieves request, it will log the request in the logfile server.log inside the src directory.

The log file will consist of a single line for every request in the following format:
	"timestamp : client IP:client port request method requested URL : response code"
Where the timestamp is in ISO 8601 format, precise up to seconds, "client IP" and "client port" are the IP address and port number of the client which sent the request, "request method" will consist of the method type sent by the request(GET or POST or HEAD), "requested URL" is the URL of the response and "response code" is the response code sent to the client by the server:
		- 200 OK, for a successful GET or HEAD request
		- 201 Created, for a successful POST request
		- 405 Method not allowed, for a unsupported request method
		- 505 HTTP Version not Supported, for a request of  

# IMPLEMENTATION

We implemented the server to allow 3 types of request from clients: GET, POST and HEAD.
The server can handle up to x parallel connections, which can be persistent and kept-alive if the client request the connection as HTTP/1.1, but will time out after after 30 seconds of inactivity, whereas the connection will be closed after each response from the server if the client requests a HTTP/1.0 connection, for all other types of HTTP version requests and will result in a 505 error(HTTP Version not supported).
# GET
	In the case of a GET request, the server will generate and build a HTML5 page which it stores in memory and also generate a header for the requested page.
	The content of the HTML5 page will include the URL of the requested page and the IP address and port number of the client which the browser will then display:
		http://127.0.0.1/page 123.123.123.123:59514

# HEAD
	In the case of a HEAD request, the server will only generate the header for the requested page.

# POST
	In the case of a POST request, the server will also generate and build a HTML5 page which it stores in memory and a header for the requested page.
	The content of the HTML5 page will include the URL of the requested page, the IP address and portnumber of the requesting client and also the data sent via the clients POST request in the body of HTML5 request.

In the case of any other request of the server, which will be unsupported, the server will send the appropriate error: 405 Method Not Allowed.

# BUILDING AND RUNNING

The build the program, the following command will need to be run from the source of the program:
	"make -C ./src"
To run the server, the following command will ned to be run from the source of the program:
	"./src/httpd PORT"
Whereas PORT will include the port the server will run on, for example:	
	"./src/httpd 59513"

# SENDING REQUEST TO SERVER

To make send a request to the server as a client the following methods can be done:

# Web browser
	"localhost:PORT"
	"localhost:PORT/example"
Whereas PORT will include the port the server is running on.

# Curl
	"curl -v localhost:PORT", for a GET request
	"curl -v -I localhost:PORT", for a HEAD request
	"curl -v -d "some data" localhost:PORT", for a POST request
	"curl -X DELETE localhost:PORT", for an example of a unsupported method(delete method)
Whereas PORT will include the port the server is running on.

# TESTING

We tested the parallel connection feature of our server using a script, 
we found the said script here:
	http://marianlonga.com/simulate-parallel-connections-to-a-website/
This greatly helped us measure the responsiveness of our server if there are multiple users requesting the server at the same time.
To use this script on our server, you need to create a file(script.sh), where you place the following code:

	#!/bin/bash
	 
	# Parallel Connection Simulator
	# ($1) 1st parameter = website
	# ($2) 2nd parameter = number of simultaneous requests
	 
	initial_time=$(date +%s)
	 
	for ((i=1; i<=$2; i++))
	do
	  curl -s -o /dev/null $1 && echo "done $i" &
	done
	 
	wait
	 
	final_time=$(date +%s)
	time_elapsed=$(($final_time-$initial_time))
	 
	echo $2 "requests processed in" $time_elapsed "s"

Then you need to open a terminal and navigate to the location of the script, execute:
	"chmod -x script.sh"
To make the script executable	
and finally execute the script it like so:
	"./script.sh 127.0.0.1:PORT number_of_connections"
Whereas PORT will include the port the server is running on and number_of_connections includes how many users you want to send a request to the server at the same time.
