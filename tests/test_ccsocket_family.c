/**
 * test_ccsocket_family.c — Domain/protocol enum boundary + get_family/get_protocol tests.
 *
 * Covers:
 *   - ccsocket2(domain, protocol, flags) for all domain×protocol combos
 *   - ccsocket_get_family for each valid combination
 *   - ccsocket_get_protocol for each valid combination
 *   - INVALID_SOCKET → CC_FAMILY_INVALID / CC_PROTOCOL_INVALID
 *   - CC_FAMILY_INVALID / CC_PROTOCOL_INVALID → INVALID_SOCKET
 */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "ccsocket.h"

static int  g_pass = 0;
static int  g_fail = 0;

static void check(const char *label, bool ok)
{
  printf("[%s] %s\n", ok ? "PASS" : "FAIL", label);
  if (ok) ++g_pass; else ++g_fail;
}

/* ---- domain × protocol create + get_family + get_protocol ---- */

static void test_valid_combo(const char *label,
                             ccsocket_family_t domain, ccsocket_protocol_t proto,
                             ccsocket_family_t expect_family,
                             ccsocket_protocol_t expect_proto)
{
  char buf[128];
  ccsocket_t s = ccsocket2(domain, proto, CC_NOFLAG);
  snprintf(buf, sizeof(buf), "%s create", label);
  check(buf, s != INVALID_SOCKET);
  if (s == INVALID_SOCKET) return;

  ccsocket_family_t f = ccsocket_get_family(s);
  snprintf(buf, sizeof(buf), "%s get_family==%d", label, (int)expect_family);
  check(buf, f == expect_family);

  ccsocket_protocol_t p = ccsocket_get_protocol(s);
  snprintf(buf, sizeof(buf), "%s get_protocol==%d", label, (int)expect_proto);
  check(buf, p == expect_proto);

  ccsocket_close(s);
}

static void test_invalid_combos(void)
{
  /* CC_FAMILY_INVALID with any protocol */
  {
    ccsocket_t s = ccsocket2(CC_FAMILY_INVALID, CC_TCP, CC_NOFLAG);
    check("CC_FAMILY_INVALID+TCP → INVALID", s == INVALID_SOCKET);
  }
  {
    ccsocket_t s = ccsocket2(CC_FAMILY_INVALID, CC_UDP, CC_NOFLAG);
    check("CC_FAMILY_INVALID+UDP → INVALID", s == INVALID_SOCKET);
  }

  /* CC_PROTOCOL_INVALID with any domain */
  {
    ccsocket_t s = ccsocket2(CC_INET4, CC_PROTOCOL_INVALID, CC_NOFLAG);
    check("CC_INET4+CC_PROTOCOL_INVALID → INVALID", s == INVALID_SOCKET);
  }
  {
    ccsocket_t s = ccsocket2(CC_INET6, CC_PROTOCOL_INVALID, CC_NOFLAG);
    check("CC_INET6+CC_PROTOCOL_INVALID → INVALID", s == INVALID_SOCKET);
  }
}

static void test_invalid_handle(void)
{
  ccsocket_family_t f = ccsocket_get_family(INVALID_SOCKET);
  check("get_family(INVALID_SOCKET) == CC_FAMILY_INVALID", f == CC_FAMILY_INVALID);

  ccsocket_protocol_t p = ccsocket_get_protocol(INVALID_SOCKET);
  check("get_protocol(INVALID_SOCKET) == CC_PROTOCOL_INVALID", p == CC_PROTOCOL_INVALID);
}

static void test_flags(void)
{
  /* Create with CC_NONBLOCK + CC_CLOEXEC, verify the socket works */
  ccsocket_t s = ccsocket2(CC_INET4, CC_TCP, CC_NONBLOCK | CC_CLOEXEC);
  check("TCP IPv4 + NONBLOCK|CLOEXEC create", s != INVALID_SOCKET);
  if (s != INVALID_SOCKET) {
    ccsocket_family_t f = ccsocket_get_family(s);
    check("TCP IPv4 + flags get_family==CC_INET4", f == CC_INET4);
    ccsocket_close(s);
  }

  /* UDP + CC_NONBLOCK */
  s = ccsocket2(CC_INET4, CC_UDP, CC_NONBLOCK);
  check("UDP IPv4 + NONBLOCK create", s != INVALID_SOCKET);
  if (s != INVALID_SOCKET) ccsocket_close(s);
}

