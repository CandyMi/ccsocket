#ifndef _GNU_SOURCE
  #define _GNU_SOURCE
#endif

#include "ccsocket.h"

#include <errno.h>
// #include <stdio.h>
#include <string.h>

#if WIN32
  #include <winsock2.h>
  #include <mstcpip.h>
  #include <WS2tcpip.h>
  #if defined(AF_UNIX)
    #include <afunix.h>
  #endif
  #include <Windows.h>
  #pragma comment(lib, "Ws2_32.lib")
  BOOL WINAPI DllMain(
      _In_ HINSTANCE hinstDLL,
      _In_ DWORD     fdwReason,
      _In_ LPVOID    lpvReserved
  ) {
      if (fdwReason == DLL_PROCESS_ATTACH) {
          //printf("init.\n");
          WSADATA wsaData; // 用于初始化套接字
          if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
              WSACleanup();
              exit(1);
          }
      }
      return true;
  }
#else
  #include <fcntl.h>
  #include <unistd.h>
  #include <sys/un.h>
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <sys/socket.h>
#if defined(__linux__) || defined(__sun__)
  #include <sys/sendfile.h>
#endif
  #define closesocket(s) close(s)
  typedef int SOCKET;
  #define SOCKET_ERROR (~0)
#endif

typedef enum {
  CCSERVER = 1,
  CCCLIENT = 2,
} cc_socket_t;

static inline
int ccsizeof(const struct sockaddr_storage* sa)
{
  switch ((int)sa->ss_family)
  {
#if defined(AF_UNIX)
    case AF_UNIX:
      return sizeof(struct sockaddr_un);
#endif
    case AF_INET:
      return sizeof(struct sockaddr_in);
    case AF_INET6:
      return sizeof(struct sockaddr_in6);
  }
  return 0;
}

static inline
bool ccsocket2addr(const struct sockaddr_storage* sa, char addr[MAX_ADDRLEN], uint16_t *port)
{
  switch ((int)sa->ss_family)
  {
#if defined(AF_UNIX)
    case AF_UNIX:
    {
      const struct sockaddr_un* in = (const struct sockaddr_un*)sa;
      memcpy(addr, in->sun_path, strlen(in->sun_path));
      *port = 0;
      break;
    }
      // return false;
#endif
    case AF_INET:
    {
      const struct sockaddr_in* in = (const struct sockaddr_in*)sa;
      inet_ntop(AF_INET, &in->sin_addr, addr, MAX_ADDRLEN);
      *port = ntohs(in->sin_port);
      break;
    }
    case AF_INET6:
    {
      const struct sockaddr_in6* in = (const struct sockaddr_in6*)sa;
      inet_ntop(AF_INET6, &in->sin6_addr, addr, MAX_ADDRLEN);
      *port = ntohs(in->sin6_port);
      break;
    }
    default:
      return false;
  }
  return true;
}

static inline
int ccsocket_set_flags(ccsocket_t s, ccsocket_flags_t flags)
{
  int r = -1;
#if WIN32
  u_long mode = 1;
  if (flags & CC_CLOEXEC)
    r = SetHandleInformation((HANDLE)s, HANDLE_FLAG_INHERIT, 0) ? 0 : -1;
  if (flags & CC_NONBLOCK)
    r = ioctlsocket(s, FIONBIO, &mode);
#else
  if (flags & CC_CLOEXEC)
    r = fcntl(s, F_SETFD, FD_CLOEXEC | fcntl(s, F_GETFD));
  if (flags & CC_NONBLOCK)
    r = fcntl(s, F_SETFL, O_NONBLOCK | fcntl(s, F_GETFL));
#endif
  return r;
}

static inline
int ccsocket_get_family(ccsocket_t s, struct sockaddr_storage* sa)
{
  socklen_t addrlen = sizeof(*sa); memset(sa, 0x0, sizeof(*sa));
#if WIN32
  WSAPROTOCOL_INFOA info; int len = sizeof(info); // WinSock使用getsockname会失败. :<
  int r = getsockopt((SOCKET)s, SOL_SOCKET, SO_PROTOCOL_INFO, (char*)&info, &len);
  sa->ss_family = info.iAddressFamily;
#else
  int r = getsockname((SOCKET)s, (struct sockaddr*)sa, (socklen_t*)&addrlen);
#endif
  return r;
}

