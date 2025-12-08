#ifndef _GNU_SOURCE
  #define _GNU_SOURCE
#endif

/* 兼容C89/C90 */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199409L)
  #define CC_INLINE static inline
#else
  #define CC_INLINE static
#endif

#define STRICT
#define WIN32_LEAN_AND_MEAN
/*
* WSAStringToAddress / WSAAddressToString
*/
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "ccsocket.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if _WIN32
  #include <sdkddkver.h>
  #include <winsock2.h>
  #include <mstcpip.h>
  #include <ws2tcpip.h>
  #include <Windows.h>
  #include <io.h>
  #if defined(_MSC_VER) && WDK_NTDDI_VERSION > NTDDI_WIN10_RS2
    #include <afunix.h>
  #else
    #define UNIX_PATH_MAX 108
    typedef struct sockaddr_un
    {
        short sun_family;              /* AF_UNIX */
        char  sun_path[UNIX_PATH_MAX]; /* pathname */
    } SOCKADDR_UN, * PSOCKADDR_UN;
  #endif
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

CC_INLINE
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

CC_INLINE
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
    case AF_INET6:
    {
#if _WIN32
      DWORD len = MAX_ADDRLEN; WSAAddressToString((struct sockaddr*)sa, ccsizeof(sa), NULL, addr, &len);
#else
      sa->ss_family == AF_INET ? 
          inet_ntop(AF_INET, &(((struct sockaddr_in*)sa)->sin_addr), addr, MAX_ADDRLEN) :
          inet_ntop(AF_INET6, &(((struct sockaddr_in6*)sa)->sin6_addr), addr, MAX_ADDRLEN) ;
#endif
      *port = sa->ss_family == AF_INET ?
          ntohs(((struct sockaddr_in*)sa)->sin_port) :
          ntohs(((struct sockaddr_in6*)sa)->sin6_port);
      // Done.
      break;
    }
    default:
      return false;
  }
  return true;
}

CC_INLINE
int _ccsocket_set_flags(ccsocket_t s, ccsocket_flags_t flags, bool on)
{
  int r = -1;
#if _WIN32
  u_long mode = on ? 1 : 0;
  if (flags & CC_CLOEXEC)
    r = SetHandleInformation((HANDLE)s, HANDLE_FLAG_INHERIT, 0) ? 0 : -1;
  if (flags & CC_NONBLOCK)
    r = ioctlsocket(s, FIONBIO, &mode);
#else
  if (flags & CC_CLOEXEC) {
    if (on) {
      r = fcntl(s, F_SETFD, FD_CLOEXEC | fcntl(s, F_GETFD));
    } else {
      r = fcntl(s, F_SETFD, FD_CLOEXEC ^ fcntl(s, F_GETFD));
    }
    if (r) return r;
  }
  if (flags & CC_NONBLOCK) {
    if (on) {
      r = fcntl(s, F_SETFL, O_NONBLOCK | fcntl(s, F_GETFL));
    } else {
      r = fcntl(s, F_SETFL, O_NONBLOCK ^ fcntl(s, F_GETFL));
    }
    if (r) return r;
  }
#endif
  return r;
}

CC_INLINE
int ccsocket_set_flags(ccsocket_t s, ccsocket_flags_t flags)
{
  return _ccsocket_set_flags(s, flags, true);
}

CC_INLINE
int _ccsocket_get_family(ccsocket_t s, struct sockaddr_storage* sa)
{
  socklen_t addrlen = sizeof(*sa); memset(sa, 0x0, sizeof(*sa));
#if _WIN32
  WSAPROTOCOL_INFOA info; int len = sizeof(info); // WinSock使用getsockname会失败. :<
  int r = getsockopt((SOCKET)s, SOL_SOCKET, SO_PROTOCOL_INFO, (char*)&info, &len);
  sa->ss_family = info.iAddressFamily;
#else
  int r = getsockname((SOCKET)s, (struct sockaddr*)sa, (socklen_t*)&addrlen);
#endif
  return r;
}

