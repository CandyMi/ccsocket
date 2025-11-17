#include "ccsocket.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

int main(int argc, char const *argv[])
{
  //SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

  ccsocket_t sock = ccsocket(CC_INET4, CC_TCP);
  //ccsocket_t sock = ccsocket1(CC_INET4, CC_TCP, CC_NONBLOCK);
  printf("new fd = %ld\n", sock);

  int rc = ccsocket_connect(sock, "120.24.216.230", 80);
  // int rc = ccsocket_connect(sock, "127.0.0.1", 7888);
  printf("connect r = %d\n", rc); perror("conn: ");

  char req[] = "GET / HTTP/1.1\r\nHost: shangpiaoyi.cn\r\n\r\n";
  int rs = ccsocket_send(sock, req, strlen(req));
  printf("send r = %d\n", rs);

  char buf[1024]; memset(buf, 0x0, 1024);
  int rr = ccsocket_recv(sock, buf, 1024);
  printf("recv r = %d, ['%s']\n", rr, buf);

  printf("close fd = %d\n", ccsocket_close(sock));

  return 0;
}
