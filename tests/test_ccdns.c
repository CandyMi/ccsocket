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
 * Tier 6 — TCP mode encode
 * ==================================================================== */

static void test_tcp_encode(void)
{
    struct ccdns_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    assert(ccdns_init(&ctx));

    /* First encode without TCP to know the reference body */
    uint8_t ref[CCDNS_MAX_MSG];
    uint16_t reflen = ccdns_encode(&ctx, ref, sizeof(ref), "google.com", CCDNS_A);
    assert(reflen > 0);

    /* Now encode in TCP mode (new ctx to reset ID) */
    struct ccdns_t tcp_ctx;
    memset(&tcp_ctx, 0, sizeof(tcp_ctx));
    assert(ccdns_init(&tcp_ctx));
    ccdns_set_tcp(&tcp_ctx, true);

    uint8_t buf[CCDNS_MAX_MSG];
    uint16_t n = ccdns_encode(&tcp_ctx, buf, sizeof(buf), "google.com", CCDNS_A);
    assert(n > 0);

    /* Total length = 2 (prefix) + body length */
    assert(n == reflen + 2);

    /* Verify 2-byte length prefix */
    uint16_t prefixed_len = ((uint16_t)buf[0] << 8) | buf[1];
    assert(prefixed_len == reflen);

    /* Verify the DNS body (buf[2..]) matches the reference */
    assert(memcmp(buf + 2, ref, reflen) == 0);

    ccdns_close(&tcp_ctx);
}

/* ====================================================================
 * Tier 7 — TCP mode decode
 * ==================================================================== */

