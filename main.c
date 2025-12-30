#define _CRT_SECURE_NO_WARNINGS
#include "ccsocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#if _WIN32
  #include <io.h>
  #include <fcntl.h>
  #include <Windows.h>
  #define cc_usleep Sleep
  #define close _close
  #define open  _open
#else
  #include <fcntl.h>
  #include <unistd.h>
  #define cc_usleep(timeout) usleep((timeout) * 1000)
#endif
static const char *http_server_domain = "cfadmin.cn";
static int i = 0;
#define CCSOCKET_TEST_FUNCTION(fname, code)                              \
static void fname() {                                                    \
    code;                                                                \
    printf("%02d. test case : '%s' successed.\n",++i, __FUNCTION__);     \
}

static void ccsigaction (int sig)
{
  printf("ccsigaction: %d\n", sig);
  exit(0);
}

static void cc_wait() {
  while (true)
  { cc_usleep(1000); }  
}

void cctest_signal(){
  signal(SIGINT,    ccsigaction);
  signal(SIGTERM,   ccsigaction);
#if defined(SIGBREAK)
  signal(SIGBREAK,  ccsigaction);
#endif
}

/* test socketpair in all platform. */
CCSOCKET_TEST_FUNCTION(cctest_sockpair, {
  ccsocket_t sv[2]; const char *buffer = "1024";
  char buf[1024]; memset(buf, 0x0, 1024);
  bool ok = ccsocketpair(sv, CC_NONBLOCK | CC_CLOEXEC);
  assert(ok);
  int wsize;
  ccsocket_stcode_t state1 = ccsocket_send(sv[0], buffer, strlen(buffer), &wsize);
  assert(state1 == CC_OPCODE_OK && wsize == strlen(buffer));
  int rsize;
  ccsocket_stcode_t state2 = ccsocket_recv(sv[1], buf, 1024, &rsize);
  assert(state2 == CC_OPCODE_OK && rsize == strlen(buffer));
  assert(!ccsocket_close(sv[0]));
  assert(!ccsocket_close(sv[1]));
})

/* test TCP/UDP socket in all platform. */
CCSOCKET_TEST_FUNCTION(cctest_socketnew, {
  ccsocket_t s4; ccsocket_t s6;
  s4 = ccsocket1(CC_INET4, CC_TCP, CC_NONBLOCK|CC_CLOEXEC);
  s6 = ccsocket1(CC_INET6, CC_TCP, CC_NONBLOCK|CC_CLOEXEC);
  assert(s4 != INVALID_SOCKET && s6 != INVALID_SOCKET);
  s4 = ccsocket1(CC_INET4, CC_UDP, CC_NONBLOCK|CC_CLOEXEC);
  s6 = ccsocket1(CC_INET6, CC_UDP, CC_NONBLOCK|CC_CLOEXEC);
  assert(s4 != INVALID_SOCKET && s6 != INVALID_SOCKET);
})

CCSOCKET_TEST_FUNCTION(cctest_listen_and_connect, {
  const char *ipv4 = "127.0.0.1"; const char *ipv6 = "::1"; uint16_t port = 7888;
  ccsocket_t s4; ccsocket_t c4; ccsocket_t ss4 = INVALID_SOCKET;
  ccsocket_t s6; ccsocket_t c6; ccsocket_t ss6 = INVALID_SOCKET;
  s4 = ccsocket1(CC_INET4, CC_TCP, CC_NONBLOCK|CC_CLOEXEC);
  c4 = ccsocket1(CC_INET4, CC_TCP, CC_NONBLOCK|CC_CLOEXEC);
  s6 = ccsocket1(CC_INET6, CC_TCP, CC_NONBLOCK|CC_CLOEXEC);
  c6 = ccsocket1(CC_INET6, CC_TCP, CC_NONBLOCK|CC_CLOEXEC);
  assert(c4 != INVALID_SOCKET && c6 != INVALID_SOCKET);
  assert(s4 != INVALID_SOCKET && s6 != INVALID_SOCKET);
  /* listen ipv6 and ipv4 */
  assert(ccsocket_listen(s4, ipv4, port));
  assert(ccsocket_listen(s6, ipv6, port));
  /* accept ipv6 and ipv4 client socket. */
  ss4 = ccsocket_accept(s4, CC_NONBLOCK|CC_CLOEXEC);
  assert(!ss4);
  ss6 = ccsocket_accept(s6, CC_NONBLOCK|CC_CLOEXEC);
  assert(!ss6);
  /* connect to ipv6 and ipv4 */
  bool ok;
  ok = ccsocket_connect(c4, ipv4, port);
  ok = ccsocket_connect(c6, ipv6, port);
  assert(ok || !ok);
  /* check connection status. */
  ccsocket_conn_state_t c4state = ccsocket_is_connected(c4);
  assert(c4state == CC_CONNECTING || c4state == CC_CONNECTED);
  ccsocket_conn_state_t c6state = ccsocket_is_connected(c6);
  assert(c6state == CC_CONNECTING || c6state == CC_CONNECTED);
  /* sleep a second. */
  cc_usleep(10);
  /* accept socket again */
  ss4 = ccsocket_accept(s4, CC_NONBLOCK|CC_CLOEXEC);
  ss6 = ccsocket_accept(s6, CC_NONBLOCK|CC_CLOEXEC);
  assert(ss4 > 0);
  assert(ss6 > 0);
  /* verify client was connected. */
  c4state = ccsocket_is_connected(c4);
  assert(c4state == CC_CONNECTED);
  c6state = ccsocket_is_connected(c6);
  assert(c6state == CC_CONNECTED);
  /* close all sockets.*/
  // sleep(10);
  assert(!ccsocket_close(s4));
  assert(!ccsocket_close(s6));
  assert(!ccsocket_close(c4));
  assert(!ccsocket_close(c6));
  assert(!ccsocket_close(ss4));
  assert(!ccsocket_close(ss6));
  // sleep(10000);
})

