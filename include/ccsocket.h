/**
 * @file ccsocket.h
 * @brief Cross-platform socket abstraction library.
 *
 * Provides a unified C API for TCP, UDP, ICMP, and Unix domain sockets
 * across Linux, macOS, FreeBSD, Solaris, AIX, and Windows.
 *
 * @author CandyMi
 * @copyright MIT
 */

#ifndef CCSOCKET_H
#define CCSOCKET_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#ifndef CCSOCKET_SOCK_T
  #define CCSOCKET_SOCK_T
  #if _WIN32
    typedef intptr_t ccsocket_t;
  #else
    typedef int ccsocket_t;
  #endif
#endif

#ifndef INVALID_SOCKET
  #define INVALID_SOCKET ((ccsocket_t)(~0))
#endif

#ifndef SOCKET_ERROR
  #define SOCKET_ERROR (-1)
#endif

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
  typedef struct ccsocket_iovec { cciovec_len_t  len; cciovec_buf_t  buf; } ccsocket_iovec_t;
#else
  #define CCSOCKET_EXPORT __attribute__((visibility("default")))
  typedef void*   cciovec_buf_t;
  typedef size_t  cciovec_len_t;
  typedef ccsocket_t SOCKET;
  typedef struct ccsocket_iovec { cciovec_buf_t  buf; cciovec_len_t  len; } ccsocket_iovec_t;
#endif

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
 * @brief Disable sending and/or receiving on a socket.
 *
 * Thin wrapper over shutdown(2) / shutdown() (WinSock).  The `rw` value
 * is platform-portable: 0 = receive side, 1 = send side, 2 = both
 * (SHUT_RD/SHUT_WR/SHUT_RDWR on POSIX, SD_RECEIVE/SD_SEND/SD_BOTH on
 * Windows — numerically identical).  Does not close the socket; it stays
 * open and must still be released with ccsocket_close().
 *
 * @param s   Socket handle.
 * @param rw  0 (receive), 1 (send), or 2 (both).
 * @return 0 on success, SOCKET_ERROR on failure (errno / WSAGetLastError).
 */
CCSOCKET_EXPORT int ccsocket_shutdown(ccsocket_t s, int rw);

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
 * @brief Clean up the WinSock library (WSACleanup).
 *
 * Should be called after all sockets are closed.
 * On POSIX this is a no-op.  Safe to call without a matching init.
 */
CCSOCKET_EXPORT void ccsocket_cleanup(void);

/**
 * @brief Create a pair of connected pipes backed by socket handles.
 * @fn ccsocket_pipe
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
#if defined(_WIN32)
  #ifndef pipe
    #define pipe(fds) ccsocket_pipe(fds)
  #endif
#endif
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
 * @brief Bind a socket to a local address without listening.
 *
 * Separated from ccsocket_listen_internal to expose raw bind() semantics
 * as a public API.  Useful for protocols that need a bound-but-not-listening
 * socket (e.g. UDP, ICMP, or custom connection setup).
 *
 * @param s     Socket handle.
 * @param ip    IP address string (e.g. "0.0.0.0", "::", or NULL for any).
 * @param port  Port number (0 for Unix domain or ephemeral-port binding).
 * @return true on success, false on failure (errno / WSAGetLastError is set).
 */
CCSOCKET_EXPORT bool ccsocket_bind(ccsocket_t s, const char *ip, uint16_t port);

/**
 * @brief Bind a socket to an address and start listening (exclusive mode).
 *
 * Uses SO_EXCLUSIVEADDRUSE (Windows) or SO_EXCLBIND (Solaris) when available;
 * falls back to SO_REUSEADDR on other platforms.
 *
 * @param s        Socket handle (created via ccsocket/ccsocket2).
 * @param addr     IP address string (e.g. "0.0.0.0") or Unix domain path.
 * @param port     Port number (0 for Unix domain sockets).
 * @param backlog  listen() backlog (pass -1 for SOMAXCONN, same as the old behaviour).
 * @return true on success, false on failure.
 */
CCSOCKET_EXPORT bool ccsocket_listen(ccsocket_t s, const char *addr, uint16_t port, int backlog);

