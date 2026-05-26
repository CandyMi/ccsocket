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

#include "cctls.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#if _WIN32
  #include <io.h>
  #define _OFF_T_DEFINED
  #if _WIN64
    #define read  _read
    #define lseek _lseeki64
    typedef int64_t off_t;
  #else
    #define read  _read
    #define lseek _lseek
    typedef int32_t off_t;
  #endif
  #if defined(_MSC_VER)
    #include <BaseTsd.h>
    typedef SSIZE_T ssize_t;
  #endif
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
  #include <unistd.h>
#endif

#include <openssl/ssl.h>
#include <openssl/pem.h>
#include <openssl/opensslv.h>
#include <openssl/err.h>

#if OPENSSL_VERSION_NUMBER < 0x10101000
  #error "We wanna openssl version must more than 1.1.1. : )"
#endif

static const int tls_versions[] = {
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

static int ctls_mypassword_cb(char* buf, int size, int rwflag, void* udata)
{
  size_t len = 0;
  if (udata)
  {
    len = strlen((char*)udata);
    memcpy(buf, udata, len);
  }
  return (int)len;
}

int cctls_init(tls_realloc_t alloc)
{
  if (alloc)
    tls_realloc = alloc;
  CRYPTO_set_mem_functions(cctls_malloc, cctls_realloc, cctls_free);
  return 0;
}

void cctls_get_error(tls_t *tls, int retcode, char err[MAX_ERRORLEN])
{
  assert(tls && err);
  ERR_error_string_n(retcode, err, MAX_ERRORLEN);
}

cctls_mode_t cctls_get_mode(tls_t *tls)
{
  return SSL_is_server(tls) ? CCTLS_SERVER_MODE : CCTLS_CLIENT_MODE;
}

tls_t* cctls_create1(cctls_mode_t mode, ccsocket_t s)
{
  assert(mode == CCTLS_SERVER_MODE || mode == CCTLS_CLIENT_MODE);
  SSL_CTX *ctx = SSL_CTX_new(SSLv23_method());
  if (!ctx)
    return NULL;

  /* load default configure. */
  SSL_CTX_set_default_verify_dir(ctx);
  SSL_CTX_set_default_verify_file(ctx);
  SSL_CTX_set_default_verify_paths(ctx);
  SSL_CTX_set_default_verify_store(ctx);
  /* More flexible compatibility configuration */
  SSL_CTX_set_security_level(ctx, 0);
  SSL_CTX_set_default_passwd_cb(ctx, ctls_mypassword_cb);
  SSL_CTX_set_default_passwd_cb_userdata(ctx, NULL);
  SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
  SSL_CTX_set_min_proto_version(ctx, TLS1_VERSION);
  SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);

  SSL *ssl = SSL_new(ctx);
  if (!ssl)
    return NULL;

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
  return ssl;
}

void cctls_destroy(tls_t *tls)
{
  if (!tls)
    return;
  SSL_free(tls);
}

void cctls_clear(tls_t* tls)
{
  if (tls) SSL_clear(tls);
}

tls_t* cctls_dup(tls_t* tls)
{
  SSL* s = SSL_dup(tls);
  cctls_clear(s);
  return s;
}

bool cctls_init_certificate_and_key(tls_t *tls, const char *chainfile, const char *keyfile)
{
  if (SSL_CTX_use_certificate_chain_file(SSL_get_SSL_CTX(tls), chainfile) <= 0) {
    ERR_print_errors_fp(stderr);
    return false;
  }
  if (SSL_CTX_use_PrivateKey_file(SSL_get_SSL_CTX(tls), keyfile, SSL_FILETYPE_PEM) <= 0) {
    ERR_print_errors_fp(stderr);
    return false;
  }
  return SSL_CTX_check_private_key(SSL_get_SSL_CTX(tls)) == 1; /* check it. */
}

void cctls_set_fd(tls_t *tls, ccsocket_t s)
{
  SSL_set_fd(tls, (int)s);
}

ccsocket_t cctls_get_fd(tls_t *tls)
{
  return tls ? SSL_get_fd(tls) : INVALID_SOCKET;
}

void cctls_set_secure_level(tls_t *tls, int level)
{
  SSL_CTX_set_security_level(SSL_get_SSL_CTX(tls), level);
}

CC_INLINE
ccsocket_stcode_t _cctls_get_events(tls_t *tls, int code)
{
  // printf("cctls_get_events: code = %d, ecode = %d\n", code, SSL_get_error(tls, code));
  if (code == 1)
    return CC_OPCODE_OK;
  
  int state = SSL_get_error(tls, code);
  switch (state)
  {
    case SSL_ERROR_WANT_READ:
    {
      if (ccsocket_is_errno(EWOULDBLOCK))
        return CC_OPCODE_WANT_REVENT;
      break;
    }
    case SSL_ERROR_WANT_WRITE:
    {
      if (ccsocket_is_errno(EWOULDBLOCK))
        return CC_OPCODE_WANT_WEVENT;
    }
  }
  return CC_OPCODE_ERROR;
}