CCSOCKET_TEST_FUNCTION(cctest_check_timeout, {
  ccsocket_t c4 = ccsocket(CC_INET4, CC_TCP);
  assert(c4 > 0);

  ccaddrinfo_t *addrlist = NULL;
  bool r = ccsocket_getaddrinfo(http_server_domain, &addrlist);
  assert(r && addrlist);

  ccaddrinfo_t *addr = addrlist;
  while (addr) {
    if (addr->af == CC_INET4)
      break;
    addr = addr->next;
  }

  ccsocket_connect(c4, addr->address, 80); // cfadmin.cn
  assert(ccsocket_is_connected(c4) == CC_CONNECTED);

  ///* 1. not timeout. */
  const char *req = "GET / HTTP/1.1\r\nHost: cfadmin.cn\r\n\r\n";
  //// printf("len = %d\n", ccsocket_send(c4, req, strlen(req)));
  //int wlen;
  //ccsocket_stcode_t state1 = ccsocket_send(c4, req, strlen(req), &wlen);
  //assert(state1 == CC_OPCODE_OK && wlen == strlen(req));

  ccsocket_iovec_t vec[3]; ccsocket_init_iov(vec, 3);
  const char* req1 = "GET / HTTP/1.1\r\n";
  ccsocket_set_iov_buf(vec, 0, req1);
  ccsocket_set_iov_len(vec, 0, strlen(req1));
  const char* req2 = "Host: cfadmin.cn\r\n\r\n";
  ccsocket_set_iov_buf(vec, 1, req2);
  ccsocket_set_iov_len(vec, 1, strlen(req2));
  int wlen;
  ccsocket_stcode_t state1 = ccsocket_sendv(c4, vec, 2, &wlen);
  assert(state1 == CC_OPCODE_OK && wlen == strlen(req));

  char buf[1024]; memset(buf, 0x0, 1024);
  int rlen;
  ccsocket_stcode_t state2 = ccsocket_recv(c4, buf, sizeof(buf), &rlen);
  assert(state2 == CC_OPCODE_OK && rlen > 0);
  // printf("len = %d\n", len);
  // printf("res = '\n%s'", buf);

  /* 2. timeout no read. */
  assert(ccsocket_set_sndtimeout(c4, 2));
  assert(ccsocket_set_rcvtimeout(c4, 2));

  wlen = 0;
  state1 = ccsocket_send(c4, req, strlen(req), &wlen);
  assert(state1 == CC_OPCODE_OK && wlen == strlen(req));
  rlen = 0;
  state2 = ccsocket_recv(c4, buf, sizeof(buf), &rlen);
  assert(state2 == CC_OPCODE_OK || rlen < 1);
  /* close*/
  assert(!ccsocket_close(c4));
  ccsocket_freeaddrinfo(addrlist);
})

CCSOCKET_TEST_FUNCTION(cctest_check_ip_version, {
  assert(ccsocket_get_version("1.1.1.1") == CC_INET4);
  assert(ccsocket_get_version("255.255.255.255") == CC_INET4);
  assert(ccsocket_get_version("::") == CC_INET6);
  assert(ccsocket_get_version("::1") == CC_INET6);
  assert(ccsocket_get_version("::ffff:127.0.0.1") == CC_INET6);
  assert(ccsocket_get_version("::ffff:1.1.1.1") == CC_INET6);
  assert(ccsocket_get_version("www.163.com") == CC_FAMILY_INVALID);
})

