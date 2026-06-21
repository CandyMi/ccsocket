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
    assert(ccsocket_init());

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

    /* — New msg flag enum value checks — */
  assert(CC_MSG_NOFLAG    == 0);
  assert(CC_MSG_PEEK      == 1);
  assert(CC_MSG_WAITALL   == 2);
  assert(CC_MSG_DONTWAIT  == 4);
  assert(CC_MSG_NOSIGNAL  == 8);
  assert(CC_MSG_MORE      == 16);
  assert(CC_MSG_OOB       == 32);

  assert(CC_MSG_RET_TRUNC  == 1);
  assert(CC_MSG_RET_CTRUNC == 2);
  assert(CC_MSG_RET_EOR    == 4);
  assert(CC_MSG_RET_OOB    == 8);
  assert(CC_MSG_RET_BCAST  == 16);
  assert(CC_MSG_RET_MCAST  == 32);

  /* — ccsocket_cmsghdr_t layout — */
  assert(sizeof(ccsocket_cmsghdr_t) == 12);

  /* — New API function pointer smoke — */
  {
    ccsocket_stcode_t (*fn)(ccsocket_t, ccsocket_msghdr_t *, ccsocket_msg_flags_t);
    fn = ccsocket_recvmsg;
    assert(fn != NULL);
    fn = ccsocket_sendmsg;
    assert(fn != NULL);
    (void)fn;
  }
  {
    ccsocket_stcode_t (*fn)(ccsocket_t, const void *, size_t, const char *, uint16_t, int *);
    fn = ccsocket_sendto;
    assert(fn != NULL);
    (void)fn;
  }
  {
    ccsocket_stcode_t (*fn)(ccsocket_t, char *, size_t, char *, uint16_t *, int *);
    fn = ccsocket_recvfrom;
    assert(fn != NULL);
    (void)fn;
  }

  return 0;
}
