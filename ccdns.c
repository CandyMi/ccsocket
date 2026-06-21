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

#include <string.h>
#include <stdio.h>

#if defined(_MSC_VER) && _MSC_VER < 1900
  #define ccdns_snprintf _snprintf
#else
  #define ccdns_snprintf snprintf
#endif

#include "ccdns.h"

/* ---- DNS Header Offsets (RFC 1035 §4.1.1) -------------------------------- */
#define DNS_HDR_SZ      12
#define DNS_HDR_ID      0   /* uint16_t */
#define DNS_HDR_FLAGS   2   /* uint16_t */
#define DNS_HDR_QDCNT   4   /* uint16_t */
#define DNS_HDR_ANCNT   6   /* uint16_t */
#define DNS_HDR_AUCNT   8   /* uint16_t */
#define DNS_HDR_ADCNT   10  /* uint16_t */

/* Standard DNS flags */
#define DNS_FLAG_QR_QUERY   0x0100  /* RD = 1 (recursion desired) */

/* ---- Internal Helpers ---------------------------------------------------- */

/**
 * @brief Encode a domain name into DNS label format.
 *
 * "www.example.com" → 3www7example3com0
 * Each label: <length-byte> <label-chars>
 * Terminated by a zero-length label (root).
 */
static uint16_t dns_name_encode(uint8_t *dst, uint16_t dstlen, const char *src)
{
    uint16_t pos = 0;

    while (src && *src) {
        const char *dot = strchr(src, '.');
        size_t labellen = dot ? (size_t)(dot - src) : strlen(src);

        if (labellen == 0 || labellen > 63)
            return 0;
        if (pos + 1 + labellen + 1 > dstlen)
            return 0;

        dst[pos++] = (uint8_t)labellen;
        memcpy(dst + pos, src, labellen);
        pos += (uint16_t)labellen;

        src = dot ? dot + 1 : NULL;
    }

    if (pos + 1 > dstlen)
        return 0;
    dst[pos++] = 0;
    return pos;
}

/**
 * @brief Decode a DNS name from wire format, handling compression pointers
 *        (RFC 1035 §4.1.4).
 *
 * Compression pointer offsets are relative to the start of the DNS
 * message (base_off bytes into buf).  In TCP mode, base_off is 2;
 * in UDP mode it is 0.
 */