static void test_tcp_decode(void)
{
    /* Same response as test_decode_a, but with 2-byte TCP length prefix */
    uint8_t body[] = {
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
    uint16_t bodylen = sizeof(body);

    uint8_t resp[CCDNS_MAX_MSG];
    resp[0] = (uint8_t)(bodylen >> 8);
    resp[1] = (uint8_t)(bodylen & 0xFF);
    memcpy(resp + 2, body, bodylen);

    struct ccdns_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.no = 1;
    ctx.tcp = true;

    memset(&g_ans, 0, sizeof(g_ans));
    int r = ccdns_decode(&ctx, resp, bodylen + 2, NULL, on_answer);
    assert(r == 1);
    assert(g_ans.type == CCDNS_A);
    assert(g_ans.cls == CCDNS_CLASS_IN);
    assert(g_ans.ttl == 300);
    assert(strcmp(g_ans.ip, "142.250.80.46") == 0);
    assert(strcmp(g_ans.domain, "google.com") == 0);
    (void)r;

    /* ---- Without TCP mode, the TCP length prefix bytes get parsed as DNS ID ---- */
    struct ccdns_t ctx2;
    memset(&ctx2, 0, sizeof(ctx2));
    ctx2.no = 1;
    /* ctx2.tcp defaults to false — reading length prefix as header gives wrong ID */
    int r2 = ccdns_decode(&ctx2, resp, bodylen + 2, NULL, NULL);
    assert(r2 == -2);  /* ID mismatch: bodylen != 1 */
    (void)r2;
}

/* ====================================================================
 * Tier 8 — TXT mode encode
 * ==================================================================== */

static void test_txt_encode(void)
{
    struct ccdns_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    assert(ccdns_init(&ctx));

    uint8_t buf[CCDNS_MAX_MSG];
    uint16_t n = ccdns_encode(&ctx, buf, sizeof(buf), "google.com", CCDNS_TXT);
    assert(n > 0);

    /* Find QTYPE in the question section (right after the QNAME) */
    /* QNAME = 6google3com0 (12 bytes starting at offset 12) */
    /* QTYPE starts at offset 24 */
    assert(buf[24] == 0x00);
    assert(buf[25] == 0x10);  /* TXT = 16 */

    ccdns_close(&ctx);
}

/* ====================================================================
 * Tier 9 — TXT decode single string
 * ==================================================================== */

static void test_txt_decode_single(void)
{
    uint8_t resp[] = {
        0x00, 0x01,           /* ID = 1 */
        0x81, 0x80,           /* flags */
        0x00, 0x01,           /* QDCOUNT = 1 */
        0x00, 0x01,           /* ANCOUNT = 1 */
        0x00, 0x00, 0x00, 0x00,
        /* Question: google.com */
        0x06, 'g','o','o','g','l','e',
        0x03, 'c','o','m',
        0x00,
        0x00, 0x10,           /* QTYPE = TXT */
        0x00, 0x01,           /* QCLASS = IN */
        /* Answer */
        0xc0, 0x0c,           /* NAME → offset 12 */
        0x00, 0x10,           /* TYPE = TXT */
        0x00, 0x01,           /* CLASS = IN */
        0x00, 0x00, 0x01, 0x2c, /* TTL = 300 */
        0x00, 0x06,           /* RDLENGTH = 6 */
        0x05, 'h','e','l','l','o'  /* "hello" */
    };

    struct ccdns_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.no = 1;

    memset(&g_ans, 0, sizeof(g_ans));
    int r = ccdns_decode(&ctx, resp, sizeof(resp), NULL, on_answer);
    assert(r == 1);
    assert(g_ans.type == CCDNS_TXT);
    assert(g_ans.cls == CCDNS_CLASS_IN);
    assert(g_ans.ttl == 300);
    assert(strcmp(g_ans.txt, "hello") == 0);
    (void)r;
}

/* ====================================================================
 * Tier 10 — TXT decode multi-string (e.g. SPF)
 * ==================================================================== */

static void test_txt_decode_multi(void)
{
    /* Two character-strings: "hello" + "world" → joined as "hello\nworld" */
    uint8_t resp[] = {
        0x00, 0x01,           /* ID = 1 */
        0x81, 0x80,           /* flags */
        0x00, 0x01,           /* QDCOUNT = 1 */
        0x00, 0x01,           /* ANCOUNT = 1 */
        0x00, 0x00, 0x00, 0x00,
        /* Question: google.com */
        0x06, 'g','o','o','g','l','e',
        0x03, 'c','o','m',
        0x00,
        0x00, 0x10,           /* QTYPE = TXT */
        0x00, 0x01,           /* QCLASS = IN */
        /* Answer */
        0xc0, 0x0c,           /* NAME → offset 12 */
        0x00, 0x10,           /* TYPE = TXT */
        0x00, 0x01,           /* CLASS = IN */
        0x00, 0x00, 0x00, 0x3c, /* TTL = 60 */
        0x00, 0x0C,           /* RDLENGTH = 12 */
        0x05, 'h','e','l','l','o',    /* "hello" */
        0x05, 'w','o','r','l','d'     /* "world" */
    };

    struct ccdns_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.no = 1;

    memset(&g_ans, 0, sizeof(g_ans));
    int r = ccdns_decode(&ctx, resp, sizeof(resp), NULL, on_answer);
    assert(r == 1);
    assert(g_ans.type == CCDNS_TXT);
    assert(strcmp(g_ans.txt, "hello\nworld") == 0);
    (void)r;
}

/* ====================================================================
 * Tier 11 — MX encode
 * ==================================================================== */

static void test_mx_encode(void)
{
    struct ccdns_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    assert(ccdns_init(&ctx));

    uint8_t buf[CCDNS_MAX_MSG];
    uint16_t n = ccdns_encode(&ctx, buf, sizeof(buf), "example.com", CCDNS_MX);
    assert(n > 0);

    /* QTYPE is at offset 25 (after QNAME "7example3com0" + root) */
    assert(buf[25] == 0x00);
    assert(buf[26] == 0x0f);  /* MX = 15 */

    ccdns_close(&ctx);
}

/* ====================================================================
 * Tier 12 — MX decode single record
 * ==================================================================== */

static void test_mx_decode_single(void)
{
    /* Response for example.com MX: preference=10, exchange=mail.example.com */
    uint8_t resp[] = {
        0x00, 0x01,           /* ID = 1 */
        0x81, 0x80,           /* flags */
        0x00, 0x01,           /* QDCOUNT = 1 */
        0x00, 0x01,           /* ANCOUNT = 1 */
        0x00, 0x00, 0x00, 0x00,
        /* Question: example.com */
        0x07, 'e','x','a','m','p','l','e',
        0x03, 'c','o','m',
        0x00,
        0x00, 0x0f,           /* QTYPE = MX */
        0x00, 0x01,           /* QCLASS = IN */
        /* Answer: MX */
        0xc0, 0x0c,           /* NAME → offset 12 */
        0x00, 0x0f,           /* TYPE = MX */
        0x00, 0x01,           /* CLASS = IN */
        0x00, 0x00, 0x00, 0x3c, /* TTL = 60 */
        0x00, 0x14,           /* RDLENGTH = 20 (2 pref + 18 wire name) */
        0x00, 0x0a,           /* PREFERENCE = 10 */
        0x04, 'm','a','i','l',
        0x07, 'e','x','a','m','p','l','e',
        0x03, 'c','o','m',
        0x00
    };

    struct ccdns_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.no = 1;

    memset(&g_ans, 0, sizeof(g_ans));
    int r = ccdns_decode(&ctx, resp, sizeof(resp), NULL, on_answer);
    assert(r == 1);
    assert(g_ans.type == CCDNS_MX);
    assert(g_ans.cls == CCDNS_CLASS_IN);
    assert(g_ans.ttl == 60);
    assert(g_ans.pref == 10);
    assert(strcmp(g_ans.domain, "mail.example.com") == 0);
    (void)r;
}

/* ====================================================================
 * Tier 13 — MX decode multiple records (priority ordering)
 * ==================================================================== */

static void test_mx_decode_multi(void)
{
    /* Two MX answers: preference=10 (mail), preference=20 (backup) */
    uint8_t resp[] = {
        0x00, 0x01,           /* ID = 1 */
        0x81, 0x80,           /* flags */
        0x00, 0x01,           /* QDCOUNT = 1 */
        0x00, 0x02,           /* ANCOUNT = 2 */
        0x00, 0x00, 0x00, 0x00,
        /* Question: example.com */
        0x07, 'e','x','a','m','p','l','e',
        0x03, 'c','o','m',
        0x00,
        0x00, 0x0f,           /* QTYPE = MX */
        0x00, 0x01,           /* QCLASS = IN */
        /* Answer 1: MX preference=10, exchange=mail.example.com */
        0xc0, 0x0c,           /* NAME → offset 12 */
        0x00, 0x0f,           /* TYPE = MX */
        0x00, 0x01,           /* CLASS = IN */
        0x00, 0x00, 0x00, 0x3c, /* TTL = 60 */
        0x00, 0x14,           /* RDLENGTH = 20 (2 pref + 18 wire name) */
        0x00, 0x0a,           /* PREFERENCE = 10 */
        0x04, 'm','a','i','l',
        0x07, 'e','x','a','m','p','l','e',
        0x03, 'c','o','m',
        0x00,
        /* Answer 2: MX preference=20, exchange=backup.example.com */
        0xc0, 0x0c,           /* NAME → offset 12 (same owner) */
        0x00, 0x0f,           /* TYPE = MX */
        0x00, 0x01,           /* CLASS = IN */
        0x00, 0x00, 0x01, 0x2c, /* TTL = 300 */
        0x00, 0x16,           /* RDLENGTH = 22 */
        0x00, 0x14,           /* PREFERENCE = 20 */
        0x06, 'b','a','c','k','u','p',
        0x07, 'e','x','a','m','p','l','e',
        0x03, 'c','o','m',
        0x00
    };

    struct ccdns_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.no = 1;

    g_ans_cnt = 0;
    memset(g_ans_list, 0, sizeof(g_ans_list));

    int r = ccdns_decode(&ctx, resp, sizeof(resp), NULL, on_answer_list);
    assert(r == 2);
    assert(g_ans_cnt == 2);

    /* Answer 1: pref=10 */
    assert(g_ans_list[0].type == CCDNS_MX);
    assert(g_ans_list[0].pref == 10);
    assert(strcmp(g_ans_list[0].domain, "mail.example.com") == 0);

    /* Answer 2: pref=20 */
    assert(g_ans_list[1].type == CCDNS_MX);
    assert(g_ans_list[1].pref == 20);
    assert(strcmp(g_ans_list[1].domain, "backup.example.com") == 0);
    assert(g_ans_list[1].ttl == 300);

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
    test_tcp_encode();
    test_tcp_decode();
    test_txt_encode();
    test_txt_decode_single();
    test_txt_decode_multi();
    test_mx_encode();
    test_mx_decode_single();
    test_mx_decode_multi();
    return 0;
}
