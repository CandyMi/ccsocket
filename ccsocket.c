/**
 * @file ccsocket.c
 * @brief Cross-platform socket abstraction — implementation.
 *
 * @author CandyMi
 * @license MIT
 */

#ifndef _GNU_SOURCE
  #define _GNU_SOURCE
#endif

/* for supported C89/C90 */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199409L)
  #define CC_INLINE static inline
#elif defined(__cplusplus)
  #define CC_INLINE static inline
#elif _MSC_VER >= 1200
  #define CC_INLINE static __inline
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
#include "ccdns.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#ifndef NDEBUG
  #define ccsocket_dump(msg, ...) fprintf(stdout, "[libccsocket]: " msg "\n", ##__VA_ARGS__)
#else
  #define ccsocket_dump(msg, ...)
#endif

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

  /* WSARecvMsg / WSASendMsg function pointers (loaded via WSAIoctl at runtime).
   * Avoids dependency on mswsock.h declarations which vary across compilers. */
  typedef struct cc_WSAMSG {
      SOCKADDR    *name;
      INT          namelen;
      WSABUF      *lpBuffers;
      ULONG        dwBufferCount;
      WSABUF       Control;
      ULONG        dwFlags;
  } CC_WSAMSG;
  typedef int (WSAAPI *cc_pfn_WSARecvMsg)(SOCKET, CC_WSAMSG *, LPDWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
  typedef int (WSAAPI *cc_pfn_WSASendMsg)(SOCKET, CC_WSAMSG *, DWORD, LPDWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
  static cc_pfn_WSARecvMsg cc_WSARecvMsg_fn = NULL;
  static cc_pfn_WSASendMsg cc_WSASendMsg_fn = NULL;
  static bool cc_msg_ext_loaded = false;

  static bool cc_load_msg_ext(void)
  {
      if (cc_msg_ext_loaded) return (cc_WSARecvMsg_fn != NULL);
      cc_msg_ext_loaded = true;

      SOCKET tmp_s = socket(AF_INET, SOCK_DGRAM, 0);
      if (tmp_s == INVALID_SOCKET) return false;

      DWORD bytes;
      {
          GUID guid = {0xf689d7c8, 0x6f1f, 0x436b, {0x8a, 0x53, 0xe5, 0x4f, 0xe3, 0x51, 0xc3, 0xdb}};
          if (SOCKET_ERROR == WSAIoctl(tmp_s, SIO_GET_EXTENSION_FUNCTION_POINTER,
              &guid, sizeof(guid), &cc_WSARecvMsg_fn, sizeof(cc_WSARecvMsg_fn), &bytes, NULL, NULL))
              cc_WSARecvMsg_fn = NULL;
      }
      {
          GUID guid = {0xa441e712, 0x754f, 0x43ca, {0x84, 0xa7, 0x0d, 0xee, 0x44, 0xcf, 0x60, 0x6d}};
          if (SOCKET_ERROR == WSAIoctl(tmp_s, SIO_GET_EXTENSION_FUNCTION_POINTER,
              &guid, sizeof(guid), &cc_WSASendMsg_fn, sizeof(cc_WSASendMsg_fn), &bytes, NULL, NULL))
              cc_WSASendMsg_fn = NULL;
      }

      closesocket(tmp_s);
      return (cc_WSARecvMsg_fn != NULL && cc_WSASendMsg_fn != NULL);
  }

  /* Per-process WinSock initialisation.
   *
   * DllMain (DLL builds only):
   *   Debug   (NDEBUG not set)  → calls WSAStartup for developer convenience.
   *   Release (NDEBUG set)      → no WSAStartup; relies on ccsocket_init().
   *
   * Static library users: call ccsocket_init() explicitly.
   */
  #ifdef CCSOCKET_BUILD_SHARED
  BOOL WINAPI DllMain(
    _In_ HINSTANCE hinstDLL,
    _In_ DWORD     fdwReason,
    _In_ LPVOID    lpvReserved
  ) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
  #ifndef NDEBUG
      /* Debug builds: auto-init for developer convenience.
       * Per MSDN, WSAStartup in DllMain is technically unsafe (loader lock),
       * but in practice works reliably during DEBUG attachment and is the
       * simplest way to avoid "need to call init" for every dev/test run. */
      WSADATA wsaData;
      if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        WSACleanup();
        return FALSE;
      }
  #endif /* !NDEBUG */
    } else if (fdwReason == DLL_PROCESS_DETACH) {
      WSACleanup();
    }
    return TRUE;
  }
  #endif /* CCSOCKET_BUILD_SHARED */

  /* WSAStartup helper — used by both ccsocket_init() (public) and
   * internally when DllMain didn't already initialise WinSock. */
  static bool ccsocket_wsa_init_once(void) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
      WSACleanup();
      return false;
    }
  #ifndef NDEBUG
    ccsocket_dump("WSAStartup -> {Ver: %g, maxVer: %g, sockets: %d, udpMax: %d, sys: '%s', desc: '%s'}",
      (float)(HIBYTE(wsaData.wVersion) + LOBYTE(wsaData.wVersion) * 0.1),
      (float)(HIBYTE(wsaData.wHighVersion) + LOBYTE(wsaData.wHighVersion) * 0.1),
      wsaData.iMaxSockets, wsaData.iMaxUdpDg,
      wsaData.szSystemStatus, wsaData.szDescription
    );
  #endif
    return true;
  }
  #define ccsocket_init_errno() do {errno = 0; WSASetLastError(0);}while(0)
  #define ccsocket_is_errno(err) (WSA##err == WSAGetLastError())
  #define ccsocket_set_errno(err) do{errno = err; WSASetLastError(WSA##err);}while(0)
#else
  #include <fcntl.h>
  #include <unistd.h>
  #include <sys/un.h>
  #include <sys/uio.h>
  #include <netdb.h>
  #include <sys/stat.h>
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
#ifndef EWOULDBLOCK
  #define EWOULDBLOCK EAGAIN
#endif
  #define ccsocket_init_errno() errno = 0
  #define ccsocket_is_errno(err) (err == errno)
  #define ccsocket_set_errno(err) errno = err
#endif

/* -- Windows: public init (WSAStartup) ------------------------------------ */
CCSOCKET_EXPORT bool ccsocket_init(void)
{
#if _WIN32
  return ccsocket_wsa_init_once();
#else
  return true;
#endif
}

/* File-scope defines for the sendfile fallback on Windows.
 * Must be at file scope, not inside a function body, to avoid
 * leaking preprocessor defines across the translation unit. */
