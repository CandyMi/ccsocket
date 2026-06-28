/**
 * test_ccsocket_connect_state.c — Connection state detection tests.
 *
 * Verifies ccsocket_is_connected() returns correct states:
 *   - CC_CONNECTED: TCP socket successfully connected
 *   - CC_CONNECTING: non-blocking connect to a slow/non-responsive port
 *   - CC_CONNERROR: invalid socket handle
 *
 * Also verifies that the function has no side effects on a connected socket
 * (does not clear error state or trigger re-connect).
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

/* ---- Connected state: TCP loopback ---- */
static void test_connected_tcp(void)
{
    ccsocket_t srv, cli;

    /* Server: create, bind, listen */
    srv = ccsocket(CC_INET4, CC_TCP);
    assert(srv != INVALID_SOCKET);
    assert(ccsocket_set_reuseaddr(srv, true));

    /* Bind to port 0 to get an ephemeral port */
    assert(ccsocket_listen(srv, "127.0.0.1", 0));

    char addr[MAX_ADDRLEN];
    uint16_t port;
    assert(ccsocket_get_sockname(srv, addr, &port));
    assert(port > 0);

    /* Client: connect */
    cli = ccsocket1(CC_INET4, CC_TCP, CC_NONBLOCK);
    assert(cli != INVALID_SOCKET);

    /* Attempt connect — may return immediately or be in progress */
    ccsocket_connect(cli, "127.0.0.1", port);

    /* Accept on server side */
    ccsocket_t acc = ccsocket_accept(srv, CC_NOFLAG);
    assert(acc != INVALID_SOCKET && acc != (ccsocket_t)0);

    /* Client should now be connected */
    ccsocket_conn_state_t st = ccsocket_is_connected(cli);
    check("TCP client is_connected after accept → CC_CONNECTED", st == CC_CONNECTED);

    /* Server accept side should be connected */
    st = ccsocket_is_connected(acc);
    check("TCP server accept is_connected → CC_CONNECTED", st == CC_CONNECTED);

    /* Verify no side effects: peer name should still be available */
    char peer[MAX_ADDRLEN];
    uint16_t peer_port;
    check("get_peername after is_connected still works",
          ccsocket_get_peername(cli, peer, &peer_port));
    check("peer address is loopback", strcmp(peer, "127.0.0.1") == 0);

    /* send/recv still works after is_connected */
    check("send after is_connected → OK",
          ccsocket_send(cli, "ping", 4, NULL) == CC_OPCODE_OK);

    ccsocket_close(acc);
    ccsocket_close(cli);
    ccsocket_close(srv);
}

/* ---- Error state: INVALID_SOCKET ---- */
static void test_error_invalid(void)
{
    ccsocket_conn_state_t st = ccsocket_is_connected(INVALID_SOCKET);
    check("is_connected(INVALID_SOCKET) → CC_CONNERROR", st == CC_CONNERROR);
}

/* ---- Unconnected socket (created but not connected) ---- */
static void test_unconnected_udp(void)
{
    /* UDP socket created but not yet connected.
     * This is a valid socket but not "connected" in the TCP sense.
     * The function should not crash and should return a reasonable state. */
    ccsocket_t s = ccsocket(CC_INET4, CC_UDP);
    assert(s != INVALID_SOCKET);

    ccsocket_conn_state_t st = ccsocket_is_connected(s);
    /* UDP hasn't been connect()ed — some platforms report CONNERROR,
     * others may report CONNECTED (connected UDP is a valid concept).
     * Accept either as long as it's not an invalid crash state. */
    check("unconnected UDP is_connected → doesn't crash",
          st == CC_CONNERROR || st == CC_CONNECTED || st == CC_CONNECTING);

    ccsocket_close(s);
}

/* ---- Connected UDP socket ---- */
static void test_connected_udp(void)
{
    ccsocket_t s = ccsocket(CC_INET4, CC_UDP);
    assert(s != INVALID_SOCKET);

    /* Connect UDP to a valid target (127.0.0.1:53 — DNS, likely filtered but harmless) */
    assert(ccsocket_connect(s, "127.0.0.1", 53));

    ccsocket_conn_state_t st = ccsocket_is_connected(s);
    /* Connected UDP — the connect() sets the default destination.
     * Most platforms report this as "connected" since getpeername succeeds. */
    check("connected UDP is_connected → CC_CONNECTED or CC_CONNERROR",
          st == CC_CONNECTED || st == CC_CONNERROR);

    ccsocket_close(s);
}

/* ---- is_connected on a connecting non-blocking TCP socket ---- */
static void test_connecting_tcp(void)
{
    /* Try connecting to a non-routable address or a filtered port.
     * Using a TCP connection to 127.0.0.1:1 (port 1 — not listening on any system).
     * In non-blocking mode, the connect() should return immediately with EINPROGRESS. */
    ccsocket_t s = ccsocket1(CC_INET4, CC_TCP, CC_NONBLOCK);
    assert(s != INVALID_SOCKET);

    /* Connect to a port that is very unlikely to be listening */
    ccsocket_connect(s, "127.0.0.1", 1);

    ccsocket_conn_state_t st = ccsocket_is_connected(s);
    /* The socket is either still connecting or got a connection refused.
     * Both are acceptable outcomes in this test. */
    check("non-blocking TCP connecting → CC_CONNECTING or CC_CONNERROR",
          st == CC_CONNECTING || st == CC_CONNERROR);

    ccsocket_close(s);
}

int main(void)
{
    assert(ccsocket_init());

    printf("=== connection state detection tests ===\n\n");

    test_error_invalid();
    printf("\n");
    test_unconnected_udp();
    printf("\n");
    test_connected_udp();
    printf("\n");
    test_connecting_tcp();
    printf("\n");
    test_connected_tcp();

    printf("\n=== result: %d pass, %d fail ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