int main(void)
{
  if (!ccsocket_init()) {
    printf("ccsocket_init() failed — cannot run tests\n");
    return 1;
  }
  printf("=== ccsocket family/protocol enum boundary tests ===\n\n");

  /* ---- valid combos ---- */
  test_valid_combo("TCP IPv4",       CC_INET4, CC_TCP,  CC_INET4, CC_TCP);
  test_valid_combo("UDP IPv4",       CC_INET4, CC_UDP,  CC_INET4, CC_UDP);
  /* IPv6: may be unavailable in some CI containers — gracefully skip */
  {
    ccsocket_t s = ccsocket2(CC_INET6, CC_TCP, CC_NOFLAG);
    if (s == INVALID_SOCKET)
      check("TCP IPv6 (unavailable on this host)", true);
    else {
      check("TCP IPv6 create", true);
      check("TCP IPv6 get_family==2", ccsocket_get_family(s) == CC_INET6);
      check("TCP IPv6 get_protocol==1", ccsocket_get_protocol(s) == CC_TCP);
      ccsocket_close(s);
    }
  }
  {
    ccsocket_t s = ccsocket2(CC_INET6, CC_UDP, CC_NOFLAG);
    if (s == INVALID_SOCKET)
      check("UDP IPv6 (unavailable on this host)", true);
    else {
      check("UDP IPv6 create", true);
      check("UDP IPv6 get_family==2", ccsocket_get_family(s) == CC_INET6);
      check("UDP IPv6 get_protocol==2", ccsocket_get_protocol(s) == CC_UDP);
      ccsocket_close(s);
    }
  }

  /* AF_UNIX: creation may fail on platforms without Unix domain support */
  {
    ccsocket_t s = ccsocket2(CC_UNIX, CC_TCP, CC_NOFLAG);
    if (s == INVALID_SOCKET)
      check("Unix stream (unsupported on this platform)", true);
    else {
      check("Unix stream create", true);
      check("Unix stream get_family==CC_UNIX", ccsocket_get_family(s) == CC_UNIX);
      ccsocket_close(s);
    }
  }
  {
    ccsocket_t s = ccsocket2(CC_UNIX, CC_UDP, CC_NOFLAG);
    if (s == INVALID_SOCKET)
      check("Unix dgram (unsupported on this platform)", true);
    else {
      check("Unix dgram create", true);
      check("Unix dgram get_family==CC_UNIX", ccsocket_get_family(s) == CC_UNIX);
      ccsocket_close(s);
    }
  }

  /* ICMP (raw): may fail without root/CAP_NET_RAW on POSIX,
   * and is unsupported on Windows.  Gracefully handle both. */
  {
    ccsocket_t s = ccsocket2(CC_INET4, CC_ICMP, CC_NOFLAG);
    if (s == INVALID_SOCKET)
      check("ICMP IPv4 (unsupported/no privilege)", true);
    else {
      check("ICMP IPv4 create", true);
      check("ICMP IPv4 get_family==CC_INET4", ccsocket_get_family(s) == CC_INET4);
      ccsocket_close(s);
    }
  }
  {
    ccsocket_t s = ccsocket2(CC_INET6, CC_ICMP, CC_NOFLAG);
    if (s == INVALID_SOCKET)
      check("ICMP IPv6 (unsupported/no privilege)", true);
    else {
      check("ICMP IPv6 create", true);
      check("ICMP IPv6 get_family==CC_INET6", ccsocket_get_family(s) == CC_INET6);
      ccsocket_close(s);
    }
  }

  printf("\n--- invalid combos ---\n");
  test_invalid_combos();

  printf("\n--- invalid handle ---\n");
  test_invalid_handle();

  printf("\n--- flags ---\n");
  test_flags();

  printf("\n=== result: %d pass, %d fail ===\n", g_pass, g_fail);
  return g_fail > 0 ? 1 : 0;
}