CCSOCKET_TEST_FUNCTION(cctest_check_setsockopt, {
  ccsocket_t s4 = ccsocket(CC_INET4, CC_TCP);
  ccsocket_t s6 = ccsocket(CC_INET6, CC_TCP);
  assert(s4 > 0);
  assert(s6 > 0);

  assert(ccsocket_get_family(s4) == CC_INET4);
  assert(ccsocket_get_family(s6) == CC_INET6);

  assert(ccsocket_set_nonblock(s4, true));
  assert(ccsocket_set_nonblock(s6, true));

  assert(ccsocket_set_cloexec(s4, true));
  assert(ccsocket_set_cloexec(s6, true));

  assert(ccsocket_set_nodelay(s4, true));
  assert(ccsocket_set_nodelay(s6, true));

  assert(ccsocket_set_keepalive(s4, true));
  assert(ccsocket_set_keepalive(s6, true));

  const char *addr4 = "127.0.0.1";
  const char *addr6 = "::1";
  uint16_t port = 7888;
  /**
   * `ccsocket_enable_accept_defer` must after call listen method on bsd.
   */
  bool ok;
  ok = ccsocket_listen(s4, addr4, port);
  assert(ok);
  ok = ccsocket_listen(s6, addr6, port);
  assert(ok);

  assert(ccsocket_enable_accept_defer(s4));
  assert(ccsocket_enable_accept_defer(s6));

  ccsocket_t c4 = ccsocket1(CC_INET4, CC_TCP, CC_NONBLOCK | CC_CLOEXEC);
  ccsocket_t c6 = ccsocket1(CC_INET6, CC_TCP, CC_NONBLOCK | CC_CLOEXEC);
  assert(c4 > 0);
  assert(c6 > 0);

  ok = ccsocket_connect(c4, addr4, port);
  ok = ccsocket_connect(c6, addr6, port);

  bool c4ok = false; bool c6ok = false;
  int no = 0; ccsocket_t c; const char *buf = "hello";

  do {
    cc_usleep(100);
    c = ccsocket_accept(s4, CC_NOFLAG);
    if (c > 0) {
      no++; int ok = !ccsocket_close(c); assert(ok); // printf("s4 accepted.\n");
    }
    c = ccsocket_accept(s6, CC_NOFLAG);
    if (c > 0) {
      no++; int ok = !ccsocket_close(c); assert(ok); // printf("s6 accepted.\n");
    }
    if (!c4ok && ccsocket_is_connected(c4) == CC_CONNECTED) {
      ccsocket_stcode_t state = ccsocket_send(c4, buf, sizeof(buf), NULL);
      assert(CC_OPCODE_OK == state); c4ok = true;
    }
    if (!c6ok && ccsocket_is_connected(c6) == CC_CONNECTED) {
      ccsocket_stcode_t state = ccsocket_send(c6, buf, sizeof(buf), NULL);
      assert(CC_OPCODE_OK == state); c6ok = true;
    }
    // sleep a few minutes.
    if (no < 2)
      printf("no = %d, c4ok = %d, c6ok = %d\n", no, c4ok, c6ok);
  } while (no < 2);

  assert(!ccsocket_close(s4));
  assert(!ccsocket_close(s6));
  assert(!ccsocket_close(c4));
  assert(!ccsocket_close(c6));
})

// 'httpc.test' in project dir.
const char *path[] = {
  "httpc.txt",
  "../httpc.txt",
  "../../httpc.txt",
  "..\\httpc.txt",
  "..\\..\\httpc.txt",
  NULL,
};
CCSOCKET_TEST_FUNCTION(cctest_check_sendfile, {
  ccsocket_t c4 = ccsocket1(CC_INET4, CC_TCP, CC_NONBLOCK | CC_CLOEXEC);
  assert(c4 > 0);

  ccaddrinfo_t* addrlist = NULL;
  bool r = ccsocket_getaddrinfo(http_server_domain, &addrlist);
  assert(r&& addrlist);

  ccaddrinfo_t *addr = addrlist;
  while (addr) {
    if (addr->af == CC_INET4)
      break;
    addr = addr->next;
  }
  ccsocket_connect(c4, addr->address, 80); // cfadmin.cn
  /* nonblock connect like event-driven. */
  do {
    ccsocket_conn_state_t state = ccsocket_is_connected(c4);
    //printf("ccsocket_conn_state_t state = %d\n", state);
    if (state == CC_CONNECTED)
      break;
    assert(state != CC_CONNERROR);
    cc_usleep(10);
  } while (1);

  int fd = -1;
  for (size_t i = 0; i < (sizeof(path) / sizeof(void*)); i++)
  {
    if (!path[i]) break;
    fd = open(path[i], O_RDONLY);
    if (fd > 0) break;
  }
  // sendfile
  assert(fd > 0);
  do {
    ccsocket_sendf_state_t state = ccsocket_sendfile(c4, fd);
    if (state == CC_SENDALL)
      break;
    assert(state != CC_SENDERROR);
    cc_usleep(10);
  } while (1);
  // recv
  int rsize; char buffer[1024];  memset(buffer, 0x0, 1024);
  do {
    ccsocket_stcode_t state = ccsocket_recv(c4, buffer, 1024, &rsize);
    if (state == CC_OPCODE_OK)
      break;
    assert(state != CC_OPCODE_ERROR);
    cc_usleep(10);
  } while(1);
  // printf("http resp = \n'%s'\n", buffer);
  assert(!close(fd));
  assert(!ccsocket_close(c4));
  ccsocket_freeaddrinfo(addrlist);
})

