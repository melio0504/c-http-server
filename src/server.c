#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>


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
  static const char response[] =
      "HTTP/1.0 200 OK\r\n"
      "content-type: text/html; charset=utf-8\r\n"
      "content-length: 46\r\n"
      "connection: close\r\n"
      "\r\n"
      "<!doctype html><html><body>hello</body></html>";
  struct sockaddr_in address;
  unsigned short port;
  int server_fd;
  int reuse_address = 1;

  if (argc != 2 || parse_port(argv[1], &port) != 0) {
    fprintf(stderr, "usage: %s <port from 1 to 65535>\n", argv[0]);
    return EXIT_FAILURE;
  }

  /* ignoring sigpipe keeps a disconnected client from terminating the server.
   */
  if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
    perror("signal");
    return EXIT_FAILURE;
  }

}