#if _WIN32
  #ifndef CC_SENDFILE_FALLBACK
    #if _WIN64
      #define read  _read
      #define lseek _lseeki64
      #ifndef _OFF_T_DEFINED
        typedef int64_t off_t;
      #endif
    #else
      #define read  _read
      #define lseek _lseek
      #ifndef _OFF_T_DEFINED
        typedef int32_t off_t;
      #endif
    #endif
    #define CC_SENDFILE_FALLBACK
  #endif
#endif

CC_INLINE
int ccsizeof(const struct sockaddr_storage* sa)
{
  if (sa)
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
  }
  return 0;
}

CC_INLINE
bool ccsocket2addr(const struct sockaddr_storage* sa, char *addr, uint16_t *port)
{
  switch ((int)sa->ss_family)
  {
#if defined(AF_UNIX)
    case AF_UNIX:
    {
      const struct sockaddr_un* in = (const struct sockaddr_un*)sa;
      size_t _pathlen = strlen(in->sun_path);
      memcpy(addr, in->sun_path, _pathlen);
      addr[_pathlen] = '\0';
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
#if _WIN32
  u_long mode = on ? 1 : 0;
  int r = 0;
  if (flags & CC_CLOEXEC) {
    if (!SetHandleInformation((HANDLE)s, HANDLE_FLAG_INHERIT, mode ? 0 : 1))
      r = -1;
  }
  if (flags & CC_NONBLOCK) {
    if (ioctlsocket(s, FIONBIO, &mode) != 0)
      r = -1;
  }
#else
  int r = -1;
  if (flags & CC_CLOEXEC) {
    int cur = fcntl(s, F_GETFD);
    if (cur == -1) return r;
    r = fcntl(s, F_SETFD, on ? (cur | FD_CLOEXEC) : (cur & ~FD_CLOEXEC));
    if (r) return r;
  }
  if (flags & CC_NONBLOCK) {
    int cur = fcntl(s, F_GETFL);
    if (cur == -1) return r;
    r = fcntl(s, F_SETFL, on ? (cur | O_NONBLOCK) : (cur & ~O_NONBLOCK));
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
  memset(sa, 0x0, sizeof(*sa));
#if _WIN32
  WSAPROTOCOL_INFOA info; int len = sizeof(info); // getsockname will fail on WinSock.
  int r = getsockopt((SOCKET)s, SOL_SOCKET, SO_PROTOCOL_INFO, (char*)&info, &len);
  sa->ss_family = info.iAddressFamily;
#else
  socklen_t addrlen = sizeof(*sa);
  int r = getsockname((SOCKET)s, (struct sockaddr*)sa, (socklen_t*)&addrlen);
#endif
  return r;
}

CC_INLINE
bool ccsocket_wrap_ip_and_port(ccsocket_t s, struct sockaddr_storage* sa, const char *addr, uint16_t port)
{
  if (_ccsocket_get_family(s, sa))
    return false;

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
#if _WIN32 // for mingw and msvc
      int len = ccsizeof(sa);
      if (WSAStringToAddress((char*)addr, sa->ss_family, NULL, (struct sockaddr*)sa, &len)) return false;
#else
      if (sa->ss_family == AF_INET && 1 != inet_pton(AF_INET, addr, &(((struct sockaddr_in*)sa)->sin_addr))) return false;
      else if (sa->ss_family == AF_INET6 && 1 != inet_pton(AF_INET6, addr, &(((struct sockaddr_in6*)sa)->sin6_addr))) return false;
#endif
      if (sa->ss_family == AF_INET) ((struct sockaddr_in*)sa)->sin_port = htons(port);
      else if (sa->ss_family == AF_INET6) ((struct sockaddr_in6*)sa)->sin6_port = htons(port);
      // Done.
      break;
    }
    default:
      ccsocket_set_errno(EINVAL);
      return false;
  }
  return true;
}

int ccsocket_close(ccsocket_t s)
{
  return closesocket(s);
}

/* create ccsocket with flags */
ccsocket_t ccsocket2(ccsocket_family_t domain, ccsocket_protocol_t proto, ccsocket_flags_t flags)
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
    case CC_FAMILY_INVALID:
    default:
      return SOCKET_ERROR;
  }

  int flag_r = IPPROTO_IP;
  int proto_r = 0;
  switch (proto)
  {
    case CC_TCP:
      proto_r = SOCK_STREAM;
      flag_r = IPPROTO_TCP;
      break;
    case CC_UDP:
      proto_r = SOCK_DGRAM;
      flag_r = IPPROTO_UDP;
      break;
    case CC_ICMP: case CC_ICMP1:
      proto_r = proto == CC_ICMP ? 
            SOCK_RAW : SOCK_DGRAM;
      flag_r = IPPROTO_ICMP;
      /* IPv6 ICMP uses IPPROTO_ICMPV6 (58) not IPPROTO_ICMP (1) */
      if (domain == CC_INET6) {
#ifndef IPPROTO_ICMPV6
        #define IPPROTO_ICMPV6 -1
#endif
        flag_r = IPPROTO_ICMPV6;
      }
      break;
    case CC_PROTOCOL_INVALID:
    default:
      return SOCKET_ERROR;
  }

  bool isset = false;
  if (flags & CC_NONBLOCK) {
#if defined(SOCK_NONBLOCK) // for nonblocking
    isset = true;
    proto_r |= SOCK_NONBLOCK;
#endif
  }
  if (flags & CC_CLOEXEC) {
#if defined(SOCK_CLOEXEC) // for closexec
    isset = true;
    proto_r |= SOCK_CLOEXEC;
#endif
  }

#if _WIN32 // for support overlap io in winsock.
  ccsocket_t s = WSASocket(domain_r, proto_r, flag_r, NULL, 0, WSA_FLAG_OVERLAPPED);
#else
  ccsocket_t s = socket(domain_r, proto_r, flag_r);
#endif
  if (s == INVALID_SOCKET)
    return INVALID_SOCKET;
  /**
  * not safe on `cloexec`
  */
  if (!isset && flags) {
    int r = ccsocket_set_flags(s, flags);
    if (r == -1) {
      ccsocket_close(s);
      s = INVALID_SOCKET;
    }
  }
#if defined(SO_NOSIGPIPE)
  int enable = 1;
  setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, (char*)&enable, sizeof(enable));
#endif
  return s;
}

