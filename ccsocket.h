#ifndef CCSOCKET_H
#define CCSOCKET_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#if _WIN32
    #define CCSOCKET_EXPORT __declspec(dllexport)
    typedef intptr_t ccsocket_t;
#else
    #define CCSOCKET_EXPORT __attribute__((visibility("default")))
    typedef int ccsocket_t;
    #define INVALID_SOCKET (~0)
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
#define CC_NOFLAG  CC_NOFLAG
    CC_NOFLAG   = 0, // normal flags
#define CC_CLOEXEC CC_CLOEXEC
    CC_CLOEXEC  = 1, // with cloexec
#define CC_NONBLOCK  CC_NONBLOCK
    CC_NONBLOCK = 2, // with non-block
} ccsocket_flags_t;

typedef enum {
#define CC_UNIX  CC_UNIX
    CC_UNIX  = 0, // AF_UNIX  -> LOCAL
#define CC_INET4 CC_INET4
    CC_INET4 = 1, // AF_INET  -> IPv4
#define CC_INET6 CC_INET6
    CC_INET6 = 2, // AF_INET6 -> IPv6
} ccsocket_domain_t;

typedef enum {
#define CC_TCP  CC_TCP
    CC_TCP   = 1,  // use `so_stream`
#define CC_UDP  CC_UDP
    CC_UDP   = 2,  // use `so_datagram`
#define CC_ICMP CC_ICMP
    CC_ICMP  = 3,  // use `so_raw -> icmp`
} ccsocket_protocol_t;

typedef enum {
#define CC_CONNERROR  CC_CONNERROR
    CC_CONNERROR  = -1, // socket connect failed.
#define CC_CONNECTING CC_CONNECTING
    CC_CONNECTING =  0, // socket connecting(try later)
#define CC_CONNECTED  CC_CONNECTED
    CC_CONNECTED  =  1, // socket connected and succeed
} ccsocket_conn_state_t;

typedef enum {
#define CC_SENDERROR CC_SENDERROR
    CC_SENDERROR  = -1,  // sending error(Unrecoverable).
#define CC_SENDWAIT  CC_SENDWAIT
    CC_SENDWAIT   =  0,  // send buffer was fully.(wait a seconds.)
#define CC_SENDNEXT  CC_SENDNEXT
    CC_SENDNEXT   =  1,  // try call `ccsocket_sendfile` again.
#define CC_SENDALL   CC_SENDALL
    CC_SENDALL    =  2,  // send completed.
} ccsocket_sendf_state_t;

/* close ccsocket */
CCSOCKET_EXPORT int ccsocket_close(ccsocket_t s);

/* create socketpair base `SOCK_STREAM` */
CCSOCKET_EXPORT bool ccsocketpair(ccsocket_t sv[2], ccsocket_flags_t flags);

/* create `ccsocket` */
CCSOCKET_EXPORT ccsocket_t ccsocket(ccsocket_domain_t domain, ccsocket_protocol_t proto);
/* create `ccsocket` with flags */
CCSOCKET_EXPORT ccsocket_t ccsocket1(ccsocket_domain_t domain, ccsocket_protocol_t proto, ccsocket_flags_t flags);

/* accept a client from listen `ccsocket`, return `(ccsocket_t)0` when non-block mode syscall want wait events. */
CCSOCKET_EXPORT ccsocket_t ccsocket_accept(ccsocket_t s, ccsocket_flags_t flags);

/* accept client from listen `ccsocket` with client `address` and `port`, return `(ccsocket_t)0` when non-block mode syscall want wait events. */
CCSOCKET_EXPORT ccsocket_t ccsocket_accept1(ccsocket_t s, char addr[MAX_ADDRLEN], uint16_t *port, ccsocket_flags_t flags);

/* listen a `ccsocket` (Only a listener) */
CCSOCKET_EXPORT bool ccsocket_listen(ccsocket_t s, const char *addr, uint16_t port);