CC_INLINE
int ccsocket_wrap_ip_and_port(ccsocket_t s, struct sockaddr_storage* sa, const char addr[MAX_ADDRLEN], uint16_t port)
{
  int r = _ccsocket_get_family(s, sa);
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
      memcpy(in->sun_path, addr, strlen(addr));
      break;
    }
#endif
    case AF_INET:
    case AF_INET6:
    {
#if _WIN32 // 为了兼容MingW的环境, 同时可以在低版本的编译器可用.
      int len = ccsizeof(sa);
      if (WSAStringToAddress((char*)addr, sa->ss_family, NULL, (struct sockaddr*)sa, &len)) return -1;
#else
      if (sa->ss_family == AF_INET && 1 != inet_pton(AF_INET, addr, &(((struct sockaddr_in*)sa)->sin_addr))) return -1;
      else if (sa->ss_family == AF_INET6 && 1 != inet_pton(AF_INET6, addr, &(((struct sockaddr_in6*)sa)->sin6_addr))) return -1;
#endif
      if (sa->ss_family == AF_INET) ((struct sockaddr_in*)sa)->sin_port = htons(port);
      else if (sa->ss_family == AF_INET6) ((struct sockaddr_in6*)sa)->sin6_port = htons(port);
      // Done.
      break;
    }
    default:
      return -1;
  }
  return 0;
}

int ccsocket_close(ccsocket_t s)
{
  return closesocket(s);
}

