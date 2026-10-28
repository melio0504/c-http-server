#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int send_all(int socket_fd, const char *response,
                    size_t response_length) {
  size_t bytes_sent = 0;

  // send can write only part of the response
  // so we keep going until it is complete
  while (bytes_sent < response_length) {
    ssize_t result =
        send(socket_fd, response + bytes_sent, response_length - bytes_sent, 0);

    if (result < 0) {
      if (errno == EINTR) {
        continue;
      }

      return -1;
    }

    if (result == 0) {
      errno = EPIPE;
      return -1;
    }

    bytes_sent += (size_t)result;
  }

  return 0;
}

static int request_is_complete(const char *request, size_t request_length) {
  size_t index;

  for (index = 0; index < request_length; ++index) {
    if (index + 3 < request_length && request[index] == '\r' &&
        request[index + 1] == '\n' && request[index + 2] == '\r' &&
        request[index + 3] == '\n') {
      return 1;
    }

    if (index + 1 < request_length && request[index] == '\n' &&
        request[index + 1] == '\n') {
      return 1;
    }
  }

  return 0;
}

static ssize_t receive_request(int socket_fd, char *request,
                               size_t request_capacity) {
  size_t bytes_received = 0;

  // receive enough data to log the request before sending the response.
  while (bytes_received < request_capacity - 1) {
    ssize_t result = recv(socket_fd, request + bytes_received,
                          request_capacity - bytes_received - 1, 0);

    if (result < 0) {
      if (errno == EINTR) {
        continue;
      }

      return -1;
    }

    if (result == 0) {
      break;
    }

    bytes_received += (size_t)result;
    if (request_is_complete(request, bytes_received)) {
      break;
    }
  }

  request[bytes_received] = '\0';
  return (ssize_t)bytes_received;
}

static void log_request_line(const char *request, size_t request_length) {
  size_t index;

  fputs("request: ", stdout);

  if (request_length == 0) {
    fputs("(no data)", stdout);
  }

  for (index = 0; index < request_length; ++index) {
    unsigned char character = (unsigned char)request[index];

    if (character == '\r' || character == '\n') {
      break;
    }

    // replacing control characters keeps untrusted request data safe to log.
    putchar(character >= 32 && character <= 126 ? character : '?');
  }

  putchar('\n');
}

static int parse_port(const char *text, unsigned short *port) {
  char *end = NULL;
  long value;

  errno = 0;
  value = strtol(text, &end, 10);

  if (errno != 0 || end == text || *end != '\0' || value < 1 || value > 65535) {
    return -1;
  }

  *port = (unsigned short)value;
  return 0;
}

int main(int argc, char **argv) {
  static const char body[] =
      "<!doctype html><html><body>Hello! My name is Romelio. This has been "
      "transferred through HTTP/1.0 that was published in 1996! How cool is "
      "that!?</body></html>";
  char response[512];
  struct sockaddr_in address;
  unsigned short port;
  int server_fd;
  int reuse_address = 1;
  int formatted_length;
  size_t response_length;

  if (argc != 2 || parse_port(argv[1], &port) != 0) {
    fprintf(stderr, "usage: %s <port from 1 to 65535>\n", argv[0]);
    return EXIT_FAILURE;
  }

  // calculating content-length keeps the header correct when the body changes.
  formatted_length = snprintf(response, sizeof(response),
                              "HTTP/1.0 200 OK\r\n"
                              "content-type: text/html; charset=utf-8\r\n"
                              "content-length: %zu\r\n"
                              "connection: close\r\n"
                              "\r\n"
                              "%s",
                              sizeof(body) - 1, body);
  if (formatted_length < 0 || (size_t)formatted_length >= sizeof(response)) {
    fputs("response is too large\n", stderr);
    return EXIT_FAILURE;
  }
  response_length = (size_t)formatted_length;

  // ignoring sigpipe keeps a disconnected client from terminating the server.
  if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
    perror("signal");
    return EXIT_FAILURE;
  }

  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("socket");
    return EXIT_FAILURE;
  }

  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_address,
                 sizeof(reuse_address)) < 0) {
    perror("setsockopt");
    close(server_fd);
    return EXIT_FAILURE;
  }

  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(port);

  // bind assigns the requested local port to this listening socket.
  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind");
    close(server_fd);
    return EXIT_FAILURE;
  }

  if (listen(server_fd, 16) < 0) {
    perror("listen");
    close(server_fd);
    return EXIT_FAILURE;
  }

  printf("server listening on port %hu\n", port);
  fflush(stdout);

  for (;;) {
    char client_ip[INET_ADDRSTRLEN];
    char request[4096];
    struct sockaddr_in client_address;
    socklen_t client_address_length = sizeof(client_address);
    int client_fd;
    ssize_t request_length;

    // accept blocks until a client connects and returns its connected socket.
    client_fd = accept(server_fd, (struct sockaddr *)&client_address,
                       &client_address_length);
    if (client_fd < 0) {
      if (errno == EINTR) {
        continue;
      }

      perror("accept");
      close(server_fd);
      return EXIT_FAILURE;
    }

    if (inet_ntop(AF_INET, &client_address.sin_addr, client_ip,
                  sizeof(client_ip)) == NULL) {
      strcpy(client_ip, "unknown");
    }

    printf("connection from %s:%hu\n", client_ip,
           ntohs(client_address.sin_port));

    request_length = receive_request(client_fd, request, sizeof(request));
    if (request_length < 0) {
      perror("receive");
      request_length = 0;
    }

    log_request_line(request, (size_t)request_length);
    printf("response: http/1.0 200 ok (%zu bytes)\n", response_length);
    fflush(stdout);

    if (send_all(client_fd, response, response_length) < 0) {
      perror("send");
    }

    // closing the client socket finishes this http/1.0 response.
    close(client_fd);
    puts("connection closed");
    fflush(stdout);
  }
}

// TODO:
//
// 1. Parse HTTP Requests
//
// - Validate the request format and HTTP method.
// - Return 400 Bad Request for improperly formatted requests.
// - Return 501 Not Implemented for valid requests using other methods
//
// 2. Serve Files
//
// - Parse the requested URL path from valid GET requests.
// - Map the URL path to a file inside the root directory.
// - Serve index.html when the requested path is `/`.
// - Return 404 Not Found when the requested file does not exist.
// - Support both text files and binary files, including images.
//
// 3. Support Directories and Nested Paths
//
// - Create multiple subdirectories inside the root directory.
// - Add dummy HTML files and `index.html` files to subdirectories.
// - Create several levels of nested directories.
// - Update path parsing to handle nested folder paths correctly.
//
// 4. Handle File Permissions
//
// - Modify permissions on selected files and directories to deny read or
// access permissions.
// - Detect when the server cannot access or read a requested resource.
// - Return 403 Forbidden when access is denied.
//
// 5. Improve URI and Path Security
//
// - Review URI syntax and identify characters requiring special handling.
// - Parse and normalize URL paths safely.
// - Handle URL-encoded characters appropriately.
// - Ignore query strings (`?`) when resolving the requested file.
// - Ignore fragments (`#`) when resolving the requested file.
// - Prevent directory traversal attacks using `..`.
// - Ensure the resolved file path remains within the configured root directory.
// - Handle malformed or suspicious paths safely.
