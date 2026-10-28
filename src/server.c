#include <stdio.h>


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
