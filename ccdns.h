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

/* ---- Constants ----------------------------------------------------------- */

/** @brief Maximum address string length (IPv6 colon-hex). */
#define CCDNS_MAX_ADDR  65
/** @brief Maximum domain name length (RFC 1035 §2.3.4). */
#define CCDNS_MAX_NAME  255
/** @brief Maximum DNS message size (UDP, RFC 6891 EDNS). */
#define CCDNS_MAX_MSG   4096

/* ---- Enums --------------------------------------------------------------- */

/** @brief DNS class codes (RFC 1035 §3.2.4). */
typedef enum ccdns_class {
    CCDNS_CLASS_IN = 1   /**< Internet (IN). */
} ccdns_class_t;

/** @brief DNS record types (RFC 1035 §3.2.2). */
typedef enum ccdns_type {
    CCDNS_A     = 1,     /**< IPv4 address record. */
    CCDNS_NS    = 2,     /**< Name server record. */
    CCDNS_CNAME = 5,     /**< Canonical name (alias) record. */
    CCDNS_AAAA  = 28     /**< IPv6 address record. */
} ccdns_type_t;

/* ---- DNS Answer Record --------------------------------------------------- */

/**
 * @brief A parsed DNS resource record from the answer section.
 *
 * Populated by ccdns_decode() and delivered via callback.
 * Unused fields are zeroed (e.g. ip for CNAME, domain for A).
 */
typedef struct ccdns_ans {
    char          ip[CCDNS_MAX_ADDR];    /**< Address string (A/AAAA) or empty. */
    char          domain[CCDNS_MAX_NAME];/**< Owner name or CNAME target. */
    uint32_t      ttl;                   /**< Time-to-live in seconds. */
    ccdns_type_t  type;                  /**< Record type (A / NS / CNAME / AAAA). */
    ccdns_class_t cls;                   /**< Record class. */
} ccdns_ans_t;

/* ---- DNS Context --------------------------------------------------------- */

/**
 * @brief DNS client context.
 *
 * Pure algorithm state — no socket, no I/O.
 * Must be initialised via ccdns_init() before use and released
 * via ccdns_close() when done.  The no field holds the next
 * query identifier and is never 0 while the context is active.
 */
typedef struct ccdns_t {
    uint16_t no;           /**< Next query identifier (never 0 while active). */
    uint16_t last;         /**< Last query identifier used (for decode verification). */
    uint16_t edns_payload; /**< EDNS UDP payload size (0 = disabled). */
    uint8_t  edns_flags;   /**< EDNS flags (e.g. 0x80 for DO bit). */
} ccdns_t;

/* ---- Callback ------------------------------------------------------------ */

/**
 * @brief Callback invoked by ccdns_decode() for each answer record.
 *
 * @param udata  User-supplied context pointer (passed through from decode).
 * @param ans    Parsed answer record (valid only during the callback).
 */
typedef void (*ccdns_callback_t)(void *udata, const ccdns_ans_t *ans);

/* ---- Public API ---------------------------------------------------------- */

/**
 * @brief Initialise a DNS client context.
 *
 * Sets the query identifier counter to 1.  No socket or I/O is created.
 *
 * @param ctx  Uninitialised ccdns_t context pointer (must not be NULL).
 * @return true on success (always).
 */
CCSOCKET_EXPORT bool ccdns_init(struct ccdns_t *ctx);

/**
 * @brief Release a DNS client context.
 *
 * Resets the identifier counter to 0, marking the context as inactive.
 * After this call, ctx must be re-initialised via ccdns_init() before reuse.
 *
 * @param ctx  ccdns_t context to release (must not be NULL).
 */
CCSOCKET_EXPORT void ccdns_close(struct ccdns_t *ctx);

/**
 * @brief Enable EDNS (RFC 6891) on subsequent queries.
 *
 * When enabled, ccdns_encode() appends an OPT pseudo-record with the
 * specified UDP payload size, allowing the DNS server to send responses
 * larger than 512 bytes.
 *
 * Call this after ccdns_init() and before ccdns_encode().
 *
 * @param ctx     Initialised ccdns_t context.
 * @param payload Maximum UDP payload size the caller can receive
 *                (recommend 4096, 0 to disable).
 * @param flags   EDNS flags (e.g. 0x80 for DNSSEC OK).
 */
CCSOCKET_EXPORT void ccdns_set_edns(struct ccdns_t *ctx,
                                     uint16_t payload, uint8_t flags);

/**
 * @brief Encode a DNS question into wire-format bytes.
 *
 * Builds a 12-byte DNS header followed by the question section.
 * Uses ctx->no as the query ID and increments it, ensuring no
 * never wraps to 0.
 *
 * @param ctx     Initialised ccdns_t context.
 * @param buf     Output buffer for the wire-format message.
 * @param buflen  Capacity of buf (recommend CCDNS_MAX_MSG when EDNS
 *                is disabled, up to 65535 with EDNS).
 * @param domain  Query domain name (e.g. "example.com").
 * @param qtype   Query record type (CCDNS_A / CCDNS_AAAA / etc.).
 * @return The encoded message length on success, 0 on failure.
 */
CCSOCKET_EXPORT uint16_t ccdns_encode(struct ccdns_t *ctx,
                                       uint8_t *buf, uint16_t buflen,
                                       const char *domain, ccdns_type_t qtype);

/**
 * @brief Decode a DNS response, invoking cb for each answer record.
 *
 * Parses the DNS header, verifies the ID matches ctx->no, checks
 * flags/RCODE, skips the question section, and iterates answer RRs.
 * For each A / AAAA / NS / CNAME record found, cb is called with a
 * populated ccdns_ans_t.
 *
 * @param ctx    DNS context (for expected ID via ctx->no).
 * @param buf    Wire-format DNS response message.
 * @param len    Length of the response message.
 * @param udata  User pointer forwarded to cb (may be NULL).
 * @param cb     Callback invoked per answer record (may be NULL to skip).
 * @return Number of answer records delivered to cb on success,
 *         -1 on parse error, -2 on ID mismatch, -3 on truncation.
 */
CCSOCKET_EXPORT int ccdns_decode(struct ccdns_t *ctx,
                                  const uint8_t *buf, uint16_t len,
                                  void *udata, ccdns_callback_t cb);

#ifdef __cplusplus
}
#endif

#endif /* CCDNS_H */
