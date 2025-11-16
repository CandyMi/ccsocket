#include "ccsocket.h"
#include <stdio.h>
#include <errno.h>

int main(int argc, char const *argv[])
{
  // ccsocket_t sock = ccsocket(CC_INET4, CC_TCP);
  ccsocket_t sock = ccsocket1(CC_INET4, CC_TCP, CC_NONBLOCK);
  printf("new fd = %d\n", sock);

  int rc = ccsocket_connect(sock, "120.24.216.230", 80);
  // int rc = ccsocket_connect(sock, "127.0.0.1", 7888);
  printf("connect r = %d\n", rc); perror("conn: ");

  printf("close fd = %d\n", ccsocket_close(sock));
  return 0;
}
