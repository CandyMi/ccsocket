/**
 * test_ccsocket_http.c — HTTP text protocol round-trip test.
 *
 * Uses httpc.txt as the request template to verify TCP combined
 * functionality: server binds + listens + accepts + reads a text
 * protocol message, then sends a response; client connects + sends
 * the request + reads the response.
 *
 * Tests: TCP listen/accept/connect/send/recv + text protocol parsing.
 */

#include "ccsocket.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* The HTTP request from httpc.txt */
static const char HTTP_REQ[] =
    "GET / HTTP/1.1\r\n"
    "Host: cfadmin.cn\r\n"
    "\r\n";

static const char HTTP_RES[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: 2\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n"
    "OK";

int main(void)
{
    ccsocket_t srv, cli, acc;
    char addr[MAX_ADDRLEN];
    uint16_t port;
    char buf[512];
    int n;
    (void)addr; (void)port;
    (void)HTTP_REQ; (void)HTTP_RES;

    /* --- Server: create, bind, listen --- */
    srv = ccsocket(CC_INET4, CC_TCP);
    assert(srv != INVALID_SOCKET);
    assert(ccsocket_set_reuseaddr(srv, true));
    assert(ccsocket_listen(srv, "127.0.0.1", 0));
    assert(ccsocket_get_sockname(srv, addr, &port));
    assert(port > 0);

    /* --- Client: connect to server --- */
    cli = ccsocket(CC_INET4, CC_TCP);
    assert(cli != INVALID_SOCKET);
    assert(ccsocket_connect(cli, "127.0.0.1", port));

    /* --- Server: accept client --- */
    acc = ccsocket_accept(srv, CC_NOFLAG);
    assert(acc != INVALID_SOCKET && acc != (ccsocket_t)0);

    /* --- Client: send HTTP request --- */
    {
        assert(ccsocket_send(cli, HTTP_REQ, strlen(HTTP_REQ), &n) == CC_OPCODE_OK);
        assert(n == (int)strlen(HTTP_REQ));
    }

    /* --- Server: receive and verify HTTP request --- */
    {
        n = 0;
        assert(ccsocket_recv(acc, buf, sizeof(buf) - 1, &n) == CC_OPCODE_OK);
        assert(n > 0);
        buf[n] = '\0';

        /* Verify request starts with "GET / HTTP/1.1" */
        assert(strstr(buf, "GET / HTTP/1.1") != NULL);
        /* Verify Host header */
        assert(strstr(buf, "Host:") != NULL);
    }

    /* --- Server: send HTTP response --- */
    {
        assert(ccsocket_send(acc, HTTP_RES, strlen(HTTP_RES), &n) == CC_OPCODE_OK);
        assert(n == (int)strlen(HTTP_RES));
    }

    /* --- Client: receive and verify HTTP response --- */
    {
        n = 0;
        assert(ccsocket_recv(cli, buf, sizeof(buf) - 1, &n) == CC_OPCODE_OK);
        assert(n > 0);
        buf[n] = '\0';

        /* Verify response starts with "HTTP/1.1 200 OK" */
        assert(strstr(buf, "HTTP/1.1 200 OK") != NULL);
        /* Verify body */
        assert(strstr(buf, "OK") != NULL);
    }

    /* --- Cleanup --- */
    ccsocket_close(acc);
    ccsocket_close(cli);
    ccsocket_close(srv);

    return 0;
}