ccsocket_stcode_t cctls_peek(tls_t *tls, void *buffer, size_t len, int *rsize)
{
  assert(tls && buffer && rsize);
  ccsocket_init_errno();
  size_t sz = 0;
  int code = SSL_peek_ex(tls, buffer, len, &sz);
  if (sz > 0 && rsize) {
    *rsize = (int)sz;
  }
  return _cctls_get_events(tls, code);
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

ccsocket_stcode_t cctls_do_handshake(tls_t* tls, int* retcode)
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
  return _cctls_get_events(tls, code);
}

ccsocket_sendf_state_t cctls_sendfile(tls_t *tls, int fd)
{
  ccsocket_init_errno();
#if OPENSSL_VERSION_NUMBER >= 0x30000000
  BIO* wbio = SSL_get_wbio(tls);
  if (wbio && BIO_get_ktls_send(wbio)) {
    off_t cur = lseek(fd, 0, SEEK_CUR);
    if (-1 == cur)
      return CC_SENDERROR;
    off_t max = lseek(fd, 0, SEEK_END);
    if (-1 == max)
      return CC_SENDERROR;
    if (-1 == lseek(fd, cur, SEEK_SET))
      return CC_SENDERROR;
    ssize_t wsize = SSL_sendfile(tls, fd, cur, max - cur, 0);
    if (wsize < 0)
      return cctls_to_sendf_state(_cctls_get_events(tls, (int)wsize));
    lseek(fd, cur + wsize, SEEK_SET);
    if (cur + wsize == max)
      return CC_SENDALL;
    return cctls_sendfile(tls, fd);
  }
#endif
  #define CC_SENDFILE_PER_LEN 4096
  char buf[CC_SENDFILE_PER_LEN];
  off_t cur = lseek(fd, 0, SEEK_CUR);
  if (cur == -1)
    return CC_SENDERROR;
  ssize_t rsize; ssize_t wsize;
  while (1)
  {
    rsize = read(fd, buf, CC_SENDFILE_PER_LEN);
    if (rsize == 0)
      return CC_SENDALL;
    wsize = 0;
    int ret = SSL_write_ex(tls, buf, (size_t)rsize, &wsize);
    if (!ret) {
      lseek(fd, cur, SEEK_SET);
      return cctls_to_sendf_state(_cctls_get_events(tls, ret));
    }
    cur += wsize;
  }
  #undef CC_SENDFILE_PER_LEN
}

/* Safe mapping: ccsocket_stcode_t → ccsocket_sendf_state_t */
CC_INLINE
ccsocket_sendf_state_t cctls_to_sendf_state(ccsocket_stcode_t code)
{
  switch (code) {
    case CC_OPCODE_OK:   return CC_SENDALL;
    case CC_OPCODE_WAIT: return CC_SENDWAIT;
    default:             return CC_SENDERROR;
  }
}

void cctls_set_aio(tls_t *tls)
{
  if (!SSL_get_rbio(tls))
    SSL_set0_rbio(tls, BIO_new(BIO_s_mem()));

  if (!SSL_get_wbio(tls))
    SSL_set0_wbio(tls, BIO_new(BIO_s_mem()));
}

void cctls_set_version(tls_t *tls, cctls_version_t min_ver, cctls_version_t max_ver)
{
  assert(min_ver >= CCTLS_VERSION_1_0 && min_ver <= CCTLS_VERSION_1_3);
  assert(max_ver >= CCTLS_VERSION_1_0 && max_ver <= CCTLS_VERSION_1_3);
  assert(min_ver <= max_ver);
  SSL_set_min_proto_version(tls, tls_versions[min_ver]);
  SSL_set_max_proto_version(tls, tls_versions[max_ver]);
}

void cctls_set_servername(tls_t *tls, const char *domain)
{
  SSL_set_tlsext_host_name(tls, domain);
}

void cctls_set_alpn(tls_t *tls, const char *protocols[])
{
#define TLS_ALPN_MAX_SIZE 128 /* That's really enough. */
  if (!protocols)
    return;
  char buffer[TLS_ALPN_MAX_SIZE]; memset(buffer, 0, TLS_ALPN_MAX_SIZE);
  uint8_t *protocol = (uint8_t*)buffer;
  int i = 0; uint32_t bsize = 0;
  while (protocols[i]) {
    size_t len = strlen(protocols[i]);
    /* copy data into buffer. */
    *protocol++ = len & 0xff; memcpy(protocol, protocols[i], len);
    bsize += (uint32_t)len + 1; protocol += len; i++;
  }
  /* nothing todo. */
  if (protocol == (uint8_t*)buffer)
    return;
  SSL_set_alpn_protos(tls, (const uint8_t *)buffer, bsize);
#undef TLS_ALPN_MAX_SIZE
}
