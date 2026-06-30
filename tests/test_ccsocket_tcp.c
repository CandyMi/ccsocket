/**
 * test_ccsocket_tcp.c — TCP loopback round-trip test.
 *
 * Creates a server socket, binds to 127.0.0.1:0 (random port),
 * listens, accepts a client, sends data, receives it, and
 * verifies the payload matches.
 */

#include "ccsocket.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void)
{
    assert(ccsocket_init());

    ccsocket_t srv, cli, acc;
    char addr[MAX_ADDRLEN];
    uint16_t port;
    (void)addr; (void)port;

    /* --- Server: create, bind, listen --- */
    srv = ccsocket(CC_INET4, CC_TCP);
    assert(srv != INVALID_SOCKET);

    assert(ccsocket_set_reuseaddr(srv, true));
    assert(ccsocket_listen(srv, "127.0.0.1", 0, -1));

    /* get the assigned port */
    assert(ccsocket_get_sockname(srv, addr, &port));
    assert(strcmp(addr, "127.0.0.1") == 0);
    assert(port > 0);

    /* --- Client: connect --- */
    cli = ccsocket(CC_INET4, CC_TCP);
    assert(cli != INVALID_SOCKET);

    assert(ccsocket_connect(cli, "127.0.0.1", port));

    /* --- Server: accept --- */
    acc = ccsocket_accept(srv, CC_NOFLAG);
    assert(acc != INVALID_SOCKET && acc != (ccsocket_t)0);

    /* optional: get client address */
    char peer[MAX_ADDRLEN];
    uint16_t peer_port;
    assert(ccsocket_get_peername(acc, peer, &peer_port));
    (void)peer; (void)peer_port;
    assert(strcmp(peer, "127.0.0.1") == 0);

    /* --- Send data: client → server --- */
    const char *tx = "Hello TCP!";
    (void)tx;
    int wsent = 0;
    assert(ccsocket_send(cli, tx, strlen(tx), &wsent) == CC_OPCODE_OK);
    (void)wsent;
    assert(wsent > 0);

    /* --- Receive data: server side --- */
    char rx[64];
    int rrecv = 0;
    assert(ccsocket_recv(acc, rx, sizeof(rx), &rrecv) == CC_OPCODE_OK);
    assert(rrecv == (int)strlen(tx));
    rx[rrecv] = '\0';
    assert(strcmp(rx, tx) == 0);

    /* --- Cleanup --- */
    ccsocket_close(acc);
    ccsocket_close(cli);
    ccsocket_close(srv);

    return 0;
}
