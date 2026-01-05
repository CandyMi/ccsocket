#ifndef CCTLS_H
#define CCTLS_H

#include "ccsocket.h"

typedef void tls_t;

typedef enum {
#define CCTLS_CLIENT_MODE CCTLS_CLIENT_MODE
  CCTLS_CLIENT_MODE = 1, // when you wanna connect to server.
#define CCTLS_SERVER_MODE CCTLS_SERVER_MODE
  CCTLS_SERVER_MODE = 2, // when you wanna listen client coming.
} cctls_mode_t;

typedef enum {
#define CCTLS_VERSION_1_0 CCTLS_VERSION_1_0
  CCTLS_VERSION_1_0 = 0,
#define CCTLS_VERSION_1_1 CCTLS_VERSION_1_1
  CCTLS_VERSION_1_1 = 1,
#define CCTLS_VERSION_1_2 CCTLS_VERSION_1_2
  CCTLS_VERSION_1_2 = 2,
#define CCTLS_VERSION_1_3 CCTLS_VERSION_1_3
  CCTLS_VERSION_1_3 = 3,
} cctls_version_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Optional user-defined memory allocator */
typedef void*(*tls_realloc_t)(void*, size_t);

/* init openssl */
CCSOCKET_EXPORT int cctls_init(tls_realloc_t alloc);

/* create tls context. */
#define cctls_create(mode) cctls_create1((mode), (INVALID_SOCKET))
/* create tls context with socket. */
CCSOCKET_EXPORT tls_t* cctls_create1(cctls_mode_t mode, ccsocket_t s);

/* clear/reset tls, reuse to new connection. */
CCSOCKET_EXPORT void cctls_clear(tls_t* tls);

/* dup new tls context. */
CCSOCKET_EXPORT tls_t* cctls_dup(tls_t* tls);

/* init cert and key to tls context from file. */
CCSOCKET_EXPORT bool cctls_init_certificate_and_key(tls_t* tls, const char* chainfile, const char* keyfile);

/* get tls run mode (CCTLS_CLIENT_MODE/CCTLS_SERVER_MODE). */
CCSOCKET_EXPORT cctls_mode_t cctls_get_mode(tls_t* tls);

/* set tls securety level (0 ~ 5). */
CCSOCKET_EXPORT void cctls_set_secure_level(tls_t* tls, int level);

/* tls context destory. */
CCSOCKET_EXPORT void cctls_destroy(tls_t *ctx);

/* set cctls version. */
CCSOCKET_EXPORT void cctls_set_version(tls_t *tls, cctls_version_t min_ver, cctls_version_t max_ver);

/* get socket to tls context */
CCSOCKET_EXPORT ccsocket_t cctls_get_fd(tls_t *tls);

/* set socket from tls context */
CCSOCKET_EXPORT void cctls_set_fd(tls_t *tls, ccsocket_t s);

/* Perform a tls handshake immediately after the socket connection is established. */
CCSOCKET_EXPORT ccsocket_stcode_t cctls_do_handshake(tls_t *tls, int *retcode);

/* get error information from `retcode` */
CCSOCKET_EXPORT void cctls_get_error(tls_t *tls, int retcode, char err[MAX_ERRORLEN]);

/* peek tls data */
CCSOCKET_EXPORT ccsocket_stcode_t cctls_peek(tls_t *tls, void *buffer, size_t len, int *rsize);

/* recv tls data */
CCSOCKET_EXPORT ccsocket_stcode_t cctls_recv(tls_t *tls, void *buffer, size_t len, int *rsize);

/* send tls data */
CCSOCKET_EXPORT ccsocket_stcode_t cctls_send(tls_t *tls, const void *buffer, size_t len, int *wsize);

/* send tls file data */
CCSOCKET_EXPORT ccsocket_sendf_state_t cctls_sendfile(tls_t *tls, int fd);

/* set alpn for tls client. */
CCSOCKET_EXPORT void cctls_set_alpn(tls_t *tls, const char *protocols[]);

/* set peer server name for tls client. */
CCSOCKET_EXPORT void cctls_set_servername(tls_t *tls, const char *domain);

#ifdef __cplusplus
}
#endif
#endif