/** listen a `ccsocket` with load balance in supported kernal. 
 *  the following platforms support multi-process load balancing using `ccsocket`:
 *  DragonFly | FreeBSD , but listener must less than `255`.
 *  * Linux 3.9 : SO_REUSEPORT
 *  * DragonFlyBSD 3.6 : SO_REUSEPORT
 *  * FreeBSD 12 : SO_REUSEPORT_LB
 *  * Solaris 11.4 : SO_REUSEPORT
 *  * AIX 7.2.5.0 : SO_REUSEPORT
 */
CCSOCKET_EXPORT bool ccsocket_listen1(ccsocket_t s, const char *addr, uint16_t port);

/* connect to server using ccsocket. */
CCSOCKET_EXPORT bool ccsocket_connect(ccsocket_t s, const char *addr, uint16_t port);

/* check `ccsocket` connecting state. */
CCSOCKET_EXPORT ccsocket_conn_state_t ccsocket_is_connected(ccsocket_t s);

/* Read data sent by the peer from the `ccsocket`. */
CCSOCKET_EXPORT int ccsocket_recv(ccsocket_t s, char *buf, size_t bsize);
/* Read data sent by the peer from the `ccsocket`. */
CCSOCKET_EXPORT int ccsocket_recvfrom(ccsocket_t s, void *buf, size_t bsize, char *addr, uint16_t *port);
/* Sneaking a view of the data sent by the other end through a ccsocket. (platform must be supported) */
CCSOCKET_EXPORT int ccsocket_peek(ccsocket_t s, char* buf, size_t bsize);

/* send buffer to peer `ccsocket` */
CCSOCKET_EXPORT int ccsocket_send(ccsocket_t s, const void *buf, size_t bsize);
/* send buffer to peer `ccsocket` */
CCSOCKET_EXPORT int ccsocket_sendto(ccsocket_t s, const void *buf, size_t bsize, char *addr, uint16_t port);

/* call sendfile using `ccsocket` with `zero-copy` */
CCSOCKET_EXPORT ccsocket_sendf_state_t ccsocket_sendfile(ccsocket_t s, int fd);

/* ********** Below are some settings that can be used to change the behavior of `ccsocket` ********** */

/* get string error information from `ccsocket` */
CCSOCKET_EXPORT void ccsocket_get_error(ccsocket_t s, char buf[MAX_ERRORLEN]);

/* get peer address/port from `ccsocket` */
CCSOCKET_EXPORT bool ccsocket_get_peername(ccsocket_t s, char addr[MAX_ADDRLEN], uint16_t *port);
/* get listen address/port from `ccsocket` */
CCSOCKET_EXPORT bool ccsocket_get_sockname(ccsocket_t s, char addr[MAX_ADDRLEN], uint16_t *port);

/* get `Address Family` from `ccscket`, (e.g : check it's ipv4 or ipv6?) */
CCSOCKET_EXPORT int ccsocket_get_family(ccsocket_t s);

/* set the receive timeout (`timeout` in milliseconds). */
CCSOCKET_EXPORT bool ccsocket_set_rcvtimeout(ccsocket_t s, int timeout);
/* set the timeout period for sending (`timeout` in milliseconds). */
CCSOCKET_EXPORT bool ccsocket_set_sndtimeout(ccsocket_t s, int timeout);

/* Used to enable/disable the `Nagle-algorithm`. */
CCSOCKET_EXPORT bool ccsocket_set_nodelay(ccsocket_t s, bool on);

/* Used to enable/disable the `reuse address`. */
CCSOCKET_EXPORT bool ccsocket_set_reuseaddr(ccsocket_t s, bool on);

/* Used to enable/disable the `reuse port`. */
CCSOCKET_EXPORT bool ccsocket_set_reuseport(ccsocket_t s, bool on);

/* Used to enable/disable the `ccsocket block/nonblock`. */
CCSOCKET_EXPORT bool ccsocket_set_nonblock(ccsocket_t s, bool on);

/* Used to enable/disable whether "sockets will be automatically inherited by sub-processes". */
CCSOCKET_EXPORT bool ccsocket_set_cloexec(ccsocket_t s, bool on);

#ifdef __cplusplus
}
#endif

#endif // CCSOCKET_H