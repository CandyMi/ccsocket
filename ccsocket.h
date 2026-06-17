/**
 * @file ccsocket.h
 * @brief Cross-platform socket abstraction library.
 *
 * Provides a unified C API for TCP, UDP, ICMP, and Unix domain sockets
 * across Linux, macOS, FreeBSD, Solaris, AIX, and Windows.
 *
 * @author CandyMi
 * @license MIT
 */

#ifndef CCSOCKET_H
#define CCSOCKET_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#if _WIN32
  /* Shared library export/import:
   *   CCSOCKET_BUILD_SHARED — set by CMake when building the shared library.
   *   CCSOCKET_SHARED       — set by CMake for consumers of the shared library.
   *   Neither defined       — static library; no decoration needed.
   */
  #if defined(CCSOCKET_BUILD_SHARED)
    #define CCSOCKET_EXPORT __declspec(dllexport)
  #elif defined(CCSOCKET_SHARED)
    #define CCSOCKET_EXPORT __declspec(dllimport)
  #else
    #define CCSOCKET_EXPORT
  #endif
  typedef char*    cciovec_buf_t;
  typedef uint32_t cciovec_len_t;
  typedef intptr_t ccsocket_t;
#else
  #define CCSOCKET_EXPORT __attribute__((visibility("default")))
  #ifndef INVALID_SOCKET
    #define INVALID_SOCKET (~0)
  #endif
  typedef void*   cciovec_buf_t;
  typedef size_t  cciovec_len_t;
  typedef int ccsocket_t;
#endif

typedef struct ccsocket_iovec
{
#if _WIN32
  cciovec_len_t  len;
  cciovec_buf_t  buf;
#else
  cciovec_buf_t  buf;
  cciovec_len_t  len;
#endif
} ccsocket_iovec_t;

/* init ccsocket_iovec_t */
#define ccsocket_init_iov(iov, count)         memset((iov), 0x0, sizeof(ccsocket_iovec_t)*(count))
/* get/set ccsocket_iovec_t len */
#define ccsocket_get_iov_len(iov, idx)        (iov)[idx].len
#define ccsocket_set_iov_len(iov, idx, slen)  ccsocket_get_iov_len((iov), (idx)) = (cciovec_len_t)(slen)
/* get/set ccsocket_iovec_t len */
#define ccsocket_get_iov_buf(iov, idx)        (iov)[idx].buf
#define ccsocket_set_iov_buf(iov, idx, sbuf)  ccsocket_get_iov_buf((iov), (idx)) = (cciovec_buf_t)(sbuf)

#ifndef OPTIONAL
  #define OPTIONAL /* When modifying a method parameter, it means that this parameter is optional and can be `NULL`. */
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MAX_ADDRLEN
  #define MAX_ADDRLEN 255
#endif

#ifndef MAX_ERRORLEN
  #define MAX_ERRORLEN 255
#endif

typedef enum {
#define CC_NOFLAG   CC_NOFLAG
  CC_NOFLAG   = 0,
#define CC_CLOEXEC  CC_CLOEXEC
  CC_CLOEXEC  = 1,
#define CC_NONBLOCK CC_NONBLOCK
  CC_NONBLOCK = 2,
} ccsocket_flags_t;

/* Used for `ccsocket_recv`/`ccsocket_send` and `cctls_recv`/`cctls_send`. */
typedef enum {
#define CC_OPCODE_ERROR CC_OPCODE_ERROR
  CC_OPCODE_ERROR         = -1,
#define CC_OPCODE_OK    CC_OPCODE_OK
  CC_OPCODE_OK            =  0,
#define CC_OPCODE_WAIT  CC_OPCODE_WAIT
  CC_OPCODE_WAIT          =  1,
#define CC_OPCODE_WANT_REVENT CC_OPCODE_WANT_REVENT
  CC_OPCODE_WANT_REVENT   =  2,
#define CC_OPCODE_WANT_WEVENT CC_OPCODE_WANT_WEVENT
  CC_OPCODE_WANT_WEVENT   =  3,
} ccsocket_stcode_t;

