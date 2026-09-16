#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>

#define QUEUE_LENGTH 10
#define RECV_BUFFER_SIZE 2048

/* TODO: server()
 * Open socket and wait for client to connect
 * Print received message to stdout
 * Return 0 on success, non-zero on failure
*/
int server(char *server_port) {
    int status, sockfd;
    struct addrinfo hints;
    struct addrinfo *sockaddr_in;

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(NULL, server_port, &hints, &sockaddr_in)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        exit(1);
    }

    // Create socket
    sockfd = socket(sockaddr_in->ai_family, sockaddr_in->ai_socktype, sockaddr_in->ai_protocol);
    if (sockfd == -1) {
        perror("Socket error");
        exit(1);
    }

    // Bind to socket
    if (bind(sockfd, sockaddr_in->ai_addr, sockaddr_in->ai_addrlen) == -1) {
      perror("Bind error");
      exit(1);
    }

    // Listen for incoming connections--up to QUEUE_LENGTH can be pending
    if (listen(sockfd, QUEUE_LENGTH) == -1) {
      perror("Listen error");
      exit(1);
    }

    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    addr_size = sizeof their_addr;
    int conn_fd;

    // Continuously accept new connections
    while ((conn_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size)) != -1) {
      char buf[RECV_BUFFER_SIZE];
      ssize_t bytes_recv;
      
      // Receive data from the established connection
      while ((bytes_recv = recv(conn_fd, buf, sizeof(buf), 0)) > 0) {
        // Ensure data is pushed to stdout
        fwrite(buf, sizeof(char), bytes_recv, stdout);
        fflush(stdout);
      }

      // Handle receive errors
      if (bytes_recv == -1) {
        perror("Receiving error");
        exit(1);
      }
    }
    
    // Handle accept error
    if (conn_fd == -1) {
      perror("Accept connection error");
      exit(1);
    }

    close(sockfd); 
    freeaddrinfo(sockaddr_in);

    return 0;
}

/*
 * main():
 * Parse command-line arguments and call server function
*/
int main(int argc, char **argv) {
  char *server_port;

  if (argc != 2) {
    fprintf(stderr, "Usage: ./server-c [server port]\n");
    exit(EXIT_FAILURE);
  }

  server_port = argv[1];
  return server(server_port);
}
