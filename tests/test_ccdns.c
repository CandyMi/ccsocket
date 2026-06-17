/**
 * test_ccdns.c — DNS client encode/decode test.
 *
 * Verifies:
 *   1. ccdns_encode produces correct wire-format query bytes
 *   2. ccdns_decode extracts the address from a known response
 *   3. ccdns_init / ccdns_close lifecycle
 */

#include "ccdns.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* ====================================================================
 * Tier 1 — Encode test
 * ==================================================================== */

static void test_encode_basic(void)
{
    struct ccdns_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    /* We need a real UDP socket for init.  If it fails, skip. */
    if (!ccdns_init(&ctx)) {
        /* No network stack (e.g. sandbox) — skip */
        return;
    }

    uint8_t buf[512];
    uint16_t n = ccdns_encode(&ctx, buf, sizeof(buf), "google.com", CCDNS_A);
    assert(n > 0);
    (void)n;

    /* Verify header: ID should be 0 (first request) */
    assert(buf[0] == 0 && buf[1] == 0);

    /* Verify flags: RD=1 */
    assert(buf[2] == 0x01 && buf[3] == 0x00);

    /* Verify QDCOUNT = 1 */
    assert(buf[4] == 0 && buf[5] == 1);

    /* Verify question: encoded name "google.com" */
    assert(buf[12] == 6);                               /* label length = 6 */
    assert(memcmp(buf + 13, "google", 6) == 0);
    assert(buf[19] == 3);                               /* label length = 3 */
    assert(memcmp(buf + 20, "com", 3) == 0);
    assert(buf[23] == 0);                               /* root terminator */

    /* Verify QTYPE = A (1) */
    assert(buf[24] == 0 && buf[25] == 1);

    /* Verify QCLASS = IN (1) */
    assert(buf[26] == 0 && buf[27] == 1);

    /* Total length: header(12) + "google.com"(13) + QTYPE(2) + QCLASS(2) = 28 */
    assert(n == 28);

    ccdns_close(&ctx);
}

/* ====================================================================
 * Tier 2 — Decode test (with known response vector)
 * ==================================================================== */

static void test_decode_ipv4(void)
{
    /* Build a known DNS response for google.com → 142.250.80.46
     *
     * Header:
     *   ID=0x1234, flags=0x8180 (QR=1,RD=1,RA=1), QD=1, AN=1, NS=0, AR=0
     *
     * Question (copy of query):
     *   06google03com0 + QTYPE=1(0x0001) + QCLASS=1(0x0001)
     *
     * Answer:
     *   NAME=0xc00c (compression ptr to offset 12)
     *   TYPE=1 (0x0001), CLASS=1 (0x0001), TTL=300 (0x0000012c)
     *   RDLENGTH=4, RDATA=142.250.80.46 (0x8efa502e)
     */
    uint8_t resp[] = {
        /* Header */
        0x12, 0x34,           /* ID = 0x1234 */
        0x81, 0x80,           /* flags: QR=1, RD=1, RA=1 */
        0x00, 0x01,           /* QDCOUNT = 1 */
        0x00, 0x01,           /* ANCOUNT = 1 */
        0x00, 0x00,           /* NSCOUNT = 0 */
        0x00, 0x00,           /* ARCOUNT = 0 */
        /* Question */
        0x06, 'g','o','o','g','l','e',
        0x03, 'c','o','m',
        0x00,                 /* root */
        0x00, 0x01,           /* QTYPE = A */
        0x00, 0x01,           /* QCLASS = IN */
        /* Answer */
        0xc0, 0x0c,           /* NAME = compression to offset 12 */
        0x00, 0x01,           /* TYPE = A */
        0x00, 0x01,           /* CLASS = IN */
        0x00, 0x00, 0x01, 0x2c, /* TTL = 300 */
        0x00, 0x04,           /* RDLENGTH = 4 */
        142, 250, 80, 46      /* RDATA = 142.250.80.46 */
    };

    char addr[64];
    uint16_t addrlen = sizeof(addr);
    int r = ccdns_decode(resp, sizeof(resp), 0x1234, addr, &addrlen);
    assert(r == 0);
    (void)r;
    assert(strcmp(addr, "142.250.80.46") == 0);
    assert(addrlen == 13);  /* strlen("142.250.80.46") */
}

static void test_decode_id_mismatch(void)
{
    uint8_t resp[] = {
        0x12, 0x34, 0x81, 0x80, 0x00, 0x01,
        0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x06, 'g','o','o','g','l','e', 0x03, 'c','o','m', 0x00,
        0x00, 0x01, 0x00, 0x01,
        0xc0, 0x0c, 0x00, 0x01, 0x00, 0x01,
        0x00, 0x00, 0x01, 0x2c, 0x00, 0x04,
        142, 250, 80, 46
    };
    char addr[64];
    uint16_t addrlen = sizeof(addr);
    int r = ccdns_decode(resp, sizeof(resp), 0x5678, addr, &addrlen);
    assert(r == -2);  /* ID mismatch */
    (void)r;
}

/* ====================================================================
 * Tier 3 — Lifecycle test
 * ==================================================================== */

static void test_lifecycle(void)
{
    struct ccdns_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    bool ok = ccdns_init(&ctx);
    if (ok) {
        assert(ctx.fd != INVALID_SOCKET);
        assert(ctx.reqno == 0);

        /* Encode should increment reqno */
        uint8_t buf[512];
        uint16_t n = ccdns_encode(&ctx, buf, sizeof(buf), "example.com", CCDNS_A);
        assert(n > 0);
        (void)n;
        assert(ctx.reqno == 1);

        ccdns_close(&ctx);
        assert(ctx.fd == INVALID_SOCKET);
        assert(ctx.reqno == 0);
    }
}

/* ====================================================================
 * Main
 * ==================================================================== */

int main(void)
{
    test_encode_basic();
    test_decode_ipv4();
    test_decode_id_mismatch();
    test_lifecycle();

    return 0;
}
