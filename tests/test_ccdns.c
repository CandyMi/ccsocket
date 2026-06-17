/**
 * test_ccdns.c — DNS client encode/decode test.
 *
 * Verifies:
 *   1. ccdns_encode produces correct wire-format query bytes
 *   2. ccdns_decode with callback extracts A record
 *   3. ccdns_decode ID mismatch detection
 *   4. ccdns_init / ccdns_close lifecycle
 *   5. DNS name compression (RFC 1035 §4.1.4)
 */

#include "ccdns.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* ====================================================================
 * Decode callbacks
 * ==================================================================== */

/* Single-answer collector (for tests 2, 3) */
static ccdns_ans_t g_ans;

static void on_answer(void *udata, const ccdns_ans_t *ans)
{
    (void)udata;
    memcpy(&g_ans, ans, sizeof(g_ans));
}

/* Multi-answer collector (for test 5 — compression) */
static ccdns_ans_t g_ans_list[8];
static int         g_ans_cnt;

static void on_answer_list(void *udata, const ccdns_ans_t *ans)
{
    (void)udata;
    if (g_ans_cnt < 8) {
        memcpy(&g_ans_list[g_ans_cnt++], ans, sizeof(g_ans_list[0]));
    }
}

/* ====================================================================
 * Tier 1 — Encode test
 * ==================================================================== */

static void test_encode_basic(void)
{
    struct ccdns_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    assert(ccdns_init(&ctx));
    assert(ctx.no == 1);

    uint8_t buf[CCDNS_MAX_MSG];
    uint16_t n = ccdns_encode(&ctx, buf, sizeof(buf), "google.com", CCDNS_A);
    assert(n > 0);
    assert(ctx.no == 2);

    /* Header: ID=1, flags=RD(0x0100), QDCOUNT=1 */
    assert(buf[0] == 0 && buf[1] == 1);
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
    assert(n == 28);

    ccdns_close(&ctx);
    assert(ctx.no == 0);
}

/* ====================================================================
 * Tier 2 — Decode A record (with compression pointer)
 * ==================================================================== */

static void test_decode_a(void)
{
    uint8_t resp[] = {
        0x00, 0x01,           /* ID = 1 */
        0x81, 0x80,           /* flags */
        0x00, 0x01,           /* QDCOUNT = 1 */
        0x00, 0x01,           /* ANCOUNT = 1 */
        0x00, 0x00, 0x00, 0x00,
        /* Question */
        0x06, 'g','o','o','g','l','e', 0x03, 'c','o','m', 0x00,
        0x00, 0x01, 0x00, 0x01,
        /* Answer (NAME = 0xc00c → compression ptr to offset 12) */
        0xc0, 0x0c,
        0x00, 0x01,           /* TYPE = A */
        0x00, 0x01,           /* CLASS = IN */
        0x00, 0x00, 0x01, 0x2c, /* TTL = 300 */
        0x00, 0x04,           /* RDLENGTH = 4 */
        142, 250, 80, 46
    };

    struct ccdns_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.no = 1;

    memset(&g_ans, 0, sizeof(g_ans));
    int r = ccdns_decode(&ctx, resp, sizeof(resp), NULL, on_answer);
    assert(r == 1);
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
    ctx.no = 0x5678;

    int r = ccdns_decode(&ctx, resp, sizeof(resp), NULL, NULL);
    assert(r == -2);
    (void)r;
}

/* ====================================================================
 * Tier 4 — Lifecycle
 * ==================================================================== */

static void test_lifecycle(void)
{
    struct ccdns_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    assert(ccdns_init(&ctx));
    assert(ctx.no == 1);

    {
        uint8_t buf[CCDNS_MAX_MSG];
        uint16_t n = ccdns_encode(&ctx, buf, sizeof(buf), "example.com", CCDNS_A);
        assert(n > 0);
        assert(ctx.no == 2);
        (void)n;
    }

    ccdns_close(&ctx);
    assert(ctx.no == 0);
}

/* ====================================================================
 * Tier 5 — Compression name (RFC 1035 §4.1.4)
 *
 * Tests multiple answers using compression pointers in both
 * NAME fields and RDATA (CNAME target).
 *
 * Response for www.example.com:
 *   Question: www.example.com (A)
 *   Answer 1: CNAME example.com   (NAME=ptr→QNAME, RDATA=ptr→"example" within QNAME)
 *   Answer 2: A 93.184.216.34     (NAME=ptr→"example" within QNAME)
 *
 * Wire layout offsets:
 *   0-11: header
 *   12:   3www
 *   16:   7example
 *   24:   3com
 *   28:   0 (root)
 *   29-32: QTYPE + QCLASS
 *   33+:   answers
 * ==================================================================== */

static void test_compression_name(void)
{
    uint8_t resp[] = {
        /* Header */
        0x00, 0x01, 0x81, 0x80,
        0x00, 0x01,           /* QDCOUNT = 1 */
        0x00, 0x02,           /* ANCOUNT = 2 */
        0x00, 0x00, 0x00, 0x00,
        /* Question: www.example.com */
        0x03, 'w','w','w',
        0x07, 'e','x','a','m','p','l','e',
        0x03, 'c','o','m',
        0x00,
        0x00, 0x01,           /* QTYPE = A */
        0x00, 0x01,           /* QCLASS = IN */
        /* Answer 1: CNAME, NAME=ptr→12, RDATA=ptr→16 */
        0xc0, 0x0c,           /* NAME → offset 12 = "www.example.com" */
        0x00, 0x05,           /* TYPE = CNAME */
        0x00, 0x01,           /* CLASS = IN */
        0x00, 0x00, 0x00, 0x3c, /* TTL = 60 */
        0x00, 0x02,           /* RDLENGTH = 2 */
        0xc0, 0x10,           /* RDATA → offset 16 = "example.com" */
        /* Answer 2: A, NAME=ptr→16 */
        0xc0, 0x10,           /* NAME → offset 16 = "example.com" */
        0x00, 0x01,           /* TYPE = A */
        0x00, 0x01,           /* CLASS = IN */
        0x00, 0x00, 0x00, 0x3c, /* TTL = 60 */
        0x00, 0x04,           /* RDLENGTH = 4 */
        93, 184, 216, 34
    };

    struct ccdns_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.no = 1;

    g_ans_cnt = 0;
    memset(g_ans_list, 0, sizeof(g_ans_list));

    int r = ccdns_decode(&ctx, resp, sizeof(resp), NULL, on_answer_list);
    assert(r == 2);      /* 2 answers delivered */
    assert(g_ans_cnt == 2);

    /* Answer 1: CNAME — owner name decoded from ptr, RDATA decoded from ptr */
    assert(g_ans_list[0].type == CCDNS_CNAME);
    assert(g_ans_list[0].cls == CCDNS_CLASS_IN);
    assert(g_ans_list[0].ttl == 60);
    /* CNAME: domain field holds the target (example.com) from RDATA ptr */
    assert(strcmp(g_ans_list[0].domain, "example.com") == 0);

    /* Answer 2: A — name decoded from compression ptr to "example.com" */
    assert(g_ans_list[1].type == CCDNS_A);
    assert(g_ans_list[1].cls == CCDNS_CLASS_IN);
    assert(g_ans_list[1].ttl == 60);
    assert(strcmp(g_ans_list[1].domain, "example.com") == 0);
    assert(strcmp(g_ans_list[1].ip, "93.184.216.34") == 0);

    (void)r;
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
    test_compression_name();
    return 0;
}
