#include <errno.h>
#include <stdio.h>
#include <stdlib.h>


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
}
