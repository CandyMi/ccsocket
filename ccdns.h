/**
 * @file ccdns.h
 * @brief DNS client sub-module for libccsocket.
 *
 * Provides DNS query encoding and response decoding per RFC 1034/1035.
 * Pure algorithm — encode produces wire-format bytes suitable for
 * ccsocket_send(), decode parses bytes from ccsocket_recv().
 *
 * @author CandyMi
 * @license MIT
 */

#ifndef CCDNS_H
#define CCDNS_H

#include "ccsocket.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- DNS Constants ------------------------------------------------------ */

/** @brief DNS query type: IPv4 address record. */
#define CCDNS_A     1
/** @brief DNS query type: IPv6 address record. */
#define CCDNS_AAAA  28
/** @brief DNS class: Internet (IN). */
#define CCDNS_CLASS_IN 1

/** @brief Maximum DNS message size (UDP, RFC 1035 §4.2.1). */
#define CCDNS_MAX_MSG   512
/** @brief Maximum domain name length (RFC 1035 §2.3.4). */
#define CCDNS_MAX_NAME  255

/* ---- DNS Context -------------------------------------------------------- */

/**
 * @brief DNS client context.
 *
 * Manages a UDP socket and an auto-incrementing query identifier.
 * Must be initialised via ccdns_init() before use and closed via
 * ccdns_close() when done.
 */
typedef struct ccdns_t {
    ccsocket_t fd;      /**< UDP socket for DNS queries. */
    uint16_t   reqno;   /**< Auto-incrementing request identifier. */
} ccdns_t;

/* ---- Public API ---------------------------------------------------------- */

/**
 * @brief Initialise a DNS client context.
 *
 * Creates a UDP socket (IPv4) and initialises the request counter.
 *
 * @param ctx  Uninitialised ccdns_t context pointer (must not be NULL).
 * @return true on success, false on failure.
 */
CCSOCKET_EXPORT bool ccdns_init(struct ccdns_t *ctx);

/**
 * @brief Destroy a DNS client context and close the underlying socket.
 *
 * After this call, ctx must be re-initialised via ccdns_init() before reuse.
 * Sets ctx->fd to INVALID_SOCKET.
 *
 * @param ctx  ccdns_t context to close (must not be NULL).
 */
CCSOCKET_EXPORT void ccdns_close(struct ccdns_t *ctx);

/**
 * @brief Encode a DNS question into wire-format bytes.
 *
 * Builds a 12-byte DNS header followed by the question section.
 * Uses ctx->reqno as the query ID and increments it atomically.
 * The encoded message can be sent via ccsocket_send().
 *
 * @param ctx     Initialised ccdns_t context.
 * @param buf     Output buffer for the wire-format message.
 * @param buflen  Capacity of buf (recommend CCDNS_MAX_MSG).
 * @param domain  Query domain name (e.g. "example.com").
 * @param qtype   Query type: CCDNS_A or CCDNS_AAAA.
 * @return The encoded message length on success, 0 on failure.
 */
CCSOCKET_EXPORT uint16_t ccdns_encode(struct ccdns_t *ctx,
                                       uint8_t *buf, uint16_t buflen,
                                       const char *domain, uint16_t qtype);

/**
 * @brief Decode a DNS response and extract the first address record.
 *
 * Parses the DNS header, verifies the ID matches expected_id, checks the
 * QR/TC/RCODE flags, skips the question section, and searches answer RRs
 * for the first A (IPv4) or AAAA (IPv6) record.
 *
 * On success, addr receives a NUL-terminated string:
 *   - IPv4: dotted decimal ("1.2.3.4")
 *   - IPv6: colon-hex ("2001:db8::1")
 *
 * @param buf          Wire-format DNS response message.
 * @param len          Length of the response message.
 * @param expected_id  Expected query ID (from ccdns_encode).
 * @param addr         Output buffer for the address string (>= 64 bytes).
 * @param addrlen      In: capacity of addr. Out: written bytes (excluding NUL).
 * @return 0 on success, -1 on decode error, -2 on ID mismatch,
 *         -3 on truncation, -4 on no address record found.
 */
CCSOCKET_EXPORT int ccdns_decode(const uint8_t *buf, uint16_t len,
                                  uint16_t expected_id,
                                  char *addr, uint16_t *addrlen);

#ifdef __cplusplus
}
#endif

#endif /* CCDNS_H */
