/**
 * @file ccdns.c
 * @brief DNS client — wire-format encode/decode (RFC 1035).
 *
 * Pure algorithm implementation.  No network I/O — encode() produces
 * bytes for ccsocket_send(), decode() parses bytes from ccsocket_recv().
 *
 * @author CandyMi
 * @license MIT
 */

#include "ccdns.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#if defined(_MSC_VER) && _MSC_VER < 1900
  #define ccdns_snprintf _snprintf
#else
  #define ccdns_snprintf snprintf
#endif

/* ---- DNS Header Offsets (RFC 1035 §4.1.1) -------------------------------- */
#define DNS_HDR_SZ      12
#define DNS_HDR_ID      0   /* uint16_t */
#define DNS_HDR_FLAGS   2   /* uint16_t */
#define DNS_HDR_QDCNT   4   /* uint16_t */
#define DNS_HDR_ANCNT   6   /* uint16_t */
#define DNS_HDR_NSCNT   8   /* uint16_t */
#define DNS_HDR_ARCNT   10  /* uint16_t */

/* Standard DNS flags for a query (RD=1) */
#define DNS_FLAG_QR_QUERY   0x0100  /* RD = 1 */
/* Standard DNS flags for a response (QR=1, RA=1) */
#define DNS_FLAG_QR_RESP    0x8180  /* QR=1, RD=1, RA=1 */

/* ---- Internal Helpers ---------------------------------------------------- */

/**
 * @brief Encode a domain name into DNS label format.
 *
 * "www.example.com" → 3www7example3com0
 * Each label: <length-byte> <label-chars>
 * Terminated by a zero-length label (root).
 *
 * @param dst     Output buffer.
 * @param dstlen  Capacity of dst.
 * @param src     NUL-terminated domain name string.
 * @return Number of bytes written, or 0 on error (buffer too small).
 */
static uint16_t dns_name_encode(uint8_t *dst, uint16_t dstlen, const char *src)
{
    uint16_t pos = 0;
    const char *dot;
    size_t labellen;

    while (src && *src) {
        dot = strchr(src, '.');
        labellen = dot ? (size_t)(dot - src) : strlen(src);

        if (labellen == 0 || labellen > 63)
            return 0;          /* empty or overlong label */
        if (pos + 1 + labellen + 1 > dstlen)
            return 0;          /* buffer overflow */

        dst[pos++] = (uint8_t)labellen;
        memcpy(dst + pos, src, labellen);
        pos += (uint16_t)labellen;

        src = dot ? dot + 1 : NULL;
    }

    /* Root label (zero-length terminator) */
    if (pos + 1 > dstlen)
        return 0;
    dst[pos++] = 0;
    return pos;
}

/**
 * @brief Decode a DNS name from wire format, handling compression pointers
 *        (RFC 1035 §4.1.4).
 *
 * @param msg   Start of the DNS message (for pointer resolution).
 * @param pos   Pointer to current position in msg (in/out).
 * @param len   Total length of msg.
 * @param dst   Output buffer for the decoded NUL-terminated name.
 * @param dstlen Capacity of dst.
 * @return 0 on success, -1 on error.
 */
static int dns_name_decode(const uint8_t *msg, uint16_t *pos, uint16_t len,
                            char *dst, uint16_t dstlen)
{
    uint16_t di = 0;
    uint16_t p  = *pos;
    int      jumped = 0;
    int      cnt = 0;   /* prevent infinite loops via pointer chains */

    while (p < len) {
        uint8_t b = msg[p];
        if (b == 0) {
            /* Root label terminator */
            p++;
            break;
        }
        if ((b & 0xC0) == 0xC0) {
            /* Compression pointer (upper 2 bits = 11) */
            if (p + 2 > len) return -1;
            uint16_t off = ((uint16_t)(b & 0x3F) << 8) | msg[p + 1];
            if (!jumped) {
                *pos = p + 2;  /* advance caller past pointer */
                jumped = 1;
            }
            p = off;
            if (++cnt > 16) return -1;  /* safety limit */
            continue;
        }
        /* Normal label: length byte followed by label data */
        uint8_t labellen = b;
        if (p + 1 + labellen > len) return -1;
        p++;
        if (di + labellen + 1 > dstlen) return -1;
        if (di > 0) dst[di++] = '.';
        memcpy(dst + di, msg + p, labellen);
        di += labellen;
        p += labellen;
    }

    if (!jumped)
        *pos = p;

    if (di >= dstlen) return -1;
    dst[di] = '\0';
    return 0;
}

/* ---- Public API ---------------------------------------------------------- */

bool ccdns_init(struct ccdns_t *ctx)
{
    if (!ctx)
        return false;

    ctx->reqno = 0;
    ctx->fd = ccsocket(CC_INET4, CC_UDP);
    if (ctx->fd == INVALID_SOCKET)
        return false;

    return true;
}

void ccdns_close(struct ccdns_t *ctx)
{
    if (!ctx)
        return;
    if (ctx->fd != INVALID_SOCKET)
        ccsocket_close(ctx->fd);
    ctx->fd = INVALID_SOCKET;
    ctx->reqno = 0;
}