/**
 * @brief Bind a socket to an address and start listening (load-balanced mode).
 *
 * Allows multiple processes/threads to share the same port.
 * Supported on Linux 3.9+ (SO_REUSEPORT), FreeBSD 12+ (SO_REUSEPORT_LB),
 * DragonFlyBSD 3.6+, Solaris 11.4+, and AIX 7.2.5.0+.
 *
 * @param s        Socket handle.
 * @param addr     IP address string.
 * @param port     Port number.
 * @param backlog  listen() backlog (pass -1 for SOMAXCONN, same as the old behaviour).
 * @return true on success, false on failure.
 */
CCSOCKET_EXPORT bool ccsocket_listen1(ccsocket_t s, const char *addr, uint16_t port, int backlog);

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
 * No side-effects on any socket type (no reconnect, no buffer mutation).
 *
 * **POSIX**: Uses `getsockopt(SO_ERROR)` + `getpeername()` — pure read-only.
 * Avoids `poll()`/`select()` with 0-timeout, which are unreliable on macOS
 * for TCP sockets that just completed send/recv I/O.
 *
 * **Windows**: Uses `connect(s, NULL, 0)` probe (KB 137973) with a
 * retry loop (not recursion) for EINTR.
 *
 * @param s  Socket handle.
 * @return CC_CONNECTED if connected, CC_CONNECTING if non-blocking connect in progress,
 *         CC_CONNERROR on failure (invalid handle, listen socket, connection refused).
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

/* ========== sendmsg / recvmsg ========== */

/* Flags for ccsocket_recvmsg / ccsocket_sendmsg */
typedef enum {
#define CC_MSG_NOFLAG    CC_MSG_NOFLAG
  CC_MSG_NOFLAG    = 0,
#define CC_MSG_PEEK      CC_MSG_PEEK
  CC_MSG_PEEK      = (1 << 0),  /* recv: peek without consuming */
#define CC_MSG_WAITALL   CC_MSG_WAITALL
  CC_MSG_WAITALL   = (1 << 1),  /* recv: block until full request satisfied */
#define CC_MSG_DONTWAIT  CC_MSG_DONTWAIT
  CC_MSG_DONTWAIT  = (1 << 2),  /* send/recv: non-blocking for this call only */
#define CC_MSG_NOSIGNAL  CC_MSG_NOSIGNAL
  CC_MSG_NOSIGNAL  = (1 << 3),  /* send: suppress SIGPIPE (auto-added on POSIX) */
#define CC_MSG_MORE      CC_MSG_MORE
  CC_MSG_MORE      = (1 << 4),  /* send: more data coming (TCP_CORK hint) */
#define CC_MSG_OOB       CC_MSG_OOB
  CC_MSG_OOB       = (1 << 5),  /* send/recv: out-of-band data */
} ccsocket_msg_flags_t;

/* Return flags written to msg_flags by ccsocket_recvmsg */
typedef enum {
#define CC_MSG_RET_TRUNC   CC_MSG_RET_TRUNC
  CC_MSG_RET_TRUNC   = (1 << 0),  /* data truncated (UDP packet > buffer) */
#define CC_MSG_RET_CTRUNC  CC_MSG_RET_CTRUNC
  CC_MSG_RET_CTRUNC  = (1 << 1),  /* control data truncated */
#define CC_MSG_RET_EOR     CC_MSG_RET_EOR
  CC_MSG_RET_EOR     = (1 << 2),  /* end of record */
#define CC_MSG_RET_OOB     CC_MSG_RET_OOB
  CC_MSG_RET_OOB     = (1 << 3),  /* out-of-band data received */
#define CC_MSG_RET_BCAST   CC_MSG_RET_BCAST
  CC_MSG_RET_BCAST   = (1 << 4),  /* broadcast packet */
#define CC_MSG_RET_MCAST   CC_MSG_RET_MCAST
  CC_MSG_RET_MCAST   = (1 << 5),  /* multicast packet */
} ccsocket_msg_ret_flags_t;

/**
 * @brief Cross-platform ancillary message header (CMSG).
 *
 * Layout-compatible with both POSIX struct cmsghdr and Windows WSACMSGHDR.
 * Fields match exactly: cmsg_len is 32-bit, cmsg_level and cmsg_type are int.
 * Use the CC_CMSG_* macros to walk the control buffer.
 *
 * @see CC_CMSG_FIRSTHDR
 * @see CC_CMSG_NXTHDR
 * @see CC_CMSG_DATA
 */