static int dns_name_decode(const uint8_t *msg, uint16_t *pos, uint16_t len,
                            uint16_t base_off, char *dst, uint16_t dstlen)
{
    uint16_t di = 0;
    uint16_t p  = *pos;
    int      jumped = 0;
    int      cnt = 0;

    while (p < len) {
        uint8_t b = msg[p];
        if (b == 0) {
            p++;
            break;
        }
        if ((b & 0xC0) == 0xC0) {
            if (p + 2 > len) return -1;
            uint16_t off = ((uint16_t)(b & 0x3F) << 8) | msg[p + 1];
            uint16_t jumped_to = (uint16_t)(off + base_off);
            if (!jumped) {
                *pos = p + 2;
                jumped = 1;
            }
            p = jumped_to;
            if (++cnt > 16) return -1;
            continue;
        }
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

    ctx->no = 1;
    ctx->last = 0;
    ctx->edns_payload = 0;
    ctx->edns_flags = 0;
    ctx->tcp = false;
    return true;
}

void ccdns_close(struct ccdns_t *ctx)
{
    if (!ctx)
        return;
    ctx->no = 0;
    ctx->last = 0;
    ctx->edns_payload = 0;
    ctx->edns_flags = 0;
    ctx->tcp = false;
}

void ccdns_set_edns(struct ccdns_t *ctx, uint16_t payload, uint8_t flags)
{
    if (!ctx) return;
    ctx->edns_payload = payload;
    ctx->edns_flags = flags;
}

void ccdns_set_tcp(struct ccdns_t *ctx, bool enable)
{
    if (!ctx) return;
    ctx->tcp = enable;
}

uint16_t ccdns_encode(struct ccdns_t *ctx,
                       uint8_t *buf, uint16_t buflen,
                       const char *domain, ccdns_type_t qtype)
{
    if (!ctx || !buf || !domain)
        return 0;

    uint16_t off = ctx->tcp ? 2 : 0;

    if (buflen < off + DNS_HDR_SZ + 5)
        return 0;

    uint16_t id = ctx->no++;
    ctx->last = id;

    /* Ensure no never wraps to 0 */
    if (ctx->no == 0)
        ctx->no = 1;

    /* ---- Header ---- */
    memset(buf + off, 0, DNS_HDR_SZ);
    buf[off + DNS_HDR_ID]     = (uint8_t)(id >> 8);
    buf[off + DNS_HDR_ID + 1] = (uint8_t)(id & 0xFF);
    buf[off + DNS_HDR_FLAGS]     = (uint8_t)(DNS_FLAG_QR_QUERY >> 8);
    buf[off + DNS_HDR_FLAGS + 1] = (uint8_t)(DNS_FLAG_QR_QUERY & 0xFF);
    buf[off + DNS_HDR_QDCNT + 1] = 1;  /* QDCOUNT = 1 */

    /* ---- Question: QNAME ---- */
    uint16_t pos = off + DNS_HDR_SZ;
    uint16_t nlen = dns_name_encode(buf + pos, buflen - pos, domain);
    if (nlen == 0)
        return 0;
    pos += nlen;

    /* ---- Question: QTYPE + QCLASS ---- */
    if (pos + 4 > buflen)
        return 0;
    buf[pos]     = (uint8_t)((uint16_t)qtype >> 8);
    buf[pos + 1] = (uint8_t)((uint16_t)qtype & 0xFF);
    buf[pos + 2] = 0;
    buf[pos + 3] = CCDNS_CLASS_IN;
    pos += 4;

    /* ---- OPT Pseudo-Record (EDNS, RFC 6891) ---- */
    if (ctx->edns_payload > 0) {
        buf[off + DNS_HDR_ADCNT + 1] = 1; // EDNS add count must > 0;        
        if (pos + 11 > buflen) return 0;
        buf[pos]     = 0x00;                       /* NAME = root */
        buf[pos + 1] = 0x00; buf[pos + 2] = 0x29;  /* TYPE = OPT(41) */
        buf[pos + 3] = (uint8_t)(ctx->edns_payload >> 8);
        buf[pos + 4] = (uint8_t)(ctx->edns_payload & 0xFF);  /* CLASS = payload size */
        buf[pos + 5] = 0;                           /* extended RCODE */
        buf[pos + 6] = 0;                           /* version = 0 */
        buf[pos + 7] = 0;
        buf[pos + 8] = ctx->edns_flags;             /* flags (DO bit = 0x80) */
        buf[pos + 9] = 0;  buf[pos + 10] = 0;       /* RDLENGTH = 0 */
        pos += 11;
    }

    /* ---- TCP length prefix (RFC 1035 §4.2.2) ---- */
    if (ctx->tcp) {
        uint16_t bodylen = pos - off;
        buf[0] = (uint8_t)(bodylen >> 8);
        buf[1] = (uint8_t)(bodylen & 0xFF);
    }

    return pos;
}

int ccdns_decode(struct ccdns_t *ctx,
                  const uint8_t *buf, uint16_t len,
                  void *udata, ccdns_callback_t cb)
{
    if (!ctx || !buf)
        return -3;

    uint16_t off = ctx->tcp ? 2 : 0;

    if (len < off + DNS_HDR_SZ)
        return -3;

    /* ---- Parse Header ---- */
    uint16_t id     = ((uint16_t)buf[off + DNS_HDR_ID] << 8) | buf[off + DNS_HDR_ID + 1];
    uint16_t flags  = ((uint16_t)buf[off + DNS_HDR_FLAGS] << 8) | buf[off + DNS_HDR_FLAGS + 1];
    uint16_t qdcnt  = ((uint16_t)buf[off + DNS_HDR_QDCNT] << 8) | buf[off + DNS_HDR_QDCNT + 1];
    uint16_t ancnt  = ((uint16_t)buf[off + DNS_HDR_ANCNT] << 8) | buf[off + DNS_HDR_ANCNT + 1];

    uint16_t expected_id = ctx->last ? ctx->last : ctx->no;
    if (id != expected_id)
        return -2;

    /* Check QR=1 (response) and RCODE=0 (no error) */
    if (!(flags & 0x8000) || (flags & 0x0F) != 0)
        return -1;

    /* ---- Skip Question Section ---- */
    uint16_t pos = off + DNS_HDR_SZ;
    for (uint16_t i = 0; i < qdcnt; i++) {
        unsigned labels = 0;
        while (pos < len) {
            uint8_t b = buf[pos];
            if (b == 0) { pos++; break; }
            if ((b & 0xC0) == 0xC0) { pos += 2; break; }
            if (b > 63) return -1;           /* RFC 1035 §2.3.4: label length ≤ 63 */
            if (++labels > 255) return -1;   /* defense: max 255 labels per QNAME */
            pos += 1 + b;
        }
        if (pos + 4 > len) return -1;
        pos += 4;
    }

    /* ---- Parse Answer Records ---- */
    int delivered = 0;

    for (uint16_t i = 0; i < ancnt; i++) {
        ccdns_ans_t ans;
        memset(&ans, 0, sizeof(ans));
        /* ans.cls is already zeroed by memset above */

        /* NAME */
        if (dns_name_decode(buf, &pos, len, off, ans.domain, sizeof(ans.domain)) != 0)
            return -1;

        if (pos + 10 > len) return -1;
        uint16_t rrtype  = ((uint16_t)buf[pos] << 8) | buf[pos + 1];
        uint16_t rrclass = ((uint16_t)buf[pos + 2] << 8) | buf[pos + 3];
        uint32_t ttl     = ((uint32_t)buf[pos + 4] << 24)
                         | ((uint32_t)buf[pos + 5] << 16)
                         | ((uint32_t)buf[pos + 6] << 8)
                         |  buf[pos + 7];
        uint16_t rdlength = ((uint16_t)buf[pos + 8] << 8) | buf[pos + 9];
        pos += 10;

        if (pos + rdlength > len) return -1;

        ans.type = (ccdns_type_t)rrtype;
        ans.cls  = (ccdns_class_t)rrclass;
        ans.ttl  = ttl;

        switch (rrtype) {
        case CCDNS_A:
            if (rdlength == 4) {
                ccdns_snprintf(ans.ip, sizeof(ans.ip),
                              "%u.%u.%u.%u",
                              buf[pos], buf[pos+1],
                              buf[pos+2], buf[pos+3]);
            }
            break;
        case CCDNS_AAAA:
            if (rdlength == 16) {
                ccdns_snprintf(ans.ip, sizeof(ans.ip),
                              "%02x%02x:%02x%02x:%02x%02x:%02x%02x:"
                              "%02x%02x:%02x%02x:%02x%02x:%02x%02x",
                              buf[pos], buf[pos+1],
                              buf[pos+2], buf[pos+3],
                              buf[pos+4], buf[pos+5],
                              buf[pos+6], buf[pos+7],
                              buf[pos+8], buf[pos+9],
                              buf[pos+10], buf[pos+11],
                              buf[pos+12], buf[pos+13],
                              buf[pos+14], buf[pos+15]);
            }
            break;
        case CCDNS_CNAME:
        case CCDNS_NS: {
            /* Parse the target domain from RDATA (may use compression) */
            uint16_t save = pos;
            char target[CCDNS_MAX_NAME];
            if (dns_name_decode(buf, &save, len, off, target, sizeof(target)) == 0) {
                memcpy(ans.domain, target, sizeof(ans.domain));
            }
            break;
        }
        case CCDNS_MX: {
            /* Preference (2 bytes) + exchange domain (wire format) */
            if (rdlength >= 3) {
                ans.pref = ((uint16_t)buf[pos] << 8) | buf[pos + 1];
                uint16_t save = pos + 2;
                char target[CCDNS_MAX_NAME];
                if (dns_name_decode(buf, &save, len, off, target, sizeof(target)) == 0) {
                    memcpy(ans.domain, target, sizeof(ans.domain));
                }
            }
            break;
        }
        case CCDNS_TXT: {
            uint16_t end = pos + rdlength;
            uint16_t di = 0;
            uint16_t rp = pos;
            while (rp < end && di < sizeof(ans.txt) - 1) {
                uint8_t slen = buf[rp++];
                if (rp + slen > end) break;
                uint16_t copy = slen;
                if (copy > sizeof(ans.txt) - 1 - di)
                    copy = (uint16_t)(sizeof(ans.txt) - 1 - di);
                memcpy(ans.txt + di, buf + rp, copy);
                di += copy;
                rp += slen;
                if (rp < end && di < sizeof(ans.txt) - 1)
                    ans.txt[di++] = '\n';
            }
            ans.txt[di] = '\0';
            break;
        }
        default:
            break;
        }

        if (cb)
            cb(udata, &ans);

        delivered++;
        pos += rdlength;
    }

    return delivered;
}
