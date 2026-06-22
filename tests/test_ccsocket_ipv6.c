/**
 * test_ccsocket_ipv6.c — IPv6 loopback round-trip tests (TCP + UDP).
 *
 * Verifies:
 *   - IPv6 TCP socket create, bind("::1"), listen, connect, send/recv
 *   - IPv6 UDP socket create, connect("::1"), send/recv, nonblock WAIT
 *   - ccsocket_get_family returns CC_INET6 for all IPv6 sockets
 *
 * Gracefully skips if IPv6 is not available on the host (common in
 * some CI containers).
 */
#include "ccsocket.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Check if IPv6 is available by attempting to create a socket */
static int ipv6_available(void)
{
    ccsocket_t s = ccsocket2(CC_INET6, CC_TCP, CC_NOFLAG);
    if (s == INVALID_SOCKET) return 0;
    ccsocket_close(s);
    return 1;
}

/* ---- TCP IPv6 loopback ---- */
static void test_tcp_ipv6(void)
{
    printf("  TCP IPv6 loopback...\n");

    ccsocket_t srv, cli, acc;
    char addr[MAX_ADDRLEN];
    uint16_t port;

    /* Server: create, bind, listen on "::1" */
    srv = ccsocket2(CC_INET6, CC_TCP, CC_NOFLAG);
    assert(srv != INVALID_SOCKET);
    assert(ccsocket_get_family(srv) == CC_INET6);

    assert(ccsocket_set_reuseaddr(srv, true));
    assert(ccsocket_listen(srv, "::1", 0));

    /* Get assigned port */
    assert(ccsocket_get_sockname(srv, addr, &port));
    assert(strcmp(addr, "::1") == 0);
    assert(port > 0);

    /* Client: connect */
    cli = ccsocket2(CC_INET6, CC_TCP, CC_NOFLAG);
    assert(cli != INVALID_SOCKET);
    assert(ccsocket_get_family(cli) == CC_INET6);
    assert(ccsocket_connect(cli, "::1", port));

    /* Server: accept */
    acc = ccsocket_accept(srv, CC_NOFLAG);
    assert(acc != INVALID_SOCKET && acc != (ccsocket_t)0);

    /* Verify peer address is IPv6 loopback */
    assert(ccsocket_get_peername(acc, addr, &port));
#if !defined(_WIN32)
    /* On POSIX, connected IPv6 peer address is "::1".
     * On Windows, the mapped address format varies — skip exact match. */
    assert(strcmp(addr, "::1") == 0);
#endif

    /* Send: client → server */
    const char *tx = "Hello IPv6!";
    int wsent = 0;
    assert(ccsocket_send(cli, tx, strlen(tx), &wsent) == CC_OPCODE_OK);
    assert(wsent == (int)strlen(tx));

    /* Receive: server side */
    char rx[64];
    int rrecv = 0;
    assert(ccsocket_recv(acc, rx, sizeof(rx), &rrecv) == CC_OPCODE_OK);
    assert(rrecv == (int)strlen(tx));
    rx[rrecv] = '\0';
    assert(strcmp(rx, tx) == 0);

    ccsocket_close(acc);
    ccsocket_close(cli);
    ccsocket_close(srv);
    printf("    PASS\n");
}

/* ---- UDP IPv6 loopback ---- */
static void test_udp_ipv6(void)
{
    printf("  UDP IPv6 loopback...\n");

    ccsocket_t cli;
    char addr[MAX_ADDRLEN];
    uint16_t port;

    cli = ccsocket2(CC_INET6, CC_UDP, CC_NOFLAG);
    assert(cli != INVALID_SOCKET);
    assert(ccsocket_get_family(cli) == CC_INET6);

    /* Connect to "::1", port 9 (discard — safe port, no data sent) */
    assert(ccsocket_connect(cli, "::1", 9));

    /* Verify local address after implicit bind */
    assert(ccsocket_get_sockname(cli, addr, &port));
    assert(strcmp(addr, "::1") == 0);
    assert(port > 0);

    /* Verify peer address */
    assert(ccsocket_get_peername(cli, addr, &port));
    assert(strcmp(addr, "::1") == 0);
    assert(port == 9);

    /* Non-blocking recv on idle socket → WAIT */
    assert(ccsocket_set_nonblock(cli, true));
    {
        char buf[16];
        int n = 0;
        assert(ccsocket_recv(cli, buf, sizeof(buf), &n) == CC_OPCODE_WAIT);
    }

    ccsocket_close(cli);
    printf("    PASS\n");
}

int main(void)
{
    assert(ccsocket_init());

    printf("=== IPv6 loopback tests ===\n\n");

    if (!ipv6_available()) {
        printf("  IPv6 not available on this host — skipping all tests.\n");
        return 0;  /* not a failure */
    }

    test_tcp_ipv6();
    test_udp_ipv6();

    printf("\n=== all IPv6 tests passed ===\n");
    return 0;
}