typedef struct ccsocket_cmsghdr {
    uint32_t cmsg_len;    /* data length including header */
    int      cmsg_level;  /* protocol level (SOL_SOCKET, IPPROTO_IP, ...) */
    int      cmsg_type;   /* message type identifier */
    /* followed by aligned payload data */
} ccsocket_cmsghdr_t;

#if _WIN32
  #define CC_CMSG_ALIGN(len)  (((len) + sizeof(DWORD) - 1) & ~(sizeof(DWORD) - 1))
#else
  #define CC_CMSG_ALIGN(len)  (((len) + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1))
#endif

/**
 * @def CC_CMSG_FIRSTHDR
 * @brief Return a pointer to the first ccsocket_cmsghdr_t in a control buffer.
 * @param ctl      Pointer to the control buffer (msg_control).
 * @param ctl_len  Length of the control buffer (msg_controllen).
 */
#define CC_CMSG_FIRSTHDR(ctl, ctl_len) \
    (((void *)(ctl) != NULL && (size_t)(ctl_len) >= sizeof(ccsocket_cmsghdr_t)) \
        ? (ccsocket_cmsghdr_t *)(ctl) \
        : (ccsocket_cmsghdr_t *)0)

/**
 * @def CC_CMSG_NXTHDR
 * @brief Return a pointer to the next ccsocket_cmsghdr_t in a control buffer.
 * @param ctl      Pointer to the control buffer (msg_control).
 * @param ctl_len  Length of the control buffer (msg_controllen).
 * @param cmsg     Pointer to the current ccsocket_cmsghdr_t.
 */
#define CC_CMSG_NXTHDR(ctl, ctl_len, cmsg) \
    (!(cmsg) \
     ? (ccsocket_cmsghdr_t *)0 \
     : ((char *)(cmsg) + CC_CMSG_ALIGN((cmsg)->cmsg_len) \
        >= (char *)(ctl) + (size_t)(ctl_len)) \
       ? (ccsocket_cmsghdr_t *)0 \
       : (ccsocket_cmsghdr_t *)(((char *)(cmsg) + CC_CMSG_ALIGN((cmsg)->cmsg_len))))

/**
 * @def CC_CMSG_DATA
 * @brief Return a pointer to the payload data following a ccsocket_cmsghdr_t.
 * @param cmsg  Pointer to the ccsocket_cmsghdr_t.
 */
#define CC_CMSG_DATA(cmsg) \
    ((unsigned char *)(cmsg) + CC_CMSG_ALIGN(sizeof(ccsocket_cmsghdr_t)))

/**
 * @def CC_CMSG_SPACE
 * @brief Total bytes needed for a cmsghdr with a given payload length (includes alignment).
 * @param len  Payload length.
 */
#define CC_CMSG_SPACE(len) \
    (CC_CMSG_ALIGN(sizeof(ccsocket_cmsghdr_t) + (len)))

/**
 * @def CC_CMSG_LEN
 * @brief Total cmsg_len value for a cmsghdr with a given payload length.
 * @param len  Payload length.
 */
#define CC_CMSG_LEN(len) \
    (CC_CMSG_ALIGN(sizeof(ccsocket_cmsghdr_t)) + (len))

/**
 * @brief Message header for ccsocket_recvmsg / ccsocket_sendmsg.
 *
 * Designed as a cross-platform abstraction over POSIX struct msghdr
 * and Windows WSAMSG.
 *
 * Fields msg_iov/msg_iovlen mirror struct iovec/WSABUF.
 * Fields msg_name/msg_port work like sendto/recvfrom:
 *   - recvmsg: output (source address)
 *   - sendmsg: input  (destination address, set before call)
 * Field msg_control(msg_controllen provide access to ancillary data (CMSG).
 * Walk the control buffer with CC_CMSG_FIRSTHDR / CC_CMSG_NXTHDR.
 */
