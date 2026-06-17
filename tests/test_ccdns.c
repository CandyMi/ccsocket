/**
 * test_ccdns.c — DNS client encode/decode test.
 *
 * Verifies:
 *   1. ccdns_encode produces correct wire-format query bytes
 *   2. ccdns_decode with callback extracts A record
 *   3. ccdns_decode ID mismatch detection
 *   4. ccdns_init / ccdns_close lifecycle
 */

#include "ccdns.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* ====================================================================
 * Decode callback — collects first answer into a static var
 * ==================================================================== */

static ccdns_ans_t g_ans;

static void on_answer(void *udata, const ccdns_ans_t *ans)
{
    (void)udata;
    memcpy(&g_ans, ans, sizeof(g_ans));
}

/* ====================================================================
 * Tier 1 — Encode test
 * ==================================================================== */

static void test_encode_basic(void)
{
    struct ccdns_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    if (!ccdns_init(&ctx))
        return;  /* no network stack */

    uint8_t buf[CCDNS_MAX_MSG];
    uint16_t n = ccdns_encode(&ctx, buf, sizeof(buf), "google.com", CCDNS_A);
    assert(n > 0);
    (void)n;

    /* Header: ID=0, flags=RD(0x0100), QDCOUNT=1 */
    assert(buf[0] == 0 && buf[1] == 0);
    assert(buf[2] == 0x01 && buf[3] == 0x00);
    assert(buf[4] == 0 && buf[5] == 1);

    /* Question: 6google3com0 */
    assert(buf[12] == 6);
    assert(memcmp(buf + 13, "google", 6) == 0);
    assert(buf[19] == 3);
    assert(memcmp(buf + 20, "com", 3) == 0);
    assert(buf[23] == 0);

    /* QTYPE=A(1), QCLASS=IN(1) */
    assert(buf[24] == 0 && buf[25] == 1);
    assert(buf[26] == 0 && buf[27] == 1);

    /* Total: header(12) + "google.com"(13) + QTYPE(2) + QCLASS(2) = 28 */
    assert(n == 28);

    ccdns_close(&ctx);
}

/* ====================================================================
 * Tier 2 — Decode A record
 * ==================================================================== */

static void test_decode_a(void)
{
    /* Response for google.com → 142.250.80.46 */
    uint8_t resp[] = {
        0x00, 0x01,           /* ID = 1 */
        0x81, 0x80,           /* flags: QR, RD, RA */
        0x00, 0x01,           /* QDCOUNT = 1 */
        0x00, 0x01,           /* ANCOUNT = 1 */
        0x00, 0x00, 0x00, 0x00,
        /* Question */
        0x06, 'g','o','o','g','l','e', 0x03, 'c','o','m', 0x00,
        0x00, 0x01, 0x00, 0x01,
        /* Answer */
        0xc0, 0x0c,
        0x00, 0x01,           /* TYPE = A */
        0x00, 0x01,           /* CLASS = IN */
        0x00, 0x00, 0x01, 0x2c, /* TTL = 300 */
        0x00, 0x04,           /* RDLENGTH = 4 */
        142, 250, 80, 46
    };

    struct ccdns_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.fd = INVALID_SOCKET;
    ctx.reqno = 1;  /* expected ID */

    memset(&g_ans, 0, sizeof(g_ans));

    int r = ccdns_decode(&ctx, resp, sizeof(resp), NULL, on_answer);
    assert(r == 1);      /* 1 answer delivered */
    assert(g_ans.type == CCDNS_A);
    assert(g_ans.cls == CCDNS_CLASS_IN);
    assert(g_ans.ttl == 300);
    assert(strcmp(g_ans.ip, "142.250.80.46") == 0);
    assert(strcmp(g_ans.domain, "google.com") == 0);
    (void)r;
}

/* ====================================================================
 * Tier 3 — Decode ID mismatch
 * ==================================================================== */

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

    struct ccdns_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.fd = INVALID_SOCKET;
    ctx.reqno = 0x5678;  /* different from resp ID=0x1234 */

    int r = ccdns_decode(&ctx, resp, sizeof(resp), NULL, NULL);
    assert(r == -2);  /* ID mismatch */
    (void)r;
}

/* ====================================================================
 * Tier 4 — Lifecycle
 * ==================================================================== */

static void test_lifecycle(void)
{
    struct ccdns_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    bool ok = ccdns_init(&ctx);
    if (ok) {
        assert(ctx.fd != INVALID_SOCKET);
        assert(ctx.reqno == 0);

        uint8_t buf[CCDNS_MAX_MSG];
        uint16_t n = ccdns_encode(&ctx, buf, sizeof(buf), "example.com", CCDNS_A);
        assert(n > 0);
        assert(ctx.reqno == 1);
        (void)n;

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
    test_decode_a();
    test_decode_id_mismatch();
    test_lifecycle();
    return 0;
}
