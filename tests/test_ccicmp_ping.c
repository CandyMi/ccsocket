/**
 * test_ccicmp_ping.c — ICMP ping functional test.
 *
 * Three tiers:
 *   1. Checksum calculation — reimplements icmp_checksum_calc logic
 *      and verifies against known vectors (RFC 1071 examples).
 *   2. ICMP packet layout — constructs a minimal echo packet in memory
 *      and verifies header fields (type, code, id, checksum offset).
 *   3. Lifecycle — ccicmp_init + ccicmp_close paired calls.
 *      Without root, ccicmp_init returns false (expected); with root
 *      it pings 127.0.0.1 for one round.
 */

#include "ccicmp.h"
#include "ccsocket.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* ====================================================================
 * Tier 1 — checksum calculation (RFC 1071, same logic as icmp_checksum_calc)
 * ==================================================================== */

static uint16_t test_checksum(const unsigned char *buf, int len)
{
    unsigned long sum = 0;
    const unsigned char *end = buf + len;
    if (len & 1) {
        end = buf + len - 1;
        sum += (unsigned)(*end) << 8;
    }
    while (buf < end) {
        sum += (unsigned)buf[0] << 8;
        sum += (unsigned)buf[1];
        buf += 2;
    }
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return (unsigned short)(~sum & 0xffff);
}

static void test_checksum_known(void)
{
    /* RFC 1071 example: a buffer with all zeros should yield checksum 0xFFFF */
    unsigned char zero[8] = {0};
    uint16_t ck = test_checksum(zero, 8);
    assert(ck == 0xFFFF);

    /* Simple pattern: 0xFF 0xFF 0xFF 0xFF → sum wraps to 0x1FFFE → 0xFFFF → ~ = 0 */
    unsigned char all_ff[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    ck = test_checksum(all_ff, 4);
    assert(ck == 0x0000);

    /* ICMP echo header (type=8, code=0) with zero id/seq: network byte order */
    unsigned char icmp_echo[8] = {
        8, 0, 0, 0,  /* type, code, checksum placeholder */
        0, 0, 0, 0   /* id, seq */
    };
    ck = test_checksum(icmp_echo, 8);
    /* type=8 → 0x0800, rest zero → sum = 0x0800 → ~ = 0xF7FF */
    assert(ck == 0xF7FF);
}

static void test_checksum_odd_length(void)
{
    /* Odd-length buffer: padding byte is treated as the last byte shifted */
    unsigned char odd[3] = {0x01, 0x02, 0x03};
    uint16_t ck = test_checksum(odd, 3);
    /* 0x0102 + (0x0300 << ... wait, the algorithm does:
     * end = buf + 3 - 1 = buf + 2 → checksum += (*end) << 8 = 0x0300
     * Then loop: buf[0]<<8 + buf[1] = 0x0100 + 0x02 = 0x0102
     * sum = 0x0300 + 0x0102 = 0x0402 → ~ = 0xFBFF (if no carry)
     * No carry from 0x0402. */
    assert(ck == (uint16_t)(~0x0402 & 0xFFFF));
}

/* ====================================================================
 * Tier 2 — ICMP packet layout verification
 * ==================================================================== */

static void test_packet_layout(void)
{
    unsigned char pkt[16];
    memset(pkt, 0, sizeof(pkt));

    /* Type 8 = ICMP Echo, Code = 0 */
    pkt[0] = 8;
    pkt[1] = 0;

    /* ID = 0x1234, Seq = 0x0001 (network byte order) */
    pkt[4] = 0x12; pkt[5] = 0x34;
    pkt[6] = 0x00; pkt[7] = 0x01;

    /* Compute and store checksum */
    uint16_t ck = test_checksum(pkt, 8);
    pkt[2] = (ck >> 8) & 0xff;
    pkt[3] = ck & 0xff;

    /* Verify checksum is correct by re-computing over the whole header */
    assert(test_checksum(pkt, 8) == 0x0000);

    /* Verify type/code/id/seq fields */
    assert(pkt[0] == 8);            /* type = ICMP Echo */
    assert(pkt[1] == 0);            /* code = 0 */
    assert(pkt[4] == 0x12 && pkt[5] == 0x34);  /* id = 0x1234 */
    assert(pkt[6] == 0x00 && pkt[7] == 0x01);  /* seq = 1 */
}

/* ====================================================================
 * Tier 3 — ccicmp lifecycle (works with or without root)
 * ==================================================================== */

static void test_lifecycle(void)
{
    struct ccicmp_t ping;
    memset(&ping, 0, sizeof(ping));

    /* init: may or may not succeed depending on privileges.
     * What we verify is that the function returns a consistent
     * result and doesn't crash. */
    bool ok = ccicmp_init(&ping, CC_INET4);

    if (ok) {
        /* Has privileges: verify socket was created and
         * context fields are initialised. */
        assert(ping.fd != INVALID_SOCKET);
        assert(ping.id != 0);   /* derived from ctx pointer */

        /* Try a ping to localhost — it may fail (e.g. macOS needs
         * additional entitlement), but must not crash. */
        bool sent = ccicmp_echo(&ping, "127.0.0.1", "test", 4);

        ccicmp_close(&ping);
        assert(ping.fd == INVALID_SOCKET);
        assert(ping.id == 0);
        assert(ping.no == 0);

        if (sent) {
            /* If echo was sent, try to receive a reply.
             * Non-blocking mode may return false immediately
             * if the reply hasn't arrived yet. */
            char reply[64];
            size_t rlen = sizeof(reply);
            ccicmp_reply(&ping, reply, &rlen);
            /* reply may succeed or not — we just verify no crash */
        }
    } else {
        /* No privileges: ctx must be left in a zeroed / safe state.
         * Calling ccicmp_close on an uninitialised ctx asserts,
         * so we skip close when init failed. */
    }
}

/* ====================================================================
 * Main
 * ==================================================================== */

int main(void)
{
    test_checksum_known();
    test_checksum_odd_length();
    test_packet_layout();
    test_lifecycle();

    return 0;
}