typedef struct ccsocket_msghdr {
    ccsocket_iovec_t     *msg_iov;         /* [in] scatter/gather buffer array */
    int                   msg_iovlen;      /* [in] number of iovec entries */
    char                  msg_name[65];    /* [in/out] send:dst addr / recv:src addr */
    uint16_t              msg_port;        /* [in/out] send:dst port / recv:src port */
    void                 *msg_control;     /* [in/out] CMSG ancillary data buffer */
    size_t                msg_controllen;  /* [in/out] buffer size → actual used */
    int                   msg_flags;       /* [out] CC_MSG_RET_* bitmask (recvmsg only) */
    int                   msg_bytes;       /* [out] actual bytes transferred */
} ccsocket_msghdr_t;

/**
 * @brief Receive a message with full control (source address, scatter/gather,
 *        ancillary data, and arbitrary flags).
 *
 * Provides direct access to the underlying recvmsg() / WSARecvMsg().
 * Source address is populated for connectionless sockets (UDP, ICMP/DGRAM).
 * Stream sockets (TCP) should prefer ccsocket_recv/recv1.
 *
 * Cross-platform flag notes:
 *   - CC_MSG_DONTWAIT: not supported on Windows (flag is silently ignored).
 *   - CC_MSG_MORE:     Linux only; silently ignored on other platforms.
 *
 * @param s      Socket handle.
 * @param msg    Message header — see ccsocket_msghdr_t field docs.
 * @param flags  Receive flags (CC_MSG_PEEK, CC_MSG_WAITALL, CC_MSG_DONTWAIT,
 *               CC_MSG_OOB; may be OR'd).  Pass CC_MSG_NOFLAG for default.
 * @return CC_OPCODE_OK on success, CC_OPCODE_WAIT if non-blocking and no data,
 *         CC_OPCODE_ERROR on failure.
 */
CCSOCKET_EXPORT ccsocket_stcode_t ccsocket_recvmsg(ccsocket_t s, ccsocket_msghdr_t *msg, ccsocket_msg_flags_t flags);

/**
 * @brief Send a message with full control (destination address, scatter/gather,
 *        ancillary data, and arbitrary flags).
 *
 * Provides direct access to the underlying sendmsg() / WSASendMsg().
 * For connected sockets (TCP), msg_name/msg_port are ignored.
 * For connectionless sockets (UDP, ICMP/DGRAM), set msg_name/msg_port to
 * specify the destination (sendto semantics).  Leave msg_name[0] == '\0'
 * to skip destination (equivalent to sendv1 for connected sockets).
 *
 * POSIX automatically adds MSG_NOSIGNAL to suppress SIGPIPE (matching the
 * behaviour of ccsocket_sendv1).
 *
 * Cross-platform flag notes:
 *   - CC_MSG_DONTWAIT: not supported on Windows (flag is silently ignored).
 *   - CC_MSG_NOSIGNAL: only relevant on Linux; auto-added on POSIX, ignored
 *     on Windows (no SIGPIPE).
 *   - CC_MSG_MORE:     Linux only; silently ignored on other platforms.
 *
 * @param s      Socket handle.
 * @param msg    Message header — see ccsocket_msghdr_t field docs.
 * @param flags  Send flags (CC_MSG_DONTWAIT, CC_MSG_NOSIGNAL, CC_MSG_MORE,
 *               CC_MSG_OOB; may be OR'd).  Pass CC_MSG_NOFLAG for default.
 * @return CC_OPCODE_OK on success, CC_OPCODE_WAIT if buffer full in non-blocking mode,
 *         CC_OPCODE_ERROR on failure.
 */
CCSOCKET_EXPORT ccsocket_stcode_t ccsocket_sendmsg(ccsocket_t s, ccsocket_msghdr_t *msg, ccsocket_msg_flags_t flags);

/**
 * @brief Send data to a specific destination (sendto semantics).
 *
 * Thin wrapper over ccsocket_sendmsg for the common single-buffer case.
 * For connected sockets, addr may be NULL (msg_name is skipped silently).
 *
 * @param s      Socket handle.
 * @param buf    Data buffer to send.
 * @param bsize  Number of bytes to send.
 * @param addr   Target address string (NULL for connected sockets).
 * @param port   Target port (0 for raw/Unix sockets).
 * @param wsize  Optional: receives the number of bytes actually written.
 * @return CC_OPCODE_OK, CC_OPCODE_WAIT, or CC_OPCODE_ERROR.
 */
