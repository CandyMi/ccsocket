/**
 * test_ccsocket_cxx_embed.cpp — C++ embedding compile-and-link test
 *
 * Verifies that:
 *   - All public headers compile cleanly under a C++ compiler.
 *   - The `extern "C"` wrappers produce linkable C++ symbols.
 *   - Core types, enums, and macros are accessible from C++.
 *   - The library links without unresolved symbols.
 *
 * This is NOT a functional test — it only validates build system
 * and C++ interop.
 */

#include "ccsocket.h"
#include "ccicmp.h"
#include "ccdns.h"
#include <cassert>
#include <cstring>

/* ---- Static checks (compile-time) ---- */
static void enum_smoke(void)
{
    assert(CC_TCP    == 1);
    assert(CC_UDP    == 2);
    assert(CC_ICMP   == 3);
    assert(CC_INET4  == 1);
    assert(CC_INET6  == 2);
    assert(CC_NOFLAG == 0);
    assert(CC_CLOEXEC == 1);
    assert(CC_NONBLOCK == 2);
    assert(sizeof(ccsocket_t) > 0);
}

/* ---- Function pointer smoke (linkable symbols) ---- */
static void fptr_smoke(void)
{
    /* clang-format off */
    int     (*close_fn)(ccsocket_t)                    = ccsocket_close;
    bool    (*init_fn)(void)                            = ccsocket_init;
    bool    (*connect_fn)(ccsocket_t, const char *, uint16_t)
                                                       = ccsocket_connect;
    bool    (*listen_fn)(ccsocket_t, const char *, uint16_t)
                                                       = ccsocket_listen;
    bool    (*peername_fn)(ccsocket_t, char *, uint16_t *)
                                                       = ccsocket_get_peername;
    bool    (*sockname_fn)(ccsocket_t, char *, uint16_t *)
                                                       = ccsocket_get_sockname;
    ccsocket_family_t (*family_fn)(ccsocket_t)         = ccsocket_get_family;
    int     (*proto_fn)(ccsocket_t)                    = ccsocket_get_protocol;
    bool    (*nodelay_fn)(ccsocket_t, bool)            = ccsocket_set_nodelay;
    bool    (*reuseaddr_fn)(ccsocket_t, bool)          = ccsocket_set_reuseaddr;
    bool    (*nonblock_fn)(ccsocket_t, bool)           = ccsocket_set_nonblock;
    bool    (*cloexec_fn)(ccsocket_t, bool)            = ccsocket_set_cloexec;
    ccsocket_family_t (*ver_fn)(const char *)          = ccsocket_get_version;
    bool    (*icmp_init_fn)(struct ccicmp_t *, ccsocket_family_t)
                                                       = ccicmp_init;
    void    (*icmp_close_fn)(struct ccicmp_t *)        = ccicmp_close;
    bool    (*dns_init_fn)(struct ccdns_t *)           = ccdns_init;
    void    (*dns_close_fn)(struct ccdns_t *)          = ccdns_close;
    /* clang-format on */

    assert(close_fn     != nullptr);
    assert(init_fn      != nullptr);
    assert(connect_fn   != nullptr);
    assert(listen_fn    != nullptr);
    assert(peername_fn  != nullptr);
    assert(sockname_fn  != nullptr);
    assert(family_fn    != nullptr);
    assert(proto_fn     != nullptr);
    assert(nodelay_fn   != nullptr);
    assert(reuseaddr_fn != nullptr);
    assert(nonblock_fn  != nullptr);
    assert(cloexec_fn   != nullptr);
    assert(ver_fn       != nullptr);
    assert(icmp_init_fn != nullptr);
    assert(icmp_close_fn  != nullptr);
    assert(dns_init_fn  != nullptr);
    assert(dns_close_fn != nullptr);

    (void)close_fn;
    (void)init_fn;
    (void)connect_fn;
    (void)listen_fn;
    (void)peername_fn;
    (void)sockname_fn;
    (void)family_fn;
    (void)proto_fn;
    (void)nodelay_fn;
    (void)reuseaddr_fn;
    (void)nonblock_fn;
    (void)cloexec_fn;
    (void)ver_fn;
    (void)icmp_init_fn;
    (void)icmp_close_fn;
    (void)dns_init_fn;
    (void)dns_close_fn;
}

/* ---- iovec accessor macros (C++ type safety) ---- */
static void iovec_smoke(void)
{
    ccsocket_iovec_t iov[2];
    ccsocket_init_iov(iov, 2);

    char buf_a[16];
    ccsocket_set_iov_buf(iov, 0, buf_a);
    ccsocket_set_iov_len(iov, 0, sizeof(buf_a));
    assert(ccsocket_get_iov_buf(iov, 0) == static_cast<cciovec_buf_t>(buf_a));
    assert(ccsocket_get_iov_len(iov, 0) == 16);

    (void)iov;
}

/* ---- ICMP context lifecycle ---- */
static void icmp_lifecycle(void)
{
    ccicmp_t icmp;
    bool ok = ccicmp_init(&icmp, CC_INET4);
    if (ok) {
        ccicmp_close(&icmp);
    }
}

/* ---- DNS context lifecycle ---- */
static void dns_lifecycle(void)
{
    ccdns_t dns;
    bool ok = ccdns_init(&dns);
    (void)ok;
    ccdns_close(&dns);
}

int main(void)
{
    assert(ccsocket_init());

    enum_smoke();
    fptr_smoke();
    iovec_smoke();
    icmp_lifecycle();
    dns_lifecycle();

    return 0;
}
