/**
 * test_ccicmp_smoke.c — Compile-and-link smoke test for ccicmp.
 *
 * Verifies that:
 *   - The ccicmp header compiles (and transitively ccsocket.h).
 *   - The ccicmp_t type and public function signatures are accessible.
 *   - The library links without unresolved symbols.
 *
 * Does NOT attempt actual ICMP traffic (requires root / CAP_NET_RAW).
 */

#include "ccsocket.h"
#include "ccicmp.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    assert(ccsocket_init());

    /* — Type layout — */
    assert(sizeof(struct ccicmp_t) >= sizeof(ccsocket_t) + sizeof(uint16_t) * 2);

    /* — Function pointer smoke — */
    /* Verify the symbols are linkable by taking their addresses at runtime. */
    bool (*init_fn)(struct ccicmp_t *, int)    = ccicmp_init;
    void (*close_fn)(struct ccicmp_t *)                       = ccicmp_close;
    bool (*echo_fn)(struct ccicmp_t *, const char *, const char *, size_t) = ccicmp_echo;
    bool (*reply_fn)(struct ccicmp_t *, char *, size_t *)     = ccicmp_reply;

    assert(init_fn  != NULL);
    assert(close_fn != NULL);
    assert(echo_fn  != NULL);
    assert(reply_fn != NULL);

    /* Suppress -Wunused-variable */
    (void)init_fn;
    (void)close_fn;
    (void)echo_fn;
    (void)reply_fn;

    /* — Domain parameter validation — */
    {
        struct ccicmp_t ctx;
        memset(&ctx, 0, sizeof(ctx));

        /* Invalid domain should fail gracefully (no crash) */
        bool r = ccicmp_init(&ctx, CC_FAMILY_INVALID);
        assert(r == false);

        /* Cleanup on an uninitialised context must be idempotent.
         * ccicmp_close uses assert(ctx) internally, so calling it
         * with a zeroed struct is UB — we only verify init failed. */
        (void)r;
    }

    return 0;
}