static inline
int ccsocket_wrap_ip_and_port(ccsocket_t s, struct sockaddr_storage* sa, const char ip[MAX_ADDRLEN], uint16_t port)
{
  int r = ccsocket_get_family(s, sa);
  if (r)
    return r;
  // 根据协议转换IP与端口
  //struct sockaddr_in*  in;
  //struct sockaddr_in6* in6;
  switch ((int)sa->ss_family)
  {
#if defined(AF_UNIX)
    case AF_UNIX:
    {
      struct sockaddr_un* in = (struct sockaddr_un*)sa;
      memcpy(in->sun_path, ip, strlen(ip));
      break;
    }
#endif
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
      struct sockaddr_in6* in6 = (struct sockaddr_in6*)sa;
      in6->sin6_port = htons(port);
      if (1 != inet_pton(AF_INET6, ip, &in6->sin6_addr))
        return -1;
      break;
    }
  }
  return 0;
}

int ccsocket_close(ccsocket_t s)
{
  return closesocket(s);
}

/* 创建 ccsocket */
ccsocket_t ccsocket(ccsocket_domain_t domain, ccsocket_protocol_t proto)
{
  return ccsocket1(domain, proto, 0);
}

/* 创建 ccsocket 顺便设置标记 */
ccsocket_t ccsocket1(ccsocket_domain_t domain, ccsocket_protocol_t proto, ccsocket_flags_t flags)
{
  int domain_r = AF_UNSPEC;
#if defined(AF_UNIX)
  if (domain == CC_LOCAL)
    domain_r = AF_UNIX;
#endif
  if (domain == CC_INET4)
    domain_r = AF_INET;
  if (domain == CC_INET6)
    domain_r = AF_INET6;

  int flag_r = IPPROTO_IP;
  int proto_r = SOCK_RAW;
  if (proto == CC_TCP)
  {
    proto_r = SOCK_STREAM;
    //flag_r = IPPROTO_TCP;
  }
  if (proto == CC_UDP)
  {
    proto_r = SOCK_DGRAM;
    //flag_r = IPPROTO_UDP;
  }

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
  ccsocket_t s = socket(domain_r, proto_r, flag_r);
  if (s == INVALID_SOCKET)
    return INVALID_SOCKET;
  /**
  * 如果之前没有设置, 则再这里完成.
  * 但是在多线程环境下这不能绝对保证.
  */
  if (!isset && flags) {
    int r = ccsocket_set_flags(s, flags);
    if (r == -1) {
      ccsocket_close(s);
      s = INVALID_SOCKET;
    }
  }
  return s;
}

/* 准入 ccsocket */
ccsocket_t ccsocket_accept(ccsocket_t s, ccsocket_flags_t flags)
{
  return ccsocket_accept1(s, NULL, NULL, flags);
}

ccsocket_t ccsocket_accept1(ccsocket_t s, char ip[MAX_ADDRLEN], uint16_t *port, ccsocket_flags_t flags)
{
  errno = 0; socklen_t sasize = 0;
  struct sockaddr_storage* sap = NULL; socklen_t* sasizep = NULL;
  struct sockaddr_storage sa; memset(&sa, 0x0, sizeof(sa));
  if (ip && port) {
    if (ccsocket_get_family(s, &sa))
      return false;
    sap = &sa; sasizep = &sasize; sasize = ccsizeof(sap);
  }
  SOCKET c;
#if defined(SOCK_NONBLOCK) && defined(SOCK_CLOEXEC)
  int flags_r = 0;
  if (flags & CC_NONBLOCK)
    flags |= SOCK_NONBLOCK;
  if (flags & CC_CLOEXEC)
    flags |= SOCK_CLOEXEC;
  c = accept4(s, (struct sockaddr*)sap, sasizep, flags_r);
#else
  c = accept(s, (struct sockaddr*)sap, sasizep);
  if (c == INVALID_SOCKET)
    return INVALID_SOCKET;
  if (flags) {
    int r = ccsocket_set_flags(c, flags);
    if (r == -1) {
      ccsocket_close(c);
      c = INVALID_SOCKET;
    }
  }
#endif
  if (ip && port)
    ccsocket2addr(sap, ip, port);
  return c;
}

