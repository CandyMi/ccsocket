#include "ccsocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#if WIN32
  #include <Windows.h>
#else
  #include <unistd.h>
#endif

static int i = 0;
#define CCSOCKET_TEST_FUNCTION(fname, code)    \
static void fname() { code; printf(         \
"%02d. test case : '%s' successed.\n",++i, __FUNCTION__); }

/* test socketpair in all platform. */
CCSOCKET_TEST_FUNCTION(cctest_sockpair, {
  ccsocket_t sv[2]; char buffer[] = "1024";
  char buf[1024]; memset(buf, 0x0, 1024);
  assert(ccsocketpair(sv, (ccsocket_flags_t)(CC_NONBLOCK|CC_CLOEXEC)));
  int wsize = ccsocket_send(sv[0], buffer, strlen(buffer));
  assert(wsize == strlen(buffer));
  int rsize = ccsocket_recv(sv[1], buf, 1024);
  assert(rsize == strlen(buffer));
  assert(!ccsocket_close(sv[0]));
  assert(!ccsocket_close(sv[1]));
})

/* test TCP/UDP socket in all platform. */
CCSOCKET_TEST_FUNCTION(cctest_socketnew, {
  ccsocket_t s4; ccsocket_t s6;
  s4 = ccsocket1(CC_INET4, CC_TCP, (ccsocket_flags_t)(CC_NONBLOCK|CC_CLOEXEC));
  s6 = ccsocket1(CC_INET6, CC_TCP, (ccsocket_flags_t)(CC_NONBLOCK|CC_CLOEXEC));
  assert(s4 != INVALID_SOCKET && s6 != INVALID_SOCKET);
  s4 = ccsocket1(CC_INET4, CC_UDP, (ccsocket_flags_t)(CC_NONBLOCK|CC_CLOEXEC));
  s6 = ccsocket1(CC_INET6, CC_UDP, (ccsocket_flags_t)(CC_NONBLOCK|CC_CLOEXEC));
  assert(s4 != INVALID_SOCKET && s6 != INVALID_SOCKET);
})

