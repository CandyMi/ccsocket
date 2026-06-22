/**
 * test_ccsocket_error.c — Error path tests (INVALID_SOCKET, invalid params).
 *
 * Verifies that all public API functions handle bad inputs gracefully
 * (return error codes / false) rather than crashing.
 *
 * These tests never require a live socket or network access.
 */
#include "ccsocket.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int g_pass = 0;
static int g_fail = 0;

static void check(const char *label, bool ok)
{
  printf("[%s] %s\n", ok ? "PASS" : "FAIL", label);
  if (ok) ++g_pass; else ++g_fail;
}

/* ---- INVALID_SOCKET send/recv ---- */
static void test_invalid_send_recv(void)
{
  char buf[16];
  int n;

  /* send1 / recv with INVALID_SOCKET */
  check("send1(INVALID_SOCKET) → CC_OPCODE_ERROR",
        ccsocket_send1(INVALID_SOCKET, buf, sizeof(buf), &n, 0) == CC_OPCODE_ERROR);
  check("recv(INVALID_SOCKET) → CC_OPCODE_ERROR",
        ccsocket_recv(INVALID_SOCKET, buf, sizeof(buf), &n) == CC_OPCODE_ERROR);

  /* sendv1 / recv1 with INVALID_SOCKET */
  ccsocket_iovec_t iov[1];
  ccsocket_init_iov(iov, 1);
  ccsocket_set_iov_buf(iov, 0, buf);
  ccsocket_set_iov_len(iov, 0, sizeof(buf));
  check("sendv1(INVALID_SOCKET) → CC_OPCODE_ERROR",
        ccsocket_sendv1(INVALID_SOCKET, iov, 1, &n, 0) == CC_OPCODE_ERROR);
  check("recv1(INVALID_SOCKET) → CC_OPCODE_ERROR",
        ccsocket_recv1(INVALID_SOCKET, iov, 1, &n) == CC_OPCODE_ERROR);

  /* peek */
  check("peek(INVALID_SOCKET) → CC_OPCODE_ERROR",
        ccsocket_peek(INVALID_SOCKET, buf, sizeof(buf), &n) == CC_OPCODE_ERROR);

  /* sendto */
  check("sendto(INVALID_SOCKET) → CC_OPCODE_ERROR",
        ccsocket_sendto(INVALID_SOCKET, buf, sizeof(buf), "127.0.0.1", 9, &n) == CC_OPCODE_ERROR);
}

/* ---- INVALID_SOCKET setter/getter ---- */
static void test_invalid_options(void)
{
  check("set_nodelay(INVALID_SOCKET) → false",
        ccsocket_set_nodelay(INVALID_SOCKET, true) == false);
  check("set_reuseaddr(INVALID_SOCKET) → false",
        ccsocket_set_reuseaddr(INVALID_SOCKET, true) == false);
  check("set_keepalive(INVALID_SOCKET) → false",
        ccsocket_set_keepalive(INVALID_SOCKET, true) == false);
  check("set_nonblock(INVALID_SOCKET) → false",
        ccsocket_set_nonblock(INVALID_SOCKET, true) == false);
  check("set_cloexec(INVALID_SOCKET) → false",
        ccsocket_set_cloexec(INVALID_SOCKET, true) == false);
}

/* ---- INVALID_SOCKET listen/connect ---- */
static void test_invalid_listen_connect(void)
{
  char addr[MAX_ADDRLEN];
  uint16_t port;

  check("listen(INVALID_SOCKET) → false",
        ccsocket_listen(INVALID_SOCKET, "127.0.0.1", 0) == false);
  check("connect(INVALID_SOCKET) → false",
        ccsocket_connect(INVALID_SOCKET, "127.0.0.1", 9) == false);
  check("get_peername(INVALID_SOCKET) → false",
        ccsocket_get_peername(INVALID_SOCKET, addr, &port) == false);
  check("get_sockname(INVALID_SOCKET) → false",
        ccsocket_get_sockname(INVALID_SOCKET, addr, &port) == false);
}

/* ---- INVALID_SOCKET get_family / get_protocol ---- */
static void test_invalid_query(void)
{
  check("get_family(INVALID_SOCKET) == CC_FAMILY_INVALID",
        ccsocket_get_family(INVALID_SOCKET) == CC_FAMILY_INVALID);
  check("get_protocol(INVALID_SOCKET) == CC_PROTOCOL_INVALID",
        ccsocket_get_protocol(INVALID_SOCKET) == CC_PROTOCOL_INVALID);
}

/* ---- INVALID_SOCKET accept ---- */
static void test_invalid_accept(void)
{
  ccsocket_t c = ccsocket_accept(INVALID_SOCKET, CC_NOFLAG);
  check("accept(INVALID_SOCKET) == INVALID_SOCKET", c == INVALID_SOCKET);
}

/* ---- sendfile with INVALID_SOCKET (if sendfile is compiled in) ---- */
static void test_invalid_sendfile(void)
{
#if defined(CCSOCKET_HAS_SENDFILE)
  /* sendfile(INVALID_SOCKET, ...) should return false */
  int ws = 0;
  check("sendfile(INVALID_SOCKET) → false",
        ccsocket_sendfile(INVALID_SOCKET, -1, 0, 4096, &ws) == false);
#else
  check("sendfile not compiled (skip)", true);
#endif
}

/* ---- NULL / zero-size parameter edge cases ---- */
static void test_null_params(void)
{
  /* Create a UDP socket and connect it, then try send with NULL wsize */
  ccsocket_t s = ccsocket2(CC_INET4, CC_UDP, CC_NOFLAG);
  if (s != INVALID_SOCKET) {
    if (ccsocket_connect(s, "127.0.0.1", 9)) {
      /* NULL wsize is OPTIONAL — should not crash */
      ccsocket_send1(s, "x", 1, NULL, 0);
      check("send1(valid, connected, NULL wsize) — no crash", true);
    }
    ccsocket_close(s);
  }
}

/* ---- sendmsg/recvmsg with invalid socket ---- */
static void test_invalid_msg(void)
{
  ccsocket_iovec_t iov[1];
  ccsocket_msghdr_t msg;
  char ctrl[32];

  ccsocket_init_iov(iov, 1);
  ccsocket_set_iov_buf(iov, 0, (void*)"x");
  ccsocket_set_iov_len(iov, 0, 1);
  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = iov;
  msg.msg_iovlen = 1;
  msg.msg_name[0] = '\0';
  msg.msg_control = ctrl;
  msg.msg_controllen = sizeof(ctrl);

  check("sendmsg(INVALID_SOCKET) → CC_OPCODE_ERROR",
        ccsocket_sendmsg(INVALID_SOCKET, &msg, 0) == CC_OPCODE_ERROR);
  check("recvmsg(INVALID_SOCKET) → CC_OPCODE_ERROR",
        ccsocket_recvmsg(INVALID_SOCKET, &msg, 0) == CC_OPCODE_ERROR);
}

int main(void)
{
  printf("=== error path tests ===\n\n");

  test_invalid_send_recv();
  printf("\n");
  test_invalid_options();
  printf("\n");
  test_invalid_listen_connect();
  printf("\n");
  test_invalid_query();
  printf("\n");
  test_invalid_accept();
  printf("\n");
  test_invalid_sendfile();
  printf("\n");
  test_null_params();
  printf("\n");
  test_invalid_msg();

  printf("\n=== result: %d pass, %d fail ===\n", g_pass, g_fail);
  return g_fail > 0 ? 1 : 0;
}
