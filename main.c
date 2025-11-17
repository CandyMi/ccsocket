#include "ccsocket.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#if WIN32
  #include <Windows.h>
#else
  #include <unistd.h>
#endif
int main(int argc, char const *argv[])
{
  //SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

  // ccsocket_t sock = ccsocket(CC_INET4, CC_TCP);
  ccsocket_t sock = ccsocket1(CC_INET4, CC_TCP, CC_NONBLOCK);
  printf("new fd = %ld\n", sock);

  int rc = ccsocket_connect(sock, "120.24.216.230", 80);
  // int rc = ccsocket_connect(sock, "127.0.0.1", 7888);
  printf("connect r = %d\n", rc); perror("conn");
  while (1) {
    int r = ccsocket_is_connected(sock);
    printf("connect state r = %d\n", r);
    if (r == CC_CONNERROR) {
      perror("connect error: ");
      return 0;
    }
    if (r == CC_CONNECTED) {
      perror("connected error: ");
      break;
    }
#if WIN32
    Sleep(1);
#else
    usleep(1000);
#endif
  }

  char req[] = "GET / HTTP/1.1\r\nHost: shangpiaoyi.cn\r\n\r\n";
  int rs = ccsocket_send(sock, req, strlen(req));
  printf("send r = %d\n", rs); if (rs == -1) { perror("send error: "); return 0; }

#if WIN32
    Sleep(1000);
#else
    sleep(1);
#endif

char buf[1024]; memset(buf, 0x0, 1024);
  int rr = ccsocket_recv(sock, buf, 1024);
  printf("recv r = %d, ['%s']\n", rr, buf); if (rr == -1) { perror("recv error: "); return 0; }

  char addr[MAX_ADDRLEN]; int port;
  int r = ccsocket_get_sockname(sock, addr, &port);
  printf("client = {'%s', %d}, r = %d", addr, port, r);

  printf("close fd = %d\n", ccsocket_close(sock));

  return 0;
}