static inline
bool ccsocket_listen_internal(ccsocket_t s, const char ip[MAX_ADDRLEN], uint16_t port)
{
  errno = 0; int r = 0;
  struct sockaddr_storage sa; memset(&sa, 0x0, sizeof(sa));
  r = ccsocket_wrap_ip_and_port(s, &sa, ip, port);
  if (r)
    return false;

  r = bind(s, (const struct sockaddr*)&sa, ccsizeof(&sa));
  if (r < 0)
    return false;

  r = listen(s, SOMAXCONN);
  if (r < 0)
    return false;

  return true;
}

/* 监听 ccsocket */
bool ccsocket_listen(ccsocket_t s, const char *ip, uint16_t port)
{
  /**
   * 确保端口/地址被独占, 解决部分平台安全问题.
   */
#if defined(SO_EXCLUSIVEADDRUSE)
  int Enable = 1; errno = 0;
  if (SOCKET_ERROR == setsockopt((SOCKET)s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char*)&Enable, sizeof(Enable))) {
    return false;
  }
#elif defined(SO_EXCLBIND)
  int Enable = 1; errno = 0;
  if (SOCKET_ERROR == setsockopt((SOCKET)s, SOL_SOCKET, SO_EXCLBIND, (char*)&Enable, sizeof(Enable))) {
    return false;
  }
#endif
  return ccsocket_listen_internal(s, ip, port);
}

/* 监听 ccsocket 实现负载均衡(仅部分平台) */
bool ccsocket_listen1(ccsocket_t s, const char *ip, uint16_t port)
{
  /**
   * 注意:
   * 1. 仅下面列出的平台和之后的版本才支持内核负载均衡.
   * 2. DragonFly | FreeBSD 最多支持256个进程.
   * Linux 3.9 : SO_REUSEPORT
   * DragonFlyBSD 3.6 : SO_REUSEPORT
   * FreeBSD 12 : SO_REUSEPORT_LB
   * Solaris 11.4 : SO_REUSEPORT
   * AIX 7.2.5.0 : SO_REUSEPORT
   */
#if defined(SO_REUSEPORT_LB)
  int Enable = 1;
  if (SOCKET_ERROR == setsockopt((SOCKET)s, SOL_SOCKET, SO_REUSEPORT_LB, (char*)&Enable, sizeof(Enable))) {
    errno = EINVAL;
    return false;
  }
#elif defined(SO_REUSEPORT)
  if (!ccsocket_set_reuseport(s, true)) {
    errno = EINVAL;
    return false;
  }
#elif WIN32
  if (!ccsocket_set_reuseaddr(s, true)) {
    WSASetLastError(EINVAL);
    return false;
  }
#endif
  return ccsocket_listen_internal(s, ip, port);
}

/* 连接 ccsocket */
bool ccsocket_connect(ccsocket_t s, const char *ip, uint16_t port)
{
  errno = 0; int r;
  struct sockaddr_storage sa;
  r = ccsocket_wrap_ip_and_port(s, &sa, ip, port);
  if (r)
    return false;
  return connect((SOCKET)s, (const struct sockaddr*)&sa, ccsizeof(&sa)) != SOCKET_ERROR;
}

ccsocket_conn_state_t ccsocket_is_connected(ccsocket_t s)
{
  errno = 0;
  ccsocket_conn_state_t state = CC_CONNECTING;
#if WIN32
  if (SOCKET_ERROR == connect(s, NULL, 0))
  {
    switch (WSAGetLastError())
    {
      case WSAEISCONN:
        state = CC_CONNECTED;
        break;
      case WSAEALREADY:
      case WSAEWOULDBLOCK:
      case WSAEINPROGRESS:
        state = CC_CONNECTING;
        break;
      default:
        state = CC_CONNERROR;
    }
  } else {
    state = CC_CONNECTED;
  }
#else
  int error = 0; socklen_t len = sizeof(error);
  int r = getsockopt((SOCKET)s, SOL_SOCKET, SO_ERROR, &error, (socklen_t*)&len);
  // printf("getsockopt r = %d, error = %d, errno = %d\n", r, error, errno);
  if (r || error) {
    errno = error;
    return CC_CONNERROR;
  }
  /**
   * 如果还没发生错误, 就尝试获取对端地址
   * 获取地址失败可能是因为一些平台未连接成功时不会设置的.
   * 因此这里需要做一些额外的判断来处理这种特殊的情况.
   */
  char addr[MAX_ADDRLEN]; uint16_t port;
  if (!ccsocket_get_peername(s, addr, &port)) {
    // printf("ccsocket_get_peername errno = %d\n", errno);
    if (errno == ENOTCONN)
      return CC_CONNECTING;
  }
  /**
   * 即使能获取到对端地址, 也可能尚未完成连接.
   * 因此需要再次调用 connect 来判断连接状态.
   * EINPROGRESS 表示连接正在进行中.
   * EALREADY 表示还在继续尝试链接.
   */
  if (!ccsocket_connect(s, addr, port)) {
    // printf("ccsocket_connect errno = %d\n", errno);
    if (errno == EINPROGRESS || errno == EALREADY)
      return CC_CONNECTING;
    else if (errno != EISCONN)
      return CC_CONNERROR;
  }
  state = CC_CONNECTED;
#endif
  return state;
}