CCSOCKET_TEST_FUNCTION(cctest_check_getaddrinfo, {
  ccaddrinfo_t *addrlist = NULL;
  bool ok = ccsocket_getaddrinfo(http_server_domain, &addrlist);
  assert(ok); int i = 1;
  ccaddrinfo_t *addr = addrlist;
  while (addr) {
    printf("%02d. family = %d, addr = '%s'\n", i++, addr->af, addr->address);
    addr = addr->next;
  }
  ccsocket_freeaddrinfo(addrlist);
})

// #include "cctls.h"
// // CCSOCKET_TEST_FUNCTION(cctest_check_tls, {
// void cctest_check_tls() {
//   ccsocket_t c4 = ccsocket1(CC_INET4, CC_TCP, CC_NONBLOCK | CC_CLOEXEC);
//   assert(c4 > 0);

//   ccaddrinfo_t* addrlist = NULL;
//   bool r = ccsocket_getaddrinfo(http_server_domain, &addrlist);
//   assert(r && addrlist);

//   ccsocket_connect(c4, addrlist->address, 443); // cfadmin.cn
//   /* nonblock connect like event-driven. */
//   do {
//     ccsocket_conn_state_t state = ccsocket_is_connected(c4);
//     printf("1. ccsocket_conn_state_t state = %d, addr = %s\n", state, addrlist->address);
//     if (state == CC_CONNECTED)
//       break;
//     assert(state != CC_CONNERROR);
//     cc_usleep(10);
//   } while (1);

//   // init
//   cctls_init(realloc);

//   tls_t *ctx = cctls_create1(CCTLS_CLIENT_MODE, c4);
//   assert(ctx);
//   printf("s = %d, fd = %d\n", (int)c4, (int)cctls_get_fd(ctx));
//   assert(cctls_get_fd(ctx) == c4);
//   // cctls_set_version(ctx, CCTLS_VERSION_1_1, CCTLS_VERSION_1_1);
//   // cctls_set_version(ctx, CCTLS_VERSION_1_2, CCTLS_VERSION_1_2);
//   // cctls_set_version(ctx, CCTLS_VERSION_1_2, CCTLS_VERSION_1_3);
//   cctls_set_servername(ctx, http_server_domain);

//   const char *protocols[5]; int i = 0;
//   protocols[i++] = "http/1.1";
//   protocols[i++] = "h2";
//   protocols[i++] = "http/1.1";
//   protocols[i++] = "h2";
//   protocols[i++] = NULL;
//   cctls_set_alpn(ctx, protocols);

//   do {
//     ccsocket_stcode_t state = cctls_do_handshake(ctx, NULL);
//     printf("2. ccsocket_conn_state_t state = %d\n", state);
//     if (state == CC_OPCODE_OK)
//         break;
//     assert(state != CC_OPCODE_ERROR);
//     cc_usleep(10);
//   } while (1);

//   int wsize = 0; int rsize = 0;
//   const char *req = "GET / HTTP/1.1\r\nHost: cfadmin.cn\r\n\r\n";
//   size_t wlen = strlen(req);
//   printf("%zu\n", wlen);
//   ccsocket_stcode_t state = cctls_send(ctx, req, wlen, &wsize);
//   printf("%zu, %d, %d\n", wlen, wsize, state);
//   assert(state == CC_OPCODE_OK && wlen == wsize);

//   size_t rlen = 1024;
//   char buffer[1024]; memset(buffer, 0x0, rlen);
//   do {
//     ccsocket_stcode_t state = cctls_recv(ctx, buffer, rlen, &rsize);
//     if (state == CC_OPCODE_OK)
//       break;
//     assert(state != CC_OPCODE_ERROR);
//     cc_usleep(10);
//   } while(1);
//   printf("cctls -> \n'%s'\n", buffer);

//   cctls_destroy(ctx);
//   assert(!ccsocket_close(c4));
// }

int main(int argc, char const *argv[])
{
  cctest_signal();
  cctest_sockpair();
  cctest_socketnew();
  cctest_listen_and_connect();
  cctest_check_timeout();
  cctest_check_getaddrinfo();
  cctest_check_ip_version();
  cctest_check_setsockopt();
  cctest_check_sendfile();
  // cctest_check_tls();

  cc_wait();
}