/* Used for ccsocket* */
typedef enum {
#define CC_FAMILY_INVALID   CC_FAMILY_INVALID 
  CC_FAMILY_INVALID = -1,
#define CC_UNIX  CC_UNIX
  CC_UNIX  = 0,
#define CC_INET4 CC_INET4
  CC_INET4 = 1,
#define CC_INET6 CC_INET6
  CC_INET6 = 2,
} ccsocket_family_t;

/* Used for ccsocket* */
typedef enum {
#define CC_PROTOCOL_INVALID  CC_PROTOCOL_INVALID
  CC_PROTOCOL_INVALID = -1,
#define CC_TCP    CC_TCP
  CC_TCP   = 1,
#define CC_UDP    CC_UDP
  CC_UDP   = 2,
#define CC_ICMP   CC_ICMP  /* raw + icmp */
  CC_ICMP  = 3,
#define CC_ICMP1  CC_ICMP1 /* dgram + icmp */
  CC_ICMP1 = 4,
} ccsocket_protocol_t;

/* Used for ccsocket_connect* */
typedef enum {
#define CC_CONNERROR  CC_CONNERROR
  CC_CONNERROR  = -1,
#define CC_CONNECTED  CC_CONNECTED
  CC_CONNECTED  =  0,
#define CC_CONNECTING CC_CONNECTING
  CC_CONNECTING =  1,
} ccsocket_conn_state_t;

/* Used for ccsocket_sendfile* */
typedef enum {
#define CC_SENDERROR CC_SENDERROR
  CC_SENDERROR  = -1,
#define CC_SENDALL   CC_SENDALL
  CC_SENDALL    =  0,
#define CC_SENDWAIT  CC_SENDWAIT
  CC_SENDWAIT   =  1,
#define CC_SENF_WANT_REVENT CC_SENF_WANT_REVENT
  CC_SENF_WANT_REVENT   =  CC_OPCODE_WANT_REVENT,
#define CC_SENF_WANT_WEVENT CC_SENF_WANT_WEVENT
  CC_SENF_WANT_WEVENT   =  CC_OPCODE_WANT_WEVENT,
} ccsocket_sendf_state_t;

/**
 * @brief Close a socket and release its resources.
 *
 * @param s  Socket handle to close.
 * @return 0 on success, non-zero on failure (see errno / WSAGetLastError).
 */
CCSOCKET_EXPORT int ccsocket_close(ccsocket_t s);

/**
 * @brief Initialise the WinSock library (WSAStartup).
 *
 * Required when the library is linked statically or built as a
 * Release DLL (Debug DLL builds auto-init via DllMain).
 * Safe to call multiple times — only the first call has effect.
 *
 * @return true on success, false on failure.
 */
CCSOCKET_EXPORT bool ccsocket_init(void);

/**
 * @brief Create a pair of connected pipes backed by socket handles.
 *
 * Emulates POSIX pipe() using a socketpair.  The returned handles
 * are ccsocket_t values fully compatible with ccsocket_send/recv/close.
 *
 * Native OS pipe() returns raw file descriptors that CANNOT be used
 * with the ccsocket API — pipe() is therefore redirected to this
 * function so that cross-platform code always gets socket handles.
 *
 * @param sv  Output array of two connected socket handles.
 * @return 0 on success, SOCKET_ERROR on failure.
 */
#undef pipe
#define pipe(fds) ccsocket_pipe(fds)
CCSOCKET_EXPORT int ccsocket_pipe(ccsocket_t sv[2]);

/* create socketpair base `SOCK_STREAM` */
#define ccsocketpair(fds, flags) ccsocketpair1((fds), (ccsocket_flags_t)(flags))
/**
 * @brief Create a pair of connected stream sockets (low-level ABI).
 *
 * Prefer the `ccsocketpair()` convenience macro.
 * On POSIX this wraps socketpair(AF_UNIX, SOCK_STREAM, 0).
 * On Windows it emulates via a TCP loopback connection.
 *
 * @param sv     Output array of two connected socket handles.
 * @param flags  Socket flags (CC_CLOEXEC, CC_NONBLOCK, or CC_NOFLAG).
 * @return true on success, false on failure.
 */
