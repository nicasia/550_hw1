/*
 * client.c
 * Version 20161003
 * Written by Harry Wong (RedAndBlueEraser)
 */

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_NAME_LEN_MAX 255
#define FILE_NAME_LEN_MAX 255

int main(int argc, char *argv[]) {
    char server_name[SERVER_NAME_LEN_MAX + 1] = { 0 };
    int server_port, socket_fd, ind;
    char file_path[FILE_NAME_LEN_MAX + 1] = { 0 };
    struct hostent *server_host;
    struct sockaddr_in server_address;

    /* Get server name from command line arguments or stdin. */
    if (argc > 1) {
        strncpy(server_name, argv[1], SERVER_NAME_LEN_MAX);
    } else {
        printf("Enter Server Name: ");
        scanf("%s", server_name);
    }

    /* Get server port from command line arguments or stdin. */
    server_port = argc > 2 ? atoi(argv[2]) : 0;
    if (!server_port) {
        printf("Enter Port: ");
        scanf("%d", &server_port);
    }

    
    if (argc > 3) {
        strncpy(file_path, argv[3], FILE_NAME_LEN_MAX);
    } else {
        printf("Enter File Name: ");
        scanf("%s", file_path);
    }

/* Get server port from command line arguments or stdin. */
    ind = argc > 4 ? atoi(argv[4]) : 0;
    if (!ind) {
        printf("Enter wait stat: ");
        scanf("%d", &ind);
    } 

    /* Get server host from server name. */
    server_host = gethostbyname(server_name);

    /* Initialise IPv4 server address with server host. */
    memset(&server_address, 0, sizeof server_address);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    memcpy(&server_address.sin_addr.s_addr, server_host->h_addr, server_host->h_length);

    /* Create TCP socket. */
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    /* Connect to socket with server address. */
    if (connect(socket_fd, (struct sockaddr *)&server_address, sizeof server_address) == -1) {
		perror("connect");
        exit(1);
	}
    
    //char buff[2000];
    //int byte_count;
    //byte_count = recv(socket_fd, buff, sizeof buff, 0);
    //puts("Received msg from server");
    //puts(buff);

    if(ind == 1){
	sleep(10);
    }
    puts("SENDING filename...\n");
    puts(file_path);
    if (send(socket_fd, "a", sizeof file_path, 0) == -1) {
        perror("sending");
        exit(1);
    }
if (send(socket_fd, ".txt", sizeof file_path, 0) == -1) {
        perror("sending");
        exit(1);
    }

    
    int byte_count;
    char buff2[2000];
    byte_count = recv(socket_fd, buff2, sizeof buff2, 0);
    puts("Received msg from server");
    puts(buff2);

  
    sleep(2);
    puts("done sleeping");

    /* TODO: Put server interaction code here. For example, use
     * write(socket_fd,,) and read(socket_fd,,) to send and receive messages
     * with the client.
     */

    close(socket_fd);
    return 0;
}