uint16_t ccdns_encode(struct ccdns_t *ctx,
                       uint8_t *buf, uint16_t buflen,
                       const char *domain, uint16_t qtype)
{
    if (!ctx || !buf || !domain || buflen < DNS_HDR_SZ + 5)
        return 0;

    uint16_t id = ctx->reqno++;

    /* ---- Header ---- */
    memset(buf, 0, DNS_HDR_SZ);
    buf[DNS_HDR_ID]     = (uint8_t)(id >> 8);
    buf[DNS_HDR_ID + 1] = (uint8_t)(id & 0xFF);
    buf[DNS_HDR_FLAGS]     = (uint8_t)(DNS_FLAG_QR_QUERY >> 8);
    buf[DNS_HDR_FLAGS + 1] = (uint8_t)(DNS_FLAG_QR_QUERY & 0xFF);
    buf[DNS_HDR_QDCNT + 1] = 1;  /* QDCOUNT = 1 */

    /* ---- Question: QNAME ---- */
    uint16_t pos = DNS_HDR_SZ;
    uint16_t nlen = dns_name_encode(buf + pos, buflen - pos, domain);
    if (nlen == 0)
        return 0;
    pos += nlen;

    /* ---- Question: QTYPE + QCLASS ---- */
    if (pos + 4 > buflen)
        return 0;
    buf[pos]     = (uint8_t)(qtype >> 8);
    buf[pos + 1] = (uint8_t)(qtype & 0xFF);
    buf[pos + 2] = 0;
    buf[pos + 3] = CCDNS_CLASS_IN;
    pos += 4;

    return pos;
}

int ccdns_decode(const uint8_t *buf, uint16_t len,
                  uint16_t expected_id,
                  char *addr, uint16_t *addrlen)
{
    if (!buf || !addr || !addrlen || len < DNS_HDR_SZ)
        return -1;

    uint16_t minaddr = *addrlen;
    if (minaddr < 4) return -3;

    /* ---- Parse Header ---- */
    uint16_t id     = ((uint16_t)buf[DNS_HDR_ID] << 8) | buf[DNS_HDR_ID + 1];
    uint16_t flags  = ((uint16_t)buf[DNS_HDR_FLAGS] << 8) | buf[DNS_HDR_FLAGS + 1];
    uint16_t qdcnt  = ((uint16_t)buf[DNS_HDR_QDCNT] << 8) | buf[DNS_HDR_QDCNT + 1];
    uint16_t ancnt  = ((uint16_t)buf[DNS_HDR_ANCNT] << 8) | buf[DNS_HDR_ANCNT + 1];

    if (id != expected_id)
        return -2;

    /* Check QR=1 (response) and RCODE=0 (no error) */
    if (!(flags & 0x8000) || (flags & 0x0F) != 0)
        return -1;

    if (ancnt == 0)
        return -4;

    /* ---- Skip Question Section ---- */
    uint16_t pos = DNS_HDR_SZ;
    for (uint16_t i = 0; i < qdcnt; i++) {
        /* Skip QNAME */
        while (pos < len) {
            uint8_t b = buf[pos];
            if (b == 0) {
                pos++;
                break;
            }
            if ((b & 0xC0) == 0xC0) {
                pos += 2;
                break;
            }
            pos += 1 + b;
        }
        if (pos + 4 > len) return -1;
        pos += 4;  /* skip QTYPE + QCLASS */
    }

    /* ---- Parse Answer Section (first matching A/AAAA) ---- */
    char namebuf[CCDNS_MAX_NAME];
    for (uint16_t i = 0; i < ancnt; i++) {
        /* NAME */
        if (dns_name_decode(buf, &pos, len, namebuf, sizeof(namebuf)) != 0)
            return -1;

        if (pos + 10 > len) return -1;  /* TYPE + CLASS + TTL + RDLENGTH */
        uint16_t rrtype  = ((uint16_t)buf[pos] << 8) | buf[pos + 1];
        uint16_t rdlength = ((uint16_t)buf[pos + 8] << 8) | buf[pos + 9];
        pos += 10;

        if (pos + rdlength > len) return -1;

        if ((rrtype == CCDNS_A || rrtype == CCDNS_AAAA) && rdlength > 0) {
            if (rrtype == CCDNS_A && rdlength == 4 && minaddr >= 16) {
                /* IPv4 */
                int n = ccdns_snprintf(addr, minaddr, "%u.%u.%u.%u",
                                 buf[pos], buf[pos+1], buf[pos+2], buf[pos+3]);
                if (n > 0) *addrlen = (uint16_t)n;
                return 0;
            }
            if (rrtype == CCDNS_AAAA && rdlength == 16 && minaddr >= 40) {
                /* IPv6 */
                int n = ccdns_snprintf(addr, minaddr,
                                 "%02x%02x:%02x%02x:%02x%02x:%02x%02x:"
                                 "%02x%02x:%02x%02x:%02x%02x:%02x%02x",
                                 buf[pos], buf[pos+1], buf[pos+2], buf[pos+3],
                                 buf[pos+4], buf[pos+5], buf[pos+6], buf[pos+7],
                                 buf[pos+8], buf[pos+9], buf[pos+10], buf[pos+11],
                                 buf[pos+12], buf[pos+13], buf[pos+14], buf[pos+15]);
                if (n > 0) *addrlen = (uint16_t)n;
                return 0;
            }
        }
        pos += rdlength;
    }

    return -4;  /* No matching address record */
}