CCSOCKET_EXPORT bool ccsocketpair1(ccsocket_t sv[2], ccsocket_flags_t flags);

/* create `ccsocket` */
#define ccsocket(domain, protocol) ccsocket2((domain), (protocol), CC_NOFLAG)

/* create `ccsocket` with flags */
#define ccsocket1(domain, protocol, flags) ccsocket2((domain), (protocol), (ccsocket_flags_t)(flags))

/**
 * @brief Create a socket with full control over protocol and flags (low-level ABI).
 *
 * Prefer the `ccsocket()` or `ccsocket1()` convenience macros.
 *
 * @param domain  Address family: CC_UNIX, CC_INET4, or CC_INET6.
 * @param proto   Protocol: CC_TCP, CC_UDP, or CC_ICMP.
 * @param flags   Socket flags: CC_NOFLAG, CC_CLOEXEC, CC_NONBLOCK (may be OR'd).
 * @return A valid socket handle, or INVALID_SOCKET on failure.
 */
CCSOCKET_EXPORT ccsocket_t ccsocket2(ccsocket_family_t domain, ccsocket_protocol_t proto, ccsocket_flags_t flags);

/* accept a client from listen `ccsocket`, return `(ccsocket_t)0` when non-block mode syscall want wait events. */
#define ccsocket_accept(s, flags) ccsocket_accept2((s), NULL, NULL, (ccsocket_flags_t)(flags))

/* accept client from listen `ccsocket` with client `address` and `port`, return `(ccsocket_t)0` when non-block mode syscall want wait events. */
#define ccsocket_accept1(s, paddr, pport, flags) ccsocket_accept2((s), (paddr), (pport), (ccsocket_flags_t)(flags))

/**
 * @brief Accept a client connection from a listening socket (low-level ABI).
 *
 * Prefer the `ccsocket_accept()` or `ccsocket_accept1()` convenience macros.
 * In non-blocking mode, returns (ccsocket_t)0 when no connection is available.
 *
 * @param s      Listening socket handle.
 * @param addr   Optional output buffer for the client address (>= MAX_ADDRLEN).
 * @param port   Optional output pointer for the client port.
 * @param flags  Flags for the newly accepted socket.
 * @return Client socket handle, INVALID_SOCKET on error, or 0 if non-blocking and no pending connection.
 */
CCSOCKET_EXPORT ccsocket_t ccsocket_accept2(ccsocket_t s, OPTIONAL char *addr, OPTIONAL uint16_t *port, ccsocket_flags_t flags);

/**
 * @brief Bind a socket to an address and start listening (exclusive mode).
 *
 * Uses SO_EXCLUSIVEADDRUSE (Windows) or SO_EXCLBIND (Solaris) when available;
 * falls back to SO_REUSEADDR on other platforms.
 *
 * @param s     Socket handle (created via ccsocket/ccsocket2).
 * @param addr  IP address string (e.g. "0.0.0.0") or Unix domain path.
 * @param port  Port number (0 for Unix domain sockets).
 * @return true on success, false on failure.
 */
CCSOCKET_EXPORT bool ccsocket_listen(ccsocket_t s, const char *addr, uint16_t port);

/**
 * @brief Bind a socket to an address and start listening (load-balanced mode).
 *
 * Allows multiple processes/threads to share the same port.
 * Supported on Linux 3.9+ (SO_REUSEPORT), FreeBSD 12+ (SO_REUSEPORT_LB),
 * DragonFlyBSD 3.6+, Solaris 11.4+, and AIX 7.2.5.0+.
 *
 * @param s     Socket handle.
 * @param addr  IP address string.
 * @param port  Port number.
 * @return true on success, false on failure.
 */
