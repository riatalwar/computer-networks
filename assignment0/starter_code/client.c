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
  /*
  socket()
  bind() to specified port
  connect()
  read from stdin--will require looping to ensure that all input is read (literally why r we using c making this more difficult for no reason)
  send() msg including the stdin stream
  exit after msg is successfully sent
  iterative read/send flow instead of reading everything and sending everything in two large batches
  client should andle partial sends: socket only transmits parts of data, need to resend remaining until complete

  failure at any of these points should result in a nonzero err code
  graceful err handling of socket fns
  */

    int status, sockfd;
    struct addrinfo hints;
    struct addrinfo *sockaddr_in;

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_INET;     // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = 0;

    if ((status = getaddrinfo(server_ip, server_port, &hints, &sockaddr_in)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        exit(1);
    }

    sockfd = socket(sockaddr_in->ai_family, sockaddr_in->ai_socktype, sockaddr_in->ai_protocol);
    if (sockfd == -1) {
        // err creating socket
        perror("Socket error");
        exit(1);
    }

    // don't need to call bind() as we only care about target port
    if (connect(sockfd, sockaddr_in->ai_addr, sockaddr_in->ai_addrlen) == -1) {
      // err connecting to server
      perror("Connection error");
      exit(1);
    }

    /*
    read/send loop
    int send(int sockfd, const void *msg, int len, int flags); 

    */
    printf("Input: ");

    char buf[SEND_BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buf, 1, sizeof(buf), stdin)) > 0) {
      char * shift = buf;
      ssize_t bytes_sent;
      ssize_t total_sent = 0;

      while (total_sent < bytes_read && (bytes_sent = send(sockfd, shift, bytes_read - total_sent, 0)) > 0) {
        shift += bytes_sent;
        total_sent += bytes_sent;
      }
      if (bytes_sent == -1) exit(1);
    }

    close(sockfd); 
    free(sockaddr_in);
    // make sure all data is freed here -- check for mem leaks

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
