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

#define SEND_BUFFER_SIZE 2048


/* TODO: client()
 * Open socket and send message from stdin.
 * Return 0 on success, non-zero on failure
*/
int client(char *server_ip, char *server_port) {
    int status, sockfd;
    struct addrinfo hints;
    struct addrinfo *sockaddr_in;

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = 0;

    if ((status = getaddrinfo(server_ip, server_port, &hints, &sockaddr_in)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        exit(1);
    }

    // Create socket
    sockfd = socket(sockaddr_in->ai_family, sockaddr_in->ai_socktype, sockaddr_in->ai_protocol);
    if (sockfd == -1) {
        perror("Socket error");
        exit(1);
    }

    // Connect socket
    if (connect(sockfd, sockaddr_in->ai_addr, sockaddr_in->ai_addrlen) == -1) {
      perror("Connection error");
      exit(1);
    }

    printf("Input: ");

    // Continually read in data to buffer until EOF
    char buf[SEND_BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buf, 1, sizeof(buf), stdin)) > 0) {
      char * shift = buf;
      ssize_t bytes_sent;
      ssize_t total_sent = 0;

      // Send in chunks until entire message is sent
      while (total_sent < bytes_read && (bytes_sent = send(sockfd, shift, bytes_read - total_sent, 0)) > 0) {
        shift += bytes_sent;
        total_sent += bytes_sent;
      }

      // Handle send error
      if (bytes_sent == -1) {
        perror("Send error");
        exit(1);
      }
    }

    close(sockfd); 
    free(sockaddr_in);

    return 0;
}

/*
 * main()
 * Parse command-line arguments and call client function
*/
int main(int argc, char **argv) {
  char *server_ip;
  char *server_port;

  if (argc != 3) {
    fprintf(stderr, "Usage: ./client-c [server IP] [server port] < [message]\n");
    exit(EXIT_FAILURE);
  }

  server_ip = argv[1];
  server_port = argv[2];
  return client(server_ip, server_port);
}