/* accept ccsocket */
ccsocket_t ccsocket_accept2(ccsocket_t s, OPTIONAL char *ip, OPTIONAL uint16_t *port, ccsocket_flags_t flags)
{
  ccsocket_init_errno(); socklen_t sasize = 0;
  struct sockaddr_storage* sap = NULL; socklen_t* sasizep = NULL;
  struct sockaddr_storage sa; memset(&sa, 0x0, sizeof(sa));
  if (ip && port) {
    if (_ccsocket_get_family(s, &sa))
      return INVALID_SOCKET;
    sap = &sa; sasizep = &sasize; sasize = ccsizeof(sap);
  }
  ccsocket_t c = INVALID_SOCKET; int flags_r = 0;
#if defined(SOCK_NONBLOCK) && defined(SOCK_CLOEXEC)
  if (flags & CC_NONBLOCK)
    flags_r |= SOCK_NONBLOCK;
  if (flags & CC_CLOEXEC)
    flags_r |= SOCK_CLOEXEC;
  c = accept4(s, (struct sockaddr*)sap, sasizep, flags_r);
#else
  c = accept(s, (struct sockaddr*)sap, sasizep);
#endif
  if (c == INVALID_SOCKET) {
    if (ccsocket_is_errno(EINTR))
      return ccsocket_accept1(s, ip, port, flags);
    if (ccsocket_is_errno(EWOULDBLOCK))
      return 0;
    return INVALID_SOCKET;
  }
  if (flags && !flags_r) {
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
bool ccsocket_listen_internal(ccsocket_t s, const char *ip, uint16_t port)
{
  ccsocket_init_errno(); int r = 0;
  struct sockaddr_storage sa; memset(&sa, 0x0, sizeof(sa));
  if (!ccsocket_wrap_ip_and_port(s, &sa, ip, port))
    return false;

  r = bind(s, (const struct sockaddr*)&sa, ccsizeof(&sa));
  if (r < 0)
    return false;

  r = listen(s, SOMAXCONN);
  if (r < 0)
    return false;

  return true;
}

/* listen ccsocket */
bool ccsocket_listen(ccsocket_t s, const char *ip, uint16_t port)
{
  /**
   * ensure that the socket is an exclusive listener.
   */
#if defined(SO_EXCLUSIVEADDRUSE)
  int Enable = 1; ccsocket_init_errno();
  if (SOCKET_ERROR == setsockopt((SOCKET)s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char*)&Enable, sizeof(Enable))) {
    return false;
  }
#elif defined(SO_EXCLBIND)
  int Enable = 1; ccsocket_init_errno();
  if (SOCKET_ERROR == setsockopt((SOCKET)s, SOL_SOCKET, SO_EXCLBIND, (char*)&Enable, sizeof(Enable))) {
    return false;
  }
#else
  ccsocket_init_errno();
  if (!ccsocket_set_reuseaddr((SOCKET)s, true))
    return false;
#endif
  return ccsocket_listen_internal(s, ip, port);
}

/* listen ccsocket for loadbalance (part) */
bool ccsocket_listen1(ccsocket_t s, const char *ip, uint16_t port)
{
#if defined(SO_REUSEPORT_LB)
  int Enable = 1;
  if (SOCKET_ERROR == setsockopt((SOCKET)s, SOL_SOCKET, SO_REUSEPORT_LB, (char*)&Enable, sizeof(Enable))) {
    // errno = EINVAL;
    return false;
  }
#elif defined(SO_REUSEPORT)
  if (!ccsocket_set_reuseport(s, true)) {
    // errno = EINVAL;
    return false;
  }
#elif _WIN32
  if (!ccsocket_set_reuseaddr(s, true)) {
    // WSASetLastError(EINVAL);
    return false;
  }
#endif
  return ccsocket_listen_internal(s, ip, port);
}

int ccsocket_pipe(ccsocket_t sv[2])
{
  bool ok = ccsocketpair(sv, CC_NOFLAG);
  if (!ok) return SOCKET_ERROR;
#if _WIN32
  shutdown(sv[0], SD_SEND);
  shutdown(sv[1], SD_RECEIVE);
#else
  shutdown(sv[0], SHUT_WR);
  shutdown(sv[1], SHUT_RD);
#endif
  return 0;
}

/* like socketpair */
bool ccsocketpair1(ccsocket_t sv[2], ccsocket_flags_t flags)
{
  ccsocket_init_errno();
  if (!sv || flags < 0 || flags > 3) {
    ccsocket_set_errno(EINVAL);
    return false;
  }
#if _WIN32
  /* 本地管道 */
  ccsocket_t srv = ccsocket1(CC_INET4, CC_TCP, CC_NOFLAG);
  if (srv == SOCKET_ERROR)
    return false;
  /* listen random port */
  if (!ccsocket_listen_internal(srv, "127.0.0.1", 0)) {
    ccsocket_close(srv); /* failed */
    return false;
  }
  char addr[MAX_ADDRLEN]; uint16_t port;
  /* get listen ip and port */
  if (!ccsocket_get_sockname(srv, addr, &port)) {
    ccsocket_close(srv); /* failed */
    return false;
  }
  /* create socket 1 */
  ccsocket_t c = ccsocket1(CC_INET4, CC_TCP, CC_NOFLAG);
  if (c == SOCKET_ERROR || !ccsocket_connect(c, addr, port)) {
    if (c != SOCKET_ERROR)
      ccsocket_close(c); /* failed */
    ccsocket_close(srv); /* failed */
    return false;
  }
  /* create socket 2 */
  ccsocket_t s = ccsocket_accept(srv, flags);
  if (s == SOCKET_ERROR) {
    ccsocket_close(srv); /* failed */
    ccsocket_close(c);   /* failed */
    return false;
  }
  ccsocket_set_flags(c, flags);
  sv[0] = c, sv[1] = s;
  /* destroy */
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

/* connect ccsocket */
bool ccsocket_connect(ccsocket_t s, const char *addr, uint16_t port)
{
  struct sockaddr_storage sa;
  struct sockaddr_storage *sap = NULL; socklen_t salen = 0;
  if (addr) {
    if (!ccsocket_wrap_ip_and_port(s, &sa, addr, port))
      return false;
    sap = &sa; salen = ccsizeof(sap);
  }
  return connect((SOCKET)s, (const struct sockaddr*)sap, salen) != SOCKET_ERROR;
}

ccsocket_conn_state_t ccsocket_is_connected(ccsocket_t s)
{
  ccsocket_init_errno();
  ccsocket_conn_state_t state = CC_CONNECTED;
#if _WIN32
  if (!ccsocket_connect(s, NULL, 0)) {
    if (ccsocket_is_errno(EINTR)) {
      return ccsocket_is_connected(s);
    }
    if (!ccsocket_is_errno(EISCONN)) {
      if (ccsocket_is_errno(EALREADY) || ccsocket_is_errno(EWOULDBLOCK) || ccsocket_is_errno(EINPROGRESS)) {
        state = CC_CONNECTING;
      } else {
        state = CC_CONNERROR;
      }
    }
  }
#else
  int error = 0; socklen_t len = sizeof(error);
  int r = getsockopt((SOCKET)s, SOL_SOCKET, SO_ERROR, &error, (socklen_t*)&len);
  // printf("getsockopt r = %d, error = %d, errno = %d\n", r, error, errno);
  if (r || error) {
    return CC_CONNERROR;
  }
  /**
   * no sure.
   */
  char addr[MAX_ADDRLEN]; uint16_t port;
  if (!ccsocket_get_peername(s, addr, &port)) {
    // printf("ccsocket_get_peername errno = %d\n", errno);
    if (errno == ENOTCONN)
      return CC_CONNECTING;
    return CC_CONNERROR;
  }
  /**
   * test state for connect again
   */
  if (!ccsocket_connect(s, addr, port)) {
    // printf("ccsocket_connect errno = %d\n", errno);
    if (errno == EINPROGRESS || errno == EALREADY)
      return CC_CONNECTING;
    else if (errno != EISCONN)
      return CC_CONNERROR;
  }
#endif
  return state;
}

ccsocket_stcode_t ccsocket_send1(ccsocket_t s, const void *buf, size_t bsize, OPTIONAL int *wsize, int flags)
{
  ccsocket_iovec_t iov[1]; ccsocket_init_iov(iov, 1);
  ccsocket_set_iov_len(iov, 0, bsize); ccsocket_set_iov_buf(iov, 0, buf);
  return ccsocket_sendv1(s, iov, 1, wsize, flags);
}

ccsocket_stcode_t ccsocket_sendv1(ccsocket_t s, ccsocket_iovec_t *iov, int iovcnt, OPTIONAL int *wsize, int flags)
{
  int w = 0; int wsz = 0;
  ccsocket_init_errno();
#if _WIN32
  w = WSASend((SOCKET)s, (LPWSABUF)iov, iovcnt, (LPDWORD)&wsz, 0, NULL, NULL);
#else
#if defined(MSG_NOSIGNAL)
  flags |= MSG_NOSIGNAL;
#endif
  struct msghdr msg; memset(&msg, 0x0, sizeof(msg));
  msg.msg_iov = (struct iovec *)iov; msg.msg_iovlen = iovcnt;
  w = sendmsg(s, &msg, flags);
  if (w > 0) wsz = w;
#endif
  if (w == SOCKET_ERROR) {
    if (ccsocket_is_errno(EINTR))
      return ccsocket_sendv1(s, iov, iovcnt, wsize, flags);
    if (ccsocket_is_errno(EWOULDBLOCK))
      return CC_OPCODE_WAIT;
    return CC_OPCODE_ERROR;
  }
  if (wsize) *wsize = (int)wsz;
  return CC_OPCODE_OK;
}

CC_INLINE
ccsocket_stcode_t ccsocket_recv_internal(ccsocket_t s, ccsocket_iovec_t *iov, int iovcnt, OPTIONAL int *rsize, int flags)
{
  int r = 0;
#if _WIN32
  DWORD rsz = 0;
  r = WSARecv(s, (LPWSABUF)iov, iovcnt, (LPDWORD)&rsz, (LPDWORD)&flags, NULL, NULL);
#else
  int rsz = 0;
  struct msghdr msg; memset(&msg, 0x0, sizeof(msg));
  msg.msg_iov = (struct iovec *)iov; msg.msg_iovlen = iovcnt;
  r = recvmsg(s, &msg, flags);
  if (r > 0) rsz = r;
  if (r == 0) {
    r = SOCKET_ERROR;
    ccsocket_set_errno(ENOTCONN);
  }
#endif
  if (r == SOCKET_ERROR) {
    // char buffer[255]; ccsocket_get_error(s, buffer); 
    // ccsocket_dump("errno = %d, err = '%s'.", errno, buffer);
    if (ccsocket_is_errno(EINTR))
      return ccsocket_recv_internal(s, iov, iovcnt, rsize, flags);
    if (ccsocket_is_errno(EWOULDBLOCK))
      return CC_OPCODE_WAIT;
    return CC_OPCODE_ERROR;
  }
  if (rsize) *rsize = (int)rsz;
  return CC_OPCODE_OK;
}

/* recv for iov copy */
ccsocket_stcode_t ccsocket_recv1(ccsocket_t s, ccsocket_iovec_t *iov, int iovcnt, OPTIONAL int *rsize)
{
  return ccsocket_recv_internal(s, iov, iovcnt, rsize, 0);
}

/* recv for copy */
ccsocket_stcode_t ccsocket_recv(ccsocket_t s, char *buf, size_t bsize, OPTIONAL int *rsize)
{
  ccsocket_iovec_t iov[1]; ccsocket_init_iov(iov, 1);
  ccsocket_set_iov_len(iov, 0, bsize); ccsocket_set_iov_buf(iov, 0, buf);
  return ccsocket_recv_internal(s, iov, 1, rsize, 0);
}

/* recv for peek */
ccsocket_stcode_t ccsocket_peek(ccsocket_t s, char* buf, size_t bsize, OPTIONAL int *rsize)
{
  ccsocket_iovec_t iov[1]; ccsocket_init_iov(iov, 1);
  ccsocket_set_iov_len(iov, 0, bsize); ccsocket_set_iov_buf(iov, 0, buf);
  return ccsocket_recv_internal(s, iov, 1, rsize, MSG_PEEK);
}

/* ==== msg_flags mapping helpers (cross-platform) ==== */

CC_INLINE
int ccsocket_msg_flags_to_os(ccsocket_msg_flags_t flags, bool is_recv)
{
    int os_flags = 0;
    (void)is_recv;
#if _WIN32
    if (flags & CC_MSG_PEEK)       os_flags |= MSG_PEEK;
    if (flags & CC_MSG_WAITALL)    os_flags |= MSG_WAITALL;
    if (flags & CC_MSG_OOB)        os_flags |= MSG_OOB;
    /* CC_MSG_DONTWAIT — no Windows equivalent; silently ignored */
    /* CC_MSG_NOSIGNAL — no SIGPIPE on Windows; silently ignored */
    /* CC_MSG_MORE     — no Windows equivalent; silently ignored */
#else
    if (flags & CC_MSG_PEEK)       os_flags |= MSG_PEEK;
    if (flags & CC_MSG_WAITALL)    os_flags |= MSG_WAITALL;
    if (flags & CC_MSG_DONTWAIT)   os_flags |= MSG_DONTWAIT;
#if defined(MSG_NOSIGNAL)
    if (flags & CC_MSG_NOSIGNAL)   os_flags |= MSG_NOSIGNAL;
#endif
#if defined(MSG_MORE)
    if (flags & CC_MSG_MORE)       os_flags |= MSG_MORE;
#endif
    if (flags & CC_MSG_OOB)        os_flags |= MSG_OOB;
#endif
    return os_flags;
}

CC_INLINE
int ccsocket_ret_flags_from_os(int os_flags)
{
    int ret = 0;
#if _WIN32
    if (os_flags & MSG_PARTIAL)    ret |= CC_MSG_RET_TRUNC;
#else
    if (os_flags & MSG_TRUNC)      ret |= CC_MSG_RET_TRUNC;
    if (os_flags & MSG_CTRUNC)     ret |= CC_MSG_RET_CTRUNC;
    if (os_flags & MSG_EOR)        ret |= CC_MSG_RET_EOR;
    if (os_flags & MSG_OOB)        ret |= CC_MSG_RET_OOB;
#if defined(MSG_BCAST)
    if (os_flags & MSG_BCAST)      ret |= CC_MSG_RET_BCAST;
#endif
#if defined(MSG_MCAST)
    if (os_flags & MSG_MCAST)      ret |= CC_MSG_RET_MCAST;
#endif
#endif
    return ret;
}

/* ==== recvmsg ==== */

ccsocket_stcode_t ccsocket_recvmsg(ccsocket_t s, ccsocket_msghdr_t *msg, ccsocket_msg_flags_t flags)
{
    ccsocket_init_errno();

#if _WIN32
    if (!cc_load_msg_ext())
        return CC_OPCODE_ERROR;

    SOCKADDR_STORAGE sa;
    CC_WSAMSG hdr;
    DWORD rsz;
    int r;
    int os_flags;

    memset(&hdr, 0, sizeof(hdr));
    memset(&sa, 0, sizeof(sa));
    hdr.name = (SOCKADDR *)&sa;
    hdr.namelen = sizeof(sa);
    hdr.lpBuffers = (WSABUF *)msg->msg_iov;
    hdr.dwBufferCount = (DWORD)msg->msg_iovlen;
    hdr.Control.buf = (char *)msg->msg_control;
    hdr.Control.len = (ULONG)msg->msg_controllen;

    os_flags = ccsocket_msg_flags_to_os(flags, true);
    r = cc_WSARecvMsg_fn((SOCKET)s, &hdr, &rsz, NULL, NULL);
    if (r == SOCKET_ERROR) {
        if (ccsocket_is_errno(EINTR))
            return ccsocket_recvmsg(s, msg, flags);
        if (ccsocket_is_errno(EWOULDBLOCK))
            return CC_OPCODE_WAIT;
        return CC_OPCODE_ERROR;
    }

    ccsocket2addr((const struct sockaddr_storage *)&sa, msg->msg_name, &msg->msg_port);
    msg->msg_flags = ccsocket_ret_flags_from_os((int)hdr.dwFlags);
    msg->msg_controllen = (size_t)hdr.Control.len;
    msg->msg_bytes = (int)rsz;
#else
    struct sockaddr_storage sa;
    struct msghdr hdr;
    int r;
    int os_flags;

    memset(&hdr, 0, sizeof(hdr));
    memset(&sa, 0, sizeof(sa));
    hdr.msg_name = &sa;
    hdr.msg_namelen = sizeof(sa);
    hdr.msg_iov = (struct iovec *)msg->msg_iov;
    hdr.msg_iovlen = msg->msg_iovlen;
    hdr.msg_control = msg->msg_control;
    hdr.msg_controllen = msg->msg_controllen;

    os_flags = ccsocket_msg_flags_to_os(flags, true);
    r = (int)recvmsg((SOCKET)s, &hdr, os_flags);
    if (r == -1) {
        if (ccsocket_is_errno(EINTR))
            return ccsocket_recvmsg(s, msg, flags);
        if (ccsocket_is_errno(EWOULDBLOCK))
            return CC_OPCODE_WAIT;
        return CC_OPCODE_ERROR;
    }

    ccsocket2addr((const struct sockaddr_storage *)&sa, msg->msg_name, &msg->msg_port);
    msg->msg_flags = ccsocket_ret_flags_from_os(hdr.msg_flags);
    msg->msg_controllen = (size_t)hdr.msg_controllen;
    msg->msg_bytes = r;
#endif
    return CC_OPCODE_OK;
}

/* ==== sendmsg ==== */

ccsocket_stcode_t ccsocket_sendmsg(ccsocket_t s, ccsocket_msghdr_t *msg, ccsocket_msg_flags_t flags)
{
    ccsocket_init_errno();

#if _WIN32
    if (!cc_load_msg_ext())
        return CC_OPCODE_ERROR;

    SOCKADDR_STORAGE sa;
    CC_WSAMSG hdr;
    DWORD wsz;
    int r;
    int os_flags;

    memset(&hdr, 0, sizeof(hdr));
    memset(&sa, 0, sizeof(sa));
    hdr.lpBuffers = (WSABUF *)msg->msg_iov;
    hdr.dwBufferCount = (DWORD)msg->msg_iovlen;
    hdr.Control.buf = (char *)msg->msg_control;
    hdr.Control.len = (ULONG)msg->msg_controllen;

    /* destination address */
    if (msg->msg_name[0]) {
        if (!ccsocket_wrap_ip_and_port(s, (struct sockaddr_storage *)&sa, msg->msg_name, msg->msg_port))
            return CC_OPCODE_ERROR;
        hdr.name = (SOCKADDR *)&sa;
        hdr.namelen = ccsizeof((const struct sockaddr_storage *)&sa);
    }

    os_flags = ccsocket_msg_flags_to_os(flags, false);
    r = cc_WSASendMsg_fn((SOCKET)s, &hdr, (DWORD)os_flags, &wsz, NULL, NULL);
    if (r == SOCKET_ERROR) {
        if (ccsocket_is_errno(EINTR))
            return ccsocket_sendmsg(s, msg, flags);
        if (ccsocket_is_errno(EWOULDBLOCK))
            return CC_OPCODE_WAIT;
        return CC_OPCODE_ERROR;
    }

    msg->msg_bytes = (int)wsz;
#else
    struct sockaddr_storage sa;
    struct msghdr hdr;
    int os_flags;
    int w;

    memset(&hdr, 0, sizeof(hdr));
    memset(&sa, 0, sizeof(sa));
    hdr.msg_iov = (struct iovec *)msg->msg_iov;
    hdr.msg_iovlen = msg->msg_iovlen;
    hdr.msg_control = msg->msg_control;
    hdr.msg_controllen = msg->msg_controllen;

    /* destination address (sendto semantics) */
    if (msg->msg_name[0]) {
        if (!ccsocket_wrap_ip_and_port(s, &sa, msg->msg_name, msg->msg_port))
            return CC_OPCODE_ERROR;
        hdr.msg_name = &sa;
        hdr.msg_namelen = ccsizeof(&sa);
    }

    os_flags = ccsocket_msg_flags_to_os(flags, false);
#if defined(MSG_NOSIGNAL)
    os_flags |= MSG_NOSIGNAL;
#endif
    w = (int)sendmsg((SOCKET)s, &hdr, os_flags);
    if (w == -1) {
        if (ccsocket_is_errno(EINTR))
            return ccsocket_sendmsg(s, msg, flags);
        if (ccsocket_is_errno(EWOULDBLOCK))
            return CC_OPCODE_WAIT;
        return CC_OPCODE_ERROR;
    }

    msg->msg_bytes = w;
#endif
    return CC_OPCODE_OK;
}

/* ==== sendto (thin wrapper over sendmsg) ==== */

ccsocket_stcode_t ccsocket_sendto(ccsocket_t s, const void *buf, size_t bsize,
                                   const char *addr, uint16_t port,
                                   OPTIONAL int *wsize)
{
    ccsocket_iovec_t iov[1];
    ccsocket_msghdr_t msg;

    ccsocket_init_iov(iov, 1);
    ccsocket_set_iov_len(iov, 0, bsize);
    ccsocket_set_iov_buf(iov, 0, (void *)buf);

    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    if (addr) {
        strncpy(msg.msg_name, addr, sizeof(msg.msg_name) - 1);
        msg.msg_name[sizeof(msg.msg_name) - 1] = '\0';
        msg.msg_port = port;
    }

    ccsocket_stcode_t r = ccsocket_sendmsg(s, &msg, CC_MSG_NOFLAG);
    if (wsize) *wsize = msg.msg_bytes;
    return r;
}

/* ==== recvfrom (thin wrapper over recvmsg) ==== */

ccsocket_stcode_t ccsocket_recvfrom(ccsocket_t s, char *buf, size_t bsize,
                                     OPTIONAL char *addr, OPTIONAL uint16_t *port,
                                     OPTIONAL int *rsize)
{
    ccsocket_iovec_t iov[1];
    ccsocket_msghdr_t msg;

    ccsocket_init_iov(iov, 1);
    ccsocket_set_iov_len(iov, 0, bsize);
    ccsocket_set_iov_buf(iov, 0, buf);

    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    ccsocket_stcode_t r = ccsocket_recvmsg(s, &msg, CC_MSG_NOFLAG);
    if (addr) memcpy(addr, msg.msg_name, 65);
    if (port) *port = msg.msg_port;
    if (rsize) *rsize = msg.msg_bytes;
    return r;
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
  ccsocket_init_errno();
  socklen_t type; socklen_t len = sizeof(socklen_t);
  if (getsockopt((SOCKET)s, SOL_SOCKET, SO_TYPE, (char*)&type, &len) == SOCKET_ERROR)
    return false;
  if (type != SOCK_STREAM) {
    ccsocket_set_errno(EINVAL);
    return false;
  }
#if defined(TCP_DEFER_ACCEPT)
  int Enable = 1;
  if (setsockopt((SOCKET)s, IPPROTO_TCP, TCP_DEFER_ACCEPT, &Enable, sizeof(Enable)) == SOCKET_ERROR)
    return false;
#elif defined(SO_ACCEPTFILTER)
/*
1. use command -> `kldload accf_data.ko`
2. add 'accf_data_load="YES"' -> /boot/loader.conf

root@freebsd:~ # kldstat
Id Refs Address                Size Name
 1    7 0xffffffff80200000  1f30590 kernel
 2    1 0xffffffff82318000     3218 intpm.ko
 3    1 0xffffffff8231c000     2180 smbus.ko
 4    1 0xffffffff8231f000     20e0 accf_data.ko
*/
#define ACCF_NAME "dataready"
  struct accept_filter_arg afa; memset(&afa, 0x0, sizeof(afa)); strcpy(afa.af_name, ACCF_NAME);
  if (setsockopt((SOCKET)s, SOL_SOCKET, SO_ACCEPTFILTER, &afa, sizeof(afa)) == SOCKET_ERROR)
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
bool ccsocket_get_peername(ccsocket_t s, char *addr, uint16_t *port)
{
  struct sockaddr_storage sa;
  socklen_t addrlen = sizeof(sa); memset(&sa, 0x0, sizeof(sa));
  int r = getpeername((SOCKET)s, (struct sockaddr*)&sa, (socklen_t*)&addrlen);
  if (r == SOCKET_ERROR)
    return false;
  return ccsocket2addr(&sa, addr, port);
}

/* 获取本端地址/端口 */
bool ccsocket_get_sockname(ccsocket_t s, char *addr, uint16_t *port)
{
  struct sockaddr_storage sa;
  socklen_t addrlen = sizeof(sa); memset(&sa, 0x0, sizeof(sa));
  int r = getsockname((SOCKET)s, (struct sockaddr*)&sa, (socklen_t*)&addrlen);
  if (r == SOCKET_ERROR)
    return false;
  return ccsocket2addr(&sa, addr, port);
}

ccsocket_family_t ccsocket_get_family(ccsocket_t s)
{
  struct sockaddr_storage sa;
  int r = _ccsocket_get_family(s, &sa);
  if (r)
    return CC_FAMILY_INVALID;

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
  return CC_FAMILY_INVALID;
}

int ccsocket_get_protocol(ccsocket_t s)
{
  int type = -1;
  socklen_t optlen = sizeof(type);
#if _WIN32
  if (getsockopt((SOCKET)s, SOL_SOCKET, SO_TYPE, (char *)&type, &optlen) != 0)
    return -1;
#else
  if (getsockopt((SOCKET)s, SOL_SOCKET, SO_TYPE, &type, &optlen) != 0)
    return -1;
#endif
  return type;
}

ccsocket_family_t ccsocket_get_version(const char *addr)
{
  if (!addr) {
    ccsocket_set_errno(EINVAL);
    return CC_FAMILY_INVALID;
  }
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
  /* 增加对unix domain socket的判断支持. */
#if !defined(_WIN32)
  struct stat st;
  if (!stat(addr, &st) && S_ISSOCK(st.st_mode))
    return CC_UNIX;
#endif
  ccsocket_set_errno(EINVAL);
  return CC_FAMILY_INVALID;
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
  ccsocket_init_errno();
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__DragonFlyBSD__)
  off_t wsize = 0;
  off_t offset = lseek(fd, 0, SEEK_CUR);
  if (offset == -1)
    return CC_SENDERROR;
  off_t eof = lseek(fd, 0, SEEK_END);
  if (eof == -1)
    return CC_SENDERROR;
  lseek(fd, offset, SEEK_SET);
  #if defined(__APPLE__)
  int r = sendfile(fd, (SOCKET)s, offset, &wsize, NULL, 0);
  #else
  int r = sendfile(fd, (SOCKET)s, offset, 0, NULL, &wsize, 0);
  #endif
  if (r == SOCKET_ERROR) {
    if (ccsocket_is_errno(EINTR))
      return ccsocket_sendfile(s, fd);
    return ccsocket_is_errno(EWOULDBLOCK) ? CC_SENDWAIT : CC_SENDERROR;
  }
  lseek(fd, offset + wsize, SEEK_SET);
  if ((offset + wsize) == eof)
    return CC_SENDALL;
  return ccsocket_sendfile(s, fd);
#elif defined(__linux__) || defined(__sun__)
  off_t offset = lseek(fd, 0, SEEK_CUR);
  if (offset == -1)
    return CC_SENDERROR;
  off_t eof = lseek(fd, 0, SEEK_END);
  if (eof == -1)
    return CC_SENDERROR;
  lseek(fd, offset, SEEK_SET);
  off_t wsize = sendfile(s, fd, &offset, eof - offset);
  if (wsize == SOCKET_ERROR) {
    if (ccsocket_is_errno(EINTR))
      return ccsocket_sendfile(s, fd);
    return ccsocket_is_errno(EWOULDBLOCK) ? CC_SENDWAIT : CC_SENDERROR;
  }
  if (offset == eof)
    return CC_SENDALL;
  lseek(fd, offset, SEEK_SET);
  return ccsocket_sendfile(s, fd);
#elif _AIX
  off_t offset = lseek(fd, 0, SEEK_CUR);
  if (offset == -1)
    return CC_SENDERROR;
  struct sf_parms params = {
    .header_data = NULL, .header_length = 0,   // 无头部数据
    .trailer_data = NULL, .trailer_length = 0, // 无尾部数据
    .file_descriptor = fd, .file_offset = offset, .file_bytes = -1,
  };
  int wsize = send_file(s, &params, 0);
  if (wsize == SOCKET_ERROR) {
    if (ccsocket_is_errno(EINTR))
      return ccsocket_sendfile(s, fd);
    return ccsocket_is_errno(EWOULDBLOCK) ? CC_SENDWAIT : CC_SENDERROR;
  }
  offset = offset + params.bytes_sent;
  if (offset == params.file_size)
    return CC_SENDALL;
  // lseek(fd, size, SEEK_SET);
  return ccsocket_sendfile(s, fd);
#else
#define CC_SENDFILE_PER_LEN 1024
  int wsize;
  uint32_t bsize = CC_SENDFILE_PER_LEN;
  char buffer[CC_SENDFILE_PER_LEN];
  off_t offset = lseek(fd, 0, SEEK_CUR);
  if (offset == SOCKET_ERROR)
    return CC_SENDERROR;
  off_t eof = lseek(fd, 0, SEEK_END);
  if (eof == SOCKET_ERROR)
    return CC_SENDERROR;
  lseek(fd, offset, SEEK_SET);
  while (offset < eof) {
    int rsize = read(fd, buffer, bsize);
    if (rsize == SOCKET_ERROR)
      return CC_SENDERROR;
    if (ccsocket_send(s, buffer, rsize, &wsize) != CC_OPCODE_OK) {
      lseek(fd, offset, SEEK_SET);
      if (ccsocket_is_errno(EINTR))
        return ccsocket_sendfile(s, fd);
      return ccsocket_is_errno(EWOULDBLOCK) ? CC_SENDWAIT : CC_SENDERROR;
    }
    offset += wsize;
  }
  return CC_SENDALL;
#endif
}

/* ---- DNS resolver via ccdns + ccsocket (replaces getaddrinfo) ---------- */

#define MAX_NS 4

#if !defined(_WIN32)
/** @brief Read nameservers from /etc/resolv.conf, up to max. Returns count. */
static int read_dns_servers(char nslist[][CCDNS_MAX_ADDR], int max)
{
  FILE *f = fopen("/etc/resolv.conf", "r");
  if (!f) return 0;
  char line[256];
  int count = 0;
  while (fgets(line, sizeof(line), f) && count < max) {
    if (strncmp(line, "nameserver", 10) == 0) {
      const char *ns = line + 10;
      while (*ns == ' ' || *ns == '\t') ns++;
      const char *end = ns;
      while (*end && *end != '\n' && *end != ' ' && *end != '\t') end++;
      size_t len = (size_t)(end - ns);
      if (len > 0 && len < CCDNS_MAX_ADDR) {
        memcpy(nslist[count], ns, len);
        nslist[count][len] = '\0';
        count++;
      }
    }
  }
  fclose(f);
  return count;
}

#else

/** @brief Read nameservers from Windows registry, up to max. Returns count. */
static int read_dns_servers(char nslist[][CCDNS_MAX_ADDR], int max)
{
  HKEY hKey;
  LONG ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
    "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces",
    0, KEY_READ, &hKey);
  if (ret != ERROR_SUCCESS) return 0;

  DWORD idx = 0;
  int count = 0;

  while (count < max) {
    char guid[256];
    DWORD guidlen = sizeof(guid);
    ret = RegEnumKeyEx(hKey, idx++, guid, &guidlen, NULL, NULL, NULL, NULL);
    if (ret != ERROR_SUCCESS) break;

    HKEY hSub;
    if (RegOpenKeyEx(hKey, guid, 0, KEY_READ, &hSub) != ERROR_SUCCESS)
      continue;

    DWORD type = 0;
    char ns[512];
    DWORD nslen = sizeof(ns);
    ret = RegQueryValueEx(hSub, "NameServer", NULL, &type, (BYTE*)ns, &nslen);
    if (ret == ERROR_SUCCESS && type == REG_SZ && ns[0]) {
      char *p = ns;
      while (*p && count < max) {
        while (*p == ' ' || *p == ',' || *p == '\t') p++;
        if (!*p) break;
        char *end = p;
        while (*end && *end != ' ' && *end != ',' && *end != '\t') end++;
        size_t len = (size_t)(end - p);
        if (len > 0 && len < CCDNS_MAX_ADDR) {
          memcpy(nslist[count], p, len);
          nslist[count][len] = '\0';
          count++;
        }
        p = end;
      }
    }
    RegCloseKey(hSub);
  }

  RegCloseKey(hKey);
  return count;
}
#endif

/* Context passed to the DNS decode callback via udata. */
struct dns_collect_ctx {
  ccaddrinfo_t **head;
  ccaddrinfo_t  *tail;  /* last node (NULL = empty) */
};

/* ccdns_decode callback: collect A/AAAA addresses into the linked list. */
static void on_dns_answer(void *udata, const ccdns_ans_t *ans)
{
  struct dns_collect_ctx *col = (struct dns_collect_ctx *)udata;
  ccsocket_family_t af;
  const char *addr_str = NULL;

  if (ans->type == CCDNS_A && ans->ip[0]) {
    af = CC_INET4;
    addr_str = ans->ip;
  } else if (ans->type == CCDNS_AAAA && ans->ip[0]) {
    af = CC_INET6;
    addr_str = ans->ip;
  } else {
    return;
  }

  /* deduplicate */
  for (ccaddrinfo_t *p = *col->head; p; p = p->next)
    if (!strcmp(p->address, addr_str)) return;

  ccaddrinfo_t *node = (ccaddrinfo_t *)malloc(sizeof(ccaddrinfo_t));
  if (!node) return;
  memset(node, 0, sizeof(*node));
  node->af = af;
  node->ttl = ans->ttl;
  strncpy(node->address, addr_str, sizeof(node->address) - 1);
  node->address[sizeof(node->address) - 1] = '\0';

  if (col->tail)
    col->tail->next = node;
  else
    *col->head = node;
  col->tail = node;
}

/** @brief Do a single DNS query (A or AAAA), decode, collect results. */
static bool dns_query_one(const char *dns_server, struct ccdns_t *dns,
                           const char *domain, ccdns_type_t qtype,
                           struct dns_collect_ctx *col)
{
  uint8_t qbuf[CCDNS_MAX_MSG], rbuf[CCDNS_MAX_MSG];
  int rsize;
  ccsocket_stcode_t st;

  ccsocket_family_t af = ccsocket_get_version(dns_server);
  if (af != CC_INET4 && af != CC_INET6) return false;
  ccsocket_t fd = ccsocket(af, CC_UDP);
  if (fd == INVALID_SOCKET) return false;

  ccsocket_set_rcvtimeout(fd, 5000);
  ccsocket_set_sndtimeout(fd, 3000);

  if (!ccsocket_connect(fd, dns_server, 53)) {
    ccsocket_close(fd);
    return false;
  }

  uint16_t qlen = ccdns_encode(dns, qbuf, sizeof(qbuf), domain, qtype);
  if (!qlen) { ccsocket_close(fd); return false; }

  st = ccsocket_send(fd, qbuf, qlen, NULL);
  if (st != CC_OPCODE_OK) { ccsocket_close(fd); return false; }

  rsize = 0;
  st = ccsocket_recv(fd, (char *)rbuf, sizeof(rbuf), &rsize);
  ccsocket_close(fd);
  if (st != CC_OPCODE_OK || rsize <= 0 || (size_t)rsize > sizeof(rbuf)) return false;

  return ccdns_decode(dns, rbuf, (uint16_t)rsize, col, on_dns_answer) > 0;
}

/** @brief Try dns_query_one across multiple nameservers with retries. */
static bool dns_query_retry(const char nslist[][CCDNS_MAX_ADDR], int nscount,
                             struct ccdns_t *dns, const char *domain,
                             ccdns_type_t qtype, struct dns_collect_ctx *col)
{
  for (int attempt = 0; attempt < 2; attempt++)
    for (int i = 0; i < nscount; i++)
      if (dns_query_one(nslist[i], dns, domain, qtype, col))
        return true;
  return false;
}

bool ccsocket_getaddrinfo(const char *domain, ccaddrinfo_t **addrlist)
{
  if (!domain) { ccsocket_set_errno(EINVAL); return false; }
  if (!addrlist) { ccsocket_set_errno(EINVAL); return false; }
  *addrlist = NULL;
  ccsocket_init_errno();

  /* --- 1. Check if domain is already an IP address --- */
  {
    ccsocket_family_t af = ccsocket_get_version(domain);
    if (af == CC_INET4 || af == CC_INET6) {
      *addrlist = (ccaddrinfo_t*)malloc(sizeof(ccaddrinfo_t));
      if (!*addrlist) return false;
      memset(*addrlist, 0, sizeof(ccaddrinfo_t));
      strncpy((*addrlist)->address, domain, sizeof((*addrlist)->address) - 1);
      (*addrlist)->address[sizeof((*addrlist)->address) - 1] = '\0';
      (*addrlist)->ttl = 0;
      (*addrlist)->af = af;
      return true;
    }
  }

  /* --- 2. Special-case well-known local names --- */
  if (strcmp(domain, "localhost") == 0 || strcmp(domain, "localhost.localdomain") == 0) {
    /* Return 127.0.0.1 (and ::1 if IPv6 is available) */
    *addrlist = (ccaddrinfo_t*)malloc(sizeof(ccaddrinfo_t));
    if (!*addrlist) return false;
    memset(*addrlist, 0, sizeof(ccaddrinfo_t));
    memcpy((*addrlist)->address, "127.0.0.1", 10);
    (*addrlist)->af = CC_INET4;
    (*addrlist)->ttl = 0;
    return true;
  }

  /* --- 3. Get DNS server addresses --- */
  char nslist[MAX_NS][CCDNS_MAX_ADDR];
  int nscount = read_dns_servers(nslist, MAX_NS);
  if (nscount == 0) {
    nscount = 1;
    memcpy(nslist[0], "1.1.1.1", 9);
  }

  /* --- 4. DNS lookup via ccdns + ccsocket --- */
  struct ccdns_t dns;
  struct dns_collect_ctx col;
  bool got_v4 = false, got_v6 = false;

  ccdns_init(&dns);
  ccdns_set_edns(&dns, CCDNS_MAX_MSG, 0);  /* enable EDNS, 4KB payload */
  col.head = addrlist;
  col.tail = NULL;

  /* Query A (IPv4) */
  got_v4 = dns_query_retry(nslist, nscount, &dns, domain, CCDNS_A, &col);

  /* Query AAAA (IPv6) */
  got_v6 = dns_query_retry(nslist, nscount, &dns, domain, CCDNS_AAAA, &col);

  ccdns_close(&dns);

  if ((got_v4 || got_v6) && *addrlist) return true;

  ccsocket_set_errno(EHOSTUNREACH);
  return false;
}

void ccsocket_freeaddrinfo(ccaddrinfo_t *addrlist)
{
  while (addrlist)
  {
    ccaddrinfo_t *addr = addrlist->next;
    free(addrlist);
    addrlist = addr;
  }
}
