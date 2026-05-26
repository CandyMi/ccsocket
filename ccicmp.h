#ifndef CCICMP_H
#define CCICMP_H

#include "ccsocket.h"

typedef struct ccicmp_t
{
  ccsocket_t fd; // socket
  uint16_t id;   // id
  uint16_t no;   // seq
} ccicmp_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * init ccicmp context, `domain` only between `CC_INET4` and `CC_INET6`.
 *
 * @note ICMP raw socket requires root / admin privileges on most systems:
 *       - Linux: needs `CAP_NET_RAW` or root.
 *       - BSD/macOS: requires root.
 *       - Windows: raw socket is restricted, `ccicmp_init` will fail directly.
 *       Call `ccicmp_init` early so the caller can fallback or notify the user.
 */
CCSOCKET_EXPORT bool ccicmp_init(struct ccicmp_t *ctx, ccsocket_family_t domain);

/* destroy and close ccicmp context */
CCSOCKET_EXPORT void ccicmp_close(struct ccicmp_t *ctx);

/* send ICMP echo request to `addr` with optional payload `data` */
CCSOCKET_EXPORT bool ccicmp_echo(struct ccicmp_t *ctx, const char *addr, const char *data, size_t len);

/* receive ICMP echo reply, `data` is output buffer, `len` in/out max/actual */
CCSOCKET_EXPORT bool ccicmp_reply(struct ccicmp_t *ctx, char *data, size_t *len);

#ifdef __cplusplus
}
#endif

#endif /* CCICMP_H */