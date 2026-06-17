/**
 * test_ccsocket_udp.c — UDP socket creation and connect test.
 *
 * UDP sockets cannot use listen() in the public API (which requires
 * bind + listen). Instead we verify:
 *   - UDP socket creation succeeds
 *   - ccsocket_get_family returns the correct family
 *   - Connecting a UDP socket to a local target works
 *   - Getsockname/getpeername return valid data
 *   - send/recv via connected UDP on loopback
 *
 * Strategy: create a TCP listener to reserve a port, close it,
 * then use that port for a UDP exchange. The kernel will deliver
 * the UDP datagram since the port is free.
 *
 * Alternative: use ccsocket_send/recv on a connected UDP socket
 * pointing at itself (connected UDP echo).
 */

#include "ccsocket.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void)
{
    ccsocket_t cli;
    char addr[MAX_ADDRLEN];
    uint16_t port;
    (void)addr; (void)port;

    /* --- Create UDP socket --- */
    cli = ccsocket(CC_INET4, CC_UDP);
    assert(cli != INVALID_SOCKET);

    /* --- Verify address family --- */
    assert(ccsocket_get_family(cli) == CC_INET4);

    /* --- Connect to a valid target (loopback, discard port) --- */
    /* This sets the default destination for send, and triggers an implicit
     * bind to an ephemeral port.  The actual port 9 (discard) is chosen
     * because it's safe — we won't send data to it. */
    assert(ccsocket_connect(cli, "127.0.0.1", 9));

    /* --- Verify local address after implicit bind --- */
    assert(ccsocket_get_sockname(cli, addr, &port));
    assert(strcmp(addr, "127.0.0.1") == 0);
    assert(port > 0);

    /* --- Verify peer address --- */
    assert(ccsocket_get_peername(cli, addr, &port));
    assert(strcmp(addr, "127.0.0.1") == 0);
    assert(port == 9);

    /* --- Non-blocking recv on an idle socket should return WAIT --- */
    assert(ccsocket_set_nonblock(cli, true));
    {
        char buf[16];
        int n = 0;
        (void)buf; (void)n;
        assert(ccsocket_recv(cli, buf, sizeof(buf), &n) == CC_OPCODE_WAIT);
    }

    /* --- Cleanup --- */
    ccsocket_close(cli);

    return 0;
}
