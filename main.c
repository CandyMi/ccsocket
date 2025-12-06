#include "ccsocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#if WIN32
  #include <Windows.h>
#else
  #include <unistd.h>
#endif

#define ccsocket_dump_error(sock, prefix)   \
{                                           \
  char errinfo[MAX_ERRORLEN];               \
  ccsocket_get_error(sock, errinfo);        \
  printf(prefix ": %s\n", errinfo);         \
}

void test_client_socket(ccsocket_t sock)
{
  int rc = ccsocket_connect(sock, "120.24.216.230", 80);
  // int rc = ccsocket_connect(sock, "127.0.0.1", 7888);
  ccsocket_dump_error(sock, "ccsocket_connect");
  while (1) {
    int r = ccsocket_is_connected(sock);
    // printf("connect state r = %d\n", r);
    if (r == CC_CONNERROR) {
      ccsocket_dump_error(sock, "CC_CONNERROR");
      exit(1);
    }
    if (r == CC_CONNECTED) {
      ccsocket_dump_error(sock, "CC_CONNECTED");
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
  printf("send r = %d\n", rs);
  if (rs == -1) {
    ccsocket_dump_error(sock, "ccsocket_send");
    exit(1);
  }

#if WIN32
    Sleep(1000);
#else
    sleep(1);
#endif

char buf[1024]; memset(buf, 0x0, 1024);
  int rr = ccsocket_recv(sock, buf, 1024);
  printf("recv r = %d, ['%s']\n", rr, buf);
  if (rr == -1) {
    ccsocket_dump_error(sock, "ccsocket_recv");
    exit(1);
  }

  char addr[MAX_ADDRLEN]; uint16_t port;
  int r = ccsocket_get_sockname(sock, addr, &port);
  printf("ccsocket_get_sockname = {'%s', %d}, r = %d\n", addr, port, r);
}

void test_server_socket(ccsocket_t sock) {

  if (!ccsocket_listen(sock, "0.0.0.0", 7888)) {
    ccsocket_dump_error(sock, "ccsocket_listen");
    exit(1);
  }

  char addr[MAX_ADDRLEN]; uint16_t port;
  ccsocket_t csock = ccsocket_accept1(sock, addr, &port, CC_NOFLAG);
  if (csock == INVALID_SOCKET) {
      ccsocket_dump_error(sock, "ccsocket_accept");
      exit(1);
  }
  printf("sock{ip = '%s', port = %d}\n", addr, port);
  
  ccsocket_send(csock, "hello\r\n", 7);
  ccsocket_close(csock);
}

int main(int argc, char const *argv[])
{
  //SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

  ccsocket_t sock = ccsocket(CC_INET4, CC_TCP);
  // ccsocket_t sock = ccsocket1(CC_INET4, CC_TCP, CC_NONBLOCK);
  printf("new fd = %ld\n", (long)sock);

  // test_client_socket(sock);
  test_server_socket(sock);

  printf("close fd = %d\n", ccsocket_close(sock));

  return 0;
}