CCSOCKET_EXPORT bool ccsocket_listen1(ccsocket_t s, const char *addr, uint16_t port);

/**
 * @brief Connect a socket to a remote address.
 *
 * When addr is NULL, the socket is already connected (used internally
 * for connection-state probing on Windows).
 *
 * @param s     Socket handle.
 * @param addr  Target IP address or Unix domain path.
 * @param port  Target port number (0 for Unix domain or raw sockets).
 * @return true on success, false on failure.
 */
CCSOCKET_EXPORT bool ccsocket_connect(ccsocket_t s, const char *addr, uint16_t port);

/**
 * @brief Check whether a socket is connected, still connecting, or in an error state.
 *
 * @param s  Socket handle.
 * @return CC_CONNECTED if connected, CC_CONNECTING if non-blocking connect in progress,
 *         CC_CONNERROR on failure.
 */
CCSOCKET_EXPORT ccsocket_conn_state_t ccsocket_is_connected(ccsocket_t s);

/**
 * @brief Receive data from a connected socket into a buffer.
 *
 * @param s      Socket handle.
 * @param buf    Buffer to receive data into.
 * @param bsize  Capacity of the buffer.
 * @param rsize  Optional: receives the number of bytes actually read.
 * @return CC_OPCODE_OK on success, CC_OPCODE_WAIT if non-blocking and no data,
 *         CC_OPCODE_ERROR on failure.
 */
CCSOCKET_EXPORT ccsocket_stcode_t ccsocket_recv(ccsocket_t s, char *buf, size_t bsize, OPTIONAL int *rsize);
/**
 * @brief Receive data into a scatter/gather iovec array.
 *
 * @param s      Socket handle.
 * @param iov    Array of iovec buffers.
 * @param iovcnt Number of iovec entries.
 * @param rsize  Optional: receives the number of bytes actually read.
 * @return CC_OPCODE_OK, CC_OPCODE_WAIT, or CC_OPCODE_ERROR.
 */
CCSOCKET_EXPORT ccsocket_stcode_t ccsocket_recv1(ccsocket_t s, ccsocket_iovec_t *iov, int iovcnt, OPTIONAL int *rsize);
/**
 * @brief Peek at data on a socket without removing it from the receive buffer.
 *
 * @param s      Socket handle.
 * @param buf    Buffer to receive peeked data.
 * @param bsize  Capacity of the buffer.
 * @param rsize  Optional: receives the number of bytes peeked.
 * @return CC_OPCODE_OK, CC_OPCODE_WAIT, or CC_OPCODE_ERROR.
 */
CCSOCKET_EXPORT ccsocket_stcode_t ccsocket_peek(ccsocket_t s, char* buf, size_t bsize, OPTIONAL int *rsize);

#define ccsocket_send(s, buf, bsize, wsizep) ccsocket_send1((s), (buf), (bsize), (wsizep), 0)
/**
 * @brief Send data to a connected socket (low-level ABI).
 *
 * Prefer the `ccsocket_send()` convenience macro.
 *
 * @param s      Socket handle.
 * @param buf    Data buffer to send.
 * @param bsize  Number of bytes to send.
 * @param wsize  Optional: receives the number of bytes actually written.
 * @param flags  Send flags (platform-specific; pass 0 for normal behaviour).
 * @return CC_OPCODE_OK on success, CC_OPCODE_WAIT if buffer full in non-blocking mode,
 *         CC_OPCODE_ERROR on failure.
 */
CCSOCKET_EXPORT ccsocket_stcode_t ccsocket_send1(ccsocket_t s, const void *buf, size_t bsize, OPTIONAL int *wsize, int flags);

#define ccsocket_sendv(s, iov, iovcnt, wsizep) ccsocket_sendv1((s), (iov), (iovcnt), (wsizep), 0)
/**
 * @brief Send scatter/gather iovec data to a connected socket.
 *
 * Prefer the `ccsocket_sendv()` convenience macro.
 *
 * @param s      Socket handle.
 * @param iov    Array of iovec buffers to send.
 * @param iovcnt Number of iovec entries.
 * @param wsize  Optional: receives the number of bytes actually written.
 * @param flags  Send flags (platform-specific; pass 0 for normal behaviour).
 * @return CC_OPCODE_OK, CC_OPCODE_WAIT, or CC_OPCODE_ERROR.
 */
