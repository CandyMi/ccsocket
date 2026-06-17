/**
 * test_ccsocket_pair.c — socketpair bidirectional test.
 *
 * Creates a pair of connected stream sockets and sends data
 * in both directions, verifying payload integrity.
 */

#include "ccsocket.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void)
{
    ccsocket_t sv[2];
    char buf[64];
    int n;

    /* --- Create socketpair --- */
    assert(ccsocketpair(sv, CC_NOFLAG));
    assert(sv[0] != INVALID_SOCKET);
    assert(sv[1] != INVALID_SOCKET);

    /* --- Send sv[0] → sv[1] --- */
    const char *msg_a = "Hello from A";
    size_t len_a = strlen(msg_a);
    assert(ccsocket_send(sv[0], msg_a, len_a, &n) == CC_OPCODE_OK);
    assert(n == (int)len_a);

    n = 0;
    assert(ccsocket_recv(sv[1], buf, sizeof(buf), &n) == CC_OPCODE_OK);
    assert(n == (int)len_a);
    buf[n] = '\0';
    assert(strcmp(buf, msg_a) == 0);

    /* --- Send sv[1] → sv[0] --- */
    const char *msg_b = "Hello from B";
    size_t len_b = strlen(msg_b);
    assert(ccsocket_send(sv[1], msg_b, len_b, &n) == CC_OPCODE_OK);
    assert(n == (int)len_b);

    n = 0;
    assert(ccsocket_recv(sv[0], buf, sizeof(buf), &n) == CC_OPCODE_OK);
    assert(n == (int)len_b);
    buf[n] = '\0';
    assert(strcmp(buf, msg_b) == 0);

    /* --- Cleanup --- */
    ccsocket_close(sv[0]);
    ccsocket_close(sv[1]);

    return 0;
}
