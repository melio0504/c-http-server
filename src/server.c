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

    bytes_sent += (size_t)result;
  }

  return 0;
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

  for (;;) {
    int client_fd;

    // accept blocks until a client connects and returns its connected socket.
    if (client_fd < 0) {
      if (errno == EINTR) {
        continue;
      }

      perror("accept");
      close(server_fd);
      return EXIT_FAILURE;
    }

    if (send_all(client_fd, response, sizeof(response) - 1) < 0) {
      perror("send");
    }

    // closing the client socket finishes this http/1.0 response.
    close(client_fd);
  }
}
