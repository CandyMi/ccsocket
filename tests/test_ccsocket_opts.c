/**
 * test_ccsocket_opts.c — Socket option round-trip tests.
 *
 * Verifies that set-nodelay, set-reuseaddr, set-keepalive,
 * set-nonblock, and set-cloexec return true on valid sockets
 * and false on invalid handles.
 */

#include "ccsocket.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void)
{
    ccsocket_t s = ccsocket(CC_INET4, CC_TCP);
    assert(s != INVALID_SOCKET);

    /* --- TCP_NODELAY --- */
    assert(ccsocket_set_nodelay(s, true));
    assert(ccsocket_set_nodelay(s, false));

    /* --- SO_REUSEADDR --- */
    assert(ccsocket_set_reuseaddr(s, true));
    assert(ccsocket_set_reuseaddr(s, false));

    /* --- SO_KEEPALIVE --- */
    assert(ccsocket_set_keepalive(s, true));
    assert(ccsocket_set_keepalive(s, false));

    /* --- Non-blocking mode --- */
    assert(ccsocket_set_nonblock(s, true));
    assert(ccsocket_set_nonblock(s, false));

    /* --- Close-on-exec --- */
    assert(ccsocket_set_cloexec(s, true));
    assert(ccsocket_set_cloexec(s, false));

    /* --- Timeouts --- */
    assert(ccsocket_set_rcvtimeout(s, 1000));
    assert(ccsocket_set_sndtimeout(s, 1000));

    /* --- Reuse port (best-effort; may fail if platform lacks support) --- */
    /* Don't assert — SO_REUSEPORT is platform-dependent */
    ccsocket_set_reuseport(s, true);
    ccsocket_set_reuseport(s, false);

    /* --- Invalid handle: all setters should return false --- */
    ccsocket_t bad = INVALID_SOCKET;
    (void)bad;
    assert(!ccsocket_set_nodelay(bad, true));
    assert(!ccsocket_set_reuseaddr(bad, true));
    assert(!ccsocket_set_keepalive(bad, true));
    assert(!ccsocket_set_nonblock(bad, true));
    assert(!ccsocket_set_cloexec(bad, true));
    assert(!ccsocket_set_rcvtimeout(bad, 1000));
    assert(!ccsocket_set_sndtimeout(bad, 1000));

    /* --- Cleanup --- */
    ccsocket_close(s);

    return 0;
}
