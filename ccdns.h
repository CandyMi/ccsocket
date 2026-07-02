/**
 * @file ccdns.h
 * @brief DNS client sub-module for libccsocket.
 *
 * Provides DNS query encoding and response decoding per RFC 1034/1035.
 * Pure algorithm — encode produces wire-format bytes suitable for
 * ccsocket_send(), decode parses bytes from ccsocket_recv().
 *
 * @author CandyMi
 * @copyright MIT
 */

#ifndef CCDNS_H
#define CCDNS_H

#include <stdbool.h>
#include <stdint.h>

/* ---- Export decoration ---- */
#if defined(_WIN32)
  #if defined(CCDNS_BUILD_SHARED)
    #define CCDNS_EXPORT __declspec(dllexport)
  #elif defined(CCDNS_SHARED)
    #define CCDNS_EXPORT __declspec(dllimport)
  #else
    #define CCDNS_EXPORT
  #endif
#else
  #define CCDNS_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Constants ----------------------------------------------------------- */

/** @brief Maximum address string length (IPv6 colon-hex). */
#define CCDNS_MAX_ADDR  65
/** @brief Maximum domain name length (RFC 1035 §2.3.4). */
#define CCDNS_MAX_NAME  255
/** @brief Maximum DNS message size (UDP, RFC 6891 EDNS).
 *  For TCP mode, the maximum message body is 65535 bytes
 *  (caller should provide a buffer of at least 65535).
 */
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
    CCDNS_MX    = 15,    /**< Mail exchange (MX) record. */
    CCDNS_TXT   = 16,    /**< Text (TXT) record. */
    CCDNS_AAAA  = 28     /**< IPv6 address record. */
} ccdns_type_t;

/** @brief Maximum TXT record data length (RFC 1035 §3.3: <character-string> ≤ 255). */
#define CCDNS_MAX_TXT   512

/* ---- DNS Answer Record --------------------------------------------------- */

/**
 * @brief A parsed DNS resource record from the answer section.
 *
 * Populated by ccdns_decode() and delivered via callback.
 * Unused fields are zeroed (e.g. ip for CNAME/TXT, domain for A).
 */
typedef struct ccdns_ans {
    char          ip[CCDNS_MAX_ADDR];    /**< Address string (A/AAAA) or empty. */
    char          domain[CCDNS_MAX_NAME];/**< Owner name, CNAME/NS/MX target, or empty. */
    char          txt[CCDNS_MAX_TXT];    /**< TXT record data (multi-string joined by newline). */
    uint16_t      pref;                  /**< MX preference (lower = higher priority). */
    uint32_t      ttl;                   /**< Time-to-live in seconds. */
    ccdns_type_t  type;                  /**< Record type (A / NS / CNAME / MX / TXT / AAAA). */
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
 *
 * When tcp is true, ccdns_encode() prepends a 2-byte length
 * prefix (RFC 1035 §4.2.2) and ccdns_decode() skips it.
 *
 * When ecs is true, ccdns_encode() embeds an EDNS Client Subnet
 * option (RFC 7871) in the OPT pseudo-record.  The ecs_family /
 * ecs_mask / ecs_addr fields specify the client prefix.
 */
typedef struct ccdns_t {
    uint16_t no;           /**< Next query identifier (never 0 while active). */
    uint16_t last;         /**< Last query identifier used (for decode verification). */
    uint16_t edns_payload; /**< EDNS UDP payload size (0 = disabled). */
    uint8_t  edns_flags;   /**< EDNS flags (e.g. 0x80 for DO bit). */
    bool     tcp;          /**< TCP mode: encode/decode with 2-byte length prefix. */
    /* EDNS Client Subnet (ECS, RFC 7871) */
    bool     ecs;          /**< ECS enabled. */
    uint8_t  ecs_family;   /**< Address family: 1=IPv4, 2=IPv6. */
    uint8_t  ecs_mask;     /**< Source prefix length (0-32 v4 / 0-128 v6). */
    uint8_t  ecs_addr[16]; /**< Prefix bytes (4 for v4, 16 for v6). */
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
CCDNS_EXPORT bool ccdns_init(struct ccdns_t *ctx);

/**
 * @brief Release a DNS client context.
 *
 * Resets the identifier counter to 0, marking the context as inactive.
 * After this call, ctx must be re-initialised via ccdns_init() before reuse.
 *
 * @param ctx  ccdns_t context to release (must not be NULL).
 */
CCDNS_EXPORT void ccdns_close(struct ccdns_t *ctx);

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
CCDNS_EXPORT void ccdns_set_edns(struct ccdns_t *ctx,
                                     uint16_t payload, uint8_t flags);

/**
 * @brief Set EDNS Client Subnet (ECS, RFC 7871).
 *
 * When set, ccdns_encode() embeds an EDNS0 option (code 0x0008) in
 * the OPT pseudo-record, signalling the authoritative server to
 * tailor its response based on the client subnet.
 *
 * Call this after ccdns_init() and before ccdns_encode().
 * Pass addr=NULL to disable ECS without affecting other EDNS settings.
 *
 * @param ctx   Initialised ccdns_t context.
 * @param addr  Client address string (e.g. "1.2.3.0", "2001:db8::").
 *              NULL or empty disables ECS.
 * @param mask  Source prefix length (0-32 for IPv4, 0-128 for IPv6).
 * @return true  on success.
 *         false on parse error or invalid mask (ECS disabled).
 */
CCDNS_EXPORT bool ccdns_set_edns_client_subnet(struct ccdns_t *ctx,
                                                const char *addr,
                                                uint8_t mask);

/**
 * @brief Enable or disable TCP mode (RFC 1035 §4.2.2).
 *
 * When enabled, ccdns_encode() prepends a 2-byte length prefix
 * to the wire-format message, and ccdns_decode() skips it before
 * parsing.
 *
 * Call this after ccdns_init() and before ccdns_encode().
 *
 * @param ctx     Initialised ccdns_t context.
 * @param enable  true to enable TCP mode, false to disable (UDP mode).
 */
CCDNS_EXPORT void ccdns_set_tcp(struct ccdns_t *ctx, bool enable);

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
 *                is disabled, up to 65535 with EDNS;
 *                ECS may add up to 28 bytes).
 * @param domain  Query domain name (e.g. "example.com").
 * @param qtype   Query record type (CCDNS_A / CCDNS_AAAA / etc.).
 * @return The encoded message length on success, 0 on failure.
 */
CCDNS_EXPORT uint16_t ccdns_encode(struct ccdns_t *ctx,
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
CCDNS_EXPORT int ccdns_decode(struct ccdns_t *ctx,
                                  const uint8_t *buf, uint16_t len,
                                  void *udata, ccdns_callback_t cb);

#ifdef __cplusplus
}
#endif

#endif /* CCDNS_H */