/* 创建 ccsocket 顺便设置标记 */
ccsocket_t ccsocket2(ccsocket_domain_t domain, ccsocket_protocol_t proto, ccsocket_flags_t flags)
{
  int domain_r = AF_UNSPEC;
  switch (domain)
  {
#if defined(AF_UNIX)
    case CC_UNIX:
      domain_r = AF_UNIX;
      break;
#endif
    case CC_INET4:
      domain_r = AF_INET;
      break;
    case CC_INET6:
      domain_r = AF_INET6;
      break;
    case CC_DOMAIN_INVALID:
    default:
      return SOCKET_ERROR;
  }

  int flag_r = IPPROTO_IP;
  int proto_r = 0;
  switch (proto)
  {
    case CC_TCP:
      proto_r = SOCK_STREAM;
      // flag_r = IPPROTO_TCP;
      break;
    case CC_UDP:
      proto_r = SOCK_DGRAM;
      // flag_r = IPPROTO_UDP;
      break;
    case CC_ICMP:
      proto_r = SOCK_RAW;
      // flag_r = IPPROTO_ICMP;
      break;
    case CC_PROTOCOL_INVALID:
    default:
      return SOCKET_ERROR;
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
#if _WIN32 // 创建支持重叠I/O
  ccsocket_t s = WSASocket(domain_r, proto_r, flag_r, NULL, 0, WSA_FLAG_OVERLAPPED);
#else
  ccsocket_t s = socket(domain_r, proto_r, flag_r);
#endif
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
ccsocket_t ccsocket_accept2(ccsocket_t s, char ip[MAX_ADDRLEN], uint16_t *port, ccsocket_flags_t flags)
{
  errno = 0; socklen_t sasize = 0;
  struct sockaddr_storage* sap = NULL; socklen_t* sasizep = NULL;
  struct sockaddr_storage sa; memset(&sa, 0x0, sizeof(sa));
  if (ip && port) {
    if (_ccsocket_get_family(s, &sa))
      return false;
    sap = &sa; sasizep = &sasize; sasize = ccsizeof(sap);
  }
  ccsocket_t c;
#if defined(SOCK_NONBLOCK) && defined(SOCK_CLOEXEC)
  int flags_r = 0;
  if (flags & CC_NONBLOCK)
    flags_r |= SOCK_NONBLOCK;
  if (flags & CC_CLOEXEC)
    flags_r |= SOCK_CLOEXEC;
  c = accept4(s, (struct sockaddr*)sap, sasizep, flags_r);
#else
  c = accept(s, (struct sockaddr*)sap, sasizep);
#endif
  if (c == INVALID_SOCKET) {
#if _WIN32
    int ecode = WSAGetLastError();
    if (ecode == WSAEINTR)
      return ccsocket_accept1(s, ip, port, flags);
    if (ecode == WSAEWOULDBLOCK)
      return 0;
#else
    if (errno == EINTR)
      return ccsocket_accept1(s, ip, port, flags);
    if (errno == EAGAIN)
      return 0;
#endif
    return INVALID_SOCKET;
  }
  if (flags) {
    int r = ccsocket_set_flags(c, flags);
    if (r == -1) {
      ccsocket_close(c);
      c = INVALID_SOCKET;
    }
  }
  if (ip && port)
    ccsocket2addr(sap, ip, port);
  return c;
}

CC_INLINE
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
#else
  errno = 0;
  if (!ccsocket_set_reuseaddr((SOCKET)s, true))
    return false;
#endif
  return ccsocket_listen_internal(s, ip, port);
}

/* 监听 ccsocket 实现负载均衡(仅部分平台) */
bool ccsocket_listen1(ccsocket_t s, const char *ip, uint16_t port)
{
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
#elif _WIN32
  if (!ccsocket_set_reuseaddr(s, true)) {
    WSASetLastError(EINVAL);
    return false;
  }
#endif
  return ccsocket_listen_internal(s, ip, port);
}

/* 创建双向连接的SOCK_STREAM管道 */
bool ccsocketpair1(ccsocket_t sv[2], ccsocket_flags_t flags)
{
  errno = 0;
  if (!sv || flags < 0 || flags > 3) {
    errno = EINVAL;
#if _WIN32
    WSASetLastError(WSAEINVAL);
#endif
    return false;
  }
#if _WIN32
  /* 本地管道 */
  ccsocket_t srv = ccsocket1(CC_INET4, CC_TCP, CC_NOFLAG);
  if (srv == SOCKET_ERROR)
    return false;
  /* 开启监听(`port`为0表示随机端口) */
  if (!ccsocket_listen_internal(srv, "127.0.0.1", 0)) {
    ccsocket_close(srv); /* 创建失败 */
    return false;
  }
  char addr[MAX_ADDRLEN]; uint16_t port;
  /* 获得监听服务器地址 */
  if (!ccsocket_get_sockname(srv, addr, &port)) {
    ccsocket_close(srv); /* 创建失败 */
    return false;
  }
  /* 创建 socket 1 */
  ccsocket_t c = ccsocket1(CC_INET4, CC_TCP, CC_NOFLAG);
  ccsocket_set_nonblock(c, true);
  if (!ccsocket_connect(c, addr, port)) {
    int code = WSAGetLastError();
    if (code != WSAEINPROGRESS && code != WSAEWOULDBLOCK) {
      ccsocket_close(srv); /* 创建失败 */
      ccsocket_close(c);   /* 创建失败 */
      return false;
    }
  }
  /* 创建 socket 2 */
  ccsocket_t s = ccsocket_accept(srv, flags);
  if (s == SOCKET_ERROR) {
    ccsocket_close(srv); /* 创建失败 */
    ccsocket_close(c);   /* 创建失败 */
    return false;
  }
  ccsocket_set_nonblock(c, false);
  ccsocket_set_flags(c, flags);
  sv[0] = c, sv[1] = s;
  /* 别忘记销毁监听套接字哦 */
  ccsocket_close(srv);
#else
  int r = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
  if (r == SOCKET_ERROR)
    return false;
  if (flags) {
    ccsocket_set_flags(sv[0], flags);
    ccsocket_set_flags(sv[1], flags);
  }
#endif
  return true;
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
#if _WIN32
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

int ccsocket_sendto(ccsocket_t s, const void *buf, size_t bsize, char *addr, uint16_t port)
{
  int flags = 0;
#if defined(MSG_NOSIGNAL)
  flags |= MSG_NOSIGNAL;
#endif
  socklen_t slen = 0;
  struct sockaddr_storage sa, *sap = NULL; 
  if (addr) {
    if (ccsocket_wrap_ip_and_port(s, &sa, addr, port))
      return SOCKET_ERROR;
    sap = &sa; slen = ccsizeof(sap);
  }
  return sendto((SOCKET)s, (const char *)buf, (int)bsize, flags, (const struct sockaddr *)sap, slen);
}

/* 发送 ccsocket */
int ccsocket_send(ccsocket_t s, const void* buf, size_t bsize)
{
  int flags = 0;
#if defined(MSG_NOSIGNAL)
  flags |= MSG_NOSIGNAL;
#endif
  return send((SOCKET)s, (const char *)buf, (int)bsize, flags);
}

int ccsocket_recvfrom(ccsocket_t s, void *buf, size_t bsize, char *addr, uint16_t *port)
{
  struct sockaddr_storage sa;
  int af = _ccsocket_get_family(s, &sa);
  if (af == SOCKET_ERROR)
    return SOCKET_ERROR;
  socklen_t len = ccsizeof(&sa);
  int r = recvfrom((SOCKET)s, (char *)buf, (int)bsize, 0, (struct sockaddr *)&sa, &len);
  if (r >= 0 && addr && port) {
    memset(addr, 0, MAX_ADDRLEN);
    ccsocket2addr(&sa, addr, port);
  }
  return r;
}

/* 接收 ccsocket */
int ccsocket_recv(ccsocket_t s, char* buf, size_t bsize)
{
  int flags = 0;
  // TODO: 
  return recv((SOCKET)s, (char *)buf, (int)bsize, flags);
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

/* 开启/关闭 keepalive */
bool ccsocket_set_keepalive(ccsocket_t s, bool on)
{
  int Enable = on ? 1 : 0;
  return SOCKET_ERROR != setsockopt((SOCKET)s, SOL_SOCKET, SO_KEEPALIVE, (char*)&Enable, sizeof(Enable));
}

/* 准入的连接为发送数据, 使用延迟`Accept`方式 */
bool ccsocket_enable_accept_defer(ccsocket_t s)
{
  socklen_t type; socklen_t len = sizeof(socklen_t);
  if (getsockopt((SOCKET)s, SOL_SOCKET, SO_TYPE, (char*)&type, &len))
    return false;
  if (type != SOCK_STREAM) {
    errno = EINVAL;
#if _WIN32
    WSASetLastError(WSAEINVAL);
#endif
    return false;
  }
#if defined(TCP_DEFER_ACCEPT)
  int Enable = 1;
  if (setsockopt((SOCKET)s, IPPROTO_TCP, TCP_DEFER_ACCEPT, &Enable, sizeof(Enable)))
    return false;
#elif defined(SO_ACCEPTFILTER)
/*
1. use bash -> kldload accf_data.ko
2. add 'accf_data_load="YES"' -> /boot/loader.conf

root@freebsd:~ # kldstat
Id Refs Address                Size Name
 1    7 0xffffffff80200000  1f30590 kernel
 2    1 0xffffffff82318000     3218 intpm.ko
 3    1 0xffffffff8231c000     2180 smbus.ko
 4    1 0xffffffff8231f000     20e0 accf_data.ko
*/
  struct accept_filter_arg afa; // 如果设置报错, 检查模块是否挂载.
  memset(&afa, 0x0, sizeof(afa)); strcpy(afa.af_name, "dataready");
  if (setsockopt((SOCKET)s, SOL_SOCKET, SO_ACCEPTFILTER, &afa, sizeof(afa)))
    return false;
#endif
  return true;
}

CC_INLINE
bool _ccsocket_set_timeout(ccsocket_t s, int type, int timeout)
{
#if _WIN32
  socklen_t tm = timeout;
#else
  struct timeval tm;
  tm.tv_sec = (timeout - (timeout % 1000)) / 1000;
  tm.tv_usec = (timeout % 1000) * 1000;
#endif
  return SOCKET_ERROR != setsockopt((SOCKET)s, SOL_SOCKET, type, (char*)&tm, sizeof(tm));
}

/* 设置最大接收时间 */
bool ccsocket_set_rcvtimeout(ccsocket_t s, int timeout)
{
  return _ccsocket_set_timeout(s, SO_RCVTIMEO, timeout);
}

/* 设置最大发送时间 */
bool ccsocket_set_sndtimeout(ccsocket_t s, int timeout)
{
  return _ccsocket_set_timeout(s, SO_SNDTIMEO, timeout);
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

int ccsocket_get_family(ccsocket_t s)
{
  struct sockaddr_storage sa;
  int r = _ccsocket_get_family(s, &sa);
  if (r)
    return CC_DOMAIN_INVALID;

  switch (sa.ss_family)
  {
#if defined(AF_UNIX)
    case AF_UNIX:
      return CC_UNIX;
#endif
    case AF_INET:
      return CC_INET4;
    case AF_INET6:
      return CC_INET6;
  }
  return CC_DOMAIN_INVALID;
}

ccsocket_domain_t ccsocket_get_version(const char addr[MAX_ADDRLEN])
{
  struct sockaddr_in sa4; memset(&sa4, 0x0, sizeof(sa4));
#if _WIN32
  socklen_t len4 = sizeof(sa4);
  if (!WSAStringToAddress((char *)addr, AF_INET, NULL, (struct sockaddr *)&sa4, &len4))
#else
  if (inet_pton(AF_INET, addr, &sa4.sin_addr) == 1)
#endif
    return CC_INET4;
  struct sockaddr_in6 sa6; memset(&sa6, 0x0, sizeof(sa6));
#if _WIN32
  socklen_t len6 = sizeof(sa6);
  if (!WSAStringToAddress((char*)addr, AF_INET6, NULL, (struct sockaddr *)&sa6, &len6))
#else
  if (inet_pton(AF_INET6, addr, &sa6.sin6_addr) == 1)
#endif
    return CC_INET6;
  errno = EINVAL;
#if _WIN32
  WSASetLastError(WSAEINVAL);
#endif
  return CC_DOMAIN_INVALID;
}

bool ccsocket_set_nonblock(ccsocket_t s, bool on)
{
  return _ccsocket_set_flags(s, CC_NONBLOCK, on) == 0;
}

bool ccsocket_set_cloexec(ccsocket_t s, bool on)
{
  return _ccsocket_set_flags(s, CC_CLOEXEC, on) == 0;
}

void ccsocket_get_error(ccsocket_t s, char buf[MAX_ERRORLEN])
{
  (void)s;
#if _WIN32
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
  off_t wsize = sendfile(s, fd, &offset, (size_t)INT64_MAX);
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
#else
#ifndef SEEK_CUR
  #define SEEK_CUR    1
#endif
#ifndef SEEK_END
  #define SEEK_END    2
#endif
#ifndef SEEK_SET
  #define SEEK_SET    0
#endif
#if _WIN32
  #define read _read
  #define lseek _lseeki64
  typedef int64_t off_t;
#endif
  errno = 0;
  uint32_t bsize = 1024; char buffer[1024];
  off_t offset = lseek(fd, 0, SEEK_CUR);
  if (offset == -1)
    return CC_SENDERROR;
  off_t eof = lseek(fd, 0, SEEK_END);
  if (eof == -1)
    return CC_SENDERROR;
  while (offset < eof) {
    int rsize = read(fd, buffer, bsize);
    if (rsize == -1)
      return CC_SENDERROR;
    int wsize = ccsocket_send(s, buffer, rsize);
    if (wsize == -1) {
#if _WIN32
      int err = WSAGetLastError();
      if (err == WSAEINTR)
#else
    #define WSAEWOULDBLOCK EAGAIN
      int err = errno;
      if (err == EINTR)
#endif
        return CC_SENDNEXT;
      return err == WSAEWOULDBLOCK ? CC_SENDWAIT : CC_SENDERROR;
    }
    offset += wsize;
    lseek(fd, offset, SEEK_SET);
  }
  return CC_SENDALL;
#endif
}
