#include "proxy_parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>
#include <netinet/tcp.h>

#define QUEUE_LENGTH 10
#define RECV_BUFFER_SIZE 2048
#define METHOD_GET "GET"
#define BAD_REQUEST "HTTP/1.0 400 Bad Request\r\n\r\n"
#define NOT_IMPLEMENTED "HTTP/1.0 501 Not Implemented\r\n\r\n"

int handle_conn(int conn_fd);
int send_request(struct ParsedRequest * req);
int get_response(int server_fd, int conn_fd);
int send_all(int fd, const char * buf, size_t len);

/* TODO: proxy()
 * Establish a socket connection to listen for incoming connections.
 * Accept each client request in a new process.
 * Parse header of request and get requested URL.
 * Get data from requested remote server.
 * Send data to the client
 * Return 0 on success, non-zero on failure
*/
int proxy(char *proxy_port) {
  int status, sockfd;
  struct addrinfo hints;
  struct addrinfo *sockaddr_in;

  memset(&hints, 0, sizeof hints); // make sure the struct is empty
  hints.ai_family = AF_INET;       // IPv4
  hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
  hints.ai_flags = AI_PASSIVE;

  if ((status = getaddrinfo(NULL, proxy_port, &hints, &sockaddr_in)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    exit(1);
  }

  // Create socket
  sockfd = socket(sockaddr_in->ai_family, sockaddr_in->ai_socktype, sockaddr_in->ai_protocol);
  if (sockfd == -1) {
    // perror("Socket error");
    freeaddrinfo(sockaddr_in);
    exit(1);
  }

  // Bind to socket
  if (bind(sockfd, sockaddr_in->ai_addr, sockaddr_in->ai_addrlen) == -1) {
    // perror("Bind error");
    close(sockfd); 
    freeaddrinfo(sockaddr_in);
    exit(1);
  }

  // Listen for incoming connections--up to QUEUE_LENGTH can be pending
  if (listen(sockfd, QUEUE_LENGTH) == -1) {
    // perror("Listen error");
    close(sockfd); 
    freeaddrinfo(sockaddr_in);
    exit(1);
  }


  struct sockaddr_storage their_addr;
  socklen_t addr_size;
  addr_size = sizeof their_addr;
  int conn_fd;

  // Continuously accept new connections
  while ((conn_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size)) != -1) {
    pid_t pid = fork();

    if (pid < 0) {
      // perror("Fork failed");
      close(conn_fd);
    } else if (pid == 0) { 
      // in child process
      close(sockfd);
      handle_conn(conn_fd);
      exit(0);
    } else {
      close(conn_fd);
      // kill off completed children
      while (waitpid(-1, NULL, WNOHANG) > 0);
    }
  }
    
  close(sockfd); 
  freeaddrinfo(sockaddr_in);

  return 0;
}

int handle_conn(int conn_fd) {
  // Read in data
  char buf[RECV_BUFFER_SIZE];
  ssize_t bytes_recv;
  int total_bytes = 0;
      
  // Receive data from the established connection
  while ((bytes_recv = recv(conn_fd, buf + total_bytes, sizeof(buf) - total_bytes, 0)) > 0) {
    total_bytes += bytes_recv;
    if ((total_bytes >= 4 && strncmp(buf + total_bytes - 4, "\r\n\r\n", 4) == 0)
        || total_bytes == sizeof(buf)) {
      break;
    }
  }

  // Handle receive errors
  if (bytes_recv == -1) {
    // perror("Receiving error");
    exit(1);
  }

  // Parse the request
  struct ParsedRequest * req = ParsedRequest_create();
  int ret = ParsedRequest_parse(req, buf, total_bytes);

  // Handle bad requests
  if (ret == -1) {
    send_all(conn_fd, BAD_REQUEST, strlen(BAD_REQUEST));
    ParsedRequest_destroy(req);
    return -1;
  }
  if (strcmp(METHOD_GET, req->method) != 0) {
    send_all(conn_fd, NOT_IMPLEMENTED, strlen(NOT_IMPLEMENTED));
    ParsedRequest_destroy(req);
    return -1;
  }
  
  int server_fd = send_request(req);
  if (server_fd < 0) {
    // perror("Request forwarding error");
    ParsedRequest_destroy(req);
    exit(1);
  }

  if (get_response(server_fd, conn_fd) < 0) {
    // perror("Response handling error");
    ParsedRequest_destroy(req);
    exit(1);
  }

  ParsedRequest_destroy(req);

  return 0;
}

