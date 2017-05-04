/**
 * @file
 * @author  Murray Fordyce <Murray.Fordyce@gmail.com>
 * @version 1.0
 *
 * @section DESCRIPTION
 * Simple File server over TCP.
 *
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define DEBUG 1
#if DEBUG == 1
	int debug_out;
	#define debug_init() debug_out=dup(1)
	#define fdebug(...) do{fprintf(__VA_ARGS__);}while(0)
	#define debug(...) do{dprintf(debug_out,__VA_ARGS__);}while(0)
#else
	#define debug_init() do{}while(0)
	#define fdebug(...) do{}while(0)
	#define debug(...) do{}while(0)
#endif
#define exit_fail(x) \
	do{                 \
		close(1);          \
		close(2);           \
		close(connSock);     \
		exit(x);              \
	}while(0)

char root[100];

void print_file(char*const path){
	char buffer[1024];
	int fd, count;
	fd = open(path, O_RDONLY);
	if (fd == -1) {
		perror(path);
		exit(EXIT_FAILURE);
	}
	// read from file
	while((count = read(fd, buffer, 1024))!=0){
		if (count == -1) {
			perror(path);
			exit(EXIT_FAILURE);
		}
		debug("%s\n",buffer);
		dprintf(1,"%s\n",buffer);
	}
	close(fd);
}
void print_folder(char*const path){
	//check if index.html exists
	char indexname[100];
	strcat(strcpy(indexname,path),"/index.html");
	if(access(indexname,F_OK)!=-1){
		debug("found index.html\n");
		return print_file(indexname);
	}

	DIR *dirp;
	struct dirent *dirEntry;
	debug("%s\n",path);
	dirp = opendir(path);
	if (dirp == 0) {
		perror(path);
		exit(EXIT_FAILURE);
	}

	while((dirEntry = readdir(dirp))!=NULL){
		debug("%s\n",dirEntry->d_name);
		dprintf(1,"%s\n",dirEntry->d_name);
	}
	closedir(dirp);
}

void processClientRequest(int connSock){
	char path[100];
	debug_init();
	close(1);
	dup(connSock);
	close(2);
	dup(connSock);
	int received;
	char buffer[1024];
	// read a message from the client
	if((received = read(connSock, buffer, sizeof(buffer)))<=0){
		perror("read");
		exit_fail(EXIT_FAILURE);
	}
	debug("Client sent %d bytes: \"%s\"\n",received,buffer);
	if(1024==received){
		debug("Message to long");
		dprintf(2,"Message was over 1024 chars, Ignoring\n");
		exit_fail(EXIT_FAILURE);
	}

	if(strncmp(buffer,"GET ",4)==0){
		debug("is a GET request\n");
		{
			int j=4;
			while(j<1023&&buffer[j]!=' '&&buffer[j]!='\0')
				j++;
			if(1023==j){
				debug("Input overflow\n");
				dprintf(2,"Input overflow\n");
				exit_fail(EXIT_FAILURE);
			}
			if('\0'!=buffer[j]){
				debug("received wrong number of args\n");
				dprintf(2,"Wrong number of arguments\nusage: GET pathname\n");
				exit_fail(EXIT_FAILURE);
			}
		}
		//TODO: turn first ' '  into a '\0' (after 4)
		if(strstr(buffer+4,"..")!=NULL){
			debug("attempted to access ..\n");
			dprintf(2,"Permission denied to access ..");
			exit_fail(EXIT_FAILURE);
		}
		if(buffer[received-2]=='/'){
			strcat(strcpy(path,root),buffer+4);
			print_folder(path);
		}else{
			strcat(strcpy(path,root),buffer+4);
			print_file(path);
		}
	}else if(strncmp(buffer,"INFO",4)==0){
		debug("INFO\n");
		system("date");
	}else{
		int j;
		for(j=0;j<1023&&buffer[j]!=' '&&buffer[j]!='\0';j++);
		buffer[j]='\0';
		dprintf(2,"%s: No such command\n",buffer);
		exit_fail(EXIT_FAILURE);
	}
	close(1);
	close(2);
	close(connSock);
	exit(EXIT_SUCCESS);
}

int main(int argc,char*argv[]){
	debug_init();
	if(argc != 3){
		fprintf(stderr,"USAGE: TCPServerFork port path\n");
		exit(EXIT_FAILURE);
	}
	if(strlen(argv[2])>100){
		fprintf(stderr,"path parameter longer that 100 chars\n");
		exit(EXIT_FAILURE);
	}
	strcat(strcpy(root,argv[2]),"/");

	// Create the TCP socket
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0){
		perror("socket");
		exit(EXIT_FAILURE);
	}
	// create address structures
	struct sockaddr_in server_address;	// structure for address of server
	struct sockaddr_in client_address;	// structure for address of client
	unsigned int addrlen = sizeof(client_address);
	// Construct the server sockaddr_in structure
	memset(&server_address, 0, sizeof(server_address));		/* Clear struct */
	server_address.sin_family = AF_INET;									/* Internet/IP */
	server_address.sin_addr.s_addr = INADDR_ANY;					/* Any IP address */
	server_address.sin_port = htons(atoi(argv[1]));				/* server port */
	// Bind the socket
	if(bind(sock,(struct sockaddr *)&server_address, sizeof(server_address))< 0){
		perror("bind");
		exit(EXIT_FAILURE);
	}
	// listen: make socket passive and set length of queue
	if(listen(sock, 64)< 0){
		perror("listen");
		exit(EXIT_FAILURE);
	}
	debug("TCPServer listening on port: %s\n",argv[1]);
	for(;;){
		int connSock=accept(sock,(struct sockaddr *)&client_address, &addrlen);
		if(connSock < 0){
			perror("accept");
			exit(EXIT_FAILURE);
		}
		if(fork()){
			close(connSock);
		}else{
			processClientRequest(connSock);
			exit(0);
		}
	}
	close(sock);
	return 0;
}
