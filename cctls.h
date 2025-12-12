#ifndef CCTLS_H
#define CCTLS_H

#include "ccsocket.h"

typedef void tls_t; // 抽象套接字

typedef enum {
#define CCTLS_CLIENT_MODE CCTLS_CLIENT_MODE
  CCTLS_CLIENT_MODE = 1, // 连接到服务器选这个
#define CCTLS_SERVER_MODE CCTLS_SERVER_MODE
  CCTLS_SERVER_MODE = 2, // 监听客户端就选这个
} cctls_mode_t;

#ifdef __cplusplus
extern "C" {
#endif

/* 自定义内存分配器, 传入标准库`realloc`或自行封装的分配器 */
typedef void*(*tls_realloc_t)(void*, size_t);

/* init openssl */
CCSOCKET_EXPORT int cctls_init(tls_realloc_t alloc);

/* create tls context. */
#define cctls_create(mode) cctls_create1((mode), (INVALID_SOCKET))
/* create tls context with socket. */
CCSOCKET_EXPORT tls_t* cctls_create1(cctls_mode_t mode, ccsocket_t s);

/* tls context destory. */
CCSOCKET_EXPORT void cctls_destroy(tls_t *ctx);

/* set socket to tls context */
CCSOCKET_EXPORT ccsocket_t cctls_get_fd(tls_t* tls);

/* get socket from tls context */
CCSOCKET_EXPORT void cctls_set_fd(tls_t* tls, ccsocket_t s);

/* Perform a tls handshake immediately after the socket connection is established. */
CCSOCKET_EXPORT ccsocket_stcode_t cctls_do_handshake(tls_t* tls, int *retcode);

/* get error information from `retcode` */
CCSOCKET_EXPORT void cctls_get_error(tls_t* tls, int retcode, char err[MAX_ERRORLEN]);

/* recv tls data */
CCSOCKET_EXPORT ccsocket_stcode_t cctls_recv(tls_t *tls, void *buffer, size_t *len);

/* send tls data */
CCSOCKET_EXPORT ccsocket_stcode_t cctls_send(tls_t *tls, const void *buffer, size_t *len);

/* set alpn for tls client. */
CCSOCKET_EXPORT void cctls_set_alpn(tls_t *tls, const char *protocols[]);

/* set peer server name for tls client. */
CCSOCKET_EXPORT void cctls_set_servername(tls_t* tls, const char *domain);

#ifdef __cplusplus
}
#endif
#endif