int send_request(struct ParsedRequest * req) {
  // Setup connection
  int status, sockfd;
  struct addrinfo hints;
  struct addrinfo *sockaddr_in;

  memset(&hints, 0, sizeof hints); // make sure the struct is empty
  hints.ai_family = AF_INET;       // IPv4
  hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
  hints.ai_flags = 0;

  // Get socket details, default to port 80
  if ((status = getaddrinfo(req->host, req->port ? req->port : "80", &hints, &sockaddr_in)) != 0) {
    // fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    exit(1);
  }

  // Create socket
  sockfd = socket(sockaddr_in->ai_family, sockaddr_in->ai_socktype, sockaddr_in->ai_protocol);
  if (sockfd == -1) {
    // perror("Socket error");
    freeaddrinfo(sockaddr_in);
    exit(1);
  }

  // Connect socket
  if (connect(sockfd, sockaddr_in->ai_addr, sockaddr_in->ai_addrlen) == -1) {
    // perror("Connection error");
    freeaddrinfo(sockaddr_in);
    exit(1);
  }

  if (ParsedHeader_set(req, "Host", req->host) < 0 ||
      ParsedHeader_set(req, "Connection", "close") < 0) {
    // perror("Header set error");
    freeaddrinfo(sockaddr_in);
    exit(1);
  }

  // Send actual request
  size_t line_len = strlen(METHOD_GET) + 1 + strlen(req->path) + strlen(" HTTP/1.0\r\n");
  size_t headers_len = ParsedHeader_headersLen(req);
  size_t req_len = line_len + headers_len;
  char * buf = malloc(req_len + 1);
  snprintf(buf, line_len + 1, "%s %s HTTP/1.0\r\n", METHOD_GET, req->path);

  if (ParsedRequest_unparse_headers(req, buf + line_len, headers_len) != 0) {
    // perror("Buffer creation error");
    free(buf);
    freeaddrinfo(sockaddr_in);
    exit(1);
  }

  // Handle send error
  if (send_all(sockfd, buf, req_len) < 0) {
    // perror("Send error");
    free(buf);
    freeaddrinfo(sockaddr_in);
    exit(1);
  }

  free(buf);
  freeaddrinfo(sockaddr_in);

  return sockfd;
}

int get_response(int server_fd, int conn_fd) {
  // Read in data
  char buf[RECV_BUFFER_SIZE];
  ssize_t bytes_recv;

  // Receive data from the established connection
  while ((bytes_recv = recv(server_fd, buf, sizeof(buf), 0)) > 0) {
    if (send_all(conn_fd, buf, bytes_recv) < 0) {
      // perror("Send error");
      exit(1);
    }
  }

  // Handle receive errors
  if (bytes_recv == -1) {
    // perror("Receiving error");
    close(server_fd);
    exit(1);
  }

  close(server_fd);
  return 0;
}

// handle sending buffer data to specified host
int send_all(int fd, const char * buf, size_t len) {
  size_t total_sent = 0;
  while (total_sent < len) {
    ssize_t bytes_sent = send(fd, buf + total_sent, len - total_sent, 0);
    if (bytes_sent == -1) {
      return -1;
    }
    total_sent += bytes_sent;
  }
  return 0;
}


int main(int argc, char * argv[]) {
  char *proxy_port;

  if (argc != 2) {
    fprintf(stderr, "Usage: ./proxy <port>\n");
    exit(EXIT_FAILURE);
  }

  proxy_port = argv[1];
  return proxy(proxy_port);
}