/* 发送 ccsocket */
int ccsocket_send(ccsocket_t s, const void* buf, size_t bsize)
{
  int flags = 0;
#if defined(MSG_NOSIGNAL)
  flags |= MSG_NOSIGNAL;
#endif
  return send((SOCKET)s, buf, (int)bsize, flags);
}

/* 接收 ccsocket */
int ccsocket_recv(ccsocket_t s, char* buf, size_t bsize)
{
  int flags = 0;
  // TODO: 
  return recv((SOCKET)s, buf, (int)bsize, flags);
}

/* 偷看 ccsocket */
int ccsocket_peek(ccsocket_t s, char* buf, size_t bsize)
{
#ifdef MSG_PEEK
  return recv((SOCKET)s, buf, (int)bsize, MSG_PEEK);
#else
  errno = EIO; 
  return SOCKET_ERROR;
#endif
}

/* 开启/关闭 nodelay */
bool ccsocket_set_nodelay(ccsocket_t s, bool on)
{
  int Enable = on ? 1 : 0;
  return SOCKET_ERROR != setsockopt((SOCKET)s, IPPROTO_TCP, TCP_NODELAY, (char*)&Enable, sizeof(Enable));
}

/* 开启/关闭 reuse address */
bool ccsocket_set_reuseaddr(ccsocket_t s, bool on)
{
  int Enable = on ? 1 : 0;
  return SOCKET_ERROR != setsockopt((SOCKET)s, SOL_SOCKET, SO_REUSEADDR, (char*)&Enable, sizeof(Enable));
}

/* 开启/关闭 reuse port */
bool ccsocket_set_reuseport(ccsocket_t s, bool on)
{
#if defined(SO_REUSEPORT)
  int Enable = on ? 1 : 0;
  return SOCKET_ERROR != setsockopt((SOCKET)s, SOL_SOCKET, SO_REUSEPORT, (char*)&Enable, sizeof(Enable));
#else
  return false;
#endif
}

/* 设置最大接收时间 */
bool ccsocket_set_rcvtimeout(ccsocket_t s, int timeout)
{
  socklen_t tm = timeout;
  return SOCKET_ERROR != setsockopt((SOCKET)s, SOL_SOCKET, SO_RCVTIMEO, (char*)&tm, sizeof(tm));
}

/* 设置最大发送时间 */
bool ccsocket_set_sndtimeout(ccsocket_t s, int timeout)
{
  socklen_t tm = timeout;
  return SOCKET_ERROR != setsockopt((SOCKET)s, SOL_SOCKET, SO_SNDTIMEO, (char*)&tm, sizeof(tm));
}

/* 获取对端地址/端口 */
bool ccsocket_get_peername(ccsocket_t s, char addr[MAX_ADDRLEN], uint16_t *port)
{
  struct sockaddr_storage sa;
  socklen_t addrlen = sizeof(sa); memset(&sa, 0x0, sizeof(sa));
  int r = getpeername((SOCKET)s, (struct sockaddr*)&sa, (socklen_t*)&addrlen);
  if (r == SOCKET_ERROR)
    return false;
  return ccsocket2addr(&sa, addr, port);
}

/* 获取本端地址/端口 */
bool ccsocket_get_sockname(ccsocket_t s, char addr[MAX_ADDRLEN], uint16_t *port)
{
  struct sockaddr_storage sa;
  socklen_t addrlen = sizeof(sa); memset(&sa, 0x0, sizeof(sa));
  int r = getsockname((SOCKET)s, (struct sockaddr*)&sa, (socklen_t*)&addrlen);
  if (r == SOCKET_ERROR)
    return false;
  return ccsocket2addr(&sa, addr, port);
}

