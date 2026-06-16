/**
 * test_ccsocket_smoke.c — Compile-and-link smoke test for libccsocket.
 *
 * Verifies that:
 *   - All public headers compile cleanly with the project's C standard.
 *   - Core types, enums, and macros are accessible.
 *   - The library links without unresolved symbols.
 *
 * This is NOT a functional test — it only validates the build system.
 */

#include "ccsocket.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    /* — Type size / alignment smoke checks — */
    assert(sizeof(ccsocket_t) > 0);

    /* — Enum value smoke checks — */
    assert(CC_NOFLAG   == 0);
    assert(CC_CLOEXEC  == 1);
    assert(CC_NONBLOCK == 2);

    assert(CC_INET4 == 1);
    assert(CC_INET6 == 2);

    assert(CC_TCP  == 1);
    assert(CC_UDP  == 2);
    assert(CC_ICMP == 3);

    /* — Macro sanity — */
    assert(MAX_ADDRLEN  == 255);
    assert(MAX_ERRORLEN == 255);

    /* — iovec accessor macros — */
    {
        ccsocket_iovec_t iov[2];
        ccsocket_init_iov(iov, 2);

        char buf_a[16];
        ccsocket_set_iov_buf(iov, 0, buf_a);
        ccsocket_set_iov_len(iov, 0, sizeof(buf_a));
        assert(ccsocket_get_iov_buf(iov, 0) == (cciovec_buf_t)buf_a);
        assert(ccsocket_get_iov_len(iov, 0) == 16);

        char buf_b[32];
        ccsocket_set_iov_buf(iov, 1, buf_b);
        ccsocket_set_iov_len(iov, 1, sizeof(buf_b));
        assert(ccsocket_get_iov_buf(iov, 1) == (cciovec_buf_t)buf_b);
        assert(ccsocket_get_iov_len(iov, 1) == 32);
    }

    /* — Family detection — */
    assert(ccsocket_get_version("1.1.1.1")     == CC_INET4);
    assert(ccsocket_get_version("::1")          == CC_INET6);
    assert(ccsocket_get_version("127.0.0.1")   == CC_INET4);
    assert(ccsocket_get_version(NULL)           == CC_FAMILY_INVALID);

    return 0;
}