CCSOCKET_TEST_FUNCTION(cctest_listen_and_connect, {
  const char ipv4[] = "127.0.0.1"; const char ipv6[] = "::1"; uint16_t port = 7888;
  ccsocket_t s4; ccsocket_t c4; ccsocket_t ss4 = INVALID_SOCKET;
  ccsocket_t s6; ccsocket_t c6; ccsocket_t ss6 = INVALID_SOCKET;
  s4 = ccsocket1(CC_INET4, CC_TCP, (ccsocket_flags_t)(CC_NONBLOCK|CC_CLOEXEC));
  c4 = ccsocket1(CC_INET4, CC_TCP, (ccsocket_flags_t)(CC_NONBLOCK|CC_CLOEXEC));
  s6 = ccsocket1(CC_INET6, CC_TCP, (ccsocket_flags_t)(CC_NONBLOCK|CC_CLOEXEC));
  c6 = ccsocket1(CC_INET6, CC_TCP, (ccsocket_flags_t)(CC_NONBLOCK|CC_CLOEXEC));
  assert(c4 != INVALID_SOCKET && c6 != INVALID_SOCKET);
  assert(s4 != INVALID_SOCKET && s6 != INVALID_SOCKET);
  /* listen ipv6 and ipv4 */
  assert(ccsocket_listen(s4, ipv4, port));
  assert(ccsocket_listen(s6, ipv6, port));
  /* accept ipv6 and ipv4 client socket. */
  ss4 = ccsocket_accept(s4, (ccsocket_flags_t)(CC_NONBLOCK|CC_CLOEXEC));
  assert(!ss4);
  ss6 = ccsocket_accept(s6, (ccsocket_flags_t)(CC_NONBLOCK|CC_CLOEXEC));
  assert(!ss6);
  /* connect to ipv6 and ipv4 */
  ccsocket_connect(c4, ipv4, port);
  ccsocket_connect(c6, ipv6, port);
  /* check connection status. */
  ccsocket_conn_state_t c4state = ccsocket_is_connected(c4);
  assert(c4state == CC_CONNECTING || c4state == CC_CONNECTED);
  ccsocket_conn_state_t c6state = ccsocket_is_connected(c6);
  assert(c6state == CC_CONNECTING || c6state == CC_CONNECTED);
#if _WIN32
  Sleep(10);
#else
  usleep(10000);
#endif
  /* accept socket again */
  ss4 = ccsocket_accept(s4, (ccsocket_flags_t)(CC_NONBLOCK|CC_CLOEXEC));
  ss6 = ccsocket_accept(s6, (ccsocket_flags_t)(CC_NONBLOCK|CC_CLOEXEC));
  assert(ss4 > 0);
  assert(ss6 > 0);
  /* verify client was connected. */
  assert(ccsocket_is_connected(c4) == CC_CONNECTED);
  assert(ccsocket_is_connected(c6) == CC_CONNECTED);
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
  ccsocket_connect(c4, "61.241.54.211", 80); // qq.com
  assert(ccsocket_is_connected(c4) == CC_CONNECTED);

  /* 1. not timeout. */
  const char *req = "GET / HTTP/1.1\r\nHost: www.163.com\r\n\r\n";
  // printf("len = %d\n", ccsocket_send(c4, req, strlen(req)));
  assert(ccsocket_send(c4, req, strlen(req)) == strlen(req));

  char buf[1024]; memset(buf, 0x0, 1024);
  int len = ccsocket_recv(c4, buf, sizeof(buf));
  assert(len > 0);
  // printf("len = %d\n", len);
  // printf("res = '\n%s'", buf);

  /* 2. timeout no read. */
  assert(ccsocket_set_sndtimeout(c4, 2));
  assert(ccsocket_set_rcvtimeout(c4, 2));

  assert(ccsocket_send(c4, req, strlen(req)) == strlen(req));
  assert(ccsocket_recv(c4, buf, sizeof(buf)) == -1);
  /* close*/
  assert(!ccsocket_close(c4));
})

CCSOCKET_TEST_FUNCTION(cctest_check_ip_version, {
  assert(ccsocket_get_version("1.1.1.1") == CC_INET4);
  assert(ccsocket_get_version("255.255.255.255") == CC_INET4);
  assert(ccsocket_get_version("::") == CC_INET6);
  assert(ccsocket_get_version("::1") == CC_INET6);
  assert(ccsocket_get_version("::ffff:127.0.0.1") == CC_INET6);
  assert(ccsocket_get_version("::ffff:1.1.1.1") == CC_INET6);
  assert(ccsocket_get_version("www.163.com") == CC_DOMAIN_INVALID);
})

CCSOCKET_TEST_FUNCTION(cctest_check_setsockopt, {
  ccsocket_t c4 = ccsocket(CC_INET4, CC_TCP);
  ccsocket_t c6 = ccsocket(CC_INET6, CC_TCP);
  assert(c4 > 0);
  assert(c6 > 0);

  assert(ccsocket_get_family(c4) == CC_INET4);
  assert(ccsocket_get_family(c4) == CC_INET4);

  assert(ccsocket_set_nonblock(c4, true));
  assert(ccsocket_set_nonblock(c6, true));

  assert(ccsocket_set_cloexec(c4, true));
  assert(ccsocket_set_cloexec(c6, true));

  assert(ccsocket_set_nodelay(c4, true));
  assert(ccsocket_set_nodelay(c6, true));

  assert(ccsocket_set_keepalive(c4, true));
  assert(ccsocket_set_keepalive(c6, true));

  assert(ccsocket_enable_accept_defer(c4));
  assert(ccsocket_enable_accept_defer(c6));

  assert(!ccsocket_close(c4));
  assert(!ccsocket_close(c6));
})

int main(int argc, char const *argv[])
{
  cctest_sockpair();
  cctest_socketnew();
  cctest_listen_and_connect();
  cctest_check_timeout();
  cctest_check_ip_version();
  cctest_check_setsockopt();
}