bool ccsocket_set_nonblock(ccsocket_t s)
{
  return ccsocket_set_flags(s, CC_NONBLOCK) == 0;
}

bool ccsocket_set_cloexec(ccsocket_t s)
{
  return ccsocket_set_flags(s, CC_CLOEXEC) == 0;
}

void ccsocket_get_error(ccsocket_t s, char buf[MAX_ERRORLEN])
{
  (void)s;
#if WIN32
  memset(buf, 0x0, MAX_ERRORLEN);
  int err = WSAGetLastError();
  FormatMessageA(
    FORMAT_MESSAGE_FROM_SYSTEM,
    NULL,
    err,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    buf, (DWORD)MAX_ERRORLEN, NULL
  );
#else
  memset(buf, 0x0, MAX_ERRORLEN);
  const char *info = strerror(errno);
  memcpy(buf, info, strlen(info));
#endif
}

ccsocket_sendf_state_t ccsocket_sendfile(ccsocket_t s, int fd)
{
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__DragonFlyBSD__)
  off_t size = 0; errno = 0;
  off_t offset = lseek(fd, 0, SEEK_CUR);
  if (offset == -1)
    return CC_SENDERROR;
  #if defined(__APPLE__)
  int r = sendfile(fd, (SOCKET)s, offset, &size, NULL, 0);
  #else
  int r = sendfile(fd, (SOCKET)s, offset, 0, NULL, &size, 0);
  #endif
  if (r == -1) {
    if (errno == EINTR)
      return CC_SENDNEXT;
    return (errno == EAGAIN) ? CC_SENDWAIT : CC_SENDERROR;
  }
  off_t eof = lseek(fd, 0, SEEK_END);
  if (size == eof)
    return CC_SENDALL;
  lseek(fd, size, SEEK_SET);
  return ccsocket_sendfile(s, fd);
#elif defined(__linux__) || defined(__sun__)
  errno = 0;
  off_t offset = lseek(fd, 0, SEEK_CUR);
  if (offset == -1)
    return CC_SENDERROR;
  off_t wsize = sendfile(s, fd, &offset, INT64_MAX);
  if (wsize == -1) {
    if (errno == EINTR)
      return CC_SENDNEXT;
    return errno == EAGAIN ? CC_SENDWAIT : CC_SENDERROR;
  }
  off_t eof = lseek(fd, 0, SEEK_END);
  if (offset == eof)
    return CC_SENDALL;
  lseek(fd, offset, SEEK_SET);
  return ccsocket_sendfile(s, fd);
#elif _AIX
  errno = 0;
  off_t offset = lseek(fd, 0, SEEK_CUR);
  if (offset == -1)
    return CC_SENDERROR;
  struct sf_parms params = {
    .header_data = NULL, .header_length = 0,   // 无头部数据
    .trailer_data = NULL, .trailer_length = 0, // 无尾部数据
    .file_descriptor = fd, .file_offset = offset, .file_bytes = -1,
  };
  int wsize = send_file(s, &params, 0);
  if (wsize == -1) {
    if (errno == EINTR)
      return CC_SENDNEXT;
    return errno == EAGAIN ? CC_SENDWAIT : CC_SENDERROR;
  }
  offset = offset + params.bytes_sent;
  if (offset == params.file_size)
    return CC_SENDALL;
  // lseek(fd, size, SEEK_SET);
  return ccsocket_sendfile(s, fd);
#elif WIN32
  WSASetLastError(ENODEV);
  return CC_SENDERROR;
#else
  int wsize; errno = 0;
  uint32_t bsize = 1024; char buffer[1024];
  off_t offset = lseek(fd, 0, SEEK_CUR);
  if (offset == -1)
    return CC_SENDERROR;
  off_t eof = lseek(fd, 0, SEEK_END);
  if (eof == -1)
    return CC_SENDERROR;
  while (offset < eof) {
    int rsize = pread(fd, buffer, bsize, offset);
    if (rsize == -1)
      return CC_SENDERROR;
    int wsize = ccsocket_send(s, buffer, rsize);
    if (wsize == -1) {
      if (errno == EINTR)
        return CC_SENDNEXT;
      return errno == EAGAIN ? CC_SENDWAIT : CC_SENDERROR;
    }
    offset += wsize;
    lseek(fd, offset, SEEK_SET);
  }
  return CC_SENDALL;
#endif
}