#include "cctls.h"

/* 兼容C89/C90 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
  #define CC_INLINE static inline
#else
  #define CC_INLINE static
#endif

#if _WIN32
  #define ccsocket_init_errno() do {errno = 0; WSASetLastError(0);}while(0)
  #define ccsocket_is_errno(err) (WSA##err == WSAGetLastError())
  #define ccsocket_set_errno(err) do{errno = err; WSASetLastError(WSA##err);}while(0)
  #pragma comment(lib, "Crypt32.lib")
#else
#ifndef EWOULDBLOCK
  #define EWOULDBLOCK EAGAIN
#endif
  #define ccsocket_init_errno() errno = 0
  #define ccsocket_is_errno(err) (err == errno)
  #define ccsocket_set_errno(err) errno = err
#endif

#include <string.h>
#include <errno.h>
#include <assert.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

/**
 * 最低版本要求`OpenSSL 1.1.1`, 更低的版本现代化的特性没有! ：）
 */
#if OPENSSL_VERSION_NUMBER < 0x10101000
  #error "OSSL Version to old."
#endif

static int tls_versions[] = {
  TLS1_VERSION,
  TLS1_1_VERSION,
  TLS1_2_VERSION,
  TLS1_3_VERSION,
};

static tls_realloc_t tls_realloc = realloc;

static void* cctls_malloc(size_t num, const char *file, int line)
{
  return tls_realloc(NULL, num);
}

static void* cctls_realloc(void *addr, size_t num, const char *file, int line)
{
  return tls_realloc(addr, num);
}

static void cctls_free(void *addr, const char *file, int line)
{
  tls_realloc(addr, 0);
}

int cctls_init(tls_realloc_t alloc)
{
  /* 定制OpenSSL的内存分配器 */
  if (alloc)
    tls_realloc = alloc;
  CRYPTO_set_mem_functions(cctls_malloc, cctls_realloc, cctls_free);
  /* TODO: 如何最优雅的初始化呢? */
  OPENSSL_init_ssl( OPENSSL_INIT_SSL_DEFAULT, NULL);
  return 0;
}

void cctls_get_error(tls_t *tls, int retcode, char err[MAX_ERRORLEN])
{
  assert(tls);
  ERR_error_string_n(retcode, err, MAX_ERRORLEN);
}

tls_t* cctls_create1(cctls_mode_t mode, ccsocket_t s)
{
  assert(mode == CCTLS_SERVER_MODE || mode == CCTLS_CLIENT_MODE);
  SSL_CTX *ctx = SSL_CTX_new(SSLv23_method());
  if (!ctx)
    return NULL;
  SSL *ssl = SSL_new(ctx);
  if (!ssl) {
    SSL_CTX_free(ctx);
    return NULL;
  }
  SSL_set_SSL_CTX(ssl, ctx);
  if (s > (ccsocket_t)INVALID_SOCKET)
    cctls_set_fd(ssl, s);
  switch (mode)
  {
    case CCTLS_SERVER_MODE:
      SSL_set_accept_state(ssl);
      break;
    case CCTLS_CLIENT_MODE:
      SSL_set_connect_state(ssl);
      break;
    default:
      cctls_destroy(ssl);
      return NULL;
  }
  cctls_set_version(ssl, CCTLS_VERSION_1_0, CCTLS_VERSION_1_3);
  return ssl;
}

void cctls_destroy(tls_t *tls)
{
  if (!tls)
    return;
  SSL_CTX *ctx = SSL_get_SSL_CTX(tls);
  SSL_free(tls);
  SSL_CTX_free(ctx);
}

void cctls_set_fd(tls_t *tls, ccsocket_t s)
{
  SSL_set_fd(tls, (int)s);
}

ccsocket_t cctls_get_fd(tls_t *tls)
{
  return tls ? SSL_get_fd(tls) : INVALID_SOCKET;
}

ccsocket_stcode_t cctls_do_handshake(tls_t *tls, int *retcode)
{
  assert(tls);
  ccsocket_init_errno();
  int code = SSL_do_handshake(tls);
  if (code == 1) 
    return CC_OPCODE_OK;
  int ecode = SSL_get_error(tls, code);
  if (retcode)
    *retcode = ecode;
  // printf("cctls_do_handshake: code = %d, ecode = %d\n", code, ecode);
  switch (ecode)
  {
    case SSL_ERROR_WANT_READ:
      return CC_OPCODE_WANT_REVENT;
    case SSL_ERROR_WANT_WRITE:
      return CC_OPCODE_WANT_WEVENT;
  }
  return CC_OPCODE_ERROR;
}

CC_INLINE
ccsocket_stcode_t _cctls_get_events(tls_t *tls, int code)
{
  // printf("cctls_get_events: code = %d, ecode = %d\n", code, SSL_get_error(tls, code));
  if (code == 1)
    return CC_OPCODE_OK;
  if (ccsocket_is_errno(EWOULDBLOCK))
  {
    if (SSL_want_read(tls)) {
      return CC_OPCODE_WANT_REVENT;
    } else if (SSL_want_write(tls)) {
      return CC_OPCODE_WANT_WEVENT;
    }
  }
  return CC_OPCODE_ERROR;
}

ccsocket_stcode_t cctls_recv(tls_t *tls, void *buffer, size_t len, int *rsize)
{
  assert(tls && buffer && rsize);
  ccsocket_init_errno();
  size_t sz = 0;
  int code = SSL_read_ex(tls, buffer, len, &sz);
  if (sz > 0 && rsize) {
      *rsize = (int)sz;
  }
  return _cctls_get_events(tls, code);
}

ccsocket_stcode_t cctls_send(tls_t *tls, const void *buffer, size_t len, int *wsize)
{
  assert(tls && buffer && wsize);
  ccsocket_init_errno();
  size_t sz = 0;
  int code = SSL_write_ex(tls, buffer, len, &sz);
  if (sz > 0 && wsize) {
      *wsize = (int)sz;
  }
  return _cctls_get_events(tls, code);
}

void cctls_set_version(tls_t *tls, cctls_version_t min_ver, cctls_version_t max_ver)
{
  assert(min_ver >= CCTLS_VERSION_1_0 && min_ver <= CCTLS_VERSION_1_3);
  assert(max_ver >= CCTLS_VERSION_1_0 && max_ver <= CCTLS_VERSION_1_3);
  assert(min_ver <= max_ver);
  SSL_set_min_proto_version(tls, tls_versions[min_ver]);
  SSL_set_max_proto_version(tls, tls_versions[max_ver]);
}

// ccsocket_sendf_state_t cctls_sendfile(tls_t *tls, int fd)
// {
//   return CC_SENDERROR;
// }

void cctls_set_servername(tls_t *tls, const char *domain)
{
  SSL_set_tlsext_host_name(tls, domain);
}

void cctls_set_alpn(tls_t *tls, const char *protocols[])
{
#define TLS_ALPN_MAX_SIZE 512
  if (!protocols)
    return;
  char buffer[TLS_ALPN_MAX_SIZE]; uint8_t *protocol = (uint8_t*)buffer;
  int i = 0; uint32_t bsize = 0;
  while (protocols[i]) {
    size_t len = strlen(protocols[i]);
    /* copy data into buffer. */
    *protocol++ = len & 0xff; memcpy(protocol, protocols[i], len);
    bsize += len + 1; protocol += len; i++;
  }
  /* nothing todo. */
  if (protocol == (uint8_t*)buffer)
    return;
  buffer[bsize] = 0;
  SSL_set_alpn_protos(tls, (const uint8_t *)buffer, bsize);
#undef TLS_ALPN_MAX_SIZE
}