CCSOCKET_EXPORT ccsocket_stcode_t ccsocket_sendto(ccsocket_t s, const void *buf, size_t bsize,
                                                   const char *addr, uint16_t port,
                                                   OPTIONAL int *wsize);

/**
 * @brief Receive data from a socket, capturing the source address (recvfrom semantics).
 *
 * Thin wrapper over ccsocket_recvmsg for the common single-buffer case.
 * Source address and port are written when addr/port are non-NULL.
 *
 * @param s      Socket handle.
 * @param buf    Buffer to receive data into.
 * @param bsize  Capacity of the buffer.
 * @param addr   Optional: receives the source address string (>= MAX_ADDRLEN).
 * @param port   Optional: receives the source port.
 * @param rsize  Optional: receives the number of bytes actually read.
 * @return CC_OPCODE_OK, CC_OPCODE_WAIT, or CC_OPCODE_ERROR.
 */
CCSOCKET_EXPORT ccsocket_stcode_t ccsocket_recvfrom(ccsocket_t s, char *buf, size_t bsize,
                                                     OPTIONAL char *addr, OPTIONAL uint16_t *port,
                                                     OPTIONAL int *rsize);

/* ********** Below are some settings that can be used to change the behavior of `ccsocket` ********** */

/**
 * @brief Get human-readable error information for a socket.
 *
 * @param s   Socket handle.
 * @param buf Output buffer of at least MAX_ERRORLEN bytes.
 */
CCSOCKET_EXPORT void ccsocket_get_error(ccsocket_t s, char buf[MAX_ERRORLEN]);

/**
 * @brief Get the number of bytes available for reading (FIONREAD).
 *
 * Returns the amount of data queued in the socket receive buffer
 * without consuming it. Works on TCP, UDP, and raw sockets.
 *
 * @param s      Socket handle.
 * @param nread  Output pointer for the available byte count.
 * @return true on success, false on failure.
 */
CCSOCKET_EXPORT bool ccsocket_get_nread(ccsocket_t s, uint32_t *nread);

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
 * @brief Get the protocol of a socket as a ccsocket_protocol_t value.
 *
 * Maps the OS socket type (SO_TYPE) to the library's protocol enum:
 *   SOCK_STREAM → CC_TCP
 *   SOCK_DGRAM  → CC_UDP or CC_ICMP1 (distinguished by protocol number)
 *   SOCK_RAW    → CC_ICMP
 *
 * For Unix domain sockets, SO_TYPE is SOCK_STREAM or SOCK_DGRAM,
 * so the return value is CC_TCP or CC_UDP respectively.  To check
 * the address family, use ccsocket_get_family().
 *
 * Useful for runtime protocol detection, e.g. distinguishing a
 * CC_ICMP1 (SOCK_DGRAM) from a CC_ICMP (SOCK_RAW) ICMP socket.
 *
 * @param s  Socket handle.
 * @return CC_TCP, CC_UDP, CC_ICMP, CC_ICMP1 on success,
 *         or CC_PROTOCOL_INVALID on failure (check errno / WSAGetLastError).
 */
CCSOCKET_EXPORT ccsocket_protocol_t ccsocket_get_protocol(ccsocket_t s);

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
 * @brief Parse an address string into raw binary address bytes.
 *
 * Writes the network-byte-order address to @p out (4 bytes for IPv4,
 * 16 bytes for IPv6).  Returns the address family so the caller knows
 * how many bytes are valid.
 *
 * Reuses the same platform-specific parsing (inet_pton / WSAStringToAddress)
 * as ccsocket_get_version() but returns the bytes instead of just the family.
 *
 * @param addr  Address string (IPv4 or IPv6, NULL returns CC_FAMILY_INVALID).
 * @param out   Output buffer (must be at least 16 bytes).
 * @return CC_INET4, CC_INET6, or CC_FAMILY_INVALID on error.
 */
CCSOCKET_EXPORT ccsocket_family_t ccsocket_get_addrbytes(const char *addr, uint8_t out[16]);

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
