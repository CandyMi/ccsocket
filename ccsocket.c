#include "ccsocket.h"

#include <errno.h>
// #include <stdio.h>
#include <string.h>

#if WIN32
  #include <windows.h>
  #include <winsock2.h>
  #include <mstcpip.h>
  #include <WS2tcpip.h>
  #pragma comment(lib, "Ws2_32.lib")
  BOOL WINAPI DllMain(
    _In_ HINSTANCE hinstDLL,
    _In_ DWORD     fdwReason,
    _In_ LPVOID    lpvReserved
  ) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
      WSADATA wsaData; // 用于初始化套接字
      if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        exit(1);
      }
    }
    return true;
  }
  #define cc_fd2socket(fd)  (SOCKET)_get_osfhandle(fd)
  #define cc_socket2fd(sock) _open_osfhandle(sock)
#else
  #include <fcntl.h>
  #include <unistd.h>
  #include <sys/un.h>
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <sys/socket.h>
  #define cc_fd2socket(fd)   (fd)
  #define cc_socket2fd(sock) (sock)
#endif

typedef enum {
  CCSERVER = 1,
  CCCLIENT = 2,
} cc_socket_t;

static inline
int ccsizeof(const struct sockaddr_storage *sa)
{
  switch (sa->ss_family)
  {
    case AF_LOCAL:
      return sizeof (struct sockaddr_un);
    case AF_INET:
      return sizeof (struct sockaddr_in);
    case AF_INET6:
      return sizeof (struct sockaddr_in6);
  }
  return 0;
}

static inline
int ccsocket_wrap_ip_and_port(ccsocket_t s, struct sockaddr_storage *sa, const char ip[], uint16_t port)
{
#if WIN32
  // TODO
#else
  socklen_t addrlen = sizeof(*sa); memset(sa, 0x0, sizeof(*sa));
  int r = getsockname(s, (struct sockaddr*)sa, &addrlen);
  if (r)
    return r;
  switch (sa->ss_family)
  {
    case AF_LOCAL:
    {
      struct sockaddr_un* in = (struct sockaddr_un*)sa;
      memcpy(in->sun_path, ip, strlen(ip));
      break;
    }
    case AF_INET:
    {
      struct sockaddr_in* in = (struct sockaddr_in*)sa;
      in->sin_port = htons(port);
      if (1 != inet_pton(AF_INET, ip, &in->sin_addr))
        return -1;
      break;
    }
    case AF_INET6:
    {
      struct sockaddr_in6* in = (struct sockaddr_in6*)sa;
      in->sin6_port = htons(port);
      if (1 != inet_pton(AF_INET6, ip, &in->sin6_addr))
        return -1;
      break;
    }
  }
  return 0;
#endif
}

int ccsocket_close(ccsocket_t s)
{
#if WIN32
  SOCKET ss = (SOCKET)_get_osfhandle(s);
  if (ss == INVALID_HANDLE_VALUE)
    return -1;
  _close(s); closesocket(ss);
  return 0;
#else
  return close(s);
#endif
}

/* 创建 ccsocket */
ccsocket_t ccsocket(ccsocket_domain_t domain, ccsocket_protocol_t proto)
{
#if WIN32
  // TODO
#else
  return ccsocket1(domain, proto, 0);
#endif
}

/* 创建 ccsocket 顺便设置标记 */
ccsocket_t ccsocket1(ccsocket_domain_t domain, ccsocket_protocol_t proto, ccsocket_flags_t flags)
{
#if WIN32
  // TODO
#else
  int domain_r = AF_LOCAL; 
  if (domain == CC_INET4) domain_r = AF_INET;
  if (domain == CC_INET6) domain_r = AF_INET6;

  int proto_r = SOCK_RAW;
  if (proto == CC_TCP) proto_r = SOCK_STREAM;
  if (proto == CC_UDP) proto_r = SOCK_DGRAM;

  bool isset = false;
  if (flags & CC_NONBLOCK) {
#if defined(SOCK_NONBLOCK) // 避免子进程继承
    isset = true;
    proto_r |= SOCK_NONBLOCK;
#endif
  }
  if (flags & CC_CLOEXEC) {
#if defined(SOCK_CLOEXEC) // 直接
    isset = true;
    proto_r |= SOCK_CLOEXEC;
#endif
  }
  // 创建
  ccsocket_t s = socket(domain_r, proto_r, 0);
  if (s == INVALID_SOCKET)
    return INVALID_SOCKET;
  /**
   * 如果之前没有设置, 则再这里完成.
   * 但是在多线程环境下这不能绝对保证.
   */
  if (!isset && flags) {
    int r;
    if (flags & CC_CLOEXEC)
      r = fcntl(s, F_SETFD, FD_CLOEXEC | fcntl(s, F_GETFD));
    if (flags & CC_NONBLOCK)
      r = fcntl(s, F_SETFL, O_NONBLOCK | fcntl(s, F_GETFL));
    if (r == -1) {
      close(s); s = INVALID_SOCKET;
    }
  }
  return s;
#endif
}

/* 监听 ccsocket */
bool ccsocket_listen(ccsocket_t s, const char ip[], uint16_t port)
{
#if WIN32
  // TODO
#else
  errno = 0; int r;
  struct sockaddr_storage sa; memset(&sa, 0x0, sizeof(sa));
  r = ccsocket_wrap_ip_and_port(cc_fd2socket(s), &sa, ip, port);
  if (r)
    return false;

  r = bind(cc_fd2socket(s), (const struct sockaddr *)&sa, ccsizeof(&sa));
  if (r < 0)
    return false;
  r = listen(s, SOMAXCONN);
  if (r < 0)
    return false;
  return true;
#endif
}

/* 连接 ccsocket */
bool ccsocket_connect(ccsocket_t s, const char ip[], uint16_t port)
{
#if WIN32
  // TODO
#else
  errno = 0; int r;
  struct sockaddr_storage sa;
  r = ccsocket_wrap_ip_and_port(cc_fd2socket(s), &sa, ip, port);
  if (r)
    return false;
  r = connect(cc_fd2socket(s), (const struct sockaddr *)&sa, ccsizeof(&sa));
  // printf("r = %d, errno = %d\n", r, errno);
  if (r < 0)
    return false;
  return true;
#endif
}

/* 发送 ccsocket */
int ccsocket_send(ccsocket_t s, const void *buf, size_t bsize)
{
  return send(cc_fd2socket(s), buf, bsize, 0);
}

/* 接收 ccsocket */
int ccsocket_recv(ccsocket_t s, char *buf, size_t bsize)
{
  return recv(cc_fd2socket(s), buf, bsize, 0);
}

/* 开启/关闭 nodelay */
bool ccsocket_set_nodelay(ccsocket_t s, bool on)
{
  int Enable = on;
  if(setsockopt(cc_fd2socket(s), IPPROTO_TCP, TCP_NODELAY, (char*)&Enable, sizeof(Enable))) {
    return false;
  }
  return true;
}