/**
 * @file ccicmp.h
 * @brief ICMP echo (ping) sub-module for libccsocket.
 *
 * IPv4 and IPv6 ICMP echo request/reply with timestamp-based RTT.
 * Compiled as part of the ccsocket library target.
 *
 * @author CandyMi
 * @copyright MIT
 */

#ifndef CCICMP_H
#define CCICMP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ---- Platform socket type (self-contained, no ccsocket.h dependency) ---- */
/* Guard: ccsocket.h may be included alongside ccicmp.h; avoid -Wpedantic
 * "redefinition of typedef" when both define ccsocket_t identically. */
#ifndef CCICMP_CCSOCKET_T_DEFINED
#define CCICMP_CCSOCKET_T_DEFINED
#if _WIN32
  typedef intptr_t ccsocket_t;
#else
  typedef int ccsocket_t;
  #ifndef CCICMP_SOCKET_DEFINED
    #define CCICMP_SOCKET_DEFINED
    typedef ccsocket_t SOCKET;
  #endif
#endif
#endif /* CCICMP_CCSOCKET_T_DEFINED */

/* ---- Export decoration (always shared, no build/consumer toggle) ---- */
#if _WIN32
  #define CCICMP_EXPORT __declspec(dllexport)
#else
  #define CCICMP_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ccicmp_t {
  ccsocket_t fd;   /**< Socket handle. */
  uint16_t   id;   /**< ICMP identifier. */
  uint16_t   no;   /**< Sequence number. */
  int        ttl;  /**< TTL / Hop Limit of last reply (-1 = unknown). */
} ccicmp_t;

/**
 * @brief Initialise an ICMP ping context.
 *
 * Creates either a SOCK_DGRAM (CC_ICMP1) or SOCK_RAW (CC_ICMP) socket
 * for the given address family, preferring DGRAM when the platform
 * supports it (Linux ≥ 3.0, macOS) for privilege-free operation.
 * Falls back to RAW automatically.
 *
 * Must be called before ccicmp_echo() / ccicmp_reply().
 * Initialises ctx->ttl to -1 (unknown).
 * When the platform supports IP_RECVTTL / IPV6_RECVHOPLIMIT,
 * subsequent ccicmp_reply() calls populate ctx->ttl with the
 * TTL / Hop Limit from the received packet.
 *
 * @param ctx     Uninitialised ccicmp_t context pointer.
 * @param domain  Address family: CC_INET4 or CC_INET6.
 *
 * @note On Linux the DGRAM ping socket works without special privileges.
 *       On macOS the DGRAM ping socket works without root but the
 *       process must have the com.apple.private.network.socket entitlement.
 *       On Windows raw socket ICMP is restricted; init will fail.
 *       Call early so the caller can fall back gracefully.
 *
 * @return true on success, false on failure.
 */
CCICMP_EXPORT bool ccicmp_init(struct ccicmp_t *ctx, int domain);

/**
 * @brief Destroy an ICMP ping context and close the underlying socket.
 *
 * After this call, ctx must be re-initialised via ccicmp_init() before reuse.
 * Sets ctx->fd to INVALID_SOCKET and clears id/seq counters.
 *
 * @param ctx  ccicmp_t context to close (must not be NULL).
 */
CCICMP_EXPORT void ccicmp_close(struct ccicmp_t *ctx);

/**
 * @brief Send an ICMP Echo Request (ping) to a target address.
 *
 * Constructs an ICMP packet with an 8-byte header, 8-byte timestamp
 * (for RTT measurement), and optional user payload. Automatically
 * computes the correct checksum for IPv4 or IPv6.
 *
 * The socket must already be initialised via ccicmp_init().
 * After sending, call ccicmp_reply() to await the response.
 *
 * @param ctx   Initialised ccicmp_t context.
 * @param addr  Target IPv4 or IPv6 address string.
 * @param data  Optional payload data (may be NULL).
 * @param len   Payload length in bytes, capped at CCICMP_MAX_PAYLOAD.
 * @return true on success, false on failure.
 */
CCICMP_EXPORT bool ccicmp_echo(struct ccicmp_t *ctx, const char *addr, const char *data, size_t len);

/**
 * @brief Receive a matching ICMP Echo Reply.
 *
 * Reads from the raw socket and matches the response by ICMP type,
 * identifier, and code. Automatically handles platform differences
 * in IPv6 header inclusion (Linux includes it, macOS/BSD strips it).
 *
 * Call after ccicmp_echo(). In non-blocking mode, returns false
 * immediately if no reply is available (check errno for EAGAIN).
 * Updates ctx->ttl with the TTL / Hop Limit from the received packet.
 *
 * @param ctx   Initialised ccicmp_t context.
 * @param data  Optional buffer to receive the reply payload (may be NULL
 *              to discard payload data).
 * @param len   In: capacity of data buffer. Out: actual payload bytes.
 *              May be NULL if data is NULL.
 * @return true if a matching reply was received, false otherwise.
 */
CCICMP_EXPORT bool ccicmp_reply(struct ccicmp_t *ctx, char *data, size_t *len);

#ifdef __cplusplus
}
#endif

#endif /* CCICMP_H */