CCSOCKET_EXPORT ccsocket_stcode_t ccsocket_sendv1(ccsocket_t s, ccsocket_iovec_t *iov, int iovcnt, OPTIONAL int *wsize, int flags);

/**
 * @brief Send the contents of an open file descriptor to a connected socket.
 *
 * Uses zero-copy sendfile() on supported kernels (Linux, macOS, FreeBSD, Solaris);
 * falls back to read()+send() on other platforms.
 * Must be called in a loop until CC_SENDALL is returned.
 *
 * @param s   Socket handle.
 * @param fd  Open file descriptor to send from.
 * @return CC_SENDALL on completion, CC_SENDWAIT if buffer is full,
 *         CC_SENDERROR on failure.
 */
CCSOCKET_EXPORT ccsocket_sendf_state_t ccsocket_sendfile(ccsocket_t s, int fd);

/* ********** Below are some settings that can be used to change the behavior of `ccsocket` ********** */

/**
 * @brief Get human-readable error information for a socket.
 *
 * @param s   Socket handle.
 * @param buf Output buffer of at least MAX_ERRORLEN bytes.
 */
CCSOCKET_EXPORT void ccsocket_get_error(ccsocket_t s, char buf[MAX_ERRORLEN]);

/**
 * @brief Get the remote (peer) address and port of a connected socket.
 *
 * @param s     Socket handle.
 * @param addr  Output buffer for the address string (>= MAX_ADDRLEN).
 * @param port  Output pointer for the port number.
 * @return true on success, false on failure.
 */
CCSOCKET_EXPORT bool ccsocket_get_peername(ccsocket_t s, char *addr, uint16_t *port);
/**
 * @brief Get the local address and port of a socket.
 *
 * @param s     Socket handle.
 * @param addr  Output buffer for the address string (>= MAX_ADDRLEN).
 * @param port  Output pointer for the port number.
 * @return true on success, false on failure.
 */
CCSOCKET_EXPORT bool ccsocket_get_sockname(ccsocket_t s, char *addr, uint16_t *port);

/**
 * @brief Get the address family of a socket.
 *
 * @param s  Socket handle.
 * @return CC_INET4, CC_INET6, CC_UNIX, or CC_FAMILY_INVALID.
 */
CCSOCKET_EXPORT ccsocket_family_t ccsocket_get_family(ccsocket_t s);

/**
 * @brief Get the underlying OS socket type (SOCK_STREAM, SOCK_DGRAM, SOCK_RAW, ...).
 *
 * Useful for runtime protocol detection, e.g. distinguishing a
 * SOCK_DGRAM (CC_ICMP1) from a SOCK_RAW (CC_ICMP) ICMP socket.
 *
 * @param s  Socket handle.
 * @return The socket type (e.g. SOCK_STREAM, SOCK_DGRAM, SOCK_RAW)
 *         on success, or -1 on failure (check errno / WSAGetLastError).
 */
CCSOCKET_EXPORT int ccsocket_get_protocol(ccsocket_t s);

/**
 * @brief Determine the address family from an address string.
 *
 * Parses the string and returns the corresponding family constant.
 * On POSIX, a path to a Unix domain socket is also detected via stat().
 *
 * @param addr  Address string (IPv4, IPv6, or Unix socket path).
 * @return CC_INET4, CC_INET6, CC_UNIX, or CC_FAMILY_INVALID (sets errno = EINVAL).
 */
CCSOCKET_EXPORT ccsocket_family_t ccsocket_get_version(const char *addr);

/**
 * @brief Enable deferred accept on a listening TCP socket.
 *
 * The socket will not complete the connection until data arrives.
 * Supported on Linux (TCP_DEFER_ACCEPT) and FreeBSD (SO_ACCEPTFILTER).
 *
 * @param s  Listening socket handle (must be SOCK_STREAM).
 * @return true on success, false on failure.
 */
CCSOCKET_EXPORT bool ccsocket_enable_accept_defer(ccsocket_t s);

/**
 * @brief Set the receive timeout on a socket.
 *
 * @param s       Socket handle.
 * @param timeout Timeout in milliseconds.
 * @return true on success, false on failure.
 */
CCSOCKET_EXPORT bool ccsocket_set_rcvtimeout(ccsocket_t s, int timeout);
/**
 * @brief Set the send timeout on a socket.
 *
 * @param s       Socket handle.
 * @param timeout Timeout in milliseconds.
 * @return true on success, false on failure.
 */
CCSOCKET_EXPORT bool ccsocket_set_sndtimeout(ccsocket_t s, int timeout);

/**
 * @brief Enable or disable TCP keep-alive on a socket.
 *
 * @param s   Socket handle.
 * @param on  true to enable keep-alive, false to disable.
 * @return true on success, false on failure.
 */
CCSOCKET_EXPORT bool ccsocket_set_keepalive(ccsocket_t s, bool on);

/**
 * @brief Enable or disable the Nagle algorithm (TCP_NODELAY).
 *
 * @param s   Socket handle.
 * @param on  true to disable Nagle (no delay), false to enable.
 * @return true on success, false on failure.
 */
CCSOCKET_EXPORT bool ccsocket_set_nodelay(ccsocket_t s, bool on);

/**
 * @brief Enable or disable SO_REUSEADDR on a socket.
 *
 * @param s   Socket handle.
 * @param on  true to enable, false to disable.
 * @return true on success, false on failure.
 */
CCSOCKET_EXPORT bool ccsocket_set_reuseaddr(ccsocket_t s, bool on);

/**
 * @brief Enable or disable SO_REUSEPORT on a socket.
 *
 * @param s   Socket handle.
 * @param on  true to enable, false to disable.
 * @return true on success, false on failure (e.g. platform does not support SO_REUSEPORT).
 */
CCSOCKET_EXPORT bool ccsocket_set_reuseport(ccsocket_t s, bool on);

/**
 * @brief Set a socket to non-blocking or blocking mode.
 *
 * @param s   Socket handle.
 * @param on  true to set non-blocking, false to restore blocking.
 * @return true on success, false on failure.
 */
CCSOCKET_EXPORT bool ccsocket_set_nonblock(ccsocket_t s, bool on);

/**
 * @brief Enable or disable close-on-exec (FD_CLOEXEC) on a socket.
 *
 * When enabled, the socket will be automatically closed on exec() calls.
 *
 * @param s   Socket handle.
 * @param on  true to enable close-on-exec, false to disable.
 * @return true on success, false on failure.
 */
CCSOCKET_EXPORT bool ccsocket_set_cloexec(ccsocket_t s, bool on);

/* addrinfo */
typedef struct ccaddrinfo
{
  char address[65];
  uint32_t ttl;
  ccsocket_family_t af;
  struct ccaddrinfo *next;
} ccaddrinfo_t;

/**
 * @brief Resolve a domain name to a linked list of IP addresses.
 *
 * Duplicate addresses are automatically removed from the list.
 * The caller must free the list with ccsocket_freeaddrinfo().
 *
 * @param domain    Domain name or IP string to resolve.
 * @param addrlist  Output pointer to the address list head.
 * @return true on success, false on failure (errno is set).
 */
CCSOCKET_EXPORT bool ccsocket_getaddrinfo(const char *domain, ccaddrinfo_t **addrlist);

/**
 * @brief Free an address list returned by ccsocket_getaddrinfo().
 *
 * @param addrlist  Address list head to free (may be NULL).
 */
CCSOCKET_EXPORT void ccsocket_freeaddrinfo(ccaddrinfo_t *addrlist);

#ifdef __cplusplus
}
#endif

#endif /* CCSOCKET_H */
